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

class GamePlay {
public:
    explicit GamePlay(Engine& engine);
    ~GamePlay();

    void onEnter();      // called when switching into play mode
    void update(float dt);
    void draw();

private:
    enum class Phase { Field, Message, Menu, GameOver };

    void loadMap(int id);
    void updateField(float dt);
    void tryMove(Direction d);
    void interact();
    void runEvent(Event& e);
    void drawField();
    void drawMessage();
    void showMessage(const std::string& text); // splits on '|' into pages
    void drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint = WHITE);

    // --- field combat ---
    void spawnMonsters();
    void spawnOne();
    void updateMonsters(float dt);
    void playerAttack();
    void drawMonsters();
    FieldMonster* monsterAt(int x, int y);
    bool walkable(int x, int y);            // not blocked / not occupied
    void onMonsterKilled(const FieldMonster& m);

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
    float attackCd_ = 0;        // cooldown between swings
    float playerHurt_ = 0;      // red flash when the player takes damage

    std::vector<FieldMonster> monsters_;
    std::vector<NpcInst>      npcs_;
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
