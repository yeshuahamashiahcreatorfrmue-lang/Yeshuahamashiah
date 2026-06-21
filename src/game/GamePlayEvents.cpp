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
            runQuestEvent(e);
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

// ----------------------------- quests / NPC rewards -----------------------------
bool GamePlay::questObjectiveMet(const Event& e, const QuestState& q) const {
    switch (e.questObjective) {
        case 1: return q.count >= e.questCount;                                       // 처치
        case 2: return engine_.state().inventory.count(e.questTarget) >= e.questCount;// 수집
        case 3: return q.count >= 1;                                                  // 도달
        default: return true;                                                         // 즉시
    }
}

std::string GamePlay::questProgressText(const Event& e, const QuestState& q) const {
    const Database& db = engine_.project().database;
    switch (e.questObjective) {
        case 1: { const EnemyDef* en = db.enemy(e.questTarget);
            return TextFormat("%s 처치 %d/%d", en ? en->name.c_str() : "적군", q.count, e.questCount); }
        case 2: { const Item* it = db.item(e.questTarget);
            return TextFormat("%s 수집 %d/%d", it ? it->name.c_str() : "아이템",
                              engine_.state().inventory.count(e.questTarget), e.questCount); }
        case 3: { std::shared_ptr<Map> m = engine_.project().map(e.questTarget);
            return std::string("목표 지역 도달: ") + (m ? m->name : "?") + (q.count >= 1 ? " (완료)" : ""); }
        default: return "보상 받기";
    }
}

void GamePlay::grantQuestReward(const Event& e) {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    std::string r;
    if (e.rewardGold > 0) { gs.inventory.gold += e.rewardGold; r += TextFormat("골드 +%d   ", e.rewardGold); }
    if (e.rewardExp > 0 && !gs.party.empty()) { gs.party[0].gainExp(e.rewardExp); r += TextFormat("경험치 +%d   ", e.rewardExp); }
    if (e.rewardItemId >= 0) {
        gs.inventory.addItem(e.rewardItemId, std::max(1, e.rewardItemCount));
        const Item* it = db.item(e.rewardItemId);
        r += (it ? it->name : std::string("아이템")) + TextFormat(" x%d", std::max(1, e.rewardItemCount));
    }
    engine_.audio().playSfx("levelup");
    toast_ = "보상 획득!"; toastTimer_ = 1.6f;
    std::string head = e.questDoneText.empty() ? "퀘스트 완료! 보상을 받았다." : e.questDoneText;
    showMessage(r.empty() ? head : (head + "|" + r));
}

void GamePlay::runQuestEvent(Event& e) {
    GameState& gs = engine_.state();
    bool hasReward = e.rewardGold > 0 || e.rewardExp > 0 || e.rewardItemId >= 0;
    // Pure announcement: objective 0 with no reward just sets the HUD goal & shows
    // its text (the classic "quest objective" event) — it never enters the log.
    if (e.questObjective == 0 && !hasReward) {
        if (!e.text.empty()) { gs.objective = e.text; showMessage(e.text); }
        if (e.switchId >= 0) gs.setSwitch(e.switchId, true);
        return;
    }
    long key = GameState::questKey(map_->id, e.id);
    QuestState& q = gs.quests[key];
    if (q.status == 0) {                         // 미수락
        if (e.questObjective == 0) {             // 즉시 보상형 NPC
            grantQuestReward(e); q.status = 2; q.title = e.text; return;
        }
        q.status = 1; q.count = 0;
        q.objective = e.questObjective; q.target = e.questTarget;
        q.need = std::max(1, e.questCount); q.title = e.text;
        refreshQuestObjective();
        showMessage((e.text.empty() ? "의뢰를 맡았다." : e.text) + "|[수락] " + questProgressText(e, q));
    } else if (q.status == 1) {                  // 진행중 -> 완료 검사
        if (questObjectiveMet(e, q)) {
            if (e.questObjective == 2 && e.questTakeItems && e.questTarget >= 0)
                gs.inventory.removeItem(e.questTarget, e.questCount);   // 수집형: 제출하면 소비
            grantQuestReward(e);
            q.status = 2; gs.objective.clear(); refreshQuestObjective();
        } else {
            showMessage("아직 끝나지 않았네. (" + questProgressText(e, q) + ")");
        }
    } else {                                     // 완료됨
        showMessage(e.questDoneText.empty() ? "고맙네, 모험가여!" : e.questDoneText);
    }
}

void GamePlay::refreshQuestObjective() {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    for (auto& kv : gs.quests) {
        QuestState& q = kv.second;
        if (q.status != 1) continue;
        std::string s;
        if (q.objective == 1) { const EnemyDef* en = db.enemy(q.target);
            s = TextFormat("%s %d/%d 처치", en ? en->name.c_str() : "적군", q.count, q.need); }
        else if (q.objective == 2) { const Item* it = db.item(q.target);
            s = TextFormat("%s %d/%d 수집", it ? it->name.c_str() : "아이템", gs.inventory.count(q.target), q.need); }
        else if (q.objective == 3) { std::shared_ptr<Map> m = engine_.project().map(q.target);
            s = std::string("목표 지역 도달: ") + (m ? m->name : "?"); }
        else s = q.title;
        gs.objective = s;
        return;
    }
}

void GamePlay::drawQuestLog() {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
    Rectangle box = { sw/2.0f - 280, 60, 560, (float)sh - 160 };
    ui::panel(box);
    DrawTextU("퀘스트 일지   (J 또는 ESC 로 닫기)", (int)box.x + 18, (int)box.y + 14, 22, ui::kAccent);
    if (ui::button({ box.x + box.width - 52, box.y + 12, 40, 28 }, "X")) questLogOpen_ = false;
    int y = (int)box.y + 56;
    int active = 0, done = 0;
    for (auto& kv : gs.quests) {
        const QuestState& q = kv.second;
        if (q.status == 0) continue;
        bool fin = q.status == 2;
        if (fin) ++done; else ++active;
        Color c = fin ? ui::kTextDim : ui::kText;
        DrawTextU(fin ? "[완료]" : "[진행]", (int)box.x + 18, y, 16, fin ? ui::kGood : ui::kAccentHi);
        DrawTextU(q.title.empty() ? "(이름 없는 의뢰)" : q.title.c_str(), (int)box.x + 86, y, 16, c);
        y += 22;
        if (!fin) {
            std::string p;
            if (q.objective == 1) { const EnemyDef* en = db.enemy(q.target);
                p = TextFormat("- %s 처치 %d/%d", en ? en->name.c_str() : "적군", q.count, q.need); }
            else if (q.objective == 2) { const Item* it = db.item(q.target);
                p = TextFormat("- %s 수집 %d/%d", it ? it->name.c_str() : "아이템", gs.inventory.count(q.target), q.need); }
            else if (q.objective == 3) { std::shared_ptr<Map> m = engine_.project().map(q.target);
                p = std::string("- 목표 지역 도달: ") + (m ? m->name : "?") + (q.count >= 1 ? " (완료)" : ""); }
            else p = "- 보상 받기";
            DrawTextU(p.c_str(), (int)box.x + 36, y, 14, ui::kTextDim); y += 24;
        }
        if (y > box.y + box.height - 40) break;
    }
    if (active == 0 && done == 0)
        DrawTextU("받은 의뢰가 없습니다. NPC에게 말을 걸어보세요.", (int)box.x + 18, (int)box.y + 60, 16, ui::kTextDim);
    DrawTextU(TextFormat("진행 %d · 완료 %d", active, done), (int)box.x + 18, (int)(box.y + box.height - 30), 14, ui::kTextDim);
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
