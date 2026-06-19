#pragma once
// Shared lightweight types used across the engine.
// This header is raylib-independent so it can be used by the core/logic layer.
#include <string>
#include <cstdint>

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

// The default tile size used by the engine (pixels).
constexpr int kDefaultTileSize = 32;

} // namespace tsukuru
