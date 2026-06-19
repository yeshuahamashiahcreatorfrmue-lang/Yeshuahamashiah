// gen_game: builds the polished single-map showcase game "Willowbrook".
// Generates a detailed tileset + character/enemy sprites (raylib Image API),
// then hand-builds one complete, playable village map with houses, a pond,
// gardens, NPCs, a shop, a chest, signs, decorations and field monsters.
#include "raylib.h"
#include "render/AssetGen.h"
#include "project/Project.h"
#include "database/Database.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>

using namespace tsukuru;

// ---- tileset layout (8 columns) ----
enum {
    GRASS=0, GRASS2=1, FLGRASS=2, PATH=3, COBBLE=4, WOOD=5, WATER=6, WATER2=7,
    SHALLOW=8, SHALLOW2=9, SAND=10, DIRT2=11, HEDGE=12, HEDGETOP=13, BRIDGE=14, PLANK=15,
    WALL=16, WINDOW=17, DOOR=18, ROOF_TL=19, ROOF_TM=20, ROOF_TR=21, ROOF_BL=22, ROOF_BM=23,
    ROOF_BR=24, FENCE_H=25, FENCE_V=26, GATE=27, LAMP=28, WELL=29, BARREL=30, CRATE=31,
    TRUNK=32, CANOPY=33, BUSH=34, ROCK=35, FL_RED=36, FL_YEL=37, SIGN=38, FLOWERBED=39,
    STATUE=40, STAIRS=41, TALLG=42, STUMP=43, MUSH=44, CHEST=45, SAND2=46, COBBLE2=47
};
static const int TSC = 8, TSR = 6, T = 32;

static int hsh(int x, int y) { unsigned n = (unsigned)x*374761393u + (unsigned)y*668265263u; n = (n ^ (n>>13)) * 1274126177u; return (int)((n>>16)&255u); }
static void R(Image* im,int x,int y,int w,int h,Color c){ ImageDrawRectangle(im,x,y,w,h,c); }
static void P(Image* im,int x,int y,Color c){ ImageDrawPixel(im,x,y,c); }

static void texFill(Image* im,int ox,int oy,Color base,Color lo,Color hi,int loT=46,int hiT=210){
    unsigned char* d=(unsigned char*)im->data; int W=im->width, Hh=im->height;
    for(int j=0;j<T;j++)for(int i=0;i<T;i++){
        int X=ox+i,Y=oy+j; if(X<0||Y<0||X>=W||Y>=Hh) continue;
        int v=hsh(X,Y);Color c=base;if(v<loT)c=lo;else if(v>hiT)c=hi;
        long p=((long)Y*W+X)*4; d[p]=c.r;d[p+1]=c.g;d[p+2]=c.b;d[p+3]=255;
    }
}

static Image genTileset() {
    Image im = GenImageColor(TSC*T, TSR*T, BLANK);
    auto cell=[&](int idx,int&ox,int&oy){ox=(idx%TSC)*T;oy=(idx/TSC)*T;};
    int ox,oy;
    Color gB={86,152,72,255},gL={108,176,90,255},gD={66,130,58,255};
    Color wB={70,120,205,255},wL={120,170,235,255},wD={48,96,180,255};
    Color sB={96,150,220,255},sL={150,195,240,255};
    Color pB={182,152,104,255},pL={205,182,140,255},pD={150,122,80,255};
    Color cB={152,152,160,255},cL={178,178,186,255},cD={112,112,122,255};
    Color woB={160,116,72,255},woD={130,92,56,255};

    // 0 grass, 1 grass2, 2 flower grass
    cell(GRASS,ox,oy); texFill(&im,ox,oy,gB,gD,gL);
    cell(GRASS2,ox,oy); texFill(&im,ox,oy,gB,gD,{98,166,84,255}); for(int k=0;k<8;k++)P(&im,ox+(hsh(ox+k,oy)%30)+1,oy+(hsh(ox,oy+k*3)%30)+1,{72,138,64,255});
    cell(FLGRASS,ox,oy); texFill(&im,ox,oy,gB,gD,gL);
      P(&im,ox+8,oy+9,{230,90,90,255});P(&im,ox+9,oy+9,{230,90,90,255});P(&im,ox+8,oy+8,{240,210,90,255});
      P(&im,ox+21,oy+19,{240,210,90,255});P(&im,ox+22,oy+19,{240,210,90,255});P(&im,ox+15,oy+25,{220,120,210,255});
      P(&im,ox+25,oy+8,{235,235,235,255});
    // 3 path, 4 cobble, 5 wood
    cell(PATH,ox,oy); texFill(&im,ox,oy,pB,pD,pL);
      for(int k=0;k<5;k++)P(&im,ox+(hsh(ox+k,oy)%30)+1,oy+(hsh(ox,oy+k)%30)+1,pD);
    cell(COBBLE,ox,oy); texFill(&im,ox,oy,cB,cD,cL);
      for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,cD); for(int x=0;x<T;x+=8)R(&im,ox+x,oy,1,T,cD);
    cell(WOOD,ox,oy); texFill(&im,ox,oy,woB,woD,{180,134,86,255});
      for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,woD);
    // 6/7 water anim, 8/9 shallow anim
    cell(WATER,ox,oy); texFill(&im,ox,oy,wB,wD,wB); for(int y=3;y<T;y+=8)R(&im,ox+2,oy+y,T-6,2,wL);
    cell(WATER2,ox,oy); texFill(&im,ox,oy,wB,wD,wB); for(int y=7;y<T;y+=8)R(&im,ox+4,oy+y,T-8,2,wL);
    cell(SHALLOW,ox,oy); texFill(&im,ox,oy,sB,wB,sL); for(int y=4;y<T;y+=10)R(&im,ox+2,oy+y,T-4,2,sL);
    cell(SHALLOW2,ox,oy); texFill(&im,ox,oy,sB,wB,sL); for(int y=8;y<T;y+=10)R(&im,ox+3,oy+y,T-6,2,sL);
    // 10 sand, 11 dirt2, 46 sand2, 47 cobble2
    cell(SAND,ox,oy); texFill(&im,ox,oy,{222,206,150,255},{200,184,128,255},{236,222,170,255});
    cell(DIRT2,ox,oy); texFill(&im,ox,oy,pD,{132,106,68,255},pB);
    cell(SAND2,ox,oy); texFill(&im,ox,oy,{214,198,142,255},{196,180,124,255},{232,218,166,255});
    cell(COBBLE2,ox,oy); texFill(&im,ox,oy,cL,cB,{196,196,204,255}); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,cD);
    // 12 hedge (solid), 13 hedge top (overhead)
    cell(HEDGE,ox,oy); texFill(&im,ox,oy,{56,110,52,255},{44,92,44,255},{72,134,66,255});
    cell(HEDGETOP,ox,oy); R(&im,ox+2,oy+2,T-4,T-8,{64,124,60,255}); R(&im,ox+5,oy+4,T-10,8,{84,150,76,255});
    // 14 bridge, 15 plank
    cell(BRIDGE,ox,oy); texFill(&im,ox,oy,woB,woD,{180,134,86,255}); R(&im,ox,oy,T,2,woD);R(&im,ox,oy+T-2,T,2,woD);
    cell(PLANK,ox,oy); texFill(&im,ox,oy,{172,128,80,255},woD,{188,142,92,255});
    // 16 wall, 17 window, 18 door
    Color waB={200,184,152,255},waL={150,134,108,255};
    cell(WALL,ox,oy); R(&im,ox,oy,T,T,waB); for(int y=0;y<T;y+=8)R(&im,ox,oy+y,T,1,waL);
      for(int y=0;y<T;y+=16)for(int x=0;x<T;x+=16)R(&im,ox+x,oy+y,1,8,waL); for(int y=8;y<T;y+=16)for(int x=8;x<T;x+=16)R(&im,ox+x,oy+y,1,8,waL);
    cell(WINDOW,ox,oy); R(&im,ox,oy,T,T,waB); R(&im,ox+6,oy+7,20,18,{110,90,60,255}); R(&im,ox+8,oy+9,16,14,{130,190,225,255});
      R(&im,ox+15,oy+9,2,14,{110,90,60,255}); R(&im,ox+8,oy+15,16,2,{110,90,60,255});
    cell(DOOR,ox,oy); R(&im,ox,oy,T,T,waB); R(&im,ox+7,oy+5,18,27,{120,82,48,255}); R(&im,ox+9,oy+7,14,25,{96,62,36,255});
      R(&im,ox+15,oy+7,2,25,{120,82,48,255}); P(&im,ox+21,oy+19,{230,200,90,255});P(&im,ox+22,oy+19,{230,200,90,255});
    // 19-24 roof
    Color rB={184,76,66,255},rD={146,54,48,255},rL={206,104,92,255};
    auto roof=[&](int idx,bool top,bool l,bool r){cell(idx,ox,oy);R(&im,ox,oy,T,T,rB);
      for(int y=(top?4:0);y<T;y+=6)R(&im,ox,oy+y,T,3,rD); if(top)R(&im,ox,oy,T,4,{120,44,40,255});
      if(l)R(&im,ox,oy,3,T,rD); if(r)R(&im,ox+T-3,oy,3,T,rD); if(top)R(&im,ox,oy+5,T,1,rL);};
    roof(ROOF_TL,true,true,false); roof(ROOF_TM,true,false,false); roof(ROOF_TR,true,false,true);
    roof(ROOF_BL,false,true,false); roof(ROOF_BM,false,false,false); roof(ROOF_BR,false,false,true);
    // 25 fence_h, 26 fence_v, 27 gate, 28 lamp, 29 well
    Color fc={156,116,74,255},fd={124,90,56,255};
    cell(FENCE_H,ox,oy); R(&im,ox,oy+12,T,5,fc); R(&im,ox,oy+12,T,1,fd); R(&im,ox+4,oy+8,4,16,fc); R(&im,ox+24,oy+8,4,16,fc);
    cell(FENCE_V,ox,oy); R(&im,ox+12,oy,5,T,fc); R(&im,ox+12,oy,1,T,fd); R(&im,ox+8,oy+4,16,4,fc); R(&im,ox+8,oy+24,16,4,fc);
    cell(GATE,ox,oy); texFill(&im,ox,oy,pB,pD,pL); R(&im,ox+2,oy+6,4,20,fc); R(&im,ox+26,oy+6,4,20,fc);
    cell(LAMP,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+14,oy+8,4,22,{80,70,60,255}); R(&im,ox+11,oy+4,10,8,{255,230,140,255}); R(&im,ox+12,oy+5,8,6,{255,245,200,255});
    cell(WELL,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+5,oy+10,22,18,cB); R(&im,ox+5,oy+10,22,18,{0,0,0,0}); R(&im,ox+8,oy+13,16,12,{40,60,90,255}); R(&im,ox+5,oy+10,22,3,cD); R(&im,ox+4,oy+4,3,8,woD);R(&im,ox+25,oy+4,3,8,woD);R(&im,ox+4,oy+3,24,3,{150,60,52,255});
    // 30 barrel, 31 crate
    cell(BARREL,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+8,oy+6,16,22,{150,108,64,255}); R(&im,ox+8,oy+10,16,2,fd);R(&im,ox+8,oy+20,16,2,fd); R(&im,ox+8,oy+6,16,2,{120,86,50,255});
    cell(CRATE,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+7,oy+7,18,18,{170,128,76,255}); R(&im,ox+7,oy+7,18,18,{0,0,0,0}); R(&im,ox+7,oy+7,18,2,fd);R(&im,ox+7,oy+23,18,2,fd);R(&im,ox+7,oy+7,2,18,fd);R(&im,ox+23,oy+7,2,18,fd);R(&im,ox+7,oy+7,18,18,fd);
    // 32 trunk(+grass), 33 canopy(overhead), 34 bush, 35 rock
    cell(TRUNK,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+13,oy+12,6,18,{120,84,52,255}); R(&im,ox+13,oy+12,2,18,{96,64,40,255});
    cell(CANOPY,ox,oy); R(&im,ox+3,oy+5,26,24,{56,118,56,255}); R(&im,ox+6,oy+3,20,12,{74,148,72,255}); R(&im,ox+10,oy+8,14,8,{96,178,96,255}); P(&im,ox+12,oy+11,{120,200,120,255});
    cell(BUSH,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+6,oy+12,20,14,{60,124,58,255}); R(&im,ox+9,oy+9,14,8,{80,150,76,255});
    cell(ROCK,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+8,oy+14,16,12,{140,140,148,255}); R(&im,ox+11,oy+11,10,6,{168,168,176,255}); R(&im,ox+8,oy+24,16,2,cD);
    // 36 flower red, 37 flower yellow
    auto flow=[&](int idx,Color c){cell(idx,ox,oy);texFill(&im,ox,oy,gB,gD,gL);
      int sp[3][2]={{9,12},{18,9},{14,20}};for(auto&s:sp){R(&im,ox+s[0],oy+s[1],3,3,c);P(&im,ox+s[0]+1,oy+s[1]+1,{255,250,210,255});R(&im,ox+s[0]+1,oy+s[1]+3,1,3,gD);}};
    flow(FL_RED,{225,80,80,255}); flow(FL_YEL,{245,210,90,255});
    // 38 sign, 39 flowerbed, 40 statue, 41 stairs
    cell(SIGN,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+14,oy+12,4,16,woD); R(&im,ox+7,oy+6,18,11,{170,130,80,255}); R(&im,ox+7,oy+6,18,11,{0,0,0,0}); R(&im,ox+9,oy+9,14,1,woD);R(&im,ox+9,oy+12,10,1,woD);
    cell(FLOWERBED,ox,oy); R(&im,ox,oy,T,T,{120,86,56,255}); for(int k=0;k<6;k++){int fx=ox+4+(k*5)%24,fy=oy+5+(k*7)%22;Color c=(k%2)?Color{225,90,90,255}:Color{245,210,90,255};R(&im,fx,fy,3,3,c);}
    cell(STATUE,ox,oy); texFill(&im,ox,oy,cB,cD,cL); R(&im,ox+10,oy+6,12,20,{180,180,188,255}); R(&im,ox+12,oy+3,8,7,{196,196,204,255}); R(&im,ox+8,oy+26,16,4,cD);
    cell(STAIRS,ox,oy); for(int s=0;s<4;s++)R(&im,ox,oy+s*8,T,7,s%2?cB:cL);
    // 42 tallgrass(overhead), 43 stump, 44 mushroom, 45 chest
    cell(TALLG,ox,oy); for(int k=0;k<14;k++){int gx=ox+2+(k*7)%28;int gh=8+(hsh(gx,oy)%10);R(&im,gx,oy+T-gh,2,gh,{72,140,66,255});}
    cell(STUMP,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+9,oy+12,14,12,{132,94,58,255}); R(&im,ox+11,oy+12,10,4,{168,128,84,255});
    cell(MUSH,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+13,oy+16,3,8,{230,220,200,255}); R(&im,ox+9,oy+11,12,7,{210,80,70,255}); P(&im,ox+12,oy+13,{250,240,230,255});P(&im,ox+16,oy+14,{250,240,230,255});
    cell(CHEST,ox,oy); texFill(&im,ox,oy,gB,gD,gL); R(&im,ox+7,oy+12,18,14,{160,116,64,255}); R(&im,ox+7,oy+10,18,5,{120,84,48,255}); R(&im,ox+7,oy+10,18,16,{0,0,0,0}); R(&im,ox+14,oy+15,4,5,{240,210,90,255});
    return im;
}

static void saveImg(Image im,const std::string&p){ ExportImage(im,p.c_str()); UnloadImage(im); }

// ---------- procedural audio (16-bit mono WAV) ----------
static const int RATE = 22050;
static const double TAU = 6.28318530717958647692;
static void note(std::vector<short>&b,double f,double dur,double vol,int wave=0){
    int n=(int)(dur*RATE);
    for(int i=0;i<n;i++){double t=(double)i/RATE; double env=1.0-(double)i/n;
        double ph=f*t,s; if(wave==0)s=(fmod(ph,1.0)<0.5?1:-1); else if(wave==1)s=sin(ph*TAU);
        else s=((rand()%2001)-1000)/1000.0;
        int v=(int)(s*vol*env*30000); if(v>32767)v=32767; if(v<-32768)v=-32768; b.push_back((short)v);}
}
static void rest(std::vector<short>&b,double dur){ b.insert(b.end(),(size_t)(dur*RATE),0); }
static void saveWav(const std::vector<short>&s,const std::string&path){
    Wave w; w.frameCount=(unsigned)s.size(); w.sampleRate=RATE; w.sampleSize=16; w.channels=1;
    w.data=malloc(s.size()*sizeof(short)); memcpy(w.data,s.data(),s.size()*sizeof(short));
    ExportWave(w,path.c_str()); free(w.data);
}
static void genAudio(const std::string& ad){
    std::filesystem::create_directories(ad+"/sfx");
    std::vector<short> b;
    b.clear(); note(b,900,0.05,0.5,2); note(b,500,0.06,0.4,2); saveWav(b,ad+"/sfx/attack.wav");
    b.clear(); note(b,200,0.09,0.7,0); saveWav(b,ad+"/sfx/hit.wav");
    b.clear(); note(b,420,0.09,0.6,0); note(b,300,0.09,0.6,0); note(b,170,0.16,0.6,0); saveWav(b,ad+"/sfx/defeat.wav");
    b.clear(); note(b,140,0.14,0.6,0); saveWav(b,ad+"/sfx/hurt.wav");
    b.clear(); note(b,880,0.05,0.5,1); note(b,1320,0.07,0.5,1); saveWav(b,ad+"/sfx/coin.wav");
    b.clear(); note(b,523,0.07,0.5,0); note(b,659,0.07,0.5,0); note(b,784,0.07,0.5,0); note(b,1047,0.13,0.5,0); saveWav(b,ad+"/sfx/levelup.wav");
    b.clear(); note(b,1000,0.04,0.4,1); saveWav(b,ad+"/sfx/select.wav");
    // BGM: village (gentle major loop)
    { std::vector<short> m; double q=0.26; int mel[]={523,659,784,659,587,659,523,392, 523,659,784,880,784,659,587,523};
      for(int rep=0;rep<2;rep++) for(int i=0;i<16;i++){ note(m,mel[i],q,0.30,1); note(m,mel[i]/2.0,q*0.0+0.0,0,1);} saveWav(m,ad+"/bgm_village.wav"); }
    // BGM: cave (slow eerie minor loop)
    { std::vector<short> m; double q=0.42; int mel[]={220,262,196,247,220,175,196,165};
      for(int rep=0;rep<3;rep++) for(int i=0;i<8;i++){ note(m,mel[i],q,0.26,0); rest(m,0.04);} saveWav(m,ad+"/bgm_cave.wav"); }
}

int main(int argc,char**argv){
    std::string out = argc>1?argv[1]:"projects/willowbrook";
    SetTraceLogLevel(LOG_WARNING);
    std::string ad = out+"/assets";
    std::error_code ec;
    std::filesystem::create_directories(ad, ec);
    std::filesystem::create_directories(out+"/maps", ec);
    // generate art
    saveImg(genTileset(), ad+"/tileset.png");
    saveImg(gen::characterSheet({80,140,220,255},{240,200,160,255}), ad+"/hero.png");
    saveImg(gen::characterSheet({200,120,80,255},{240,200,160,255}),  ad+"/villager1.png");
    saveImg(gen::characterSheet({120,170,110,255},{238,202,168,255}), ad+"/villager2.png");
    saveImg(gen::characterSheet({170,170,180,255},{235,205,175,255}), ad+"/elder.png");
    saveImg(gen::characterSheet({210,150,180,255},{240,205,170,255}), ad+"/girl.png");
    saveImg(gen::enemySprite({110,200,120,255}), ad+"/slime.png");
    saveImg(gen::enemySprite({130,90,170,255}),  ad+"/bat.png");
    saveImg(gen::enemySprite({190,120,80,255}),  ad+"/boar.png");
    saveImg(gen::enemySprite({150,40,60,255}),   ad+"/guardian.png");
    genAudio(ad);
    printf("art + audio generated\n");

    auto p = std::make_shared<Project>();
    p->dir = out; p->name = "Willowbrook";
    int A_ts   = p->assets.addExisting(AssetType::Image,"tileset","assets/tileset.png");
    int A_hero = p->assets.addExisting(AssetType::Image,"hero","assets/hero.png");
    int A_v1   = p->assets.addExisting(AssetType::Image,"villager1","assets/villager1.png");
    int A_v2   = p->assets.addExisting(AssetType::Image,"villager2","assets/villager2.png");
    int A_eld  = p->assets.addExisting(AssetType::Image,"elder","assets/elder.png");
    int A_girl = p->assets.addExisting(AssetType::Image,"girl","assets/girl.png");
    int A_sl   = p->assets.addExisting(AssetType::Image,"slime","assets/slime.png");
    int A_bat  = p->assets.addExisting(AssetType::Image,"bat","assets/bat.png");
    int A_boar = p->assets.addExisting(AssetType::Image,"boar","assets/boar.png");
    int A_guard= p->assets.addExisting(AssetType::Image,"guardian","assets/guardian.png");
    int A_bgmV = p->assets.addExisting(AssetType::Audio,"bgm_village","assets/bgm_village.wav");
    int A_bgmC = p->assets.addExisting(AssetType::Audio,"bgm_cave","assets/bgm_cave.wav");

    // database
    Database& db = p->database;
    db.items.push_back({1,"Potion","Restores 60 HP.",30,-1,ItemEffect::HealHP,60,true});
    db.items.push_back({2,"Hi-Potion","Restores 200 HP.",120,-1,ItemEffect::HealHP,200,true});
    db.items.push_back({3,"Ether","Restores 40 MP.",80,-1,ItemEffect::HealMP,40,true});
    db.items.push_back({4,"Crystal","The village's sacred Crystal.",0,-1,ItemEffect::None,0,false});
    db.equipment.push_back({1,"Iron Sword",EquipSlot::Weapon,140,-1,12,0});
    db.equipment.push_back({2,"Leather Armor",EquipSlot::Armor,120,-1,0,8});
    db.skills.push_back({1,"Slash",4,18,false});
    db.skills.push_back({2,"Heal",6,40,true});
    db.actors.push_back({1,"Hero",A_hero,140,30,15,8,7,{1,2}});
    db.enemies.push_back({1,"Slime",A_sl,34,0,9,4,4,9,7});
    db.enemies.push_back({2,"Bat",A_bat,26,0,12,3,7,11,9});
    db.enemies.push_back({3,"Boar",A_boar,60,0,15,6,5,22,18});
    db.enemies.push_back({4,"Cave Guardian",A_guard,220,0,18,9,4,150,120});

    auto m = p->addMap("Willowbrook Village", 44, 34);
    m->tileset.assetId=A_ts; m->tileset.tileWidth=32; m->tileset.tileHeight=32; m->tileset.columns=8; m->tileset.rows=6;
    m->animTiles = { WATER, SHALLOW };
    Tilemap& tm = m->tilemap;
    int W=44,H=34;
    auto setT=[&](int l,int x,int y,int t){ tm.setTile(l,x,y,t); };
    auto blk=[&](int x,int y,bool b=true){ tm.setBlocked(x,y,b); };
    auto rectT=[&](int l,int x,int y,int w,int h,int t){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)if(tm.inBounds(i,j))setT(l,i,j,t);};
    auto rectBlk=[&](int x,int y,int w,int h,bool b=true){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)blk(i,j,b);};

    // ground: grass with variation
    for(int y=0;y<H;y++)for(int x=0;x<W;x++){int v=hsh(x*7,y*7);int t=GRASS;if(v>247)t=FLGRASS;else if(v>235)t=GRASS2;setT(0,x,y,t);}

    // tree border (trunk below + canopy overhead)
    auto tree=[&](int x,int y){if(!tm.inBounds(x,y))return;setT(1,x,y,TRUNK);setT(2,x,y,CANOPY);blk(x,y);};
    for(int x=0;x<W;x++){tree(x,0);tree(x,H-1);}
    for(int y=0;y<H;y++){tree(0,y);tree(W-1,y);}

    // pond (water + shallow ring) with a bridge
    int px=5,py=6,pw=9,ph=6;
    rectT(0,px-1,py-1,pw+2,ph+2,SHALLOW); rectBlk(px-1,py-1,pw+2,ph+2);
    rectT(0,px,py,pw,ph,WATER);
    rectT(0,px+3,py-1,2,ph+2,BRIDGE); rectBlk(px+3,py-1,2,ph+2,false); // walkable bridge
    for(int j=py;j<py+ph;j++)for(int i=px;i<px+pw;i++) if(!(i>=px+3&&i<px+5)) blk(i,j);

    // main paths (cobble plaza + dirt roads)
    rectT(0,18,13,9,8,COBBLE);          // central plaza
    rectT(0,1,17,42,2,PATH);            // horizontal road
    rectT(0,21,1,2,32,PATH);            // vertical road
    setT(0,21,16,GATE);setT(0,22,16,GATE);

    // well + statue + lamps in plaza
    setT(1,22,15,WELL); blk(22,15);
    setT(1,20,14,LAMP); blk(20,14); setT(1,25,16,LAMP); blk(25,16);
    setT(1,24,14,STATUE); blk(24,14);

    // house builder: 3 wide footprint (roof 3x2 + wall row with window/door/window)
    auto house=[&](int x,int y,int doorCol){
        setT(1,x,y,ROOF_TL);setT(1,x+1,y,ROOF_TM);setT(1,x+2,y,ROOF_TR);
        setT(1,x,y+1,ROOF_BL);setT(1,x+1,y+1,ROOF_BM);setT(1,x+2,y+1,ROOF_BR);
        for(int i=0;i<3;i++) setT(1,x+i,y+2, (i==doorCol)?DOOR:WINDOW);
        rectBlk(x,y,3,3);
        return std::pair<int,int>(x+doorCol,y+3); // tile in front of the door
    };
    auto h1=house(4,20,1);
    auto h2=house(34,20,1);
    auto h3=house(34,4,1);
    auto h4=house(4,26,1);

    // garden with fences + flowerbeds (south-west)
    rectT(1,30,26,8,1,FENCE_H); rectT(1,30,31,8,1,FENCE_H);
    for(int j=26;j<=31;j++){setT(1,30,j,FENCE_V);setT(1,37,j,FENCE_V);}
    setT(1,33,31,GATE);blk(33,31,false);
    for(int j=27;j<=30;j++)for(int i=31;i<=36;i++){setT(0,i,j,FLOWERBED);}
    rectBlk(30,26,8,1);rectBlk(30,31,8,1);for(int j=26;j<=31;j++){blk(30,j);blk(37,j);}blk(33,31,false);

    // scattered decoration
    int deco[][3]={{12,24,BUSH},{15,28,FL_RED},{16,22,FL_YEL},{28,8,BUSH},{30,11,ROCK},
        {9,30,MUSH},{12,12,FL_YEL},{38,24,ROCK},{18,30,BUSH},{40,30,STUMP},{8,14,FL_RED},
        {27,28,TALLG},{29,29,TALLG},{14,5,FL_YEL},{17,9,BUSH}};
    for(auto&d:deco){ if(d[2]==TALLG) setT(2,d[0],d[1],TALLG); else { setT(1,d[0],d[1],d[2]); if(d[2]==BUSH||d[2]==ROCK||d[2]==STUMP) blk(d[0],d[1]); } }
    setT(1,19,12,BARREL);blk(19,12); setT(1,26,12,CRATE);blk(26,12);
    setT(1,2,18,LAMP);blk(2,18); setT(1,41,18,LAMP);blk(41,18);

    // ---- events: NPCs, shop, chest, signs, house doors ----
    int eid=1;
    auto ev=[&](Event e){ e.id=eid++; m->events.push_back(e); };
    auto npc=[&](int x,int y,int gfx,const std::string&txt,bool wander=false){ Event e; e.x=x;e.y=y;e.type=EventType::Message;e.trigger=TriggerType::ActionButton;e.graphicAsset=gfx;e.text=txt;e.wander=wander; ev(e); };
    auto sign=[&](int x,int y,const std::string&txt){ setT(1,x,y,SIGN);blk(x,y); Event e;e.x=x;e.y=y;e.type=EventType::Message;e.trigger=TriggerType::ActionButton;e.text=txt;ev(e); };
    auto door=[&](std::pair<int,int> pos,const std::string&txt){ Event e;e.x=pos.first;e.y=pos.second;e.type=EventType::Message;e.trigger=TriggerType::ActionButton;e.text=txt;ev(e); };

    npc(23,16,A_eld,"Elder: Welcome to Willowbrook, young hero!|Wild beasts roam the eastern fields,|and something stirs in the northern cave...");
    npc(19,15,A_v1,"Villager: The well water is cold and sweet.|Try the shop to the east!",true);
    npc(28,20,A_v2,"Farmer: My flowers bloom nicely|even in this gentle rain.",true);
    npc(8,16,A_girl,"Girl: I saw a shiny box near the trees!|Hee hee.",true);

    // autorun intro (cutscene + quest objective, plays once on arrival)
    { Event e; e.x=22;e.y=20;e.type=EventType::Quest;e.trigger=TriggerType::Autorun;
      e.text="The village Crystal was stolen by a cave beast! Recover it from the northern cave."; ev(e); }
    // autorun ending: fires when you return with the Crystal (switch 3)
    { Event e; e.x=22;e.y=20;e.type=EventType::Ending;e.trigger=TriggerType::Autorun;
      e.conditionSwitch=3;e.conditionValue=true; ev(e); }

    // shopkeeper (sells a potion via Shop event)
    { Event e; e.x=30;e.y=18;e.type=EventType::Shop;e.trigger=TriggerType::ActionButton;e.graphicAsset=A_v1;e.itemId=1;e.text="Shop: Buy a Potion?"; ev(e); }

    // treasure chest (gives Hi-Potion once)
    setT(1,3,2,CHEST);
    { Event e; e.x=3;e.y=2;e.type=EventType::GiveItem;e.trigger=TriggerType::ActionButton;e.itemId=2;e.amount=1;e.once=true;e.text="You found a Hi-Potion!"; ev(e); }

    sign(21,18,"Willowbrook Village  -  Plaza");
    sign(40,17,"East Field: Monsters ahead! Press Space to attack.");

    door(h1,"A cozy cottage. The door is locked.");
    door(h2,"The carpenter is out today.");
    door(h3,"Someone is napping inside...");
    door(h4,"Home sweet home.");

    // field monsters in the open east area
    m->encounterEnemies = {1,2};
    m->bgmAsset = A_bgmV; m->darkness = 0;
    m->dayNight = true;       // slow day->night ambient cycle
    m->weather = 1;           // gentle rain

    // ===== Map 2: Whispering Cave (dark, torch-lit, tougher) =====
    auto cave = p->addMap("Whispering Cave", 32, 26);
    cave->tileset.assetId=A_ts; cave->tileset.tileWidth=32; cave->tileset.tileHeight=32;
    cave->tileset.columns=8; cave->tileset.rows=6;
    cave->animTiles={WATER,SHALLOW}; cave->bgmAsset=A_bgmC; cave->darkness=185;
    cave->encounterEnemies={3,2};
    Tilemap& cm=cave->tilemap; int CW=32,CH=26;
    auto cset=[&](int l,int x,int y,int t){cm.setTile(l,x,y,t);};
    auto cblk=[&](int x,int y,bool b=true){cm.setBlocked(x,y,b);};
    auto crect=[&](int l,int x,int y,int w,int h,int t){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)if(cm.inBounds(i,j))cset(l,i,j,t);};
    crect(0,0,0,CW,CH,COBBLE);
    for(int x=0;x<CW;x++){cset(1,x,0,ROCK);cblk(x,0);cset(1,x,CH-1,ROCK);cblk(x,CH-1);}
    for(int y=0;y<CH;y++){cset(1,0,y,ROCK);cblk(0,y);cset(1,CW-1,y,ROCK);cblk(CW-1,y);}
    int rocks[][2]={{6,6},{7,6},{6,7},{20,5},{21,5},{12,14},{13,14},{24,18},{25,18},{25,19},{9,18},{16,9},{10,11},{22,12}};
    for(auto&r:rocks){cset(1,r[0],r[1],ROCK);cblk(r[0],r[1]);}
    crect(0,17,11,5,4,WATER); for(int j=11;j<15;j++)for(int i=17;i<22;i++)cblk(i,j);
    crect(0,16,11,7,1,SHALLOW);
    cset(1,16,2,STAIRS);     // sealed gate (return)
    cset(1,16,23,STAIRS);    // entrance from village
    cset(1,26,6,LAMP);       // a lit brazier landmark near the lever
    cset(1,5,5,CHEST);
    cset(1,5,4,CHEST);       // crystal chest (revealed after the boss)
    int ceid=1; auto cev=[&](Event e){e.id=ceid++;cave->events.push_back(e);};
    // potion chest near entrance (once)
    { Event e;e.x=5;e.y=5;e.type=EventType::GiveItem;e.trigger=TriggerType::ActionButton;e.itemId=2;e.amount=2;e.once=true;e.text="A glint in the dark...|You found 2 Hi-Potions!"; cev(e); }
    // BOSS: stepping into the inner cavern summons the Cave Guardian (once).
    //       Defeating it sets switch 2 (clears the way + reveals the Crystal).
    { Event e;e.x=16;e.y=16;e.type=EventType::StartBattle;e.trigger=TriggerType::PlayerTouch;e.once=true;
      e.itemId=4;e.amount=1;e.switchId=2; cev(e); }
    // Crystal: only obtainable after the Guardian falls (switch 2). Sets switch 3.
    { Event e;e.x=5;e.y=4;e.type=EventType::GiveItem;e.trigger=TriggerType::ActionButton;e.once=true;
      e.itemId=4;e.amount=1;e.switchId=3;e.conditionSwitch=2;e.conditionValue=true;
      e.text="The stolen Crystal!|You reclaim it.|Carry it back through the gate to the village."; cev(e); }
    // Guardian still alive -> the Crystal pedestal won't budge (event page on same tile)
    { Event e;e.x=5;e.y=4;e.type=EventType::Message;e.trigger=TriggerType::ActionButton;
      e.text="A crystal pedestal, sealed by the Guardian's magic."; cev(e); }
    // return gate: opens once the Guardian is defeated (switch 2)
    { Event e;e.x=16;e.y=2;e.type=EventType::Teleport;e.trigger=TriggerType::ActionButton;e.targetMap=m->id;e.targetX=22;e.targetY=3;e.conditionSwitch=2;e.conditionValue=true; cev(e); }
    { cset(1,14,2,SIGN);cblk(14,2); Event e;e.x=14;e.y=2;e.type=EventType::Message;e.trigger=TriggerType::ActionButton;e.text="A sealed gate.|It will open when the cave's guardian is slain."; cev(e); }
    // lost miner npc (wanders)
    { Event e;e.x=9;e.y=20;e.type=EventType::Message;e.trigger=TriggerType::ActionButton;e.graphicAsset=A_v2;e.wander=true;e.text="Miner: A Guardian hoards the Crystal deeper in!|Strike it down to pass."; cev(e); }

    // village -> cave entrance (top of the road)
    setT(1,22,2,STAIRS);
    { Event e;e.id=eid++;e.x=22;e.y=2;e.type=EventType::Teleport;e.trigger=TriggerType::ActionButton;e.targetMap=cave->id;e.targetX=16;e.targetY=23; m->events.push_back(e); }
    sign(20,3,"Cave entrance ->|Beware what whispers within.");

    p->startMap=m->id; p->startX=22; p->startY=20; p->startActor=1; p->playerSprite=A_hero;
    p->save();
    printf("Built '%s': village(%d ev) + cave(%d ev) at %s\n", p->name.c_str(), (int)m->events.size(), (int)cave->events.size(), out.c_str());
    return 0;
}
