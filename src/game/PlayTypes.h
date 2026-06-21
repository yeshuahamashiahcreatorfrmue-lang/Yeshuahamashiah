#pragma once
// PlayTypes: the small value types the play-mode runtime operates on —
// grouped here by purpose so GamePlay.h stays a pure controller declaration.
#include <string>
#include "raylib.h"
#include "entity/Event.h"   // NpcFaction / NpcBehavior

namespace tsukuru {

// --- entities ---------------------------------------------------------------

// A live monster wandering the field.
struct FieldMonster {
    int enemyId = -1;
    std::string name;
    int spriteAsset = -1;
    int x = 0, y = 0;          // grid position
    float px = 0, py = 0;      // pixel position (for smooth movement)
    int destX = 0, destY = 0;
    bool moving = false;
    int dir = 0;
    int hp = 0, maxHp = 0;
    int atk = 0, def = 0;
    int expReward = 0, goldReward = 0;
    float moveCd = 0;          // time until next move decision
    float atkCd = 0;           // time until it can hit the player again
    float hurtFlash = 0;       // white/red flash timer when struck
    float spawnFreeze = 0;     // 탄생 직후 무적·비공격 시간(초): 갑툭튀해서 바로 때리지 않게
    int  defeatSwitch = -1;    // when this troop is cleared, set this switch (boss gate)
    // map-placed mob spawner link (CharacterDef-based mobs, db.mobs):
    int  mobCharId = -1;       // Database::mobs id for motion/effect rendering (-1 = sprite enemy)
    int  homeX = -1, homeY = -1;  // spawn-point origin (for respawn); -1 = not a spawner mob
    float respawnTimer = 0;    // counts down after death; 0 + alive=false + respawnSecs>0 -> respawn
    float respawnSecs = 0;     // 탄생 주기: respawn period seconds (0 = no respawn)
    int  frame = 0; float animTime = 0;  // walk-motion playback for CharacterDef mobs
    bool alive() const { return hp > 0; }
};

// A live NPC instance (from a map event). Drives autonomous movement and, for
// Ally/Enemy factions, lightweight field combat.
struct NpcInst {
    int eventId = -1;
    int spriteAsset = -1;
    int x = 0, y = 0, destX = 0, destY = 0;
    float px = 0, py = 0;
    bool moving = false;
    int dir = 0;
    float moveCd = 0;
    int frame = 0; float animTime = 0;

    // behaviour / faction (copied from the source Event)
    NpcFaction  faction  = NpcFaction::Neutral;
    NpcBehavior behavior = NpcBehavior::Idle;
    int  drawPct = 100;
    int  drawTilesW = 1, drawTilesH = 1;  // tile footprint copied from the source Event
    int  homeX = 0, homeY = 0;   // spawn tile — patrol anchor / leash centre
    int  patrolDir = -1;         // 0=down 1=left 2=right 3=up, flips at obstacles

    // combat (Ally/Enemy only; maxHp == 0 means non-combatant)
    int   hp = 0, maxHp = 0, atk = 0, def = 0;
    float atkCd = 0;
    float hurtFlash = 0;
    int   switchOnDeath = -1;    // event.switchId flipped when this NPC dies
    float lifeTimer = 0;         // >0: 시간제한 NPC(대화로 소환된 적대/우호/추종) — 0이 되면 사라짐
    bool  follower = false;      // 추종 NPC(대화로 떠나보낼 수 있음)
    bool  combatant() const { return maxHp > 0; }
    bool  alive() const { return maxHp <= 0 || hp > 0; }
};

// --- combat projectiles / effects -------------------------------------------

// A travelling ranged bolt (projectile skill).
struct Projectile {
    float px = 0, py = 0;   // pixel position
    int dir = 0;
    float life = 0;
    int dmg = 0;
};

// A short-lived visual effect. If assetId >= 0 it draws an animated sprite
// strip; otherwise a procedural shape.
struct SkillFx {
    int type = 0;           // 0 slash, 1 bolt-impact, 2 dash, 3 aoe-ring
    float px = 0, py = 0;
    float t = 0, dur = 0.25f;
    int dir = 0;
    int assetId = -1;
    float radius = 0;       // for aoe ring
    int loops = 1;          // times the sprite strip replays across `dur`
    float sizePx = 0;       // explicit draw size in px (0 = use the type's default)
};

// A floating combat number/text that rises and fades above a target (damage to
// an enemy, damage taken by the player, heals, etc.).
struct FloatingText {
    float px = 0, py = 0;    // world pixel position (rises over life)
    float t = 0, dur = 0.8f;
    std::string text;
    Color color = { 255, 255, 255, 255 };
};

// --- atmosphere -------------------------------------------------------------

struct Particle { float x, y, vx, vy, life; };

// --- skills -----------------------------------------------------------------

// Player skill key slots: Z, X, C, V, F, G.
inline constexpr int kSkillSlots = 6;
enum SkillSlot { SK_Attack = 0, SK_Ranged = 1, SK_Dash = 2, SK_Ult = 3 };

} // namespace tsukuru
