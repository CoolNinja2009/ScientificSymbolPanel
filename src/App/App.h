#pragma once
#include "Core/Types.h"
#include "Core/Config.h"
#include <memory>
#include <SDL.h>
#include <string>
#include <chrono>
#include <filesystem>

struct SDL_Window;
typedef struct SDL_Window SDL_Window;

namespace ssp {
class SymbolDatabase;
class SearchEngine;
class Renderer;
class ThemeManager;
class Animation;
class InputHandler;
class RecentManager;
class FavoritesManager;
class SnippetManager;
}

namespace ssp {

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool Initialize();
    int  Run();
    void Shutdown();

    void ShowPanel();
    void HidePanel();
    void TogglePanel();

    Config& GetConfig() { return m_config; }
    SymbolDatabase& GetDatabase();
    SearchEngine& GetSearchEngine();
    Renderer* GetRenderer() const { return m_renderer.get(); }
    InputHandler* GetInputHandler() const { return m_inputHandler.get(); }
    RecentManager* GetRecentManager() const { return m_recentManager.get(); }
    FavoritesManager* GetFavoritesManager() const { return m_favoritesManager.get(); }
    SnippetManager* GetSnippetManager() const { return m_snippetManager.get(); }
    Animation* GetAnimation() const { return m_animation.get(); }
    SDL_Window* GetWindow() const { return m_window; }
    bool IsVisible() const { return m_visible; }
    float GetDpiScale() const { return m_dpiScale; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    void OnSymbolInserted(const Symbol* sym);
    double GetStartupTimeMs() const { return m_startupMs; }
    void RequestRedraw() { m_needsRedraw = true; }

private:
    void InitSubsystems();
    void RenderFrame();
    void LoadDatabase();
    void ReloadDatabaseIfChanged();
    void ProcessEvent(const SDL_Event& e);

    SDL_Window* m_window = nullptr;
    bool m_visible = false;
    bool m_needsRedraw = false;
    bool m_running = true;
    bool m_inserting = false;
    bool m_cursorVisible = true;
    float m_dpiScale = 1.0f;
    int m_width = 360;
    int m_height = 480;

    Config m_config;
    std::unique_ptr<SymbolDatabase> m_database;
    std::unique_ptr<SearchEngine> m_searchEngine;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<ThemeManager> m_themeManager;
    std::unique_ptr<Animation> m_animation;
    std::unique_ptr<InputHandler> m_inputHandler;
    std::unique_ptr<RecentManager> m_recentManager;
    std::unique_ptr<FavoritesManager> m_favoritesManager;
    std::unique_ptr<SnippetManager> m_snippetManager;

    std::filesystem::file_time_type m_symbolsMtime;
    std::filesystem::path m_symbolsPath;

    std::chrono::steady_clock::time_point m_constructTime;
    double m_startupMs = 0.0;

public:
    static App* s_instance;
};

inline App& GetApp() { return *App::s_instance; }

} // namespace ssp
