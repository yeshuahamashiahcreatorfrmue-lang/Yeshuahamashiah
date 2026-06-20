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
    float bx = 16, by = kToolbarH + 72, bw = area.width - 32, bh = 250;
    ui::panel({ bx, by, bw, bh }, ui::kPanel);
    ui::label("커스텀 캐릭터 (이미지 모션 빌더)", (int)bx + 12, (int)by + 8, 17, ui::kAccent);

    float cy = by + 34;
    if (ui::button({ bx + 12, cy, 110, 26 }, "+ 새 캐릭터")) {
        CharacterDef cd; cd.id = (int)db.characters.size() + 1;
        cd.name = "캐릭터" + std::to_string(cd.id);
        db.characters.push_back(cd); charDefSel_ = (int)db.characters.size() - 1; p.save();
    }
    float clx = bx + 130;
    for (int i = 0; i < (int)db.characters.size(); ++i) {
        if (ui::button({ clx, cy, 120, 26 }, db.characters[i].name, charDefSel_ == i)) {
            charDefSel_ = i; charDefNameFocus_ = false;
        }
        clx += 126;
        if (clx > bx + bw - 130) break;
    }

    if (charDefSel_ < 0 && !db.characters.empty()) charDefSel_ = 0;
    if (charDefSel_ < 0 || charDefSel_ >= (int)db.characters.size()) {
        DrawTextU("'+ 새 캐릭터'로 만든 뒤, 모션 탭마다 아래 이미지를 클릭해 프레임을 추가하세요.",
                 (int)bx + 12, (int)by + 72, 14, ui::kTextDim);
    } else {
        CharacterDef& cd = db.characters[charDefSel_];
        float ry = cy + 34;
        ui::label("이름:", (int)bx + 12, (int)ry, 12, ui::kTextDim);
        Rectangle nf = { bx + 50, ry - 4, 200, 24 };
        if (ui::mouseIn(nf) && lclick) charDefNameFocus_ = true;
        else if (lclick && !ui::mouseIn(nf)) charDefNameFocus_ = false;
        ui::textField(nf, cd.name, charDefNameFocus_, 20);
        bool isP = (p.playerCharId == cd.id);
        if (ui::button({ bx + 262, ry - 4, 156, 24 }, isP ? "플레이어 (현재)" : "플레이어로 설정", isP)) {
            p.playerCharId = cd.id; p.save(); setStatus("커스텀 캐릭터를 플레이어로 설정.");
        }
        if (ui::button({ bx + 426, ry - 4, 80, 24 }, "삭제", false)) {
            if (p.playerCharId == cd.id) p.playerCharId = -1;
            db.characters.erase(db.characters.begin() + charDefSel_);
            charDefSel_ = -1; p.save(); return;
        }

        // motion tabs (with frame counts)
        float ty = ry + 32;
        for (int m = 0; m < MO_COUNT; ++m) {
            std::string lbl = std::string(kMotionNames[m]) + "(" + std::to_string((int)cd.motions[m].frames.size()) + ")";
            if (ui::button({ bx + 12 + m*88, ty, 84, 26 }, lbl, charMotionTab_ == m)) charMotionTab_ = m;
        }
        MotionClip& clip = cd.motions[charMotionTab_];

        float fy = ty + 34;
        ui::intStepper({ bx + 12, fy, 150, 24 }, "fps", clip.fps, 1, 1, 30);
        // animated preview of the selected motion
        Rectangle pv = { bx + 176, fy - 4, 60, 60 };
        DrawRectangleRec(pv, Color{ 20, 22, 30, 255 });
        if (!clip.frames.empty()) {
            int n = (int)clip.frames.size();
            int fi = (int)(GetTime() * std::max(1, clip.fps)) % n;
            const Texture2D& t = engine_.assetTexture(clip.frames[fi]);
            float sc = std::min(56.0f / std::max(1, t.width), 56.0f / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height },
                           { pv.x + (60 - t.width*sc)/2, pv.y + (60 - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
        }
        DrawTextU(TextFormat("%s 모션 · %d 프레임", kMotionNames[charMotionTab_], (int)clip.frames.size()),
                 (int)bx + 248, (int)fy, 13, ui::kText);
        DrawTextU("아래 라이브러리 이미지 클릭 = 프레임 추가 · 아래 프레임 클릭 = 삭제",
                 (int)bx + 248, (int)fy + 20, 12, ui::kTextDim);
        // current motion's frame thumbnails (click to remove)
        float frx = bx + 248, fry = fy + 40;
        for (int i = 0; i < (int)clip.frames.size() && frx + i*40 < bx + bw - 60; ++i) {
            Rectangle fr = { frx + i*40, fry, 36, 36 };
            DrawRectangleRec(fr, ui::kPanelHi);
            const Texture2D& t = engine_.assetTexture(clip.frames[i]);
            float sc = std::min(32.0f / std::max(1, t.width), 32.0f / std::max(1, t.height));
            DrawTexturePro(t, { 0,0,(float)t.width,(float)t.height },
                           { fr.x + (36 - t.width*sc)/2, fr.y + (36 - t.height*sc)/2, t.width*sc, t.height*sc }, {0,0}, 0, WHITE);
            DrawRectangleLinesEx(fr, 1, Fade(BLACK, 0.5f));
            DrawTextU(TextFormat("%d", i+1), (int)fr.x+1, (int)fr.y+1, 10, ui::kTextDim);
            if (ui::mouseIn(fr) && lclick) { clip.frames.erase(clip.frames.begin() + i); p.save(); break; }
        }
    }

    // ============ image library ============
    bool building = (charDefSel_ >= 0 && charDefSel_ < (int)db.characters.size());
    float gy2 = by + bh + 10;
    ui::label(building ? "이미지 라이브러리 (이미지 클릭 = 현재 모션에 프레임 추가)"
                       : "이미지 라이브러리", 20, (int)gy2, 16, ui::kAccent);
    float x = 20, y = gy2 + 24, cell = 116;
    for (auto* a : imgs) {
        Rectangle c = { x, y, cell, cell + 44 };
        bool isSheet = (a->id == p.playerSprite && p.playerCharId < 0);
        ui::panel(c, isSheet ? ui::kPanelHi : ui::kPanel);
        const Texture2D& tex = engine_.assetTexture(a->id);
        float sc = std::min((cell-12) / std::max(1, tex.width), 74.0f / std::max(1, tex.height));
        DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height },
                       { x + (cell - tex.width*sc)/2, y + 6, tex.width*sc, tex.height*sc }, {0,0}, 0, WHITE);
        DrawTextU(a->name.c_str(), (int)x + 6, (int)(y + cell - 30), 12, ui::kText);
        Rectangle clickArea = { x, y, cell, cell - 22 };   // image area = add-frame target
        if (building && ui::mouseIn(clickArea) && lclick) {
            db.characters[charDefSel_].motions[charMotionTab_].frames.push_back(a->id);
            p.save(); setStatus(std::string(kMotionNames[charMotionTab_]) + " 프레임 추가: " + a->name);
        }
        if (ui::button({ x + 6, y + cell - 16, cell - 12, 22 }, "시트 플레이어")) {
            p.playerSprite = a->id; p.playerCharId = -1;
            if (a->name.rfind("char_", 0) == 0) { p.playerFrames = 4; p.playerAtkFrames = 2; }
            p.save(); setStatus("시트 플레이어 설정.");
        }
        x += cell + 12;
        if (x + cell > area.width - 20) { x = 20; y += cell + 50; }
    }
    if (imgs.empty())
        ui::label("(이미지가 없습니다 - 시트 캐릭터 생성 또는 이미지를 드롭하세요)", 20, (int)gy2 + 30, 16, ui::kTextDim);
}


} // namespace tsukuru
