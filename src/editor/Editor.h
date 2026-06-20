#pragma once
// Editor: the in-engine game maker. Tabs for Map painting, Event placement,
// Asset registration (drag & drop files), and the Database editor.
#include <memory>
#include <string>
#include <vector>
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
    enum class Tab { World, Map, Events, Chars, Assets, Database, Skills };
    enum class Tool { Pencil, Erase, Fill, Rect, Stamp };

    void drawToolbar();
    void drawWorldTab();
    void drawMapTab();
    void drawEventsTab();
    void drawCharsTab();
    void drawAssetsTab();
    void drawDatabaseTab();
    void drawSkillsTab();
    void drawTilePalette(Rectangle area);
    void drawMapCanvas(Rectangle area);
    void handleAssetDrop();
    int  generateCharacter();          // make + register a new character sheet
    int  generateEffect(int style);    // make + register a skill-effect sheet
    int  generateSound(int style);     // make + register a skill sound effect
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
    int  newMapTier_ = 0;           // standardized size tier for a new map (0..6)
    // Character generation
    int  charColor_ = 0;
    // Skills tab (field-skill designer)
    int  skillSel_ = -1;
    bool skillNameFocus_ = false;
    int  skillPatSize_ = 3;            // range/radius used by shape presets
    // Undo/redo (snapshots of the active map's tilemap) + rectangle drag
    std::vector<std::string> undo_, redo_;
    int  undoMap_ = -1;             // which map id the undo stacks belong to
    bool rectDragging_ = false;
    int  rectStartX_ = 0, rectStartY_ = 0;
    int  prefabSel_ = 0;            // selected stamp/prefab index
    std::string status_;
    float statusTimer_ = 0;

    void setStatus(const std::string& s) { status_ = s; statusTimer_ = 3.0f; }
    void pushUndo();   // snapshot current map before an edit
    void doUndo();
    void doRedo();
    void stampPrefab(int ox, int oy); // place the selected prefab at a tile
    void drawPrefabPalette(Rectangle area);
};

} // namespace tsukuru
