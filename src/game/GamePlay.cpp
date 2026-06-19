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
    projectiles_.clear(); fx_.clear();
    loadSkills();
    spawnMonsters();
    runAutoruns();
}

void GamePlay::loadMap(int id) {
    map_ = engine_.project().map(id);
    if (!map_ && !engine_.project().maps.empty()) map_ = engine_.project().maps.front();
    engine_.state().currentMap = map_ ? map_->id : -1;
    monsters_.clear();
    weatherP_.clear();
    if (minimapValid_) { UnloadTexture(minimapTex_); minimapValid_ = false; }
    spawnNpcs();
    if (map_ && map_->bgmAsset >= 0) engine_.audio().playBgm(engine_.assetPath(map_->bgmAsset));
}

// ----------------------------- spawning -----------------------------
void GamePlay::update(float dt) {
    if (IsKeyPressed(KEY_F2)) { engine_.setMode(Mode::Editor); return; }

    if (toastTimer_ > 0) toastTimer_ -= dt;

    switch (phase_) {
        case Phase::Field: updateField(dt); break;
        case Phase::Message: {
            static const bool autodismiss = getenv("TSUKURU_AUTOWALK") != nullptr;
            if (autodismiss || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                if (msgPage_ + 1 < (int)msgPages_.size()) {
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
        case Phase::GameOver:
        case Phase::GameClear:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                engine_.setMode(Mode::Title);
            break;
    }
}

void GamePlay::updateField(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;

    static const bool autowalk = getenv("TSUKURU_AUTOWALK") != nullptr;

    if (attackTimer_ > 0) attackTimer_ -= dt;
    for (float& c : skillCd_) if (c > 0) c -= dt;
    if (playerHurt_ > 0)  playerHurt_ -= dt;

    // slow MP regeneration so skills are sustainable
    if (!engine_.state().party.empty()) {
        mpRegen_ -= dt;
        if (mpRegen_ <= 0) {
            mpRegen_ = 1.5f;
            PartyMember& h = engine_.state().party[0];
            if (h.mp < h.maxMp) h.mp = std::min(h.maxMp, h.mp + 1);
        }
    }

    updateProjectiles(dt);
    updateFx(dt);

    if (IsKeyPressed(KEY_ESCAPE)) { menu_->open(); phase_ = Phase::Menu; return; }

    // Skills: Z/Space slot0, X slot1, C slot2, V slot3, F slot4, G slot5.
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_LEFT_CONTROL))
        castSlot(0);
    if (IsKeyPressed(KEY_X)) castSlot(1);
    if (IsKeyPressed(KEY_C)) castSlot(2);
    if (IsKeyPressed(KEY_V)) castSlot(3);
    if (IsKeyPressed(KEY_F)) castSlot(4);
    if (IsKeyPressed(KEY_G)) castSlot(5);
    handleSkillClicks();                 // touch / mouse click on the skill panel
    if (IsKeyPressed(KEY_ENTER)) interact();

    // Debug autopilot: chase and attack the nearest monster (verifies combat).
    Direction autoDir = Direction::Right; bool autoMove = false;
    if (autowalk) {
        FieldMonster* near = nullptr; int best = 1000000;
        for (auto& m : monsters_) {
            if (!m.alive()) continue;
            int d = std::abs(m.x - destX_) + std::abs(m.y - destY_);
            if (d < best) { best = d; near = &m; }
        }
        if (near) {
            int ddx = near->x - destX_, ddy = near->y - destY_;
            int cheb = std::max(std::abs(ddx), std::abs(ddy));
            if (std::abs(ddx) >= std::abs(ddy) && ddx != 0)
                autoDir = ddx > 0 ? Direction::Right : Direction::Left;
            else if (ddy != 0)
                autoDir = ddy > 0 ? Direction::Down : Direction::Up;
            dir_ = (int)autoDir;
            autoMove = true;
            // attack any orthogonally-adjacent monster (face it first)
            for (auto& m : monsters_) {
                if (!m.alive()) continue;
                int adx = m.x - destX_, ady = m.y - destY_;
                if (std::abs(adx) + std::abs(ady) != 1) continue;
                dir_ = adx>0?(int)Direction::Right : adx<0?(int)Direction::Left
                     : ady>0?(int)Direction::Down  : (int)Direction::Up;
                castSlot(0);
                break;
            }
            (void)cheb;
        }
    }

    if (!moving_) {
        Direction d; bool press = true;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))         d = Direction::Up;
        else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  d = Direction::Down;
        else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  d = Direction::Left;
        else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) d = Direction::Right;
        else press = false;
        if (!press && autoMove) { d = autoDir; press = true; }
        if (press) {
            tryMove(d);
            // autowalk: if blocked, try other directions so it doesn't get stuck
            if (autowalk && !moving_) {
                Direction alts[4] = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
                for (Direction a : alts) { tryMove(a); if (moving_) break; }
            }
        }
    }

    if (moving_) {
        float tx = destX_ * (float)TS, ty = destY_ * (float)TS;
        float speed = TS * 5.0f;
        float dx = tx - pxX_, dy = ty - pxY_;
        float dist = std::sqrt(dx*dx + dy*dy);
        float step = speed * dt;
        if (dist <= step) {
            pxX_ = tx; pxY_ = ty; moving_ = false;
            engine_.state().playerX = destX_;
            engine_.state().playerY = destY_;
            if (autowalk) TraceLog(LOG_INFO, "AUTOWALK arrived at tile (%d,%d)", destX_, destY_);
            if (Event* e = map_->eventAt(destX_, destY_))
                if (e->trigger == TriggerType::PlayerTouch) runEvent(*e);
        } else {
            pxX_ += dx / dist * step;
            pxY_ += dy / dist * step;
        }
        animTime_ += dt;
        int pframes = std::max(1, engine_.project().playerFrames);
        if (animTime_ > 0.12f) { animTime_ = 0; frame_ = (frame_ + 1) % pframes; }
    } else {
        frame_ = 0;
    }
    engine_.state().playerDir = dir_;

    worldTime_ += dt;
    updateMonsters(dt);
    updateNpcs(dt);
}

void GamePlay::tryMove(Direction d) {
    dir_ = (int)d;
    Vec2i delta = dirToDelta(d);
    int nx = destX_ + delta.x, ny = destY_ + delta.y;
    if (map_->tilemap.blocked(nx, ny)) return;
    if (monsterAt(nx, ny)) return;                    // can't walk through monsters
    if (npcAt(nx, ny)) return;                        // ...or NPCs
    destX_ = nx; destY_ = ny; moving_ = true;
}

// pick the first ActionButton event at (x,y) whose condition is satisfied
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

// ----------------------------- combat -----------------------------
// Apply damage to a monster; returns true if it died (and handles the kill).
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
        case EventType::Shop: {
            const Item* it = engine_.project().database.item(e.itemId);
            if (it) {
                if (gs.inventory.gold >= it->price) {
                    gs.inventory.gold -= it->price;
                    gs.inventory.addItem(it->id, 1);
                    engine_.audio().playSfx("coin");
                    showMessage(it->name + "을(를) 구입했다!");
                } else showMessage("골드가 부족합니다...");
            } else showMessage(e.text.empty() ? "어서 오세요!" : e.text);
            break;
        }
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

// ----------------------------- rendering -----------------------------
void GamePlay::drawMessage() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle box = { 40, (float)sh - 160, (float)sw - 80, 120 };
    DrawRectangleRec(box, Fade(Color{ 20, 24, 36, 255 }, 0.95f));
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    DrawTextU(message_.c_str(), (int)box.x + 20, (int)box.y + 20, 22, ui::kText);
    DrawTextU("[Enter]", (int)(box.x + box.width - 96), (int)(box.y + box.height - 28), 16, ui::kTextDim);
}

void GamePlay::draw() {
    if (phase_ == Phase::GameClear) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        DrawRectangleGradientV(0, 0, sw, sh, Color{ 30, 30, 60, 255 }, Color{ 10, 10, 24, 255 });
        const char* a = "THE END";
        int aw = MeasureTextU(a, 72);
        DrawTextU(a, sw/2 - aw/2, sh/3, 72, Color{ 255, 220, 120, 255 });
        const char* b = "윌로우브룩에 평화가 찾아왔습니다. 플레이해 주셔서 감사합니다!";
        int bw = MeasureTextU(b, 22);
        DrawTextU(b, sw/2 - bw/2, sh/3 + 96, 22, ui::kText);
        const char* c = "Enter를 누르세요";
        DrawTextU(c, sw/2 - MeasureTextU(c,18)/2, sh/3 + 150, 18, ui::kTextDim);
        return;
    }
    if (phase_ == Phase::GameOver) {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(), Color{0,0,0,255});
        const char* go = "GAME OVER";
        int w = MeasureTextU(go, 64);
        DrawTextU(go, GetScreenWidth()/2 - w/2, GetScreenHeight()/2 - 60, 64, ui::kDanger);
        const char* sub = "Enter를 누르세요";
        int sw2 = MeasureTextU(sub, 22);
        DrawTextU(sub, GetScreenWidth()/2 - sw2/2, GetScreenHeight()/2 + 20, 22, ui::kTextDim);
        return;
    }
    drawField();
    if (phase_ == Phase::Message) drawMessage();
    if (phase_ == Phase::Menu && menu_) menu_->draw();
}

} // namespace tsukuru
