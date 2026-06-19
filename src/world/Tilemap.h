#pragma once
// Tilemap: multiple visual layers of tile indices plus a collision layer.
// Tile value -1 means "empty". Collision value 1 means "blocked".
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace tsukuru {

constexpr int kLayerCount = 3; // ground, deco, overhead

class Tilemap {
public:
    Tilemap() = default;
    Tilemap(int w, int h) { resize(w, h); }

    void resize(int w, int h);

    int width()  const { return width_; }
    int height() const { return height_; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width_ && y < height_; }

    int  tile(int layer, int x, int y) const;
    void setTile(int layer, int x, int y, int value);

    bool blocked(int x, int y) const;
    void setBlocked(int x, int y, bool v);

    void fill(int layer, int x, int y, int newValue); // flood fill (bucket tool)

    const std::vector<int>& layer(int i) const { return layers_[i]; }

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    int idx(int x, int y) const { return y * width_ + x; }
    int width_  = 0;
    int height_ = 0;
    std::vector<int>     layers_[kLayerCount];
    std::vector<uint8_t> collision_;
};

} // namespace tsukuru
