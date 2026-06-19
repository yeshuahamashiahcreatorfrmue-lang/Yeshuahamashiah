#include "game/GamePlay.h"
#include "core/Engine.h"
#include "game/Menu.h"
#include "render/UI.h"
#include "database/Database.h"
#include <set>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace tsukuru {

static std::set<long> g_firedOnce; // (mapId<<16 | eventId) one-shot events this session

static int isign(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

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
    attackTimer_ = attackCd_ = playerHurt_ = 0;
    spawnMonsters();
}

void GamePlay::loadMap(int id) {
    map_ = engine_.project().map(id);
    if (!map_ && !engine_.project().maps.empty()) map_ = engine_.project().maps.front();
    engine_.state().currentMap = map_ ? map_->id : -1;
    monsters_.clear();
}

// ----------------------------- spawning -----------------------------
void GamePlay::spawnMonsters() {
    monsters_.clear();
    if (!map_) { targetMonsters_ = 0; return; }
    const Database& db = engine_.project().database;
    bool haveEnemies = !map_->encounterEnemies.empty() || !db.enemies.empty();
    if (!haveEnemies) { targetMonsters_ = 0; return; }
    int area = map_->tilemap.width() * map_->tilemap.height();
    targetMonsters_ = std::min(8, std::max(3, area / 45));
    for (int i = 0; i < targetMonsters_; ++i) spawnOne();
}

void GamePlay::spawnOne() {
    if (!map_) return;
    const Database& db = engine_.project().database;
    std::vector<int> pool = map_->encounterEnemies;
    if (pool.empty()) for (const auto& e : db.enemies) pool.push_back(e.id);
    if (pool.empty()) return;
    int enemyId = pool[std::rand() % pool.size()];
    const EnemyDef* def = db.enemy(enemyId);
    if (!def) return;

    int TS = map_->tileset.tileWidth;
    for (int tries = 0; tries < 40; ++tries) {
        int x = std::rand() % map_->tilemap.width();
        int y = std::rand() % map_->tilemap.height();
        if (!walkable(x, y)) continue;
        if (std::abs(x - destX_) + std::abs(y - destY_) < 4) continue; // not on top of player
        FieldMonster m;
        m.enemyId = def->id; m.name = def->name; m.spriteAsset = def->spriteAsset;
        m.x = m.destX = x; m.y = m.destY = y;
        m.px = x * (float)TS; m.py = y * (float)TS;
        m.hp = m.maxHp = def->maxHp;
        m.atk = def->atk; m.def = def->def;
        m.expReward = def->expReward; m.goldReward = def->goldReward;
        m.moveCd = 0.3f + (std::rand() % 100) / 100.0f;
        monsters_.push_back(m);
        return;
    }
}

FieldMonster* GamePlay::monsterAt(int x, int y) {
    for (auto& m : monsters_) if (m.alive() && m.x == x && m.y == y) return &m;
    return nullptr;
}

bool GamePlay::walkable(int x, int y) {
    if (!map_ || !map_->tilemap.inBounds(x, y)) return false;
    if (map_->tilemap.blocked(x, y)) return false;
    if (monsterAt(x, y)) return false;
    if (Event* e = map_->eventAt(x, y))
        if (e->graphicAsset >= 0 && e->trigger != TriggerType::PlayerTouch) return false;
    return true;
}

// ----------------------------- update -----------------------------
void GamePlay::update(float dt) {
    if (IsKeyPressed(KEY_F2)) { engine_.setMode(Mode::Editor); return; }

    if (toastTimer_ > 0) toastTimer_ -= dt;

    switch (phase_) {
        case Phase::Field: updateField(dt); break;
        case Phase::Message:
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ESCAPE)) {
                phase_ = Phase::Field;
                message_.clear();
            }
            break;
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

    static const bool autowalk = getenv("TSUKURU_AUTOWALK") != nullptr;

    if (attackTimer_ > 0) attackTimer_ -= dt;
    if (attackCd_ > 0)    attackCd_ -= dt;
    if (playerHurt_ > 0)  playerHurt_ -= dt;

    if (IsKeyPressed(KEY_ESCAPE)) { menu_->open(); phase_ = Phase::Menu; return; }

    // Space / Z = attack (or talk if nothing to hit). Enter = talk only.
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_LEFT_CONTROL))
        playerAttack();
    else if (IsKeyPressed(KEY_ENTER))
        interact();

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
            if (std::abs(ddx) >= std::abs(ddy) && ddx != 0)
                autoDir = ddx > 0 ? Direction::Right : Direction::Left;
            else if (ddy != 0)
                autoDir = ddy > 0 ? Direction::Down : Direction::Up;
            dir_ = (int)autoDir;
            autoMove = true;
            if (best <= 1) playerAttack();
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
        if (press) tryMove(d);
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
        if (animTime_ > 0.12f) { animTime_ = 0; frame_ = (frame_ + 1) % 4; }
    } else {
        frame_ = 0;
    }
    engine_.state().playerDir = dir_;

    updateMonsters(dt);
}

void GamePlay::tryMove(Direction d) {
    dir_ = (int)d;
    Vec2i delta = dirToDelta(d);
    int nx = destX_ + delta.x, ny = destY_ + delta.y;
    if (map_->tilemap.blocked(nx, ny)) return;
    if (monsterAt(nx, ny)) return;                    // can't walk through monsters
    if (Event* e = map_->eventAt(nx, ny))             // solid NPC-style event
        if (e->graphicAsset >= 0 && e->trigger != TriggerType::PlayerTouch) return;
    destX_ = nx; destY_ = ny; moving_ = true;
}

void GamePlay::interact() {
    Vec2i delta = dirToDelta((Direction)dir_);
    int fx = destX_ + delta.x, fy = destY_ + delta.y;
    Event* e = map_->eventAt(fx, fy);
    if (!e) e = map_->eventAt(destX_, destY_);
    if (e && e->trigger == TriggerType::ActionButton) runEvent(*e);
}

// ----------------------------- combat -----------------------------
void GamePlay::playerAttack() {
    if (attackCd_ > 0) return;
    attackCd_ = 0.32f;
    attackTimer_ = 0.18f;

    Vec2i delta = dirToDelta((Direction)dir_);
    int fx = destX_ + delta.x, fy = destY_ + delta.y;

    const Database& db = engine_.project().database;
    int atk = engine_.state().party.empty() ? 10 : engine_.state().party[0].totalAtk(db);

    bool hit = false;
    for (auto& m : monsters_) {
        if (!m.alive()) continue;
        if ((m.x == fx && m.y == fy) || (m.x == destX_ && m.y == destY_)) {
            int dmg = std::max(1, atk - m.def);
            m.hp -= dmg;
            m.hurtFlash = 0.18f;
            hit = true;
            if (m.hp <= 0) onMonsterKilled(m);
        }
    }
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                    [](const FieldMonster& m){ return !m.alive(); }), monsters_.end());

    if (!hit) interact(); // nothing to hit -> talk to an NPC in front
}

void GamePlay::onMonsterKilled(const FieldMonster& m) {
    GameState& gs = engine_.state();
    gs.inventory.gold += m.goldReward;
    for (auto& p : gs.party) if (p.alive()) p.gainExp(m.expReward);
    toast_ = m.name + " defeated!  +" + std::to_string(m.expReward) + " EXP  +" +
             std::to_string(m.goldReward) + " G";
    toastTimer_ = 1.8f;
    TraceLog(LOG_INFO, "KILL: %s  party gold=%d exp=%d lv=%d", m.name.c_str(),
             gs.inventory.gold, gs.party.empty()?0:gs.party[0].exp,
             gs.party.empty()?0:gs.party[0].level);
}

void GamePlay::updateMonsters(float dt) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    GameState& gs = engine_.state();
    const Database& db = engine_.project().database;
    int pdef = gs.party.empty() ? 0 : gs.party[0].totalDef(db);

    // maintain population
    if ((int)monsters_.size() < targetMonsters_) {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0) { spawnOne(); spawnTimer_ = 2.5f; }
    }

    for (auto& m : monsters_) {
        if (m.hurtFlash > 0) m.hurtFlash -= dt;
        if (m.atkCd > 0)     m.atkCd -= dt;

        // smooth move
        if (m.moving) {
            float tx = m.destX * (float)TS, ty = m.destY * (float)TS;
            float dx = tx - m.px, dy = ty - m.py;
            float dist = std::sqrt(dx*dx + dy*dy);
            float step = TS * 3.0f * dt;
            if (dist <= step) { m.px = tx; m.py = ty; m.x = m.destX; m.y = m.destY; m.moving = false; }
            else { m.px += dx / dist * step; m.py += dy / dist * step; }
        } else {
            m.moveCd -= dt;
            if (m.moveCd <= 0) {
                m.moveCd = 0.45f + (std::rand() % 60) / 100.0f;
                int cheb = std::max(std::abs(m.x - destX_), std::abs(m.y - destY_));
                int ddx = 0, ddy = 0;
                if (cheb <= 6) {                  // chase the player
                    ddx = isign(destX_ - m.x);
                    ddy = isign(destY_ - m.y);
                } else if (std::rand() % 3 != 0) { // wander
                    int r = std::rand() % 4;
                    ddx = (r == 0) - (r == 1);
                    ddy = (r == 2) - (r == 3);
                }
                // try preferred axis first, then the other
                int tries[4][2] = { {ddx,0}, {0,ddy}, {ddy,ddx}, {-ddx,-ddy} };
                for (auto& t : tries) {
                    if (t[0] == 0 && t[1] == 0) continue;
                    int nx = m.x + isign(t[0]), ny = m.y + isign(t[1]);
                    if (nx == destX_ && ny == destY_) continue; // don't step onto player
                    if (walkable(nx, ny)) {
                        m.destX = nx; m.destY = ny; m.moving = true;
                        m.dir = t[1] > 0 ? 0 : t[1] < 0 ? 3 : t[0] < 0 ? 1 : 2;
                        break;
                    }
                }
            }
        }

        // attack the player when adjacent
        int cheb = std::max(std::abs(m.x - destX_), std::abs(m.y - destY_));
        if (cheb <= 1 && m.atkCd <= 0 && !gs.party.empty()) {
            m.atkCd = 1.1f;
            int dmg = std::max(1, m.atk - pdef);
            PartyMember& hero = gs.party[0];
            hero.hp = std::max(0, hero.hp - dmg);
            playerHurt_ = 0.22f;
            if (gs.partyWiped()) { phase_ = Phase::GameOver; return; }
        }
    }
}

void GamePlay::runEvent(Event& e) {
    GameState& gs = engine_.state();
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
            spawnMonsters();
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
                    message_ = "Bought " + it->name + "!";
                } else message_ = "Not enough gold...";
            } else message_ = e.text.empty() ? "Welcome!" : e.text;
            phase_ = Phase::Message;
            break;
        }
    }
    if (e.once) g_firedOnce.insert(key);
}

// ----------------------------- rendering -----------------------------
void GamePlay::drawCharacter(int assetId, int dir, int frame, float px, float py, Color tint) {
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    if (assetId >= 0) {
        const Texture2D& tex = engine_.assetTexture(assetId);
        float fw = tex.width / 4.0f, fh = tex.height / 4.0f;
        Rectangle src = { frame * fw, dir * fh, fw, fh };
        Rectangle dst = { px, py, (float)TS, (float)TS };
        DrawTexturePro(tex, src, dst, {0,0}, 0, tint);
    } else {
        DrawRectangle((int)px+6, (int)py+6, TS-12, TS-12, Color{ 80, 140, 220, 255 });
        DrawRectangleLines((int)px+6, (int)py+6, TS-12, TS-12, BLACK);
        int cx = (int)px + TS/2, cy = (int)py + TS/2;
        Vec2i d = dirToDelta((Direction)dir);
        DrawCircle(cx + d.x*6, cy + d.y*6, 3, WHITE);
    }
}

void GamePlay::drawMonsters() {
    int TS = map_->tileset.tileWidth;
    for (auto& m : monsters_) {
        Color tint = m.hurtFlash > 0 ? Color{ 255, 120, 120, 255 } : WHITE;
        if (m.spriteAsset >= 0) {
            const Texture2D& tex = engine_.assetTexture(m.spriteAsset);
            float size = TS * 1.15f;
            Rectangle src = { 0, 0, (float)tex.width, (float)tex.height };
            Rectangle dst = { m.px + (TS - size)/2, m.py + (TS - size)/2, size, size };
            DrawTexturePro(tex, src, dst, {0,0}, 0, tint);
        } else {
            DrawCircle((int)m.px + TS/2, (int)m.py + TS/2, TS*0.4f,
                       m.hurtFlash > 0 ? RED : Color{ 180, 90, 90, 255 });
        }
        // HP bar when damaged
        if (m.hp < m.maxHp) {
            float w = TS, ratio = (float)m.hp / m.maxHp;
            DrawRectangle((int)m.px, (int)m.py - 7, (int)w, 4, Fade(BLACK, 0.6f));
            DrawRectangle((int)m.px, (int)m.py - 7, (int)(w * ratio), 4, Color{ 220, 80, 80, 255 });
        }
    }
}

void GamePlay::drawField() {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    int w = map_->tilemap.width(), h = map_->tilemap.height();

    // follow the player, but clamp so the view never shows past the map edges
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float halfW = sw / (2.0f * cam_.zoom), halfH = sh / (2.0f * cam_.zoom);
    float mapW = w * (float)TS, mapH = h * (float)TS;
    float tgx = pxX_ + TS/2.0f, tgy = pxY_ + TS/2.0f;
    if (mapW > 2*halfW) tgx = std::min(std::max(tgx, halfW), mapW - halfW); else tgx = mapW/2;
    if (mapH > 2*halfH) tgy = std::min(std::max(tgy, halfH), mapH - halfH); else tgy = mapH/2;
    cam_.target = { tgx, tgy };
    cam_.offset = { sw/2.0f, sh/2.0f };

    const Texture2D& ts = engine_.assetTexture(map_->tileset.assetId);
    const Tileset& set = map_->tileset;

    // animated tiles: cycle base id <-> id+1
    int phase = (int)(GetTime() * 2.5) % 2;

    BeginMode2D(cam_);
    auto drawLayer = [&](int layer) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                int t = map_->tilemap.tile(layer, x, y);
                if (t < 0) continue;
                if (phase == 1)
                    for (int a : map_->animTiles) if (a == t) { t = t + 1; break; }
                int sx, sy; set.srcOf(t, sx, sy);
                Rectangle src = { (float)sx, (float)sy, (float)set.tileWidth, (float)set.tileHeight };
                Rectangle dst = { (float)x*TS, (float)y*TS, (float)TS, (float)TS };
                DrawTexturePro(ts, src, dst, {0,0}, 0, WHITE);
            }
    };
    // Ground + decoration layers render below entities; the top layer is an
    // "overhead" layer drawn above them so the player can walk behind treetops/roofs.
    for (int layer = 0; layer < kLayerCount - 1; ++layer) drawLayer(layer);

    for (auto& e : map_->events) {
        if (e.graphicAsset >= 0)
            drawCharacter(e.graphicAsset, 0, 0, (float)e.x*TS, (float)e.y*TS);
        else if (e.type == EventType::Teleport)
            DrawRectangleLines(e.x*TS+2, e.y*TS+2, TS-4, TS-4, Fade(ui::kAccent, 0.5f));
    }

    drawMonsters();

    // player (red flash when hurt)
    Color ptint = playerHurt_ > 0 ? Color{ 255, 130, 130, 255 } : WHITE;
    drawCharacter(engine_.project().playerSprite, dir_, moving_ ? frame_ : 0, pxX_, pxY_, ptint);

    // melee slash effect in the facing tile
    if (attackTimer_ > 0) {
        Vec2i d = dirToDelta((Direction)dir_);
        float fx = (destX_ + d.x) * (float)TS, fy = (destY_ + d.y) * (float)TS;
        DrawRectangle((int)fx, (int)fy, TS, TS, Fade(Color{ 255, 240, 160, 255 }, 0.45f));
        DrawRectangleLinesEx({ fx, fy, (float)TS, (float)TS }, 2, Fade(WHITE, 0.8f));
    }
    // overhead layer (treetops, roof edges) on top of the player
    drawLayer(kLayerCount - 1);
    EndMode2D();

    // HUD
    GameState& gs = engine_.state();
    DrawRectangle(0, 0, GetScreenWidth(), 32, Fade(BLACK, 0.55f));
    if (!gs.party.empty()) {
        PartyMember& m = gs.party[0];
        DrawText(TextFormat("Lv %d   HP %d/%d   MP %d/%d   EXP %d   Gold %d",
                 m.level, m.hp, m.maxHp, m.mp, m.maxMp, m.exp, gs.inventory.gold),
                 12, 8, 16, ui::kText);
    }
    DrawText("Space:Attack  Arrows/WASD:Move  Enter:Talk  ESC:Menu  F2:Editor",
             12, GetScreenHeight() - 24, 15, Fade(ui::kText, 0.7f));

    if (toastTimer_ > 0) {
        int tw = MeasureText(toast_.c_str(), 18);
        DrawRectangle(GetScreenWidth()/2 - tw/2 - 10, 40, tw + 20, 30, Fade(ui::kAccent, 0.9f));
        DrawText(toast_.c_str(), GetScreenWidth()/2 - tw/2, 46, 18, BLACK);
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

void GamePlay::draw() {
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
