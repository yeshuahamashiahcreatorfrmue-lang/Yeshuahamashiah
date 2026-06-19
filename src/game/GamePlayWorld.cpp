// GamePlayWorld: NPCs, autoruns, weather/minimap/lighting, field rendering.
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

// ----------------------------- NPCs -----------------------------
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
void GamePlay::drawWeather(float dt) {
    if (!map_ || map_->weather == 0) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    bool rain = map_->weather == 1;
    int target = rain ? 220 : 120;
    if ((int)weatherP_.capacity() < target) weatherP_.reserve(target); // avoid realloc churn
    while ((int)weatherP_.size() < target) {
        Particle p; p.x = (float)(std::rand()%sw); p.y = (float)(std::rand()%sh);
        if (rain) { p.vx=-120; p.vy=900; } else { p.vx=(float)(std::rand()%40-20); p.vy=70; }
        p.life=1; weatherP_.push_back(p);
    }
    for (auto& p : weatherP_) {
        p.x += p.vx*dt; p.y += p.vy*dt;
        if (p.y > sh) { p.y = -5; p.x = (float)(std::rand()%sw); }
        if (p.x < 0) p.x = (float)sw;
        if (rain) DrawLine((int)p.x,(int)p.y,(int)(p.x+3),(int)(p.y+12), Fade(Color{160,190,230,255},0.5f));
        else      DrawCircle((int)p.x,(int)p.y,2, Fade(WHITE,0.7f));
    }
    if (rain) DrawRectangle(0,0,sw,sh, Fade(Color{40,50,80,255},0.12f));
}

void GamePlay::drawMinimap() {
    if (!map_) return;
    int w = map_->tilemap.width(), h = map_->tilemap.height();

    // Build the terrain layer once per map into a cached texture (1px per tile),
    // instead of issuing ~w*h rectangle draw calls every frame.
    if (!minimapValid_ || minimapW_ != w || minimapH_ != h) {
        if (minimapValid_) UnloadTexture(minimapTex_);
        Image img = GenImageColor(w, h, Color{ 46, 62, 46, 255 });
        for (int y=0;y<h;y++) for (int x=0;x<w;x++) {
            Color c{60,90,60,255}; bool any=false;
            for (int l=0;l<kLayerCount;l++){ int t=map_->tilemap.tile(l,x,y); if(t>=0){any=true;
                if(t==6||t==7||t==8||t==9) c=Color{70,110,190,255};
                else if(t==3||t==4) c=Color{170,150,110,255};
                else if(t>=16&&t<=24) c=Color{150,80,70,255};
                else if(t==32||t==33||t==12) c=Color{50,100,50,255}; } }
            if (map_->tilemap.blocked(x,y) && !any) c=Color{40,40,48,255};
            ImageDrawPixel(&img, x, y, c);
        }
        minimapTex_ = LoadTextureFromImage(img);
        UnloadImage(img);
        minimapW_ = w; minimapH_ = h; minimapValid_ = true;
    }

    int mmW = 132, mmH = 100;
    float s = std::min((float)mmW/w, (float)mmH/h);
    int ox = GetScreenWidth() - (int)(w*s) - 12, oy = 40;
    DrawRectangle(ox-3, oy-3, (int)(w*s)+6, (int)(h*s)+6, Fade(BLACK,0.55f));
    DrawTexturePro(minimapTex_, { 0,0,(float)w,(float)h },
                   { (float)ox,(float)oy,(float)w*s,(float)h*s }, {0,0}, 0, WHITE);
    for (auto& n : npcs_) DrawRectangle(ox+(int)(n.x*s), oy+(int)(n.y*s), 3,3, YELLOW);
    for (auto& mo : monsters_) DrawRectangle(ox+(int)(mo.x*s), oy+(int)(mo.y*s), 3,3, RED);
    DrawRectangle(ox+(int)(destX_*s)-1, oy+(int)(destY_*s)-1, 4,4, WHITE);
}

void GamePlay::visibleRange(int& x0,int& y0,int& x1,int& y1) const {
    int TS = map_->tileset.tileWidth;
    Vector2 tl = GetScreenToWorld2D({0,0}, cam_);
    Vector2 br = GetScreenToWorld2D({(float)GetScreenWidth(),(float)GetScreenHeight()}, cam_);
    x0 = std::max(0, (int)(tl.x/TS) - 1);  y0 = std::max(0, (int)(tl.y/TS) - 1);
    x1 = std::min(map_->tilemap.width()-1,  (int)(br.x/TS) + 1);
    y1 = std::min(map_->tilemap.height()-1, (int)(br.y/TS) + 1);
}

// ----------------------------- rendering helpers -----------------------------
void GamePlay::drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint, int frames) {
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    if (frames < 1) frames = 1;
    if (assetId >= 0) {
        const Texture2D& tex = engine_.assetTexture(assetId);
        float fw = tex.width / (float)frames, fh = tex.height / 4.0f;
        if (frame >= frames) frame %= frames;
        Rectangle src = { frame * fw, dir * fh, fw, fh };
        Rectangle dst = { px, py, (float)TS, (float)TS };
        DrawTexturePro(tex, src, dst, {0,0}, 0, tint);
    } else {
        DrawRectangle((int)px+6, (int)py+6, TS-12, TS-12, Color{ 80, 140, 220, 255 });
        DrawRectangleLines((int)px+6, (int)py+6, TS-12, TS-12, BLACK);
        int cx = (int)px + TS/2, cy = (int)py + TS/2;
        Vec2i d = dirToDelta((Direction)dir);
        DrawCircle(cx + d.x*6, cy + d.y*6, 3, WHITE);
    }
}

// ----------------------------- field rendering -----------------------------
void GamePlay::drawField() {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    int w = map_->tilemap.width(), h = map_->tilemap.height();

    // follow the player, but clamp so the view never shows past the map edges
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float halfW = sw / (2.0f * cam_.zoom), halfH = sh / (2.0f * cam_.zoom);
    float mapW = w * (float)TS, mapH = h * (float)TS;
    float tgx = pxX_ + TS/2.0f, tgy = pxY_ + TS/2.0f;
    if (mapW > 2*halfW) tgx = std::min(std::max(tgx, halfW), mapW - halfW); else tgx = mapW/2;
    if (mapH > 2*halfH) tgy = std::min(std::max(tgy, halfH), mapH - halfH); else tgy = mapH/2;
    cam_.target = { tgx, tgy };
    cam_.offset = { sw/2.0f, sh/2.0f };

    const Texture2D& ts = engine_.assetTexture(map_->tileset.assetId);
    const Tileset& set = map_->tileset;

    // animated tiles: cycle base id <-> id+1
    int phase = (int)(GetTime() * 2.5) % 2;

    int vx0, vy0, vx1, vy1; visibleRange(vx0, vy0, vx1, vy1); // cull to viewport
    BeginMode2D(cam_);
    auto drawLayer = [&](int layer) {
        for (int y = vy0; y <= vy1; ++y)
            for (int x = vx0; x <= vx1; ++x) {
                int t = map_->tilemap.tile(layer, x, y);
                if (t < 0) continue;
                if (phase == 1)
                    for (int a : map_->animTiles) if (a == t) { t = t + 1; break; }
                int sx, sy; set.srcOf(t, sx, sy);
                Rectangle src = { (float)sx, (float)sy, (float)set.tileWidth, (float)set.tileHeight };
                Rectangle dst = { (float)x*TS, (float)y*TS, (float)TS, (float)TS };
                DrawTexturePro(ts, src, dst, {0,0}, 0, WHITE);
            }
    };
    // Ground + decoration layers render below entities; the top layer is an
    // "overhead" layer drawn above them so the player can walk behind treetops/roofs.
    for (int layer = 0; layer < kLayerCount - 1; ++layer) drawLayer(layer);

    for (auto& e : map_->events)               // teleport markers (NPCs drawn separately)
        if (e.graphicAsset < 0 && e.type == EventType::Teleport)
            DrawRectangleLines(e.x*TS+2, e.y*TS+2, TS-4, TS-4, Fade(ui::kAccent, 0.5f));

    drawNpcs();
    drawMonsters();

    // player (red flash when hurt)
    Color ptint = playerHurt_ > 0 ? Color{ 255, 130, 130, 255 } : WHITE;
    drawCharacter(engine_.project().playerSprite, dir_, moving_ ? frame_ : 0, pxX_, pxY_, ptint,
                  std::max(1, engine_.project().playerFrames));

    drawProjectiles();
    drawFx();
    // overhead layer (treetops, roof edges) on top of the player
    drawLayer(kLayerCount - 1);
    EndMode2D();

    // darkness + torch-light (cave / night atmosphere)
    if (map_->darkness > 0) {
        unsigned char a = (unsigned char)std::min(245, map_->darkness);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{ 6, 8, 16, a });
        Vector2 ps = GetWorldToScreen2D({ pxX_ + TS/2.0f, pxY_ + TS/2.0f }, cam_);
        BeginBlendMode(BLEND_ADDITIVE);
        float R = TS * 5.0f;
        DrawCircleGradient((int)ps.x, (int)ps.y, R, Color{ 255, 220, 150, 150 }, Color{ 0,0,0,0 });
        DrawCircleGradient((int)ps.x, (int)ps.y, R*0.5f, Color{ 255, 230, 180, 120 }, Color{ 0,0,0,0 });
        EndBlendMode();
    }

    // day/night ambient cycle (outdoor maps)
    if (map_->dayNight) {
        float t = fmodf(worldTime_, 120.0f) / 120.0f;        // full cycle every 2 min
        float night = 0.5f - 0.5f * cosf(t * 2.0f * PI);     // 0 noon -> 1 midnight
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{ 20, 24, 64, (unsigned char)(night * 150) });
    }

    // weather particles
    drawWeather(GetFrameTime());

    // minimap
    drawMinimap();

    // HUD
    GameState& gs = engine_.state();
    DrawRectangle(0, 0, GetScreenWidth(), 32, Fade(BLACK, 0.55f));
    if (!gs.party.empty()) {
        PartyMember& m = gs.party[0];
        DrawTextU(TextFormat("Lv %d   HP %d/%d   MP %d/%d   EXP %d   Gold %d",
                 m.level, m.hp, m.maxHp, m.mp, m.maxMp, m.exp, gs.inventory.gold),
                 12, 8, 16, ui::kText);
    }
    if (!gs.objective.empty()) {
        DrawRectangle(0, 32, MeasureTextU(gs.objective.c_str(), 16) + 110, 26, Fade(BLACK, 0.45f));
        DrawTextU(TextFormat("목표: %s", gs.objective.c_str()), 12, 36, 16, ui::kAccentHi);
    }
    DrawTextU("Z:공격 X:원거리 C:회피 V:궁극기  방향키/WASD:이동  Enter:대화  ESC:메뉴  F2:에디터",
             12, GetScreenHeight() - 24, 15, Fade(ui::kText, 0.7f));

    drawSkillPanel();

    if (toastTimer_ > 0) {
        int tw = MeasureTextU(toast_.c_str(), 18);
        DrawRectangle(GetScreenWidth()/2 - tw/2 - 10, 40, tw + 20, 30, Fade(ui::kAccent, 0.9f));
        DrawTextU(toast_.c_str(), GetScreenWidth()/2 - tw/2, 46, 18, BLACK);
    }
}


} // namespace tsukuru
