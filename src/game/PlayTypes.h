#pragma once
// PlayTypes: the small value types the play-mode runtime operates on —
// grouped here by purpose so GamePlay.h stays a pure controller declaration.
#include <string>
#include "raylib.h"

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
    int  defeatSwitch = -1;    // when this troop is cleared, set this switch (boss gate)
    bool alive() const { return hp > 0; }
};

// A live NPC instance (from a map event) that can wander autonomously.
struct NpcInst {
    int eventId = -1;
    int spriteAsset = -1;
    bool wander = false;
    int x = 0, y = 0, destX = 0, destY = 0;
    float px = 0, py = 0;
    bool moving = false;
    int dir = 0;
    float moveCd = 0;
    int frame = 0; float animTime = 0;
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
};

// --- atmosphere -------------------------------------------------------------

struct Particle { float x, y, vx, vy, life; };

// --- skills -----------------------------------------------------------------

// Player skill key slots: Z, X, C, V, F, G.
inline constexpr int kSkillSlots = 6;
enum SkillSlot { SK_Attack = 0, SK_Ranged = 1, SK_Dash = 2, SK_Ult = 3 };

} // namespace tsukuru
