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

// Per-quest runtime state. A quest is keyed by its giver event (mapId<<16|evId)
// and snapshots the objective at accept time so progress can be tracked even when
// the giver's map isn't loaded (e.g. killing monsters elsewhere).
struct QuestState {
    int status = 0;      // 0 미수락 / 1 진행중 / 2 완료(보상수령)
    int count  = 0;      // 처치/도달 진행도
    int objective = 0;   // 0 즉시 / 1 처치 / 2 수집 / 3 도달
    int target = -1;     // 적/아이템/맵 id
    int need   = 1;      // 필요 수량
    std::string title;   // 로그 표시용 퀘스트 안내문
    nlohmann::json toJson() const {
        return {{"status",status},{"count",count},{"objective",objective},
                {"target",target},{"need",need},{"title",title}};
    }
    static QuestState fromJson(const nlohmann::json& j) {
        QuestState q; q.status=j.value("status",0); q.count=j.value("count",0);
        q.objective=j.value("objective",0); q.target=j.value("target",-1);
        q.need=j.value("need",1); q.title=j.value("title",std::string()); return q;
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

    // --- quests (keyed by giver event) ---
    std::map<long,QuestState> quests;
    static long questKey(int mapId, int evId) { return ((long)mapId << 16) | (evId & 0xffff); }
    void addKillProgress(int enemyId);   // bump active 처치 quests targeting enemyId (or any)
    void markReached(int mapId);         // satisfy active 도달 quests for mapId

    void setSwitch(int id, bool value) { switches_[id] = value; }
    bool getSwitch(int id) const {
        auto it = switches_.find(id); return it != switches_.end() && it->second;
    }
    void setVar(int id, int value) { variables_[id] = value; }
    int  getVar(int id) const {
        auto it = variables_.find(id); return it == variables_.end() ? 0 : it->second;
    }
    // read-only views for the in-game debug inspector (event testing)
    const std::map<int,bool>& switches() const { return switches_; }
    const std::map<int,int>&  variables() const { return variables_; }

    // Start a brand-new game from the database/start settings.
    void newGame(const Database& db, int startActorId, int playerCharId, int startMap, int sx, int sy,
                 int startGold = 0, const std::vector<std::pair<int,int>>& startItems = {});

    bool partyWiped() const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::map<int,bool> switches_;
    std::map<int,int>  variables_;
};

} // namespace tsukuru
