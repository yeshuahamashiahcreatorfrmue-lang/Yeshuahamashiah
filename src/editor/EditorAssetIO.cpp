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
#include <fstream>
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

// Per-channel max colour difference (0..255). Matches how image editors measure
// "tolerance" — more intuitive and precise than a summed distance.
static int chDiff(Color a, Color b) {
    int r = std::abs(a.r-b.r), g = std::abs(a.g-b.g), bl = std::abs(a.b-b.b);
    return std::max(r, std::max(g, bl));
}

// Detect the background colour the way a commercial tool does: sample the whole
// border ring, histogram it (quantised), and take the dominant colour. `conf` is
// the fraction of the border that colour covers — high conf => a genuine
// single-colour background (works even if the corners differ or the subject
// touches an edge). Returns false on an empty image.
static bool detectBg(const Color* px, int W, int H, Color& bg, float& conf) {
    std::vector<int> cnt(4096, 0);
    std::vector<long> sr(4096,0), sg(4096,0), sb(4096,0);
    int total = 0;
    auto add = [&](Color c){ int k = (c.r>>4)*256 + (c.g>>4)*16 + (c.b>>4);
                             cnt[k]++; sr[k]+=c.r; sg[k]+=c.g; sb[k]+=c.b; total++; };
    for (int x = 0; x < W; ++x) { add(px[x]); add(px[(H-1)*W+x]); }
    for (int y = 0; y < H; ++y) { add(px[y*W]); add(px[y*W+W-1]); }
    if (total == 0) return false;
    int best = 0; for (int k = 1; k < 4096; ++k) if (cnt[k] > cnt[best]) best = k;
    if (cnt[best] == 0) return false;
    bg = Color{ (unsigned char)(sr[best]/cnt[best]), (unsigned char)(sg[best]/cnt[best]),
                (unsigned char)(sb[best]/cnt[best]), 255 };
    int near = 0;
    auto chk = [&](Color c){ if (chDiff(c, bg) <= 24) near++; };
    for (int x = 0; x < W; ++x) { chk(px[x]); chk(px[(H-1)*W+x]); }
    for (int y = 0; y < H; ++y) { chk(px[y*W]); chk(px[y*W+W-1]); }
    conf = (float)near / (float)total;
    return true;
}

// Magic-wand background removal: flood-fill from the image EDGES through pixels
// within `tolSoft` of the background colour, so only the border-connected
// background is touched (interior same-colour pixels are kept). Pixels within
// `tolHard` go fully transparent; the thin tolHard..tolSoft band gets partial
// alpha (anti-aliased edge / defringe). Tolerances are per-channel (chDiff).
static int floodFillBorder(Color* px, int W, int H, Color bg, int tolHard, int tolSoft) {
    std::vector<unsigned char> vis(W * H, 0);
    std::vector<int> stk; stk.reserve(W + H);
    auto seed = [&](int x, int y) {
        int i = y*W + x;
        if (!vis[i] && px[i].a > 0 && chDiff(px[i], bg) <= tolSoft) { vis[i] = 1; stk.push_back(i); }
    };
    for (int x = 0; x < W; ++x) { seed(x, 0); seed(x, H-1); }
    for (int y = 0; y < H; ++y) { seed(0, y); seed(W-1, y); }
    int cleared = 0;
    float span = (float)std::max(1, tolSoft - tolHard);
    while (!stk.empty()) {
        int i = stk.back(); stk.pop_back();
        int d = chDiff(px[i], bg);
        if (d <= tolHard) { px[i] = Color{ 0, 0, 0, 0 }; ++cleared; }   // solid background
        else {                                                          // thin anti-aliased fringe
            unsigned char na = (unsigned char)(px[i].a * ((float)(d - tolHard) / span));
            px[i].a = na;
            if (na == 0) ++cleared;
        }
        int x = i % W, y = i / W;
        if (x > 0)   seed(x-1, y);
        if (x < W-1) seed(x+1, y);
        if (y > 0)   seed(x, y-1);
        if (y < H-1) seed(x, y+1);
    }
    return cleared;
}

// Crop an RGBA image to the bounding box of its opaque pixels.
static void cropToOpaque(Image& img) {
    int W = img.width, H = img.height;
    Color* px = (Color*)img.data;
    int minx=W, miny=H, maxx=-1, maxy=-1;
    for (int y=0;y<H;++y) for (int x=0;x<W;++x)
        if (px[y*W+x].a > 0) { if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y; }
    if (maxx < minx) return;
    if (minx>0 || miny>0 || maxx<W-1 || maxy<H-1)
        ImageCrop(&img, { (float)minx, (float)miny, (float)(maxx-minx+1), (float)(maxy-miny+1) });
}

// Auto background removal on import. Detects whether the image sits on a single
// colour background (border histogram) — even without an obvious frame — and if
// so removes the border-connected background and crops. Busy/photographic
// borders (low confidence) are left untouched. Returns true if it processed.
static bool autoRemoveBg(Image& img) {
    if (!img.data || img.width < 3 || img.height < 3) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    int W = img.width, H = img.height;
    Color* px = (Color*)img.data;
    Color c0 = px[0], c1 = px[W-1], c2 = px[(H-1)*W], c3 = px[(H-1)*W + (W-1)];
    if (c0.a==0 && c1.a==0 && c2.a==0 && c3.a==0) { cropToOpaque(img); return true; } // already transparent
    Color bg; float conf;
    if (!detectBg(px, W, H, bg, conf)) return false;
    if (conf < 0.55f) return false;                 // border isn't predominantly one colour -> leave it
    floodFillBorder(px, W, H, bg, 40, 72);          // tight, precise, thin AA edge
    cropToOpaque(img);
    return true;
}

// AI subject cut-out: ask the neural model (U^2-Net) for a foreground matte and
// crop to it. Phone-grade "lift subject" — works on photos/busy backgrounds the
// colour-based remover can't handle. Returns false (caller falls back) when the
// model isn't loaded or declines. Only meaningful for single images, not strips.
bool Editor::aiCutout(Image& img) {
    if (!seg_.ready() || !img.data || img.width < 2 || img.height < 2) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (!seg_.cutout(img)) return false;
    cropToOpaque(img);
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
        bool ai = aiCutout(img);                 // phone-grade subject lift (if model present)
        bool trimmed = ai || autoRemoveBg(img);  // else classic colour-based removal
        int n = 1; fs::path dest;
        do { dest = fs::path(p.dir)/"assets"/("import_"+std::to_string(n++)+".png"); } while (fs::exists(dest));
        ExportImage(img, dest.string().c_str());
        UnloadImage(img);
        std::string rel = (fs::path("assets")/dest.filename()).generic_string();
        int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
        setStatus(ai ? "이미지 등록 (AI 배경 제거)"
                     : trimmed ? "이미지 등록 (배경·여백 자동 제거)" : "이미지 등록됨");
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

// Import external image(s) and assign them as the pending skill's effect.
//   • ONE file  → used as-is; a horizontal sprite-strip's frame count is auto-
//     detected (width == N×height → N square frames). GIFs keep their frames.
//   • MANY files → each picture becomes one consecutive frame, composited (with
//     its background removed, centred) into a single horizontal strip — exactly
//     like assembling a character's walk motion, but for a skill effect.
// The 반복(회) panel setting then replays the finished strip 1/3/7… times.
void Editor::pickAndImportEffect() {
    FieldSkill* s = pendingEffectSkill_;
    pendingEffectSkill_ = nullptr;
    if (!s) return;
    std::vector<std::string> files = plat::openImageFiles();
    if (files.empty()) { setStatus("이펙트 불러오기 취소됨."); return; }
    Project& p = engine_.project();
    std::error_code ec;

    // keep only images, ordered by filename so frame_01, frame_02… line up
    std::vector<std::string> imgs;
    for (auto& f : files) {
        std::string ext = fs::path(f).extension().string();
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (isImageExt(ext)) imgs.push_back(f);
    }
    if (imgs.empty()) { setStatus("이미지 파일이 아닙니다."); return; }
    std::sort(imgs.begin(), imgs.end());

    auto stagingPath = [&](const std::string& src) {
        std::string ext = fs::path(src).extension().string();
        fs::path tmp = fs::path(p.dir) / "assets" / ("_fxstaging" + ext);
        int k = 1;
        while (fs::exists(tmp, ec)) tmp = fs::path(p.dir)/"assets"/("_fxstaging"+std::to_string(k++)+ext);
        return tmp;
    };

    try {
        fs::create_directories(fs::path(p.dir) / "assets", ec);

        // ---- single file: reuse the importer + strip auto-detect ----
        if (imgs.size() == 1) {
            fs::path tmp = stagingPath(imgs[0]);
            if (!plat::copyFileUtf8(imgs[0], tmp.string())) { setStatus("이펙트 복사 실패."); return; }
            int id = importImageFile(tmp.string());
            fs::remove(tmp, ec);
            if (id < 0) { setStatus("이펙트 불러오기 실패."); return; }
            const AssetEntry* ae = p.assets.find(id);
            if (ae && ae->frames <= 1) {
                Image img = LoadImage(p.assetFullPath(id).c_str());
                if (img.data) {
                    if (img.height > 0 && img.width % img.height == 0) {
                        int n = img.width / img.height;
                        if (n >= 2 && n <= 32) p.assets.setAnim(id, n, 12);
                    }
                    UnloadImage(img);
                }
            }
            s->effectAsset = id; s->effectLoops = std::max(1, s->effectLoops);
            const AssetEntry* fin = p.assets.find(id);
            p.save();
            setStatus(TextFormat("이펙트 적용됨 (%d프레임)", fin ? fin->frames : 1));
            return;
        }

        // ---- many files: composite each picture into one frame strip ----
        std::vector<Image> frames;
        int cw = 0, ch = 0;
        for (auto& f : imgs) {
            fs::path tmp = stagingPath(f);
            if (!plat::copyFileUtf8(f, tmp.string())) continue;
            Image im = LoadImage(tmp.string().c_str());
            fs::remove(tmp, ec);
            if (!im.data) continue;
            ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
            if (!aiCutout(im)) autoRemoveBg(im);            // transparent bg per frame
            frames.push_back(im);
            cw = std::max(cw, im.width); ch = std::max(ch, im.height);
        }
        if (frames.empty()) { setStatus("이펙트 이미지를 읽지 못했습니다."); return; }
        int N = (int)frames.size();
        Image strip = GenImageColor(cw * N, ch, BLANK);
        ImageFormat(&strip, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        for (int i = 0; i < N; ++i) {
            Image& im = frames[i];
            float ox = i*cw + (cw - im.width)/2.0f, oy = (ch - im.height)/2.0f;   // centre in cell
            ImageDraw(&strip, im, {0,0,(float)im.width,(float)im.height},
                      {ox, oy, (float)im.width, (float)im.height}, WHITE);
            UnloadImage(im);
        }
        int n2 = 1; fs::path dest;
        do { dest = fs::path(p.dir)/"assets"/("effect_"+std::to_string(n2++)+".png"); } while (fs::exists(dest, ec));
        ExportImage(strip, dest.string().c_str());
        UnloadImage(strip);
        std::string rel = (fs::path("assets")/dest.filename()).generic_string();
        int id = p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
        p.assets.setAnim(id, N, 12);
        s->effectAsset = id; s->effectLoops = std::max(1, s->effectLoops);
        p.save();
        setStatus(TextFormat("이펙트 %d프레임 연속 등록됨", N));
    } catch (const std::exception& e) {
        setStatus(std::string("이펙트 불러오기 실패: ") + e.what());
    }
}

// Import an external image and use it as the ACTIVE MAP's tileset. Unlike the
// sprite importer this does NOT remove a background or crop — the tile grid must
// stay intact. Columns/rows are guessed from the image size (square tiles).
// Copy an external image into the project's assets/ (UTF-8 safe, no crop/bg
// removal so tile grids & character sheets stay intact) and register it.
// Returns the new asset id, or -1 on failure. Shared by the tileset & NPC pickers.
int Editor::stageImportImage(const std::string& src, const char* prefix) {
    Project& p = engine_.project();
    std::error_code ec;
    std::string ext = fs::path(src).extension().string();
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    if (!isImageExt(ext)) return -1;
    fs::create_directories(fs::path(p.dir) / "assets", ec);
    int n = 1; fs::path dest;
    do { dest = fs::path(p.dir)/"assets"/(std::string(prefix)+"_"+std::to_string(n++)+ext); } while (fs::exists(dest, ec));
    if (!plat::copyFileUtf8(src, dest.string())) return -1;
    std::string rel = (fs::path("assets")/dest.filename()).generic_string();
    return p.assets.addExisting(AssetType::Image, dest.stem().string(), rel);
}

void Editor::pickAndImportTileset() {
    auto m = activeMap();
    if (!m) { setStatus("먼저 맵을 선택하세요."); return; }
    std::vector<std::string> files = plat::openImageFiles();
    if (files.empty()) { setStatus("타일셋 불러오기 취소됨."); return; }
    int id = stageImportImage(files.front(), "tileset");
    if (id < 0) { setStatus("타일셋 불러오기 실패 (이미지 아님)."); return; }
    Project& p = engine_.project();
    m->tileset.assetId = id;
    // guess the grid from the image dimensions (assume square tiles)
    Image probe = LoadImage(p.assetFullPath(id).c_str());
    if (probe.data) {
        int tw = m->tileset.tileWidth  > 0 ? m->tileset.tileWidth  : 32;
        int th = m->tileset.tileHeight > 0 ? m->tileset.tileHeight : 32;
        m->tileset.columns = std::max(1, probe.width  / tw);
        m->tileset.rows    = std::max(1, probe.height / th);
        UnloadImage(probe);
    }
    p.save();
    setStatus(TextFormat("타일셋 적용됨 (%d×%d 칸)", m->tileset.columns, m->tileset.rows));
}

// Import an external audio file and assign it as the pending skill's sound.
void Editor::pickAndImportSound() {
    FieldSkill* s = pendingEffectSkill_;
    pendingEffectSkill_ = nullptr;
    if (!s) return;
    std::vector<std::string> files = plat::openAudioFiles();
    if (files.empty()) { setStatus("사운드 불러오기 취소됨."); return; }
    Project& p = engine_.project();
    int id = -1;
    for (auto& f : files) {
        std::string ext = fs::path(f).extension().string();
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (!isAudioExt(ext)) continue;
        id = p.assets.registerAsset(p.dir, f, AssetType::Audio);   // copies into the project
        if (id >= 0) break;
    }
    if (id < 0) { setStatus("사운드 불러오기 실패 (오디오 파일이 아님)."); return; }
    s->soundAsset = id;
    p.save();
    setStatus("사운드 적용됨: " + assetName(id));
}

// Make the BORDER background transparent (magic-wand flood-fill from the edges),
// so interior same-colour pixels are kept. The top-left corner is the background
// colour. Saves a NEW asset (non-destructive). Single images are cropped to the
// subject; animated strips keep their size/frames so frames stay aligned.
int Editor::makeTransparentBg(int assetId) {
    Project& p = engine_.project();
    const AssetEntry* e = p.assets.find(assetId);
    if (!e) return -1;
    Image img = LoadImage(p.assetFullPath(assetId).c_str());
    if (!img.data) { setStatus("배경 제거 실패: 이미지를 열 수 없음"); return -1; }
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (img.width <= 0 || img.height <= 0) { UnloadImage(img); return -1; }
    // Single images: try the AI subject cut-out first (handles photos / busy
    // backgrounds). Animated strips can't be AI-segmented as one scene, so they
    // always use the colour-based border removal that preserves the frame grid.
    bool ai = (e->frames <= 1) && aiCutout(img);
    if (!ai) {
        Color* px = (Color*)img.data;
        Color bg; float conf;                                    // robust background detection
        if (!detectBg(px, img.width, img.height, bg, conf)) bg = px[0];
        floodFillBorder(px, img.width, img.height, bg, 40, 72);  // border-connected, thin AA edge
        if (e->frames <= 1) cropToOpaque(img);                   // don't crop strips (keeps frame grid)
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
    setStatus((ai ? "AI 배경 제거 이미지 생성: " : "배경(테두리) 제거 이미지 생성: ") + dest.stem().string());
    return id;
}

// Import an external character image and assign it as an NPC's sprite. Imported
// as-is (no crop/bg-removal) so a 4-direction character sheet keeps its grid.
void Editor::pickAndImportNpcChar() {
    int evId = pendingNpcEventId_; pendingNpcEventId_ = -1;
    auto m = activeMap();
    if (!m || evId < 0) { setStatus("NPC 대상이 없습니다."); return; }
    Event* ev = nullptr; for (auto& e : m->events) if (e.id == evId) ev = &e;
    if (!ev) { setStatus("NPC를 찾지 못했습니다."); return; }
    std::vector<std::string> files = plat::openImageFiles();
    if (files.empty()) { setStatus("캐릭터 불러오기 취소됨."); return; }
    int id = stageImportImage(files.front(), "npc_char");
    if (id < 0) { setStatus("캐릭터 불러오기 실패 (이미지 아님)."); return; }
    ev->graphicAsset = id;
    engine_.project().save();
    setStatus("NPC 캐릭터 적용됨: " + assetName(id));
}

// Load a map .json from disk into mapPreview_ (World tab previews it before the
// user commits it to the project as a new map).
void Editor::pickAndImportMapFile() {
    std::vector<std::string> files = plat::openMapFiles();
    if (files.empty()) { setStatus("맵 불러오기 취소됨."); return; }
    try {
        std::ifstream in(files.front(), std::ios::binary);
        if (!in) { setStatus("맵 파일을 열 수 없습니다."); return; }
        nlohmann::json j; in >> j;
        auto mp = std::make_shared<Map>();
        mp->fromJson(j);
        if (mp->tilemap.width() <= 0 || mp->tilemap.height() <= 0) { setStatus("유효한 맵이 아닙니다."); return; }
        mapPreview_ = mp;
        mapPreviewName_ = fs::path(files.front()).filename().string();
        setStatus("맵 미리보기 로드됨: " + mapPreviewName_);
    } catch (const std::exception& e) {
        setStatus(std::string("맵 불러오기 실패: ") + e.what());
    }
}

// ============================ draw ============================

} // namespace tsukuru
