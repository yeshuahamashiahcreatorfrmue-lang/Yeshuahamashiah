#include "entity/Event.h"

using nlohmann::json;

namespace tsukuru {

static const char* eventTypeName(EventType t) {
    switch (t) {
        case EventType::Message:     return "message";
        case EventType::Teleport:    return "teleport";
        case EventType::GiveItem:    return "giveItem";
        case EventType::SetSwitch:   return "setSwitch";
        case EventType::StartBattle: return "startBattle";
        case EventType::Shop:        return "shop";
        case EventType::Quest:       return "quest";
        case EventType::Ending:      return "ending";
    }
    return "message";
}
static EventType eventTypeFrom(const std::string& s) {
    if (s == "teleport")    return EventType::Teleport;
    if (s == "giveItem")    return EventType::GiveItem;
    if (s == "setSwitch")   return EventType::SetSwitch;
    if (s == "startBattle") return EventType::StartBattle;
    if (s == "shop")        return EventType::Shop;
    if (s == "quest")       return EventType::Quest;
    if (s == "ending")      return EventType::Ending;
    return EventType::Message;
}
static const char* triggerName(TriggerType t) {
    switch (t) {
        case TriggerType::ActionButton: return "action";
        case TriggerType::PlayerTouch:  return "touch";
        case TriggerType::Autorun:      return "autorun";
    }
    return "action";
}
static TriggerType triggerFrom(const std::string& s) {
    if (s == "touch")   return TriggerType::PlayerTouch;
    if (s == "autorun") return TriggerType::Autorun;
    return TriggerType::ActionButton;
}

json Event::toJson() const {
    return {
        {"id", id}, {"x", x}, {"y", y},
        {"type", eventTypeName(type)}, {"trigger", triggerName(trigger)},
        {"graphicAsset", graphicAsset},
        {"text", text},
        {"targetMap", targetMap}, {"targetX", targetX}, {"targetY", targetY},
        {"itemId", itemId}, {"amount", amount},
        {"switchId", switchId}, {"switchValue", switchValue},
        {"conditionSwitch", conditionSwitch}, {"conditionValue", conditionValue},
        {"once", once},
        {"wander", behavior == NpcBehavior::Wander},
        {"faction", (int)faction}, {"behavior", (int)behavior},
        {"drawPct", drawPct}, {"drawTilesW", drawTilesW}, {"drawTilesH", drawTilesH},
        {"npcHp", npcHp}, {"npcAtk", npcAtk}, {"npcDef", npcDef},
        {"questObjective", questObjective}, {"questTarget", questTarget},
        {"questCount", questCount}, {"questDoneText", questDoneText},
        {"rewardGold", rewardGold}, {"rewardExp", rewardExp},
        {"rewardItemId", rewardItemId}, {"rewardItemCount", rewardItemCount},
        {"questTakeItems", questTakeItems},
        {"conditionVar", conditionVar}, {"conditionVarMin", conditionVarMin},
        {"speakerName", speakerName}, {"faceAsset", faceAsset},
        {"choiceA", choiceA}, {"choiceB", choiceB}, {"choiceSwitch", choiceSwitch},
        {"giveGold", giveGold},
        {"varId", varId}, {"varOp", varOp}, {"varValue", varValue},
        {"faceDir", faceDir}, {"battleTurnBased", battleTurnBased},
        {"shopItems", shopItems}, {"rewardSwitch", rewardSwitch}
    };
}

Event Event::fromJson(const json& j) {
    Event e;
    e.id              = j.value("id", -1);
    e.x               = j.value("x", 0);
    e.y               = j.value("y", 0);
    e.type            = eventTypeFrom(j.value("type", "message"));
    e.trigger         = triggerFrom(j.value("trigger", "action"));
    e.graphicAsset    = j.value("graphicAsset", -1);
    e.text            = j.value("text", "");
    e.targetMap       = j.value("targetMap", -1);
    e.targetX         = j.value("targetX", 0);
    e.targetY         = j.value("targetY", 0);
    e.itemId          = j.value("itemId", -1);
    e.amount          = j.value("amount", 1);
    e.switchId        = j.value("switchId", -1);
    e.switchValue     = j.value("switchValue", true);
    e.conditionSwitch = j.value("conditionSwitch", -1);
    e.conditionValue  = j.value("conditionValue", true);
    e.once            = j.value("once", false);
    e.wander          = j.value("wander", false);
    e.faction         = (NpcFaction)j.value("faction", 0);
    // behavior: if absent, fall back to the legacy wander flag (Wander/Idle).
    if (j.contains("behavior")) e.behavior = (NpcBehavior)j.value("behavior", 0);
    else                        e.behavior = e.wander ? NpcBehavior::Wander : NpcBehavior::Idle;
    e.drawPct         = j.value("drawPct", 100);
    e.drawTilesW      = j.value("drawTilesW", 1);
    e.drawTilesH      = j.value("drawTilesH", 1);
    e.npcHp           = j.value("npcHp", 20);
    e.npcAtk          = j.value("npcAtk", 8);
    e.npcDef          = j.value("npcDef", 2);
    e.questObjective  = j.value("questObjective", 0);
    e.questTarget     = j.value("questTarget", -1);
    e.questCount      = j.value("questCount", 1);
    e.questDoneText   = j.value("questDoneText", "");
    e.rewardGold      = j.value("rewardGold", 0);
    e.rewardExp       = j.value("rewardExp", 0);
    e.rewardItemId    = j.value("rewardItemId", -1);
    e.rewardItemCount = j.value("rewardItemCount", 1);
    e.questTakeItems  = j.value("questTakeItems", true);
    e.conditionVar    = j.value("conditionVar", -1);
    e.conditionVarMin = j.value("conditionVarMin", 1);
    e.speakerName     = j.value("speakerName", "");
    e.faceAsset       = j.value("faceAsset", -1);
    e.choiceA         = j.value("choiceA", "");
    e.choiceB         = j.value("choiceB", "");
    e.choiceSwitch    = j.value("choiceSwitch", -1);
    e.giveGold        = j.value("giveGold", 0);
    e.varId           = j.value("varId", -1);
    e.varOp           = j.value("varOp", 0);
    e.varValue        = j.value("varValue", 0);
    e.faceDir         = j.value("faceDir", -1);
    e.battleTurnBased = j.value("battleTurnBased", false);
    e.shopItems       = j.value("shopItems", std::vector<int>{});
    e.rewardSwitch    = j.value("rewardSwitch", -1);
    return e;
}

} // namespace tsukuru
