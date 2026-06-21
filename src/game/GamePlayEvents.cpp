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
    if (!e.enabled) return false;
    if (e.conditionSwitch >= 0 && gs.getSwitch(e.conditionSwitch) != e.conditionValue) return false;
    if (e.conditionVar >= 0 && gs.getVar(e.conditionVar) < e.conditionVarMin) return false;
    if (e.conditionItemId >= 0 && gs.inventory.count(e.conditionItemId) < e.conditionItemCount) return false;
    if (e.conditionGold > 0 && gs.inventory.gold < e.conditionGold) return false;
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
        // turn the NPC to look back at the player (opposite of the player's facing)
        static const int opposite[4] = { 3, 2, 1, 0 }; // Down<->Up, Left<->Right
        n->dir = opposite[dir_ & 3];
        n->moving = false;
        Event* best = actionEventAt(n->x, n->y);
        if (best) { runEvent(*best); return; }
    }
    Event* e = actionEventAt(fx, fy);
    if (!e) e = actionEventAt(destX_, destY_);
    if (e) runEvent(*e);
}

// ----------------------------- message box -----------------------------
static int utf8Len(unsigned char c) {
    if (c < 0x80) return 1; if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3; if ((c >> 3) == 0x1E) return 4; return 1;
}
// Wrap a string to maxW pixels, breaking at spaces when possible and at glyph
// boundaries otherwise (so Korean, which can run without spaces, still wraps).
static std::vector<std::string> wrapToWidth(const std::string& s, int maxW, int fontSize) {
    std::vector<std::string> out;
    std::string line; int lastSpace = -1;
    for (size_t i = 0; i < s.size(); ) {
        int n = utf8Len((unsigned char)s[i]);
        std::string ch = s.substr(i, n); i += n;
        if (ch == "\n") { out.push_back(line); line.clear(); lastSpace = -1; continue; }
        std::string trial = line + ch;
        if (!line.empty() && MeasureTextU(trial.c_str(), fontSize) > maxW) {
            if (ch == " ") { out.push_back(line); line.clear(); lastSpace = -1; continue; }
            if (lastSpace >= 0) { out.push_back(line.substr(0, lastSpace)); line = line.substr(lastSpace + 1); lastSpace = -1; }
            else { out.push_back(line); line.clear(); }
            line += ch;
        } else {
            if (ch == " ") lastSpace = (int)line.size();
            line += ch;
        }
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

void GamePlay::showMessage(const std::string& text) {
    showMessageEx(text, "", -1, {}, -1, -1);
}

void GamePlay::showMessageEx(const std::string& text, const std::string& speaker, int faceAsset,
                             const std::vector<std::string>& choices, int choiceSwitch, int choiceVar) {
    msgSpeaker_ = speaker; msgFace_ = faceAsset;
    msgChoices_.clear();
    for (const auto& c : choices) if (!c.empty()) msgChoices_.push_back(c);
    bool hasChoice = msgChoices_.size() >= 2;           // need at least 2 options
    msgChoiceSwitch_ = hasChoice ? choiceSwitch : -2;   // -2 = no choice
    msgChoiceVar_ = hasChoice ? choiceVar : -1;

    // Build pages: split on explicit '|', then word-wrap each segment to the box
    // width and group wrapped lines into pages of at most 3 lines.
    const int maxW = screenW() - 80 - 40;       // box width minus padding
    const int maxLines = 3;
    msgPages_.clear();
    size_t start = 0;
    while (true) {
        size_t bar = text.find('|', start);
        std::string seg = text.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
        std::vector<std::string> lines = wrapToWidth(seg, maxW, 22);
        if (lines.empty()) lines.push_back("");
        for (size_t i = 0; i < lines.size(); i += maxLines) {
            std::string page;
            for (size_t j = i; j < lines.size() && j < i + maxLines; ++j)
                page += (j > i ? "\n" : "") + lines[j];
            msgPages_.push_back(page);
        }
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
    if (!eventConditionMet(gs, e)) return;
    long key = ((long)map_->id << 16) | (e.id & 0xffff);
    if (e.once && firedOnce_.count(key)) return;
    gs.addTalkProgress(e.id);   // "NPC와 대화" 퀘스트 진행 (말 건 이벤트 기준)
    refreshQuestObjective();
    if (!e.sfx.empty()) engine_.audio().playSfx(e.sfx);   // 이벤트별 효과음

    switch (e.type) {
        case EventType::Message:
            showMessageEx(e.text, e.speakerName, e.faceAsset,
                          { e.choiceA, e.choiceB, e.choiceC, e.choiceD }, e.choiceSwitch, e.choiceVar);
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
            for (const auto& gi : e.giveItems)                              // 추가 아이템(상자 등)
                if (gi.first >= 0) { if (gi.second < 0) gs.inventory.removeItem(gi.first, -gi.second);
                                     else gs.inventory.addItem(gi.first, gi.second); }
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
            // Always field combat: spawn the troop as live monsters near the event,
            // then (if any) show the dialogue — "대화 후 교전".
            std::vector<int> troop = e.battleEnemies;
            if (troop.empty()) for (int i = 0; i < std::max(1, e.amount); ++i) troop.push_back(e.itemId);
            const Database& db = engine_.project().database;
            int TS = map_->tileset.tileWidth;
            for (int i = 0; i < (int)troop.size(); ++i) {
                const EnemyDef* def = db.enemy(troop[i]);
                if (!def) continue;
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
            if (!e.text.empty()) showMessage(e.text);   // 교전 전 대사
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
        case EventType::Heal:
            for (auto& mm : gs.party) {
                mm.hp = mm.maxHp; mm.mp = mm.maxMp;
                mm.hunger = mm.maxHunger; mm.thirst = mm.maxThirst;
            }
            engine_.audio().playSfx("levelup");
            showMessage(e.text.empty() ? "충분히 쉬어 기운을 모두 회복했다!" : e.text);
            break;
    }
    if (e.once) firedOnce_.insert(key);
}

// ----------------------------- shop (multi-item) -----------------------------
void GamePlay::openShop(const std::vector<int>& items, const std::string& title) {
    shopItems_ = items;
    shopTitle_ = title;
    shopMode_ = 0;
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

    // In sell mode the rows come from the player's inventory (sellable = priced items).
    std::vector<std::pair<int,int>> sellRows; // (itemId, owned count)
    if (shopMode_ == 1) {
        for (const auto& pr : gs.inventory.list()) {
            const Item* it = db.item(pr.first);
            if (it && it->price > 0) sellRows.push_back(pr);
        }
    }
    int rows = std::max(1, shopMode_ == 0 ? (int)shopItems_.size() : (int)sellRows.size());
    float listH = rows * 64.0f;
    Rectangle box = { sw/2.0f - 280, sh/2.0f - (listH + 170) / 2, 560, listH + 170 };
    ui::panel(box);
    DrawTextU(shopTitle_.c_str(), (int)box.x + 18, (int)box.y + 14, 24, ui::kAccent);
    DrawTextU(TextFormat("골드: %d", gs.inventory.gold), (int)(box.x + box.width - 170), (int)box.y + 18, 18, Color{230,200,90,255});

    // 구매 / 판매 toggle
    Rectangle buyTab  = { box.x + 18, box.y + 48, 130, 34 };
    Rectangle sellTab = { box.x + 154, box.y + 48, 130, 34 };
    if (ui::button(buyTab,  "구매", shopMode_ == 0)) shopMode_ = 0;
    if (ui::button(sellTab, "판매", shopMode_ == 1)) shopMode_ = 1;

    float ry = box.y + 92;
    if (shopMode_ == 0) {
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
    } else {
        if (sellRows.empty())
            DrawTextU("팔 수 있는 물건이 없습니다.", (int)box.x + 18, (int)ry, 18, ui::kTextDim);
        for (const auto& pr : sellRows) {
            const Item* it = db.item(pr.first);
            if (!it) continue;
            int sellPrice = std::max(1, it->price / 2);
            Rectangle row = { box.x + 16, ry, box.width - 32, 56 };
            DrawRectangleRec(row, Color{26,30,40,255});
            DrawRectangleLinesEx(row, 1, Fade(ui::kAccent, 0.5f));
            if (it->iconAsset >= 0) {
                const Texture2D& tx = engine_.assetTexture(it->iconAsset);
                DrawTexturePro(tx, {0,0,(float)tx.width,(float)tx.height}, {row.x+6,row.y+6,44,44}, {0,0},0,WHITE);
            }
            DrawTextU(it->name.c_str(), (int)row.x + 58, (int)row.y + 8, 18, ui::kText);
            DrawTextU(TextFormat("%d G   보유 %d", sellPrice, pr.second),
                      (int)row.x + 58, (int)row.y + 32, 14, Color{230,200,90,255});
            Rectangle sellB = { row.x + row.width - 110, row.y + 12, 96, 32 };
            if (ui::button(sellB, "판매", false)) {
                gs.inventory.removeItem(it->id, 1);
                gs.inventory.gold += sellPrice;
                engine_.audio().playSfx("coin");
                toast_ = it->name + " 판매! +" + std::to_string(sellPrice) + "G"; toastTimer_ = 1.2f;
            }
            ry += 64;
        }
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
        case 4: return q.count >= 1;                                                  // NPC 대화
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
        case 4: return std::string("NPC와 대화") + (q.count >= 1 ? " (완료)" : "");
        default: return "보상 받기";
    }
}

void GamePlay::grantQuestReward(const Event& e) {
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    std::string r;
    if (e.rewardGold > 0) { gs.inventory.gold += e.rewardGold; r += TextFormat("골드 +%d   ", e.rewardGold); }
    int beforeLv = gs.party.empty() ? 0 : gs.party[0].level;
    if (e.rewardExp > 0 && !gs.party.empty()) { gs.party[0].gainExp(e.rewardExp); r += TextFormat("경험치 +%d   ", e.rewardExp); }
    if (!gs.party.empty() && gs.party[0].level > beforeLv)
        r += TextFormat("★레벨 업! Lv %d   ", gs.party[0].level);
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
        else if (q.objective == 4) s = q.title.empty() ? std::string("NPC와 대화") : q.title;
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
            else if (q.objective == 4) p = std::string("- NPC와 대화") + (q.count >= 1 ? " (완료)" : "");
            else p = "- 보상 받기";
            DrawTextU(p.c_str(), (int)box.x + 36, y, 14, ui::kTextDim); y += 24;
        }
        if (y > box.y + box.height - 40) break;
    }
    if (active == 0 && done == 0)
        DrawTextU("받은 의뢰가 없습니다. NPC에게 말을 걸어보세요.", (int)box.x + 18, (int)box.y + 60, 16, ui::kTextDim);
    DrawTextU(TextFormat("진행 %d · 완료 %d", active, done), (int)box.x + 18, (int)(box.y + box.height - 30), 14, ui::kTextDim);
}

// Floating markers over interactable events so the player can tell at a glance
// what can be talked to / bought / healed at — a core RPG affordance.
void GamePlay::drawEventMarkers() {
    if (!map_) return;
    GameState& gs = engine_.state();
    int TS = map_->tileset.tileWidth;
    float bob = sinf(worldTime_ * 4.0f) * 2.0f;
    for (auto& e : map_->events) {
        if (e.trigger != TriggerType::ActionButton) continue;
        if (!eventConditionMet(gs, e)) continue;
        std::string mk; Color c = WHITE; bool bubble = false;
        if (e.type == EventType::Quest) {
            bool hasReward = e.rewardGold || e.rewardExp || e.rewardItemId >= 0;
            auto it = gs.quests.find(GameState::questKey(map_->id, e.id));
            int st = it == gs.quests.end() ? 0 : it->second.status;
            if (st == 2) continue;                                   // claimed
            if (st == 0) { if (e.questObjective == 0 && !hasReward) continue; mk = "!"; c = Color{255,210,80,255}; }
            else if (questObjectiveMet(e, it->second)) { mk = "?"; c = Color{120,230,120,255}; }
            else continue;                                           // in progress: no clutter
        } else if (e.type == EventType::Shop) { mk = "$"; c = Color{230,200,90,255}; }
        else if (e.type == EventType::Heal)  { mk = "+"; c = Color{120,230,120,255}; }
        else if (e.type == EventType::Message || e.type == EventType::GiveItem || e.type == EventType::SetSwitch)
            bubble = true;
        else continue;
        float mx = e.x * TS + TS * 0.5f, my = e.y * TS - 8 + bob;
        if (bubble) {
            DrawRectangleRounded({ mx - 9, my - 14, 18, 13 }, 0.4f, 4, Fade(WHITE, 0.92f));
            DrawTriangle({ mx - 3, my - 1 }, { mx, my + 4 }, { mx + 3, my - 1 }, Fade(WHITE, 0.92f));
            DrawCircle((int)(mx - 4), (int)(my - 8), 1.2f, BLACK);
            DrawCircle((int)mx,       (int)(my - 8), 1.2f, BLACK);
            DrawCircle((int)(mx + 4), (int)(my - 8), 1.2f, BLACK);
        } else {
            int w = MeasureTextU(mk.c_str(), 18);
            DrawTextU(mk.c_str(), (int)(mx - w / 2 + 1), (int)(my - 20) + 1, 18, Fade(BLACK, 0.6f));
            DrawTextU(mk.c_str(), (int)(mx - w / 2),     (int)(my - 20),     18, c);
        }
    }
}

// F1: a concise on-screen reference of every control (the key set has grown).
void GamePlay::drawHelp() {
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));
    Rectangle box = { sw/2.0f - 260, sh/2.0f - 240, 520, 480 };
    ui::panel(box);
    DrawTextU("조작 도움말   (F1 또는 ESC 로 닫기)", (int)box.x + 18, (int)box.y + 14, 22, ui::kAccent);
    struct Row { const char* k; const char* d; };
    static const Row rows[] = {
        { "방향키 / WASD", "이동" },
        { "Shift (이동 중)", "달리기" },
        { "Enter", "대화 / 상호작용" },
        { "Z / Space", "기본 공격(스킬1)" },
        { "X / V / F / G", "스킬 2~5" },
        { "I", "인벤토리 (우클릭=버리기)" },
        { "O", "캐릭터 (스탯·장비)" },
        { "J", "퀘스트 일지" },
        { "M", "전체 지도 보기" },
        { "ESC", "메뉴 (아이템/장비/상태/설정/저장)" },
        { "F1", "이 도움말" },
        { "F2", "에디터로 전환" },
        { "F3", "스위치/변수 보기(디버그)" },
        { "F9 / F12", "퀵세이브 / 퀵로드" },
    };
    int y = (int)box.y + 56;
    for (const auto& r : rows) {
        DrawTextU(r.k, (int)box.x + 24, y, 18, ui::kAccentHi);
        DrawTextU(r.d, (int)box.x + 230, y, 18, ui::kText);
        y += 30;
    }
}

// F3: a read-only inspector of all switches & variables — invaluable for testing
// event logic (conditions, quest flags, choice results) without guesswork.
void GamePlay::drawDebugVars() {
    GameState& gs = engine_.state();
    int sw = screenW();
    Rectangle box = { (float)sw - 320, 70, 300, 360 };
    ui::panel(box);
    DrawTextU("스위치 / 변수 (F3 닫기)", (int)box.x + 14, (int)box.y + 10, 18, ui::kAccent);
    int y = (int)box.y + 40;
    DrawTextU("─ 스위치 (ON) ─", (int)box.x + 14, y, 13, ui::kAccentHi); y += 18;
    int shown = 0;
    for (const auto& kv : gs.switches()) {
        if (!kv.second) continue;
        DrawTextU(TextFormat("#%d = ON", kv.first), (int)box.x + 22, y, 14, ui::kGood); y += 18;
        if (++shown >= 8 || y > box.y + 200) break;
    }
    if (shown == 0) { DrawTextU("(켜진 스위치 없음)", (int)box.x + 22, y, 13, ui::kTextDim); y += 18; }
    y += 6;
    DrawTextU("─ 변수 ─", (int)box.x + 14, y, 13, ui::kAccentHi); y += 18;
    int vshown = 0;
    for (const auto& kv : gs.variables()) {
        DrawTextU(TextFormat("#%d = %d", kv.first, kv.second), (int)box.x + 22, y, 14, ui::kText); y += 18;
        if (++vshown >= 8 || y > box.y + box.height - 16) break;
    }
    if (vshown == 0) DrawTextU("(변수 없음)", (int)box.x + 22, y, 13, ui::kTextDim);
}

void GamePlay::drawMessage() {
    int sw = screenW(), sh = screenH();
    Rectangle box = { 40, (float)sh - 160, (float)sw - 80, 120 };
    bool lastPage = msgPage_ + 1 >= (int)msgPages_.size();
    bool choice = lastPage && (int)msgChoices_.size() >= 2;
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
    // draw the page line-by-line (pages are pre-wrapped, '\n'-joined)
    {
        const std::string& pg = message_;
        int ly = (int)box.y + 16; size_t p = 0;
        while (p <= pg.size()) {
            size_t nl = pg.find('\n', p);
            std::string ln = pg.substr(p, nl == std::string::npos ? std::string::npos : nl - p);
            DrawTextU(ln.c_str(), (int)textX, ly, 22, ui::kText); ly += 28;
            if (nl == std::string::npos) break; p = nl + 1;
        }
    }
    if (choice) {
        // up to 4 choice buttons laid out right-to-left
        int n = (int)msgChoices_.size();
        float bw = 168, gap = 8;
        for (int i = 0; i < n; ++i) {
            Rectangle b = { box.x + box.width - (bw + gap) * (n - i), box.y + box.height - 44, bw, 34 };
            if (ui::button(b, msgChoices_[i], false)) {
                if (msgChoiceVar_ >= 0)    engine_.state().setVar(msgChoiceVar_, i);
                if (msgChoiceSwitch_ >= 0) engine_.state().setSwitch(msgChoiceSwitch_, i == 0);
                phase_ = Phase::Field; message_.clear();
            }
        }
    } else {
        DrawTextU("[Enter]", (int)(box.x + box.width - 96), (int)(box.y + box.height - 28), 16, ui::kTextDim);
    }
}


} // namespace tsukuru
