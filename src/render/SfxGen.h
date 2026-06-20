#pragma once
// SfxGen: tiny procedural sound-effect synthesizer (16-bit mono Wave) so users
// can create per-skill sounds inside the engine without external audio files.
// Shared by the editor's "사운드 생성" tool.
#include "raylib.h"
#include <vector>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace tsukuru {
namespace gen {

inline constexpr int kSfxRate = 22050;

// Append `dur` seconds of frequency `f` with a linear decay envelope.
// wave: 0 square, 1 sine, 2 noise.
inline void sfxNote(std::vector<short>& b, double f, double dur, double vol, int wave, double sweep = 0.0) {
    const double TAU = 6.28318530717958647692;
    int n = (int)(dur * kSfxRate);
    double phase = 0;
    for (int i = 0; i < n; ++i) {
        double prog = (double)i / n;
        double env = 1.0 - prog;                    // simple decay
        double freq = f * (1.0 + sweep * prog);     // optional pitch sweep
        phase += freq / kSfxRate;
        double s;
        if (wave == 0)      s = (fmod(phase, 1.0) < 0.5 ? 1.0 : -1.0);
        else if (wave == 1) s = sin(phase * TAU);
        else                s = ((rand() % 2001) - 1000) / 1000.0;
        int v = (int)(s * vol * env * 30000.0);
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        b.push_back((short)v);
    }
}

// Build a skill sound. style: 0 베기 slash, 1 마법 cast, 2 폭발 explosion,
// 3 대시 whoosh, 4 회복 heal.  Returns a Wave (caller UnloadWave()s it).
inline Wave skillSound(int style) {
    std::vector<short> b;
    switch (style & 7) {
        case 0: // slash: bright down-sweep
            sfxNote(b, 1100, 0.06, 0.55, 2, -0.6);
            sfxNote(b, 600,  0.05, 0.45, 0, -0.4);
            break;
        case 1: // magic cast: rising sine arpeggio
            sfxNote(b, 523, 0.06, 0.5, 1);
            sfxNote(b, 784, 0.06, 0.5, 1);
            sfxNote(b, 1047, 0.10, 0.5, 1, 0.4);
            break;
        case 2: // explosion: low noise burst + boom
            sfxNote(b, 180, 0.05, 0.7, 2);
            sfxNote(b, 90,  0.22, 0.8, 1, -0.3);
            sfxNote(b, 60,  0.14, 0.6, 2);
            break;
        case 3: // dash whoosh: short noise sweep up
            sfxNote(b, 300, 0.12, 0.45, 2, 1.4);
            break;
        default: // heal: gentle ascending chord
            sfxNote(b, 659, 0.08, 0.4, 1);
            sfxNote(b, 880, 0.08, 0.4, 1);
            sfxNote(b, 1175, 0.14, 0.4, 1, 0.2);
            break;
    }
    if (b.empty()) b.push_back(0);
    Wave w{};
    w.frameCount = (unsigned)b.size();
    w.sampleRate = kSfxRate;
    w.sampleSize = 16;
    w.channels   = 1;
    w.data = malloc(b.size() * sizeof(short));
    memcpy(w.data, b.data(), b.size() * sizeof(short));
    return w;
}

inline const char* skillSoundName(int style) {
    static const char* n[5] = { "베기", "마법", "폭발", "대시", "회복" };
    return n[style % 5];
}

} // namespace gen
} // namespace tsukuru
