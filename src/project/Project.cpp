#include "project/Project.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;
using nlohmann::json;

namespace tsukuru {

static bool writeJson(const std::string& path, const json& j) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream f(path);
    if (!f) return false;
    f << j.dump(2);
    return true;
}
static bool readJson(const std::string& path, json& out) {
    std::ifstream f(path);
    if (!f) return false;
    try { f >> out; } catch (...) { return false; }
    return true;
}

std::string Project::mapPath(int id) const {
    return (fs::path(dir) / "maps" / (std::to_string(id) + ".json")).string();
}

std::string Project::assetFullPath(int assetId) const {
    const AssetEntry* e = assets.find(assetId);
    if (!e) return "";
    return (fs::path(dir) / e->relPath).string();
}

int Project::nextMapId() const {
    int m = 0;
    for (const auto& mp : maps) m = std::max(m, mp->id);
    return m + 1;
}

std::shared_ptr<Map> Project::map(int id) {
    for (auto& m : maps) if (m->id == id) return m;
    if (loadMap(id)) {
        for (auto& m : maps) if (m->id == id) return m;
    }
    return nullptr;
}

std::shared_ptr<Map> Project::addMap(const std::string& mapName, int w, int h) {
    auto m = std::make_shared<Map>();
    m->id = nextMapId();
    m->name = mapName;
    m->tilemap.resize(w, h);
    maps.push_back(m);
    return m;
}

bool Project::load(const std::string& projectDir) {
    dir = projectDir;
    json meta;
    if (!readJson((fs::path(dir) / "project.json").string(), meta)) return false;

    name        = meta.value("name", "Untitled");
    startMap    = meta.value("startMap", -1);
    startX      = meta.value("startX", 0);
    startY      = meta.value("startY", 0);
    startActor  = meta.value("startActor", -1);
    playerSprite= meta.value("playerSprite", -1);
    playerFrames= meta.value("playerFrames", 4);
    playerAtkFrames = meta.value("playerAtkFrames", 0);
    playerCharId = meta.value("playerCharId", -1);
    if (meta.contains("assets")) assets.fromJson(meta["assets"]);

    json db;
    if (readJson((fs::path(dir) / "database.json").string(), db)) database.fromJson(db);

    // Load all maps listed in meta.maps (ids).
    maps.clear();
    for (int id : meta.value("maps", std::vector<int>{})) loadMap(id);
    return true;
}

bool Project::loadMap(int id) {
    json mj;
    if (!readJson(mapPath(id), mj)) return false;
    auto m = std::make_shared<Map>();
    m->fromJson(mj);
    m->id = id;
    // replace if already present
    maps.erase(std::remove_if(maps.begin(), maps.end(),
               [id](const std::shared_ptr<Map>& mm){ return mm->id == id; }), maps.end());
    maps.push_back(m);
    return true;
}

bool Project::saveMap(const Map& m) const {
    return writeJson(mapPath(m.id), m.toJson());
}

bool Project::save() const {
    std::vector<int> ids;
    for (const auto& m : maps) { ids.push_back(m->id); saveMap(*m); }

    json meta = {
        {"name", name},
        {"startMap", startMap}, {"startX", startX}, {"startY", startY},
        {"startActor", startActor}, {"playerSprite", playerSprite},
        {"playerFrames", playerFrames}, {"playerAtkFrames", playerAtkFrames},
        {"playerCharId", playerCharId},
        {"assets", assets.toJson()},
        {"maps", ids}
    };
    bool ok = writeJson((fs::path(dir) / "project.json").string(), meta);
    ok = writeJson((fs::path(dir) / "database.json").string(), database.toJson()) && ok;
    return ok;
}

std::shared_ptr<Project> Project::createNew(const std::string& projectDir, const std::string& name) {
    std::error_code ec;
    fs::create_directories(fs::path(projectDir) / "maps", ec);
    fs::create_directories(fs::path(projectDir) / "assets", ec);
    auto p = std::make_shared<Project>();
    p->dir = projectDir;
    p->name = name;
    auto m = p->addMap("Map001", 20, 15);
    p->startMap = m->id;
    p->startX = 5; p->startY = 5;
    p->save();
    return p;
}

} // namespace tsukuru
