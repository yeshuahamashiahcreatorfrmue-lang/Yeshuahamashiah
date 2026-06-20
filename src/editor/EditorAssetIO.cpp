// EditorAssetIO: drag-and-drop registration of images (incl. GIF) + audio.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "core/Platform.h"
#include "render/UI.h"
#include "core/Text.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {


// raylib-supported image / audio extensions (broadened so any common image —
// and animated GIFs — can be registered and used on the map).
bool Editor::isImageExt(const std::string& e) {
    static const char* k[] = { ".png",".bmp",".tga",".jpg",".jpeg",".gif",".qoi",
        ".psd",".hdr",".dds",".ktx",".astc",".pkm",".pvr",".pic",".ppm",".pgm" };
    for (auto* s : k) if (e == s) return true;
    return false;
}
bool Editor::isAudioExt(const std::string& e) {
    static const char* k[] = { ".wav",".ogg",".mp3",".flac",".qoa",".xm",".mod" };
    for (auto* s : k) if (e == s) return true;
    return false;
}

// Auto-trim a single-colour border: if the four corners are the same solid
// colour, key that colour to transparent; then crop away the fully-transparent
// margins so only the picture remains. Returns true if it changed the image.
static bool autoRemoveBg(Image& img) {
    if (!img.data || img.width < 3 || img.height < 3) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int W = img.width, H = img.height;
    Color* px = (Color*)img.data;
    Color c0 = px[0], c1 = px[W-1], c2 = px[(H-1)*W], c3 = px[(H-1)*W + (W-1)];
    auto rgbClose = [](Color a, Color b, int t){
        return std::abs(a.r-b.r) + std::abs(a.g-b.g) + std::abs(a.b-b.b) <= t;
    };
    bool transparentCorners = (c0.a==0 && c1.a==0 && c2.a==0 && c3.a==0);
    bool uniformColor = c0.a>0 && rgbClose(c0,c1,28) && rgbClose(c0,c2,28) && rgbClose(c0,c3,28);
    if (!transparentCorners && !uniformColor) return false;     // border isn't a single colour
    if (uniformColor) {                                          // key the background colour out
        const int tol = 42;
        for (int i = 0; i < W*H; ++i)
            if (px[i].a > 0 && rgbClose(px[i], c0, tol)) px[i] = Color{0,0,0,0};
    }
    int minx=W, miny=H, maxx=-1, maxy=-1;                        // bounding box of opaque pixels
    for (int y=0;y<H;++y) for (int x=0;x<W;++x)
        if (px[y*W+x].a > 0) { if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y; }
    if (maxx < minx) return true;                                // whole image was background
    if (minx>0 || miny>0 || maxx<W-1 || maxy<H-1)
        ImageCrop(&img, { (float)minx, (float)miny, (float)(maxx-minx+1), (float)(maxy-miny+1) });
    return true;
}

// Import one image file into the project. An animated GIF is unpacked into a
// horizontal sprite-strip; a static image gets its single-colour background and
// outer margin auto-removed, then saved as PNG. Returns the asset id.
int Editor::importImageFile(const std::string& path) {
    Project& p = engine_.project();
    std::string ext = GetFileExtension(path.c_str() ? path.c_str() : "");
    for (auto& c : ext) c = (char)tolower(c);

    if (ext == ".gif") {                       // animated GIF -> sprite-strip
        int frames = 1;
        Image anim = LoadImageAnim(path.c_str(), &frames);
        if (anim.data && frames > 1) {
            int fw = anim.width, fh = anim.height;            // single-frame size
            int frameBytes = GetPixelDataSize(fw, fh, anim.format);
            Image strip = GenImageColor(fw * frames, fh, BLANK);
            ImageFormat(&strip, anim.format);
            for (int f = 0; f < frames; ++f) {
                Image one = anim;                              // shallow view of frame f
                one.data = (unsigned char*)anim.data + (size_t)f * frameBytes;
                ImageDraw(&strip, one, { 0,0,(float)fw,(float)fh }, { (float)(f*fw),0,(float)fw,(float)fh }, WHITE);
            }
            fs::create_directories(fs::path(p.dir) / "assets");
            int n = 1; fs::path dest;
            do { dest = fs::path(p.dir)/"assets"/("import_"+std::to_string(n++)+".png"); } while (fs::exists(dest));
            ExportImage(strip, dest.string().c_str());
            UnloadImage(strip); UnloadImage(anim);
            std::string rel = (fs::path("assets")/dest.filename()).generic_string();
            int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
            p.assets.setAnim(id, frames, 12);
            setStatus(TextFormat("움짤 등록됨 (%d프레임)", frames));
            return id;
        }
        if (anim.data) UnloadImage(anim);       // static gif -> handle as a normal image
    }
    // static image: auto-remove a single-colour background + outer margin, save PNG
    fs::create_directories(fs::path(p.dir) / "assets");
    Image img = LoadImage(path.c_str());
    if (img.data) {
        bool trimmed = autoRemoveBg(img);
        int n = 1; fs::path dest;
        do { dest = fs::path(p.dir)/"assets"/("import_"+std::to_string(n++)+".png"); } while (fs::exists(dest));
        ExportImage(img, dest.string().c_str());
        UnloadImage(img);
        std::string rel = (fs::path("assets")/dest.filename()).generic_string();
        int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
        setStatus(trimmed ? "이미지 등록 (배경·여백 자동 제거)" : "이미지 등록됨");
        return id;
    }
    int id = p.assets.registerAsset(p.dir, path, AssetType::Image);   // undecodable -> keep as-is
    if (id >= 0) setStatus("이미지 등록됨: " + std::string(GetFileName(path.c_str())));
    return id;
}

void Editor::handleAssetDrop() {
    if (!IsFileDropped()) return;
    FilePathList dropped = LoadDroppedFiles();
    Project& p = engine_.project();
    for (unsigned i = 0; i < dropped.count; ++i) {
        std::string path = dropped.paths[i];
        std::string ext = GetFileExtension(path.c_str() ? path.c_str() : "");
        for (auto& c : ext) c = (char)tolower(c);
        if (isImageExt(ext)) {
            importImageFile(path);               // GIF-aware, all image formats
        } else if (isAudioExt(ext)) {
            int id = p.assets.registerAsset(p.dir, path, AssetType::Audio);
            if (id >= 0) setStatus("사운드 등록됨: " + std::string(GetFileName(path.c_str())));
        }
    }
    UnloadDroppedFiles(dropped);
    p.save();
}

// Open the native OS file picker (Windows Explorer) and import every selected
// image. Source files are copied with a Unicode-safe copy into the project under
// ASCII names, so Korean/Unicode source paths and filenames work correctly.
void Editor::pickAndImportImages() {
    std::vector<std::string> files = plat::openImageFiles();
    if (files.empty()) { setStatus("불러오기 취소됨."); return; }
    Project& p = engine_.project();
    int added = 0;
    // Guard every file: a bad/unconvertible path must never crash the engine.
    for (const std::string& src : files) {
        try {
            std::error_code ec;
            std::string ext = fs::path(src).extension().string();
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            if (!isImageExt(ext)) continue;
            fs::create_directories(fs::path(p.dir) / "assets", ec);
            // Stage to an ASCII temp file (in-project, system encoding), import it
            // (importImageFile writes its own PNG asset), then delete the staging copy.
            fs::path tmp = fs::path(p.dir) / "assets" / ("_staging" + ext);
            int k = 1;
            while (fs::exists(tmp, ec)) tmp = fs::path(p.dir) / "assets" / ("_staging" + std::to_string(k++) + ext);
            if (!plat::copyFileUtf8(src, tmp.string())) continue;
            if (importImageFile(tmp.string()) >= 0) added++;
            fs::remove(tmp, ec);
        } catch (const std::exception& e) {
            setStatus(std::string("불러오기 실패: ") + e.what());
        }
    }
    if (added > 0) { p.save(); setStatus(TextFormat("이미지 %d개 불러옴", added)); }
    else if (added == 0) setStatus("불러온 이미지가 없습니다.");
}

// Make a solid/single-colour (e.g. white) background transparent. The top-left
// corner pixel is taken as the background colour; pixels within a tolerance of it
// become transparent. Saves a NEW asset (non-destructive); animated strips keep
// their frame metadata.
int Editor::makeTransparentBg(int assetId) {
    Project& p = engine_.project();
    const AssetEntry* e = p.assets.find(assetId);
    if (!e) return -1;
    Image img = LoadImage(p.assetFullPath(assetId).c_str());
    if (!img.data) { setStatus("배경 제거 실패: 이미지를 열 수 없음"); return -1; }
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int N = img.width * img.height;
    if (N <= 0) { UnloadImage(img); return -1; }
    Color* px = (Color*)img.data;
    Color bg = px[0];                                   // top-left = background
    const int tol = 44;
    for (int i = 0; i < N; ++i) {
        if (px[i].a == 0) continue;
        int d = std::abs(px[i].r - bg.r) + std::abs(px[i].g - bg.g) + std::abs(px[i].b - bg.b);
        if (d <= tol) px[i] = Color{ 0, 0, 0, 0 };
    }
    fs::create_directories(fs::path(p.dir) / "assets");
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir) / "assets" / ("nobg_" + std::to_string(assetId) + "_" + std::to_string(n++) + ".png"); }
    while (fs::exists(dest));
    ExportImage(img, dest.string().c_str());
    UnloadImage(img);
    std::string rel = (fs::path("assets") / dest.filename()).generic_string();
    int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
    if (e->frames > 1) p.assets.setAnim(id, e->frames, e->fps);   // preserve strip frames
    p.save();
    setStatus("배경 제거된 이미지 생성: " + dest.stem().string());
    return id;
}

// ============================ draw ============================

} // namespace tsukuru
