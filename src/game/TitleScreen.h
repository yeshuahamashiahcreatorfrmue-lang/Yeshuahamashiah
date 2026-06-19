#pragma once
// TitleScreen: the game's entry screen — New Game / Continue / Quit to Editor.
#include "raylib.h"

namespace tsukuru {

class Engine;

class TitleScreen {
public:
    explicit TitleScreen(Engine& engine);
    void update(float dt);
    void draw();

private:
    Engine& engine_;
    int selection_ = 0;
    bool hasSave() const;
};

} // namespace tsukuru
