#pragma once
// GamePlay: the runtime that plays a project as a game. Movement is grid-based
// with smooth interpolation. Combat is real-time and on-field (Kingdom-of-the-
// Winds style): monsters roam the map and the player kills them with melee
// attacks in the facing direction — there is no separate battle screen.
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <unordered_set>
#include "raylib.h"
#include "world/Map.h"
#include "core/Types.h"
#include "database/Database.h"
#include "game/PlayTypes.h"

namespace tsukuru {

class Engine;
class Menu;

// GamePlay is a thin controller; its method bodies live in purpose-grouped
// translation units (GamePlay, GamePlayMovement, GamePlayEvents,
// GamePlayMonsters, GamePlaySkills, GamePlayNpc, GamePlayRender). The member
// declarations below mirror that grouping.
class GamePlay {
public:
    explicit GamePlay(Engine& engine);
    ~GamePlay();

    void onEnter();      // called when switching into play mode
    void update(float dt);
    void draw();

private:
    enum class Phase { Field, Message, Menu, GameOver, GameClear };

    // --- lifecycle / dispatch (GamePlay.cpp) ---
    void loadMap(int id);

    // --- movement (GamePlayMovement.cpp) ---
    void updateField(float dt);
    void tryMove(Direction d);
    void tryZoneTransition(Direction d);    // walk off an edge -> load the adjacent placed map
    bool walkable(int x, int y);            // not blocked / not occupied (any mover)

    // --- events / dialogue (GamePlayEvents.cpp) ---
    void interact();
    Event* actionEventAt(int x, int y);
    void runEvent(Event& e);
    void showMessage(const std::string& text); // splits on '|' into pages
    void drawMessage();

    // --- field monsters (GamePlayMonsters.cpp) ---
    void spawnMonsters();
    void spawnOne();
    void updateMonsters(float dt);
    void drawMonsters();
    FieldMonster* monsterAt(int x, int y);
    bool damageMonster(FieldMonster& m, int dmg); // returns true if killed
    void onMonsterKilled(const FieldMonster& m);
    void reapDead();                              // drop killed monsters & NPCs in one pass

    // --- data-driven skills (GamePlaySkills.cpp): definition + cast + HUD ---
    void loadSkills();                       // pull from db (or built-in defaults)
    void castSlot(int slot);                 // cast the skill bound to a key slot
    void castFieldSkill(const FieldSkill& s, int slot);
    void spawnSkillEffect(const FieldSkill& s);  // place the visual effect (per-tile / one-big / scaled)
    void playerAttack();                     // convenience: cast slot 0
    void drawSkillPanel();                   // right-side cooldown/description panel
    void handleSkillClicks();                // touch/click to cast
    static Vec2i rotateToFacing(int ox, int oy, int dir); // canonical up -> facing

    // --- projectiles & visual effects (GamePlayFx.cpp) ---
    void spawnFx(int type, float px, float py, int dir, int assetId, float dur, float radius = 0, int loops = 1, float sizePx = 0);
    void updateProjectiles(float dt);
    void updateFx(float dt);
    void drawProjectiles();
    void drawFx();

    // --- NPCs / autoruns (GamePlayNpc.cpp) ---
    void spawnNpcs();
    void updateNpcs(float dt);
    void drawNpcs();
    NpcInst* npcAt(int x, int y);
    NpcInst* hostileNpcAt(int x, int y);    // a living Enemy-faction NPC the player can hit
    bool damageNpc(NpcInst& n, int dmg);    // returns true if it died
    void onNpcKilled(NpcInst& n);
    void npcDecide(NpcInst& n, float dt);   // pick the next move/facing for one NPC
    void runAutoruns();

    // --- custom-character motion playback (GamePlayRender.cpp) ---
    const CharacterDef* customChar() const;  // null unless a CharacterDef drives the player
    void triggerMotion(int motionId);        // play a one-shot motion (attack/skill/death)
    void updateMotion(float dt);             // advance the current motion's frames
    int  motionFrameAsset() const;           // current frame's image id, or -1 (use sheet)

    // --- atmosphere / rendering (GamePlayRender.cpp) ---
    void drawField();
    void drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint = WHITE, int frames = 4, float scale = 1.0f);
    void drawWeather(float dt);
    void drawMinimap();
    void visibleRange(int& x0, int& y0, int& x1, int& y1) const; // tile culling

    Engine& engine_;
    std::shared_ptr<Map> map_;
    Camera2D cam_{};

    Phase phase_ = Phase::Field;
    std::set<long> firedOnce_;   // (mapId<<16 | eventId) one-shot events this session
    std::unordered_set<int> animTileSet_;  // map's animated tile ids (built once per map)

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

    // custom-character motion playback
    int   playMotion_ = 0;      // MotionId currently playing (MO_Walk by default)
    int   motionFrame_ = 0;     // current frame index within the motion
    float motionAnim_ = 0;      // frame timer
    float motionTimer_ = 0;     // >0 while a non-walk one-shot motion plays
    float dyingTimer_ = 0;      // death-motion countdown before GameOver

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
