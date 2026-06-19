#pragma once
// GameState: the *mutable* runtime state of a playthrough — party members with
// current HP/MP/level, switches, variables, inventory, and player position.
// This is what gets written to / read from a save file.
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "game/Inventory.h"
#include "database/Database.h"

namespace tsukuru {

// A live party member, instantiated from an ActorDef but carrying current stats.
struct PartyMember {
    int actorId = -1;
    int level = 1;
    int exp = 0;
    int hp = 0, mp = 0;            // current
    int maxHp = 0, maxMp = 0;      // effective (base + level growth)
    int atk = 0, def = 0, spd = 0; // base (equipment added on top)
    int weaponId = -1;             // equipped equipment ids (-1 none)
    int armorId  = -1;

    // Effective stats including equipment bonuses (looked up via Database).
    int totalAtk(const Database& db) const;
    int totalDef(const Database& db) const;
    bool alive() const { return hp > 0; }

    static PartyMember fromActor(const ActorDef& def);
    void gainExp(int amount); // simple leveling

    nlohmann::json toJson() const;
    static PartyMember fromJson(const nlohmann::json& j);
};

class GameState {
public:
    Inventory inventory;
    std::vector<PartyMember> party;

    int  currentMap = -1;
    int  playerX = 0, playerY = 0;
    int  playerDir = 0; // Direction

    void setSwitch(int id, bool value) { switches_[id] = value; }
    bool getSwitch(int id) const {
        auto it = switches_.find(id); return it != switches_.end() && it->second;
    }
    void setVar(int id, int value) { variables_[id] = value; }
    int  getVar(int id) const {
        auto it = variables_.find(id); return it == variables_.end() ? 0 : it->second;
    }

    // Start a brand-new game from the database/start settings.
    void newGame(const Database& db, int startActorId, int startMap, int sx, int sy);

    bool partyWiped() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::map<int,bool> switches_;
    std::map<int,int>  variables_;
};

} // namespace tsukuru
