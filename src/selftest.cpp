// Headless self-test for the Tsukuru engine core (no graphics required).
// Verifies serialization round-trips and core game logic. Exits 0 on success.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <filesystem>

#include "project/Project.h"
#include "world/Tilemap.h"
#include "database/Database.h"
#include "game/GameState.h"
#include "game/Inventory.h"
#include "battle/Battle.h"
#include "net/Net.h"
#include <memory>

namespace fs = std::filesystem;
using namespace tsukuru;

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { std::printf("  [PASS] %s\n", msg); } \
    else { std::printf("  [FAIL] %s\n", msg); ++g_failures; } } while (0)

static void testTilemap() {
    std::printf("== Tilemap ==\n");
    Tilemap tm(10, 8);
    tm.setTile(0, 3, 4, 12);
    tm.setBlocked(3, 4, true);
    tm.fill(1, 0, 0, 7); // bucket-fill layer 1
    CHECK(tm.tile(0, 3, 4) == 12, "set/get tile");
    CHECK(tm.blocked(3, 4), "set/get collision");
    CHECK(tm.tile(1, 9, 7) == 7, "flood fill reaches far corner");
    CHECK(tm.blocked(-1, 0), "out of bounds blocks");

    Tilemap tm2;
    tm2.fromJson(tm.toJson());
    CHECK(tm2.tile(0, 3, 4) == 12 && tm2.blocked(3, 4) && tm2.tile(1, 9, 7) == 7,
          "tilemap json round-trip");
}

static void testDatabase() {
    std::printf("== Database ==\n");
    Database db;
    db.items.push_back({1, "Potion", "Heals 50 HP", 50, -1, ItemEffect::HealHP, 50, true});
    db.equipment.push_back({1, "Sword", EquipSlot::Weapon, 100, -1, 8, 0});
    db.equipment.push_back({2, "Shield", EquipSlot::Armor, 80, -1, 0, 6});
    db.skills.push_back({1, "Fireball", 5, 20, false});
    db.actors.push_back({1, "Hero", -1, 120, 30, 14, 7, 6, {1}});
    db.enemies.push_back({1, "Slime", -1, 30, 0, 8, 3, 4, 12, 8});

    Database db2;
    db2.fromJson(db.toJson());
    CHECK(db2.item(1) && db2.item(1)->power == 50, "item round-trip");
    CHECK(db2.equip(1) && db2.equip(1)->atk == 8, "weapon round-trip");
    CHECK(db2.skill(1) && db2.skill(1)->power == 20, "skill round-trip");
    CHECK(db2.actor(1) && db2.actor(1)->maxHp == 120, "actor round-trip");
    CHECK(db2.enemy(1) && db2.enemy(1)->goldReward == 8, "enemy round-trip");
}

static void testInventory(Database& db) {
    std::printf("== Inventory & Equipment ==\n");
    Inventory inv;
    inv.gold = 100;
    inv.addItem(1, 3);
    CHECK(inv.count(1) == 3, "add item");
    CHECK(inv.removeItem(1, 2) && inv.count(1) == 1, "remove item");
    CHECK(!inv.removeItem(1, 5), "cannot over-remove");

    PartyMember m = PartyMember::fromActor(*db.actor(1));
    int baseAtk = m.totalAtk(db);
    m.weaponId = 1; // Sword +8
    CHECK(m.totalAtk(db) == baseAtk + 8, "equipment raises attack");
    m.armorId = 2;  // Shield +6
    CHECK(m.totalDef(db) == m.def + 6, "equipment raises defense");

    Inventory inv2;
    inv2.fromJson(inv.toJson());
    CHECK(inv2.gold == 100 && inv2.count(1) == 1, "inventory round-trip");
}

static void testGameStateAndSave(Database& db) {
    std::printf("== GameState / Switches / Save ==\n");
    GameState gs;
    gs.newGame(db, 1, -1, 1, 5, 5);
    gs.setSwitch(10, true);
    gs.setVar(3, 42);
    gs.inventory.addItem(1, 2);
    CHECK(gs.party.size() == 1, "new game creates party");
    CHECK(gs.getSwitch(10) && gs.getVar(3) == 42, "switch & variable set/get");

    GameState gs2;
    gs2.fromJson(gs.toJson());
    CHECK(gs2.getSwitch(10) && gs2.getVar(3) == 42, "gamestate switch/var round-trip");
    CHECK(gs2.party.size() == 1 && gs2.inventory.count(1) == 2, "gamestate party/inv round-trip");

    // leveling
    PartyMember& pm = gs.party[0];
    int lv = pm.level;
    pm.gainExp(1000);
    CHECK(pm.level > lv, "exp gain levels up");
}

static void testBattle(Database& db) {
    std::printf("== Battle ==\n");
    std::srand(12345);
    GameState gs;
    gs.newGame(db, 1, -1, 1, 0, 0);
    gs.inventory.addItem(1, 1); // a potion
    Battle b(db, gs, {1, 1}); // two slimes
    CHECK(b.result() == BattleResult::Ongoing, "battle starts ongoing");

    int guard = 0;
    while (b.result() == BattleResult::Ongoing && guard++ < 100) {
        BattleAction a;
        a.kind = ActionKind::Attack;
        a.targetIndex = b.firstAliveEnemy();
        b.submit(a);
    }
    CHECK(b.result() == BattleResult::Victory || b.result() == BattleResult::Defeat,
          "battle reaches a terminal state");
    CHECK(guard < 100, "battle terminates (no infinite loop)");
    if (b.result() == BattleResult::Victory)
        CHECK(gs.inventory.gold > 0, "victory grants gold");
}

static void testProjectIO() {
    std::printf("== Project save/load ==\n");
    std::string tmp = (fs::temp_directory_path() / "tsukuru_selftest_proj").string();
    std::error_code ec; fs::remove_all(tmp, ec);

    auto p = Project::createNew(tmp, "TestGame");
    p->database.items.push_back({1, "Potion", "", 50, -1, ItemEffect::HealHP, 50, true});
    p->database.actors.push_back({1, "Hero", -1, 100, 20, 10, 5, 5, {}});
    auto m = p->maps.front();
    m->name = "Town";
    m->tilemap.setTile(0, 2, 2, 5);
    m->tilemap.setBlocked(2, 2, true);
    Event ev; ev.id = 1; ev.x = 4; ev.y = 4; ev.type = EventType::Message; ev.text = "Hello!";
    m->events.push_back(ev);
    Event npc; npc.id = 2; npc.x = 6; npc.y = 6; npc.graphicAsset = 0;
    npc.faction = NpcFaction::Enemy; npc.behavior = NpcBehavior::Chase;
    npc.drawPct = 200; npc.npcHp = 80; npc.npcAtk = 14; npc.npcDef = 3;
    m->events.push_back(npc);
    p->startActor = 1;
    CHECK(p->save(), "project saves to disk");

    Project p2;
    CHECK(p2.load(tmp), "project loads from disk");
    CHECK(p2.name == "TestGame", "project name persisted");
    CHECK(p2.database.item(1) && p2.database.item(1)->power == 50, "database persisted in project");
    auto lm = p2.map(m->id);
    CHECK(lm != nullptr, "map loaded");
    if (lm) {
        CHECK(lm->name == "Town", "map name persisted");
        CHECK(lm->tilemap.tile(0, 2, 2) == 5 && lm->tilemap.blocked(2, 2), "map tiles persisted");
        CHECK(lm->events.size() == 2 && lm->events[0].text == "Hello!", "events persisted");
        const Event* en = nullptr;
        for (auto& e : lm->events) if (e.id == 2) en = &e;
        CHECK(en && en->faction == NpcFaction::Enemy && en->behavior == NpcBehavior::Chase,
              "NPC faction/behavior persisted");
        CHECK(en && en->drawPct == 200 && en->npcHp == 80 && en->npcAtk == 14 && en->npcDef == 3,
              "NPC draw size & combat stats persisted");
    }
    fs::remove_all(tmp, ec);
}

// Mirrors the character-creation panel's full workflow with NO graphics:
// register new image files -> create a new character -> build all six motions
// (walk/attack/skill1/skill2/ultimate/death) from those images -> set it as the
// driving player -> save -> reload, and verify every step persisted.
static void testCharacterBuilder() {
    std::printf("== Character builder (register images -> 6 motions -> new character) ==\n");
    std::string tmp = (fs::temp_directory_path() / "tsukuru_charbuilder").string();
    std::error_code ec; fs::remove_all(tmp, ec);
    auto p = Project::createNew(tmp, "CharTest");

    // 1) "내 이미지 불러오기": register NEW image files into the project.
    fs::path srcdir = fs::path(tmp) / "_incoming"; fs::create_directories(srcdir, ec);
    std::vector<int> imgIds;
    for (int i = 0; i < MO_COUNT * 2; ++i) {
        fs::path f = srcdir / ("frame_" + std::to_string(i) + ".png");
        if (FILE* fp = std::fopen(f.string().c_str(), "wb")) { std::fputs("PNGDUMMY", fp); std::fclose(fp); }
        imgIds.push_back(p->assets.registerAsset(p->dir, f.string(), AssetType::Image));
    }
    bool allReg = imgIds.size() == (size_t)MO_COUNT * 2;
    for (int id : imgIds) if (id < 0 || !p->assets.find(id)) allReg = false;
    CHECK(allReg, "register new images into the project (12 frames)");

    // 2) "+새 캐릭터" + fill ALL six motion tabs from the registered images.
    CharacterDef cd; cd.id = 1; cd.name = "테스트영웅";
    cd.maxHp = 250; cd.maxGp = 80; cd.atk = 40; cd.def = 12; cd.spd = 9;   // 캐릭터 데이터
    cd.drawPct = 175;                                                       // 미세 175%
    cd.drawTilesW = 2; cd.drawTilesH = 3;                                   // 차지 칸수 2×3
    for (int m = 0; m < MO_COUNT; ++m) {
        cd.motions[m].frames = { imgIds[m*2], imgIds[m*2 + 1] };   // = clicking 2 library images
        cd.motions[m].fps    = 6 + m;
        cd.motions[m].loop   = (m == MO_Walk);
    }
    // 4-directional walk: dedicated left + up frames (right/down fall back to 정면)
    cd.motions[MO_Walk].left = { imgIds[8], imgIds[9] };
    cd.motions[MO_Walk].up   = { imgIds[10] };
    // the character's OWN skill (range/power/effect/sound), authored in the panel
    FieldSkill cs; cs.slot = 1; cs.name = "캐릭터파이어"; cs.projectile = true;
    cs.range = 9; cs.powerPct = 250; cs.mpCost = 7; cs.cooldown = 1.2f;
    cs.patX = {0, 0}; cs.patY = {0, -1};
    cs.effectAsset = imgIds[0]; cs.effectLoops = 7;   // 7프레임 이펙트를 7회 반복
    cs.effectDist = 3;                                 // 정면 3칸 앞으로 밀기
    cs.efxX = {0, 1, -1}; cs.efxY = {-1, -1, -1};      // 이펙트 범위(데미지와 별도)
    cs.effectMode = 1; cs.effectScale = 250;          // 한 곳에 크게, 2.5칸 크기
    cd.skills.push_back(cs);
    // extra F/G slots (4·5) — editable per-character like Z/X/C/V
    FieldSkill fsk; fsk.slot = 4; fsk.name = "F스킬"; fsk.powerPct = 140; cd.skills.push_back(fsk);
    FieldSkill gsk; gsk.slot = 5; gsk.name = "G스킬"; gsk.powerPct = 160; cd.skills.push_back(gsk);
    p->database.characters.push_back(cd);
    p->playerCharId = cd.id;                                       // "플레이어로 설정"
    CHECK(p->save(), "save new character + registered images");

    // 3) reload and verify the whole chain survived a round-trip.
    Project p2; CHECK(p2.load(tmp), "reload project");
    const CharacterDef* c = p2.database.character(1);
    CHECK(c != nullptr, "new custom character persisted");
    bool motionsOk = (c != nullptr);
    if (c) for (int m = 0; m < MO_COUNT; ++m)
        if (c->motions[m].frames.size() != 2 || c->motions[m].fps != 6 + m) motionsOk = false;
    CHECK(motionsOk, "all 6 motions (걷기/공격/스킬1/스킬2/궁극기/죽음) kept their image frames");
    CHECK(c && c->motions[MO_Walk].loop && !c->motions[MO_Attack].loop, "per-motion loop flags persisted");
    CHECK(c && c->motions[MO_Walk].left.size() == 2 && c->motions[MO_Walk].up.size() == 1,
          "4-directional walk frames (left/up) persisted");
    CHECK(c && c->motions[MO_Walk].dirFrames(1).size() == 2 && c->motions[MO_Walk].dirFrames(3).size() == 1
            && c->motions[MO_Walk].dirFrames(2).size() == 2,   // right empty -> falls back to 아래
          "directional lookup + empty-direction fallback to 아래");
    CHECK(p2.playerCharId == 1, "character is set as the driving player");
    CHECK(c && c->drawPct == 175, "character draw size (칸) persisted");
    bool skillOk = c && c->skills.size() == 3 && c->skills[0].slot == 1 &&
                   c->skills[0].powerPct == 250 && c->skills[0].range == 9 &&
                   c->skills[0].projectile && c->skills[0].name == "캐릭터파이어";
    CHECK(skillOk, "character's own skill (range/power/projectile) persisted");
    CHECK(c && c->skills[0].effectLoops == 7 && c->skills[0].effectAsset == imgIds[0],
          "skill effect image + 반복(회) count persisted");
    CHECK(c && c->skills[0].effectDist == 3, "skill effect distance(칸) persisted");
    CHECK(c && c->skills[0].efxX.size() == 3 && c->skills[0].effectMode == 1 && c->skills[0].effectScale == 250,
          "skill effect placement(범위/모드/크기) persisted");
    // F·G slots (4·5) authored per-character round-trip
    const FieldSkill* fS = nullptr; const FieldSkill* gS = nullptr;
    if (c) for (const auto& sk : c->skills) { if (sk.slot == 4) fS = &sk; if (sk.slot == 5) gS = &sk; }
    CHECK(fS && fS->powerPct == 140 && gS && gS->powerPct == 160,
          "캐릭터 F·G(slot 4·5) 스킬 저장/복원");
    bool framesResolve = (c != nullptr);
    if (c) for (int m = 0; m < MO_COUNT; ++m) for (int fid : c->motions[m].frames)
        if (!p2.assets.find(fid)) framesResolve = false;
    CHECK(framesResolve, "every motion frame resolves to a registered image asset");

    // character battle data (체력/기력/공격력/방어력/속도) round-trips
    CHECK(c && c->maxHp == 250 && c->maxGp == 80 && c->atk == 40 && c->def == 12 && c->spd == 9,
          "character stats (체력/기력/공격력/방어력/속도) persisted");
    CHECK(c && c->drawTilesW == 2 && c->drawTilesH == 3 && c->drawPct == 175,
          "character tile footprint (차지 칸수 2×3 + 미세%) persisted");
    // and those stats drive the live party + skill damage uses 공격력 before the multiplier
    GameState gs; gs.newGame(p2.database, 1, p2.playerCharId, 1, 0, 0);
    bool statApplied = !gs.party.empty() && gs.party[0].maxHp == 250 && gs.party[0].maxMp == 80
                       && gs.party[0].atk == 40;
    CHECK(statApplied, "player character data applied to the live party (기력=maxMp)");
    if (c && c->skills.size() == 1) {
        int dmg = std::max(1, gs.party[0].totalAtk(p2.database) * c->skills[0].powerPct / 100);
        CHECK(dmg == 40 * 250 / 100, "skill damage = 공격력 × 위력배수 (배수 이전 공격력 공통 적용)");
    }

    fs::remove_all(tmp, ec);
}

// Count regular files in a directory (0 if it doesn't exist).
static int countFiles(const fs::path& dir) {
    std::error_code ec; int n = 0;
    if (!fs::exists(dir, ec)) return 0;
    for (auto& e : fs::directory_iterator(dir, ec)) if (e.is_regular_file()) ++n;
    return n;
}
// Write a tiny dummy file (stand-in for an imported image; no graphics needed).
static void writeDummy(const fs::path& path, const std::string& tag) {
    std::error_code ec; fs::create_directories(path.parent_path(), ec);
    FILE* f = std::fopen(path.string().c_str(), "wb");
    if (f) { std::fputs(tag.c_str(), f); std::fclose(f); }
}

// Add/remove stress test: repeatedly register & delete maps and assets and verify
// NOTHING accumulates — no leftover map .json, no orphan asset files, no dangling
// references. This guards the "잔여물 남지 않게" (residue-free) requirement.
static void testResidueStress() {
    std::printf("== Residue / add-remove stress (잔여물 검사) ==\n");
    std::string tmp = (fs::temp_directory_path() / "tsukuru_residue").string();
    std::error_code ec; fs::remove_all(tmp, ec);
    auto p = Project::createNew(tmp, "ResidueTest");
    p->save();

    fs::path mapsDir   = fs::path(tmp) / "maps";
    fs::path assetsDir = fs::path(tmp) / "assets";
    fs::path incoming  = fs::path(tmp) / "_incoming";

    // --- 1. map add/delete cycles leave no orphan map files ---
    size_t baseMaps  = p->maps.size();
    int    baseMapF  = (p->save(), countFiles(mapsDir));
    for (int i = 0; i < 200; ++i) {
        auto m = p->addMap("Stress" + std::to_string(i), 16, 16);
        int id = m->id; p->save();
        p->deleteMap(id); p->save();
    }
    CHECK(p->maps.size() == baseMaps, "맵 200회 추가/삭제 후 맵 수 원상복구");
    CHECK(countFiles(mapsDir) == baseMapF, "맵 추가/삭제 후 잔여 .json 없음");

    // --- 2. asset register/delete cycles leave no orphan files ---
    int baseAssets = (int)p->assets.all().size();
    int baseAssetF = countFiles(assetsDir);
    for (int i = 0; i < 200; ++i) {
        fs::path src = incoming / ("img" + std::to_string(i) + ".png");
        writeDummy(src, "PNGDUMMY");
        int id = p->assets.registerAsset(p->dir, src.string(), AssetType::Image, "img");
        p->deleteAssets({ id });
    }
    CHECK((int)p->assets.all().size() == baseAssets, "에셋 200회 등록/삭제 후 에셋 수 원상복구");
    CHECK(countFiles(assetsDir) == baseAssetF, "에셋 등록/삭제 후 잔여 파일 없음");

    // --- 3. deleting an asset scrubs EVERY reference (no dangling ids) ---
    fs::path s2 = incoming / "ref.png"; writeDummy(s2, "PNGDUMMY");
    int rid = p->assets.registerAsset(p->dir, s2.string(), AssetType::Image, "ref");
    auto rm = p->addMap("RefMap", 12, 12);
    rm->tileset.assetId = rid; rm->bgmAsset = rid;
    Event rev; rev.id = 1; rev.x = 1; rev.y = 1; rev.graphicAsset = rid; rm->events.push_back(rev);
    p->database.actors.push_back({99, "RefHero", rid, 100, 20, 10, 5, 5, {}});
    CharacterDef rcd; rcd.id = 99; rcd.motions[MO_Walk].frames = { rid }; rcd.skills.push_back({});
    rcd.skills.back().effectAsset = rid; rcd.skills.back().soundAsset = rid;
    p->database.characters.push_back(rcd);
    p->playerSprite = rid;
    p->deleteAssets({ rid });
    bool scrubbed = rm->tileset.assetId == -1 && rm->bgmAsset == -1
                 && rm->events[0].graphicAsset == -1
                 && p->database.actors.back().spriteAsset == -1
                 && p->database.characters.back().motions[MO_Walk].frames.empty()
                 && p->database.characters.back().skills[0].effectAsset == -1
                 && p->database.characters.back().skills[0].soundAsset == -1
                 && p->playerSprite == -1
                 && p->assets.find(rid) == nullptr;
    CHECK(scrubbed, "에셋 삭제 시 모든 참조(타일셋/BGM/NPC/액터/모션/스킬/플레이어) 정리됨");

    // --- 4. viewer copy-paste duplicate + delete leaves no residue ---
    p->deleteMap(rm->id);                  // tidy the ref map first
    size_t beforeDup = p->maps.size();
    int    beforeF   = (p->save(), countFiles(mapsDir));
    auto base = p->maps.front();
    for (int i = 0; i < 100; ++i) {
        auto nm = std::make_shared<Map>(*base);
        nm->id = p->nextMapId(); nm->name = base->name + " (" + std::to_string(i+2) + ")";
        nm->placed = true; nm->viewerCopy = true;
        p->maps.push_back(nm); p->save();
        p->deleteMap(nm->id); p->save();   // right-click delete of a viewer copy
    }
    CHECK(p->maps.size() == beforeDup, "전맵뷰어 복제 100회 추가/삭제 후 맵 수 원상복구");
    CHECK(countFiles(mapsDir) == beforeF, "전맵뷰어 복제 추가/삭제 후 잔여 .json 없음");

    fs::remove_all(tmp, ec);
}

// MMO capacity: a host must accept 42 external clients concurrently, and food/
// hunger data must round-trip. Verifies the "42명 동시 접속" requirement locally
// (42 real loopback TCP connections — NOT 42 separate external Windows machines,
// which can't be spun up in this environment).
static void testNetCapacity() {
    std::printf("== Multiplayer (42-client capacity) ==\n");
    const int port = 7801;
    Net host;
    CHECK(host.startHost(port, 42), "MMO 호스트 시작 (포트 7801, 최대 42)");
    std::vector<std::unique_ptr<Net>> clients;
    int connected = 0;
    for (int i = 0; i < 42; ++i) {
        auto c = std::make_unique<Net>();
        if (c->startClient("127.0.0.1", port)) ++connected;
        clients.push_back(std::move(c));
    }
    CHECK(connected == 42, "외부 클라이언트 42개 소켓 접속 성공");
    NetPlayer lp; lp.mapId = 1; lp.x = 5; lp.y = 5;
    for (int t = 0; t < 80; ++t) {
        host.update(0.05f, lp);
        for (auto& c : clients) { lp.x = (t % 7); c->update(0.05f, lp); }
    }
    CHECK(host.playerCount() >= 43, "호스트가 42명 동시 수용 (호스트 포함 43)");
    int withId = 0; for (auto& c : clients) if (c->myId() > 0) ++withId;
    CHECK(withId == 42, "42개 클라이언트 모두 서버에서 ID 부여받음");
    // zone sync: everyone is in map 1, so each side sees the others there
    CHECK((int)host.remotesInMap(1).size() == 42, "호스트가 같은 존(맵1)의 42명 인식");
    CHECK(clients[0]->remotesInMap(1).size() >= 1, "클라이언트가 같은 존의 다른 플레이어 인식");
    for (auto& c : clients) c->stop();
    host.stop();
}

static void testFoodAndSurvival() {
    std::printf("== Survival / food item round-trip ==\n");
    Database db;
    Item food; food.id = 7; food.name = "물병"; food.kind = 1;
    food.satiety = 500; food.hydration = 9000; food.healHp = 20; food.bonusSpd = 1;
    db.items.push_back(food);
    Item gear; gear.id = 8; gear.name = "강철투구"; gear.kind = 2; gear.bodySlot = 1; gear.bonusDef = 12;
    db.items.push_back(gear);
    Database d2; d2.fromJson(db.toJson());
    const Item* f = d2.item(7); const Item* g = d2.item(8);
    CHECK(f && f->kind == 1 && f->satiety == 500 && f->hydration == 9000 && f->healHp == 20,
          "식품 아이템(포만/수분/HP 회복) 직렬화");
    CHECK(g && g->kind == 2 && g->bodySlot == 1 && g->bonusDef == 12, "장비 아이템(부위/방어보너스) 직렬화");
    PartyMember m; m.maxHunger = m.hunger = 42000; m.maxThirst = m.thirst = 42000;
    PartyMember m2 = PartyMember::fromJson(m.toJson());
    CHECK(m2.maxHunger == 42000 && m2.thirst == 42000, "포만/수분(42000) 직렬화");

    // --- shared consume / equip / timed-buff logic (used by both the field
    //     windows and the ESC menu) ---
    Item buffFood; buffFood.id = 9; buffFood.name = "전투식량"; buffFood.kind = 1;
    buffFood.satiety = 6000; buffFood.hydration = 3000; buffFood.bonusAtk = 5; buffFood.buffSecs = 60;
    db.items.push_back(buffFood);
    GameState gs;
    PartyMember pm; pm.actorId = 1; pm.maxHp = 100; pm.hp = 50; pm.atk = 10;
    pm.maxHunger = pm.hunger = 42000; pm.maxThirst = pm.thirst = 42000;
    gs.party.push_back(pm);
    // eat a 식품 with hunger 42000 already full -> no overflow; bonusAtk timed buff applies
    gs.party[0].hunger = 40000; gs.party[0].thirst = 40000;
    gs.inventory.addItem(9, 1);
    gs.consumeFood(db, 9);
    CHECK(gs.party[0].hunger == 42000 && gs.party[0].thirst == 42000, "식품 섭취: 포만/수분 회복(최대 클램프)");
    CHECK(gs.party[0].atk == 15 && gs.buffs.size() == 1, "식품 버프: 공격력 +5 즉시 적용 + 버프 등록");
    CHECK(gs.inventory.count(9) == 0, "식품 섭취 후 인벤토리에서 소비");
    gs.tickBuffs(61.0f);   // expire the 60s buff
    CHECK(gs.party[0].atk == 10 && gs.buffs.empty(), "식품 버프 만료 후 공격력 원복");

    // equip / unequip an item into a body slot
    gs.inventory.addItem(8, 1);            // 강철투구 (bodySlot 1, def +12)
    gs.equipItem(db, 8);
    CHECK(gs.equipped[1] == 8 && gs.party[0].def == 12 && gs.inventory.count(8) == 0,
          "장비 장착: 부위 슬롯 등록 + 방어 +12 + 인벤토리 차감");
    gs.unequipSlot(db, 1);
    CHECK(gs.equipped.count(1) == 0 && gs.party[0].def == 0 && gs.inventory.count(8) == 1,
          "장비 해제: 슬롯 비움 + 스탯 원복 + 인벤토리 반환");

    // buffs survive a save/load round-trip
    gs.buffs.push_back(ActiveBuff{ 3, 0, 0, 30.0f, "테스트버프" });
    GameState gs2; gs2.fromJson(gs.toJson());
    CHECK(gs2.buffs.size() == 1 && gs2.buffs[0].atk == 3 && gs2.buffs[0].name == "테스트버프",
          "활성 버프 세이브/로드 직렬화");
}

int main() {
    std::printf("===== Tsukuru Engine Core Self-Test =====\n");
    testTilemap();
    Database db;
    db.items.push_back({1, "Potion", "Heals 50 HP", 50, -1, ItemEffect::HealHP, 50, true});
    db.equipment.push_back({1, "Sword", EquipSlot::Weapon, 100, -1, 8, 0});
    db.equipment.push_back({2, "Shield", EquipSlot::Armor, 80, -1, 0, 6});
    db.skills.push_back({1, "Fireball", 5, 20, false});
    db.actors.push_back({1, "Hero", -1, 120, 30, 14, 7, 6, {1}});
    db.enemies.push_back({1, "Slime", -1, 30, 0, 8, 3, 4, 12, 8});
    testDatabase();
    testInventory(db);
    testGameStateAndSave(db);
    testBattle(db);
    testProjectIO();
    testCharacterBuilder();
    testResidueStress();
    testFoodAndSurvival();
    testNetCapacity();

    std::printf("=========================================\n");
    if (g_failures == 0) { std::printf("ALL TESTS PASSED\n"); return 0; }
    std::printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
