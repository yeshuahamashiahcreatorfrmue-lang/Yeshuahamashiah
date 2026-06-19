#pragma once
// AssetManager: registers external files (images / audio) into a project.
// Pure metadata + file management (raylib-independent). The graphical layer
// (render/TextureCache) loads the actual GPU textures from these paths.
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace tsukuru {

enum class AssetType { Image, Audio };

struct AssetEntry {
    int         id   = -1;       // unique id within the project
    AssetType   type = AssetType::Image;
    std::string name;           // display name
    std::string relPath;        // path relative to project dir, e.g. "assets/town.png"
    int         frames = 1;     // >1 = animated (horizontal sprite-sheet of N frames)
    int         fps    = 8;     // playback speed for animated assets
};

class AssetManager {
public:
    // Register a file into the project: copies it into <projectDir>/assets/,
    // assigns a unique id and records it. Returns the new asset id (-1 on error).
    int registerAsset(const std::string& projectDir,
                      const std::string& sourceFile,
                      AssetType type,
                      const std::string& displayName = "");

    // Register an asset that already lives inside the project (no copy).
    int addExisting(AssetType type, const std::string& name, const std::string& relPath);

    // Mark an asset as an animated sprite-sheet (N frames laid horizontally).
    void setAnim(int id, int frames, int fps = 8);

    void remove(int id);

    const AssetEntry* find(int id) const;
    const std::vector<AssetEntry>& all() const { return assets_; }
    std::vector<const AssetEntry*> byType(AssetType type) const;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);

private:
    std::vector<AssetEntry> assets_;
    int nextId_ = 1;
};

} // namespace tsukuru
