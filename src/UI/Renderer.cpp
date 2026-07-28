#include "UI/Renderer.h"
#include "Core/Log.h"
#include <SDL_ttf.h>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <cmath>

#ifdef DrawText
#undef DrawText
#endif

namespace ssp {

// ============================================================================
// UTF-8 conversion
// ============================================================================

std::string Renderer::ToUtf8(std::wstring_view ws) {
    if (ws.empty()) return {};
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(ws.data(), ws.data() + ws.size());
}

// ============================================================================
// Construction
// ============================================================================

Renderer::Renderer(SDL_Window* window)
    : m_window(window), m_width(360), m_height(480) {}

Renderer::~Renderer() {
    for (auto& [size, font] : m_fontCache)
        if (font) TTF_CloseFont(font);
    m_fontCache.clear();
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
}

// ============================================================================
// Lifecycle
// ============================================================================

bool Renderer::Initialize(const char* fontPath, int fontSize) {
    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer) {
        SSP_LOG_DEBUG("Renderer: SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    // Enable blending for alpha
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

    m_fontPath = fontPath ? fontPath : "";
    m_baseFontSize = fontSize;

    if (fontPath && *fontPath) {
        if (TTF_Init() == 0) {
            m_baseFont = TTF_OpenFont(fontPath, fontSize);
            if (!m_baseFont)
                SSP_LOG_DEBUG("Renderer: TTF_OpenFont failed: %s", TTF_GetError());
        } else {
            SSP_LOG_DEBUG("Renderer: TTF_Init failed: %s", TTF_GetError());
        }
    }

    SDL_GetWindowSize(m_window, &m_width, &m_height);
    return true;
}

void Renderer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
}

TTF_Font* Renderer::GetFont(float size) {
    int isize = (int)(size + 0.5f);
    auto it = m_fontCache.find((float)isize);
    if (it != m_fontCache.end()) return it->second;

    if (m_fontPath.empty() || !m_baseFont) return m_baseFont;

    TTF_Font* font = TTF_OpenFont(m_fontPath.c_str(), isize);
    if (font) {
        m_fontCache[(float)isize] = font;
        return font;
    }
    return m_baseFont;
}

// ============================================================================
// Frame
// ============================================================================

bool Renderer::BeginDraw() {
    if (!m_renderer) return false;
    SDL_RenderClear(m_renderer);
    return true;
}

bool Renderer::EndDraw() {
    if (!m_renderer) return false;
    SDL_RenderPresent(m_renderer);
    return true;
}

// ============================================================================
// Theme
// ============================================================================

void Renderer::SetTheme(const ThemeColors& /*colors*/) {
    // Colors applied per draw call
}

// ============================================================================
// Drawing helpers
// ============================================================================

void Renderer::SetDrawColor(uint32_t color) {
    Uint8 r = (color >> 16) & 0xFF;
    Uint8 g = (color >> 8) & 0xFF;
    Uint8 b = color & 0xFF;
    Uint8 a = (color >> 24) & 0xFF;
    SDL_SetRenderDrawColor(m_renderer, r, g, b, a);
}

// ============================================================================
// Drawing
// ============================================================================

void Renderer::FillRect(const RectF& rect, uint32_t color) {
    if (!m_renderer) return;
    SetDrawColor(color);
    SDL_Rect r = {(int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height};
    SDL_RenderFillRect(m_renderer, &r);
}

void Renderer::DrawText(std::wstring_view text, float x, float y,
                         float fontSize, uint32_t color) {
    if (!m_renderer || text.empty()) return;

    TTF_Font* font = GetFont(fontSize);
    if (!font) return;

    Uint8 r = (color >> 16) & 0xFF;
    Uint8 g = (color >> 8) & 0xFF;
    Uint8 b = color & 0xFF;

    std::string utf8 = ToUtf8(text);
    SDL_Color fg = {r, g, b, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, utf8.c_str(), fg);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    if (tex) {
        SDL_Rect dst = {(int)x, (int)y, surf->w, surf->h};
        SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void Renderer::DrawTextCentered(std::wstring_view text, const RectF& rect,
                                 float fontSize, uint32_t color) {
    if (!m_renderer || text.empty()) return;

    TTF_Font* font = GetFont(fontSize);
    if (!font) return;

    Uint8 r = (color >> 16) & 0xFF;
    Uint8 g = (color >> 8) & 0xFF;
    Uint8 b = color & 0xFF;

    std::string utf8 = ToUtf8(text);
    SDL_Color fg = {r, g, b, 255};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, utf8.c_str(), fg);
    if (!surf) return;

    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    if (tex) {
        int cx = (int)(rect.x + (rect.width - surf->w) * 0.5f);
        int cy = (int)(rect.y + (rect.height - surf->h) * 0.5f);
        SDL_Rect dst = {cx, cy, surf->w, surf->h};
        SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void Renderer::DrawRoundedRect(const RectF& rect, float radius, uint32_t color) {
    if (!m_renderer) return;
    // Simple approximation: draw a filled rect (no rounded corners in SDL2 base)
    // For a production app, use SDL2_gfx or custom rendering
    int r = (int)(radius + 0.5f);
    int rx = (int)rect.x, ry = (int)rect.y;
    int rw = (int)rect.width, rh = (int)rect.height;

    SetDrawColor(color);

    // Fill center
    SDL_Rect cr = {rx + r, ry, rw - 2*r, rh};
    SDL_RenderFillRect(m_renderer, &cr);
    // Fill top/bottom strips (minus corners)
    SDL_Rect tr = {rx + r, ry, rw - 2*r, r};
    SDL_RenderFillRect(m_renderer, &tr);
    SDL_Rect br = {rx + r, ry + rh - r, rw - 2*r, r};
    SDL_RenderFillRect(m_renderer, &br);
    // Fill left/right strips
    SDL_Rect lr = {rx, ry + r, r, rh - 2*r};
    SDL_RenderFillRect(m_renderer, &lr);
    SDL_Rect rr2 = {rx + rw - r, ry + r, r, rh - 2*r};
    SDL_RenderFillRect(m_renderer, &rr2);
    // Draw corner circles approximated as filled squares for now
    SDL_Rect tl = {rx, ry, r, r};
    SDL_RenderFillRect(m_renderer, &tl);
    SDL_Rect tr2 = {rx + rw - r, ry, r, r};
    SDL_RenderFillRect(m_renderer, &tr2);
    SDL_Rect bl = {rx, ry + rh - r, r, r};
    SDL_RenderFillRect(m_renderer, &bl);
    SDL_Rect br2 = {rx + rw - r, ry + rh - r, r, r};
    SDL_RenderFillRect(m_renderer, &br2);
}

void Renderer::DrawLine(const PointF& p0, const PointF& p1, uint32_t color, float width) {
    if (!m_renderer) return;
    SetDrawColor(color);
    SDL_RenderDrawLine(m_renderer, (int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y);
    (void)width;
}

// ============================================================================
// Clip
// ============================================================================

void Renderer::PushClip(const RectF& rect) {
    if (!m_renderer) return;
    SDL_Rect r = {(int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height};
    SDL_RenderSetClipRect(m_renderer, &r);
}

void Renderer::PopClip() {
    if (!m_renderer) return;
    SDL_RenderSetClipRect(m_renderer, nullptr);
}

// ============================================================================
// Text measurement
// ============================================================================

float Renderer::MeasureTextWidth(std::wstring_view text, float fontSize,
                                  int upToChars) const {
    if (text.empty()) return 0.0f;

    TTF_Font* font = const_cast<Renderer*>(this)->GetFont(fontSize);
    if (!font) return 0.0f;

    std::string utf8 = ToUtf8(text);
    if (upToChars > 0) {
        // Approximate: TTF doesn't support partial string measurement easily
        // Use ratio of character count
        int totalChars = 0;
        for (auto c : text) { (void)c; totalChars++; }
        if (totalChars == 0) return 0.0f;
        float ratio = (float)upToChars / (float)totalChars;
        int w, h;
        TTF_SizeUTF8(font, utf8.c_str(), &w, &h);
        return (float)w * ratio;
    }

    int w, h;
    TTF_SizeUTF8(font, utf8.c_str(), &w, &h);
    return (float)w;
}

} // namespace ssp
