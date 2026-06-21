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

// Event activation condition: an optional switch AND an optional variable gate.
bool eventConditionMet(GameState& gs, const Event& e) {
    if (e.conditionSwitch >= 0 && gs.getSwitch(e.conditionSwitch) != e.conditionValue) return false;
    if (e.conditionVar >= 0 && gs.getVar(e.conditionVar) < e.conditionVarMin) return false;
    return true;
}

// Pick the first ActionButton event at (x,y) whose condition is satisfied
// (multiple events on one tile act like RPG-Maker "event pages").
Event* GamePlay::actionEventAt(int x, int y) {
    GameState& gs = engine_.state();
    Event* fallback = nullptr;
    for (auto& e : map_->events) {
        if (e.x != x || e.y != y || e.trigger != TriggerType::ActionButton) continue;
        if (eventConditionMet(gs, e)) return &e;
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
    showMessageEx(text, "", -1, "", "", -1);
}

void GamePlay::showMessageEx(const std::string& text, const std::string& speaker, int faceAsset,
                             const std::string& choiceA, const std::string& choiceB, int choiceSwitch) {
    msgSpeaker_ = speaker; msgFace_ = faceAsset;
    msgChoiceA_ = choiceA; msgChoiceB_ = choiceB;
    // a choice is pending only when both options are provided
    msgChoiceSwitch_ = (!choiceA.empty() && !choiceB.empty()) ? choiceSwitch : -2; // -2 = no choice
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

// true while the player must still pick a choice (last page + both options set)
static bool choicePending(int sw, const std::string& a, const std::string& b) {
    return sw != -2 && !a.empty() && !b.empty();
}

void GamePlay::runEvent(Event& e) {
    GameState& gs = engine_.state();
    if (!eventConditionMet(gs, e)) return;
    long key = ((long)map_->id << 16) | (e.id & 0xffff);
    if (e.once && firedOnce_.count(key)) return;

    switch (e.type) {
        case EventType::Message:
            showMessageEx(e.text, e.speakerName, e.faceAsset, e.choiceA, e.choiceB, e.choiceSwitch);
            break;
        case EventType::Teleport: {
            loadMap(e.targetMap);
            int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
            destX_ = e.targetX; destY_ = e.targetY;
            pxX_ = destX_ * (float)TS; pxY_ = destY_ * (float)TS;
            moving_ = false;
            if (e.faceDir >= 0 && e.faceDir <= 3) dir_ = e.faceDir;   // face a set direction
            gs.playerX = destX_; gs.playerY = destY_; gs.playerDir = dir_;
            spawnMonsters();
            runAutoruns();
            break;
        }
        case EventType::GiveItem: {
            if (e.amount < 0) gs.inventory.removeItem(e.itemId, -e.amount);  // 음수 = 회수
            else if (e.itemId >= 0) gs.inventory.addItem(e.itemId, e.amount);
            if (e.giveGold != 0) gs.inventory.gold = std::max(0, gs.inventory.gold + e.giveGold);
            if (e.switchId >= 0) gs.setSwitch(e.switchId, true);   // mark quest progress
            engine_.audio().playSfx("coin");
            std::string msg = e.text;
            if (msg.empty()) msg = e.amount < 0 ? "아이템을 건넸다." : e.giveGold > 0 ? "골드를 얻었다!" : "아이템을 얻었다!";
            showMessage(msg);
            break;
        }
        case EventType::SetSwitch:
            if (e.switchId >= 0) gs.setSwitch(e.switchId, e.switchValue);
            if (e.varId >= 0) {                                      // 변수 대입/증가
                int cur = gs.getVar(e.varId);
                gs.setVar(e.varId, e.varOp == 1 ? cur + e.varValue : e.varValue);
            }
            if (!e.text.empty()) showMessage(e.text);
            break;
        case EventType::StartBattle: {
            if (e.battleTurnBased) {                                // 즉시 턴제 전투
                std::vector<int> ids(std::max(1, e.amount), e.itemId);
                startBattleWith(ids);
                break;
            }
            // Otherwise: spawn live monsters on the field near the event.
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
        case EventType::Shop: {
            std::vector<int> wares = e.shopItems;
            if (wares.empty() && e.itemId >= 0) wares.push_back(e.itemId);  // fallback: single ware
            openShop(wares, e.text.empty() ? "상점" : e.text);
            break;
        }
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

// ----------------------------- shop (multi-item) -----------------------------
void GamePlay::openShop(const std::vector<int>& items, const std::string& title) {
    shopItems_ = items;
    shopTitle_ = title;
    phase_ = Phase::Shop;
}

void GamePlay::updateShop(float dt) {
    (void)dt;
    if (IsKeyPressed(KEY_ESCAPE)) phase_ = Phase::Field;
}

void GamePlay::drawShop() {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
    int rows = std::max(1, (int)shopItems_.size());
    float listH = rows * 64.0f;
    Rectangle box = { sw/2.0f - 280, sh/2.0f - (listH + 130) / 2, 560, listH + 130 };
    ui::panel(box);
    DrawTextU(shopTitle_.c_str(), (int)box.x + 18, (int)box.y + 14, 24, ui::kAccent);
    DrawTextU(TextFormat("골드: %d", gs.inventory.gold), (int)(box.x + box.width - 170), (int)box.y + 18, 18, Color{230,200,90,255});

    float ry = box.y + 54;
    if (shopItems_.empty())
        DrawTextU("판매 상품이 없습니다.", (int)box.x + 18, (int)ry, 18, ui::kTextDim);
    for (int it_i = 0; it_i < (int)shopItems_.size(); ++it_i) {
        const Item* it = db.item(shopItems_[it_i]);
        if (!it) continue;
        Rectangle row = { box.x + 16, ry, box.width - 32, 56 };
        DrawRectangleRec(row, Color{26,30,40,255});
        DrawRectangleLinesEx(row, 1, Fade(ui::kAccent, 0.5f));
        if (it->iconAsset >= 0) {
            const Texture2D& tx = engine_.assetTexture(it->iconAsset);
            DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {row.x+6,row.y+6,44,44}, {0,0},0,WHITE);
        }
        DrawTextU(it->name.c_str(), (int)row.x + 58, (int)row.y + 8, 18, ui::kText);
        DrawTextU(TextFormat("%d G   보유 %d", it->price, gs.inventory.count(it->id)),
                  (int)row.x + 58, (int)row.y + 32, 14, Color{230,200,90,255});
        bool canBuy = gs.inventory.gold >= it->price;
        Rectangle buyB = { row.x + row.width - 110, row.y + 12, 96, 32 };
        if (ui::button(buyB, canBuy ? "구입" : "골드부족", false) && canBuy) {
            gs.inventory.gold -= it->price;
            gs.inventory.addItem(it->id, 1);
            engine_.audio().playSfx("coin");
            toast_ = it->name + " 구입!"; toastTimer_ = 1.2f;
        }
        ry += 64;
    }
    if (ui::button({ box.x + box.width/2 - 100, box.y + box.height - 46, 200, 36 }, "닫기 (ESC)", false))
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
    if (e.rewardSwitch >= 0) gs.setSwitch(e.rewardSwitch, true);   // 완료 게이트 스위치
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
    bool lastPage = msgPage_ + 1 >= (int)msgPages_.size();
    bool choice = lastPage && choicePending(msgChoiceSwitch_, msgChoiceA_, msgChoiceB_);
    float textX = box.x + 20;
    // portrait
    if (msgFace_ >= 0) {
        const Texture2D& tx = engine_.assetTexture(msgFace_);
        Rectangle fr = { box.x + 10, box.y - 80, 84, 84 };
        DrawRectangleRec(fr, Fade(Color{20,24,36,255},0.95f));
        DrawRectangleLinesEx(fr, 2, ui::kAccent);
        DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {fr.x+4,fr.y+4,76,76}, {0,0},0,WHITE);
    }
    // name plate
    if (!msgSpeaker_.empty()) {
        int nw = MeasureTextU(msgSpeaker_.c_str(), 18) + 24;
        DrawRectangle((int)box.x, (int)box.y - 30, nw, 28, ui::kAccent);
        DrawTextU(msgSpeaker_.c_str(), (int)box.x + 12, (int)box.y - 26, 18, BLACK);
    }
    DrawRectangleRec(box, Fade(Color{ 20, 24, 36, 255 }, 0.95f));
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    DrawTextU(message_.c_str(), (int)textX, (int)box.y + 20, 22, ui::kText);
    if (choice) {
        // two-way choice buttons
        Rectangle bA = { box.x + box.width - 360, box.y + box.height - 44, 168, 34 };
        Rectangle bB = { box.x + box.width - 184, box.y + box.height - 44, 168, 34 };
        if (ui::button(bA, msgChoiceA_, false)) {
            if (msgChoiceSwitch_ >= 0) engine_.state().setSwitch(msgChoiceSwitch_, true);
            phase_ = Phase::Field; message_.clear();
        }
        if (ui::button(bB, msgChoiceB_, false)) {
            if (msgChoiceSwitch_ >= 0) engine_.state().setSwitch(msgChoiceSwitch_, false);
            phase_ = Phase::Field; message_.clear();
        }
    } else {
        DrawTextU("[Enter]", (int)(box.x + box.width - 96), (int)(box.y + box.height - 28), 16, ui::kTextDim);
    }
}


} // namespace tsukuru
