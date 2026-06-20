#pragma once
// Segmenter: AI subject/background separation (like a phone's "lift subject").
// Wraps a U^2-Net ONNX model via ONNX Runtime. Entirely optional and lazily
// loaded: if the runtime or model file is missing, ready() is false and callers
// fall back to the classic colour-based removal. Compiled to a no-op stub unless
// TSUKURU_ONNX is defined at build time.
#include <memory>
#include <string>
#include "raylib.h"

namespace tsukuru {

class Segmenter {
public:
    Segmenter();
    ~Segmenter();

    // Load the .onnx model. Returns true if the model is ready for inference.
    bool load(const std::string& modelPath);
    bool ready() const;

    // Replace `img`'s alpha with the model's foreground matte (soft edges), like
    // an automatic subject cut-out. Returns true on success; false => caller
    // should use the fallback. `img` is left untouched on failure.
    bool cutout(Image& img);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tsukuru
