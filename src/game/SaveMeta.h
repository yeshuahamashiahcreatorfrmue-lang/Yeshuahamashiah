#pragma once
// Lightweight reader for save-file metadata (level / location / play time / gold)
// shown on save slots and the title screen's "이어하기" without fully loading a
// GameState. Header-only so both Menu.cpp and TitleScreen.cpp share one copy.
#include <string>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace tsukuru {

struct SaveMeta {
    bool exists = false;
    int  level = 1;
    int  currentMap = -1;
    int  gold = 0;
    double playSeconds = 0;
    std::filesystem::file_time_type writeTime{};
};

// Read just the headline fields from a save JSON. Returns {exists=false} if the
// file is missing or unreadable.
inline SaveMeta readSaveMeta(const std::filesystem::path& f) {
    namespace fs = std::filesystem;
    SaveMeta m;
    std::error_code ec;
    if (!fs::exists(f, ec)) return m;
    std::ifstream in(f.string());
    if (!in) return m;
    try {
        nlohmann::json j; in >> j;
        m.exists = true;
        m.currentMap = j.value("currentMap", -1);
        m.gold = j.contains("inventory") ? j["inventory"].value("gold", 0) : 0;
        m.playSeconds = j.value("playSeconds", 0.0);
        if (j.contains("party") && j["party"].is_array() && !j["party"].empty())
            m.level = j["party"][0].value("level", 1);
        m.writeTime = fs::last_write_time(f, ec);
    } catch (...) { m.exists = false; }
    return m;
}

// "1시간 23분" / "5분 12초" style compact play-time label.
inline std::string formatPlayTime(double seconds) {
    int s = (int)seconds;
    int h = s / 3600, mm = (s % 3600) / 60, ss = s % 60;
    char buf[48];
    if (h > 0) snprintf(buf, sizeof(buf), "%d시간 %d분", h, mm);
    else       snprintf(buf, sizeof(buf), "%d분 %d초", mm, ss);
    return buf;
}

} // namespace tsukuru
