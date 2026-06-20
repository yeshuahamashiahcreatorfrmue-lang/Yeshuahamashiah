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
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    // ---- left: map list ----
    float lx = 12, ly = kToolbarH + 12, lw = 300;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("맵 목록", (int)lx + 12, (int)ly + 10, 22, ui::kAccent);
    DrawTextU(TextFormat("%d개", (int)p.maps.size()), (int)lx + 120, (int)ly + 16, 16, ui::kTextDim);

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

    // scrollable filtered list
    Rectangle listR = { lx + 6, ly + 108, lw - 12, area.height - 24 - (ly + 108 - ly) - 6 };
    float rowH = 32, contentH = shown.size() * rowH + 4;
    BeginScissorMode((int)listR.x, (int)listR.y, (int)listR.width, (int)listR.height);
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
    cy += 40;

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
            p.maps.erase(p.maps.begin() + worldSelected_);
            std::error_code ec;
            fs::remove(fs::path(p.dir) / "maps" / (std::to_string(delId) + ".json"), ec);
            if (p.startMap == delId) p.startMap = p.maps.front()->id;
            if (activeMapId_ == delId) activeMapId_ = p.maps.front()->id;
            worldSelected_ = 0;
            p.save();
            setStatus("맵 삭제됨.");
            return;
        }
    }
    DrawTextU("팁: 이동 이벤트로 맵을 연결하세요 (이벤트 탭).",
             (int)dx, (int)(ly + area.height - 60), 14, ui::kTextDim);
}


} // namespace tsukuru
