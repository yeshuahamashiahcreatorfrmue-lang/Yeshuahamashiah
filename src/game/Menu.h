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

private:
    enum class Page { Root, Items, Equip, Status };
    void drawRoot();
    void drawItems();
    void drawEquip();
    void drawStatus();

    Engine& engine_;
    Page page_ = Page::Root;
    int  selection_ = 0;
    std::string toast_;
    float toastTimer_ = 0;
};

} // namespace tsukuru
