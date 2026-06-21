// GamePlayEvents: event resolution, interaction, message box, runEvent.
#include "game/GamePlay.h"
#include "core/Engine.h"
#include "game/Menu.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <set>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace tsukuru {

// Pick the first ActionButton event at (x,y) whose condition is satisfied
// (multiple events on one tile act like RPG-Maker "event pages").
Event* GamePlay::actionEventAt(int x, int y) {
    GameState& gs = engine_.state();
    Event* fallback = nullptr;
    for (auto& e : map_->events) {
        if (e.x != x || e.y != y || e.trigger != TriggerType::ActionButton) continue;
        bool condOk = (e.conditionSwitch < 0) || (gs.getSwitch(e.conditionSwitch) == e.conditionValue);
        if (condOk) return &e;
        if (!fallback) fallback = &e;
    }
    return fallback;
}

void GamePlay::interact() {
    Vec2i delta = dirToDelta((Direction)dir_);
    int fx = destX_ + delta.x, fy = destY_ + delta.y;
    // a wandering NPC may have moved off its event tile — resolve by live position
    if (NpcInst* n = npcAt(fx, fy)) {
        Event* best = actionEventAt(n->x, n->y);
        if (best) { runEvent(*best); return; }
    }
    Event* e = actionEventAt(fx, fy);
    if (!e) e = actionEventAt(destX_, destY_);
    if (e) runEvent(*e);
}

// ----------------------------- message box -----------------------------
void GamePlay::showMessage(const std::string& text) {
    msgPages_.clear();
    size_t start = 0;
    while (true) {
        size_t bar = text.find('|', start);
        msgPages_.push_back(text.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
        if (bar == std::string::npos) break;
        start = bar + 1;
    }
    msgPage_ = 0;
    message_ = msgPages_.empty() ? "" : msgPages_[0];
    phase_ = Phase::Message;
    engine_.audio().playSfx("select", 0.5f);
}

void GamePlay::runEvent(Event& e) {
    GameState& gs = engine_.state();
    if (e.conditionSwitch >= 0 && gs.getSwitch(e.conditionSwitch) != e.conditionValue) return;
    long key = ((long)map_->id << 16) | (e.id & 0xffff);
    if (e.once && firedOnce_.count(key)) return;

    switch (e.type) {
        case EventType::Message:
            showMessage(e.text); break;
        case EventType::Teleport: {
            loadMap(e.targetMap);
            int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
            destX_ = e.targetX; destY_ = e.targetY;
            pxX_ = destX_ * (float)TS; pxY_ = destY_ * (float)TS;
            moving_ = false;
            gs.playerX = destX_; gs.playerY = destY_;
            spawnMonsters();
            runAutoruns();
            break;
        }
        case EventType::GiveItem:
            gs.inventory.addItem(e.itemId, e.amount);
            if (e.switchId >= 0) gs.setSwitch(e.switchId, true);   // mark quest progress
            engine_.audio().playSfx("coin");
            showMessage(e.text.empty() ? "아이템을 얻었다!" : e.text);
            break;
        case EventType::SetSwitch:
            gs.setSwitch(e.switchId, e.switchValue);
            if (!e.text.empty()) showMessage(e.text);
            break;
        case EventType::StartBattle: {
            // Repurposed: spawn live monsters on the field near the event.
            const Database& db = engine_.project().database;
            const EnemyDef* def = db.enemy(e.itemId);
            int TS = map_->tileset.tileWidth;
            int n = std::max(1, e.amount);
            for (int i = 0; i < n && def; ++i) {
                FieldMonster m;
                m.enemyId = def->id; m.name = def->name; m.spriteAsset = def->spriteAsset;
                m.x = m.destX = std::min(map_->tilemap.width()-1, e.x + 1 + i);
                m.y = m.destY = e.y;
                m.px = m.x * (float)TS; m.py = m.y * (float)TS;
                m.hp = m.maxHp = def->maxHp; m.atk = def->atk; m.def = def->def;
                m.expReward = def->expReward; m.goldReward = def->goldReward;
                m.defeatSwitch = e.switchId;       // boss gate: set switch when cleared
                monsters_.push_back(m);
                targetMonsters_ = std::max(targetMonsters_, (int)monsters_.size());
            }
            break;
        }
        case EventType::Shop:
            openShop(e.itemId, e.text.empty() ? "상점" : e.text);
            break;
        case EventType::Quest:
            gs.objective = e.text;
            if (e.switchId >= 0) gs.setSwitch(e.switchId, true);
            showMessage(e.text);
            break;
        case EventType::Ending:
            gs.objective.clear();
            engine_.audio().playSfx("levelup");
            phase_ = Phase::GameClear;
            break;
    }
    if (e.once) firedOnce_.insert(key);
}

// ----------------------------- shop -----------------------------
void GamePlay::openShop(int itemId, const std::string& title) {
    shopItemId_ = itemId;
    shopTitle_  = title;
    phase_ = Phase::Shop;
}

void GamePlay::updateShop(float dt) {
    (void)dt;
    if (IsKeyPressed(KEY_ESCAPE)) phase_ = Phase::Field;
}

void GamePlay::drawShop() {
    GameState& gs = engine_.state();
    const Item* it = engine_.project().database.item(shopItemId_);
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
    Rectangle box = { sw/2.0f - 230, sh/2.0f - 170, 460, 340 };
    ui::panel(box);
    DrawTextU(shopTitle_.c_str(), (int)box.x + 18, (int)box.y + 14, 24, ui::kAccent);
    DrawTextU(TextFormat("골드: %d", gs.inventory.gold), (int)(box.x + box.width - 160), (int)box.y + 18, 18, Color{230,200,90,255});

    if (!it) {
        DrawTextU("판매 상품이 없습니다.", (int)box.x + 18, (int)box.y + 70, 18, ui::kTextDim);
    } else {
        // item icon
        Rectangle ir = { box.x + 24, box.y + 70, 96, 96 };
        DrawRectangleRec(ir, Color{26,30,40,255});
        DrawRectangleLinesEx(ir, 1, ui::kAccent);
        if (it->iconAsset >= 0) {
            const Texture2D& tx = engine_.assetTexture(it->iconAsset);
            DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {ir.x+6,ir.y+6,84,84}, {0,0},0,WHITE);
        }
        DrawTextU(it->name.c_str(), (int)box.x + 136, (int)box.y + 76, 22, ui::kText);
        DrawTextU(TextFormat("가격: %d G", it->price), (int)box.x + 136, (int)box.y + 110, 18, Color{230,200,90,255});
        DrawTextU(TextFormat("보유: %d개", gs.inventory.count(it->id)), (int)box.x + 136, (int)box.y + 136, 16, ui::kTextDim);
        if (!it->description.empty())
            DrawTextU(it->description.c_str(), (int)box.x + 24, (int)box.y + 180, 15, ui::kText);

        bool canBuy = gs.inventory.gold >= it->price;
        if (ui::button({ box.x + 24, box.y + 280, 200, 40 }, "구입", false) && canBuy) {
            gs.inventory.gold -= it->price;
            gs.inventory.addItem(it->id, 1);
            engine_.audio().playSfx("coin");
            toast_ = it->name + " 구입!"; toastTimer_ = 1.2f;
        }
        if (!canBuy)
            DrawTextU("골드가 부족합니다", (int)box.x + 24, (int)box.y + 254, 14, ui::kDanger);
    }
    if (ui::button({ box.x + box.width - 224, box.y + 280, 200, 40 }, "닫기 (ESC)", false))
        phase_ = Phase::Field;

    if (toastTimer_ > 0) {
        int tw = MeasureTextU(toast_.c_str(), 18);
        DrawRectangle(sw/2 - tw/2 - 10, (int)box.y - 36, tw + 20, 28, Fade(ui::kAccent, 0.92f));
        DrawTextU(toast_.c_str(), sw/2 - tw/2, (int)box.y - 30, 18, BLACK);
    }
}

void GamePlay::drawMessage() {
    int sw = screenW(), sh = screenH();
    Rectangle box = { 40, (float)sh - 160, (float)sw - 80, 120 };
    DrawRectangleRec(box, Fade(Color{ 20, 24, 36, 255 }, 0.95f));
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    DrawTextU(message_.c_str(), (int)box.x + 20, (int)box.y + 20, 22, ui::kText);
    DrawTextU("[Enter]", (int)(box.x + box.width - 96), (int)(box.y + box.height - 28), 16, ui::kTextDim);
}


} // namespace tsukuru
