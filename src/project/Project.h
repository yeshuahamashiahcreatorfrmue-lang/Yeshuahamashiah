#pragma once
// Project: the top-level container that ties together assets, database, and maps.
// On disk a project is a directory:
//   <dir>/project.json   (meta + assets + start settings)
//   <dir>/database.json  (items/actors/enemies/skills)
//   <dir>/maps/<id>.json (one file per map)
//   <dir>/assets/...     (registered media)
#include <string>
#include <vector>
#include <memory>
#include "project/AssetManager.h"
#include "database/Database.h"
#include "world/Map.h"

namespace tsukuru {

class Project {
public:
    std::string dir;                 // project directory (absolute or relative)
    std::string name = "Untitled";
    AssetManager assets;
    Database     database;

    int startMap = -1;
    int startX = 0, startY = 0;
    int startActor = -1;             // actor id the party begins with
    int playerSprite = -1;          // asset id for the on-map player graphic
    int playerFrames = 4;           // walk frames per direction in the sheet (4..7)
    int playerAtkFrames = 0;        // attack frames appended after the walk frames

    // Loaded maps (lazy-managed; the editor keeps the active one).
    std::vector<std::shared_ptr<Map>> maps;

    // ---- persistence ----
    bool load(const std::string& projectDir);
    bool save() const;

    std::shared_ptr<Map> map(int id);
    std::shared_ptr<Map> addMap(const std::string& name, int w, int h);
    int nextMapId() const;

    std::string assetFullPath(int assetId) const; // dir + relPath, "" if missing

    // Create a fresh empty project on disk.
    static std::shared_ptr<Project> createNew(const std::string& projectDir, const std::string& name);

private:
    bool loadMap(int id);
    bool saveMap(const Map& m) const;
    std::string mapPath(int id) const;
};

} // namespace tsukuru
