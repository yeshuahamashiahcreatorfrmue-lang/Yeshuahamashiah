#pragma once
// Database: the game's static definitions — items, equipment, actors, enemies,
// and skills. Edited in the editor's DB tab; serialized to database.json.
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tsukuru {

enum class ItemEffect { None, HealHP, HealMP, Damage };

struct Item {
    int         id = -1;
    std::string name = "Item";
    std::string description;
    int         price = 0;
    int         iconAsset = -1;
    ItemEffect  effect = ItemEffect::None;
    int         power = 0;       // amount healed / damage dealt
    bool        consumable = true;
};

enum class EquipSlot { Weapon, Armor };

struct Equipment {
    int         id = -1;
    std::string name = "Gear";
    EquipSlot   slot = EquipSlot::Weapon;
    int         price = 0;
    int         iconAsset = -1;
    int         atk = 0;        // bonus attack (weapon)
    int         def = 0;        // bonus defense (armor)
};

struct Skill {
    int         id = -1;
    std::string name = "Skill";
    int         mpCost = 0;
    int         power = 0;       // base damage / heal
    bool        healing = false;
};

struct ActorDef {
    int         id = -1;
    std::string name = "Hero";
    int         spriteAsset = -1;
    int         maxHp = 100, maxMp = 20;
    int         atk = 10, def = 5, spd = 5;
    std::vector<int> skills;     // skill ids
};

struct EnemyDef {
    int         id = -1;
    std::string name = "Slime";
    int         spriteAsset = -1;
    int         maxHp = 30, maxMp = 0;
    int         atk = 8, def = 3, spd = 4;
    int         expReward = 10;
    int         goldReward = 5;
};

class Database {
public:
    std::vector<Item>      items;
    std::vector<Equipment> equipment;
    std::vector<Skill>     skills;
    std::vector<ActorDef>  actors;
    std::vector<EnemyDef>  enemies;

    const Item*      item(int id) const;
    const Equipment* equip(int id) const;
    const Skill*     skill(int id) const;
    const ActorDef*  actor(int id) const;
    const EnemyDef*  enemy(int id) const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace tsukuru
