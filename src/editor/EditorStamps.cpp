// EditorStamps: multi-tile prefab stamping + stamp palette.
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


// ---- prefab stamps (multi-tile / multi-layer building blocks) ----
void Editor::stampPrefab(int ox, int oy) {
    auto m = activeMap();
    if (!m) return;
    const auto& list = prefabs();
    if (prefabSel_ < 0 || prefabSel_ >= (int)list.size()) return;
    pushUndo();
    for (const auto& c : list[prefabSel_].cells) {
        int x = ox + c.dx, y = oy + c.dy;
        if (!m->tilemap.inBounds(x, y)) continue;
        if (c.tile >= 0) m->tilemap.setTile(c.layer, x, y, c.tile);
        if (c.blocked)   m->tilemap.setBlocked(x, y, true);
    }
}

void Editor::drawPrefabPalette(Rectangle area) {
    ui::panel(area, ui::kPanel);
    ui::label("스탬프", (int)area.x + 10, (int)area.y + 8, 16, ui::kAccent);
    const auto& list = prefabs();
    float y = area.y + 32;
    for (int i = 0; i < (int)list.size(); ++i) {
        if (ui::button({ area.x + 10, y, area.width - 20, 24 }, list[i].name, prefabSel_ == i))
            prefabSel_ = i;
        y += 27;
    }
    ui::label("맵을 클릭해 배치하세요.", (int)area.x + 10, (int)(y + 6), 13, ui::kTextDim);
    ui::label("(나무/집/연못 등의", (int)area.x + 10, (int)(y + 24), 12, ui::kTextDim);
    ui::label(" 타일+충돌 묶음)", (int)area.x + 10, (int)(y + 40), 12, ui::kTextDim);
}

// ============================ update ============================

} // namespace tsukuru
