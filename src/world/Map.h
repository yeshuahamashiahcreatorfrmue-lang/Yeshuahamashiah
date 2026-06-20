#pragma once
// Map = tilemap + tileset + events + metadata (encounters, bgm, start point).
#include <string>
#include <vector>
#include "world/Tilemap.h"
#include "world/Tileset.h"
#include "entity/Event.h"

namespace tsukuru {

class Map {
public:
    int         id = -1;
    std::string name = "New Map";
    Tilemap     tilemap;
    Tileset     tileset;
    std::vector<Event> events;

    // Random battle encounters when walking (enemy troop = list of enemy ids).
    std::vector<int> encounterEnemies; // enemy ids that can appear
    int  encounterRate = 0;            // 0 = none; higher = more frequent (per step %)
    int  bgmAsset = -1;
    int  darkness = 0;                 // 0 = daylight; >0 dims the map and lights only
                                       // a radius around the player (cave/night atmosphere)
    int  weather = 0;                  // 0 none, 1 rain, 2 snow
    bool dayNight = false;             // run a slow day->night ambient tint cycle

    // Animated tiles: each listed tile id cycles between id and id+1 over time
    // (the tileset stores the alternate frame right after the base tile).
    std::vector<int> animTiles;

    // Zone position on the All-Map Viewer grid (in map-slot units). Maps whose
    // slots are orthogonally adjacent are walk-connected at their shared edge;
    // unplaced/far-apart maps are independent. placed=false = not on the grid.
    int  worldX = 0, worldY = 0;
    bool placed = false;

    Map() : tilemap(20, 15) {}

    Event* eventAt(int x, int y);
    int nextEventId() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace tsukuru
