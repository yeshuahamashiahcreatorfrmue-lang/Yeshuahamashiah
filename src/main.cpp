// Tsukuru Engine entry point.
// Double-clicking the executable boots straight into the editor with the sample
// project loaded. Press F5 to test-play, ESC to return to the editor.
#include <string>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include "raylib.h"
#include "core/Engine.h"

namespace fs = std::filesystem;

// ---- crash-proof file logging -------------------------------------------------
// With -mwindows there is no console, so any startup error (e.g. no OpenGL 3.x)
// would make the window vanish silently. We mirror raylib's log to a text file
// next to the executable so problems are always diagnosable.
static FILE* g_log = nullptr;

static void FileLog(int level, const char* fmt, va_list args) {
    if (g_log) {
        const char* tag = level >= LOG_ERROR ? "ERROR" : level >= LOG_WARNING ? "WARN " : "INFO ";
        fprintf(g_log, "[%s] ", tag);
        vfprintf(g_log, fmt, args);
        fputc('\n', g_log);
        fflush(g_log);
    }
    va_list copy; va_copy(copy, args);
    vfprintf(stderr, fmt, copy);
    fputc('\n', stderr);
    va_end(copy);
}

int main(int argc, char** argv) {
    fs::path base = fs::path(GetApplicationDirectory());

    // Open the log file next to the exe (fall back to current dir).
    g_log = fopen((base / "tsukuru_log.txt").string().c_str(), "w");
    if (!g_log) g_log = fopen("tsukuru_log.txt", "w");
    SetTraceLogCallback(FileLog);
    SetTraceLogLevel(LOG_ALL);
    TraceLog(LOG_INFO, "Tsukuru Engine starting. AppDir: %s", base.string().c_str());

    // Make the executable's folder the working directory so the bundled
    // assets/ and projects/ are always found, even when launched from elsewhere.
    ChangeDirectory(base.string().c_str());

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

    if (projectDir.empty()) {
        // Prefer the bundled sample project; otherwise create a new one.
        fs::path sample = base / "projects" / "sample_rpg";
        std::error_code ec;
        if (fs::exists(sample / "project.json", ec)) projectDir = sample.string();
        else if (fs::exists(fs::path("projects") / "sample_rpg" / "project.json", ec))
            projectDir = (fs::path("projects") / "sample_rpg").string();
        else projectDir = (base / "projects" / "my_rpg").string();
    }
    TraceLog(LOG_INFO, "Using project: %s", projectDir.c_str());

    try {
        tsukuru::Engine engine;
        if (startMode == "play")  engine.setStartMode(tsukuru::Mode::Play);
        else if (startMode == "title") engine.setStartMode(tsukuru::Mode::Title);
        else if (startMode == "editor") engine.setStartMode(tsukuru::Mode::Editor);
        if (!shotPath.empty()) engine.setScreenshot(shotPath);
        int rc = engine.run(projectDir, maxFrames);
        TraceLog(LOG_INFO, "Exited cleanly (rc=%d).", rc);
        if (g_log) fclose(g_log);
        return rc;
    } catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "FATAL EXCEPTION: %s", e.what());
        if (g_log) fclose(g_log);
        return 1;
    } catch (...) {
        TraceLog(LOG_ERROR, "FATAL: unknown exception");
        if (g_log) fclose(g_log);
        return 2;
    }
}
