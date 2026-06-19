#!/usr/bin/env python3
"""Builds the sample RPG project (project.json, database.json, maps/*.json).
Tiles indices match tools/gen_assets.cpp:
  0 grass  1 dirt/path  2 water  3 tree  4 brick wall  5 wood floor
  6 flowers 7 stone  8 dark grass 9 sand 10 deep water 11 roof
"""
import json, os

ROOT = os.path.join(os.path.dirname(__file__), "..", "projects", "sample_rpg")
MAPS = os.path.join(ROOT, "maps")
os.makedirs(MAPS, exist_ok=True)

EMPTY = -1
LAYERS = 3

def grid(w, h, fill=EMPTY):
    return [fill] * (w * h)

def setg(layer, w, x, y, v):
    layer[y * w + x] = v

def rect(layer, w, x0, y0, x1, y1, v):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            layer[y * w + x] = v

def tilemap(w, h, ground, deco, over, collision):
    return {
        "width": w, "height": h,
        "layers": [ground, deco, over],
        "collision": collision,
    }

TILESET = {"assetId": 1, "tileWidth": 32, "tileHeight": 32, "columns": 8, "rows": 2}

# ----------------------------------------------------------------- TOWN (map 1)
def build_town():
    w, h = 20, 15
    g = grid(w, h, 0)            # grass everywhere
    d = grid(w, h)              # deco
    o = grid(w, h)              # overhead
    c = [0] * (w * h)

    # paths (dirt cross)
    rect(g, w, 0, 7, 19, 8, 1)
    rect(g, w, 9, 0, 10, 14, 1)

    # tree border
    for x in range(w):
        setg(d, w, x, 0, 3); c[0 * w + x] = 1
        setg(d, w, x, h - 1, 3); c[(h - 1) * w + x] = 1
    for y in range(h):
        setg(d, w, 0, y, 3); c[y * w + 0] = 1
        setg(d, w, w - 1, y, 3); c[y * w + (w - 1)] = 1
    # keep path openings on south edge walkable
    setg(d, w, 9, h - 1, EMPTY); c[(h - 1) * w + 9] = 0
    setg(d, w, 10, h - 1, EMPTY); c[(h - 1) * w + 10] = 0

    # pond
    rect(g, w, 3, 11, 6, 13, 2)
    for y in range(11, 14):
        for x in range(3, 7):
            c[y * w + x] = 1

    # house (top-right): roof + brick walls + wood floor patch
    rect(d, w, 13, 2, 17, 2, 11)        # roof
    rect(d, w, 13, 3, 17, 4, 4)         # brick walls
    rect(g, w, 13, 3, 17, 4, 5)         # wood floor under
    for y in range(2, 5):
        for x in range(13, 18):
            c[y * w + x] = 1
    # door opening (walkable, teleport tile)
    setg(d, w, 15, 4, EMPTY); c[4 * w + 15] = 0
    setg(g, w, 15, 4, 5)

    # a few flowers
    for (fx, fy) in [(5, 5), (6, 9), (12, 10), (4, 6)]:
        setg(d, w, fx, fy, 6)

    events = [
        {"id": 1, "x": 15, "y": 4, "type": "teleport", "trigger": "touch",
         "targetMap": 2, "targetX": 6, "targetY": 7, "text": ""},
        {"id": 2, "x": 8, "y": 9, "type": "message", "trigger": "action",
         "graphicAsset": 3, "text": "Welcome to our village, brave hero!"},
        {"id": 3, "x": 11, "y": 6, "type": "giveItem", "trigger": "action",
         "graphicAsset": 4, "itemId": 1, "amount": 3, "once": True,
         "text": "Old Man: Take these 3 potions on your journey!"},
        {"id": 4, "x": 5, "y": 8, "type": "message", "trigger": "action",
         "text": "Sign: SOUTH -> Monster Fields. EAST house -> Shop."},
        {"id": 5, "x": 9, "y": 14, "type": "teleport", "trigger": "touch",
         "targetMap": 3, "targetX": 8, "targetY": 1, "text": ""},
    ]
    return {
        "id": 1, "name": "Town", "tileset": TILESET,
        "tilemap": tilemap(w, h, g, d, o, c),
        "events": events,
        "encounterEnemies": [], "encounterRate": 0, "bgmAsset": -1,
    }

# ----------------------------------------------------------------- HOUSE (map 2)
def build_house():
    w, h = 13, 9
    g = grid(w, h, 5)            # wood floor
    d = grid(w, h)
    o = grid(w, h)
    c = [0] * (w * h)
    # brick walls around
    for x in range(w):
        setg(d, w, x, 0, 4); c[0 * w + x] = 1
        setg(d, w, x, h - 1, 4); c[(h - 1) * w + x] = 1
    for y in range(h):
        setg(d, w, 0, y, 4); c[y * w + 0] = 1
        setg(d, w, w - 1, y, 4); c[y * w + (w - 1)] = 1
    # exit door at bottom center
    setg(d, w, 6, h - 1, EMPTY); c[(h - 1) * w + 6] = 0
    setg(g, w, 6, h - 1, 7)

    events = [
        {"id": 1, "x": 6, "y": 8, "type": "teleport", "trigger": "touch",
         "targetMap": 1, "targetX": 15, "targetY": 5, "text": ""},
        {"id": 2, "x": 6, "y": 2, "type": "shop", "trigger": "action",
         "graphicAsset": 3, "itemId": 2, "amount": 1,
         "text": "Shopkeeper: A Hi-Potion costs 100 gold. (press to buy)"},
        {"id": 3, "x": 3, "y": 3, "type": "giveItem", "trigger": "action",
         "itemId": 4, "amount": 1, "once": True,
         "text": "You found an Iron Sword in the chest!"},
        {"id": 4, "x": 9, "y": 3, "type": "message", "trigger": "action",
         "graphicAsset": 4, "text": "Grandpa: Rest here anytime, hero."},
    ]
    return {
        "id": 2, "name": "House", "tileset": TILESET,
        "tilemap": tilemap(w, h, g, d, o, c),
        "events": events,
        "encounterEnemies": [], "encounterRate": 0, "bgmAsset": -1,
    }

# ----------------------------------------------------------------- FIELD (map 3)
def build_field():
    w, h = 18, 14
    g = grid(w, h, 8)            # dark grass
    d = grid(w, h)
    o = grid(w, h)
    c = [0] * (w * h)
    # sandy path down the middle
    rect(g, w, 7, 0, 9, 13, 9)
    # scattered trees (with collision)
    for (tx, ty) in [(3, 3), (4, 7), (13, 4), (14, 9), (5, 11), (12, 11), (2, 9), (15, 6)]:
        setg(d, w, tx, ty, 3); c[ty * w + tx] = 1
    # tree border on sides
    for y in range(h):
        setg(d, w, 0, y, 3); c[y * w + 0] = 1
        setg(d, w, w - 1, y, 3); c[y * w + (w - 1)] = 1
    setg(d, w, 0, 7, EMPTY); c[7 * w + 0] = 0  # small opening

    events = [
        {"id": 1, "x": 8, "y": 0, "type": "teleport", "trigger": "touch",
         "targetMap": 1, "targetX": 9, "targetY": 13, "text": ""},
        {"id": 2, "x": 8, "y": 7, "type": "startBattle", "trigger": "action",
         "graphicAsset": 5, "itemId": 1, "amount": 2,
         "text": ""},  # itemId reused as enemyId, amount = count
        {"id": 3, "x": 4, "y": 5, "type": "message", "trigger": "action",
         "text": "A chilling wind blows. Monsters roam here..."},
    ]
    return {
        "id": 3, "name": "Field", "tileset": TILESET,
        "tilemap": tilemap(w, h, g, d, o, c),
        "events": events,
        "encounterEnemies": [1, 2], "encounterRate": 12, "bgmAsset": -1,
    }

# ----------------------------------------------------------------- DATABASE
def build_database():
    return {
        "items": [
            {"id": 1, "name": "Potion", "description": "Restores 50 HP.", "price": 30,
             "iconAsset": -1, "effect": "healHP", "power": 50, "consumable": True},
            {"id": 2, "name": "Hi-Potion", "description": "Restores 150 HP.", "price": 100,
             "iconAsset": -1, "effect": "healHP", "power": 150, "consumable": True},
            {"id": 3, "name": "Ether", "description": "Restores 30 MP.", "price": 80,
             "iconAsset": -1, "effect": "healMP", "power": 30, "consumable": True},
        ],
        "equipment": [
            {"id": 1, "name": "Bronze Sword", "slot": "weapon", "price": 50, "iconAsset": -1, "atk": 6, "def": 0},
            {"id": 4, "name": "Iron Sword", "slot": "weapon", "price": 200, "iconAsset": -1, "atk": 14, "def": 0},
            {"id": 2, "name": "Leather Armor", "slot": "armor", "price": 60, "iconAsset": -1, "atk": 0, "def": 6},
            {"id": 3, "name": "Iron Armor", "slot": "armor", "price": 220, "iconAsset": -1, "atk": 0, "def": 13},
        ],
        "skills": [
            {"id": 1, "name": "Fireball", "mpCost": 5, "power": 22, "healing": False},
            {"id": 2, "name": "Heal", "mpCost": 8, "power": 45, "healing": True},
        ],
        "actors": [
            {"id": 1, "name": "Hero", "spriteAsset": 2, "maxHp": 120, "maxMp": 30,
             "atk": 14, "def": 7, "spd": 6, "skills": [1, 2]},
        ],
        "enemies": [
            {"id": 1, "name": "Slime", "spriteAsset": 5, "maxHp": 30, "maxMp": 0,
             "atk": 8, "def": 3, "spd": 4, "expReward": 8, "goldReward": 6},
            {"id": 2, "name": "Bat", "spriteAsset": 6, "maxHp": 22, "maxMp": 0,
             "atk": 10, "def": 2, "spd": 8, "expReward": 10, "goldReward": 8},
        ],
    }

# ----------------------------------------------------------------- PROJECT
def build_project():
    return {
        "name": "Hero's Adventure",
        "startMap": 1, "startX": 9, "startY": 9,
        "startActor": 1, "playerSprite": 2,
        "assets": {
            "nextId": 7,
            "items": [
                {"id": 1, "type": "image", "name": "tileset", "path": "assets/tileset.png"},
                {"id": 2, "type": "image", "name": "hero", "path": "assets/hero.png"},
                {"id": 3, "type": "image", "name": "npc", "path": "assets/npc.png"},
                {"id": 4, "type": "image", "name": "oldman", "path": "assets/oldman.png"},
                {"id": 5, "type": "image", "name": "slime", "path": "assets/slime.png"},
                {"id": 6, "type": "image", "name": "bat", "path": "assets/bat.png"},
            ],
        },
        "maps": [1, 2, 3],
    }

def write(path, obj):
    with open(path, "w") as f:
        json.dump(obj, f, indent=2)
    print("wrote", os.path.relpath(path, ROOT))

if __name__ == "__main__":
    write(os.path.join(ROOT, "project.json"), build_project())
    write(os.path.join(ROOT, "database.json"), build_database())
    write(os.path.join(MAPS, "1.json"), build_town())
    write(os.path.join(MAPS, "2.json"), build_house())
    write(os.path.join(MAPS, "3.json"), build_field())
    print("Sample project built.")
