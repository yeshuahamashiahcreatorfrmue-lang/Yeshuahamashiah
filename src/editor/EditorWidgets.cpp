// EditorWidgets: reusable editor UI — dropdown pickers (click → expand all
// options → select) for plain options, strings, assets, and database entities.
// One open picker at a time; the open list is drawn on top by drawPickerOverlay().
#include "editor/Editor.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "database/Database.h"
#include <algorithm>
#include <cctype>

namespace tsukuru {

// case-insensitive "name contains query" (ASCII-folded; Korean compares as-is)
bool Editor::nameMatch(const std::string& name, const std::string& q) {
    if (q.empty()) return true;
    auto low = [](std::string s){ for (char& c : s) c = (char)tolower((unsigned char)c); return s; };
    return low(name).find(low(q)) != std::string::npos;
}

// 검색 입력칸: a small text field with a placeholder; returns the current query.
std::string Editor::searchBox(Rectangle r, std::string& text, int id) {
    DrawRectangleRec(r, ui::kPanel);
    DrawRectangleLinesEx(r, 1, searchFocusId_ == id ? ui::kAccent : Fade(BLACK, 0.5f));
    if (ui::g_inputEnabled && ui::mouseIn(r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) searchFocusId_ = id;
    else if (ui::g_inputEnabled && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(r) && searchFocusId_ == id) searchFocusId_ = -1;
    ui::textField({ r.x + 6, r.y, r.width - 10, r.height }, text, searchFocusId_ == id, 40);
    if (text.empty() && searchFocusId_ != id)
        DrawTextU("검색…", (int)r.x + 8, (int)r.y + 4, 13, ui::kTextDim);
    return text;
}

// web-style autoscroll: middle-click toggles a scroll anchor; moving the cursor
// away from it scrolls that way with speed ∝ distance. Wheel also scrolls.
void Editor::autoScroll(Rectangle r, float* sf, int* si, float maxS) {
    Vector2 m = GetMousePosition();
    void* key = si ? (void*)si : (void*)sf;
    auto get = [&]{ return si ? (float)*si : *sf; };
    auto set = [&](float v){ if (si) *si = (int)v; else *sf = v; };
    bool inR = CheckCollisionPointRec(m, r);
    if (inR) { float w = GetMouseWheelMove(); if (w != 0) set(get() - w * 48); }
    if (inR && IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        if (autoScrollTarget_ == key) autoScrollTarget_ = nullptr;
        else { autoScrollTarget_ = key; autoScrollOrigin_ = m; autoScrollAccum_ = 0; }
    }
    if (autoScrollTarget_ == key) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) autoScrollTarget_ = nullptr;
        else {
            float dy = m.y - autoScrollOrigin_.y;
            autoScrollAccum_ += dy * 0.10f;
            int step = (int)autoScrollAccum_; autoScrollAccum_ -= step;
            set(get() + step);
            DrawCircleLines((int)autoScrollOrigin_.x, (int)autoScrollOrigin_.y, 13, ui::kAccentHi);
            DrawCircle((int)autoScrollOrigin_.x, (int)autoScrollOrigin_.y, 3, Fade(ui::kAccentHi, 0.7f));
        }
    }
    float v = get(); if (v < 0) v = 0; if (v > maxS) v = maxS; set(v);
}

// float scrollbar: wheel + middle-autoscroll + draggable thumb (web-like).
void Editor::scrollbar(Rectangle r, float& scroll, float contentH) {
    float maxS = std::max(0.0f, contentH - r.height);
    autoScroll(r, &scroll, nullptr, maxS);
    if (maxS <= 0) { scroll = 0; return; }
    Vector2 m = GetMousePosition();
    const float trackW = 10;
    float tx = r.x + r.width - trackW - 2, th = r.height;
    float thumbH = std::max(28.0f, th * r.height / contentH);
    if (barDragTarget_ == &scroll) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float denom = std::max(1.0f, th - thumbH);
            scroll = ((m.y - barDragGrab_) - r.y) / denom * maxS;
        } else barDragTarget_ = nullptr;
    }
    if (scroll < 0) scroll = 0; if (scroll > maxS) scroll = maxS;
    float thumbY = r.y + (th - thumbH) * (scroll / maxS);
    Rectangle thumb = { tx, thumbY, trackW, thumbH };
    DrawRectangleRounded({ tx, r.y, trackW, th }, 0.5f, 4, Fade(BLACK, 0.30f));
    bool hot = CheckCollisionPointRec(m, thumb) || barDragTarget_ == &scroll;
    DrawRectangleRounded(thumb, 0.5f, 4, hot ? ui::kAccentHi : ui::kAccent);
    if (CheckCollisionPointRec(m, thumb) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        barDragTarget_ = &scroll; barDragGrab_ = m.y - thumbY;
    }
}

// Open this picker, recording the trigger button rect (called when clicked).
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

// asset dropdown: 없음 + every registered image. Builds the full list only while
// open (closed buttons just show the current name → no per-frame allocation).
void Editor::assetButton(Rectangle r, const std::string& label, int& assetId, int id) {
    std::string cur = assetId < 0 ? "없음" : assetName(assetId);
    std::string txt = label.empty() ? cur : (label + ": " + cur);
    if (ui::button(r, txt, pickerId_ == id)) {
        if (pickerId_ == id) pickerId_ = -1;
        else { pickerId_ = id; pickerScroll_ = 0; }
    }
    if (pickerId_ == id) {
        std::vector<std::string> opts = { "없음" }; std::vector<int> vals = { -1 };
        for (const auto* a : engine_.project().assets.byType(AssetType::Image)) { opts.push_back(a->name); vals.push_back(a->id); }
        pickerAnchor_ = r; pickerOpts_ = std::move(opts); pickerValues_ = std::move(vals);
        pickerTarget_ = &assetId; pickerStrTarget_ = nullptr;
    }
}

// database-entity dropdown: pick item/mob/character/dialogue/scene by NAME, store
// its id (-1 = 없음). The button always shows the current name; the full list is
// built only when open.
void Editor::entityButton(Rectangle r, const std::string& label, int& id, int kind, int pickerId) {
    Database& db = engine_.project().database;
    auto nameOf = [&](int v) -> std::string {
        if (v < 0) return "없음";
        if (kind == ENT_Item)      { if (auto* it = db.item(v)) return it->name; }
        else if (kind == ENT_Mob)  { if (auto* m  = db.mob(v))  return m->name; }
        else if (kind == ENT_Character) { if (auto* c = db.character(v)) return c->name; }
        else if (kind == ENT_Dialogue)  { if (auto* d = db.dialogue(v)) return d->name; }
        else if (kind == ENT_Scene) { for (auto& s : db.scenes) if (s.id == v) return s.name; }
        return "#" + std::to_string(v);
    };
    std::string cur = nameOf(id);
    std::string txt = label.empty() ? cur : (label + ": " + cur);
    if (ui::button(r, txt, pickerId_ == pickerId)) {
        if (pickerId_ == pickerId) pickerId_ = -1;
        else { pickerId_ = pickerId; pickerScroll_ = 0; }
    }
    if (pickerId_ == pickerId) {
        std::vector<std::string> opts = { "없음" }; std::vector<int> vals = { -1 };
        auto add = [&](int vid, const std::string& nm) { opts.push_back(nm); vals.push_back(vid); };
        if (kind == ENT_Item)           for (auto& it : db.items)      add(it.id, it.name);
        else if (kind == ENT_Mob)       for (auto& m  : db.mobs)       add(m.id,  m.name);
        else if (kind == ENT_Character) for (auto& c  : db.characters) add(c.id,  c.name);
        else if (kind == ENT_Dialogue)  for (auto& d  : db.dialogues)  add(d.id,  d.name);
        else if (kind == ENT_Scene)     for (auto& s  : db.scenes)     add(s.id,  s.name);
        pickerAnchor_ = r; pickerOpts_ = std::move(opts); pickerValues_ = std::move(vals);
        pickerTarget_ = &id; pickerStrTarget_ = nullptr;
    }
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

} // namespace tsukuru
