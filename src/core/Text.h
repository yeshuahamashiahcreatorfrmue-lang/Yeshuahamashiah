#pragma once
// Unicode-aware text rendering. raylib's built-in DrawText/MeasureText only
// handle the ASCII default font, so they cannot draw Korean (Hangul). These
// drop-in wrappers route through a TTF (NanumGothic) loaded with exactly the
// glyphs the UI uses — keeping the font atlas tiny (optimization) while still
// rendering full Korean. If the font fails to load we transparently fall back
// to raylib's ASCII font so the engine never crashes.
#include <string>
#include "raylib.h"

namespace tsukuru {

// The shared UI font (loaded once after InitWindow). g_uiFontReady is false
// until LoadUIFont succeeds; the DrawTextU/MeasureTextU wrappers consult it.
extern Font g_uiFont;
extern bool g_uiFontReady;

// Load `ttfPath` baking ASCII plus every distinct codepoint found in
// `glyphSourceUtf8`. Duplicate codepoints are de-duplicated so the source may
// be the raw concatenation of all UI strings. No-op-safe if the file is
// missing (leaves the ASCII fallback active).
void LoadUIFont(const std::string& ttfPath, const std::string& glyphSourceUtf8);
void UnloadUIFont();

// Drop-in replacements for raylib's DrawText / MeasureText. Accept UTF-8.
void DrawTextU(const char* text, int x, int y, int fontSize, Color color);
int  MeasureTextU(const char* text, int fontSize);

// --- global UI scale (webpage-style zoom) ---------------------------------
// The whole UI is rendered at a LOGICAL resolution and scaled up to the window,
// so all editor/game code lays out against screenW()/screenH() (logical), NOT
// raylib's screenW(). Engine updates these every frame.
void setLogicalScreen(int w, int h);
int  screenW();           // logical width  (= window width  / uiScale)
int  screenH();           // logical height (= window height / uiScale)
void  setUiScale(float s); // clamps to [1.0, 3.0]
float uiScale();

// --- crisp scaling helpers -------------------------------------------------
// The UI is laid out in LOGICAL coordinates but rendered at NATIVE resolution
// for sharp text: a modelview scale of uiScale() maps logical -> native pixels.
// uiBeginScaled/uiEndScaled wrap the whole frame; uiBeginWorld/uiEndWorld wrap
// a Camera2D region (the scale is composed into the camera, then restored on
// end); uiScissor clips in native pixels (logical * uiScale).
void uiBeginScaled();
void uiEndScaled();
void uiBeginWorld(Camera2D cam);
void uiEndWorld();
void uiScissor(int x, int y, int w, int h);

} // namespace tsukuru
