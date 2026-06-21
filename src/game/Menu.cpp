#include "game/Menu.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;

namespace tsukuru {

Menu::Menu(Engine& engine) : engine_(engine) {}

static void saveGame(Engine& e) {
    fs::path dir = fs::path(e.project().dir) / "save";
    std::error_code ec; fs::create_directories(dir, ec);
    std::ofstream f((dir / "slot1.json").string());
    if (f) f << e.state().toJson().dump(2);
}

bool Menu::update(float dt) {
    if (toastTimer_ > 0) toastTimer_ -= dt;

    if (IsKeyPressed(KEY_ESCAPE)) {
        if (page_ == Page::Root) return false; // close menu
        page_ = Page::Root; selection_ = 0; return true;
    }

    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;

    if (page_ == Page::Root) {
        const int N = 5; // Items, Equip, Status, Save, Close
        if (IsKeyPressed(KEY_DOWN)) selection_ = (selection_ + 1) % N;
        if (IsKeyPressed(KEY_UP))   selection_ = (selection_ + N - 1) % N;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            switch (selection_) {
                case 0: page_ = Page::Items;  selection_ = 0; break;
                case 1: page_ = Page::Equip;  selection_ = 0; break;
                case 2: page_ = Page::Status; selection_ = 0; break;
                case 3: saveGame(engine_); toast_ = "게임이 저장되었습니다!"; toastTimer_ = 2.0f; break;
                case 4: return false;
            }
        }
    } else if (page_ == Page::Items) {
        auto items = gs.inventory.list();
        int N = std::max(1, (int)items.size());
        if (IsKeyPressed(KEY_DOWN)) selection_ = (selection_ + 1) % N;
        if (IsKeyPressed(KEY_UP))   selection_ = (selection_ + N - 1) % N;
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && !items.empty()) {
            int itemId = items[selection_ % items.size()].first;
            const Item* it = db.item(itemId);
            if (it && !gs.party.empty()) {
                if (it->kind == 2) {                       // 장비: 장착
                    if (gs.equipItem(db, itemId)) { toast_ = "장착: " + it->name; toastTimer_ = 1.5f; }
                } else {                                   // 식품/기타: 사용(포만/수분/HP/GP 회복)
                    if (gs.consumeFood(db, itemId)) { toast_ = it->name + " 사용"; toastTimer_ = 1.5f; }
                }
            }
        }
    } else if (page_ == Page::Equip) {
        if (gs.party.empty()) return true;
        const int NS = 7;                                  // 머리/몸통/손/다리/발/무기/장신구
        if (IsKeyPressed(KEY_DOWN)) selection_ = (selection_ + 1) % NS;
        if (IsKeyPressed(KEY_UP))   selection_ = (selection_ + NS - 1) % NS;
        int slot = selection_ + 1;                         // body-slot id 1..7
        auto cur = gs.equipped.find(slot);
        int curId = cur != gs.equipped.end() ? cur->second : -1;
        // candidate options for this slot: 비움(-1) + 인벤토리의 해당 부위 장비
        std::vector<int> opts{ -1 };
        for (const auto& pr : gs.inventory.list()) {
            const Item* it = db.item(pr.first);
            if (!it || it->kind != 2) continue;
            int bs = (it->bodySlot >= 1 && it->bodySlot <= 7) ? it->bodySlot : 2;
            if (bs == slot) opts.push_back(pr.first);
        }
        int idx = 0;
        for (int i = 0; i < (int)opts.size(); ++i) if (opts[i] == curId) idx = i;
        if ((IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) && curId >= 0) {
            gs.unequipSlot(db, slot);                      // 해제
        } else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_LEFT)) {
            int d = IsKeyPressed(KEY_RIGHT) ? 1 : -1;
            int newId = opts[(idx + d + (int)opts.size()) % (int)opts.size()];
            gs.unequipSlot(db, slot);                      // 기존 해제 후
            if (newId >= 0) gs.equipItem(db, newId);       // 새 장비 장착
        }
    } else if (page_ == Page::Status) {
        // view only
    }
    return true;
}

void Menu::drawRoot() {
    Rectangle r = { 40, 40, 220, 280 };
    ui::panel(r);
    ui::label("메뉴", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    const char* opts[5] = { "아이템", "장비", "상태", "저장", "닫기" };
    for (int i = 0; i < 5; ++i) {
        Color c = i == selection_ ? ui::kAccentHi : ui::kText;
        std::string t = (i == selection_ ? "> " : "  ") + std::string(opts[i]);
        DrawTextU(t.c_str(), (int)r.x + 16, (int)r.y + 56 + i * 40, 22, c);
    }
}

void Menu::drawItems() {
    Rectangle r = { 40, 40, 420, 400 };
    ui::panel(r);
    ui::label("아이템", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;
    DrawTextU(TextFormat("골드: %d", gs.inventory.gold), (int)r.x + 260, (int)r.y + 16, 18, ui::kGood);
    auto items = gs.inventory.list();
    if (items.empty()) ui::label("(비어 있음)", (int)r.x + 16, (int)r.y + 56, 18, ui::kTextDim);
    const char* kindTag[3] = { "기타", "식품", "장비" };
    for (int i = 0; i < (int)items.size(); ++i) {
        const Item* it = db.item(items[i].first);
        std::string name = it ? it->name : "?";
        Color c = i == selection_ ? ui::kAccentHi : ui::kText;
        std::string line = (i == selection_ ? "> " : "  ") + name + "  x" + std::to_string(items[i].second);
        DrawTextU(line.c_str(), (int)r.x + 16, (int)r.y + 56 + i * 28, 18, c);
        if (it) {
            int k = (it->kind >= 0 && it->kind <= 2) ? it->kind : 0;
            DrawTextU(kindTag[k], (int)r.x + 250, (int)r.y + 58 + i * 28, 14,
                      k == 1 ? Color{225,170,75,255} : k == 2 ? ui::kAccent : ui::kTextDim);
        }
    }
    // detail line for the selected item: show what it does on screen
    if (!items.empty() && selection_ < (int)items.size()) {
        const Item* it = db.item(items[selection_].first);
        if (it) {
            std::string d;
            if (it->kind == 2) {
                if (it->bonusAtk) d += TextFormat("공+%d ", it->bonusAtk);
                if (it->bonusDef) d += TextFormat("방+%d ", it->bonusDef);
                if (it->bonusSpd) d += TextFormat("속+%d ", it->bonusSpd);
                if (d.empty()) d = "장비";
            } else {
                if (it->satiety)  d += TextFormat("포만+%d ", it->satiety);
                if (it->hydration)d += TextFormat("수분+%d ", it->hydration);
                int hh = it->healHp + (it->effect == ItemEffect::HealHP ? it->power : 0);
                int gg = it->healGp + (it->effect == ItemEffect::HealMP ? it->power : 0);
                if (hh) d += TextFormat("HP+%d ", hh);
                if (gg) d += TextFormat("GP+%d ", gg);
                if (it->bonusAtk) d += TextFormat("공+%d ", it->bonusAtk);
                if (it->bonusDef) d += TextFormat("방+%d ", it->bonusDef);
                if (it->bonusSpd) d += TextFormat("속+%d ", it->bonusSpd);
                if (it->buffSecs && (it->bonusAtk||it->bonusDef||it->bonusSpd)) d += TextFormat("(%d초) ", it->buffSecs);
                if (d.empty()) d = "효과 없음";
            }
            DrawTextU(d.c_str(), (int)r.x + 16, (int)(r.y + r.height - 52), 15, ui::kGood);
        }
    }
    DrawTextU("Enter: 사용/장착   ESC: 뒤로", (int)r.x + 16, (int)(r.y + r.height - 28), 14, ui::kTextDim);
}

void Menu::drawEquip() {
    Rectangle r = { 40, 40, 460, 360 };
    ui::panel(r);
    ui::label("장비 (신체 부위)", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;
    if (gs.party.empty()) return;
    PartyMember& m = gs.party[0];
    const char* slotName[7] = { "머리", "몸통", "손", "다리", "발", "무기", "장신구" };
    for (int i = 0; i < 7; ++i) {
        int slot = i + 1;
        auto e = gs.equipped.find(slot);
        const Item* it = (e != gs.equipped.end()) ? db.item(e->second) : nullptr;
        Color c = i == selection_ ? ui::kAccentHi : ui::kText;
        std::string line = (i == selection_ ? "> " : "  ") + std::string(slotName[i]) + ": "
                         + (it ? it->name : std::string("(없음)"));
        DrawTextU(line.c_str(), (int)r.x + 16, (int)r.y + 56 + i * 28, 18, c);
        if (it && (it->bonusAtk || it->bonusDef || it->bonusSpd)) {
            std::string b;
            if (it->bonusAtk) b += TextFormat("공+%d ", it->bonusAtk);
            if (it->bonusDef) b += TextFormat("방+%d ", it->bonusDef);
            if (it->bonusSpd) b += TextFormat("속+%d ", it->bonusSpd);
            DrawTextU(b.c_str(), (int)r.x + 250, (int)r.y + 58 + i * 28, 14, ui::kGood);
        }
    }
    DrawTextU(TextFormat("총 공격 %d   방어 %d   속도 %d (장비·버프 포함)",
              m.atk, m.def, m.spd),
             (int)r.x + 16, (int)(r.y + r.height - 50), 18, ui::kGood);
    DrawTextU("위/아래: 부위  좌/우: 교체  Enter: 해제  ESC: 뒤로",
             (int)r.x + 16, (int)(r.y + r.height - 26), 14, ui::kTextDim);
}

void Menu::drawStatus() {
    Rectangle r = { 40, 40, 460, 440 };
    ui::panel(r);
    ui::label("상태", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;
    int y = (int)r.y + 56;
    for (auto& m : gs.party) {
        const ActorDef* def = db.actor(m.actorId);
        DrawTextU(def ? def->name.c_str() : "용사", (int)r.x + 16, y, 22, ui::kText);
        DrawTextU(TextFormat("Lv %d   EXP %d", m.level, m.exp), (int)r.x + 220, y, 18, ui::kTextDim);
        y += 30;
        DrawTextU(TextFormat("체력 %d/%d   기력 %d/%d", m.hp, m.maxHp, m.mp, m.maxMp),
                 (int)r.x + 24, y, 18, ui::kGood); y += 26;
        DrawTextU(TextFormat("포만 %d/%d   수분 %d/%d", m.hunger, m.maxHunger, m.thirst, m.maxThirst),
                 (int)r.x + 24, y, 18, Color{225,170,75,255}); y += 26;
        DrawTextU(TextFormat("공격 %d   방어 %d   속도 %d (장비·버프 포함)", m.atk, m.def, m.spd),
                 (int)r.x + 24, y, 18, ui::kText); y += 40;
    }
    if (!gs.buffs.empty()) {
        DrawTextU("활성 버프:", (int)r.x + 16, y, 16, ui::kAccent); y += 22;
        for (const auto& b : gs.buffs) {
            DrawTextU(TextFormat("· %s  공+%d 방+%d 속+%d  (%.0f초)",
                      b.name.c_str(), b.atk, b.def, b.spd, b.remain),
                     (int)r.x + 24, y, 15, ui::kGood); y += 20;
        }
    }
    DrawTextU("ESC: 뒤로", (int)r.x + 16, (int)(r.y + r.height - 28), 14, ui::kTextDim);
}

void Menu::draw() {
    switch (page_) {
        case Page::Root:   drawRoot();   break;
        case Page::Items:  drawItems();  break;
        case Page::Equip:  drawEquip();  break;
        case Page::Status: drawStatus(); break;
    }
    if (toastTimer_ > 0) {
        int w = MeasureTextU(toast_.c_str(), 20);
        DrawRectangle(screenW()/2 - w/2 - 12, 20, w + 24, 36, ui::kAccent);
        DrawTextU(toast_.c_str(), screenW()/2 - w/2, 28, 20, BLACK);
    }
}

} // namespace tsukuru
