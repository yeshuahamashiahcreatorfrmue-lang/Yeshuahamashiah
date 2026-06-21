#include "game/GameState.h"

using nlohmann::json;

namespace tsukuru {

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
            {"atk", atk}, {"def", def}, {"spd", spd}};
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
    return m;
}

void GameState::newGame(const Database& db, int startActorId, int playerCharId, int startMap, int sx, int sy,
                        int startGold, const std::vector<std::pair<int,int>>& startItems) {
    party.clear();
    switches_.clear();
    variables_.clear();
    equipped.clear();
    quests.clear();
    buffs.clear();
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
    // Starting loadout: project-configured gold + items take priority. If the
    // project defines no starting items, fall back to a small auto starter kit so
    // the inventory/equip windows are populated out of the box.
    inventory.gold = startGold;
    for (const auto& kv : startItems) if (kv.first >= 0) inventory.addItem(kv.first, kv.second);
    if (startItems.empty()) {
        int foodN = 0, equipN = 0;
        for (const auto& it : db.items) {
            if (it.kind == 1 && foodN  < 3) { inventory.addItem(it.id, 5); ++foodN; }
            if (it.kind == 2 && equipN < 4) { inventory.addItem(it.id, 1); ++equipN; }
        }
    }
    currentMap = startMap;
    playerX = sx; playerY = sy; playerDir = 0;
    playSeconds = 0;
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

// Eat/use a non-equipment item: restore 포만/수분/HP/GP and grant its bonus stats.
// If the food carries a buffSecs duration the bonus is temporary (tracked in buffs)
// otherwise it is a simple permanent gain. Returns true if something was consumed.
bool GameState::consumeFood(const Database& db, int itemId) {
    const Item* it = db.item(itemId);
    if (!it || party.empty()) return false;
    PartyMember& m = party[0];
    m.hunger = std::min(m.maxHunger, m.hunger + it->satiety);
    m.thirst = std::min(m.maxThirst, m.thirst + it->hydration);
    int hh = it->healHp + (it->effect == ItemEffect::HealHP ? it->power : 0);
    int gg = it->healGp + (it->effect == ItemEffect::HealMP ? it->power : 0);
    m.hp = std::min(m.maxHp, m.hp + hh);
    m.mp = std::min(m.maxMp, m.mp + gg);
    if (it->bonusAtk || it->bonusDef || it->bonusSpd) {
        m.atk += it->bonusAtk; m.def += it->bonusDef; m.spd += it->bonusSpd;
        if (it->buffSecs > 0)                               // temporary: schedule revert
            buffs.push_back(ActiveBuff{ it->bonusAtk, it->bonusDef, it->bonusSpd,
                                        (float)it->buffSecs, it->name });
    }
    inventory.removeItem(itemId, 1);
    return true;
}

bool GameState::equipItem(const Database& db, int itemId) {
    const Item* it = db.item(itemId);
    if (!it || party.empty()) return false;
    int slot = (it->bodySlot >= 1 && it->bodySlot <= 7) ? it->bodySlot : 2;
    unequipSlot(db, slot);                                  // return whatever's there first
    equipped[slot] = itemId;
    inventory.removeItem(itemId, 1);
    PartyMember& m = party[0];
    m.atk += it->bonusAtk; m.def += it->bonusDef; m.spd += it->bonusSpd;
    return true;
}

void GameState::unequipSlot(const Database& db, int slot) {
    auto e = equipped.find(slot);
    if (e == equipped.end() || e->second < 0) return;
    const Item* it = db.item(e->second);
    inventory.addItem(e->second, 1);
    if (it && !party.empty()) {
        PartyMember& m = party[0];
        m.atk -= it->bonusAtk; m.def -= it->bonusDef; m.spd -= it->bonusSpd;
    }
    equipped.erase(e);
}

void GameState::tickBuffs(float dt) {
    if (buffs.empty() || party.empty()) return;
    PartyMember& m = party[0];
    for (size_t i = 0; i < buffs.size();) {
        buffs[i].remain -= dt;
        if (buffs[i].remain <= 0) {
            m.atk -= buffs[i].atk; m.def -= buffs[i].def; m.spd -= buffs[i].spd;
            buffs.erase(buffs.begin() + i);
        } else ++i;
    }
}

void GameState::addKillProgress(int enemyId) {
    for (auto& kv : quests) {
        QuestState& q = kv.second;
        if (q.status == 1 && q.objective == 1 && (q.target < 0 || q.target == enemyId))
            if (q.count < q.need) ++q.count;
    }
}

void GameState::markReached(int mapId) {
    for (auto& kv : quests) {
        QuestState& q = kv.second;
        if (q.status == 1 && q.objective == 3 && q.target == mapId)
            q.count = std::max(q.count, q.need);
    }
}

void GameState::addTalkProgress(int eventId) {
    for (auto& kv : quests) {
        QuestState& q = kv.second;
        if (q.status == 1 && q.objective == 4 && q.target == eventId)
            q.count = std::max(q.count, q.need);
    }
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
    json bf = json::array();
    for (const auto& b : buffs) bf.push_back(b.toJson());
    json qs = json::array();
    for (const auto& kv : quests) { json e = kv.second.toJson(); e["key"] = kv.first; qs.push_back(e); }
    return {{"inventory", inventory.toJson()}, {"party", pt},
            {"currentMap", currentMap}, {"playerX", playerX}, {"playerY", playerY},
            {"playerDir", playerDir}, {"playSeconds", playSeconds},
            {"switches", sw}, {"variables", vr},
            {"objective", objective}, {"equipped", eq}, {"buffs", bf}, {"quests", qs}};
}

void GameState::fromJson(const json& j) {
    if (j.contains("inventory")) inventory.fromJson(j["inventory"]);
    party.clear();
    for (const auto& m : j.value("party", json::array())) party.push_back(PartyMember::fromJson(m));
    currentMap = j.value("currentMap", -1);
    playerX = j.value("playerX", 0); playerY = j.value("playerY", 0);
    playerDir = j.value("playerDir", 0);
    playSeconds = j.value("playSeconds", 0.0);
    objective = j.value("objective", std::string());
    switches_.clear();
    for (const auto& s : j.value("switches", json::array())) switches_[s.value("id",-1)] = s.value("v", false);
    variables_.clear();
    for (const auto& v : j.value("variables", json::array())) variables_[v.value("id",-1)] = v.value("v", 0);
    equipped.clear();
    for (const auto& e : j.value("equipped", json::array())) equipped[e.value("slot",0)] = e.value("item",-1);
    buffs.clear();
    for (const auto& b : j.value("buffs", json::array())) buffs.push_back(ActiveBuff::fromJson(b));
    quests.clear();
    for (const auto& q : j.value("quests", json::array()))
        quests[(long)q.value("key", 0)] = QuestState::fromJson(q);
}

} // namespace tsukuru
