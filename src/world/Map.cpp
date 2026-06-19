#include "world/Map.h"

using nlohmann::json;

namespace tsukuru {

Event* Map::eventAt(int x, int y) {
    for (auto& e : events) if (e.x == x && e.y == y) return &e;
    return nullptr;
}

int Map::nextEventId() const {
    int m = 0;
    for (const auto& e : events) m = std::max(m, e.id);
    return m + 1;
}

json Map::toJson() const {
    json evs = json::array();
    for (const auto& e : events) evs.push_back(e.toJson());
    return {
        {"id", id}, {"name", name},
        {"tilemap", tilemap.toJson()},
        {"tileset", tileset.toJson()},
        {"events", evs},
        {"encounterEnemies", encounterEnemies},
        {"encounterRate", encounterRate},
        {"bgmAsset", bgmAsset},
        {"darkness", darkness},
        {"weather", weather},
        {"dayNight", dayNight},
        {"animTiles", animTiles}
    };
}

void Map::fromJson(const json& j) {
    id   = j.value("id", -1);
    name = j.value("name", "Map");
    if (j.contains("tilemap")) tilemap.fromJson(j["tilemap"]);
    if (j.contains("tileset")) tileset.fromJson(j["tileset"]);
    events.clear();
    if (j.contains("events"))
        for (const auto& e : j["events"]) events.push_back(Event::fromJson(e));
    encounterEnemies = j.value("encounterEnemies", std::vector<int>{});
    encounterRate    = j.value("encounterRate", 0);
    bgmAsset         = j.value("bgmAsset", -1);
    darkness         = j.value("darkness", 0);
    weather          = j.value("weather", 0);
    dayNight         = j.value("dayNight", false);
    animTiles        = j.value("animTiles", std::vector<int>{});
}

} // namespace tsukuru
