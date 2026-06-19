#include "core/Engine.h"
#include "editor/Editor.h"
#include "game/GamePlay.h"
#include "game/TitleScreen.h"
#include "render/UI.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace tsukuru {

Engine::Engine() = default;
Engine::~Engine() = default;

const Texture2D& Engine::assetTexture(int assetId) {
    return textures_.get(project_->assetFullPath(assetId));
}

void Engine::setMode(Mode m) {
    mode_ = m;
    if (m == Mode::Play && play_) play_->onEnter();
}

int Engine::run(const std::string& projectDir, int maxFrames) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Tsukuru Engine — RPG Maker");
    SetExitKey(KEY_NULL); // we handle ESC ourselves
    SetTargetFPS(60);

    // Load project, or create a fresh one if none exists.
    project_ = std::make_shared<Project>();
    if (!project_->load(projectDir)) {
        project_ = Project::createNew(projectDir, "My RPG");
    }

    editor_ = std::make_unique<Editor>(*this);
    play_   = std::make_unique<GamePlay>(*this);
    title_  = std::make_unique<TitleScreen>(*this);

    if (startModeSet_) {
        if (startMode_ == Mode::Play) {
            state_.newGame(project_->database, project_->startActor,
                           project_->startMap, project_->startX, project_->startY);
        }
        setMode(startMode_);
    }

    int frame = 0;
    while (!WindowShouldClose() && !quit_) {
        float dt = GetFrameTime();
        update(dt);
        BeginDrawing();
        ClearBackground(Color{ 18, 20, 26, 255 });
        draw();
        EndDrawing();
        ++frame;
        if (!shotPath_.empty() && maxFrames > 0 && frame == maxFrames - 1)
            TakeScreenshot(shotPath_.c_str());
        if (maxFrames > 0 && frame >= maxFrames) break;
    }

    textures_.clear();
    CloseWindow();
    return 0;
}

void Engine::update(float dt) {
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
