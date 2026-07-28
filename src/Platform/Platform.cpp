#include "Platform/Platform.h"
#include <SDL.h>
#include "Core/Log.h"
#include "Core/Types.h"
#include <algorithm>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "dwmapi.lib")
#else
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#endif

namespace ssp::Platform {

// ============================================================================
// System info
// ============================================================================

bool IsDarkMode() {
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0, size = sizeof(value);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
        return value == 0;
    }
    return false;
#elif defined(__APPLE__)
    // macOS: check AppleInterfaceStyle
    FILE* fp = popen("defaults read -g AppleInterfaceStyle 2>/dev/null", "r");
    if (fp) {
        char buf[16] = {};
        fread(buf, 1, sizeof(buf) - 1, fp);
        pclose(fp);
        return std::string(buf).find("Dark") != std::string::npos;
    }
    return false;
#else
    // Linux: check gsettings or GTK theme
    const char* gtk = std::getenv("GTK_THEME");
    if (gtk) {
        std::string s(gtk);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s.find("dark") != std::string::npos;
    }
    return false;
#endif
}

uint32_t GetAccentColor() {
#ifdef _WIN32
    DWORD color = 0;
    DWORD size = sizeof(color);
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExW(hKey, L"AccentColor", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&color), &size);
        RegCloseKey(hKey);
    }
    if (color == 0) {
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExW(hKey, L"AccentColorMenu", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(&color), &size);
            RegCloseKey(hKey);
        }
    }
    if (color == 0) return 0xFF0078D4; // default Windows blue

    // DWM stores as ABGR, convert to ARGB
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t r = (color >> 0) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    return (a << 24) | (r << 16) | (g << 8) | b;
#else
    return 0xFF0078D4;
#endif
}

float GetDpiScale(void* sdlWindow) {
#ifdef _WIN32
    if (sdlWindow) {
        (void)sdlWindow;
        return 1.0f;  // TODO: proper DPI via platform APIs
    }
    return 1.0f;
#else
    (void)glfwWindow;
    return 1.0f;
#endif
}

bool IsLowEndMachine() {
#ifdef _WIN32
    MEMORYSTATUSEX mem = { sizeof(mem) };
    GlobalMemoryStatusEx(&mem);
    return mem.ullTotalPhys < 4ULL * 1024 * 1024 * 1024; // < 4 GB
#else
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageSize > 0)
        return static_cast<uint64_t>(pages) * pageSize < 4ULL * 1024 * 1024 * 1024;
    return false;
#endif
}

// ============================================================================
// Input
// ============================================================================

void SendUnicodeText(std::wstring_view text) {
    if (text.empty()) return;
#ifdef _WIN32
    for (size_t i = 0; i < text.size(); ) {
        INPUT inputs[2] = {};
        wchar_t ch = text[i];
        if (IS_HIGH_SURROGATE(ch) && i + 1 < text.size()) {
            inputs[0].type = INPUT_KEYBOARD;
            // Send as Unicode packet
            inputs[0].ki.wVk = 0;
            inputs[0].ki.wScan = static_cast<WORD>(text[i]);
            inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[0].ki.time = 0;
            inputs[1] = inputs[0];
            inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
            i += 2;
        } else {
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = 0;
            inputs[0].ki.wScan = ch;
            inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;
            inputs[0].ki.time = 0;
            inputs[1] = inputs[0];
            inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
            SendInput(2, inputs, sizeof(INPUT));
            i++;
        }
    }
#elif defined(__APPLE__)
    // macOS: use CGEventPost (requires linking CoreGraphics)
    // Not implemented — stub
    SSP_LOG_DEBUG("SendUnicodeText: not implemented on macOS");
#else
    // Linux: use XTest or uinput
    // Not implemented — stub
    SSP_LOG_DEBUG("SendUnicodeText: not implemented on Linux");
#endif
}

// ============================================================================
// Paths
// ============================================================================

std::filesystem::path ExeDir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
#elif defined(__APPLE__)
    char path[1024];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0)
        return std::filesystem::path(path).parent_path();
    return std::filesystem::current_path();
#else
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len > 0) {
        path[len] = '\0';
        return std::filesystem::path(path).parent_path();
    }
    return std::filesystem::current_path();
#endif
}

std::filesystem::path DataDir() {
#ifdef _WIN32
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        auto p = std::filesystem::path(localAppData) / kDataSubdir;
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }
    return std::filesystem::current_path() / kDataSubdir;
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    auto p = std::filesystem::path(home) / "Library" / "Application Support" / std::string("ScientificSymbolPanel");
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    std::filesystem::path base = xdg ? std::filesystem::path(xdg)
                                     : std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") / ".local" / "share";
    auto p = base / "ScientificSymbolPanel";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
#endif
}

std::filesystem::path FindFontFile() {
    // 1. Check assets/fonts/ relative to exe
    auto exe = ExeDir();
    auto test = exe / "assets" / "fonts" / "DejaVuSans.ttf";
    if (std::filesystem::exists(test)) return test;
    test = exe / ".." / "assets" / "fonts" / "DejaVuSans.ttf";
    if (std::filesystem::exists(test)) return test;

    // 2. Check system font paths
#ifdef _WIN32
    test = std::filesystem::path("C:\\Windows\\Fonts\\segoeui.ttf");
    if (std::filesystem::exists(test)) return test;
    test = std::filesystem::path("C:\\Windows\\Fonts\\arial.ttf");
    if (std::filesystem::exists(test)) return test;
#elif defined(__APPLE__)
    test = "/System/Library/Fonts/SF-Pro.ttf";
    if (std::filesystem::exists(test)) return test;
    test = "/Library/Fonts/Arial.ttf";
    if (std::filesystem::exists(test)) return test;
#else
    test = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    if (std::filesystem::exists(test)) return test;
    test = "/usr/share/fonts/TTF/DejaVuSans.ttf";
    if (std::filesystem::exists(test)) return test;
#endif
    return {};
}

// ============================================================================
// Startup
// ============================================================================

bool SetStartupWithWindows(bool enable) {
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            auto exe = ExeDir() / "SSP.exe";
            RegSetValueExW(hKey, L"ScientificSymbolPanel", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(exe.c_str()),
                           static_cast<DWORD>((exe.wstring().size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, L"ScientificSymbolPanel");
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
#elif defined(__APPLE__)
    // macOS: create/remove LaunchAgent plist
    return false;
#else
    // Linux: create/remove ~/.config/autostart/ .desktop file
    return false;
#endif
}

// ============================================================================
// Global hotkey
// ============================================================================

static std::thread s_hotkeyThread;
static std::atomic<bool> s_hotkeyRunning{false};
static HotkeyCallback s_hotkeyCallback = nullptr;
static int s_hotkeyMod = 0;
static int s_hotkeyKey = 0;

#ifdef _WIN32

static LRESULT CALLBACK HotkeyWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_HOTKEY && wp == 1 && s_hotkeyCallback) {
        s_hotkeyCallback();
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void HotkeyThread() {
    // Create a message-only window for global hotkey
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HotkeyWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SSP_HotkeyWindow";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, L"SSP_HotkeyWindow", L"", 0,
                                 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    if (!hwnd) {
        SSP_LOG_DEBUG("Platform: failed to create hotkey message window");
        return;
    }

    // Map SDL modifiers to Win32
    UINT fsMod = 0;
    if (s_hotkeyMod & 1) fsMod |= MOD_SHIFT;
    if (s_hotkeyMod & 2) fsMod |= MOD_CONTROL;
    if (s_hotkeyMod & 4) fsMod |= MOD_ALT;
    if (s_hotkeyMod & 8) fsMod |= MOD_WIN;

    // Map GLFW key to VK
    int vk = 0;
    if (s_hotkeyKey >= SDLK_a && s_hotkeyKey <= SDLK_z)
        vk = 'A' + (s_hotkeyKey - SDLK_a);
    else if (s_hotkeyKey >= SDLK_F1 && s_hotkeyKey <= SDLK_F12)
        vk = VK_F1 + (s_hotkeyKey - SDLK_F1);
    else
        vk = s_hotkeyKey;

    if (!RegisterHotKey(hwnd, 1, fsMod, vk)) {
        SSP_LOG_DEBUG("Platform: RegisterHotKey failed: %lu", GetLastError());
        DestroyWindow(hwnd);
        return;
    }

    SSP_LOG_DEBUG("Platform: global hotkey registered (mod=%d, key=%d)", s_hotkeyMod, s_hotkeyKey);

    MSG msg;
    while (s_hotkeyRunning && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(hwnd, 1);
    DestroyWindow(hwnd);
}

#else

static void HotkeyThread() {
    // Global hotkey not yet implemented on non-Windows platforms.
    // On Linux, use X11 key grabbing; on macOS, use CGEvent.
    SSP_LOG_DEBUG("Platform: global hotkey thread started (stub on non-Windows)");
    while (s_hotkeyRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

#endif

bool RegisterGlobalHotkey(int modifiers, int key, HotkeyCallback callback) {
    if (s_hotkeyRunning) UnregisterGlobalHotkey();

    s_hotkeyMod = modifiers;
    s_hotkeyKey = key;
    s_hotkeyCallback = callback;
    s_hotkeyRunning = true;
    s_hotkeyThread = std::thread(HotkeyThread);
    return true;
}

void UnregisterGlobalHotkey() {
    s_hotkeyRunning = false;
    if (s_hotkeyThread.joinable()) {
        s_hotkeyThread.join();
    }
    s_hotkeyCallback = nullptr;
}

} // namespace ssp::Platform
