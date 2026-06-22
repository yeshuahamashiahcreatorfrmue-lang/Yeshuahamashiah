// GamePlayNpc: map NPC instances — factions (중립/아군/적군), per-NPC behaviour
// (대기/배회/순찰/추격/도망), lightweight field combat, and autorun cutscenes.
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

void GamePlay::spawnNpcs() {
    npcs_.clear();
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    const Database& db = engine_.project().database;
    for (auto& e : map_->events) {
        if (e.graphicAsset < 0 && e.charId < 0) continue;   // NPC = 캐릭터(charId) 또는 구버전 스프라이트
        NpcInst n;
        n.eventId = e.id; n.spriteAsset = e.graphicAsset; n.charId = e.charId;
        n.faction = e.faction; n.behavior = e.behavior; n.drawPct = e.drawPct;
        n.drawTilesW = e.drawTilesW; n.drawTilesH = e.drawTilesH;
        if (const CharacterDef* cd = db.character(e.charId)) {   // footprint from the registered char
            n.drawPct = cd->drawPct; n.drawTilesW = std::max(1, cd->drawTilesW); n.drawTilesH = std::max(1, cd->drawTilesH);
        }
        n.x = n.destX = n.homeX = e.x; n.y = n.destY = n.homeY = e.y;
        n.px = e.x * (float)TS; n.py = e.y * (float)TS;
        n.moveCd = 0.6f + (std::rand() % 100) / 80.0f;
        n.patrolDir = (std::rand() % 2) ? 2 : 0;    // start pacing on X or Y
        if (e.faction != NpcFaction::Neutral) {     // Ally/Enemy carry combat stats
            n.hp = n.maxHp = std::max(1, e.npcHp);
            n.atk = e.npcAtk; n.def = e.npcDef;
            n.switchOnDeath = e.switchId;
        }
        npcs_.push_back(n);
    }
}

NpcInst* GamePlay::npcAt(int x, int y) {
    for (auto& n : npcs_) if (n.alive() && n.x == x && n.y == y) return &n;
    return nullptr;
}

NpcInst* GamePlay::hostileNpcAt(int x, int y) {
    for (auto& n : npcs_)
        if (n.faction == NpcFaction::Enemy && n.combatant() && n.alive()
            && n.x == x && n.y == y) return &n;
    return nullptr;
}

bool GamePlay::damageNpc(NpcInst& n, int dmg) {
    if (!n.combatant() || !n.alive()) return false;
    int d = std::max(1, dmg);
    n.hp -= d;
    n.hurtFlash = 0.18f;
    spawnPopup(n.px, n.py, std::to_string(d), Color{ 255, 220, 90, 255 });
    if (n.hp <= 0) { onNpcKilled(n); return true; }
    return false;
}

void GamePlay::onNpcKilled(NpcInst& n) {
    GameState& gs = engine_.state();
    if (n.switchOnDeath >= 0) gs.setSwitch(n.switchOnDeath, true);
    gs.addKillProgress(-1);    // 적 NPC 처치도 "아무거나 처치" 퀘스트에 반영
    refreshQuestObjective();
    toast_ = "적 처치!"; toastTimer_ = 1.5f;
    engine_.audio().playSfx("defeat", 0.8f);
}

// Decide one NPC's next grid move (and facing) when it is standing still.
void GamePlay::npcDecide(NpcInst& n, float dt) {
    constexpr int kAggro = 6;     // enemies notice the player within this range
    constexpr int kHelp  = 7;     // allies look for foes within this range

    int cheb = chebyshev(n.x, n.y, destX_, destY_);

    // attempt to step toward (gx,gy); returns true if a move was committed.
    auto stepToward = [&](int gx, int gy, bool away) -> bool {
        int ddx = isign(gx - n.x), ddy = isign(gy - n.y);
        if (away) { ddx = -ddx; ddy = -ddy; }
        int tries[4][2] = { {ddx, ddy}, {ddx, 0}, {0, ddy}, {ddy, ddx} };
        for (auto& t : tries) {
            if (t[0] == 0 && t[1] == 0) continue;
            int nx = n.x + isign(t[0]), ny = n.y + isign(t[1]);
            if (nx == destX_ && ny == destY_) continue;     // never step onto the player
            if (!walkable(nx, ny)) continue;
            n.destX = nx; n.destY = ny; n.moving = true;
            n.dir = dirFromDelta(nx - n.x, ny - n.y);
            return true;
        }
        return false;
    };
    auto faceTile = [&](int gx, int gy) { n.dir = faceDir(gx - n.x, gy - n.y); };
    auto wanderStep = [&]() {
        int r = std::rand() % 4;
        int ddx = (r == 2) - (r == 1), ddy = (r == 0) - (r == 3);
        int nx = n.x + ddx, ny = n.y + ddy;
        if (walkable(nx, ny) && !(nx == destX_ && ny == destY_)) {
            n.destX = nx; n.destY = ny; n.moving = true; n.dir = dirFromDelta(ddx, ddy);
        }
    };
    auto patrolStep = [&]() {
        int dx = (n.patrolDir == 1) ? -1 : (n.patrolDir == 2) ? 1 : 0;
        int dy = (n.patrolDir == 0) ? 1 : (n.patrolDir == 3) ? -1 : 0;
        int nx = n.x + dx, ny = n.y + dy;
        if (walkable(nx, ny) && !(nx == destX_ && ny == destY_)) {
            n.destX = nx; n.destY = ny; n.moving = true; n.dir = n.patrolDir;
        } else {
            // hit a wall / edge — reverse direction next tick
            n.patrolDir = n.patrolDir == 0 ? 3 : n.patrolDir == 3 ? 0
                        : n.patrolDir == 1 ? 2 : 1;
        }
    };

    // ---- Find the nearest foe an ally would engage (monster or enemy NPC) ----
    auto nearestFoe = [&](int& fx, int& fy) -> bool {
        int best = 1 << 30; bool found = false;
        for (auto& m : monsters_) {
            if (!m.alive()) continue;
            int d = chebyshev(m.x, m.y, n.x, n.y);
            if (d <= kHelp && d < best) { best = d; fx = m.x; fy = m.y; found = true; }
        }
        for (auto& o : npcs_) {
            if (o.faction != NpcFaction::Enemy || !o.alive()) continue;
            int d = chebyshev(o.x, o.y, n.x, n.y);
            if (d <= kHelp && d < best) { best = d; fx = o.x; fy = o.y; found = true; }
        }
        return found;
    };

    switch (n.faction) {
    case NpcFaction::Enemy: {
        bool aggro = (n.behavior == NpcBehavior::Chase) || (cheb <= kAggro);
        if (aggro && cheb > 1) { stepToward(destX_, destY_, false); return; }
        if (cheb <= 1) { faceTile(destX_, destY_); return; }     // adjacent: hold & attack
        // idle when no target in sight, otherwise roam per behaviour
        if (n.behavior == NpcBehavior::Wander) wanderStep();
        else if (n.behavior == NpcBehavior::Patrol) patrolStep();
        break;
    }
    case NpcFaction::Ally: {
        int fx, fy;
        if (nearestFoe(fx, fy)) {                                 // engage the foe
            int fd = chebyshev(fx, fy, n.x, n.y);
            if (fd > 1) stepToward(fx, fy, false);
            else faceTile(fx, fy);
            return;
        }
        // no foe: companion behaviours
        if (n.behavior == NpcBehavior::Wander) { wanderStep(); break; }
        if (n.behavior == NpcBehavior::Patrol) { patrolStep(); break; }
        if (n.behavior == NpcBehavior::Idle) { if (cheb == 1) faceTile(destX_, destY_); break; }
        // Chase/Flee for an ally both mean "stay near the player" (follow)
        if (cheb > 2) stepToward(destX_, destY_, false);
        else if (cheb == 1) faceTile(destX_, destY_);
        break;
    }
    case NpcFaction::Neutral:
    default: {
        if (n.behavior == NpcBehavior::Flee) {
            if (cheb <= 4) { stepToward(destX_, destY_, true); return; }
            if (cheb == 1) faceTile(destX_, destY_);
            break;
        }
        if (cheb == 1) { faceTile(destX_, destY_); break; }      // face the player to talk
        if (n.behavior == NpcBehavior::Wander) wanderStep();
        else if (n.behavior == NpcBehavior::Patrol) patrolStep();
        else if (n.behavior == NpcBehavior::Chase && cheb > 2) stepToward(destX_, destY_, false);
        break;
    }
    }
}

void GamePlay::updateNpcs(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int pdef = gs.party.empty() ? 0 : gs.party[0].totalDef(db);

    for (auto& n : npcs_) {
        if (n.hurtFlash > 0) n.hurtFlash -= dt;
        if (n.atkCd > 0)     n.atkCd -= dt;

        if (n.moving) {
            float tx = n.destX*(float)TS, ty = n.destY*(float)TS;
            float dx = tx-n.px, dy = ty-n.py, dist = std::sqrt(dx*dx+dy*dy);
            float spd = n.faction == NpcFaction::Neutral ? 2.0f : 2.8f;  // combatants hustle
            float step = TS*spd*dt;
            if (dist <= step) { n.px=tx; n.py=ty; n.x=n.destX; n.y=n.destY; n.moving=false; }
            else { n.px += dx/dist*step; n.py += dy/dist*step; }
            n.animTime += dt; if (n.animTime>0.16f){ n.animTime=0; n.frame=(n.frame+1)%4; }
        } else {
            n.frame = 0;
            n.moveCd -= dt;
            if (n.moveCd <= 0) {
                n.moveCd = 0.5f + (std::rand()%120)/100.0f;
                npcDecide(n, dt);
            } else {
                // still face the player when adjacent even between move ticks
                if (chebyshev(n.x, n.y, destX_, destY_) == 1 && n.faction != NpcFaction::Enemy)
                    n.dir = faceDir(destX_ - n.x, destY_ - n.y);
            }
        }

        // ---- combat: act on adjacency every frame (independent of move ticks) ----
        if (n.combatant() && n.atkCd <= 0) {
            if (n.faction == NpcFaction::Enemy) {
                // hit the player if adjacent...
                if (chebyshev(n.x, n.y, destX_, destY_) <= 1 && !gs.party.empty()) {
                    n.atkCd = 1.1f;
                    int dmg = std::max(1, n.atk - pdef);
                    PartyMember& hero = gs.party[0];
                    hero.hp = std::max(0, hero.hp - dmg);
                    playerHurt_ = 0.22f;
                    spawnPopup(pxX_, pxY_, "-" + std::to_string(dmg), Color{ 255, 110, 110, 255 });
                    engine_.audio().playSfx("hurt", 0.7f);
                    if (gs.partyWiped()) {
                        const CharacterDef* cd = customChar();
                        if (cd && !cd->motions[MO_Death].frames.empty()) {
                            triggerMotion(MO_Death);
                            dyingTimer_ = motionTimer_ > 0 ? motionTimer_ : 0.8f;
                        } else phase_ = Phase::GameOver;
                        return;
                    }
                } else {
                    // ...otherwise strike an adjacent ally
                    for (auto& a : npcs_) {
                        if (a.faction != NpcFaction::Ally || !a.alive()) continue;
                        if (chebyshev(a.x, a.y, n.x, n.y) > 1) continue;
                        n.atkCd = 1.1f; damageNpc(a, std::max(1, n.atk - a.def)); break;
                    }
                }
            } else if (n.faction == NpcFaction::Ally) {
                // allies strike an adjacent monster or enemy NPC
                bool struck = false;
                for (auto& m : monsters_) {
                    if (!m.alive()) continue;
                    if (chebyshev(m.x, m.y, n.x, n.y) > 1) continue;
                    n.atkCd = 1.0f; damageMonster(m, std::max(1, n.atk - m.def)); struck = true; break;
                }
                if (!struck) for (auto& o : npcs_) {
                    if (o.faction != NpcFaction::Enemy || !o.alive()) continue;
                    if (chebyshev(o.x, o.y, n.x, n.y) > 1) continue;
                    n.atkCd = 1.0f; damageNpc(o, std::max(1, n.atk - o.def)); break;
                }
            }
        }
    }

    reapDead();   // clear out monsters an ally killed, and any dead combatant NPCs
}

void GamePlay::drawNpcs() {
    const Database& db = engine_.project().database;
    for (auto& n : npcs_) {
        Color tint = WHITE;
        if (n.hurtFlash > 0)                         tint = Color{ 255, 120, 120, 255 };
        else if (n.faction == NpcFaction::Enemy)     tint = Color{ 255, 200, 200, 255 };
        else if (n.faction == NpcFaction::Ally)      tint = Color{ 205, 235, 255, 255 };
        float wS = std::max(1, n.drawTilesW) * n.drawPct / 100.0f;
        float hS = std::max(1, n.drawTilesH) * n.drawPct / 100.0f;
        const CharacterDef* cd = nullptr;
        if (n.charId >= 0) { cd = db.character(n.charId); if (!cd) cd = db.mob(n.charId); }
        if (cd) {   // 등록된 캐릭터: 방향별(상하좌우) 프레임으로 렌더 (몹과 동일 경로)
            const auto& fr = cd->motions[MO_Walk].dirFrames(n.dir);
            int asset = fr.empty() ? -1 : fr[(n.moving ? n.frame : 0) % (int)fr.size()];
            drawCharacter(asset, n.dir, 0, n.px, n.py, tint, 1, wS, hS);
        } else {    // 구버전: 단일 4방향 시트
            drawCharacter(n.spriteAsset, n.dir, n.moving ? n.frame : 0, n.px, n.py, tint, 4, wS, hS);
        }
        // faction tag dot + HP bar for combatants
        if (n.combatant()) {
            int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
            if (n.hp < n.maxHp) {
                float w = TS, ratio = (float)n.hp / n.maxHp;
                Color bar = n.faction == NpcFaction::Enemy ? Color{220,80,80,255}
                                                           : Color{90,200,120,255};
                DrawRectangle((int)n.px, (int)n.py - 7, (int)w, 4, Fade(BLACK, 0.6f));
                DrawRectangle((int)n.px, (int)n.py - 7, (int)(w * ratio), 4, bar);
            }
        }
    }
}

void GamePlay::runAutoruns() {
    if (!map_) return;
    GameState& gs = engine_.state();
    for (auto& e : map_->events) {
        if (e.trigger != TriggerType::Autorun) continue;
        if (!eventConditionMet(gs, e)) continue;
        long key = ((long)map_->id << 16) | (e.id & 0xffff);
        if (firedOnce_.count(key)) continue;        // autoruns fire once per session
        firedOnce_.insert(key);
        runEvent(e);
        break;                                       // one autorun at a time
    }
}

} // namespace tsukuru
