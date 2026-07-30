#pragma once
#include "Core/Types.h"
#include <cstddef>

namespace ssp {

struct PanelLayout {
    // Computed rectangles for all UI regions
    RectF searchBar;
    RectF recentLabel;
    RectF recentGrid;       // Horizontal strip of recent symbols
    RectF favoritesLabel;
    RectF favoritesGrid;
    RectF categoryBar;
    RectF resultsGrid;
    RectF statusBar;
    RectF previewBar;       // Optional preview area

    // Results grid cells
    int32_t columns = 0;
    int32_t rows = 0;
    float cellSize = kSymbolCellSize;

    // Scroll offset
    float scrollOffset = 0;
    float maxScroll = 0;

    // Visible range in results
    size_t visibleStart = 0;
    size_t visibleEnd = 0;
};

PanelLayout ComputeLayout(float panelWidth, float panelHeight, float dpiScale,
    bool hasRecent, bool hasFavorites, size_t resultCount);

} // namespace ssp
