// EditorAssetIO: drag-and-drop registration of images (incl. GIF) + audio.
#include "editor/Editor.h"
#include "editor/Prefabs.h"
#include "editor/EditorInternal.h"
#include "core/Engine.h"
#include "render/UI.h"
#include "core/Text.h"
#include "render/AssetGen.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;
namespace tsukuru {


// raylib-supported image / audio extensions (broadened so any common image —
// and animated GIFs — can be registered and used on the map).
static bool isImageExt(const std::string& e) {
    static const char* k[] = { ".png",".bmp",".tga",".jpg",".jpeg",".gif",".qoi",
        ".psd",".hdr",".dds",".ktx",".astc",".pkm",".pvr",".pic",".ppm",".pgm" };
    for (auto* s : k) if (e == s) return true;
    return false;
}
static bool isAudioExt(const std::string& e) {
    static const char* k[] = { ".wav",".ogg",".mp3",".flac",".qoa",".xm",".mod" };
    for (auto* s : k) if (e == s) return true;
    return false;
}

void Editor::handleAssetDrop() {
    if (!IsFileDropped()) return;
    FilePathList dropped = LoadDroppedFiles();
    Project& p = engine_.project();
    for (unsigned i = 0; i < dropped.count; ++i) {
        std::string path = dropped.paths[i];
        std::string ext = GetFileExtension(path.c_str() ? path.c_str() : "");
        for (auto& c : ext) c = (char)tolower(c);

        if (ext == ".gif") {                       // animated GIF -> sprite-sheet
            int frames = 1;
            Image anim = LoadImageAnim(path.c_str(), &frames);
            if (anim.data && frames > 1) {
                // raylib stores the `frames` consecutively in anim.data (the
                // Image height is one frame). Repack into a horizontal strip.
                int fw = anim.width, fh = anim.height;     // single-frame size
                int frameBytes = GetPixelDataSize(fw, fh, anim.format);
                Image strip = GenImageColor(fw * frames, fh, BLANK);
                ImageFormat(&strip, anim.format);
                for (int f = 0; f < frames; ++f) {
                    Image one = anim;                       // shallow view of frame f
                    one.data = (unsigned char*)anim.data + (size_t)f * frameBytes;
                    Rectangle src = { 0, 0, (float)fw, (float)fh };
                    Rectangle dst = { (float)(f*fw), 0, (float)fw, (float)fh };
                    ImageDraw(&strip, one, src, dst, WHITE);
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
                continue;
            }
            if (anim.data) UnloadImage(anim);       // static gif -> fall through
        }

        AssetType type;
        if (isImageExt(ext))      type = AssetType::Image;
        else if (isAudioExt(ext)) type = AssetType::Audio;
        else continue;
        int id = p.assets.registerAsset(p.dir, path, type);
        if (id >= 0) setStatus("등록됨: " + std::string(GetFileName(path.c_str())));
    }
    UnloadDroppedFiles(dropped);
    p.save();
}

// ============================ draw ============================

} // namespace tsukuru
