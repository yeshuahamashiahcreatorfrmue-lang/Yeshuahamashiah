#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
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

} // namespace tsukuru
