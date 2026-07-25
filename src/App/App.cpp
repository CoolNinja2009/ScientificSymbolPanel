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

bool App::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    SSP_LOG_DEBUG("App::Initialize - starting");
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
    SSP_LOG_DEBUG("App::Initialize - done, %zu symbols", m_database->GetSymbols().size());
    return true;
}

void App::InitSubsystems() {
    auto& cfg = m_config.Get();
    bool isDark = Platform::IsDarkMode();
    if (cfg.theme == ThemeMode::Light) isDark = false;
    else if (cfg.theme == ThemeMode::Dark) isDark = true;
    uint32_t accent = Platform::GetAccentColor();

    m_themeManager = std::make_unique<ThemeManager>(isDark, accent);
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

    // Insert: yield focus to target app, send text, reclaim focus via timer
    m_inputHandler->SetInsertCallback([this](const std::wstring& text) {
        HWND target = m_hwndPrevFocus;
        if (!target || !IsWindow(target)) target = GetForegroundWindow();
        if (target && target != m_hwnd) {
            SetForegroundWindow(target);
            Platform::SendUnicodeText(text);
            SetTimer(m_hwnd, 3, 50, nullptr);
        }
    });
    m_inputHandler->SetCloseCallback([this]() { HidePanel(); });
    m_inputHandler->SetInvalidateCallback([this]() { InvalidateRect(m_hwnd, nullptr, FALSE); });
}

int App::Run() {
    MSG msg = {};
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
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void App::HidePanel() {
    if (!m_visible) return;
    m_visible = false;
    KillTimer(m_hwnd, 1);
    ShowWindow(m_hwnd, SW_HIDE);
    if (m_hwndPrevFocus && IsWindow(m_hwndPrevFocus)) SetForegroundWindow(m_hwndPrevFocus);
}

void App::TogglePanel() { m_visible ? HidePanel() : ShowPanel(); }
void App::InsertSymbol(const std::wstring& text) { HidePanel(); Platform::SendUnicodeText(text); }
void App::InsertSnippet(const std::wstring& text) { HidePanel(); Platform::SendUnicodeText(text); }
void App::OnSymbolInserted(const Symbol* sym) { if (sym && m_recentManager) m_recentManager->Add(*sym); }
SymbolDatabase& App::GetDatabase()   { return *m_database; }
SearchEngine& App::GetSearchEngine() { return *m_searchEngine; }

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
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc; wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    wc.hIcon = LoadIconW(m_hInstance, MAKEINTRESOURCEW(IDI_SSP_ICON)); wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) { SSP_LOG_DEBUG("RegisterClassExW failed: %lu", GetLastError()); return false; }
    return true;
}

bool App::CreateMainWindow() {
    auto& s = m_config.Get();
    m_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kWindowClass, kAppName, WS_POPUP,
        s.windowX >= 0 ? s.windowX : CW_USEDEFAULT, s.windowY >= 0 ? s.windowY : CW_USEDEFAULT,
        s.windowWidth, s.windowHeight, nullptr, nullptr, m_hInstance, this);
    if (!m_hwnd) { SSP_LOG_DEBUG("CreateWindowExW failed: %lu", GetLastError()); return false; }
    Platform::ApplyBackdrop(m_hwnd);
    Platform::ApplyRoundedCorners(m_hwnd);
    Platform::HideFromTaskbar(m_hwnd);
    return true;
}

void App::RegisterHotkey() {
    auto& s = m_config.Get();
    if (!::RegisterHotKey(m_hwnd, 1, s.hotkeyModifiers, s.hotkeyVk))
        SSP_LOG_DEBUG("RegisterHotKey failed: %lu", GetLastError());
}
void App::UnregisterHotkey() { if (m_hwnd) ::UnregisterHotKey(m_hwnd, 1); }

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
    int visibleRows = static_cast<int>((layout.resultsGrid.height) / layout.cellSize);
    if (visibleRows < 1) visibleRows = 1;
    input.UpdateLayoutInfo(layout.columns, visibleRows, layout.maxScroll);
    if (!m_renderer->BeginDraw()) return;

    m_renderer->FillRect({0, 0, w, h}, colors.bgPrimary);
    float dpi = m_dpiScale;
    auto searchFmtKey = m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSearch * dpi);
    auto smallFmtKey  = m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSmall * dpi);
    auto symFmtKey    = m_renderer->CreateTextFormat(L"Segoe UI", kFontSizeSymbol * dpi);

    // Search bar
    {
        RectF r = layout.searchBar;
        m_renderer->DrawRoundedRect({r.x + 4, r.y + 4, r.width - 8, r.height - 8}, 6.0f, colors.bgSecondary);
        const auto& query = input.GetQuery();
        m_renderer->DrawText(query.empty() ? L"Search symbols..." : query,
            {r.x + 12, r.y, r.width - 24, r.height},
            m_renderer->GetTextFormat(searchFmtKey), query.empty() ? colors.textMuted : colors.textPrimary);
    }

    // Recent strip
    if (hasRecent) {
        m_renderer->DrawText(L"Recent", {layout.recentLabel.x + 8, layout.recentLabel.y, layout.recentLabel.width, layout.recentLabel.height},
            m_renderer->GetTextFormat(smallFmtKey), colors.textMuted);
        auto& recents = m_recentManager->GetRecent();
        float cx = layout.recentGrid.x, cy = layout.recentGrid.y, cs = layout.cellSize;
        for (size_t i = 0; i < recents.size() && cx + cs <= layout.recentGrid.x + layout.recentGrid.width; i++, cx += cs)
            m_renderer->DrawTextCentered(recents[i]->symbol, {cx, cy, cs, cs}, m_renderer->GetTextFormat(symFmtKey), colors.textPrimary);
    }

    // Favorites strip
    if (hasFavorites) {
        m_renderer->DrawText(L"Favorites", {layout.favoritesLabel.x + 8, layout.favoritesLabel.y, layout.favoritesLabel.width, layout.favoritesLabel.height},
            m_renderer->GetTextFormat(smallFmtKey), colors.textMuted);
        auto& favs = m_favoritesManager->GetFavorites();
        float cx = layout.favoritesGrid.x, cy = layout.favoritesGrid.y, cs = layout.cellSize;
        for (size_t i = 0; i < favs.size() && cx + cs <= layout.favoritesGrid.x + layout.favoritesGrid.width; i++, cx += cs)
            m_renderer->DrawTextCentered(favs[i]->symbol, {cx, cy, cs, cs}, m_renderer->GetTextFormat(symFmtKey), colors.textPrimary);
    }

    // Category bar
    {
        RectF r = layout.categoryBar;
        float cx = r.x + 4, cy = r.y, ch = r.height;
        int activeCat = input.IsAllCategories() ? -1 : static_cast<int>(input.GetCategoryFilter());
        for (int i = 0; i < static_cast<int>(Category::COUNT); i++) {
            std::wstring name(CategoryNames[i], CategoryNames[i] + strlen(CategoryNames[i]));
            name = L" " + name + L" ";
            float tw = static_cast<float>(name.size()) * 7.0f * dpi;
            RectF btn{cx, cy + 4, tw, ch - 8};
            m_renderer->DrawRoundedRect(btn, 4.0f, (i == activeCat) ? colors.accent : colors.bgTertiary);
            m_renderer->DrawText(name, {cx + 4, cy, tw - 8, ch}, m_renderer->GetTextFormat(smallFmtKey),
                (i == activeCat) ? 0xFFFFFFFF : colors.textSecondary);
            cx += tw + 4;
            if (cx > r.x + r.width) break;
        }
    }

    // Results grid
    {
        auto& results = input.GetResults();
        float cs = layout.cellSize;
        float sx = layout.resultsGrid.x, sy = layout.resultsGrid.y - layout.scrollOffset * cs;
        float ct = layout.resultsGrid.y, cb = ct + layout.resultsGrid.height;
        for (size_t i = 0; i < results.size(); i++) {
            float cx = sx + (i % layout.columns) * cs, cy = sy + (i / layout.columns) * cs;
            if (cy + cs < ct || cy > cb) continue;
            bool sel = (static_cast<int>(i) == static_cast<int>(input.GetSelectedIndex()));
            if (sel) m_renderer->DrawRoundedRect({cx + 2, cy + 2, cs - 4, cs - 4}, 4.0f, colors.selected);
            m_renderer->DrawTextCentered(results[i].symbol->symbol, {cx, cy, cs, cs},
                m_renderer->GetTextFormat(symFmtKey), sel ? 0xFFFFFFFF : colors.textPrimary);
        }
    }

    // Status bar
    if (layout.statusBar.height > 0) {
        wchar_t buf[64];
        const auto& query = input.GetQuery();
        if (!query.empty())
            swprintf_s(buf, L"%zu results for \"%s\"", input.GetResults().size(), query.c_str());
        else
            swprintf_s(buf, L"%zu symbols  •  Alt+A to toggle", m_database->GetSymbols().size());
        m_renderer->DrawText(buf, {layout.statusBar.x + 8, layout.statusBar.y, layout.statusBar.width, layout.statusBar.height},
            m_renderer->GetTextFormat(smallFmtKey), colors.textMuted);
    }

    m_renderer->EndDraw();
}

LRESULT CALLBACK App::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT*>(lp)->lpCreateParams));
        return 0;

    case WM_HOTKEY:
        if (wp == 1 && app) app->TogglePanel();
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE && app && app->IsVisible()) app->HidePanel();
        return 0;

    case WM_TIMER:
        if (wp == 1 && app && app->m_animation && app->m_animation->IsActive()) {
            app->m_animation->Update(8.0f);
            InvalidateRect(hwnd, nullptr, FALSE);
            if (!app->m_animation->IsActive()) KillTimer(hwnd, 1);
        } else if (wp == 3 && app) {
            KillTimer(hwnd, 3);
            SetForegroundWindow(hwnd);
        }
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (app && app->m_inputHandler && app->m_visible) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            if (app->m_inputHandler->HandleKeyDown(wp, shift, ctrl, hasRec, hasFav)) {
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        break;

    case WM_CHAR:
        if (app && app->m_inputHandler && app->m_visible) {
            app->m_inputHandler->HandleChar(static_cast<wchar_t>(wp));
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        if (app && app->m_inputHandler && app->m_visible) {
            bool hasRec = app->m_recentManager && !app->m_recentManager->GetRecent().empty();
            bool hasFav = app->m_favoritesManager && !app->m_favoritesManager->GetFavorites().empty();
            RECT cr; GetClientRect(hwnd, &cr);
            auto tmpLayout = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                app->m_dpiScale, hasRec, hasFav, app->m_inputHandler->GetResults().size());
            app->m_inputHandler->HandleMouseDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), tmpLayout, hasRec, hasFav);
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
            auto tmpLayout = ComputeLayout(static_cast<float>(cr.right), static_cast<float>(cr.bottom),
                app->m_dpiScale, hasRec, hasFav, app->m_inputHandler->GetResults().size());
            app->m_inputHandler->HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), pt.x, pt.y, tmpLayout);
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
