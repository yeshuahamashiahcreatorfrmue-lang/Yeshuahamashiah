// Headless asset generator: produces the sample tileset and character sheets as
// PNG files using raylib's CPU-side image API (no window / OpenGL required).
// Usage: gen_assets <output_dir>
#include "raylib.h"
#include <string>
#include <cstdio>

static void rect(Image* img, int x, int y, int w, int h, Color c) {
    ImageDrawRectangle(img, x, y, w, h, c);
}

// ---- Tileset: 8 columns x 2 rows of 32px tiles (256x64) ----
static void genTileset(const std::string& path) {
    const int T = 32, COLS = 8, ROWS = 2;
    Image img = GenImageColor(COLS * T, ROWS * T, BLANK);

    auto cell = [&](int idx) { return Vector2{ (float)(idx % COLS) * T, (float)(idx / COLS) * T }; };

    Color grass1 = { 88, 160, 80, 255 }, grass2 = { 104, 176, 92, 255 };
    Color dirt   = { 168, 132, 84, 255 }, dirt2  = { 150, 116, 72, 255 };
    Color water1 = { 64, 120, 200, 255 }, water2 = { 84, 140, 220, 255 };
    Color wood   = { 150, 110, 70, 255 }, wood2  = { 134, 96, 60, 255 };
    Color brick  = { 150, 80, 70, 255 },  brick2 = { 120, 62, 54, 255 };
    Color stone  = { 130, 134, 140, 255 };

    // 0 grass
    { auto p=cell(0); rect(&img,(int)p.x,(int)p.y,T,T,grass1);
      for(int i=0;i<10;++i) ImageDrawPixel(&img,(int)p.x+ (i*7%T),(int)p.y+(i*11%T),grass2); }
    // 1 path/dirt
    { auto p=cell(1); rect(&img,(int)p.x,(int)p.y,T,T,dirt);
      for(int i=0;i<8;++i) ImageDrawPixel(&img,(int)p.x+(i*9%T),(int)p.y+(i*5%T),dirt2); }
    // 2 water (animated look)
    { auto p=cell(2); rect(&img,(int)p.x,(int)p.y,T,T,water1);
      for(int y=4;y<T;y+=8) rect(&img,(int)p.x,(int)p.y+y,T,3,water2); }
    // 3 tree (on grass)
    { auto p=cell(3); rect(&img,(int)p.x,(int)p.y,T,T,grass1);
      rect(&img,(int)p.x+13,(int)p.y+18,6,12,wood2);            // trunk
      rect(&img,(int)p.x+6,(int)p.y+2,20,18,{56,120,56,255});   // canopy
      rect(&img,(int)p.x+9,(int)p.y+5,14,10,{72,148,72,255}); }
    // 4 brick wall
    { auto p=cell(4); rect(&img,(int)p.x,(int)p.y,T,T,brick);
      for(int y=0;y<T;y+=8) rect(&img,(int)p.x,(int)p.y+y,T,1,brick2);
      for(int y=0;y<T;y+=16) for(int x=0;x<T;x+=16) rect(&img,(int)p.x+x,(int)p.y+y,1,8,brick2);
      for(int y=8;y<T;y+=16) for(int x=8;x<T;x+=16) rect(&img,(int)p.x+x,(int)p.y+y,1,8,brick2); }
    // 5 wood floor
    { auto p=cell(5); rect(&img,(int)p.x,(int)p.y,T,T,wood);
      for(int y=0;y<T;y+=8) rect(&img,(int)p.x,(int)p.y+y,T,1,wood2); }
    // 6 flowers
    { auto p=cell(6); rect(&img,(int)p.x,(int)p.y,T,T,grass1);
      ImageDrawPixel(&img,(int)p.x+8,(int)p.y+10,{230,90,90,255});
      ImageDrawPixel(&img,(int)p.x+20,(int)p.y+18,{240,220,90,255});
      ImageDrawPixel(&img,(int)p.x+14,(int)p.y+24,{220,120,220,255}); }
    // 7 stone floor
    { auto p=cell(7); rect(&img,(int)p.x,(int)p.y,T,T,stone);
      for(int y=0;y<T;y+=16) rect(&img,(int)p.x,(int)p.y+y,T,1,{100,104,110,255}); }
    // 8 darker grass, 9 sand, 10 deep water, 11 roof
    { auto p=cell(8); rect(&img,(int)p.x,(int)p.y,T,T,grass2); }
    { auto p=cell(9); rect(&img,(int)p.x,(int)p.y,T,T,{220,206,150,255}); }
    { auto p=cell(10); rect(&img,(int)p.x,(int)p.y,T,T,{40,86,150,255}); }
    { auto p=cell(11); rect(&img,(int)p.x,(int)p.y,T,T,{180,70,60,255});
      for(int y=0;y<T;y+=6) rect(&img,(int)p.x,(int)p.y+y,T,2,{150,54,46,255}); }

    ExportImage(img, path.c_str());
    UnloadImage(img);
    printf("  wrote %s\n", path.c_str());
}

// ---- Character sheet: 4 frames x 4 directions (Down,Left,Right,Up), 32px ----
static void genCharacter(const std::string& path, Color shirt, Color skin) {
    const int F = 32, FRAMES = 4, DIRS = 4;
    Image img = GenImageColor(FRAMES * F, DIRS * F, BLANK);
    Color hair = { 60, 45, 35, 255 };
    Color pants = { 60, 70, 110, 255 };

    for (int d = 0; d < DIRS; ++d) {
        for (int f = 0; f < FRAMES; ++f) {
            int ox = f * F, oy = d * F;
            int legShift = (f == 1) ? -2 : (f == 3) ? 2 : 0; // simple walk cycle
            // legs
            rect(&img, ox+11, oy+24, 4, 6+ (legShift>0?legShift:0), pants);
            rect(&img, ox+17, oy+24, 4, 6+ (legShift<0?-legShift:0), pants);
            // body
            rect(&img, ox+10, oy+14, 12, 11, shirt);
            // head
            rect(&img, ox+11, oy+5, 10, 9, skin);
            // hair (varies by direction)
            rect(&img, ox+10, oy+4, 12, 4, hair);
            if (d == 3) rect(&img, ox+10, oy+8, 12, 6, hair); // facing up: back of head
            // eyes for down/left/right
            if (d == 0) { ImageDrawPixel(&img,ox+13,oy+10,BLACK); ImageDrawPixel(&img,ox+18,oy+10,BLACK); }
            if (d == 1) { ImageDrawPixel(&img,ox+13,oy+10,BLACK); }
            if (d == 2) { ImageDrawPixel(&img,ox+18,oy+10,BLACK); }
            // arms
            rect(&img, ox+8, oy+15, 2, 7, skin);
            rect(&img, ox+22, oy+15, 2, 7, skin);
        }
    }
    ExportImage(img, path.c_str());
    UnloadImage(img);
    printf("  wrote %s\n", path.c_str());
}

// ---- Enemy sprite: a simple slime / blob (48px) ----
static void genEnemy(const std::string& path, Color body) {
    const int S = 48;
    Image img = GenImageColor(S, S, BLANK);
    rect(&img, 6, 20, 36, 22, body);
    rect(&img, 10, 14, 28, 10, body);
    rect(&img, 14, 26, 4, 4, WHITE);  rect(&img, 30, 26, 4, 4, WHITE);
    rect(&img, 15, 27, 2, 2, BLACK);  rect(&img, 31, 27, 2, 2, BLACK);
    ExportImage(img, path.c_str());
    UnloadImage(img);
    printf("  wrote %s\n", path.c_str());
}

int main(int argc, char** argv) {
    std::string out = (argc > 1) ? argv[1] : ".";
    SetTraceLogLevel(LOG_WARNING);
    printf("Generating assets into %s\n", out.c_str());
    genTileset(out + "/tileset.png");
    genCharacter(out + "/hero.png",  Color{ 80, 140, 220, 255 }, Color{ 240, 200, 160, 255 });
    genCharacter(out + "/npc.png",   Color{ 200, 120, 80, 255 }, Color{ 240, 200, 160, 255 });
    genCharacter(out + "/oldman.png",Color{ 150, 150, 160, 255 }, Color{ 235, 205, 170, 255 });
    genEnemy(out + "/slime.png", Color{ 110, 200, 120, 255 });
    genEnemy(out + "/bat.png",   Color{ 130, 90, 170, 255 });
    return 0;
}
