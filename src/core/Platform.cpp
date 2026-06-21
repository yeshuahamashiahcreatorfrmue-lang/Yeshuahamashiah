// Platform-specific implementation. On Windows this includes <windows.h>; it
// deliberately does NOT include raylib.h to avoid symbol clashes.
#include "core/Platform.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <imm.h>

namespace {
void (*g_onCrash)(unsigned long) = nullptr;
LONG WINAPI sehFilter(EXCEPTION_POINTERS* ep) {
    if (g_onCrash)
        g_onCrash(ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0);
    return EXCEPTION_EXECUTE_HANDLER;
}
// Paths flow through the engine as std::string in the system ANSI code page
// (CP_ACP) — that is what argv, std::filesystem and raylib's fopen all use here.
// So convert dialog paths to/from CP_ACP (NOT UTF-8); using UTF-8 made
// std::filesystem throw "no mapping for the Unicode character" on machines whose
// user name / path contains non-ASCII (e.g. a Korean Windows account).
std::string toNarrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
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
// Shared multi-select open dialog. `filter` is a Win32 double-NUL-terminated
// filter spec; `title` is the dialog caption.
static std::vector<std::string> openFiles(const wchar_t* filter, const wchar_t* title) {
    std::vector<std::string> out;
    static wchar_t buf[16384]; buf[0] = 0;
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = GetActiveWindow();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = 16384;
    ofn.lpstrTitle  = title;
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return out;        // cancelled
    std::wstring first = buf;
    wchar_t* pp = buf + first.size() + 1;
    if (*pp == 0) {                                  // single file -> `first` is the full path
        out.push_back(toNarrow(first));
    } else {                                         // multi -> `first` is the dir, then file names
        while (*pp) {
            std::wstring fn = pp;
            out.push_back(toNarrow(first + L"\\" + fn));
            pp += fn.size() + 1;
        }
    }
    return out;
}
std::vector<std::string> openImageFiles() {
    return openFiles(L"이미지 파일\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tga;*.qoi;*.psd;*.hdr\0모든 파일\0*.*\0",
                     L"이미지 불러오기");
}
std::vector<std::string> openAudioFiles() {
    return openFiles(L"오디오 파일\0*.wav;*.ogg;*.mp3;*.flac;*.qoa;*.xm;*.mod\0모든 파일\0*.*\0",
                     L"사운드 불러오기");
}
std::vector<std::string> openMapFiles() {
    return openFiles(L"맵 파일\0*.json\0모든 파일\0*.*\0", L"맵 파일 불러오기");
}
bool copyFileUtf8(const std::string& src, const std::string& dst) {
    return CopyFileW(toWide(src).c_str(), toWide(dst).c_str(), FALSE) != 0;
}
void setImeEnabled(bool enabled) {
    static bool s_state = true;          // windows start with IME associated
    static HIMC s_saved = nullptr;       // detached context to restore on enable
    if (enabled == s_state) return;      // cheap: only act on a real change
    HWND hwnd = GetActiveWindow();
    if (!hwnd) return;
    if (enabled) {
        // Re-attach the IME so composition works again (e.g. focusing a text box).
        ImmAssociateContext(hwnd, s_saved ? s_saved : ImmGetContext(hwnd));
        s_saved = nullptr;
    } else {
        // Detach the IME from the window: keys flow as normal WM_KEYDOWN events.
        HIMC prev = ImmAssociateContext(hwnd, NULL);
        if (prev) s_saved = prev;
    }
    s_state = enabled;
}
} // namespace plat

#else  // non-Windows: harmless stubs
#include <filesystem>

namespace plat {
void popup(const char*, const char*) {}
void installCrashHandler(void (*)(unsigned long)) {}
std::vector<std::string> openImageFiles() { return {}; }   // native dialog is Windows-only
std::vector<std::string> openAudioFiles() { return {}; }   // native dialog is Windows-only
std::vector<std::string> openMapFiles()   { return {}; }   // native dialog is Windows-only
bool copyFileUtf8(const std::string& src, const std::string& dst) {
    std::error_code ec;
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}
void setImeEnabled(bool) {}   // IME control is Windows-only
} // namespace plat

#endif
