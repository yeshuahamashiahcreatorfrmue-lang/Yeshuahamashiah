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
    int hunger = 42000, thirst = 42000;   // current 포만치 / 수분치 (허기·목마름)
    int maxHunger = 42000, maxThirst = 42000;
    int atk = 0, def = 0, spd = 0; // base (equipment added on top)
    int weaponId = -1;             // equipped equipment ids (-1 none)
    int armorId  = -1;

    // Effective stats including equipment bonuses (looked up via Database).
    int totalAtk(const Database& db) const;
    int totalDef(const Database& db) const;
    bool alive() const { return hp > 0; }

    static PartyMember fromActor(const ActorDef& def);
    void applyCharacter(const CharacterDef& c); // override stats from the player character (체력/기력/공격력…)
    void gainExp(int amount); // simple leveling

    nlohmann::json toJson() const;
    static PartyMember fromJson(const nlohmann::json& j);
};

// A temporary stat buff granted by eating food with a non-zero 버프 지속(buffSecs).
// While active the bonus is also folded into party[0]'s atk/def/spd so totalAtk()
// reflects it; when it expires the bonus is subtracted back out.
struct ActiveBuff {
    int atk = 0, def = 0, spd = 0;
    float remain = 0;       // seconds left
    std::string name;       // source food name (HUD label)
    nlohmann::json toJson() const {
        return {{"atk",atk},{"def",def},{"spd",spd},{"remain",remain},{"name",name}};
    }
    static ActiveBuff fromJson(const nlohmann::json& j) {
        ActiveBuff b; b.atk=j.value("atk",0); b.def=j.value("def",0); b.spd=j.value("spd",0);
        b.remain=j.value("remain",0.0f); b.name=j.value("name",std::string()); return b;
    }
};

class GameState {
public:
    Inventory inventory;
    std::vector<PartyMember> party;
    // body-part equipment: slot 1..7 (머리/몸통/손/다리/발/무기/장신구) -> item id
    std::map<int,int> equipped;
    std::vector<ActiveBuff> buffs;   // active timed food buffs
    int equipBonus(const Database& db, int which) const; // which: 0 atk,1 def,2 spd — sum of equipped items

    // Shared item actions (used by both the in-field windows and the ESC menu so
    // every on-screen surface reflects the same food/equipment systems).
    bool consumeFood(const Database& db, int itemId);   // restore 포만/수분/HP/GP + bonus(timed/perm)
    bool equipItem(const Database& db, int itemId);     // equip an item into its body slot
    void unequipSlot(const Database& db, int slot);     // return a body-slot item to the inventory
    void tickBuffs(float dt);                           // count down + revert expired buffs

    int  currentMap = -1;
    int  playerX = 0, playerY = 0;
    int  playerDir = 0; // Direction
    std::string objective;   // current quest objective shown on the HUD

    void setSwitch(int id, bool value) { switches_[id] = value; }
    bool getSwitch(int id) const {
        auto it = switches_.find(id); return it != switches_.end() && it->second;
    }
    void setVar(int id, int value) { variables_[id] = value; }
    int  getVar(int id) const {
        auto it = variables_.find(id); return it == variables_.end() ? 0 : it->second;
    }

    // Start a brand-new game from the database/start settings.
    void newGame(const Database& db, int startActorId, int playerCharId, int startMap, int sx, int sy);

    bool partyWiped() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::map<int,bool> switches_;
    std::map<int,int>  variables_;
};

} // namespace tsukuru
