#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>

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
    std::wstring symbol;        // The glyph itself, e.g. L"π"
    char32_t codepoint = 0;     // Unicode codepoint
    std::wstring name;          // Display name, e.g. L"Greek Small Letter Pi"
    std::vector<std::wstring> aliases;   // e.g. {"pi", "3.14159"}
    std::vector<std::wstring> keywords;  // e.g. {"math", "circle", "ratio"}
    Category category = Category::Miscellaneous;
    std::wstring latex;         // LaTeX equivalent, e.g. L"\\pi"
    std::wstring htmlEntity;    // HTML entity, e.g. L"&pi;"
    std::wstring description;   // Human-readable description
};

// ============================================================================
// Search Result
// ============================================================================
struct SearchResult {
    const Symbol* symbol = nullptr;
    int32_t score = 0;          // Higher = better match
};

// ============================================================================
// Snippet
// ============================================================================
struct Snippet {
    std::wstring name;          // e.g. L"Ohm's Law"
    std::wstring text;          // e.g. L"V = IR"
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
    uint32_t bgPrimary     = 0xFF1E1E1E;   // Main background
    uint32_t bgSecondary   = 0xFF2D2D2D;   // Search bar, cards
    uint32_t bgTertiary    = 0xFF3D3D3D;   // Hover, active
    uint32_t textPrimary   = 0xFFFFFFFF;   // Main text
    uint32_t textSecondary = 0xFFAAAAAA;   // Subtle text
    uint32_t textMuted     = 0xFF666666;   // Muted text
    uint32_t accent        = 0xFF0078D4;   // Windows accent blue
    uint32_t border        = 0xFF404040;   // Border color
    uint32_t hover         = 0xFF3A3A3A;   // Hover state
    uint32_t selected      = 0xFF3A3A3A;   // Selected state
    uint32_t focusBorder   = 0xFF0078D4;   // Focus ring
    uint32_t scrollbar     = 0xFF555555;   // Scrollbar thumb
    uint32_t scrollbarBg   = 0xFF2D2D2D;   // Scrollbar track
};

inline ThemeColors DarkTheme() {
    return {};
}

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
    // Hotkey
    uint32_t hotkeyModifiers = 0x0001;  // MOD_ALT
    uint32_t hotkeyVk        = 0x41;    // 'A'

    // Window
    int32_t windowX = -1;               // -1 = center
    int32_t windowY = -1;
    int32_t windowWidth  = 360;
    int32_t windowHeight = 480;

    // Behavior
    ThemeMode theme       = ThemeMode::System;
    bool animations       = true;
    bool startWithWindows = false;
    int32_t maxRecent     = 100;

    // Search
    bool fuzzySearch = true;
};

// ============================================================================
// Geometry types
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
// Window message identifiers
// ============================================================================
constexpr UINT WM_SSP_ACTIVATE   = WM_APP + 1;  // Show/hide window
constexpr UINT WM_SSP_INSERT     = WM_APP + 2;  // Insert symbol request
constexpr UINT WM_SSP_THEME_CHANGED = WM_APP + 3;
// ============================================================================
// Constants
// ============================================================================
constexpr float kSearchBarHeight    = 40.0f;
constexpr float kCategoryBarHeight  = 36.0f;
constexpr float kSymbolCellSize     = 48.0f;
constexpr float kFontSizeSymbol     = 22.0f;
constexpr float kFontSizeSmall      = 12.0f;
constexpr float kFontSizeBody       = 14.0f;
constexpr float kFontSizeTitle      = 16.0f;
constexpr float kFontSizeSearch     = 14.0f;
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
