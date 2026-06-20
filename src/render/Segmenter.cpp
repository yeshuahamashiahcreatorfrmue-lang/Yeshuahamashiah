// Segmenter implementation. Real inference only when TSUKURU_ONNX is defined and
// ONNX Runtime headers/libs are available at build time; otherwise a stub.
#include "render/Segmenter.h"

namespace tsukuru {

#ifdef TSUKURU_ONNX
} // close namespace so we can include ORT headers at file scope safely
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <algorithm>
#include <cmath>
namespace tsukuru {

struct Segmenter::Impl {
    Ort::Env env{ ORT_LOGGING_LEVEL_WARNING, "tsukuru" };
    std::unique_ptr<Ort::Session> session;
    Ort::AllocatorWithDefaultOptions alloc;
    std::string inName, outName;
    int side = 320;           // U^2-Net input is 320x320
    bool ok = false;
};

Segmenter::Segmenter() : impl_(new Impl) {}
Segmenter::~Segmenter() = default;

bool Segmenter::load(const std::string& modelPath) {
    try {
        Ort::SessionOptions so;
        so.SetIntraOpNumThreads(2);
        so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
#ifdef _WIN32
        std::wstring wp(modelPath.begin(), modelPath.end());   // ASCII model path -> wide
        impl_->session.reset(new Ort::Session(impl_->env, wp.c_str(), so));
#else
        impl_->session.reset(new Ort::Session(impl_->env, modelPath.c_str(), so));
#endif
        impl_->inName  = impl_->session->GetInputNameAllocated(0, impl_->alloc).get();
        impl_->outName = impl_->session->GetOutputNameAllocated(0, impl_->alloc).get();
        impl_->ok = true;
    } catch (...) { impl_->ok = false; }
    return impl_->ok;
}

bool Segmenter::ready() const { return impl_ && impl_->ok; }

bool Segmenter::cutout(Image& img) {
    if (!ready() || !img.data || img.width < 2 || img.height < 2) return false;
    try {
        const int S = impl_->side, W = img.width, H = img.height;
        // ---- preprocess: resize to SxS, normalise (ImageNet mean/std), CHW ----
        Image rs = ImageCopy(img);
        ImageFormat(&rs, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ImageResize(&rs, S, S);
        const Color* rp = (const Color*)rs.data;
        const float mean[3] = { 0.485f, 0.456f, 0.406f }, sd[3] = { 0.229f, 0.224f, 0.225f };
        std::vector<float> in((size_t)3 * S * S);
        for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
            const Color& c = rp[y*S + x];
            float r = c.r/255.0f, g = c.g/255.0f, b = c.b/255.0f;
            in[0*S*S + y*S + x] = (r - mean[0]) / sd[0];
            in[1*S*S + y*S + x] = (g - mean[1]) / sd[1];
            in[2*S*S + y*S + x] = (b - mean[2]) / sd[2];
        }
        UnloadImage(rs);

        std::array<int64_t,4> shape{ 1, 3, S, S };
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inT = Ort::Value::CreateTensor<float>(mem, in.data(), in.size(), shape.data(), shape.size());
        const char* inN[] = { impl_->inName.c_str() };
        const char* outN[] = { impl_->outName.c_str() };
        auto out = impl_->session->Run(Ort::RunOptions{nullptr}, inN, &inT, 1, outN, 1);
        const float* m = out[0].GetTensorData<float>();   // [1,1,S,S]

        // ---- normalise mask to 0..1 ----
        float mi = m[0], ma = m[0];
        for (int i = 1; i < S*S; ++i) { mi = std::min(mi, m[i]); ma = std::max(ma, m[i]); }
        float rng = (ma - mi) > 1e-6f ? (ma - mi) : 1.0f;

        // ---- build SxS grayscale mask, resize to original, apply as alpha ----
        Image mk = GenImageColor(S, S, BLACK);
        ImageFormat(&mk, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
        unsigned char* mg = (unsigned char*)mk.data;
        for (int i = 0; i < S*S; ++i) mg[i] = (unsigned char)(std::clamp((m[i]-mi)/rng, 0.0f, 1.0f) * 255.0f);
        ImageResize(&mk, W, H);
        const unsigned char* mr = (const unsigned char*)mk.data;

        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        Color* px = (Color*)img.data;
        for (int i = 0; i < W*H; ++i) {
            int a = (px[i].a * mr[i]) / 255;        // multiply existing alpha by the matte
            px[i].a = (unsigned char)a;
        }
        UnloadImage(mk);
        return true;
    } catch (...) { return false; }
}

#else  // ---- no ONNX at build time: stub that always declines ----

struct Segmenter::Impl {};
Segmenter::Segmenter() {}
Segmenter::~Segmenter() = default;
bool Segmenter::load(const std::string&) { return false; }
bool Segmenter::ready() const { return false; }
bool Segmenter::cutout(Image&) { return false; }

#endif

} // namespace tsukuru
