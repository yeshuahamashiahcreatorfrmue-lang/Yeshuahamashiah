#include "game/TitleScreen.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using nlohmann::json;

namespace tsukuru {

TitleScreen::TitleScreen(Engine& engine) : engine_(engine) {}

bool TitleScreen::hasSave() const {
    return fs::exists(fs::path(engine_.project().dir) / "save" / "slot1.json");
}

void TitleScreen::update(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) { engine_.setMode(Mode::Editor); return; }

    int count = 3;
    if (IsKeyPressed(KEY_DOWN)) selection_ = (selection_ + 1) % count;
    if (IsKeyPressed(KEY_UP))   selection_ = (selection_ + count - 1) % count;

    bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);

    // Mouse support: hovering highlights an option, clicking confirms it.
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int oy = sh / 2 + 40;
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < count; ++i) {
        Rectangle r = { (float)sw / 2 - 160, (float)(oy + i * 44 - 4), 320, 36 };
        if (CheckCollisionPointRec(mp, r)) {
            selection_ = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) confirm = true;
        }
    }

    if (!confirm) return;

    if (selection_ == 0) { // New Game
        Project& p = engine_.project();
        engine_.state().newGame(p.database, p.startActor, p.startMap, p.startX, p.startY);
        engine_.setMode(Mode::Play);
    } else if (selection_ == 1 && hasSave()) { // Continue
        std::ifstream f((fs::path(engine_.project().dir) / "save" / "slot1.json").string());
        if (f) { json j; f >> j; engine_.state().fromJson(j); engine_.setMode(Mode::Play); }
    } else if (selection_ == 2) { // Quit to editor
        engine_.setMode(Mode::Editor);
    }
}

void TitleScreen::draw() {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    // Backdrop gradient
    DrawRectangleGradientV(0, 0, sw, sh, Color{ 20, 28, 48, 255 }, Color{ 8, 10, 18, 255 });

    const char* title = engine_.project().name.c_str();
    int ts = 64;
    int tw = MeasureTextU(title, ts);
    DrawTextU(title, (sw - tw) / 2, sh / 4, ts, ui::kText);
    const char* sub = "쯔꾸르 엔진으로 제작됨";
    int subw = MeasureTextU(sub, 18);
    DrawTextU(sub, (sw - subw) / 2, sh / 4 + ts + 8, 18, ui::kTextDim);

    const char* opts[3] = { "새 게임", "이어하기", "에디터로 나가기" };
    bool enabled[3] = { true, hasSave(), true };
    int oy = sh / 2 + 40;
    for (int i = 0; i < 3; ++i) {
        Color c = !enabled[i] ? ui::kTextDim : (i == selection_ ? ui::kAccentHi : ui::kText);
        std::string text = (i == selection_ ? "> " : "  ") + std::string(opts[i]);
        int w = MeasureTextU(text.c_str(), 28);
        DrawTextU(text.c_str(), (sw - w) / 2, oy + i * 44, 28, c);
    }
    DrawTextU("위/아래: 선택   Enter: 확인   ESC: 에디터",
             20, sh - 30, 16, ui::kTextDim);
}

} // namespace tsukuru
