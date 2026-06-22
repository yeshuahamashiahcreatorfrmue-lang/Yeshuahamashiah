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
    if (scroll < 0) scroll = 0;
    if (scroll > maxS) scroll = maxS;
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
// Selection is applied later (in drawPickerOverlay) via the deferred pickerApply_
// callback, so different value/target types share one overlay with no casts.
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
        pickerAnchor_ = r; pickerOpts_ = opts; pickerCurIdx_ = cur;
        pickerApply_ = [&target, values](int i){ target = values.empty() ? i : values[i]; };
    }
}

// string-valued dropdown (e.g. sound-effect name)
void Editor::optionButtonStr(Rectangle r, const std::string& label,
                             const std::vector<std::string>& opts, const std::vector<std::string>& values,
                             std::string& target, int id) {
    int cur = -1;
    for (int i = 0; i < (int)values.size(); ++i) if (values[i] == target) cur = i;
    std::string disp = (cur >= 0 && cur < (int)opts.size()) ? opts[cur] : std::string();
    std::string txt = label.empty() ? disp : (label + ": " + disp);
    if (ui::button(r, txt, pickerId_ == id)) {
        if (pickerId_ == id) pickerId_ = -1;
        else { pickerId_ = id; pickerScroll_ = 0; }
    }
    if (pickerId_ == id) {
        pickerAnchor_ = r; pickerOpts_ = opts; pickerCurIdx_ = cur;
        pickerApply_ = [&target, values](int i){ if (i < (int)values.size()) target = values[i]; };
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
        int cur2 = 0; for (int i = 0; i < (int)vals.size(); ++i) if (vals[i] == assetId) cur2 = i;
        pickerAnchor_ = r; pickerOpts_ = std::move(opts); pickerCurIdx_ = cur2;
        pickerApply_ = [&assetId, vals](int i){ assetId = (i < (int)vals.size()) ? vals[i] : -1; };
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
        int cur = 0; for (int i = 0; i < (int)vals.size(); ++i) if (vals[i] == id) cur = i;
        pickerAnchor_ = r; pickerOpts_ = std::move(opts); pickerCurIdx_ = cur;
        pickerApply_ = [&id, vals](int i){ id = (i < (int)vals.size()) ? vals[i] : -1; };
    }
}

// Drawn ON TOP, after the active tab, so the expanded list overlays everything.
void Editor::drawPickerOverlay() {
    if (pickerId_ < 0 || !pickerApply_ || pickerOpts_.empty()) return;
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
    if (pickerScroll_ < 0) pickerScroll_ = 0;
    if (pickerScroll_ > maxScroll) pickerScroll_ = maxScroll;
    BeginScissorMode((int)(box.x * uiScale()), (int)(box.y * uiScale()), (int)(box.width * uiScale()), (int)(box.height * uiScale()));
    float oy = y - pickerScroll_;
    for (int i = 0; i < n; ++i) {
        bool sel = (i == pickerCurIdx_);
        if (oy + rowH > y - rowH && oy < y + listH) {
            if (ui::button({ x, oy, w, rowH - 2 }, pickerOpts_[i], sel)) {
                pickerApply_(i);
                pickerId_ = -1; pickerApply_ = nullptr;
                engine_.project().save();
                EndScissorMode(); return;
            }
        }
        oy += rowH;
    }
    EndScissorMode();
    // click outside the list (and not on the anchor) closes it
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !ui::mouseIn(box) && !ui::mouseIn(pickerAnchor_)) { pickerId_ = -1; pickerApply_ = nullptr; }
}

// Open the Explorer-like character/image browser. `apply` gets the chosen asset id.
void Editor::openCharBrowser(std::function<void(int)> apply) {
    charBrowserOpen_ = true; charBrowserApply_ = std::move(apply);
    charBrowserSearch_.clear(); charBrowserScroll_ = 0; pickerId_ = -1;
}

// A registered character's representative sprite asset (걷기 아래 첫 프레임), or any
// motion frame, else -1. Used for thumbnails and as the NPC graphic.
int Editor::charThumbAsset(const CharacterDef& c) {
    if (!c.motions[MO_Walk].frames.empty()) return c.motions[MO_Walk].frames.front();
    for (const auto& mo : c.motions) {
        if (!mo.frames.empty()) return mo.frames.front();
        for (const auto* v : { &mo.left, &mo.right, &mo.up }) if (!v->empty()) return v->front();
    }
    return -1;
}

// Assign a registered character (상하좌우 각각 등록) to an NPC event: store its id
// (charId, used for directional runtime rendering) plus a thumbnail/footprint in the
// event so markers/preview keep working. Replaces the old "single sheet asset" NPC.
void Editor::applyCharToNpc(int mapId, int eventId, int charId) {
    auto m = engine_.project().map(mapId);
    if (!m) return;
    const CharacterDef* c = engine_.project().database.character(charId);
    for (auto& e : m->events) if (e.id == eventId) {
        e.charId = charId;
        e.graphicAsset = c ? charThumbAsset(*c) : -1;
        if (c) { e.drawTilesW = std::max(1, c->drawTilesW); e.drawTilesH = std::max(1, c->drawTilesH); e.drawPct = c->drawPct; }
    }
    engine_.project().save();
}

// An event's best "real game" sprite asset: the registered character's front
// (걷기 아래) frame if it has a charId, else its stored graphicAsset (-1 = none).
int Editor::eventSpriteAsset(const Event& e) {
    if (e.charId >= 0) {
        const CharacterDef* c = engine_.project().database.character(e.charId);
        if (c) { int a = charThumbAsset(*c); if (a >= 0) return a; }
    }
    return e.graphicAsset;
}

// Draw a sprite asset centred at `c`, fit within a `sz`×`sz` box. 4-direction
// sheets (width ≥ 2×height) show their first (정면/아래) column so map markers
// look like the in-game character rather than a stretched strip.
void Editor::drawSpriteCentered(int assetId, Vector2 c, float sz, Color tint) {
    if (assetId < 0) return;
    const Texture2D& tex = engine_.assetTexture(assetId);
    if (!tex.id) return;
    float fw = tex.width >= tex.height * 2 ? tex.width / 4.0f : (float)tex.width;
    float s = std::min(sz / fw, sz / (float)tex.height);
    float dw = fw * s, dh = tex.height * s;
    DrawTexturePro(tex, { 0, 0, fw, (float)tex.height },
                   { c.x - dw / 2, c.y - dh / 2, dw, dh }, { 0, 0 }, 0, tint);
}

// Draw EVERY map element (events by type + mob spawns) the way the World preview
// shows them, but as REAL in-game sprites: NPCs/objects render their character
// image (정면 칸) and mob spawns render the mob's sprite; graphic-less events
// (텔레포트/아이템/상점 등) keep a small labelled dot. Read-only (selection/editing
// is handled by the caller). bx/by/pw/ph = the on-screen rect of the map thumbnail.
void Editor::drawMapElementMarkers(Map& m, float bx, float by, float pw, float ph, bool includeNpc) {
    Project& p = engine_.project();
    int mwT = m.tilemap.width(), mhT = m.tilemap.height();
    if (mwT <= 0 || mhT <= 0) return;
    auto t2s = [&](int tx, int ty){ return Vector2{ bx + (tx+0.5f)/mwT*pw, by + (ty+0.5f)/mhT*ph }; };
    Vector2 mouse = GetMousePosition();
    float sprSz = std::max(14.0f, std::min(pw/mwT, ph/mhT) * 1.4f);  // ~1.4 tiles
    const char* tn[] = { "메시지","이동","아이템","스위치","전투","상점","퀘스트","엔딩","회복" };
    // events: NPC/object (real sprite) vs graphic-less event (teleport/item/shop/…)
    for (auto& e : m.events) {
        bool isNpc = (e.graphicAsset >= 0 || e.charId >= 0);
        if (isNpc && !includeNpc) continue;   // NPCs drawn as sprites by the caller
        Vector2 sp = t2s(e.x, e.y);
        bool entrance = (e.type == EventType::Teleport && e.targetMap >= 0);
        int spr = isNpc ? eventSpriteAsset(e) : -1;
        if (spr >= 0) {                        // real game-style image
            drawSpriteCentered(spr, sp, sprSz);
        } else {                               // no image → labelled dot
            Color col = entrance ? ui::kGood
                      : e.type == EventType::StartBattle ? ui::kDanger
                      : isNpc ? ui::factionColor((int)e.faction)
                              : Color{ 240, 210, 80, 255 };
            DrawCircleV(sp, 4.5f, Fade(BLACK, 0.6f));
            DrawCircleV(sp, 3.5f, col);
            if (entrance) DrawRectangleLinesEx({ sp.x-5, sp.y-5, 10, 10 }, 1, WHITE);
        }
        if (CheckCollisionPointCircle(mouse, sp, sprSz/2)) {
            std::string lbl = isNpc
                ? (!e.speakerName.empty() ? e.speakerName
                   : e.faction==NpcFaction::Enemy?"몹/적":e.faction==NpcFaction::Ally?"NPC아군":"NPC")
                : ((int)e.type>=0 && (int)e.type<9 ? tn[(int)e.type] : "?");
            DrawTextU(lbl.c_str(), (int)sp.x+8, (int)sp.y-8, 12, WHITE);
        }
    }
    // mob spawn points → the mob's real sprite (fallback to a purple dot)
    for (auto& s : m.mobSpawns) {
        Vector2 sp = t2s(s.x, s.y);
        const CharacterDef* md = p.database.mob(s.mobId);
        int spr = md ? charThumbAsset(*md) : -1;
        if (spr >= 0) drawSpriteCentered(spr, sp, sprSz);
        else { DrawCircleV(sp, 5, Fade(BLACK, 0.6f)); DrawCircleV(sp, 4, Color{ 180, 90, 220, 255 }); DrawTextU("M", (int)sp.x-3, (int)sp.y-6, 11, WHITE); }
        if (CheckCollisionPointCircle(mouse, sp, sprSz/2))
            DrawTextU(md?md->name.c_str():"몹", (int)sp.x+8, (int)sp.y-8, 12, WHITE);
    }
}

// Remove a registered character cleanly: scrub the player reference, fix the
// editor selection. (Characters are separate from raw assets and from db.mobs.)
void Editor::deleteCharacterDef(int idx) {
    Database& db = engine_.project().database;
    if (idx < 0 || idx >= (int)db.characters.size()) return;
    int id = db.characters[idx].id;
    if (engine_.project().playerCharId == id) engine_.project().playerCharId = -1;
    db.characters.erase(db.characters.begin() + idx);
    if (charDefSel_ >= (int)db.characters.size()) charDefSel_ = (int)db.characters.size() - 1;
    engine_.project().save();
    setStatus("캐릭터 삭제됨 (참조 정리 완료)");
}

// Full-screen modal: a searchable thumbnail grid of every REGISTERED CHARACTER
// (db.characters — separate from raw image assets; each carries motions/effects).
// Left-click = 선택, 우클릭 = 삭제, plus 없음 and '캐릭터 탭에서 제작'.
void Editor::drawCharBrowser() {
    Database& db = engine_.project().database;
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.72f));
    Rectangle box = { 40, 40, (float)sw - 80, (float)sh - 80 };
    ui::panel(box, ui::kPanel);
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    DrawTextU("등록된 캐릭터에서 선택 (검색·클릭=선택 · 우클릭=삭제)", (int)box.x + 16, (int)box.y + 12, 20, ui::kAccent);
    if (ui::button({ box.x + box.width - 96, box.y + 10, 84, 30 }, "닫기") || IsKeyPressed(KEY_ESCAPE)) {
        charBrowserOpen_ = false; charBrowserApply_ = nullptr; return;
    }
    float x = box.x + 16, top = box.y + 50;
    searchBox({ x, top, 320, 28 }, charBrowserSearch_, 9400);
    if (ui::button({ x + 332, top, 96, 28 }, "없음")) {
        if (charBrowserApply_) charBrowserApply_(-1);
        charBrowserOpen_ = false; charBrowserApply_ = nullptr; return;
    }
    if (ui::button({ x + 436, top, 180, 28 }, "캐릭터 탭에서 제작")) {
        charBrowserOpen_ = false; charBrowserApply_ = nullptr; tab_ = Tab::Chars; return;
    }

    // grid of registered-character thumbnails (filtered by name)
    float gridTop = top + 40;
    Rectangle grid = { box.x + 8, gridTop, box.width - 16, box.y + box.height - gridTop - 12 };
    const float cell = 112, thumb = 92, pad = 10;
    int cols = std::max(1, (int)((grid.width - pad) / (cell + pad)));
    std::vector<int> shown;   // indices into db.characters that match the search
    for (int i = 0; i < (int)db.characters.size(); ++i)
        if (nameMatch(db.characters[i].name, charBrowserSearch_)) shown.push_back(i);
    int rows = ((int)shown.size() + cols - 1) / cols;
    float contentH = rows * (cell + pad) + pad;

    uiScissor((int)grid.x, (int)grid.y, (int)grid.width, (int)grid.height);
    Vector2 m = GetMousePosition();
    int chosen = -2;          // -2 = none picked this frame
    int toDelete = -1;        // index to delete after the loop
    for (int k = 0; k < (int)shown.size(); ++k) {
        const CharacterDef& cdc = db.characters[shown[k]];
        int r = k / cols, c = k % cols;
        float cx = grid.x + pad + c * (cell + pad);
        float cy = grid.y + pad + r * (cell + pad) - charBrowserScroll_;
        if (cy + cell < grid.y || cy > grid.y + grid.height) continue;   // cull
        Rectangle cellR = { cx, cy, cell, cell };
        bool hot = CheckCollisionPointRec(m, cellR);
        DrawRectangleRec(cellR, hot ? ui::kPanelHi : Color{ 30, 33, 42, 255 });
        DrawRectangleLinesEx(cellR, hot ? 2 : 1, hot ? ui::kAccent : Fade(BLACK, 0.5f));
        int aid = charThumbAsset(cdc);
        if (aid >= 0) {
            const Texture2D& tex = engine_.assetTexture(aid);
            if (tex.id) {
                float fw = tex.width >= tex.height * 2 ? tex.width / 4.0f : (float)tex.width;  // 4방향 시트→첫 칸
                float s = std::min(thumb / fw, thumb / (float)tex.height);
                float dw = fw * s, dh = tex.height * s;
                DrawTexturePro(tex, { 0, 0, fw, (float)tex.height },
                               { cx + (cell - dw) / 2, cy + 6 + (thumb - dh) / 2, dw, dh }, { 0, 0 }, 0, WHITE);
            }
        } else {
            DrawTextU("(이미지 없음)", (int)cx + 14, (int)cy + 42, 12, ui::kTextDim);
        }
        std::string nm = cdc.name;
        if ((int)nm.size() > 14) nm = nm.substr(0, 13) + "..";
        int tw = MeasureTextU(nm.c_str(), 12);
        DrawTextU(nm.c_str(), (int)(cx + (cell - tw) / 2), (int)(cy + cell - 16), 12, ui::kText);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))  chosen = cdc.id;
        if (hot && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) toDelete = shown[k];
    }
    EndScissorMode();
    scrollbar(grid, charBrowserScroll_, contentH);
    if (db.characters.empty())
        DrawTextU("등록된 캐릭터가 없습니다. '캐릭터 탭에서 제작'으로 만드세요.",
                  (int)grid.x + 12, (int)grid.y + 12, 14, ui::kTextDim);

    if (toDelete >= 0) { deleteCharacterDef(toDelete); return; }   // db changed; bail this frame
    if (chosen != -2) {
        if (charBrowserApply_) charBrowserApply_(chosen);
        charBrowserOpen_ = false; charBrowserApply_ = nullptr;
        engine_.project().save();
    }
}

// Open the Explorer-like 이펙트(이미지) browser. `apply` gets the chosen image asset id.
void Editor::openFxBrowser(std::function<void(int)> apply) {
    fxBrowserOpen_ = true; fxBrowserApply_ = std::move(apply);
    fxBrowserSearch_.clear(); fxBrowserScroll_ = 0; pickerId_ = -1;
}

// Full-screen modal: a searchable thumbnail grid of every registered IMAGE asset
// (usable as an effect), plus '없음' and '외부에서 추가(파일)'. Left-click = 선택.
void Editor::drawFxBrowser() {
    Project& p = engine_.project();
    int sw = screenW(), sh = screenH();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.72f));
    Rectangle box = { 40, 40, (float)sw - 80, (float)sh - 80 };
    ui::panel(box, ui::kPanel);
    DrawRectangleLinesEx(box, 2, ui::kAccent);
    DrawTextU("이펙트 선택 — 등록 이미지에서 고르거나 외부 파일에서 추가 (검색·클릭=선택)",
              (int)box.x + 16, (int)box.y + 12, 20, ui::kAccent);
    if (ui::button({ box.x + box.width - 96, box.y + 10, 84, 30 }, "닫기") || IsKeyPressed(KEY_ESCAPE)) {
        fxBrowserOpen_ = false; fxBrowserApply_ = nullptr; return;
    }
    float x = box.x + 16, top = box.y + 50;
    searchBox({ x, top, 320, 28 }, fxBrowserSearch_, 9410);
    if (ui::button({ x + 332, top, 96, 28 }, "없음")) {
        if (fxBrowserApply_) fxBrowserApply_(-1);
        fxBrowserOpen_ = false; fxBrowserApply_ = nullptr; return;
    }
    if (ui::button({ x + 436, top, 200, 28 }, "외부에서 추가(파일)")) {
        pendingFxImport_ = true;   // 네이티브 파일 다이얼로그는 드로우 프레임 밖(update)에서 연다
        return;
    }

    float gridTop = top + 40;
    Rectangle grid = { box.x + 8, gridTop, box.width - 16, box.y + box.height - gridTop - 12 };
    const float cell = 110, thumb = 92, pad = 10;
    int cols = std::max(1, (int)((grid.width - pad) / (cell + pad)));
    auto allFx = p.assets.byType(AssetType::Image);
    decltype(allFx) shown;
    for (auto* a : allFx)
        if (nameMatch(a->name, fxBrowserSearch_)) shown.push_back(a);
    int rows = ((int)shown.size() + cols - 1) / cols;
    float contentH = rows * (cell + pad) + pad;

    uiScissor((int)grid.x, (int)grid.y, (int)grid.width, (int)grid.height);
    Vector2 m = GetMousePosition();
    int chosen = -2;
    for (int k = 0; k < (int)shown.size(); ++k) {
        auto* a = shown[k];
        int r = k / cols, c = k % cols;
        float cx = grid.x + pad + c * (cell + pad);
        float cy = grid.y + pad + r * (cell + pad) - fxBrowserScroll_;
        if (cy + cell < grid.y || cy > grid.y + grid.height) continue;
        Rectangle cellR = { cx, cy, cell, cell };
        bool hot = CheckCollisionPointRec(m, cellR);
        DrawRectangleRec(cellR, hot ? ui::kPanelHi : Color{ 30, 33, 42, 255 });
        DrawRectangleLinesEx(cellR, hot ? 2 : 1, hot ? ui::kAccent : Fade(BLACK, 0.5f));
        const Texture2D& tex = engine_.assetTexture(a->id);
        if (tex.id) {
            // effect images are often horizontal strips → show the first frame square
            float fw = tex.width >= tex.height * 2 ? (float)tex.height : (float)tex.width;
            float s = std::min(thumb / fw, thumb / (float)tex.height);
            float dw = fw * s, dh = tex.height * s;
            DrawTexturePro(tex, { 0, 0, fw, (float)tex.height },
                           { cx + (cell - dw) / 2, cy + 6 + (thumb - dh) / 2, dw, dh }, { 0, 0 }, 0, WHITE);
        } else DrawTextU("(이미지)", (int)cx + 20, (int)cy + 42, 12, ui::kTextDim);
        std::string nm = a->name; if ((int)nm.size() > 14) nm = nm.substr(0, 13) + "..";
        int tw = MeasureTextU(nm.c_str(), 12);
        DrawTextU(nm.c_str(), (int)(cx + (cell - tw) / 2), (int)(cy + cell - 16), 12, ui::kText);
        if (hot && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) chosen = a->id;
    }
    EndScissorMode();
    scrollbar(grid, fxBrowserScroll_, contentH);
    if (shown.empty())
        DrawTextU("등록된 이미지가 없습니다. '외부에서 추가(파일)'로 가져오세요.",
                  (int)grid.x + 12, (int)grid.y + 12, 14, ui::kTextDim);

    if (chosen != -2) {
        if (fxBrowserApply_) fxBrowserApply_(chosen);
        fxBrowserOpen_ = false; fxBrowserApply_ = nullptr;
    }
}

} // namespace tsukuru
