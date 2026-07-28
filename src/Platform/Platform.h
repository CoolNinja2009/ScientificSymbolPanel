#pragma once
#include <string>
#include <string_view>
#include <filesystem>

namespace ssp::Platform {

// ============================================================================
// System info
// ============================================================================

// Check if system is in dark mode
bool IsDarkMode();

// Get system accent color (returns 0xFF0078D4 on platforms where unavailable)
uint32_t GetAccentColor();

// Get DPI scale factor for the given GLFW window
float GetDpiScale(void* sdlWindow);

// Check if this is a low-end machine (disable animations)
bool IsLowEndMachine();

// ============================================================================
// Input
// ============================================================================

// Send Unicode text to the currently focused application
void SendUnicodeText(std::wstring_view text);

// ============================================================================
// Paths
// ============================================================================

// Get the platform-specific data directory for settings/favorites/etc.
std::filesystem::path DataDir();

// Get the directory containing the executable
std::filesystem::path ExeDir();

// Find a usable font file. Tries: assets/fonts/ relative to exe, then system paths.
std::filesystem::path FindFontFile();

// ============================================================================
// Startup
// ============================================================================

// Add/remove from OS startup
bool SetStartupWithWindows(bool enable);

// ============================================================================
// Global hotkey
// ============================================================================

// Register a system-wide hotkey. Calls `callback` from a background thread.
// Returns true on success.
using HotkeyCallback = void(*)();
bool RegisterGlobalHotkey(int modifiers, int key, HotkeyCallback callback);
void UnregisterGlobalHotkey();

} // namespace ssp::Platform
