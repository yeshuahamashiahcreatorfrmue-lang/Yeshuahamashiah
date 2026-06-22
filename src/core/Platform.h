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

// Pick a single map/data file (*.json) to register/import.
std::vector<std::string> openMapFiles();

// Copy a file using UTF-8 paths (Unicode-safe on Windows). Returns true on success.
bool copyFileUtf8(const std::string& src, const std::string& dst);

// Enable/disable the OS text IME (Korean/Japanese/Chinese composition) for the
// game window. When DISABLED, key presses (WASD, hotkeys) are delivered straight
// to the app instead of being swallowed by the IME for composition — so movement
// works even if the user left their input language set to Korean. We enable it
// only while an editor text field is focused (so names can be typed), and disable
// it everywhere else. No-op off Windows. Cheap to call every frame (state-cached).
void setImeEnabled(bool enabled);

// Returns the IME's current in-progress composition string (UTF-8) — the
// half-composed Hangul/CJK syllable that has been typed but not yet committed,
// or "" if nothing is being composed. Text fields append it live so each
// 자음/모음 shows immediately instead of one syllable behind. The committed text
// still arrives normally through raylib's GetCharPressed. Empty off Windows
// (where no OS IME composition is intercepted).
std::string imeComposition();

} // namespace plat
