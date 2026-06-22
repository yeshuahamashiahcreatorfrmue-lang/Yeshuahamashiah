#pragma once
// Database: the game's static definitions — items, equipment, actors, enemies,
// and skills. Edited in the editor's DB tab; serialized to database.json.
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "entity/Story.h"

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
    // category: 0 기타(misc) · 1 식품(food/drink) · 2 장비(equipment)
    int         kind = 0;
    // 식품(food) — restored / granted when eaten
    int         satiety = 0;     // 포만도 회복
    int         hydration = 0;   // 수분 회복
    int         healHp = 0;      // 추가 HP 회복
    int         healGp = 0;      // 추가 기력(GP) 회복
    // 장비(equipment) — body slot 0 없음,1 머리,2 몸통,3 손,4 다리,5 발,6 무기,7 장신구
    int         bodySlot = 0;
    int         bonusAtk = 0, bonusDef = 0, bonusSpd = 0; // 장착 시 보너스(식품은 일시 효과로도 사용)
    int         buffSecs = 0;    // 식품 버프 지속(초); 0 = 즉시효과만
};

// NOTE: equipment is modelled as Items with kind==2 (body-slot gear); the old
// Equipment/EquipSlot and the turn-based Skill struct were removed with the
// turn-based battle system. Field skills live in FieldSkill (below).

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
    std::vector<int> patX, patY; // relative DAMAGE tiles (canonical facing-up)
    int         effectAsset = -1;// sprite drawn for the effect (-1 = procedural)
    int         effectLoops = 1; // how many times the effect's frame strip replays per cast
    int         effectDist  = 0; // extra tiles FORWARD to shift the whole effect
    int         soundAsset  = -1;// audio asset to play (-1 = built-in "attack")
    // Effect PLACEMENT — authored on its own grid layer like the damage range.
    // efxX/efxY are the tiles the effect appears on (canonical facing-up); when
    // empty the effect follows the damage tiles (patX/patY).
    std::vector<int> efxX, efxY;
    int         effectMode  = 0; // 0 = 각 타일마다(per tile), 1 = 한 곳에 크게(one big over the area)
    int         effectScale = 100;// effect motion size, % of one tile (per-tile size / big size)
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
    int maxHunger = 42000; // 포만치 (허기)
    int maxThirst = 42000; // 수분치 (목마름)
    int atk   = 12;    // 공격력 (모든 스킬 공통, 배수 이전)
    int def   = 5;     // 방어력
    int spd   = 5;     // 속도
    int drawPct = 125; // fine size %, applied on top of the tile footprint (100 = exact)
    int drawTilesW = 1; // tile footprint WIDTH  the character occupies on the map (칸)
    int drawTilesH = 1; // tile footprint HEIGHT the character occupies on the map (칸)
    MotionClip motions[MO_COUNT]; // walk/attack/skill1/skill2/ultimate/death
    std::vector<FieldSkill> skills; // this character's own skills (by slot); override
                                    // the global field skills when it drives the player

    // --- mob-only fields (used when this def lives in Database::mobs) ---
    int  expReward = 10;     // 처치 시 경험치
    int  goldReward = 5;     // 처치 시 골드
    int  dropItemId = -1;    // 드롭 아이템 (-1 = 없음)
    int  dropRate = 0;       // 드롭 확률 0..100 (%)
    float spawnFreezeSecs = 1.2f; // 탄생 직후 무적·비공격(갑툭튀 방지) 시간(초)
    int  respawnSecs = 0;    // 탄생 주기(초): >0 이면 처치 후 이 시간마다 재생성 (0=재생성 없음)
};

struct ActorDef {
    int         id = -1;
    std::string name = "Hero";
    int         spriteAsset = -1;
    int         maxHp = 100, maxMp = 20;
    int         atk = 10, def = 5, spd = 5;
};

struct EnemyDef {
    int         id = -1;
    std::string name = "Slime";
    int         spriteAsset = -1;
    int         maxHp = 30;
    int         atk = 8, def = 3, spd = 4;
    int         expReward = 10;
    int         goldReward = 5;
    int         dropItemId = -1;   // item dropped on kill (-1 = none)
    int         dropRate   = 0;    // drop chance, 0..100 (%)
};

class Database {
public:
    std::vector<Item>       items;
    std::vector<ActorDef>   actors;
    std::vector<EnemyDef>   enemies;
    std::vector<FieldSkill> fieldSkills;
    std::vector<CharacterDef> characters;   // custom multi-motion characters (players/NPCs)
    std::vector<CharacterDef> mobs;         // monsters — same motion/effect/image system as characters
    std::vector<DialogueScenario> dialogues; // branching 대화로그 시나리오
    std::vector<Scene> scenes;               // 스토리 시나리오 시퀀스
    std::vector<std::string> sceneGroups;    // 시나리오 제목(그룹) 목록 — 장면을 제목 아래로 묶음
    std::vector<int> sceneGroupBgm;          // 제목별 음악(오디오 에셋), sceneGroups와 인덱스 동기(-1=없음)

    const Item*      item(int id) const;
    const CharacterDef* character(int id) const;
    const CharacterDef* mob(int id) const;
    const DialogueScenario* dialogue(int id) const;
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
