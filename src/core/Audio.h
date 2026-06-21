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
    void playSfxFile(const std::string& path, float volume = 1.0f); // cache + play by path

    void playBgm(const std::string& path);          // streams + loops; ignores repeats
    void stopBgm();

    bool ready() const { return ready_; }

    // --- volume (0..1) — master scales everything; music/sfx are relative ---
    void  setMasterVolume(float v);
    void  setMusicVolume(float v);
    void  setSfxVolume(float v);
    float masterVolume() const { return masterVol_; }
    float musicVolume()  const { return musicVol_; }
    float sfxVolume()    const { return sfxVol_; }

private:
    bool ready_ = false;
    std::unordered_map<std::string, Sound> sfx_;
    Music bgm_{};
    bool  bgmLoaded_ = false;
    std::string bgmPath_;
    float masterVol_ = 0.75f;   // raylib master volume
    float musicVol_  = 0.6f;    // relative BGM volume
    float sfxVol_    = 1.0f;    // relative SFX volume
};

} // namespace tsukuru
