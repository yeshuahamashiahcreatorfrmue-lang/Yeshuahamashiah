#include "core/Audio.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace tsukuru {

void Audio::init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (ready_) SetMasterVolume(0.75f);
}

void Audio::shutdown() {
    if (!ready_) return;
    stopBgm();
    for (auto& kv : sfx_) UnloadSound(kv.second);
    sfx_.clear();
    CloseAudioDevice();
    ready_ = false;
}

void Audio::loadSfxFolder(const std::string& dir) {
    if (!ready_) return;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        auto p = e.path();
        std::string ext = p.extension().string();
        for (auto& c : ext) c = (char)tolower(c);
        if (ext != ".wav" && ext != ".ogg") continue;
        Sound s = LoadSound(p.string().c_str());
        sfx_[p.stem().string()] = s;
    }
}

void Audio::playSfx(const std::string& name, float volume) {
    if (!ready_) return;
    auto it = sfx_.find(name);
    if (it == sfx_.end()) return;
    SetSoundVolume(it->second, volume);
    PlaySound(it->second);
}

void Audio::playSfxFile(const std::string& path, float volume) {
    if (!ready_ || path.empty()) return;
    auto it = sfx_.find(path);
    if (it == sfx_.end()) {                 // load + cache by full path on first use
        std::error_code ec;
        if (!fs::exists(path, ec)) return;
        Sound s = LoadSound(path.c_str());
        it = sfx_.emplace(path, s).first;
    }
    SetSoundVolume(it->second, volume);
    PlaySound(it->second);
}

void Audio::playBgm(const std::string& path) {
    if (!ready_) return;
    if (bgmLoaded_ && path == bgmPath_) return; // already playing this track
    stopBgm();
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec)) return;
    bgm_ = LoadMusicStream(path.c_str());
    bgm_.looping = true;
    PlayMusicStream(bgm_);
    SetMusicVolume(bgm_, 0.6f);
    bgmLoaded_ = true;
    bgmPath_ = path;
}

void Audio::stopBgm() {
    if (!ready_ || !bgmLoaded_) return;
    StopMusicStream(bgm_);
    UnloadMusicStream(bgm_);
    bgmLoaded_ = false;
    bgmPath_.clear();
}

void Audio::update() {
    if (ready_ && bgmLoaded_) UpdateMusicStream(bgm_);
}

} // namespace tsukuru
