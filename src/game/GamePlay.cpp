#include "game/GamePlay.h"
#include "core/Engine.h"
#include "game/Menu.h"
#include "battle/Battle.h"
#include "render/UI.h"
#include <set>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace tsukuru {

static std::set<long> g_firedOnce; // (mapId<<16 | eventId) one-shot events this session

GamePlay::GamePlay(Engine& engine) : engine_(engine) {
    cam_.zoom = 2.0f;
    cam_.rotation = 0;
}
GamePlay::~GamePlay() = default;

void GamePlay::onEnter() {
    if (!menu_) menu_ = std::make_unique<Menu>(engine_);
    g_firedOnce.clear();
    GameState& gs = engine_.state();
    loadMap(gs.currentMap);
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    pxX_ = gs.playerX * (float)TS;
    pxY_ = gs.playerY * (float)TS;
    destX_ = gs.playerX; destY_ = gs.playerY;
    dir_ = gs.playerDir;
    moving_ = false;
    phase_ = Phase::Field;
}

void GamePlay::loadMap(int id) {
    map_ = engine_.project().map(id);
    if (!map_ && !engine_.project().maps.empty()) map_ = engine_.project().maps.front();
    engine_.state().currentMap = map_ ? map_->id : -1;
}

void GamePlay::update(float dt) {
    if (IsKeyPressed(KEY_F2)) { engine_.setMode(Mode::Editor); return; }

    switch (phase_) {
        case Phase::Field: updateField(dt); break;
        case Phase::Message:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                phase_ = Phase::Field;
                message_.clear();
            }
            break;
        case Phase::Battle: updateBattle(dt); break;
        case Phase::Menu:
            if (menu_ && !menu_->update(dt)) phase_ = Phase::Field;
            break;
        case Phase::GameOver:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                engine_.setMode(Mode::Title);
            break;
    }
}

void GamePlay::updateField(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;

    if (IsKeyPressed(KEY_ESCAPE)) { menu_->open(); phase_ = Phase::Menu; return; }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) { interact(); return; }

    if (!moving_) {
        Direction d; bool press = true;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))         d = Direction::Up;
        else if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  d = Direction::Down;
        else if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  d = Direction::Left;
        else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) d = Direction::Right;
        else press = false;
        if (press) tryMove(d);
    }

    if (moving_) {
        float tx = destX_ * (float)TS, ty = destY_ * (float)TS;
        float speed = TS * 5.0f; // tiles? 5 tiles/sec
        float dx = tx - pxX_, dy = ty - pxY_;
        float dist = std::sqrt(dx*dx + dy*dy);
        float step = speed * dt;
        if (dist <= step) {
            pxX_ = tx; pxY_ = ty; moving_ = false;
            engine_.state().playerX = destX_;
            engine_.state().playerY = destY_;
            // arrival: touch events + encounter
            if (Event* e = map_->eventAt(destX_, destY_))
                if (e->trigger == TriggerType::PlayerTouch) runEvent(*e);
            if (phase_ == Phase::Field) checkEncounter();
        } else {
            pxX_ += dx / dist * step;
            pxY_ += dy / dist * step;
        }
        animTime_ += dt;
        if (animTime_ > 0.12f) { animTime_ = 0; frame_ = (frame_ + 1) % 4; }
    } else {
        frame_ = 0;
    }
    engine_.state().playerDir = dir_;
}

void GamePlay::tryMove(Direction d) {
    dir_ = (int)d;
    Vec2i delta = dirToDelta(d);
    int nx = destX_ + delta.x, ny = destY_ + delta.y;
    if (map_->tilemap.blocked(nx, ny)) return;        // wall
    if (Event* e = map_->eventAt(nx, ny))             // solid NPC-style event
        if (e->graphicAsset >= 0 && e->trigger != TriggerType::PlayerTouch) return;
    destX_ = nx; destY_ = ny; moving_ = true;
}

void GamePlay::interact() {
    Vec2i delta = dirToDelta((Direction)dir_);
    int fx = destX_ + delta.x, fy = destY_ + delta.y;
    Event* e = map_->eventAt(fx, fy);
    if (!e) e = map_->eventAt(destX_, destY_); // stand-on
    if (e && e->trigger == TriggerType::ActionButton) runEvent(*e);
}

void GamePlay::runEvent(Event& e) {
    GameState& gs = engine_.state();
    // condition gate
    if (e.conditionSwitch >= 0 && gs.getSwitch(e.conditionSwitch) != e.conditionValue) return;
    long key = ((long)map_->id << 16) | (e.id & 0xffff);
    if (e.once && g_firedOnce.count(key)) return;

    switch (e.type) {
        case EventType::Message:
            message_ = e.text; phase_ = Phase::Message; break;
        case EventType::Teleport: {
            loadMap(e.targetMap);
            int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
            destX_ = e.targetX; destY_ = e.targetY;
            pxX_ = destX_ * (float)TS; pxY_ = destY_ * (float)TS;
            moving_ = false;
            gs.playerX = destX_; gs.playerY = destY_;
            break;
        }
        case EventType::GiveItem:
            gs.inventory.addItem(e.itemId, e.amount);
            message_ = e.text.empty() ? "Got an item!" : e.text;
            phase_ = Phase::Message;
            break;
        case EventType::SetSwitch:
            gs.setSwitch(e.switchId, e.switchValue);
            if (!e.text.empty()) { message_ = e.text; phase_ = Phase::Message; }
            break;
        case EventType::StartBattle: {
            // event.itemId reused as enemy id, event.amount as count
            std::vector<int> troop;
            int n = std::max(1, e.amount);
            for (int i = 0; i < n; ++i) troop.push_back(e.itemId);
            startBattle(troop);
            break;
        }
        case EventType::Shop: {
            const Item* it = engine_.project().database.item(e.itemId);
            if (it) {
                if (gs.inventory.gold >= it->price) {
                    gs.inventory.gold -= it->price;
                    gs.inventory.addItem(it->id, 1);
                    message_ = "Bought " + it->name + "!";
                } else message_ = "Not enough gold...";
            } else message_ = e.text.empty() ? "Welcome!" : e.text;
            phase_ = Phase::Message;
            break;
        }
    }
    if (e.once) g_firedOnce.insert(key);
}

void GamePlay::checkEncounter() {
    if (!map_ || map_->encounterRate <= 0 || map_->encounterEnemies.empty()) return;
    if (++stepsSinceEncounter_ < 3) return; // grace period
    if (std::rand() % 100 < map_->encounterRate) {
        stepsSinceEncounter_ = 0;
        std::vector<int> troop;
        int n = 1 + std::rand() % 2;
        for (int i = 0; i < n; ++i)
            troop.push_back(map_->encounterEnemies[std::rand() % map_->encounterEnemies.size()]);
        startBattle(troop);
    }
}

void GamePlay::startBattle(const std::vector<int>& enemyIds) {
    battle_ = std::make_unique<Battle>(engine_.project().database, engine_.state(), enemyIds);
    battleSelection_ = 0; battleSubSelection_ = 0; battleSubMenu_ = false;
    phase_ = Phase::Battle;
}

void GamePlay::updateBattle(float dt) {
    if (!battle_) { phase_ = Phase::Field; return; }
    Database& db = engine_.project().database;
    GameState& gs = engine_.state();

    if (battle_->result() != BattleResult::Ongoing) {
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            BattleResult r = battle_->result();
            battle_.reset();
            if (r == BattleResult::Defeat) phase_ = Phase::GameOver;
            else phase_ = Phase::Field;
        }
        return;
    }
    if (!battle_->actorReady()) return;

    PartyMember& me = gs.party[battle_->currentActor()];

    if (!battleSubMenu_) {
        const int N = 4; // Attack, Skill, Item, Flee
        if (IsKeyPressed(KEY_DOWN)) battleSelection_ = (battleSelection_ + 1) % N;
        if (IsKeyPressed(KEY_UP))   battleSelection_ = (battleSelection_ + N - 1) % N;
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (battleSelection_ == 0) { // Attack
                BattleAction a; a.kind = ActionKind::Attack; a.targetIndex = battle_->firstAliveEnemy();
                battle_->submit(a);
            } else if (battleSelection_ == 1 || battleSelection_ == 2) {
                battleSubMenu_ = true; battleSubSelection_ = 0;
            } else { // Flee
                BattleAction a; a.kind = ActionKind::Flee; battle_->submit(a);
            }
        }
    } else {
        if (IsKeyPressed(KEY_ESCAPE)) { battleSubMenu_ = false; return; }
        if (battleSelection_ == 1) { // skills
            const ActorDef* def = db.actor(me.actorId);
            int n = def ? (int)def->skills.size() : 0;
            if (n == 0) { battleSubMenu_ = false; return; }
            if (IsKeyPressed(KEY_DOWN)) battleSubSelection_ = (battleSubSelection_ + 1) % n;
            if (IsKeyPressed(KEY_UP))   battleSubSelection_ = (battleSubSelection_ + n - 1) % n;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                BattleAction a; a.kind = ActionKind::Skill; a.id = def->skills[battleSubSelection_];
                battle_->submit(a); battleSubMenu_ = false;
            }
        } else { // items
            auto items = gs.inventory.list();
            if (items.empty()) { battleSubMenu_ = false; return; }
            int n = (int)items.size();
            if (IsKeyPressed(KEY_DOWN)) battleSubSelection_ = (battleSubSelection_ + 1) % n;
            if (IsKeyPressed(KEY_UP))   battleSubSelection_ = (battleSubSelection_ + n - 1) % n;
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                BattleAction a; a.kind = ActionKind::Item; a.id = items[battleSubSelection_].first;
                battle_->submit(a); battleSubMenu_ = false;
            }
        }
    }
}

// --------------------- rendering ---------------------

void GamePlay::drawCharacter(int assetId, int dir, int frame, float px, float py) {
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    if (assetId >= 0) {
        const Texture2D& tex = engine_.assetTexture(assetId);
        float fw = tex.width / 4.0f, fh = tex.height / 4.0f;
        Rectangle src = { frame * fw, dir * fh, fw, fh };
        Rectangle dst = { px, py, (float)TS, (float)TS };
        DrawTexturePro(tex, src, dst, {0,0}, 0, WHITE);
    } else {
        // Fallback hero: body + facing marker
        DrawRectangle((int)px+6, (int)py+6, TS-12, TS-12, Color{ 80, 140, 220, 255 });
        DrawRectangleLines((int)px+6, (int)py+6, TS-12, TS-12, BLACK);
        int cx = (int)px + TS/2, cy = (int)py + TS/2;
        Vec2i d = dirToDelta((Direction)dir);
        DrawCircle(cx + d.x*6, cy + d.y*6, 3, WHITE);
    }
}

void GamePlay::drawField() {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    cam_.target = { pxX_ + TS/2.0f, pxY_ + TS/2.0f };
    cam_.offset = { GetScreenWidth()/2.0f, GetScreenHeight()/2.0f };

    const Texture2D& ts = engine_.assetTexture(map_->tileset.assetId);
    const Tileset& set = map_->tileset;

    BeginMode2D(cam_);
    // visible tile range
    int w = map_->tilemap.width(), h = map_->tilemap.height();
    for (int layer = 0; layer < kLayerCount; ++layer) {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int t = map_->tilemap.tile(layer, x, y);
                if (t < 0) continue;
                int sx, sy; set.srcOf(t, sx, sy);
                Rectangle src = { (float)sx, (float)sy, (float)set.tileWidth, (float)set.tileHeight };
                Rectangle dst = { (float)x*TS, (float)y*TS, (float)TS, (float)TS };
                DrawTexturePro(ts, src, dst, {0,0}, 0, WHITE);
            }
        }
    }
    // events with a graphic
    for (auto& e : map_->events) {
        if (e.graphicAsset >= 0)
            drawCharacter(e.graphicAsset, 0, 0, (float)e.x*TS, (float)e.y*TS);
        else if (e.type == EventType::Teleport)
            DrawRectangleLines(e.x*TS+2, e.y*TS+2, TS-4, TS-4, Fade(ui::kAccent, 0.5f));
    }
    // player
    drawCharacter(engine_.project().playerSprite, dir_, moving_ ? frame_ : 0, pxX_, pxY_);
    EndMode2D();

    // HUD
    GameState& gs = engine_.state();
    DrawRectangle(0, 0, GetScreenWidth(), 32, Fade(BLACK, 0.5f));
    if (!gs.party.empty()) {
        PartyMember& m = gs.party[0];
        DrawText(TextFormat("HP %d/%d   MP %d/%d   Gold %d   (ESC: menu, F2: editor)",
                 m.hp, m.maxHp, m.mp, m.maxMp, gs.inventory.gold), 12, 8, 16, ui::kText);
    }
}

void GamePlay::drawMessage() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle box = { 40, (float)sh - 160, (float)sw - 80, 120 };
    DrawRectangleRec(box, Fade(Color{ 20, 24, 36, 255 }, 0.95f));
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    DrawText(message_.c_str(), (int)box.x + 20, (int)box.y + 20, 22, ui::kText);
    DrawText("[Enter]", (int)(box.x + box.width - 90), (int)(box.y + box.height - 28), 16, ui::kTextDim);
}

void GamePlay::drawBattle() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangleGradientV(0, 0, sw, sh, Color{ 40, 20, 28, 255 }, Color{ 12, 8, 12, 255 });
    Database& db = engine_.project().database;
    GameState& gs = engine_.state();

    // enemies
    const auto& enemies = battle_->enemies();
    int ex = sw/2 - (int)enemies.size()*70;
    for (int i = 0; i < (int)enemies.size(); ++i) {
        const auto& e = enemies[i];
        int x = ex + i*140, y = 120;
        Color tint = e.alive() ? WHITE : Fade(RED, 0.3f);
        const EnemyDef* def = db.enemy(e.enemyId);
        if (def && def->spriteAsset >= 0) {
            const Texture2D& tex = engine_.assetTexture(def->spriteAsset);
            DrawTextureEx(tex, {(float)x, (float)y}, 0, 96.0f / std::max(1, tex.width), tint);
        } else {
            DrawCircle(x + 48, y + 48, 40, e.alive() ? Color{180,80,80,255} : Fade(GRAY,0.4f));
        }
        DrawText(e.name.c_str(), x, y + 100, 16, ui::kText);
        DrawText(TextFormat("HP %d/%d", e.hp < 0 ? 0 : e.hp, e.maxHp), x, y + 120, 14, ui::kGood);
    }

    // party status
    DrawRectangle(0, sh - 200, sw, 200, Fade(Color{ 16, 18, 26, 255 }, 0.95f));
    int py = sh - 188;
    for (auto& m : gs.party) {
        const ActorDef* def = db.actor(m.actorId);
        DrawText(TextFormat("%s  HP %d/%d  MP %d/%d  Lv %d",
                 def ? def->name.c_str() : "Hero", m.hp, m.maxHp, m.mp, m.maxMp, m.level),
                 sw/2 + 20, py, 18, m.alive() ? ui::kText : ui::kDanger);
        py += 26;
    }

    // command menu
    if (battle_->result() == BattleResult::Ongoing && battle_->actorReady()) {
        const char* cmds[4] = { "Attack", "Skill", "Item", "Flee" };
        for (int i = 0; i < 4; ++i) {
            Color c = (i == battleSelection_ && !battleSubMenu_) ? ui::kAccentHi : ui::kText;
            DrawText(TextFormat("%s%s", (i == battleSelection_ && !battleSubMenu_) ? "> " : "  ", cmds[i]),
                     30, sh - 180 + i*34, 22, c);
        }
        if (battleSubMenu_) {
            DrawRectangle(180, sh - 200, 260, 200, Fade(BLACK, 0.85f));
            PartyMember& me = gs.party[battle_->currentActor()];
            if (battleSelection_ == 1) {
                const ActorDef* def = db.actor(me.actorId);
                if (def) for (int i = 0; i < (int)def->skills.size(); ++i) {
                    const Skill* sk = db.skill(def->skills[i]);
                    Color c = i == battleSubSelection_ ? ui::kAccentHi : ui::kText;
                    DrawText(TextFormat("%s%s (MP%d)", i==battleSubSelection_?"> ":"  ",
                             sk?sk->name.c_str():"?", sk?sk->mpCost:0), 195, sh-190+i*30, 18, c);
                }
            } else {
                auto items = gs.inventory.list();
                for (int i = 0; i < (int)items.size(); ++i) {
                    const Item* it = db.item(items[i].first);
                    Color c = i == battleSubSelection_ ? ui::kAccentHi : ui::kText;
                    DrawText(TextFormat("%s%s x%d", i==battleSubSelection_?"> ":"  ",
                             it?it->name.c_str():"?", items[i].second), 195, sh-190+i*30, 18, c);
                }
            }
        }
    } else {
        const char* msg = battle_->result() == BattleResult::Victory ? "VICTORY!  [Enter]"
                        : battle_->result() == BattleResult::Defeat  ? "DEFEAT...  [Enter]"
                        : "Escaped!  [Enter]";
        DrawText(msg, 30, sh - 150, 28, ui::kAccentHi);
    }

    // battle log (last few lines)
    const auto& log = battle_->log();
    int n = (int)log.size();
    for (int i = 0; i < 4 && i < n; ++i)
        DrawText(log[n-1-i].c_str(), 30, 320 + (3-i)*22, 16, Fade(ui::kText, 0.5f + 0.12f*i));
}

void GamePlay::draw() {
    if (phase_ == Phase::Battle && battle_) { drawBattle(); return; }
    if (phase_ == Phase::GameOver) {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(), Color{0,0,0,255});
        const char* go = "GAME OVER";
        int w = MeasureText(go, 64);
        DrawText(go, GetScreenWidth()/2 - w/2, GetScreenHeight()/2 - 60, 64, ui::kDanger);
        const char* sub = "Press Enter";
        int sw2 = MeasureText(sub, 22);
        DrawText(sub, GetScreenWidth()/2 - sw2/2, GetScreenHeight()/2 + 20, 22, ui::kTextDim);
        return;
    }
    drawField();
    if (phase_ == Phase::Message) drawMessage();
    if (phase_ == Phase::Menu && menu_) menu_->draw();
}

} // namespace tsukuru
