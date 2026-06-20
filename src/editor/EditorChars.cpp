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

void Editor::drawCharsTab() {
    if (charSkillEdit_) { drawCharSkillEditor(); return; }   // modal: this character's skill
    float W = (float)GetScreenWidth(), H = (float)GetScreenHeight();
    DrawRectangleRec({ 0, kToolbarH, W, H - kToolbarH }, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    handleAssetDrop();   // drag&drop still works anywhere on this tab

    ui::label("캐릭터 제작", 14, (int)kToolbarH + 8, 20, ui::kAccent);

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
        float listTop = panelTop + 32, listH = panelH - 252;
        Rectangle listReg = { leftX, listTop, leftW, listH };
        int rows = (int)db.characters.size();
        int maxLS = std::max(0, (int)(rows * 30 + 4 - listH));
        if (GetMouseWheelMove() != 0 && ui::mouseIn(listReg)) charListScroll_ -= (int)(GetMouseWheelMove() * 40);
        charListScroll_ = std::max(0, std::min(charListScroll_, maxLS));
        BeginScissorMode((int)leftX, (int)listTop, (int)leftW, (int)listH);
        float ly = listTop - charListScroll_;
        for (int i = 0; i < rows; ++i) {
            if (ly + 28 > listTop && ly < listTop + listH)
                if (ui::button({ x, ly, w, 26 }, db.characters[i].name, charDefSel_ == i)) {
                    charDefSel_ = i; charDefNameFocus_ = false; charFrameSel_ = -1;
                }
            ly += 30;
        }
        EndScissorMode();
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
            if (ui::button({ sx, sy, std::min(260.0f, sw), 26 }, TextFormat("[%s] 스킬 동작 편집", slotKey[castSlot]), hasSkill)) { charSkillEdit_ = true; return; }
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
        int maxFS = std::max(0, (int)(frows * (cell + 8) + 4 - gridH));
        Rectangle gReg = { midX, gridTop, midW, gridH };
        if (GetMouseWheelMove() != 0 && ui::mouseIn(gReg)) charFrameScroll_ -= (int)(GetMouseWheelMove() * 40);
        charFrameScroll_ = std::max(0, std::min(charFrameScroll_, maxFS));
        BeginScissorMode((int)midX, (int)gridTop, (int)midW, (int)gridH);
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
        if (fv.empty())
            DrawTextU("→ 오른쪽 '이미지 소스'에서 이미지를 클릭해 이 방향을 채우세요.", (int)x, (int)gridTop + 8, 13, ui::kTextDim);
    }

    // ====================== RIGHT: 이미지 소스 ======================
    {
        float x = rightX + 10, w = rightW - 20;
        ui::label("이미지 소스", (int)x, (int)panelTop + 8, 15, ui::kAccent);
        if (ui::button({ x, panelTop + 30, w, 30 }, "+ 내 이미지 불러오기 (PNG·JPG·GIF…)", true)) { pickAndImportImages(); }
        float ry = panelTop + 66;
        if (ui::button({ x, ry, w/2 - 2, 24 }, charLibFilter_ ? "필터: 캐릭터만" : "필터: 전체", charLibFilter_)) charLibFilter_ = !charLibFilter_;
        if (ui::button({ x + w/2 + 2, ry, w/2 - 2, 24 }, charSliceMode_ ? TextFormat("추가: %d분할", charSliceN_) : "추가: 1프레임", charSliceMode_)) charSliceMode_ = !charSliceMode_;
        ry += 28;
        if (charSliceMode_) { ui::intStepper({ x, ry, w, 24 }, "분할수", charSliceN_, 1, 2, 16); ry += 28; }
        {
            static const char* dn[4] = { "아래","왼쪽","오른쪽","위" };
            DrawTextU(building ? TextFormat("클릭 → '%s·%s'에 추가 (움짤 통째로)", kMotionNames[charMotionTab_], dn[charDirTab_])
                               : "먼저 캐릭터를 선택/생성하세요",
                      (int)x, (int)ry, 12, building ? ui::kAccentHi : ui::kTextDim);
        }
        ry += 20;

        float gridTop = ry, gridH = panelBot - 8 - gridTop;
        float cell = (w - 8) / 2, cardH = cell + 34;
        int visN = 0;
        for (auto* a : imgs) { if (building && charLibFilter_) { const Texture2D& tt = engine_.assetTexture(a->id); if (tt.height > 64) continue; } visN++; }
        int grows = (visN + 1) / 2;
        int maxGS = std::max(0, (int)(grows * (cardH + 8) + 4 - gridH));
        Rectangle gReg = { rightX, gridTop, rightW, gridH };
        if (GetMouseWheelMove() != 0 && ui::mouseIn(gReg)) charLibScroll_ -= (int)(GetMouseWheelMove() * 44);
        charLibScroll_ = std::max(0, std::min(charLibScroll_, maxGS));
        BeginScissorMode((int)rightX, (int)gridTop, (int)rightW, (int)gridH);
        int idx = 0;
        for (auto* a : imgs) {
            if (building && charLibFilter_) { const Texture2D& tt = engine_.assetTexture(a->id); if (tt.height > 64) continue; }
            int col = idx % 2, row = idx / 2; idx++;
            float cx = x + col * (cell + 8);
            float cy = gridTop + row * (cardH + 8) - charLibScroll_;
            if (cy + cardH < gridTop || cy > panelBot) continue;
            ui::panel({ cx, cy, cell, cardH }, ui::kPanelHi);
            const Texture2D& tex = engine_.assetTexture(a->id);
            float sc = std::min((cell - 12) / std::max(1, tex.width), (cell - 14) / std::max(1, tex.height));
            DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height },
                           { cx + (cell - tex.width*sc)/2, cy + 4, tex.width*sc, tex.height*sc }, {0,0}, 0, WHITE);
            if (a->frames > 1) DrawTextU(TextFormat("움짤%d", a->frames), (int)cx + 4, (int)cy + 4, 11, ui::kGood);
            DrawTextU(a->name.c_str(), (int)cx + 4, (int)(cy + cardH - 30), 10, ui::kText);
            Rectangle clickArea = { cx, cy, cell, cardH - 18 };
            if (building && ui::mouseIn(clickArea) && lclick) {
                MotionClip& mc = db.characters[charDefSel_].motions[charMotionTab_];
                charUndo_ = mc; charUndoSet_ = true;
                auto& mf = dirVecOf(mc, charDirTab_);     // add into the selected direction
                int pos = (charFrameSel_ >= 0 && charFrameSel_ < (int)mf.size()) ? charFrameSel_ + 1 : (int)mf.size();
                int n = (a->frames > 1) ? a->frames : (charSliceMode_ ? charSliceN_ : 1);
                if (n > 1) {
                    std::vector<int> sl = sliceAsset(a->id, n);
                    mf.insert(mf.begin() + pos, sl.begin(), sl.end());
                    if (charFrameSel_ >= 0) charFrameSel_ += (int)sl.size();
                    static const char* dn[4] = { "아래","왼쪽","오른쪽","위" };
                    setStatus(TextFormat("%s·%s: %d프레임 추가", kMotionNames[charMotionTab_], dn[charDirTab_], (int)sl.size()));
                } else {
                    mf.insert(mf.begin() + pos, a->id);
                    if (charFrameSel_ >= 0) charFrameSel_++;
                    setStatus(std::string(kMotionNames[charMotionTab_]) + " 프레임 추가: " + a->name);
                }
                p.save();
            }
            float bhalf = (cell - 10) / 2;
            if (building && ui::button({ cx + 4, cy + cardH - 16, bhalf, 14 }, "자동구성")) {
                CharacterDef& c = db.characters[charDefSel_];
                std::vector<int> fr = sliceSheetRow0(a->id);
                int wlk = std::min((int)fr.size(), 4);
                c.motions[MO_Walk].frames.assign(fr.begin(), fr.begin() + wlk);
                c.motions[MO_Walk].loop = true;
                c.motions[MO_Attack].frames.assign(fr.begin() + wlk, fr.end());
                charFrameSel_ = -1; p.save();
                setStatus(TextFormat("시트 자동구성: 걷기%d+공격%d", wlk, (int)fr.size() - wlk));
            }
            if (ui::button({ cx + 6 + bhalf, cy + cardH - 16, bhalf, 14 }, "배경제거"))
                makeTransparentBg(a->id);   // make the solid/white background transparent
        }
        EndScissorMode();
        if (imgs.empty())
            DrawTextU("(이미지 없음 — 위 '+ 내 이미지 불러오기')", (int)x, (int)gridTop + 10, 12, ui::kTextDim);
    }
}


// Per-character skill behaviour editor (range / power / effect tiles / sound) for
// the slot the current motion maps to. Edits this character's own skill, which
// overrides the global field skill for that slot when the character drives play.
void Editor::drawCharSkillEditor() {
    Project& p = engine_.project();
    Database& db = p.database;
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 22, 24, 32, 255 });

    if (charDefSel_ < 0 || charDefSel_ >= (int)db.characters.size()) { charSkillEdit_ = false; return; }
    CharacterDef& cd = db.characters[charDefSel_];
    int slot = castSlotForMotion(charMotionTab_);
    if (slot < 0) { charSkillEdit_ = false; return; }

    // find this character's skill for the slot, or create it (seeded from the
    // matching global skill if one exists, else a simple front-tile preset).
    FieldSkill* sp = nullptr;
    for (auto& s : cd.skills) if (s.slot == slot) { sp = &s; break; }
    if (!sp) {
        FieldSkill ns;
        const FieldSkill* g = db.fieldSkillForSlot(slot);
        if (g) ns = *g; else { ns.name = kMotionNames[charMotionTab_]; applyShape(ns, 0, 1); }
        ns.slot = slot; ns.id = (int)cd.skills.size() + 1;
        cd.skills.push_back(ns); sp = &cd.skills.back();
    }
    FieldSkill& s = *sp;

    static const char* slotKey[4] = { "Z", "X", "C", "V" };
    ui::label(TextFormat("스킬 동작 편집 — %s / %s 모션 (단축키 %s)",
              cd.name.c_str(), kMotionNames[charMotionTab_], slotKey[slot]),
              20, (int)kToolbarH + 10, 20, ui::kAccent);
    if (ui::button({ area.width - 180, kToolbarH + 8, 160, 28 }, "← 저장하고 닫기")) { p.save(); charSkillEdit_ = false; return; }
    if (ui::button({ area.width - 348, kToolbarH + 8, 160, 28 }, "이 스킬 비우기")) {   // revert to global slot skill
        for (size_t k = 0; k < cd.skills.size(); ++k) if (cd.skills[k].slot == slot) { cd.skills.erase(cd.skills.begin()+k); break; }
        p.save(); charSkillEdit_ = false; return;
    }

    // ---- effect-area tile grid + shape presets ----
    float gx = 24, gy = kToolbarH + 50;
    bool usesPattern = !s.projectile;
    ui::label("효과 적용 범위 (플레이어 기준, 위=정면)", (int)gx, (int)gy, 16, ui::kAccent); gy += 24;
    ui::label("범위 프리셋:", (int)gx, (int)gy, 13, ui::kTextDim); gy += 18;
    static const char* shapeName[6] = { "정면","직선","십자","부채꼴","원형","주변" };
    for (int i = 0; i < 6; ++i)
        if (ui::button({ gx + i*45, gy, 43, 24 }, shapeName[i]) && usesPattern) applyShape(s, i, skillPatSize_);
    gy += 28;
    ui::intStepper({ gx, gy, 200, 24 }, "범위/사거리", skillPatSize_, 1, 1, 4); gy += 28;

    const int GRID = 9, HALF = GRID/2; float cs = 30;
    DrawTriangle({ gx + HALF*cs + cs/2, gy }, { gx + HALF*cs + cs/2 - 7, gy + 11 },
                 { gx + HALF*cs + cs/2 + 7, gy + 11 }, ui::kGood);
    DrawTextU("정면", (int)(gx + HALF*cs + cs/2 + 12), (int)gy, 12, ui::kGood);
    gy += 14;
    for (int ry = 0; ry < GRID; ++ry) for (int rx = 0; rx < GRID; ++rx) {
        int ox = rx - HALF, oy = ry - HALF;
        Rectangle cell = { gx + rx*cs, gy + ry*cs, cs-2, cs-2 };
        bool isPlayer = (ox == 0 && oy == 0);
        bool on = false;
        for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) { on = true; break; }
        Color c = isPlayer ? ui::kAccent : (on ? Color{210,120,90,255} : ui::kPanelHi);
        if (!usesPattern) c = Fade(c, 0.35f);
        DrawRectangleRec(cell, c);
        DrawRectangleLinesEx(cell, 1, Fade(BLACK,0.5f));
        if (isPlayer) DrawTextU("P", (int)cell.x+9, (int)cell.y+6, 16, BLACK);
        if (usesPattern && !isPlayer && ui::mouseIn(cell) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (on) { for (size_t k = 0; k < s.patX.size(); ++k) if (s.patX[k]==ox && s.patY[k]==oy) {
                          s.patX.erase(s.patX.begin()+k); s.patY.erase(s.patY.begin()+k); break; } }
            else { s.patX.push_back(ox); s.patY.push_back(oy); }
        }
    }
    float gridBottom = gy + GRID*cs + 6;
    DrawTextU(usesPattern ? TextFormat("칸 클릭=수동 편집 · 적용 타일 %d개 (시전 시 방향 회전)", (int)s.patX.size())
                          : "발사체 모드: 범위 패턴 미사용 · 오른쪽 '사거리'만 적용",
              (int)gx, (int)gridBottom, 12, usesPattern ? ui::kTextDim : ui::kAccentHi);

    // ---- parameters + one-click effect/sound creation ----
    float dx = gx + GRID*cs + 28, dy = kToolbarH + 50, dw = area.width - dx - 16;
    ui::panel({ dx - 8, dy - 6, dw + 12, 430 }, ui::kPanel);
    ui::label("스킬 설정", (int)dx, (int)dy, 18, ui::kAccent); dy += 28;
    ui::label("이름:", (int)dx, (int)dy, 13, ui::kTextDim); dy += 18;
    Rectangle nf = { dx, dy, std::min(280.0f, dw), 26 };
    if (ui::mouseIn(nf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) skillNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(nf)) skillNameFocus_ = false;
    ui::textField(nf, s.name, skillNameFocus_, 24); dy += 32;
    DrawTextU(TextFormat("단축키: %s (모션에 고정)", slotKey[slot]), (int)dx, (int)dy, 13, ui::kTextDim); dy += 24;
    if (ui::button({ dx, dy, 260, 26 }, s.projectile ? "유형: 발사체(전방 직선)" : "유형: 범위(타일 패턴)"))
        s.projectile = !s.projectile;
    dy += 30;
    ui::intStepper({ dx, dy, 260, 24 }, "사거리(발사체)", s.range, 1, 1, 20); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "순간이동 칸", s.blink, 1, 0, 10); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "위력(%ATK)", s.powerPct, 10, 0, 1000); dy += 27;
    ui::intStepper({ dx, dy, 260, 24 }, "MP 소모", s.mpCost, 1, 0, 99); dy += 27;
    int cdTenths = (int)(s.cooldown * 10 + 0.5f);
    if (ui::intStepper({ dx, dy, 260, 24 }, "쿨다운(0.1초)", cdTenths, 1, 1, 200)) s.cooldown = cdTenths / 10.0f;
    dy += 32;
    auto name = [&](int id){ const AssetEntry* e = p.assets.find(id); return e ? e->name : std::string("없음"); };
    auto cycle = [&](int& cur, AssetType t){
        auto list = p.assets.byType(t);
        int idx = -1; for (int i=0;i<(int)list.size();++i) if (list[i]->id==cur) idx=i;
        idx++; cur = (idx >= (int)list.size()) ? -1 : list[idx]->id;
    };
    if (ui::button({ dx, dy, 260, 24 }, std::string("이펙트: ") + name(s.effectAsset), s.effectAsset>=0))
        cycle(s.effectAsset, AssetType::Image);
    dy += 26;
    DrawTextU("이펙트 생성:", (int)dx, (int)dy+4, 12, ui::kTextDim);
    static const char* fxName[4] = { "베기","볼트","대시","폭발" };
    for (int i = 0; i < 4; ++i) if (ui::button({ dx + 78 + i*46, dy, 44, 22 }, fxName[i])) s.effectAsset = generateEffect(i);
    dy += 30;
    if (ui::button({ dx, dy, 260, 24 }, std::string("사운드: ") + name(s.soundAsset), s.soundAsset>=0))
        cycle(s.soundAsset, AssetType::Audio);
    dy += 26;
    DrawTextU("효과음 생성:", (int)dx, (int)dy+4, 12, ui::kTextDim);
    static const char* sndName[5] = { "베기","마법","폭발","대시","회복" };
    for (int i = 0; i < 5; ++i) if (ui::button({ dx + 78 + i*38, dy, 36, 22 }, sndName[i])) s.soundAsset = generateSound(i);
}


} // namespace tsukuru
