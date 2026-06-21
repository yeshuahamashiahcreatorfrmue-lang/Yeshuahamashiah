#include "core/Text.h"
#include "rlgl.h"
#include <vector>
#include <algorithm>
#include <unordered_set>

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
    // Build a de-duplicated codepoint set (unordered_set keeps it O(n), important
    // now that we bake the whole Hangul block — ~11k glyphs).
    std::vector<int> cps;
    std::unordered_set<int> seen;
    auto add = [&](int cp) { if (cp >= 32 && seen.insert(cp).second) cps.push_back(cp); };

    // Printable ASCII so English/numbers/symbols always render.
    for (int c = 32; c < 127; ++c) add(c);

    // Every distinct codepoint used by the baked-in UI/data strings (covers the
    // non-Hangul symbols: ·, →, ★, ─, box-drawing, etc.).
    int count = 0;
    int* found = LoadCodepoints(glyphSourceUtf8.c_str(), &count);
    for (int i = 0; i < count; ++i) add(found[i]);
    UnloadCodepoints(found);

    // The FULL modern Hangul syllable block (가–힣) + Hangul Compatibility Jamo, so
    // the user can type ANY Korean name in editor fields and it renders correctly
    // (item/character/event/effect names, etc.).
    for (int c = 0xAC00; c <= 0xD7A3; ++c) add(c);   // 11,172 완성형 음절
    for (int c = 0x3131; c <= 0x3163; ++c) add(c);   // 호환 자모 (ㄱ–ㅣ)

    // Bake at a resolution that stays sharp at UI sizes and when the whole UI is
    // scaled up. With the full Hangul set this produces a large atlas; 48px keeps
    // it within a widely-supported texture size while remaining crisp.
    const int kBaseSize = 48;
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

// Push the global logical->native scale onto the modelview (whole-frame wrap).
void uiBeginScaled() { rlPushMatrix(); rlScalef(s_uiScale, s_uiScale, 1.0f); }
void uiEndScaled()   { rlPopMatrix(); }

// Enter a Camera2D region: compose the global scale into the camera so world
// content is rasterized at native density. (A pure scale about the origin folds
// into the camera as offset*=s, zoom*=s; target/rotation are unchanged.)
void uiBeginWorld(Camera2D cam) {
    cam.offset.x *= s_uiScale; cam.offset.y *= s_uiScale; cam.zoom *= s_uiScale;
    BeginMode2D(cam);
}
// Leave the region. EndMode2D resets the modelview, so re-apply the global scale
// for the UI drawn afterwards (balanced by the single uiEndScaled() pop).
void uiEndWorld() { EndMode2D(); rlScalef(s_uiScale, s_uiScale, 1.0f); }

// Scissor clips in framebuffer pixels, which are unaffected by the modelview
// scale, so convert the logical rect to native pixels here.
void uiScissor(int x, int y, int w, int h) {
    float s = s_uiScale;
    BeginScissorMode((int)(x * s), (int)(y * s), (int)(w * s), (int)(h * s));
}

} // namespace tsukuru
