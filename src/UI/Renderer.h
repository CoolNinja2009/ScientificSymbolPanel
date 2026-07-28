#pragma once
#include "Core/Types.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ssp {

class Renderer {
public:
    Renderer(SDL_Window* window);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Lifecycle
    bool Initialize(const char* fontPath, int fontSize);
    void Resize(int width, int height);

    // Frame
    bool BeginDraw();
    bool EndDraw();

    // Theme
    void SetTheme(const ThemeColors& colors);

    // Drawing
    void FillRect(const RectF& rect, uint32_t color);
    void DrawText(std::wstring_view text, float x, float y, float fontSize, uint32_t color);
    void DrawTextCentered(std::wstring_view text, const RectF& rect, float fontSize, uint32_t color);
    void DrawRoundedRect(const RectF& rect, float radius, uint32_t color);
    void DrawLine(const PointF& p0, const PointF& p1, uint32_t color, float width = 1.0f);

    // Clip
    void PushClip(const RectF& rect);
    void PopClip();

    // Text measurement
    float MeasureTextWidth(std::wstring_view text, float fontSize, int upToChars = -1) const;

    // Accessors
    int GetWidth() const  { return m_width; }
    int GetHeight() const { return m_height; }

private:
    void SetDrawColor(uint32_t color);
    TTF_Font* GetFont(float size);
    static std::string ToUtf8(std::wstring_view ws);

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    TTF_Font* m_baseFont = nullptr;  // font at default size
    std::unordered_map<float, TTF_Font*> m_fontCache;
    int m_width = 0;
    int m_height = 0;
    std::string m_fontPath;
    int m_baseFontSize = 14;
};

} // namespace ssp
