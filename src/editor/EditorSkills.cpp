// EditorSkills: field-skill / tile-pattern designer.
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

void Editor::drawSkillsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;

    // ---- left: skill list ----
    float lx = 12, ly = kToolbarH + 12, lw = 250;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("스킬 목록", (int)lx + 12, (int)ly + 10, 20, ui::kAccent);
    float y = ly + 44;
    if (ui::button({ lx + 10, y, lw - 20, 28 }, "+ 새 스킬")) {
        FieldSkill s; s.id = (int)db.fieldSkills.size() + 1;
        s.name = "새 스킬"; s.slot = -1;
        db.fieldSkills.push_back(s); skillSel_ = (int)db.fieldSkills.size() - 1; p.save();
    }
    y += 32;
    if (db.fieldSkills.empty()) {
        if (ui::button({ lx + 10, y, lw - 20, 28 }, "기본 스킬 4종 불러오기")) {
            db.fieldSkills = Database::defaultFieldSkills(); skillSel_ = 0; p.save();
        }
        y += 32;
    }
    static const char* keyName[6] = { "Z","X","C","V","F","G" };
    for (int i = 0; i < (int)db.fieldSkills.size(); ++i) {
        const FieldSkill& s = db.fieldSkills[i];
        std::string lbl = (s.slot >= 0 && s.slot < 6 ? std::string("[")+keyName[s.slot]+"] " : "[-] ") + s.name;
        if (ui::button({ lx + 10, y, lw - 20, 26 }, lbl, skillSel_ == i)) { skillSel_ = i; skillNameFocus_ = false; }
        y += 28;
    }

    if (skillSel_ < 0 && !db.fieldSkills.empty()) skillSel_ = 0; // auto-select first
    if (skillSel_ < 0 || skillSel_ >= (int)db.fieldSkills.size()) {
        ui::label("스킬을 선택하거나 추가하세요.", (int)lx + lw + 30, (int)ly + 20, 16, ui::kTextDim);
        return;
    }
    FieldSkill& s = db.fieldSkills[skillSel_];

    // ---- middle: player-relative tile pattern designer ----
    float gx = lx + lw + 24, gy = ly + 8;
    ui::label("효과 적용 타일 (플레이어 기준, 위=정면)", (int)gx, (int)gy, 16, ui::kAccent);
    gy += 26;
    const int GRID = 9, HALF = GRID/2; // player at center; canonical facing = up
    float cs = 34;
    // upward "front" marker (drawn triangle so it never depends on a glyph)
    DrawTriangle({ gx + HALF*cs + cs/2, gy }, { gx + HALF*cs + cs/2 - 7, gy + 12 },
                 { gx + HALF*cs + cs/2 + 7, gy + 12 }, ui::kGood);
    DrawTextU("정면", (int)(gx + HALF*cs + cs/2 + 12), (int)gy, 13, ui::kGood);
    gy += 16;
    for (int ry = 0; ry < GRID; ++ry) {
        for (int rx = 0; rx < GRID; ++rx) {
            int ox = rx - HALF, oy = ry - HALF;     // offset relative to player
            Rectangle cell = { gx + rx*cs, gy + ry*cs, cs-2, cs-2 };
            bool isPlayer = (ox == 0 && oy == 0);
            bool on = false;
            for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) { on = true; break; }
            Color c = isPlayer ? ui::kAccent : (on ? Color{210,120,90,255} : ui::kPanelHi);
            DrawRectangleRec(cell, c);
            DrawRectangleLinesEx(cell, 1, Fade(BLACK,0.5f));
            if (isPlayer) DrawTextU("P", (int)cell.x+11, (int)cell.y+8, 18, BLACK);
            if (!isPlayer && ui::mouseIn(cell) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (on) { // remove
                    for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) {
                        s.patX.erase(s.patX.begin()+k); s.patY.erase(s.patY.begin()+k); break; }
                } else { s.patX.push_back(ox); s.patY.push_back(oy); }
            }
        }
    }
    float gridBottom = gy + GRID*cs + 8;
    DrawTextU("칸 클릭 = 적용 타일 켜기/끄기 (방향 회전)",
             (int)gx, (int)gridBottom, 12, ui::kTextDim);
    DrawTextU(TextFormat("선택된 타일: %d개", (int)s.patX.size()), (int)gx, (int)gridBottom + 18, 13, ui::kText);

    // ---- right: parameters ----
    float dx = gx + GRID*cs + 30, dy = ly + 8, dw = area.width - dx - 16;
    if (dw < 240) { dx = gx; dy = gridBottom + 44; dw = 300; } // wrap on narrow screens
    ui::panel({ dx - 8, dy - 6, dw + 12, 430 }, ui::kPanel);
    ui::label("스킬 설정", (int)dx, (int)dy, 18, ui::kAccent); dy += 30;

    ui::label("이름:", (int)dx, (int)dy, 13, ui::kTextDim); dy += 18;
    Rectangle nf = { dx, dy, std::min(280.0f, dw), 26 };
    if (ui::mouseIn(nf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) skillNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(nf)) skillNameFocus_ = false;
    ui::textField(nf, s.name, skillNameFocus_, 24); dy += 34;

    // key binding (slot): -1..5
    static const char* slotLbl[7] = { "없음","Z","X","C","V","F","G" };
    if (ui::button({ dx, dy, 260, 26 }, std::string("단축키: ") + slotLbl[s.slot+1]))
        s.slot = (s.slot + 2) % 7 - 1;       // cycle -1..5
    dy += 32;
    if (ui::button({ dx, dy, 260, 26 }, s.projectile ? "발사체: 예 (전방 직선)" : "발사체: 아니오"))
        s.projectile = !s.projectile;
    dy += 30;
    ui::intStepper({ dx, dy, 260, 24 }, "사거리(발사체)", s.range, 1, 1, 20); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "순간이동 칸", s.blink, 1, 0, 10); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "위력(%ATK)", s.powerPct, 10, 0, 1000); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "MP 소모", s.mpCost, 1, 0, 99); dy += 28;
    int cdTenths = (int)(s.cooldown * 10 + 0.5f);
    if (ui::intStepper({ dx, dy, 260, 24 }, "쿨다운(0.1초)", cdTenths, 1, 1, 200)) s.cooldown = cdTenths / 10.0f;
    dy += 32;

    // effect + sound asset assignment (cycle through registered assets)
    auto imgName = [&](int id){ const AssetEntry* e = p.assets.find(id); return e ? e->name : std::string("없음"); };
    auto cycle = [&](int& slot, AssetType t){
        auto list = p.assets.byType(t);
        int idx = -1; for (int i=0;i<(int)list.size();++i) if (list[i]->id==slot) idx=i;
        idx++; slot = (idx >= (int)list.size()) ? -1 : list[idx]->id;
    };
    if (ui::button({ dx, dy, 260, 26 }, std::string("이펙트: ") + imgName(s.effectAsset), s.effectAsset>=0))
        cycle(s.effectAsset, AssetType::Image);
    dy += 30;
    if (ui::button({ dx, dy, 260, 26 }, std::string("사운드: ") + imgName(s.soundAsset), s.soundAsset>=0))
        cycle(s.soundAsset, AssetType::Audio);
    dy += 34;

    if (ui::button({ dx, dy, 125, 28 }, "저장")) { p.save(); setStatus("스킬 저장됨."); }
    if (ui::button({ dx + 135, dy, 125, 28 }, "삭제", false)) {
        db.fieldSkills.erase(db.fieldSkills.begin() + skillSel_);
        skillSel_ = -1; p.save(); setStatus("스킬 삭제됨.");
    }
}

} // namespace tsukuru
