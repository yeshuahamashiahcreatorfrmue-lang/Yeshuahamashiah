// EditorChars: character/effect asset workshop.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "render/AssetGen.h"
#include "render/SfxGen.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {

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
    handleAssetDrop(); // drop new images/sheets to register them

    auto imgs = p.assets.byType(AssetType::Image);

    // ============ custom multi-motion character builder ============
    float bx = 16, by = kToolbarH + 72, bw = area.width - 32, bh = 318;
    ui::panel({ bx, by, bw, bh }, ui::kPanel);
    ui::label("커스텀 캐릭터 빌더", (int)bx + 12, (int)by + 8, 17, ui::kAccent);

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
        // motion preview (right edge)
        Rectangle pv = { bx + bw - 72, ey - 4, 56, 56 };
        DrawRectangleRec(pv, Color{ 20, 22, 30, 255 });
        if (!clip.frames.empty()) {
            int n = (int)clip.frames.size(); int fi = (int)(GetTime() * std::max(1, clip.fps)) % n;
            const Texture2D& t = engine_.assetTexture(clip.frames[fi]);
            float sc = std::min(52.0f / std::max(1, t.width), 52.0f / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height },
                           { pv.x + (56 - t.width*sc)/2, pv.y + (56 - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
        }
        DrawTextU("미리보기", (int)pv.x - 2, (int)(pv.y + 58), 11, ui::kTextDim);

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
    ui::label(building ? "이미지 라이브러리 (클릭 = 현재 모션에 프레임 추가)" : "이미지 라이브러리",
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
                if (charSliceMode_) {                      // B: insert at selected frame
                    int n = (a->frames > 1) ? a->frames : charSliceN_;
                    std::vector<int> sl = sliceAsset(a->id, n);
                    mf.insert(mf.begin() + pos, sl.begin(), sl.end());
                    if (charFrameSel_ >= 0) charFrameSel_ += (int)sl.size();
                    setStatus(TextFormat("%s: %d프레임 분할 추가", kMotionNames[charMotionTab_], (int)sl.size()));
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
        ui::label("(이미지 없음 - 시트 캐릭터 생성 또는 이미지를 드롭하세요)", 20, (int)gy2 + 30, 16, ui::kTextDim);
}


} // namespace tsukuru
