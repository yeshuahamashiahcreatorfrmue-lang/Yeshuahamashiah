// Tsukuru Engine entry point — with heavy startup diagnostics.
// If anything goes wrong before/while the window opens, we (a) pop up a message
// box with the real reason, and (b) write tsukuru_log.txt to several locations
// (next to the exe, the Desktop, and the temp folder) so it is always findable.
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include "raylib.h"
#include "core/Engine.h"
#include "core/Platform.h"

namespace fs = std::filesystem;

// ------------------------------------------------------------------ logging ---
static std::vector<FILE*> g_logs;
static std::string        g_lastStage = "startup";
static char               g_buf[2048];

static void logRaw(const char* line) {
    for (FILE* f : g_logs) { fputs(line, f); fputc('\n', f); fflush(f); }
    fputs(line, stderr); fputc('\n', stderr);
}
static void logf(const char* fmt, ...) {
    va_list a; va_start(a, fmt); vsnprintf(g_buf, sizeof(g_buf), fmt, a); va_end(a);
    logRaw(g_buf);
}
static void stage(const char* s) { g_lastStage = s; logf("STAGE: %s", s); }
static void popup(const char* title, const char* msg) { plat::popup(title, msg); }

static void onCrash(unsigned long code) {
    logf("FATAL CRASH: exception code 0x%08lx during stage '%s'", code, g_lastStage.c_str());
    snprintf(g_buf, sizeof(g_buf),
             "Tsukuru Engine crashed.\n\nStage: %s\nCode: 0x%08lx\n\n"
             "이 내용과 tsukuru_log.txt 파일을 보내주세요.",
             g_lastStage.c_str(), code);
    popup("Tsukuru Engine - Crash", g_buf);
}

// raylib routes all of its internal logging here. We mirror it to the files and,
// on a FATAL message (e.g. no OpenGL), show it to the user before raylib exits.
static void rayLog(int level, const char* fmt, va_list args) {
    char body[1600];
    vsnprintf(body, sizeof(body), fmt, args);
    const char* tag = level >= LOG_FATAL ? "FATAL" : level >= LOG_ERROR ? "ERROR"
                    : level >= LOG_WARNING ? "WARN " : "INFO ";
    logf("[%s] %s", tag, body);
    if (level >= LOG_FATAL) {
        snprintf(g_buf, sizeof(g_buf),
                 "그래픽 초기화에 실패했습니다 (OpenGL).\n\n%s\n\n"
                 "해결: 동봉된 opengl32.dll 과 libgallium_wgl.dll 을\n"
                 "exe 와 같은 폴더에 복사한 뒤 다시 실행해 보세요.", body);
        popup("Tsukuru Engine - Graphics Error", g_buf);
    }
}

static void openLogs(const fs::path& exeDir) {
    std::vector<fs::path> targets;
    targets.push_back(exeDir / "tsukuru_log.txt");
    if (const char* up = getenv("USERPROFILE")) targets.push_back(fs::path(up) / "Desktop" / "tsukuru_log.txt");
    if (const char* tp = getenv("TEMP"))        targets.push_back(fs::path(tp) / "tsukuru_log.txt");
    if (const char* hp = getenv("HOME"))        targets.push_back(fs::path(hp) / "tsukuru_log.txt");
    targets.push_back("tsukuru_log.txt"); // current dir, last resort
    for (const auto& t : targets) {
        std::error_code ec; fs::create_directories(t.parent_path(), ec);
        if (FILE* f = fopen(t.string().c_str(), "w")) g_logs.push_back(f);
    }
}

int main(int argc, char** argv) {
    plat::installCrashHandler(onCrash);
    fs::path base;
    try { base = fs::path(GetApplicationDirectory()); } catch (...) { base = "."; }

    openLogs(base);
    logf("===== Tsukuru Engine diagnostic log =====");
    logf("AppDir: %s", base.string().c_str());
    logf("Log written to %d location(s).", (int)g_logs.size());

    SetTraceLogCallback(rayLog);
    SetTraceLogLevel(LOG_ALL);

    stage("change working directory");
    ChangeDirectory(base.string().c_str());

    stage("parse args");
    int maxFrames = 0;
    std::string projectDir, startMode, shotPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames" && i + 1 < argc) maxFrames = atoi(argv[++i]);
        else if (a == "--start" && i + 1 < argc) startMode = argv[++i];
        else if (a == "--shot" && i + 1 < argc) shotPath = argv[++i];
        else if (a.rfind("--", 0) != 0) projectDir = a;
    }

    stage("resolve project");
    if (projectDir.empty()) {
        std::error_code ec;
        fs::path sample = base / "projects" / "sample_rpg";
        if (fs::exists(sample / "project.json", ec)) projectDir = sample.string();
        else if (fs::exists(fs::path("projects") / "sample_rpg" / "project.json", ec))
            projectDir = (fs::path("projects") / "sample_rpg").string();
        else projectDir = (base / "projects" / "my_rpg").string();
    }
    logf("Project: %s", projectDir.c_str());

    try {
        stage("construct engine");
        tsukuru::Engine engine;
        if (startMode == "play")  engine.setStartMode(tsukuru::Mode::Play);
        else if (startMode == "title") engine.setStartMode(tsukuru::Mode::Title);
        else if (startMode == "editor") engine.setStartMode(tsukuru::Mode::Editor);
        if (!shotPath.empty()) engine.setScreenshot(shotPath);

        stage("run engine (InitWindow + main loop)");
        int rc = engine.run(projectDir, maxFrames);
        stage("clean exit");
        logf("Exited cleanly (rc=%d).", rc);
        for (FILE* f : g_logs) fclose(f);
        return rc;
    } catch (const std::exception& e) {
        logf("FATAL EXCEPTION during '%s': %s", g_lastStage.c_str(), e.what());
        snprintf(g_buf, sizeof(g_buf), "오류로 종료되었습니다.\n\nStage: %s\n%s\n\n"
                 "tsukuru_log.txt 를 보내주세요.", g_lastStage.c_str(), e.what());
        popup("Tsukuru Engine - Error", g_buf);
        for (FILE* f : g_logs) fclose(f);
        return 1;
    } catch (...) {
        logf("FATAL unknown exception during '%s'", g_lastStage.c_str());
        for (FILE* f : g_logs) fclose(f);
        return 2;
    }
}
