// EditorSkills: shared skill-editor widgets (pattern grid, effect/sound row,
// shape presets) used by the per-character skill editor in EditorChars.cpp.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {

// 9x9 player-relative hit-pattern grid (canonical facing = up). Draws the
// "front" marker, every cell (toggled by click), and the summary line; returns
// the Y just below the grid. Used by the per-character skill editor.
float Editor::drawSkillPatternGrid(FieldSkill& s, float gx, float gy, bool usesPattern) {
    const int GRID = 9, HALF = GRID/2; float cs = 30;
    DrawTriangle({ gx + HALF*cs + cs/2, gy }, { gx + HALF*cs + cs/2 - 7, gy + 11 },
                 { gx + HALF*cs + cs/2 + 7, gy + 11 }, ui::kGood);
    DrawTextU("정면", (int)(gx + HALF*cs + cs/2 + 12), (int)gy, 12, ui::kGood);
    gy += 14;
    for (int ry = 0; ry < GRID; ++ry) for (int rx = 0; rx < GRID; ++rx) {
        int ox = rx - HALF, oy = ry - HALF;     // offset relative to player
        Rectangle cell = { gx + rx*cs, gy + ry*cs, cs-2, cs-2 };
        bool isPlayer = (ox == 0 && oy == 0);
        bool on = false;
        for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) { on = true; break; }
        Color c = isPlayer ? ui::kAccent : (on ? Color{210,120,90,255} : ui::kPanelHi);
        if (!usesPattern) c = Fade(c, 0.35f);   // dim — pattern unused for projectiles
        DrawRectangleRec(cell, c);
        DrawRectangleLinesEx(cell, 1, Fade(BLACK,0.5f));
        if (isPlayer) DrawTextU("P", (int)cell.x+9, (int)cell.y+6, 16, BLACK);
        if (usesPattern && !isPlayer && ui::mouseIn(cell) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (on) { for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) {
                          s.patX.erase(s.patX.begin()+k); s.patY.erase(s.patY.begin()+k); break; } }
            else { s.patX.push_back(ox); s.patY.push_back(oy); }
        }
    }
    // effect-spawn position preview: a ring `effectDist` tiles forward (up).
    Color efxCol = { 255, 170, 60, 255 };
    if (s.effectDist > 0) {
        int edy = HALF - s.effectDist;          // forward = up in the canonical grid
        if (edy >= 0) {
            Rectangle ec = { gx + HALF*cs, gy + edy*cs, cs-2, cs-2 };
            DrawRectangleLinesEx(ec, 3, efxCol);
            DrawCircle((int)(ec.x + cs/2), (int)(ec.y + cs/2), 5, Fade(efxCol, 0.9f));
        }
    }
    float gridBottom = gy + GRID*cs + 6;
    DrawTextU(usesPattern ? TextFormat("칸 클릭=수동 편집 · 적용 타일 %d개 (시전 시 방향 회전)", (int)s.patX.size())
                          : "발사체 모드: 범위 패턴 미사용 · 오른쪽 '사거리'만 적용",
              (int)gx, (int)gridBottom, 12, usesPattern ? ui::kTextDim : ui::kAccentHi);
    if (s.effectDist > 0) {
        DrawCircle((int)gx + 5, (int)gridBottom + 22, 5, efxCol);
        DrawTextU(TextFormat("이펙트 발생 위치: 정면 %d칸 앞", s.effectDist),
                  (int)gx + 14, (int)gridBottom + 16, 12, efxCol);
    }
    return gridBottom;
}

// Effect + sound row block: assign an existing asset, generate a built-in one,
// import an external strip, and set its frame count / fps / replay count. `dy`
// is advanced past the block.
void Editor::drawSkillFxControls(FieldSkill& s, float dx, float& dy) {
    Project& p = engine_.project();
    float fxTop = dy;                            // anchor for the side preview box
    if (ui::button({ dx, dy, 260, 24 }, std::string("이펙트: ") + assetName(s.effectAsset), s.effectAsset>=0))
        cycleAsset(s.effectAsset, AssetType::Image);
    dy += 26;
    DrawTextU("이펙트 생성:", (int)dx, (int)dy+4, 12, ui::kTextDim);
    static const char* fxName[4] = { "베기","볼트","대시","폭발" };
    for (int i = 0; i < 4; ++i)
        if (ui::button({ dx + 78 + i*46, dy, 44, 22 }, fxName[i])) s.effectAsset = generateEffect(i);
    dy += 28;
    if (ui::button({ dx, dy, 260, 22 }, "이펙트 불러오기 (여러 장=연속 프레임)"))
        { pendingEffectSkill_ = &s; pendingEffectImport_ = true; }
    dy += 24;
    DrawTextU("여러 이미지를 한번에 고르면 그 순서대로 프레임이 됩니다.",
              (int)dx, (int)dy, 11, ui::kTextDim); dy += 18;
    if (s.effectAsset >= 0) {
        const AssetEntry* ae = p.assets.find(s.effectAsset);
        int fr = ae ? ae->frames : 1, fpsv = ae ? ae->fps : 12;
        if (ui::intStepper({ dx, dy, 126, 24 }, "프레임", fr, 1, 1, 32))      p.assets.setAnim(s.effectAsset, fr, fpsv);
        if (ui::intStepper({ dx + 134, dy, 126, 24 }, "속도fps", fpsv, 1, 1, 60)) p.assets.setAnim(s.effectAsset, fr, fpsv);
        dy += 27;
    }
    ui::intStepper({ dx, dy, 260, 24 }, "반복(회) 1·3·7…", s.effectLoops, 1, 1, 20); dy += 28;
    ui::intStepper({ dx, dy, 260, 24 }, "이펙트 거리(정면 칸)", s.effectDist, 1, 0, 12); dy += 28;
    if (ui::button({ dx, dy, 260, 24 }, std::string("사운드: ") + assetName(s.soundAsset), s.soundAsset>=0))
        cycleAsset(s.soundAsset, AssetType::Audio);
    dy += 26;
    if (ui::button({ dx, dy, 260, 22 }, "사운드 불러오기 (외부 파일)"))
        { pendingEffectSkill_ = &s; pendingSoundImport_ = true; }
    dy += 26;
    DrawTextU("효과음 생성:", (int)dx, (int)dy+4, 12, ui::kTextDim);
    static const char* sndName[5] = { "베기","마법","폭발","대시","회복" };
    for (int i = 0; i < 5; ++i)
        if (ui::button({ dx + 78 + i*38, dy, 36, 22 }, sndName[i])) s.soundAsset = generateSound(i);
    dy += 34;

    // ---- live effect preview (right of the column): plays the strip in real time ----
    float pvx = dx + 272, pvw = 104, pvh = 104;
    if (pvx + pvw + 8 < GetScreenWidth()) {
        DrawTextU("이펙트 미리보기", (int)pvx, (int)fxTop, 13, ui::kAccent);
        Rectangle box = { pvx, fxTop + 20, pvw, pvh };
        ui::panel(box, ui::kPanelHi);
        if (s.effectAsset >= 0) {
            const Texture2D& tex = engine_.assetTexture(s.effectAsset);
            const AssetEntry* ae = p.assets.find(s.effectAsset);
            int frames = (ae && ae->frames > 1) ? ae->frames : 1;
            float fps  = (ae && ae->fps  > 0) ? (float)ae->fps : 12.0f;
            int loops  = std::max(1, s.effectLoops);
            int total  = frames * loops;
            int fr = total > 0 ? ((int)(GetTime() * fps) % total) % frames : 0;
            float fw = tex.width / (float)frames, fh = (float)tex.height;
            if (fw > 0 && fh > 0) {
                float sc = std::min((pvw - 12) / fw, (pvh - 12) / fh);
                Rectangle src = { fr*fw, 0, fw, fh };
                Rectangle dst = { box.x + (pvw - fw*sc)/2, box.y + (pvh - fh*sc)/2, fw*sc, fh*sc };
                DrawTexturePro(tex, src, dst, {0,0}, 0, WHITE);
            }
            DrawTextU(TextFormat("%d프레임 · %d회 · %dfps", frames, loops, (int)fps),
                      (int)pvx, (int)(box.y + pvh + 4), 11, ui::kTextDim);
        } else {
            DrawTextU("(이펙트 없음)", (int)pvx + 12, (int)(box.y + pvh/2 - 6), 12, ui::kTextDim);
        }
        DrawTextU(TextFormat("사운드: %s", assetName(s.soundAsset).c_str()),
                  (int)pvx, (int)(fxTop + 20 + pvh + 22), 11, ui::kTextDim);
    }
}

// Fill a skill's hit pattern from a shape preset (canonical facing = up).
// shape: 0 정면, 1 직선, 2 십자, 3 부채꼴, 4 원형, 5 주변(3x3).
void Editor::applyShape(FieldSkill& s, int shape, int size) {
    s.patX.clear(); s.patY.clear();
    auto add = [&](int x, int y){ s.patX.push_back(x); s.patY.push_back(y); };
    if (size < 1) size = 1;
    switch (shape) {
        case 0: add(0,0); add(0,-1); break;                       // front
        case 1: add(0,0); for (int i=1;i<=size;++i) add(0,-i); break; // line forward
        case 2: add(0,0); add(0,-1); add(0,1); add(-1,0); add(1,0); break; // cross
        case 3: add(0,-1); add(-1,-1); add(1,-1); add(0,-2); add(-1,-2); add(1,-2); break; // cone
        case 4: for (int y=-size;y<=size;++y) for (int x=-size;x<=size;++x) add(x,y); break; // circle r
        default: for (int y=-1;y<=1;++y) for (int x=-1;x<=1;++x) add(x,y); break; // 3x3
    }
}

} // namespace tsukuru
