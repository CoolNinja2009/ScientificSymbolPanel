#pragma once
#include <string>
#include <string_view>
#include <filesystem>
#include <cstdint>

namespace ssp::Platform {

// Get the directory containing the executable
std::filesystem::path GetExecutableDir();

// Copy text to system clipboard
void CopyToClipboard(std::wstring_view text);

// True if OS is in dark mode (best-effort)
bool IsDarkMode();

// OS accent color, or default blue (0xFF0078D4)
uint32_t GetAccentColor();

} // namespace ssp::Platform
