#pragma once
#include <windows.h>
#include <string>
#include <string_view>

namespace ssp::Platform {

// ============================================================================
// Window helpers
// ============================================================================

// Apply DWM backdrop (Mica on Win11 22H2+, Acrylic fallback)
bool ApplyBackdrop(HWND hwnd);

// Apply rounded corners (Win11 only)
bool ApplyRoundedCorners(HWND hwnd);

// Hide window from taskbar, Alt+Tab, etc.
void HideFromTaskbar(HWND hwnd);

// Make window topmost
void SetTopmost(HWND hwnd, bool topmost);

// Get DPI for window
float GetDpiScale(HWND hwnd);

// ============================================================================
// Startup
// ============================================================================

// Add/remove from Windows startup registry
bool SetStartupWithWindows(bool enable);

// Check if current user has admin rights
bool IsAdmin();

// ============================================================================
// Input
// ============================================================================

// Send Unicode text to the currently focused control via SendInput
// Does NOT touch the clipboard.
void SendUnicodeText(std::wstring_view text);

// Check if a key is currently pressed
bool IsKeyDown(int vk);

// ============================================================================
// System info
// ============================================================================

// Check if we're on Windows 11 (build >= 22000)
bool IsWindows11();

// Check if we're on a low-end machine (disable animations)
bool IsLowEndMachine();

// Get Windows accent color
uint32_t GetAccentColor();

// Check if system is in dark mode
bool IsDarkMode();

} // namespace ssp::Platform
