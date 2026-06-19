#include "game/Menu.h"
#include "core/Engine.h"
#include "render/UI.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

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
                case 3: saveGame(engine_); toast_ = "Game Saved!"; toastTimer_ = 2.0f; break;
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
                PartyMember& m = gs.party[0];
                if (it->effect == ItemEffect::HealHP) m.hp = std::min(m.maxHp, m.hp + it->power);
                else if (it->effect == ItemEffect::HealMP) m.mp = std::min(m.maxMp, m.mp + it->power);
                if (it->consumable) gs.inventory.removeItem(itemId);
                toast_ = "Used " + it->name; toastTimer_ = 1.5f;
            }
        }
    } else if (page_ == Page::Equip) {
        if (gs.party.empty()) return true;
        PartyMember& m = gs.party[0];
        // selection 0 = weapon, 1 = armor; Left/Right cycles equipment
        if (IsKeyPressed(KEY_DOWN)) selection_ = (selection_ + 1) % 2;
        if (IsKeyPressed(KEY_UP))   selection_ = (selection_ + 1) % 2;
        EquipSlot slot = selection_ == 0 ? EquipSlot::Weapon : EquipSlot::Armor;
        std::vector<int> opts{ -1 };
        for (const auto& e : db.equipment) if (e.slot == slot) opts.push_back(e.id);
        int& cur = selection_ == 0 ? m.weaponId : m.armorId;
        int idx = 0;
        for (int i = 0; i < (int)opts.size(); ++i) if (opts[i] == cur) idx = i;
        if (IsKeyPressed(KEY_RIGHT)) cur = opts[(idx + 1) % opts.size()];
        if (IsKeyPressed(KEY_LEFT))  cur = opts[(idx + opts.size() - 1) % opts.size()];
    } else if (page_ == Page::Status) {
        // view only
    }
    return true;
}

void Menu::drawRoot() {
    Rectangle r = { 40, 40, 220, 280 };
    ui::panel(r);
    ui::label("MENU", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    const char* opts[5] = { "Items", "Equipment", "Status", "Save", "Close" };
    for (int i = 0; i < 5; ++i) {
        Color c = i == selection_ ? ui::kAccentHi : ui::kText;
        std::string t = (i == selection_ ? "> " : "  ") + std::string(opts[i]);
        DrawText(t.c_str(), (int)r.x + 16, (int)r.y + 56 + i * 40, 22, c);
    }
}

void Menu::drawItems() {
    Rectangle r = { 40, 40, 420, 400 };
    ui::panel(r);
    ui::label("ITEMS", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;
    DrawText(TextFormat("Gold: %d", gs.inventory.gold), (int)r.x + 260, (int)r.y + 16, 18, ui::kGood);
    auto items = gs.inventory.list();
    if (items.empty()) ui::label("(empty)", (int)r.x + 16, (int)r.y + 56, 18, ui::kTextDim);
    for (int i = 0; i < (int)items.size(); ++i) {
        const Item* it = db.item(items[i].first);
        std::string name = it ? it->name : "?";
        Color c = i == selection_ ? ui::kAccentHi : ui::kText;
        std::string line = (i == selection_ ? "> " : "  ") + name + "  x" + std::to_string(items[i].second);
        DrawText(line.c_str(), (int)r.x + 16, (int)r.y + 56 + i * 28, 18, c);
    }
    DrawText("Enter: use   ESC: back", (int)r.x + 16, (int)(r.y + r.height - 28), 14, ui::kTextDim);
}

void Menu::drawEquip() {
    Rectangle r = { 40, 40, 460, 320 };
    ui::panel(r);
    ui::label("EQUIPMENT", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;
    if (gs.party.empty()) return;
    PartyMember& m = gs.party[0];
    const Equipment* w = db.equip(m.weaponId);
    const Equipment* a = db.equip(m.armorId);
    Color wc = selection_ == 0 ? ui::kAccentHi : ui::kText;
    Color ac = selection_ == 1 ? ui::kAccentHi : ui::kText;
    DrawText(TextFormat("Weapon: %s", w ? w->name.c_str() : "(none)"), (int)r.x + 16, (int)r.y + 70, 20, wc);
    DrawText(TextFormat("Armor : %s", a ? a->name.c_str() : "(none)"), (int)r.x + 16, (int)r.y + 110, 20, ac);
    DrawText(TextFormat("ATK %d   DEF %d", m.totalAtk(db), m.totalDef(db)),
             (int)r.x + 16, (int)r.y + 170, 18, ui::kGood);
    DrawText("Up/Down: slot   Left/Right: change   ESC: back",
             (int)r.x + 16, (int)(r.y + r.height - 28), 14, ui::kTextDim);
}

void Menu::drawStatus() {
    Rectangle r = { 40, 40, 460, 360 };
    ui::panel(r);
    ui::label("STATUS", (int)r.x + 16, (int)r.y + 12, 22, ui::kAccent);
    GameState& gs = engine_.state();
    Database&  db = engine_.project().database;
    int y = (int)r.y + 56;
    for (auto& m : gs.party) {
        const ActorDef* def = db.actor(m.actorId);
        DrawText(def ? def->name.c_str() : "Hero", (int)r.x + 16, y, 22, ui::kText);
        DrawText(TextFormat("Lv %d   EXP %d", m.level, m.exp), (int)r.x + 220, y, 18, ui::kTextDim);
        y += 30;
        DrawText(TextFormat("HP %d/%d   MP %d/%d", m.hp, m.maxHp, m.mp, m.maxMp),
                 (int)r.x + 24, y, 18, ui::kGood); y += 26;
        DrawText(TextFormat("ATK %d   DEF %d   SPD %d", m.totalAtk(db), m.totalDef(db), m.spd),
                 (int)r.x + 24, y, 18, ui::kText); y += 40;
    }
    DrawText("ESC: back", (int)r.x + 16, (int)(r.y + r.height - 28), 14, ui::kTextDim);
}

void Menu::draw() {
    switch (page_) {
        case Page::Root:   drawRoot();   break;
        case Page::Items:  drawItems();  break;
        case Page::Equip:  drawEquip();  break;
        case Page::Status: drawStatus(); break;
    }
    if (toastTimer_ > 0) {
        int w = MeasureText(toast_.c_str(), 20);
        DrawRectangle(GetScreenWidth()/2 - w/2 - 12, 20, w + 24, 36, ui::kAccent);
        DrawText(toast_.c_str(), GetScreenWidth()/2 - w/2, 28, 20, BLACK);
    }
}

} // namespace tsukuru
