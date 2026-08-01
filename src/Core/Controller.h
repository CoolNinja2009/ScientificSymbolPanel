#pragma once
#include "Core/Types.h"
#include "Core/Config.h"
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace ssp {

class SymbolDatabase;
class SearchEngine;
class RecentManager;
class FavoritesManager;
class SnippetManager;

// ============================================================================
// Controller — shared backend for all frontends
//
// Owns all data services (database, search, recent, favorites, snippets) and
// search state (query, category filter, results, selection).  Both the Win32
// Direct2D frontend and the GLFW ImGui frontend use this single class so the
// backend is implemented once.
// ============================================================================
class Controller {
public:
    using Callback     = std::function<void()>;
    using TextCallback = std::function<void(const std::wstring&)>;

    Controller();
    ~Controller();

    // Non-copyable
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    // --- Lifecycle -----------------------------------------------
    bool Initialize();
    void Shutdown();
    void OnShow();   // reset transient state
    void OnHide();   // persist recent / favorites

    // --- Query --------------------------------------------------
    void SetQuery(const std::wstring& query);
    const std::wstring& GetQuery() const { return m_query; }

    // --- Category filter ----------------------------------------
    // -1 = all categories; 0..N = specific Category enum ordinal
    void SetCategory(int index);
    int  GetCategory() const          { return m_selectedCategory; }
    bool IsAllCategories() const      { return m_selectedCategory < 0; }
    Category GetCategoryFilter() const;   // only valid when !IsAllCategories()

    // --- Results ------------------------------------------------
    const std::vector<SearchResult>&  GetResults() const         { return m_results; }
    const std::vector<const Symbol*>& GetFilteredSymbols() const { return m_filteredSymbols; }
    size_t GetResultCount() const;

    // --- Selection ----------------------------------------------
    void   SetSelectedIndex(size_t index);
    size_t GetSelectedIndex() const { return m_selectedIndex; }
    bool   SelectCurrent();                // insert currently selected symbol
    bool   SelectIndex(size_t index);      // insert symbol at index
    void   SelectSymbol(const Symbol* sym);// insert arbitrary symbol (recent/fav strips)

    // --- Converters (Enter in empty search → try all pipelines) -
    std::wstring TryConvert(const std::wstring& input);
    void        InsertText(const std::wstring& text);  // send arbitrary text via insert callback

    // --- Recent / Favorites -------------------------------------
    bool HasRecent() const;
    bool HasFavorites() const;
    void ToggleFavorite(const Symbol* sym);
    bool IsFavorite(const Symbol* sym) const;

    // --- Service accessors --------------------------------------
    Config&           GetConfig()         { return m_config; }
    SymbolDatabase&   GetDatabase()       { return *m_database; }
    SearchEngine&     GetSearchEngine()   { return *m_searchEngine; }
    RecentManager&    GetRecentManager()  { return *m_recentManager; }
    FavoritesManager& GetFavoritesManager(){ return *m_favoritesManager; }
    SnippetManager&   GetSnippetManager() { return *m_snippetManager; }
    const std::vector<const Symbol*>& GetRecentSymbols() const;
    const std::vector<const Symbol*>& GetFavoritesSymbols() const;

    // --- Callbacks ----------------------------------------------
    void SetInsertCallback(TextCallback cb)  { m_onInsert = std::move(cb); }
    void SetCloseCallback(Callback cb)       { m_onClose = std::move(cb); }
    void SetChangedCallback(Callback cb)     { m_onChanged = std::move(cb); }

    // --- Actions that trigger callbacks -------------------------
    void RequestClose();   // Calls close callback (Esc key, etc.)

private:
    void PerformSearch();
    void ClampSelection();
    void NotifyChanged();

    // --- Owned services -----------------------------------------
    Config m_config;
    std::unique_ptr<SymbolDatabase>   m_database;
    std::unique_ptr<SearchEngine>     m_searchEngine;
    std::unique_ptr<RecentManager>    m_recentManager;
    std::unique_ptr<FavoritesManager> m_favoritesManager;
    std::unique_ptr<SnippetManager>   m_snippetManager;

    // --- Search state -------------------------------------------
    std::wstring m_query;
    int          m_selectedCategory = -1;   // -1 = all
    size_t       m_selectedIndex    = 0;
    std::vector<SearchResult>  m_results;
    std::vector<const Symbol*> m_filteredSymbols;

    // --- Callbacks ----------------------------------------------
    TextCallback m_onInsert;
    Callback     m_onClose;
    Callback     m_onChanged;
};

} // namespace ssp
