#include "database/Database.h"

using nlohmann::json;

namespace tsukuru {

// ---- FieldSkill <-> json (shared by global field skills and per-character skills) ----
static json skillToJson(const FieldSkill& s) {
    return {{"id", s.id}, {"name", s.name}, {"slot", s.slot},
            {"projectile", s.projectile}, {"blink", s.blink}, {"range", s.range},
            {"mpCost", s.mpCost}, {"cooldown", s.cooldown}, {"powerPct", s.powerPct},
            {"patX", s.patX}, {"patY", s.patY},
            {"effectAsset", s.effectAsset}, {"effectLoops", s.effectLoops},
            {"effectDist", s.effectDist}, {"soundAsset", s.soundAsset},
            {"efxX", s.efxX}, {"efxY", s.efxY},
            {"effectMode", s.effectMode}, {"effectScale", s.effectScale}};
}
static FieldSkill skillFromJson(const json& s) {
    FieldSkill fs;
    fs.id = s.value("id", -1); fs.name = s.value("name", "스킬");
    fs.slot = s.value("slot", -1); fs.projectile = s.value("projectile", false);
    fs.blink = s.value("blink", 0); fs.range = s.value("range", 6);
    fs.mpCost = s.value("mpCost", 0); fs.cooldown = s.value("cooldown", 0.5f);
    fs.powerPct = s.value("powerPct", 100);
    fs.patX = s.value("patX", std::vector<int>{});
    fs.patY = s.value("patY", std::vector<int>{});
    fs.effectAsset = s.value("effectAsset", -1);
    fs.effectLoops = s.value("effectLoops", 1);
    fs.effectDist = s.value("effectDist", 0);
    fs.soundAsset = s.value("soundAsset", -1);
    fs.efxX = s.value("efxX", std::vector<int>{});
    fs.efxY = s.value("efxY", std::vector<int>{});
    fs.effectMode = s.value("effectMode", 0);
    fs.effectScale = s.value("effectScale", 100);
    return fs;
}

// ---- effect enum <-> string ----
static const char* effName(ItemEffect e) {
    switch (e) {
        case ItemEffect::HealHP: return "healHP";
        case ItemEffect::HealMP: return "healMP";
        case ItemEffect::Damage: return "damage";
        default: return "none";
    }
}
static ItemEffect effFrom(const std::string& s) {
    if (s == "healHP") return ItemEffect::HealHP;
    if (s == "healMP") return ItemEffect::HealMP;
    if (s == "damage") return ItemEffect::Damage;
    return ItemEffect::None;
}

// ---- lookups ----
const Item* Database::item(int id) const {
    for (const auto& i : items) if (i.id == id) return &i;
    return nullptr;
}
const ActorDef* Database::actor(int id) const {
    for (const auto& a : actors) if (a.id == id) return &a;
    return nullptr;
}
const EnemyDef* Database::enemy(int id) const {
    for (const auto& e : enemies) if (e.id == id) return &e;
    return nullptr;
}
const FieldSkill* Database::fieldSkillForSlot(int slot) const {
    for (const auto& s : fieldSkills) if (s.slot == slot) return &s;
    return nullptr;
}
const CharacterDef* Database::mob(int id) const {
    for (const auto& c : mobs) if (c.id == id) return &c;
    return nullptr;
}
const DialogueScenario* Database::dialogue(int id) const {
    for (const auto& d : dialogues) if (d.id == id) return &d;
    return nullptr;
}
const CharacterDef* Database::character(int id) const {
    for (const auto& c : characters) if (c.id == id) return &c;
    return nullptr;
}

std::vector<FieldSkill> Database::defaultFieldSkills() {
    FieldSkill atk;  atk.id=1; atk.name="공격"; atk.slot=0; atk.cooldown=0.32f;
        atk.powerPct=100; atk.patX={0,0}; atk.patY={0,-1};         // own + front tile
    FieldSkill rng;  rng.id=2; rng.name="원거리"; rng.slot=1; rng.projectile=true;
        rng.range=8; rng.mpCost=4; rng.cooldown=0.9f; rng.powerPct=130;
    FieldSkill dsh;  dsh.id=3; dsh.name="회피 이동"; dsh.slot=2; dsh.blink=4;
        dsh.cooldown=1.6f; dsh.powerPct=0;                          // movement only
    FieldSkill ult;  ult.id=4; ult.name="궁극기"; ult.slot=3; ult.blink=5;
        ult.mpCost=16; ult.cooldown=8.0f; ult.powerPct=200;
        for (int dy=-2; dy<=2; ++dy) for (int dx=-2; dx<=2; ++dx)   // 5x5 area
            { ult.patX.push_back(dx); ult.patY.push_back(dy); }
    return { atk, rng, dsh, ult };
}

// ---- CharacterDef (de)serialization (shared by characters AND mobs) ----
static json charToJson(const CharacterDef& c) {
    json mo = json::array();
    for (const auto& m : c.motions) mo.push_back({{"frames", m.frames},
        {"left", m.left}, {"right", m.right}, {"up", m.up}, {"fps", m.fps}, {"loop", m.loop}});
    json sk = json::array();
    for (const auto& s : c.skills) sk.push_back(skillToJson(s));
    return {{"id", c.id}, {"name", c.name},
        {"maxHp", c.maxHp}, {"maxGp", c.maxGp}, {"maxHunger", c.maxHunger}, {"maxThirst", c.maxThirst},
        {"atk", c.atk}, {"def", c.def}, {"spd", c.spd},
        {"drawPct", c.drawPct}, {"drawTilesW", c.drawTilesW}, {"drawTilesH", c.drawTilesH},
        {"motions", mo}, {"skills", sk},
        {"expReward", c.expReward}, {"goldReward", c.goldReward},
        {"dropItemId", c.dropItemId}, {"dropRate", c.dropRate},
        {"spawnFreezeSecs", c.spawnFreezeSecs}, {"respawnSecs", c.respawnSecs}};
}
static CharacterDef charFromJson(const json& c) {
    CharacterDef cd;
    cd.id = c.value("id", -1); cd.name = c.value("name", "캐릭터");
    cd.maxHp = c.value("maxHp", 100); cd.maxGp = c.value("maxGp", 30);
    cd.maxHunger = c.value("maxHunger", 42000); cd.maxThirst = c.value("maxThirst", 42000);
    cd.atk = c.value("atk", 12); cd.def = c.value("def", 5); cd.spd = c.value("spd", 5);
    cd.drawPct = c.value("drawPct", 125);
    cd.drawTilesW = c.value("drawTilesW", 1); cd.drawTilesH = c.value("drawTilesH", 1);
    const auto& mo = c.value("motions", json::array());
    for (int i = 0; i < MO_COUNT && i < (int)mo.size(); ++i) {
        cd.motions[i].frames = mo[i].value("frames", std::vector<int>{});
        cd.motions[i].left   = mo[i].value("left",  std::vector<int>{});
        cd.motions[i].right  = mo[i].value("right", std::vector<int>{});
        cd.motions[i].up     = mo[i].value("up",    std::vector<int>{});
        cd.motions[i].fps = mo[i].value("fps", 8);
        cd.motions[i].loop = mo[i].value("loop", i == MO_Walk);
    }
    for (const auto& s : c.value("skills", json::array())) cd.skills.push_back(skillFromJson(s));
    cd.expReward = c.value("expReward", 10); cd.goldReward = c.value("goldReward", 5);
    cd.dropItemId = c.value("dropItemId", -1); cd.dropRate = c.value("dropRate", 0);
    cd.spawnFreezeSecs = c.value("spawnFreezeSecs", 1.2f); cd.respawnSecs = c.value("respawnSecs", 0);
    return cd;
}

// ---- serialization ----
json Database::toJson() const {
    json j;
    j["items"] = json::array();
    for (const auto& i : items)
        j["items"].push_back({{"id", i.id}, {"name", i.name}, {"description", i.description},
            {"price", i.price}, {"iconAsset", i.iconAsset}, {"effect", effName(i.effect)},
            {"power", i.power}, {"consumable", i.consumable},
            {"kind", i.kind}, {"satiety", i.satiety}, {"hydration", i.hydration},
            {"healHp", i.healHp}, {"healGp", i.healGp}, {"bodySlot", i.bodySlot},
            {"bonusAtk", i.bonusAtk}, {"bonusDef", i.bonusDef}, {"bonusSpd", i.bonusSpd},
            {"buffSecs", i.buffSecs}});

    j["actors"] = json::array();
    for (const auto& a : actors)
        j["actors"].push_back({{"id", a.id}, {"name", a.name}, {"spriteAsset", a.spriteAsset},
            {"maxHp", a.maxHp}, {"maxMp", a.maxMp}, {"atk", a.atk}, {"def", a.def}, {"spd", a.spd}});

    j["enemies"] = json::array();
    for (const auto& e : enemies)
        j["enemies"].push_back({{"id", e.id}, {"name", e.name}, {"spriteAsset", e.spriteAsset},
            {"maxHp", e.maxHp}, {"atk", e.atk}, {"def", e.def},
            {"spd", e.spd}, {"expReward", e.expReward}, {"goldReward", e.goldReward},
            {"dropItemId", e.dropItemId}, {"dropRate", e.dropRate}});

    j["fieldSkills"] = json::array();
    for (const auto& s : fieldSkills) j["fieldSkills"].push_back(skillToJson(s));

    j["characters"] = json::array();
    for (const auto& c : characters) j["characters"].push_back(charToJson(c));
    j["mobs"] = json::array();
    for (const auto& c : mobs) j["mobs"].push_back(charToJson(c));
    j["dialogues"] = json::array();
    for (const auto& d : dialogues) j["dialogues"].push_back(dialogueToJson(d));
    j["scenes"] = json::array();
    for (const auto& s : scenes) j["scenes"].push_back(sceneToJson(s));
    j["sceneGroups"] = sceneGroups;
    return j;
}

void Database::fromJson(const json& j) {
    items.clear(); actors.clear(); enemies.clear();
    fieldSkills.clear();

    for (const auto& i : j.value("items", json::array())) {
        Item it;
        it.id = i.value("id", -1); it.name = i.value("name", "Item");
        it.description = i.value("description", ""); it.price = i.value("price", 0);
        it.iconAsset = i.value("iconAsset", -1); it.effect = effFrom(i.value("effect", "none"));
        it.power = i.value("power", 0); it.consumable = i.value("consumable", true);
        it.kind = i.value("kind", 0); it.satiety = i.value("satiety", 0); it.hydration = i.value("hydration", 0);
        it.healHp = i.value("healHp", 0); it.healGp = i.value("healGp", 0); it.bodySlot = i.value("bodySlot", 0);
        it.bonusAtk = i.value("bonusAtk", 0); it.bonusDef = i.value("bonusDef", 0); it.bonusSpd = i.value("bonusSpd", 0);
        it.buffSecs = i.value("buffSecs", 0);
        items.push_back(it);
    }
    for (const auto& a : j.value("actors", json::array())) {
        ActorDef ac;
        ac.id = a.value("id", -1); ac.name = a.value("name", "Hero");
        ac.spriteAsset = a.value("spriteAsset", -1);
        ac.maxHp = a.value("maxHp", 100); ac.maxMp = a.value("maxMp", 20);
        ac.atk = a.value("atk", 10); ac.def = a.value("def", 5); ac.spd = a.value("spd", 5);
        actors.push_back(ac);
    }
    for (const auto& e : j.value("enemies", json::array())) {
        EnemyDef en;
        en.id = e.value("id", -1); en.name = e.value("name", "Slime");
        en.spriteAsset = e.value("spriteAsset", -1);
        en.maxHp = e.value("maxHp", 30);
        en.atk = e.value("atk", 8); en.def = e.value("def", 3); en.spd = e.value("spd", 4);
        en.expReward = e.value("expReward", 10); en.goldReward = e.value("goldReward", 5);
        en.dropItemId = e.value("dropItemId", -1); en.dropRate = e.value("dropRate", 0);
        enemies.push_back(en);
    }
    for (const auto& s : j.value("fieldSkills", json::array())) fieldSkills.push_back(skillFromJson(s));
    characters.clear();
    for (const auto& c : j.value("characters", json::array())) characters.push_back(charFromJson(c));
    mobs.clear();
    for (const auto& c : j.value("mobs", json::array())) mobs.push_back(charFromJson(c));
    dialogues.clear();
    for (const auto& d : j.value("dialogues", json::array())) dialogues.push_back(dialogueFromJson(d));
    scenes.clear();
    for (const auto& s : j.value("scenes", json::array())) scenes.push_back(sceneFromJson(s));
    sceneGroups = j.value("sceneGroups", std::vector<std::string>{});
}

} // namespace tsukuru
