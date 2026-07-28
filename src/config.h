#pragma once

// ============================================================================
// User-editable configuration for Scientific Symbol Panel
// Change values here, rebuild, and the changes take effect.
// ============================================================================

// --- Hotkey ---
// GLFW modifier: KMOD_ALT, KMOD_CTRL, KMOD_GUI, KMOD_SHIFT
#define SSP_HOTKEY_MOD      KMOD_ALT
// GLFW key: SDLK_a .. SDLK_z, SDLK_F1 .. SDLK_F12, etc.
#define SSP_HOTKEY_KEY      SDLK_a

// --- Window ---
#define SSP_WINDOW_WIDTH    360
#define SSP_WINDOW_HEIGHT   480

// --- Behavior ---
#define SSP_MAX_RECENT      100     // Maximum recent symbols remembered
#define SSP_ANIMATIONS      1       // 1 = enabled, 0 = disabled
#define SSP_FUZZY_SEARCH    1       // 1 = fuzzy matching, 0 = exact prefix only

// --- Font ---
#define SSP_FONT_NAME       "DejaVu Sans"
#define SSP_FONT_SIZE_SYMBOL  22.0f
#define SSP_FONT_SIZE_SMALL   12.0f
#define SSP_FONT_SIZE_BODY    14.0f
#define SSP_FONT_SIZE_TITLE   16.0f
#define SSP_FONT_SIZE_SEARCH  14.0f

// --- Paths ---
// Font path — relative to executable, or absolute.
// Leave empty to auto-detect from assets/fonts/ or system paths.
#define SSP_FONT_PATH       ""
