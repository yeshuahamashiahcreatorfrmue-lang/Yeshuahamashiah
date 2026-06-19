#include "world/Tilemap.h"
#include <vector>

using nlohmann::json;

namespace tsukuru {

void Tilemap::resize(int w, int h) {
    width_ = w < 1 ? 1 : w;
    height_ = h < 1 ? 1 : h;
    for (auto& l : layers_) l.assign(width_ * height_, -1);
    collision_.assign(width_ * height_, 0);
}

void Tilemap::resizePreserve(int w, int h) {
    w = w < 1 ? 1 : w; h = h < 1 ? 1 : h;
    std::vector<int>     old[kLayerCount];
    std::vector<uint8_t> oldCol = collision_;
    for (int i = 0; i < kLayerCount; ++i) old[i] = layers_[i];
    int ow = width_, oh = height_;

    int nw = w, nh = h;
    width_ = nw; height_ = nh;
    for (auto& l : layers_) l.assign(nw * nh, -1);
    collision_.assign(nw * nh, 0);

    int cw = ow < nw ? ow : nw, ch = oh < nh ? oh : nh;
    for (int y = 0; y < ch; ++y)
        for (int x = 0; x < cw; ++x) {
            for (int i = 0; i < kLayerCount; ++i)
                layers_[i][y * nw + x] = old[i][y * ow + x];
            collision_[y * nw + x] = oldCol[y * ow + x];
        }
}

int Tilemap::tile(int layer, int x, int y) const {
    if (layer < 0 || layer >= kLayerCount || !inBounds(x, y)) return -1;
    return layers_[layer][idx(x, y)];
}

void Tilemap::setTile(int layer, int x, int y, int value) {
    if (layer < 0 || layer >= kLayerCount || !inBounds(x, y)) return;
    layers_[layer][idx(x, y)] = value;
}

bool Tilemap::blocked(int x, int y) const {
    if (!inBounds(x, y)) return true; // out of bounds blocks movement
    return collision_[idx(x, y)] != 0;
}

void Tilemap::setBlocked(int x, int y, bool v) {
    if (!inBounds(x, y)) return;
    collision_[idx(x, y)] = v ? 1 : 0;
}

void Tilemap::fill(int layer, int x, int y, int newValue) {
    if (layer < 0 || layer >= kLayerCount || !inBounds(x, y)) return;
    int target = layers_[layer][idx(x, y)];
    if (target == newValue) return;

    std::vector<std::pair<int,int>> stack{{x, y}};
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        if (!inBounds(cx, cy)) continue;
        if (layers_[layer][idx(cx, cy)] != target) continue;
        layers_[layer][idx(cx, cy)] = newValue;
        stack.push_back({cx + 1, cy});
        stack.push_back({cx - 1, cy});
        stack.push_back({cx, cy + 1});
        stack.push_back({cx, cy - 1});
    }
}

json Tilemap::toJson() const {
    json j;
    j["width"]  = width_;
    j["height"] = height_;
    json layers = json::array();
    for (int i = 0; i < kLayerCount; ++i) layers.push_back(layers_[i]);
    j["layers"]    = layers;
    j["collision"] = collision_;
    return j;
}

void Tilemap::fromJson(const json& j) {
    resize(j.value("width", 1), j.value("height", 1));
    if (j.contains("layers")) {
        const auto& ls = j["layers"];
        for (int i = 0; i < kLayerCount && i < (int)ls.size(); ++i) {
            std::vector<int> data = ls[i].get<std::vector<int>>();
            if ((int)data.size() == width_ * height_) layers_[i] = std::move(data);
        }
    }
    if (j.contains("collision")) {
        std::vector<uint8_t> c = j["collision"].get<std::vector<uint8_t>>();
        if ((int)c.size() == width_ * height_) collision_ = std::move(c);
    }
}

} // namespace tsukuru
