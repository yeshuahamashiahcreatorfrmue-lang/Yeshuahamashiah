#pragma once
// Shared lightweight types used across the engine.
// This header is raylib-independent so it can be used by the core/logic layer.
#include <string>
#include <cstdint>
#include <cstdlib>

namespace tsukuru {

struct Vec2i {
    int x = 0;
    int y = 0;
    bool operator==(const Vec2i& o) const { return x == o.x && y == o.y; }
};

enum class Direction { Down = 0, Left = 1, Right = 2, Up = 3 };

inline Vec2i dirToDelta(Direction d) {
    switch (d) {
        case Direction::Down:  return { 0,  1};
        case Direction::Left:  return {-1,  0};
        case Direction::Right: return { 1,  0};
        case Direction::Up:    return { 0, -1};
    }
    return {0, 0};
}

// --- grid / facing helpers (shared by the field movement & AI code) ----------
// Sign of an integer: -1, 0, or +1.
inline int isign(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

// Chebyshev (king-move) distance between two tiles — the engine's notion of
// "adjacent" (<= 1) and aggro range.
inline int chebyshev(int ax, int ay, int bx, int by) {
    int dx = std::abs(ax - bx), dy = std::abs(ay - by);
    return dx > dy ? dx : dy;
}

// Movement direction (0=Down,1=Left,2=Right,3=Up) implied by a step (dx,dy):
// vertical wins ties so a mover that can't go sideways still faces up/down.
inline int dirFromDelta(int dx, int dy) {
    return dy > 0 ? 0 : dy < 0 ? 3 : dx < 0 ? 1 : 2;
}

// Facing to look toward an offset (dx,dy): the larger axis wins, so an entity
// turns to the side the target is mostly on.
inline int faceDir(int dx, int dy) {
    return std::abs(dx) >= std::abs(dy) ? (dx > 0 ? 2 : 1) : (dy > 0 ? 0 : 3);
}

// The default tile size used by the engine (pixels).
constexpr int kDefaultTileSize = 32;

} // namespace tsukuru
