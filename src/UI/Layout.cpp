#include "UI/Layout.h"
#include "Core/Log.h"
#include <algorithm>
#include <cmath>

namespace ssp {

namespace {
    constexpr float kLabelHeight       = 18.0f;  // Height of section labels ("Recent", "Favorites")
    constexpr float kStatusBarHeight   = 24.0f;
    constexpr float kHorizontalPadding = 8.0f;
    constexpr float kVerticalSpacing   = 4.0f;
}

PanelLayout ComputeLayout(float panelWidth, float panelHeight, float dpiScale,
    bool hasRecent, bool hasFavorites, size_t resultCount)
{
    PanelLayout layout;
    layout.cellSize = kSymbolCellSize * dpiScale;

    const float searchBarH  = kSearchBarHeight * dpiScale;
    const float statusBarH  = kStatusBarHeight * dpiScale;
    const float labelH      = kLabelHeight * dpiScale;
    const float cellSz      = layout.cellSize;
    const float hPad        = kHorizontalPadding * dpiScale;
    const float vGap        = kVerticalSpacing * dpiScale;

    float y = 0.0f;

    // 1. Search bar — full width, fixed height at top
    layout.searchBar = RectF{0.0f, y, panelWidth, searchBarH};
    y += searchBarH + vGap;

    // 2. Recent strip — label + single row of cells
    if (hasRecent) {
        layout.recentLabel = RectF{hPad, y, panelWidth - 2.0f * hPad, labelH};
        y += labelH;
        layout.recentGrid = RectF{hPad, y, panelWidth - 2.0f * hPad, cellSz};
        y += cellSz + vGap;
    } else {
        layout.recentLabel = RectF{};
        layout.recentGrid  = RectF{};
    }

    // 3. Favorites strip — same layout as recent
    if (hasFavorites) {
        layout.favoritesLabel = RectF{hPad, y, panelWidth - 2.0f * hPad, labelH};
        y += labelH;
        layout.favoritesGrid = RectF{hPad, y, panelWidth - 2.0f * hPad, cellSz};
        y += cellSz + vGap;
    } else {
        layout.favoritesLabel = RectF{};
        layout.favoritesGrid  = RectF{};
    }


    // 5. Results grid — fills remaining space down to status bar
    layout.categoryBar = RectF{hPad, y, panelWidth - 2.0f * hPad, kCategoryBarHeight * dpiScale};
    y += layout.categoryBar.height + vGap;

    float remainingH = panelHeight - y - statusBarH - vGap;
    if (remainingH < 0.0f) remainingH = 0.0f;
    layout.resultsGrid = RectF{hPad, y, panelWidth - 2.0f * hPad, remainingH};
    y += remainingH + vGap;

    // 6. Status bar — bottom
    layout.statusBar = RectF{0.0f, y, panelWidth, statusBarH};

    // Preview bar — reserved, zeroed for now
    layout.previewBar = RectF{};

    // ----- Grid column / scroll calculations -----
    float gridWidth  = layout.resultsGrid.width;
    float gridHeight = layout.resultsGrid.height;

    if (cellSz > 0.0f && gridWidth > 0.0f) {
        layout.columns = std::max(1, static_cast<int32_t>(std::floor(gridWidth / cellSz)));
    } else {
        layout.columns = 1;
    }

    if (resultCount > 0 && layout.columns > 0) {
        size_t totalRows     = (resultCount + static_cast<size_t>(layout.columns) - 1)
                               / static_cast<size_t>(layout.columns);
        float totalGridHeight = static_cast<float>(totalRows) * cellSz;
        layout.maxScroll = std::max(0.0f, totalGridHeight - gridHeight);

        // scrollOffset stays at default 0 (caller updates on scroll)
        layout.scrollOffset = 0.0f;

        // Visible rows
        if (cellSz > 0.0f) {
            layout.rows = std::max(1, static_cast<int32_t>(std::ceil(gridHeight / cellSz)));
        } else {
            layout.rows = 1;
        }

        int32_t firstRow = static_cast<int32_t>(
            std::floor(layout.scrollOffset / cellSz));
        layout.visibleStart = static_cast<size_t>(firstRow) * static_cast<size_t>(layout.columns);
        layout.visibleEnd = std::min(resultCount,
            (static_cast<size_t>(firstRow) + static_cast<size_t>(layout.rows))
            * static_cast<size_t>(layout.columns));
    } else {
        // Zero results or degenerate grid
        layout.columns     = 1;
        layout.rows        = 0;
        layout.maxScroll   = 0.0f;
        layout.scrollOffset = 0.0f;
        layout.visibleStart = 0;
        layout.visibleEnd   = 0;
    }

    SSP_LOG_DEBUG("Layout: %.0fx%.0f panel, DPI %.2f, %zu results -> %dx%d grid, "
                  "scroll %.0f/%.0f, visible [%zu, %zu)",
                  panelWidth, panelHeight, dpiScale, resultCount,
                  layout.columns, layout.rows,
                  layout.scrollOffset, layout.maxScroll,
                  layout.visibleStart, layout.visibleEnd);

    return layout;
}

} // namespace ssp
