#include <SDL.h>
#include "App/App.h"
#include "config.h"
#include "Platform/Platform.h"
#include "Symbols/Database.h"
#include "Search/SearchEngine.h"
#include "Storage/Recent.h"
#include "Storage/Favorites.h"
#include "Symbols/Snippets.h"
#include "UI/Renderer.h"
#include "UI/Themes.h"
#include "UI/Layout.h"
#include "UI/Animation.h"
#include "App/InputHandler.h"
#include <algorithm>
#include <chrono>
#include <codecvt>

#ifdef DrawText
#undef DrawText
#endif

namespace ssp {

App* App::s_instance = nullptr;

App::App() {
    s_instance = this;
    m_constructTime = std::chrono::steady_clock::now();
}

App::~App() { Shutdown(); s_instance = nullptr; }

// ============================================================================
// Initialize
// ============================================================================

bool App::Initialize() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    m_window = SDL_CreateWindow("Scientific Symbol Panel",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SSP_WINDOW_WIDTH, SSP_WINDOW_HEIGHT,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP |
        SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    m_config.Load();

    // Font
    std::string fontPath = Platform::FindFontFile().string();

    // Renderer
    m_renderer = std::make_unique<Renderer>(m_window);
    if (!m_renderer->Initialize(fontPath.empty() ? nullptr : fontPath.c_str(), 14)) {
        fprintf(stderr, "Renderer init failed\n");
        return false;
    }

    SDL_GetWindowSize(m_window, &m_width, &m_height);

    LoadDatabase();
    m_searchEngine = std::make_unique<SearchEngine>();
    m_searchEngine->Build(m_database->GetSymbols());
    InitSubsystems();

    // Global hotkey
    Platform::RegisterGlobalHotkey(SSP_HOTKEY_MOD, SSP_HOTKEY_KEY, []() {
        if (App::s_instance) App::s_instance->TogglePanel();
    });

    // Benchmark
    auto now = std::chrono::steady_clock::now();
    m_startupMs = std::chrono::duration<double, std::milli>(now - m_constructTime).count();

    return true;
}

void App::LoadDatabase() {
    m_database = std::make_unique<SymbolDatabase>();
    auto exeDir = Platform::ExeDir();
    m_symbolsPath = exeDir / "data" / "symbols.bin";
    if (!std::filesystem::exists(m_symbolsPath))
        m_symbolsPath = exeDir / ".." / "data" / "symbols.bin";
    if (!std::filesystem::exists(m_symbolsPath))
        m_symbolsPath = std::filesystem::current_path() / "data" / "symbols.bin";
    m_database->Load();
    for (auto& p : { m_symbolsPath, m_symbolsPath.parent_path() / "symbols.json" }) {
        if (std::filesystem::exists(p)) {
            m_symbolsMtime = std::filesystem::last_write_time(p);
            break;
        }
    }
}

void App::ReloadDatabaseIfChanged() {
    auto paths = { m_symbolsPath, m_symbolsPath.parent_path() / "symbols.json" };
    bool changed = false;
    for (auto& p : paths) {
        if (std::filesystem::exists(p)) {
            auto mt = std::filesystem::last_write_time(p);
            if (mt != m_symbolsMtime) { changed = true; m_symbolsMtime = mt; break; }
        }
    }
    if (!changed) return;
    m_database->Load();
    m_searchEngine->Build(m_database->GetSymbols());
    if (m_inputHandler) {
        m_inputHandler->SetDatabase(m_database.get());
        m_inputHandler->RefreshResults();
    }
}

void App::InitSubsystems() {
    auto& cfg = m_config.Get();
    bool isDark = Platform::IsDarkMode();
    if (cfg.theme == ThemeMode::Light) isDark = false;
    else if (cfg.theme == ThemeMode::Dark) isDark = true;

    m_themeManager = std::make_unique<ThemeManager>(isDark, Platform::GetAccentColor());
    m_animation = std::make_unique<Animation>();
    m_animation->enabled = SSP_ANIMATIONS && !Platform::IsLowEndMachine();

    m_renderer->SetTheme(m_themeManager->GetColors());

    m_recentManager = std::make_unique<RecentManager>(m_database.get(), m_config);
    m_recentManager->Load();
    m_favoritesManager = std::make_unique<FavoritesManager>(m_database.get(), m_config);
    m_favoritesManager->Load();

    m_snippetManager = std::make_unique<SnippetManager>();
    auto exeDir = Platform::ExeDir();
    auto bundledSnippets = exeDir / "data" / "snippets.json";
    if (!std::filesystem::exists(bundledSnippets))
        bundledSnippets = std::filesystem::current_path() / "data" / "snippets.json";
    m_snippetManager->Load(bundledSnippets, m_config.DataDir());

    m_inputHandler = std::make_unique<InputHandler>();
    m_inputHandler->SetSearchEngine(m_searchEngine.get());
    m_inputHandler->SetDatabase(m_database.get());
    m_inputHandler->SetRecentManager(m_recentManager.get());
    m_inputHandler->SetFavoritesManager(m_favoritesManager.get());

    m_inputHandler->SetInsertCallback([this](const std::wstring& text) {
        m_inserting = true;
        HidePanel();
        Platform::SendUnicodeText(text);
        m_inserting = false;
    });
    m_inputHandler->SetCloseCallback([this]() { HidePanel(); });
    m_inputHandler->SetInvalidateCallback([this]() { RequestRedraw(); });
}

// ============================================================================
// Run / Shutdown
// ============================================================================

int App::Run() {
    SDL_Event e;
    while (m_running) {
        while (SDL_PollEvent(&e)) {
            ProcessEvent(e);
        }
        if (m_visible && m_needsRedraw) {
            m_needsRedraw = false;
            ReloadDatabaseIfChanged();
            RenderFrame();
        }
        SDL_Delay(8); // ~120 FPS cap when visible, 8ms poll when hidden
    }
    return 0;
}

void App::Shutdown() {
    Platform::UnregisterGlobalHotkey();
    if (m_recentManager) m_recentManager->Save();
    if (m_favoritesManager) m_favoritesManager->Save();
    m_config.Save();
    m_inputHandler.reset(); m_snippetManager.reset(); m_favoritesManager.reset();
    m_recentManager.reset(); m_renderer.reset(); m_themeManager.reset();
    m_animation.reset(); m_searchEngine.reset(); m_database.reset();
    if (m_window) { SDL_DestroyWindow(m_window); m_window = nullptr; }
    SDL_Quit();
}

void App::ShowPanel() {
    if (m_visible) return;
    ReloadDatabaseIfChanged();

    SDL_Rect db;
    SDL_GetDisplayBounds(0, &db);
    int x = (db.w - m_width) / 2 + db.x;
    int y = (db.h - m_height) / 2 + db.y;
    SDL_SetWindowPosition(m_window, x, y);

    SDL_ShowWindow(m_window);
    SDL_RaiseWindow(m_window);
    m_visible = true;

    if (m_inputHandler) m_inputHandler->Reset();
    if (m_animation && m_animation->enabled) m_animation->StartFadeIn();
    RequestRedraw();
}

void App::HidePanel() {
    if (!m_visible) return;
    m_visible = false;
    SDL_HideWindow(m_window);
}

void App::TogglePanel() { m_visible ? HidePanel() : ShowPanel(); }

SymbolDatabase& App::GetDatabase()   { return *m_database; }
SearchEngine& App::GetSearchEngine() { return *m_searchEngine; }
void App::OnSymbolInserted(const Symbol* sym) {
    if (sym && m_recentManager) m_recentManager->Add(*sym);
}

// ============================================================================
// Rendering
// ============================================================================

void App::RenderFrame() {
    if (!m_renderer || !m_visible) return;

    auto& colors = m_themeManager->GetColors();
    float w = (float)m_width;
    float h = (float)m_height;
    auto& input = *m_inputHandler;
    bool hasRecent = m_recentManager ? !m_recentManager->GetRecent().empty() : false;
    bool hasFavorites = m_favoritesManager ? !m_favoritesManager->GetFavorites().empty() : false;

    auto layout = ComputeLayout(w, h, m_dpiScale, hasRecent, hasFavorites, input.GetResults().size());
    layout.scrollOffset = input.GetScrollOffset();
    int vr = (int)(layout.resultsGrid.height / layout.cellSize);
    if (vr < 1) vr = 1;
    input.UpdateLayoutInfo(layout.columns, vr, layout.maxScroll, layout.cellSize);

    if (!m_renderer->BeginDraw()) return;

    m_renderer->FillRect({0, 0, w, h}, colors.bgPrimary);

    // Search bar
    {
        RectF r = layout.searchBar;
        m_renderer->DrawRoundedRect(
            {r.x + 4, r.y + 4, r.width - 8, r.height - 8}, 6.0f, colors.bgSecondary);

        const auto& q = input.GetQuery();
        float fs = SSP_FONT_SIZE_SEARCH * m_dpiScale;

        if (input.GetActiveZone() == Zone::SearchBar && input.HasSelection()) {
            int ss = input.GetSelectStart(), se = input.GetSelectEnd();
            if (ss > se) std::swap(ss, se);
            float selX = r.x + 12.0f + m_renderer->MeasureTextWidth(q, fs, ss);
            float selW = m_renderer->MeasureTextWidth(q, fs, se) -
                         m_renderer->MeasureTextWidth(q, fs, ss);
            m_renderer->FillRect({selX, r.y + 6.0f, selW, r.height - 12.0f}, 0x664078D4);
        }

        if (q.empty())
            m_renderer->DrawText(L"Search symbols...", r.x + 12, r.y + 4, fs, colors.textMuted);
        else
            m_renderer->DrawText(q, r.x + 12, r.y + 4, fs, colors.textPrimary);

        if (input.GetActiveZone() == Zone::SearchBar && m_cursorVisible) {
            float cx = r.x + 12.0f + m_renderer->MeasureTextWidth(q, fs, (int)input.GetCursorPos());
            m_renderer->FillRect({cx, r.y + 8.0f, 1.5f, r.height - 16.0f}, colors.accent);
        }
    }

    // Category bar
    {
        RectF r = layout.categoryBar;
        float fs = SSP_FONT_SIZE_SMALL * m_dpiScale;
        constexpr const wchar_t* catShort[] = {
            L"Math", L"Greek", L"Phys", L"Chem", L"Elec", L"SI",
            L"Logic", L"Prog", L"Arrow", L"Curr", L"Frac", L"Super",
            L"Sub", L"Stat", L"Geom", L"Calc", L"Astro", L"Misc", L"Custom"
        };
        float catW = r.width / 19.0f;
        for (int i = 0; i < 19 && i < (int)Category::COUNT; i++) {
            RectF cr = {r.x + i * catW, r.y, catW - 2, r.height};
            bool active = !input.IsAllCategories() &&
                          input.GetCategoryFilter() == (Category)i;
            if (active) m_renderer->DrawRoundedRect(cr, 4.0f, colors.accent);
            m_renderer->DrawTextCentered(catShort[i], cr, fs,
                active ? colors.textPrimary : colors.textSecondary);
        }
    }

    // Results grid
    {
        RectF r = layout.resultsGrid;
        m_renderer->PushClip(r);

        const auto& results = input.GetResults();
        float cellSize = layout.cellSize;
        int cols = layout.columns;
        float sy = r.y - input.GetScrollOffset();
        float sf = SSP_FONT_SIZE_SYMBOL * m_dpiScale;
        float tf = SSP_FONT_SIZE_SMALL * m_dpiScale;
        size_t sel = input.GetSelectedIndex();

        for (size_t i = 0; i < results.size(); i++) {
            int row = (int)i / cols, col = (int)i % cols;
            float cx = r.x + col * cellSize;
            float cy = sy + row * cellSize;
            if (cy + cellSize < r.y || cy > r.y + r.height) continue;

            RectF cell = {cx, cy, cellSize, cellSize};
            if (i == sel)
                m_renderer->DrawRoundedRect(cell, 6.0f, colors.selected);
            else if (input.IsHovering() && (size_t)input.GetHoverIndex() == i)
                m_renderer->DrawRoundedRect(cell, 6.0f, colors.hover);

            const auto* sym = results[i].symbol;
            if (sym) {
                m_renderer->DrawTextCentered(sym->symbol,
                    {cx, cy, cellSize, cellSize * 0.65f}, sf, colors.textPrimary);
                if (!sym->name.empty())
                    m_renderer->DrawTextCentered(sym->name,
                        {cx, cy + cellSize * 0.55f, cellSize, cellSize * 0.4f}, tf, colors.textMuted);
            }
        }

        m_renderer->PopClip();
    }

    // Scrollbar
    if (layout.maxScroll > 0) {
        float barW = 6.0f;
        float visH = layout.resultsGrid.height;
        float totalH = visH + layout.maxScroll;
        float thumbH = visH * (visH / totalH);
        if (thumbH < 20) thumbH = 20;
        float thumbY = layout.resultsGrid.y + (input.GetScrollOffset() / layout.maxScroll) * (visH - thumbH);
        m_renderer->DrawRoundedRect(
            {layout.resultsGrid.x + layout.resultsGrid.width - barW - 2, thumbY, barW, thumbH},
            3.0f, colors.scrollbar);
    }

    m_renderer->EndDraw();
}

// ============================================================================
// Event processing
// ============================================================================

void App::ProcessEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_QUIT:
        m_running = false;
        break;
    case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && m_visible && !m_inserting) {
            HidePanel();
        } else if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            m_width = e.window.data1;
            m_height = e.window.data2;
            if (m_renderer) m_renderer->Resize(m_width, m_height);
        } else if (e.window.event == SDL_WINDOWEVENT_EXPOSED && m_visible) {
            RequestRedraw();
        }
        break;
    case SDL_KEYDOWN:
        if (m_visible) {
            auto* input = GetInputHandler();
            if (!input) break;
            SDL_Keymod mod = SDL_GetModState();
            bool ctrl  = (mod & KMOD_CTRL) != 0;
            bool shift = (mod & KMOD_SHIFT) != 0;
            int vk = 0;
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:    vk = 256; break;
            case SDLK_RETURN:    vk = 257; break;
            case SDLK_TAB:       vk = 258; break;
            case SDLK_LEFT:      vk = 263; break;
            case SDLK_UP:        vk = 264; break;
            case SDLK_RIGHT:     vk = 265; break;
            case SDLK_DOWN:      vk = 266; break;
            case SDLK_PAGEUP:    vk = 267; break;
            case SDLK_PAGEDOWN:  vk = 268; break;
            case SDLK_HOME:      vk = 269; break;
            case SDLK_END:       vk = 270; break;
            case SDLK_DELETE:    vk = 271; break;
            case SDLK_BACKSPACE: vk = 272; break;
            case SDLK_SLASH:     vk = 287; break;
            default: break;
            }
            if (ctrl && e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_9)
                vk = '1' + (e.key.keysym.sym - SDLK_1);
            if (vk != 0) {
                bool hasR = m_recentManager && !m_recentManager->GetRecent().empty();
                bool hasF = m_favoritesManager && !m_favoritesManager->GetFavorites().empty();
                if (input->HandleKeyDown(vk, shift, ctrl, hasR, hasF))
                    RequestRedraw();
            }
        }
        break;
    case SDL_TEXTINPUT:
        if (m_visible) {
            auto* input = GetInputHandler();
            if (input && e.text.text[0]) {
                // Convert UTF-8 to wchar_t
                std::string utf8(e.text.text);
                std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
                std::wstring w = conv.from_bytes(utf8);
                if (input->HandleChar(w[0]))
                    RequestRedraw();
            }
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (m_visible) {
            auto* input = GetInputHandler();
            if (!input) break;
            int mx = e.button.x, my = e.button.y;
            bool hasR = m_recentManager && !m_recentManager->GetRecent().empty();
            bool hasF = m_favoritesManager && !m_favoritesManager->GetFavorites().empty();
            auto layout = ComputeLayout((float)m_width, (float)m_height, m_dpiScale,
                                         hasR, hasF, input->GetResults().size());
            layout.scrollOffset = input->GetScrollOffset();
            if (input->HandleMouseDown(mx, my, layout, hasR, hasF))
                RequestRedraw();
        }
        break;
    case SDL_MOUSEWHEEL:
        if (m_visible) {
            auto* input = GetInputHandler();
            if (!input) break;
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            bool hasR = m_recentManager && !m_recentManager->GetRecent().empty();
            bool hasF = m_favoritesManager && !m_favoritesManager->GetFavorites().empty();
            auto layout = ComputeLayout((float)m_width, (float)m_height, m_dpiScale,
                                         hasR, hasF, input->GetResults().size());
            layout.scrollOffset = input->GetScrollOffset();
            if (input->HandleMouseWheel(e.wheel.y * 40, mx, my, layout))
                RequestRedraw();
        }
        break;
    }
}

} // namespace ssp
