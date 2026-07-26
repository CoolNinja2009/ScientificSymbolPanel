#pragma once
#include "Core/Types.h"
#include "Core/Config.h"
#include <memory>
#include <string>

// Forward declarations
namespace ssp {
class SymbolDatabase;
class SearchEngine;
class JsonStore;
class Renderer;
class ThemeManager;
class Animation;
class InputHandler;
class RecentManager;
class FavoritesManager;
class SnippetManager;
}

struct IWICImagingFactory;
struct ID2D1Factory;
struct IDWriteFactory;

namespace ssp {

class App {
public:
    App();
    ~App();

    // Prevent copy
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Lifecycle
    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Shutdown();

    // Actions
    void ShowPanel();
    void HidePanel();
    void TogglePanel();
    void InsertSymbol(const std::wstring& text);
    void InsertSnippet(const std::wstring& text);

    // Access
    Config& GetConfig() { return m_config; }
    SymbolDatabase& GetDatabase();
    SearchEngine& GetSearchEngine();
    Renderer* GetRenderer() const { return m_renderer.get(); }
    ThemeManager* GetThemeManager() const { return m_themeManager.get(); }
    InputHandler* GetInputHandler() const { return m_inputHandler.get(); }
    RecentManager* GetRecentManager() const { return m_recentManager.get(); }
    FavoritesManager* GetFavoritesManager() const { return m_favoritesManager.get(); }
    SnippetManager* GetSnippetManager() const { return m_snippetManager.get(); }
    Animation* GetAnimation() const { return m_animation.get(); }
    ID2D1Factory* GetD2DFactory() const { return m_d2dFactory.get(); }
    IDWriteFactory* GetDWriteFactory() const { return m_dwriteFactory.get(); }
    IWICImagingFactory* GetWicFactory() const { return m_wicFactory.get(); }
    HINSTANCE GetInstance() const { return m_hInstance; }
    HWND GetWindow() const { return m_hwnd; }
    bool IsVisible() const { return m_visible; }
    float GetDpiScale() const { return m_dpiScale; }

    // Called by InputHandler when symbol is inserted
    void OnSymbolInserted(const Symbol* sym);

private:
    bool InitCOM();
    void InitFactories();
    void InitSubsystems();
    bool RegisterWindowClass();
    bool CreateMainWindow();
    void RegisterHotkey();
    void UnregisterHotkey();
    void RenderFrame();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hwndPrevFocus = nullptr;
    bool m_visible = false;
    float m_dpiScale = 1.0f;
    bool m_inserting = false;       // True during focus-yield insert (don't hide on deactivate)
    bool m_cursorVisible = true;    // Blinking cursor in search bar

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

    // COM/DirectX
    struct ComDeleter {
        template<typename T>
        void operator()(T* p) const { if (p) p->Release(); }
    };
    template<typename T>
    using ComPtr = std::unique_ptr<T, ComDeleter>;

    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<IWICImagingFactory> m_wicFactory;

public:
    // The single App instance (public for GetApp())
    static App* s_instance;
};

inline App& GetApp() { return *App::s_instance; }

} // namespace ssp
