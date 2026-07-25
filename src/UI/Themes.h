#pragma once
#include "Core/Types.h"
#include <cstdint>

namespace ssp {

class ThemeManager {
public:
    ThemeManager(bool isDarkMode, uint32_t accentColor);

    const ThemeColors& GetColors() const { return m_colors; }
    bool IsDark() const { return m_isDark; }

    void SetDarkMode(bool isDark);
    void SetAccentColor(uint32_t color);

    void Rebuild();

private:
    bool m_isDark;
    uint32_t m_accentColor;
    ThemeColors m_colors;
};

} // namespace ssp
