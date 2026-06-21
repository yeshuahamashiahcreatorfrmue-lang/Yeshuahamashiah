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
#include "game/GameState.h"
#include "battle/Battle.h"

namespace tsukuru {

class Engine;
class Menu;

// Event activation gate (switch AND variable condition). Shared by interaction,
// autoruns and touch triggers so every path honours the same rules.
bool eventConditionMet(GameState& gs, const Event& e);

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
    enum class Phase { Field, Message, Menu, Battle, Shop, GameOver, GameClear };

    // --- lifecycle / dispatch (GamePlay.cpp) ---
    void loadMap(int id);

    // --- movement (GamePlayMovement.cpp) ---
    void updateField(float dt);
    void tryMove(Direction d);
    void tryZoneTransition(Direction d);    // step on a middle-edge gate -> adjacent placed map
    bool zoneEdgeDir(int x, int y, Direction& out) const; // is (x,y) a middle-7 edge gate? dir out
    void carveZoneGates();                  // open the middle-7 edge tiles toward placed neighbours
    bool walkable(int x, int y);            // not blocked / not occupied (any mover)

    // --- events / dialogue (GamePlayEvents.cpp) ---
    void interact();
    Event* actionEventAt(int x, int y);
    void runEvent(Event& e);
    void showMessage(const std::string& text); // splits on '|' into pages
    // rich message: speaker name plate, portrait, and an optional 2..4-way choice.
    void showMessageEx(const std::string& text, const std::string& speaker, int faceAsset,
                       const std::vector<std::string>& choices, int choiceSwitch, int choiceVar);
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

    // --- turn-based battle (random encounters; surfaces the Battle system) ---
    void startEncounterBattle();   // build a troop from the map's encounter list
    void startBattleWith(const std::vector<int>& enemyIds); // event-driven turn battle
    void updateBattle(float dt);
    void drawBattle();

    // --- survival (hunger/thirst), inventory grid & body-part equipment ---
    void drawInventoryOverlay();   // I key: rectangular item grid (식품/장비/기타)
    void drawEquipOverlay();       // C key: body-part equipment window
    void useOrEquipItem(int itemId); // consume food (restore) or equip equipment
    void unequipSlot(int slot);

    // --- shop (EventType::Shop opens an on-screen buy screen) ---
    void openShop(const std::vector<int>& items, const std::string& title);
    void updateShop(float dt);
    void drawShop();

    // --- quests / NPC rewards (EventType::Quest) ---
    void runQuestEvent(Event& e);   // accept / check progress / claim reward flow
    bool questObjectiveMet(const Event& e, const QuestState& q) const;
    std::string questProgressText(const Event& e, const QuestState& q) const;
    void grantQuestReward(const Event& e); // gold/exp/item + toast
    void refreshQuestObjective();   // sync HUD objective to the first active quest
    void drawQuestLog();            // J: list of active/finished quests
    void drawDebugVars();           // F3: switch/variable inspector (event testing)
    void drawEventMarkers();        // floating !/?/$/+/bubble over interactable events
    void drawHelp();                // F1: on-screen controls reference

    // --- atmosphere / rendering (GamePlayRender.cpp) ---
    void drawField();
    void drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint = WHITE, int frames = 4, float wScale = 1.0f, float hScale = 1.0f);
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

    // survival + inventory/equipment windows
    float survivalAcc_ = 0;     // accumulates dt to drain hunger/thirst 1/sec
    bool  invOpen_ = false;     // I: inventory grid window open
    bool  equipOpen_ = false;   // C: body-part equipment window open
    int   invCat_ = 0;          // inventory category tab (0 전체/1 식품/2 장비/3 기타)
    Rectangle invBtn_{}, equipBtn_{}; // bottom HUD buttons
    std::unique_ptr<Battle> battle_;  // active turn-based battle (null when none)
    int   battleMenu_ = 0;      // 0 root / 1 skill submenu / 2 item submenu

    // shop screen state
    std::vector<int> shopItems_; // items the current shop sells
    std::string shopTitle_;      // shop window title (event text)
    bool  questLogOpen_ = false;// J: quest log overlay
    bool  debugVarsOpen_ = false;// F3: switch/variable inspector
    bool  helpOpen_ = false;     // F1: controls help overlay

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
    std::string areaBanner_;   // map name shown briefly when entering a new area
    float areaBannerT_ = 0;

    // message box (supports multiple pages split on '|')
    std::string message_;
    std::vector<std::string> msgPages_;
    int msgPage_ = 0;
    // rich-message extras
    std::string msgSpeaker_;
    int  msgFace_ = -1;
    std::vector<std::string> msgChoices_;  // 2..4 options when a choice is pending
    int  msgChoiceSwitch_ = -1;            // 2-way compat switch
    int  msgChoiceVar_ = -1;               // variable receiving the chosen index

    std::unique_ptr<Menu> menu_;
};

} // namespace tsukuru
