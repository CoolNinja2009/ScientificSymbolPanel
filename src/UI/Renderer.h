#pragma once
#include <windows.h>
#include "Core/Types.h"
#include <d2d1.h>
#include <dwrite.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

struct ID2D1Factory;
struct IDWriteFactory;
struct ID2D1HwndRenderTarget;
struct ID2D1SolidColorBrush;
struct IDWriteTextFormat;

namespace ssp {

class Renderer {
public:
    Renderer(HWND hwnd, ID2D1Factory* d2dFactory, IDWriteFactory* dwriteFactory);
    ~Renderer();

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Lifecycle
    bool Initialize();
    void Resize(uint32_t width, uint32_t height);

    // Frame
    bool BeginDraw();
    bool EndDraw();

    // Theme
    void SetTheme(const ThemeColors& colors);

    // Drawing
    void FillRect(const RectF& rect, uint32_t color);
    void DrawText(std::wstring_view text, const RectF& rect,
                  IDWriteTextFormat* format, uint32_t color);
    void DrawTextCentered(std::wstring_view text, const RectF& rect,
                          IDWriteTextFormat* format, uint32_t color);
    void DrawRoundedRect(const RectF& rect, float radius, uint32_t color);
    void DrawLine(const PointF& p0, const PointF& p1, uint32_t color,
                  float width = 1.0f);

    // Text format management
    std::wstring CreateTextFormat(const wchar_t* fontName, float fontSize,
                                   DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL,
                                   DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL);
    IDWriteTextFormat* GetTextFormat(const std::wstring& key) const;

    // Accessors
    ID2D1HwndRenderTarget* GetRenderTarget() const { return m_renderTarget.get(); }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    struct ComDeleter {
        template<typename T>
        void operator()(T* p) const { if (p) p->Release(); }
    };
    template<typename T>
    using ComPtr = std::unique_ptr<T, ComDeleter>;

    bool CreateRenderTarget();
    static D2D1::ColorF ToColorF(uint32_t color);
    ID2D1SolidColorBrush* GetBrush(uint32_t color);
    void ClearBrushes();
    std::wstring MakeTextFormatKey(const wchar_t* fontName, float fontSize,
                                    DWRITE_FONT_WEIGHT fontWeight,
                                    DWRITE_FONT_STYLE style) const;

    HWND m_hwnd = nullptr;
    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;

    ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> m_brushes;
    std::unordered_map<std::wstring, ComPtr<IDWriteTextFormat>> m_textFormats;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace ssp
