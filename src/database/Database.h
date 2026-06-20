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
    std::vector<int> frames;            // 정면(아래) 프레임 — 비어있는 방향의 기본값(fallback)
    std::vector<int> left, right, up;   // 선택적 방향별 프레임 (비면 frames 사용)
    int fps = 8;                 // playback speed
    bool loop = false;           // true = cycle continuously, false = play once
    // Frames shown for a facing direction (Down0/Left1/Right2/Up3). Each direction
    // is registered separately; an empty direction falls back to `frames`(아래) so
    // the character is never invisible.
    const std::vector<int>& dirFrames(int dir) const {
        if (dir == 1 && !left.empty())  return left;
        if (dir == 2 && !right.empty()) return right;
        if (dir == 3 && !up.empty())    return up;
        return frames;
    }
};

struct CharacterDef {
    int id = -1;
    std::string name = "캐릭터";
    // Battle stats (shown/edited in the character data editor). 기력(GP) replaces
    // the old MP; 공격력(atk) is applied to EVERY skill before its power multiplier
    // (실제 데미지 = atk * skill.powerPct/100).
    int maxHp = 100;   // 체력
    int maxGp = 30;    // 기력 (구 MP)
    int atk   = 12;    // 공격력 (모든 스킬 공통, 배수 이전)
    int def   = 5;     // 방어력
    int spd   = 5;     // 속도
    int drawPct = 125; // on-map draw size as a % of one tile (100 = 1칸, 200 = 2칸)
    MotionClip motions[MO_COUNT]; // walk/attack/skill1/skill2/ultimate/death
    std::vector<FieldSkill> skills; // this character's own skills (by slot); override
                                    // the global field skills when it drives the player
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
