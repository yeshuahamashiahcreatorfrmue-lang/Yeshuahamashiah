#pragma once
// Audio: streaming background music per map plus cached sound effects.
// All calls are safe no-ops when no audio device is available (e.g. headless
// CI), so the engine never crashes without sound hardware.
#include <string>
#include <unordered_map>
#include "raylib.h"

namespace tsukuru {

class Audio {
public:
    void init();
    void shutdown();
    void update();                                  // pump the music stream

    void loadSfxFolder(const std::string& dir);     // load every .wav in dir by stem name
    void playSfx(const std::string& name, float volume = 1.0f);

    void playBgm(const std::string& path);          // streams + loops; ignores repeats
    void stopBgm();

    bool ready() const { return ready_; }

private:
    bool ready_ = false;
    std::unordered_map<std::string, Sound> sfx_;
    Music bgm_{};
    bool  bgmLoaded_ = false;
    std::string bgmPath_;
};

} // namespace tsukuru
