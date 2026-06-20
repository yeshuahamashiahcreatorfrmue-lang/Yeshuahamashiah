#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
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
    // Load the AI cut-out model if it shipped next to the exe. Tries a couple of
    // common locations; silently no-ops (classic removal) if absent or unbuilt.
    {
        std::string appDir = GetApplicationDirectory();
        const char* names[] = { "u2netp.onnx", "assets/u2netp.onnx" };
        for (const char* n : names) {
            std::string p = appDir + n;
            if (FileExists(p.c_str()) && seg_.load(p)) break;
            if (FileExists(n) && seg_.load(n)) break;   // fallback: current working dir
        }
    }
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
void Editor::update(float dt) {
    if (statusTimer_ > 0) statusTimer_ -= dt;
    // Open the native file dialog OUTSIDE the draw frame: showing a modal Win32
    // dialog mid-render (between BeginDrawing/EndDrawing) corrupts the GL frame
    // and crashes. Deferring it here (update runs before BeginDrawing) is safe.
    if (pendingImport_) { pendingImport_ = false; pickAndImportImages(); }
    if (pendingEffectImport_) { pendingEffectImport_ = false; pickAndImportEffect(); }

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

} // namespace tsukuru
