#pragma once
// Tiny platform shim so main.cpp can show a native message box and catch hard
// crashes WITHOUT including <windows.h> (which clashes with raylib's symbols
// such as Rectangle / CloseWindow / ShowCursor). The Windows bits live in
// Platform.cpp, which never includes raylib.
namespace plat {

// Show a modal error popup (no-op on platforms without one).
void popup(const char* title, const char* message);

// Install an unhandled-exception filter. On a hard crash (e.g. access
// violation) the callback is invoked with the OS exception code, then the
// process terminates.
void installCrashHandler(void (*onCrash)(unsigned long code));

} // namespace plat
