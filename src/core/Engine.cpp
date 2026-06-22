#include "core/Engine.h"
#include "core/Text.h"
#include <cstdlib>
#include "core/GlyphSet.h"
#include "editor/Editor.h"
#include "game/GamePlay.h"
#include "game/TitleScreen.h"
#include "render/UI.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace tsukuru {

Engine::Engine() = default;
Engine::~Engine() = default;

const Texture2D& Engine::assetTexture(int assetId) {
    // Fast path: id -> texture (one int-hash lookup, no string hashing).
    auto ti = texByIdCache_.find(assetId);
    if (ti != texByIdCache_.end()) return ti->second;

    auto it = assetPathCache_.find(assetId);
    if (it == assetPathCache_.end()) {
        std::string path = project_->assetFullPath(assetId);
        if (path.empty()) return textures_.get(path);   // don't cache unresolved ids
        it = assetPathCache_.emplace(assetId, std::move(path)).first;
    }
    const Texture2D& tex = textures_.get(it->second);   // owns/loads the GPU texture
    return texByIdCache_.emplace(assetId, tex).first->second;  // cache the id->texture copy
}

void Engine::invalidateAsset(int assetId) {
    texByIdCache_.erase(assetId);           // drop the id->texture fast-path entry
    auto it = assetPathCache_.find(assetId);
    if (it != assetPathCache_.end()) {
        textures_.invalidate(it->second);   // free the GPU texture
        assetPathCache_.erase(it);          // drop the stale id->path entry
    }
}

void Engine::setMode(Mode m) {
    // 플레이/테스트에서 에디터로 나오면 배경음을 끈다(에디터와 겹치지 않게).
    if (m == Mode::Editor && mode_ != Mode::Editor) audio_.stopBgm();   // 에디터로 나오면 음악 정지
    mode_ = m;
    if (m == Mode::Play && play_) play_->onEnter();
}

void Engine::startPlaytest() {
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
    setMode(Mode::Play);
}

// Playtest starting on a specific map/tile (editor "이 맵에서 플레이 F6").
void Engine::startPlaytestAt(int mapId, int x, int y) {
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
    state_.currentMap = mapId; state_.playerX = x; state_.playerY = y;
    setMode(Mode::Play);
}

// Editor 테스트: jump into play on the scene's editing map and run the scene now.
void Engine::startPlaytestScene(int sceneId) {
    int mapId = project_->startMap;
    for (const auto& s : project_->database.scenes)
        if (s.id == sceneId && s.editMapId >= 0) mapId = s.editMapId;
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
    state_.currentMap = mapId;
    if (auto m = project_->map(mapId)) { state_.playerX = m->tilemap.width()/2; state_.playerY = m->tilemap.height()/2; }
    setMode(Mode::Play);                 // onEnter() loads the map
    if (play_) play_->beginScene(sceneId);
}

// 맵 전체재생: 같은 맵의 여러 장면을 차례로 재생.
void Engine::startPlaytestScenes(const std::vector<int>& sceneIds) {
    if (sceneIds.empty()) return;
    int mapId = project_->startMap;
    for (const auto& s : project_->database.scenes)
        if (s.id == sceneIds[0] && s.editMapId >= 0) mapId = s.editMapId;
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
    state_.currentMap = mapId;
    if (auto m = project_->map(mapId)) { state_.playerX = m->tilemap.width()/2; state_.playerY = m->tilemap.height()/2; }
    setMode(Mode::Play);
    if (play_) play_->beginSceneChain(sceneIds);
}

// 에디터 장면 미리보기 시작: 플레이 모드로 전환하지 않고 컷신만 재생하도록 play_를
// 준비한다(setPreviewMode→onEnter→beginScene/Chain). 렌더는 renderScenePreview()가 담당.
void Engine::startScenePreview(const std::vector<int>& sceneIds) {
    if (sceneIds.empty() || !play_) return;
    int mapId = project_->startMap;
    scenePreviewName_.clear();
    for (const auto& s : project_->database.scenes) {
        if (s.id == sceneIds[0]) { if (s.editMapId >= 0) mapId = s.editMapId; scenePreviewName_ = s.name; }
    }
    if (sceneIds.size() > 1) scenePreviewName_ += TextFormat(" 외 %d개", (int)sceneIds.size()-1);
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
    state_.currentMap = mapId;
    // 에디터 시나리오 편집과 동일하게 플레이어(태그0)를 그 맵 중앙에서 시작시킨다
    // (에디터의 stage[0]=맵 중앙과 일치 → 장면이 편집한 그대로 재생됨).
    if (auto m = project_->map(mapId)) {
        state_.playerX = m->tilemap.width() / 2;
        state_.playerY = m->tilemap.height() / 2;
    } else { state_.playerX = project_->startX; state_.playerY = project_->startY; }
    play_->setPreviewMode(true);
    play_->setPreviewInteractive(false);   // 시작은 작은(자동) 미리보기
    play_->onEnter();                 // 맵 로드(미리보기 모드)
    if (sceneIds.size() == 1) play_->beginScene(sceneIds[0]);
    else play_->beginSceneChain(sceneIds);
    scenePreview_ = true;
}

void Engine::stopScenePreview() {
    scenePreview_ = false;
    audio_.stopBgm();   // 미리보기 종료 시 음악 정지(에디터와 겹치지 않게)
    if (play_) { play_->setPreviewMode(false); play_->setPreviewInteractive(false); }
}

void Engine::setScenePreviewInteractive(bool v) {
    if (play_) play_->setPreviewInteractive(v);
}

// Render the running scene into scenePreviewRT_ at a fixed 640×360. Called from
// update() (before the main frame RT is bound) so the BeginTextureMode isn't nested.
void Engine::renderScenePreview() {
    if (!scenePreview_ || !play_) return;
    if (scenePreviewRT_.id == 0) {
        scenePreviewRT_ = LoadRenderTexture(640, 360);
        SetTextureFilter(scenePreviewRT_.texture, TEXTURE_FILTER_BILINEAR);
    }
    int savedW = screenW(), savedH = screenH();
    float scl = uiScale();
    setLogicalScreen(std::max(320, (int)(640 / scl)), std::max(180, (int)(360 / scl)));
    BeginTextureMode(scenePreviewRT_);
    ClearBackground(Color{ 12, 14, 20, 255 });
    uiBeginScaled();
    play_->draw();
    uiEndScaled();
    EndTextureMode();
    setLogicalScreen(savedW, savedH);
}

// Editor 테스트: jump into play near the NPC and run the dialogue now.
void Engine::startPlaytestDialogue(int dialogueId, int mapId, int x, int y) {
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
    if (mapId >= 0) state_.currentMap = mapId;
    if (x >= 0) state_.playerX = x;
    if (y >= 0) state_.playerY = y;
    setMode(Mode::Play);
    if (play_) play_->beginDialogue(dialogueId);
}

int Engine::run(const std::string& projectDir, int maxFrames) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "쯔꾸르 엔진 — RPG 메이커");
    SetExitKey(KEY_NULL); // we handle ESC ourselves
    SetTargetFPS(60);

    // Load the Korean-capable UI font (baked with just the glyphs we use).
    LoadUIFont("assets/korean.ttf", std::string(kUiGlyphs));

    // Load project, or create a fresh one if none exists.
    project_ = std::make_shared<Project>();
    if (!project_->load(projectDir)) {
        project_ = Project::createNew(projectDir, "My RPG");
    }

    audio_.init();
    audio_.loadSettings("settings.json");   // persisted volume preferences (app-dir relative)
    audio_.loadSfxFolder((std::filesystem::path(project_->dir) / "assets" / "sfx").string());

    editor_ = std::make_unique<Editor>(*this);
    play_   = std::make_unique<GamePlay>(*this);
    title_  = std::make_unique<TitleScreen>(*this);

    if (startModeSet_) {
        if (startMode_ == Mode::Play) {
            state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY,
                   project_->startGold, project_->startItems);
        }
        setMode(startMode_);
    }
    if (const char* ts = getenv("TSUKURU_TESTSCENE")) startPlaytestScene(atoi(ts));   // debug: run a scene
    if (const char* sp = getenv("TSUKURU_PREVIEW")) {                                  // debug: 에디터 장면 미리보기
        setMode(Mode::Editor); startScenePreview({ atoi(sp) });
    }

    int frame = 0;
    while (!WindowShouldClose() && !quit_) {
        float dt = GetFrameTime();
        audio_.update();

        // --- global UI zoom: logical size = window / uiScale ---
        int winW = GetScreenWidth(), winH = GetScreenHeight();
        // Ctrl + wheel zooms the whole UI like a web page — unless the active view
        // claims Ctrl+wheel for its own zoom (the All-Map Viewer).
        bool ctrlWheelClaimed = (mode_ == Mode::Editor && editor_ && editor_->wantsCtrlWheel());
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && !ctrlWheelClaimed) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) setUiScale(uiScale() + wheel * 0.1f);
        }
        float sc = uiScale();
        // Layout happens in LOGICAL space (window / uiScale) so elements reflow,
        // but we render into a NATIVE-resolution texture and scale the drawing up
        // with the modelview — so text is rasterized at full window density and
        // stays crisp instead of being magnified from a low-res frame.
        int lw = std::max(640, (int)(winW / sc)), lh = std::max(360, (int)(winH / sc));
        setLogicalScreen(lw, lh);
        if (rtW_ != winW || rtH_ != winH) {
            if (frameRT_.id) UnloadRenderTexture(frameRT_);
            frameRT_ = LoadRenderTexture(winW, winH);
            SetTextureFilter(frameRT_.texture, TEXTURE_FILTER_POINT);
            rtW_ = winW; rtH_ = winH;
        }
        // map mouse into logical space so all hit-testing matches the scaled view
        SetMouseScale(1.0f / sc, 1.0f / sc);

        update(dt);

        BeginTextureMode(frameRT_);
        ClearBackground(Color{ 18, 20, 26, 255 });
        uiBeginScaled();      // logical -> native modelview scale
        draw();
        drawUiScaleBar();
        uiEndScaled();
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        // blit the native frame to the window 1:1 (flip Y for RT) — no magnify
        DrawTexturePro(frameRT_.texture,
                       { 0, 0, (float)winW, -(float)winH },
                       { 0, 0, (float)winW, (float)winH }, { 0, 0 }, 0, WHITE);
        EndDrawing();
        ++frame;
        if (!shotPath_.empty() && maxFrames > 0 && frame == maxFrames - 1)
            TakeScreenshot(shotPath_.c_str());
        if (maxFrames > 0 && frame >= maxFrames) break;
    }
    if (frameRT_.id) UnloadRenderTexture(frameRT_);
    if (scenePreviewRT_.id) UnloadRenderTexture(scenePreviewRT_);

    audio_.shutdown();
    textures_.clear();
    UnloadUIFont();
    CloseWindow();
    return 0;
}

// Bottom-centre UI zoom control (글자/패널 크기). Drawn in logical space so its
// own mouse hit-testing matches; visible in every mode. Ctrl+wheel also zooms.
void Engine::drawUiScaleBar() {
    int sw = screenW(), sh = screenH();
    float h = 24, y = sh - h - 5, cx = sw / 2.0f - 132;
    DrawRectangle((int)cx - 8, (int)y - 4, 290, (int)h + 8, Fade(BLACK, 0.6f));
    DrawTextU("UI 크기", (int)cx, (int)y + 4, 14, ui::kText);
    if (ui::button({ cx + 66, y, 30, h }, "-"))   setUiScale(uiScale() - 0.1f);
    DrawTextU(TextFormat("%d%%", (int)(uiScale() * 100 + 0.5f)), (int)cx + 104, (int)y + 4, 15, ui::kAccentHi);
    if (ui::button({ cx + 150, y, 30, h }, "+"))  setUiScale(uiScale() + 0.1f);
    if (ui::button({ cx + 188, y, 40, h }, "1x"))  setUiScale(1.0f);
    if (ui::button({ cx + 232, y, 40, h }, "2x"))  setUiScale(2.0f);
}

void Engine::update(float dt) {
    // One-time multiplayer auto-start via env (handy for testing/headless):
    //   TSUKURU_HOST=7777            -> host an MMO server (up to 42)
    //   TSUKURU_JOIN=1.2.3.4:7777    -> join a host
    static bool netEnvDone = false;
    if (!netEnvDone) {
        netEnvDone = true;
        if (const char* h = getenv("TSUKURU_HOST")) net_.startHost(atoi(h), 42);
        else if (const char* j = getenv("TSUKURU_JOIN")) {
            std::string s = j; auto c = s.find(':');
            std::string ip = c == std::string::npos ? s : s.substr(0, c);
            int port = c == std::string::npos ? 7777 : atoi(s.c_str() + c + 1);
            net_.startClient(ip, port);
        }
    }
    // sync the local player to the network every frame (zone = current map)
    if (net_.active()) {
        NetPlayer lp; lp.id = net_.myId();
        lp.mapId = state_.currentMap; lp.x = state_.playerX; lp.y = state_.playerY;
        lp.dir = state_.playerDir; lp.charId = project_ ? project_->playerCharId : -1;
        net_.update(dt, lp);
    }
    switch (mode_) {
        case Mode::Editor: editor_->update(dt); break;
        case Mode::Title:  title_->update(dt);  break;
        case Mode::Play:   play_->update(dt);   break;
    }
    // 에디터 장면 미리보기: 컷신을 진행시키고 오프스크린 RT에 렌더(메인 프레임 RT 바인딩 전).
    if (mode_ == Mode::Editor && scenePreview_ && play_) {
        play_->update(dt);
        renderScenePreview();
    }
}

void Engine::draw() {
    switch (mode_) {
        case Mode::Editor: editor_->draw(); break;
        case Mode::Title:  title_->draw();  break;
        case Mode::Play:   play_->draw();   break;
    }
}

} // namespace tsukuru
