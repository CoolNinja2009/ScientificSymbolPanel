#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <cstddef>

namespace ssp {

// ============================================================================
// Symbol Categories
// ============================================================================
enum class Category : uint8_t {
    Mathematics,
    GreekLetters,
    Physics,
    Chemistry,
    Electronics,
    SIUnits,
    Logic,
    Programming,
    Arrows,
    Currency,
    Fractions,
    Superscripts,
    Subscripts,
    Statistics,
    Geometry,
    Calculus,
    Astronomy,
    Miscellaneous,
    Custom,
    COUNT
};

constexpr const char* CategoryNames[] = {
    "Mathematics",
    "Greek Letters",
    "Physics",
    "Chemistry",
    "Electronics",
    "SI Units",
    "Logic",
    "Programming",
    "Arrows",
    "Currency",
    "Fractions",
    "Superscripts",
    "Subscripts",
    "Statistics",
    "Geometry",
    "Calculus",
    "Astronomy",
    "Miscellaneous",
    "Custom"
};

static_assert(std::size(CategoryNames) == static_cast<size_t>(Category::COUNT));

// ============================================================================
// Symbol
// ============================================================================
struct Symbol {
    std::wstring symbol;
    char32_t codepoint = 0;
    std::wstring name;
    std::vector<std::wstring> aliases;
    std::vector<std::wstring> keywords;
    Category category = Category::Miscellaneous;
    std::wstring latex;
    std::wstring htmlEntity;
    std::wstring description;
};

// ============================================================================
// Search Result
// ============================================================================
struct SearchResult {
    const Symbol* symbol = nullptr;
    int32_t score = 0;
};

// ============================================================================
// Snippet
// ============================================================================
struct Snippet {
    std::wstring name;
    std::wstring text;
    std::wstring description;
    std::vector<std::wstring> aliases;
};

// ============================================================================
// Theme
// ============================================================================
enum class ThemeMode : uint8_t {
    System,
    Dark,
    Light
};

struct ThemeColors {
    uint32_t bgPrimary     = 0xFF1E1E1E;
    uint32_t bgSecondary   = 0xFF2D2D2D;
    uint32_t bgTertiary    = 0xFF3D3D3D;
    uint32_t textPrimary   = 0xFFFFFFFF;
    uint32_t textSecondary = 0xFFAAAAAA;
    uint32_t textMuted     = 0xFF666666;
    uint32_t accent        = 0xFF0078D4;
    uint32_t border        = 0xFF404040;
    uint32_t hover         = 0xFF3A3A3A;
    uint32_t selected      = 0xFF3A3A3A;
    uint32_t focusBorder   = 0xFF0078D4;
    uint32_t scrollbar     = 0xFF555555;
    uint32_t scrollbarBg   = 0xFF2D2D2D;
};

inline ThemeColors DarkTheme() { return {}; }

inline ThemeColors LightTheme() {
    return {
        .bgPrimary     = 0xFFF3F3F3,
        .bgSecondary   = 0xFFFFFFFF,
        .bgTertiary    = 0xFFE6E6E6,
        .textPrimary   = 0xFF1A1A1A,
        .textSecondary = 0xFF666666,
        .textMuted     = 0xFF999999,
        .accent        = 0xFF0078D4,
        .border        = 0xFFD1D1D1,
        .hover         = 0xFFE6E6E6,
        .selected      = 0xFFE6E6E6,
        .focusBorder   = 0xFF0078D4,
        .scrollbar     = 0xFFC1C1C1,
        .scrollbarBg   = 0xFFF3F3F3,
    };
}

// ============================================================================
// App Settings
// ============================================================================
struct AppSettings {
    uint32_t hotkeyModifiers = 0x0001;
    uint32_t hotkeyVk        = 0x41;
    int32_t windowX = -1;
    int32_t windowY = -1;
    int32_t windowWidth  = 360;
    int32_t windowHeight = 480;
    ThemeMode theme       = ThemeMode::System;
    bool animations       = true;
    bool startWithWindows = false;
    int32_t maxRecent     = 100;
    bool fuzzySearch = true;
};

// ============================================================================
// Geometry
// ============================================================================
struct RectF {
    float x = 0, y = 0, width = 0, height = 0;
};

struct PointF {
    float x = 0, y = 0;
};

struct SizeF {
    float width = 0, height = 0;
};

// ============================================================================
// Constants
// ============================================================================
constexpr float kSearchBarHeight    = 40.0f;
constexpr float kCategoryBarHeight  = 36.0f;
constexpr float kSymbolCellSize     = 48.0f;
constexpr float kCornerRadius       = 12.0f;
constexpr float kBorderWidth        = 1.0f;
constexpr float kAnimationDurationMs = 120.0f;

constexpr const wchar_t* kAppName        = L"Scientific Symbol Panel";
constexpr const wchar_t* kWindowClass    = L"SSP_MainWindow";
constexpr const wchar_t* kSettingsFile   = L"settings.json";
constexpr const wchar_t* kRecentFile     = L"recent.json";
constexpr const wchar_t* kFavoritesFile  = L"favorites.json";
constexpr const wchar_t* kSymbolsFile    = L"symbols.json";
constexpr const wchar_t* kSnippetsFile   = L"snippets.json";
constexpr const wchar_t* kDataSubdir     = L"ScientificSymbolPanel";

} // namespace ssp
