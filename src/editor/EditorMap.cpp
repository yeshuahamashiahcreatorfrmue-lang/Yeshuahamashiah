// EditorMap: tile palette + map canvas painting.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "render/AssetGen.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {

void Editor::drawMapTab() {
    Rectangle paletteArea = { 0, kToolbarH, kPaletteW, (float)GetScreenHeight() - kToolbarH };
    Rectangle canvasArea  = { kPaletteW, kToolbarH, (float)GetScreenWidth() - kPaletteW,
                              (float)GetScreenHeight() - kToolbarH };
    drawMapCanvas(canvasArea);
    if (tool_ == Tool::Stamp) drawPrefabPalette(paletteArea);
    else drawTilePalette(paletteArea);
}

void Editor::drawTilePalette(Rectangle area) {
    ui::panel(area, ui::kPanel);
    auto m = activeMap();
    if (!m) return;
    const Tileset& set = m->tileset;

    // Layer selector
    float ly = area.y + 8;
    ui::label("레이어", (int)area.x + 10, (int)ly, 16, ui::kTextDim);
    for (int i = 0; i < kLayerCount; ++i) {
        if (ui::button({ area.x + 10 + i*64, ly + 22, 60, 26 },
                       TextFormat("L%d", i+1), activeLayer_ == i))
            activeLayer_ = i;
    }
    float ty = ly + 60;

    // Tileset image asset selector
    ui::label("타일셋", (int)area.x + 10, (int)ty, 16, ui::kTextDim);
    auto imgs = engine_.project().assets.byType(AssetType::Image);
    if (ui::button({ area.x + 10, ty + 22, area.width - 20, 26 },
                   set.assetId >= 0 ? "타일셋 이미지 변경" : "타일셋 이미지 선택")) {
        // cycle to next image asset
        if (!imgs.empty()) {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == set.assetId) idx = i;
            m->tileset.assetId = imgs[(idx + 1) % imgs.size()]->id;
        }
    }
    ty += 54;
    ui::intStepper({ area.x + 10, ty, area.width - 20, 24 }, "열", m->tileset.columns, 1, 1, 64); ty += 28;
    ui::intStepper({ area.x + 10, ty, area.width - 20, 24 }, "행", m->tileset.rows, 1, 1, 64); ty += 30;

    // mark the selected tile as animated (cycles tile <-> tile+1 in play)
    bool isAnim = std::find(m->animTiles.begin(), m->animTiles.end(), selectedTile_) != m->animTiles.end();
    if (ui::button({ area.x + 10, ty, area.width - 20, 24 },
                   isAnim ? "애니메이션: 켜짐" : "애니메이션: 꺼짐", isAnim)) {
        if (isAnim) m->animTiles.erase(std::remove(m->animTiles.begin(), m->animTiles.end(), selectedTile_), m->animTiles.end());
        else m->animTiles.push_back(selectedTile_);
    }
    ty += 32;

    // Tile grid
    if (set.assetId < 0) {
        ui::label("에셋 탭에 이미지를", (int)area.x + 10, (int)ty, 14, ui::kTextDim);
        ui::label("끌어다 놓으세요.", (int)area.x + 10, (int)ty + 18, 14, ui::kTextDim);
        return;
    }
    const Texture2D& tex = engine_.assetTexture(set.assetId);
    float gridW = area.width - 20;
    float scale = gridW / std::max(1, tex.width);
    Rectangle dst = { area.x + 10, ty, gridW, tex.height * scale };
    DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height }, dst, {0,0}, 0, WHITE);

    // grid lines + selection
    float cellW = gridW / std::max(1, set.columns);
    float cellH = (set.tileHeight * scale);
    for (int c = 0; c <= set.columns; ++c)
        DrawLine((int)(dst.x + c*cellW), (int)dst.y, (int)(dst.x + c*cellW), (int)(dst.y + set.rows*cellH), Fade(BLACK,0.3f));
    for (int r = 0; r <= set.rows; ++r)
        DrawLine((int)dst.x, (int)(dst.y + r*cellH), (int)(dst.x + set.columns*cellW), (int)(dst.y + r*cellH), Fade(BLACK,0.3f));

    // selection highlight
    int selC = selectedTile_ % std::max(1, set.columns);
    int selR = selectedTile_ / std::max(1, set.columns);
    DrawRectangleLinesEx({ dst.x + selC*cellW, dst.y + selR*cellH, cellW, cellH }, 2, ui::kAccent);

    // click to select
    if (ui::mouseIn(dst) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mp = GetMousePosition();
        int c = (int)((mp.x - dst.x) / cellW);
        int r = (int)((mp.y - dst.y) / cellH);
        if (c >= 0 && c < set.columns && r >= 0 && r < set.rows)
            selectedTile_ = r * set.columns + c;
    }
}

void Editor::drawMapCanvas(Rectangle area) {
    auto m = activeMap();
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    if (!m) { EndScissorMode(); return; }
    int TS = m->tileset.tileWidth;

    BeginMode2D(cam_);
    const Texture2D& tex = engine_.assetTexture(m->tileset.assetId);
    const Tileset& set = m->tileset;
    int w = m->tilemap.width(), h = m->tilemap.height();

    // viewport culling: only iterate tiles visible in the canvas
    Vector2 tl = GetScreenToWorld2D({ area.x, area.y }, cam_);
    Vector2 br = GetScreenToWorld2D({ area.x + area.width, area.y + area.height }, cam_);
    int cx0 = std::max(0, (int)(tl.x/TS) - 1), cy0 = std::max(0, (int)(tl.y/TS) - 1);
    int cx1 = std::min(w-1, (int)(br.x/TS) + 1), cy1 = std::min(h-1, (int)(br.y/TS) + 1);

    // map background
    DrawRectangle(0, 0, w*TS, h*TS, Color{ 30, 33, 42, 255 });
    for (int layer = 0; layer < kLayerCount; ++layer) {
        // dim layers above the active one for clarity
        unsigned char a = (layer == activeLayer_ || !collisionMode_) ? 255 : 120;
        for (int y = cy0; y <= cy1; ++y)
            for (int x = cx0; x <= cx1; ++x) {
                int t = m->tilemap.tile(layer, x, y);
                if (t < 0 || set.assetId < 0) continue;
                int sx, sy; set.srcOf(t, sx, sy);
                DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                               { (float)x*TS,(float)y*TS,(float)TS,(float)TS }, {0,0}, 0, Fade(WHITE, a/255.0f));
            }
    }
    // grid
    for (int x = 0; x <= w; ++x) DrawLine(x*TS, 0, x*TS, h*TS, Fade(BLACK, 0.25f));
    for (int y = 0; y <= h; ++y) DrawLine(0, y*TS, w*TS, y*TS, Fade(BLACK, 0.25f));
    // collision overlay
    if (collisionMode_)
        for (int y = cy0; y <= cy1; ++y)
            for (int x = cx0; x <= cx1; ++x)
                if (m->tilemap.blocked(x, y))
                    DrawRectangle(x*TS, y*TS, TS, TS, Fade(ui::kDanger, 0.45f));
    EndMode2D();

    // painting
    if (ui::mouseIn(area) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
        int tx = (int)std::floor(world.x / TS), ty = (int)std::floor(world.y / TS);
        bool inMap = m->tilemap.inBounds(tx, ty);
        bool eyedrop = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

        // hover highlight + rectangle preview
        BeginMode2D(cam_);
        if (inMap)
            DrawRectangleLinesEx({ (float)tx*TS,(float)ty*TS,(float)TS,(float)TS }, 2,
                                 eyedrop ? GREEN : ui::kAccentHi);
        if (tool_ == Tool::Rect && rectDragging_ && inMap) {
            int x0 = std::min(rectStartX_, tx), y0 = std::min(rectStartY_, ty);
            int x1 = std::max(rectStartX_, tx), y1 = std::max(rectStartY_, ty);
            DrawRectangle(x0*TS, y0*TS, (x1-x0+1)*TS, (y1-y0+1)*TS, Fade(ui::kAccent, 0.30f));
            DrawRectangleLinesEx({ (float)x0*TS,(float)y0*TS,(float)(x1-x0+1)*TS,(float)(y1-y0+1)*TS },
                                 2, ui::kAccentHi);
        }
        if (tool_ == Tool::Stamp && inMap) {                  // prefab footprint preview
            const Prefab& pf = prefabs()[prefabSel_];
            DrawRectangle(tx*TS, ty*TS, pf.w*TS, pf.h*TS, Fade(ui::kAccent, 0.25f));
            DrawRectangleLinesEx({ (float)tx*TS,(float)ty*TS,(float)pf.w*TS,(float)pf.h*TS }, 2, ui::kAccentHi);
        }
        EndMode2D();

        if (inMap) {
            if (eyedrop) {                                  // eyedropper: pick a tile
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    int t = m->tilemap.tile(activeLayer_, tx, ty);
                    if (t >= 0) { selectedTile_ = t; setStatus("타일 선택: " + std::to_string(t)); }
                }
            } else if (tool_ == Tool::Stamp) {              // prefab stamp
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    stampPrefab(tx, ty);
                    setStatus(prefabs()[prefabSel_].name + " 스탬프됨");
                }
            } else if (tool_ == Tool::Rect) {               // rectangle fill (drag)
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rectDragging_ = true; rectStartX_ = tx; rectStartY_ = ty; }
                if (rectDragging_ && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    rectDragging_ = false;
                    pushUndo();
                    int x0 = std::min(rectStartX_, tx), y0 = std::min(rectStartY_, ty);
                    int x1 = std::max(rectStartX_, tx), y1 = std::max(rectStartY_, ty);
                    for (int yy = y0; yy <= y1; ++yy)
                        for (int xx = x0; xx <= x1; ++xx) {
                            if (collisionMode_) m->tilemap.setBlocked(xx, yy, true);
                            else m->tilemap.setTile(activeLayer_, xx, yy, selectedTile_);
                        }
                    setStatus("사각형 채움");
                }
            } else if (collisionMode_) {                    // collision paint
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) pushUndo();
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))  m->tilemap.setBlocked(tx, ty, true);
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) m->tilemap.setBlocked(tx, ty, false);
            } else if (tool_ == Tool::Fill) {               // bucket fill
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { pushUndo(); m->tilemap.fill(activeLayer_, tx, ty, selectedTile_); }
            } else {                                        // pencil / erase
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) pushUndo();
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))  m->tilemap.setTile(activeLayer_, tx, ty,
                                                            tool_ == Tool::Erase ? -1 : selectedTile_);
                if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) m->tilemap.setTile(activeLayer_, tx, ty, -1);
            }
        }
    }
    EndScissorMode();
}


} // namespace tsukuru
