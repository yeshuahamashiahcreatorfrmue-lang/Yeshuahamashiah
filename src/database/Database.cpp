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
            {"effectDist", s.effectDist}, {"soundAsset", s.soundAsset}};
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
const Equipment* Database::equip(int id) const {
    for (const auto& e : equipment) if (e.id == id) return &e;
    return nullptr;
}
const Skill* Database::skill(int id) const {
    for (const auto& s : skills) if (s.id == id) return &s;
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

// ---- serialization ----
json Database::toJson() const {
    json j;
    j["items"] = json::array();
    for (const auto& i : items)
        j["items"].push_back({{"id", i.id}, {"name", i.name}, {"description", i.description},
            {"price", i.price}, {"iconAsset", i.iconAsset}, {"effect", effName(i.effect)},
            {"power", i.power}, {"consumable", i.consumable}});

    j["equipment"] = json::array();
    for (const auto& e : equipment)
        j["equipment"].push_back({{"id", e.id}, {"name", e.name},
            {"slot", e.slot == EquipSlot::Weapon ? "weapon" : "armor"},
            {"price", e.price}, {"iconAsset", e.iconAsset}, {"atk", e.atk}, {"def", e.def}});

    j["skills"] = json::array();
    for (const auto& s : skills)
        j["skills"].push_back({{"id", s.id}, {"name", s.name}, {"mpCost", s.mpCost},
            {"power", s.power}, {"healing", s.healing}});

    j["actors"] = json::array();
    for (const auto& a : actors)
        j["actors"].push_back({{"id", a.id}, {"name", a.name}, {"spriteAsset", a.spriteAsset},
            {"maxHp", a.maxHp}, {"maxMp", a.maxMp}, {"atk", a.atk}, {"def", a.def},
            {"spd", a.spd}, {"skills", a.skills}});

    j["enemies"] = json::array();
    for (const auto& e : enemies)
        j["enemies"].push_back({{"id", e.id}, {"name", e.name}, {"spriteAsset", e.spriteAsset},
            {"maxHp", e.maxHp}, {"maxMp", e.maxMp}, {"atk", e.atk}, {"def", e.def},
            {"spd", e.spd}, {"expReward", e.expReward}, {"goldReward", e.goldReward}});

    j["fieldSkills"] = json::array();
    for (const auto& s : fieldSkills) j["fieldSkills"].push_back(skillToJson(s));

    j["characters"] = json::array();
    for (const auto& c : characters) {
        json mo = json::array();
        for (const auto& m : c.motions) mo.push_back({{"frames", m.frames},
            {"left", m.left}, {"right", m.right}, {"up", m.up}, {"fps", m.fps}, {"loop", m.loop}});
        json sk = json::array();
        for (const auto& s : c.skills) sk.push_back(skillToJson(s));
        j["characters"].push_back({{"id", c.id}, {"name", c.name},
            {"maxHp", c.maxHp}, {"maxGp", c.maxGp}, {"atk", c.atk}, {"def", c.def}, {"spd", c.spd},
            {"drawPct", c.drawPct}, {"motions", mo}, {"skills", sk}});
    }
    return j;
}

void Database::fromJson(const json& j) {
    items.clear(); equipment.clear(); skills.clear(); actors.clear(); enemies.clear();
    fieldSkills.clear();

    for (const auto& i : j.value("items", json::array())) {
        Item it;
        it.id = i.value("id", -1); it.name = i.value("name", "Item");
        it.description = i.value("description", ""); it.price = i.value("price", 0);
        it.iconAsset = i.value("iconAsset", -1); it.effect = effFrom(i.value("effect", "none"));
        it.power = i.value("power", 0); it.consumable = i.value("consumable", true);
        items.push_back(it);
    }
    for (const auto& e : j.value("equipment", json::array())) {
        Equipment eq;
        eq.id = e.value("id", -1); eq.name = e.value("name", "Gear");
        eq.slot = e.value("slot", "weapon") == "armor" ? EquipSlot::Armor : EquipSlot::Weapon;
        eq.price = e.value("price", 0); eq.iconAsset = e.value("iconAsset", -1);
        eq.atk = e.value("atk", 0); eq.def = e.value("def", 0);
        equipment.push_back(eq);
    }
    for (const auto& s : j.value("skills", json::array())) {
        Skill sk;
        sk.id = s.value("id", -1); sk.name = s.value("name", "Skill");
        sk.mpCost = s.value("mpCost", 0); sk.power = s.value("power", 0);
        sk.healing = s.value("healing", false);
        skills.push_back(sk);
    }
    for (const auto& a : j.value("actors", json::array())) {
        ActorDef ac;
        ac.id = a.value("id", -1); ac.name = a.value("name", "Hero");
        ac.spriteAsset = a.value("spriteAsset", -1);
        ac.maxHp = a.value("maxHp", 100); ac.maxMp = a.value("maxMp", 20);
        ac.atk = a.value("atk", 10); ac.def = a.value("def", 5); ac.spd = a.value("spd", 5);
        ac.skills = a.value("skills", std::vector<int>{});
        actors.push_back(ac);
    }
    for (const auto& e : j.value("enemies", json::array())) {
        EnemyDef en;
        en.id = e.value("id", -1); en.name = e.value("name", "Slime");
        en.spriteAsset = e.value("spriteAsset", -1);
        en.maxHp = e.value("maxHp", 30); en.maxMp = e.value("maxMp", 0);
        en.atk = e.value("atk", 8); en.def = e.value("def", 3); en.spd = e.value("spd", 4);
        en.expReward = e.value("expReward", 10); en.goldReward = e.value("goldReward", 5);
        enemies.push_back(en);
    }
    for (const auto& s : j.value("fieldSkills", json::array())) fieldSkills.push_back(skillFromJson(s));
    characters.clear();
    for (const auto& c : j.value("characters", json::array())) {
        CharacterDef cd;
        cd.id = c.value("id", -1); cd.name = c.value("name", "캐릭터");
        cd.maxHp = c.value("maxHp", 100); cd.maxGp = c.value("maxGp", 30);
        cd.atk = c.value("atk", 12); cd.def = c.value("def", 5); cd.spd = c.value("spd", 5);
        cd.drawPct = c.value("drawPct", 125);
        const auto& mo = c.value("motions", json::array());
        for (int i = 0; i < MO_COUNT && i < (int)mo.size(); ++i) {
            cd.motions[i].frames = mo[i].value("frames", std::vector<int>{});
            cd.motions[i].left   = mo[i].value("left",  std::vector<int>{});
            cd.motions[i].right  = mo[i].value("right", std::vector<int>{});
            cd.motions[i].up     = mo[i].value("up",    std::vector<int>{});
            cd.motions[i].fps = mo[i].value("fps", 8);
            cd.motions[i].loop = mo[i].value("loop", i == MO_Walk); // walk loops by default
        }
        for (const auto& s : c.value("skills", json::array())) cd.skills.push_back(skillFromJson(s));
        characters.push_back(cd);
    }
}

} // namespace tsukuru
