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
    bool leveled = !gs.party.empty() && gs.party[0].level > beforeLv;
    if (leveled) engine_.audio().playSfx("levelup");
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
    // item drop roll (from EnemyDef)
    if (const EnemyDef* def = engine_.project().database.enemy(m.enemyId)) {
        if (def->dropItemId >= 0 && def->dropRate > 0 && (std::rand() % 100) < def->dropRate) {
            gs.inventory.addItem(def->dropItemId, 1);
            const Item* it = engine_.project().database.item(def->dropItemId);
            toast_ += "   [" + (it ? it->name : std::string("아이템")) + " 획득!]";
        }
    }
    if (leveled) toast_ += "   ★레벨 업! Lv " + std::to_string(gs.party[0].level);
    toastTimer_ = leveled ? 2.6f : 1.8f;
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

// ----------------------------- field engagement (no turn-based) -----------------------------
// Random encounter: enemies appear ON THE FIELD around the player and are fought
// in real time (this game has no turn-based battle screen).
void GamePlay::startEncounterBattle() {
    if (!map_ || map_->encounterEnemies.empty()) return;
    std::vector<int> ids;
    int n = 1 + std::rand() % 3;                       // 1..3 enemies
    for (int i = 0; i < n; ++i)
        ids.push_back(map_->encounterEnemies[std::rand() % map_->encounterEnemies.size()]);
    startBattleWith(ids);
}

// Spawn the given enemy troop on the field near the player (used by random
// encounters AND StartBattle events — everything is field combat now).
void GamePlay::startBattleWith(const std::vector<int>& enemyIds) {
    if (!map_ || enemyIds.empty()) return;
    const Database& db = engine_.project().database;
    int TS = map_->tileset.tileWidth;
    int W = map_->tilemap.width(), H = map_->tilemap.height();
    bool any = false;
    for (int i = 0; i < (int)enemyIds.size(); ++i) {
        const EnemyDef* def = db.enemy(enemyIds[i]);
        if (!def) continue;
        // search a free tile near the player (ring out from a small offset)
        int sx = destX_, sy = destY_, fx = sx, fy = sy; bool found = false;
        for (int r = 1; r <= 5 && !found; ++r)
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    int x = sx + dx, y = sy + dy;
                    if (x < 0 || y < 0 || x >= W || y >= H) continue;
                    if ((x == sx && y == sy) || !walkable(x, y) || monsterAt(x, y)) continue;
                    fx = x; fy = y; found = true;
                }
        if (!found) continue;
        FieldMonster m;
        m.enemyId = def->id; m.name = def->name; m.spriteAsset = def->spriteAsset;
        m.x = m.destX = fx; m.y = m.destY = fy;
        m.px = fx * (float)TS; m.py = fy * (float)TS;
        m.hp = m.maxHp = def->maxHp; m.atk = def->atk; m.def = def->def;
        m.expReward = def->expReward; m.goldReward = def->goldReward;
        monsters_.push_back(m);
        targetMonsters_ = std::max(targetMonsters_, (int)monsters_.size());
        any = true;
    }
    if (any) { toast_ = "적이 나타났다!"; toastTimer_ = 1.5f; engine_.audio().playSfx("select"); }
}


} // namespace tsukuru
