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
    gs.newGame(db, 1, 1, 5, 5);
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
    gs.newGame(db, 1, 1, 0, 0);
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
        CHECK(lm->events.size() == 1 && lm->events[0].text == "Hello!", "events persisted");
    }
    fs::remove_all(tmp, ec);
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

    std::printf("=========================================\n");
    if (g_failures == 0) { std::printf("ALL TESTS PASSED\n"); return 0; }
    std::printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
