#pragma once
// Event: a data-driven interactive object placed on a map tile.
// Covers the common RPG Maker event kinds: dialogue, teleport, shop, battle,
// item-give, and switch-setting. Conditions gate visibility/activation.
#include <string>
#include <nlohmann/json.hpp>

namespace tsukuru {

enum class EventType {
    Message,    // show dialogue text
    Teleport,   // move player to another map/position
    GiveItem,   // add an item to inventory (also sets switchId if >= 0)
    SetSwitch,  // turn a switch on/off
    StartBattle,// spawn field monsters (switchId = defeat switch, set when cleared)
    Shop,       // open a shop
    Quest,      // set the on-screen quest objective (and show the text)
    Ending      // roll the victory / game-clear screen
};

enum class TriggerType {
    ActionButton, // player presses interact key while facing the event
    PlayerTouch,  // triggers when the player steps onto the tile
    Autorun       // triggers automatically when conditions met
};

// Which side an NPC (an event with a sprite) belongs to. Drives whether it
// fights the player, fights for the player, or simply lives on the map.
enum class NpcFaction {
    Neutral = 0,  // 중립: harmless townsfolk/critters — only dialogue/atmosphere
    Ally    = 1,  // 아군: fights enemies/monsters near it, never hurts the player
    Enemy   = 2   // 적군: chases & attacks the player, killable in the field
};

// How an NPC moves when it has nothing more urgent to do. Combat factions
// override this with chase/attack when a target is in range.
enum class NpcBehavior {
    Idle   = 0,   // 대기: stand still (faces the player when adjacent)
    Wander = 1,   // 배회: random roaming
    Patrol = 2,   // 순찰: pace back and forth along an axis
    Chase  = 3,   // 추격/동행: enemies hunt the player; allies follow the player
    Flee   = 4    // 도망: back away from the player when close
};

struct Event {
    int         id = -1;
    int         x = 0, y = 0;
    EventType   type    = EventType::Message;
    TriggerType trigger = TriggerType::ActionButton;
    int         graphicAsset = -1; // optional sprite shown on the map

    // Generic parameters (interpreted per type):
    std::string text;        // Message / Shop title
    int         targetMap = -1;       // Teleport
    int         targetX = 0, targetY = 0; // Teleport
    int         itemId = -1;          // GiveItem / Shop
    int         amount = 1;           // GiveItem amount / price etc.
    int         switchId = -1;        // SetSwitch / condition
    bool        switchValue = true;   // SetSwitch value

    // Activation condition: if conditionSwitch >= 0, event only acts when that
    // switch == conditionValue.
    int  conditionSwitch = -1;
    bool conditionValue  = true;
    int  conditionVar    = -1;   // also gate on a variable: fires only if var >= conditionVarMin
    int  conditionVarMin = 1;
    bool once = false; // run only one time (sets a hidden flag)
    bool wander = false; // legacy roam flag (kept in sync with behavior==Wander)

    // --- Message extras (speaker name plate, portrait, yes/no choice) ---
    std::string speakerName;     // name shown above the message box
    int  faceAsset = -1;         // portrait image drawn in the message box
    std::string choiceA, choiceB;// if both non-empty, show a 2-way choice
    int  choiceSwitch = -1;      // set true if A chosen, false if B chosen

    // --- GiveItem extras: also give/remove gold; amount<0 removes items ---
    int  giveGold = 0;

    // --- SetSwitch extras: optionally set/add a variable too ---
    int  varId = -1;             // -1 = none
    int  varOp = 0;              // 0 = 대입(set), 1 = 증가(add)
    int  varValue = 0;

    // --- Teleport extra: facing after arrival (-1 = keep) ---
    int  faceDir = -1;

    // --- StartBattle extra: true = 즉시 턴제 전투(아니면 필드 스폰) ---
    bool battleTurnBased = false;

    // --- Shop extra: multiple wares (falls back to itemId when empty) ---
    std::vector<int> shopItems;

    // --- Quest extra: set a switch ON when the quest is completed ---
    int  rewardSwitch = -1;

    // --- Quest / reward (EventType::Quest) ---
    // A self-contained quest: the giver NPC offers it on first talk, the player
    // fulfils the objective, then talks again to claim the reward. No separate
    // quest registry needed — everything a quest needs lives on its event.
    int  questObjective = 0; // 0 즉시지급 / 1 몬스터 처치 / 2 아이템 수집 / 3 지역 도달
    int  questTarget    = -1;// 처치: 적ID(-1=아무거나) · 수집: 아이템ID · 도달: 맵ID
    int  questCount     = 1; // required kills / items
    std::string questDoneText;     // shown when the reward is claimed (optional)
    int  rewardGold     = 0;
    int  rewardExp      = 0;
    int  rewardItemId   = -1;
    int  rewardItemCount= 1;
    bool questTakeItems = true;    // remove collected items on turn-in (수집형)

    // --- NPC presentation & behaviour (only used when graphicAsset >= 0) ---
    NpcFaction  faction  = NpcFaction::Neutral;
    NpcBehavior behavior = NpcBehavior::Idle;
    int  drawPct = 100;  // fine size %, applied on top of the tile footprint (100 = exact)
    int  drawTilesW = 1; // tile footprint WIDTH  the NPC occupies on the map (칸)
    int  drawTilesH = 1; // tile footprint HEIGHT the NPC occupies on the map (칸)
    // Combat stats for Ally/Enemy NPCs (ignored for Neutral):
    int  npcHp  = 20;    // 체력
    int  npcAtk = 8;     // 공격력
    int  npcDef = 2;     // 방어력

    nlohmann::json toJson() const;
    static Event fromJson(const nlohmann::json& j);
};

} // namespace tsukuru
