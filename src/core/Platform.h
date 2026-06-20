#pragma once
// Tiny platform shim so main.cpp can show a native message box and catch hard
// crashes WITHOUT including <windows.h> (which clashes with raylib's symbols
// such as Rectangle / CloseWindow / ShowCursor). The Windows bits live in
// Platform.cpp, which never includes raylib.
#include <string>
#include <vector>

namespace plat {

// Show a modal error popup (no-op on platforms without one).
void popup(const char* title, const char* message);

// Install an unhandled-exception filter. On a hard crash (e.g. access
// violation) the callback is invoked with the OS exception code, then the
// process terminates.
void installCrashHandler(void (*onCrash)(unsigned long code));

// Open the NATIVE OS file picker (Windows Explorer dialog) to choose one or
// more image files. Returns the chosen paths as UTF-8. Empty if cancelled or
// unsupported on this platform.
std::vector<std::string> openImageFiles();

// Same as openImageFiles() but filtered to audio files (wav/ogg/mp3/flac…).
std::vector<std::string> openAudioFiles();

// Copy a file using UTF-8 paths (Unicode-safe on Windows). Returns true on success.
bool copyFileUtf8(const std::string& src, const std::string& dst);

} // namespace plat
