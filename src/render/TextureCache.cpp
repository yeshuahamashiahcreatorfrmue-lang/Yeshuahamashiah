#include "render/TextureCache.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace tsukuru {

TextureCache::~TextureCache() { clear(); }

Texture2D TextureCache::placeholder() {
    if (placeholderReady_) return placeholder_;
    // 64x64 magenta/black checker so missing assets are obvious.
    Image img = GenImageColor(64, 64, MAGENTA);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            if (((x / 16) + (y / 16)) % 2 == 0)
                ImageDrawPixel(&img, x, y, BLACK);
    placeholder_ = LoadTextureFromImage(img);
    UnloadImage(img);
    placeholderReady_ = true;
    return placeholder_;
}

const Texture2D& TextureCache::get(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second;

    Texture2D tex;
    std::error_code ec;
    if (!path.empty() && fs::exists(path, ec)) {
        tex = LoadTexture(path.c_str());
        if (tex.id == 0) tex = placeholder();
    } else {
        tex = placeholder();
    }
    auto res = cache_.emplace(path, tex);
    return res.first->second;
}

void TextureCache::invalidate(const std::string& path) {
    auto it = cache_.find(path);
    if (it == cache_.end()) return;
    if (it->second.id != placeholder_.id) UnloadTexture(it->second);
    cache_.erase(it);   // next get() reloads from disk
}

void TextureCache::clear() {
    for (auto& kv : cache_) {
        // Don't double-free the shared placeholder.
        if (kv.second.id != placeholder_.id) UnloadTexture(kv.second);
    }
    cache_.clear();
    if (placeholderReady_) { UnloadTexture(placeholder_); placeholderReady_ = false; }
}

} // namespace tsukuru
