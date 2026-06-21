#include "game/GameState.h"

using nlohmann::json;

namespace tsukuru {

int PartyMember::totalAtk(const Database& db) const {
    int t = atk;
    if (const Equipment* w = db.equip(weaponId)) t += w->atk;
    return t;
}
int PartyMember::totalDef(const Database& db) const {
    int t = def;
    if (const Equipment* a = db.equip(armorId)) t += a->def;
    return t;
}

PartyMember PartyMember::fromActor(const ActorDef& d) {
    PartyMember m;
    m.actorId = d.id;
    m.level = 1; m.exp = 0;
    m.maxHp = d.maxHp; m.maxMp = d.maxMp;
    m.hp = d.maxHp; m.mp = d.maxMp;
    m.atk = d.atk; m.def = d.def; m.spd = d.spd;
    m.maxHunger = m.hunger = 42000; m.maxThirst = m.thirst = 42000;
    return m;
}

void PartyMember::applyCharacter(const CharacterDef& c) {
    maxHp = c.maxHp; maxMp = c.maxGp;      // 기력(GP) is stored in maxMp at runtime
    maxHunger = c.maxHunger; maxThirst = c.maxThirst;
    atk = c.atk; def = c.def; spd = c.spd;
    hp = maxHp; mp = maxMp; hunger = maxHunger; thirst = maxThirst;
}

void PartyMember::gainExp(int amount) {
    exp += amount;
    // Simple curve: level up every (level * 20) exp; stats grow each level.
    while (exp >= level * 20) {
        exp -= level * 20;
        level++;
        maxHp += 10; maxMp += 3;
        atk += 2; def += 1; spd += 1;
        hp = maxHp; mp = maxMp; // full heal on level up
    }
}

json PartyMember::toJson() const {
    return {{"actorId", actorId}, {"level", level}, {"exp", exp},
            {"hp", hp}, {"mp", mp}, {"maxHp", maxHp}, {"maxMp", maxMp},
            {"hunger", hunger}, {"thirst", thirst}, {"maxHunger", maxHunger}, {"maxThirst", maxThirst},
            {"atk", atk}, {"def", def}, {"spd", spd},
            {"weaponId", weaponId}, {"armorId", armorId}};
}

PartyMember PartyMember::fromJson(const json& j) {
    PartyMember m;
    m.actorId = j.value("actorId", -1);
    m.level = j.value("level", 1); m.exp = j.value("exp", 0);
    m.hp = j.value("hp", 0); m.mp = j.value("mp", 0);
    m.maxHp = j.value("maxHp", 0); m.maxMp = j.value("maxMp", 0);
    m.atk = j.value("atk", 0); m.def = j.value("def", 0); m.spd = j.value("spd", 0);
    m.hunger = j.value("hunger", 42000); m.thirst = j.value("thirst", 42000);
    m.maxHunger = j.value("maxHunger", 42000); m.maxThirst = j.value("maxThirst", 42000);
    m.weaponId = j.value("weaponId", -1); m.armorId = j.value("armorId", -1);
    return m;
}

void GameState::newGame(const Database& db, int startActorId, int playerCharId, int startMap, int sx, int sy) {
    party.clear();
    switches_.clear();
    variables_.clear();
    equipped.clear();
    inventory = Inventory{};
    if (const ActorDef* a = db.actor(startActorId))
        party.push_back(PartyMember::fromActor(*a));
    else if (!db.actors.empty())
        party.push_back(PartyMember::fromActor(db.actors.front()));
    // The playable custom character's own stats (체력/기력/공격력/방어력/속도) take
    // priority over the actor template when one is assigned as the player.
    if (const CharacterDef* c = db.character(playerCharId)) {
        if (party.empty()) party.push_back(PartyMember{});
        party[0].applyCharacter(*c);
    }
    if (party.empty()) party.push_back(PartyMember{});   // never leave the party empty
    currentMap = startMap;
    playerX = sx; playerY = sy; playerDir = 0;
    objective.clear();
}

// Sum a stat bonus across all body-equipped items. which: 0 atk, 1 def, 2 spd.
int GameState::equipBonus(const Database& db, int which) const {
    int t = 0;
    for (const auto& kv : equipped) {
        const Item* it = db.item(kv.second);
        if (!it) continue;
        t += which == 0 ? it->bonusAtk : which == 1 ? it->bonusDef : it->bonusSpd;
    }
    return t;
}

bool GameState::partyWiped() const {
    for (const auto& m : party) if (m.alive()) return false;
    return true;
}

json GameState::toJson() const {
    json sw = json::array();
    for (const auto& kv : switches_) sw.push_back({{"id", kv.first}, {"v", kv.second}});
    json vr = json::array();
    for (const auto& kv : variables_) vr.push_back({{"id", kv.first}, {"v", kv.second}});
    json pt = json::array();
    for (const auto& m : party) pt.push_back(m.toJson());
    json eq = json::array();
    for (const auto& kv : equipped) eq.push_back({{"slot", kv.first}, {"item", kv.second}});
    return {{"inventory", inventory.toJson()}, {"party", pt},
            {"currentMap", currentMap}, {"playerX", playerX}, {"playerY", playerY},
            {"playerDir", playerDir}, {"switches", sw}, {"variables", vr},
            {"objective", objective}, {"equipped", eq}};
}

void GameState::fromJson(const json& j) {
    if (j.contains("inventory")) inventory.fromJson(j["inventory"]);
    party.clear();
    for (const auto& m : j.value("party", json::array())) party.push_back(PartyMember::fromJson(m));
    currentMap = j.value("currentMap", -1);
    playerX = j.value("playerX", 0); playerY = j.value("playerY", 0);
    playerDir = j.value("playerDir", 0);
    objective = j.value("objective", std::string());
    switches_.clear();
    for (const auto& s : j.value("switches", json::array())) switches_[s.value("id",-1)] = s.value("v", false);
    variables_.clear();
    for (const auto& v : j.value("variables", json::array())) variables_[v.value("id",-1)] = v.value("v", 0);
    equipped.clear();
    for (const auto& e : j.value("equipped", json::array())) equipped[e.value("slot",0)] = e.value("item",-1);
}

} // namespace tsukuru
