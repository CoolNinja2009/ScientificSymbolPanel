#include "UI/Renderer.h"
#include "Core/Log.h"
#include <windows.h>
#include <sstream>

namespace ssp {

// ============================================================================
// Color conversion
// ============================================================================

D2D1::ColorF Renderer::ToColorF(uint32_t color) {
    return D2D1::ColorF(
        ((color >> 16) & 0xFF) / 255.0f,  // R
        ((color >> 8) & 0xFF) / 255.0f,   // G
        (color & 0xFF) / 255.0f,          // B
        ((color >> 24) & 0xFF) / 255.0f   // A
    );
}

// ============================================================================
// Construction / Destruction
// ============================================================================

Renderer::Renderer(HWND hwnd, ID2D1Factory* d2dFactory, IDWriteFactory* dwriteFactory)
    : m_hwnd(hwnd), m_d2dFactory(d2dFactory), m_dwriteFactory(dwriteFactory) {
}

Renderer::~Renderer() = default;

// ============================================================================
// Lifecycle
// ============================================================================

bool Renderer::Initialize() {
    if (!m_hwnd || !m_d2dFactory) {
        SSP_LOG_DEBUG("Renderer::Initialize - invalid state (hwnd=%p, d2d=%p)",
                      static_cast<void*>(m_hwnd), static_cast<void*>(m_d2dFactory));
        return false;
    }

    RECT rc;
    if (!GetClientRect(m_hwnd, &rc)) {
        SSP_LOG_DEBUG("Renderer::Initialize - GetClientRect failed");
        return false;
    }

    m_width = static_cast<uint32_t>(rc.right - rc.left);
    m_height = static_cast<uint32_t>(rc.bottom - rc.top);

    // Defer creation if window is not yet sized — WM_SIZE will trigger Resize()
    if (m_width == 0 || m_height == 0) {
        SSP_LOG_DEBUG("Renderer::Initialize - window not yet sized, deferring");
        return true;
    }
    return CreateRenderTarget();
}

bool Renderer::CreateRenderTarget() {
    if (m_width == 0 || m_height == 0) return false;

    UINT dpi = GetDpiForWindow(m_hwnd);
    float dpiX = static_cast<float>(dpi);
    float dpiY = static_cast<float>(dpi);

    auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    auto hwndProps = D2D1::HwndRenderTargetProperties(
        m_hwnd,
        D2D1::SizeU(m_width, m_height)
    );

    HRESULT hr = m_d2dFactory->CreateHwndRenderTarget(props, hwndProps,
        reinterpret_cast<ID2D1HwndRenderTarget**>(&m_renderTarget));
    if (FAILED(hr)) {
        SSP_LOG_DEBUG("Renderer::CreateRenderTarget - failed: 0x%08X", hr);
        return false;
    }

    SSP_LOG_DEBUG("Renderer::CreateRenderTarget - success, %u x %u, DPI: %u", m_width, m_height, dpi);
    return true;
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;

    m_width = width;
    m_height = height;

    if (m_renderTarget) {
        HRESULT hr = m_renderTarget->Resize(D2D1::SizeU(width, height));
        if (FAILED(hr)) {
            SSP_LOG_DEBUG("Renderer::Resize - failed: 0x%08X, recreating", hr);
            m_renderTarget.reset();
            m_brushes.clear();
            CreateRenderTarget();
        } else {
            SSP_LOG_DEBUG("Renderer::Resize - %u x %u", width, height);
        }
    } else {
        // Render target not yet created (window was 0x0 at Initialize time)
        SSP_LOG_DEBUG("Renderer::Resize - lazy init, creating render target");
        CreateRenderTarget();
    }
}

// ============================================================================
// Frame
// ============================================================================

bool Renderer::BeginDraw() {
    if (!m_renderTarget) return false;

    m_renderTarget->BeginDraw();
    return true;
}

bool Renderer::EndDraw() {
    if (!m_renderTarget) return false;

    HRESULT hr = m_renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        SSP_LOG_DEBUG("Renderer::EndDraw - device lost, recreating target");
        m_renderTarget.reset();
        m_brushes.clear();
        CreateRenderTarget();
    }
    return SUCCEEDED(hr);
}

// ============================================================================
// Theme
// ============================================================================

void Renderer::SetTheme(const ThemeColors& /*colors*/) {
    ClearBrushes();
    SSP_LOG_DEBUG("Renderer::SetTheme - brushes cleared");
}

// ============================================================================
// Brush management
// ============================================================================

ID2D1SolidColorBrush* Renderer::GetBrush(uint32_t color) {
    auto it = m_brushes.find(color);
    if (it != m_brushes.end()) {
        return it->second.get();
    }

    if (!m_renderTarget) return nullptr;

    ComPtr<ID2D1SolidColorBrush> brush;
    HRESULT hr = m_renderTarget->CreateSolidColorBrush(ToColorF(color),
        reinterpret_cast<ID2D1SolidColorBrush**>(&brush));
    if (FAILED(hr)) {
        SSP_LOG_DEBUG("Renderer::GetBrush - CreateSolidColorBrush(0x%08X) failed: 0x%08X",
                      color, hr);
        return nullptr;
    }

    ID2D1SolidColorBrush* ptr = brush.get();
    m_brushes[color] = std::move(brush);
    return ptr;
}

void Renderer::ClearBrushes() {
    m_brushes.clear();
}

// ============================================================================
// Drawing
// ============================================================================

void Renderer::FillRect(const RectF& rect, uint32_t color) {
    if (!m_renderTarget) return;

    auto* brush = GetBrush(color);
    if (!brush) return;

    m_renderTarget->FillRectangle(
        D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height),
        brush
    );
}

void Renderer::DrawText(std::wstring_view text, const RectF& rect,
                         IDWriteTextFormat* format, uint32_t color) {
    if (!m_renderTarget || !format) return;

    auto* brush = GetBrush(color);
    if (!brush) return;

    m_renderTarget->DrawText(
        text.data(),
        static_cast<UINT32>(text.size()),
        format,
        D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height),
        brush
    );
}

void Renderer::DrawTextCentered(std::wstring_view text, const RectF& rect,
                                 IDWriteTextFormat* format, uint32_t color) {
    if (!m_renderTarget || !format || !m_dwriteFactory) return;

    auto* brush = GetBrush(color);
    if (!brush) return;

    // Create a temporary layout to set centering without modifying the shared format
    IDWriteTextLayout* rawLayout = nullptr;
    HRESULT hr = m_dwriteFactory->CreateTextLayout(
        text.data(),
        static_cast<UINT32>(text.size()),
        format,
        rect.width,
        rect.height,
        &rawLayout
    );
    ComPtr<IDWriteTextLayout> layout(rawLayout);

    if (FAILED(hr)) {
        SSP_LOG_DEBUG("Renderer::DrawTextCentered - CreateTextLayout failed: 0x%08X", hr);
        return;
    }

    layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    m_renderTarget->DrawTextLayout(
        D2D1::Point2F(rect.x, rect.y),
        layout.get(),
        brush
    );
}

void Renderer::DrawRoundedRect(const RectF& rect, float radius, uint32_t color) {
    if (!m_renderTarget) return;

    auto* brush = GetBrush(color);
    if (!brush) return;

    auto rr = D2D1::RoundedRect(
        D2D1::RectF(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height),
        radius,
        radius
    );

    m_renderTarget->FillRoundedRectangle(rr, brush);
}

void Renderer::DrawLine(const PointF& p0, const PointF& p1, uint32_t color, float width) {
    if (!m_renderTarget) return;

    auto* brush = GetBrush(color);
    if (!brush) return;

    m_renderTarget->DrawLine(
        D2D1::Point2F(p0.x, p0.y),
        D2D1::Point2F(p1.x, p1.y),
        brush,
        width
    );
}

// ============================================================================
// Text format management
// ============================================================================

std::wstring Renderer::MakeTextFormatKey(const wchar_t* fontName, float fontSize,
                                          DWRITE_FONT_WEIGHT fontWeight,
                                          DWRITE_FONT_STYLE style) const {
    std::wostringstream oss;
    oss << fontName << L'|' << fontSize << L'|'
        << static_cast<int>(fontWeight) << L'|'
        << static_cast<int>(style);
    return oss.str();
}

std::wstring Renderer::CreateTextFormat(const wchar_t* fontName, float fontSize,
                                         DWRITE_FONT_WEIGHT fontWeight,
                                         DWRITE_FONT_STYLE style) {
    if (!m_dwriteFactory) return {};

    auto key = MakeTextFormatKey(fontName, fontSize, fontWeight, style);

    if (m_textFormats.find(key) != m_textFormats.end()) {
        return key;
    }

    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = m_dwriteFactory->CreateTextFormat(
        fontName,
        nullptr,                     // font collection
        fontWeight,
        style,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"",                         // locale
        reinterpret_cast<IDWriteTextFormat**>(&format)
    );

    if (FAILED(hr)) {
        SSP_LOG_DEBUG("Renderer::CreateTextFormat - failed for '%ls' %.1f: 0x%08X",
                      fontName, fontSize, hr);
        return {};
    }

    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

    m_textFormats[key] = std::move(format);
    return key;
}

IDWriteTextFormat* Renderer::GetTextFormat(const std::wstring& key) const {
    auto it = m_textFormats.find(key);
    if (it != m_textFormats.end()) {
        return it->second.get();
    }
    return nullptr;
}

} // namespace ssp
