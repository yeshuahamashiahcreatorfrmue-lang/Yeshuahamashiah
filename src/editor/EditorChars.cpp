// EditorChars: character/effect asset workshop.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "core/Platform.h"
#include "render/UI.h"
#include "core/Text.h"
#include "gen/AssetGen.h"
#include "gen/SfxGen.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {

// The editable frame list for a motion facing direction (0아래/1왼쪽/2오른쪽/3위).
static std::vector<int>& dirVecOf(MotionClip& c, int d) {
    switch (d) { case 1: return c.left; case 2: return c.right; case 3: return c.up; default: return c.frames; }
}

// The cast-key slot a skill-capable motion maps to (-1 = not a skill motion).
static int castSlotForMotion(int m) {
    switch (m) {
        case MO_Attack: return 0;   // Z
        case MO_Skill1: return 1;   // X
        case MO_Skill2: return 2;   // C
        case MO_Ult:    return 3;   // V
        default:        return -1;  // 걷기 / 죽음 — no skill
    }
}
// Inverse of castSlotForMotion: the motion a skill slot animates with. F/G
// (slots 4·5) have no dedicated motion and reuse the attack motion at runtime.
static int motionForCastSlot(int slot) {
    switch (slot) {
        case 0: return MO_Attack; case 1: return MO_Skill1;
        case 2: return MO_Skill2; case 3: return MO_Ult;
        default: return -1;       // F / G
    }
}
static const char* const kSlotKeys6[6] = { "Z", "X", "C", "V", "F", "G" };

// Unregister a set of image assets from the project and remove any references to
// them from every character motion (so no frame points at a deleted id).
void Editor::drawFootprintGrid(Rectangle a, int& wT, int& hT, int previewAsset, bool sheet4dir, int maxN) {
    wT = std::max(1, std::min(maxN, wT));
    hT = std::max(1, std::min(maxN, hT));
    float cs = std::min(a.width, a.height) / maxN;     // square cells
    float gridBottom = a.y + maxN * cs;
    // grid cells; the chosen footprint is the bottom-left wT×hT block
    for (int gy = 0; gy < maxN; ++gy)
        for (int gx = 0; gx < maxN; ++gx) {
            int rb = maxN - 1 - gy;                     // row index counted from the bottom
            Rectangle c = { a.x + gx*cs, a.y + gy*cs, cs - 3, cs - 3 };
            bool inFp = (gx < wT && rb < hT);
            DrawRectangleRec(c, inFp ? Fade(ui::kAccent, 0.35f) : Color{ 40, 44, 56, 255 });
            DrawRectangleLinesEx(c, 1, Fade(BLACK, 0.5f));
            // click OR drag over a cell sets the footprint to (col+1)×(rowFromBottom+1)
            if (ui::mouseIn(c) && (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))) {
                wT = gx + 1; hT = rb + 1;
            }
        }
    // live preview: the sprite drawn filling exactly the chosen footprint block
    if (previewAsset >= 0) {
        const Texture2D& tex = engine_.assetTexture(previewAsset);
        float fw = sheet4dir ? tex.width / 4.0f : (float)tex.width;
        float fh = sheet4dir ? tex.height / 4.0f : (float)tex.height;
        Rectangle src = { 0, 0, fw, fh };              // facing-down / first frame
        Rectangle dst = { a.x, gridBottom - hT*cs, wT*cs - 3, hT*cs - 3 };
        DrawTexturePro(tex, src, dst, { 0, 0 }, 0, Fade(WHITE, 0.95f));
    }
    DrawRectangleLinesEx({ a.x, a.y, maxN*cs, maxN*cs }, 1, Fade(WHITE, 0.25f));
}

// Shared "차지 칸수" editor used by the character data editor, the NPC inspector,
// and the player editor — keeps the three layouts identical.
void Editor::drawFootprintControl(float x, float& y, float w, int& wT, int& hT, int& pct,
                                  int previewAsset, bool sheet4dir) {
    const float grid = 120.0f;
    DrawTextU("차지 칸수 (드래그/클릭)", (int)x, (int)y, 13, ui::kTextDim);
    y += 18;
    drawFootprintGrid({ x, y, grid, grid }, wT, hT, previewAsset, sheet4dir);
    DrawTextU(TextFormat("%d×%d칸", wT, hT), (int)x + (int)grid + 12, (int)y + 6, 16, ui::kAccentHi);
    ui::intStepper({ x + grid + 12, y + 34, w - grid - 12, 24 }, "미세 %", pct, 5, 25, 400);
    y += grid + 14;
}

void Editor::deleteAssets(const std::vector<int>& ids) {
    Project& p = engine_.project();
    for (int id : ids) engine_.invalidateAsset(id);   // free GPU texture + path cache first
    p.deleteAssets(ids);                              // scrub refs, delete files, unregister
    charLibSel_.clear();                              // selection may reference removed ids
    charFrameSel_ = -1;
    p.save();
}

// Duplicate an image asset: copy its file under a new name and register it.
int Editor::duplicateAsset(int id) {
    Project& p = engine_.project();
    const AssetEntry* e = p.assets.find(id);
    if (!e) return -1;
    fs::path src = p.assetFullPath(id);
    std::string ext = fs::path(e->relPath).extension().string();
    std::string base = fs::path(e->relPath).stem().string();
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir)/"assets"/(base + "_copy" + std::to_string(n++) + ext); } while (fs::exists(dest));
    std::error_code ec; fs::copy_file(src, dest, ec);
    if (ec) return -1;
    std::string rel = (fs::path("assets")/dest.filename()).generic_string();
    int nid = p.assets.addExisting(e->type, dest.stem().string(), rel);
    if (e->frames > 1) p.assets.setAnim(nid, e->frames, e->fps);
    return nid;
}

int Editor::generateCharacter() {
    Project& p = engine_.project();
    static const Color shirts[] = {
        {80,140,220,255}, {200,90,90,255}, {90,180,110,255}, {200,160,70,255},
        {170,110,200,255}, {90,190,200,255}, {220,130,180,255}, {110,120,130,255}
    };
    static const Color skins[] = { {240,200,160,255}, {225,180,140,255}, {200,150,120,255} };
    Color shirt = shirts[charColor_ % 8];
    Color skin  = skins[(charColor_ / 8) % 3];
    charColor_++;

    // unique filename in the project's assets folder
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir) / "assets" / ("char_" + std::to_string(n++) + ".png"); }
    while (fs::exists(dest));

    Image img = gen::characterSheet(shirt, skin, 2);   // 4 walk + 2 attack frames
    ExportImage(img, dest.string().c_str());
    UnloadImage(img);

    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
    p.save();
    setStatus("캐릭터 생성됨 (걷기4+공격2): " + dest.stem().string());
    return id;
}

// Generate a skill-effect sheet (4 frames x 4 dirs) and register it as an asset.
int Editor::generateEffect(int style) {
    Project& p = engine_.project();
    static const Color cols[4] = {
        {255,235,150,255},  // slash - warm
        {130,200,255,255},  // bolt  - blue
        {190,225,255,255},  // dash  - pale
        {255,170,110,255}   // burst - orange
    };
    static const char* tags[4] = { "fx_slash", "fx_bolt", "fx_dash", "fx_burst" };
    int s = style & 3;
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir) / "assets" / (std::string(tags[s]) + "_" + std::to_string(n++) + ".png"); }
    while (fs::exists(dest));

    const int FR = 6;
    Image img = gen::effectSheet(cols[s], s, FR);
    ExportImage(img, dest.string().c_str());
    UnloadImage(img);

    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
    p.assets.setAnim(id, FR, 14);           // single-row animated effect
    p.save();
    setStatus("이펙트 에셋 생성됨: " + dest.stem().string());
    return id;
}

// Synthesize a skill sound effect and register it as an audio asset.
int Editor::generateSound(int style) {
    Project& p = engine_.project();
    static const char* tags[5] = { "snd_slash", "snd_magic", "snd_boom", "snd_dash", "snd_heal" };
    int s = style % 5;
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir) / "assets" / (std::string(tags[s]) + "_" + std::to_string(n++) + ".wav"); }
    while (fs::exists(dest));

    Wave w = gen::skillSound(s);
    ExportWave(w, dest.string().c_str());
    UnloadWave(w);

    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Audio, dest.stem().string(), rel);
    p.save();
    engine_.audio().playSfxFile(dest.string(), 0.9f);   // immediate audio preview
    setStatus(std::string("효과음 생성됨 (재생): ") + gen::skillSoundName(s));
    return id;
}

// Split a registered image into `n` equal horizontal columns, exporting each as
// a new image asset; returns the new frame asset ids (left to right).
std::vector<int> Editor::sliceAsset(int assetId, int n) {
    std::vector<int> out;
    Project& p = engine_.project();
    const AssetEntry* e = p.assets.find(assetId);
    if (!e || n < 1) return out;
    Image img = LoadImage(p.assetFullPath(assetId).c_str());
    if (!img.data) return out;
    int fw = img.width / n, fh = img.height;
    if (fw < 1) { UnloadImage(img); return out; }
    fs::create_directories(fs::path(p.dir) / "assets");
    std::string base = fs::path(e->relPath).stem().string();
    for (int i = 0; i < n; ++i) {
        Image f = ImageFromImage(img, { (float)(i*fw), 0, (float)fw, (float)fh });
        int k = 1; fs::path dest;
        do { dest = fs::path(p.dir)/"assets"/(base+"_f"+std::to_string(i)+(k>1?("_"+std::to_string(k)):std::string())+".png"); k++; }
        while (fs::exists(dest));
        ExportImage(f, dest.string().c_str());
        UnloadImage(f);
        std::string rel = (fs::path("assets")/dest.filename()).generic_string();
        out.push_back(p.assets.addExisting(AssetType::Image, dest.stem().string(), rel));
    }
    UnloadImage(img);
    return out;
}

// Slice the top row of a sprite sheet into square-cell frames. Detects the cell
// size (single row of square frames, or a common 4-direction sheet) and exports
// each cell of row 0 as a frame asset; returns the new ids (left to right).
std::vector<int> Editor::sliceSheetRow0(int assetId) {
    std::vector<int> out;
    Project& p = engine_.project();
    const AssetEntry* e = p.assets.find(assetId);
    if (!e) return out;
    Image img = LoadImage(p.assetFullPath(assetId).c_str());
    if (!img.data) return out;
    int cell = img.height;                                  // assume one row of square frames
    if (img.height > 0 && img.width % img.height != 0 &&    // not a single square row →
        img.height % 4 == 0 && img.width % (img.height/4) == 0)
        cell = img.height / 4;                              // common 4-direction sheet
    int cols = (cell > 0) ? std::max(1, img.width / cell) : 1;
    if (cell < 1) { UnloadImage(img); return out; }
    fs::create_directories(fs::path(p.dir) / "assets");
    std::string base = fs::path(e->relPath).stem().string();
    for (int i = 0; i < cols; ++i) {
        Image f = ImageFromImage(img, { (float)(i*cell), 0, (float)cell, (float)cell });
        int k = 1; fs::path dest;
        do { dest = fs::path(p.dir)/"assets"/(base+"_r0c"+std::to_string(i)+(k>1?("_"+std::to_string(k)):std::string())+".png"); k++; }
        while (fs::exists(dest));
        ExportImage(f, dest.string().c_str());
        UnloadImage(f);
        std::string rel = (fs::path("assets")/dest.filename()).generic_string();
        out.push_back(p.assets.addExisting(AssetType::Image, dest.stem().string(), rel));
    }
    UnloadImage(img);
    return out;
}

// A scrollable region: mouse wheel + middle-button drag to scroll, plus a
// draggable scrollbar thumb on the right edge. Call AFTER drawing the region's
// content (and after EndScissorMode) so the bar sits on top. Updates `scroll`.
void Editor::scrollbar(Rectangle r, int& scroll, float contentH) {
    int maxS = (int)std::max(0.0f, contentH - r.height);
    Vector2 m = GetMousePosition();
    bool inR = CheckCollisionPointRec(m, r);
    if (inR) {
        float w = GetMouseWheelMove();
        if (w != 0) scroll -= (int)(w * 48);
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) scroll -= (int)GetMouseDelta().y;  // 휠 클릭 드래그
    }
    if (maxS <= 0) { scroll = 0; return; }
    const float trackW = 10;
    float tx = r.x + r.width - trackW - 2, th = r.height;
    float thumbH = std::max(28.0f, th * r.height / contentH);
    if (scrollDragTarget_ == &scroll) {                       // continue an active thumb drag
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float denom = std::max(1.0f, th - thumbH);
            scroll = (int)(((m.y - scrollDragGrab_) - r.y) / denom * maxS);
        } else scrollDragTarget_ = nullptr;
    }
    scroll = std::max(0, std::min(scroll, maxS));
    float thumbY = r.y + (th - thumbH) * (scroll / (float)maxS);
    Rectangle thumb = { tx, thumbY, trackW, thumbH };
    DrawRectangleRounded({ tx, r.y, trackW, th }, 0.5f, 4, Fade(BLACK, 0.30f));
    bool hot = CheckCollisionPointRec(m, thumb) || scrollDragTarget_ == &scroll;
    DrawRectangleRounded(thumb, 0.5f, 4, hot ? ui::kAccentHi : ui::kAccent);
    if (CheckCollisionPointRec(m, thumb) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        scrollDragTarget_ = &scroll; scrollDragGrab_ = m.y - thumbY;
    }
}

void Editor::drawCharsTab() {
    if (charSkillEdit_) { drawCharSkillEditor(); return; }   // modal: this character's skill
    if (charDataEdit_)  { drawCharDataEditor();  return; }   // modal: this character's full data
    float W = (float)screenW(), H = (float)screenH();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    handleAssetDrop();   // drag&drop still works anywhere on this tab

    ui::label("캐릭터 제작", 14, (int)kToolbarH + 8, 20, ui::kAccent);
    DrawTextU(seg_.ready() ? (std::string(kBuildTag) + " · AI배경제거 ON").c_str()
                           : (std::string(kBuildTag) + " · AI배경제거 미탑재").c_str(),
              168, (int)kToolbarH + 14, 13, ui::kGood);

    // ---- 3-column layout: [캐릭터] [모션·프레임·재생] [이미지 소스] ----
    float pad = 10;
    float panelTop = kToolbarH + 36, panelBot = H - 10, panelH = panelBot - panelTop;
    float leftW = 230, rightW = std::min(380.0f, std::max(300.0f, W * 0.30f));
    float leftX = 10, midX = leftX + leftW + pad;
    float rightX = W - rightW - 10, midW = rightX - midX - pad;
    ui::panel({ leftX,  panelTop, leftW,  panelH }, ui::kPanel);
    ui::panel({ midX,   panelTop, midW,   panelH }, ui::kPanel);
    ui::panel({ rightX, panelTop, rightW, panelH }, ui::kPanel);

    auto imgs = p.assets.byType(AssetType::Image);
    if (charDefSel_ < 0 && !db.characters.empty()) charDefSel_ = 0;
    bool building = (charDefSel_ >= 0 && charDefSel_ < (int)db.characters.size());

    // ============================ LEFT: 캐릭터 ============================
    {
        float x = leftX + 10, w = leftW - 20;
        ui::label("캐릭터", (int)x, (int)panelTop + 8, 15, ui::kAccent);
        float listTop = panelTop + 32, listH = panelH - 290;
        Rectangle listReg = { leftX, listTop, leftW, listH };
        int rows = (int)db.characters.size();
        uiScissor((int)leftX, (int)listTop, (int)leftW, (int)listH);
        float ly = listTop - charListScroll_;
        for (int i = 0; i < rows; ++i) {
            if (ly + 28 > listTop && ly < listTop + listH)
                if (ui::button({ x, ly, w - 12, 26 }, db.characters[i].name, charDefSel_ == i)) {
                    charDefSel_ = i; charDefNameFocus_ = false; charFrameSel_ = -1;
                }
            ly += 30;
        }
        EndScissorMode();
        scrollbar(listReg, charListScroll_, rows * 30.0f + 4);
        if (rows == 0) DrawTextU("(없음)", (int)x, (int)listTop + 6, 13, ui::kTextDim);

        float by = listTop + listH + 6;
        if (ui::button({ x, by, w, 30 }, "+ 새 캐릭터")) {
            CharacterDef cd; cd.id = (int)db.characters.size() + 1;
            cd.name = "캐릭터" + std::to_string(cd.id); cd.motions[MO_Walk].loop = true;
            db.characters.push_back(cd); charDefSel_ = (int)db.characters.size() - 1;
            charFrameSel_ = -1; charMotionTab_ = 0; p.save(); building = true;
        }
        by += 38;
        DrawLine((int)x, (int)by, (int)(x + w), (int)by, ui::kPanelHi); by += 8;
        if (building) {
            CharacterDef& cd = db.characters[charDefSel_];
            DrawTextU("이름", (int)x, (int)by + 5, 12, ui::kTextDim);
            Rectangle nf = { x + 42, by, w - 42, 26 };
            if (ui::mouseIn(nf) && lclick) charDefNameFocus_ = true;
            else if (lclick && !ui::mouseIn(nf)) charDefNameFocus_ = false;
            ui::textField(nf, cd.name, charDefNameFocus_, 20); by += 32;
            if (ui::button({ x, by, w, 30 }, "⚙ 캐릭터 데이터 수정 (능력치·스킬)", false)) { charDataEdit_ = true; charDataNameFocus_ = -1; return; }
            by += 36;
            bool isP = (p.playerCharId == cd.id);
            if (ui::button({ x, by, w, 28 }, isP ? "★ 플레이어 (현재)" : "플레이어로 설정", isP)) {
                p.playerCharId = cd.id; p.save();
                setStatus(cd.motions[MO_Walk].frames.empty()
                    ? "주의: '걷기' 모션이 비어 인게임에서 안 보일 수 있습니다."
                    : "이 캐릭터를 플레이어로 설정.");
            }
            by += 32;
            if (ui::button({ x, by, w/2 - 2, 26 }, "복제")) {
                CharacterDef cp = cd; cp.id = (int)db.characters.size() + 1; cp.name = cd.name + " 사본";
                db.characters.push_back(cp); charDefSel_ = (int)db.characters.size() - 1; charFrameSel_ = -1; p.save(); return;
            }
            if (ui::button({ x + w/2 + 2, by, w/2 - 2, 26 }, "삭제")) {
                if (p.playerCharId == cd.id) p.playerCharId = -1;
                db.characters.erase(db.characters.begin() + charDefSel_);
                charDefSel_ = -1; charFrameSel_ = -1; p.save(); return;
            }
            by += 34;
        }
        DrawLine((int)x, (int)by, (int)(x + w), (int)by, ui::kPanelHi); by += 8;
        DrawTextU("빠른 생성 도구", (int)x, (int)by + 2, 12, ui::kTextDim); by += 20;
        if (ui::button({ x, by, w, 26 }, "+ 시트 캐릭터 자동 생성")) generateCharacter();
        by += 30;
        ui::intStepper({ x, by, w, 24 }, "시트 걷기수", p.playerFrames, 1, 4, 7); by += 27;
        ui::intStepper({ x, by, w, 24 }, "시트 공격수", p.playerAtkFrames, 1, 0, 4);
    }

    // ================== CENTER: 모션 · 프레임 · 재생 ==================
    if (!building) {
        ui::label("← 왼쪽에서 '+ 새 캐릭터'를 눌러 시작하세요.", (int)midX + 14, (int)panelTop + 14, 15, ui::kTextDim);
    } else {
        CharacterDef& cd = db.characters[charDefSel_];
        float x = midX + 10, w = midW - 20;
        DrawTextU("모션 (탭 선택)", (int)x, (int)panelTop + 6, 13, ui::kTextDim);
        float tabsY = panelTop + 26, tw = w / MO_COUNT;
        for (int m = 0; m < MO_COUNT; ++m) {
            std::string lbl = std::string(kMotionNames[m]) + "(" + std::to_string((int)cd.motions[m].frames.size()) + ")";
            if (ui::button({ x + m*tw, tabsY, tw - 3, 28 }, lbl, charMotionTab_ == m)) { charMotionTab_ = m; charFrameSel_ = -1; }
        }
        MotionClip& clip = cd.motions[charMotionTab_];

        // ---- direction sub-tabs: 상하좌우 each registered separately ----
        static const char* dirName[4] = { "아래", "왼쪽", "오른쪽", "위" };
        float dirY = tabsY + 32;
        DrawRectangleRec({ midX + 4, dirY - 4, midW - 8, 34 }, Fade(ui::kAccent, 0.16f)); // highlight band
        DrawTextU("방향(상하좌우):", (int)x, (int)dirY + 6, 13, ui::kAccentHi);
        for (int d = 0; d < 4; ++d) {
            int cnt = (int)dirVecOf(clip, d).size();
            if (ui::button({ x + 100 + d*72, dirY, 68, 26 }, TextFormat("%s(%d)", dirName[d], cnt), charDirTab_ == d))
                { charDirTab_ = d; charFrameSel_ = -1; }
        }
        std::vector<int>& fv = dirVecOf(clip, charDirTab_);     // the direction being edited
        if (charFrameSel_ >= (int)fv.size()) charFrameSel_ = -1;
        auto snap = [&]() { charUndo_ = clip; charUndoSet_ = true; };

        // ---- preview (shows exactly what the game renders for this facing) ----
        float rowY = dirY + 32;
        const float PVS = 140;
        Rectangle pv = { x, rowY, PVS, PVS };
        DrawRectangleRec(pv, Color{ 16, 18, 26, 255 });
        DrawRectangleLinesEx(pv, 2, ui::kPanelHi);
        DrawTextU("재생", (int)pv.x + 5, (int)pv.y + 4, 12, ui::kAccent);
        const std::vector<int>& pf = clip.dirFrames(charDirTab_);   // 비면 '아래'로 대체
        if (!pf.empty()) {
            int n = (int)pf.size();
            int fi = (int)(GetTime() * std::max(1, clip.fps)) % n;
            const Texture2D& t = engine_.assetTexture(pf[fi]);
            float sc = std::min((PVS - 22) / std::max(1, t.width), (PVS - 22) / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height }, { pv.x + (PVS - t.width*sc)/2, pv.y + (PVS - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
            DrawTextU(TextFormat("%d/%d", fi+1, n), (int)pv.x + PVS - 46, (int)pv.y + 5, 14, ui::kGood);
        } else {
            DrawTextU("프레임 없음", (int)pv.x + 28, (int)pv.y + PVS/2 - 8, 13, ui::kTextDim);
        }
        float sx = pv.x + PVS + 14, sw = x + w - sx, sy = rowY;
        ui::intStepper({ sx, sy, std::min(220.0f, sw), 26 }, "fps", clip.fps, 1, 1, 30); sy += 32;
        if (ui::button({ sx, sy, std::min(220.0f, sw), 26 }, clip.loop ? "반복 재생: 켜짐" : "반복 재생: 꺼짐", clip.loop)) { clip.loop = !clip.loop; p.save(); }
        sy += 32;
        DrawTextU(fv.empty() && charDirTab_ != 0 ? "이 방향 비어있음 → '아래'로 대체됨"
                                                  : TextFormat("이 방향 %d프레임", (int)fv.size()),
                  (int)sx, (int)sy + 3, 12, fv.empty() && charDirTab_ != 0 ? ui::kDanger : ui::kTextDim);
        sy += 24;
        DrawTextU("걷기·공격·스킬 모두 상하좌우 각각 등록하세요", (int)sx, (int)sy + 2, 11, ui::kTextDim);
        sy += 22;
        int castSlot = castSlotForMotion(charMotionTab_);
        if (castSlot >= 0) {
            static const char* slotKey[4] = { "Z", "X", "C", "V" };
            bool hasSkill = false; for (auto& sk : cd.skills) if (sk.slot == castSlot) { hasSkill = true; break; }
            if (ui::button({ sx, sy, std::min(260.0f, sw), 26 }, TextFormat("[%s] 스킬 동작 편집", slotKey[castSlot]), hasSkill)) { charSkillSlot_ = castSlot; charSkillEdit_ = true; return; }
        }

        // ---- frame timeline (edits the selected direction) ----
        float tlY = rowY + PVS + 10;
        DrawTextU(TextFormat("프레임 타임라인 — %s · %s", kMotionNames[charMotionTab_], dirName[charDirTab_]), (int)x, (int)tlY, 13, ui::kAccent);
        tlY += 22;
        if (ui::button({ x, tlY, 74, 24 }, "전체 비우기")) { snap(); fv.clear(); charFrameSel_ = -1; p.save(); }
        if (ui::button({ x + 78, tlY, 60, 24 }, "되돌리기") && charUndoSet_) { std::swap(clip, charUndo_); charFrameSel_ = -1; p.save(); }
        if (ui::button({ x + 142, tlY, 50, 24 }, "복사")) { charClip_ = clip; charClipSet_ = true; setStatus("모션 복사됨 (전 방향)"); }
        if (ui::button({ x + 196, tlY, 64, 24 }, "붙여넣기") && charClipSet_) { snap(); clip = charClip_; charFrameSel_ = -1; p.save(); }
        if (ui::button({ x + 264, tlY, 56, 24 }, "뒤집기")) { snap(); std::reverse(fv.begin(), fv.end()); p.save(); }
        tlY += 28;
        if (charFrameSel_ >= 0) {
            if (ui::button({ x, tlY, 60, 24 }, "◀ 앞으로") && charFrameSel_ > 0) { std::swap(fv[charFrameSel_], fv[charFrameSel_-1]); charFrameSel_--; p.save(); }
            if (ui::button({ x + 64, tlY, 50, 24 }, "복제")) { fv.insert(fv.begin()+charFrameSel_+1, fv[charFrameSel_]); charFrameSel_++; p.save(); }
            if (ui::button({ x + 118, tlY, 50, 24 }, "삭제")) { snap(); fv.erase(fv.begin()+charFrameSel_); charFrameSel_ = -1; p.save(); }
            if (ui::button({ x + 172, tlY, 60, 24 }, "뒤로 ▶") && charFrameSel_ < (int)fv.size()-1) { std::swap(fv[charFrameSel_], fv[charFrameSel_+1]); charFrameSel_++; p.save(); }
        } else {
            DrawTextU("썸네일 클릭 = 선택 후 이동 / 복제 / 삭제", (int)x, (int)tlY + 5, 12, ui::kTextDim);
        }
        tlY += 30;
        float cell = 58, gridTop = tlY, gridH = panelBot - 8 - gridTop;
        int per = std::max(1, (int)(w / (cell + 8)));
        int frows = ((int)fv.size() + per - 1) / per;
        Rectangle gReg = { midX, gridTop, midW, gridH };
        uiScissor((int)midX, (int)gridTop, (int)midW, (int)gridH);
        for (int i = 0; i < (int)fv.size(); ++i) {
            float fx = x + (i % per) * (cell + 8);
            float fy = gridTop + (i / per) * (cell + 8) - charFrameScroll_;
            if (fy + cell < gridTop || fy > panelBot) continue;
            Rectangle fr = { fx, fy, cell, cell };
            DrawRectangleRec(fr, ui::kPanelHi);
            const Texture2D& t = engine_.assetTexture(fv[i]);
            float sc = std::min((cell - 8) / std::max(1, t.width), (cell - 8) / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height },
                           { fr.x + (cell - t.width*sc)/2, fr.y + (cell - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
            DrawRectangleLinesEx(fr, 2, charFrameSel_ == i ? ui::kAccent : Fade(BLACK, 0.5f));
            DrawTextU(TextFormat("%d", i+1), (int)fr.x + 2, (int)fr.y + 2, 11, ui::kGood);
            if (ui::mouseIn(fr) && lclick) charFrameSel_ = i;
        }
        EndScissorMode();
        scrollbar(gReg, charFrameScroll_, frows * (cell + 8.0f) + 4);
        if (fv.empty())
            DrawTextU("→ 오른쪽 '이미지 소스'에서 이미지를 클릭해 이 방향을 채우세요.", (int)x, (int)gridTop + 8, 13, ui::kTextDim);
    }

    // ====================== RIGHT: 이미지 소스 ======================
    {
        float x = rightX + 10, w = rightW - 20;
        ui::label("이미지 소스", (int)x, (int)panelTop + 8, 15, ui::kAccent);
        bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool shift = IsKeyDown(KEY_LEFT_SHIFT)   || IsKeyDown(KEY_RIGHT_SHIFT);
        bool blocked = charLibMenuOpen_ || charLibRenameId_ >= 0;   // overlay eats grid input

        if (ui::button({ x, panelTop + 30, w, 30 }, "+ 내 이미지 불러오기 (PNG·JPG·GIF…)", true)) { pendingImport_ = true; }
        float ry = panelTop + 66;
        if (ui::button({ x, ry, w/2 - 2, 24 }, charLibFilter_ ? "필터: 캐릭터만" : "필터: 전체", charLibFilter_)) charLibFilter_ = !charLibFilter_;
        if (ui::button({ x + w/2 + 2, ry, w/2 - 2, 24 }, charSliceMode_ ? TextFormat("추가: %d분할", charSliceN_) : "추가: 1프레임", charSliceMode_)) charSliceMode_ = !charSliceMode_;
        ry += 28;
        if (charSliceMode_) { ui::intStepper({ x, ry, w, 24 }, "분할수", charSliceN_, 1, 2, 16); ry += 28; }

        // visible asset list (stable indices for shift-range & rubber-band select)
        std::vector<const AssetEntry*> vis;
        for (auto* a : imgs) {
            if (building && charLibFilter_) { const Texture2D& tt = engine_.assetTexture(a->id); if (tt.height > 64) continue; }
            vis.push_back(a);
        }
        auto isSel    = [&](int id){ return std::find(charLibSel_.begin(), charLibSel_.end(), id) != charLibSel_.end(); };
        auto toggleSel= [&](int id){ auto it = std::find(charLibSel_.begin(), charLibSel_.end(), id);
                                     if (it != charLibSel_.end()) charLibSel_.erase(it); else charLibSel_.push_back(id); };
        auto addToMotion = [&](int assetId){
            if (!building) return 0;
            const AssetEntry* a = p.assets.find(assetId); if (!a) return 0;
            MotionClip& mc = db.characters[charDefSel_].motions[charMotionTab_];
            charUndo_ = mc; charUndoSet_ = true;
            auto& mf = dirVecOf(mc, charDirTab_);
            int pos = (charFrameSel_ >= 0 && charFrameSel_ < (int)mf.size()) ? charFrameSel_ + 1 : (int)mf.size();
            int n = (a->frames > 1) ? a->frames : (charSliceMode_ ? charSliceN_ : 1);
            if (n > 1) {
                std::vector<int> sl = sliceAsset(a->id, n);
                mf.insert(mf.begin() + pos, sl.begin(), sl.end());
                if (charFrameSel_ >= 0) charFrameSel_ += (int)sl.size();
                return (int)sl.size();
            }
            mf.insert(mf.begin() + pos, a->id);
            if (charFrameSel_ >= 0) charFrameSel_++;
            return 1;
        };

        // ---- selection toolbar ----
        DrawTextU(TextFormat("선택 %d개  (Ctrl=다중·Shift=범위·드래그=상자·우클릭=메뉴)", (int)charLibSel_.size()),
                  (int)x, (int)ry, 11, charLibSel_.empty() ? ui::kTextDim : ui::kAccentHi);
        ry += 18;
        float tb = (w - 8) / 3;
        if (ui::button({ x, ry, tb, 22 }, "전체 선택")) { charLibSel_.clear(); for (auto* a : vis) charLibSel_.push_back(a->id); }
        if (ui::button({ x + tb + 4, ry, tb, 22 }, "선택 해제")) charLibSel_.clear();
        if (ui::button({ x + 2*(tb + 4), ry, tb, 22 }, TextFormat("삭제(%d)", (int)charLibSel_.size())) && !charLibSel_.empty()) {
            auto ids = charLibSel_; deleteAssets(ids); charLibSel_.clear();
            setStatus(TextFormat("%d개 삭제됨", (int)ids.size())); return;
        }
        ry += 26;
        {
            static const char* dn[4] = { "아래","왼쪽","오른쪽","위" };
            DrawTextU(building ? TextFormat("클릭 → '%s·%s'에 추가 (움짤 통째로)", kMotionNames[charMotionTab_], dn[charDirTab_])
                               : "먼저 캐릭터를 선택/생성하세요",
                      (int)x, (int)ry, 12, building ? ui::kAccentHi : ui::kTextDim);
        }
        ry += 18;

        float gridTop = ry, gridH = panelBot - 8 - gridTop;
        float cell = (w - 8) / 2, cardH = cell + 34;
        int grows = ((int)vis.size() + 1) / 2;
        Rectangle gReg = { rightX, gridTop, rightW, gridH };
        Vector2 m = GetMousePosition();
        auto cardRect = [&](int idx)->Rectangle {
            int col = idx % 2, row = idx / 2;
            return { x + col*(cell+8), gridTop + row*(cardH+8) - charLibScroll_, cell, cardH };
        };

        // rubber-band may begin on a left press inside the grid
        if (!blocked && CheckCollisionPointRec(m, gReg) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            charLibDragStart_ = m; charLibDragMaybe_ = true; charLibDragging_ = false; charLibPressOnCard_ = false;
        }

        uiScissor((int)rightX, (int)gridTop, (int)rightW, (int)gridH);
        for (int idx = 0; idx < (int)vis.size(); ++idx) {
            const AssetEntry* a = vis[idx];
            Rectangle card = cardRect(idx);
            float cx = card.x, cy = card.y;
            if (cy + cardH < gridTop || cy > panelBot) continue;
            ui::panel({ cx, cy, cell, cardH }, ui::kPanelHi);
            const Texture2D& tex = engine_.assetTexture(a->id);
            float sc = std::min((cell - 12) / std::max(1, tex.width), (cell - 14) / std::max(1, tex.height));
            DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height },
                           { cx + (cell - tex.width*sc)/2, cy + 4, tex.width*sc, tex.height*sc }, {0,0}, 0, WHITE);
            if (a->frames > 1) DrawTextU(TextFormat("움짤%d", a->frames), (int)cx + 4, (int)cy + 4, 11, ui::kGood);
            DrawTextU(a->name.c_str(), (int)cx + 4, (int)(cy + cardH - 30), 10, ui::kText);
            bool sel = isSel(a->id);
            if (sel) {
                DrawRectangleLinesEx({ cx, cy, cell, cardH }, 3, ui::kGood);
                DrawRectangleRec({ cx + cell - 19, cy + 3, 16, 16 }, ui::kGood);
                DrawTextU("✓", (int)cx + cell - 16, (int)cy + 3, 14, BLACK);
            }
            // right-click: select this card (unless already in a multi-selection) + open menu
            if (!blocked && CheckCollisionPointRec(m, { cx, cy, cell, cardH }) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                if (!sel) charLibSel_ = { a->id };
                charLibMenuOpen_ = true; charLibMenuPos_ = m; charLibPressOnCard_ = true; charLibDragMaybe_ = false;
            }
            // left-click on the image area
            Rectangle clickArea = { cx, cy, cell, cardH - 18 };
            if (!blocked && ui::mouseIn(clickArea) && lclick) {
                charLibPressOnCard_ = true;
                if (ctrl) { toggleSel(a->id); charLibAnchor_ = idx; }
                else if (shift && charLibAnchor_ >= 0) {
                    int lo = std::min(charLibAnchor_, idx), hi = std::max(charLibAnchor_, idx);
                    charLibSel_.clear();
                    for (int k = lo; k <= hi && k < (int)vis.size(); ++k) charLibSel_.push_back(vis[k]->id);
                } else {                              // plain click = add to motion (+ become selection)
                    int n = addToMotion(a->id);
                    charLibSel_ = { a->id }; charLibAnchor_ = idx;
                    if (n > 0) { p.save(); setStatus(std::string(kMotionNames[charMotionTab_]) + " 프레임 추가: " + a->name); }
                }
            }
            float bhalf = (cell - 10) / 2;
            if (building && !blocked && ui::button({ cx + 4, cy + cardH - 16, bhalf, 14 }, "자동구성")) {
                charLibPressOnCard_ = true;
                CharacterDef& c = db.characters[charDefSel_];
                std::vector<int> fr = sliceSheetRow0(a->id);
                int wlk = std::min((int)fr.size(), 4);
                c.motions[MO_Walk].frames.assign(fr.begin(), fr.begin() + wlk);
                c.motions[MO_Walk].loop = true;
                c.motions[MO_Attack].frames.assign(fr.begin() + wlk, fr.end());
                charFrameSel_ = -1; p.save();
                setStatus(TextFormat("시트 자동구성: 걷기%d+공격%d", wlk, (int)fr.size() - wlk));
            }
            if (!blocked && ui::button({ cx + 6 + bhalf, cy + cardH - 16, bhalf, 14 }, "배경제거")) {
                charLibPressOnCard_ = true; makeTransparentBg(a->id);
            }
        }
        EndScissorMode();

        // ---- rubber-band box selection ----
        if (!blocked && charLibDragMaybe_ && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            if (!charLibDragging_ && !charLibPressOnCard_ &&
                (fabsf(m.x - charLibDragStart_.x) > 5 || fabsf(m.y - charLibDragStart_.y) > 5)) {
                charLibDragging_ = true;
                charLibSelBase_ = ctrl ? charLibSel_ : std::vector<int>{};   // additive only with Ctrl
            }
            if (charLibDragging_) {
                Rectangle rub = { std::min(m.x, charLibDragStart_.x), std::min(m.y, charLibDragStart_.y),
                                  fabsf(m.x - charLibDragStart_.x), fabsf(m.y - charLibDragStart_.y) };
                charLibSel_ = charLibSelBase_;                               // rebuild each frame (box reflects exactly)
                for (int idx = 0; idx < (int)vis.size(); ++idx) {
                    Rectangle c = cardRect(idx);
                    if (c.y + cardH < gridTop || c.y > panelBot) continue;   // only on-screen cards
                    if (CheckCollisionRecs(c, rub) && !isSel(vis[idx]->id)) charLibSel_.push_back(vis[idx]->id);
                }
                DrawRectangleRec(rub, Fade(ui::kAccent, 0.20f));
                DrawRectangleLinesEx(rub, 1, ui::kAccent);
            }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) { charLibDragMaybe_ = false; charLibDragging_ = false; charLibPressOnCard_ = false; }

        scrollbar(gReg, charLibScroll_, grows * (cardH + 8.0f) + 4);
        if (vis.empty())
            DrawTextU("(이미지 없음 — 위 '+ 내 이미지 불러오기')", (int)x, (int)gridTop + 10, 12, ui::kTextDim);

        // ---- right-click context menu (drawn on top) ----
        if (charLibMenuOpen_) {
            int nsel = (int)charLibSel_.size();
            std::vector<std::pair<std::string,int>> items;
            if (building) items.push_back({ std::string(TextFormat("모션에 추가 (%d)", nsel)), 1 });
            items.push_back({ std::string(TextFormat("배경 제거 (%d)", nsel)), 2 });
            items.push_back({ std::string(TextFormat("복제 (%d)", nsel)), 3 });
            if (nsel == 1) items.push_back({ "이름 바꾸기", 4 });
            items.push_back({ "전체 선택", 5 });
            items.push_back({ "선택 해제", 6 });
            items.push_back({ std::string(TextFormat("삭제 (%d)", nsel)), 7 });

            float mw = 196, ih = 26, mh = items.size() * ih + 8;
            float mx = std::min(charLibMenuPos_.x, W - mw - 6);
            float my = std::min(charLibMenuPos_.y, H - mh - 6);
            Rectangle menuR = { mx, my, mw, mh };
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) &&
                !CheckCollisionPointRec(m, menuR))
                charLibMenuOpen_ = false;
            ui::panel(menuR, ui::kPanelHi);
            DrawRectangleLinesEx(menuR, 1, ui::kAccent);
            float iy = my + 4; int chosen = -1;
            for (auto& it : items) { if (ui::button({ mx + 4, iy, mw - 8, ih - 2 }, it.first.c_str())) chosen = it.second; iy += ih; }
            if (chosen >= 0) {
                auto ids = charLibSel_;
                charLibMenuOpen_ = false;
                switch (chosen) {
                    case 1: { int tot = 0; for (int id : ids) tot += addToMotion(id); p.save();
                              setStatus(TextFormat("모션에 %d프레임 추가", tot)); return; }
                    case 2: for (int id : ids) makeTransparentBg(id); setStatus(TextFormat("%d개 배경 제거", (int)ids.size())); return;
                    case 3: { for (int id : ids) duplicateAsset(id); p.save(); setStatus(TextFormat("%d개 복제됨", (int)ids.size())); return; }
                    case 4: if (nsel == 1) { charLibRenameId_ = ids[0]; const AssetEntry* e = p.assets.find(ids[0]);
                                             charLibRenameBuf_ = e ? e->name : ""; charLibRenameFocus_ = true; } break;
                    case 5: charLibSel_.clear(); for (auto* a : vis) charLibSel_.push_back(a->id); break;
                    case 6: charLibSel_.clear(); break;
                    case 7: askConfirm(TextFormat("에셋 %d개를 삭제할까요? 맵/DB의 참조도 함께 정리됩니다.", (int)ids.size()),
                                [this, ids]() { deleteAssets(ids); charLibSel_.clear();
                                                setStatus(TextFormat("%d개 삭제됨", (int)ids.size())); }); return;
                }
            }
        }

        // ---- rename overlay ----
        if (charLibRenameId_ >= 0) {
            const AssetEntry* e = p.assets.find(charLibRenameId_);
            if (!e) charLibRenameId_ = -1;
            else {
                float rw = 320, rh = 100, rx = (W - rw)/2, ryy = (H - rh)/2;
                ui::panel({ rx, ryy, rw, rh }, ui::kPanelHi);
                DrawRectangleLinesEx({ rx, ryy, rw, rh }, 2, ui::kAccent);
                DrawTextU("이름 바꾸기", (int)rx + 12, (int)ryy + 8, 15, ui::kAccent);
                Rectangle nf = { rx + 12, ryy + 34, rw - 24, 26 };
                if (lclick) charLibRenameFocus_ = ui::mouseIn(nf);
                ui::textField(nf, charLibRenameBuf_, charLibRenameFocus_, 40);
                if (ui::button({ rx + 12, ryy + 66, rw/2 - 16, 24 }, "확인") || IsKeyPressed(KEY_ENTER)) {
                    p.assets.rename(charLibRenameId_, charLibRenameBuf_); charLibRenameId_ = -1; p.save();
                    setStatus("이름 변경됨"); return;
                }
                if (ui::button({ rx + rw/2 + 4, ryy + 66, rw/2 - 16, 24 }, "취소") || IsKeyPressed(KEY_ESCAPE))
                    charLibRenameId_ = -1;
            }
        }
    }
}


// Per-character skill behaviour editor (range / power / effect tiles / sound) for
// the slot the current motion maps to. Edits this character's own skill, which
// overrides the global field skill for that slot when the character drives play.
void Editor::drawCharSkillEditor() {
    Project& p = engine_.project();
    Database& db = p.database;
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
    DrawRectangleRec(area, Color{ 22, 24, 32, 255 });

    if (charDefSel_ < 0 || charDefSel_ >= (int)db.characters.size()) { charSkillEdit_ = false; return; }
    CharacterDef& cd = db.characters[charDefSel_];
    // The slot is set explicitly on entry (Z/X/C/V motions or the F/G cards);
    // fall back to the motion tab for safety.
    int slot = charSkillSlot_ >= 0 ? charSkillSlot_ : castSlotForMotion(charMotionTab_);
    if (slot < 0 || slot >= 6) { charSkillEdit_ = false; charSkillSlot_ = -1; return; }
    int mot = motionForCastSlot(slot);

    // find this character's skill for the slot, or create it (seeded from the
    // matching global skill if one exists, else a simple front-tile preset).
    FieldSkill* sp = nullptr;
    for (auto& s : cd.skills) if (s.slot == slot) { sp = &s; break; }
    if (!sp) {
        FieldSkill ns;
        const FieldSkill* g = db.fieldSkillForSlot(slot);
        if (g) ns = *g; else { ns.name = mot >= 0 ? kMotionNames[mot] : (std::string(kSlotKeys6[slot]) + " 스킬"); applyShape(ns, 0, 1); }
        ns.slot = slot; ns.id = (int)cd.skills.size() + 1;
        cd.skills.push_back(ns); sp = &cd.skills.back();
    }
    FieldSkill& s = *sp;

    ui::label(TextFormat("스킬 동작 편집 — %s / %s (단축키 %s)",
              cd.name.c_str(), mot >= 0 ? kMotionNames[mot] : "추가 스킬", kSlotKeys6[slot]),
              20, (int)kToolbarH + 10, 20, ui::kAccent);
    if (ui::button({ area.width - 180, kToolbarH + 8, 160, 28 }, "← 저장하고 닫기")) { p.save(); charSkillEdit_ = false; charSkillSlot_ = -1; return; }
    if (ui::button({ area.width - 348, kToolbarH + 8, 160, 28 }, "이 스킬 비우기")) {   // revert to global slot skill
        for (size_t k = 0; k < cd.skills.size(); ++k) if (cd.skills[k].slot == slot) { cd.skills.erase(cd.skills.begin()+k); break; }
        p.save(); charSkillEdit_ = false; charSkillSlot_ = -1; return;
    }

    // ---- tile grid + shape presets (damage layer / effect layer) ----
    float gx = 24, gy = kToolbarH + 50;
    bool usesPattern = !s.projectile;
    ui::label("범위 편집 (플레이어 기준, 위=정면)", (int)gx, (int)gy, 16, ui::kAccent); gy += 24;
    // layer toggle: paint either the DAMAGE tiles or the EFFECT tiles
    if (ui::button({ gx, gy, 132, 24 }, "데미지 범위", !editEfxLayer_)) editEfxLayer_ = false;
    if (ui::button({ gx + 138, gy, 132, 24 }, "이펙트 범위", editEfxLayer_)) editEfxLayer_ = true;
    gy += 30;
    DrawTextU(editEfxLayer_ ? "프리셋:(이펙트 칸)" : "프리셋:(데미지 칸)", (int)gx, (int)gy, 13, ui::kTextDim); gy += 18;
    static const char* shapeName[6] = { "정면","직선","십자","부채꼴","원형","주변" };
    for (int i = 0; i < 6; ++i)
        if (ui::button({ gx + i*45, gy, 43, 24 }, shapeName[i]) && usesPattern) applyShape(s, i, skillPatSize_, editEfxLayer_);
    gy += 28;
    ui::intStepper({ gx, gy, 200, 24 }, "범위/사거리", skillPatSize_, 1, 1, 4); gy += 28;

    const int GRID = 9; float cs = 30;
    drawSkillPatternGrid(s, gx, gy, usesPattern);

    // ---- parameters + one-click effect/sound creation ----
    float dx = gx + GRID*cs + 28, dy = kToolbarH + 50, dw = area.width - dx - 16;
    ui::panel({ dx - 8, dy - 6, dw + 12, 620 }, ui::kPanel);
    ui::label("스킬 설정", (int)dx, (int)dy, 18, ui::kAccent); dy += 28;
    ui::label("이름:", (int)dx, (int)dy, 13, ui::kTextDim); dy += 18;
    Rectangle nf = { dx, dy, std::min(280.0f, dw), 26 };
    if (ui::mouseIn(nf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) skillNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(nf)) skillNameFocus_ = false;
    ui::textField(nf, s.name, skillNameFocus_, 24); dy += 32;
    DrawTextU(mot >= 0 ? TextFormat("단축키: %s (모션에 고정)", kSlotKeys6[slot])
                       : TextFormat("단축키: %s (모션 없음 · 공격 모션으로 시전)", kSlotKeys6[slot]),
              (int)dx, (int)dy, 13, ui::kTextDim); dy += 24;
    if (ui::button({ dx, dy, 260, 26 }, s.projectile ? "유형: 발사체(전방 직선)" : "유형: 범위(타일 패턴)"))
        s.projectile = !s.projectile;
    dy += 30;
    ui::intStepper({ dx, dy, 260, 24 }, "사거리(발사체)", s.range, 1, 1, 20); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "순간이동 칸", s.blink, 1, 0, 10); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "위력 배수(%공격력)", s.powerPct, 10, 0, 1000); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "기력 소모", s.mpCost, 1, 0, 99); dy += 27;
    int cdTenths = (int)(s.cooldown * 10 + 0.5f);
    if (ui::intStepper({ dx, dy, 260, 24 }, "쿨다운(0.1초)", cdTenths, 1, 1, 200)) s.cooldown = cdTenths / 10.0f;
    dy += 32;
    drawSkillFxControls(s, dx, dy);     // 이펙트·사운드 지정/생성/불러오기 + 프레임/반복
}


// Bulk character-data editor: edit one character's whole data set in a single
// screen — battle stats (체력/기력/공격력/방어력/속도) on the left, and every
// skill (Z/X/C/V) with its numbers on the right, all visible & editable at once.
void Editor::drawCharDataEditor() {
    Project& p = engine_.project();
    Database& db = p.database;
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
    DrawRectangleRec(area, Color{ 22, 24, 32, 255 });
    if (charDefSel_ < 0 || charDefSel_ >= (int)db.characters.size()) { charDataEdit_ = false; return; }
    CharacterDef& cd = db.characters[charDefSel_];
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (lclick) charDataNameFocus_ = -1;   // a click elsewhere drops text focus (set again below if on a field)

    ui::label(TextFormat("캐릭터 데이터 — %s", cd.name.c_str()), 20, (int)kToolbarH + 10, 20, ui::kAccent);
    if (ui::button({ area.width - 180, kToolbarH + 8, 160, 28 }, "← 저장하고 닫기")) { p.save(); charDataEdit_ = false; return; }

    // ===================== LEFT: 능력치 =====================
    float lx = 24, ly = kToolbarH + 50, lw = 320;
    ui::panel({ lx - 8, ly - 6, lw + 16, 396 }, ui::kPanel);
    ui::label("능력치", (int)lx, (int)ly, 18, ui::kAccent); ly += 28;
    ui::label("이름:", (int)lx, (int)ly, 13, ui::kTextDim); ly += 18;
    Rectangle nf = { lx, ly, lw, 26 };
    if (ui::mouseIn(nf) && lclick) charDataNameFocus_ = 0;
    ui::textField(nf, cd.name, charDataNameFocus_ == 0, 24); ly += 34;
    ui::intStepper({ lx, ly, lw, 26 }, "체력 (HP)",     cd.maxHp, 10, 1, 99999); ly += 32;
    ui::intStepper({ lx, ly, lw, 26 }, "기력 (GP)",     cd.maxGp,  5, 0, 9999);  ly += 32;
    ui::intStepper({ lx, ly, lw, 26 }, "공격력",        cd.atk,    1, 0, 9999);  ly += 32;
    ui::intStepper({ lx, ly, lw, 26 }, "방어력",        cd.def,    1, 0, 9999);  ly += 32;
    ui::intStepper({ lx, ly, lw, 26 }, "속도 (이동)",   cd.spd,    1, 0, 999);   ly += 32;
    ui::intStepper({ lx, ly, lw, 26 }, "포만치 (허기)",   cd.maxHunger, 1000, 0, 999999); ly += 32;
    ui::intStepper({ lx, ly, lw, 26 }, "수분치 (목마름)", cd.maxThirst, 1000, 0, 999999); ly += 32;
    // tile footprint (칸): drag/click the grid; sprite fits the chosen block
    {
        int prev = cd.motions[MO_Walk].frames.empty() ? -1 : cd.motions[MO_Walk].frames.front();
        drawFootprintControl(lx, ly, lw, cd.drawTilesW, cd.drawTilesH, cd.drawPct, prev, false);
    }
    DrawTextU("※ 공격력은 모든 스킬에 공통 적용됩니다.", (int)lx, (int)ly, 12, ui::kAccentHi); ly += 16;
    DrawTextU("   실제 데미지 = 공격력 × (스킬 위력 배수%)", (int)lx, (int)ly, 12, ui::kTextDim); ly += 26;
    bool isP = (p.playerCharId == cd.id);
    if (ui::button({ lx, ly, lw, 28 }, isP ? "★ 플레이어 (현재)" : "플레이어로 설정", isP)) {
        p.playerCharId = cd.id; p.save(); setStatus("이 캐릭터를 플레이어로 설정 (데이터 적용).");
    }

    // ===================== RIGHT: 스킬 일괄 =====================
    static const char* slotKey[4]  = { "Z", "X", "C", "V" };
    static const int   slotMot[4]  = { MO_Attack, MO_Skill1, MO_Skill2, MO_Ult };
    float rx0 = lx + lw + 28, ry0 = kToolbarH + 50;
    float rArea = area.width - rx0 - 16;
    float cardW = (rArea - 12) / 2, cardH = 250;
    ui::label("스킬 (Z·X·C·V 일괄 편집)", (int)rx0, (int)(ry0 - 24), 16, ui::kAccent);
    for (int slot = 0; slot < 4; ++slot) {
        float cx = rx0 + (slot % 2) * (cardW + 12);
        float cy = ry0 + (slot / 2) * (cardH + 12);
        ui::panel({ cx, cy, cardW, cardH }, ui::kPanelHi);
        float ix = cx + 12, iy = cy + 10, iw = cardW - 24;
        DrawTextU(TextFormat("[%s] %s 모션", slotKey[slot], kMotionNames[slotMot[slot]]), (int)ix, (int)iy, 15, ui::kAccent);
        iy += 24;
        FieldSkill* sp = nullptr;
        for (auto& s : cd.skills) if (s.slot == slot) { sp = &s; break; }
        if (!sp) {
            DrawTextU("이 슬롯에 스킬 없음 (전역 기본 사용)", (int)ix, (int)iy + 6, 12, ui::kTextDim); iy += 30;
            if (ui::button({ ix, iy, iw, 28 }, "+ 이 스킬 만들기")) {
                FieldSkill ns; const FieldSkill* g = db.fieldSkillForSlot(slot);
                if (g) ns = *g; else { ns.name = kMotionNames[slotMot[slot]]; applyShape(ns, 0, 1); }
                ns.slot = slot; ns.id = (int)cd.skills.size() + 1;
                cd.skills.push_back(ns); p.save();
            }
            continue;
        }
        FieldSkill& s = *sp;
        Rectangle snf = { ix, iy, iw, 24 };
        if (ui::mouseIn(snf) && lclick) charDataNameFocus_ = slot + 1;
        ui::textField(snf, s.name, charDataNameFocus_ == (slot + 1), 24); iy += 30;
        float colw = (iw - 8) / 2;
        if (ui::button({ ix, iy, iw, 24 }, s.projectile ? "유형: 발사체(직선)" : "유형: 범위(타일)")) { s.projectile = !s.projectile; p.save(); }
        iy += 28;
        ui::intStepper({ ix, iy, colw, 24 }, "기력", s.mpCost, 1, 0, 99);
        ui::intStepper({ ix + colw + 8, iy, colw, 24 }, "사거리", s.range, 1, 1, 20); iy += 28;
        ui::intStepper({ ix, iy, colw, 24 }, "위력%", s.powerPct, 10, 0, 1000);
        ui::intStepper({ ix + colw + 8, iy, colw, 24 }, "순간이동", s.blink, 1, 0, 10); iy += 28;
        int cdT = (int)(s.cooldown * 10 + 0.5f);
        if (ui::intStepper({ ix, iy, colw, 24 }, "쿨다운0.1초", cdT, 1, 1, 200)) { s.cooldown = cdT / 10.0f; }
        DrawTextU(TextFormat("데미지 %d", std::max(1, cd.atk * s.powerPct / 100)),
                  (int)(ix + colw + 12), (int)iy + 4, 14, ui::kGood); iy += 30;
        if (ui::button({ ix, iy, colw, 26 }, "범위·이펙트 편집")) {
            charMotionTab_ = slotMot[slot]; charSkillSlot_ = slot; charSkillEdit_ = true; charDataEdit_ = false; return;
        }
        if (ui::button({ ix + colw + 8, iy, colw, 26 }, "스킬 삭제")) {
            for (size_t k = 0; k < cd.skills.size(); ++k) if (cd.skills[k].slot == slot) { cd.skills.erase(cd.skills.begin()+k); break; }
            p.save(); return;
        }
    }

    // ---- F · G 추가 슬롯 (모션 없음 — 공격 모션으로 시전) ----
    float fy = ry0 + 2 * (cardH + 12) + 8;       // just below the Z/X/C/V grid
    ui::label("추가 스킬 (F · G — 모션 없음)", (int)rx0, (int)fy, 14, ui::kAccent); fy += 22;
    for (int e = 0; e < 2; ++e) {
        int slot = 4 + e;                        // 4=F, 5=G
        float cx = rx0 + e * (cardW + 12);
        ui::panel({ cx, fy, cardW, 70 }, ui::kPanelHi);
        float ix = cx + 12, iy = fy + 10, iw = cardW - 24;
        FieldSkill* sp = nullptr;
        for (auto& s : cd.skills) if (s.slot == slot) { sp = &s; break; }
        if (!sp) {
            DrawTextU(TextFormat("[%s] 스킬 없음", kSlotKeys6[slot]), (int)ix, (int)iy, 13, ui::kTextDim); iy += 22;
            if (ui::button({ ix, iy, iw, 26 }, "+ 이 스킬 만들기")) {
                FieldSkill ns; const FieldSkill* g = db.fieldSkillForSlot(slot);
                if (g) ns = *g; else { ns.name = std::string(kSlotKeys6[slot]) + " 스킬"; applyShape(ns, 0, 1); }
                ns.slot = slot; ns.id = (int)cd.skills.size() + 1;
                cd.skills.push_back(ns); p.save();
            }
            continue;
        }
        FieldSkill& s = *sp;
        DrawTextU(TextFormat("[%s]", kSlotKeys6[slot]), (int)ix, (int)iy + 3, 14, ui::kAccent);
        Rectangle snf = { ix + 28, iy, iw - 28, 24 };
        if (ui::mouseIn(snf) && lclick) charDataNameFocus_ = slot + 1;
        ui::textField(snf, s.name, charDataNameFocus_ == (slot + 1), 24); iy += 30;
        float colw = (iw - 8) / 2;
        if (ui::button({ ix, iy, colw, 26 }, "범위·이펙트 편집")) {
            charSkillSlot_ = slot; charSkillEdit_ = true; charDataEdit_ = false; return;
        }
        if (ui::button({ ix + colw + 8, iy, colw, 26 }, "스킬 삭제")) {
            for (size_t k = 0; k < cd.skills.size(); ++k) if (cd.skills[k].slot == slot) { cd.skills.erase(cd.skills.begin()+k); break; }
            p.save(); return;
        }
    }
}


} // namespace tsukuru
