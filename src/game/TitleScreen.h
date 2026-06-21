#pragma once
// TitleScreen: New Game / Continue / MMO Host / MMO Join / Quit to Editor.
#include <string>
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
    std::string ipText_ = "127.0.0.1";  // MMO 접속 대상 IP
    bool ipFocus_ = false;
    bool hasSave() const;
    void startSingle();
};

} // namespace tsukuru
