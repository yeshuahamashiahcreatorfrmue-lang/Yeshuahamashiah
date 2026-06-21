#pragma once
// Editor: the in-engine game maker. Tabs for Map painting, Event placement,
// Asset registration (drag & drop files), and the Database editor.
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "raylib.h"
#include "world/Map.h"
#include "database/Database.h"
#include "project/AssetManager.h"   // AssetType (asset-cycle helpers)
#include "render/Segmenter.h"

namespace tsukuru {

class Engine;

class Editor {
public:
    explicit Editor(Engine& engine);

    void update(float dt);
    void draw();

private:
    enum class Tab { World, WorldView, Map, Npc, Events, Chars, Assets, Database };
    enum class Tool { Pencil, Erase, Fill, Rect, Stamp };

    void drawToolbar();
    void drawWorldTab();
    void drawWorldViewTab();   // All-Map Viewer: lay maps on a zone grid for edge-to-edge travel
    void drawWorldPreviewOverlay();   // fullscreen map preview (click thumbnail to open, X/ESC to close)
    void drawMapZoomBar(Rectangle canvas);   // map-only zoom control (− / % / + / 전체보기)
    void drawMapScrollbars(Rectangle canvas);// draggable H/V scrollbars for large maps
    // map thumbnails (rendered to cached textures in update(), drawn on World/WorldView)
    RenderTexture2D makeMapThumb(Map& m, float maxW, float maxH); // render a map into a fit texture
    void buildMapThumb(Map& m);
    const RenderTexture2D* mapThumb(int mapId);
    void dropMapThumb(int mapId);   // unload+erase one cached thumbnail (on map removal)
    void clearMapThumbs();
    void ensureThumbsForTab();   // build any missing thumbnails for the current tab
    void drawMapTab();
    void drawEventsTab();
    void drawCharsTab();
    void pickAndImportImages();        // native OS file picker -> import selected images
    int  makeTransparentBg(int assetId); // remove a solid/white background -> new transparent asset
    void scrollbar(Rectangle region, int& scroll, float contentH); // wheel + middle-drag + draggable bar
    void drawCharSkillEditor();        // per-character skill behaviour editor (range/power/effect/sound)
    void drawCharDataEditor();         // bulk editor: all of a character's stats + every skill
    void applyShape(FieldSkill& s, int shape, int size, bool toEfx = false); // fill damage OR effect tiles from a preset
    // --- skill-editor widgets (used by the per-character skill editor) ---
    std::string assetName(int id) const;                 // asset display name, or "없음"
    void cycleAsset(int& cur, AssetType t);              // advance to the next asset id (wraps to -1)
    float drawSkillPatternGrid(FieldSkill& s, float gx, float gy, bool usesPattern); // -> grid bottom Y
    void drawSkillFxControls(FieldSkill& s, float dx, float& dy); // effect/sound assign + import + frames/loops
    void drawEffectFrameStrip(FieldSkill& s, float x, float y);   // per-frame select/replace/add/remove panel
    void pickAndImportEfxFrame();        // file picker -> replace one effect strip frame
    void efxReplaceFrame(int assetId, int frame, const std::string& src); // composite an image into one cell
    bool efxAddFrame(int assetId);       // append a blank cell to the strip
    bool efxRemoveFrame(int assetId, int frame);
    void drawAssetsTab();
    void drawDatabaseTab();
    void drawTilePalette(Rectangle area);
    void drawMapCanvas(Rectangle area);
    // Interactive tile-footprint picker: a maxN×maxN grid you click/drag to set how
    // many tiles (칸) the character/NPC occupies; the sprite is drawn filling the
    // chosen block as a live preview. Updates wTiles/hTiles in place.
    void drawFootprintGrid(Rectangle gridArea, int& wTiles, int& hTiles, int previewAsset, bool sheet4dir, int maxN = 4);
    // Full "차지 칸수" block (label + grid + W×H readout + 미세% stepper). Advances y.
    void drawFootprintControl(float x, float& y, float w, int& wTiles, int& hTiles, int& pct, int previewAsset, bool sheet4dir);
    void drawNpcInspector(Event& ev, Rectangle panel); // NPC data panel (sprite/진영/AI/stats)
    void drawEventInspector(Event& ev, Map& m, Rectangle panel); // full event editor (all object types) + delete
    void drawObjectPalette(Rectangle area);            // left panel: all placeable object types
    void newObjectAt(Map& m, int tx, int ty);          // create the selected object preset on a tile
    void drawNpcTab();                                 // NPC/몹/플레이어 통합 관리 탭
    void drawPlayerEditor(Rectangle panel);            // edit the player character's stats/footprint
    void drawNpcStatRows(Event& ev, float x, float& y, float w); // 진영/AI/크기/전투 rows (shared)
    int  stageImportImage(const std::string& src, const char* prefix); // copy ext image -> asset id, -1 fail
    void pickAndImportNpcChar();                     // file picker -> assign an NPC's character sprite
    void pickAndImportMapFile();                     // file picker -> load a map .json for preview/registration
    void handleAssetDrop();
    int  importImageFile(const std::string& path);   // GIF-aware image import -> asset id (-1 on fail)
    void pickAndImportEffect();                       // file picker -> assign a skill effect strip (multi = frames)
    void pickAndImportSound();                        // file picker -> assign a skill sound
    void pickAndImportBgm();                          // file picker -> import + assign a map's BGM
    void pickAndImportTileset();                      // file picker -> assign the active map's tileset (no crop)
    bool aiCutout(Image& img);                        // AI subject cut-out (true if the model handled it)
    void deleteAssets(const std::vector<int>& ids);  // unregister assets + scrub character motion refs
    int  duplicateAsset(int id);                      // copy an image asset to a new file+entry
    static bool isImageExt(const std::string& ext);  // any raylib-loadable image extension
    static bool isAudioExt(const std::string& ext);
    int  generateCharacter();          // make + register a new character sheet
    int  generateEffect(int style);    // make + register a skill-effect sheet
    int  generateSound(int style);     // make + register a skill sound effect
    std::vector<int> sliceAsset(int assetId, int n); // split an image into n frame assets
    std::vector<int> sliceSheetRow0(int assetId);    // slice the top row of a sheet into frames
    std::shared_ptr<Map> activeMap();

    Engine& engine_;
    Segmenter seg_;          // AI subject/background separation (loads u2netp.onnx if present)
    Tab  tab_ = Tab::World;   // open on the map list so all maps are visible at a glance
    Tool tool_ = Tool::Pencil;
    int  activeLayer_ = 0;
    bool collisionMode_ = false;
    bool objMode_ = false;           // Map tab: place/select/delete map objects (events) instead of painting
    int  objPlaceType_ = 0;          // selected object-type preset to drop
    bool npcTabPlayer_ = true;       // NPC tab: the player row is selected (default view)
    int  npcListScroll_ = 0;         // NPC tab: list scroll offset
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
    std::string mapSearch_;         // World-tab map-list filter text
    bool mapSearchFocus_ = false;
    float worldListScroll_ = 0;     // scroll offset for the (filtered) map list
    int  worldViewSel_ = -1;        // map selected in the All-Map Viewer
    Camera2D worldCam_{};           // pan/zoom for the zone grid
    bool worldCamInit_ = false;
    // All-Map Viewer: drag a placed map to a new cell (move) / onto another (swap)
    int  wvDragId_ = -1;            // map id being dragged (-1 = none)
    bool wvDragging_ = false;       // passed the click->drag threshold
    Vector2 wvDragStart_{};         // press point (screen) used to detect a drag
    // World tab: fullscreen map preview overlay
    bool worldPreviewFull_ = false;
    RenderTexture2D worldBigThumb_{}; // hi-res preview texture (built on demand)
    int  worldBigId_ = -1;            // map id the big preview was built for
    int  worldPreviewMapId_ = -1;     // map currently shown in the overlay (navigable)
    int  worldPrevSelEvent_ = -1;     // event whose info box is shown in the overlay
    std::vector<int> worldPreviewStack_; // back-stack for entering building interiors
    int  scrollDragAxis_ = 0;         // map-canvas scrollbar drag: 1=horizontal, 2=vertical
    std::unordered_map<int, RenderTexture2D> mapThumbs_;  // map id -> cached thumbnail
    Tab  prevTab_ = Tab::Map;       // detect tab changes to refresh thumbnails
    int  newMapW_ = 30, newMapH_ = 24;
    int  newMapTier_ = 0;           // standardized size tier for a new map (0..13)
    // Character generation
    int  charColor_ = 0;
    // Per-character skill editor (range pattern + effect/sound)
    bool skillNameFocus_ = false;
    int  skillPatSize_ = 3;            // range/radius used by shape presets
    bool editEfxLayer_ = false;        // grid edits the EFFECT tiles instead of damage tiles
    // Character builder (custom multi-motion characters)
    int  charDefSel_ = -1;            // selected custom character index
    int  charMotionTab_ = 0;          // selected motion tab (0..5)
    int  charSkillSlot_ = -1;         // skill slot being edited (0..5 Z/X/C/V/F/G; -1 = derive from motion)
    int  charDirTab_ = 0;             // edited direction: 0정면(아래)/1좌/2우/3위
    bool pendingImport_ = false;      // request the native file picker outside the draw frame
    bool pendingEffectImport_ = false;     // request the picker to assign a skill effect strip
    bool pendingSoundImport_  = false;     // request the picker to assign a skill sound
    bool pendingBgmImport_ = false;        // request the picker to import a map BGM
    int  pendingBgmMapId_ = -1;            // map awaiting an imported BGM
    bool pendingTilesetImport_ = false;    // request the picker to assign the map tileset
    bool pendingNpcCharImport_ = false;    // request the picker to assign an NPC sprite
    int  pendingNpcEventId_ = -1;          // event awaiting an imported NPC sprite
    bool pendingMapImport_ = false;        // request the picker to load a map .json
    int  efxFrameSel_ = 0;                  // selected effect frame (per-frame editor)
    bool pendingEfxFrameImport_ = false;    // request the picker to replace an effect frame
    int  pendingEfxAsset_ = -1, pendingEfxFrame_ = -1;
    std::shared_ptr<Map> mapPreview_;      // a loaded-but-not-yet-added map (World tab preview)
    std::string mapPreviewName_;           // source filename of the preview map
    FieldSkill* pendingEffectSkill_ = nullptr; // skill awaiting an imported effect/sound (valid 1 frame)
    int* scrollDragTarget_ = nullptr; // which scroll offset the dragged scrollbar thumb controls
    float scrollDragGrab_ = 0;        // grab offset within the thumb while dragging
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
    // Image-source panel: multi-select / rubber-band / right-click menu / rename
    std::vector<int> charLibSel_;     // selected asset ids in the 이미지 소스 grid
    std::vector<int> charLibSelBase_; // selection snapshot at rubber-band start (for live box)
    int   charLibAnchor_ = -1;        // visible-index anchor for shift-range select
    bool  charLibDragMaybe_ = false;  // left pressed in grid; may turn into a rubber-band
    bool  charLibDragging_ = false;   // rubber-band box select is active
    bool  charLibPressOnCard_ = false;// the press landed on a card (suppresses rubber-band)
    Vector2 charLibDragStart_{};      // rubber-band anchor point (screen)
    bool  charLibMenuOpen_ = false;   // right-click context menu is open
    Vector2 charLibMenuPos_{};        // context menu top-left (screen)
    int   charLibRenameId_ = -1;      // asset id being renamed (-1 = none)
    bool  charLibRenameFocus_ = false;
    std::string charLibRenameBuf_;    // edit buffer for rename overlay
    bool charSkillEdit_ = false;      // per-character skill editor modal is open
    bool charDataEdit_ = false;       // character data (stats + all skills) editor modal is open
    int  charDataNameFocus_ = -1;     // -1 none / 0 char name / 1..4 skill name for slot 0..3
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
