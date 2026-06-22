#include "render/UI.h"
#include "core/Text.h"
#include "core/Platform.h"
#include <cstring>

namespace tsukuru {
namespace ui {

bool g_inputEnabled = true;

bool mouseIn(Rectangle r) {
    return CheckCollisionPointRec(GetMousePosition(), r);
}

void panel(Rectangle r, Color c) {
    DrawRectangleRec(r, c);
    DrawRectangleLinesEx(r, 1, Fade(BLACK, 0.4f));
}

void label(const std::string& text, int x, int y, int size, Color c) {
    DrawTextU(text.c_str(), x, y, size, c);
}

void labelCentered(const std::string& text, Rectangle r, int size, Color c) {
    int w = MeasureTextU(text.c_str(), size);
    DrawTextU(text.c_str(), (int)(r.x + (r.width - w) / 2),
             (int)(r.y + (r.height - size) / 2), size, c);
}

bool button(Rectangle r, const std::string& text, bool active, int fontSize) {
    bool hover = mouseIn(r);
    bool click = g_inputEnabled && hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
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

    if (focused && g_inputEnabled) {
        int key = GetCharPressed();
        while (key > 0) {
            // Accept ANY printable codepoint (Korean/CJK/accented Latin…) and store
            // it UTF-8 encoded. maxLen is a byte budget (Korean ≈ 3 bytes/char).
            if (key >= 32 && key != 127) {
                int n = 0;
                const char* enc = CodepointToUTF8(key, &n);
                if ((int)text.size() + n <= maxLen) text.append(enc, (size_t)n);
            }
            key = GetCharPressed();
        }
        // Backspace removes a whole UTF-8 codepoint (drop trailing continuation bytes).
        if (IsKeyPressed(KEY_BACKSPACE) && !text.empty()) {
            size_t i = text.size();
            do { --i; } while (i > 0 && ((unsigned char)text[i] & 0xC0) == 0x80);
            text.erase(i);
        }
    }
    // Live IME composition: show the half-typed Hangul/CJK syllable immediately
    // (each 자음/모음 appears as it's composed) — committed text still comes via
    // GetCharPressed above. The composing part is drawn in the accent colour.
    std::string comp = (focused && g_inputEnabled) ? plat::imeComposition() : std::string();
    int ty = (int)(r.y + (r.height - 16) / 2);
    DrawTextU(text.c_str(), (int)r.x + 6, ty, 16, kText);
    int caretX = (int)r.x + 6 + MeasureTextU(text.c_str(), 16);
    if (!comp.empty()) {
        DrawTextU(comp.c_str(), caretX, ty, 16, kAccentHi);
        int cw = MeasureTextU(comp.c_str(), 16);
        DrawLine(caretX, ty + 18, caretX + cw, ty + 18, kAccentHi);   // composition underline
        caretX += cw;
    }
    if (focused && ((int)(GetTime() * 2) % 2 == 0))
        DrawTextU("_", caretX, ty, 16, kText);
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
