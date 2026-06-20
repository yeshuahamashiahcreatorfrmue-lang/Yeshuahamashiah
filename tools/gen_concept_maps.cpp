// gen_concept_maps: builds a large themed TILESET (112 tiles) plus 14 concept
// maps (field, battlefield, mountains, city, castle, boss, mine, ruins, sacked
// city, destroyed village, burning mountain, beggars' den, restaurant, inn)
// into a ready-to-play project. Pure raylib Image API + the core Project model.
#include "raylib.h"
#include "project/Project.h"
#include "world/Map.h"
#include "gen/AssetGen.h"
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <filesystem>

using namespace tsukuru;
namespace fs = std::filesystem;

static const int TSC = 8, TSR = 16, T = 32;

// ---- tile indices (row-major, 8 per row) ----
enum {
    F_GRASS,F_GRASS2,F_FLGRASS,F_DIRT,F_PATH,WATER,TREE,CANOPY,                 // 0
    BUSH,ROCK,FLR,FLY,TALLG,STUMP,MUSH,SAND,                                    // 1
    MUD,BLOODMUD,CRATER,SANDBAG,BARRICADE,BROKENGND,BANNER,ASH,                 // 2
    ROCKFLR,CLIFF,SNOW,SNOWROCK,PINE,PINETOP,BOULDER,SCREE,                     // 3
    COBBLE,PAVE,BRICKWALL,WINDOW,DOOR,ROOFTOP,ROOFBOT,LAMP,                     // 4
    FOUNTAIN,STALL,STONEBRICK,BATTLE,TOWER,GATE,PILLAR,TORCH,                   // 5
    DARKFLR,LAVA,LAVACRACK,BONEFLR,MAGIC,SKULL,BRAZIER,OBSIDIAN,                // 6
    CAVEFLR,OREGOLD,OREIRON,ORECRYS,WOODSUP,RAIL,MINECART,RUBBLE,               // 7
    CRACKED,BROKENPILLAR,RUBBLE2,OVERGROWN,BROKENWALL,STATUEFRAG,MOSS,WEEDS,    // 8
    FIRE,CHARRED,BURNTWALL,EMBER,SMOKE,BROKENROOF,ASH2,SCORCH,                  // 9
    WOODFLR,WOODFLR2,RUGRED,RUGBLUE,STONEFLRIN,TILEFLR,STRAW,DIRTFLRIN,         // 10
    TABLE,CHAIR,BED,COUNTER,FIREPLACE,SHELF,POT,SACK,                           // 11
    PLATE,BREAD,MUG,CAULDRON,LANTERN,RUGWORN,CRATEIN,BARRELIN,                  // 12
    WOODWALLIN,WINDOWIN,DOORIN,SIGN,FENCE_H,FENCE_V,WATERDEEP,BRIDGE,           // 13
    THATCH,TILEROOF,BRICKRED,APARTWALL,GLASSWALL,OFFICEWALL,ASPHALT,CONCRETE,   // 14
    TENT,BALLOON,FERRIS,SCAFFOLD,CRANE,TRASH,SLUDGE,PIPE                        // 15
};

static int hsh(int x,int y){ unsigned n=(unsigned)x*374761393u+(unsigned)y*668265263u; n=(n^(n>>13))*1274126177u; return (int)((n>>16)&255u); }
static void R(Image*im,int x,int y,int w,int h,Color c){ ImageDrawRectangle(im,x,y,w,h,c); }
static void P(Image*im,int x,int y,Color c){ ImageDrawPixel(im,x,y,c); }
static void texFill(Image*im,int ox,int oy,Color base,Color lo,Color hi,int loT=46,int hiT=210){
    unsigned char* d=(unsigned char*)im->data; int W=im->width,Hh=im->height;
    for(int j=0;j<T;j++)for(int i=0;i<T;i++){ int X=ox+i,Y=oy+j; if(X<0||Y<0||X>=W||Y>=Hh)continue;
        int v=hsh(X,Y);Color c=base;if(v<loT)c=lo;else if(v>hiT)c=hi;
        long p=((long)Y*W+X)*4; d[p]=c.r;d[p+1]=c.g;d[p+2]=c.b;d[p+3]=255; } }

static Image genTiles(){
    Image im=GenImageColor(TSC*T,TSR*T,BLANK);
    auto cell=[&](int idx,int&ox,int&oy){ox=(idx%TSC)*T;oy=(idx/TSC)*T;};
    int ox,oy;
    Color gB={86,152,72,255},gL={108,176,90,255},gD={66,130,58,255};
    Color woB={160,116,72,255},woD={120,86,52,255},woL={186,140,92,255};
    Color stB={150,150,158,255},stD={108,108,118,255},stL={182,182,190,255};
    Color drkB={54,50,62,255},drkD={38,34,46,255},drkL={74,70,84,255};

    // ---- row 0: FIELD ----
    cell(F_GRASS,ox,oy); texFill(&im,ox,oy,gB,gD,gL);
    cell(F_GRASS2,ox,oy); texFill(&im,ox,oy,gB,gD,{98,166,84,255}); for(int k=0;k<8;k++)P(&im,ox+(hsh(ox+k,oy)%30)+1,oy+(hsh(ox,oy+k*3)%30)+1,{72,138,64,255});
    cell(F_FLGRASS,ox,oy); texFill(&im,ox,oy,gB,gD,gL); P(&im,ox+8,oy+9,{230,90,90,255});P(&im,ox+9,oy+9,{240,210,90,255});P(&im,ox+22,oy+19,{240,210,90,255});P(&im,ox+15,oy+25,{220,120,210,255});
    cell(F_DIRT,ox,oy); texFill(&im,ox,oy,{150,118,78,255},{126,98,64,255},{172,140,96,255});
    cell(F_PATH,ox,oy); texFill(&im,ox,oy,{182,152,104,255},{150,122,80,255},{205,182,140,255});
    cell(WATER,ox,oy); texFill(&im,ox,oy,{70,120,205,255},{48,96,180,255},{70,120,205,255}); for(int y=3;y<T;y+=8)R(&im,ox+2,oy+y,T-6,2,{120,170,235,255});
    cell(TREE,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+13,oy+12,6,18,{120,84,52,255}); R(&im,ox+13,oy+12,2,18,{96,64,40,255});
    cell(CANOPY,ox,oy); R(&im,ox+3,oy+5,26,24,{56,118,56,255}); R(&im,ox+6,oy+3,20,12,{74,148,72,255}); R(&im,ox+10,oy+8,14,8,{96,178,96,255});
    // ---- row 1 ----
    cell(BUSH,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+6,oy+12,20,14,{60,124,58,255}); R(&im,ox+9,oy+9,14,8,{80,150,76,255});
    cell(ROCK,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+8,oy+14,16,12,{140,140,148,255}); R(&im,ox+11,oy+11,10,6,{168,168,176,255});
    cell(FLR,ox,oy); texFill(&im,ox,oy,gB,gD,gL); int sp[3][2]={{9,12},{18,9},{14,20}};for(auto&s:sp){R(&im,ox+s[0],oy+s[1],3,3,{225,80,80,255});P(&im,ox+s[0]+1,oy+s[1]+1,{255,250,210,255});}
    cell(FLY,ox,oy); texFill(&im,ox,oy,gB,gD,gL); for(auto&s:sp){R(&im,ox+s[0],oy+s[1],3,3,{245,210,90,255});P(&im,ox+s[0]+1,oy+s[1]+1,{255,255,220,255});}
    cell(TALLG,ox,oy); for(int k=0;k<14;k++){int gx=ox+2+(k*7)%28;int gh=8+(hsh(gx,oy)%10);R(&im,gx,oy+T-gh,2,gh,{72,140,66,255});}
    cell(STUMP,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+9,oy+12,14,12,{132,94,58,255}); R(&im,ox+11,oy+12,10,4,{168,128,84,255});
    cell(MUSH,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+13,oy+16,3,8,{230,220,200,255}); R(&im,ox+9,oy+11,12,7,{210,80,70,255});
    cell(SAND,ox,oy); texFill(&im,ox,oy,{222,206,150,255},{200,184,128,255},{236,222,170,255});
    // ---- row 2: WAR ----
    Color mud={92,72,52,255};
    cell(MUD,ox,oy); texFill(&im,ox,oy,mud,{72,56,40,255},{112,90,64,255});
    cell(BLOODMUD,ox,oy); texFill(&im,ox,oy,mud,{72,56,40,255},{112,90,64,255}); for(int k=0;k<5;k++)R(&im,ox+2+(hsh(ox+k,oy)%24),oy+2+(hsh(ox,oy+k)%24),3,2,{120,30,28,255});
    cell(CRATER,ox,oy); texFill(&im,ox,oy,mud,{60,46,34,255},{100,80,58,255}); ImageDrawCircle(&im,ox+16,oy+16,11,{50,38,28,255}); ImageDrawCircle(&im,ox+16,oy+16,6,{36,28,20,255});
    cell(SANDBAG,ox,oy); texFill(&im,ox,oy,mud,{72,56,40,255},{100,80,58,255}); for(int r=0;r<3;r++)for(int c=0;c<3;c++)R(&im,ox+3+c*9,oy+8+r*7,8,6,{150,138,96,255});
    cell(BARRICADE,ox,oy); texFill(&im,ox,oy,mud,{72,56,40,255},{100,80,58,255}); R(&im,ox+2,oy+14,28,5,woB); R(&im,ox+6,oy+6,4,20,woD); R(&im,ox+22,oy+6,4,20,woD);
    cell(BROKENGND,ox,oy); texFill(&im,ox,oy,mud,{60,46,34,255},{100,80,58,255}); R(&im,ox+4,oy+6,3,20,{40,30,22,255}); R(&im,ox+16,oy+10,3,16,{40,30,22,255});
    cell(BANNER,ox,oy); texFill(&im,ox,oy,mud,{72,56,40,255},{100,80,58,255}); R(&im,ox+14,oy+2,3,28,woD); R(&im,ox+8,oy+4,8,14,{180,50,46,255}); P(&im,ox+11,oy+10,{240,210,90,255});
    cell(ASH,ox,oy); texFill(&im,ox,oy,{96,92,96,255},{74,70,74,255},{120,116,120,255});
    // ---- row 3: MOUNTAIN ----
    cell(ROCKFLR,ox,oy); texFill(&im,ox,oy,{130,126,128,255},{104,100,104,255},{156,152,154,255});
    cell(CLIFF,ox,oy); texFill(&im,ox,oy,{96,92,96,255},{70,66,70,255},{122,118,122,255}); R(&im,ox,oy,T,3,{60,56,60,255}); R(&im,ox,oy+T-4,T,4,{52,48,52,255});
    cell(SNOW,ox,oy); texFill(&im,ox,oy,{226,232,240,255},{200,210,224,255},{244,248,255,255});
    cell(SNOWROCK,ox,oy); texFill(&im,ox,oy,{110,108,116,255},{84,82,90,255},{140,138,146,255}); R(&im,ox,oy,T,7,{232,238,246,255});
    cell(PINE,ox,oy); texFill(&im,ox,oy,{120,126,120,255},{96,102,96,255},{146,152,146,255}); R(&im,ox+14,oy+18,4,12,{96,64,40,255});
    cell(PINETOP,ox,oy); for(int r=0;r<3;r++){int wd=8+r*7; R(&im,ox+16-wd/2,oy+4+r*8,wd,8,{36,92,52,255}); R(&im,ox+16-wd/2+2,oy+4+r*8,wd-4,4,{52,116,68,255});}
    cell(BOULDER,ox,oy); texFill(&im,ox,oy,{130,126,128,255},{104,100,104,255},{156,152,154,255}); ImageDrawCircle(&im,ox+16,oy+18,11,{116,112,116,255}); ImageDrawCircle(&im,ox+13,oy+15,4,{150,146,150,255});
    cell(SCREE,ox,oy); texFill(&im,ox,oy,{138,132,128,255},{110,104,100,255},{162,156,152,255}); for(int k=0;k<10;k++)R(&im,ox+(hsh(ox+k,oy)%28),oy+(hsh(ox,oy+k)%28),3,2,{96,92,90,255});
    // ---- row 4: CITY ----
    Color cB={152,152,160,255},cD={112,112,122,255},cL={178,178,186,255};
    cell(COBBLE,ox,oy); texFill(&im,ox,oy,cB,cD,cL); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,cD); for(int x=0;x<T;x+=8)R(&im,ox+x,oy,1,T,cD);
    cell(PAVE,ox,oy); texFill(&im,ox,oy,{170,166,170,255},{146,142,146,255},{192,188,192,255});
    Color waB={200,184,152,255},waL={150,134,108,255};
    cell(BRICKWALL,ox,oy); R(&im,ox,oy,T,T,waB); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,waL); for(int y=0;y<T;y+=16)for(int x=0;x<T;x+=16)R(&im,ox+x,oy+y,1,8,waL); for(int y=8;y<T;y+=16)for(int x=8;x<T;x+=16)R(&im,ox+x,oy+y,1,8,waL);
    cell(WINDOW,ox,oy); R(&im,ox,oy,T,T,waB); R(&im,ox+6,oy+7,20,18,{110,90,60,255}); R(&im,ox+8,oy+9,16,14,{130,190,225,255}); R(&im,ox+15,oy+9,2,14,{110,90,60,255});
    cell(DOOR,ox,oy); R(&im,ox,oy,T,T,waB); R(&im,ox+7,oy+5,18,27,{120,82,48,255}); R(&im,ox+9,oy+7,14,25,{96,62,36,255}); P(&im,ox+21,oy+19,{230,200,90,255});
    cell(ROOFTOP,ox,oy); R(&im,ox,oy,T,T,{184,76,66,255}); R(&im,ox,oy,T,4,{120,44,40,255}); for(int y=5;y<T;y+=6)R(&im,ox,oy+y,T,3,{146,54,48,255});
    cell(ROOFBOT,ox,oy); R(&im,ox,oy,T,T,{184,76,66,255}); for(int y=0;y<T;y+=6)R(&im,ox,oy+y,T,3,{146,54,48,255});
    cell(LAMP,ox,oy); texFill(&im,ox,oy,cB,cD,cL); R(&im,ox+14,oy+8,4,22,{80,70,60,255}); R(&im,ox+11,oy+4,10,8,{255,230,140,255});
    // ---- row 5: CITY2 / CASTLE ----
    cell(FOUNTAIN,ox,oy); texFill(&im,ox,oy,cB,cD,cL); ImageDrawCircle(&im,ox+16,oy+16,13,{120,120,128,255}); ImageDrawCircle(&im,ox+16,oy+16,9,{90,150,210,255}); R(&im,ox+15,oy+8,2,8,{170,170,178,255});
    cell(STALL,ox,oy); texFill(&im,ox,oy,cB,cD,cL); R(&im,ox+3,oy+6,26,6,{200,70,70,255}); for(int x=0;x<26;x+=6)R(&im,ox+3+x,oy+6,3,6,{240,240,240,255}); R(&im,ox+5,oy+12,22,14,{150,108,64,255});
    cell(STONEBRICK,ox,oy); texFill(&im,ox,oy,stB,stD,stL); for(int y=0;y<T;y+=10)R(&im,ox,oy+y,T,1,stD); for(int x=0;x<T;x+=12)R(&im,ox+x,oy,1,T,stD);
    cell(BATTLE,ox,oy); texFill(&im,ox,oy,stB,stD,stL); R(&im,ox,oy,T,8,stD); for(int x=0;x<T;x+=10)R(&im,ox+x,oy,6,5,{60,58,64,255});
    cell(TOWER,ox,oy); texFill(&im,ox,oy,stD,{84,84,92,255},stB); R(&im,ox+4,oy+2,24,28,stB); R(&im,ox+4,oy+2,24,5,stD); for(int x=6;x<28;x+=8)R(&im,ox+x,oy+12,4,6,{60,58,64,255});
    cell(GATE,ox,oy); texFill(&im,ox,oy,stB,stD,stL); R(&im,ox+5,oy+4,22,26,{70,50,34,255}); R(&im,ox+8,oy+8,16,22,{120,82,48,255}); for(int y=8;y<30;y+=5)R(&im,ox+8,oy+y,16,1,{70,50,34,255});
    cell(PILLAR,ox,oy); texFill(&im,ox,oy,stB,stD,stL); R(&im,ox+10,oy+2,12,28,{196,196,204,255}); R(&im,ox+8,oy+2,16,4,stL); R(&im,ox+8,oy+26,16,4,stL);
    cell(TORCH,ox,oy); texFill(&im,ox,oy,stB,stD,stL); R(&im,ox+14,oy+12,4,16,{90,64,40,255}); R(&im,ox+12,oy+5,8,8,{255,170,60,255}); R(&im,ox+13,oy+4,6,5,{255,230,140,255});
    // ---- row 6: BOSS ----
    cell(DARKFLR,ox,oy); texFill(&im,ox,oy,drkB,drkD,drkL);
    cell(LAVA,ox,oy); texFill(&im,ox,oy,{210,80,30,255},{150,40,20,255},{255,180,60,255}); for(int y=4;y<T;y+=9)R(&im,ox+2,oy+y,T-4,2,{255,210,90,255});
    cell(LAVACRACK,ox,oy); texFill(&im,ox,oy,drkB,drkD,drkL); R(&im,ox+5,oy+4,2,24,{230,90,30,255}); R(&im,ox+5,oy+15,18,2,{230,90,30,255}); R(&im,ox+20,oy+6,2,20,{255,150,50,255});
    cell(BONEFLR,ox,oy); texFill(&im,ox,oy,{198,192,176,255},{170,164,148,255},{220,214,198,255}); for(int k=0;k<4;k++)R(&im,ox+3+(hsh(ox+k,oy)%22),oy+5+(hsh(ox,oy+k)%20),7,2,{150,144,128,255});
    cell(MAGIC,ox,oy); texFill(&im,ox,oy,drkB,drkD,drkL); ImageDrawCircleLines(&im,ox+16,oy+16,13,{150,90,230,255}); ImageDrawCircleLines(&im,ox+16,oy+16,8,{180,130,240,255});
    cell(SKULL,ox,oy); texFill(&im,ox,oy,drkB,drkD,drkL); R(&im,ox+10,oy+9,12,11,{220,214,198,255}); R(&im,ox+12,oy+13,2,3,{30,28,34,255}); R(&im,ox+18,oy+13,2,3,{30,28,34,255}); R(&im,ox+11,oy+20,10,3,{220,214,198,255});
    cell(BRAZIER,ox,oy); texFill(&im,ox,oy,drkB,drkD,drkL); R(&im,ox+10,oy+16,12,12,{70,66,72,255}); R(&im,ox+11,oy+8,10,8,{255,150,50,255}); R(&im,ox+13,oy+5,6,6,{255,225,130,255});
    cell(OBSIDIAN,ox,oy); texFill(&im,ox,oy,drkD,{26,24,32,255},drkB); R(&im,ox+9,oy+2,14,28,{40,36,50,255}); R(&im,ox+9,oy+2,5,28,{58,52,70,255});
    // ---- row 7: MINE ----
    Color caB={92,84,78,255},caD={68,62,58,255},caL={116,106,98,255};
    cell(CAVEFLR,ox,oy); texFill(&im,ox,oy,caB,caD,caL);
    cell(OREGOLD,ox,oy); texFill(&im,ox,oy,caB,caD,caL); for(int k=0;k<5;k++)R(&im,ox+5+(hsh(ox+k,oy)%20),oy+5+(hsh(ox,oy+k)%20),3,3,{245,205,70,255});
    cell(OREIRON,ox,oy); texFill(&im,ox,oy,caB,caD,caL); for(int k=0;k<5;k++)R(&im,ox+5+(hsh(ox+k,oy)%20),oy+5+(hsh(ox,oy+k)%20),3,3,{170,120,90,255});
    cell(ORECRYS,ox,oy); texFill(&im,ox,oy,caB,caD,caL); for(int k=0;k<4;k++)R(&im,ox+6+(hsh(ox+k,oy)%18),oy+6+(hsh(ox,oy+k)%18),3,5,{120,210,230,255});
    cell(WOODSUP,ox,oy); texFill(&im,ox,oy,caB,caD,caL); R(&im,ox+3,oy,5,T,woB); R(&im,ox+24,oy,5,T,woB); R(&im,ox,oy+2,T,5,woD);
    cell(RAIL,ox,oy); texFill(&im,ox,oy,caB,caD,caL); R(&im,ox+8,oy,3,T,{120,116,120,255}); R(&im,ox+21,oy,3,T,{120,116,120,255}); for(int y=2;y<T;y+=8)R(&im,ox+6,oy+y,20,2,{90,66,44,255});
    cell(MINECART,ox,oy); texFill(&im,ox,oy,caB,caD,caL); R(&im,ox+6,oy+12,20,12,{90,66,44,255}); R(&im,ox+8,oy+10,16,5,{140,140,148,255}); ImageDrawCircle(&im,ox+11,oy+26,3,{40,38,40,255}); ImageDrawCircle(&im,ox+21,oy+26,3,{40,38,40,255});
    cell(RUBBLE,ox,oy); texFill(&im,ox,oy,caB,caD,caL); for(int k=0;k<8;k++)R(&im,ox+(hsh(ox+k,oy)%26),oy+(hsh(ox,oy+k)%26),5,4,{100,94,88,255});
    // ---- row 8: RUINS ----
    cell(CRACKED,ox,oy); texFill(&im,ox,oy,stB,stD,stL); R(&im,ox+6,oy+3,2,26,{80,78,84,255}); R(&im,ox+6,oy+16,16,2,{80,78,84,255});
    cell(BROKENPILLAR,ox,oy); texFill(&im,ox,oy,{120,150,110,255},{96,124,90,255},stL); R(&im,ox+11,oy+12,12,18,{190,190,198,255}); R(&im,ox+9,oy+26,16,4,stD);
    cell(RUBBLE2,ox,oy); texFill(&im,ox,oy,stB,stD,stL); for(int k=0;k<9;k++)R(&im,ox+(hsh(ox+k,oy)%26),oy+(hsh(ox,oy+k)%26),5,4,{150,148,154,255});
    cell(OVERGROWN,ox,oy); texFill(&im,ox,oy,stB,stD,stL); for(int k=0;k<10;k++)P(&im,ox+(hsh(ox+k,oy)%30),oy+(hsh(ox,oy+k)%30),{72,138,64,255}); R(&im,ox+4,oy+20,8,6,{70,130,64,255});
    cell(BROKENWALL,ox,oy); R(&im,ox,oy+8,T,T-8,waB); for(int y=8;y<T;y+=8)R(&im,ox,oy+y,T,1,waL); R(&im,ox+18,oy+8,14,8,{0,0,0,0});
    cell(STATUEFRAG,ox,oy); texFill(&im,ox,oy,stB,stD,stL); R(&im,ox+10,oy+14,12,16,{180,180,188,255}); R(&im,ox+8,oy+28,16,2,stD);
    cell(MOSS,ox,oy); texFill(&im,ox,oy,{96,128,88,255},{74,108,70,255},{120,152,108,255});
    cell(WEEDS,ox,oy); texFill(&im,ox,oy,stB,stD,stL); for(int k=0;k<10;k++){int gx=ox+2+(k*7)%28;R(&im,gx,oy+T-9,2,9,{96,138,80,255});}
    // ---- row 9: FIRE / DESTRUCTION ----
    cell(FIRE,ox,oy); texFill(&im,ox,oy,{60,40,30,255},{40,26,20,255},{86,56,40,255}); R(&im,ox+9,oy+10,14,16,{240,120,40,255}); R(&im,ox+12,oy+6,8,16,{255,190,70,255}); R(&im,ox+14,oy+4,4,10,{255,240,160,255});
    cell(CHARRED,ox,oy); texFill(&im,ox,oy,{58,52,52,255},{40,36,36,255},{78,70,70,255});
    cell(BURNTWALL,ox,oy); R(&im,ox,oy,T,T,{72,64,60,255}); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,{48,42,40,255}); R(&im,ox+18,oy+2,12,10,{0,0,0,0});
    cell(EMBER,ox,oy); texFill(&im,ox,oy,{58,52,52,255},{40,36,36,255},{78,70,70,255}); for(int k=0;k<6;k++)P(&im,ox+4+(hsh(ox+k,oy)%24),oy+4+(hsh(ox,oy+k)%24),{255,150,50,255});
    cell(SMOKE,ox,oy); for(int k=0;k<40;k++){int sx=ox+(hsh(ox+k,oy)%30),sy=oy+(hsh(ox,oy+k)%30); P(&im,sx,sy,{120,116,120,160});}
    cell(BROKENROOF,ox,oy); R(&im,ox,oy,T,T,{146,54,48,255}); for(int y=0;y<T;y+=6)R(&im,ox,oy+y,T,3,{110,40,36,255}); R(&im,ox+16,oy+10,14,16,{0,0,0,0});
    cell(ASH2,ox,oy); texFill(&im,ox,oy,{108,104,108,255},{84,80,84,255},{132,128,132,255});
    cell(SCORCH,ox,oy); texFill(&im,ox,oy,{70,56,46,255},{48,38,32,255},{96,78,64,255}); ImageDrawCircle(&im,ox+16,oy+16,10,{44,34,28,255});
    // ---- row 10: INTERIOR FLOORS ----
    cell(WOODFLR,ox,oy); texFill(&im,ox,oy,woB,woD,woL); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,woD);
    cell(WOODFLR2,ox,oy); texFill(&im,ox,oy,{150,108,68,255},woD,{176,132,86,255}); for(int x=0;x<T;x+=8)R(&im,ox+x,oy,1,T,woD);
    cell(RUGRED,ox,oy); R(&im,ox,oy,T,T,{160,56,52,255}); R(&im,ox+3,oy+3,T-6,T-6,{200,80,72,255}); R(&im,ox+6,oy+6,T-12,T-12,{160,56,52,255});
    cell(RUGBLUE,ox,oy); R(&im,ox,oy,T,T,{56,76,150,255}); R(&im,ox+3,oy+3,T-6,T-6,{84,108,190,255}); R(&im,ox+6,oy+6,T-12,T-12,{56,76,150,255});
    cell(STONEFLRIN,ox,oy); texFill(&im,ox,oy,stB,stD,stL); for(int y=0;y<T;y+=11)R(&im,ox,oy+y,T,1,stD);
    cell(TILEFLR,ox,oy); for(int y=0;y<T;y+=8)for(int x=0;x<T;x+=8){bool a=((x/8)+(y/8))%2; R(&im,ox+x,oy+y,8,8,a?Color{206,200,190,255}:Color{170,162,150,255});}
    cell(STRAW,ox,oy); texFill(&im,ox,oy,{196,170,96,255},{170,144,76,255},{216,192,120,255}); for(int k=0;k<10;k++)R(&im,ox+(hsh(ox+k,oy)%28),oy+(hsh(ox,oy+k)%28),5,1,{150,126,68,255});
    cell(DIRTFLRIN,ox,oy); texFill(&im,ox,oy,{120,98,72,255},{98,80,58,255},{142,118,88,255});
    // ---- row 11: FURNITURE ----
    cell(TABLE,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+4,oy+8,24,16,{150,108,64,255}); R(&im,ox+4,oy+8,24,3,{176,132,86,255}); R(&im,ox+6,oy+22,3,8,woD); R(&im,ox+23,oy+22,3,8,woD);
    cell(CHAIR,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+10,oy+12,12,12,{150,108,64,255}); R(&im,ox+10,oy+6,12,7,{132,94,56,255}); R(&im,ox+11,oy+22,2,7,woD); R(&im,ox+19,oy+22,2,7,woD);
    cell(BED,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+4,oy+4,24,24,{120,84,52,255}); R(&im,ox+6,oy+12,20,14,{210,210,224,255}); R(&im,ox+6,oy+6,20,7,{200,80,76,255});
    cell(COUNTER,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox,oy+10,T,18,{132,94,56,255}); R(&im,ox,oy+10,T,4,{168,128,84,255});
    cell(FIREPLACE,ox,oy); R(&im,ox,oy,T,T,{96,90,86,255}); R(&im,ox+5,oy+10,22,20,{40,36,34,255}); R(&im,ox+9,oy+18,14,10,{240,120,40,255}); R(&im,ox+12,oy+16,8,8,{255,200,90,255});
    cell(SHELF,ox,oy); texFill(&im,ox,oy,woD,{100,72,44,255},woB); R(&im,ox+3,oy+2,26,28,{140,100,60,255}); for(int y=8;y<T;y+=9)R(&im,ox+3,oy+y,26,2,{100,72,44,255}); R(&im,ox+6,oy+3,5,5,{200,80,72,255}); R(&im,ox+18,oy+12,5,5,{84,108,190,255});
    cell(POT,ox,oy); texFill(&im,ox,oy,woB,woD,woL); ImageDrawCircle(&im,ox+16,oy+18,8,{120,90,70,255}); R(&im,ox+12,oy+9,8,4,{96,72,56,255});
    cell(SACK,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+9,oy+12,14,16,{180,160,110,255}); R(&im,ox+12,oy+8,8,6,{160,140,94,255});
    // ---- row 12: FOOD / INN ----
    cell(PLATE,ox,oy); texFill(&im,ox,oy,woB,woD,woL); ImageDrawCircle(&im,ox+16,oy+16,9,{225,225,230,255}); ImageDrawCircle(&im,ox+16,oy+16,5,{180,120,70,255});
    cell(BREAD,ox,oy); texFill(&im,ox,oy,woB,woD,woL); ImageDrawCircle(&im,ox+16,oy+16,8,{196,146,80,255}); R(&im,ox+12,oy+15,8,2,{150,108,60,255});
    cell(MUG,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+11,oy+10,9,14,{150,150,158,255}); R(&im,ox+11,oy+10,9,4,{210,190,120,255}); R(&im,ox+20,oy+13,3,6,{120,120,128,255});
    cell(CAULDRON,ox,oy); texFill(&im,ox,oy,{96,90,86,255},{72,68,64,255},{116,110,106,255}); ImageDrawCircle(&im,ox+16,oy+18,10,{40,38,40,255}); R(&im,ox+8,oy+10,16,4,{60,56,58,255}); R(&im,ox+10,oy+14,12,4,{90,150,90,255});
    cell(LANTERN,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+11,oy+6,10,16,{90,70,40,255}); R(&im,ox+13,oy+9,6,9,{255,225,130,255});
    cell(RUGWORN,ox,oy); R(&im,ox,oy,T,T,{120,86,72,255}); R(&im,ox+3,oy+3,T-6,T-6,{146,108,90,255}); for(int k=0;k<5;k++)P(&im,ox+5+(hsh(ox+k,oy)%22),oy+5+(hsh(ox,oy+k)%22),{90,64,52,255});
    cell(CRATEIN,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+6,oy+6,20,20,{170,128,76,255}); R(&im,ox+6,oy+6,20,2,woD);R(&im,ox+6,oy+24,20,2,woD);R(&im,ox+6,oy+6,2,20,woD);R(&im,ox+24,oy+6,2,20,woD);
    cell(BARRELIN,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox+8,oy+6,16,22,{150,108,64,255}); R(&im,ox+8,oy+10,16,2,woD);R(&im,ox+8,oy+20,16,2,woD);
    // ---- row 13: WALLS / MISC ----
    cell(WOODWALLIN,ox,oy); R(&im,ox,oy,T,T,{120,86,52,255}); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,2,{96,68,42,255}); for(int x=0;x<T;x+=10)R(&im,ox+x,oy,2,T,{104,74,46,255});
    cell(WINDOWIN,ox,oy); R(&im,ox,oy,T,T,{120,86,52,255}); R(&im,ox+6,oy+7,20,16,{60,44,30,255}); R(&im,ox+8,oy+9,16,12,{120,160,200,255}); R(&im,ox+15,oy+9,2,12,{60,44,30,255});
    cell(DOORIN,ox,oy); R(&im,ox,oy,T,T,{120,86,52,255}); R(&im,ox+7,oy+5,18,27,{150,108,64,255}); R(&im,ox+9,oy+7,14,25,{120,84,48,255}); P(&im,ox+21,oy+19,{240,210,90,255});
    cell(SIGN,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+14,oy+12,4,16,woD); R(&im,ox+7,oy+6,18,11,{170,130,80,255}); R(&im,ox+9,oy+9,14,1,woD);R(&im,ox+9,oy+12,10,1,woD);
    cell(FENCE_H,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox,oy+12,T,5,{156,116,74,255}); R(&im,ox+4,oy+8,4,16,{156,116,74,255}); R(&im,ox+24,oy+8,4,16,{156,116,74,255});
    cell(FENCE_V,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+12,oy,5,T,{156,116,74,255}); R(&im,ox+8,oy+4,16,4,{156,116,74,255}); R(&im,ox+8,oy+24,16,4,{156,116,74,255});
    cell(WATERDEEP,ox,oy); texFill(&im,ox,oy,{40,80,160,255},{28,60,130,255},{60,104,190,255});
    cell(BRIDGE,ox,oy); texFill(&im,ox,oy,woB,woD,woL); R(&im,ox,oy,T,2,woD);R(&im,ox,oy+T-2,T,2,woD); for(int x=0;x<T;x+=8)R(&im,ox+x,oy,1,T,woD);
    // ---- row 14: BUILDINGS / SURFACES ----
    cell(THATCH,ox,oy); R(&im,ox,oy,T,T,{182,150,86,255}); for(int y=0;y<T;y+=5)R(&im,ox,oy+y,T,2,{150,120,64,255}); for(int x=0;x<T;x+=6)R(&im,ox+x,oy,2,T,{198,168,104,255});
    cell(TILEROOF,ox,oy); R(&im,ox,oy,T,T,{70,86,110,255}); for(int y=0;y<T;y+=6)for(int x=0;x<T;x+=8)ImageDrawCircle(&im,ox+x+4,oy+y+3,4,{96,116,144,255});
    cell(BRICKRED,ox,oy); R(&im,ox,oy,T,T,{168,82,66,255}); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,{120,56,46,255}); for(int y=0;y<T;y+=16)for(int x=0;x<T;x+=16)R(&im,ox+x,oy+y,1,8,{120,56,46,255}); for(int y=8;y<T;y+=16)for(int x=8;x<T;x+=16)R(&im,ox+x,oy+y,1,8,{120,56,46,255});
    cell(APARTWALL,ox,oy); R(&im,ox,oy,T,T,{172,168,160,255}); for(int wy=4;wy<T-4;wy+=10)for(int wx=4;wx<T-4;wx+=10){R(&im,ox+wx,oy+wy,6,6,{120,150,180,255});R(&im,ox+wx,oy+wy,6,1,{90,110,140,255});}
    cell(GLASSWALL,ox,oy); R(&im,ox,oy,T,T,{120,150,170,255}); for(int y=0;y<T;y+=6)R(&im,ox,oy+y,T,1,{90,120,140,255}); for(int x=0;x<T;x+=6)R(&im,ox+x,oy,1,T,{90,120,140,255}); R(&im,ox+2,oy+2,6,10,{180,210,230,255});
    cell(OFFICEWALL,ox,oy); R(&im,ox,oy,T,T,{210,206,198,255}); R(&im,ox,oy+T-6,T,6,{150,146,140,255}); R(&im,ox,oy,T,2,{180,176,170,255});
    cell(ASPHALT,ox,oy); texFill(&im,ox,oy,{74,74,80,255},{60,60,66,255},{90,90,96,255}); R(&im,ox+14,oy,4,T,{0,0,0,0}); for(int y=2;y<T;y+=10)R(&im,ox+14,oy+y,4,5,{200,196,120,255});
    cell(CONCRETE,ox,oy); texFill(&im,ox,oy,{172,170,168,255},{150,148,146,255},{190,188,186,255});
    // ---- row 15: FESTIVAL / CONSTRUCTION / WASTE ----
    cell(TENT,ox,oy); R(&im,ox+3,oy+12,26,16,{220,220,225,255}); for(int x=0;x<26;x+=6)R(&im,ox+3+x,oy+12,3,16,{210,80,80,255}); ImageDrawTriangle(&im,{(float)ox+16,(float)oy+2},{(float)ox+3,(float)oy+13},{(float)ox+29,(float)oy+13},{200,70,70,255});
    cell(BALLOON,ox,oy); texFill(&im,ox,oy,{170,166,170,255},{146,142,146,255},{192,188,192,255}); ImageDrawCircle(&im,ox+10,oy+9,6,{220,80,80,255}); ImageDrawCircle(&im,ox+20,oy+11,6,{90,150,220,255}); ImageDrawCircle(&im,ox+15,oy+6,5,{240,210,90,255});
    cell(FERRIS,ox,oy); ImageDrawCircleLines(&im,ox+16,oy+15,13,{210,210,220,255}); ImageDrawCircleLines(&im,ox+16,oy+15,7,{180,180,190,255}); R(&im,ox+15,oy+15,2,14,{150,150,160,255}); for(int a=0;a<8;a++){float an=a*0.785f; int px=ox+16+(int)(13*cos(an)),py=oy+15+(int)(13*sin(an)); R(&im,px-2,py-2,4,4,a%2?Color{220,80,80,255}:Color{90,150,220,255});}
    cell(SCAFFOLD,ox,oy); texFill(&im,ox,oy,{120,98,72,255},{98,80,58,255},{142,118,88,255}); for(int x=2;x<T;x+=10)R(&im,ox+x,oy,3,T,{180,150,90,255}); for(int y=2;y<T;y+=10)R(&im,ox,oy+y,T,3,{180,150,90,255});
    cell(CRANE,ox,oy); texFill(&im,ox,oy,{74,74,80,255},{60,60,66,255},{90,90,96,255}); R(&im,ox+13,oy+2,5,28,{230,190,60,255}); R(&im,ox+13,oy+4,18,4,{230,190,60,255}); R(&im,ox+29,oy+8,2,8,{120,120,128,255});
    cell(TRASH,ox,oy); texFill(&im,ox,oy,{86,80,72,255},{66,60,54,255},{104,98,90,255}); R(&im,ox+6,oy+16,8,8,{120,150,90,255}); R(&im,ox+16,oy+12,9,12,{160,120,80,255}); R(&im,ox+10,oy+10,6,6,{90,110,150,255}); P(&im,ox+20,oy+18,{220,210,200,255});
    cell(SLUDGE,ox,oy); texFill(&im,ox,oy,{90,110,60,255},{68,86,44,255},{112,134,78,255}); for(int y=4;y<T;y+=9)R(&im,ox+2,oy+y,T-4,2,{130,154,86,255});
    cell(PIPE,ox,oy); texFill(&im,ox,oy,{96,94,98,255},{74,72,76,255},{120,118,122,255}); R(&im,ox,oy+9,T,14,{130,128,134,255}); R(&im,ox,oy+9,T,3,{160,158,164,255}); R(&im,ox,oy+20,T,3,{84,82,86,255});
    return im;
}

static bool isSolid(int t){
    switch(t){ case WATER: case WATERDEEP: case TREE: case ROCK: case CLIFF: case SNOWROCK:
        case PINE: case BOULDER: case BRICKWALL: case WINDOW: case FOUNTAIN: case STALL:
        case STONEBRICK: case BATTLE: case TOWER: case PILLAR: case OBSIDIAN: case WOODSUP:
        case MINECART: case BROKENPILLAR: case BROKENWALL: case STATUEFRAG: case BURNTWALL:
        case TABLE: case BED: case COUNTER: case FIREPLACE: case SHELF: case CAULDRON:
        case CRATEIN: case BARRELIN: case WOODWALLIN: case WINDOWIN: case FENCE_H: case FENCE_V:
        case LAVA: case SANDBAG: case BARRICADE: case ROOFTOP: case BROKENROOF:
        case BRICKRED: case APARTWALL: case GLASSWALL: case OFFICEWALL:
        case TENT: case FERRIS: case SCAFFOLD: case CRANE: case TRASH: case SLUDGE: case PIPE:
            return true; }
    return false;
}

static void saveImg(Image im,const std::string&p){ ExportImage(im,p.c_str()); UnloadImage(im); }

int main(int argc,char**argv){
    // Default: ADD the concept pack into the bundled demo project so the maps are
    // reachable on a plain launch. Pass a fresh dir to build a standalone pack.
    std::string out = argc>1?argv[1]:"projects/sample_rpg";
    SetTraceLogLevel(LOG_WARNING);
    std::error_code ec;
    fs::create_directories(out+"/assets", ec);
    fs::create_directories(out+"/maps", ec);

    std::shared_ptr<Project> p;
    bool append = fs::exists(fs::path(out)/"project.json", ec);
    if (append) { p = std::make_shared<Project>(); if (!p->load(out)) { append = false; } }
    if (!append) p = Project::createNew(out, "컨셉 맵 팩");

    // idempotency guard: if the pack's tileset is already registered, do nothing.
    for (const auto& a : p->assets.all())
        if (a.name == "concept_tiles") { printf("concept pack already present in %s — skipping\n", out.c_str()); return 0; }

    saveImg(genTiles(), out+"/assets/concept_tiles.png");
    saveImg(gen::characterSheet({200,120,80,255},{240,200,160,255}), out+"/assets/concept_npc.png");
    int A_tiles = p->assets.addExisting(AssetType::Image,"concept_tiles","assets/concept_tiles.png");
    int A_npc   = p->assets.addExisting(AssetType::Image,"concept_npc","assets/concept_npc.png");
    if (!append) {                                  // standalone pack needs a player sprite
        saveImg(gen::characterSheet({80,140,220,255},{240,200,160,255},2), out+"/assets/hero.png");
        int A_hero = p->assets.addExisting(AssetType::Image,"hero","assets/hero.png");
        p->playerSprite = A_hero; p->playerFrames = 4; p->playerAtkFrames = 2;
    }

    auto& rng = *new unsigned(1234567);
    auto rnd = [&](int n){ rng = rng*1103515245u + 12345u; return (int)((rng>>16) % (unsigned)(n>0?n:1)); };

    // map-build helpers ------------------------------------------------------
    auto setC = [&](Map& m,int layer,int x,int y,int t){
        if(!m.tilemap.inBounds(x,y)) return;
        m.tilemap.setTile(layer,x,y,t);
        if(layer<2 && isSolid(t)) m.tilemap.setBlocked(x,y,true);
    };
    auto baseFill = [&](Map& m,int a,int b){
        for(int y=0;y<m.tilemap.height();y++)for(int x=0;x<m.tilemap.width();x++)
            m.tilemap.setTile(0,x,y,(hsh(x,y)%5==0)?b:a);
    };
    auto patch = [&](Map& m,int t,int n,int sz){            // random blobs on ground
        int w=m.tilemap.width(),h=m.tilemap.height();
        for(int i=0;i<n;i++){int cx=1+rnd(w-2),cy=1+rnd(h-2);
            for(int dy=-sz;dy<=sz;dy++)for(int dx=-sz;dx<=sz;dx++)
                if(rnd(3)&&abs(dx)+abs(dy)<=sz) setC(m,0,cx+dx,cy+dy,t);}
    };
    auto scatter = [&](Map& m,int layer,int t,int n){       // random props
        int w=m.tilemap.width(),h=m.tilemap.height();
        for(int i=0;i<n;i++){int x=1+rnd(w-2),y=1+rnd(h-2);
            if(!m.tilemap.blocked(x,y)) setC(m,layer,x,y,t);}
    };
    auto border = [&](Map& m,int t){
        int w=m.tilemap.width(),h=m.tilemap.height();
        for(int x=0;x<w;x++){setC(m,0,x,0,t);setC(m,0,x,h-1,t);}
        for(int y=0;y<h;y++){setC(m,0,0,y,t);setC(m,0,w-1,y,t);}
    };
    auto tree = [&](Map& m,int x,int y){ setC(m,1,x,y,TREE); setC(m,2,x,y-1,CANOPY); };
    auto pine = [&](Map& m,int x,int y){ setC(m,1,x,y,PINE); setC(m,2,x,y-1,PINETOP); };
    auto sign = [&](Map& m,int x,int y,const std::string& txt){
        Event e; e.id=m.nextEventId(); e.x=x; e.y=y; e.type=EventType::Message; e.text=txt; setC(m,1,x,y,SIGN); m.events.push_back(e);
    };
    auto npc = [&](Map& m,int x,int y,const std::string& txt){
        Event e; e.id=m.nextEventId(); e.x=x; e.y=y; e.type=EventType::Message; e.text=txt;
        e.graphicAsset=A_npc; e.behavior=NpcBehavior::Wander; m.events.push_back(e);
    };

    const int W=40,H=30;
    std::vector<std::shared_ptr<Map>> maps;
    auto mk=[&](const char* name)->Map&{
        std::shared_ptr<Map> m;
        if(!append && maps.empty() && !p->maps.empty()){ m=p->maps.front(); m->name=name; m->tilemap.resize(W,H); }
        else m=p->addMap(name,W,H);
        m->tileset.assetId=A_tiles; m->tileset.columns=TSC; m->tileset.rows=TSR;
        m->tileset.tileWidth=32; m->tileset.tileHeight=32;
        maps.push_back(m); return *m;
    };

    // 1. 필드 -----------------------------------------------------------------
    { Map& m=mk("1. 필드"); baseFill(m,F_GRASS,F_GRASS2); patch(m,F_PATH,3,2); patch(m,WATER,2,2);
      for(int i=0;i<26;i++) tree(m,2+rnd(W-4),2+rnd(H-4));
      scatter(m,1,BUSH,14); scatter(m,1,ROCK,8); scatter(m,1,FLR,10); scatter(m,1,FLY,10);
      scatter(m,2,TALLG,16); scatter(m,1,MUSH,5);
      sign(m,W/2,H/2-1,"드넓은 필드. 모험의 시작."); npc(m,W/2+3,H/2,"좋은 날이군요!"); }
    // 2. 전쟁터 ---------------------------------------------------------------
    { Map& m=mk("2. 전쟁터"); baseFill(m,MUD,BROKENGND); patch(m,BLOODMUD,6,2); patch(m,CRATER,5,1);
      scatter(m,1,SANDBAG,12); scatter(m,1,BARRICADE,10); scatter(m,1,BANNER,5);
      scatter(m,2,SMOKE,10); scatter(m,1,ROCK,6); sign(m,W/2,H/2,"전쟁터. 시체와 포연이 가득하다."); }
    // 3. 산맥 -----------------------------------------------------------------
    { Map& m=mk("3. 산맥"); baseFill(m,ROCKFLR,SCREE); border(m,CLIFF); patch(m,SNOW,5,2);
      for(int i=0;i<22;i++) pine(m,2+rnd(W-4),2+rnd(H-4));
      scatter(m,1,BOULDER,14); scatter(m,1,SNOWROCK,10); scatter(m,1,CLIFF,16);
      sign(m,W/2,H/2,"험준한 산맥. 발을 조심하라."); }
    // 4. 도시 -----------------------------------------------------------------
    { Map& m=mk("4. 도시"); baseFill(m,COBBLE,PAVE);
      for(int i=0;i<10;i++){int bx=2+rnd(W-8),by=2+rnd(H-8);
        for(int dx=0;dx<4;dx++){setC(m,1,bx+dx,by+1,BRICKWALL);setC(m,2,bx+dx,by,ROOFTOP);setC(m,2,bx+dx,by-1,ROOFTOP);}
        setC(m,1,bx+1,by+1,DOOR); setC(m,1,bx+2,by+1,WINDOW);}
      scatter(m,1,LAMP,10); scatter(m,1,STALL,6); setC(m,1,W/2,H/2,FOUNTAIN);
      npc(m,W/2+2,H/2,"도시에 오신 걸 환영합니다."); npc(m,8,8,"물건 사세요~"); sign(m,4,4,"중앙 도시"); }
    // 5. 성곽 -----------------------------------------------------------------
    { Map& m=mk("5. 성곽"); baseFill(m,STONEBRICK,STONEFLRIN); border(m,BATTLE);
      for(int x=0;x<W;x+=6){setC(m,1,x,0,TOWER);setC(m,1,x,H-1,TOWER);}
      setC(m,1,W/2,H-1,GATE); setC(m,0,W/2,H-1,COBBLE); setC(m,1,W/2,H-2,GATE);
      scatter(m,1,PILLAR,12); scatter(m,1,TORCH,14); scatter(m,1,BANNER,8);
      sign(m,W/2,H/2,"굳건한 성곽."); npc(m,W/2+3,H/2,"누구냐, 멈춰라!"); }
    // 6. 보스전 ---------------------------------------------------------------
    { Map& m=mk("6. 보스전"); baseFill(m,DARKFLR,BONEFLR); border(m,OBSIDIAN); patch(m,LAVACRACK,6,2);
      setC(m,0,W/2,H/2,MAGIC); for(int dx=-1;dx<=1;dx++)for(int dy=-1;dy<=1;dy++)if(dx||dy)setC(m,0,W/2+dx,H/2+dy,MAGIC);
      scatter(m,1,SKULL,12); scatter(m,1,BRAZIER,8); scatter(m,1,OBSIDIAN,8); scatter(m,0,LAVA,6);
      sign(m,W/2,H/2+3,"보스의 제단. 강력한 기운이 감돈다."); }
    // 7. 광산 채굴 ------------------------------------------------------------
    { Map& m=mk("7. 광산 채굴"); baseFill(m,CAVEFLR,RUBBLE); border(m,CLIFF);
      for(int x=4;x<W-4;x+=6)for(int y=0;y<H;y++)setC(m,0,x,y,RAIL);
      scatter(m,1,WOODSUP,12); scatter(m,1,MINECART,5); scatter(m,1,OREGOLD,8);
      scatter(m,1,OREIRON,8); scatter(m,1,ORECRYS,5); scatter(m,1,RUBBLE,10);
      sign(m,W/2,H/2,"광산 채굴장. 광맥이 빛난다."); }
    // 8. 폐허 -----------------------------------------------------------------
    { Map& m=mk("8. 폐허"); baseFill(m,CRACKED,OVERGROWN); patch(m,MOSS,6,2);
      scatter(m,1,BROKENPILLAR,12); scatter(m,1,BROKENWALL,10); scatter(m,1,RUBBLE2,12);
      scatter(m,1,STATUEFRAG,6); scatter(m,2,WEEDS,14); sign(m,W/2,H/2,"오래된 폐허. 시간이 멈춘 듯하다."); }
    // 9. 약탈당하는 도시 ------------------------------------------------------
    { Map& m=mk("9. 약탈당하는 도시"); baseFill(m,COBBLE,CHARRED); patch(m,SCORCH,6,1);
      for(int i=0;i<8;i++){int bx=2+rnd(W-6),by=2+rnd(H-6); setC(m,1,bx,by,BURNTWALL); setC(m,2,bx,by-1,BROKENROOF); setC(m,1,bx+1,by,BRICKWALL);}
      scatter(m,1,FIRE,12); scatter(m,1,BARRICADE,8); scatter(m,2,SMOKE,12); scatter(m,1,RUBBLE2,8);
      npc(m,W/2,H/2,"도와줘요! 도시가 불타고 있어요!"); sign(m,5,5,"약탈당하는 도시"); }
    // 10. 파괴된 마을 ---------------------------------------------------------
    { Map& m=mk("10. 파괴된 마을"); baseFill(m,F_GRASS,CHARRED); patch(m,SCORCH,5,1);
      scatter(m,1,BURNTWALL,10); scatter(m,2,BROKENROOF,8); scatter(m,1,RUBBLE2,10);
      scatter(m,1,STUMP,8); scatter(m,1,FENCE_H,6); scatter(m,1,EMBER,8);
      sign(m,W/2,H/2,"파괴된 마을. 아무도 남지 않았다."); }
    // 11. 불타는 산 -----------------------------------------------------------
    { Map& m=mk("11. 불타는 산"); baseFill(m,ROCKFLR,SCORCH); border(m,CLIFF); patch(m,LAVA,6,2);
      scatter(m,1,FIRE,16); scatter(m,1,BOULDER,10); scatter(m,2,SMOKE,16); scatter(m,1,CHARRED,10);
      sign(m,W/2,H/2,"불타는 산. 용암이 흐른다."); }
    // 12. 거지굴 (실내) -------------------------------------------------------
    { Map& m=mk("12. 거지굴"); baseFill(m,DIRTFLRIN,STRAW); border(m,WOODWALLIN);
      scatter(m,1,SACK,10); scatter(m,1,BARRELIN,6); scatter(m,1,POT,6); scatter(m,1,RUGWORN,5);
      setC(m,1,4,4,FIREPLACE); scatter(m,1,CRATEIN,6);
      npc(m,W/2,H/2,"한 푼만 줍쇼..."); sign(m,3,H-3,"거지굴"); }
    // 13. 식당 (실내) ---------------------------------------------------------
    { Map& m=mk("13. 식당"); baseFill(m,WOODFLR,WOODFLR2); border(m,WOODWALLIN);
      for(int ty=4;ty<H-4;ty+=5)for(int tx=4;tx<W-4;tx+=6){setC(m,1,tx,ty,TABLE);setC(m,1,tx-1,ty,CHAIR);setC(m,1,tx+1,ty,CHAIR);setC(m,1,tx,ty,PLATE);setC(m,1,tx,ty+1,BREAD);}
      for(int x=3;x<W-3;x++){setC(m,1,x,2,COUNTER);} setC(m,1,W/2,3,CAULDRON);
      scatter(m,1,SHELF,4); scatter(m,1,MUG,8); scatter(m,1,LANTERN,5); scatter(m,0,RUGRED,4);
      npc(m,W/2,5,"어서오세요! 뭘 드릴까요?"); sign(m,3,3,"식당"); }
    // 14. 여관 (실내) ---------------------------------------------------------
    { Map& m=mk("14. 여관"); baseFill(m,WOODFLR,STONEFLRIN); border(m,WOODWALLIN);
      for(int i=0;i<10;i++){int bx=3+rnd(W-6),by=3+rnd(H-6); setC(m,1,bx,by,BED); setC(m,0,bx,by+1,RUGBLUE);}
      for(int x=3;x<14;x++){setC(m,1,x,2,COUNTER);} setC(m,1,4,2,LANTERN);
      setC(m,1,W-4,4,FIREPLACE); scatter(m,1,BARRELIN,6); scatter(m,1,TABLE,4); scatter(m,1,CHAIR,6);
      scatter(m,1,WINDOWIN,6); setC(m,1,W/2,H-1,DOORIN);
      npc(m,7,4,"하룻밤 묵으시려고요?"); sign(m,3,3,"여관"); }
    // helper: stamp a small house (walls + roof overhead + door)
    auto house=[&](Map& m,int x,int y,int w,int h,int wall,int roof,int doorT){
        for(int dx=0;dx<w;dx++){ setC(m,1,x+dx,y+h-1,wall); for(int dy=0;dy<h-1;dy++) setC(m,2,x+dx,y+dy,roof); }
        setC(m,1,x+w/2,y+h-1,doorT);
    };
    // 15. 초가집 -------------------------------------------------------------
    { Map& m=mk("15. 초가집"); baseFill(m,F_GRASS,F_DIRT); patch(m,F_PATH,2,2);
      house(m,8,8,5,4,WOODWALLIN,THATCH,DOORIN); house(m,24,16,5,4,WOODWALLIN,THATCH,DOORIN);
      scatter(m,1,FENCE_H,10); scatter(m,1,BUSH,8); scatter(m,1,WEEDS,8); setC(m,1,14,20,SIGN);
      npc(m,12,12,"초가집에 오신 걸 환영해요."); sign(m,W/2,H/2,"초가집 마을"); }
    // 16. 기와집 -------------------------------------------------------------
    { Map& m=mk("16. 기와집"); baseFill(m,COBBLE,F_GRASS);
      house(m,7,7,6,4,STONEBRICK,TILEROOF,GATE); house(m,22,15,6,4,STONEBRICK,TILEROOF,GATE);
      scatter(m,1,PILLAR,6); scatter(m,1,LAMP,6); scatter(m,1,FLR,6);
      npc(m,12,11,"기와집은 격조가 다르지요."); sign(m,W/2,H/2,"기와집"); }
    // 17. 벽돌집 -------------------------------------------------------------
    { Map& m=mk("17. 벽돌집"); baseFill(m,PAVE,COBBLE);
      for(int i=0;i<6;i++){int bx=3+rnd(W-8),by=3+rnd(H-7); house(m,bx,by,5,4,BRICKRED,ROOFTOP,DOOR); setC(m,1,bx+1,by+3,WINDOW);}
      scatter(m,1,LAMP,8); scatter(m,1,FENCE_H,6); npc(m,W/2,H/2,"벽돌집 동네입니다."); sign(m,4,4,"벽돌집 거리"); }
    // 18. 아파트집 -----------------------------------------------------------
    { Map& m=mk("18. 아파트집"); baseFill(m,ASPHALT,PAVE);
      for(int bx=3;bx<W-4;bx+=8)for(int by=3;by<H-6;by+=7){ for(int dx=0;dx<5;dx++)for(int dy=0;dy<5;dy++)setC(m,1,bx+dx,by+dy,APARTWALL); setC(m,1,bx+2,by+4,DOOR);}
      scatter(m,1,LAMP,8); scatter(m,1,STALL,4); npc(m,W/2,H-3,"몇 동 몇 호 찾으세요?"); sign(m,4,4,"아파트 단지"); }
    // 19. 빌딩집 -------------------------------------------------------------
    { Map& m=mk("19. 빌딩집"); baseFill(m,ASPHALT,CONCRETE);
      for(int bx=3;bx<W-5;bx+=9){ int bh=8+rnd(10); for(int dx=0;dx<6;dx++)for(int dy=0;dy<bh;dy++)setC(m,1,bx+dx,3+dy,GLASSWALL); setC(m,1,bx+2,3+bh-1,DOOR);}
      scatter(m,1,LAMP,8); scatter(m,0,ASPHALT,4); npc(m,W/2,H-3,"도심 빌딩가입니다."); sign(m,4,H-4,"빌딩 거리"); }
    // 20. 사무실 (실내) ------------------------------------------------------
    { Map& m=mk("20. 사무실"); baseFill(m,TILEFLR,CONCRETE); border(m,OFFICEWALL);
      for(int ty=5;ty<H-4;ty+=5)for(int tx=5;tx<W-4;tx+=7){setC(m,1,tx,ty,TABLE);setC(m,1,tx,ty+1,CHAIR);setC(m,1,tx,ty,PLATE);}
      for(int x=3;x<W-3;x++){setC(m,1,x,2,COUNTER);} scatter(m,1,SHELF,6); scatter(m,1,LANTERN,5);
      npc(m,W/2,4,"회의 시작합니다."); sign(m,3,3,"사무실"); }
    // 21. 황무지 위 안전가옥 -------------------------------------------------
    { Map& m=mk("21. 황무지 안전가옥"); baseFill(m,SAND,MUD); patch(m,SCORCH,4,1);
      for(int dx=0;dx<7;dx++)for(int dy=0;dy<5;dy++)setC(m,1,14+dx,12+dy,CONCRETE);
      house(m,15,13,5,3,BRICKRED,ROOFTOP,DOOR);
      scatter(m,1,SANDBAG,14); scatter(m,1,BARRICADE,8); scatter(m,1,CRATEIN,6); scatter(m,1,ROCK,6);
      npc(m,17,16,"여긴 안전해. 들어와."); sign(m,W/2,H-4,"황무지 안전가옥"); }
    // 22. 축제 ----------------------------------------------------------------
    { Map& m=mk("22. 축제"); baseFill(m,COBBLE,PAVE);
      scatter(m,1,STALL,12); scatter(m,1,TENT,8); scatter(m,1,BANNER,10); scatter(m,1,LAMP,12);
      scatter(m,2,BALLOON,10); setC(m,1,W/2,H/2,FOUNTAIN);
      npc(m,W/2+2,H/2,"축제다! 즐기고 가세요!"); npc(m,10,10,"솜사탕 사세요~"); sign(m,5,5,"마을 축제"); }
    // 23. 놀이동산 -----------------------------------------------------------
    { Map& m=mk("23. 놀이동산"); baseFill(m,PAVE,COBBLE);
      setC(m,1,10,10,FERRIS); setC(m,1,28,12,FERRIS);
      scatter(m,1,TENT,8); scatter(m,1,STALL,8); scatter(m,2,BALLOON,14); scatter(m,1,LAMP,10); scatter(m,1,FLY,8);
      npc(m,W/2,H/2,"놀이기구 타러 가요!"); sign(m,5,H-4,"놀이동산"); }
    // 24. 건설현장 -----------------------------------------------------------
    { Map& m=mk("24. 건설현장"); baseFill(m,CONCRETE,F_DIRT); patch(m,ASPHALT,3,2);
      scatter(m,1,SCAFFOLD,14); scatter(m,1,CRANE,5); scatter(m,1,CRATEIN,10); scatter(m,1,SANDBAG,10);
      scatter(m,1,RUBBLE,10); scatter(m,1,BARRICADE,8); npc(m,W/2,H/2,"안전모 착용하세요!"); sign(m,4,4,"건설현장"); }
    // 25. 쓰레기장 -----------------------------------------------------------
    { Map& m=mk("25. 쓰레기장"); baseFill(m,DIRTFLRIN,CHARRED); patch(m,SCORCH,4,1);
      scatter(m,1,TRASH,20); scatter(m,1,SACK,12); scatter(m,1,BARRELIN,8); scatter(m,1,CRATEIN,8);
      scatter(m,1,RUBBLE2,10); scatter(m,2,SMOKE,8); npc(m,W/2,H/2,"여기서 뭐 쓸만한 거 없나..."); sign(m,4,4,"쓰레기장"); }
    // 26. 폐수 지하통로 ------------------------------------------------------
    { Map& m=mk("26. 폐수 지하통로"); baseFill(m,STONEBRICK,CAVEFLR); border(m,STONEBRICK);
      for(int x=4;x<W-4;x+=1){setC(m,0,x,H/2,SLUDGE);} for(int x=4;x<W-4;x+=1){setC(m,0,x,H/2+1,SLUDGE);}
      scatter(m,1,PIPE,12); scatter(m,1,RUBBLE,10); scatter(m,1,BARRELIN,5); scatter(m,0,WATERDEEP,5);
      npc(m,8,8,"여긴 냄새가 지독하군..."); sign(m,W/2,4,"폐수 지하통로"); }

    // for a standalone pack, start on the field map; when appending, leave the
    // demo's existing start untouched.
    if (!append) {
        p->startMap = maps.front()->id; p->startX = W/2; p->startY = H/2+2;
        maps.front()->tilemap.setBlocked(W/2, H/2+2, false);
    }
    p->save();
    printf("concept pack: +%d themed maps, 128-tile tileset -> %s (%s)\n",
           (int)maps.size(), out.c_str(), append?"appended":"new");
    return 0;
}
