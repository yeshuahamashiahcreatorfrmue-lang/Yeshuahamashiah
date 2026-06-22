#pragma once
// Editor internals shared across the per-tab translation units (EditorMap.cpp,
// EditorWorld.cpp, ...). Inline so every editor TU sees one definition.
#include <cstdlib>

namespace tsukuru {

inline constexpr float kToolbarH = 40;   // top toolbar height
inline constexpr float kPaletteW = 220;  // left tile/stamp palette width
inline constexpr int   kUndoLimit = 42;  // Ctrl+Z history depth

// Build tag shown in the editor so a user can confirm they launched the newest
// build. Bump on every delivered build.
inline const char* const kBuildTag = "빌드 0627f — 전맵뷰어 썸네일 한칸 채움·캐릭터 브라우저(에셋과 분리·플레이어/삭제)·에셋 우클릭 삭제";

// Standardized world sizes (14 tiers), square, up to 1742x1742.
inline constexpr int kSizeTierCount = 14;
inline constexpr int kSizeTiers[kSizeTierCount] = {
    30, 60, 120, 180, 270, 360, 420,
    540, 720, 900, 1100, 1320, 1536, 1742
};
inline const char* const kSizeTierNames[kSizeTierCount] = {
    "1단계 30x30", "2단계 60x60", "3단계 120x120", "4단계 180x180",
    "5단계 270x270", "6단계 360x360", "7단계 420x420",
    "8단계 540x540", "9단계 720x720", "10단계 900x900", "11단계 1100x1100",
    "12단계 1320x1320", "13단계 1536x1536", "14단계 1742x1742 (최대)"
};
// nearest size tier index for a given side length
inline int sizeTierOf(int side) {
    int best = 0, bestd = 1 << 30;
    for (int i = 0; i < kSizeTierCount; ++i) { int d = std::abs(kSizeTiers[i] - side); if (d < bestd) { bestd = d; best = i; } }
    return best;
}

} // namespace tsukuru
