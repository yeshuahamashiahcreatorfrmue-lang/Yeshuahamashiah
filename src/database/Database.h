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

// A real-time field skill: a player-relative tile pattern + behaviour, fully
// data-driven so users can design new skills (pattern, effect, sound) in the
// editor. `patX/patY` are tile offsets in a canonical "facing up" frame and are
// rotated to the player's facing at cast time.
struct FieldSkill {
    int         id   = -1;
    std::string name = "스킬";
    int         slot = -1;       // key binding 0..5 = Z,X,C,V,F,G (-1 = unbound)
    bool        projectile = false; // travels forward `range` tiles
    int         blink = 0;       // teleport forward N tiles before applying pattern
    int         range = 6;       // projectile travel distance
    int         mpCost = 0;
    float       cooldown = 0.5f;
    int         powerPct = 100;  // damage = ATK * powerPct/100
    std::vector<int> patX, patY; // relative tiles (canonical facing-up)
    int         effectAsset = -1;// sprite drawn on each hit tile (-1 = procedural)
    int         soundAsset  = -1;// audio asset to play (-1 = built-in "attack")
};

// A custom character built from registered images: each motion is a sequence of
// image asset ids played back as a flipbook. Motions are fixed-order (see
// kMotionNames). Lets users author characters without sprite-sheet layout rules.
enum MotionId { MO_Walk=0, MO_Attack=1, MO_Skill1=2, MO_Skill2=3, MO_Ult=4, MO_Death=5, MO_COUNT=6 };
inline const char* const kMotionNames[MO_COUNT] = { "걷기","공격","스킬1","스킬2","궁극기","죽음" };

struct MotionClip {
    std::vector<int> frames;     // image asset ids, played in order
    int fps = 8;                 // playback speed
    bool loop = false;           // true = cycle continuously, false = play once
};

struct CharacterDef {
    int id = -1;
    std::string name = "캐릭터";
    MotionClip motions[MO_COUNT]; // walk/attack/skill1/skill2/ultimate/death
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
    std::vector<Item>       items;
    std::vector<Equipment>  equipment;
    std::vector<Skill>      skills;
    std::vector<ActorDef>   actors;
    std::vector<EnemyDef>   enemies;
    std::vector<FieldSkill> fieldSkills;
    std::vector<CharacterDef> characters;   // custom multi-motion characters

    const Item*      item(int id) const;
    const CharacterDef* character(int id) const;
    const Equipment* equip(int id) const;
    const Skill*     skill(int id) const;
    const ActorDef*  actor(int id) const;
    const EnemyDef*  enemy(int id) const;
    const FieldSkill* fieldSkillForSlot(int slot) const; // first bound skill for a key

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

    // The 4 built-in skills (melee/ranged/dash/ultimate), used as defaults for
    // new/old projects that have no field skills defined yet.
    static std::vector<FieldSkill> defaultFieldSkills();
};

} // namespace tsukuru
