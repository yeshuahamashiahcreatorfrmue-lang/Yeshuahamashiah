#pragma once
// Editor: the in-engine game maker. Tabs for Map painting, Event placement,
// Asset registration (drag & drop files), and the Database editor.
#include <memory>
#include <string>
#include "raylib.h"
#include "world/Map.h"

namespace tsukuru {

class Engine;

class Editor {
public:
    explicit Editor(Engine& engine);

    void update(float dt);
    void draw();

private:
    enum class Tab { World, Map, Events, Chars, Assets, Database };
    enum class Tool { Pencil, Erase, Fill };

    void drawToolbar();
    void drawWorldTab();
    void drawMapTab();
    void drawEventsTab();
    void drawCharsTab();
    void drawAssetsTab();
    void drawDatabaseTab();
    void drawTilePalette(Rectangle area);
    void drawMapCanvas(Rectangle area);
    void handleAssetDrop();
    int  generateCharacter();          // make + register a new character sheet
    std::shared_ptr<Map> activeMap();

    Engine& engine_;
    Tab  tab_ = Tab::Map;
    Tool tool_ = Tool::Pencil;
    int  activeLayer_ = 0;
    bool collisionMode_ = false;
    int  selectedTile_ = 0;
    int  activeMapId_ = -1;

    Camera2D cam_{};
    // Event editing
    int  editingEventId_ = -1;
    bool eventTextFocus_ = false;
    // Database editing
    int  dbCategory_ = 0;     // 0 items,1 equip,2 skills,3 actors,4 enemies
    int  dbSelected_ = -1;
    int  dbNameFocus_ = -1;
    // World / map management
    int  worldSelected_ = -1;       // map index selected in the World tab
    bool mapNameFocus_ = false;
    int  newMapW_ = 30, newMapH_ = 24;
    // Character generation
    int  charColor_ = 0;
    std::string status_;
    float statusTimer_ = 0;

    void setStatus(const std::string& s) { status_ = s; statusTimer_ = 3.0f; }
};

} // namespace tsukuru
