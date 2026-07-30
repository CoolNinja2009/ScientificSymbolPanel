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
} // namespace ssp::Platform
