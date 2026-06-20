#pragma once
// Editor: the in-engine game maker. Tabs for Map painting, Event placement,
// Asset registration (drag & drop files), and the Database editor.
#include <memory>
#include <string>
#include <vector>
#include "raylib.h"
#include "world/Map.h"
#include "database/Database.h"

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
    void pickAndImportImages();        // native OS file picker -> import selected images
    int  makeTransparentBg(int assetId); // remove a solid/white background -> new transparent asset
    void drawCharSkillEditor();        // per-character skill behaviour editor (range/power/effect/sound)
    void applyShape(FieldSkill& s, int shape, int size); // fill a skill pattern from a preset
    void drawAssetsTab();
    void drawDatabaseTab();
    void drawSkillsTab();
    void drawTilePalette(Rectangle area);
    void drawMapCanvas(Rectangle area);
    void handleAssetDrop();
    int  importImageFile(const std::string& path);   // GIF-aware image import -> asset id (-1 on fail)
    static bool isImageExt(const std::string& ext);  // any raylib-loadable image extension
    static bool isAudioExt(const std::string& ext);
    int  generateCharacter();          // make + register a new character sheet
    int  generateEffect(int style);    // make + register a skill-effect sheet
    int  generateSound(int style);     // make + register a skill sound effect
    std::vector<int> sliceAsset(int assetId, int n); // split an image into n frame assets
    std::vector<int> sliceSheetRow0(int assetId);    // slice the top row of a sheet into frames
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
    // Character builder (custom multi-motion characters)
    int  charDefSel_ = -1;            // selected custom character index
    int  charMotionTab_ = 0;          // selected motion tab (0..5)
    bool charDefNameFocus_ = false;
    int  charFrameSel_ = -1;          // selected frame within the current motion
    bool charSliceMode_ = false;      // library click adds N sliced frames instead of 1
    int  charSliceN_ = 4;             // number of columns to slice into
    MotionClip charClip_;             // motion clipboard (copy/paste)
    bool charClipSet_ = false;        // clipboard has content
    bool charLibFilter_ = false;      // library shows only character-frame images
    MotionClip charUndo_;             // 1-level undo snapshot of the active motion
    bool charUndoSet_ = false;        // undo snapshot available
    int  charLibScroll_ = 0;          // library grid scroll offset (px)
    bool charSkillEdit_ = false;      // per-character skill editor modal is open
    int  charListScroll_ = 0;         // character list scroll (left column)
    int  charFrameScroll_ = 0;        // frame timeline scroll (center column)
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
