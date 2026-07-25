#include "Win32.h"
#include "../Core/Types.h"
#include "../Core/Log.h"
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <vector>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shcore.lib")

namespace ssp::Platform {

// ============================================================================
// Mica / Backdrop
// ============================================================================

// DWM window attribute for Mica (undocumented, Win11 22H2+)
#ifndef DWMWA_USE_HOSTBACKDROPBRUSH
#define DWMWA_USE_HOSTBACKDROPBRUSH 38
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

// DWM system backdrop types
enum DWM_SYSTEMBACKDROP_TYPE : int32_t {
    DWMSBT_AUTO    = 0,
    DWMSBT_NONE    = 1,
    DWMSBT_MAINWINDOW = 2,  // Mica
    DWMSBT_TRANSIENTWINDOW = 3,  // Acrylic
    DWMSBT_TABBEDWINDOW = 4,  // Mica Alt
};

bool ApplyBackdrop(HWND hwnd) {
    // Try Mica (Win11 22H2+)
    DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
        &backdrop, sizeof(backdrop));
    if (SUCCEEDED(hr)) {
        SSP_LOG_DEBUG("Mica backdrop applied");
        return true;
    }

    // Fallback: try older DWMWA_USE_HOSTBACKDROPBRUSH
    BOOL useBackdrop = TRUE;
    hr = DwmSetWindowAttribute(hwnd, DWMWA_USE_HOSTBACKDROPBRUSH,
        &useBackdrop, sizeof(useBackdrop));
    if (SUCCEEDED(hr)) {
        SSP_LOG_DEBUG("Host backdrop brush applied");
        return true;
    }

    // Final fallback: dark glass
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    hr = DwmEnableBlurBehindWindow(hwnd, &bb);
    SSP_LOG_DEBUG("Blur behind: 0x%08X", hr);
    return SUCCEEDED(hr);
}

bool ApplyRoundedCorners(HWND hwnd) {
    // Win11 only
    DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
        &pref, sizeof(pref));
    SSP_LOG_DEBUG("Rounded corners: 0x%08X", hr);
    return SUCCEEDED(hr);
}

void HideFromTaskbar(HWND hwnd) {
    // Set as tool window (no taskbar entry, no Alt+Tab)
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_TOOLWINDOW;
    exStyle &= ~WS_EX_APPWINDOW;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);

    // Also hide from Alt+Tab by setting as owned
    // We use a special trick: set the owner to a hidden helper window
    // But WS_EX_TOOLWINDOW + WS_EX_NOACTIVATE is usually sufficient
}

void SetTopmost(HWND hwnd, bool topmost) {
    SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

float GetDpiScale(HWND hwnd) {
    UINT dpi = GetDpiForWindow(hwnd);
    return static_cast<float>(dpi) / 96.0f;
}

// ============================================================================
// Startup
// ============================================================================

bool SetStartupWithWindows(bool enable) {
    HKEY hKey;
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (result != ERROR_SUCCESS) return false;

    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(path) + L"\" --minimized";
        result = RegSetValueExW(hKey, kAppName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(cmd.c_str()),
            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(hKey, kAppName);
    }

    RegCloseKey(hKey);
    return result == ERROR_SUCCESS;
}

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

// ============================================================================
// Input
// ============================================================================

void SendUnicodeText(std::wstring_view text) {
    // Build INPUT array — one key-down + key-up pair per character
    std::vector<INPUT> inputs;
    inputs.reserve(text.size() * 2);

    for (wchar_t ch : text) {
        // Skip nulls
        if (ch == L'\0') continue;

        // Surrogate pairs need special handling
        if (ch >= 0xD800 && ch <= 0xDFFF) {
            // Pass through as-is — the receiving app handles it
        }

        INPUT down = {};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = 0;
        down.ki.wScan = ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);

        INPUT up = down;
        up.ki.dwFlags |= KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }

    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
}

bool IsKeyDown(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// ============================================================================
// System info
// ============================================================================

bool IsWindows11() {
    // Check build number
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;

    auto RtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!RtlGetVersion) return false;

    RTL_OSVERSIONINFOW osvi = { sizeof(osvi) };
    if (RtlGetVersion(&osvi) != 0) return false;

    return osvi.dwBuildNumber >= 22000;
}

bool IsLowEndMachine() {
    // Check RAM and CPU cores
    MEMORYSTATUSEX mem = { sizeof(mem) };
    GlobalMemoryStatusEx(&mem);

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    // Low-end: < 4GB RAM or < 2 cores
    return mem.ullTotalPhys < 4ULL * 1024 * 1024 * 1024 ||
           si.dwNumberOfProcessors < 2;
}

uint32_t GetAccentColor() {
    DWORD color = 0x0078D4; // Default blue
    DWORD size = sizeof(color);
    RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\DWM",
        L"AccentColor",
        RRF_RT_DWORD,
        nullptr,
        &color,
        &size);
    // DWM stores as ABGR, convert to ARGB
    uint32_t argb = static_cast<uint32_t>(color);
    return ((argb & 0xFF000000)) |
           ((argb & 0x00FF0000) >> 16) |
           (argb & 0x0000FF00) |
           ((argb & 0x000000FF) << 16);
}

bool IsDarkMode() {
    DWORD value = 0;
    DWORD size = sizeof(value);
    LONG result = RegGetValueW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_DWORD,
        nullptr,
        &value,
        &size);
    if (result != ERROR_SUCCESS) return false;
    return value == 0;
}

} // namespace ssp::Platform
