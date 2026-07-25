#pragma once
#include "Core/Types.h"
#include <vector>
#include <string>
#include <functional>

namespace ssp {

// Forward declarations — peers are building these
class SearchEngine;
class SymbolDatabase;
class RecentManager;
class FavoritesManager;
struct PanelLayout;

// ============================================================================
// Zone — which UI region currently has keyboard focus
// ============================================================================
enum class Zone : uint8_t {
    SearchBar,
    RecentStrip,
    FavoritesStrip,
    CategoryBar,
    ResultsGrid,
    COUNT
};

// ============================================================================
// InputHandler — keyboard and mouse input for the symbol panel
// ============================================================================
class InputHandler {
public:
    InputHandler();

    // --- Dependencies (set once after construction) ---
    void SetSearchEngine(SearchEngine* se)     { m_searchEngine = se; }
    void SetDatabase(SymbolDatabase* db)       { m_database = db; }
    void SetRecentManager(RecentManager* rm)   { m_recentManager = rm; }
    void SetFavoritesManager(FavoritesManager* fm) { m_favoritesManager = fm; }

    // --- Callbacks ---
    using ActionCallback = std::function<void()>;
    using TextCallback   = std::function<void(const std::wstring&)>;

    void SetInsertCallback(TextCallback cb)         { m_onInsert = std::move(cb); }
    void SetCloseCallback(ActionCallback cb)        { m_onClose = std::move(cb); }
    void SetInvalidateCallback(ActionCallback cb)   { m_onInvalidate = std::move(cb); }

    // --- Input handlers — return true if the event was consumed ---
    bool HandleKeyDown(WPARAM vk, bool shift, bool ctrl,
                       bool hasRecent, bool hasFavorites);
    bool HandleChar(wchar_t ch);
    bool HandleMouseDown(int x, int y, const PanelLayout& layout,
                         bool hasRecent, bool hasFavorites);
    bool HandleMouseWheel(int delta, int x, int y, const PanelLayout& layout);
    void HandleMouseMove(int x, int y, const PanelLayout& layout,
                         bool hasRecent, bool hasFavorites);
    // --- Hover state (read by Renderer) ---
    bool IsHovering() const                   { return m_hoverIndex >= 0; }
    int  GetHoverIndex() const                { return m_hoverIndex; }
    Zone GetHoverZone() const                 { return m_hoverZone; }
    // Called after layout computation so grid nav uses real dimensions
    void UpdateLayoutInfo(int gridColumns, int visibleRows, float maxScroll);

    // --- Lifecycle ---
    void Reset();           // Called when panel opens / resets to defaults
    void RefreshResults();  // Re-run search after external change

    // --- State queries (read by Renderer) ---
    Zone GetActiveZone() const               { return m_activeZone; }
    const std::wstring& GetQuery() const     { return m_query; }
    Category GetCategoryFilter() const       { return m_categoryFilter; }
    bool IsAllCategories() const             { return m_allCategories; }
    size_t GetSelectedIndex() const          { return m_selectedIndex; }
    float GetScrollOffset() const            { return m_scrollOffset; }

    const std::vector<SearchResult>& GetResults() const        { return m_results; }
    const std::vector<const Symbol*>& GetFilteredSymbols() const { return m_filteredSymbols; }

private:
    // --- Navigation ---
    void CycleZone(bool forward, bool hasRecent, bool hasFavorites);
    void ScrollBy(float delta);

    // --- Actions ---
    void ExecuteAction(bool hasRecent, bool hasFavorites);
    void InsertSelectedResult();

    // --- Search ---
    void PerformSearch();

    // --- Hit testing ---
    static int HitTestCategory(float x, const RectF& bar, int numCategories);
    static int HitTestResultCell(float x, float y, const RectF& grid,
                                  float cellSize, int columns);
    static int HitTestStripItem(float x, const RectF& strip,
                                 float cellSize, int itemCount);

    // --- State ---
    Zone        m_activeZone     = Zone::SearchBar;
    std::wstring m_query;
    bool        m_allCategories  = true;
    Category    m_categoryFilter = Category::Mathematics;
    size_t      m_selectedIndex  = 0;
    float       m_scrollOffset   = 0.0f;
    int         m_gridColumns    = 6;
    int         m_visibleRows    = 5;
    float       m_maxScroll      = 0.0f;
    int         m_hoverIndex     = -1;   // -1 = not hovering
    Zone        m_hoverZone      = Zone::SearchBar;

    // Cached query results
    std::vector<SearchResult>  m_results;
    std::vector<const Symbol*> m_filteredSymbols; // category-only view (no query)

    // Dependencies (not owned)
    SearchEngine*     m_searchEngine     = nullptr;
    SymbolDatabase*   m_database         = nullptr;
    RecentManager*    m_recentManager    = nullptr;
    FavoritesManager* m_favoritesManager = nullptr;

    // Callbacks
    TextCallback   m_onInsert;
    ActionCallback m_onClose;
    ActionCallback m_onInvalidate;
};

} // namespace ssp
