#include <windows.h>
#include "UI/InputHandler.h"
#include "Core/Log.h"
#include "Symbols/Database.h"
#include "Storage/Recent.h"
#include "Symbols/Converters.h"
#include "UI/Layout.h"
#include "UI/TextEdit.h"
#include <algorithm>

// Use actual includes instead of forward declarations
#include "Search/SearchEngine.h"
#include "Storage/Favorites.h"

namespace ssp {

// ============================================================================
// Helpers
// ============================================================================

static constexpr int kVisibleCategories = 9;   // Ctrl+1..9 map

// ============================================================================
// Construction / Destruction
// ============================================================================

InputHandler::InputHandler()
{
    m_textEdit = TextEdit_Create();
    TextEdit_SetString(m_textEdit, &m_query);
}

InputHandler::~InputHandler() { TextEdit_Destroy(m_textEdit); }
// ============================================================================

void InputHandler::Reset() {
    m_activeZone     = Zone::SearchBar;
    m_query.clear();
    m_allCategories  = true;
    m_categoryFilter = Category::Mathematics;
    m_selectedIndex  = 0;
    m_scrollOffset   = 0.0f;
    m_results.clear();
    m_filteredSymbols.clear();
    TextEdit_Init(m_textEdit);
    TextEdit_SetString(m_textEdit, &m_query);
    PerformSearch();
}

// ============================================================================
// Refresh
// ============================================================================

void InputHandler::RefreshResults() {
    PerformSearch();
}

// ============================================================================
// Text editing (stb_textedit delegates)
// ============================================================================

void InputHandler::AppendToQuery(const std::wstring& text) {
    TextEdit_Paste(m_textEdit, text);
    m_activeZone = Zone::SearchBar;
    m_selectedIndex = 0;
    m_scrollOffset = 0.0f;
    PerformSearch();
}

int InputHandler::GetCursorPos() const    { return TextEdit_GetCursor(m_textEdit); }
bool InputHandler::HasSelection() const   { return TextEdit_HasSelection(m_textEdit); }
int InputHandler::GetSelectStart() const  { return TextEdit_GetSelectStart(m_textEdit); }
int InputHandler::GetSelectEnd() const    { return TextEdit_GetSelectEnd(m_textEdit); }
std::wstring InputHandler::GetSelection() const { return TextEdit_GetSelection(m_textEdit); }
void InputHandler::CutSelection()         { TextEdit_Cut(m_textEdit); }

// ============================================================================
// Search
// ============================================================================

void InputHandler::PerformSearch() {
    m_results.clear();
    m_filteredSymbols.clear();

    if (!m_database) return;

    const auto& allSymbols = m_database->GetSymbols();

    if (m_query.empty()) {
        // No query: show by category or all
        if (m_allCategories) {
            m_results.reserve(allSymbols.size());
            m_filteredSymbols.reserve(allSymbols.size());
            for (const auto& sym : allSymbols) {
                m_results.push_back({&sym, 0});
                m_filteredSymbols.push_back(&sym);
            }
        } else {
            auto catSymbols = m_database->GetByCategory(m_categoryFilter);
            m_filteredSymbols = std::move(catSymbols);
            m_results.reserve(m_filteredSymbols.size());
            for (auto* sym : m_filteredSymbols) {
                m_results.push_back({sym, 0});
            }
        }
    } else {
        // Has query: use search engine
        if (m_searchEngine) {
            auto raw = m_searchEngine->Search(m_query);
            if (m_allCategories) {
                m_results = std::move(raw);
            } else {
                // Filter by category
                for (auto& r : raw) {
                    if (r.symbol && r.symbol->category == m_categoryFilter) {
                        m_results.push_back(r);
                    }
                }
            }
        }
    }
    // Clamp selection
    size_t total = m_query.empty() ? m_filteredSymbols.size() : m_results.size();
    if (total > 0 && m_selectedIndex >= total) {
        m_selectedIndex = total - 1;
    }
    if (total == 0) {
        m_selectedIndex = 0;
    }

    SSP_LOG_DEBUG("InputHandler::PerformSearch query='%ls' results=%zu filtered=%zu",
        m_query.c_str(), m_results.size(), m_filteredSymbols.size());
}

// ============================================================================
// Keyboard handling
// ============================================================================

bool InputHandler::HandleKeyDown(WPARAM vk, bool shift, bool ctrl,
                                  bool hasRecent, bool hasFavorites) {

    // --- Ctrl+digit: switch category ---
    if (ctrl && vk >= '0' && vk <= '9') {
        int idx = static_cast<int>(vk - '0');
        if (idx == 0) {
            m_allCategories = true;
        } else if (idx <= kVisibleCategories) {
            m_allCategories = false;
            m_categoryFilter = static_cast<Category>(idx - 1);
        }
        m_selectedIndex = 0;
        m_scrollOffset = 0.0f;
        PerformSearch();
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    // --- Ctrl+F: focus search bar ---
    if (ctrl && vk == 'F') {
        m_activeZone = Zone::SearchBar;
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    // --- Escape: close panel ---
    if (vk == VK_ESCAPE) {
        if (m_onClose) m_onClose();
        return true;
    }

    // --- Tab / Shift+Tab: cycle zones ---
    if (vk == VK_TAB) {
        CycleZone(!shift, hasRecent, hasFavorites);
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    // --- Zone-specific keys ---
    switch (m_activeZone) {

    case Zone::SearchBar: {
        // Ctrl+Backspace: delete word left
        if (vk == VK_BACK && ctrl && !m_query.empty()) {
            int pos = static_cast<int>(GetCursorPos());
            int start = pos;
            while (start > 0 && std::iswspace(m_query[start - 1])) start--;
            while (start > 0 && !std::iswspace(m_query[start - 1])) start--;
            m_query.erase(start, pos - start);
            TextEdit_Init(m_textEdit); TextEdit_SetString(m_textEdit, &m_query);
            m_selectedIndex = 0; m_scrollOffset = 0.0f;
            PerformSearch();
            if (m_onInvalidate) m_onInvalidate();
            return true;
        }
        // Ctrl+Delete: delete word right
        if (vk == VK_DELETE && ctrl && GetCursorPos() < static_cast<int>(m_query.size())) {
            int pos = static_cast<int>(GetCursorPos());
            int end = pos;
            while (end < static_cast<int>(m_query.size()) && !std::iswspace(m_query[end])) end++;
            while (end < static_cast<int>(m_query.size()) && std::iswspace(m_query[end])) end++;
            m_query.erase(pos, end - pos);
            TextEdit_Init(m_textEdit); TextEdit_SetString(m_textEdit, &m_query);
            m_selectedIndex = 0; m_scrollOffset = 0.0f;
            if (m_onInvalidate) m_onInvalidate();
            return true;
        }
        // Delegate to stb_textedit for text editing keys
        if (vk == VK_BACK || vk == VK_DELETE ||
            vk == VK_LEFT || vk == VK_RIGHT ||
            vk == VK_HOME || vk == VK_END) {
            TextEdit_Key(m_textEdit, (int)vk, ctrl, shift);
            m_selectedIndex = 0;
            m_scrollOffset = 0.0f;
            PerformSearch();
            if (m_onInvalidate) m_onInvalidate();
            return true;
        }
        if (vk == VK_RETURN) {
            ExecuteAction(hasRecent, hasFavorites);
            return true;
        }
        // Arrow keys: switch to results if there are any
        if (vk == VK_DOWN || vk == VK_UP) {
            size_t total = m_query.empty() ? m_filteredSymbols.size() : m_results.size();
            if (total > 0) {
                m_activeZone = Zone::ResultsGrid;
                m_selectedIndex = (vk == VK_DOWN) ? 0 : total - 1;
                if (m_onInvalidate) m_onInvalidate();
            }
            return true;
        }
        break;
    }

    case Zone::ResultsGrid: {
        size_t total = m_query.empty() ? m_filteredSymbols.size() : m_results.size();
        if (total == 0) break;

        int cols = m_gridColumns;

        switch (vk) {
        case VK_LEFT:
            if (m_selectedIndex > 0) m_selectedIndex--;
            else m_selectedIndex = total - 1;
            break;
        case VK_RIGHT:
            if (m_selectedIndex + 1 < total) m_selectedIndex++;
            else m_selectedIndex = 0;
            break;
        case VK_UP: {
            size_t row = m_selectedIndex / cols;
            if (row > 0) {
                m_selectedIndex = std::min(m_selectedIndex - cols, total - 1);
            } else {
                size_t col = m_selectedIndex % cols;
                size_t lastRow = (total - 1) / cols;
                size_t candidate = lastRow * cols + col;
                m_selectedIndex = std::min(candidate, total - 1);
            }
            break;
        }
        case VK_DOWN: {
            size_t candidate = m_selectedIndex + cols;
            if (candidate < total) {
                m_selectedIndex = candidate;
            } else {
                m_selectedIndex = m_selectedIndex % cols;
                if (m_selectedIndex >= total) m_selectedIndex = 0;
            }
            break;
        }
        case VK_RETURN:
            ExecuteAction(hasRecent, hasFavorites);
            return true;
        case VK_HOME:
            m_selectedIndex = 0;
            break;
        case VK_END:
            m_selectedIndex = total - 1;
            break;
        case VK_PRIOR: {
            size_t step = static_cast<size_t>(m_visibleRows) * cols;
            m_selectedIndex = (m_selectedIndex >= step) ? m_selectedIndex - step : 0;
            break;
        }
        case VK_NEXT: {
            size_t step = static_cast<size_t>(m_visibleRows) * cols;
            m_selectedIndex = std::min(m_selectedIndex + step, total - 1);
            break;
        }

        default:
            return false;
        }

        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    case Zone::CategoryBar: {
        int numCats = static_cast<int>(Category::COUNT);
        switch (vk) {
        case VK_LEFT:
            m_categoryFilter = static_cast<Category>(
                (static_cast<int>(m_categoryFilter) - 1 + numCats) % numCats);
            m_allCategories = false;
            m_selectedIndex = 0;
            m_scrollOffset = 0.0f;
            PerformSearch();
            if (m_onInvalidate) m_onInvalidate();
            return true;
        case VK_RIGHT:
            m_categoryFilter = static_cast<Category>(
                (static_cast<int>(m_categoryFilter) + 1) % numCats);
            m_allCategories = false;
            m_selectedIndex = 0;
            m_scrollOffset = 0.0f;
            PerformSearch();
            if (m_onInvalidate) m_onInvalidate();
            return true;
        case VK_RETURN:
            m_allCategories = false;
            m_selectedIndex = 0;
            m_scrollOffset = 0.0f;
            PerformSearch();
            m_activeZone = Zone::ResultsGrid;
            if (m_onInvalidate) m_onInvalidate();
            return true;

        default:
            break;
        }
        break;
    }

    case Zone::RecentStrip: {
        if (!m_recentManager) break;
        const auto& recents = m_recentManager->GetRecent();
        size_t n = recents.size();
        if (n == 0) break;

        switch (vk) {
        case VK_LEFT:
            m_selectedIndex = (m_selectedIndex > 0) ? m_selectedIndex - 1 : n - 1;
            if (m_onInvalidate) m_onInvalidate();
            return true;
        case VK_RIGHT:
            m_selectedIndex = (m_selectedIndex + 1 < n) ? m_selectedIndex + 1 : 0;
            if (m_onInvalidate) m_onInvalidate();
            return true;
        case VK_RETURN:
            if (m_selectedIndex < n && m_onInsert) {
                m_onInsert(recents[m_selectedIndex]->symbol);
            }
            return true;

        default:
            break;
        }
        break;
    }

    case Zone::FavoritesStrip: {
        if (!m_favoritesManager) break;
        const auto& favs = m_favoritesManager->GetFavorites();
        size_t n = favs.size();
        if (n == 0) break;

        switch (vk) {
        case VK_LEFT:
            m_selectedIndex = (m_selectedIndex > 0) ? m_selectedIndex - 1 : n - 1;
            if (m_onInvalidate) m_onInvalidate();
            return true;
        case VK_RIGHT:
            m_selectedIndex = (m_selectedIndex + 1 < n) ? m_selectedIndex + 1 : 0;
            if (m_onInvalidate) m_onInvalidate();
            return true;
        case VK_RETURN:
            if (m_selectedIndex < n && m_onInsert) {
                m_onInsert(favs[m_selectedIndex]->symbol);
            }
            return true;

        default:
            break;
        }
        break;
    }

    default:
        break;
    }

    return false;
}

// ============================================================================
// Character input
// ============================================================================

bool InputHandler::HandleChar(wchar_t ch) {
    if (ch < L' ' || ch == 127) return false;

    TextEdit_Char(m_textEdit, ch);
    m_activeZone = Zone::SearchBar;
    m_selectedIndex = 0;
    m_scrollOffset = 0.0f;
    PerformSearch();

    SSP_LOG_DEBUG("InputHandler::HandleChar '%lc' query='%ls'", ch, m_query.c_str());
    return true;
}

// ============================================================================
// Mouse handling
// ============================================================================

bool InputHandler::HandleMouseDown(int x, int y, const PanelLayout& layout,
                                    bool hasRecent, bool hasFavorites) {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    auto hit = [](const RectF& r, float px, float py) -> bool {
        return px >= r.x && px < r.x + r.width &&
               py >= r.y && py < r.y + r.height;
    };

    // 1. Search bar
    if (hit(layout.searchBar, fx, fy)) {
        m_activeZone = Zone::SearchBar;
        TextEdit_Click(m_textEdit, fx - layout.searchBar.x - 12.0f);
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    // 2. Category bar
    if (hit(layout.categoryBar, fx, fy)) {
        int numCats = static_cast<int>(Category::COUNT);
        int catIdx = HitTestCategory(fx - layout.categoryBar.x, layout.categoryBar, numCats);
        if (catIdx >= 0 && catIdx < numCats) {
            m_allCategories = false;
            m_categoryFilter = static_cast<Category>(catIdx);
            m_selectedIndex = 0;
            m_scrollOffset = 0.0f;
            PerformSearch();
            if (m_onInvalidate) m_onInvalidate();
        }
        return true;
    }

    // 3. Results grid
    if (hit(layout.resultsGrid, fx, fy)) {
        size_t total = m_query.empty() ? m_filteredSymbols.size() : m_results.size();
        if (total > 0 && layout.cellSize > 0 && layout.columns > 0) {
            int cellIdx = HitTestResultCell(
                fx - layout.resultsGrid.x,
                fy - layout.resultsGrid.y + m_scrollOffset,
                layout.resultsGrid, layout.cellSize, layout.columns);
            if (cellIdx >= 0 && static_cast<size_t>(cellIdx) < total) {
                m_selectedIndex = static_cast<size_t>(cellIdx);
                InsertSelectedResult();
                return true;
            }
        }
        m_activeZone = Zone::ResultsGrid;
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    // 4. Recent strip
    if (hasRecent && hit(layout.recentGrid, fx, fy)) {
        if (m_recentManager) {
            const auto& recents = m_recentManager->GetRecent();
            int n = static_cast<int>(recents.size());
            if (n > 0) {
                int idx = HitTestStripItem(fx - layout.recentGrid.x,
                    layout.recentGrid, kSymbolCellSize, n);
                if (idx >= 0 && idx < n && m_onInsert) {
                    m_onInsert(recents[idx]->symbol);
                    return true;
                }
            }
        }
        m_activeZone = Zone::RecentStrip;
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    // 5. Favorites strip
    if (hasFavorites && hit(layout.favoritesGrid, fx, fy)) {
        if (m_favoritesManager) {
            const auto& favs = m_favoritesManager->GetFavorites();
            int n = static_cast<int>(favs.size());
            if (n > 0) {
                int idx = HitTestStripItem(fx - layout.favoritesGrid.x,
                    layout.favoritesGrid, kSymbolCellSize, n);
                if (idx >= 0 && idx < n && m_onInsert) {
                    m_onInsert(favs[idx]->symbol);
                    return true;
                }
            }
        }
        m_activeZone = Zone::FavoritesStrip;
        if (m_onInvalidate) m_onInvalidate();
        return true;
    }

    return false;
}
bool InputHandler::HandleMouseWheel(int delta, int x, int y, const PanelLayout& layout) {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    auto hit = [](const RectF& r, float px, float py) -> bool {
        return px >= r.x && px < r.x + r.width &&
               py >= r.y && py < r.y + r.height;
    };

    // Only scroll if over results grid
    if (!hit(layout.resultsGrid, fx, fy)) return false;

    float scrollAmount = static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA) * kSymbolCellSize * 2.0f;
    ScrollBy(-scrollAmount);
    if (m_onInvalidate) m_onInvalidate();
    return true;
}
void InputHandler::HandleMouseMove(int x, int y, const PanelLayout& layout,
                                    bool hasRecent, bool hasFavorites) {
    float fx = static_cast<float>(x);
    float fy = static_cast<float>(y);

    auto hit = [](const RectF& r, float px, float py) -> bool {
        return px >= r.x && px < r.x + r.width &&
               py >= r.y && py < r.y + r.height;
    };

    m_hoverIndex = -1;

    // Check results grid hover
    if (hit(layout.resultsGrid, fx, fy)) {
        m_hoverZone = Zone::ResultsGrid;
        size_t total = m_query.empty() ? m_filteredSymbols.size() : m_results.size();
        if (total > 0 && layout.cellSize > 0 && layout.columns > 0) {
            int cellIdx = HitTestResultCell(
                fx - layout.resultsGrid.x,
                fy - layout.resultsGrid.y + m_scrollOffset,
                layout.resultsGrid, layout.cellSize, layout.columns);
            if (cellIdx >= 0 && static_cast<size_t>(cellIdx) < total) {
                m_hoverIndex = cellIdx;
            }
        }
        return;
    }

    // Check recent strip hover
    if (hasRecent && hit(layout.recentGrid, fx, fy)) {
        m_hoverZone = Zone::RecentStrip;
        if (m_recentManager) {
            int n = static_cast<int>(m_recentManager->GetRecent().size());
            if (n > 0) {
                int idx = HitTestStripItem(fx - layout.recentGrid.x,
                    layout.recentGrid, kSymbolCellSize, n);
                if (idx >= 0 && idx < n) m_hoverIndex = idx;
            }
        }
        return;
    }

    // Check favorites strip hover
    if (hasFavorites && hit(layout.favoritesGrid, fx, fy)) {
        m_hoverZone = Zone::FavoritesStrip;
        if (m_favoritesManager) {
            int n = static_cast<int>(m_favoritesManager->GetFavorites().size());
            if (n > 0) {
                int idx = HitTestStripItem(fx - layout.favoritesGrid.x,
                    layout.favoritesGrid, kSymbolCellSize, n);
                if (idx >= 0 && idx < n) m_hoverIndex = idx;
            }
        }
        return;
    }

    // Check search bar hover
    if (hit(layout.searchBar, fx, fy)) {
        m_hoverZone = Zone::SearchBar;
        return;
    }
}

// ============================================================================
// Navigation helpers
// ============================================================================

void InputHandler::CycleZone(bool forward, bool hasRecent, bool hasFavorites) {
    static constexpr Zone kOrder[] = {
        Zone::SearchBar, Zone::RecentStrip, Zone::FavoritesStrip,
        Zone::CategoryBar, Zone::ResultsGrid
    };
    static constexpr int kCount = static_cast<int>(std::size(kOrder));

    int cur = -1;
    for (int i = 0; i < kCount; i++) {
        if (kOrder[i] == m_activeZone) { cur = i; break; }
    }
    if (cur < 0) { m_activeZone = Zone::SearchBar; return; }

    int dir = forward ? 1 : -1;
    for (int attempt = 0; attempt < kCount; attempt++) {
        cur = (cur + dir + kCount) % kCount;
        Zone candidate = kOrder[cur];
        if (candidate == Zone::RecentStrip && !hasRecent) continue;
        if (candidate == Zone::FavoritesStrip && !hasFavorites) continue;
        m_activeZone = candidate;
        return;
    }
}

void InputHandler::ScrollBy(float delta) {
    m_scrollOffset += delta;
    if (m_scrollOffset < 0.0f) m_scrollOffset = 0.0f;
    if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;
}

void InputHandler::UpdateLayoutInfo(int gridColumns, int visibleRows, float maxScroll) {
    m_gridColumns = std::max(1, gridColumns);
    m_visibleRows = std::max(1, visibleRows);
    m_maxScroll   = std::max(0.0f, maxScroll);
    if (m_scrollOffset > m_maxScroll) m_scrollOffset = m_maxScroll;
}

// ============================================================================
// Actions
// ============================================================================

void InputHandler::ExecuteAction(bool hasRecent, bool hasFavorites) {
    (void)hasRecent; (void)hasFavorites;
    switch (m_activeZone) {
    case Zone::SearchBar: {
        if (!m_query.empty()) {
            std::wstring converted;

            converted = ScientificConverter::Convert(m_query);
            if (!converted.empty() && converted != m_query) {
                if (m_onInsert) m_onInsert(converted);
                return;
            }
            converted = LaTeXConverter::Convert(m_query);
            if (!converted.empty() && converted != m_query) {
                if (m_onInsert) m_onInsert(converted);
                return;
            }
            converted = SuperscriptBuilder::Convert(m_query);
            if (!converted.empty() && converted != m_query) {
                if (m_onInsert) m_onInsert(converted);
                return;
            }
            converted = SubscriptBuilder::Convert(m_query);
            if (!converted.empty() && converted != m_query) {
                if (m_onInsert) m_onInsert(converted);
                return;
            }
            converted = FractionBuilder::Convert(m_query);
            if (!converted.empty() && converted != m_query) {
                if (m_onInsert) m_onInsert(converted);
                return;
            }
            if (m_onInsert) m_onInsert(m_query);
            return;
        }
        break;
    }

    case Zone::ResultsGrid:
        InsertSelectedResult();
        break;

    case Zone::RecentStrip:
        if (m_recentManager) {
            const auto& recents = m_recentManager->GetRecent();
            if (m_selectedIndex < recents.size() && m_onInsert) {
                m_onInsert(recents[m_selectedIndex]->symbol);
            }
        }
        break;

    case Zone::FavoritesStrip:
        if (m_favoritesManager) {
            const auto& favs = m_favoritesManager->GetFavorites();
            if (m_selectedIndex < favs.size() && m_onInsert) {
                m_onInsert(favs[m_selectedIndex]->symbol);
            }
        }
        break;

    case Zone::CategoryBar:
        m_allCategories = false;
        m_selectedIndex = 0;
        m_scrollOffset = 0.0f;
        PerformSearch();
        m_activeZone = Zone::ResultsGrid;
        if (m_onInvalidate) m_onInvalidate();
        break;

    default:
        break;
    }
}

void InputHandler::InsertSelectedResult() {
    size_t total = m_query.empty() ? m_filteredSymbols.size() : m_results.size();
    if (total == 0) return;
    if (m_selectedIndex >= total) m_selectedIndex = total - 1;

    const Symbol* sym = nullptr;
    if (m_query.empty()) {
        sym = m_filteredSymbols[m_selectedIndex];
    } else {
        sym = m_results[m_selectedIndex].symbol;
    }
    if (sym && m_onInsert) {
        m_onInsert(sym->symbol);
    }
}

// ============================================================================
// Hit testing helpers
// ============================================================================

int InputHandler::HitTestCategory(float localX, const RectF& bar, int numCategories) {
    if (numCategories <= 0 || bar.width <= 0.0f) return -1;
    float catWidth = bar.width / static_cast<float>(numCategories);
    int idx = static_cast<int>(localX / catWidth);
    if (idx < 0 || idx >= numCategories) return -1;
    return idx;
}

int InputHandler::HitTestResultCell(float localX, float localY,
                                     const RectF& grid, float cellSize, int columns) {
    if (cellSize <= 0.0f || columns <= 0) return -1;
    int col = static_cast<int>(localX / cellSize);
    int row = static_cast<int>(localY / cellSize);
    if (col < 0 || col >= columns) return -1;
    if (row < 0) return -1;
    return row * columns + col;
}

int InputHandler::HitTestStripItem(float localX, const RectF& strip,
                                    float cellSize, int itemCount) {
    if (itemCount <= 0 || cellSize <= 0.0f) return -1;
    int idx = static_cast<int>(localX / cellSize);
    if (idx < 0 || idx >= itemCount) return -1;
    return idx;
}

} // namespace ssp
