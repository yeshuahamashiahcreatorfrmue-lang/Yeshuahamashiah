// EditorDatabase: items/equipment/skills/actors/enemies.
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

void Editor::drawDatabaseTab() {
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Database& db = engine_.project().database;

    DrawTextU("데이터베이스 — 분류를 펼쳐 전체 목록에서 선택", 12, (int)kToolbarH + 12, 16, ui::kAccent);

    // ---- accordion list: all 3 categories visible at once; click a header to
    //      fold/unfold, click an entry to select & edit it (no view-switching). ----
    const char* cats[] = { "아이템", "액터", "적" };
    float listX = 12, listY = kToolbarH + 44, listW = 280;
    // search box above the accordion
    searchBox({ listX, listY, listW, 26 }, dbSearch_, 9201);
    listY += 32;
    Rectangle listPanel = { listX, listY, listW, area.height - 86 };
    ui::panel(listPanel, ui::kPanel);
    uiScissor((int)listX, (int)listY, (int)listW, (int)listPanel.height);
    auto catCount = [&](int c){ return c==0?(int)db.items.size():c==1?(int)db.actors.size():(int)db.enemies.size(); };
    auto catName  = [&](int c,int i)->std::string{ return c==0?db.items[i].name:c==1?db.actors[i].name:db.enemies[i].name; };
    // measure content height for the scrollbar (only expanded + matching rows)
    float contentH = 6;
    for (int c = 0; c < 3; ++c) {
        contentH += 32;
        if (!dbExpanded_[c]) continue;
        for (int i = 0; i < catCount(c); ++i) if (nameMatch(catName(c, i), dbSearch_)) contentH += 26;
        contentH += 32;
    }
    float ly = listY + 6 - dbListScroll_;
    for (int c = 0; c < 3; ++c) {
        // section header ([-]/[+] + name + count)
        Rectangle hr = { listX + 6, ly, listW - 12, 28 };
        std::string htxt = std::string(dbExpanded_[c] ? "[-] " : "[+] ") + cats[c] + "  (" + std::to_string(catCount(c)) + ")";
        if (ui::button(hr, htxt, false)) dbExpanded_[c] = !dbExpanded_[c];
        ly += 32;
        if (!dbExpanded_[c]) continue;
        for (int i = 0; i < catCount(c); ++i) {
            if (!nameMatch(catName(c, i), dbSearch_)) continue;
            Rectangle r = { listX + 18, ly, listW - 26, 24 };
            bool sel = (dbCategory_ == c && dbSelected_ == i);
            if (ly + 24 > listY && ly < listY + listPanel.height)
                if (ui::button(r, catName(c, i), sel)) { dbCategory_ = c; dbSelected_ = i; dbNameFocus_ = -1; dbScroll_ = 0; }
            ly += 26;
        }
        Rectangle ar = { listX + 18, ly, listW - 26, 24 };
        if (ui::button(ar, "+ 새로 추가", false)) {
            if (c==0){ Item it; it.id=(int)db.items.size()+1; db.items.push_back(it); dbSelected_=(int)db.items.size()-1; }
            else if (c==1){ ActorDef a; a.id=(int)db.actors.size()+1; db.actors.push_back(a); dbSelected_=(int)db.actors.size()-1; }
            else { EnemyDef en; en.id=(int)db.enemies.size()+1; db.enemies.push_back(en); dbSelected_=(int)db.enemies.size()-1; }
            dbCategory_ = c; dbNameFocus_ = -1; dbScroll_ = 0;
        }
        ly += 32;
    }
    EndScissorMode();
    scrollbar(listPanel, dbListScroll_, contentH);

    int count = catCount(dbCategory_);
    if (dbSelected_ < 0 || dbSelected_ >= count) return;

    // ---- detail editor (scrollable: tall editors like 식품 overflow the panel) ----
    float dx = listX + listW + 20, dw = area.width - dx - 20;
    Rectangle detailR = { dx - 8, listY, dw + 16, area.height - 60 };
    ui::panel(detailR, ui::kPanel);
    if (ui::mouseIn(detailR)) dbScroll_ -= GetMouseWheelMove() * 40;
    if (dbScroll_ < 0) dbScroll_ = 0;
    uiScissor((int)detailR.x, (int)detailR.y, (int)detailR.width, (int)detailR.height);
    float dy = listY + 8 - dbScroll_;

    auto nameField = [&](std::string& name) {
        ui::label("이름:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
        Rectangle tf = { dx, dy, std::min(360.0f, dw), 28 };
        bool focus = (dbNameFocus_ == dbSelected_);
        if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbNameFocus_ = dbSelected_;
        else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf) && dbNameFocus_ == dbSelected_) dbNameFocus_ = -1;
        ui::textField(tf, name, focus, 32);
        dy += 38;
    };
    auto step = [&](const char* lbl, int& v, int s, int lo, int hi) {
        ui::intStepper({ dx, dy, std::min(300.0f, dw), 26 }, lbl, v, s, lo, hi); dy += 30;
    };

    switch (dbCategory_) {
        case 0: { Item& it = db.items[dbSelected_]; nameField(it.name);
            // description (shown in inventory/menu as flavour/effect text)
            ui::label("설명:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
            {
                Rectangle tf = { dx, dy, std::min(360.0f, dw), 28 };
                bool focus = (dbDescFocus_ == dbSelected_);
                if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbDescFocus_ = dbSelected_;
                else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf) && dbDescFocus_ == dbSelected_) dbDescFocus_ = -1;
                ui::textField(tf, it.description, focus, 64);
                dy += 36;
            }
            optionButton({dx,dy,260,26}, "분류", {"기타","식품(음식/음료)","장비"}, {}, it.kind, 1001);
            dy+=32;
            // icon image: pick from registered assets, or import an external picture
            assetButton({dx,dy,260,26}, "이미지", it.iconAsset, 1002);
            dy+=30;
            if (ui::button({dx,dy,260,24}, "이미지 가져오기 (외부 파일)", true)) {
                pendingItemIcon_ = true; pendingItemIconId_ = it.id;
            }
            dy+=30;
            step("가격", it.price, 10, 0, 99999);
            if (it.kind == 1) {                                  // 식품(food/drink)
                DrawTextU("─ 식품 효과 ─", (int)dx, (int)dy, 13, ui::kAccentHi); dy+=18;
                step("포만도 +", it.satiety, 100, 0, 99999);
                step("수분 +",   it.hydration, 100, 0, 99999);
                step("HP 회복 +", it.healHp, 5, 0, 99999);
                step("기력 회복 +", it.healGp, 5, 0, 9999);
                DrawTextU("─ 추가 효과(공/방/이동) ─", (int)dx, (int)dy, 13, ui::kAccentHi); dy+=18;
                step("공격 +", it.bonusAtk, 1, -999, 999);
                step("방어 +", it.bonusDef, 1, -999, 999);
                step("이동 +", it.bonusSpd, 1, -99, 99);
                step("버프 지속(초·0=영구)", it.buffSecs, 10, 0, 9999);
            } else if (it.kind == 2) {                           // 장비(equipment)
                optionButton({dx,dy,260,26}, "장착 부위", {"없음","머리","몸통","손","다리","발","무기","장신구"}, {}, it.bodySlot, 1003);
                dy+=32;
                step("공격 +", it.bonusAtk, 1, -999, 999);
                step("방어 +", it.bonusDef, 1, -999, 999);
                step("이동 +", it.bonusSpd, 1, -99, 99);
            } else {                                             // 기타
                step("효과량", it.power, 5, 0, 9999);
                optionButtonEnum({dx,dy,200,26}, "효과", {"없음","HP회복","MP회복","데미지"}, it.effect, 1004);
                dy+=32;
                if (ui::button({dx,dy,200,26}, it.consumable?"소모성: 예":"소모성: 아니오")) it.consumable=!it.consumable;
                dy+=36;
            }
            break; }
        case 1: { ActorDef& a = db.actors[dbSelected_]; nameField(a.name);
            assetButton({dx,dy,260,26}, "스프라이트", a.spriteAsset, 1010);
            dy+=32;
            step("최대 HP", a.maxHp, 10, 1, 9999);
            step("최대 GP", a.maxMp, 5, 0, 9999);
            step("공격", a.atk, 1, 0, 999);
            step("방어", a.def, 1, 0, 999);
            step("속도", a.spd, 1, 0, 999);
            break; }
        case 2: { EnemyDef& e = db.enemies[dbSelected_]; nameField(e.name);
            assetButton({dx,dy,260,26}, "스프라이트", e.spriteAsset, 1011);
            dy+=32;
            step("최대 HP", e.maxHp, 10, 1, 9999);
            step("공격", e.atk, 1, 0, 999);
            step("방어", e.def, 1, 0, 999);
            step("속도", e.spd, 1, 0, 999);
            step("경험치", e.expReward, 5, 0, 99999);
            step("골드", e.goldReward, 5, 0, 99999);
            step("드롭 아이템ID(-1없음)", e.dropItemId, 1, -1, 999);
            if (e.dropItemId >= 0) {
                const Item* di = db.item(e.dropItemId);
                DrawTextU(("→ " + (di ? di->name : std::string("(없는 아이템)"))).c_str(),
                          (int)dx + 4, (int)dy, 12, di ? ui::kAccentHi : ui::kDanger); dy += 18;
                step("드롭 확률 %", e.dropRate, 5, 0, 100);
            }
            break; }
    }
    DrawTextU(TextFormat("id: %d   (Ctrl+S로 프로젝트 저장)",
             dbCategory_==0?db.items[dbSelected_].id:
             dbCategory_==1?db.actors[dbSelected_].id:db.enemies[dbSelected_].id),
             (int)dx, (int)dy + 6, 14, ui::kTextDim);
    EndScissorMode();
    // clamp scroll so you can't scroll past the content (uses this frame's end dy)
    float contentBottom = dy + 6 + 20 + dbScroll_;          // absolute bottom of content
    float maxScroll = std::max(0.0f, contentBottom - (detailR.y + detailR.height));
    if (dbScroll_ > maxScroll) dbScroll_ = maxScroll;
}


} // namespace tsukuru
