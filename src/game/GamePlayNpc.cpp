// GamePlayNpc: map NPC instances, wandering, autorun cutscenes.
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
    for (auto& e : map_->events) {
        if (e.graphicAsset < 0) continue;           // only events with a sprite are NPCs
        NpcInst n;
        n.eventId = e.id; n.spriteAsset = e.graphicAsset; n.wander = e.wander;
        n.x = n.destX = e.x; n.y = n.destY = e.y;
        n.px = e.x * (float)TS; n.py = e.y * (float)TS;
        n.moveCd = 0.6f + (std::rand() % 100) / 80.0f;
        npcs_.push_back(n);
    }
}

NpcInst* GamePlay::npcAt(int x, int y) {
    for (auto& n : npcs_) if (n.x == x && n.y == y) return &n;
    return nullptr;
}

void GamePlay::updateNpcs(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    for (auto& n : npcs_) {
        if (n.moving) {
            float tx = n.destX*(float)TS, ty = n.destY*(float)TS;
            float dx = tx-n.px, dy = ty-n.py, dist = std::sqrt(dx*dx+dy*dy), step = TS*2.2f*dt;
            if (dist <= step) { n.px=tx; n.py=ty; n.x=n.destX; n.y=n.destY; n.moving=false; }
            else { n.px += dx/dist*step; n.py += dy/dist*step; }
            n.animTime += dt; if (n.animTime>0.18f){ n.animTime=0; n.frame=(n.frame+1)%4; }
        } else {
            n.frame = 0;
            // face the player when adjacent
            int cheb = std::max(std::abs(n.x-destX_), std::abs(n.y-destY_));
            if (cheb == 1) {
                int dx=destX_-n.x, dy=destY_-n.y;
                n.dir = std::abs(dx)>=std::abs(dy) ? (dx>0?2:1) : (dy>0?0:3);
            } else if (n.wander) {
                n.moveCd -= dt;
                if (n.moveCd <= 0) {
                    n.moveCd = 1.0f + (std::rand()%150)/100.0f;
                    int r = std::rand()%4;          // pick a random direction
                    int ddx=0, ddy=0;
                    if (r==0) ddy=1; else if (r==1) ddx=-1; else if (r==2) ddx=1; else ddy=-1;
                    int nx=n.x+ddx, ny=n.y+ddy;
                    if (walkable(nx,ny) && !(nx==destX_&&ny==destY_)) {
                        n.destX=nx; n.destY=ny; n.moving=true;
                        n.dir = ddy>0?0: ddy<0?3: ddx<0?1:2;
                    }
                }
            }
        }
    }
}

void GamePlay::drawNpcs() {
    for (auto& n : npcs_)
        drawCharacter(n.spriteAsset, n.dir, n.moving ? n.frame : 0, n.px, n.py);
}

void GamePlay::runAutoruns() {
    if (!map_) return;
    GameState& gs = engine_.state();
    for (auto& e : map_->events) {
        if (e.trigger != TriggerType::Autorun) continue;
        if (e.conditionSwitch >= 0 && gs.getSwitch(e.conditionSwitch) != e.conditionValue) continue;
        long key = ((long)map_->id << 16) | (e.id & 0xffff);
        if (firedOnce_.count(key)) continue;        // autoruns fire once per session
        firedOnce_.insert(key);
        runEvent(e);
        break;                                       // one autorun at a time
    }
}

// ----------------------------- atmosphere -----------------------------

} // namespace tsukuru
