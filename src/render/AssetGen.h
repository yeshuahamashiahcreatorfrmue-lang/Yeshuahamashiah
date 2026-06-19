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

// A character sheet: 4 walk frames x 4 directions (Down,Left,Right,Up), 32px.
inline Image characterSheet(Color shirt, Color skin) {
    const int F = 32, FRAMES = 4, DIRS = 4;
    Image img = GenImageColor(FRAMES * F, DIRS * F, BLANK);
    Color hair  = { 60, 45, 35, 255 };
    Color pants = { 60, 70, 110, 255 };
    for (int d = 0; d < DIRS; ++d) {
        for (int f = 0; f < FRAMES; ++f) {
            int ox = f * F, oy = d * F;
            int legShift = (f == 1) ? -2 : (f == 3) ? 2 : 0;
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
