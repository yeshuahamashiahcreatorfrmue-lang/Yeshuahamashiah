// Tsukuru Engine entry point.
// Double-clicking the executable boots straight into the editor with the sample
// project loaded. Press F5 to test-play, ESC to return to the editor.
#include <string>
#include <cstdlib>
#include <filesystem>
#include "raylib.h"
#include "core/Engine.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    // Resolve paths relative to the executable so it runs from anywhere.
    fs::path base = fs::path(GetApplicationDirectory());

    int maxFrames = 0;
    std::string projectDir, startMode, shotPath;
    // Parse args: [projectDir] [--frames N] [--start editor|title|play] [--shot file.png]
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) maxFrames = atoi(argv[++i]);
        else if (a == "--start" && i + 1 < argc) startMode = argv[++i];
        else if (a == "--shot" && i + 1 < argc) shotPath = argv[++i];
        else if (a.rfind("--", 0) != 0) projectDir = a;
    }

    if (!projectDir.empty()) {
        // explicit project dir provided
    } else {
        // Prefer the bundled sample project; otherwise create a new one.
        fs::path sample = base / "projects" / "sample_rpg";
        if (fs::exists(sample / "project.json")) projectDir = sample.string();
        else {
            fs::path local = fs::path("projects") / "sample_rpg";
            if (fs::exists(local / "project.json")) projectDir = local.string();
            else projectDir = (base / "projects" / "my_rpg").string();
        }
    }

    tsukuru::Engine engine;
    if (startMode == "play")  engine.setStartMode(tsukuru::Mode::Play);
    else if (startMode == "title") engine.setStartMode(tsukuru::Mode::Title);
    else if (startMode == "editor") engine.setStartMode(tsukuru::Mode::Editor);
    if (!shotPath.empty()) engine.setScreenshot(shotPath);
    return engine.run(projectDir, maxFrames);
}
