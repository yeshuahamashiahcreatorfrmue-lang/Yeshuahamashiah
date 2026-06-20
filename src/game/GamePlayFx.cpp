// GamePlayFx: combat projectile + visual-effect simulation and rendering.
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

void GamePlay::spawnFx(int type, float px, float py, int dir, int assetId, float dur, float radius, int loops) {
    SkillFx f; f.type = type; f.px = px; f.py = py; f.dir = dir;
    f.assetId = assetId; f.t = 0; f.radius = radius;
    f.loops = std::max(1, loops);
    // For an imported animated effect strip, play the whole strip at its own fps
    // for `loops` cycles so a 7-frame motion can repeat 1/3/7… times as authored.
    if (assetId >= 0) {
        const AssetEntry* ae = engine_.project().assets.find(assetId);
        if (ae && ae->frames > 1) {
            float fps = ae->fps > 0 ? (float)ae->fps : 12.0f;
            dur = ae->frames / fps * (float)f.loops;
        }
    }
    f.dur = dur;
    fx_.push_back(f);
}

void GamePlay::updateProjectiles(float dt) {
    if (!map_) { projectiles_.clear(); return; }
    int TS = map_->tileset.tileWidth;
    float speed = TS * 11.0f;
    for (auto& pr : projectiles_) {
        Vec2i d = dirToDelta((Direction)pr.dir);
        pr.px += d.x * speed * dt;
        pr.py += d.y * speed * dt;
        pr.life -= dt;
        int tx = (int)((pr.px + TS/2) / TS), ty = (int)((pr.py + TS/2) / TS);
        if (!map_->tilemap.inBounds(tx, ty) || map_->tilemap.blocked(tx, ty)) { pr.life = 0; continue; }
        if (FieldMonster* m = monsterAt(tx, ty)) {
            damageMonster(*m, pr.dmg);
            spawnFx(1, m->px, m->py, pr.dir, -1, 0.2f);
            engine_.audio().playSfx("hit", 0.8f);
            pr.life = 0;
        } else if (NpcInst* en = hostileNpcAt(tx, ty)) {
            damageNpc(*en, pr.dmg);
            spawnFx(1, en->px, en->py, pr.dir, -1, 0.2f);
            engine_.audio().playSfx("hit", 0.8f);
            pr.life = 0;
        }
    }
    reapDead();
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                    [](const Projectile& p){ return p.life <= 0; }), projectiles_.end());
}

void GamePlay::updateFx(float dt) {
    for (auto& f : fx_) f.t += dt;
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(),
              [](const SkillFx& f){ return f.t >= f.dur; }), fx_.end());
}

void GamePlay::drawProjectiles() {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    for (auto& pr : projectiles_) {
        float cx = pr.px + TS/2.0f, cy = pr.py + TS/2.0f;
        Vec2i d = dirToDelta((Direction)pr.dir);
        Color core = { 120, 200, 255, 255 };
        DrawCircle((int)cx, (int)cy, TS*0.18f, Fade(core, 0.95f));
        DrawCircle((int)(cx - d.x*6), (int)(cy - d.y*6), TS*0.12f, Fade(core, 0.5f)); // trail
        DrawCircleLines((int)cx, (int)cy, TS*0.22f, Fade(WHITE, 0.7f));
    }
}

// world-space skill effects (sprite sheet if assigned, else procedural)
void GamePlay::drawFx() {
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    for (auto& f : fx_) {
        float k = f.dur > 0 ? f.t / f.dur : 1.0f;          // 0..1 progress
        if (f.assetId >= 0) {
            const Texture2D& tex = engine_.assetTexture(f.assetId);
            const AssetEntry* ae = engine_.project().assets.find(f.assetId);
            int frames = (ae && ae->frames > 1) ? ae->frames : 1;  // single-row anim
            float fw = tex.width / (float)frames, fh = (float)tex.height;
            int fr;
            if (f.loops > 1 && frames > 1) {        // replay the strip `loops` times
                int total = frames * f.loops;
                fr = std::min(total - 1, (int)(k * total)) % frames;
            } else {
                fr = std::min(frames - 1, (int)(k * frames));
            }
            Rectangle src = { fr*fw, 0, fw, fh };
            float sz = (f.type == 3) ? f.radius*2 : TS*1.3f;
            Rectangle dst = { f.px + TS/2 - sz/2, f.py + TS/2 - sz/2, sz, sz };
            DrawTexturePro(tex, src, dst, {0,0}, 0, Fade(WHITE, 1.0f - k*0.3f));
            continue;
        }
        // procedural fallbacks
        float cx = f.px + TS/2.0f, cy = f.py + TS/2.0f;
        if (f.type == 0) {                                  // melee slash arc
            DrawRectangle((int)f.px, (int)f.py, TS, TS, Fade(Color{255,240,160,255}, 0.45f*(1-k)));
            DrawRectangleLinesEx({ f.px, f.py, (float)TS, (float)TS }, 2, Fade(WHITE, 0.8f*(1-k)));
        } else if (f.type == 1) {                           // bolt impact
            DrawCircle((int)cx, (int)cy, TS*0.5f*k, Fade(Color{150,210,255,255}, 0.6f*(1-k)));
        } else if (f.type == 2) {                           // dash trail
            DrawCircle((int)cx, (int)cy, TS*0.4f*(1-k), Fade(Color{180,220,255,255}, 0.5f*(1-k)));
        } else if (f.type == 3) {                           // AoE ring
            float r = f.radius * k;
            DrawCircleGradient((int)cx, (int)cy, r, Fade(Color{255,200,120,255}, 0.5f*(1-k)), Fade(Color{255,120,80,0},0));
            DrawCircleLines((int)cx, (int)cy, r, Fade(Color{255,230,160,255}, 0.9f*(1-k)));
            DrawCircleLines((int)cx, (int)cy, r*0.7f, Fade(WHITE, 0.7f*(1-k)));
        }
    }
}

} // namespace tsukuru
