#pragma once
// GamePlay: the runtime that plays a project as a game. Movement is grid-based
// with smooth interpolation. Combat is real-time and on-field (Kingdom-of-the-
// Winds style): monsters roam the map and the player kills them with melee
// attacks in the facing direction — there is no separate battle screen.
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "world/Map.h"
#include "core/Types.h"
#include "database/Database.h"

namespace tsukuru {

class Engine;
class Menu;

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

struct Particle { float x, y, vx, vy, life; };

// A travelling ranged bolt (X skill).
struct Projectile {
    float px = 0, py = 0;   // pixel position
    int dir = 0;
    float life = 0;
    int dmg = 0;
};

// A short-lived visual effect. If assetId >= 0 it draws an animated sprite
// sheet (4 frames x 4 dirs, like a character); otherwise a procedural shape.
struct SkillFx {
    int type = 0;           // 0 slash, 1 bolt-impact, 2 dash, 3 aoe-ring
    float px = 0, py = 0;
    float t = 0, dur = 0.25f;
    int dir = 0;
    int assetId = -1;
    float radius = 0;       // for aoe ring
};

// Player skill key slots: Z, X, C, V, F, G.
static constexpr int kSkillSlots = 6;
enum SkillSlot { SK_Attack = 0, SK_Ranged = 1, SK_Dash = 2, SK_Ult = 3 };

class GamePlay {
public:
    explicit GamePlay(Engine& engine);
    ~GamePlay();

    void onEnter();      // called when switching into play mode
    void update(float dt);
    void draw();

private:
    enum class Phase { Field, Message, Menu, GameOver, GameClear };

    void loadMap(int id);
    void updateField(float dt);
    void tryMove(Direction d);
    void interact();
    Event* actionEventAt(int x, int y);
    void runEvent(Event& e);
    void drawField();
    void drawMessage();
    void showMessage(const std::string& text); // splits on '|' into pages
    void drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint = WHITE, int frames = 4);

    // --- field combat ---
    void spawnMonsters();
    void spawnOne();
    void updateMonsters(float dt);
    void drawMonsters();
    FieldMonster* monsterAt(int x, int y);
    bool walkable(int x, int y);            // not blocked / not occupied
    void onMonsterKilled(const FieldMonster& m);

    // --- data-driven skills (Z/X/C/V/F/G): pattern, projectile, blink, FX ----
    void loadSkills();                       // pull from db (or built-in defaults)
    void castSlot(int slot);                 // cast the skill bound to a key slot
    void castFieldSkill(const FieldSkill& s, int slot);
    void playerAttack();                     // convenience: cast slot 0
    bool damageMonster(FieldMonster& m, int dmg); // returns true if killed
    void spawnFx(int type, float px, float py, int dir, int assetId, float dur, float radius = 0);
    void updateProjectiles(float dt);
    void updateFx(float dt);
    void drawProjectiles();
    void drawFx();
    void drawSkillPanel();                   // right-side cooldown/description panel
    void handleSkillClicks();                // touch/click to cast
    static Vec2i rotateToFacing(int ox, int oy, int dir); // canonical up -> facing

    // --- NPCs / atmosphere / hud ---
    void spawnNpcs();
    void updateNpcs(float dt);
    void drawNpcs();
    NpcInst* npcAt(int x, int y);
    void runAutoruns();
    void drawWeather(float dt);
    void drawMinimap();
    void visibleRange(int& x0, int& y0, int& x1, int& y1) const; // tile culling

    Engine& engine_;
    std::shared_ptr<Map> map_;
    Camera2D cam_{};

    Phase phase_ = Phase::Field;

    // Player smooth movement (grid -> pixel interpolation)
    float pxX_ = 0, pxY_ = 0;
    int   destX_ = 0, destY_ = 0;
    bool  moving_ = false;
    int   dir_ = 0;
    float animTime_ = 0;
    int   frame_ = 0;

    // Player melee attack
    float attackTimer_ = 0;     // >0 while the slash effect shows
    float playerHurt_ = 0;      // red flash when the player takes damage

    // Skills: per-slot cooldown remaining (Z/X/C/V/F/G).
    float skillCd_[kSkillSlots] = {};
    Rectangle skillBtn_[kSkillSlots] = {}; // screen rects for click/touch casting
    float mpRegen_ = 0;         // MP regenerates slowly over time
    std::vector<FieldSkill> skills_;       // active skill set (from db or defaults)

    std::vector<FieldMonster> monsters_;
    std::vector<NpcInst>      npcs_;
    std::vector<Projectile>   projectiles_;
    std::vector<SkillFx>      fx_;
    std::vector<Particle>     weatherP_;
    Texture2D minimapTex_{};    // cached minimap terrain (rebuilt once per map)
    bool minimapValid_ = false;
    int  minimapW_ = 0, minimapH_ = 0;
    float worldTime_ = 0;       // seconds, drives day/night cycle
    float spawnTimer_ = 0;
    int   targetMonsters_ = 0;
    std::string toast_;
    float toastTimer_ = 0;

    // message box (supports multiple pages split on '|')
    std::string message_;
    std::vector<std::string> msgPages_;
    int msgPage_ = 0;

    std::unique_ptr<Menu> menu_;
};

} // namespace tsukuru
