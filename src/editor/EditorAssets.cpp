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

// ---- dropdown picker (click → expand all options → select) ------------------
void Editor::optionButton(Rectangle r, const std::string& label,
                          const std::vector<std::string>& opts, const std::vector<int>& values,
                          int& target, int id) {
    int cur = 0;
    for (int i = 0; i < (int)opts.size(); ++i) { int v = values.empty() ? i : values[i]; if (v == target) cur = i; }
    std::string disp = opts.empty() ? "" : opts[cur];
    std::string txt = label.empty() ? disp : (label + ": " + disp);
    if (ui::button(r, txt, pickerId_ == id)) {
        if (pickerId_ == id) pickerId_ = -1;
        else { pickerId_ = id; pickerScroll_ = 0; }
    }
    if (pickerId_ == id) {   // keep the open list's data fresh against layout/scroll
        pickerAnchor_ = r; pickerOpts_ = opts; pickerValues_ = values;
        pickerTarget_ = &target; pickerStrTarget_ = nullptr;
    }
}

// string-valued dropdown (e.g. sound-effect name)
void Editor::optionButtonStr(Rectangle r, const std::string& label,
                             const std::vector<std::string>& opts, const std::vector<std::string>& values,
                             std::string& target, int id) {
    int cur = 0;
    for (int i = 0; i < (int)values.size(); ++i) if (values[i] == target) cur = i;
    std::string disp = opts.empty() ? "" : opts[cur];
    std::string txt = label.empty() ? disp : (label + ": " + disp);
    if (ui::button(r, txt, pickerId_ == id)) {
        if (pickerId_ == id) pickerId_ = -1;
        else { pickerId_ = id; pickerScroll_ = 0; }
    }
    if (pickerId_ == id) {
        pickerAnchor_ = r; pickerOpts_ = opts; pickerStrValues_ = values;
        pickerStrTarget_ = &target; pickerTarget_ = nullptr;
    }
}

// asset dropdown: 없음 + every registered image, mapping to its asset id.
void Editor::assetButton(Rectangle r, const std::string& label, int& assetId, int id) {
    std::vector<std::string> opts = { "없음" };
    std::vector<int> vals = { -1 };
    for (const auto* a : engine_.project().assets.byType(AssetType::Image)) { opts.push_back(a->name); vals.push_back(a->id); }
    optionButton(r, label, opts, vals, assetId, id);
}

// Drawn ON TOP, after the active tab, so the expanded list overlays everything.
void Editor::drawPickerOverlay() {
    if (pickerId_ < 0 || (!pickerTarget_ && !pickerStrTarget_) || pickerOpts_.empty()) return;
    int n = (int)pickerOpts_.size();
    float rowH = 24, w = std::max(pickerAnchor_.width, 180.0f);
    float x = pickerAnchor_.x;
    float y = pickerAnchor_.y + pickerAnchor_.height + 2;
    float maxH = (float)screenH() - y - 12;
    float fullH = n * rowH + 6;
    float listH = std::min(fullH, std::max(rowH * 3, maxH));
    Rectangle box = { x - 2, y - 2, w + 4, listH + 4 };
    DrawRectangleRec(box, ui::kPanelHi);
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    if (ui::mouseIn(box)) pickerScroll_ -= GetMouseWheelMove() * rowH * 2;
    float maxScroll = std::max(0.0f, fullH - listH);
    if (pickerScroll_ < 0) pickerScroll_ = 0; if (pickerScroll_ > maxScroll) pickerScroll_ = maxScroll;
    BeginScissorMode((int)(box.x * uiScale()), (int)(box.y * uiScale()), (int)(box.width * uiScale()), (int)(box.height * uiScale()));
    float oy = y - pickerScroll_;
    for (int i = 0; i < n; ++i) {
        bool sel = pickerStrTarget_ ? (i < (int)pickerStrValues_.size() && pickerStrValues_[i] == *pickerStrTarget_)
                                    : ((pickerValues_.empty() ? i : pickerValues_[i]) == *pickerTarget_);
        if (oy + rowH > y - rowH && oy < y + listH) {
            if (ui::button({ x, oy, w, rowH - 2 }, pickerOpts_[i], sel)) {
                if (pickerStrTarget_) { if (i < (int)pickerStrValues_.size()) *pickerStrTarget_ = pickerStrValues_[i]; }
                else *pickerTarget_ = pickerValues_.empty() ? i : pickerValues_[i];
                pickerId_ = -1; pickerTarget_ = nullptr; pickerStrTarget_ = nullptr;
                engine_.project().save();
                EndScissorMode(); return;
            }
        }
        oy += rowH;
    }
    EndScissorMode();
    // click outside the list (and not on the anchor) closes it
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !ui::mouseIn(box) && !ui::mouseIn(pickerAnchor_)) { pickerId_ = -1; pickerTarget_ = nullptr; pickerStrTarget_ = nullptr; }
}

void Editor::drawAssetsTab() {
    Rectangle area = { 0, kToolbarH, (float)screenW(), (float)screenH() - kToolbarH };
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
        if (x + cellW > screenW() - 20) { x = 20; y += thumb + 60 + pad; }
    }

    if (assets.empty())
        ui::label("(아직 에셋이 없습니다)", 20, (int)kToolbarH + 56, 18, ui::kTextDim);
}


} // namespace tsukuru
