// GamePlayMonsters: field-monster spawning, AI, walkability, rendering.
#include "game/GamePlay.h"
#include "core/Engine.h"
#include "game/Menu.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <set>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace tsukuru {

// Drop every monster/NPC that died this frame. Centralised so the four call
// sites (melee, projectile, monster & NPC updates) prune identically.
void GamePlay::reapDead() {
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                    [](const FieldMonster& m){ return !m.alive(); }), monsters_.end());
    npcs_.erase(std::remove_if(npcs_.begin(), npcs_.end(),
                [](const NpcInst& n){ return !n.alive(); }), npcs_.end());
}

void GamePlay::spawnMonsters() {
    monsters_.clear();
    if (!map_) { targetMonsters_ = 0; return; }
    // only maps that explicitly list encounter enemies spawn random field monsters
    if (map_->encounterEnemies.empty()) { targetMonsters_ = 0; return; }
    if (map_->encounterRate > 0) { targetMonsters_ = 0; return; } // turn-based encounters instead
    int area = map_->tilemap.width() * map_->tilemap.height();
    targetMonsters_ = std::min(8, std::max(3, area / 45));
    for (int i = 0; i < targetMonsters_; ++i) spawnOne();
}

void GamePlay::spawnOne() {
    if (!map_) return;
    const Database& db = engine_.project().database;
    const std::vector<int>& pool = map_->encounterEnemies;
    if (pool.empty()) return;
    int enemyId = pool[std::rand() % pool.size()];
    const EnemyDef* def = db.enemy(enemyId);
    if (!def) return;

    int TS = map_->tileset.tileWidth;
    for (int tries = 0; tries < 40; ++tries) {
        int x = std::rand() % map_->tilemap.width();
        int y = std::rand() % map_->tilemap.height();
        if (!walkable(x, y)) continue;
        if (std::abs(x - destX_) + std::abs(y - destY_) < 4) continue; // not on top of player
        FieldMonster m;
        m.enemyId = def->id; m.name = def->name; m.spriteAsset = def->spriteAsset;
        m.x = m.destX = x; m.y = m.destY = y;
        m.px = x * (float)TS; m.py = y * (float)TS;
        m.hp = m.maxHp = def->maxHp;
        m.atk = def->atk; m.def = def->def;
        m.expReward = def->expReward; m.goldReward = def->goldReward;
        m.moveCd = 0.3f + (std::rand() % 100) / 100.0f;
        monsters_.push_back(m);
        return;
    }
}

FieldMonster* GamePlay::monsterAt(int x, int y) {
    for (auto& m : monsters_) if (m.alive() && m.x == x && m.y == y) return &m;
    return nullptr;
}

void GamePlay::onMonsterKilled(const FieldMonster& m) {
    GameState& gs = engine_.state();
    gs.inventory.gold += m.goldReward;
    int beforeLv = gs.party.empty() ? 0 : gs.party[0].level;
    for (auto& p : gs.party) if (p.alive()) p.gainExp(m.expReward);
    gs.addKillProgress(m.enemyId);    // 처치형 퀘스트 진행
    refreshQuestObjective();
    engine_.audio().playSfx("defeat", 0.8f);
    if (!gs.party.empty() && gs.party[0].level > beforeLv) engine_.audio().playSfx("levelup");
    // boss gate: when the last monster of a tagged troop dies, flip its switch
    if (m.defeatSwitch >= 0) {
        bool anyLeft = false;
        for (const auto& o : monsters_)
            if (&o != &m && o.alive() && o.defeatSwitch == m.defeatSwitch) { anyLeft = true; break; }
        if (!anyLeft) {
            gs.setSwitch(m.defeatSwitch, true);
            toast_ = "앞길이 열렸다!"; toastTimer_ = 2.5f;
            engine_.audio().playSfx("levelup");
            return;
        }
    }
    toast_ = m.name + " 처치!  +" + std::to_string(m.expReward) + " EXP  +" +
             std::to_string(m.goldReward) + " G";
    toastTimer_ = 1.8f;
    TraceLog(LOG_INFO, "KILL: %s  party gold=%d exp=%d lv=%d", m.name.c_str(),
             gs.inventory.gold, gs.party.empty()?0:gs.party[0].exp,
             gs.party.empty()?0:gs.party[0].level);
}

void GamePlay::updateMonsters(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int pdef = gs.party.empty() ? 0 : gs.party[0].totalDef(db);

    // maintain population
    if ((int)monsters_.size() < targetMonsters_) {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0) { spawnOne(); spawnTimer_ = 2.5f; }
    }

    for (auto& m : monsters_) {
        if (m.hurtFlash > 0) m.hurtFlash -= dt;
        if (m.atkCd > 0)     m.atkCd -= dt;

        // smooth move
        if (m.moving) {
            float tx = m.destX * (float)TS, ty = m.destY * (float)TS;
            float dx = tx - m.px, dy = ty - m.py;
            float dist = std::sqrt(dx*dx + dy*dy);
            float step = TS * 3.0f * dt;
            if (dist <= step) { m.px = tx; m.py = ty; m.x = m.destX; m.y = m.destY; m.moving = false; }
            else { m.px += dx / dist * step; m.py += dy / dist * step; }
        } else {
            m.moveCd -= dt;
            if (m.moveCd <= 0) {
                m.moveCd = 0.45f + (std::rand() % 60) / 100.0f;
                int cheb = chebyshev(m.x, m.y, destX_, destY_);
                int ddx = 0, ddy = 0;
                if (cheb <= 6) {                  // chase the player
                    ddx = isign(destX_ - m.x);
                    ddy = isign(destY_ - m.y);
                } else if (std::rand() % 3 != 0) { // wander
                    int r = std::rand() % 4;
                    ddx = (r == 0) - (r == 1);
                    ddy = (r == 2) - (r == 3);
                }
                // try preferred axis first, then the other
                int tries[4][2] = { {ddx,0}, {0,ddy}, {ddy,ddx}, {-ddx,-ddy} };
                for (auto& t : tries) {
                    if (t[0] == 0 && t[1] == 0) continue;
                    int nx = m.x + isign(t[0]), ny = m.y + isign(t[1]);
                    if (nx == destX_ && ny == destY_) continue; // don't step onto player
                    if (walkable(nx, ny)) {
                        m.destX = nx; m.destY = ny; m.moving = true;
                        m.dir = dirFromDelta(t[0], t[1]);
                        break;
                    }
                }
            }
        }

        // attack the player when adjacent
        int cheb = chebyshev(m.x, m.y, destX_, destY_);
        if (cheb <= 1 && m.atkCd <= 0 && !gs.party.empty()) {
            m.atkCd = 1.1f;
            int dmg = std::max(1, m.atk - pdef);
            PartyMember& hero = gs.party[0];
            hero.hp = std::max(0, hero.hp - dmg);
            playerHurt_ = 0.22f;
            engine_.audio().playSfx("hurt", 0.7f);
            if (gs.partyWiped()) {
                // play the custom death motion first (if any), else go straight to GameOver
                const CharacterDef* cd = customChar();
                if (cd && !cd->motions[MO_Death].frames.empty()) {
                    triggerMotion(MO_Death);
                    dyingTimer_ = motionTimer_ > 0 ? motionTimer_ : 0.8f;
                } else phase_ = Phase::GameOver;
                return;
            }
        } else if (m.atkCd <= 0) {
            // not next to the player — swat an adjacent ally NPC instead
            for (auto& a : npcs_) {
                if (a.faction != NpcFaction::Ally || !a.alive()) continue;
                if (chebyshev(a.x, a.y, m.x, m.y) > 1) continue;
                m.atkCd = 1.1f; damageNpc(a, std::max(1, m.atk - a.def)); break;
            }
        }
    }
    reapDead();   // an ally may have been killed by a monster this frame
}

// ----------------------------- monster rendering -----------------------------
void GamePlay::drawMonsters() {
    int TS = map_->tileset.tileWidth;
    for (auto& m : monsters_) {
        Color tint = m.hurtFlash > 0 ? Color{ 255, 120, 120, 255 } : WHITE;
        if (m.spriteAsset >= 0) {
            const Texture2D& tex = engine_.assetTexture(m.spriteAsset);
            float size = TS * 1.15f;
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
            Rectangle dst = { m.px + (TS - size)/2, m.py + (TS - size)/2, size, size };
            DrawTexturePro(tex, src, dst, {0,0}, 0, tint);
        } else {
            DrawCircle((int)m.px + TS/2, (int)m.py + TS/2, TS*0.4f,
                       m.hurtFlash > 0 ? RED : Color{ 180, 90, 90, 255 });
        }
        // HP bar when damaged
        if (m.hp < m.maxHp) {
            float w = TS, ratio = (float)m.hp / m.maxHp;
            DrawRectangle((int)m.px, (int)m.py - 7, (int)w, 4, Fade(BLACK, 0.6f));
            DrawRectangle((int)m.px, (int)m.py - 7, (int)(w * ratio), 4, Color{ 220, 80, 80, 255 });
        }
    }
}

// ----------------------------- damage / death -----------------------------
bool GamePlay::damageMonster(FieldMonster& m, int dmg) {
    if (!m.alive()) return false;
    m.hp -= std::max(1, dmg);
    m.hurtFlash = 0.18f;
    if (m.hp <= 0) { onMonsterKilled(m); return true; }
    return false;
}

// ----------------------------- turn-based battle -----------------------------
void GamePlay::startEncounterBattle() {
    if (!map_ || map_->encounterEnemies.empty()) return;
    std::vector<int> ids;
    int n = 1 + std::rand() % 3;                       // 1..3 enemies
    for (int i = 0; i < n; ++i)
        ids.push_back(map_->encounterEnemies[std::rand() % map_->encounterEnemies.size()]);
    startBattleWith(ids);
}

void GamePlay::startBattleWith(const std::vector<int>& enemyIds) {
    if (enemyIds.empty()) return;
    battle_ = std::make_unique<Battle>(engine_.project().database, engine_.state(), enemyIds);
    battleMenu_ = 0;
    phase_ = Phase::Battle;
    engine_.audio().playSfx("select");
}

void GamePlay::updateBattle(float dt) {
    (void)dt;
    if (!battle_) { phase_ = Phase::Field; return; }   // input handled in drawBattle (immediate mode)
}

void GamePlay::drawBattle() {
    if (!battle_) { phase_ = Phase::Field; return; }
    Database& db = engine_.project().database;
    GameState& gs = engine_.state();
    int sw = screenW(), sh = screenH();
    DrawRectangleGradientV(0, 0, sw, sh, Color{ 46, 24, 30, 255 }, Color{ 12, 10, 20, 255 });
    DrawTextU("전투!", 24, 18, 28, ui::kDanger);

    // enemies
    const auto& ens = battle_->enemies();
    for (int i = 0; i < (int)ens.size(); ++i) {
        const BattleEnemy& e = ens[i];
        int x = 120 + i * 200, y = 110;
        if (e.spriteAsset >= 0) {   // draw the enemy battler image when assigned
            const Texture2D& tx = engine_.assetTexture(e.spriteAsset);
            Color tint = e.alive() ? WHITE : Color{ 90, 90, 100, 255 };
            DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {(float)x,(float)y,140,90}, {0,0}, 0, tint);
        } else {
            DrawRectangle(x, y, 140, 90, e.alive() ? Color{ 200, 90, 90, 255 } : Color{ 70, 70, 82, 255 });
        }
        DrawRectangleLines(x, y, 140, 90, BLACK);
        DrawTextU(e.name.c_str(), x + 6, y - 22, 16, ui::kText);
        DrawTextU(TextFormat("HP %d/%d", e.hp, e.maxHp), x + 6, y + 96, 14, e.alive() ? ui::kText : ui::kTextDim);
    }

    // party status
    int py = sh - 230;
    if (!gs.party.empty()) {
        PartyMember& m = gs.party[0];
        DrawTextU(TextFormat("아군 — 체력 %d/%d   기력 %d/%d", m.hp, m.maxHp, m.mp, m.maxMp),
                  24, py, 18, ui::kAccentHi);
    }
    // last log lines
    const auto& lg = battle_->log();
    int ly = py + 28;
    for (int i = std::max(0, (int)lg.size() - 4); i < (int)lg.size(); ++i) { DrawTextU(lg[i].c_str(), 24, ly, 15, ui::kText); ly += 20; }

    // result -> continue
    if (battle_->result() != BattleResult::Ongoing) {
        const char* r = battle_->result() == BattleResult::Victory ? "승리!"
                      : battle_->result() == BattleResult::Defeat  ? "패배..." : "도망쳤다";
        DrawTextU(r, sw / 2 - 60, sh / 2 - 30, 40, ui::kGood);
        DrawTextU("Enter / 클릭으로 계속", sw / 2 - 96, sh / 2 + 24, 18, ui::kTextDim);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            BattleResult res = battle_->result();
            battle_.reset();
            phase_ = (res == BattleResult::Defeat) ? Phase::GameOver : Phase::Field;
        }
        return;
    }

    // action menu for the current actor
    if (battle_->actorReady()) {
        float bx = 24, by = sh - 150;
        int firstE = battle_->firstAliveEnemy();
        if (battleMenu_ == 0) {
            if (ui::button({ bx,       by, 120, 36 }, "공격"))   { BattleAction a; a.kind = ActionKind::Attack; a.targetIndex = firstE; battle_->submit(a); }
            if (ui::button({ bx + 130, by, 120, 36 }, "스킬"))   battleMenu_ = 1;
            if (ui::button({ bx + 260, by, 120, 36 }, "아이템")) battleMenu_ = 2;
            if (ui::button({ bx + 390, by, 120, 36 }, "도망"))   { BattleAction a; a.kind = ActionKind::Flee; battle_->submit(a); }
        } else if (battleMenu_ == 1) {
            DrawTextU("스킬 선택:", (int)bx, (int)by - 22, 14, ui::kTextDim);
            float yy = by; int shown = 0; int ai = battle_->currentActor();
            if (ai >= 0 && ai < (int)gs.party.size()) {
                if (const ActorDef* ad = db.actor(gs.party[ai].actorId))
                    for (int sid : ad->skills) {
                        const Skill* sk = db.skill(sid); if (!sk) continue;
                        if (ui::button({ bx, yy, 240, 30 }, TextFormat("%s (MP%d)", sk->name.c_str(), sk->mpCost))) {
                            BattleAction a; a.kind = ActionKind::Skill; a.id = sid; a.targetIndex = firstE; battle_->submit(a); battleMenu_ = 0;
                        }
                        yy += 34; ++shown;
                    }
            }
            if (!shown) DrawTextU("사용할 스킬이 없습니다.", (int)bx, (int)yy, 14, ui::kTextDim);
            if (ui::button({ bx + 280, by, 90, 30 }, "뒤로")) battleMenu_ = 0;
        } else {
            DrawTextU("아이템 선택:", (int)bx, (int)by - 22, 14, ui::kTextDim);
            float yy = by; int shown = 0;
            for (auto& pr : gs.inventory.list()) {
                const Item* it = db.item(pr.first);
                if (!it || it->effect == ItemEffect::None) continue;   // only battle-usable items
                if (ui::button({ bx, yy, 280, 30 }, TextFormat("%s x%d", it->name.c_str(), pr.second))) {
                    BattleAction a; a.kind = ActionKind::Item; a.id = pr.first; a.targetIndex = 0; battle_->submit(a); battleMenu_ = 0;
                }
                yy += 34; ++shown; if (yy > sh - 30) break;
            }
            if (!shown) DrawTextU("전투에서 쓸 아이템이 없습니다.", (int)bx, (int)yy, 14, ui::kTextDim);
            if (ui::button({ bx + 300, by, 90, 30 }, "뒤로")) battleMenu_ = 0;
        }
    }
}

} // namespace tsukuru
