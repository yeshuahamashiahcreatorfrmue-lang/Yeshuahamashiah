// EditorAssets: registered-asset browser.
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

// --- shared asset pickers (one definition for every editor panel) -----------
std::string Editor::assetName(int id) const {
    const AssetEntry* e = engine_.project().assets.find(id);
    return e ? e->name : std::string("없음");
}

void Editor::cycleAsset(int& cur, AssetType t) {
    auto list = engine_.project().assets.byType(t);
    int idx = -1;
    for (int i = 0; i < (int)list.size(); ++i) if (list[i]->id == cur) idx = i;
    ++idx;
    cur = (idx >= (int)list.size()) ? -1 : list[idx]->id;
}

void Editor::drawAssetsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    ui::label(".png / .wav / .ogg 파일을 창에 끌어다 놓으면 등록됩니다.",
              20, (int)kToolbarH + 16, 20, ui::kText);

    Project& p = engine_.project();
    auto& assets = p.assets.all();
    float x = 20, y = kToolbarH + 56;
    float thumb = 96, pad = 16, cellW = thumb + 80;

    for (const auto& a : assets) {
        Rectangle cell = { x, y, cellW, thumb + 60 };
        ui::panel(cell, ui::kPanel);
        if (a.type == AssetType::Image) {
            const Texture2D& tex = engine_.assetTexture(a.id);
            float s = std::min(thumb / std::max(1, tex.width), thumb / std::max(1, tex.height));
            DrawTextureEx(tex, { x + 8, y + 8 }, 0, s, WHITE);
        } else {
            DrawTextU("♪ AUDIO", (int)x + 10, (int)y + 40, 18, ui::kAccent);
        }
        DrawTextU(a.name.c_str(), (int)x + 8, (int)(y + thumb + 12), 14, ui::kText);
        DrawTextU(TextFormat("id %d", a.id), (int)x + 8, (int)(y + thumb + 30), 12, ui::kTextDim);

        // image action buttons
        if (a.type == AssetType::Image) {
            if (ui::button({ x + thumb + 12, y + 8, 100, 24 }, "타일셋", false)) {
                if (auto m = activeMap()) { m->tileset.assetId = a.id; setStatus("타일셋 설정됨."); }
            }
            if (ui::button({ x + thumb + 12, y + 36, 100, 24 }, "플레이어", false)) {
                p.playerSprite = a.id; setStatus("플레이어 스프라이트 설정됨.");
            }
        }
        x += cellW + pad;
        if (x + cellW > GetScreenWidth() - 20) { x = 20; y += thumb + 60 + pad; }
    }

    if (assets.empty())
        ui::label("(아직 에셋이 없습니다)", 20, (int)kToolbarH + 56, 18, ui::kTextDim);
}


} // namespace tsukuru
