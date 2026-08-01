#pragma once
#include "Core/Types.h"
#include "Core/Config.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>

struct GLFWwindow;

namespace ssp {

class SymbolDatabase;
class SearchEngine;
class RecentManager;
class FavoritesManager;
class SnippetManager;

class Panel {
public:
    using InsertCallback = std::function<void(const std::wstring&)>;

    Panel();
    ~Panel();

    bool Initialize(GLFWwindow* window);
    void Render();
    void Shutdown();

    // Lifecycle
    void OnShow();
    void OnHide();
    bool WantsHide() const;       // Escape pressed
    bool HasPendingInsert() const; // Symbol selected, ready to type
    std::wstring TakePendingInsert();

    // Access config (read by InlineExpander for runtime settings)
    const Config& GetConfig() const { return m_config; }

private:
    Config m_config;
    std::unique_ptr<SymbolDatabase> m_database;
    std::unique_ptr<SearchEngine> m_searchEngine;
    std::unique_ptr<RecentManager> m_recentManager;
    std::unique_ptr<FavoritesManager> m_favoritesManager;
    std::unique_ptr<SnippetManager> m_snippetManager;

    GLFWwindow* m_window = nullptr;

    // UI state
    char m_searchBuf[256] = {};
    int  m_selectedCategory = -1;
    int  m_selectedResult = 0;
    int  m_hoveredResult = -1;
    int  m_gridColumns = 6;
    float m_dpiScale = 1.0f;
    bool m_styleInitialized = false;
    int m_dragStartX = 0, m_dragStartY = 0;  // For smooth window drag

    bool m_wantsHide = false;
    bool m_focusSearch = false;       // Set by OnShow, consumed by DrawSearchBar
    std::wstring m_pendingInsert;     // Symbol text to insert after panel closes

    // Search results
    std::vector<SearchResult> m_results;
    std::vector<const Symbol*> m_filteredSymbols;

    // Callbacks
    InsertCallback m_onInsert;

    // Methods
    void PerformSearch();
    void SelectSymbol(const Symbol* sym);

    void DrawSearchBar();
    void DrawCategoryBar();
    void DrawResultsGrid();
    void DrawRecentStrip();
    void DrawFavoritesStrip();
    bool DrawSymbolCell(const Symbol* sym, int id, float cellSize, bool selected);
    void DrawStatusBar();
    void HandleKeyboardShortcuts();
};

} // namespace ssp
