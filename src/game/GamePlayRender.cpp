// GamePlayRender: weather, minimap, lighting, culling, field rendering.
#include "game/GamePlay.h"
#include "core/Engine.h"
#include "game/Menu.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <set>
#include <unordered_set>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace tsukuru {

// ----------------------------- custom-character motion -----------------------------
const CharacterDef* GamePlay::customChar() const {
    int id = engine_.project().playerCharId;
    if (id < 0) return nullptr;
    return engine_.project().database.character(id);
}

void GamePlay::triggerMotion(int motionId) {
    const CharacterDef* cd = customChar();
    if (!cd) return;
    playMotion_ = motionId; motionFrame_ = 0; motionAnim_ = 0;
    const MotionClip& clip = cd->motions[motionId];
    int n = (int)clip.frames.size();
    int fps = std::max(1, clip.fps);
    motionTimer_ = (motionId == MO_Walk) ? 0.0f : (n > 0 ? (float)n / fps : 0.25f);
}

void GamePlay::updateMotion(float dt) {
    const CharacterDef* cd = customChar();
    if (!cd) return;
    if (motionTimer_ > 0) {
        motionTimer_ -= dt;
        if (motionTimer_ <= 0 && playMotion_ != MO_Death) {
            playMotion_ = MO_Walk; motionFrame_ = 0; motionAnim_ = 0;
        }
    }
    const MotionClip& clip = cd->motions[playMotion_];
    int n = (int)clip.dirFrames(dir_).size();   // count of the CURRENT facing's frames
    if (n <= 0) return;
    if (playMotion_ == MO_Walk && !moving_) { motionFrame_ = 0; motionAnim_ = 0; return; }
    motionAnim_ += dt;
    float spf = 1.0f / std::max(1, clip.fps);
    while (motionAnim_ >= spf) {
        motionAnim_ -= spf;
        ++motionFrame_;
        if (clip.loop) motionFrame_ %= n;                   // looping motion cycles
        else if (motionFrame_ >= n) motionFrame_ = n - 1;   // one-shot holds last frame
    }
}

int GamePlay::motionFrameAsset() const {
    const CharacterDef* cd = customChar();
    if (!cd) return -1;
    int m = playMotion_;
    if (cd->motions[m].dirFrames(dir_).empty()) m = MO_Walk;  // fall back to walk
    const std::vector<int>& fl = cd->motions[m].dirFrames(dir_);
    if (fl.empty()) return -1;                                // none -> use sheet sprite
    int fi = motionFrame_;
    if (fi < 0) fi = 0;
    if (fi >= (int)fl.size()) fi = (int)fl.size() - 1;
    return fl[fi];
}

void GamePlay::drawWeather(float dt) {
    if (!map_ || map_->weather == 0) return;
    int sw = screenW(), sh = screenH();
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
    int ox = screenW() - (int)(w*s) - 12, oy = 40;
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
    Vector2 br = GetScreenToWorld2D({(float)screenW(),(float)screenH()}, cam_);
    x0 = std::max(0, (int)(tl.x/TS) - 1);  y0 = std::max(0, (int)(tl.y/TS) - 1);
    x1 = std::min(map_->tilemap.width()-1,  (int)(br.x/TS) + 1);
    y1 = std::min(map_->tilemap.height()-1, (int)(br.y/TS) + 1);
}

// ----------------------------- rendering helpers -----------------------------
void GamePlay::drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint, int frames, float wScale, float hScale) {
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    if (frames < 1) frames = 1;
    if (assetId >= 0) {
        const Texture2D& tex = engine_.assetTexture(assetId);
        float fw = tex.width / (float)frames, fh = tex.height / 4.0f;
        if (frame >= frames) frame %= frames;
        Rectangle src = { frame * fw, dir * fh, fw, fh };
        // The sprite fills its tile FOOTPRINT (wScale×hScale tiles), centred
        // horizontally on the base column and standing on the base row — so a
        // 2×3칸 character rises up/out from its tile. 1×1 == exact TS×TS fill.
        float sw = TS * wScale, sh = TS * hScale;
        Rectangle dst = { px + (TS - sw) / 2.0f, py + (TS - sh), sw, sh };
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
    float sw = (float)screenW(), sh = (float)screenH();
    float halfW = sw / (2.0f * cam_.zoom), halfH = sh / (2.0f * cam_.zoom);
    float mapW = w * (float)TS, mapH = h * (float)TS;
    float tgx = pxX_ + TS/2.0f, tgy = pxY_ + TS/2.0f;
    if (mapW > 2*halfW) tgx = std::min(std::max(tgx, halfW), mapW - halfW); else tgx = mapW/2;
    if (mapH > 2*halfH) tgy = std::min(std::max(tgy, halfH), mapH - halfH); else tgy = mapH/2;
    cam_.target = { tgx, tgy };
    cam_.offset = { sw/2.0f, sh/2.0f };

    const Texture2D& ts = engine_.assetTexture(map_->tileset.assetId);
    const Tileset& set = map_->tileset;

    // animated tiles: cycle base id <-> id+1 on the "+1" phase. The id set is
    // cached per-map (animTileSet_), so here we only flip a flag — no per-frame
    // allocation. `animOn` gates the O(1) membership test in the tile loop.
    int phase = (int)(GetTime() * 2.5) % 2;
    bool animOn = (phase == 1) && !animTileSet_.empty();

    int vx0, vy0, vx1, vy1; visibleRange(vx0, vy0, vx1, vy1); // cull to viewport
    uiBeginWorld(cam_);
    auto drawLayer = [&](int layer) {
        for (int y = vy0; y <= vy1; ++y)
            for (int x = vx0; x <= vx1; ++x) {
                int t = map_->tilemap.tile(layer, x, y);
                if (t < 0) continue;
                if (animOn && animTileSet_.count(t)) t = t + 1;
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

    // player: a custom CharacterDef motion flipbook, else the walk/attack sheet
    Color ptint = playerHurt_ > 0 ? Color{ 255, 130, 130, 255 } : WHITE;
    const Project& proj = engine_.project();
    int frameAsset = motionFrameAsset();
    if (frameAsset >= 0) {
        const Texture2D& ftex = engine_.assetTexture(frameAsset);
        const CharacterDef* pcd = customChar();
        float pct = (pcd ? pcd->drawPct : 125) / 100.0f;
        float wT  = pcd ? std::max(1, pcd->drawTilesW) : 1;
        float hT  = pcd ? std::max(1, pcd->drawTilesH) : 1;
        float sw = TS * wT * pct, sh = TS * hT * pct;
        Rectangle src = { 0, 0, (float)ftex.width, (float)ftex.height };   // each facing uses its own frames
        Rectangle dst = { pxX_ + (TS - sw)/2, pxY_ + (TS - sh), sw, sh };  // centred horiz, stand on base
        DrawTexturePro(ftex, src, dst, {0,0}, 0, ptint);
    } else {
        int walk = std::max(1, proj.playerFrames);
        int atk  = std::max(0, proj.playerAtkFrames);
        int total = walk + atk;
        int col;
        if (attackTimer_ > 0 && atk > 0) {
            float prog = 1.0f - attackTimer_ / 0.18f;            // 0..1 through the swing
            col = walk + std::min(atk - 1, std::max(0, (int)(prog * atk)));
        } else {
            col = moving_ ? frame_ : 0;
        }
        drawCharacter(proj.playerSprite, dir_, col, pxX_, pxY_, ptint, total);
    }

    // remote players (MMO): everyone standing in this same map/zone
    if (engine_.net().active() && map_) {
        const Database& rdb = proj.database;
        for (const NetPlayer& rp : engine_.net().remotesInMap(map_->id)) {
            int spr = proj.playerSprite, frames = std::max(1, proj.playerFrames);
            if (const CharacterDef* rc = rdb.character(rp.charId)) {
                const auto& fl = rc->motions[MO_Walk].dirFrames(rp.dir);
                if (!fl.empty()) { spr = fl[0]; frames = 1; }
            }
            drawCharacter(spr, rp.dir, 0, (float)rp.x*TS, (float)rp.y*TS, Color{180,255,180,255}, frames);
        }
    }

    drawProjectiles();
    drawFx();
    // overhead layer (treetops, roof edges) on top of the player
    drawLayer(kLayerCount - 1);
    uiEndWorld();

    // darkness + torch-light (cave / night atmosphere)
    if (map_->darkness > 0) {
        unsigned char a = (unsigned char)std::min(245, map_->darkness);
        DrawRectangle(0, 0, screenW(), screenH(), Color{ 6, 8, 16, a });
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
        DrawRectangle(0, 0, screenW(), screenH(),
                      Color{ 20, 24, 64, (unsigned char)(night * 150) });
    }

    // weather particles
    drawWeather(GetFrameTime());

    // minimap
    drawMinimap();

    // HUD
    GameState& gs = engine_.state();
    DrawRectangle(0, 0, screenW(), 32, Fade(BLACK, 0.55f));
    if (!gs.party.empty()) {
        PartyMember& m = gs.party[0];
        DrawTextU(TextFormat("Lv %d   체력 %d/%d   기력 %d/%d   EXP %d   Gold %d",
                 m.level, m.hp, m.maxHp, m.mp, m.maxMp, m.exp, gs.inventory.gold),
                 12, 8, 16, ui::kText);
        // 포만/수분 bars (top-right) — drain 1/sec, refilled by 식품
        auto bar = [&](int x, float frac, Color c, const char* lbl) {
            DrawRectangle(x, 9, 150, 14, Fade(BLACK, 0.55f));
            DrawRectangle(x, 9, (int)(150 * std::clamp(frac, 0.0f, 1.0f)), 14, c);
            DrawRectangleLines(x, 9, 150, 14, Fade(BLACK, 0.6f));
            DrawTextU(lbl, x + 4, 10, 12, BLACK);
        };
        int rx = screenW() - 320;
        bar(rx,       m.maxHunger ? (float)m.hunger/m.maxHunger : 0, Color{225,170,75,255},  TextFormat("포만 %d", m.hunger));
        bar(rx + 158, m.maxThirst ? (float)m.thirst/m.maxThirst : 0, Color{85,170,235,255},  TextFormat("수분 %d", m.thirst));
    }
    // active food buffs: a badge per buff on the left (clear of the right skill panel)
    {
        int by = 84;
        for (const auto& b : gs.buffs) {
            std::string t = TextFormat("%s ", b.name.c_str());
            if (b.atk) t += TextFormat("공+%d ", b.atk);
            if (b.def) t += TextFormat("방+%d ", b.def);
            if (b.spd) t += TextFormat("속+%d ", b.spd);
            t += TextFormat("(%.0f초)", b.remain);
            int tw = MeasureTextU(t.c_str(), 14);
            DrawRectangle(12, by, tw + 10, 18, Fade(Color{120,90,200,255}, 0.9f));
            DrawTextU(t.c_str(), 16, by + 2, 14, WHITE);
            by += 20;
        }
    }
    if (!gs.objective.empty()) {
        DrawRectangle(0, 32, MeasureTextU(gs.objective.c_str(), 16) + 110, 26, Fade(BLACK, 0.45f));
        DrawTextU(TextFormat("목표: %s", gs.objective.c_str()), 12, 36, 16, ui::kAccentHi);
    }
    if (engine_.net().active())
        DrawTextU(TextFormat("MMO %d/%d명 · %s", engine_.net().playerCount(), engine_.net().maxPlayers(),
                  engine_.net().status().c_str()), 12, 58, 14, ui::kGood);
    DrawTextU("Z/X/V:스킬  I:인벤토리  C:장비  방향키/WASD:이동  Enter:대화  ESC:메뉴",
             12, screenH() - 24, 14, Fade(ui::kText, 0.7f));
    // bottom buttons: inventory (I) / equipment (C)
    invBtn_   = { 12,  (float)screenH() - 58, 116, 28 };
    equipBtn_ = { 134, (float)screenH() - 58, 96,  28 };
    if (ui::button(invBtn_,   "인벤토리 (I)", invOpen_))   { invOpen_ = !invOpen_; equipOpen_ = false; }
    if (ui::button(equipBtn_, "장비 (C)",     equipOpen_)) { equipOpen_ = !equipOpen_; invOpen_ = false; }

    drawSkillPanel();

    if (toastTimer_ > 0) {
        int tw = MeasureTextU(toast_.c_str(), 18);
        DrawRectangle(screenW()/2 - tw/2 - 10, 40, tw + 20, 30, Fade(ui::kAccent, 0.9f));
        DrawTextU(toast_.c_str(), screenW()/2 - tw/2, 46, 18, BLACK);
    }
}



// ---- consume food / equip equipment from the inventory ----
void GamePlay::useOrEquipItem(int itemId) {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    const Item* it = db.item(itemId);
    if (!it || gs.party.empty()) return;
    if (it->kind == 2) {                                   // 장비: equip into its body slot
        if (gs.equipItem(db, itemId)) { toast_ = "장착: " + it->name; toastTimer_ = 1.5f; }
    } else {                                               // 식품/기타: consume
        if (gs.consumeFood(db, itemId)) {
            toast_ = (it->kind == 1 ? "먹음: " : "사용: ") + it->name;
            if (it->buffSecs > 0 && (it->bonusAtk || it->bonusDef || it->bonusSpd))
                toast_ += TextFormat(" (%d초 버프)", it->buffSecs);
            toastTimer_ = 1.5f;
        }
    }
}

void GamePlay::unequipSlot(int slot) {
    engine_.state().unequipSlot(engine_.project().database, slot);
}

// ---- I: rectangular inventory grid (식품 / 장비 / 기타) ----
void GamePlay::drawInventoryOverlay() {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Color{ 10, 11, 16, 240 });
    DrawTextU("인벤토리   (I 또는 ESC 로 닫기)", 20, 40, 20, ui::kAccent);
    DrawTextU(TextFormat("Gold: %d", gs.inventory.gold), sw - 160, 40 + 2, 16, Color{230,200,90,255});
    if (ui::button({ (float)sw - 56, (float)40, 46, 30 }, "X")) invOpen_ = false;

    const char* cats[4] = { "전체", "식품", "장비", "기타" };
    for (int i = 0; i < 4; ++i)
        if (ui::button({ 20.0f + i*86, (float)40 + 34, 80, 26 }, cats[i], invCat_ == i)) invCat_ = i;

    // grid
    float gx0 = 20, gy0 = 40 + 74, cell = 92, pad = 8;
    int cols = std::max(1, (int)((sw - 40) / (cell + pad)));
    int idx = 0;
    const Item* hovItem = nullptr;
    for (auto& pr : gs.inventory.list()) {
        const Item* it = db.item(pr.first);
        if (!it) continue;
        if (invCat_ == 1 && it->kind != 1) continue;
        if (invCat_ == 2 && it->kind != 2) continue;
        if (invCat_ == 3 && it->kind != 0) continue;
        int cx = idx % cols, cy = idx / cols;
        Rectangle r = { gx0 + cx*(cell+pad), gy0 + cy*(cell+pad), cell, cell };
        bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        if (hov) hovItem = it;
        DrawRectangleRec(r, hov ? Color{40,46,60,255} : Color{26,30,40,255});
        DrawRectangleLinesEx(r, 1, it->kind==1?Color{225,170,75,255}:it->kind==2?ui::kAccent:Fade(WHITE,0.3f));
        if (it->iconAsset >= 0) {
            const Texture2D& tx = engine_.assetTexture(it->iconAsset);
            DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {r.x+8,r.y+6,cell-16,cell-34}, {0,0},0,WHITE);
        }
        DrawTextU(it->name.c_str(), (int)r.x+6, (int)r.y+cell-24, 13, ui::kText);
        DrawTextU(TextFormat("x%d", pr.second), (int)r.x+cell-30, (int)r.y+6, 13, ui::kAccentHi);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) useOrEquipItem(pr.first);
        ++idx;
    }
    if (idx == 0) DrawTextU("이 분류에 아이템이 없습니다. (DB 탭의 아이템에서 식품/장비를 만들고 획득)",
                            (int)gx0, (int)gy0, 15, ui::kTextDim);
    // hovered item: name, effect summary, and description (설명)
    if (hovItem) {
        std::string eff;
        if (hovItem->kind == 2) {
            if (hovItem->bonusAtk) eff += TextFormat("공+%d ", hovItem->bonusAtk);
            if (hovItem->bonusDef) eff += TextFormat("방+%d ", hovItem->bonusDef);
            if (hovItem->bonusSpd) eff += TextFormat("속+%d ", hovItem->bonusSpd);
        } else {
            if (hovItem->satiety)   eff += TextFormat("포만+%d ", hovItem->satiety);
            if (hovItem->hydration) eff += TextFormat("수분+%d ", hovItem->hydration);
            if (hovItem->healHp)    eff += TextFormat("HP+%d ", hovItem->healHp);
            if (hovItem->healGp)    eff += TextFormat("GP+%d ", hovItem->healGp);
            if (hovItem->bonusAtk)  eff += TextFormat("공+%d ", hovItem->bonusAtk);
            if (hovItem->bonusDef)  eff += TextFormat("방+%d ", hovItem->bonusDef);
            if (hovItem->bonusSpd)  eff += TextFormat("속+%d ", hovItem->bonusSpd);
            if (hovItem->buffSecs && (hovItem->bonusAtk||hovItem->bonusDef||hovItem->bonusSpd))
                eff += TextFormat("(%d초) ", hovItem->buffSecs);
        }
        DrawTextU(TextFormat("%s   %s", hovItem->name.c_str(), eff.c_str()), 20, sh - 70, 16, ui::kAccentHi);
        if (!hovItem->description.empty())
            DrawTextU(hovItem->description.c_str(), 20, sh - 48, 14, ui::kText);
    }
    DrawTextU("아이템 클릭: 식품=먹기(포만/수분/HP/GP 회복) · 장비=장착",
              20, sh - 26, 14, ui::kTextDim);
}

// ---- C: body-part equipment window ----
void GamePlay::drawEquipOverlay() {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Color{ 10, 11, 16, 240 });
    DrawTextU("장비   (C 또는 ESC 로 닫기)", 20, 40, 20, ui::kAccent);
    if (ui::button({ (float)sw - 56, (float)40, 46, 30 }, "X")) equipOpen_ = false;

    // human silhouette on the left
    float cxm = 230, top = 40 + 70;
    DrawRectangleRounded({ cxm-26, top, 52, 52 }, 0.4f, 8, Color{60,66,82,255});     // head
    DrawRectangle((int)cxm-34, (int)top+58, 68, 96, Color{60,66,82,255});             // torso
    DrawRectangle((int)cxm-58, (int)top+58, 24, 90, Color{55,60,75,255});             // L arm
    DrawRectangle((int)cxm+34, (int)top+58, 24, 90, Color{55,60,75,255});             // R arm
    DrawRectangle((int)cxm-30, (int)top+156, 26, 96, Color{55,60,75,255});            // L leg
    DrawRectangle((int)cxm+4,  (int)top+156, 26, 96, Color{55,60,75,255});            // R leg

    // slots: {name, slotId, x, y}
    struct S { const char* n; int id; float x, y; };
    S slots[7] = {
        { "머리",   1, cxm - 24,  top - 4 },
        { "몸통",   2, cxm - 24,  top + 78 },
        { "손",     3, cxm + 64,  top + 70 },
        { "다리",   4, cxm - 24,  top + 178 },
        { "발",     5, cxm - 24,  top + 250 },
        { "무기",   6, cxm - 112, top + 70 },
        { "장신구", 7, cxm + 64,  top + 160 },
    };
    for (auto& s : slots) {
        Rectangle r = { s.x, s.y, 64, 64 };
        bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, hov ? Color{42,48,62,255} : Color{24,28,38,255});
        DrawRectangleLinesEx(r, 2, ui::kAccent);
        DrawTextU(s.n, (int)r.x + 2, (int)r.y - 16, 13, ui::kTextDim);
        auto e = gs.equipped.find(s.id);
        if (e != gs.equipped.end() && e->second >= 0) {
            const Item* it = db.item(e->second);
            if (it && it->iconAsset >= 0) {
                const Texture2D& tx = engine_.assetTexture(it->iconAsset);
                DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {r.x+4,r.y+4,56,56}, {0,0},0,WHITE);
            } else if (it) DrawTextU(it->name.c_str(), (int)r.x+4, (int)r.y+24, 11, ui::kText);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) unequipSlot(s.id);   // click=벗기
        }
    }

    // totals
    if (!gs.party.empty()) {
        PartyMember& m = gs.party[0];
        DrawTextU(TextFormat("공격 %d   방어 %d   이동 %d", m.atk, m.def, m.spd),
                  (int)cxm - 100, (int)top + 320, 16, ui::kAccentHi);
    }

    // right: equippable items in the inventory
    float lx = 480, ly = 40 + 60;
    DrawTextU("장착 가능한 장비 (클릭=장착)", (int)lx, (int)ly, 16, ui::kAccent); ly += 28;
    int shown = 0;
    for (auto& pr : gs.inventory.list()) {
        const Item* it = db.item(pr.first);
        if (!it || it->kind != 2) continue;
        Rectangle r = { lx, ly, 360, 30 };
        bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, hov ? Color{40,46,60,255} : Color{26,30,40,255});
        DrawRectangleLinesEx(r, 1, Fade(WHITE,0.25f));
        const char* bn[8] = { "-","머리","몸통","손","다리","발","무기","장신구" };
        DrawTextU(TextFormat("%s  [%s]  공%d 방%d 속%d  x%d", it->name.c_str(),
                  bn[(it->bodySlot>=0&&it->bodySlot<=7)?it->bodySlot:0],
                  it->bonusAtk, it->bonusDef, it->bonusSpd, pr.second),
                  (int)r.x+8, (int)r.y+7, 14, ui::kText);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) useOrEquipItem(pr.first);
        ly += 34; ++shown;
        if (ly > sh - 40) break;
    }
    if (shown == 0) DrawTextU("장비 아이템이 없습니다. (DB 탭에서 분류=장비로 만들고 획득)",
                              (int)lx, (int)ly, 14, ui::kTextDim);
}

} // namespace tsukuru
