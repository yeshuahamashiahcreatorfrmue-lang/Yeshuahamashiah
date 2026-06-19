// Generates a larger multi-map sample world (data only, no graphics needed).
// Builds an overworld, a town, house/inn interiors, and a two-floor dungeon,
// all linked by teleport events. Run: gen_world <projectDir>
#include "project/Project.h"
#include "world/Map.h"
#include "database/Database.h"
#include <filesystem>
#include <cstdio>
#include <cstdlib>

namespace fs = std::filesystem;
using namespace tsukuru;

// tile indices in the sample tileset (8 cols x 2 rows)
enum { GRASS=0, PATH=1, WATER=2, TREE=3, BRICK=4, WOOD=5, FLOWER=6, STONE=7,
       DGRASS=8, SAND=9, DWATER=10, ROOF=11 };

static void fill(Map& m, int layer, int x, int y, int w, int h, int tile) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            m.tilemap.setTile(layer, i, j, tile);
}
static void collide(Map& m, int x, int y, int w, int h, bool b = true) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            m.tilemap.setBlocked(i, j, b);
}
static void border(Map& m) {
    int w = m.tilemap.width(), h = m.tilemap.height();
    fill(m, 1, 0, 0, w, 1, TREE);   fill(m, 1, 0, h-1, w, 1, TREE);
    fill(m, 1, 0, 0, 1, h, TREE);   fill(m, 1, w-1, 0, 1, h, TREE);
    collide(m, 0, 0, w, 1); collide(m, 0, h-1, w, 1);
    collide(m, 0, 0, 1, h); collide(m, w-1, 0, 1, h);
}
static Event tele(Map& m, int x, int y, int toMap, int tx, int ty, int marker = STONE) {
    m.tilemap.setTile(1, x, y, marker);
    Event e; e.id = m.nextEventId(); e.x = x; e.y = y;
    e.type = EventType::Teleport; e.trigger = TriggerType::PlayerTouch;
    e.targetMap = toMap; e.targetX = tx; e.targetY = ty;
    m.events.push_back(e); return e;
}
static void npc(Map& m, int x, int y, int graphic, const std::string& text) {
    Event e; e.id = m.nextEventId(); e.x = x; e.y = y;
    e.type = EventType::Message; e.trigger = TriggerType::ActionButton;
    e.graphicAsset = graphic; e.text = text;
    m.events.push_back(e);
}
static void shop(Map& m, int x, int y, int graphic, int itemId, const std::string& text) {
    Event e; e.id = m.nextEventId(); e.x = x; e.y = y;
    e.type = EventType::Shop; e.trigger = TriggerType::ActionButton;
    e.graphicAsset = graphic; e.itemId = itemId; e.text = text;
    m.events.push_back(e);
}
static void chest(Map& m, int x, int y, int itemId, int amount, const std::string& text) {
    Event e; e.id = m.nextEventId(); e.x = x; e.y = y;
    e.type = EventType::GiveItem; e.trigger = TriggerType::ActionButton;
    e.itemId = itemId; e.amount = amount; e.text = text; e.once = true;
    e.graphicAsset = -1;
    m.events.push_back(e);
}
static void building(Map& m, int x, int y, int w, int h) {
    fill(m, 1, x, y, w, h, BRICK);
    fill(m, 1, x, y, w, 2, ROOF);
    collide(m, x, y, w, h);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "projects/sample_rpg";
    fs::create_directories(fs::path(dir) / "maps");
    fs::create_directories(fs::path(dir) / "assets");

    Project p;
    p.dir = dir;
    p.name = "Realm of Tsukuru";

    // ---- assets (PNGs are produced separately by gen_assets) ----
    int tileset = p.assets.addExisting(AssetType::Image, "tileset", "assets/tileset.png");
    int hero    = p.assets.addExisting(AssetType::Image, "hero",    "assets/hero.png");
    int npcA    = p.assets.addExisting(AssetType::Image, "npc",     "assets/npc.png");
    int oldman  = p.assets.addExisting(AssetType::Image, "oldman",  "assets/oldman.png");
    int slimeS  = p.assets.addExisting(AssetType::Image, "slime",   "assets/slime.png");
    int batS    = p.assets.addExisting(AssetType::Image, "bat",     "assets/bat.png");
    p.playerSprite = hero;
    p.startActor = 1;

    // ---- database ----
    Database& db = p.database;
    db.items = {
        {1,"Potion","Restores 50 HP.",30,-1,ItemEffect::HealHP,50,true},
        {2,"Hi-Potion","Restores 150 HP.",100,-1,ItemEffect::HealHP,150,true},
        {3,"Ether","Restores 30 MP.",80,-1,ItemEffect::HealMP,30,true},
    };
    db.equipment = {
        {1,"Bronze Sword",EquipSlot::Weapon,50,-1,8,0},
        {2,"Iron Sword",EquipSlot::Weapon,200,-1,18,0},
        {3,"Leather Armor",EquipSlot::Armor,60,-1,0,6},
        {4,"Chain Mail",EquipSlot::Armor,220,-1,0,14},
    };
    db.skills = {
        {1,"Fireball",5,20,false},
        {2,"Heal",8,40,true},
    };
    db.actors = { {1,"Hero",hero,120,30,14,7,6,{1,2}} };
    db.enemies = {
        {1,"Slime", slimeS,30,0,8,3,4,8,6},
        {2,"Bat",   batS, 22,0,10,2,6,10,8},
        {3,"Orc",   slimeS,70,0,18,9,5,35,28},
        {4,"Dragon",batS, 240,0,34,16,7,400,500},
    };

    auto tset = [&](Map& m){ m.tileset.assetId = tileset; m.tileset.tileWidth=32;
        m.tileset.tileHeight=32; m.tileset.columns=8; m.tileset.rows=2; };

    // ============ Map 1: Overworld (50x38) ============
    auto over = p.addMap("Overworld", 50, 38); tset(*over);
    fill(*over, 0, 0, 0, 50, 38, GRASS);
    border(*over);
    fill(*over, 0, 24, 1, 2, 36, PATH);     // vertical road
    fill(*over, 0, 1, 18, 48, 2, PATH);     // horizontal road
    fill(*over, 0, 6, 27, 9, 7, WATER);  collide(*over, 6, 27, 9, 7);   // lake
    for (int i = 0; i < 18; ++i) {          // scattered trees
        int tx = 3 + (i*7) % 44, ty = 3 + (i*5) % 32;
        if (tx >= 23 && tx <= 26) continue;
        over->tilemap.setTile(1, tx, ty, TREE); over->tilemap.setBlocked(tx, ty, true);
    }
    over->encounterEnemies = {1,2}; over->encounterRate = 0;
    tele(*over, 24, 3, 2, 17, 23);          // -> Town
    tele(*over, 24, 34, 5, 4, 2);           // -> Dungeon B1
    npc(*over, 22, 17, oldman, "Welcome, traveler! The town lies north, danger lies south.");
    p.startMap = over->id; p.startX = 24; p.startY = 20;

    // ============ Map 2: Town (34x26) ============
    auto town = p.addMap("Town", 34, 26); tset(*town);
    fill(*town, 0, 0, 0, 34, 26, GRASS);
    fill(*town, 0, 6, 6, 22, 14, STONE);    // plaza
    border(*town);
    building(*town, 4, 3, 6, 5);            // house
    building(*town, 24, 3, 6, 5);           // inn
    building(*town, 14, 16, 7, 6);          // shop bldg
    tele(*town, 17, 24, 1, 24, 5);          // -> Overworld (south gate)
    tele(*town, 6, 7, 3, 9, 12);            // house door -> House
    tele(*town, 26, 7, 4, 11, 13);          // inn door -> Inn
    npc(*town, 12, 10, npcA, "This is the town square. Visit the shop for potions!");
    npc(*town, 20, 12, oldman, "Rumor says a Dragon sleeps deep in the southern cave...");
    shop(*town, 17, 15, npcA, 1, "Shopkeeper: Buy a Potion for 30 gold?");
    shop(*town, 18, 15, npcA, 2, "Shopkeeper: Hi-Potion for 100 gold?");

    // ============ Map 3: House (18x14) ============
    auto house = p.addMap("House", 18, 14); tset(*house);
    fill(*house, 0, 0, 0, 18, 14, WOOD);
    border(*house);
    npc(*house, 8, 6, npcA, "Make yourself at home, hero.");
    chest(*house, 14, 3, 1, 3, "Found 3 Potions!");
    tele(*house, 9, 13, 2, 6, 8);           // -> Town

    // ============ Map 4: Inn (22x15) ============
    auto inn = p.addMap("Inn", 22, 15); tset(*inn);
    fill(*inn, 0, 0, 0, 22, 15, WOOD);
    border(*inn);
    npc(*inn, 11, 6, oldman, "Welcome to the Inn. Rest well!");
    chest(*inn, 18, 3, 3, 2, "Found 2 Ethers!");
    tele(*inn, 11, 14, 2, 26, 8);           // -> Town

    // ============ Map 5: Dungeon B1 (44x32) ============
    auto d1 = p.addMap("Dungeon B1", 44, 32); tset(*d1);
    fill(*d1, 0, 0, 0, 44, 32, STONE);
    border(*d1);
    for (int i = 0; i < 10; ++i) {          // pillars / walls
        int wx = 5 + (i*9) % 34, wy = 5 + (i*6) % 22;
        fill(*d1, 1, wx, wy, 3, 3, BRICK); collide(*d1, wx, wy, 3, 3);
    }
    d1->encounterEnemies = {1,2,3}; d1->encounterRate = 0;
    chest(*d1, 40, 3, 2, 5, "Treasure! 5 Hi-Potions.");
    tele(*d1, 4, 2, 1, 24, 33);             // back to Overworld
    tele(*d1, 40, 29, 6, 4, 2);             // -> Dungeon B2
    npc(*d1, 6, 5, oldman, "Turn back... the Dragon is near.");

    // ============ Map 6: Dungeon B2 - Boss (40x30) ============
    auto d2 = p.addMap("Dungeon B2", 40, 30); tset(*d2);
    fill(*d2, 0, 0, 0, 40, 30, STONE);
    border(*d2);
    d2->encounterEnemies = {3}; d2->encounterRate = 0;
    // Boss as a field StartBattle event (spawns the Dragon as a live monster)
    {
        Event boss; boss.id = d2->nextEventId(); boss.x = 20; boss.y = 8;
        boss.type = EventType::StartBattle; boss.trigger = TriggerType::PlayerTouch;
        boss.itemId = 4; boss.amount = 1; boss.once = true;
        boss.text = ""; boss.graphicAsset = -1;
        d2->events.push_back(boss);
    }
    chest(*d2, 20, 4, 2, 9, "The Dragon's hoard! 9 Hi-Potions!");
    tele(*d2, 4, 28, 5, 40, 28);            // back to B1

    p.save();
    printf("Generated world '%s' with %d maps at %s\n", p.name.c_str(), (int)p.maps.size(), dir.c_str());
    return 0;
}
