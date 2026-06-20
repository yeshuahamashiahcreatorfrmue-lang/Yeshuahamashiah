// EditorChars: character/effect asset workshop.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
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
    if (charBrowse_) { drawImageBrowser(); return; }     // modal: import your own image files
    if (charSkillEdit_) { drawCharSkillEditor(); return; } // modal: this character's skill behaviour
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();
    Database& db = p.database;
    bool lclick = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    ui::label("캐릭터 제작", 20, (int)kToolbarH + 10, 22, ui::kAccent);
    // quick generators + sheet-player frame config
    if (ui::button({ 20, kToolbarH + 38, 150, 26 }, "+ 시트 캐릭터")) generateCharacter();
    static const char* fxBtn[4] = { "이펙트:베기", "볼트", "대시", "폭발" };
    for (int s = 0; s < 4; ++s)
        if (ui::button({ 178.0f + s*92, kToolbarH + 38, 88, 26 }, fxBtn[s])) generateEffect(s);
    ui::intStepper({ 560, kToolbarH + 38, 150, 26 }, "시트걷기", p.playerFrames, 1, 4, 7);
    ui::intStepper({ 718, kToolbarH + 38, 150, 26 }, "시트공격", p.playerAtkFrames, 1, 0, 4);
    // import your own images without depending on drag&drop
    if (ui::button({ 876, kToolbarH + 38, 168, 26 }, "+ 내 이미지 불러오기", true)) { charBrowse_ = true; browseScroll_ = 0; return; }
    handleAssetDrop(); // drag&drop new images/sheets to register them

    auto imgs = p.assets.byType(AssetType::Image);

    // ============ custom multi-motion character builder ============
    float bx = 16, by = kToolbarH + 72, bw = area.width - 32, bh = 318;
    ui::panel({ bx, by, bw, bh }, ui::kPanel);
    ui::label("커스텀 캐릭터 빌더", (int)bx + 12, (int)by + 8, 17, ui::kAccent);
    DrawTextU("순서:  1. 위 '내 이미지 불러오기'로 등록   2. '+ 새' 캐릭터   3. 모션 탭 선택   "
              "4. 아래 라이브러리 클릭 = 프레임 추가   5. '플레이어로 설정'",
              (int)bx + 170, (int)by + 11, 12, ui::kTextDim);

    // ---- [1] 캐릭터: 목록 + 새로 만들기 ----
    float cy = by + 32;
    DrawTextU("캐릭터", (int)bx + 12, (int)cy + 5, 12, ui::kTextDim);
    if (ui::button({ bx + 62, cy, 64, 24 }, "+ 새")) {
        CharacterDef cd; cd.id = (int)db.characters.size() + 1;
        cd.name = "캐릭터" + std::to_string(cd.id); cd.motions[MO_Walk].loop = true;
        db.characters.push_back(cd); charDefSel_ = (int)db.characters.size() - 1; charFrameSel_ = -1; p.save();
    }
    float clx = bx + 134;
    for (int i = 0; i < (int)db.characters.size() && clx < bx + bw - 108; ++i) {
        if (ui::button({ clx, cy, 102, 24 }, db.characters[i].name, charDefSel_ == i)) {
            charDefSel_ = i; charDefNameFocus_ = false; charFrameSel_ = -1;
        }
        clx += 108;
    }

    if (charDefSel_ < 0 && !db.characters.empty()) charDefSel_ = 0;
    bool building = (charDefSel_ >= 0 && charDefSel_ < (int)db.characters.size());
    if (!building) {
        DrawTextU("'+ 새'로 캐릭터를 만든 뒤, 모션 탭을 고르고 아래 라이브러리 이미지를 클릭해 프레임을 채우세요.",
                 (int)bx + 12, (int)by + 84, 14, ui::kTextDim);
    } else {
        CharacterDef& cd = db.characters[charDefSel_];

        // ---- [2] 캐릭터 속성: 이름 / 플레이어 / 복제 / 삭제 ----
        float ry = cy + 30;
        DrawTextU("이름", (int)bx + 12, (int)ry + 5, 12, ui::kTextDim);
        Rectangle nf = { bx + 50, ry, 188, 24 };
        if (ui::mouseIn(nf) && lclick) charDefNameFocus_ = true;
        else if (lclick && !ui::mouseIn(nf)) charDefNameFocus_ = false;
        ui::textField(nf, cd.name, charDefNameFocus_, 20);
        bool isP = (p.playerCharId == cd.id);
        if (ui::button({ bx + 248, ry, 150, 24 }, isP ? "플레이어 (현재)" : "플레이어로 설정", isP)) {
            p.playerCharId = cd.id; p.save();
            setStatus(cd.motions[MO_Walk].frames.empty()
                ? "주의: '걷기' 모션이 비어 인게임에서 안 보일 수 있습니다."
                : "커스텀 캐릭터를 플레이어로 설정.");
        }
        if (ui::button({ bx + 404, ry, 56, 24 }, "복제", false)) {
            CharacterDef cp = cd; cp.id = (int)db.characters.size() + 1; cp.name = cd.name + " 사본";
            db.characters.push_back(cp); charDefSel_ = (int)db.characters.size()-1; charFrameSel_ = -1; p.save(); return;
        }
        if (ui::button({ bx + 466, ry, 56, 24 }, "삭제", false)) {
            if (p.playerCharId == cd.id) p.playerCharId = -1;
            db.characters.erase(db.characters.begin() + charDefSel_);
            charDefSel_ = -1; charFrameSel_ = -1; p.save(); return;
        }

        // ---- [3] 모션 탭 ----
        float ty = ry + 30;
        DrawTextU("모션", (int)bx + 12, (int)ty + 6, 12, ui::kTextDim);
        for (int m = 0; m < MO_COUNT; ++m) {
            std::string lbl = std::string(kMotionNames[m]) + "(" + std::to_string((int)cd.motions[m].frames.size()) + ")";
            if (ui::button({ bx + 50 + m*86, ty, 82, 26 }, lbl, charMotionTab_ == m)) { charMotionTab_ = m; charFrameSel_ = -1; }
        }
        MotionClip& clip = cd.motions[charMotionTab_];
        if (charFrameSel_ >= (int)clip.frames.size()) charFrameSel_ = -1;

        // ---- [4] 모션 편집: fps / 반복 / 길이 / 비우기·복사·붙여넣기·뒤집기 + 미리보기 ----
        float ey = ty + 34;
        ui::intStepper({ bx + 50, ey, 126, 24 }, "fps", clip.fps, 1, 1, 30);
        if (ui::button({ bx + 182, ey, 78, 24 }, clip.loop ? "반복: 켜짐" : "반복: 꺼짐", clip.loop)) { clip.loop = !clip.loop; p.save(); }
        float dur = clip.frames.empty() ? 0 : (float)clip.frames.size() / std::max(1, clip.fps);
        DrawTextU(TextFormat("길이 %.2f초", dur), (int)bx + 268, (int)ey + 5, 12, ui::kText);
        // skill-capable motions (공격/스킬1/스킬2/궁극기) get a per-character skill editor
        int castSlot = castSlotForMotion(charMotionTab_);
        if (castSlot >= 0) {
            static const char* slotKey[4] = { "Z", "X", "C", "V" };
            bool hasSkill = false;
            for (auto& sk : cd.skills) if (sk.slot == castSlot) { hasSkill = true; break; }
            if (ui::button({ bx + 360, ey, 256, 24 },
                           TextFormat("[%s] 스킬 동작 편집 (사거리/위력/효과/사운드)", slotKey[castSlot]), hasSkill))
                { charSkillEdit_ = true; return; }
        }
        float ey2 = ey + 30;
        auto snap = [&]() { charUndo_ = clip; charUndoSet_ = true; };   // 1-level undo snapshot
        if (ui::button({ bx + 50, ey2, 56, 22 }, "비우기", false)) { snap(); clip.frames.clear(); charFrameSel_ = -1; p.save(); }
        if (ui::button({ bx + 110, ey2, 50, 22 }, "복사"))        { charClip_ = clip; charClipSet_ = true; setStatus("모션 복사됨"); }
        if (ui::button({ bx + 164, ey2, 66, 22 }, "붙여넣기", false) && charClipSet_) { snap(); clip.frames = charClip_.frames; clip.fps = charClip_.fps; p.save(); setStatus("모션 붙여넣기"); }
        if (ui::button({ bx + 234, ey2, 56, 22 }, "뒤집기", false)) { snap(); std::reverse(clip.frames.begin(), clip.frames.end()); p.save(); }
        if (ui::button({ bx + 294, ey2, 66, 22 }, "되돌리기", false) && charUndoSet_) { std::swap(clip, charUndo_); charFrameSel_ = -1; p.save(); setStatus("되돌리기"); }
        if (ui::button({ bx + 364, ey2, 78, 22 }, "프레임 생성")) {     // 즉석 포즈 4프레임 생성
            snap();
            int sid = generateCharacter();
            std::vector<int> fr = sliceSheetRow0(sid);
            int add = std::min((int)fr.size(), 4);
            for (int i = 0; i < add; ++i) clip.frames.push_back(fr[i]);
            p.save(); setStatus(TextFormat("프레임 %d개 생성 추가", add));
        }
        // motion preview (right edge) — large, always-cycling so you can SEE the
        // motion play even for one-shot (non-loop) motions like 공격/궁극기.
        const float PVS = 116;
        Rectangle pv = { bx + bw - PVS - 8, ey - 6, PVS, PVS };
        DrawTextU("재생 미리보기", (int)pv.x, (int)pv.y - 16, 13, ui::kAccent);
        DrawRectangleRec(pv, Color{ 16, 18, 26, 255 });
        DrawRectangleLinesEx(pv, 2, ui::kPanelHi);
        if (!clip.frames.empty()) {
            int n = (int)clip.frames.size();
            int fi = (int)(GetTime() * std::max(1, clip.fps)) % n;   // always cycles in preview
            const Texture2D& t = engine_.assetTexture(clip.frames[fi]);
            float sc = std::min((PVS-16) / std::max(1, t.width), (PVS-16) / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height },
                           { pv.x + (PVS - t.width*sc)/2, pv.y + (PVS - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
            DrawTextU(TextFormat("%d / %d", fi+1, n), (int)pv.x + 5, (int)pv.y + 4, 14, ui::kGood);
            DrawTextU(TextFormat("%dfps · %s", clip.fps, clip.loop ? "반복" : "1회"),
                      (int)pv.x, (int)(pv.y + PVS + 2), 12, ui::kTextDim);
        } else {
            DrawTextU("프레임 없음", (int)pv.x + 14, (int)pv.y + PVS/2 - 8, 13, ui::kTextDim);
            DrawTextU("아래 라이브러리 클릭", (int)pv.x, (int)(pv.y + PVS + 2), 12, ui::kTextDim);
        }

        // ---- [5] 프레임: 선택 후 이동/복제/삭제 + 썸네일 ----
        float py3 = ey2 + 30;
        DrawTextU("프레임", (int)bx + 12, (int)py3 + 4, 12, ui::kTextDim);
        if (charFrameSel_ >= 0) {
            float bX = bx + 64;
            if (ui::button({ bX, py3, 50, 22 }, "앞으로") && charFrameSel_ > 0) { std::swap(clip.frames[charFrameSel_], clip.frames[charFrameSel_-1]); charFrameSel_--; p.save(); }
            if (ui::button({ bX+56, py3, 50, 22 }, "복제")) { clip.frames.insert(clip.frames.begin()+charFrameSel_+1, clip.frames[charFrameSel_]); charFrameSel_++; p.save(); }
            if (ui::button({ bX+112, py3, 50, 22 }, "삭제", false)) { snap(); clip.frames.erase(clip.frames.begin()+charFrameSel_); charFrameSel_ = -1; p.save(); }
            if (ui::button({ bX+168, py3, 50, 22 }, "뒤로") && charFrameSel_ < (int)clip.frames.size()-1) { std::swap(clip.frames[charFrameSel_], clip.frames[charFrameSel_+1]); charFrameSel_++; p.save(); }
        } else {
            DrawTextU("썸네일 클릭 = 프레임 선택 후 이동/복제/삭제", (int)bx + 64, (int)py3 + 4, 12, ui::kTextDim);
        }
        float frx0 = bx + 12, fry0 = py3 + 26;
        int perRow = std::max(1, (int)((bx + bw - 24 - frx0) / 40));
        for (int i = 0; i < (int)clip.frames.size(); ++i) {
            Rectangle fr = { frx0 + (i % perRow)*40, fry0 + (i / perRow)*40, 36, 36 };
            if (fr.y + 36 > by + bh - 4) break;
            DrawRectangleRec(fr, ui::kPanelHi);
            const Texture2D& t = engine_.assetTexture(clip.frames[i]);
            float sc = std::min(32.0f / std::max(1, t.width), 32.0f / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height },
                           { fr.x + (36 - t.width*sc)/2, fr.y + (36 - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
            DrawRectangleLinesEx(fr, 2, charFrameSel_ == i ? ui::kAccent : Fade(BLACK, 0.5f));
            DrawTextU(TextFormat("%d", i+1), (int)fr.x + 1, (int)fr.y + 1, 10, ui::kTextDim);
            if (ui::mouseIn(fr) && lclick) charFrameSel_ = i;
        }
    }

    // ============ image library (frame source) ============
    float gy2 = by + bh + 8;
    ui::label(building ? TextFormat("이미지 라이브러리 — 클릭하면 '%s' 모션에 프레임으로 추가됩니다", kMotionNames[charMotionTab_])
                       : "이미지 라이브러리",
             20, (int)gy2, 16, ui::kAccent);
    if (building) {
        if (ui::button({ 430, gy2 - 4, 120, 24 }, charLibFilter_ ? "필터: 캐릭터만" : "필터: 전체", charLibFilter_))
            charLibFilter_ = !charLibFilter_;
        if (ui::button({ 558, gy2 - 4, 150, 24 },
                       charSliceMode_ ? TextFormat("추가: %d분할", charSliceN_) : "추가: 1프레임", charSliceMode_))
            charSliceMode_ = !charSliceMode_;
        if (charSliceMode_) ui::intStepper({ 716, gy2 - 4, 130, 24 }, "분할수", charSliceN_, 1, 2, 16);
    }
    float cell = 92;
    // mouse-wheel scroll over the library region (E)
    float clipY = gy2 + 22;
    Rectangle libRegion = { 0, clipY, area.width, (float)GetScreenHeight() - clipY };
    int perRow   = std::max(1, (int)((area.width - 36) / (cell + 10)));
    int rowsTot  = ((int)imgs.size() + perRow - 1) / perRow;
    float contentH = rowsTot * (cell + 42) + 8;
    int maxScroll  = std::max(0, (int)(contentH - libRegion.height));
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && ui::mouseIn(libRegion)) charLibScroll_ -= (int)(wheel * 48);
    charLibScroll_ = std::max(0, std::min(charLibScroll_, maxScroll));

    BeginScissorMode(0, (int)clipY, (int)area.width, GetScreenHeight() - (int)clipY);
    float x = 20, y = gy2 + 26 - charLibScroll_;
    for (auto* a : imgs) {
        if (building && charLibFilter_) {                 // show only frame-sized images
            const Texture2D& tt = engine_.assetTexture(a->id);
            if (tt.height > 64) continue;
        }
        bool vis = !(y + cell + 36 < clipY || y > GetScreenHeight());
        if (vis) {
            ui::panel({ x, y, cell, cell + 36 }, (a->id == p.playerSprite && p.playerCharId < 0) ? ui::kPanelHi : ui::kPanel);
            const Texture2D& tex = engine_.assetTexture(a->id);
            float sc = std::min((cell-10) / std::max(1, tex.width), 58.0f / std::max(1, tex.height));
            DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height },
                           { x + (cell - tex.width*sc)/2, y + 5, tex.width*sc, tex.height*sc }, {0,0}, 0, WHITE);
            DrawTextU(a->name.c_str(), (int)x + 5, (int)(y + cell - 26), 11, ui::kText);
            Rectangle clickArea = { x, y, cell, cell - 18 };
            if (building && ui::mouseIn(clickArea) && lclick) {
                MotionClip& mc = db.characters[charDefSel_].motions[charMotionTab_];
                charUndo_ = mc; charUndoSet_ = true;       // snapshot for 되돌리기
                auto& mf = mc.frames;
                int pos = (charFrameSel_ >= 0 && charFrameSel_ < (int)mf.size()) ? charFrameSel_ + 1 : (int)mf.size();
                // Animated assets (움짤 GIF / sprite-strips) auto-expand into all
                // their frames so one click turns a 움짤 into a whole motion.
                int n = (a->frames > 1) ? a->frames : (charSliceMode_ ? charSliceN_ : 1);
                if (n > 1) {
                    std::vector<int> sl = sliceAsset(a->id, n);
                    mf.insert(mf.begin() + pos, sl.begin(), sl.end());
                    if (charFrameSel_ >= 0) charFrameSel_ += (int)sl.size();
                    setStatus(TextFormat("%s: %d프레임 추가 (움짤/시트 분할)", kMotionNames[charMotionTab_], (int)sl.size()));
                } else {
                    mf.insert(mf.begin() + pos, a->id);
                    if (charFrameSel_ >= 0) charFrameSel_++;
                    setStatus(std::string(kMotionNames[charMotionTab_]) + " 프레임 추가: " + a->name);
                }
                p.save();
            }
            if (ui::button({ x + 5, y + cell - 14, cell - 10, 20 }, "시트P")) {
                p.playerSprite = a->id; p.playerCharId = -1;
                if (a->name.rfind("char_", 0) == 0) { p.playerFrames = 4; p.playerAtkFrames = 2; }
                p.save(); setStatus("시트 플레이어 설정.");
            }
            if (building && ui::button({ x + 5, y + cell + 8, cell - 10, 20 }, "자동구성")) {  // D: sheet → walk+attack
                CharacterDef& c = db.characters[charDefSel_];
                std::vector<int> fr = sliceSheetRow0(a->id);
                int w = std::min((int)fr.size(), 4);
                c.motions[MO_Walk].frames.assign(fr.begin(), fr.begin() + w);
                c.motions[MO_Walk].loop = true;
                c.motions[MO_Attack].frames.assign(fr.begin() + w, fr.end());
                charFrameSel_ = -1; p.save();
                setStatus(TextFormat("시트 자동구성: 걷기%d+공격%d", w, (int)fr.size() - w));
            }
        }
        x += cell + 10;
        if (x + cell > area.width - 16) { x = 20; y += cell + 42; }
    }
    EndScissorMode();
    if (imgs.empty())
        ui::label("(이미지 없음 — 위 '+ 내 이미지 불러오기'로 가져오거나 '+ 시트 캐릭터'로 생성하세요)",
                  20, (int)gy2 + 30, 16, ui::kTextDim);
}

// Built-in image file browser: lets the user import their own image files into
// the project without relying on OS drag&drop. Folders navigate; image files are
// copied into the project and registered so they appear in the library.
void Editor::drawImageBrowser() {
    Project& p = engine_.project();
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 20, 22, 30, 255 });
    handleAssetDrop();   // dropping files still works here too

    if (browseDir_.empty() || !fs::exists(browseDir_)) {
        const char* h = getenv("HOME"); const char* u = getenv("USERPROFILE");
        browseDir_ = h ? h : (u ? u : ".");
    }

    ui::label("내 이미지 불러오기", 20, (int)kToolbarH + 10, 22, ui::kAccent);
    DrawTextU("폴더를 눌러 이동, 이미지 파일을 누르면 등록됩니다. PNG·JPG·BMP·GIF·TGA 등 모두 가능 · 움짤(GIF)은 자동으로 여러 프레임이 됩니다.",
              290, (int)kToolbarH + 16, 13, ui::kTextDim);
    if (ui::button({ area.width - 180, kToolbarH + 8, 160, 28 }, "← 빌더로 돌아가기")) { charBrowse_ = false; return; }

    float ny = kToolbarH + 46;
    if (ui::button({ 20, ny, 70, 26 }, ".. 상위")) {
        fs::path pp(browseDir_); auto par = pp.parent_path();
        if (!par.empty() && par != pp) browseDir_ = par.string();
        browseScroll_ = 0;
    }
    float qx = 98;
    auto quick = [&](const char* lbl, const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        if (ui::button({ qx, ny, 100, 26 }, lbl)) { browseDir_ = dir.string(); browseScroll_ = 0; }
        qx += 106;
    };
    if (const char* h = getenv("HOME"))        { quick("홈", h); quick("바탕화면", fs::path(h) / "Desktop"); }
    if (const char* u = getenv("USERPROFILE")) { quick("홈", u); quick("바탕화면", fs::path(u) / "Desktop"); }
    quick("프로젝트", fs::path(p.dir) / "assets");
    DrawTextU(browseDir_.c_str(), 20, (int)ny + 32, 13, ui::kAccentHi);

    // gather sub-folders and image files
    std::vector<std::string> dirs, files;
    std::error_code ec;
    for (auto& de : fs::directory_iterator(browseDir_, fs::directory_options::skip_permission_denied, ec)) {
        std::string name = de.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        std::error_code e2;
        if (de.is_directory(e2)) { dirs.push_back(name); continue; }
        std::string ext = de.path().extension().string();
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (isImageExt(ext)) files.push_back(name);
    }
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    // scrollable list of folders (then image files)
    float listY = ny + 56;
    Rectangle listRegion = { 0, listY, area.width, (float)GetScreenHeight() - listY };
    float rowH = 28;
    int total = (int)dirs.size() + (int)files.size();
    int maxScroll = std::max(0, (int)(total * rowH + 8 - listRegion.height));
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && ui::mouseIn(listRegion)) browseScroll_ -= (int)(wheel * 48);
    browseScroll_ = std::max(0, std::min(browseScroll_, maxScroll));

    if (total == 0)
        DrawTextU("(이 폴더에 이미지가 없습니다 — 다른 폴더로 이동하세요)", 24, (int)listY + 8, 15, ui::kTextDim);

    BeginScissorMode(0, (int)listY, (int)area.width, GetScreenHeight() - (int)listY);
    float ry = listY - browseScroll_;
    for (auto& d : dirs) {
        if (ry + rowH > listY && ry < GetScreenHeight() &&
            ui::button({ 20, ry, area.width - 40, rowH - 4 }, "[폴더]  " + d))
            { browseDir_ = (fs::path(browseDir_) / d).string(); browseScroll_ = 0; }
        ry += rowH;
    }
    for (auto& f : files) {
        if (ry + rowH > listY && ry < GetScreenHeight() &&
            ui::button({ 20, ry, area.width - 40, rowH - 4 }, "[그림]  " + f)) {
            std::string full = (fs::path(browseDir_) / f).string();
            int id = importImageFile(full);     // GIF-aware; all image formats
            if (id >= 0) p.save();              // setStatus handled by importImageFile
            else setStatus("불러오기 실패: " + f);
        }
        ry += rowH;
    }
    EndScissorMode();
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
