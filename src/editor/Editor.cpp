#include "editor/Editor.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "render/AssetGen.h"
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace tsukuru {

static const float kToolbarH = 40;
static const float kPaletteW = 220;

Editor::Editor(Engine& engine) : engine_(engine) {
    cam_.zoom = 1.5f;
    cam_.offset = { kPaletteW + 20, kToolbarH + 20 };
    auto m = activeMap();
    if (m) activeMapId_ = m->id;
    if (const char* t = getenv("TSUKURU_TAB")) { // debug: pick initial tab
        std::string s = t;
        if (s == "world") tab_ = Tab::World;     else if (s == "events") tab_ = Tab::Events;
        else if (s == "chars") tab_ = Tab::Chars; else if (s == "assets") tab_ = Tab::Assets;
        else if (s == "db") tab_ = Tab::Database;
    }
}

std::shared_ptr<Map> Editor::activeMap() {
    Project& p = engine_.project();
    if (activeMapId_ >= 0) { if (auto m = p.map(activeMapId_)) return m; }
    if (!p.maps.empty()) { activeMapId_ = p.maps.front()->id; return p.maps.front(); }
    return nullptr;
}

// ============================ update ============================
void Editor::update(float dt) {
    if (statusTimer_ > 0) statusTimer_ -= dt;

    // Global shortcuts
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        engine_.project().save();
        setStatus("Project saved.");
    }
    if (IsKeyPressed(KEY_F5)) { engine_.project().save(); engine_.startPlaytest(); return; }

    // Camera pan (arrow keys) & zoom (wheel) when not typing
    bool typing = eventTextFocus_ || dbNameFocus_ >= 0;
    if (!typing) {
        float panSpeed = 400 * dt / cam_.zoom;
        if (IsKeyDown(KEY_RIGHT)) cam_.target.x += panSpeed;
        if (IsKeyDown(KEY_LEFT))  cam_.target.x -= panSpeed;
        if (IsKeyDown(KEY_DOWN))  cam_.target.y += panSpeed;
        if (IsKeyDown(KEY_UP))    cam_.target.y -= panSpeed;
    }
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && tab_ != Tab::Database) {
        cam_.zoom += wheel * 0.15f;
        if (cam_.zoom < 0.3f) cam_.zoom = 0.3f;
        if (cam_.zoom > 4.0f) cam_.zoom = 4.0f;
    }
    // Middle-drag pan
    if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 d = GetMouseDelta();
        cam_.target.x -= d.x / cam_.zoom;
        cam_.target.y -= d.y / cam_.zoom;
    }

    if (tab_ == Tab::Assets) handleAssetDrop();
}

void Editor::handleAssetDrop() {
    if (!IsFileDropped()) return;
    FilePathList dropped = LoadDroppedFiles();
    Project& p = engine_.project();
    for (unsigned i = 0; i < dropped.count; ++i) {
        std::string path = dropped.paths[i];
        std::string ext = GetFileExtension(path.c_str() ? path.c_str() : "");
        for (auto& c : ext) c = (char)tolower(c);
        AssetType type;
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif")
            type = AssetType::Image;
        else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
            type = AssetType::Audio;
        else continue;
        int id = p.assets.registerAsset(p.dir, path, type);
        if (id >= 0) setStatus("Registered: " + std::string(GetFileName(path.c_str())));
    }
    UnloadDroppedFiles(dropped);
    p.save();
}

// ============================ draw ============================
void Editor::draw() {
    switch (tab_) {
        case Tab::World:    drawWorldTab();    break;
        case Tab::Map:      drawMapTab();      break;
        case Tab::Events:   drawEventsTab();   break;
        case Tab::Chars:    drawCharsTab();    break;
        case Tab::Assets:   drawAssetsTab();   break;
        case Tab::Database: drawDatabaseTab(); break;
    }
    drawToolbar();

    if (statusTimer_ > 0) {
        int w = MeasureText(status_.c_str(), 16);
        DrawRectangle(GetScreenWidth() - w - 28, GetScreenHeight() - 34, w + 20, 26, ui::kAccent);
        DrawText(status_.c_str(), GetScreenWidth() - w - 18, GetScreenHeight() - 30, 16, BLACK);
    }
}

void Editor::drawToolbar() {
    int sw = GetScreenWidth();
    ui::panel({ 0, 0, (float)sw, kToolbarH }, ui::kPanelHi);

    float x = 8;
    auto tabBtn = [&](const char* name, Tab t) {
        if (ui::button({ x, 6, 78, 28 }, name, tab_ == t)) tab_ = t;
        x += 80;
    };
    tabBtn("World", Tab::World);
    tabBtn("Map", Tab::Map);
    tabBtn("Events", Tab::Events);
    tabBtn("Chars", Tab::Chars);
    tabBtn("Assets", Tab::Assets);
    tabBtn("Database", Tab::Database);

    x += 12;
    if (ui::button({ x, 6, 90, 28 }, "Save")) { engine_.project().save(); setStatus("Saved."); }
    x += 94;
    if (ui::button({ x, 6, 110, 28 }, "Play (F5)", false)) { engine_.project().save(); engine_.startPlaytest(); }

    // Map-specific tools on the right
    if (tab_ == Tab::Map) {
        float rx = sw - 8 - 4*64;
        if (ui::button({ rx, 6, 60, 28 }, "Pencil", tool_ == Tool::Pencil && !collisionMode_)) { tool_ = Tool::Pencil; collisionMode_ = false; }
        rx += 64;
        if (ui::button({ rx, 6, 60, 28 }, "Erase",  tool_ == Tool::Erase && !collisionMode_)) { tool_ = Tool::Erase; collisionMode_ = false; }
        rx += 64;
        if (ui::button({ rx, 6, 60, 28 }, "Fill",   tool_ == Tool::Fill && !collisionMode_)) { tool_ = Tool::Fill; collisionMode_ = false; }
        rx += 64;
        if (ui::button({ rx, 6, 60, 28 }, "Collide", collisionMode_)) collisionMode_ = !collisionMode_;
    }
}

// ----------------------------- MAP -----------------------------
void Editor::drawMapTab() {
    Rectangle paletteArea = { 0, kToolbarH, kPaletteW, (float)GetScreenHeight() - kToolbarH };
    Rectangle canvasArea  = { kPaletteW, kToolbarH, (float)GetScreenWidth() - kPaletteW,
                              (float)GetScreenHeight() - kToolbarH };
    drawMapCanvas(canvasArea);
    drawTilePalette(paletteArea);
}

void Editor::drawTilePalette(Rectangle area) {
    ui::panel(area, ui::kPanel);
    auto m = activeMap();
    if (!m) return;
    const Tileset& set = m->tileset;

    // Layer selector
    float ly = area.y + 8;
    ui::label("Layer", (int)area.x + 10, (int)ly, 16, ui::kTextDim);
    for (int i = 0; i < kLayerCount; ++i) {
        if (ui::button({ area.x + 10 + i*64, ly + 22, 60, 26 },
                       TextFormat("L%d", i+1), activeLayer_ == i))
            activeLayer_ = i;
    }
    float ty = ly + 60;

    // Tileset image asset selector
    ui::label("Tileset", (int)area.x + 10, (int)ty, 16, ui::kTextDim);
    auto imgs = engine_.project().assets.byType(AssetType::Image);
    if (ui::button({ area.x + 10, ty + 22, area.width - 20, 26 },
                   set.assetId >= 0 ? "Change tileset image" : "Pick tileset image")) {
        // cycle to next image asset
        if (!imgs.empty()) {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == set.assetId) idx = i;
            m->tileset.assetId = imgs[(idx + 1) % imgs.size()]->id;
        }
    }
    ty += 54;
    ui::intStepper({ area.x + 10, ty, area.width - 20, 24 }, "Cols", m->tileset.columns, 1, 1, 64); ty += 28;
    ui::intStepper({ area.x + 10, ty, area.width - 20, 24 }, "Rows", m->tileset.rows, 1, 1, 64); ty += 32;

    // Tile grid
    if (set.assetId < 0) {
        ui::label("Drop an image in", (int)area.x + 10, (int)ty, 14, ui::kTextDim);
        ui::label("the Assets tab.", (int)area.x + 10, (int)ty + 18, 14, ui::kTextDim);
        return;
    }
    const Texture2D& tex = engine_.assetTexture(set.assetId);
    float gridW = area.width - 20;
    float scale = gridW / std::max(1, tex.width);
    Rectangle dst = { area.x + 10, ty, gridW, tex.height * scale };
    DrawTexturePro(tex, { 0,0,(float)tex.width,(float)tex.height }, dst, {0,0}, 0, WHITE);

    // grid lines + selection
    float cellW = gridW / std::max(1, set.columns);
    float cellH = (set.tileHeight * scale);
    for (int c = 0; c <= set.columns; ++c)
        DrawLine((int)(dst.x + c*cellW), (int)dst.y, (int)(dst.x + c*cellW), (int)(dst.y + set.rows*cellH), Fade(BLACK,0.3f));
    for (int r = 0; r <= set.rows; ++r)
        DrawLine((int)dst.x, (int)(dst.y + r*cellH), (int)(dst.x + set.columns*cellW), (int)(dst.y + r*cellH), Fade(BLACK,0.3f));

    // selection highlight
    int selC = selectedTile_ % std::max(1, set.columns);
    int selR = selectedTile_ / std::max(1, set.columns);
    DrawRectangleLinesEx({ dst.x + selC*cellW, dst.y + selR*cellH, cellW, cellH }, 2, ui::kAccent);

    // click to select
    if (ui::mouseIn(dst) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mp = GetMousePosition();
        int c = (int)((mp.x - dst.x) / cellW);
        int r = (int)((mp.y - dst.y) / cellH);
        if (c >= 0 && c < set.columns && r >= 0 && r < set.rows)
            selectedTile_ = r * set.columns + c;
    }
}

void Editor::drawMapCanvas(Rectangle area) {
    auto m = activeMap();
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    if (!m) { EndScissorMode(); return; }
    int TS = m->tileset.tileWidth;

    BeginMode2D(cam_);
    const Texture2D& tex = engine_.assetTexture(m->tileset.assetId);
    const Tileset& set = m->tileset;
    int w = m->tilemap.width(), h = m->tilemap.height();

    // map background
    DrawRectangle(0, 0, w*TS, h*TS, Color{ 30, 33, 42, 255 });
    for (int layer = 0; layer < kLayerCount; ++layer) {
        // dim layers above the active one for clarity
        unsigned char a = (layer == activeLayer_ || !collisionMode_) ? 255 : 120;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                int t = m->tilemap.tile(layer, x, y);
                if (t < 0 || set.assetId < 0) continue;
                int sx, sy; set.srcOf(t, sx, sy);
                DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                               { (float)x*TS,(float)y*TS,(float)TS,(float)TS }, {0,0}, 0, Fade(WHITE, a/255.0f));
            }
    }
    // grid
    for (int x = 0; x <= w; ++x) DrawLine(x*TS, 0, x*TS, h*TS, Fade(BLACK, 0.25f));
    for (int y = 0; y <= h; ++y) DrawLine(0, y*TS, w*TS, y*TS, Fade(BLACK, 0.25f));
    // collision overlay
    if (collisionMode_)
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (m->tilemap.blocked(x, y))
                    DrawRectangle(x*TS, y*TS, TS, TS, Fade(ui::kDanger, 0.45f));
    EndMode2D();

    // painting
    if (ui::mouseIn(area) && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
        int tx = (int)(world.x / TS), ty = (int)(world.y / TS);
        if (world.x < 0) tx = -1;
        if (world.y < 0) ty = -1;
        if (m->tilemap.inBounds(tx, ty)) {
            // hover highlight
            BeginMode2D(cam_);
            DrawRectangleLinesEx({ (float)tx*TS,(float)ty*TS,(float)TS,(float)TS }, 2, ui::kAccentHi);
            EndMode2D();
            bool paint = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
            bool rclick = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
            if (collisionMode_) {
                if (paint)  m->tilemap.setBlocked(tx, ty, true);
                if (rclick) m->tilemap.setBlocked(tx, ty, false);
            } else if (tool_ == Tool::Fill) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m->tilemap.fill(activeLayer_, tx, ty, selectedTile_);
            } else {
                if (paint)  m->tilemap.setTile(activeLayer_, tx, ty,
                                               tool_ == Tool::Erase ? -1 : selectedTile_);
                if (rclick) m->tilemap.setTile(activeLayer_, tx, ty, -1);
            }
        }
    }
    EndScissorMode();
}

// ---------------------------- EVENTS ----------------------------
void Editor::drawEventsTab() {
    Rectangle canvasArea = { 0, kToolbarH, (float)GetScreenWidth() - 320, (float)GetScreenHeight() - kToolbarH };
    auto m = activeMap();
    BeginScissorMode((int)canvasArea.x, (int)canvasArea.y, (int)canvasArea.width, (int)canvasArea.height);
    DrawRectangleRec(canvasArea, Color{ 24, 26, 34, 255 });
    if (m) {
        int TS = m->tileset.tileWidth;
        BeginMode2D(cam_);
        const Texture2D& tex = engine_.assetTexture(m->tileset.assetId);
        const Tileset& set = m->tileset;
        int w = m->tilemap.width(), h = m->tilemap.height();
        for (int layer = 0; layer < kLayerCount; ++layer)
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    int t = m->tilemap.tile(layer, x, y);
                    if (t < 0 || set.assetId < 0) continue;
                    int sx, sy; set.srcOf(t, sx, sy);
                    DrawTexturePro(tex, { (float)sx,(float)sy,(float)set.tileWidth,(float)set.tileHeight },
                                   { (float)x*TS,(float)y*TS,(float)TS,(float)TS }, {0,0}, 0, WHITE);
                }
        for (int x = 0; x <= w; ++x) DrawLine(x*TS, 0, x*TS, h*TS, Fade(BLACK, 0.25f));
        for (int y = 0; y <= h; ++y) DrawLine(0, y*TS, w*TS, y*TS, Fade(BLACK, 0.25f));
        // event markers
        for (auto& e : m->events) {
            DrawRectangle(e.x*TS, e.y*TS, TS, TS, Fade(ui::kAccent, 0.5f));
            DrawRectangleLinesEx({ (float)e.x*TS,(float)e.y*TS,(float)TS,(float)TS }, 2,
                                 e.id == editingEventId_ ? ui::kAccentHi : ui::kAccent);
            DrawText(TextFormat("%d", e.id), e.x*TS+3, e.y*TS+2, 12, WHITE);
        }
        EndMode2D();

        // click to select/create event
        if (ui::mouseIn(canvasArea) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 world = GetScreenToWorld2D(GetMousePosition(), cam_);
            int tx = (int)(world.x / TS), ty = (int)(world.y / TS);
            if (m->tilemap.inBounds(tx, ty)) {
                Event* existing = m->eventAt(tx, ty);
                if (existing) editingEventId_ = existing->id;
                else {
                    Event ne; ne.id = m->nextEventId(); ne.x = tx; ne.y = ty;
                    ne.text = "Hello!";
                    m->events.push_back(ne);
                    editingEventId_ = ne.id;
                }
                eventTextFocus_ = false;
            }
        }
    }
    EndScissorMode();

    // ---- event inspector panel ----
    Rectangle panel = { (float)GetScreenWidth() - 320, kToolbarH, 320, (float)GetScreenHeight() - kToolbarH };
    ui::panel(panel, ui::kPanel);
    ui::label("EVENT", (int)panel.x + 12, (int)panel.y + 10, 22, ui::kAccent);
    Event* ev = nullptr;
    if (m) for (auto& e : m->events) if (e.id == editingEventId_) ev = &e;
    if (!ev) { ui::label("Click a tile to add", (int)panel.x + 12, (int)panel.y + 48, 16, ui::kTextDim);
               ui::label("or edit an event.", (int)panel.x + 12, (int)panel.y + 68, 16, ui::kTextDim);
               return; }

    float y = panel.y + 44;
    const char* typeNames[] = { "Message", "Teleport", "GiveItem", "SetSwitch", "Battle", "Shop" };
    if (ui::button({ panel.x + 12, y, 296, 26 }, TextFormat("Type: %s", typeNames[(int)ev->type]))) {
        ev->type = (EventType)(((int)ev->type + 1) % 6);
    }
    y += 32;
    const char* trigNames[] = { "Action", "Touch", "Autorun" };
    if (ui::button({ panel.x + 12, y, 296, 26 }, TextFormat("Trigger: %s", trigNames[(int)ev->trigger]))) {
        ev->trigger = (TriggerType)(((int)ev->trigger + 1) % 3);
    }
    y += 36;

    // text field (used by Message/GiveItem/SetSwitch/Shop)
    ui::label("Text:", (int)panel.x + 12, (int)y, 14, ui::kTextDim); y += 18;
    Rectangle tf = { panel.x + 12, y, 296, 26 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) eventTextFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) eventTextFocus_ = false;
    ui::textField(tf, ev->text, eventTextFocus_, 120);
    y += 34;

    // type-specific params
    switch (ev->type) {
        case EventType::Teleport:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "ToMap", ev->targetMap, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "X", ev->targetX, 1, 0, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "Y", ev->targetY, 1, 0, 999); y += 28;
            break;
        case EventType::GiveItem:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "ItemId", ev->itemId, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "Amount", ev->amount, 1, 1, 99); y += 28;
            break;
        case EventType::SetSwitch:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "SwitchId", ev->switchId, 1, 0, 999); y += 28;
            if (ui::button({ panel.x + 12, y, 296, 24 }, ev->switchValue ? "Value: ON" : "Value: OFF"))
                ev->switchValue = !ev->switchValue;
            y += 28;
            break;
        case EventType::StartBattle:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "EnemyId", ev->itemId, 1, -1, 999); y += 28;
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "Count", ev->amount, 1, 1, 6); y += 28;
            break;
        case EventType::Shop:
            ui::intStepper({ panel.x + 12, y, 296, 24 }, "ItemId", ev->itemId, 1, -1, 999); y += 28;
            break;
        default: break;
    }
    y += 6;
    // condition switch
    ui::intStepper({ panel.x + 12, y, 296, 24 }, "CondSwitch", ev->conditionSwitch, 1, -1, 999); y += 28;
    if (ui::button({ panel.x + 12, y, 144, 24 }, ev->once ? "Once: YES" : "Once: NO")) ev->once = !ev->once;
    // graphic asset cycle
    if (ui::button({ panel.x + 164, y, 144, 24 }, ev->graphicAsset >= 0 ? "Graphic: set" : "Graphic: none")) {
        auto imgs = engine_.project().assets.byType(AssetType::Image);
        if (imgs.empty()) ev->graphicAsset = -1;
        else {
            int idx = -1;
            for (int i = 0; i < (int)imgs.size(); ++i) if (imgs[i]->id == ev->graphicAsset) idx = i;
            idx++;
            ev->graphicAsset = (idx >= (int)imgs.size()) ? -1 : imgs[idx]->id;
        }
    }
    y += 32;
    if (ui::button({ panel.x + 12, y, 296, 28 }, "Delete Event", false)) {
        auto& evs = m->events;
        evs.erase(std::remove_if(evs.begin(), evs.end(),
                  [&](const Event& e){ return e.id == editingEventId_; }), evs.end());
        editingEventId_ = -1;
    }
}

// ---------------------------- ASSETS ----------------------------
void Editor::drawAssetsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    ui::label("Drag & drop .png / .wav / .ogg files anywhere to register them.",
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
            DrawText("♪ AUDIO", (int)x + 10, (int)y + 40, 18, ui::kAccent);
        }
        DrawText(a.name.c_str(), (int)x + 8, (int)(y + thumb + 12), 14, ui::kText);
        DrawText(TextFormat("id %d", a.id), (int)x + 8, (int)(y + thumb + 30), 12, ui::kTextDim);

        // image action buttons
        if (a.type == AssetType::Image) {
            if (ui::button({ x + thumb + 12, y + 8, 100, 24 }, "Tileset", false)) {
                if (auto m = activeMap()) { m->tileset.assetId = a.id; setStatus("Set tileset."); }
            }
            if (ui::button({ x + thumb + 12, y + 36, 100, 24 }, "Player", false)) {
                p.playerSprite = a.id; setStatus("Set player sprite.");
            }
        }
        x += cellW + pad;
        if (x + cellW > GetScreenWidth() - 20) { x = 20; y += thumb + 60 + pad; }
    }

    if (assets.empty())
        ui::label("(no assets yet)", 20, (int)kToolbarH + 56, 18, ui::kTextDim);
}

// --------------------------- DATABASE ---------------------------
void Editor::drawDatabaseTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Database& db = engine_.project().database;

    // category tabs
    const char* cats[] = { "Items", "Equipment", "Skills", "Actors", "Enemies" };
    for (int i = 0; i < 5; ++i)
        if (ui::button({ 12.0f + i*120, kToolbarH + 10, 112, 28 }, cats[i], dbCategory_ == i)) {
            dbCategory_ = i; dbSelected_ = -1; dbNameFocus_ = -1;
        }

    float listX = 12, listY = kToolbarH + 50, listW = 280;
    ui::panel({ listX, listY, listW, area.height - 60 }, ui::kPanel);

    // "Add" button
    if (ui::button({ listX + 8, listY + 8, listW - 16, 28 }, "+ Add New")) {
        switch (dbCategory_) {
            case 0: { Item it; it.id = (int)db.items.size()+1; db.items.push_back(it); dbSelected_=(int)db.items.size()-1; } break;
            case 1: { Equipment e; e.id = (int)db.equipment.size()+1; db.equipment.push_back(e); dbSelected_=(int)db.equipment.size()-1; } break;
            case 2: { Skill s; s.id = (int)db.skills.size()+1; db.skills.push_back(s); dbSelected_=(int)db.skills.size()-1; } break;
            case 3: { ActorDef a; a.id = (int)db.actors.size()+1; db.actors.push_back(a); dbSelected_=(int)db.actors.size()-1; } break;
            case 4: { EnemyDef en; en.id = (int)db.enemies.size()+1; db.enemies.push_back(en); dbSelected_=(int)db.enemies.size()-1; } break;
        }
    }

    // list
    float ly = listY + 44;
    auto listEntry = [&](int i, const std::string& name) {
        Rectangle r = { listX + 8, ly, listW - 16, 26 };
        if (ui::button(r, name, dbSelected_ == i)) { dbSelected_ = i; dbNameFocus_ = -1; }
        ly += 28;
    };
    int count = 0;
    switch (dbCategory_) {
        case 0: count=(int)db.items.size();     for (int i=0;i<count;++i) listEntry(i, db.items[i].name); break;
        case 1: count=(int)db.equipment.size(); for (int i=0;i<count;++i) listEntry(i, db.equipment[i].name); break;
        case 2: count=(int)db.skills.size();    for (int i=0;i<count;++i) listEntry(i, db.skills[i].name); break;
        case 3: count=(int)db.actors.size();    for (int i=0;i<count;++i) listEntry(i, db.actors[i].name); break;
        case 4: count=(int)db.enemies.size();   for (int i=0;i<count;++i) listEntry(i, db.enemies[i].name); break;
    }

    if (dbSelected_ < 0 || dbSelected_ >= count) return;

    // ---- detail editor ----
    float dx = listX + listW + 20, dy = listY + 8, dw = area.width - dx - 20;
    ui::panel({ dx - 8, listY, dw + 16, area.height - 60 }, ui::kPanel);

    auto nameField = [&](std::string& name) {
        ui::label("Name:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
        Rectangle tf = { dx, dy, std::min(360.0f, dw), 28 };
        bool focus = (dbNameFocus_ == dbSelected_);
        if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dbNameFocus_ = dbSelected_;
        else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf) && dbNameFocus_ == dbSelected_) dbNameFocus_ = -1;
        ui::textField(tf, name, focus, 32);
        dy += 38;
    };
    auto step = [&](const char* lbl, int& v, int s, int lo, int hi) {
        ui::intStepper({ dx, dy, std::min(300.0f, dw), 26 }, lbl, v, s, lo, hi); dy += 30;
    };

    switch (dbCategory_) {
        case 0: { Item& it = db.items[dbSelected_]; nameField(it.name);
            step("Price", it.price, 10, 0, 99999);
            step("Power", it.power, 5, 0, 9999);
            const char* effs[] = {"None","HealHP","HealMP","Damage"};
            if (ui::button({dx,dy,200,26}, TextFormat("Effect: %s", effs[(int)it.effect]))) it.effect=(ItemEffect)(((int)it.effect+1)%4);
            dy+=32;
            if (ui::button({dx,dy,200,26}, it.consumable?"Consumable: YES":"Consumable: NO")) it.consumable=!it.consumable;
            dy+=36;
            break; }
        case 1: { Equipment& e = db.equipment[dbSelected_]; nameField(e.name);
            if (ui::button({dx,dy,200,26}, e.slot==EquipSlot::Weapon?"Slot: Weapon":"Slot: Armor"))
                e.slot = e.slot==EquipSlot::Weapon?EquipSlot::Armor:EquipSlot::Weapon;
            dy+=32;
            step("Price", e.price, 10, 0, 99999);
            step("ATK+", e.atk, 1, 0, 999);
            step("DEF+", e.def, 1, 0, 999);
            break; }
        case 2: { Skill& s = db.skills[dbSelected_]; nameField(s.name);
            step("MP Cost", s.mpCost, 1, 0, 999);
            step("Power", s.power, 5, 0, 9999);
            if (ui::button({dx,dy,200,26}, s.healing?"Type: Heal":"Type: Damage")) s.healing=!s.healing;
            dy+=36;
            break; }
        case 3: { ActorDef& a = db.actors[dbSelected_]; nameField(a.name);
            step("Max HP", a.maxHp, 10, 1, 9999);
            step("Max MP", a.maxMp, 5, 0, 9999);
            step("ATK", a.atk, 1, 0, 999);
            step("DEF", a.def, 1, 0, 999);
            step("SPD", a.spd, 1, 0, 999);
            break; }
        case 4: { EnemyDef& e = db.enemies[dbSelected_]; nameField(e.name);
            step("Max HP", e.maxHp, 10, 1, 9999);
            step("ATK", e.atk, 1, 0, 999);
            step("DEF", e.def, 1, 0, 999);
            step("SPD", e.spd, 1, 0, 999);
            step("EXP", e.expReward, 5, 0, 99999);
            step("Gold", e.goldReward, 5, 0, 99999);
            break; }
    }
    DrawText(TextFormat("id: %d   (Ctrl+S to save project)",
             dbCategory_==0?db.items[dbSelected_].id:
             dbCategory_==1?db.equipment[dbSelected_].id:
             dbCategory_==2?db.skills[dbSelected_].id:
             dbCategory_==3?db.actors[dbSelected_].id:db.enemies[dbSelected_].id),
             (int)dx, (int)dy + 6, 14, ui::kTextDim);
}

// ---------------------------- WORLD (map management) ----------------------------
void Editor::drawWorldTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    // ---- left: map list ----
    float lx = 12, ly = kToolbarH + 12, lw = 300;
    ui::panel({ lx, ly, lw, area.height - 24 }, ui::kPanel);
    ui::label("MAPS", (int)lx + 12, (int)ly + 10, 22, ui::kAccent);
    DrawText(TextFormat("%d maps", (int)p.maps.size()), (int)lx + 120, (int)ly + 16, 16, ui::kTextDim);

    float y = ly + 46;
    for (int i = 0; i < (int)p.maps.size(); ++i) {
        auto& m = p.maps[i];
        bool isStart = (m->id == p.startMap);
        bool isActive = (m->id == activeMapId_);
        Rectangle r = { lx + 10, y, lw - 20, 28 };
        std::string label = (isStart ? "* " : "  ") + m->name + "  (#" + std::to_string(m->id) + ")";
        if (ui::button(r, label, worldSelected_ == i || isActive)) {
            worldSelected_ = i; activeMapId_ = m->id; mapNameFocus_ = false;
        }
        y += 32;
    }

    // new map controls
    y += 8;
    ui::intStepper({ lx + 10, y, (lw-30)/2, 26 }, "W", newMapW_, 5, 5, 200);
    ui::intStepper({ lx + 10 + (lw-30)/2 + 10, y, (lw-30)/2, 26 }, "H", newMapH_, 5, 5, 200);
    y += 34;
    if (ui::button({ lx + 10, y, lw - 20, 30 }, "+ New Map")) {
        auto nm = p.addMap("Map" + std::to_string(p.nextMapId()), newMapW_, newMapH_);
        // copy tileset from current map so it is paintable immediately
        if (auto cur = activeMap()) nm->tileset = cur->tileset;
        activeMapId_ = nm->id;
        worldSelected_ = (int)p.maps.size() - 1;
        p.save();
        setStatus("Created " + nm->name);
        tab_ = Tab::Map;
    }

    // ---- right: selected map details ----
    if (worldSelected_ < 0 || worldSelected_ >= (int)p.maps.size())
        worldSelected_ = activeMap() ? 0 : -1;
    if (worldSelected_ < 0) return;
    auto m = p.maps[worldSelected_];

    float dx = lx + lw + 24, dy = ly;
    ui::panel({ dx - 8, ly, area.width - dx - 4, area.height - 24 }, ui::kPanel);
    ui::label("MAP SETTINGS", (int)dx + 4, (int)dy + 10, 22, ui::kAccent);
    dy += 46;

    ui::label("Name:", (int)dx, (int)dy, 14, ui::kTextDim); dy += 18;
    Rectangle tf = { dx, dy, 360, 28 };
    if (ui::mouseIn(tf) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) mapNameFocus_ = true;
    else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !ui::mouseIn(tf)) mapNameFocus_ = false;
    ui::textField(tf, m->name, mapNameFocus_, 40);
    dy += 40;

    int w = m->tilemap.width(), h = m->tilemap.height();
    DrawText(TextFormat("Size: %d x %d tiles", w, h), (int)dx, (int)dy, 16, ui::kText); dy += 28;
    int nw = w, nh = h;
    ui::intStepper({ dx, dy, 220, 26 }, "Width", nw, 5, 5, 200); dy += 30;
    ui::intStepper({ dx, dy, 220, 26 }, "Height", nh, 5, 5, 200); dy += 32;
    if ((nw != w || nh != h)) m->tilemap.resizePreserve(nw, nh);

    ui::intStepper({ dx, dy, 260, 26 }, "Encounter%", m->encounterRate, 5, 0, 100); dy += 34;

    if (ui::button({ dx, dy, 220, 30 }, p.startMap == m->id ? "Start Map (current)" : "Set as Start Map",
                   p.startMap == m->id)) {
        p.startMap = m->id; p.startX = w/2; p.startY = h/2; setStatus("Start map set.");
    }
    dy += 38;
    if (ui::button({ dx, dy, 220, 30 }, "Edit this Map")) { activeMapId_ = m->id; tab_ = Tab::Map; }
    dy += 38;
    if ((int)p.maps.size() > 1) {
        if (ui::button({ dx, dy, 220, 30 }, "Delete Map", false)) {
            int delId = m->id;
            p.maps.erase(p.maps.begin() + worldSelected_);
            std::error_code ec;
            fs::remove(fs::path(p.dir) / "maps" / (std::to_string(delId) + ".json"), ec);
            if (p.startMap == delId) p.startMap = p.maps.front()->id;
            if (activeMapId_ == delId) activeMapId_ = p.maps.front()->id;
            worldSelected_ = 0;
            p.save();
            setStatus("Map deleted.");
            return;
        }
    }
    DrawText("Tip: connect maps with Teleport events (Events tab).",
             (int)dx, (int)(ly + area.height - 60), 14, ui::kTextDim);
}

// ---------------------------- CHARS (character assets) ----------------------------
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
    setStatus("Character created: " + dest.stem().string());
    return id;
}

void Editor::drawCharsTab() {
    Rectangle area = { 0, kToolbarH, (float)GetScreenWidth(), (float)GetScreenHeight() - kToolbarH };
    DrawRectangleRec(area, Color{ 24, 26, 34, 255 });
    Project& p = engine_.project();

    ui::label("CHARACTERS", 20, (int)kToolbarH + 14, 24, ui::kAccent);
    DrawText("Generate new characters in-engine, or drag a PNG sprite-sheet (4 frames x 4 dirs) onto the window.",
             20, (int)kToolbarH + 44, 16, ui::kTextDim);
    if (ui::button({ 20, kToolbarH + 70, 240, 32 }, "+ Generate New Character"))
        generateCharacter();
    handleAssetDrop(); // allow dropping character sheets here too

    // grid of image assets (characters), with the 'facing-down' frame preview
    auto imgs = p.assets.byType(AssetType::Image);
    float x = 20, y = kToolbarH + 118, cell = 150;
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
        DrawText(a->name.c_str(), (int)x + 8, (int)(y + cell - 36), 14, ui::kText);
        if (ui::button({ x + 8, y + cell - 16, cell - 16, 26 },
                       isPlayer ? "PLAYER" : "Set Player", isPlayer)) {
            p.playerSprite = a->id; p.save(); setStatus("Player character set.");
        }
        x += cell + 14;
        if (x + cell > area.width - 20) { x = 20; y += cell + 70; }
    }
    if (imgs.empty())
        ui::label("(no characters yet - click Generate)", 20, (int)kToolbarH + 120, 18, ui::kTextDim);
}

} // namespace tsukuru
