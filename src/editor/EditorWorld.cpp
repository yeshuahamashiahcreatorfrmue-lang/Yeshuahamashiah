// EditorWorld: map list + map settings.
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

void Editor::drawWorldTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    // ---- left: map list ----
    float lx = 12, ly = kToolbarH + 12, lw = 300;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("맵 목록", (int)lx + 12, (int)ly + 10, 22, ui::kAccent);
    DrawTextU(TextFormat("%d개", (int)p.maps.size()), (int)lx + 120, (int)ly + 16, 16, ui::kTextDim);

    float y = ly + 46;
    for (int i = 0; i < (int)p.maps.size(); ++i) {
        auto& m = p.maps[i];
        bool isStart = (m->id == p.startMap);
        bool isActive = (m->id == activeMapId_);
        Rectangle r = { lx + 10, y, lw - 20, 28 };
        std::string label = (isStart ? "* " : "  ") + m->name + "  (#" + std::to_string(m->id) + ")";
        if (ui::button(r, label, worldSelected_ == i || isActive)) {
            worldSelected_ = i; activeMapId_ = m->id; mapNameFocus_ = false;
        }
        y += 32;
    }

    // new map controls — standardized world size (7 tiers, up to 420x420)
    y += 8;
    ui::label("새 맵 크기 (규격)", (int)lx + 10, (int)y, 14, ui::kTextDim); y += 20;
    if (ui::button({ lx + 10, y, lw - 20, 26 }, kSizeTierNames[newMapTier_]))
        newMapTier_ = (newMapTier_ + 1) % 7;
    y += 34;
    if (ui::button({ lx + 10, y, lw - 20, 30 }, "+ 새 맵")) {
        int side = kSizeTiers[newMapTier_];
        auto nm = p.addMap("Map" + std::to_string(p.nextMapId()), side, side);
        // copy tileset from current map so it is paintable immediately
        if (auto cur = activeMap()) nm->tileset = cur->tileset;
        activeMapId_ = nm->id;
        worldSelected_ = (int)p.maps.size() - 1;
        p.save();
        setStatus(nm->name + " 생성됨");
        tab_ = Tab::Map;
    }

    // ---- right: selected map details ----
    if (worldSelected_ < 0 || worldSelected_ >= (int)p.maps.size())
        worldSelected_ = activeMap() ? 0 : -1;
    if (worldSelected_ < 0) return;
    auto m = p.maps[worldSelected_];

    float dx = lx + lw + 24, dy = ly;
    ui::panel({ dx - 8, ly, area.width - dx - 4, area.height - 24 }, ui::kPanel);
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
        int nt = (tier + 1) % 7;
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

// ---------------------------- CHARS (character assets) ----------------------------

} // namespace tsukuru
