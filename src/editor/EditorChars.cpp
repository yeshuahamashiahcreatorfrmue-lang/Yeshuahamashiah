// EditorChars: character/effect asset workshop.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "render/AssetGen.h"
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

    Image img = gen::characterSheet(shirt, skin);
    ExportImage(img, dest.string().c_str());
    UnloadImage(img);

    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
    p.save();
    setStatus("캐릭터 생성됨: " + dest.stem().string());
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

void Editor::drawCharsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    ui::label("캐릭터 / 이펙트 에셋", 20, (int)kToolbarH + 14, 24, ui::kAccent);
    DrawTextU("캐릭터/이펙트를 엔진에서 생성하거나, PNG 시트(N프레임 x 4방향)를 창에 끌어다 놓으세요.",
             20, (int)kToolbarH + 44, 15, ui::kTextDim);
    if (ui::button({ 20, kToolbarH + 68, 220, 30 }, "+ 새 캐릭터 생성"))
        generateCharacter();
    // generate one of each effect style (slash/bolt/dash/burst)
    static const char* fxBtn[4] = { "+ 베기", "+ 볼트", "+ 대시", "+ 폭발" };
    for (int s = 0; s < 4; ++s)
        if (ui::button({ 252.0f + s*110, kToolbarH + 68, 104, 30 }, fxBtn[s]))
            generateEffect(s);
    handleAssetDrop(); // allow dropping character/effect sheets here too

    auto imgs = p.assets.byType(AssetType::Image);

    // ---- player movement frames + skill-effect assignment ----
    auto imgName = [&](int id)->std::string {
        if (id < 0) return "없음";
        const AssetEntry* e = p.assets.find(id);
        return e ? e->name : "없음";
    };
    auto cycleAsset = [&](int& slot){            // None -> each image -> None
        int idx = -1;
        for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == slot) idx = i;
        idx++;
        slot = (idx >= (int)imgs.size()) ? -1 : imgs[idx]->id;
        p.save();
    };
    float py = kToolbarH + 104;
    ui::panel({ 20, py, 720, 92 }, ui::kPanel);
    ui::label("플레이어 / 스킬 이펙트 지정", 30, (int)py + 6, 16, ui::kAccent);
    ui::intStepper({ 30, py + 30, 200, 26 }, "이동 프레임", p.playerFrames, 1, 4, 7);
    static const char* slotName[4] = { "공격(Z)", "원거리(X)", "회피(C)", "궁극기(V)" };
    int* slots[4] = { &p.attackEffect, &p.rangedEffect, &p.dashEffect, &p.ultEffect };
    for (int i = 0; i < 4; ++i) {
        Rectangle r = { 250.0f + (i%2)*240, py + 30 + (i/2)*30, 232, 26 };
        if (ui::button(r, std::string(slotName[i]) + ": " + imgName(*slots[i]), *slots[i] >= 0))
            cycleAsset(*slots[i]);
    }

    // grid of image assets, with the 'facing-down' frame preview
    float x = 20, y = py + 108, cell = 150;
    for (auto* a : imgs) {
        Rectangle c = { x, y, cell, cell + 56 };
        bool isPlayer = (a->id == p.playerSprite);
        ui::panel(c, isPlayer ? ui::kPanelHi : ui::kPanel);
        const Texture2D& tex = engine_.assetTexture(a->id);
        // draw the down-facing first frame (sheet is 4x4); scale to ~96px
        float fw = tex.width / 4.0f, fh = tex.height / 4.0f;
        float sc = 96.0f / (fh > 0 ? fh : 1);
        Rectangle src = { 0, 0, fw, fh };
        Rectangle dst = { x + (cell - fw*sc)/2, y + 8, fw*sc, fh*sc };
        DrawTexturePro(tex, src, dst, {0,0}, 0, WHITE);
        DrawTextU(a->name.c_str(), (int)x + 8, (int)(y + cell - 36), 14, ui::kText);
        if (ui::button({ x + 8, y + cell - 16, cell - 16, 26 },
                       isPlayer ? "플레이어" : "플레이어로 설정", isPlayer)) {
            p.playerSprite = a->id; p.save(); setStatus("플레이어 캐릭터 설정됨.");
        }
        x += cell + 14;
        if (x + cell > area.width - 20) { x = 20; y += cell + 70; }
    }
    if (imgs.empty())
        ui::label("(아직 캐릭터가 없습니다 - 생성을 클릭하세요)", 20, (int)kToolbarH + 120, 18, ui::kTextDim);
}


} // namespace tsukuru
