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
    // Cached "이어하기" metadata so draw()/hasSave() don't stat & parse save files
    // every frame; refreshed on a short timer (saves change only between sessions).
    bool   continueExists_ = false;
    int    continueLevel_ = 1;
    double continueSeconds_ = 0;
    float  metaTimer_ = 0;
    void refreshContinueMeta();
    bool hasSave() const { return continueExists_; }
    void startSingle();
};

} // namespace tsukuru
