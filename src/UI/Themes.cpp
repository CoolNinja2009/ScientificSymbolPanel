#include "Themes.h"

namespace ssp {

ThemeManager::ThemeManager(bool isDarkMode, uint32_t accentColor)
    : m_isDark(isDarkMode)
    , m_accentColor(accentColor)
{
    Rebuild();
}

void ThemeManager::SetDarkMode(bool isDark) {
    if (m_isDark == isDark) return;
    m_isDark = isDark;
    Rebuild();
}

void ThemeManager::SetAccentColor(uint32_t color) {
    if (m_accentColor == color) return;
    m_accentColor = color;
    Rebuild();
}

void ThemeManager::Rebuild() {
    m_colors = m_isDark ? DarkTheme() : LightTheme();
    m_colors.accent = m_accentColor;
}

} // namespace ssp
