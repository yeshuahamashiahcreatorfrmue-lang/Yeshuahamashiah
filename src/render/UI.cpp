#include "render/UI.h"
#include <cstring>

namespace tsukuru {
namespace ui {

bool mouseIn(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r);
}

void panel(Rectangle r, Color c) {
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 1, Fade(BLACK, 0.4f));
}

void label(const std::string& text, int x, int y, int size, Color c) {
    DrawText(text.c_str(), x, y, size, c);
}

void labelCentered(const std::string& text, Rectangle r, int size, Color c) {
    int w = MeasureText(text.c_str(), size);
    DrawText(text.c_str(), (int)(r.x + (r.width - w) / 2),
             (int)(r.y + (r.height - size) / 2), size, c);
}

bool button(Rectangle r, const std::string& text, bool active, int fontSize) {
    bool hover = mouseIn(r);
    bool click = hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    Color bg = active ? kAccent : (hover ? kPanelHi : kPanel);
    if (active && hover) bg = kAccentHi;
    DrawRectangleRec(r, bg);
    DrawRectangleLinesEx(r, 1, Fade(BLACK, 0.5f));
    labelCentered(text, r, fontSize, active ? BLACK : kText);
    return click;
}

bool textField(Rectangle r, std::string& text, bool focused, int maxLen) {
    DrawRectangleRec(r, focused ? kPanelHi : kPanel);
    DrawRectangleLinesEx(r, 1, focused ? kAccent : Fade(BLACK, 0.5f));

    if (focused) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 125 && (int)text.size() < maxLen)
                text.push_back((char)key);
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) text.pop_back();
    }
    std::string shown = text;
    if (focused && ((int)(GetTime() * 2) % 2 == 0)) shown += "_";
    DrawText(shown.c_str(), (int)r.x + 6, (int)(r.y + (r.height - 16) / 2), 16, kText);
    return focused;
}

bool intStepper(Rectangle r, const std::string& lbl, int& value, int step, int lo, int hi) {
    float bw = 26;
    Rectangle minus = { r.x, r.y, bw, r.height };
    Rectangle plus  = { r.x + r.width - bw, r.y, bw, r.height };
    Rectangle mid   = { r.x + bw, r.y, r.width - 2 * bw, r.height };
    bool changed = false;
    if (button(minus, "-")) { value -= step; changed = true; }
    if (button(plus,  "+")) { value += step; changed = true; }
    if (value < lo) value = lo;
    if (value > hi) value = hi;
    DrawRectangleRec(mid, kPanel);
    labelCentered(lbl + ": " + std::to_string(value), mid, 14, kText);
    return changed;
}

} // namespace ui
} // namespace tsukuru
