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

    // category tabs
    const char* cats[] = { "아이템", "장비", "스킬", "액터", "적" };
    for (int i = 0; i < 5; ++i)
        if (ui::button({ 12.0f + i*120, kToolbarH + 10, 112, 28 }, cats[i], dbCategory_ == i)) {
            dbCategory_ = i; dbSelected_ = -1; dbNameFocus_ = -1;
        }

    float listX = 12, listY = kToolbarH + 50, listW = 280;
    ui::panel({ listX, listY, listW, area.height - 60 }, ui::kPanel);

    // "Add" button
    if (ui::button({ listX + 8, listY + 8, listW - 16, 28 }, "+ 새로 추가")) {
        switch (dbCategory_) {
            case 0: { Item it; it.id = (int)db.items.size()+1; db.items.push_back(it); dbSelected_=(int)db.items.size()-1; } break;
            case 1: { Equipment e; e.id = (int)db.equipment.size()+1; db.equipment.push_back(e); dbSelected_=(int)db.equipment.size()-1; } break;
            case 2: { Skill s; s.id = (int)db.skills.size()+1; db.skills.push_back(s); dbSelected_=(int)db.skills.size()-1; } break;
            case 3: { ActorDef a; a.id = (int)db.actors.size()+1; db.actors.push_back(a); dbSelected_=(int)db.actors.size()-1; } break;
            case 4: { EnemyDef en; en.id = (int)db.enemies.size()+1; db.enemies.push_back(en); dbSelected_=(int)db.enemies.size()-1; } break;
        }
    }

    // list
    float ly = listY + 44;
    auto listEntry = [&](int i, const std::string& name) {
        Rectangle r = { listX + 8, ly, listW - 16, 26 };
        if (ui::button(r, name, dbSelected_ == i)) { dbSelected_ = i; dbNameFocus_ = -1; }
        ly += 28;
    };
    int count = 0;
    switch (dbCategory_) {
        case 0: count=(int)db.items.size();     for (int i=0;i<count;++i) listEntry(i, db.items[i].name); break;
        case 1: count=(int)db.equipment.size(); for (int i=0;i<count;++i) listEntry(i, db.equipment[i].name); break;
        case 2: count=(int)db.skills.size();    for (int i=0;i<count;++i) listEntry(i, db.skills[i].name); break;
        case 3: count=(int)db.actors.size();    for (int i=0;i<count;++i) listEntry(i, db.actors[i].name); break;
        case 4: count=(int)db.enemies.size();   for (int i=0;i<count;++i) listEntry(i, db.enemies[i].name); break;
    }

    if (dbSelected_ < 0 || dbSelected_ >= count) return;

    // ---- detail editor ----
    float dx = listX + listW + 20, dy = listY + 8, dw = area.width - dx - 20;
    ui::panel({ dx - 8, listY, dw + 16, area.height - 60 }, ui::kPanel);

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
            const char* kinds[] = {"기타","식품(음식/음료)","장비"};
            if (ui::button({dx,dy,260,26}, TextFormat("분류: %s", kinds[it.kind%3]))) it.kind=(it.kind+1)%3;
            dy+=32;
            // icon image (cycle through image assets) + import
            if (ui::button({dx,dy,260,26}, std::string("이미지: ")+assetName(it.iconAsset), it.iconAsset>=0))
                cycleAsset(it.iconAsset, AssetType::Image);
            dy+=30;
            step("가격", it.price, 10, 0, 99999);
            if (it.kind == 1) {                                  // 식품(food/drink)
                DrawTextU("─ 식품 효과 ─", (int)dx, (int)dy, 13, ui::kAccentHi); dy+=18;
                step("포만도 +", it.satiety, 100, 0, 99999);
                step("수분 +",   it.hydration, 100, 0, 99999);
                step("HP 회복 +", it.healHp, 5, 0, 99999);
                step("기력 회복 +", it.healGp, 5, 0, 9999);
                DrawTextU("─ 추가 효과(영구 버프) ─", (int)dx, (int)dy, 13, ui::kAccentHi); dy+=18;
                step("공격 +", it.bonusAtk, 1, -999, 999);
                step("방어 +", it.bonusDef, 1, -999, 999);
                step("이동 +", it.bonusSpd, 1, -99, 99);
            } else if (it.kind == 2) {                           // 장비(equipment)
                const char* bn[] = {"없음","머리","몸통","손","다리","발","무기","장신구"};
                if (ui::button({dx,dy,260,26}, TextFormat("장착 부위: %s", bn[it.bodySlot%8]))) it.bodySlot=(it.bodySlot+1)%8;
                dy+=32;
                step("공격 +", it.bonusAtk, 1, -999, 999);
                step("방어 +", it.bonusDef, 1, -999, 999);
                step("이동 +", it.bonusSpd, 1, -99, 99);
            } else {                                             // 기타
                step("효과량", it.power, 5, 0, 9999);
                const char* effs[] = {"없음","HP회복","MP회복","데미지"};
                if (ui::button({dx,dy,200,26}, TextFormat("효과: %s", effs[(int)it.effect]))) it.effect=(ItemEffect)(((int)it.effect+1)%4);
                dy+=32;
                if (ui::button({dx,dy,200,26}, it.consumable?"소모성: 예":"소모성: 아니오")) it.consumable=!it.consumable;
                dy+=36;
            }
            break; }
        case 1: { Equipment& e = db.equipment[dbSelected_]; nameField(e.name);
            if (ui::button({dx,dy,200,26}, e.slot==EquipSlot::Weapon?"슬롯: 무기":"슬롯: 방어구"))
                e.slot = e.slot==EquipSlot::Weapon?EquipSlot::Armor:EquipSlot::Weapon;
            dy+=32;
            step("가격", e.price, 10, 0, 99999);
            step("공격+", e.atk, 1, 0, 999);
            step("방어+", e.def, 1, 0, 999);
            break; }
        case 2: { Skill& s = db.skills[dbSelected_]; nameField(s.name);
            step("MP 소모", s.mpCost, 1, 0, 999);
            step("위력", s.power, 5, 0, 9999);
            if (ui::button({dx,dy,200,26}, s.healing?"종류: 회복":"종류: 데미지")) s.healing=!s.healing;
            dy+=36;
            break; }
        case 3: { ActorDef& a = db.actors[dbSelected_]; nameField(a.name);
            step("최대 HP", a.maxHp, 10, 1, 9999);
            step("최대 MP", a.maxMp, 5, 0, 9999);
            step("공격", a.atk, 1, 0, 999);
            step("방어", a.def, 1, 0, 999);
            step("속도", a.spd, 1, 0, 999);
            break; }
        case 4: { EnemyDef& e = db.enemies[dbSelected_]; nameField(e.name);
            step("최대 HP", e.maxHp, 10, 1, 9999);
            step("공격", e.atk, 1, 0, 999);
            step("방어", e.def, 1, 0, 999);
            step("속도", e.spd, 1, 0, 999);
            step("경험치", e.expReward, 5, 0, 99999);
            step("골드", e.goldReward, 5, 0, 99999);
            break; }
    }
    DrawTextU(TextFormat("id: %d   (Ctrl+S로 프로젝트 저장)",
             dbCategory_==0?db.items[dbSelected_].id:
             dbCategory_==1?db.equipment[dbSelected_].id:
             dbCategory_==2?db.skills[dbSelected_].id:
             dbCategory_==3?db.actors[dbSelected_].id:db.enemies[dbSelected_].id),
             (int)dx, (int)dy + 6, 14, ui::kTextDim);
}


} // namespace tsukuru
