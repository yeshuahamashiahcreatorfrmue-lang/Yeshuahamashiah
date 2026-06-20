// EditorMap: tile palette + map canvas painting.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {

void Editor::drawMapTab() {
    float W = (float)screenW(), H = (float)screenH();
    float rightW = npcMode_ ? 324.0f : 0.0f;          // NPC inspector panel width
    Rectangle paletteArea = { 0, kToolbarH, kPaletteW, H - kToolbarH };
    Rectangle canvasArea  = { kPaletteW, kToolbarH, W - kPaletteW - rightW, H - kToolbarH };
    drawMapCanvas(canvasArea);
    if (tool_ == Tool::Stamp && !npcMode_) drawPrefabPalette(paletteArea);
    else drawTilePalette(paletteArea);

    if (npcMode_) {
        Rectangle panel = { W - rightW, kToolbarH, rightW, H - kToolbarH };
        ui::panel(panel, ui::kPanel);
        Event* ev = nullptr;
        if (auto m = activeMap()) for (auto& e : m->events) if (e.id == editingEventId_) ev = &e;
        if (ev && ev->graphicAsset >= 0) drawNpcInspector(*ev, panel);
        else {
            ui::label("NPC 배치", (int)panel.x + 12, (int)panel.y + 10, 22, ui::kAccent);
            DrawTextU("· 빈 칸 클릭 = NPC 추가", (int)panel.x + 12, (int)panel.y + 48, 15, ui::kText);
            DrawTextU("· 기존 NPC 클릭 = 선택/편집", (int)panel.x + 12, (int)panel.y + 70, 15, ui::kText);
            DrawTextU("선택하면 진영·AI·스탯·스프라이트를", (int)panel.x + 12, (int)panel.y + 100, 13, ui::kTextDim);
            DrawTextU("여기서 설정할 수 있습니다.", (int)panel.x + 12, (int)panel.y + 118, 13, ui::kTextDim);
        }
    }
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
                   set.assetId >= 0 ? "타일셋 이미지 변경(순환)" : "타일셋 이미지 선택(순환)")) {
        // cycle to next image asset
        if (!imgs.empty()) {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == set.assetId) idx = i;
            m->tileset.assetId = imgs[(idx + 1) % imgs.size()]->id;
        }
    }
    if (ui::button({ area.x + 10, ty + 50, area.width - 20, 26 }, "+ 타일셋 이미지 불러오기", true))
        pendingTilesetImport_ = true;        // native picker -> assign as this map's tileset
    ty += 82;
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
    uiScissor((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    if (!m) { EndScissorMode(); return; }
    int TS = m->tileset.tileWidth;
    // input region: leave the top zoom-bar strip, the right scrollbar, and the
    // bottom scrollbar clear so clicks there don't paint tiles.
    Rectangle ia = { area.x, area.y + 38, area.width - 14, area.height - 38 - 48 };

    uiBeginWorld(cam_);
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
    // grid — only the visible cells, and skipped when zoomed far out (avoids tens of
    // thousands of off-screen DrawLine calls on huge 1742² maps + grid-noise).
    if (cam_.zoom > 0.25f) {
        for (int x = cx0; x <= cx1 + 1; ++x) DrawLine(x*TS, cy0*TS, x*TS, (cy1+1)*TS, Fade(BLACK, 0.25f));
        for (int y = cy0; y <= cy1 + 1; ++y) DrawLine(cx0*TS, y*TS, (cx1+1)*TS, y*TS, Fade(BLACK, 0.25f));
    }
    // collision overlay
    if (collisionMode_)
        for (int y = cy0; y <= cy1; ++y)
            for (int x = cx0; x <= cx1; ++x)
                if (m->tilemap.blocked(x, y))
                    DrawRectangle(x*TS, y*TS, TS, TS, Fade(ui::kDanger, 0.45f));
    // NPC preview: draw every NPC event's sprite + a faction-coloured frame
    if (npcMode_) {
        for (auto& e : m->events) {
            if (e.graphicAsset < 0) continue;
            const Texture2D& nt = engine_.assetTexture(e.graphicAsset);
            float pct = (e.drawPct > 0 ? e.drawPct : 100) / 100.0f;
            float sw = TS * std::max(1, e.drawTilesW) * pct;
            float sh = TS * std::max(1, e.drawTilesH) * pct;
            float fw = nt.width / 4.0f, fh = nt.height / 4.0f;   // 4-dir sheet, frame 0 facing down
            Rectangle src = { 0, 0, fw, fh };
            Rectangle dst = { e.x*(float)TS + (TS-sw)/2, e.y*(float)TS + (TS-sh), sw, sh };
            DrawTexturePro(nt, src, dst, {0,0}, 0, WHITE);
            Color fc = ui::factionColor((int)e.faction);
            DrawRectangleLinesEx({ (float)e.x*TS, (float)e.y*TS, (float)TS, (float)TS }, 2,
                                 e.id==editingEventId_ ? ui::kAccentHi : fc);
        }
    }
    uiEndWorld();

    // large-map navigation: draggable scrollbars + map-zoom control (screen space)
    drawMapScrollbars(area);
    drawMapZoomBar(area);

    // NPC placement mode: click adds/selects an NPC event (no tile painting)
    if (npcMode_) {
        if (ui::mouseIn(ia) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
            int tx = (int)std::floor(world.x / TS), ty = (int)std::floor(world.y / TS);
            if (m->tilemap.inBounds(tx, ty)) {
                Event* hit = m->eventAt(tx, ty);
                if (hit) editingEventId_ = hit->id;
                else {
                    Event ne; ne.id = m->nextEventId(); ne.x = tx; ne.y = ty;
                    ne.text = "안녕하세요!";
                    auto imgs = engine_.project().assets.byType(AssetType::Image);
                    ne.graphicAsset = imgs.empty() ? -1 : imgs.front()->id;  // a sprite so it shows
                    ne.behavior = NpcBehavior::Idle;
                    m->events.push_back(ne);
                    editingEventId_ = ne.id;
                    setStatus("NPC 추가됨 — 오른쪽에서 설정하세요");
                }
                eventTextFocus_ = false;
            }
        }
        EndScissorMode();
        return;
    }

    // painting
    if (ui::mouseIn(ia) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
        int tx = (int)std::floor(world.x / TS), ty = (int)std::floor(world.y / TS);
        bool inMap = m->tilemap.inBounds(tx, ty);
        bool eyedrop = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);

        // hover highlight + rectangle preview
        uiBeginWorld(cam_);
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
        uiEndWorld();

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

// Map-only zoom control (independent of the global UI scale): − / % / + plus a
// "전체보기" that fits the whole map (so even 1742×1742 is fully visible) and 100%.
void Editor::drawMapZoomBar(Rectangle canvas) {
    auto m = activeMap();
    if (!m) return;
    int TS = m->tileset.tileWidth > 0 ? m->tileset.tileWidth : 32;
    // top-left of the canvas (the bottom is used by scrollbars + the global UI bar)
    float y = canvas.y + 8, x = canvas.x + 8;
    DrawRectangle((int)x - 4, (int)y - 4, 360, 30, Fade(BLACK, 0.6f));
    DrawTextU("맵 배율", (int)x, (int)y + 5, 13, ui::kText);
    if (ui::button({ x + 58, y, 26, 22 }, "-"))   cam_.zoom = std::max(0.02f, cam_.zoom * 0.8f);
    DrawTextU(TextFormat("%d%%", (int)(cam_.zoom * 100 + 0.5f)), (int)x + 90, (int)y + 5, 13, ui::kAccentHi);
    if (ui::button({ x + 148, y, 26, 22 }, "+"))  cam_.zoom = std::min(4.0f, cam_.zoom * 1.25f);
    if (ui::button({ x + 180, y, 80, 22 }, "전체보기")) {            // fit the whole map
        float mw = (float)m->tilemap.width() * TS, mh = (float)m->tilemap.height() * TS;
        float z = std::min((canvas.width - 40) / mw, (canvas.height - 90) / mh);
        cam_.zoom = std::max(0.02f, std::min(4.0f, z));
        cam_.target = { mw / 2, mh / 2 };
    }
    if (ui::button({ x + 264, y, 56, 22 }, "100%")) cam_.zoom = 1.0f;
}

// Draggable horizontal/vertical scrollbars for panning large maps with the mouse.
void Editor::drawMapScrollbars(Rectangle canvas) {
    auto m = activeMap();
    if (!m) return;
    int TS = m->tileset.tileWidth > 0 ? m->tileset.tileWidth : 32;
    float mw = (float)m->tilemap.width() * TS, mh = (float)m->tilemap.height() * TS;
    float visW = canvas.width / cam_.zoom, visH = canvas.height / cam_.zoom;
    Rectangle hTrack = { canvas.x, canvas.y + canvas.height - 44, canvas.width - 16, 9 };
    Rectangle vTrack = { canvas.x + canvas.width - 11, canvas.y, 9, canvas.height - 48 };
    Vector2 mp = GetMousePosition();
    auto bar = [&](Rectangle track, bool horiz, int axis) {
        float total = horiz ? mw : mh, vis = horiz ? visW : visH;
        float tl = horiz ? track.width : track.height;
        if (total <= 1 || tl <= 1) return;
        float frac = std::min(1.0f, vis / total);
        float thumbLen = std::max(24.0f, tl * frac);
        float camMin = (horiz ? cam_.target.x : cam_.target.y) - vis / 2;
        float denom = (total > vis) ? (total - vis) : 1.0f;
        float sf = std::min(1.0f, std::max(0.0f, camMin / denom));
        float pos = (tl - thumbLen) * sf;
        DrawRectangleRec(track, Fade(BLACK, 0.45f));
        Rectangle thumb = horiz ? Rectangle{ track.x + pos, track.y, thumbLen, track.height }
                                : Rectangle{ track.x, track.y + pos, track.width, thumbLen };
        DrawRectangleRec(thumb, scrollDragAxis_ == axis ? ui::kAccentHi : ui::kAccent);
        if (CheckCollisionPointRec(mp, track) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) scrollDragAxis_ = axis;
        if (scrollDragAxis_ == axis && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && total > vis) {
            float m0 = (horiz ? (mp.x - track.x) : (mp.y - track.y)) - thumbLen / 2;
            float nf = std::min(1.0f, std::max(0.0f, m0 / std::max(1.0f, tl - thumbLen)));
            float nMin = nf * (total - vis);
            if (horiz) cam_.target.x = nMin + vis / 2; else cam_.target.y = nMin + vis / 2;
        }
    };
    bar(hTrack, true, 1);
    bar(vTrack, false, 2);
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) scrollDragAxis_ = 0;
}

} // namespace tsukuru
