#include "core/Audio.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace tsukuru {

void Audio::init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (ready_) SetMasterVolume(masterVol_);
}

void Audio::loadSettings(const std::string& path) {
    settingsPath_ = path;
    std::error_code ec;
    if (fs::exists(path, ec)) {
        std::ifstream f(path);
        if (f) {
            try {
                nlohmann::json j; f >> j;
                masterVol_ = j.value("master", masterVol_);
                musicVol_  = j.value("music",  musicVol_);
                sfxVol_    = j.value("sfx",    sfxVol_);
            } catch (...) {}
        }
    }
    if (ready_) SetMasterVolume(masterVol_);
}

void Audio::saveSettings() const {
    if (settingsPath_.empty()) return;
    std::ofstream f(settingsPath_);
    if (f) f << nlohmann::json{{"master", masterVol_}, {"music", musicVol_}, {"sfx", sfxVol_}}.dump(2);
}

void Audio::setMasterVolume(float v) {
    masterVol_ = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (ready_) SetMasterVolume(masterVol_);
    saveSettings();
}
void Audio::setMusicVolume(float v) {
    musicVol_ = v < 0 ? 0 : (v > 1 ? 1 : v);
    if (ready_ && bgmLoaded_) SetMusicVolume(bgm_, musicVol_);
    saveSettings();
}
void Audio::setSfxVolume(float v) {
    sfxVol_ = v < 0 ? 0 : (v > 1 ? 1 : v);
    saveSettings();
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
    SetSoundVolume(it->second, volume * sfxVol_);
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
    SetSoundVolume(it->second, volume * sfxVol_);
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
    SetMusicVolume(bgm_, musicVol_);
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
