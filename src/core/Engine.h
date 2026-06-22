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
#include "net/Net.h"

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
    void startPlaytestAt(int mapId, int x, int y);  // editor "이 맵에서 플레이" (F6)
    void startPlaytestScene(int sceneId);           // editor "▶ 시나리오 테스트"
    void startPlaytestScenes(const std::vector<int>& sceneIds);  // 맵 전체재생(여러 장면 연속)
    void startPlaytestDialogue(int dialogueId, int mapId, int x, int y); // "▶ 대화 테스트"

    // 에디터 장면 미리보기: 플레이 모드로 전환하지 않고 컷신을 오프스크린 텍스처에 렌더.
    // 에디터가 작은(좌측 하단)/큰(중앙) 화면으로 그 텍스처를 띄운다.
    void startScenePreview(const std::vector<int>& sceneIds);
    void stopScenePreview();
    bool scenePreviewActive() const { return scenePreview_; }
    const Texture2D& scenePreviewTexture() const { return scenePreviewRT_.texture; }
    const std::string& scenePreviewName() const { return scenePreviewName_; }

    Project&      project()  { return *project_; }
    GameState&    state()    { return state_; }
    TextureCache& textures() { return textures_; }
    Audio&        audio()    { return audio_; }
    Net&          net()      { return net_; }

    const Texture2D& assetTexture(int assetId); // convenience: asset id -> texture
    std::string assetPath(int assetId) const { return project_->assetFullPath(assetId); }
    void invalidateAsset(int assetId);          // drop cached texture + path for a removed asset

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
    Net                      net_;
    Mode                     mode_ = Mode::Editor;
    bool                     quit_ = false;
    Mode                     startMode_ = Mode::Editor;
    bool                     startModeSet_ = false;
    std::string              shotPath_;

    std::unique_ptr<Editor>      editor_;
    std::unique_ptr<GamePlay>    play_;
    std::unique_ptr<TitleScreen> title_;

    // Webpage-style global UI zoom: the frame is drawn to a logical-size render
    // texture and scaled up to the window. Adjustable via Ctrl+wheel / on-screen
    // buttons. frameRT_ is (re)created when the window or scale changes.
    RenderTexture2D frameRT_{};
    int  rtW_ = 0, rtH_ = 0;
    void drawUiScaleBar();   // bottom-centre zoom control (drawn in logical space)

    // 장면 미리보기(에디터): 컷신을 별도 RT에 렌더해 에디터가 PiP로 보여준다.
    bool            scenePreview_ = false;
    RenderTexture2D scenePreviewRT_{};
    std::string     scenePreviewName_;
    void renderScenePreview();   // play_->draw()를 scenePreviewRT_에 렌더(메인 프레임 전에 호출)
};

} // namespace tsukuru
