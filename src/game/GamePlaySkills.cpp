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

// Build the active skill set: project's defined field skills, or the built-in
// defaults if none are defined (keeps old projects working).
void GamePlay::loadSkills() {
    const Database& db = engine_.project().database;
    if (!db.fieldSkills.empty()) skills_ = db.fieldSkills;
    else skills_ = Database::defaultFieldSkills();
    // The driving custom character's own skills override the global slot binding,
    // so skills authored in the character panel actually drive that character.
    const CharacterDef* cd = customChar();
    if (cd) for (const FieldSkill& cs : cd->skills) {
        bool replaced = false;
        for (auto& s : skills_) if (s.slot == cs.slot) { s = cs; replaced = true; break; }
        if (!replaced) skills_.push_back(cs);
    }
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
        toast_ = "기력이 부족합니다 (" + std::to_string(s.mpCost) + ")"; toastTimer_ = 1.1f;
        return;
    }
    gs.party[0].mp -= s.mpCost;
    skillCd_[slot] = s.cooldown;
    attackTimer_ = 0.18f;
    // drive the custom character's matching motion (attack / skill1 / skill2 / ult)
    static const int slotMotion[kSkillSlots] = { MO_Attack, MO_Skill1, MO_Skill2, MO_Ult, MO_Attack, MO_Attack };
    triggerMotion(slot < kSkillSlots ? slotMotion[slot] : MO_Attack);

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
            spawnFx(2, nx*(float)TS, ny*(float)TS, dir_, s.effectAsset, 0.28f, 0, s.effectLoops);
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

    // 3a) DAMAGE: hit every monster/enemy on a rotated damage tile
    bool hit = false;
    for (size_t i = 0; i < s.patX.size(); ++i) {
        Vec2i r = rotateToFacing(s.patX[i], s.patY[i], dir_);
        int tx = destX_ + r.x, ty = destY_ + r.y;
        if (s.powerPct <= 0) continue;
        if (FieldMonster* m = monsterAt(tx, ty)) { damageMonster(*m, dmg - m->def); hit = true; }
        if (NpcInst* en = hostileNpcAt(tx, ty)) { damageNpc(*en, dmg - en->def); hit = true; }
    }
    reapDead();

    // 3b) EFFECT: drawn on its own tile layer (per-tile / one-big / scaled)
    spawnSkillEffect(s);

    if (hit) engine_.audio().playSfx("hit", 0.8f);
    else if (slot == 0 && s.powerPct > 0) interact(); // basic attack hit nothing -> talk
}

// Place a skill's visual effect. The effect tiles are its own layer (efxX/efxY),
// or the damage tiles when that layer is empty, each rotated to the player's
// facing and shifted `effectDist` tiles forward. effectMode chooses ONE big
// motion over the whole area vs. one motion per tile; effectScale sizes it.
void GamePlay::spawnSkillEffect(const FieldSkill& s) {
    if (!map_) return;
    int TS = map_->tileset.tileWidth;
    Vec2i fwd = dirToDelta((Direction)dir_);
    int offx = fwd.x * s.effectDist, offy = fwd.y * s.effectDist;
    const std::vector<int>& ex = !s.efxX.empty() ? s.efxX : s.patX;
    const std::vector<int>& ey = !s.efxY.empty() ? s.efxY : s.patY;
    float scale = s.effectScale > 0 ? s.effectScale / 100.0f : 1.0f;

    if (ex.empty()) {                       // no tiles -> single effect on the player
        spawnFx(0, (destX_+offx)*(float)TS, (destY_+offy)*(float)TS, dir_, s.effectAsset, 0.4f, 0, s.effectLoops, TS*scale);
        return;
    }
    if (s.effectMode == 1) {                // ONE big motion covering the whole area
        int minx=9999,miny=9999,maxx=-9999,maxy=-9999;
        for (size_t i = 0; i < ex.size(); ++i) {
            Vec2i r = rotateToFacing(ex[i], ey[i], dir_);
            minx=std::min(minx,r.x); maxx=std::max(maxx,r.x);
            miny=std::min(miny,r.y); maxy=std::max(maxy,r.y);
        }
        float cx = (minx+maxx)/2.0f, cy = (miny+maxy)/2.0f;
        int span = std::max(maxx-minx, maxy-miny) + 1;
        float big = span * TS * scale;
        spawnFx(3, (destX_+offx+cx)*(float)TS, (destY_+offy+cy)*(float)TS,
                dir_, s.effectAsset, 0.5f, big*0.5f, s.effectLoops, big);
    } else {                                // one motion per effect tile
        for (size_t i = 0; i < ex.size(); ++i) {
            Vec2i r = rotateToFacing(ex[i], ey[i], dir_);
            int tx = destX_+offx+r.x, ty = destY_+offy+r.y;
            spawnFx(0, tx*(float)TS, ty*(float)TS, dir_, s.effectAsset, 0.25f, 0, s.effectLoops, TS*scale);
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
            DrawTextU(TextFormat("기력 %d", s->mpCost), (int)r.x+46, (int)r.y+32, 13,
                      mp >= s->mpCost ? ui::kGood : ui::kDanger);
        // short auto description (TextFormat's rotating static buffer — no heap alloc)
        const char* d = s->projectile ? TextFormat("원거리 %d칸", s->range)
                      : s->blink > 0 && s->powerPct<=0 ? TextFormat("전방 %d칸 이동", s->blink)
                      : s->blink > 0 ? TextFormat("순간이동+광역 %d", (int)s->patX.size())
                      : TextFormat("범위 %d칸", (int)s->patX.size());
        DrawTextU(d, (int)r.x+8, (int)r.y+50, 12, ui::kTextDim);

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
