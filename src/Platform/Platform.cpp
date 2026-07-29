#include "Platform.h"
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdlib>
#else
#include <unistd.h>
#include <fstream>
#endif

namespace ssp::Platform {

// ============================================================================
// Executable directory
// ============================================================================

std::filesystem::path GetExecutableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(buf).parent_path();
    }
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        return std::filesystem::path(buf).parent_path();
    }
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        return std::filesystem::path(buf).parent_path();
    }
#endif
    return std::filesystem::current_path();
}

// ============================================================================
// Clipboard (GLFW does this, but we provide a non-GLFW fallback)
// ============================================================================

// Note: when using GLFW, clipboard is handled through glfwSetClipboardString.
// This function is a fallback for non-GLFW contexts.

static std::string WideToUtf8(std::wstring_view ws) {
    std::string result;
    result.reserve(ws.size() * 3);
    for (size_t i = 0; i < ws.size(); ) {
        char32_t cp = static_cast<char32_t>(ws[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < ws.size()) {
            char32_t lo = static_cast<char32_t>(ws[i+1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = ((cp - 0xD800) << 10) | (lo - 0xDC00);
                cp += 0x10000;
                i++;
            }
        }
        i++;
        if (cp < 0x80) { result += static_cast<char>(cp); }
        else if (cp < 0x800) { result += static_cast<char>(0xC0 | (cp >> 6)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { result += static_cast<char>(0xE0 | (cp >> 12)); result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { result += static_cast<char>(0xF0 | (cp >> 18)); result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    return result;
}

void CopyToClipboard(std::wstring_view text) {
    std::string utf8 = WideToUtf8(text);
#ifdef _WIN32
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, utf8.size() + 1);
    if (hMem) {
        memcpy(GlobalLock(hMem), utf8.c_str(), utf8.size() + 1);
        GlobalUnlock(hMem);
        SetClipboardData(CF_TEXT, hMem);
    }
    CloseClipboard();
#else
    // On Linux/macOS without GLFW, we can't easily set clipboard.
    // The GLFW path in Panel.cpp handles this via glfwSetClipboardString.
    // This is a no-op fallback.
    (void)utf8;
#endif
}

// ============================================================================
// Dark mode detection (best-effort)
// ============================================================================

bool IsDarkMode() {
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value == 0;
        }
        RegCloseKey(hKey);
    }
#elif defined(__APPLE__)
    // Check macOS appearance via defaults
    FILE* f = popen("defaults read -g AppleInterfaceStyle 2>/dev/null", "r");
    if (f) {
        char buf[16] = {};
        fread(buf, 1, sizeof(buf) - 1, f);
        pclose(f);
        return strstr(buf, "Dark") != nullptr;
    }
#else
    // Try XDG desktop portal or GTK settings
    const char* gtkTheme = std::getenv("GTK_THEME");
    if (gtkTheme && strstr(gtkTheme, "dark")) return true;
    // Check gsettings
    FILE* f = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (f) {
        char buf[64] = {};
        fread(buf, 1, sizeof(buf) - 1, f);
        pclose(f);
        if (strstr(buf, "dark") || strstr(buf, "prefer-dark")) return true;
    }
#endif
    return false; // Default to light
}

// ============================================================================
// Accent color
// ============================================================================

uint32_t GetAccentColor() {
#ifdef _WIN32
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\DWM",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD value = 0;
        DWORD size = sizeof(value);
        if (RegQueryValueExW(hKey, L"AccentColor", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&value), &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            // DWM stores as ABGR; convert to ARGB
            return ((value & 0xFF) << 16) | (value & 0xFF00) | ((value >> 16) & 0xFF) | 0xFF000000;
        }
        RegCloseKey(hKey);
    }
#endif
    return 0xFF0078D4; // Default blue accent
}

} // namespace ssp::Platform
