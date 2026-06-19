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

} // namespace tsukuru
