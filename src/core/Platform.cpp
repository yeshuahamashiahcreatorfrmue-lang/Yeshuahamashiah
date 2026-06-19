// Platform-specific implementation. On Windows this includes <windows.h>; it
// deliberately does NOT include raylib.h to avoid symbol clashes.
#include "core/Platform.h"

#ifdef _WIN32
#include <windows.h>

namespace {
void (*g_onCrash)(unsigned long) = nullptr;
LONG WINAPI sehFilter(EXCEPTION_POINTERS* ep) {
    if (g_onCrash)
        g_onCrash(ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
    return EXCEPTION_EXECUTE_HANDLER;
}
}

namespace plat {
void popup(const char* title, const char* message) {
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONERROR | MB_TOPMOST);
}
void installCrashHandler(void (*onCrash)(unsigned long)) {
    g_onCrash = onCrash;
    SetUnhandledExceptionFilter(sehFilter);
}
} // namespace plat

#else  // non-Windows: harmless stubs

namespace plat {
void popup(const char*, const char*) {}
void installCrashHandler(void (*)(unsigned long)) {}
} // namespace plat

#endif
