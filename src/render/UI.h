#pragma once
// Minimal immediate-mode UI helpers built on raylib — buttons, panels, labels,
// and a simple text-input field. Keeps the engine free of extra UI dependencies.
#include <string>
#include "raylib.h"

namespace tsukuru {
namespace ui {

// Theme colors
const Color kPanel    = {  34,  38,  48, 255 };
const Color kPanelHi  = {  48,  54,  68, 255 };
const Color kAccent   = {  86, 156, 214, 255 };
const Color kAccentHi = { 120, 184, 240, 255 };
const Color kText     = { 224, 226, 232, 255 };
const Color kTextDim  = { 140, 146, 158, 255 };
const Color kDanger   = { 214,  96,  96, 255 };
const Color kGood     = { 120, 200, 120, 255 };

// NPC/몹 faction tint shared by every view: 0 중립(gray) / 1 아군(blue) / 2 적(red).
inline Color factionColor(int faction) {
    return faction == 2 ? kDanger
         : faction == 1 ? Color{ 90, 170, 255, 255 }
                        : Color{ 210, 210, 210, 255 };
}

void panel(Rectangle r, Color c = kPanel);
void label(const std::string& text, int x, int y, int size = 18, Color c = kText);
void labelCentered(const std::string& text, Rectangle r, int size = 18, Color c = kText);

// Returns true on click. `active` highlights the button (e.g. selected tool).
bool button(Rectangle r, const std::string& text, bool active = false, int fontSize = 16);

// Editable single-line text field. `focused` controls whether it captures input.
// Returns true while focused. Edits `text` in place. Caller manages focus id.
bool textField(Rectangle r, std::string& text, bool focused, int maxLen = 64);

// Integer stepper: [-] value [+]. Returns true if value changed.
bool intStepper(Rectangle r, const std::string& lbl, int& value, int step, int lo, int hi);

bool mouseIn(Rectangle r);

} // namespace ui
} // namespace tsukuru
