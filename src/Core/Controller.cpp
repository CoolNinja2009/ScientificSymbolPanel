#include "Core/Controller.h"
#include "Core/Log.h"
#include "Platform/Platform.h"
#include "Symbols/Database.h"
#include "Search/SearchEngine.h"
#include "Storage/Recent.h"
#include "Storage/Favorites.h"
#include "Symbols/Snippets.h"
#include "Symbols/Converters.h"
#include <algorithm>

namespace ssp {

// ============================================================================
// Lifecycle
// ============================================================================

Controller::Controller()  = default;
Controller::~Controller() { Shutdown(); }

bool Controller::Initialize() {
    m_config.Load();

    m_database = std::make_unique<SymbolDatabase>();
    if (!m_database->Load()) return false;

    m_searchEngine = std::make_unique<SearchEngine>();
    m_searchEngine->Build(m_database->GetSymbols());

    m_recentManager = std::make_unique<RecentManager>(m_database.get(), m_config);
    m_recentManager->Load();

    m_favoritesManager = std::make_unique<FavoritesManager>(m_database.get(), m_config);
    m_favoritesManager->Load();

    m_snippetManager = std::make_unique<SnippetManager>();
    auto exeDir = Platform::GetExecutableDir();
    auto bundledSnippets = exeDir / L"data" / L"snippets.json";
    if (!std::filesystem::exists(bundledSnippets))
        bundledSnippets = std::filesystem::current_path() / L"data" / L"snippets.json";
    m_snippetManager->Load(bundledSnippets, m_config.DataDir());

    PerformSearch();
    return true;
}

void Controller::Shutdown() {
    if (m_recentManager)    m_recentManager->Save();
    if (m_favoritesManager) m_favoritesManager->Save();
    m_config.Save();
}

void Controller::OnShow() {
    m_query.clear();
    m_selectedCategory = -1;
    m_selectedIndex = 0;
    m_results.clear();
    m_filteredSymbols.clear();
    PerformSearch();
}

void Controller::OnHide() {
    if (m_recentManager)    m_recentManager->Save();
    if (m_favoritesManager) m_favoritesManager->Save();
    m_config.Save();
}

// ============================================================================
// Query
// ============================================================================

void Controller::SetQuery(const std::wstring& query) {
    m_query = query;
    m_selectedIndex = 0;
    PerformSearch();
}

// ============================================================================
// Category filter
// ============================================================================

void Controller::SetCategory(int index) {
    m_selectedCategory = index;
    m_selectedIndex = 0;
    PerformSearch();
}

Category Controller::GetCategoryFilter() const {
    return static_cast<Category>(std::max(0, m_selectedCategory));
}

// ============================================================================
// Search
// ============================================================================

void Controller::PerformSearch() {
    m_results.clear();
    m_filteredSymbols.clear();

    if (!m_database) return;

    const auto& allSymbols = m_database->GetSymbols();

    if (m_query.empty()) {
        if (m_selectedCategory < 0) {
            m_results.reserve(allSymbols.size());
            m_filteredSymbols.reserve(allSymbols.size());
            for (const auto& sym : allSymbols) {
                m_results.push_back({&sym, 0});
                m_filteredSymbols.push_back(&sym);
            }
        } else {
            Category cat = static_cast<Category>(m_selectedCategory);
            auto catSymbols = m_database->GetByCategory(cat);
            m_filteredSymbols = std::move(catSymbols);
            m_results.reserve(m_filteredSymbols.size());
            for (auto* sym : m_filteredSymbols)
                m_results.push_back({sym, 0});
        }
    } else {
        if (m_searchEngine) {
            auto raw = m_searchEngine->Search(m_query);
            if (m_selectedCategory < 0) {
                m_results = std::move(raw);
            } else {
                Category cat = static_cast<Category>(m_selectedCategory);
                for (auto& r : raw)
                    if (r.symbol && r.symbol->category == cat)
                        m_results.push_back(r);
            }
        }
    }

    ClampSelection();
    NotifyChanged();
}

void Controller::ClampSelection() {
    size_t total = GetResultCount();
    if (total > 0 && m_selectedIndex >= total)
        m_selectedIndex = total - 1;
    if (total == 0)
        m_selectedIndex = 0;
}

// ============================================================================
// Results
// ============================================================================

size_t Controller::GetResultCount() const {
    return m_query.empty() ? m_filteredSymbols.size() : m_results.size();
}

// ============================================================================
// Selection
// ============================================================================

void Controller::SetSelectedIndex(size_t index) {
    size_t total = GetResultCount();
    if (total > 0) {
        m_selectedIndex = std::min(index, total - 1);
    }
}

bool Controller::SelectCurrent() {
    return SelectIndex(m_selectedIndex);
}

bool Controller::SelectIndex(size_t index) {
    size_t total = GetResultCount();
    if (total == 0 || index >= total) return false;

    const Symbol* sym = nullptr;
    if (m_query.empty()) {
        sym = m_filteredSymbols[index];
    } else {
        sym = m_results[index].symbol;
    }

    if (!sym || !m_onInsert) return false;

    m_recentManager->Add(*sym);
    m_onInsert(sym->symbol);
    return true;
}

void Controller::SelectSymbol(const Symbol* sym) {
    if (!sym || !m_onInsert) return;
    m_recentManager->Add(*sym);
    m_onInsert(sym->symbol);
}

// ============================================================================
// Converters
// ============================================================================

std::wstring Controller::TryConvert(const std::wstring& input) {
    std::wstring converted;

    converted = ScientificConverter::Convert(input);
    if (!converted.empty() && converted != input) return converted;

    converted = LaTeXConverter::Convert(input);
    if (!converted.empty() && converted != input) return converted;

    converted = SuperscriptBuilder::Convert(input);
    if (!converted.empty() && converted != input) return converted;

    converted = SubscriptBuilder::Convert(input);
    if (!converted.empty() && converted != input) return converted;

    converted = FractionBuilder::Convert(input);
    if (!converted.empty() && converted != input) return converted;

    return input;
}

void Controller::InsertText(const std::wstring& text) {
    if (m_onInsert) m_onInsert(text);
}

// ============================================================================
// Recent / Favorites helpers
// ============================================================================

bool Controller::HasRecent() const {
    return m_recentManager && !m_recentManager->GetRecent().empty();
}

bool Controller::HasFavorites() const {
    return m_favoritesManager && !m_favoritesManager->GetFavorites().empty();
}

void Controller::ToggleFavorite(const Symbol* sym) {
    if (!m_favoritesManager || !sym) return;
    if (m_favoritesManager->IsFavorite(*sym))
        m_favoritesManager->Remove(*sym);
    else
        m_favoritesManager->Add(*sym);
}

bool Controller::IsFavorite(const Symbol* sym) const {
    return m_favoritesManager && sym && m_favoritesManager->IsFavorite(*sym);
}

const std::vector<const Symbol*>& Controller::GetRecentSymbols() const {
    static const std::vector<const Symbol*> empty;
    return m_recentManager ? m_recentManager->GetRecent() : empty;
}

const std::vector<const Symbol*>& Controller::GetFavoritesSymbols() const {
    static const std::vector<const Symbol*> empty;
    return m_favoritesManager ? m_favoritesManager->GetFavorites() : empty;
}

// ============================================================================
// Notify / Callbacks
// ============================================================================

void Controller::NotifyChanged() {
    if (m_onChanged) m_onChanged();
}

void Controller::RequestClose() {
    if (m_onClose) m_onClose();
}

} // namespace ssp
