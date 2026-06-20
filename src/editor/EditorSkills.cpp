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

// Fill a skill's hit pattern from a shape preset (canonical facing = up).
// shape: 0 정면, 1 직선, 2 십자, 3 부채꼴, 4 원형, 5 주변(3x3).
static void applyShape(FieldSkill& s, int shape, int size) {
    s.patX.clear(); s.patY.clear();
    auto add = [&](int x, int y){ s.patX.push_back(x); s.patY.push_back(y); };
    if (size < 1) size = 1;
    switch (shape) {
        case 0: add(0,0); add(0,-1); break;                       // front
        case 1: add(0,0); for (int i=1;i<=size;++i) add(0,-i); break; // line forward
        case 2: add(0,0); add(0,-1); add(0,1); add(-1,0); add(1,0); break; // cross
        case 3: add(0,-1); add(-1,-1); add(1,-1); add(0,-2); add(-1,-2); add(1,-2); break; // cone
        case 4: for (int y=-size;y<=size;++y) for (int x=-size;x<=size;++x) add(x,y); break; // circle r
        default: for (int y=-1;y<=1;++y) for (int x=-1;x<=1;++x) add(x,y); break; // 3x3
    }
}

void Editor::drawSkillsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;

    // ---- left: skill list ----
    float lx = 12, ly = kToolbarH + 12, lw = 230;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("스킬 목록", (int)lx + 12, (int)ly + 10, 20, ui::kAccent);
    float y = ly + 44;
    if (ui::button({ lx + 10, y, lw - 20, 28 }, "+ 새 스킬")) {
        FieldSkill s; s.id = (int)db.fieldSkills.size() + 1;
        s.name = "새 스킬"; s.slot = -1; applyShape(s, 0, 1);
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

    // ---- middle: tile pattern designer + shape presets ----
    float gx = lx + lw + 22, gy = ly + 8;
    ui::label("효과 적용 범위 (플레이어 기준, 위=정면)", (int)gx, (int)gy, 16, ui::kAccent);
    gy += 24;
    // quick shape presets — fill the pattern for the user (manual edit still works)
    ui::label("범위 프리셋:", (int)gx, (int)gy, 13, ui::kTextDim); gy += 18;
    static const char* shapeName[6] = { "정면","직선","십자","부채꼴","원형","주변" };
    for (int i = 0; i < 6; ++i)
        if (ui::button({ gx + i*45, gy, 43, 24 }, shapeName[i]))
            applyShape(s, i, skillPatSize_);
    gy += 28;
    ui::intStepper({ gx, gy, 200, 24 }, "범위/사거리", skillPatSize_, 1, 1, 4); gy += 28;

    const int GRID = 9, HALF = GRID/2; // player at center; canonical facing = up
    float cs = 30;
    DrawTriangle({ gx + HALF*cs + cs/2, gy }, { gx + HALF*cs + cs/2 - 7, gy + 11 },
                 { gx + HALF*cs + cs/2 + 7, gy + 11 }, ui::kGood);
    DrawTextU("정면", (int)(gx + HALF*cs + cs/2 + 12), (int)gy, 12, ui::kGood);
    gy += 14;
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
            if (isPlayer) DrawTextU("P", (int)cell.x+9, (int)cell.y+6, 16, BLACK);
            if (!isPlayer && ui::mouseIn(cell) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (on) {
                    for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) {
                        s.patX.erase(s.patX.begin()+k); s.patY.erase(s.patY.begin()+k); break; }
                } else { s.patX.push_back(ox); s.patY.push_back(oy); }
            }
        }
    }
    float gridBottom = gy + GRID*cs + 6;
    DrawTextU(TextFormat("칸 클릭=수동 편집 · 적용 타일 %d개 (시전 시 방향 회전)", (int)s.patX.size()),
             (int)gx, (int)gridBottom, 12, ui::kTextDim);

    // ---- right: parameters + one-click effect/sound creation ----
    float dx = gx + GRID*cs + 28, dy = ly + 8, dw = area.width - dx - 16;
    if (dw < 250) { dx = gx; dy = gridBottom + 30; dw = 320; } // wrap on narrow screens
    ui::panel({ dx - 8, dy - 6, dw + 12, 468 }, ui::kPanel);
    ui::label("스킬 설정", (int)dx, (int)dy, 18, ui::kAccent); dy += 28;

    ui::label("이름:", (int)dx, (int)dy, 13, ui::kTextDim); dy += 18;
    Rectangle nf = { dx, dy, std::min(280.0f, dw), 26 };
    if (ui::mouseIn(nf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) skillNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(nf)) skillNameFocus_ = false;
    ui::textField(nf, s.name, skillNameFocus_, 24); dy += 32;

    static const char* slotLbl[7] = { "없음","Z","X","C","V","F","G" };
    if (ui::button({ dx, dy, 260, 26 }, std::string("단축키: ") + slotLbl[s.slot+1]))
        s.slot = (s.slot + 2) % 7 - 1;       // cycle -1..5
    dy += 30;
    if (ui::button({ dx, dy, 260, 26 }, s.projectile ? "유형: 발사체(전방 직선)" : "유형: 범위(타일 패턴)"))
        s.projectile = !s.projectile;
    dy += 30;
    ui::intStepper({ dx, dy, 260, 24 }, "사거리(발사체)", s.range, 1, 1, 20); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "순간이동 칸", s.blink, 1, 0, 10); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "위력(%ATK)", s.powerPct, 10, 0, 1000); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "MP 소모", s.mpCost, 1, 0, 99); dy += 27;
    int cdTenths = (int)(s.cooldown * 10 + 0.5f);
    if (ui::intStepper({ dx, dy, 260, 24 }, "쿨다운(0.1초)", cdTenths, 1, 1, 200)) s.cooldown = cdTenths / 10.0f;
    dy += 32;

    auto name = [&](int id){ const AssetEntry* e = p.assets.find(id); return e ? e->name : std::string("없음"); };
    auto cycle = [&](int& slot, AssetType t){
        auto list = p.assets.byType(t);
        int idx = -1; for (int i=0;i<(int)list.size();++i) if (list[i]->id==slot) idx=i;
        idx++; slot = (idx >= (int)list.size()) ? -1 : list[idx]->id;
    };
    // effect: assign existing + generate new in one place
    if (ui::button({ dx, dy, 260, 24 }, std::string("이펙트: ") + name(s.effectAsset), s.effectAsset>=0))
        cycle(s.effectAsset, AssetType::Image);
    dy += 26;
    DrawTextU("이펙트 생성:", (int)dx, (int)dy+4, 12, ui::kTextDim);
    static const char* fxName[4] = { "베기","볼트","대시","폭발" };
    for (int i = 0; i < 4; ++i)
        if (ui::button({ dx + 78 + i*46, dy, 44, 22 }, fxName[i])) s.effectAsset = generateEffect(i);
    dy += 30;
    // sound: assign existing + generate new
    if (ui::button({ dx, dy, 260, 24 }, std::string("사운드: ") + name(s.soundAsset), s.soundAsset>=0))
        cycle(s.soundAsset, AssetType::Audio);
    dy += 26;
    DrawTextU("효과음 생성:", (int)dx, (int)dy+4, 12, ui::kTextDim);
    static const char* sndName[5] = { "베기","마법","폭발","대시","회복" };
    for (int i = 0; i < 5; ++i)
        if (ui::button({ dx + 78 + i*38, dy, 36, 22 }, sndName[i])) s.soundAsset = generateSound(i);
    dy += 34;

    if (ui::button({ dx, dy, 125, 28 }, "저장")) { p.save(); setStatus("스킬 저장됨."); }
    if (ui::button({ dx + 135, dy, 125, 28 }, "삭제", false)) {
        db.fieldSkills.erase(db.fieldSkills.begin() + skillSel_);
        skillSel_ = -1; p.save(); setStatus("스킬 삭제됨.");
    }
}

} // namespace tsukuru
