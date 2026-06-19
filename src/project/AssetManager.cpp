#include "project/AssetManager.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using nlohmann::json;

namespace tsukuru {

static const char* typeName(AssetType t) {
    return t == AssetType::Image ? "image" : "audio";
}
static AssetType typeFromName(const std::string& s) {
    return s == "audio" ? AssetType::Audio : AssetType::Image;
}

int AssetManager::registerAsset(const std::string& projectDir,
                                const std::string& sourceFile,
                                AssetType type,
                                const std::string& displayName) {
    std::error_code ec;
    if (!fs::exists(sourceFile, ec)) return -1;

    fs::path destDir = fs::path(projectDir) / "assets";
    fs::create_directories(destDir, ec);

    fs::path src(sourceFile);
    fs::path dest = destDir / src.filename();
    // Avoid clobbering: if a different file with same name exists, suffix it.
    int dup = 1;
    while (fs::exists(dest, ec) && !fs::equivalent(src, dest, ec)) {
        dest = destDir / (src.stem().string() + "_" + std::to_string(dup++) + src.extension().string());
    }
    if (!fs::equivalent(src, dest, ec)) {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) return -1;
    }

    AssetEntry e;
    e.id      = nextId_++;
    e.type    = type;
    e.name    = displayName.empty() ? dest.stem().string() : displayName;
    e.relPath = (fs::path("assets") / dest.filename()).generic_string();
    assets_.push_back(e);
    return e.id;
}

int AssetManager::addExisting(AssetType type, const std::string& name, const std::string& relPath) {
    AssetEntry e;
    e.id      = nextId_++;
    e.type    = type;
    e.name    = name;
    e.relPath = relPath;
    assets_.push_back(e);
    return e.id;
}

void AssetManager::setAnim(int id, int frames, int fps) {
    for (auto& a : assets_) if (a.id == id) {
        a.frames = frames < 1 ? 1 : frames;
        a.fps = fps < 1 ? 1 : fps;
        return;
    }
}

void AssetManager::remove(int id) {
    assets_.erase(std::remove_if(assets_.begin(), assets_.end(),
                  [id](const AssetEntry& a){ return a.id == id; }), assets_.end());
}

const AssetEntry* AssetManager::find(int id) const {
    for (const auto& a : assets_) if (a.id == id) return &a;
    return nullptr;
}

std::vector<const AssetEntry*> AssetManager::byType(AssetType type) const {
    std::vector<const AssetEntry*> out;
    for (const auto& a : assets_) if (a.type == type) out.push_back(&a);
    return out;
}

json AssetManager::toJson() const {
    json arr = json::array();
    for (const auto& a : assets_) {
        arr.push_back({{"id", a.id}, {"type", typeName(a.type)},
                       {"name", a.name}, {"path", a.relPath},
                       {"frames", a.frames}, {"fps", a.fps}});
    }
    return {{"nextId", nextId_}, {"items", arr}};
}

void AssetManager::fromJson(const json& j) {
    assets_.clear();
    nextId_ = j.value("nextId", 1);
    if (j.contains("items")) {
        for (const auto& it : j["items"]) {
            AssetEntry e;
            e.id      = it.value("id", -1);
            e.type    = typeFromName(it.value("type", "image"));
            e.name    = it.value("name", "");
            e.relPath = it.value("path", "");
            e.frames  = it.value("frames", 1);
            e.fps     = it.value("fps", 8);
            assets_.push_back(e);
        }
    }
}

} // namespace tsukuru
