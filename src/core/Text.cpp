#include "core/Text.h"
#include <vector>
#include <algorithm>

namespace tsukuru {

Font g_uiFont = {};
bool g_uiFontReady = false;

// Extra spacing between glyphs, kept consistent between draw and measure so
// layout is stable. Scales with the requested font size.
static float spacingFor(int fontSize) { return fontSize / 16.0f; }

void LoadUIFont(const std::string& ttfPath, const std::string& glyphSourceUtf8) {
    if (!FileExists(ttfPath.c_str())) {
        TraceLog(LOG_WARNING, "UI font not found: %s (using ASCII fallback)", ttfPath.c_str());
        return;
    }
    // Start with printable ASCII so English/numbers/symbols always render.
    std::vector<int> cps;
    for (int c = 32; c < 127; ++c) cps.push_back(c);

    // Add the unique codepoints used by the localized UI strings.
    int count = 0;
    int* found = LoadCodepoints(glyphSourceUtf8.c_str(), &count);
    for (int i = 0; i < count; ++i) {
        int cp = found[i];
        if (cp < 32) continue;
        if (std::find(cps.begin(), cps.end(), cp) == cps.end()) cps.push_back(cp);
    }
    UnloadCodepoints(found);

    // Bake at a comfortable size; DrawTextEx scales down crisply for small UI.
    const int kBaseSize = 36;
    Font f = LoadFontEx(ttfPath.c_str(), kBaseSize, cps.data(), (int)cps.size());
    if (f.texture.id == 0 || f.glyphCount == 0) {
        TraceLog(LOG_WARNING, "UI font failed to bake: %s", ttfPath.c_str());
        return;
    }
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    g_uiFont = f;
    g_uiFontReady = true;
    TraceLog(LOG_INFO, "UI font loaded: %s (%d glyphs)", ttfPath.c_str(), (int)cps.size());
}

void UnloadUIFont() {
    if (g_uiFontReady) { UnloadFont(g_uiFont); g_uiFontReady = false; }
}

void DrawTextU(const char* text, int x, int y, int fontSize, Color color) {
    if (!text || !text[0]) return;
    if (g_uiFontReady) {
        DrawTextEx(g_uiFont, text, Vector2{ (float)x, (float)y },
                   (float)fontSize, spacingFor(fontSize), color);
    } else {
        DrawText(text, x, y, fontSize, color);
    }
}

int MeasureTextU(const char* text, int fontSize) {
    if (!text || !text[0]) return 0;
    if (g_uiFontReady) {
        Vector2 v = MeasureTextEx(g_uiFont, text, (float)fontSize, spacingFor(fontSize));
        return (int)v.x;
    }
    return MeasureText(text, fontSize);
}

// --- global UI scale ---
static int   s_logicalW = 1280, s_logicalH = 720;
static float s_uiScale = 1.5f;   // default: 1.5x bigger UI/text
void setLogicalScreen(int w, int h) { s_logicalW = w; s_logicalH = h; }
int  screenW() { return s_logicalW; }
int  screenH() { return s_logicalH; }
void setUiScale(float s) { s_uiScale = s < 1.0f ? 1.0f : (s > 3.0f ? 3.0f : s); }
float uiScale() { return s_uiScale; }

} // namespace tsukuru
