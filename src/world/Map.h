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

    Map() : tilemap(20, 15) {}

    Event* eventAt(int x, int y);
    int nextEventId() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace tsukuru
