#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "core/Platform.h"
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
        if (s == "world") tab_ = Tab::World;     else if (s == "worldview") tab_ = Tab::WorldView;
        else if (s == "map") tab_ = Tab::Map;
        else if (s == "npc") tab_ = Tab::Npc;
        else if (s == "events") tab_ = Tab::Events;
        else if (s == "chars") tab_ = Tab::Chars; else if (s == "mob") tab_ = Tab::Mob;
        else if (s == "dialogue") tab_ = Tab::Dialogue; else if (s == "scenario") tab_ = Tab::Scenario;
        else if (s == "assets") tab_ = Tab::Assets;
        else if (s == "db") tab_ = Tab::Database;
    }
    if (const char* pk = getenv("TSUKURU_PICKER")) pickerId_ = atoi(pk); // debug: force-open a dropdown
    if (getenv("TSUKURU_CHARBROWSER")) openCharBrowser([](int){});       // debug: open the char browser
    if (getenv("TSUKURU_SEEDSTORY")) {             // debug: seed a dialogue+scene to view panels
        Database& d = engine_.project().database;
        Scene sc; sc.id = 1; sc.name = "도입 장면";
        sc.actions.push_back({ SA_MoveChar, 0, -1, 5, 6, 1.0f, 1, {} });
        sc.actions.push_back({ SA_Dialogue, -1, 1, 0, 0, 0.0f, 1, {} });
        d.scenes.push_back(sc);
        DialogueScenario dl; dl.id = 1; dl.name = "촌장 대화";
        DialogueLine ln; ln.speaker = "촌장"; ln.text = "용사여, 무엇을 도와줄까?";
        DialogueAnswer a1; a1.text = "보상을"; a1.respType = DR_Reward; a1.rewardGold = 100;
        DialogueAnswer a2; a2.text = "시나리오"; a2.respType = DR_Scene; a2.sceneId = 1;
        ln.answers.push_back(a1); ln.answers.push_back(a2);
        dl.lines.push_back(ln);
        d.dialogues.push_back(dl);
        tab_ = Tab::Dialogue; dlgSel_ = 0; dlgLineSel_ = 0;
    }
    if (const char* c = getenv("TSUKURU_DBCAT")) { // debug: pick DB category + first entry
        tab_ = Tab::Database; dbCategory_ = atoi(c); dbSelected_ = 0;
        if (const char* s = getenv("TSUKURU_DBSEL")) dbSelected_ = atoi(s);
        if (const char* sc = getenv("TSUKURU_DBSCROLL")) dbScroll_ = (float)atoi(sc);
    }
    if (const char* e = getenv("TSUKURU_EVSEL")) { tab_ = Tab::Events; editingEventId_ = atoi(e); } // debug
    if (getenv("TSUKURU_STARTSET")) { tab_ = Tab::World; worldStartSettings_ = true; }  // debug
    if (const char* sc = getenv("TSUKURU_EVSCROLL")) { eventScroll_ = (float)atoi(sc); eventScrollId_ = atoi(getenv("TSUKURU_EVSEL")?getenv("TSUKURU_EVSEL"):"-1"); }
    if (const char* t = getenv("TSUKURU_TOOL")) { if (std::string(t) == "stamp") tool_ = Tool::Stamp; }
    if (getenv("TSUKURU_OBJ")) { tab_ = Tab::Map; objMode_ = true; }   // debug: object mode
    if (getenv("TSUKURU_PREVIEW")) {           // debug: open the fullscreen map preview
        tab_ = Tab::World; worldSelected_ = 0; worldPreviewFull_ = true;
        Project& pp = engine_.project();
        worldPreviewMapId_ = pp.maps.empty() ? -1 : pp.maps.front()->id;
        for (int i = 0; i < (int)pp.maps.size(); ++i)   // prefer a placed map (shows zone gates)
            if (pp.maps[i]->placed) { worldSelected_ = i; worldPreviewMapId_ = pp.maps[i]->id; break; }
        if (getenv("TSUKURU_OVERLAP")) {                 // debug: pile markers on one tile
            if (auto m = pp.map(worldPreviewMapId_))
                for (int k = 0; k < 5; ++k) m->mobSpawns.push_back({ 1, 12, 9 });
        }
    }
}

std::shared_ptr<Map> Editor::activeMap() {
    Project& p = engine_.project();
    if (activeMapId_ >= 0) { if (auto m = p.map(activeMapId_)) return m; }
    if (!p.maps.empty()) { activeMapId_ = p.maps.front()->id; return p.maps.front(); }
    return nullptr;
}
bool Editor::wantsCtrlWheel() const { return tab_ == Tab::WorldView; }

void Editor::update(float dt) {
    if (statusTimer_ > 0) statusTimer_ -= dt;
    { static bool dbg = getenv("TSUKURU_CONFIRM") != nullptr, shown = false;   // debug: preview confirm dialog
      if (dbg && !shown) { shown = true; askConfirm("맵 '예슈아한민족진리복종마을' 을(를) 삭제할까요? 되돌릴 수 없습니다.", []{}); } }
    ensureThumbsForTab();   // build map thumbnails OUTSIDE the frame render texture
    // Open the native file dialog OUTSIDE the draw frame: showing a modal Win32
    // dialog mid-render (between BeginDrawing/EndDrawing) corrupts the GL frame
    // and crashes. Deferring it here (update runs before BeginDrawing) is safe.
    if (pendingImport_) { pendingImport_ = false; pickAndImportImages(); }
    if (pendingEffectImport_) { pendingEffectImport_ = false; pickAndImportEffect(); }
    if (pendingSoundImport_)  { pendingSoundImport_  = false; pickAndImportSound(); }
    if (pendingBgmImport_)    { pendingBgmImport_    = false; pickAndImportBgm(); }
    if (pendingItemIcon_)     { pendingItemIcon_     = false; pickAndImportItemIcon(); }
    if (pendingTilesetImport_){ pendingTilesetImport_= false; pickAndImportTileset(); }
    if (pendingNpcCharImport_){ pendingNpcCharImport_= false; pickAndImportNpcChar(); }
    if (pendingMapImport_)    { pendingMapImport_    = false; pickAndImportMapFile(); }
    if (pendingEfxFrameImport_){ pendingEfxFrameImport_= false; pickAndImportEfxFrame(); }

    // Enable the OS IME only while a text field is focused, so Korean names can be
    // typed there; everywhere else the IME is off so tool hotkeys/arrows aren't
    // swallowed by Hangul composition.
    bool anyFieldFocused = eventTextFocus_ || eventFieldFocus_ != 0 || dbNameFocus_ >= 0 ||
        dbDescFocus_ >= 0 || mapNameFocus_ || mapSearchFocus_ || skillNameFocus_ ||
        charDefNameFocus_ || charLibRenameFocus_ || charDataNameFocus_ >= 0 ||
        dlgFocus_ >= 0 || scnFocus_ >= 0 || searchFocusId_ >= 0;
    plat::setImeEnabled(anyFieldFocused);

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
    if (IsKeyPressed(KEY_F6)) {   // playtest starting on the current map (nearest walkable tile)
        if (auto m = activeMap()) {
            engine_.project().save();
            int cx = m->tilemap.width() / 2, cy = m->tilemap.height() / 2;
            int bx = cx, by = cy, best = 1 << 30;
            for (int yy = 0; yy < m->tilemap.height(); ++yy)
                for (int xx = 0; xx < m->tilemap.width(); ++xx)
                    if (!m->tilemap.blocked(xx, yy)) {
                        int d = (xx - cx) * (xx - cx) + (yy - cy) * (yy - cy);
                        if (d < best) { best = d; bx = xx; by = yy; }
                    }
            engine_.startPlaytestAt(m->id, bx, by);
        }
        return;
    }

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
    // Map camera zoom (multiplicative so huge maps zoom out smoothly; tiny minimum
    // lets a 1742×1742 map fit on screen). World/WorldView use their own cameras;
    // Ctrl+wheel stays reserved for the global UI scale.
    if (wheel != 0 && tab_ == Tab::Map && !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_RIGHT_CONTROL)) {
        cam_.zoom *= (1.0f + wheel * 0.15f);
        if (cam_.zoom < 0.02f) cam_.zoom = 0.02f;
        if (cam_.zoom > 4.0f)  cam_.zoom = 4.0f;
    }
    // Middle-drag pan (website-style grab-scroll)
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 d = GetMouseDelta();
        cam_.target.x -= d.x / cam_.zoom;
        cam_.target.y -= d.y / cam_.zoom;
    }
    // keep the map roughly in view so it can't be lost by over-panning
    if (tab_ == Tab::Map) {
        if (auto m = activeMap()) {
            int TS = m->tileset.tileWidth > 0 ? m->tileset.tileWidth : 32;
            float mw = (float)m->tilemap.width()*TS, mh = (float)m->tilemap.height()*TS;
            float marg = 300.0f / cam_.zoom;
            cam_.target.x = std::min(std::max(cam_.target.x, -marg), mw + marg);
            cam_.target.y = std::min(std::max(cam_.target.y, -marg), mh + marg);
        }
    }

    if (tab_ == Tab::Assets) handleAssetDrop();
}
void Editor::draw() {
    // While a confirm dialog is open, draw the tabs for context but block their
    // input so a click can't fall through to a button behind the dialog.
    // tab content is locked while a dropdown picker (or confirm dialog) is open,
    // so clicks only reach the open overlay.
    ui::g_inputEnabled = !confirmOpen_ && pickerId_ < 0 && !charBrowserOpen_;
    switch (tab_) {
        case Tab::World:    drawWorldTab();    break;
        case Tab::WorldView: drawWorldViewTab(); break;
        case Tab::Map:      drawMapTab();      break;
        case Tab::Npc:      drawNpcTab();      break;
        case Tab::Events:   drawEventsTab();   break;
        case Tab::Chars:    mobMode_ = false; drawCharsTab(); break;
        case Tab::Mob:      mobMode_ = true;  drawCharsTab(); break;   // same builder, db.mobs
        case Tab::Dialogue: drawDialogueTab(); break;
        case Tab::Scenario: drawScenarioTab(); break;
        case Tab::Assets:   drawAssetsTab();   break;
        case Tab::Database: drawDatabaseTab(); break;
    }
    ui::g_inputEnabled = !confirmOpen_ && !charBrowserOpen_;   // toolbar usable with a picker open
    drawToolbar();

    if (statusTimer_ > 0) {
        int w = MeasureTextU(status_.c_str(), 16);
        DrawRectangle(screenW() - w - 28, screenH() - 34, w + 20, 26, ui::kAccent);
        DrawTextU(status_.c_str(), screenW() - w - 18, screenH() - 30, 16, BLACK);
    }

    ui::g_inputEnabled = true;   // overlays accept input
    drawPickerOverlay();
    if (charBrowserOpen_) drawCharBrowser();
    if (confirmOpen_) drawConfirmOverlay();
}

// Centered modal: a message plus 확인 / 취소. Runs the stored action on confirm.
void Editor::drawConfirmOverlay() {
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
    Rectangle box = { sw/2.0f - 220, sh/2.0f - 90, 440, 180 };
    ui::panel(box);
    DrawTextU("확인", (int)box.x + 18, (int)box.y + 14, 22, ui::kDanger);
    // wrap the message across the box width
    DrawTextU(confirmMsg_.c_str(), (int)box.x + 18, (int)box.y + 56, 18, ui::kText);
    Rectangle yes = { box.x + 40, box.y + box.height - 52, 160, 38 };
    Rectangle no  = { box.x + box.width - 200, box.y + box.height - 52, 160, 38 };
    if (ui::button(yes, "확인 (삭제)", false)) {
        confirmOpen_ = false;
        if (confirmAction_) confirmAction_();
        confirmAction_ = nullptr;
    }
    if (ui::button(no, "취소", false) || IsKeyPressed(KEY_ESCAPE)) {
        confirmOpen_ = false;
        confirmAction_ = nullptr;
    }
}

void Editor::drawToolbar() {
    int sw = screenW();
    ui::panel({ 0, 0, (float)sw, kToolbarH }, ui::kPanelHi);

    float x = 6;
    auto tabBtn = [&](const char* name, Tab t) {
        if (ui::button({ x, 6, 58, 28 }, name, tab_ == t)) { tab_ = t; pickerId_ = -1; pickerApply_ = nullptr; }
        x += 60;
    };
    tabBtn("월드", Tab::World);
    tabBtn("전맵뷰어", Tab::WorldView);
    tabBtn("맵", Tab::Map);
    tabBtn("NPC", Tab::Npc);
    tabBtn("이벤트", Tab::Events);
    tabBtn("캐릭터", Tab::Chars);
    tabBtn("몹", Tab::Mob);
    tabBtn("대화", Tab::Dialogue);
    tabBtn("시나리오", Tab::Scenario);
    tabBtn("에셋", Tab::Assets);
    tabBtn("DB", Tab::Database);

    x += 10;
    if (ui::button({ x, 6, 64, 28 }, "저장")) { engine_.project().save(); setStatus("저장됨."); }
    x += 68;
    if (ui::button({ x, 6, 88, 28 }, "플레이(F5)", false)) { engine_.project().save(); engine_.startPlaytest(); }
    x += 92;
    if (ui::button({ x, 6, 104, 28 }, "맵테스트(F6)", false)) {
        if (auto m = activeMap()) {
            engine_.project().save();
            int cx = m->tilemap.width()/2, cy = m->tilemap.height()/2, bx = cx, by = cy, best = 1<<30;
            for (int yy = 0; yy < m->tilemap.height(); ++yy)
                for (int xx = 0; xx < m->tilemap.width(); ++xx)
                    if (!m->tilemap.blocked(xx, yy)) { int d=(xx-cx)*(xx-cx)+(yy-cy)*(yy-cy); if(d<best){best=d;bx=xx;by=yy;} }
            engine_.startPlaytestAt(m->id, bx, by);
        }
    }

    // Map-specific tools on the right
    if (tab_ == Tab::Map) {
        float bw = 52, gap = 54;
        float rx = sw - 8 - (6*gap + 72);             // 6 tool buttons + a wider 오브젝트 button
        auto tbtn=[&](const char* n, Tool t){ if (ui::button({rx,6,bw,28},n, tool_==t && !collisionMode_ && !objMode_)){tool_=t;collisionMode_=false;objMode_=false;} rx+=gap; };
        tbtn("펜",Tool::Pencil); tbtn("지우개",Tool::Erase); tbtn("채우기",Tool::Fill);
        tbtn("사각형",Tool::Rect); tbtn("스탬프",Tool::Stamp);
        if (ui::button({ rx, 6, bw, 28 }, "충돌", collisionMode_)) { collisionMode_ = !collisionMode_; if(collisionMode_) objMode_=false; } rx += gap;
        if (ui::button({ rx, 6, 72, 28 }, "오브젝트", objMode_)) { objMode_ = !objMode_; if(objMode_) collisionMode_=false; }
    }
}

} // namespace tsukuru
