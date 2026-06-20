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
        {"drawPct", drawPct},
        {"npcHp", npcHp}, {"npcAtk", npcAtk}, {"npcDef", npcDef}
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
    e.npcHp           = j.value("npcHp", 20);
    e.npcAtk          = j.value("npcAtk", 8);
    e.npcDef          = j.value("npcDef", 2);
    return e;
}

} // namespace tsukuru
