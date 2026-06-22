#pragma once
// Editor: the in-engine game maker. Tabs for Map painting, Event placement,
// Asset registration (drag & drop files), and the Database editor.
#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include "raylib.h"
#include "world/Map.h"
#include "database/Database.h"
#include "project/AssetManager.h"   // AssetType (asset-cycle helpers)
#include "render/Segmenter.h"
#include "render/UI.h"               // ui::button (used by inline optionButtonEnum<>)

namespace tsukuru {

class Engine;

class Editor {
public:
    explicit Editor(Engine& engine);

    void update(float dt);
    void draw();
    // The All-Map Viewer uses Ctrl+wheel for its own zoom, so the global UI-scale
    // Ctrl+wheel handler must yield while that tab is open.
    bool wantsCtrlWheel() const;

    // Dropdown picker: a button shows the current choice; clicking it expands the
    // FULL list of options below, and you pick one (replaces click-to-cycle).
    // Rendered on top after the tab via drawPickerOverlay(); writes are deferred
    // to the stable `target` int the same frame the option list is shown.
    void optionButton(Rectangle r, const std::string& label,
                      const std::vector<std::string>& opts, const std::vector<int>& values,
                      int& target, int id);
    // enum-typed convenience: identity option indices map to the enum value. Avoids
    // reinterpret_cast<int&> on an enum (which violates strict aliasing).
    template <class E>
    void optionButtonEnum(Rectangle r, const std::string& label,
                          const std::vector<std::string>& opts, E& target, int id) {
        int cur = (int)target;
        std::string disp = (cur >= 0 && cur < (int)opts.size()) ? opts[cur] : std::string();
        if (ui::button(r, label.empty() ? disp : (label + ": " + disp), pickerId_ == id)) {
            if (pickerId_ == id) pickerId_ = -1; else { pickerId_ = id; pickerScroll_ = 0; }
        }
        if (pickerId_ == id) {
            pickerAnchor_ = r; pickerOpts_ = opts; pickerCurIdx_ = cur;
            pickerApply_ = [&target](int i){ target = (E)i; };
        }
    }
    void assetButton(Rectangle r, const std::string& label, int& assetId, int id);
    void optionButtonStr(Rectangle r, const std::string& label,
                         const std::vector<std::string>& opts, const std::vector<std::string>& values,
                         std::string& target, int id);
    // pick a database entity (item/mob/character/dialogue/scene) by NAME, store its id.
    enum EntityKind { ENT_Item, ENT_Mob, ENT_Character, ENT_Dialogue, ENT_Scene };
    void entityButton(Rectangle r, const std::string& label, int& id, int kind, int pickerId);
    void drawPickerOverlay();
    bool pickerOpen() const { return pickerId_ >= 0; }
    // Explorer-like visual browser: a thumbnail grid of every registered character
    // image (+ "없음" + "외부에서 추가") with a search box. `apply` receives the chosen
    // image asset id (-1 = none). Used when assigning an NPC's character graphic.
    void openCharBrowser(std::function<void(int)> apply);
    void drawCharBrowser();
    bool charBrowserOpen() const { return charBrowserOpen_; }
    int  charThumbAsset(const CharacterDef& c);   // a character's representative sprite asset
    void deleteCharacterDef(int idx);             // remove a registered character + scrub refs
    void applyCharToNpc(int mapId, int eventId, int charId); // NPC에 등록 캐릭터(상하좌우) 적용
    void drawMapElementMarkers(Map& m, float bx, float by, float pw, float ph, bool includeNpc = true); // 미리보기처럼 모든 요소 표시

private:
    enum class Tab { World, WorldView, Map, Npc, Events, Chars, Mob, Dialogue, Scenario, Assets, Database };
    enum class Tool { Pencil, Erase, Fill, Rect, Stamp };

    void drawToolbar();
    void drawWorldTab();
    void drawWorldViewTab();   // All-Map Viewer: lay maps on a zone grid for edge-to-edge travel
    void drawWorldPreviewOverlay();   // fullscreen map preview (click thumbnail to open, X/ESC to close)
    void drawWorldPreviewPanel(Map& m, Rectangle area); // right-side add/delete editor in the preview
    void drawStartSettingsOverlay();  // 새 게임 시작 골드/아이템 편집 모달
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
    void drawDialogueTab();   // 대화로그 시나리오 편집
    void drawScenarioTab();   // 스토리 시나리오 시퀀서 편집
    void drawCharsTab();
    void pickAndImportImages();        // native OS file picker -> import selected images
    int  makeTransparentBg(int assetId); // remove a solid/white background -> new transparent asset
    void scrollbar(Rectangle region, int& scroll, float contentH); // wheel + middle-autoscroll + draggable bar
    void scrollbar(Rectangle region, float& scroll, float contentH); // float overload (bar + autoscroll)
    void autoScroll(Rectangle region, float* scrollF, int* scrollI, float maxS); // web-style middle-click autoscroll
    std::string searchBox(Rectangle r, std::string& text, int id);  // 검색 입력칸 (returns lowercased query)
    static bool nameMatch(const std::string& name, const std::string& q); // case-insensitive contains
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
    void pickAndImportItemIcon();                     // file picker -> import + assign a DB item's icon
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
    int  eventFieldFocus_ = 0; // which event text field is focused (0 none,1 text,2 speaker,3 choiceA,4 choiceB)
    float eventScroll_ = 0;   // event inspector vertical scroll (tall quest panels)
    int   eventScrollId_ = -1;// which event eventScroll_ belongs to (reset on change)
    float eventListScroll_ = 0; // left event-list panel scroll
    Event eventClip_;           // copied event (paste works across maps)
    bool  eventClipHas_ = false;
    int   dragEventId_ = -1;     // event being dragged to a new tile (events tab)
    int   eventListFilter_ = -1; // event-list type filter (-1 = all, else EventType index)
    // Database editing
    int  dbCategory_ = 0;     // 0 items,1 equip,2 skills,3 actors,4 enemies
    int  dbSelected_ = -1;
    int  dbNameFocus_ = -1;
    int  dbDescFocus_ = -1;   // item description text-field focus
    float dbScroll_ = 0;      // detail-panel vertical scroll (tall editors overflow)
    bool  dbExpanded_[3] = { true, true, true };  // accordion: each category section open?
    float dbListScroll_ = 0;  // accordion list vertical scroll

    // 대화로그 / 시나리오 편집 상태
    int   dlgSel_ = -1, dlgLineSel_ = -1, dlgFocus_ = -1;
    float dlgLineScroll_ = 0, dlgAnsScroll_ = 0, dlgListScroll_ = 0, scnListScroll_ = 0;
    int   scnSel_ = -1, scnFocus_ = -1;
    float scnActScroll_ = 0;
    int   scnActSel_ = -1;            // 맵에서 편집 중인 동작(액션) 인덱스
    int   dlgMapId_ = -1;            // 대화 탭에서 배경으로 보는 맵
    int   dlgNpcEventId_ = -1;       // 대화 탭에서 맵에서 선택한 NPC(event) id
    bool  dlgPopupOpen_ = false;     // 대화 탭: NPC 클릭 시 뜨는 편집 팝업 표시
    int   scnTrigMode_ = 0;          // 시나리오 발동지정 모드: 0=없음 1=맵지점 2=NPC대화
    int   scnDragIdx_ = -1;          // 시나리오: 드래그 중인 마커(액션 인덱스, -2=발동지점)
    // ── 장면녹화(RTS식 무대) 상태 ──
    bool  scnRecordMode_ = false;    // 녹화 모드: 무대 토큰을 끌어 배치 → '장면 녹화'로 기록
    int   scnRecEffect_ = -1;        // 뿌릴 이펙트 에셋(오른쪽 목록에서 선택)
    int   scnRecMob_ = -1;           // 무대에 등장시킬 몹
    // RTS식 다중 선택/이동
    std::vector<int> scnSelTags_;    // 현재 선택된 토큰 태그들(다중 선택)
    bool  scnGroupDrag_ = false;     // 선택 그룹을 드래그 이동 중
    Vector2 scnDragStartTile_{};     // 그룹 드래그 시작 타일
    std::unordered_map<int, Vector2> scnDragBase_;  // 드래그 시작 시 각 태그 위치
    bool  scnMarquee_ = false;       // 마퀴(박스) 선택 드래그 중
    Vector2 scnMarqueeStart_{};      // 마퀴 시작 화면좌표
    bool  scnCtxOpen_ = false;       // 우클릭 동작/삭제 메뉴 열림(선택 전체 대상)
    Vector2 scnCtxPos_{};            // 컨텍스트 메뉴 위치
    int   scnRecRadius_ = 2;         // 뿌릴 이펙트 반경(칸)
    float scnStepDur_ = 1.0f;        // 한 장면(스텝) 표시시간 0.42~1.42초
    std::unordered_map<int, Vector2> scnDraft_;  // 태그 -> 이번 스텝의 드래프트 위치(칸)
    std::vector<SceneAction> scnPendingFx_;      // 배치했지만 아직 녹화 안 된 이펙트들

    // dropdown picker state (see optionButton/drawPickerOverlay)
    int   pickerId_ = -1;
    Rectangle pickerAnchor_{};
    std::vector<std::string> pickerOpts_;          // option labels shown in the open list
    int   pickerCurIdx_ = -1;                      // currently-selected option index (highlight)
    std::function<void(int)> pickerApply_;         // deferred write: apply chosen option index
    float pickerScroll_ = 0;
    // web-style middle-click autoscroll (toggle on, move to scroll, speed ∝ distance)
    void* autoScrollTarget_ = nullptr;
    Vector2 autoScrollOrigin_{};
    float autoScrollAccum_ = 0;
    void* barDragTarget_ = nullptr;   // float scrollbar thumb drag
    float barDragGrab_ = 0;
    // per-list search queries + which search box is focused
    int   searchFocusId_ = -1;
    std::string dlgSearch_, scnSearch_, charSearch_, npcSearch_, dbSearch_, evSearch_;
    // Explorer-like character/image browser overlay
    bool  charBrowserOpen_ = false;
    std::string charBrowserSearch_;
    float charBrowserScroll_ = 0;
    std::function<void(int)> charBrowserApply_;   // chosen asset id (-1=없음) → caller
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
    bool worldStartSettings_ = false;  // start-loadout editor modal open
    float worldPrevScroll_ = 0;        // preview edit-panel object-list scroll
    RenderTexture2D worldBigThumb_{}; // hi-res preview texture (built on demand)
    int  worldBigId_ = -1;            // map id the big preview was built for
    int  worldPreviewMapId_ = -1;     // map currently shown in the overlay (navigable)
    int  worldPrevSelEvent_ = -1;     // event whose info box is shown in the overlay
    int  prevMobToPlace_ = -1;        // mob id armed for click-to-place on the preview map
    // preview marker drag (move icons like desktop files): kind 0=event,1=mobspawn
    int  wpDragKind_ = -1, wpDragRef_ = -1;
    bool wpDragMoved_ = false;
    Vector2 wpDragStart_{};
    int  wpLastClickRef_ = -1;        // double-click detection on event markers
    double wpLastClickTime_ = -10;
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
    // Character builder (custom multi-motion characters). The SAME builder drives
    // the Mob tab: when mobMode_ is true it edits db.mobs[mobSel_] instead of
    // db.characters[charDefSel_] (mobs are CharacterDefs + monster fields).
    bool mobMode_ = false;            // true while the 몹 tab is active
    int  mobSel_  = -1;               // selected mob index (parallels charDefSel_)
    int  charDefSel_ = -1;            // selected custom character index
    int  charMotionTab_ = 0;          // selected motion tab (0..5)
    int  charSkillSlot_ = -1;         // skill slot being edited (0..5 Z/X/C/V/F/G; -1 = derive from motion)
    int  charDirTab_ = 0;             // edited direction: 0정면(아래)/1좌/2우/3위
    bool pendingImport_ = false;      // request the native file picker outside the draw frame
    bool pendingEffectImport_ = false;     // request the picker to assign a skill effect strip
    bool pendingSoundImport_  = false;     // request the picker to assign a skill sound
    bool pendingBgmImport_ = false;        // request the picker to import a map BGM
    int  pendingBgmMapId_ = -1;            // map awaiting an imported BGM
    bool pendingItemIcon_ = false;         // request the picker to import a DB item icon
    int  pendingItemIconId_ = -1;          // item awaiting an imported icon
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

    // Confirmation dialog for destructive actions (delete map/asset, etc.). The
    // action runs only if the user clicks 확인; while open it blocks the UI behind it.
    bool confirmOpen_ = false;
    std::string confirmMsg_;
    std::function<void()> confirmAction_;
    void askConfirm(const std::string& msg, std::function<void()> action) {
        confirmMsg_ = msg; confirmAction_ = std::move(action); confirmOpen_ = true;
    }
    void drawConfirmOverlay();

    void setStatus(const std::string& s) { status_ = s; statusTimer_ = 3.0f; }
    void pushUndo();   // snapshot current map before an edit
    void doUndo();
    void doRedo();
    void stampPrefab(int ox, int oy); // place the selected prefab at a tile
    void drawPrefabPalette(Rectangle area);
};

} // namespace tsukuru
