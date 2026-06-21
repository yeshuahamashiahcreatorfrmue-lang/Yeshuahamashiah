#pragma once
// Menu: the in-game pause menu (Items / Equip / Status / Save / Close).
#include <string>
#include "raylib.h"

namespace tsukuru {

class Engine;

class Menu {
public:
    explicit Menu(Engine& engine);
    // Returns true while the menu should stay open; false when closed.
    bool update(float dt);
    void draw();
    void open() { page_ = Page::Root; selection_ = 0; }
    void openPage(int p) {   // debug: jump to a page (0 Root/1 Items/2 Equip/3 Status/4 Settings/5 Save)
        page_ = p==1?Page::Items : p==2?Page::Equip : p==3?Page::Status
              : p==4?Page::Settings : p==5?Page::Save : Page::Root;
        selection_ = 0;
        saveCacheValid_ = false;
    }

private:
    enum class Page { Root, Items, Equip, Status, Settings, Save };
    void drawRoot();
    void drawItems();
    void drawEquip();
    void drawStatus();
    void drawSettings();
    void drawSave();

    Engine& engine_;
    Page page_ = Page::Root;
    int  selection_ = 0;     // Root/Items: list index · Equip: body-slot index (0..6)
    std::string toast_;
    float toastTimer_ = 0;

    // Cached save-slot summaries so drawSave() doesn't read 3 JSON files per frame.
    // Rebuilt on entering the Save page or after writing a slot.
    bool saveCacheValid_ = false;
    std::string saveSlotLine_[3];
    void buildSaveCache();
};

} // namespace tsukuru
