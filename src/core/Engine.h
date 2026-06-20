#pragma once
// Engine: owns the window, the loaded project, the shared texture cache, and the
// active game state. Drives the main loop and switches between the three modes:
//   Editor  -> build maps, register assets, edit the database
//   Title   -> the game's title screen (New Game / Continue / Quit)
//   Play    -> walk the world, talk to events, fight battles
#include <memory>
#include <string>
#include <unordered_map>
#include "project/Project.h"
#include "game/GameState.h"
#include "render/TextureCache.h"
#include "core/Audio.h"

namespace tsukuru {

class Editor;
class GamePlay;
class TitleScreen;

enum class Mode { Editor, Title, Play };

class Engine {
public:
    Engine();
    ~Engine();

    // Boots the window, loads (or creates) a project, and runs the main loop.
    // maxFrames > 0 auto-quits after that many frames (used for headless tests).
    int run(const std::string& projectDir, int maxFrames = 0);

    // Headless-test helpers (no effect on normal interactive use).
    void setStartMode(Mode m) { startMode_ = m; startModeSet_ = true; }
    void setScreenshot(const std::string& path) { shotPath_ = path; }

    void setMode(Mode m);
    Mode mode() const { return mode_; }

    // Start a fresh playtest immediately (used by the editor's Play / F5).
    void startPlaytest();

    Project&      project()  { return *project_; }
    GameState&    state()    { return state_; }
    TextureCache& textures() { return textures_; }
    Audio&        audio()    { return audio_; }

    const Texture2D& assetTexture(int assetId); // convenience: asset id -> texture
    std::string assetPath(int assetId) const { return project_->assetFullPath(assetId); }

    void requestQuit() { quit_ = true; }

private:
    void update(float dt);
    void draw();

    std::shared_ptr<Project> project_;
    // asset id -> resolved full path. relPath is immutable per id within a loaded
    // project (ids are never reused), so this stays valid for the whole session and
    // spares the per-frame fs::path build + lookup in the render loop.
    std::unordered_map<int, std::string> assetPathCache_;
    GameState                state_;
    TextureCache             textures_;
    Audio                    audio_;
    Mode                     mode_ = Mode::Editor;
    bool                     quit_ = false;
    Mode                     startMode_ = Mode::Editor;
    bool                     startModeSet_ = false;
    std::string              shotPath_;

    std::unique_ptr<Editor>      editor_;
    std::unique_ptr<GamePlay>    play_;
    std::unique_ptr<TitleScreen> title_;
};

} // namespace tsukuru
