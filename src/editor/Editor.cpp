#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "render/AssetGen.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace tsukuru {

static const float kToolbarH = 40;
static const float kPaletteW = 220;
static const int   kUndoLimit = 42;     // Ctrl+Z history depth

// Standardized world sizes (7 tiers), square, up to 420x420.
static const int kSizeTiers[7] = { 30, 60, 120, 180, 270, 360, 420 };
static const char* kSizeTierNames[7] = {
    "1단계 30x30", "2단계 60x60", "3단계 120x120", "4단계 180x180",
    "5단계 270x270", "6단계 360x360", "7단계 420x420 (최대)"
};
// nearest tier index for a given side length
static int sizeTierOf(int side) {
    int best = 0, bestd = 1<<30;
    for (int i = 0; i < 7; ++i) { int d = std::abs(kSizeTiers[i]-side); if (d < bestd){bestd=d;best=i;} }
    return best;
}

Editor::Editor(Engine& engine) : engine_(engine) {
    cam_.zoom = 1.5f;
    cam_.offset = { kPaletteW + 20, kToolbarH + 20 };
    auto m = activeMap();
    if (m) activeMapId_ = m->id;
    if (const char* t = getenv("TSUKURU_TAB")) { // debug: pick initial tab
        std::string s = t;
        if (s == "world") tab_ = Tab::World;     else if (s == "events") tab_ = Tab::Events;
        else if (s == "chars") tab_ = Tab::Chars; else if (s == "assets") tab_ = Tab::Assets;
        else if (s == "db") tab_ = Tab::Database; else if (s == "skills") tab_ = Tab::Skills;
    }
    if (const char* t = getenv("TSUKURU_TOOL")) { if (std::string(t) == "stamp") tool_ = Tool::Stamp; }
}

std::shared_ptr<Map> Editor::activeMap() {
    Project& p = engine_.project();
    if (activeMapId_ >= 0) { if (auto m = p.map(activeMapId_)) return m; }
    if (!p.maps.empty()) { activeMapId_ = p.maps.front()->id; return p.maps.front(); }
    return nullptr;
}

// ---- undo / redo (snapshots of the active map's tilemap) ----
void Editor::pushUndo() {
    auto m = activeMap();
    if (!m) return;
    if (undoMap_ != m->id) { undo_.clear(); redo_.clear(); undoMap_ = m->id; }
    undo_.push_back(m->tilemap.toJson().dump());
    if (undo_.size() > kUndoLimit) undo_.erase(undo_.begin());
    redo_.clear();
}
void Editor::doUndo() {
    auto m = activeMap();
    if (!m || undoMap_ != m->id || undo_.empty()) return;
    redo_.push_back(m->tilemap.toJson().dump());
    m->tilemap.fromJson(nlohmann::json::parse(undo_.back()));
    undo_.pop_back();
    setStatus("실행 취소");
}
void Editor::doRedo() {
    auto m = activeMap();
    if (!m || undoMap_ != m->id || redo_.empty()) return;
    undo_.push_back(m->tilemap.toJson().dump());
    m->tilemap.fromJson(nlohmann::json::parse(redo_.back()));
    redo_.pop_back();
    setStatus("다시 실행");
}

// ---- prefab stamps (multi-tile / multi-layer building blocks) ----
void Editor::stampPrefab(int ox, int oy) {
    auto m = activeMap();
    if (!m) return;
    const auto& list = prefabs();
    if (prefabSel_ < 0 || prefabSel_ >= (int)list.size()) return;
    pushUndo();
    for (const auto& c : list[prefabSel_].cells) {
        int x = ox + c.dx, y = oy + c.dy;
        if (!m->tilemap.inBounds(x, y)) continue;
        if (c.tile >= 0) m->tilemap.setTile(c.layer, x, y, c.tile);
        if (c.blocked)   m->tilemap.setBlocked(x, y, true);
    }
}

void Editor::drawPrefabPalette(Rectangle area) {
    ui::panel(area, ui::kPanel);
    ui::label("스탬프", (int)area.x + 10, (int)area.y + 8, 16, ui::kAccent);
    const auto& list = prefabs();
    float y = area.y + 32;
    for (int i = 0; i < (int)list.size(); ++i) {
        if (ui::button({ area.x + 10, y, area.width - 20, 24 }, list[i].name, prefabSel_ == i))
            prefabSel_ = i;
        y += 27;
    }
    ui::label("맵을 클릭해 배치하세요.", (int)area.x + 10, (int)(y + 6), 13, ui::kTextDim);
    ui::label("(나무/집/연못 등의", (int)area.x + 10, (int)(y + 24), 12, ui::kTextDim);
    ui::label(" 타일+충돌 묶음)", (int)area.x + 10, (int)(y + 40), 12, ui::kTextDim);
}

// ============================ update ============================
void Editor::update(float dt) {
    if (statusTimer_ > 0) statusTimer_ -= dt;

    // Global shortcuts
    bool typingNow = eventTextFocus_ || dbNameFocus_ >= 0 || mapNameFocus_ || skillNameFocus_;
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        engine_.project().save();
        setStatus("프로젝트 저장됨.");
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Z)) {
        if (IsKeyDown(KEY_LEFT_SHIFT)) doRedo(); else doUndo();
    }
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_Y)) doRedo();
    // Tool hotkeys while editing a map
    if (tab_ == Tab::Map && !typingNow && !IsKeyDown(KEY_LEFT_CONTROL)) {
        if (IsKeyPressed(KEY_B)) { tool_ = Tool::Pencil; collisionMode_ = false; }
        if (IsKeyPressed(KEY_E)) { tool_ = Tool::Erase;  collisionMode_ = false; }
        if (IsKeyPressed(KEY_G)) { tool_ = Tool::Fill;   collisionMode_ = false; }
        if (IsKeyPressed(KEY_R)) { tool_ = Tool::Rect;   collisionMode_ = false; }
        if (IsKeyPressed(KEY_T)) { tool_ = Tool::Stamp;  collisionMode_ = false; }
        if (IsKeyPressed(KEY_C)) collisionMode_ = !collisionMode_;
    }
    if (IsKeyPressed(KEY_F5)) { engine_.project().save(); engine_.startPlaytest(); return; }

    // Camera pan (arrow keys) & zoom (wheel) when not typing
    bool typing = eventTextFocus_ || dbNameFocus_ >= 0 || skillNameFocus_;
    if (!typing) {
        float panSpeed = 400 * dt / cam_.zoom;
        if (IsKeyDown(KEY_RIGHT)) cam_.target.x += panSpeed;
        if (IsKeyDown(KEY_LEFT))  cam_.target.x -= panSpeed;
        if (IsKeyDown(KEY_DOWN))  cam_.target.y += panSpeed;
        if (IsKeyDown(KEY_UP))    cam_.target.y -= panSpeed;
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && tab_ != Tab::Database) {
        cam_.zoom += wheel * 0.15f;
        if (cam_.zoom < 0.3f) cam_.zoom = 0.3f;
        if (cam_.zoom > 4.0f) cam_.zoom = 4.0f;
    }
    // Middle-drag pan
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 d = GetMouseDelta();
        cam_.target.x -= d.x / cam_.zoom;
        cam_.target.y -= d.y / cam_.zoom;
    }

    if (tab_ == Tab::Assets) handleAssetDrop();
}

// raylib-supported image / audio extensions (broadened so any common image —
// and animated GIFs — can be registered and used on the map).
static bool isImageExt(const std::string& e) {
    static const char* k[] = { ".png",".bmp",".tga",".jpg",".jpeg",".gif",".qoi",
        ".psd",".hdr",".dds",".ktx",".astc",".pkm",".pvr",".pic",".ppm",".pgm" };
    for (auto* s : k) if (e == s) return true;
    return false;
}
static bool isAudioExt(const std::string& e) {
    static const char* k[] = { ".wav",".ogg",".mp3",".flac",".qoa",".xm",".mod" };
    for (auto* s : k) if (e == s) return true;
    return false;
}

void Editor::handleAssetDrop() {
    if (!IsFileDropped()) return;
    FilePathList dropped = LoadDroppedFiles();
    Project& p = engine_.project();
    for (unsigned i = 0; i < dropped.count; ++i) {
        std::string path = dropped.paths[i];
        std::string ext = GetFileExtension(path.c_str() ? path.c_str() : "");
        for (auto& c : ext) c = (char)tolower(c);

        if (ext == ".gif") {                       // animated GIF -> sprite-sheet
            int frames = 1;
            Image anim = LoadImageAnim(path.c_str(), &frames);
            if (anim.data && frames > 1) {
                // raylib stores the `frames` consecutively in anim.data (the
                // Image height is one frame). Repack into a horizontal strip.
                int fw = anim.width, fh = anim.height;     // single-frame size
                int frameBytes = GetPixelDataSize(fw, fh, anim.format);
                Image strip = GenImageColor(fw * frames, fh, BLANK);
                ImageFormat(&strip, anim.format);
                for (int f = 0; f < frames; ++f) {
                    Image one = anim;                       // shallow view of frame f
                    one.data = (unsigned char*)anim.data + (size_t)f * frameBytes;
                    Rectangle src = { 0, 0, (float)fw, (float)fh };
                    Rectangle dst = { (float)(f*fw), 0, (float)fw, (float)fh };
                    ImageDraw(&strip, one, src, dst, WHITE);
                }
                fs::create_directories(fs::path(p.dir) / "assets");
                fs::path base = fs::path(path).stem();
                int n = 1; fs::path dest;
                do { dest = fs::path(p.dir)/"assets"/(base.string()+(n>1?("_"+std::to_string(n)):std::string())+".png"); n++; }
                while (fs::exists(dest));
                ExportImage(strip, dest.string().c_str());
                UnloadImage(strip); UnloadImage(anim);
                std::string rel = (fs::path("assets")/dest.filename()).generic_string();
                int id = p.assets.addExisting(AssetType::Image, base.string(), rel);
                p.assets.setAnim(id, frames, 12);
                setStatus(TextFormat("움짤 등록됨: %s (%d프레임)", base.string().c_str(), frames));
                continue;
            }
            if (anim.data) UnloadImage(anim);       // static gif -> fall through
        }

        AssetType type;
        if (isImageExt(ext))      type = AssetType::Image;
        else if (isAudioExt(ext)) type = AssetType::Audio;
        else continue;
        int id = p.assets.registerAsset(p.dir, path, type);
        if (id >= 0) setStatus("등록됨: " + std::string(GetFileName(path.c_str())));
    }
    UnloadDroppedFiles(dropped);
    p.save();
}

// ============================ draw ============================
void Editor::draw() {
    switch (tab_) {
        case Tab::World:    drawWorldTab();    break;
        case Tab::Map:      drawMapTab();      break;
        case Tab::Events:   drawEventsTab();   break;
        case Tab::Chars:    drawCharsTab();    break;
        case Tab::Assets:   drawAssetsTab();   break;
        case Tab::Database: drawDatabaseTab(); break;
        case Tab::Skills:   drawSkillsTab();   break;
    }
    drawToolbar();

    if (statusTimer_ > 0) {
        int w = MeasureTextU(status_.c_str(), 16);
        DrawRectangle(GetScreenWidth() - w - 28, GetScreenHeight() - 34, w + 20, 26, ui::kAccent);
        DrawTextU(status_.c_str(), GetScreenWidth() - w - 18, GetScreenHeight() - 30, 16, BLACK);
    }
}

void Editor::drawToolbar() {
    int sw = GetScreenWidth();
    ui::panel({ 0, 0, (float)sw, kToolbarH }, ui::kPanelHi);

    float x = 8;
    auto tabBtn = [&](const char* name, Tab t) {
        if (ui::button({ x, 6, 78, 28 }, name, tab_ == t)) tab_ = t;
        x += 80;
    };
    tabBtn("월드", Tab::World);
    tabBtn("맵", Tab::Map);
    tabBtn("이벤트", Tab::Events);
    tabBtn("캐릭터", Tab::Chars);
    tabBtn("에셋", Tab::Assets);
    tabBtn("DB", Tab::Database);
    tabBtn("스킬", Tab::Skills);

    x += 12;
    if (ui::button({ x, 6, 90, 28 }, "저장")) { engine_.project().save(); setStatus("저장됨."); }
    x += 94;
    if (ui::button({ x, 6, 110, 28 }, "플레이 (F5)", false)) { engine_.project().save(); engine_.startPlaytest(); }

    // Map-specific tools on the right
    if (tab_ == Tab::Map) {
        float bw = 54, gap = 56;
        float rx = sw - 8 - 6*gap;
        auto tbtn=[&](const char* n, Tool t){ if (ui::button({rx,6,bw,28},n, tool_==t && !collisionMode_)){tool_=t;collisionMode_=false;} rx+=gap; };
        tbtn("펜",Tool::Pencil); tbtn("지우개",Tool::Erase); tbtn("채우기",Tool::Fill);
        tbtn("사각형",Tool::Rect); tbtn("스탬프",Tool::Stamp);
        if (ui::button({ rx, 6, bw, 28 }, "충돌", collisionMode_)) collisionMode_ = !collisionMode_;
    }
}

// ----------------------------- MAP -----------------------------
void Editor::drawMapTab() {
    Rectangle paletteArea = { 0, kToolbarH, kPaletteW, (float)GetScreenHeight() - kToolbarH };
    Rectangle canvasArea  = { kPaletteW, kToolbarH, (float)GetScreenWidth() - kPaletteW,
                              (float)GetScreenHeight() - kToolbarH };
    drawMapCanvas(canvasArea);
    if (tool_ == Tool::Stamp) drawPrefabPalette(paletteArea);
    else drawTilePalette(paletteArea);
}

void Editor::drawTilePalette(Rectangle area) {
    ui::panel(area, ui::kPanel);
    auto m = activeMap();
    if (!m) return;
    const Tileset& set = m->tileset;

    // Layer selector
    float ly = area.y + 8;
    ui::label("레이어", (int)area.x + 10, (int)ly, 16, ui::kTextDim);
    for (int i = 0; i < kLayerCount; ++i) {
        if (ui::button({ area.x + 10 + i*64, ly + 22, 60, 26 },
                       TextFormat("L%d", i+1), activeLayer_ == i))
            activeLayer_ = i;
    }
    float ty = ly + 60;

    // Tileset image asset selector
    ui::label("타일셋", (int)area.x + 10, (int)ty, 16, ui::kTextDim);
    auto imgs = engine_.project().assets.byType(AssetType::Image);
    if (ui::button({ area.x + 10, ty + 22, area.width - 20, 26 },
                   set.assetId >= 0 ? "타일셋 이미지 변경" : "타일셋 이미지 선택")) {
        // cycle to next image asset
        if (!imgs.empty()) {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == set.assetId) idx = i;
            m->tileset.assetId = imgs[(idx + 1) % imgs.size()]->id;
        }
    }
    ty += 54;
    ui::intStepper({ area.x + 10, ty, area.width - 20, 24 }, "열", m->tileset.columns, 1, 1, 64); ty += 28;
    ui::intStepper({ area.x + 10, ty, area.width - 20, 24 }, "행", m->tileset.rows, 1, 1, 64); ty += 30;

    // mark the selected tile as animated (cycles tile <-> tile+1 in play)
    bool isAnim = std::find(m->animTiles.begin(), m->animTiles.end(), selectedTile_) != m->animTiles.end();
    if (ui::button({ area.x + 10, ty, area.width - 20, 24 },
                   isAnim ? "애니메이션: 켜짐" : "애니메이션: 꺼짐", isAnim)) {
        if (isAnim) m->animTiles.erase(std::remove(m->animTiles.begin(), m->animTiles.end(), selectedTile_), m->animTiles.end());
        else m->animTiles.push_back(selectedTile_);
    }
    ty += 32;

    // Tile grid
    if (set.assetId < 0) {
        ui::label("에셋 탭에 이미지를", (int)area.x + 10, (int)ty, 14, ui::kTextDim);
        ui::label("끌어다 놓으세요.", (int)area.x + 10, (int)ty + 18, 14, ui::kTextDim);
        return;
    }
    const Texture2D& tex = engine_.assetTexture(set.assetId);
    float gridW = area.width - 20;
    float scale = gridW / std::max(1, tex.width);
    Rectangle dst = { area.x + 10, ty, gridW, tex.height * scale };
    DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height }, dst, {0,0}, 0, WHITE);

    // grid lines + selection
    float cellW = gridW / std::max(1, set.columns);
    float cellH = (set.tileHeight * scale);
    for (int c = 0; c <= set.columns; ++c)
        DrawLine((int)(dst.x + c*cellW), (int)dst.y, (int)(dst.x + c*cellW), (int)(dst.y + set.rows*cellH), Fade(BLACK,0.3f));
    for (int r = 0; r <= set.rows; ++r)
        DrawLine((int)dst.x, (int)(dst.y + r*cellH), (int)(dst.x + set.columns*cellW), (int)(dst.y + r*cellH), Fade(BLACK,0.3f));

    // selection highlight
    int selC = selectedTile_ % std::max(1, set.columns);
    int selR = selectedTile_ / std::max(1, set.columns);
    DrawRectangleLinesEx({ dst.x + selC*cellW, dst.y + selR*cellH, cellW, cellH }, 2, ui::kAccent);

    // click to select
    if (ui::mouseIn(dst) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mp = GetMousePosition();
        int c = (int)((mp.x - dst.x) / cellW);
        int r = (int)((mp.y - dst.y) / cellH);
        if (c >= 0 && c < set.columns && r >= 0 && r < set.rows)
            selectedTile_ = r * set.columns + c;
    }
}

void Editor::drawMapCanvas(Rectangle area) {
    auto m = activeMap();
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    if (!m) { EndScissorMode(); return; }
    int TS = m->tileset.tileWidth;

    BeginMode2D(cam_);
    const Texture2D& tex = engine_.assetTexture(m->tileset.assetId);
    const Tileset& set = m->tileset;
    int w = m->tilemap.width(), h = m->tilemap.height();

    // viewport culling: only iterate tiles visible in the canvas
    Vector2 tl = GetScreenToWorld2D({ area.x, area.y }, cam_);
    Vector2 br = GetScreenToWorld2D({ area.x + area.width, area.y + area.height }, cam_);
    int cx0 = std::max(0, (int)(tl.x/TS) - 1), cy0 = std::max(0, (int)(tl.y/TS) - 1);
    int cx1 = std::min(w-1, (int)(br.x/TS) + 1), cy1 = std::min(h-1, (int)(br.y/TS) + 1);

    // map background
    DrawRectangle(0, 0, w*TS, h*TS, Color{ 30, 33, 42, 255 });
    for (int layer = 0; layer < kLayerCount; ++layer) {
        // dim layers above the active one for clarity
        unsigned char a = (layer == activeLayer_ || !collisionMode_) ? 255 : 120;
        for (int y = cy0; y <= cy1; ++y)
            for (int x = cx0; x <= cx1; ++x) {
                int t = m->tilemap.tile(layer, x, y);
                if (t < 0 || set.assetId < 0) continue;
                int sx, sy; set.srcOf(t, sx, sy);
                DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                               { (float)x*TS,(float)y*TS,(float)TS,(float)TS }, {0,0}, 0, Fade(WHITE, a/255.0f));
            }
    }
    // grid
    for (int x = 0; x <= w; ++x) DrawLine(x*TS, 0, x*TS, h*TS, Fade(BLACK, 0.25f));
    for (int y = 0; y <= h; ++y) DrawLine(0, y*TS, w*TS, y*TS, Fade(BLACK, 0.25f));
    // collision overlay
    if (collisionMode_)
        for (int y = cy0; y <= cy1; ++y)
            for (int x = cx0; x <= cx1; ++x)
                if (m->tilemap.blocked(x, y))
                    DrawRectangle(x*TS, y*TS, TS, TS, Fade(ui::kDanger, 0.45f));
    EndMode2D();

    // painting
    if (ui::mouseIn(area) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
        int tx = (int)std::floor(world.x / TS), ty = (int)std::floor(world.y / TS);
        bool inMap = m->tilemap.inBounds(tx, ty);
        bool eyedrop = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

        // hover highlight + rectangle preview
        BeginMode2D(cam_);
        if (inMap)
            DrawRectangleLinesEx({ (float)tx*TS,(float)ty*TS,(float)TS,(float)TS }, 2,
                                 eyedrop ? GREEN : ui::kAccentHi);
        if (tool_ == Tool::Rect && rectDragging_ && inMap) {
            int x0 = std::min(rectStartX_, tx), y0 = std::min(rectStartY_, ty);
            int x1 = std::max(rectStartX_, tx), y1 = std::max(rectStartY_, ty);
            DrawRectangle(x0*TS, y0*TS, (x1-x0+1)*TS, (y1-y0+1)*TS, Fade(ui::kAccent, 0.30f));
            DrawRectangleLinesEx({ (float)x0*TS,(float)y0*TS,(float)(x1-x0+1)*TS,(float)(y1-y0+1)*TS },
                                 2, ui::kAccentHi);
        }
        if (tool_ == Tool::Stamp && inMap) {                  // prefab footprint preview
            const Prefab& pf = prefabs()[prefabSel_];
            DrawRectangle(tx*TS, ty*TS, pf.w*TS, pf.h*TS, Fade(ui::kAccent, 0.25f));
            DrawRectangleLinesEx({ (float)tx*TS,(float)ty*TS,(float)pf.w*TS,(float)pf.h*TS }, 2, ui::kAccentHi);
        }
        EndMode2D();

        if (inMap) {
            if (eyedrop) {                                  // eyedropper: pick a tile
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    int t = m->tilemap.tile(activeLayer_, tx, ty);
                    if (t >= 0) { selectedTile_ = t; setStatus("타일 선택: " + std::to_string(t)); }
                }
            } else if (tool_ == Tool::Stamp) {              // prefab stamp
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    stampPrefab(tx, ty);
                    setStatus(prefabs()[prefabSel_].name + " 스탬프됨");
                }
            } else if (tool_ == Tool::Rect) {               // rectangle fill (drag)
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rectDragging_ = true; rectStartX_ = tx; rectStartY_ = ty; }
                if (rectDragging_ && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    rectDragging_ = false;
                    pushUndo();
                    int x0 = std::min(rectStartX_, tx), y0 = std::min(rectStartY_, ty);
                    int x1 = std::max(rectStartX_, tx), y1 = std::max(rectStartY_, ty);
                    for (int yy = y0; yy <= y1; ++yy)
                        for (int xx = x0; xx <= x1; ++xx) {
                            if (collisionMode_) m->tilemap.setBlocked(xx, yy, true);
                            else m->tilemap.setTile(activeLayer_, xx, yy, selectedTile_);
                        }
                    setStatus("사각형 채움");
                }
            } else if (collisionMode_) {                    // collision paint
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) pushUndo();
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))  m->tilemap.setBlocked(tx, ty, true);
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) m->tilemap.setBlocked(tx, ty, false);
            } else if (tool_ == Tool::Fill) {               // bucket fill
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { pushUndo(); m->tilemap.fill(activeLayer_, tx, ty, selectedTile_); }
            } else {                                        // pencil / erase
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) pushUndo();
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))  m->tilemap.setTile(activeLayer_, tx, ty,
                                                            tool_ == Tool::Erase ? -1 : selectedTile_);
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) m->tilemap.setTile(activeLayer_, tx, ty, -1);
            }
        }
    }
    EndScissorMode();
}

// ---------------------------- EVENTS ----------------------------
void Editor::drawEventsTab() {
    Rectangle canvasArea = { 0, kToolbarH, (float)GetScreenWidth() - 320, (float)GetScreenHeight() - kToolbarH };
    auto m = activeMap();
    BeginScissorMode((int)canvasArea.x, (int)canvasArea.y, (int)canvasArea.width, (int)canvasArea.height);
    DrawRectangleRec(canvasArea, Color{ 24, 26, 34, 255 });
    if (m) {
        int TS = m->tileset.tileWidth;
        BeginMode2D(cam_);
        const Texture2D& tex = engine_.assetTexture(m->tileset.assetId);
        const Tileset& set = m->tileset;
        int w = m->tilemap.width(), h = m->tilemap.height();
        Vector2 etl = GetScreenToWorld2D({ canvasArea.x, canvasArea.y }, cam_);
        Vector2 ebr = GetScreenToWorld2D({ canvasArea.x+canvasArea.width, canvasArea.y+canvasArea.height }, cam_);
        int ex0=std::max(0,(int)(etl.x/TS)-1), ey0=std::max(0,(int)(etl.y/TS)-1);
        int ex1=std::min(w-1,(int)(ebr.x/TS)+1), ey1=std::min(h-1,(int)(ebr.y/TS)+1);
        for (int layer = 0; layer < kLayerCount; ++layer)
            for (int y = ey0; y <= ey1; ++y)
                for (int x = ex0; x <= ex1; ++x) {
                    int t = m->tilemap.tile(layer, x, y);
                    if (t < 0 || set.assetId < 0) continue;
                    int sx, sy; set.srcOf(t, sx, sy);
                    DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                                   { (float)x*TS,(float)y*TS,(float)TS,(float)TS }, {0,0}, 0, WHITE);
                }
        for (int x = 0; x <= w; ++x) DrawLine(x*TS, 0, x*TS, h*TS, Fade(BLACK, 0.25f));
        for (int y = 0; y <= h; ++y) DrawLine(0, y*TS, w*TS, y*TS, Fade(BLACK, 0.25f));
        // event markers
        for (auto& e : m->events) {
            DrawRectangle(e.x*TS, e.y*TS, TS, TS, Fade(ui::kAccent, 0.5f));
            DrawRectangleLinesEx({ (float)e.x*TS,(float)e.y*TS,(float)TS,(float)TS }, 2,
                                 e.id == editingEventId_ ? ui::kAccentHi : ui::kAccent);
            DrawTextU(TextFormat("%d", e.id), e.x*TS+3, e.y*TS+2, 14, WHITE);
        }
        EndMode2D();

        // click to select/create event
        if (ui::mouseIn(canvasArea) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
            int tx = (int)(world.x / TS), ty = (int)(world.y / TS);
            if (m->tilemap.inBounds(tx, ty)) {
                Event* existing = m->eventAt(tx, ty);
                if (existing) editingEventId_ = existing->id;
                else {
                    Event ne; ne.id = m->nextEventId(); ne.x = tx; ne.y = ty;
                    ne.text = "안녕하세요!";
                    m->events.push_back(ne);
                    editingEventId_ = ne.id;
                }
                eventTextFocus_ = false;
            }
        }
    }
    EndScissorMode();

    // ---- event inspector panel ----
    Rectangle panel = { (float)GetScreenWidth() - 320, kToolbarH, 320, (float)GetScreenHeight() - kToolbarH };
    ui::panel(panel, ui::kPanel);
    ui::label("이벤트", (int)panel.x + 12, (int)panel.y + 10, 22, ui::kAccent);
    Event* ev = nullptr;
    if (m) for (auto& e : m->events) if (e.id == editingEventId_) ev = &e;
    if (!ev) { ui::label("타일을 클릭해 추가하거나", (int)panel.x + 12, (int)panel.y + 48, 16, ui::kTextDim);
               ui::label("이벤트를 선택하세요.", (int)panel.x + 12, (int)panel.y + 68, 16, ui::kTextDim);
               return; }

    float y = panel.y + 44;
    const char* typeNames[] = { "메시지", "이동", "아이템지급", "스위치설정", "전투", "상점", "퀘스트", "엔딩" };
    if (ui::button({ panel.x + 12, y, 296, 26 }, TextFormat("종류: %s", typeNames[(int)ev->type]))) {
        ev->type = (EventType)(((int)ev->type + 1) % 8);
    }
    y += 32;
    const char* trigNames[] = { "말걸기", "접촉", "자동실행" };
    if (ui::button({ panel.x + 12, y, 296, 26 }, TextFormat("트리거: %s", trigNames[(int)ev->trigger]))) {
        ev->trigger = (TriggerType)(((int)ev->trigger + 1) % 3);
    }
    y += 36;

    // text field (used by Message/GiveItem/SetSwitch/Shop)
    ui::label("텍스트:", (int)panel.x + 12, (int)y, 14, ui::kTextDim); y += 18;
    Rectangle tf = { panel.x + 12, y, 296, 26 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) eventTextFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) eventTextFocus_ = false;
    ui::textField(tf, ev->text, eventTextFocus_, 120);
    y += 34;

    // type-specific params
    switch (ev->type) {
        case EventType::Teleport:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "대상맵", ev->targetMap, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "X", ev->targetX, 1, 0, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "Y", ev->targetY, 1, 0, 999); y += 28;
            break;
        case EventType::GiveItem:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "아이템ID", ev->itemId, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "수량", ev->amount, 1, 1, 99); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "스위치설정", ev->switchId, 1, -1, 999); y += 28;
            break;
        case EventType::SetSwitch:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "스위치ID", ev->switchId, 1, 0, 999); y += 28;
            if (ui::button({ panel.x + 12, y, 296, 24 }, ev->switchValue ? "값: 켜짐" : "값: 꺼짐"))
                ev->switchValue = !ev->switchValue;
            y += 28;
            break;
        case EventType::StartBattle:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "적ID", ev->itemId, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "수", ev->amount, 1, 1, 6); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "처치스위치", ev->switchId, 1, -1, 999); y += 28;
            break;
        case EventType::Shop:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "아이템ID", ev->itemId, 1, -1, 999); y += 28;
            break;
        case EventType::Quest:
            DrawTextU("텍스트 = HUD에 표시되는 목표.", (int)panel.x+12, (int)y, 12, ui::kTextDim); y += 20;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "스위치설정", ev->switchId, 1, -1, 999); y += 28;
            break;
        case EventType::Ending:
            DrawTextU("게임 클리어 화면을 표시합니다.", (int)panel.x+12, (int)y, 12, ui::kTextDim); y += 22;
            break;
        default: break;
    }
    y += 6;
    // condition switch
    ui::intStepper({ panel.x + 12, y, 296, 24 }, "조건스위치", ev->conditionSwitch, 1, -1, 999); y += 28;
    if (ui::button({ panel.x + 12, y, 144, 24 }, ev->once ? "1회만: 예" : "1회만: 아니오")) ev->once = !ev->once;
    // graphic asset cycle
    if (ui::button({ panel.x + 164, y, 144, 24 }, ev->graphicAsset >= 0 ? "그래픽: 있음" : "그래픽: 없음")) {
        auto imgs = engine_.project().assets.byType(AssetType::Image);
        if (imgs.empty()) ev->graphicAsset = -1;
        else {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == ev->graphicAsset) idx = i;
            idx++;
            ev->graphicAsset = (idx >= (int)imgs.size()) ? -1 : imgs[idx]->id;
        }
    }
    y += 32;
    // NPC wander toggle (only meaningful when the event has a sprite)
    if (ui::button({ panel.x + 12, y, 296, 24 }, ev->wander ? "NPC 배회: 켜짐" : "NPC 배회: 꺼짐", ev->wander))
        ev->wander = !ev->wander;
    y += 30;
    DrawTextU("트리거 '자동실행' = 맵 진입 시 1회 재생.", (int)panel.x + 12, (int)y, 12, ui::kTextDim);
    y += 22;
    if (ui::button({ panel.x + 12, y, 296, 28 }, "이벤트 삭제", false)) {
        auto& evs = m->events;
        evs.erase(std::remove_if(evs.begin(), evs.end(),
                  [&](const Event& e){ return e.id == editingEventId_; }), evs.end());
        editingEventId_ = -1;
    }
}

// ---------------------------- ASSETS ----------------------------
void Editor::drawAssetsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    ui::label(".png / .wav / .ogg 파일을 창에 끌어다 놓으면 등록됩니다.",
              20, (int)kToolbarH + 16, 20, ui::kText);

    Project& p = engine_.project();
    auto& assets = p.assets.all();
    float x = 20, y = kToolbarH + 56;
    float thumb = 96, pad = 16, cellW = thumb + 80;

    for (const auto& a : assets) {
        Rectangle cell = { x, y, cellW, thumb + 60 };
        ui::panel(cell, ui::kPanel);
        if (a.type == AssetType::Image) {
            const Texture2D& tex = engine_.assetTexture(a.id);
            float s = std::min(thumb / std::max(1, tex.width), thumb / std::max(1, tex.height));
            DrawTextureEx(tex, { x + 8, y + 8 }, 0, s, WHITE);
        } else {
            DrawTextU("♪ AUDIO", (int)x + 10, (int)y + 40, 18, ui::kAccent);
        }
        DrawTextU(a.name.c_str(), (int)x + 8, (int)(y + thumb + 12), 14, ui::kText);
        DrawTextU(TextFormat("id %d", a.id), (int)x + 8, (int)(y + thumb + 30), 12, ui::kTextDim);

        // image action buttons
        if (a.type == AssetType::Image) {
            if (ui::button({ x + thumb + 12, y + 8, 100, 24 }, "타일셋", false)) {
                if (auto m = activeMap()) { m->tileset.assetId = a.id; setStatus("타일셋 설정됨."); }
            }
            if (ui::button({ x + thumb + 12, y + 36, 100, 24 }, "플레이어", false)) {
                p.playerSprite = a.id; setStatus("플레이어 스프라이트 설정됨.");
            }
        }
        x += cellW + pad;
        if (x + cellW > GetScreenWidth() - 20) { x = 20; y += thumb + 60 + pad; }
    }

    if (assets.empty())
        ui::label("(아직 에셋이 없습니다)", 20, (int)kToolbarH + 56, 18, ui::kTextDim);
}

// --------------------------- DATABASE ---------------------------
void Editor::drawDatabaseTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Database& db = engine_.project().database;

    // category tabs
    const char* cats[] = { "아이템", "장비", "스킬", "액터", "적" };
    for (int i = 0; i < 5; ++i)
        if (ui::button({ 12.0f + i*120, kToolbarH + 10, 112, 28 }, cats[i], dbCategory_ == i)) {
            dbCategory_ = i; dbSelected_ = -1; dbNameFocus_ = -1;
        }

    float listX = 12, listY = kToolbarH + 50, listW = 280;
    ui::panel({ listX, listY, listW, area.height - 60 }, ui::kPanel);

    // "Add" button
    if (ui::button({ listX + 8, listY + 8, listW - 16, 28 }, "+ 새로 추가")) {
        switch (dbCategory_) {
            case 0: { Item it; it.id = (int)db.items.size()+1; db.items.push_back(it); dbSelected_=(int)db.items.size()-1; } break;
            case 1: { Equipment e; e.id = (int)db.equipment.size()+1; db.equipment.push_back(e); dbSelected_=(int)db.equipment.size()-1; } break;
            case 2: { Skill s; s.id = (int)db.skills.size()+1; db.skills.push_back(s); dbSelected_=(int)db.skills.size()-1; } break;
            case 3: { ActorDef a; a.id = (int)db.actors.size()+1; db.actors.push_back(a); dbSelected_=(int)db.actors.size()-1; } break;
            case 4: { EnemyDef en; en.id = (int)db.enemies.size()+1; db.enemies.push_back(en); dbSelected_=(int)db.enemies.size()-1; } break;
        }
    }

    // list
    float ly = listY + 44;
    auto listEntry = [&](int i, const std::string& name) {
        Rectangle r = { listX + 8, ly, listW - 16, 26 };
        if (ui::button(r, name, dbSelected_ == i)) { dbSelected_ = i; dbNameFocus_ = -1; }
        ly += 28;
    };
    int count = 0;
    switch (dbCategory_) {
        case 0: count=(int)db.items.size();     for (int i=0;i<count;++i) listEntry(i, db.items[i].name); break;
        case 1: count=(int)db.equipment.size(); for (int i=0;i<count;++i) listEntry(i, db.equipment[i].name); break;
        case 2: count=(int)db.skills.size();    for (int i=0;i<count;++i) listEntry(i, db.skills[i].name); break;
        case 3: count=(int)db.actors.size();    for (int i=0;i<count;++i) listEntry(i, db.actors[i].name); break;
        case 4: count=(int)db.enemies.size();   for (int i=0;i<count;++i) listEntry(i, db.enemies[i].name); break;
    }

    if (dbSelected_ < 0 || dbSelected_ >= count) return;

    // ---- detail editor ----
    float dx = listX + listW + 20, dy = listY + 8, dw = area.width - dx - 20;
    ui::panel({ dx - 8, listY, dw + 16, area.height - 60 }, ui::kPanel);

    auto nameField = [&](std::string& name) {
        ui::label("이름:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
        Rectangle tf = { dx, dy, std::min(360.0f, dw), 28 };
        bool focus = (dbNameFocus_ == dbSelected_);
        if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbNameFocus_ = dbSelected_;
        else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf) && dbNameFocus_ == dbSelected_) dbNameFocus_ = -1;
        ui::textField(tf, name, focus, 32);
        dy += 38;
    };
    auto step = [&](const char* lbl, int& v, int s, int lo, int hi) {
        ui::intStepper({ dx, dy, std::min(300.0f, dw), 26 }, lbl, v, s, lo, hi); dy += 30;
    };

    switch (dbCategory_) {
        case 0: { Item& it = db.items[dbSelected_]; nameField(it.name);
            step("가격", it.price, 10, 0, 99999);
            step("효과량", it.power, 5, 0, 9999);
            const char* effs[] = {"없음","HP회복","MP회복","데미지"};
            if (ui::button({dx,dy,200,26}, TextFormat("효과: %s", effs[(int)it.effect]))) it.effect=(ItemEffect)(((int)it.effect+1)%4);
            dy+=32;
            if (ui::button({dx,dy,200,26}, it.consumable?"소모성: 예":"소모성: 아니오")) it.consumable=!it.consumable;
            dy+=36;
            break; }
        case 1: { Equipment& e = db.equipment[dbSelected_]; nameField(e.name);
            if (ui::button({dx,dy,200,26}, e.slot==EquipSlot::Weapon?"슬롯: 무기":"슬롯: 방어구"))
                e.slot = e.slot==EquipSlot::Weapon?EquipSlot::Armor:EquipSlot::Weapon;
            dy+=32;
            step("가격", e.price, 10, 0, 99999);
            step("공격+", e.atk, 1, 0, 999);
            step("방어+", e.def, 1, 0, 999);
            break; }
        case 2: { Skill& s = db.skills[dbSelected_]; nameField(s.name);
            step("MP 소모", s.mpCost, 1, 0, 999);
            step("위력", s.power, 5, 0, 9999);
            if (ui::button({dx,dy,200,26}, s.healing?"종류: 회복":"종류: 데미지")) s.healing=!s.healing;
            dy+=36;
            break; }
        case 3: { ActorDef& a = db.actors[dbSelected_]; nameField(a.name);
            step("최대 HP", a.maxHp, 10, 1, 9999);
            step("최대 MP", a.maxMp, 5, 0, 9999);
            step("공격", a.atk, 1, 0, 999);
            step("방어", a.def, 1, 0, 999);
            step("속도", a.spd, 1, 0, 999);
            break; }
        case 4: { EnemyDef& e = db.enemies[dbSelected_]; nameField(e.name);
            step("최대 HP", e.maxHp, 10, 1, 9999);
            step("공격", e.atk, 1, 0, 999);
            step("방어", e.def, 1, 0, 999);
            step("속도", e.spd, 1, 0, 999);
            step("경험치", e.expReward, 5, 0, 99999);
            step("골드", e.goldReward, 5, 0, 99999);
            break; }
    }
    DrawTextU(TextFormat("id: %d   (Ctrl+S로 프로젝트 저장)",
             dbCategory_==0?db.items[dbSelected_].id:
             dbCategory_==1?db.equipment[dbSelected_].id:
             dbCategory_==2?db.skills[dbSelected_].id:
             dbCategory_==3?db.actors[dbSelected_].id:db.enemies[dbSelected_].id),
             (int)dx, (int)dy + 6, 14, ui::kTextDim);
}

// ---------------------------- WORLD (map management) ----------------------------
void Editor::drawWorldTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    // ---- left: map list ----
    float lx = 12, ly = kToolbarH + 12, lw = 300;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("맵 목록", (int)lx + 12, (int)ly + 10, 22, ui::kAccent);
    DrawTextU(TextFormat("%d개", (int)p.maps.size()), (int)lx + 120, (int)ly + 16, 16, ui::kTextDim);

    float y = ly + 46;
    for (int i = 0; i < (int)p.maps.size(); ++i) {
        auto& m = p.maps[i];
        bool isStart = (m->id == p.startMap);
        bool isActive = (m->id == activeMapId_);
        Rectangle r = { lx + 10, y, lw - 20, 28 };
        std::string label = (isStart ? "* " : "  ") + m->name + "  (#" + std::to_string(m->id) + ")";
        if (ui::button(r, label, worldSelected_ == i || isActive)) {
            worldSelected_ = i; activeMapId_ = m->id; mapNameFocus_ = false;
        }
        y += 32;
    }

    // new map controls — standardized world size (7 tiers, up to 420x420)
    y += 8;
    ui::label("새 맵 크기 (규격)", (int)lx + 10, (int)y, 14, ui::kTextDim); y += 20;
    if (ui::button({ lx + 10, y, lw - 20, 26 }, kSizeTierNames[newMapTier_]))
        newMapTier_ = (newMapTier_ + 1) % 7;
    y += 34;
    if (ui::button({ lx + 10, y, lw - 20, 30 }, "+ 새 맵")) {
        int side = kSizeTiers[newMapTier_];
        auto nm = p.addMap("Map" + std::to_string(p.nextMapId()), side, side);
        // copy tileset from current map so it is paintable immediately
        if (auto cur = activeMap()) nm->tileset = cur->tileset;
        activeMapId_ = nm->id;
        worldSelected_ = (int)p.maps.size() - 1;
        p.save();
        setStatus(nm->name + " 생성됨");
        tab_ = Tab::Map;
    }

    // ---- right: selected map details ----
    if (worldSelected_ < 0 || worldSelected_ >= (int)p.maps.size())
        worldSelected_ = activeMap() ? 0 : -1;
    if (worldSelected_ < 0) return;
    auto m = p.maps[worldSelected_];

    float dx = lx + lw + 24, dy = ly;
    ui::panel({ dx - 8, ly, area.width - dx - 4, area.height - 24 }, ui::kPanel);
    ui::label("맵 설정", (int)dx + 4, (int)dy + 10, 22, ui::kAccent);
    dy += 46;

    ui::label("이름:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
    Rectangle tf = { dx, dy, 360, 28 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) mapNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) mapNameFocus_ = false;
    ui::textField(tf, m->name, mapNameFocus_, 40);
    dy += 40;

    int w = m->tilemap.width(), h = m->tilemap.height();
    DrawTextU(TextFormat("크기: %d x %d 타일", w, h), (int)dx, (int)dy, 16, ui::kText); dy += 26;
    // standardized size tiers (cycle to resize, content preserved)
    int tier = sizeTierOf(std::max(w, h));
    if (ui::button({ dx, dy, 260, 26 }, kSizeTierNames[tier])) {
        int nt = (tier + 1) % 7;
        int side = kSizeTiers[nt];
        m->tilemap.resizePreserve(side, side);
        setStatus(std::string("맵 크기 변경: ") + kSizeTierNames[nt]);
    }
    dy += 30;
    DrawTextU("위 버튼 클릭 = 다음 규격으로 크기 변경 (내용 보존)", (int)dx, (int)dy, 12, ui::kTextDim); dy += 24;

    ui::intStepper({ dx, dy, 260, 26 }, "조우율%", m->encounterRate, 5, 0, 100); dy += 34;

    // ---- second column: atmosphere / audio / field monsters ----
    float cx = dx + 300, cy = ly + 92;
    ui::label("분위기", (int)cx, (int)cy, 16, ui::kTextDim); cy += 24;
    ui::intStepper({ cx, cy, 250, 26 }, "어둠", m->darkness, 15, 0, 255); cy += 32;
    const char* wx[3] = { "날씨: 없음", "날씨: 비", "날씨: 눈" };
    if (ui::button({ cx, cy, 250, 26 }, wx[m->weather % 3])) m->weather = (m->weather + 1) % 3;
    cy += 32;
    if (ui::button({ cx, cy, 250, 26 }, m->dayNight ? "낮/밤: 켜짐" : "낮/밤: 꺼짐", m->dayNight))
        m->dayNight = !m->dayNight;
    cy += 38;

    ui::label("배경음악 (BGM)", (int)cx, (int)cy, 16, ui::kTextDim); cy += 24;
    auto auds = p.assets.byType(AssetType::Audio);
    const AssetEntry* curB = p.assets.find(m->bgmAsset);
    if (ui::button({ cx, cy, 250, 26 }, std::string("BGM: ") + (curB ? curB->name : "없음"))) {
        int idx = -1;
        for (int i = 0; i < (int)auds.size(); ++i) if (auds[i]->id == m->bgmAsset) idx = i;
        idx++;
        m->bgmAsset = (idx >= (int)auds.size()) ? -1 : auds[idx]->id;
    }
    cy += 40;

    ui::label("필드 몬스터", (int)cx, (int)cy, 16, ui::kTextDim); cy += 24;
    for (auto& en : p.database.enemies) {
        bool on = std::find(m->encounterEnemies.begin(), m->encounterEnemies.end(), en.id) != m->encounterEnemies.end();
        if (ui::button({ cx, cy, 250, 24 }, (on ? "[x] " : "[  ] ") + en.name, on)) {
            if (on) m->encounterEnemies.erase(std::remove(m->encounterEnemies.begin(), m->encounterEnemies.end(), en.id), m->encounterEnemies.end());
            else m->encounterEnemies.push_back(en.id);
        }
        cy += 27;
    }

    if (ui::button({ dx, dy, 220, 30 }, p.startMap == m->id ? "시작 맵 (현재)" : "시작 맵으로 설정",
                   p.startMap == m->id)) {
        p.startMap = m->id; p.startX = w/2; p.startY = h/2; setStatus("시작 맵 설정됨.");
    }
    dy += 38;
    if (ui::button({ dx, dy, 220, 30 }, "이 맵 편집")) { activeMapId_ = m->id; tab_ = Tab::Map; }
    dy += 38;
    if ((int)p.maps.size() > 1) {
        if (ui::button({ dx, dy, 220, 30 }, "맵 삭제", false)) {
            int delId = m->id;
            p.maps.erase(p.maps.begin() + worldSelected_);
            std::error_code ec;
            fs::remove(fs::path(p.dir) / "maps" / (std::to_string(delId) + ".json"), ec);
            if (p.startMap == delId) p.startMap = p.maps.front()->id;
            if (activeMapId_ == delId) activeMapId_ = p.maps.front()->id;
            worldSelected_ = 0;
            p.save();
            setStatus("맵 삭제됨.");
            return;
        }
    }
    DrawTextU("팁: 이동 이벤트로 맵을 연결하세요 (이벤트 탭).",
             (int)dx, (int)(ly + area.height - 60), 14, ui::kTextDim);
}

// ---------------------------- CHARS (character assets) ----------------------------
int Editor::generateCharacter() {
    Project& p = engine_.project();
    static const Color shirts[] = {
        {80,140,220,255}, {200,90,90,255}, {90,180,110,255}, {200,160,70,255},
        {170,110,200,255}, {90,190,200,255}, {220,130,180,255}, {110,120,130,255}
    };
    static const Color skins[] = { {240,200,160,255}, {225,180,140,255}, {200,150,120,255} };
    Color shirt = shirts[charColor_ % 8];
    Color skin  = skins[(charColor_ / 8) % 3];
    charColor_++;

    // unique filename in the project's assets folder
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir) / "assets" / ("char_" + std::to_string(n++) + ".png"); }
    while (fs::exists(dest));

    Image img = gen::characterSheet(shirt, skin);
    ExportImage(img, dest.string().c_str());
    UnloadImage(img);

    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
    p.save();
    setStatus("캐릭터 생성됨: " + dest.stem().string());
    return id;
}

// Generate a skill-effect sheet (4 frames x 4 dirs) and register it as an asset.
int Editor::generateEffect(int style) {
    Project& p = engine_.project();
    static const Color cols[4] = {
        {255,235,150,255},  // slash - warm
        {130,200,255,255},  // bolt  - blue
        {190,225,255,255},  // dash  - pale
        {255,170,110,255}   // burst - orange
    };
    static const char* tags[4] = { "fx_slash", "fx_bolt", "fx_dash", "fx_burst" };
    int s = style & 3;
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir) / "assets" / (std::string(tags[s]) + "_" + std::to_string(n++) + ".png"); }
    while (fs::exists(dest));

    const int FR = 6;
    Image img = gen::effectSheet(cols[s], s, FR);
    ExportImage(img, dest.string().c_str());
    UnloadImage(img);

    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
    p.assets.setAnim(id, FR, 14);           // single-row animated effect
    p.save();
    setStatus("이펙트 에셋 생성됨: " + dest.stem().string());
    return id;
}

void Editor::drawCharsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    ui::label("캐릭터 / 이펙트 에셋", 20, (int)kToolbarH + 14, 24, ui::kAccent);
    DrawTextU("캐릭터/이펙트를 엔진에서 생성하거나, PNG 시트(N프레임 x 4방향)를 창에 끌어다 놓으세요.",
             20, (int)kToolbarH + 44, 15, ui::kTextDim);
    if (ui::button({ 20, kToolbarH + 68, 220, 30 }, "+ 새 캐릭터 생성"))
        generateCharacter();
    // generate one of each effect style (slash/bolt/dash/burst)
    static const char* fxBtn[4] = { "+ 베기", "+ 볼트", "+ 대시", "+ 폭발" };
    for (int s = 0; s < 4; ++s)
        if (ui::button({ 252.0f + s*110, kToolbarH + 68, 104, 30 }, fxBtn[s]))
            generateEffect(s);
    handleAssetDrop(); // allow dropping character/effect sheets here too

    auto imgs = p.assets.byType(AssetType::Image);

    // ---- player movement frames + skill-effect assignment ----
    auto imgName = [&](int id)->std::string {
        if (id < 0) return "없음";
        const AssetEntry* e = p.assets.find(id);
        return e ? e->name : "없음";
    };
    auto cycleAsset = [&](int& slot){            // None -> each image -> None
        int idx = -1;
        for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == slot) idx = i;
        idx++;
        slot = (idx >= (int)imgs.size()) ? -1 : imgs[idx]->id;
        p.save();
    };
    float py = kToolbarH + 104;
    ui::panel({ 20, py, 720, 92 }, ui::kPanel);
    ui::label("플레이어 / 스킬 이펙트 지정", 30, (int)py + 6, 16, ui::kAccent);
    ui::intStepper({ 30, py + 30, 200, 26 }, "이동 프레임", p.playerFrames, 1, 4, 7);
    static const char* slotName[4] = { "공격(Z)", "원거리(X)", "회피(C)", "궁극기(V)" };
    int* slots[4] = { &p.attackEffect, &p.rangedEffect, &p.dashEffect, &p.ultEffect };
    for (int i = 0; i < 4; ++i) {
        Rectangle r = { 250.0f + (i%2)*240, py + 30 + (i/2)*30, 232, 26 };
        if (ui::button(r, std::string(slotName[i]) + ": " + imgName(*slots[i]), *slots[i] >= 0))
            cycleAsset(*slots[i]);
    }

    // grid of image assets, with the 'facing-down' frame preview
    float x = 20, y = py + 108, cell = 150;
    for (auto* a : imgs) {
        Rectangle c = { x, y, cell, cell + 56 };
        bool isPlayer = (a->id == p.playerSprite);
        ui::panel(c, isPlayer ? ui::kPanelHi : ui::kPanel);
        const Texture2D& tex = engine_.assetTexture(a->id);
        // draw the down-facing first frame (sheet is 4x4); scale to ~96px
        float fw = tex.width / 4.0f, fh = tex.height / 4.0f;
        float sc = 96.0f / (fh > 0 ? fh : 1);
        Rectangle src = { 0, 0, fw, fh };
        Rectangle dst = { x + (cell - fw*sc)/2, y + 8, fw*sc, fh*sc };
        DrawTexturePro(tex, src, dst, {0,0}, 0, WHITE);
        DrawTextU(a->name.c_str(), (int)x + 8, (int)(y + cell - 36), 14, ui::kText);
        if (ui::button({ x + 8, y + cell - 16, cell - 16, 26 },
                       isPlayer ? "플레이어" : "플레이어로 설정", isPlayer)) {
            p.playerSprite = a->id; p.save(); setStatus("플레이어 캐릭터 설정됨.");
        }
        x += cell + 14;
        if (x + cell > area.width - 20) { x = 20; y += cell + 70; }
    }
    if (imgs.empty())
        ui::label("(아직 캐릭터가 없습니다 - 생성을 클릭하세요)", 20, (int)kToolbarH + 120, 18, ui::kTextDim);
}

// ---------------------------- SKILLS (field-skill designer) ----------------------------
void Editor::drawSkillsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;

    // ---- left: skill list ----
    float lx = 12, ly = kToolbarH + 12, lw = 250;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("스킬 목록", (int)lx + 12, (int)ly + 10, 20, ui::kAccent);
    float y = ly + 44;
    if (ui::button({ lx + 10, y, lw - 20, 28 }, "+ 새 스킬")) {
        FieldSkill s; s.id = (int)db.fieldSkills.size() + 1;
        s.name = "새 스킬"; s.slot = -1;
        db.fieldSkills.push_back(s); skillSel_ = (int)db.fieldSkills.size() - 1; p.save();
    }
    y += 32;
    if (db.fieldSkills.empty()) {
        if (ui::button({ lx + 10, y, lw - 20, 28 }, "기본 스킬 4종 불러오기")) {
            db.fieldSkills = Database::defaultFieldSkills(); skillSel_ = 0; p.save();
        }
        y += 32;
    }
    static const char* keyName[6] = { "Z","X","C","V","F","G" };
    for (int i = 0; i < (int)db.fieldSkills.size(); ++i) {
        const FieldSkill& s = db.fieldSkills[i];
        std::string lbl = (s.slot >= 0 && s.slot < 6 ? std::string("[")+keyName[s.slot]+"] " : "[-] ") + s.name;
        if (ui::button({ lx + 10, y, lw - 20, 26 }, lbl, skillSel_ == i)) { skillSel_ = i; skillNameFocus_ = false; }
        y += 28;
    }

    if (skillSel_ < 0 && !db.fieldSkills.empty()) skillSel_ = 0; // auto-select first
    if (skillSel_ < 0 || skillSel_ >= (int)db.fieldSkills.size()) {
        ui::label("스킬을 선택하거나 추가하세요.", (int)lx + lw + 30, (int)ly + 20, 16, ui::kTextDim);
        return;
    }
    FieldSkill& s = db.fieldSkills[skillSel_];

    // ---- middle: player-relative tile pattern designer ----
    float gx = lx + lw + 24, gy = ly + 8;
    ui::label("효과 적용 타일 (플레이어 기준, 위=정면)", (int)gx, (int)gy, 16, ui::kAccent);
    gy += 26;
    const int GRID = 9, HALF = GRID/2; // player at center; canonical facing = up
    float cs = 34;
    // upward "front" marker (drawn triangle so it never depends on a glyph)
    DrawTriangle({ gx + HALF*cs + cs/2, gy }, { gx + HALF*cs + cs/2 - 7, gy + 12 },
                 { gx + HALF*cs + cs/2 + 7, gy + 12 }, ui::kGood);
    DrawTextU("정면", (int)(gx + HALF*cs + cs/2 + 12), (int)gy, 13, ui::kGood);
    gy += 16;
    for (int ry = 0; ry < GRID; ++ry) {
        for (int rx = 0; rx < GRID; ++rx) {
            int ox = rx - HALF, oy = ry - HALF;     // offset relative to player
            Rectangle cell = { gx + rx*cs, gy + ry*cs, cs-2, cs-2 };
            bool isPlayer = (ox == 0 && oy == 0);
            bool on = false;
            for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) { on = true; break; }
            Color c = isPlayer ? ui::kAccent : (on ? Color{210,120,90,255} : ui::kPanelHi);
            DrawRectangleRec(cell, c);
            DrawRectangleLinesEx(cell, 1, Fade(BLACK,0.5f));
            if (isPlayer) DrawTextU("P", (int)cell.x+11, (int)cell.y+8, 18, BLACK);
            if (!isPlayer && ui::mouseIn(cell) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (on) { // remove
                    for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) {
                        s.patX.erase(s.patX.begin()+k); s.patY.erase(s.patY.begin()+k); break; }
                } else { s.patX.push_back(ox); s.patY.push_back(oy); }
            }
        }
    }
    float gridBottom = gy + GRID*cs + 8;
    DrawTextU("칸 클릭 = 적용 타일 켜기/끄기 (방향 회전)",
             (int)gx, (int)gridBottom, 12, ui::kTextDim);
    DrawTextU(TextFormat("선택된 타일: %d개", (int)s.patX.size()), (int)gx, (int)gridBottom + 18, 13, ui::kText);

    // ---- right: parameters ----
    float dx = gx + GRID*cs + 30, dy = ly + 8, dw = area.width - dx - 16;
    if (dw < 240) { dx = gx; dy = gridBottom + 44; dw = 300; } // wrap on narrow screens
    ui::panel({ dx - 8, dy - 6, dw + 12, 430 }, ui::kPanel);
    ui::label("스킬 설정", (int)dx, (int)dy, 18, ui::kAccent); dy += 30;

    ui::label("이름:", (int)dx, (int)dy, 13, ui::kTextDim); dy += 18;
    Rectangle nf = { dx, dy, std::min(280.0f, dw), 26 };
    if (ui::mouseIn(nf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) skillNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(nf)) skillNameFocus_ = false;
    ui::textField(nf, s.name, skillNameFocus_, 24); dy += 34;

    // key binding (slot): -1..5
    static const char* slotLbl[7] = { "없음","Z","X","C","V","F","G" };
    if (ui::button({ dx, dy, 260, 26 }, std::string("단축키: ") + slotLbl[s.slot+1]))
        s.slot = (s.slot + 2) % 7 - 1;       // cycle -1..5
    dy += 32;
    if (ui::button({ dx, dy, 260, 26 }, s.projectile ? "발사체: 예 (전방 직선)" : "발사체: 아니오"))
        s.projectile = !s.projectile;
    dy += 30;
    ui::intStepper({ dx, dy, 260, 24 }, "사거리(발사체)", s.range, 1, 1, 20); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "순간이동 칸", s.blink, 1, 0, 10); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "위력(%ATK)", s.powerPct, 10, 0, 1000); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "MP 소모", s.mpCost, 1, 0, 99); dy += 28;
    int cdTenths = (int)(s.cooldown * 10 + 0.5f);
    if (ui::intStepper({ dx, dy, 260, 24 }, "쿨다운(0.1초)", cdTenths, 1, 1, 200)) s.cooldown = cdTenths / 10.0f;
    dy += 32;

    // effect + sound asset assignment (cycle through registered assets)
    auto imgName = [&](int id){ const AssetEntry* e = p.assets.find(id); return e ? e->name : std::string("없음"); };
    auto cycle = [&](int& slot, AssetType t){
        auto list = p.assets.byType(t);
        int idx = -1; for (int i=0;i<(int)list.size();++i) if (list[i]->id==slot) idx=i;
        idx++; slot = (idx >= (int)list.size()) ? -1 : list[idx]->id;
    };
    if (ui::button({ dx, dy, 260, 26 }, std::string("이펙트: ") + imgName(s.effectAsset), s.effectAsset>=0))
        cycle(s.effectAsset, AssetType::Image);
    dy += 30;
    if (ui::button({ dx, dy, 260, 26 }, std::string("사운드: ") + imgName(s.soundAsset), s.soundAsset>=0))
        cycle(s.soundAsset, AssetType::Audio);
    dy += 34;

    if (ui::button({ dx, dy, 125, 28 }, "저장")) { p.save(); setStatus("스킬 저장됨."); }
    if (ui::button({ dx + 135, dy, 125, 28 }, "삭제", false)) {
        db.fieldSkills.erase(db.fieldSkills.begin() + skillSel_);
        skillSel_ = -1; p.save(); setStatus("스킬 삭제됨.");
    }
}

} // namespace tsukuru
