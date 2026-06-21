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
#include <filesystem>
#include <fstream>

namespace tsukuru {



GamePlay::GamePlay(Engine& engine) : engine_(engine) {
    cam_.zoom = 2.0f;
    cam_.rotation = 0;
    // Pre-size the per-frame containers so combat never reallocates mid-loop.
    monsters_.reserve(16);
    projectiles_.reserve(16);
    fx_.reserve(64);
    npcs_.reserve(32);
}
GamePlay::~GamePlay() = default;

void GamePlay::onEnter() {
    if (!menu_) menu_ = std::make_unique<Menu>(engine_);
    if (getenv("TSUKURU_INV")) invOpen_ = true;       // debug: open inventory window
    if (getenv("TSUKURU_EQUIP")) equipOpen_ = true;   // debug: open equipment window
    if (getenv("TSUKURU_BUFF")) {                      // debug: eat a buff food so the HUD badge shows
        const Database& db = engine_.project().database;
        for (const auto& it : db.items)
            if (it.kind == 1 && it.buffSecs > 0) { engine_.state().inventory.addItem(it.id, 1);
                engine_.state().consumeFood(db, it.id); break; }
    }
    firedOnce_.clear();
    GameState& gs = engine_.state();
    loadMap(gs.currentMap);
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    pxX_ = gs.playerX * (float)TS;
    pxY_ = gs.playerY * (float)TS;
    destX_ = gs.playerX; destY_ = gs.playerY;
    dir_ = gs.playerDir;
    moving_ = false;
    phase_ = Phase::Field;
    attackTimer_ = playerHurt_ = 0;
    for (float& c : skillCd_) c = 0;
    playMotion_ = MO_Walk; motionFrame_ = 0; motionAnim_ = motionTimer_ = dyingTimer_ = 0;
    projectiles_.clear(); fx_.clear();
    loadSkills();
    spawnMonsters();
    runAutoruns();
    if (getenv("TSUKURU_BATTLE") && map_ && !map_->encounterEnemies.empty())
        startEncounterBattle();   // debug: spawn an encounter troop on the field
    if (const char* s = getenv("TSUKURU_SHOP")) { // debug: open a shop (item ids "1,2,3")
        openShop({ atoi(s), 2, 8 }, "무기·도구 상점");
        if (getenv("TSUKURU_SHOPSELL")) shopMode_ = 1;   // debug: open straight to sell tab
    }
    if (getenv("TSUKURU_MSGCHOICE"))              // debug: show a 4-way choice message
        showMessageEx("어디로 가시겠어요?", "촌장", -1,
                      { "북쪽 숲", "동쪽 마을", "남쪽 항구", "그냥 머문다" }, -1, 1);
    if (getenv("TSUKURU_MSGLONG"))                // debug: long message (auto-wrap test)
        showMessageEx("이 마을은 오랜 옛날부터 예슈아한민족진리복종마을이라 불렸으며, 북쪽 산맥의 동굴 깊은 곳에는 마을을 지키는 신비한 크리스탈이 잠들어 있다고 전해진다. 자네가 그것을 되찾아 준다면 온 마을이 자네를 영웅으로 기릴 것이네.",
                      "마을 장로", -1, {}, -1, -1);
    if (getenv("TSUKURU_HELP")) helpOpen_ = true; // debug: open F1 help overlay
    if (getenv("TSUKURU_FULLMAP")) fullMapOpen_ = true; // debug: open M full-map overlay
    if (getenv("TSUKURU_DEBUGVARS")) {            // debug: seed some flags + open F3 inspector
        GameState& g = engine_.state();
        g.setSwitch(10, true); g.setSwitch(40, true); g.setVar(1, 5); g.setVar(2, 12);
        debugVarsOpen_ = true;
    }
    if (getenv("TSUKURU_QUESTLOG") && map_) {     // debug: accept all quests + open log
        GameState& gs = engine_.state();
        for (auto& e : map_->events) {
            if (e.type != EventType::Quest || e.questObjective == 0) continue;
            QuestState& q = gs.quests[GameState::questKey(map_->id, e.id)];
            q.status = 1; q.objective = e.questObjective; q.target = e.questTarget;
            q.need = std::max(1, e.questCount); q.title = e.text;
        }
        refreshQuestObjective();
        questLogOpen_ = true;
    }
    if (getenv("TSUKURU_MENU") && menu_) {                  // debug: open ESC menu
        menu_->open();
        if (const char* p = getenv("TSUKURU_MENUPAGE")) menu_->openPage(atoi(p));
        phase_ = Phase::Menu;
    }
}

void GamePlay::loadMap(int id) {
    map_ = engine_.project().map(id);
    if (!map_ && !engine_.project().maps.empty()) map_ = engine_.project().maps.front();
    engine_.state().currentMap = map_ ? map_->id : -1;
    if (map_) { engine_.state().markReached(map_->id); refreshQuestObjective(); } // 도달형 퀘스트
    if (map_ && !map_->name.empty()) { areaBanner_ = map_->name; areaBannerT_ = 2.5f; } // 지역명 배너
    monsters_.clear();
    popups_.clear();
    fx_.clear();
    projectiles_.clear();
    weatherP_.clear();
    if (minimapValid_) { UnloadTexture(minimapTex_); minimapValid_ = false; }
    // Cache the map's animated-tile ids once so drawField() doesn't rebuild a
    // hash set every frame it shows the "+1" animation phase.
    animTileSet_.clear();
    if (map_) animTileSet_.insert(map_->animTiles.begin(), map_->animTiles.end());
    spawnNpcs();
    carveZoneGates();      // ensure the middle-edge gates toward neighbours are walkable
    if (map_ && map_->bgmAsset >= 0) engine_.audio().playBgm(engine_.assetPath(map_->bgmAsset));

    // Autosave on genuine map transitions (not the very first load / quickload),
    // so progress survives even if the player never opens the save menu.
    if (autosaveArmed_) autoSave();
    autosaveArmed_ = true;
}

// Silent background save written whenever the player crosses into a new map.
void GamePlay::autoSave() {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(engine_.project().dir) / "save";
    std::error_code ec; fs::create_directories(dir, ec);
    std::ofstream f((dir / "auto.json").string());
    if (f) {
        f << engine_.state().toJson().dump(2);
        toast_ = "자동 저장됨"; toastTimer_ = 1.2f;
    }
}

// ----------------------------- lifecycle / dispatch -----------------------------
// F9: write the live game state to save/quick.json (instant, no menu).
void GamePlay::quickSave() {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(engine_.project().dir) / "save";
    std::error_code ec; fs::create_directories(dir, ec);
    std::ofstream f((dir / "quick.json").string());
    if (f) {
        f << engine_.state().toJson().dump(2);
        engine_.audio().playSfx("select", 0.7f);
        toast_ = "퀵세이브 완료 (F9)"; toastTimer_ = 1.6f;
    } else {
        toast_ = "퀵세이브 실패"; toastTimer_ = 1.6f;
    }
}

// F12: restore the quick save, if one exists.
void GamePlay::quickLoad() {
    namespace fs = std::filesystem;
    fs::path f = fs::path(engine_.project().dir) / "save" / "quick.json";
    std::error_code ec;
    if (!fs::exists(f, ec)) { toast_ = "퀵세이브가 없습니다"; toastTimer_ = 1.6f; return; }
    std::ifstream in(f.string());
    if (!in) { toast_ = "퀵로드 실패"; toastTimer_ = 1.6f; return; }
    nlohmann::json j; in >> j;
    engine_.state().fromJson(j);
    loadMap(engine_.state().currentMap);
    phase_ = Phase::Field;
    engine_.audio().playSfx("select", 0.7f);
    toast_ = "퀵로드 완료 (F12)"; toastTimer_ = 1.6f;
}

void GamePlay::update(float dt) {
    if (IsKeyPressed(KEY_F2)) { engine_.setMode(Mode::Editor); return; }
    if (IsKeyPressed(KEY_F3)) debugVarsOpen_ = !debugVarsOpen_;   // switch/variable inspector
    if (IsKeyPressed(KEY_F1)) helpOpen_ = !helpOpen_;             // controls help
    if (IsKeyPressed(KEY_F9))  quickSave();                        // 퀵세이브
    if (IsKeyPressed(KEY_F12)) quickLoad();                        // 퀵로드

    if (toastTimer_ > 0) toastTimer_ -= dt;
    if (areaBannerT_ > 0) areaBannerT_ -= dt;
    if (phase_ != Phase::GameOver && phase_ != Phase::GameClear)
        engine_.state().playSeconds += dt;   // accumulate play time for save metadata

    switch (phase_) {
        case Phase::Field: updateField(dt); break;
        case Phase::Message: {
            static const bool autodismiss = getenv("TSUKURU_AUTOWALK") != nullptr;
            bool lastPage = msgPage_ + 1 >= (int)msgPages_.size();
            bool choicePend = lastPage && (int)msgChoices_.size() >= 2;
            if (choicePend) break;   // wait for the player to click a choice (drawMessage)
            if (autodismiss || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                if (!lastPage) {
                    ++msgPage_;
                    message_ = msgPages_[msgPage_];
                    engine_.audio().playSfx("select", 0.5f);
                } else {
                    phase_ = Phase::Field;
                    message_.clear();
                }
            }
            break;
        }
        case Phase::Menu:
            if (menu_ && !menu_->update(dt)) phase_ = Phase::Field;
            break;
        case Phase::Shop:   updateShop(dt); break;
        case Phase::GameOver:
        case Phase::GameClear:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                engine_.setMode(Mode::Title);
            break;
    }
}

void GamePlay::draw() {
    if (phase_ == Phase::GameClear) {
        int sw = screenW(), sh = screenH();
        DrawRectangleGradientV(0, 0, sw, sh, Color{ 30, 30, 60, 255 }, Color{ 10, 10, 24, 255 });
        const char* a = "THE END";
        int aw = MeasureTextU(a, 72);
        DrawTextU(a, sw/2 - aw/2, sh/3, 72, Color{ 255, 220, 120, 255 });
        const char* b = "예슈아한민족진리복종마을에 평화가 찾아왔습니다. 플레이해 주셔서 감사합니다!";
        int bw = MeasureTextU(b, 22);
        DrawTextU(b, sw/2 - bw/2, sh/3 + 96, 22, ui::kText);
        const char* c = "Enter를 누르세요";
        DrawTextU(c, sw/2 - MeasureTextU(c,18)/2, sh/3 + 150, 18, ui::kTextDim);
        return;
    }
    if (phase_ == Phase::GameOver) {
        DrawRectangle(0,0,screenW(),screenH(), Color{0,0,0,255});
        const char* go = "GAME OVER";
        int w = MeasureTextU(go, 64);
        DrawTextU(go, screenW()/2 - w/2, screenH()/2 - 60, 64, ui::kDanger);
        const char* sub = "Enter를 누르세요";
        int sw2 = MeasureTextU(sub, 22);
        DrawTextU(sub, screenW()/2 - sw2/2, screenH()/2 + 20, 22, ui::kTextDim);
        return;
    }
    if (phase_ == Phase::Shop)   { drawField(); drawShop(); return; }
    drawField();
    if (fullMapOpen_) drawFullMap();
    if (invOpen_)   drawInventoryOverlay();
    if (equipOpen_) drawEquipOverlay();
    if (questLogOpen_) drawQuestLog();
    if (debugVarsOpen_) drawDebugVars();
    if (helpOpen_) drawHelp();
    if (phase_ == Phase::Message) drawMessage();
    if (phase_ == Phase::Menu && menu_) menu_->draw();
}

} // namespace tsukuru
