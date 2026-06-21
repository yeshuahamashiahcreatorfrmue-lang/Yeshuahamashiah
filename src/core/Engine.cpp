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
    auto it = assetPathCache_.find(assetId);
    if (it == assetPathCache_.end()) {
        std::string path = project_->assetFullPath(assetId);
        if (path.empty()) return textures_.get(path);   // don't cache unresolved ids
        it = assetPathCache_.emplace(assetId, std::move(path)).first;
    }
    return textures_.get(it->second);
}

void Engine::invalidateAsset(int assetId) {
    auto it = assetPathCache_.find(assetId);
    if (it != assetPathCache_.end()) {
        textures_.invalidate(it->second);   // free the GPU texture
        assetPathCache_.erase(it);          // drop the stale id->path entry
    }
}

void Engine::setMode(Mode m) {
    mode_ = m;
    if (m == Mode::Play && play_) play_->onEnter();
}

void Engine::startPlaytest() {
    state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY);
    setMode(Mode::Play);
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
    audio_.loadSfxFolder((std::filesystem::path(project_->dir) / "assets" / "sfx").string());

    editor_ = std::make_unique<Editor>(*this);
    play_   = std::make_unique<GamePlay>(*this);
    title_  = std::make_unique<TitleScreen>(*this);

    if (startModeSet_) {
        if (startMode_ == Mode::Play) {
            state_.newGame(project_->database, project_->startActor, project_->playerCharId,
                   project_->startMap, project_->startX, project_->startY);
        }
        setMode(startMode_);
    }

    int frame = 0;
    while (!WindowShouldClose() && !quit_) {
        float dt = GetFrameTime();
        audio_.update();

        // --- global UI zoom: logical size = window / uiScale ---
        int winW = GetScreenWidth(), winH = GetScreenHeight();
        // Ctrl + wheel zooms the whole UI like a web page.
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
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
}

void Engine::draw() {
    switch (mode_) {
        case Mode::Editor: editor_->draw(); break;
        case Mode::Title:  title_->draw();  break;
        case Mode::Play:   play_->draw();   break;
    }
}

} // namespace tsukuru
