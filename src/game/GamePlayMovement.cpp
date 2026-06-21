// GamePlayMovement: per-frame field update loop + grid movement.
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

void GamePlay::updateField(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;

    static const bool autowalk = getenv("TSUKURU_AUTOWALK") != nullptr;

    // death sequence: play the death motion, then hand off to the GameOver screen
    if (dyingTimer_ > 0) {
        dyingTimer_ -= dt;
        updateMotion(dt);
        if (dyingTimer_ <= 0) phase_ = Phase::GameOver;
        return;
    }

    if (attackTimer_ > 0) attackTimer_ -= dt;
    for (float& c : skillCd_) if (c > 0) c -= dt;
    if (playerHurt_ > 0)  playerHurt_ -= dt;

    // slow MP regeneration so skills are sustainable
    if (!engine_.state().party.empty()) {
        mpRegen_ -= dt;
        if (mpRegen_ <= 0) {
            mpRegen_ = 1.5f;
            PartyMember& h = engine_.state().party[0];
            if (h.mp < h.maxMp) h.mp = std::min(h.maxMp, h.mp + 1);
        }
    }

    updateProjectiles(dt);
    updateFx(dt);

    // inventory (I) / equipment (C) windows freeze the field while open; they
    // handle their own clicks in draw(). Bottom HUD buttons toggle them too.
    if (IsKeyPressed(KEY_I)) { invOpen_ = !invOpen_; equipOpen_ = false; }
    if (IsKeyPressed(KEY_C)) { equipOpen_ = !equipOpen_; invOpen_ = false; }
    if (invOpen_ || equipOpen_) {
        if (IsKeyPressed(KEY_ESCAPE)) { invOpen_ = equipOpen_ = false; }
        return;
    }

    // hunger / thirst drain — 1 per second; at 0 the player slowly loses HP
    if (!engine_.state().party.empty()) {
        survivalAcc_ += dt;
        while (survivalAcc_ >= 1.0f) {
            survivalAcc_ -= 1.0f;
            PartyMember& h = engine_.state().party[0];
            if (h.hunger > 0) --h.hunger;
            if (h.thirst > 0) --h.thirst;
            if ((h.hunger == 0 || h.thirst == 0) && h.hp > 1) --h.hp;
        }
    }

    if (IsKeyPressed(KEY_ESCAPE)) { menu_->open(); phase_ = Phase::Menu; return; }

    // Skills: Z/Space slot0, X slot1, C slot2, V slot3, F slot4, G slot5.
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_LEFT_CONTROL))
        castSlot(0);
    if (IsKeyPressed(KEY_X)) castSlot(1);
    if (IsKeyPressed(KEY_V)) castSlot(3);   // (C는 장비창 단축키로 사용 — 스킬은 패널 클릭)
    if (IsKeyPressed(KEY_F)) castSlot(4);
    if (IsKeyPressed(KEY_G)) castSlot(5);
    handleSkillClicks();                 // touch / mouse click on the skill panel
    if (IsKeyPressed(KEY_ENTER)) interact();

    // Debug autopilot: chase and attack the nearest monster (verifies combat).
    Direction autoDir = Direction::Right; bool autoMove = false;
    if (autowalk) {
        FieldMonster* near = nullptr; int best = 1000000;
        for (auto& m : monsters_) {
            if (!m.alive()) continue;
            int d = std::abs(m.x - destX_) + std::abs(m.y - destY_);
            if (d < best) { best = d; near = &m; }
        }
        if (near) {
            int ddx = near->x - destX_, ddy = near->y - destY_;
            int cheb = std::max(std::abs(ddx), std::abs(ddy));
            if (std::abs(ddx) >= std::abs(ddy) && ddx != 0)
                autoDir = ddx > 0 ? Direction::Right : Direction::Left;
            else if (ddy != 0)
                autoDir = ddy > 0 ? Direction::Down : Direction::Up;
            dir_ = (int)autoDir;
            autoMove = true;
            // attack any orthogonally-adjacent monster (face it first)
            for (auto& m : monsters_) {
                if (!m.alive()) continue;
                int adx = m.x - destX_, ady = m.y - destY_;
                if (std::abs(adx) + std::abs(ady) != 1) continue;
                dir_ = adx>0?(int)Direction::Right : adx<0?(int)Direction::Left
                     : ady>0?(int)Direction::Down  : (int)Direction::Up;
                castSlot(0);
                break;
            }
            (void)cheb;
        }
    }

    if (!moving_) {
        Direction d; bool press = true;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))         d = Direction::Up;
        else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  d = Direction::Down;
        else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  d = Direction::Left;
        else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) d = Direction::Right;
        else press = false;
        if (!press && autoMove) { d = autoDir; press = true; }
        if (press) {
            tryMove(d);
            // autowalk: if blocked, try other directions so it doesn't get stuck
            if (autowalk && !moving_) {
                Direction alts[4] = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
                for (Direction a : alts) { tryMove(a); if (moving_) break; }
            }
        }
    }

    if (moving_) {
        float tx = destX_ * (float)TS, ty = destY_ * (float)TS;
        // Walk speed scales with the player's 속도(spd) stat. spd=5 keeps the
        // baseline feel (TS*5); each point shifts it by TS*0.2 (clamped sane).
        const auto& gsParty = engine_.state().party;
        int spd = gsParty.empty() ? 5 : gsParty[0].spd;
        float speed = TS * std::clamp(4.0f + spd * 0.2f, 2.5f, 12.0f);
        float dx = tx - pxX_, dy = ty - pxY_;
        float dist = std::sqrt(dx*dx + dy*dy);
        float step = speed * dt;
        if (dist <= step) {
            pxX_ = tx; pxY_ = ty; moving_ = false;
            engine_.state().playerX = destX_;
            engine_.state().playerY = destY_;
            if (autowalk) TraceLog(LOG_INFO, "AUTOWALK arrived at tile (%d,%d)", destX_, destY_);
            // stepped onto a middle-edge gate while heading that way -> change zone
            Direction zd;
            if (zoneEdgeDir(destX_, destY_, zd) && (int)zd == dir_) {
                Vec2i zl = dirToDelta(zd);
                if (engine_.project().mapAtWorld(map_->worldX + zl.x, map_->worldY + zl.y)) {
                    tryZoneTransition(zd);
                    return;
                }
            }
            if (Event* e = map_->eventAt(destX_, destY_))
                if (e->trigger == TriggerType::PlayerTouch) runEvent(*e);
        } else {
            pxX_ += dx / dist * step;
            pxY_ += dy / dist * step;
        }
        animTime_ += dt;
        int pframes = std::max(1, engine_.project().playerFrames);
        if (animTime_ > 0.12f) { animTime_ = 0; frame_ = (frame_ + 1) % pframes; }
    } else {
        frame_ = 0;
    }
    engine_.state().playerDir = dir_;

    worldTime_ += dt;
    updateMotion(dt);           // advance custom-character motion frames
    updateMonsters(dt);
    updateNpcs(dt);
}

void GamePlay::tryMove(Direction d) {
    dir_ = (int)d;
    Vec2i delta = dirToDelta(d);
    int nx = destX_ + delta.x, ny = destY_ + delta.y;
    if (!map_->tilemap.inBounds(nx, ny)) return;      // map edge blocks (gates are inside)
    if (map_->tilemap.blocked(nx, ny)) return;
    if (monsterAt(nx, ny)) return;                    // can't walk through monsters
    if (npcAt(nx, ny)) return;                        // ...or NPCs
    destX_ = nx; destY_ = ny; moving_ = true;
}

// Zone gate = the middle 7 tiles of an edge (N/S/E/W). Returns the outward
// direction of that gate, or false if (x,y) is not a gate tile.
bool GamePlay::zoneEdgeDir(int x, int y, Direction& out) const {
    if (!map_) return false;
    int W = map_->tilemap.width(), H = map_->tilemap.height();
    auto inBand = [](int v, int dim) {                // central 7-tile band of an axis
        if (dim <= 7) return true;
        int c = dim / 2;
        return v >= c - 3 && v <= c + 3;
    };
    if (x == 0      && inBand(y, H)) { out = Direction::Left;  return true; }
    if (x == W - 1  && inBand(y, H)) { out = Direction::Right; return true; }
    if (y == 0      && inBand(x, W)) { out = Direction::Up;    return true; }
    if (y == H - 1  && inBand(x, W)) { out = Direction::Down;  return true; }
    return false;
}

// Make the middle-7 edge gates passable wherever a placed neighbour exists, so
// the player can always reach and cross them (maps may have walled borders).
void GamePlay::carveZoneGates() {
    if (!map_ || !map_->placed) return;
    Project& proj = engine_.project();
    int W = map_->tilemap.width(), H = map_->tilemap.height();
    int cY = H / 2, cX = W / 2;
    auto open = [&](int x, int y) { if (map_->tilemap.inBounds(x, y)) map_->tilemap.setBlocked(x, y, false); };
    bool any = false;
    if (proj.mapAtWorld(map_->worldX - 1, map_->worldY)) { for (int dy=-3; dy<=3; ++dy) open(0,   cY+dy); any=true; }
    if (proj.mapAtWorld(map_->worldX + 1, map_->worldY)) { for (int dy=-3; dy<=3; ++dy) open(W-1, cY+dy); any=true; }
    if (proj.mapAtWorld(map_->worldX, map_->worldY - 1)) { for (int dx=-3; dx<=3; ++dx) open(cX+dx, 0);   any=true; }
    if (proj.mapAtWorld(map_->worldX, map_->worldY + 1)) { for (int dx=-3; dx<=3; ++dx) open(cX+dx, H-1); any=true; }
    (void)any;
}

// Cross a middle-edge gate into the orthogonally-adjacent placed map. The player
// enters one tile INSIDE the matching edge (so they can step back out to return),
// aligned to the same band offset. No neighbour → no-op.
void GamePlay::tryZoneTransition(Direction d) {
    if (!map_ || !map_->placed) return;
    Vec2i delta = dirToDelta(d);
    auto nb = engine_.project().mapAtWorld(map_->worldX + delta.x, map_->worldY + delta.y);
    if (!nb) return;
    GameState& gs = engine_.state();
    int oldW = map_->tilemap.width(), oldH = map_->tilemap.height();
    int curX = destX_, curY = destY_;
    loadMap(nb->id);
    int W = map_->tilemap.width(), H = map_->tilemap.height();
    // keep the crossing offset relative to each edge's centre
    int offY = curY - oldH / 2, offX = curX - oldW / 2;
    int ex, ey;
    if      (delta.x > 0) { ex = (W > 1 ? 1 : 0);     ey = std::clamp(H/2 + offY, 0, H-1); } // entered west side
    else if (delta.x < 0) { ex = (W > 1 ? W - 2 : 0); ey = std::clamp(H/2 + offY, 0, H-1); } // entered east side
    else if (delta.y > 0) { ey = (H > 1 ? 1 : 0);     ex = std::clamp(W/2 + offX, 0, W-1); } // entered north side
    else                  { ey = (H > 1 ? H - 2 : 0); ex = std::clamp(W/2 + offX, 0, W-1); } // entered south side
    // nudge to a walkable tile if the computed spot is blocked
    if (map_->tilemap.blocked(ex, ey)) {
        for (int r = 1; r <= 4; ++r) {
            bool horiz = (delta.x != 0);
            int a = horiz ? ey : ex;
            for (int s : { a + r, a - r }) {
                int tx = horiz ? ex : std::clamp(s, 0, W-1);
                int ty = horiz ? std::clamp(s, 0, H-1) : ey;
                if (!map_->tilemap.blocked(tx, ty)) { ex = tx; ey = ty; r = 99; break; }
            }
        }
    }
    int TS = map_->tileset.tileWidth;
    destX_ = ex; destY_ = ey;
    pxX_ = ex * (float)TS; pxY_ = ey * (float)TS;
    moving_ = false; attackTimer_ = 0;
    gs.playerX = ex; gs.playerY = ey;
    spawnMonsters();
    runAutoruns();
}

// ----------------------------- tile passability -----------------------------
bool GamePlay::walkable(int x, int y) {
    if (!map_ || !map_->tilemap.inBounds(x, y)) return false;
    if (map_->tilemap.blocked(x, y)) return false;
    if (monsterAt(x, y)) return false;
    if (npcAt(x, y)) return false;
    return true;
}

} // namespace tsukuru
