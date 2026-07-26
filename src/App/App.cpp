#include "App.h"
#include "resource.h"
#include "Core/Log.h"
#include "Platform/Win32.h"
#include "Symbols/Database.h"
#include "Search/SearchEngine.h"
#include "Storage/JsonStore.h"
#include "Storage/Recent.h"
#include "Storage/Favorites.h"
#include "Symbols/Snippets.h"
#include "Symbols/Converters.h"
#include "UI/Renderer.h"
#include "UI/Themes.h"
#include "UI/Layout.h"
#include "UI/Animation.h"
#include "InputHandler.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <shellapi.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwmapi.lib")

namespace ssp {

App* App::s_instance = nullptr;
App::App()  { s_instance = this; }
App::~App() { Shutdown(); s_instance = nullptr; }

// ============================================================================
// Initialize
// ============================================================================

bool App::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    m_config.Load();
    if (!InitCOM()) return false;
    InitFactories();
    if (!m_d2dFactory || !m_dwriteFactory) return false;
    if (!RegisterWindowClass()) return false;
    if (!CreateMainWindow()) return false;
    RegisterHotkey();
    m_database = std::make_unique<SymbolDatabase>();
    m_database->Load();
    m_searchEngine = std::make_unique<SearchEngine>();
    m_searchEngine->Build(m_database->GetSymbols());
    InitSubsystems();
    return true;
}

void App::InitSubsystems() {
    auto& cfg = m_config.Get();
    bool isDark = Platform::IsDarkMode();
    if (cfg.theme == ThemeMode::Light) isDark = false;
    else if (cfg.theme == ThemeMode::Dark) isDark = true;

    m_themeManager = std::make_unique<ThemeManager>(isDark, Platform::GetAccentColor());
    m_animation = std::make_unique<Animation>();
    m_animation->enabled = cfg.animations && !Platform::IsLowEndMachine();

    m_renderer = std::make_unique<Renderer>(m_hwnd, m_d2dFactory.get(), m_dwriteFactory.get());
    m_renderer->Initialize();
    m_renderer->SetTheme(m_themeManager->GetColors());
    m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSearch * m_dpiScale);
    m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSymbol * m_dpiScale);
    m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSmall * m_dpiScale);

    m_recentManager = std::make_unique<RecentManager>(m_database.get(), m_config);
    m_recentManager->Load();
    m_favoritesManager = std::make_unique<FavoritesManager>(m_database.get(), m_config);
    m_favoritesManager->Load();

    m_snippetManager = std::make_unique<SnippetManager>();
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
    std::filesystem::path bundledSnippets = exeDir / L"data" / L"snippets.json";
    if (!std::filesystem::exists(bundledSnippets))
        bundledSnippets = std::filesystem::current_path() / L"data" / L"snippets.json";
    m_snippetManager->Load(bundledSnippets, m_config.DataDir());

    m_inputHandler = std::make_unique<InputHandler>();
    m_inputHandler->SetSearchEngine(m_searchEngine.get());
    m_inputHandler->SetDatabase(m_database.get());
    m_inputHandler->SetRecentManager(m_recentManager.get());
    m_inputHandler->SetFavoritesManager(m_favoritesManager.get());

    // Insert: yield focus → send text → reclaim focus via timer
    m_inputHandler->SetInsertCallback([this](const std::wstring& text) {
        HWND target = m_hwndPrevFocus;
        if (!target || !IsWindow(target)) target = GetForegroundWindow();
        if (target && target != m_hwnd) {
            m_inserting = true;
            SetForegroundWindow(target);
            Platform::SendUnicodeText(text);
            SetTimer(m_hwnd, 3, 50, nullptr);
        }
    });
    m_inputHandler->SetCloseCallback([this]() { HidePanel(); });
    m_inputHandler->SetInvalidateCallback([this]() { InvalidateRect(m_hwnd, nullptr, FALSE); });
}

// ============================================================================
// Run / Shutdown
// ============================================================================

int App::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return static_cast<int>(msg.wParam);
}

void App::Shutdown() {
    UnregisterHotkey();
    if (m_recentManager) m_recentManager->Save();
    if (m_favoritesManager) m_favoritesManager->Save();
    m_config.Save();
    m_inputHandler.reset(); m_snippetManager.reset(); m_favoritesManager.reset();
    m_recentManager.reset(); m_renderer.reset(); m_themeManager.reset();
    m_animation.reset(); m_searchEngine.reset(); m_database.reset();
    if (m_hwnd) { KillTimer(m_hwnd, 1); KillTimer(m_hwnd, 3); DestroyWindow(m_hwnd); m_hwnd = nullptr; }
    m_d2dFactory.reset(); m_dwriteFactory.reset(); m_wicFactory.reset();
}

// ============================================================================
// Panel show/hide
// ============================================================================

void App::ShowPanel() {
    if (m_visible) return;
    m_hwndPrevFocus = GetForegroundWindow();
    HMONITOR hmon = MonitorFromWindow(m_hwndPrevFocus, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(hmon, &mi);
    RECT work = mi.rcWork;
    auto& s = m_config.Get();
    int w = s.windowWidth, h = s.windowHeight;
    int x = ((work.right - work.left) - w) / 2 + work.left;
    int y = ((work.bottom - work.top) - h) / 2 + work.top;
    if (s.windowX >= 0) x = s.windowX;
    if (s.windowY >= 0) y = s.windowY;
    m_dpiScale = static_cast<float>(GetDpiForWindow(m_hwnd)) / 96.0f;
    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    m_visible = true;
    if (m_inputHandler) m_inputHandler->Reset();
    if (m_animation && m_animation->enabled) { m_animation->StartFadeIn(); SetTimer(m_hwnd, 1, 8, nullptr); }
    SetTimer(m_hwnd, 2, 530, nullptr); // cursor blink
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::HidePanel() {
    if (!m_visible) return;
    m_visible = false;
    KillTimer(m_hwnd, 1);
    KillTimer(m_hwnd, 2);
    ShowWindow(m_hwnd, SW_HIDE);
    if (m_hwndPrevFocus && IsWindow(m_hwndPrevFocus)) SetForegroundWindow(m_hwndPrevFocus);
}

void App::TogglePanel() { m_visible ? HidePanel() : ShowPanel(); }
void App::InsertSymbol(const std::wstring& text) { HidePanel(); Platform::SendUnicodeText(text); }
void App::InsertSnippet(const std::wstring& text) { HidePanel(); Platform::SendUnicodeText(text); }
void App::OnSymbolInserted(const Symbol* sym) { if (sym && m_recentManager) m_recentManager->Add(*sym); }
SymbolDatabase& App::GetDatabase()   { return *m_database; }
SearchEngine& App::GetSearchEngine() { return *m_searchEngine; }

// ============================================================================
// Copy to clipboard
// ============================================================================

static void CopyToClipboard(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    size_t size = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem) {
        memcpy(GlobalLock(hMem), text.c_str(), size);
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();
}

// ============================================================================
// Get selected symbol
// ============================================================================

static const Symbol* GetSelectedSymbol(InputHandler* input) {
    const auto& results = input->GetResults();
    size_t idx = input->GetSelectedIndex();
    if (idx < results.size() && results[idx].symbol)
        return results[idx].symbol;
    return nullptr;
}

// ============================================================================
// COM & factories
// ============================================================================

bool App::InitCOM() { return SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)); }
void App::InitFactories() {
    D2D1_FACTORY_OPTIONS opts = {};
#ifdef SSP_DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &opts, reinterpret_cast<void**>(&m_d2dFactory));
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&m_dwriteFactory));
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&m_wicFactory));
}

bool App::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc; wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_SSP_ICON)); wc.hIconSm = wc.hIcon;
    return RegisterClassExW(&wc) != 0;
}

bool App::CreateMainWindow() {
    auto& s = m_config.Get();
    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kWindowClass, kAppName, WS_POPUP,
        s.windowX >= 0 ? s.windowX : CW_USEDEFAULT, s.windowY >= 0 ? s.windowY : CW_USEDEFAULT,
        s.windowWidth, s.windowHeight, nullptr, nullptr, m_hInstance, this);
    if (!m_hwnd) return false;
    Platform::ApplyBackdrop(m_hwnd);
    Platform::ApplyRoundedCorners(m_hwnd);
    Platform::HideFromTaskbar(m_hwnd);
    return true;
}

void App::RegisterHotkey() {
    if (!::RegisterHotKey(m_hwnd, 1, m_config.Get().hotkeyModifiers, m_config.Get().hotkeyVk))
        SSP_LOG_DEBUG("RegisterHotKey failed: %lu", GetLastError());
}
void App::UnregisterHotkey() { if (m_hwnd) ::UnregisterHotKey(m_hwnd, 1); }

// ============================================================================
// Rendering
// ============================================================================

void App::RenderFrame() {
    if (!m_renderer || !m_visible) return;
    auto& colors = m_themeManager->GetColors();
    RECT cr; GetClientRect(m_hwnd, &cr);
    float w = static_cast<float>(cr.right), h = static_cast<float>(cr.bottom);
    auto& input = *m_inputHandler;
    bool hasRecent = !m_recentManager->GetRecent().empty();
    bool hasFavorites = !m_favoritesManager->GetFavorites().empty();

    auto layout = ComputeLayout(w, h, m_dpiScale, hasRecent, hasFavorites, input.GetResults().size());
    layout.scrollOffset = input.GetScrollOffset();
    int vr = static_cast<int>(layout.resultsGrid.height / layout.cellSize);
    if (vr < 1) vr = 1;
    input.UpdateLayoutInfo(layout.columns, vr, layout.maxScroll);
    if (!m_renderer->BeginDraw()) return;

    m_renderer->FillRect({0, 0, w, h}, colors.bgPrimary);
    float dpi = m_dpiScale;
    auto sf = m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSearch * dpi);
    auto tf = m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSmall * dpi);
    auto yf = m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSymbol * dpi);

    // Search bar
    {
        RectF r = layout.searchBar;
        m_renderer->DrawRoundedRect({r.x + 4, r.y + 4, r.width - 8, r.height - 8}, 6.0f, colors.bgSecondary);
        const auto& q = input.GetQuery();
        m_renderer->DrawText(q.empty() ? L"Search symbols..." : q,
            {r.x + 12, r.y + (r.height - kFontSizeSearch * dpi) * 0.5f, r.width - 24, r.height},
            m_renderer->GetTextFormat(sf),
            q.empty() ? colors.textMuted : colors.textPrimary);
        if (input.GetActiveZone() == Zone::SearchBar && input.HasSelection()) {
            int ss = input.GetSelectStart(), se = input.GetSelectEnd();
            if (ss > se) std::swap(ss, se);
            float selX = r.x + 12.0f + m_renderer->MeasureTextWidth(q, m_renderer->GetTextFormat(sf), ss);
            float selW = m_renderer->MeasureTextWidth(q, m_renderer->GetTextFormat(sf), se) -
                         m_renderer->MeasureTextWidth(q, m_renderer->GetTextFormat(sf), ss);
            if (selX + selW > r.x + r.width - 4.0f) selW = r.x + r.width - 4.0f - selX;
            m_renderer->FillRect({selX, r.y + 6.0f, selW, r.height - 12.0f}, 0x664078D4);
        }
        if (input.GetActiveZone() == Zone::SearchBar && m_cursorVisible && !q.empty()) {
            float cursorX = r.x + 12.0f + m_renderer->MeasureTextWidth(q, m_renderer->GetTextFormat(sf),
                static_cast<int>(input.GetCursorPos()));
            if (cursorX < r.x + r.width - 12.0f)
                m_renderer->FillRect({cursorX, r.y + 8.0f, 1.5f, r.height - 16.0f}, colors.accent);
        }
    }

    // Recents
    if (hasRecent) {
        m_renderer->DrawText(L"Recent", {layout.recentLabel.x+8,layout.recentLabel.y,layout.recentLabel.width,layout.recentLabel.height}, m_renderer->GetTextFormat(tf), colors.textMuted);
        auto& rec = m_recentManager->GetRecent();
        float cx = layout.recentGrid.x, cy = layout.recentGrid.y, cs = layout.cellSize;
        for (size_t i = 0; i < rec.size() && cx+cs <= layout.recentGrid.x+layout.recentGrid.width; i++, cx+=cs)
            m_renderer->DrawTextCentered(rec[i]->symbol, {cx,cy,cs,cs}, m_renderer->GetTextFormat(yf), colors.textPrimary);
    }

    // Favorites
    if (hasFavorites) {
        m_renderer->DrawText(L"Favorites", {layout.favoritesLabel.x+8,layout.favoritesLabel.y,layout.favoritesLabel.width,layout.favoritesLabel.height}, m_renderer->GetTextFormat(tf), colors.textMuted);
        auto& fav = m_favoritesManager->GetFavorites();
        float cx = layout.favoritesGrid.x, cy = layout.favoritesGrid.y, cs = layout.cellSize;
        for (size_t i = 0; i < fav.size() && cx+cs <= layout.favoritesGrid.x+layout.favoritesGrid.width; i++, cx+=cs)
            m_renderer->DrawTextCentered(fav[i]->symbol, {cx,cy,cs,cs}, m_renderer->GetTextFormat(yf), colors.textPrimary);
    }

    // Results grid
    {
        auto& res = input.GetResults(); float cs = layout.cellSize;
        float sx = layout.resultsGrid.x, sy = layout.resultsGrid.y - layout.scrollOffset;
        float ct = layout.resultsGrid.y, cb = ct+layout.resultsGrid.height;
        int sel = static_cast<int>(input.GetSelectedIndex());
        for (size_t i = 0; i < res.size(); i++) {
            float cx = sx + (i%layout.columns)*cs, cy = sy + (i/layout.columns)*cs;
            if (cy+cs < ct || cy > cb) continue;
            int hoverIdx = input.GetHoverIndex();
            if (input.IsHovering() && input.GetHoverZone() == Zone::ResultsGrid &&
                static_cast<int>(i) == hoverIdx && static_cast<int>(i) != sel) {
                m_renderer->DrawRoundedRect({cx+2,cy+2,cs-4,cs-4}, 4.0f, colors.hover);
            }
            if (static_cast<int>(i) == sel)
                m_renderer->DrawRoundedRect({cx+2,cy+2,cs-4,cs-4}, 4.0f, colors.selected);
            if (m_favoritesManager && m_favoritesManager->IsFavorite(*res[i].symbol))
                m_renderer->DrawText(L"\u2605", {cx+2, cy+2, 14.0f*dpi, 14.0f*dpi}, m_renderer->GetTextFormat(tf), 0xFFFFD700);
            m_renderer->DrawTextCentered(res[i].symbol->symbol, {cx,cy,cs,cs}, m_renderer->GetTextFormat(yf),
                (static_cast<int>(i)==sel) ? 0xFFE0F0FF : colors.textPrimary);
        }
    }

    // Status bar
    if (layout.statusBar.height > 0) {
        wchar_t buf[128]; const auto& q = input.GetQuery();
        if (!q.empty()) swprintf_s(buf, L"%zu results \u2022 Ctrl+C copy \u2022 Ctrl+D fav \u2022 DblClick insert+close", input.GetResults().size());
        else swprintf_s(buf, L"%zu symbols \u2022 Alt+A toggle \u2022 Drag to move", m_database->GetSymbols().size());
        m_renderer->DrawText(buf, {layout.statusBar.x+8,layout.statusBar.y,layout.statusBar.width,layout.statusBar.height}, m_renderer->GetTextFormat(tf), colors.textMuted);
    }

    m_renderer->EndDraw();
}

// ============================================================================
// Window Procedure
// ============================================================================

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
        return 0;

    case WM_NCHITTEST: {
        // Allow dragging the window by its background
        LRESULT hit = DefWindowProcW(hwnd, msg, wp, lp);
        if (hit == HTCLIENT) {
            // Check if mouse is over search bar or results — pass through
            // Otherwise return HTCAPTION to allow dragging
            if (app && app->m_visible) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                float mx = static_cast<float>(pt.x), my = static_cast<float>(pt.y);
                auto& input = *app->m_inputHandler;
                bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
                bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
                RECT cr; GetClientRect(hwnd, &cr);
                auto lo = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                    app->m_dpiScale, hasRec, hasFav, input.GetResults().size());
                // Only drag on background areas (not on search bar, grid, categories, strips)
                auto hitR = [](const RectF& r, float x, float y) { return x>=r.x && x<r.x+r.width && y>=r.y && y<r.y+r.height; };
                if (!hitR(lo.searchBar, mx, my) && !hitR(lo.resultsGrid, mx, my) &&
                    !hitR(lo.recentGrid, mx, my) &&
                    !hitR(lo.favoritesGrid, mx, my) && !hitR(lo.statusBar, mx, my))
                    return HTCAPTION;
            }
        }
        return hit;
    }

    case WM_HOTKEY:
        if (wp == 1 && app) app->TogglePanel();
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && app && app->IsVisible() && !app->m_inserting)
            app->HidePanel();
        return 0;

    case WM_TIMER:
        if (wp == 1 && app && app->m_animation && app->m_animation->IsActive()) {
            app->m_animation->Update(8.0f);
            InvalidateRect(hwnd, nullptr, FALSE);
            if (!app->m_animation->IsActive()) KillTimer(hwnd, 1);
        } else if (wp == 2 && app) {
            app->m_cursorVisible = !app->m_cursorVisible;
            // Only invalidate search bar area to minimize repaint
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wp == 3 && app) {
            KillTimer(hwnd, 3);
            app->m_inserting = false;
            SetForegroundWindow(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        {
        if (app && app->m_inputHandler && app->m_visible) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            // Ctrl+C: copy selection, query, or symbol
            if (ctrl && wp == 'C') {
                if (app->m_inputHandler->GetActiveZone() == Zone::SearchBar) {
                    if (app->m_inputHandler->HasSelection())
                        CopyToClipboard(app->m_inputHandler->GetSelection());
                    else if (!app->m_inputHandler->GetQuery().empty())
                        CopyToClipboard(app->m_inputHandler->GetQuery());
                } else {
                    auto* sym = GetSelectedSymbol(app->m_inputHandler.get());
                    if (sym) CopyToClipboard(sym->symbol);
                }
                return 0;
            }
            // Ctrl+X: cut selection or all text
            if (ctrl && wp == 'X' && app->m_inputHandler->GetActiveZone() == Zone::SearchBar) {
                if (app->m_inputHandler->HasSelection()) {
                    CopyToClipboard(app->m_inputHandler->GetSelection());
                    app->m_inputHandler->CutSelection();
                } else if (!app->m_inputHandler->GetQuery().empty()) {
                    CopyToClipboard(app->m_inputHandler->GetQuery());
                    app->m_inputHandler->Reset();
                }
                app->m_inputHandler->RefreshResults();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            // Ctrl+D: toggle favorite
            if (ctrl && wp == 'D') {
                auto* sym = GetSelectedSymbol(app->m_inputHandler.get());
                if (sym && app->m_favoritesManager) {
                    if (app->m_favoritesManager->IsFavorite(*sym))
                        app->m_favoritesManager->Remove(*sym);
                    else
                        app->m_favoritesManager->Add(*sym);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            // Ctrl+V: paste into search bar
            if (ctrl && wp == 'V') {
                if (OpenClipboard(nullptr)) {
                    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                    if (hData) {
                        wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
                        if (pText) {
                            app->m_inputHandler->AppendToQuery(pText);
                            GlobalUnlock(hData);
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    CloseClipboard();
                }
                return 0;
            }
            // Ctrl+W / Escape: close
            if (ctrl && wp == 'W') { app->HidePanel(); return 0; }

            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            if (app->m_inputHandler->HandleKeyDown(wp, shift, ctrl, hasRec, hasFav)) {
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        break;
        }
    case WM_CHAR:
        if (app && app->m_inputHandler && app->m_visible) {
            app->m_inputHandler->HandleChar(static_cast<wchar_t>(wp));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_LBUTTONDBLCLK:
        // Double-click: insert and close
        if (app && app->m_inputHandler && app->m_visible) {
            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            RECT cr; GetClientRect(hwnd, &cr);
            auto lo = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                app->m_dpiScale, hasRec, hasFav, app->m_inputHandler->GetResults().size());
            app->m_inputHandler->HandleMouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), lo, hasRec, hasFav);
            InvalidateRect(hwnd, nullptr, FALSE);
            // The insert callback will be invoked, then we close
            app->HidePanel();
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (app && app->m_inputHandler && app->m_visible) {
            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            RECT cr; GetClientRect(hwnd, &cr);
            auto lo = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                app->m_dpiScale, hasRec, hasFav, app->m_inputHandler->GetResults().size());
            app->m_inputHandler->HandleMouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), lo, hasRec, hasFav);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_RBUTTONUP:
        if (app && app->m_inputHandler && app->m_visible) {
            // Determine what's under cursor
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ClientToScreen(hwnd, &pt);
            auto* sym = GetSelectedSymbol(app->m_inputHandler.get());
            if (sym) {
                bool isFav = app->m_favoritesManager && app->m_favoritesManager->IsFavorite(*sym);
                HMENU menu = CreatePopupMenu();
                AppendMenuW(menu, MF_STRING, 1, L"Copy Symbol");
                AppendMenuW(menu, MF_STRING, 2, (std::wstring(L"Copy LaTeX: ") + sym->latex).c_str());
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, 3, isFav ? L"Remove from Favorites" : L"Add to Favorites");
                int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
                if (cmd == 1) CopyToClipboard(sym->symbol);
                else if (cmd == 2) CopyToClipboard(sym->latex);
                else if (cmd == 3 && app->m_favoritesManager) {
                    if (isFav) app->m_favoritesManager->Remove(*sym);
                    else app->m_favoritesManager->Add(*sym);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                DestroyMenu(menu);
            }
            return 0;
        }
        break;


    case WM_MOUSEMOVE:
        if (app && app->m_inputHandler && app->m_visible) {
            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            RECT cr; GetClientRect(hwnd, &cr);
            auto lo = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                app->m_dpiScale, hasRec, hasFav, app->m_inputHandler->GetResults().size());
            app->m_inputHandler->HandleMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), lo, hasRec, hasFav);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_MOUSEWHEEL:
        if (app && app->m_inputHandler && app->m_visible) {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            RECT cr; GetClientRect(hwnd, &cr);
            auto lo = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                app->m_dpiScale, hasRec, hasFav, app->m_inputHandler->GetResults().size());
            app->m_inputHandler->HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), pt.x, pt.y, lo);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_SIZE:
        if (app && app->m_renderer && wp != SIZE_MINIMIZED) {
            app->m_renderer->Resize(LOWORD(lp), HIWORD(lp));
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (app) app->RenderFrame();
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DPICHANGED:
        if (app) app->m_dpiScale = static_cast<float>(HIWORD(wp)) / 96.0f;
        SetWindowPos(hwnd, nullptr,
            reinterpret_cast<RECT*>(lp)->left, reinterpret_cast<RECT*>(lp)->top,
            reinterpret_cast<RECT*>(lp)->right - reinterpret_cast<RECT*>(lp)->left,
            reinterpret_cast<RECT*>(lp)->bottom - reinterpret_cast<RECT*>(lp)->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_ERASEBKGND: return 1;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace ssp
