#pragma once
// TextureCache: loads and caches raylib textures by file path. Generates a
// visible placeholder for missing files so the engine never crashes on bad data.
#include <string>
#include <unordered_map>
#include "raylib.h"

namespace tsukuru {

class TextureCache {
public:
    ~TextureCache();
    // Returns a cached texture for the path, loading it on first use.
    // An empty path or a missing file yields a procedural placeholder.
    const Texture2D& get(const std::string& path);
    void clear();

private:
    Texture2D placeholder();
    std::unordered_map<std::string, Texture2D> cache_;
    Texture2D placeholder_{};
    bool placeholderReady_ = false;
};

} // namespace tsukuru
