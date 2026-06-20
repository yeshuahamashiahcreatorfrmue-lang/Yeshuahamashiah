// EditorWorld: map list + map settings.
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

void Editor::drawWorldTab() {
    if (worldPreviewFull_) { drawWorldPreviewOverlay(); return; }  // blocks the tab while open
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    // ---- left: map list ----
    float lx = 12, ly = kToolbarH + 12, lw = 300;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("맵 목록", (int)lx + 12, (int)ly + 10, 22, ui::kAccent);
    DrawTextU(TextFormat("%d개", (int)p.maps.size()), (int)lx + 120, (int)ly + 16, 16, ui::kTextDim);
    DrawTextU(kBuildTag, 12, screenH() - 22, 14, ui::kGood);   // build-confirm tag (bottom-left)

    // search box: filter the list by name (or #id)
    Rectangle sf = { lx + 10, ly + 40, lw - 20, 26 };
    if (ui::mouseIn(sf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) mapSearchFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(sf)) mapSearchFocus_ = false;
    ui::textField(sf, mapSearch_, mapSearchFocus_, 40);
    if (mapSearch_.empty() && !mapSearchFocus_)
        DrawTextU("검색…", (int)sf.x + 8, (int)sf.y + 6, 14, ui::kTextDim);

    // new-map controls (kept above the scrolling list so they never scroll away)
    float ncy = ly + 74;
    if (ui::button({ lx + 10, ncy, (lw-24)/2, 26 }, kSizeTierNames[newMapTier_], false, 13))
        newMapTier_ = (newMapTier_ + 1) % kSizeTierCount;
    if (ui::button({ lx + 10 + (lw-24)/2 + 4, ncy, (lw-24)/2, 26 }, "+ 새 맵")) {
        int side = kSizeTiers[newMapTier_];
        auto nm = p.addMap("Map" + std::to_string(p.nextMapId()), side, side);
        if (auto cur = activeMap()) nm->tileset = cur->tileset;   // paintable immediately
        activeMapId_ = nm->id;
        worldSelected_ = (int)p.maps.size() - 1;
        p.save();
        setStatus(nm->name + " 생성됨");
        tab_ = Tab::Map;
    }

    // case-insensitive substring match on the (lower-cased) name + "#id"
    std::string q = mapSearch_; for (auto& c : q) c = (char)tolower((unsigned char)c);
    auto matches = [&](const std::shared_ptr<Map>& m, int id){
        if (q.empty()) return true;
        std::string hay = m->name + " #" + std::to_string(id);
        for (auto& c : hay) c = (char)tolower((unsigned char)c);
        return hay.find(q) != std::string::npos;
    };
    std::vector<int> shown;
    for (int i = 0; i < (int)p.maps.size(); ++i) if (matches(p.maps[i], p.maps[i]->id)) shown.push_back(i);

    // scrollable filtered list (leave room at the bottom for a live preview)
    const float previewH = 168;
    Rectangle listR = { lx + 6, ly + 108, lw - 12, area.height - 24 - (ly + 108 - ly) - 6 - previewH };
    float rowH = 32, contentH = shown.size() * rowH + 4;
    uiScissor((int)listR.x, (int)listR.y, (int)listR.width, (int)listR.height);
    if (ui::mouseIn(listR)) worldListScroll_ -= GetMouseWheelMove() * 48;
    float maxScroll = std::max(0.0f, contentH - listR.height);
    if (worldListScroll_ < 0) worldListScroll_ = 0;
    if (worldListScroll_ > maxScroll) worldListScroll_ = maxScroll;
    float y = listR.y - worldListScroll_;
    for (int idx : shown) {
        auto& m = p.maps[idx];
        if (y + rowH >= listR.y && y <= listR.y + listR.height) {
            bool isStart = (m->id == p.startMap);
            bool isActive = (m->id == activeMapId_);
            Rectangle r = { listR.x + 4, y, listR.width - 8, 28 };
            std::string label = (isStart ? "* " : "  ") + m->name + "  (#" + std::to_string(m->id) + ")";
            if (ui::button(r, label, worldSelected_ == idx || isActive)) {
                worldSelected_ = idx; activeMapId_ = m->id; mapNameFocus_ = false;
            }
        }
        y += rowH;
    }
    EndScissorMode();
    if (shown.empty())
        DrawTextU("검색 결과 없음", (int)listR.x + 10, (int)listR.y + 8, 15, ui::kTextDim);

    // selected-map preview pinned to the bottom of the left panel (click = fullscreen)
    {
        float pvY = listR.y + listR.height + 6;
        DrawTextU("미리보기 (클릭=전체화면)", (int)lx + 12, (int)pvY, 14, ui::kAccent);
        Rectangle pvBox = { lx + 8, pvY + 20, lw - 16, previewH - 28 };
        DrawRectangleRec(pvBox, Color{ 18, 20, 28, 255 });
        int selIdx = (worldSelected_ >= 0 && worldSelected_ < (int)p.maps.size()) ? worldSelected_ : -1;
        if (selIdx >= 0) {
            if (const RenderTexture2D* th = mapThumb(p.maps[selIdx]->id)) {
                float tw = (float)th->texture.width, tht = (float)th->texture.height;
                float s = std::min(pvBox.width / tw, pvBox.height / tht);
                float pw = tw * s, ph = tht * s;
                float bx = pvBox.x + (pvBox.width - pw) / 2, by = pvBox.y + (pvBox.height - ph) / 2;
                DrawTexturePro(th->texture, { 0,0,tw,-tht }, { bx,by,pw,ph }, {0,0}, 0, WHITE);
            }
            if (ui::mouseIn(pvBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                worldPreviewFull_ = true;             // open the fullscreen overlay
                worldPreviewMapId_ = p.maps[selIdx]->id;
                worldPreviewStack_.clear();
            }
        }
        DrawRectangleLinesEx(pvBox, 1, ui::mouseIn(pvBox) ? ui::kAccentHi : Fade(BLACK, 0.6f));
    }

    // ---- right panel ----
    float dx = lx + lw + 24, dy = ly;
    ui::panel({ dx - 8, ly, area.width - dx - 4, area.height - 24 }, ui::kPanel);

    // map registration: load a whole map .json, preview it, then add it
    if (ui::button({ area.width - 230, ly + 8, 206, 26 }, "+ 맵 파일 등록 (.json)", true))
        pendingMapImport_ = true;

    if (mapPreview_) {
        ui::label("맵 등록 미리보기", (int)dx + 4, (int)dy + 10, 22, ui::kAccent); dy += 44;
        DrawTextU(TextFormat("파일: %s", mapPreviewName_.c_str()), (int)dx, (int)dy, 14, ui::kText); dy += 22;
        DrawTextU(TextFormat("이름: %s   크기: %d×%d   이벤트: %d개",
                  mapPreview_->name.c_str(), mapPreview_->tilemap.width(), mapPreview_->tilemap.height(),
                  (int)mapPreview_->events.size()), (int)dx, (int)dy, 14, ui::kTextDim); dy += 30;
        // thumbnail (best-effort: uses the preview's tileset if present, else the current one)
        const Tileset* ts = &mapPreview_->tileset;
        int tsAsset = p.assets.find(ts->assetId) ? ts->assetId : (activeMap() ? activeMap()->tileset.assetId : -1);
        if (!p.assets.find(ts->assetId) && activeMap()) ts = &activeMap()->tileset;
        Rectangle box = { dx, dy, 360, 270 };
        DrawRectangleRec(box, Color{ 20, 22, 30, 255 });
        if (tsAsset >= 0) {
            const Texture2D& tex = engine_.assetTexture(tsAsset);
            int mw = mapPreview_->tilemap.width(), mh = mapPreview_->tilemap.height();
            float sc = std::min(box.width / mw, box.height / mh);
            for (int layer = 0; layer < kLayerCount; ++layer)
                for (int ty = 0; ty < mh; ++ty)
                    for (int tx = 0; tx < mw; ++tx) {
                        int t = mapPreview_->tilemap.tile(layer, tx, ty);
                        if (t < 0) continue;
                        int sx, sy; ts->srcOf(t, sx, sy);
                        DrawTexturePro(tex, { (float)sx,(float)sy,(float)ts->tileWidth,(float)ts->tileHeight },
                                       { box.x+tx*sc, box.y+ty*sc, sc, sc }, {0,0}, 0, WHITE);
                    }
        }
        DrawRectangleLinesEx(box, 1, Fade(BLACK, 0.5f));
        dy += 280;
        if (ui::button({ dx, dy, 175, 30 }, "이 맵으로 등록", true)) {
            auto nm = mapPreview_;
            nm->id = p.nextMapId();
            if (!p.assets.find(nm->tileset.assetId) && activeMap())
                nm->tileset = activeMap()->tileset;          // give it a valid tileset
            p.maps.push_back(nm);
            activeMapId_ = nm->id; worldSelected_ = (int)p.maps.size() - 1;
            mapPreview_.reset(); p.save();
            setStatus("맵 등록됨: " + nm->name);
            tab_ = Tab::Map;
        }
        if (ui::button({ dx + 185, dy, 120, 30 }, "취소")) { mapPreview_.reset(); setStatus("등록 취소"); }
        return;   // previewing overrides the normal map-settings view
    }

    // ---- right: selected map details ----
    if (worldSelected_ < 0 || worldSelected_ >= (int)p.maps.size())
        worldSelected_ = activeMap() ? 0 : -1;
    if (worldSelected_ < 0) return;
    auto m = p.maps[worldSelected_];
    ui::label("맵 설정", (int)dx + 4, (int)dy + 10, 22, ui::kAccent);
    dy += 46;

    ui::label("이름:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
    Rectangle tf = { dx, dy, 360, 28 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) mapNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) mapNameFocus_ = false;
    ui::textField(tf, m->name, mapNameFocus_, 40);
    dy += 40;

    int w = m->tilemap.width(), h = m->tilemap.height();
    DrawTextU(TextFormat("크기: %d x %d 타일", w, h), (int)dx, (int)dy, 16, ui::kText); dy += 26;
    // standardized size tiers (cycle to resize, content preserved)
    int tier = sizeTierOf(std::max(w, h));
    if (ui::button({ dx, dy, 260, 26 }, kSizeTierNames[tier])) {
        int nt = (tier + 1) % kSizeTierCount;
        int side = kSizeTiers[nt];
        m->tilemap.resizePreserve(side, side);
        setStatus(std::string("맵 크기 변경: ") + kSizeTierNames[nt]);
    }
    dy += 30;
    DrawTextU("위 버튼 클릭 = 다음 규격으로 크기 변경 (내용 보존)", (int)dx, (int)dy, 12, ui::kTextDim); dy += 24;

    ui::intStepper({ dx, dy, 260, 26 }, "조우율%", m->encounterRate, 5, 0, 100); dy += 34;

    // ---- second column: atmosphere / audio / field monsters ----
    float cx = dx + 300, cy = ly + 92;
    ui::label("분위기", (int)cx, (int)cy, 16, ui::kTextDim); cy += 24;
    ui::intStepper({ cx, cy, 250, 26 }, "어둠", m->darkness, 15, 0, 255); cy += 32;
    const char* wx[3] = { "날씨: 없음", "날씨: 비", "날씨: 눈" };
    if (ui::button({ cx, cy, 250, 26 }, wx[m->weather % 3])) m->weather = (m->weather + 1) % 3;
    cy += 32;
    if (ui::button({ cx, cy, 250, 26 }, m->dayNight ? "낮/밤: 켜짐" : "낮/밤: 꺼짐", m->dayNight))
        m->dayNight = !m->dayNight;
    cy += 38;

    ui::label("배경음악 (BGM)", (int)cx, (int)cy, 16, ui::kTextDim); cy += 24;
    auto auds = p.assets.byType(AssetType::Audio);
    const AssetEntry* curB = p.assets.find(m->bgmAsset);
    if (ui::button({ cx, cy, 250, 26 }, std::string("BGM: ") + (curB ? curB->name : "없음"))) {
        int idx = -1;
        for (int i = 0; i < (int)auds.size(); ++i) if (auds[i]->id == m->bgmAsset) idx = i;
        idx++;
        m->bgmAsset = (idx >= (int)auds.size()) ? -1 : auds[idx]->id;
    }
    cy += 30;
    if (ui::button({ cx, cy, 250, 24 }, "BGM 파일 찾아 등록 (외부)", true)) {
        pendingBgmImport_ = true; pendingBgmMapId_ = m->id;
    }
    cy += 34;

    ui::label("필드 몬스터", (int)cx, (int)cy, 16, ui::kTextDim); cy += 24;
    for (auto& en : p.database.enemies) {
        bool on = std::find(m->encounterEnemies.begin(), m->encounterEnemies.end(), en.id) != m->encounterEnemies.end();
        if (ui::button({ cx, cy, 250, 24 }, (on ? "[x] " : "[  ] ") + en.name, on)) {
            if (on) m->encounterEnemies.erase(std::remove(m->encounterEnemies.begin(), m->encounterEnemies.end(), en.id), m->encounterEnemies.end());
            else m->encounterEnemies.push_back(en.id);
        }
        cy += 27;
    }

    if (ui::button({ dx, dy, 220, 30 }, p.startMap == m->id ? "시작 맵 (현재)" : "시작 맵으로 설정",
                   p.startMap == m->id)) {
        p.startMap = m->id; p.startX = w/2; p.startY = h/2; setStatus("시작 맵 설정됨.");
    }
    dy += 38;
    if (ui::button({ dx, dy, 220, 30 }, "이 맵 편집")) { activeMapId_ = m->id; tab_ = Tab::Map; }
    dy += 38;
    if ((int)p.maps.size() > 1) {
        if (ui::button({ dx, dy, 220, 30 }, "맵 삭제", false)) {
            int delId = m->id;
            dropMapThumb(delId);                 // free its cached thumbnail (no GPU leak)
            p.deleteMap(delId);                  // erase map + on-disk .json (no orphan file)
            if (activeMapId_ == delId) activeMapId_ = p.maps.front()->id;
            worldSelected_ = 0;
            p.save();
            setStatus("맵 삭제됨.");
            return;
        }
    }
    DrawTextU("팁: 이동 이벤트로 맵을 연결하세요 (이벤트 탭).",
             (int)dx, (int)(ly + area.height - 58), 14, ui::kTextDim);
}

// ===================== All-Map Viewer (zone grid) =====================
// Lay maps on a grid. Orthogonally-adjacent maps connect at their shared edge,
// so in-game walking off that edge loads the neighbour. Far-apart maps are
// independent. Click a map (list or grid) to select; click an empty cell to drop
// the selected map there.
void Editor::drawWorldViewTab() {
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
    DrawRectangleRec(area, Color{ 20, 22, 30, 255 });
    Project& p = engine_.project();
    const float cell = 180.0f;

    // ---- left panel: map list + actions ----
    float lx = 12, ly = kToolbarH + 12, lw = 270;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("전맵 뷰어", (int)lx + 12, (int)ly + 10, 22, ui::kAccent);
    DrawTextU("맵 선택→빈칸 좌클릭=배치, 다시=복제", (int)lx + 12, (int)ly + 40, 12, ui::kTextDim);
    DrawTextU("우클릭=제거 · 붙은 맵끼리 가장자리 이동", (int)lx + 12, (int)ly + 56, 12, ui::kTextDim);
    float yb = ly + 80;
    if (worldViewSel_ >= 0 && worldViewSel_ < (int)p.maps.size()) {
        auto& sm = p.maps[worldViewSel_];
        if (ui::button({ lx + 10, yb, lw - 20, 26 }, sm->placed ? "배치 해제(빼기)" : "선택됨 — 격자 클릭")) {
            if (sm->placed) { sm->placed = false; p.save(); setStatus(sm->name + " 배치 해제"); }
        }
    }
    yb += 32;
    Rectangle listR = { lx + 6, yb, lw - 12, area.height - 24 - (yb - ly) - 6 };
    uiScissor((int)listR.x, (int)listR.y, (int)listR.width, (int)listR.height);
    if (ui::mouseIn(listR)) worldListScroll_ -= GetMouseWheelMove() * 48;
    float contentH = p.maps.size() * 30.0f + 4;
    float maxS = std::max(0.0f, contentH - listR.height);
    worldListScroll_ = std::max(0.0f, std::min(worldListScroll_, maxS));
    float ry = listR.y - worldListScroll_;
    for (int i = 0; i < (int)p.maps.size(); ++i) {
        auto& m = p.maps[i];
        if (ry + 28 >= listR.y && ry <= listR.y + listR.height) {
            std::string lbl = (m->placed ? "[배치] " : "  ") + m->name;
            if (ui::button({ listR.x + 4, ry, listR.width - 8, 26 }, lbl, worldViewSel_ == i))
                worldViewSel_ = i;
        }
        ry += 30;
    }
    EndScissorMode();

    // ---- right: zone grid canvas ----
    Rectangle canvas = { lx + lw + 12, ly, area.width - (lx + lw + 12) - 12, area.height - 24 };
    uiScissor((int)canvas.x, (int)canvas.y, (int)canvas.width, (int)canvas.height);
    DrawRectangleRec(canvas, Color{ 26, 28, 38, 255 });
    if (!worldCamInit_) { worldCam_.zoom = 0.7f; worldCam_.offset = { canvas.x + canvas.width/2, canvas.y + canvas.height/2 }; worldCam_.target = { cell, cell }; worldCamInit_ = true; }
    worldCam_.offset = { canvas.x + canvas.width/2, canvas.y + canvas.height/2 };
    if (ui::mouseIn(canvas)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) worldCam_.zoom = std::max(0.2f, std::min(2.0f, worldCam_.zoom + wheel*0.1f));
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) { Vector2 dd = GetMouseDelta(); worldCam_.target.x -= dd.x/worldCam_.zoom; worldCam_.target.y -= dd.y/worldCam_.zoom; }
    }

    uiBeginWorld(worldCam_);
    // grid lines around the visible area
    Vector2 tl = GetScreenToWorld2D({ canvas.x, canvas.y }, worldCam_);
    Vector2 br = GetScreenToWorld2D({ canvas.x+canvas.width, canvas.y+canvas.height }, worldCam_);
    int gx0 = (int)std::floor(tl.x/cell)-1, gy0 = (int)std::floor(tl.y/cell)-1;
    int gx1 = (int)std::floor(br.x/cell)+1, gy1 = (int)std::floor(br.y/cell)+1;
    for (int gx = gx0; gx <= gx1; ++gx) DrawLine((int)(gx*cell), (int)(gy0*cell), (int)(gx*cell), (int)(gy1*cell), Fade(BLACK,0.4f));
    for (int gy = gy0; gy <= gy1; ++gy) DrawLine((int)(gx0*cell), (int)(gy*cell), (int)(gx1*cell), (int)(gy*cell), Fade(BLACK,0.4f));
    // connectors between adjacent placed maps (green = walkable edge)
    for (auto& m : p.maps) {
        if (!m->placed) continue;
        if (p.mapAtWorld(m->worldX+1, m->worldY))   // right neighbour: green edge marker
            DrawRectangle((int)((m->worldX+1)*cell-4), (int)(m->worldY*cell+cell/2-10), 8, 20, ui::kGood);
        if (p.mapAtWorld(m->worldX, m->worldY+1))   // bottom neighbour
            DrawRectangle((int)(m->worldX*cell+cell/2-10), (int)((m->worldY+1)*cell-4), 20, 8, ui::kGood);
    }
    // map boxes
    for (int i = 0; i < (int)p.maps.size(); ++i) {
        auto& m = p.maps[i];
        if (!m->placed) continue;
        Rectangle box = { m->worldX*cell+6, m->worldY*cell+6, cell-12, cell-12 };
        if (const RenderTexture2D* th = mapThumb(m->id)) {     // real map thumbnail
            DrawRectangleRec(box, Color{ 20, 22, 30, 255 });
            DrawTexturePro(th->texture, { 0,0,(float)th->texture.width,-(float)th->texture.height }, box, {0,0}, 0, WHITE);
        } else {
            Color fill = { (unsigned char)(60+(m->id*53)%120), (unsigned char)(70+(m->id*97)%110), (unsigned char)(90+(m->id*29)%120), 255 };
            DrawRectangleRec(box, fill);
        }
        DrawRectangleLinesEx(box, worldViewSel_==i ? 3 : 1, worldViewSel_==i ? ui::kAccentHi : Fade(BLACK,0.6f));
    }
    uiEndWorld();

    // labels in screen space (so text stays crisp)
    for (auto& m : p.maps) {
        if (!m->placed) continue;
        Vector2 sp = GetWorldToScreen2D({ m->worldX*cell+12, m->worldY*cell+12 }, worldCam_);
        DrawTextU(m->name.c_str(), (int)sp.x, (int)sp.y, 14, WHITE);
        DrawTextU(TextFormat("#%d  (%d,%d)", m->id, m->worldX, m->worldY), (int)sp.x, (int)sp.y+18, 11, Fade(WHITE,0.8f));
    }

    // drag ghost: while dragging a map, highlight the target cell
    Vector2 mw = GetScreenToWorld2D(GetMousePosition(), worldCam_);
    int mcx = (int)std::floor(mw.x/cell), mcy = (int)std::floor(mw.y/cell);
    if (wvDragging_) {
        uiBeginWorld(worldCam_);
        DrawRectangleLinesEx({ mcx*cell+4, mcy*cell+4, cell-8, cell-8 }, 4, ui::kAccentHi);
        uiEndWorld();
    }

    // left mouse: PRESS on a map = select + arm drag; PRESS on empty = place/duplicate.
    bool overCanvas = ui::mouseIn(canvas) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON);
    if (overCanvas && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        auto occ = p.mapAtWorld(mcx, mcy);
        if (occ) {                                    // arm a drag on the pressed map
            for (int i = 0; i < (int)p.maps.size(); ++i) if (p.maps[i] == occ) worldViewSel_ = i;
            wvDragId_ = occ->id; wvDragStart_ = GetMousePosition(); wvDragging_ = false;
        } else if (worldViewSel_ >= 0 && worldViewSel_ < (int)p.maps.size()) {
            auto sm = p.maps[worldViewSel_];
            if (!sm->placed) {                        // first instance: place the map itself
                sm->worldX = mcx; sm->worldY = mcy; sm->placed = true; p.save();
                setStatus(TextFormat("%s 배치: (%d,%d)", sm->name.c_str(), mcx, mcy));
            } else {                                  // already placed: drop a numbered copy
                auto nm = std::make_shared<Map>(*sm);
                nm->id = p.nextMapId();
                std::string stem = sm->name;          // strip an existing " (N)" suffix
                auto pos = stem.rfind(" (");
                if (pos != std::string::npos && !stem.empty() && stem.back() == ')') stem = stem.substr(0, pos);
                auto taken = [&](const std::string& s){ for (auto& mm : p.maps) if (mm->name == s) return true; return false; };
                std::string cand; int n = 2;
                do { cand = stem + " (" + std::to_string(n) + ")"; ++n; } while (taken(cand));
                nm->name = cand;
                nm->worldX = mcx; nm->worldY = mcy; nm->placed = true; nm->viewerCopy = true;
                p.maps.push_back(nm); p.save();
                setStatus(TextFormat("%s 복제 배치: (%d,%d)", nm->name.c_str(), mcx, mcy));
            }
        }
    }
    // arm -> drag once the cursor moves past a small threshold
    if (wvDragId_ >= 0 && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float dx = GetMousePosition().x - wvDragStart_.x, dy = GetMousePosition().y - wvDragStart_.y;
        if (dx*dx + dy*dy > 36) wvDragging_ = true;
    }
    // release: drop the dragged map on the target cell (move), or swap with the map there
    if (wvDragId_ >= 0 && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (wvDragging_) {
            auto src = p.map(wvDragId_);
            auto dstOcc = p.mapAtWorld(mcx, mcy);
            if (src && (!dstOcc || dstOcc == src)) {
                src->worldX = mcx; src->worldY = mcy; p.save();
                setStatus(TextFormat("%s 이동: (%d,%d)", src->name.c_str(), mcx, mcy));
            } else if (src && dstOcc) {
                int ox = src->worldX, oy = src->worldY;
                src->worldX = dstOcc->worldX; src->worldY = dstOcc->worldY;
                dstOcc->worldX = ox; dstOcc->worldY = oy; p.save();
                setStatus("맵 위치 교환");
            }
        }
        wvDragId_ = -1; wvDragging_ = false;
    }
    // right-click: remove the map under the cursor. A viewer-made copy is deleted
    // outright (map + .json + thumbnail) so repeated add/remove leaves no residue;
    // an original map is only un-placed (kept in the World list).
    if (ui::mouseIn(canvas) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        Vector2 w = GetScreenToWorld2D(GetMousePosition(), worldCam_);
        int cx = (int)std::floor(w.x/cell), cy = (int)std::floor(w.y/cell);
        if (auto occ = p.mapAtWorld(cx, cy)) {
            std::string nm = occ->name;
            if (occ->viewerCopy && (int)p.maps.size() > 1) {
                int delId = occ->id;
                dropMapThumb(delId);
                if (activeMapId_ == delId) activeMapId_ = -1;
                p.deleteMap(delId);
                worldViewSel_ = -1;
                p.save();
                setStatus(nm + " 복제본 삭제");
            } else {
                occ->placed = false; p.save();
                setStatus(nm + " 배치 제거");
            }
        }
    }
    EndScissorMode();
    DrawTextU("맵 드래그=이동/교환 · 좌클릭=배치/복제 · 우클릭=제거 · 휠=확대/축소 · 가운데드래그=화면이동", (int)canvas.x+10, (int)(canvas.y+canvas.height-24), 12, ui::kTextDim);
    DrawTextU(kBuildTag, 12, screenH()-22, 13, ui::kGood);
}

// ---- map thumbnails (rendered once into cached RenderTextures) ----
// Built in Editor::update() (outside the engine's frame render texture, so the
// nested BeginTextureMode is valid) and drawn on the World / All-Map Viewer tabs.
RenderTexture2D Editor::makeMapThumb(Map& m, float maxW, float maxH) {
    int mw = m.tilemap.width(), mh = m.tilemap.height();
    int TS = m.tileset.tileWidth > 0 ? m.tileset.tileWidth : 32;
    if (mw <= 0 || mh <= 0) return RenderTexture2D{};
    float sc = std::min(maxW / (mw*TS), maxH / (mh*TS));
    int tw = std::max(1, (int)(mw*TS*sc)), th = std::max(1, (int)(mh*TS*sc));
    RenderTexture2D rt = LoadRenderTexture(tw, th);
    SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
    BeginTextureMode(rt);
    ClearBackground(Color{ 24, 26, 34, 255 });
    const Tileset& set = m.tileset;
    if (set.assetId >= 0) {
        const Texture2D& tex = engine_.assetTexture(set.assetId);
        for (int layer = 0; layer < kLayerCount; ++layer)
            for (int y = 0; y < mh; ++y)
                for (int x = 0; x < mw; ++x) {
                    int t = m.tilemap.tile(layer, x, y);
                    if (t < 0) continue;
                    int sx, sy; set.srcOf(t, sx, sy);
                    DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                                   { x*TS*sc, y*TS*sc, TS*sc, TS*sc }, {0,0}, 0, WHITE);
                }
    }
    EndTextureMode();
    return rt;
}

void Editor::buildMapThumb(Map& m) {
    if (m.tilemap.width() <= 0 || m.tilemap.height() <= 0) return;
    RenderTexture2D rt = makeMapThumb(m, 220, 165);
    if (!rt.id) return;
    auto it = mapThumbs_.find(m.id);
    if (it != mapThumbs_.end()) UnloadRenderTexture(it->second);
    mapThumbs_[m.id] = rt;
}

// Fullscreen map preview overlay (opened by clicking the World-tab thumbnail).
// Shows the map plus markers for NPCs / events / mob spawns / building entrances;
// clicking an entrance (텔레포트) dives into that interior map — recursively, with
// the same markers. Click the dark margin, X, or ESC to close; ← 뒤로 steps back.
void Editor::drawWorldPreviewOverlay() {
    Project& p = engine_.project();
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Color{ 10, 11, 16, 248 });
    auto pm = p.map(worldPreviewMapId_);
    DrawTextU(TextFormat("미리보기: %s   (빈 곳·X·ESC = 닫기)", pm ? pm->name.c_str() : "맵"),
              20, (int)kToolbarH + 10, 18, ui::kAccent);

    Rectangle imgR = { 0, 0, 0, 0 };
    int gotoMap = -1;
    if (worldBigThumb_.id && pm && worldBigId_ == worldPreviewMapId_) {
        float tw = (float)worldBigThumb_.texture.width, th = (float)worldBigThumb_.texture.height;
        float maxW = sw - 60.0f, maxH = sh - kToolbarH - 120.0f;
        float s = std::min(maxW / tw, maxH / th);
        float pw = tw * s, ph = th * s;
        float bx = (sw - pw) / 2, by = kToolbarH + 50 + (sh - (kToolbarH + 50) - ph - 40) / 2;
        imgR = { bx, by, pw, ph };
        DrawRectangle((int)bx-3, (int)by-3, (int)pw+6, (int)ph+6, Color{ 20, 22, 30, 255 });
        DrawTexturePro(worldBigThumb_.texture, { 0,0,tw,-th }, imgR, {0,0}, 0, WHITE);
        DrawRectangleLinesEx({ bx-3,by-3,pw+6,ph+6 }, 1, Fade(BLACK, 0.6f));

        // ---- markers (events: NPCs / entrances / mob spawns / generic) ----
        int mw = pm->tilemap.width(), mh = pm->tilemap.height();
        if (mw > 0 && mh > 0) {
            float r = std::max(5.0f, std::min(pw/mw, ph/mh) * 0.5f);
            Vector2 mouse = GetMousePosition();
            for (auto& e : pm->events) {
                Vector2 sp = { bx + (e.x + 0.5f)/mw*pw, by + (e.y + 0.5f)/mh*ph };
                bool entrance = (e.type == EventType::Teleport && e.targetMap >= 0);
                Color col;
                if (entrance)                         col = ui::kGood;                       // 문(입구)
                else if (e.type == EventType::StartBattle) col = ui::kDanger;                 // 몹 스폰
                else if (e.graphicAsset >= 0)         col = ui::factionColor((int)e.faction); // NPC
                else                                   col = Color{240,210,80,255};            // 이벤트
                DrawCircleV(sp, r + 2, Fade(BLACK, 0.7f));
                DrawCircleV(sp, r, col);
                if (entrance) {
                    DrawRectangleLinesEx({ sp.x-r-3, sp.y-r-3, (r+3)*2, (r+3)*2 }, 2, WHITE);
                    bool hov = CheckCollisionPointCircle(mouse, sp, r + 5);
                    if (hov) {
                        auto tgt = p.map(e.targetMap);
                        DrawTextU(tgt ? tgt->name.c_str() : "내부", (int)sp.x + 8, (int)sp.y - 8, 14, WHITE);
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) gotoMap = e.targetMap;
                    }
                }
            }
        }
        // ---- legend ----
        float ly = by + ph + 10, lx = bx;
        auto chip = [&](Color c, const char* t){ DrawCircle((int)lx+6,(int)ly+8,6,c); DrawTextU(t,(int)lx+16,(int)ly+1,13,ui::kText); lx += 18 + MeasureTextU(t,13) + 16; };
        chip(ui::kGood, "문/입구(클릭=내부)"); chip(ui::factionColor(0), "NPC");
        chip(ui::factionColor(1), "아군"); chip(ui::factionColor(2), "적/몹");
        chip(Color{240,210,80,255}, "이벤트");
    } else {
        DrawTextU("미리보기 생성 중…", sw/2 - 70, sh/2, 18, ui::kTextDim);
    }

    // ---- top-right controls (below the toolbar so they're visible) ----
    bool back = false, close = false;
    if (!worldPreviewStack_.empty())
        back = ui::button({ (float)sw - 168, (float)kToolbarH + 8, 100, 30 }, "← 뒤로");
    close = ui::button({ (float)sw - 56, (float)kToolbarH + 8, 46, 32 }, "X");

    // click on the dark margin (outside the image & the control strip) closes
    Rectangle ctlZone = { (float)sw - 180, (float)kToolbarH, 180, 50 };
    Vector2 mp = GetMousePosition();
    bool clickedEmpty = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && mp.y > kToolbarH
                      && !CheckCollisionPointRec(mp, imgR)
                      && !CheckCollisionPointRec(mp, ctlZone);

    if (gotoMap >= 0) {                        // enter a building interior
        worldPreviewStack_.push_back(worldPreviewMapId_);
        worldPreviewMapId_ = gotoMap;
    } else if (back) {                         // step back out
        worldPreviewMapId_ = worldPreviewStack_.back();
        worldPreviewStack_.pop_back();
    } else if (close || clickedEmpty || IsKeyPressed(KEY_ESCAPE)) {
        worldPreviewFull_ = false;
        worldPreviewStack_.clear();
    }
}

const RenderTexture2D* Editor::mapThumb(int mapId) {
    auto it = mapThumbs_.find(mapId);
    return it == mapThumbs_.end() ? nullptr : &it->second;
}

void Editor::dropMapThumb(int mapId) {
    auto it = mapThumbs_.find(mapId);
    if (it != mapThumbs_.end()) { UnloadRenderTexture(it->second); mapThumbs_.erase(it); }
}

void Editor::clearMapThumbs() {
    for (auto& kv : mapThumbs_) UnloadRenderTexture(kv.second);
    mapThumbs_.clear();
}

void Editor::ensureThumbsForTab() {
    // refresh thumbnails whenever we (re)enter a tab that shows them, so map
    // edits are reflected; then lazily build the ones actually needed.
    if (tab_ != prevTab_) {
        if (tab_ == Tab::World || tab_ == Tab::WorldView) clearMapThumbs();
        if (tab_ != Tab::World) { worldPreviewFull_ = false; worldPreviewStack_.clear(); }
        prevTab_ = tab_;
    }
    Project& p = engine_.project();
    if (tab_ == Tab::WorldView) {
        for (auto& m : p.maps) if (m->placed && !mapThumb(m->id)) buildMapThumb(*m);
    } else if (tab_ == Tab::World) {
        if (worldSelected_ >= 0 && worldSelected_ < (int)p.maps.size()) {
            auto& m = p.maps[worldSelected_];
            if (!mapThumb(m->id)) buildMapThumb(*m);
        }
        // hi-res texture for the fullscreen preview (rebuild when the shown map changes,
        // including when diving into a building interior)
        if (worldPreviewFull_ && worldPreviewMapId_ >= 0 && worldBigId_ != worldPreviewMapId_) {
            if (auto pm = p.map(worldPreviewMapId_)) {
                if (worldBigThumb_.id) UnloadRenderTexture(worldBigThumb_);
                worldBigThumb_ = makeMapThumb(*pm, 1280, 860);
                worldBigId_ = worldPreviewMapId_;
            }
        }
    }
    // release the big preview texture when the overlay is closed / off the tab
    if ((!worldPreviewFull_ || tab_ != Tab::World) && worldBigId_ >= 0) {
        if (worldBigThumb_.id) UnloadRenderTexture(worldBigThumb_);
        worldBigThumb_ = RenderTexture2D{}; worldBigId_ = -1;
    }
}

} // namespace tsukuru
