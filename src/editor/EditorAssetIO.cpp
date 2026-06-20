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

// Import one image file into the project. An animated GIF is unpacked into a
// horizontal sprite-strip with frame metadata (so it becomes a ready motion);
// any other raylib-loadable image is registered as-is. Returns the asset id.
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
            fs::path base = fs::path(path).stem();
            int n = 1; fs::path dest;
            do { dest = fs::path(p.dir)/"assets"/(base.string()+(n>1?("_"+std::to_string(n)):std::string())+".png"); n++; }
            while (fs::exists(dest));
            ExportImage(strip, dest.string().c_str());
            UnloadImage(strip); UnloadImage(anim);
            std::string rel = (fs::path("assets")/dest.filename()).generic_string();
            int id = p.assets.addExisting(AssetType::Image, base.string(), rel);
            p.assets.setAnim(id, frames, 12);
            setStatus(TextFormat("움짤 등록됨: %s (%d프레임)", base.string().c_str(), frames));
            return id;
        }
        if (anim.data) UnloadImage(anim);       // static gif -> register normally
    }
    int id = p.assets.registerAsset(p.dir, path, AssetType::Image);
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
    fs::create_directories(fs::path(p.dir) / "assets");
    int added = 0;
    for (const std::string& src : files) {
        std::string ext = fs::path(src).extension().string();
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (!isImageExt(ext)) continue;
        int n = 1; fs::path dest;
        do { dest = fs::path(p.dir) / "assets" / ("import_" + std::to_string(n++) + ext); }
        while (fs::exists(dest));
        if (!plat::copyFileUtf8(src, dest.string())) continue;   // Unicode-safe copy
        if (importImageFile(dest.string()) >= 0) added++;        // GIF-aware, ASCII path
    }
    if (added > 0) { p.save(); setStatus(TextFormat("이미지 %d개 불러옴", added)); }
    else setStatus("불러온 이미지가 없습니다.");
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
