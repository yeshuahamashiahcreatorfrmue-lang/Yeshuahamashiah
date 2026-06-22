#include "entity/Story.h"

using nlohmann::json;

namespace tsukuru {

static json answerToJson(const DialogueAnswer& a) {
    return {{"text", a.text}, {"respType", a.respType},
            {"rewardGold", a.rewardGold}, {"rewardExp", a.rewardExp},
            {"rewardItemId", a.rewardItemId}, {"rewardItemCount", a.rewardItemCount},
            {"mobId", a.mobId}, {"npcCharId", a.npcCharId}, {"durationSecs", a.durationSecs},
            {"sceneId", a.sceneId}, {"gotoLine", a.gotoLine}, {"dismissFollowers", a.dismissFollowers}};
}
static DialogueAnswer answerFromJson(const json& j) {
    DialogueAnswer a;
    a.text = j.value("text", "");
    a.respType = j.value("respType", (int)DR_None);
    a.rewardGold = j.value("rewardGold", 0); a.rewardExp = j.value("rewardExp", 0);
    a.rewardItemId = j.value("rewardItemId", -1); a.rewardItemCount = j.value("rewardItemCount", 1);
    a.mobId = j.value("mobId", -1); a.npcCharId = j.value("npcCharId", -1);
    a.durationSecs = j.value("durationSecs", 0.0f);
    a.sceneId = j.value("sceneId", -1);
    a.gotoLine = j.value("gotoLine", -1); a.dismissFollowers = j.value("dismissFollowers", false);
    return a;
}

json dialogueToJson(const DialogueScenario& d) {
    json lines = json::array();
    for (const auto& l : d.lines) {
        json ans = json::array();
        for (const auto& a : l.answers) ans.push_back(answerToJson(a));
        lines.push_back({{"speaker", l.speaker}, {"text", l.text},
                         {"speakerAsset", l.speakerAsset}, {"answers", ans}});
    }
    return {{"id", d.id}, {"name", d.name}, {"lines", lines}};
}
DialogueScenario dialogueFromJson(const json& j) {
    DialogueScenario d;
    d.id = j.value("id", -1); d.name = j.value("name", "대화");
    for (const auto& lj : j.value("lines", json::array())) {
        DialogueLine l;
        l.speaker = lj.value("speaker", ""); l.text = lj.value("text", "");
        l.speakerAsset = lj.value("speakerAsset", -1);
        for (const auto& aj : lj.value("answers", json::array())) l.answers.push_back(answerFromJson(aj));
        d.lines.push_back(l);
    }
    return d;
}

json sceneToJson(const Scene& s) {
    json acts = json::array();
    for (const auto& a : s.actions) {
        json rt = json::array(); for (int t : a.removeTags) rt.push_back(t);
        acts.push_back({{"type", a.type}, {"targetId", a.targetId}, {"refId", a.refId},
                        {"x", a.x}, {"y", a.y}, {"time", a.time},
                        {"radius", a.radius}, {"removeTags", rt}});
    }
    return {{"id", s.id}, {"name", s.name}, {"editMapId", s.editMapId},
            {"group", s.group},
            {"camMode", s.camMode}, {"camZoom", s.camZoom}, {"camX", s.camX}, {"camY", s.camY}, {"camTag", s.camTag},
            {"bgmAsset", s.bgmAsset}, {"actions", acts}};
}
Scene sceneFromJson(const json& j) {
    Scene s;
    s.id = j.value("id", -1); s.name = j.value("name", "장면");
    s.editMapId = j.value("editMapId", -1);
    s.group = j.value("group", std::string());
    s.camMode = j.value("camMode", 0); s.camZoom = j.value("camZoom", 2.0f);
    s.camX = j.value("camX", 0); s.camY = j.value("camY", 0); s.camTag = j.value("camTag", 0);
    s.bgmAsset = j.value("bgmAsset", -1);
    for (const auto& aj : j.value("actions", json::array())) {
        SceneAction a;
        a.type = aj.value("type", (int)SA_Wait);
        a.targetId = aj.value("targetId", -1); a.refId = aj.value("refId", -1);
        a.x = aj.value("x", 0); a.y = aj.value("y", 0); a.time = aj.value("time", 1.0f);
        a.radius = aj.value("radius", 1);
        for (const auto& tj : aj.value("removeTags", json::array())) a.removeTags.push_back(tj.get<int>());
        s.actions.push_back(a);
    }
    return s;
}

} // namespace tsukuru
