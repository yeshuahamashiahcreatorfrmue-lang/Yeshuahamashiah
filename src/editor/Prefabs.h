#pragma once
// Prefabs (stamps): multi-tile, multi-layer building blocks that the editor can
// place with a single click — the same pieces used to author the sample world
// (houses, trees, ponds, fences, decorations). Tile indices match the engine's
// standard 8-column tileset layout.
#include <vector>
#include <string>

namespace tsukuru {

// Standard tileset indices (8 columns x 6 rows).
enum TileId {
    T_GRASS=0, T_GRASS2=1, T_FLGRASS=2, T_PATH=3, T_COBBLE=4, T_WOOD=5, T_WATER=6, T_WATER2=7,
    T_SHALLOW=8, T_SHALLOW2=9, T_SAND=10, T_DIRT2=11, T_HEDGE=12, T_HEDGETOP=13, T_BRIDGE=14, T_PLANK=15,
    T_WALL=16, T_WINDOW=17, T_DOOR=18, T_ROOF_TL=19, T_ROOF_TM=20, T_ROOF_TR=21, T_ROOF_BL=22, T_ROOF_BM=23,
    T_ROOF_BR=24, T_FENCE_H=25, T_FENCE_V=26, T_GATE=27, T_LAMP=28, T_WELL=29, T_BARREL=30, T_CRATE=31,
    T_TRUNK=32, T_CANOPY=33, T_BUSH=34, T_ROCK=35, T_FL_RED=36, T_FL_YEL=37, T_SIGN=38, T_FLOWERBED=39,
    T_STATUE=40, T_STAIRS=41, T_TALLG=42, T_STUMP=43, T_MUSH=44, T_CHEST=45
};

struct PrefabCell {
    int dx, dy;      // offset from the stamp origin
    int layer;       // 0 ground, 1 deco, 2 overhead
    int tile;        // tile index (-1 = leave)
    bool blocked;    // set collision on this cell
};

struct Prefab {
    std::string name;
    int w, h;
    std::vector<PrefabCell> cells;
};

// The editor's stamp palette.
inline const std::vector<Prefab>& prefabs() {
    static const std::vector<Prefab> list = {
        // Tree: trunk below + canopy overhead, solid
        { "Tree", 1, 1, {
            {0,0,1,T_TRUNK,true}, {0,0,2,T_CANOPY,false} } },
        // House: 3x2 roof + wall row (window/door/window), solid
        { "House", 3, 3, {
            {0,0,1,T_ROOF_TL,true},{1,0,1,T_ROOF_TM,true},{2,0,1,T_ROOF_TR,true},
            {0,1,1,T_ROOF_BL,true},{1,1,1,T_ROOF_BM,true},{2,1,1,T_ROOF_BR,true},
            {0,2,1,T_WINDOW,true},{1,2,1,T_DOOR,true},{2,2,1,T_WINDOW,true} } },
        // Pond 5x4: shallow ring + deep water, solid
        { "Pond", 5, 4, {
            {0,0,0,T_SHALLOW,true},{1,0,0,T_SHALLOW,true},{2,0,0,T_SHALLOW,true},{3,0,0,T_SHALLOW,true},{4,0,0,T_SHALLOW,true},
            {0,1,0,T_SHALLOW,true},{1,1,0,T_WATER,true},{2,1,0,T_WATER,true},{3,1,0,T_WATER,true},{4,1,0,T_SHALLOW,true},
            {0,2,0,T_SHALLOW,true},{1,2,0,T_WATER,true},{2,2,0,T_WATER,true},{3,2,0,T_WATER,true},{4,2,0,T_SHALLOW,true},
            {0,3,0,T_SHALLOW,true},{1,3,0,T_SHALLOW,true},{2,3,0,T_SHALLOW,true},{3,3,0,T_SHALLOW,true},{4,3,0,T_SHALLOW,true} } },
        // Fence runs
        { "Fence row", 3, 1, { {0,0,1,T_FENCE_H,true},{1,0,1,T_FENCE_H,true},{2,0,1,T_FENCE_H,true} } },
        { "Fence col", 1, 3, { {0,0,1,T_FENCE_V,true},{0,1,1,T_FENCE_V,true},{0,2,1,T_FENCE_V,true} } },
        // Flowerbed patch (ground)
        { "Flowerbed", 3, 2, {
            {0,0,0,T_FLOWERBED,false},{1,0,0,T_FLOWERBED,false},{2,0,0,T_FLOWERBED,false},
            {0,1,0,T_FLOWERBED,false},{1,1,0,T_FLOWERBED,false},{2,1,0,T_FLOWERBED,false} } },
        // single-tile props
        { "Well",   1,1, { {0,0,1,T_WELL,true} } },
        { "Statue", 1,1, { {0,0,1,T_STATUE,true} } },
        { "Lamp",   1,1, { {0,0,1,T_LAMP,true} } },
        { "Sign",   1,1, { {0,0,1,T_SIGN,true} } },
        { "Barrel", 1,1, { {0,0,1,T_BARREL,true} } },
        { "Crate",  1,1, { {0,0,1,T_CRATE,true} } },
        { "Bush",   1,1, { {0,0,1,T_BUSH,true} } },
        { "Rock",   1,1, { {0,0,1,T_ROCK,true} } },
        { "Chest",  1,1, { {0,0,1,T_CHEST,false} } },
        { "TallGrass", 1,1, { {0,0,2,T_TALLG,false} } },
    };
    return list;
}

} // namespace tsukuru
