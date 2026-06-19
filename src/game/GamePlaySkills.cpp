// GamePlaySkills: skills, projectiles, effects, damage, combat HUD panel.
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

bool GamePlay::damageMonster(FieldMonster& m, int dmg) {
    if (!m.alive()) return false;
    m.hp -= std::max(1, dmg);
    m.hurtFlash = 0.18f;
    if (m.hp <= 0) { onMonsterKilled(m); return true; }
    return false;
}

void GamePlay::spawnFx(int type, float px, float py, int dir, int assetId, float dur, float radius) {
    SkillFx f; f.type = type; f.px = px; f.py = py; f.dir = dir;
    f.assetId = assetId; f.dur = dur; f.t = 0; f.radius = radius;
    fx_.push_back(f);
}

// Build the active skill set: project's defined field skills, or the built-in
// defaults if none are defined (keeps old projects working).
void GamePlay::loadSkills() {
    const Database& db = engine_.project().database;
    if (!db.fieldSkills.empty()) skills_ = db.fieldSkills;
    else skills_ = Database::defaultFieldSkills();
}

// Rotate a canonical "facing-up" tile offset to the player's current facing.
Vec2i GamePlay::rotateToFacing(int ox, int oy, int dir) {
    switch (dir) {                          // Direction: Down0 Left1 Right2 Up3
        case 3: return { ox,  oy};          // up    (canonical)
        case 0: return {-ox, -oy};          // down  (180)
        case 2: return {-oy,  ox};          // right (90 CW)
        case 1: return { oy, -ox};          // left  (90 CCW)
    }
    return { ox, oy };
}

void GamePlay::playerAttack() { castSlot(0); }

// Cast whatever skill is bound to a key slot (shared by keys + clicks/touch).
void GamePlay::castSlot(int slot) {
    if (slot < 0 || slot >= kSkillSlots) return;
    for (const auto& s : skills_) if (s.slot == slot) { castFieldSkill(s, slot); return; }
}

// Generic, data-driven skill execution.
void GamePlay::castFieldSkill(const FieldSkill& s, int slot) {
    if (skillCd_[slot] > 0) return;
    GameState& gs = engine_.state();
    if (gs.party.empty()) return;
    if (s.mpCost > 0 && gs.party[0].mp < s.mpCost) {
        toast_ = "MP가 부족합니다 (" + std::to_string(s.mpCost) + ")"; toastTimer_ = 1.1f;
        return;
    }
    gs.party[0].mp -= s.mpCost;
    skillCd_[slot] = s.cooldown;
    attackTimer_ = 0.18f;

    // sound: registered asset if set, else built-in
    if (s.soundAsset >= 0) engine_.audio().playSfxFile(engine_.assetPath(s.soundAsset), 0.8f);
    else                   engine_.audio().playSfx("attack", 0.7f);

    int TS = map_->tileset.tileWidth;
    const Database& db = engine_.project().database;
    int atk = gs.party[0].totalAtk(db);
    int dmg = std::max(1, atk * s.powerPct / 100);

    // 1) blink: teleport forward up to `blink` open tiles
    if (s.blink > 0) {
        Vec2i d = dirToDelta((Direction)dir_);
        int nx = destX_, ny = destY_;
        for (int i = 0; i < s.blink; ++i) {
            int tx = nx + d.x, ty = ny + d.y;
            if (!map_->tilemap.inBounds(tx, ty) || map_->tilemap.blocked(tx, ty)) break;
            if (npcAt(tx, ty)) break;
            nx = tx; ny = ty;
            spawnFx(2, nx*(float)TS, ny*(float)TS, dir_, s.effectAsset, 0.28f);
        }
        destX_ = nx; destY_ = ny;
        pxX_ = nx*(float)TS; pxY_ = ny*(float)TS; moving_ = false;
        gs.playerX = nx; gs.playerY = ny;
    }

    // 2) projectile: fire a bolt forward
    if (s.projectile) {
        Projectile pr; pr.dir = dir_; pr.px = pxX_; pr.py = pxY_;
        pr.life = s.range * 0.085f + 0.05f; pr.dmg = dmg;
        projectiles_.push_back(pr);
        return;
    }

    // 3) instant pattern: damage every monster on a rotated pattern tile
    bool hit = false;
    bool aoe = s.patX.size() > 4;
    if (aoe) spawnFx(3, destX_*(float)TS, destY_*(float)TS, dir_, s.effectAsset, 0.5f, TS*2.6f);
    for (size_t i = 0; i < s.patX.size(); ++i) {
        Vec2i r = rotateToFacing(s.patX[i], s.patY[i], dir_);
        int tx = destX_ + r.x, ty = destY_ + r.y;
        if (!aoe) spawnFx(0, tx*(float)TS, ty*(float)TS, dir_, s.effectAsset, 0.2f);
        if (s.powerPct <= 0) continue;
        if (FieldMonster* m = monsterAt(tx, ty)) { damageMonster(*m, dmg - m->def); hit = true; }
    }
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                    [](const FieldMonster& m){ return !m.alive(); }), monsters_.end());

    if (hit) engine_.audio().playSfx("hit", 0.8f);
    else if (slot == 0 && s.powerPct > 0) interact(); // basic attack hit nothing -> talk
}

void GamePlay::updateProjectiles(float dt) {
    if (!map_) { projectiles_.clear(); return; }
    int TS = map_->tileset.tileWidth;
    float speed = TS * 11.0f;
    for (auto& pr : projectiles_) {
        Vec2i d = dirToDelta((Direction)pr.dir);
        pr.px += d.x * speed * dt;
        pr.py += d.y * speed * dt;
        pr.life -= dt;
        int tx = (int)((pr.px + TS/2) / TS), ty = (int)((pr.py + TS/2) / TS);
        if (!map_->tilemap.inBounds(tx, ty) || map_->tilemap.blocked(tx, ty)) { pr.life = 0; continue; }
        if (FieldMonster* m = monsterAt(tx, ty)) {
            damageMonster(*m, pr.dmg);
            spawnFx(1, m->px, m->py, pr.dir, -1, 0.2f);
            engine_.audio().playSfx("hit", 0.8f);
            pr.life = 0;
        }
    }
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                    [](const FieldMonster& m){ return !m.alive(); }), monsters_.end());
    projectiles_.erase(std::remove_if(projectiles_.begin(), projectiles_.end(),
                    [](const Projectile& p){ return p.life <= 0; }), projectiles_.end());
}

void GamePlay::updateFx(float dt) {
    for (auto& f : fx_) f.t += dt;
    fx_.erase(std::remove_if(fx_.begin(), fx_.end(),
              [](const SkillFx& f){ return f.t >= f.dur; }), fx_.end());
}

void GamePlay::drawProjectiles() {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    for (auto& pr : projectiles_) {
        float cx = pr.px + TS/2.0f, cy = pr.py + TS/2.0f;
        Vec2i d = dirToDelta((Direction)pr.dir);
        Color core = { 120, 200, 255, 255 };
        DrawCircle((int)cx, (int)cy, TS*0.18f, Fade(core, 0.95f));
        DrawCircle((int)(cx - d.x*6), (int)(cy - d.y*6), TS*0.12f, Fade(core, 0.5f)); // trail
        DrawCircleLines((int)cx, (int)cy, TS*0.22f, Fade(WHITE, 0.7f));
    }
}

// world-space skill effects (sprite sheet if assigned, else procedural)
void GamePlay::drawFx() {
    int TS = map_ ? map_->tileset.tileWidth : kDefaultTileSize;
    for (auto& f : fx_) {
        float k = f.dur > 0 ? f.t / f.dur : 1.0f;          // 0..1 progress
        if (f.assetId >= 0) {
            const Texture2D& tex = engine_.assetTexture(f.assetId);
            const AssetEntry* ae = engine_.project().assets.find(f.assetId);
            int frames = (ae && ae->frames > 1) ? ae->frames : 1;  // single-row anim
            float fw = tex.width / (float)frames, fh = (float)tex.height;
            int fr = std::min(frames-1, (int)(k * frames));
            Rectangle src = { fr*fw, 0, fw, fh };
            float sz = (f.type == 3) ? f.radius*2 : TS*1.3f;
            Rectangle dst = { f.px + TS/2 - sz/2, f.py + TS/2 - sz/2, sz, sz };
            DrawTexturePro(tex, src, dst, {0,0}, 0, Fade(WHITE, 1.0f - k*0.3f));
            continue;
        }
        // procedural fallbacks
        float cx = f.px + TS/2.0f, cy = f.py + TS/2.0f;
        if (f.type == 0) {                                  // melee slash arc
            DrawRectangle((int)f.px, (int)f.py, TS, TS, Fade(Color{255,240,160,255}, 0.45f*(1-k)));
            DrawRectangleLinesEx({ f.px, f.py, (float)TS, (float)TS }, 2, Fade(WHITE, 0.8f*(1-k)));
        } else if (f.type == 1) {                           // bolt impact
            DrawCircle((int)cx, (int)cy, TS*0.5f*k, Fade(Color{150,210,255,255}, 0.6f*(1-k)));
        } else if (f.type == 2) {                           // dash trail
            DrawCircle((int)cx, (int)cy, TS*0.4f*(1-k), Fade(Color{180,220,255,255}, 0.5f*(1-k)));
        } else if (f.type == 3) {                           // AoE ring
            float r = f.radius * k;
            DrawCircleGradient((int)cx, (int)cy, r, Fade(Color{255,200,120,255}, 0.5f*(1-k)), Fade(Color{255,120,80,0},0));
            DrawCircleLines((int)cx, (int)cy, r, Fade(Color{255,230,160,255}, 0.9f*(1-k)));
            DrawCircleLines((int)cx, (int)cy, r*0.7f, Fade(WHITE, 0.7f*(1-k)));
        }
    }
}

// Right-side skill panel: key, name, MP cost, cooldown sweep, description.
// Data-driven: one slot per bound skill (Z/X/C/V/F/G).
void GamePlay::drawSkillPanel() {
    static const char* keys[kSkillSlots] = { "Z", "X", "C", "V", "F", "G" };
    for (auto& r : skillBtn_) r = {};      // reset; only bound slots get rects

    // collect skills that are bound to a key, in slot order
    const FieldSkill* bound[kSkillSlots] = {};
    int nBound = 0;
    for (int slot = 0; slot < kSkillSlots; ++slot)
        for (const auto& s : skills_) if (s.slot == slot) { bound[slot] = &s; ++nBound; break; }
    if (nBound == 0) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float pw = 178, ph = 72, gap = 7;
    float px = sw - pw - 12;
    float py = sh - (ph + gap) * nBound - 14;

    GameState& gs = engine_.state();
    int mp = gs.party.empty() ? 0 : gs.party[0].mp;

    int row = 0;
    for (int slot = 0; slot < kSkillSlots; ++slot) {
        const FieldSkill* s = bound[slot];
        if (!s) continue;
        Rectangle r = { px, py + row*(ph+gap), pw, ph };
        skillBtn_[slot] = r; ++row;
        bool hover = CheckCollisionPointRec(GetMousePosition(), r);
        bool ready = skillCd_[slot] <= 0 && mp >= s->mpCost;
        Color bg = ready ? (hover ? ui::kPanelHi : ui::kPanel) : Color{40,30,30,235};
        DrawRectangleRec(r, Fade(bg, 0.95f));
        DrawRectangleLinesEx(r, 2, ready ? ui::kAccent : Fade(ui::kDanger,0.7f));

        DrawRectangle((int)r.x+8, (int)r.y+8, 30, 30, Fade(ui::kAccent, ready?0.9f:0.4f));
        DrawTextU(keys[slot], (int)r.x+17, (int)r.y+13, 22, BLACK);
        DrawTextU(s->name.c_str(), (int)r.x+46, (int)r.y+8, 18, ui::kText);
        if (s->mpCost > 0)
            DrawTextU(TextFormat("MP %d", s->mpCost), (int)r.x+46, (int)r.y+32, 13,
                      mp >= s->mpCost ? ui::kGood : ui::kDanger);
        // short auto description
        std::string d = s->projectile ? TextFormat("원거리 %d칸", s->range)
                      : s->blink > 0 && s->powerPct<=0 ? TextFormat("전방 %d칸 이동", s->blink)
                      : s->blink > 0 ? TextFormat("순간이동+광역 %d", (int)s->patX.size())
                      : TextFormat("범위 %d칸", (int)s->patX.size());
        DrawTextU(d.c_str(), (int)r.x+8, (int)r.y+50, 12, ui::kTextDim);

        if (skillCd_[slot] > 0) {
            float frac = s->cooldown > 0 ? skillCd_[slot] / s->cooldown : 0;
            if (frac > 1) frac = 1;
            DrawRectangle((int)r.x, (int)r.y, (int)r.width, (int)(r.height*frac), Fade(BLACK, 0.55f));
            DrawTextU(TextFormat("%.1f", skillCd_[slot]), (int)(r.x+r.width-40), (int)r.y+8, 16, ui::kTextDim);
        }
    }
    DrawTextU("스킬: 키 또는 클릭/터치", (int)px, (int)py - 20, 13, Fade(ui::kText,0.7f));
}

// touch / mouse click on a skill slot casts that skill
void GamePlay::handleSkillClicks() {
    bool pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    int touches = GetTouchPointCount();
    if (!pressed && touches == 0) return;
    Vector2 mp = pressed ? GetMousePosition() : GetTouchPosition(0);
    for (int i = 0; i < kSkillSlots; ++i)
        if (skillBtn_[i].width > 0 && CheckCollisionPointRec(mp, skillBtn_[i])) { castSlot(i); return; }
}



} // namespace tsukuru
