#pragma once
// Tileset metadata: links an image asset to a grid of tiles.
// Rendering computes the source rectangle for a given tile index.
#include <nlohmann/json.hpp>

namespace tsukuru {

struct Tileset {
    int assetId    = -1;   // image asset id in AssetManager
    int tileWidth  = 32;
    int tileHeight = 32;
    int columns    = 8;    // tiles per row in the image
    int rows       = 8;

    int tileCount() const { return columns * rows; }

    // Source pixel rect (x, y) for a given linear tile index.
    void srcOf(int index, int& outX, int& outY) const {
        if (columns <= 0) { outX = outY = 0; return; }
        outX = (index % columns) * tileWidth;
        outY = (index / columns) * tileHeight;
    }

    nlohmann::json toJson() const {
        return {{"assetId", assetId}, {"tileWidth", tileWidth},
                {"tileHeight", tileHeight}, {"columns", columns}, {"rows", rows}};
    }
    void fromJson(const nlohmann::json& j) {
        assetId    = j.value("assetId", -1);
        tileWidth  = j.value("tileWidth", 32);
        tileHeight = j.value("tileHeight", 32);
        columns    = j.value("columns", 8);
        rows       = j.value("rows", 8);
    }
};

} // namespace tsukuru
