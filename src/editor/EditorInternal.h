#pragma once
// Editor internals shared across the per-tab translation units (EditorMap.cpp,
// EditorWorld.cpp, ...). Inline so every editor TU sees one definition.
#include <cstdlib>

namespace tsukuru {

inline constexpr float kToolbarH = 40;   // top toolbar height
inline constexpr float kPaletteW = 220;  // left tile/stamp palette width
inline constexpr int   kUndoLimit = 42;  // Ctrl+Z history depth

// Standardized world sizes (7 tiers), square, up to 420x420.
inline constexpr int kSizeTiers[7] = { 30, 60, 120, 180, 270, 360, 420 };
inline const char* const kSizeTierNames[7] = {
    "1단계 30x30", "2단계 60x60", "3단계 120x120", "4단계 180x180",
    "5단계 270x270", "6단계 360x360", "7단계 420x420 (최대)"
};
// nearest size tier index for a given side length
inline int sizeTierOf(int side) {
    int best = 0, bestd = 1 << 30;
    for (int i = 0; i < 7; ++i) { int d = std::abs(kSizeTiers[i] - side); if (d < bestd) { bestd = d; best = i; } }
    return best;
}

} // namespace tsukuru
