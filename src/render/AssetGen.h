#pragma once
// Procedural asset generators (CPU-side raylib Image API). Used both by the
// offline tools and by the in-editor "generate character" feature so users can
// create new character sheets without external art.
#include "raylib.h"

namespace tsukuru {
namespace gen {

inline void rect(Image* img, int x, int y, int w, int h, Color c) {
    ImageDrawRectangle(img, x, y, w, h, c);
}

// A character sheet: 4 walk frames + `attackFrames` attack frames, x 4
// directions (Down,Left,Right,Up), 32px. Columns = 4 + attackFrames; the attack
// columns show a weapon thrust/swing in the facing direction.
inline Image characterSheet(Color shirt, Color skin, int attackFrames = 0) {
    const int F = 32, WALK = 4, DIRS = 4;
    if (attackFrames < 0) attackFrames = 0;
    const int COLS = WALK + attackFrames;
    Image img = GenImageColor(COLS * F, DIRS * F, BLANK);
    Color hair   = { 60, 45, 35, 255 };
    Color pants  = { 60, 70, 110, 255 };
    Color weapon = { 220, 225, 235, 255 };
    for (int d = 0; d < DIRS; ++d) {
        for (int f = 0; f < COLS; ++f) {
            int ox = f * F, oy = d * F;
            bool atk = f >= WALK;
            int legShift = (!atk && f == 1) ? -2 : (!atk && f == 3) ? 2 : 0;
            rect(&img, ox+11, oy+24, 4, 6 + (legShift>0?legShift:0), pants);
            rect(&img, ox+17, oy+24, 4, 6 + (legShift<0?-legShift:0), pants);
            rect(&img, ox+10, oy+14, 12, 11, shirt);
            rect(&img, ox+11, oy+5, 10, 9, skin);
            rect(&img, ox+10, oy+4, 12, 4, hair);
            if (d == 3) rect(&img, ox+10, oy+8, 12, 6, hair);
            if (d == 0) { ImageDrawPixel(&img,ox+13,oy+10,BLACK); ImageDrawPixel(&img,ox+18,oy+10,BLACK); }
            if (d == 1)   ImageDrawPixel(&img,ox+13,oy+10,BLACK);
            if (d == 2)   ImageDrawPixel(&img,ox+18,oy+10,BLACK);
            rect(&img, ox+8,  oy+15, 2, 7, skin);
            rect(&img, ox+22, oy+15, 2, 7, skin);
            if (atk) {
                // weapon thrust in the facing direction; reach grows over frames
                int reach = 6 + (f - WALK) * 6;
                if (d == 0) rect(&img, ox+14, oy+24, 4, reach, weapon);          // down
                else if (d == 3) rect(&img, ox+14, oy+4 - reach + 6, 4, reach, weapon); // up
                else if (d == 1) rect(&img, ox+8 - reach, oy+16, reach, 4, weapon);     // left
                else rect(&img, ox+24, oy+16, reach, 4, weapon);                 // right
            }
        }
    }
    return img;
}

// A skill-effect strip: a single row of `frames` animation frames (direction-
// agnostic; the engine rotates the hit pattern, not the sprite). style:
// 0 slash 1 bolt 2 dash 3 burst. Lets users create effect assets in-engine.
inline Image effectSheet(Color tint, int style = 0, int frames = 6) {
    const int F = 48;
    if (frames < 1) frames = 1;
    Image img = GenImageColor(frames * F, F, BLANK);
    for (int f = 0; f < frames; ++f) {
        int ox = f * F, cx = ox + F/2, cy = F/2;
        float k = (f + 1) / (float)frames;
        unsigned char a = (unsigned char)(255 * (1.0f - 0.4f*k));
        Color c = { tint.r, tint.g, tint.b, a };
        Color wh = { 255, 255, 255, a };
        if (style == 0) {                       // slash: growing arc
            ImageDrawCircle(&img, cx, cy, (int)(4 + k*16), c);
            ImageDrawCircle(&img, cx, cy, (int)(2 + k*10), wh);
        } else if (style == 1) {                // bolt: comet
            ImageDrawCircle(&img, cx, cy, (int)(4 + k*9), c);
            ImageDrawCircle(&img, cx, cy, (int)(2 + k*4), wh);
        } else if (style == 2) {                // dash: streak
            ImageDrawRectangle(&img, ox+4, cy-3, (int)(F*0.85f*k), 6, c);
        } else {                                // burst: expanding ring
            ImageDrawCircle(&img, cx, cy, (int)(k*F*0.5f), c);
            ImageDrawCircle(&img, cx, cy, (int)(k*F*0.34f), wh);
        }
    }
    return img;
}

// A simple blob enemy sprite (single 48px image).
inline Image enemySprite(Color body) {
    const int S = 48;
    Image img = GenImageColor(S, S, BLANK);
    rect(&img, 6, 20, 36, 22, body);
    rect(&img, 10, 14, 28, 10, body);
    rect(&img, 14, 26, 4, 4, WHITE);  rect(&img, 30, 26, 4, 4, WHITE);
    rect(&img, 15, 27, 2, 2, BLACK);  rect(&img, 31, 27, 2, 2, BLACK);
    return img;
}

} // namespace gen
} // namespace tsukuru
