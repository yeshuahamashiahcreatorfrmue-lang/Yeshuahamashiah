#include "database/Database.h"

using nlohmann::json;

namespace tsukuru {

// ---- effect enum <-> string ----
static const char* effName(ItemEffect e) {
    switch (e) {
        case ItemEffect::HealHP: return "healHP";
        case ItemEffect::HealMP: return "healMP";
        case ItemEffect::Damage: return "damage";
        default: return "none";
    }
}
static ItemEffect effFrom(const std::string& s) {
    if (s == "healHP") return ItemEffect::HealHP;
    if (s == "healMP") return ItemEffect::HealMP;
    if (s == "damage") return ItemEffect::Damage;
    return ItemEffect::None;
}

// ---- lookups ----
const Item* Database::item(int id) const {
    for (const auto& i : items) if (i.id == id) return &i;
    return nullptr;
}
const Equipment* Database::equip(int id) const {
    for (const auto& e : equipment) if (e.id == id) return &e;
    return nullptr;
}
const Skill* Database::skill(int id) const {
    for (const auto& s : skills) if (s.id == id) return &s;
    return nullptr;
}
const ActorDef* Database::actor(int id) const {
    for (const auto& a : actors) if (a.id == id) return &a;
    return nullptr;
}
const EnemyDef* Database::enemy(int id) const {
    for (const auto& e : enemies) if (e.id == id) return &e;
    return nullptr;
}

// ---- serialization ----
json Database::toJson() const {
    json j;
    j["items"] = json::array();
    for (const auto& i : items)
        j["items"].push_back({{"id", i.id}, {"name", i.name}, {"description", i.description},
            {"price", i.price}, {"iconAsset", i.iconAsset}, {"effect", effName(i.effect)},
            {"power", i.power}, {"consumable", i.consumable}});

    j["equipment"] = json::array();
    for (const auto& e : equipment)
        j["equipment"].push_back({{"id", e.id}, {"name", e.name},
            {"slot", e.slot == EquipSlot::Weapon ? "weapon" : "armor"},
            {"price", e.price}, {"iconAsset", e.iconAsset}, {"atk", e.atk}, {"def", e.def}});

    j["skills"] = json::array();
    for (const auto& s : skills)
        j["skills"].push_back({{"id", s.id}, {"name", s.name}, {"mpCost", s.mpCost},
            {"power", s.power}, {"healing", s.healing}});

    j["actors"] = json::array();
    for (const auto& a : actors)
        j["actors"].push_back({{"id", a.id}, {"name", a.name}, {"spriteAsset", a.spriteAsset},
            {"maxHp", a.maxHp}, {"maxMp", a.maxMp}, {"atk", a.atk}, {"def", a.def},
            {"spd", a.spd}, {"skills", a.skills}});

    j["enemies"] = json::array();
    for (const auto& e : enemies)
        j["enemies"].push_back({{"id", e.id}, {"name", e.name}, {"spriteAsset", e.spriteAsset},
            {"maxHp", e.maxHp}, {"maxMp", e.maxMp}, {"atk", e.atk}, {"def", e.def},
            {"spd", e.spd}, {"expReward", e.expReward}, {"goldReward", e.goldReward}});
    return j;
}

void Database::fromJson(const json& j) {
    items.clear(); equipment.clear(); skills.clear(); actors.clear(); enemies.clear();

    for (const auto& i : j.value("items", json::array())) {
        Item it;
        it.id = i.value("id", -1); it.name = i.value("name", "Item");
        it.description = i.value("description", ""); it.price = i.value("price", 0);
        it.iconAsset = i.value("iconAsset", -1); it.effect = effFrom(i.value("effect", "none"));
        it.power = i.value("power", 0); it.consumable = i.value("consumable", true);
        items.push_back(it);
    }
    for (const auto& e : j.value("equipment", json::array())) {
        Equipment eq;
        eq.id = e.value("id", -1); eq.name = e.value("name", "Gear");
        eq.slot = e.value("slot", "weapon") == "armor" ? EquipSlot::Armor : EquipSlot::Weapon;
        eq.price = e.value("price", 0); eq.iconAsset = e.value("iconAsset", -1);
        eq.atk = e.value("atk", 0); eq.def = e.value("def", 0);
        equipment.push_back(eq);
    }
    for (const auto& s : j.value("skills", json::array())) {
        Skill sk;
        sk.id = s.value("id", -1); sk.name = s.value("name", "Skill");
        sk.mpCost = s.value("mpCost", 0); sk.power = s.value("power", 0);
        sk.healing = s.value("healing", false);
        skills.push_back(sk);
    }
    for (const auto& a : j.value("actors", json::array())) {
        ActorDef ac;
        ac.id = a.value("id", -1); ac.name = a.value("name", "Hero");
        ac.spriteAsset = a.value("spriteAsset", -1);
        ac.maxHp = a.value("maxHp", 100); ac.maxMp = a.value("maxMp", 20);
        ac.atk = a.value("atk", 10); ac.def = a.value("def", 5); ac.spd = a.value("spd", 5);
        ac.skills = a.value("skills", std::vector<int>{});
        actors.push_back(ac);
    }
    for (const auto& e : j.value("enemies", json::array())) {
        EnemyDef en;
        en.id = e.value("id", -1); en.name = e.value("name", "Slime");
        en.spriteAsset = e.value("spriteAsset", -1);
        en.maxHp = e.value("maxHp", 30); en.maxMp = e.value("maxMp", 0);
        en.atk = e.value("atk", 8); en.def = e.value("def", 3); en.spd = e.value("spd", 4);
        en.expReward = e.value("expReward", 10); en.goldReward = e.value("goldReward", 5);
        enemies.push_back(en);
    }
}

} // namespace tsukuru
