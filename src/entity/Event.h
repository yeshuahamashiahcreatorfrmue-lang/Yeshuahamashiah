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
    bool once = false; // run only one time (sets a hidden flag)
    bool wander = false; // NPC roams the map autonomously

    nlohmann::json toJson() const;
    static Event fromJson(const nlohmann::json& j);
};

} // namespace tsukuru
