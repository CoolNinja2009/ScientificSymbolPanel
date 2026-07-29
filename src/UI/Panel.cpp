#ifdef _WIN32
#define NOMINMAX
#endif
#include "UI/Panel.h"
#include "Core/Log.h"
#include "Platform/Platform.h"
#include "Symbols/Database.h"
#include "Search/SearchEngine.h"
#include "Storage/Recent.h"
#include "Storage/Favorites.h"
#include "Symbols/Snippets.h"
#include "Symbols/Converters.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cstring>
#include <cwctype>

namespace ssp {

// ============================================================================
// Mouse nudge â€” forces ImGui to refresh its held-button state
// ============================================================================
#ifdef _WIN32
#include <windows.h>
static void NudgeMouse() {
    POINT pt;
    GetCursorPos(&pt);
    SetCursorPos(pt.x + 1, pt.y);
    SetCursorPos(pt.x, pt.y);
}
#else
static void NudgeMouse() {}
#endif

// ============================================================================
// UTF conversion helpers
// ============================================================================

static std::string WideToUtf8(std::wstring_view ws) {
    std::string result;
    result.reserve(ws.size() * 3);
    for (size_t i = 0; i < ws.size(); ) {
        char32_t cp = static_cast<char32_t>(ws[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < ws.size()) {
            char32_t lo = static_cast<char32_t>(ws[i+1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = ((cp - 0xD800) << 10) | (lo - 0xDC00);
                cp += 0x10000;
                i++;
            }
        }
        i++;
        if (cp < 0x80) { result += static_cast<char>(cp); }
        else if (cp < 0x800) { result += static_cast<char>(0xC0 | (cp >> 6)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { result += static_cast<char>(0xE0 | (cp >> 12)); result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { result += static_cast<char>(0xF0 | (cp >> 18)); result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    return result;
}

static std::wstring Utf8ToWide(const std::string& u8) {
    std::wstring result;
    result.reserve(u8.size());
    for (size_t i = 0; i < u8.size(); ) {
        char32_t cp;
        uint8_t b = static_cast<uint8_t>(u8[i]);
        if (b < 0x80) { cp = b; i += 1; }
        else if ((b & 0xE0) == 0xC0 && i + 1 < u8.size()) { cp = ((b & 0x1F) << 6) | (u8[i+1] & 0x3F); i += 2; }
        else if ((b & 0xF0) == 0xE0 && i + 2 < u8.size()) { cp = ((b & 0x0F) << 12) | ((u8[i+1] & 0x3F) << 6) | (u8[i+2] & 0x3F); i += 3; }
        else if ((b & 0xF8) == 0xF0 && i + 3 < u8.size()) { cp = ((b & 0x07) << 18) | ((u8[i+1] & 0x3F) << 12) | ((u8[i+2] & 0x3F) << 6) | (u8[i+3] & 0x3F); i += 4; }
        else { i++; continue; }
        if (cp <= 0xFFFF) { result += static_cast<wchar_t>(cp); }
        else { cp -= 0x10000; result += static_cast<wchar_t>(0xD800 | (cp >> 10)); result += static_cast<wchar_t>(0xDC00 | (cp & 0x3FF)); }
    }
    return result;
}

// ============================================================================
// Panel
// ============================================================================

Panel::Panel()  = default;
Panel::~Panel() { Shutdown(); }

bool Panel::Initialize(GLFWwindow* window) {
    m_window = window;
    m_config.Load();

    int fbw, fbh, ww, wh;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &ww, &wh);
    if (ww > 0) m_dpiScale = static_cast<float>(fbw) / static_cast<float>(ww);

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

void Panel::Shutdown() {
    if (m_recentManager) m_recentManager->Save();
    if (m_favoritesManager) m_favoritesManager->Save();
    m_config.Save();
}

void Panel::OnShow() {
    m_wantsHide = false;
    m_focusSearch = true;
    m_pendingInsert.clear();
    m_searchBuf[0] = '\0';
    m_selectedCategory = -1;
    m_selectedResult = 0;
    PerformSearch();
}

void Panel::OnHide() {
    m_recentManager->Save();
    m_favoritesManager->Save();
    m_config.Save();
}

bool Panel::WantsHide() const { return m_wantsHide; }
bool Panel::HasPendingInsert() const { return !m_pendingInsert.empty(); }

std::wstring Panel::TakePendingInsert() {
    std::wstring result = std::move(m_pendingInsert);
    m_pendingInsert.clear();
    return result;
}

// ============================================================================
// Search
// ============================================================================

void Panel::PerformSearch() {
    m_results.clear();
    m_filteredSymbols.clear();
    if (!m_database) return;

    std::wstring query = Utf8ToWide(m_searchBuf);
    const auto& allSymbols = m_database->GetSymbols();

    if (query.empty()) {
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
    } else if (m_searchEngine) {
        auto raw = m_searchEngine->Search(query);
        if (m_selectedCategory < 0) {
            m_results = std::move(raw);
        } else {
            Category cat = static_cast<Category>(m_selectedCategory);
            for (auto& r : raw)
                if (r.symbol && r.symbol->category == cat)
                    m_results.push_back(r);
        }
    }

    size_t total = query.empty() ? m_filteredSymbols.size() : m_results.size();
    if (total > 0 && static_cast<size_t>(m_selectedResult) >= total)
        m_selectedResult = static_cast<int>(total) - 1;
    if (total == 0) m_selectedResult = 0;
}

// ============================================================================
// Actions
// ============================================================================

void Panel::SelectSymbol(const Symbol* sym) {
    if (!sym) return;
    m_recentManager->Add(*sym);
    m_pendingInsert = sym->symbol;
    m_recentManager->Save();
    ImGui::ClearActiveID();
    NudgeMouse(); // Break any stale held state while focus briefly moves away.
}

// ============================================================================
// Style
// ============================================================================

static void SetupImGuiStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 10.0f;
    s.ChildRounding     = 7.0f;
    s.FrameRounding     = 7.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;
    s.WindowPadding     = ImVec2(12, 12);
    s.FramePadding      = ImVec2(10, 7);
    s.CellPadding       = ImVec2(4, 4);
    s.ItemSpacing       = ImVec2(7, 7);
    s.ItemInnerSpacing  = ImVec2(5, 5);
    s.ScrollbarSize     = 8.0f;
    s.GrabMinSize       = 16.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.FrameBorderSize   = 1.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = ImVec4(0.090f, 0.094f, 0.102f, 1.00f);
    c[ImGuiCol_ChildBg]          = ImVec4(0.090f, 0.094f, 0.102f, 1.00f);
    c[ImGuiCol_PopupBg]          = ImVec4(0.125f, 0.133f, 0.145f, 0.98f);
    c[ImGuiCol_Border]           = ImVec4(0.245f, 0.265f, 0.290f, 0.65f);
    c[ImGuiCol_BorderShadow]     = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg]          = ImVec4(0.125f, 0.133f, 0.145f, 1.00f);
    c[ImGuiCol_FrameBgHovered]   = ImVec4(0.165f, 0.176f, 0.192f, 1.00f);
    c[ImGuiCol_FrameBgActive]    = ImVec4(0.185f, 0.198f, 0.216f, 1.00f);
    c[ImGuiCol_ScrollbarBg]      = ImVec4(0.090f, 0.094f, 0.102f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]    = ImVec4(0.300f, 0.325f, 0.355f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.380f, 0.410f, 0.445f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.000f, 0.470f, 0.830f, 1.00f);
    c[ImGuiCol_Button]           = ImVec4(0.125f, 0.133f, 0.145f, 1.00f);
    c[ImGuiCol_ButtonHovered]    = ImVec4(0.165f, 0.176f, 0.192f, 1.00f);
    c[ImGuiCol_ButtonActive]     = ImVec4(0.000f, 0.470f, 0.830f, 1.00f);
    c[ImGuiCol_Text]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_TextDisabled]     = ImVec4(0.570f, 0.600f, 0.640f, 1.00f);
    c[ImGuiCol_TextSelectedBg]   = ImVec4(0.00f, 0.47f, 0.83f, 0.40f);
    c[ImGuiCol_Header]           = ImVec4(0.00f, 0.47f, 0.83f, 0.40f);
    c[ImGuiCol_HeaderHovered]    = ImVec4(0.00f, 0.47f, 0.83f, 0.60f);
    c[ImGuiCol_HeaderActive]     = ImVec4(0.00f, 0.47f, 0.83f, 0.80f);
    c[ImGuiCol_Separator]        = ImVec4(0.245f, 0.265f, 0.290f, 0.65f);
    c[ImGuiCol_Tab]              = ImVec4(0.125f, 0.133f, 0.145f, 1.00f);
    c[ImGuiCol_TabHovered]       = ImVec4(0.165f, 0.176f, 0.192f, 1.00f);
    c[ImGuiCol_TabActive]        = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
}

// ============================================================================
// Render
// ============================================================================

void Panel::Render() {
    if (!m_window) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (!m_styleInitialized) {
        SetupImGuiStyle();
        ImGui::GetStyle().ScaleAllSizes(m_dpiScale);
        m_styleInitialized = true;
    }

    int ww, wh;
    glfwGetWindowSize(m_window, &ww, &wh);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(ww), static_cast<float>(wh)));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##SSP", nullptr, flags);

    // Drag handle â€” uses raw delta for 1:1 tracking
    {
        float barH = std::max(8.0f, 10.0f * m_dpiScale);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 size(ImGui::GetContentRegionAvail().x, barH);
        ImGui::InvisibleButton("##dragbar", size);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 color = ImGui::IsItemHovered()
            ? IM_COL32(70, 78, 88, 255)
            : IM_COL32(46, 52, 60, 255);
        float w = std::min(64.0f * m_dpiScale, size.x * 0.28f);
        dl->AddRectFilled(ImVec2(pos.x + (size.x - w) * 0.5f, pos.y + size.y * 0.35f),
                          ImVec2(pos.x + (size.x + w) * 0.5f, pos.y + size.y * 0.65f),
                          color, 4.0f * m_dpiScale);
        if (ImGui::IsItemActivated()) {
            // Drag started â€” store initial window position
            glfwGetWindowPos(m_window, &m_dragStartX, &m_dragStartY);
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            glfwSetWindowPos(m_window,
                m_dragStartX + static_cast<int>(delta.x),
                m_dragStartY + static_cast<int>(delta.y));
        }
    }

    DrawSearchBar();
    ImGui::Spacing();

    bool hasRecent = m_recentManager && !m_recentManager->GetRecent().empty();
    bool hasFavorites = m_favoritesManager && !m_favoritesManager->GetFavorites().empty();

    if (hasRecent) { DrawRecentStrip(); ImGui::Spacing(); }
    if (hasFavorites) { DrawFavoritesStrip(); ImGui::Spacing(); }

    DrawCategoryBar();
    ImGui::Spacing();
    DrawResultsGrid();
    DrawStatusBar();
    HandleKeyboardShortcuts();

    ImGui::End();
    ImGui::Render();

    int fbw, fbh;
    glfwGetFramebufferSize(m_window, &fbw, &fbh);
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.118f, 0.118f, 0.118f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ============================================================================
// Search bar
// ============================================================================

void Panel::DrawSearchBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 9.0f * m_dpiScale);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
        ImVec2(12.0f * m_dpiScale, 8.0f * m_dpiScale));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.125f, 0.133f, 0.145f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.245f, 0.265f, 0.290f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    if (m_focusSearch) {
        ImGui::SetKeyboardFocusHere();
        m_focusSearch = false;
    }

    ImGui::PushItemWidth(-1);
    bool changed = ImGui::InputTextWithHint("##search", "Search symbols...",
        m_searchBuf, sizeof(m_searchBuf));
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);

    if (changed) {
        m_selectedResult = 0;
        PerformSearch();
    }
}

// ============================================================================
// Category bar
// ============================================================================

void Panel::DrawCategoryBar() {
    static const char* catLabels[] = {
        "All", "Math", "Greek", "Physics", "Chem", "Elect",
        "SI", "Logic", "Prog", "Arrows", "Currency", "Frac",
        "Super", "Sub", "Stats", "Geom", "Calc", "Astro", "Misc"
    };
    static const int numCats = sizeof(catLabels) / sizeof(catLabels[0]);

    float avail = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    int cols = avail < 360.0f * m_dpiScale ? 5 : 6;
    float btnW = std::max(42.0f * m_dpiScale,
        (avail - spacing * static_cast<float>(cols - 1)) / static_cast<float>(cols));

    for (int i = 0; i < numCats; i++) {
        if (i > 0 && i % cols != 0) ImGui::SameLine();

        bool isSel = (i == 0 && m_selectedCategory < 0) || (i > 0 && m_selectedCategory == i - 1);
        if (isSel) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.47f, 0.83f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.57f, 0.93f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.40f, 0.72f, 1.0f));
        }

        if (ImGui::Button(catLabels[i], ImVec2(btnW, 0))) {
            m_selectedCategory = (i == 0) ? -1 : i - 1;
            m_selectedResult = 0;
            PerformSearch();
        }

        if (isSel) ImGui::PopStyleColor(3);
    }
}

// ============================================================================
// Results grid â€” raw hit testing, no sticky buttons
// ============================================================================

bool Panel::DrawSymbolCell(const Symbol* sym, int id, float cellSize, bool selected) {
    if (!sym) return false;

    ImGui::PushID(id);
    ImVec2 cellPos = ImGui::GetCursorScreenPos();
    ImVec2 cellSz(cellSize - 4.0f * m_dpiScale, cellSize - 4.0f * m_dpiScale);
    ImRect bb(cellPos, ImVec2(cellPos.x + cellSz.x, cellPos.y + cellSz.y));

    ImGui::ItemSize(cellSz);
    bool added = ImGui::ItemAdd(bb, ImGui::GetID("##cell"));
    bool hovered = added && ImGui::IsMouseHoveringRect(bb.Min, bb.Max);
    bool clicked = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 bg = IM_COL32(0, 0, 0, 0);
    ImU32 border = IM_COL32(52, 58, 66, 135);
    if (selected) {
        bg = IM_COL32(0, 120, 212, 118);
        border = IM_COL32(0, 142, 245, 210);
    } else if (hovered) {
        bg = IM_COL32(42, 48, 56, 255);
        border = IM_COL32(86, 96, 108, 205);
    }

    dl->AddRectFilled(bb.Min, bb.Max, bg, 7.0f * m_dpiScale);
    dl->AddRect(bb.Min, bb.Max, border, 7.0f * m_dpiScale, 0, 1.0f * m_dpiScale);

    std::string label = WideToUtf8(sym->symbol);
    ImVec2 ts = ImGui::CalcTextSize(label.c_str());
    dl->AddText(ImVec2(cellPos.x + (cellSz.x - ts.x) * 0.5f,
                       cellPos.y + (cellSz.y - ts.y) * 0.5f - 1.0f * m_dpiScale),
                IM_COL32(247, 249, 252, 255), label.c_str());

    if (hovered) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(WideToUtf8(sym->name).c_str());
        if (!sym->description.empty())
            ImGui::TextDisabled("%s", WideToUtf8(sym->description).c_str());
        ImGui::EndTooltip();
    }

    ImGui::PopID();
    return clicked;
}

void Panel::DrawResultsGrid() {
    size_t total = m_searchBuf[0] ? m_results.size() : m_filteredSymbols.size();
    if (total == 0) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##results-empty", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);
        ImVec2 textSize = ImGui::CalcTextSize("No matching symbols");
        ImGui::SetCursorPos(ImVec2(std::max(0.0f, (avail.x - textSize.x) * 0.5f),
                                   std::max(0.0f, (avail.y - textSize.y) * 0.35f)));
        ImGui::TextDisabled("No matching symbols");
        ImGui::EndChild();
        return;
    }

    float cellSize = kSymbolCellSize * m_dpiScale;
    float availW = ImGui::GetContentRegionAvail().x;
    float footerH = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    int cols = std::max(1, static_cast<int>((availW + ImGui::GetStyle().ItemSpacing.x) / cellSize));
    m_gridColumns = cols;

    ImGui::BeginChild("##results", ImVec2(0, -footerH), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    int rows = static_cast<int>((total + cols - 1) / cols);
    ImGuiListClipper clipper;
    clipper.Begin(rows, cellSize);

    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
            float yStart = ImGui::GetCursorPosY();

            for (int col = 0; col < cols; col++) {
                size_t idx = static_cast<size_t>(row * cols + col);
                if (idx >= total) break;

                const Symbol* sym = m_searchBuf[0]
                    ? (idx < m_results.size() ? m_results[idx].symbol : nullptr)
                    : (idx < m_filteredSymbols.size() ? m_filteredSymbols[idx] : nullptr);
                if (!sym) continue;

                if (col > 0) ImGui::SameLine();

                bool isSelected = static_cast<int>(idx) == m_selectedResult;
                if (DrawSymbolCell(sym, static_cast<int>(idx), cellSize, isSelected)) {
                    m_selectedResult = static_cast<int>(idx);
                    SelectSymbol(sym);
                }
                if (ImGui::IsItemHovered()) {
                    m_hoveredResult = static_cast<int>(idx);
                }
            }

            ImGui::Dummy(ImVec2(1, std::max(0.0f, cellSize - ImGui::GetCursorPosY() + yStart)));
        }
    }

    ImGui::EndChild();
}

// ============================================================================
// Recent strip
// ============================================================================

void Panel::DrawRecentStrip() {
    const auto& recents = m_recentManager->GetRecent();
    if (recents.empty()) return;

    ImGui::TextDisabled("Recent");
    float cellSize = kSymbolCellSize * m_dpiScale;
    float availW = ImGui::GetContentRegionAvail().x;
    int maxVisible = std::max(1, static_cast<int>(availW / cellSize));

    ImGui::BeginChild("##recent", ImVec2(0, cellSize + 8), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    size_t count = std::min(recents.size(), static_cast<size_t>(maxVisible));
    for (size_t i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine();
        if (DrawSymbolCell(recents[i], static_cast<int>(i + 10000), cellSize, false))
            SelectSymbol(recents[i]);
    }
    ImGui::EndChild();
}

// ============================================================================
void Panel::DrawFavoritesStrip() {
    const auto& favs = m_favoritesManager->GetFavorites();
    if (favs.empty()) return;

    ImGui::TextDisabled("Favorites");
    float cellSize = kSymbolCellSize * m_dpiScale;
    float availW = ImGui::GetContentRegionAvail().x;
    int maxVisible = std::max(1, static_cast<int>(availW / cellSize));

    ImGui::BeginChild("##favorites", ImVec2(0, cellSize + 8), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    size_t count = std::min(favs.size(), static_cast<size_t>(maxVisible));
    for (size_t i = 0; i < count; i++) {
        if (i > 0) ImGui::SameLine();
        if (DrawSymbolCell(favs[i], static_cast<int>(i + 20000), cellSize, false))
            SelectSymbol(favs[i]);
    }
    ImGui::EndChild();
}

// ============================================================================
// Status bar
// ============================================================================

void Panel::DrawStatusBar() {
    ImGui::Separator();
    size_t total = m_searchBuf[0] ? m_results.size() : m_filteredSymbols.size();
    ImGui::Text("%zu symbol%s", total, total == 1 ? "" : "s");
    const char* version = "v1.0.0";
    float versionW = ImGui::CalcTextSize(version).x;
    float rightEdge = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x,
                             rightEdge - versionW));
    ImGui::TextDisabled("%s", version);
}

// ============================================================================
// Keyboard shortcuts
// ============================================================================

void Panel::HandleKeyboardShortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;

    size_t total = m_searchBuf[0] ? m_results.size() : m_filteredSymbols.size();

    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        m_wantsHide = true;
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) && total > 0) {
        const Symbol* sym = nullptr;
        if (m_searchBuf[0]) {
            if (static_cast<size_t>(m_selectedResult) < m_results.size())
                sym = m_results[m_selectedResult].symbol;
        } else {
            if (static_cast<size_t>(m_selectedResult) < m_filteredSymbols.size())
                sym = m_filteredSymbols[m_selectedResult];
        }
        if (sym) SelectSymbol(sym);
    }

    if (total == 0) return;
    int cols = std::max(1, m_gridColumns);

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true))
        m_selectedResult = (m_selectedResult > 0) ? m_selectedResult - 1 : static_cast<int>(total) - 1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true))
        m_selectedResult = (static_cast<size_t>(m_selectedResult + 1) < total) ? m_selectedResult + 1 : 0;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
        int row = m_selectedResult / cols;
        if (row > 0) m_selectedResult = std::min(m_selectedResult - cols, static_cast<int>(total) - 1);
        else { int c = m_selectedResult % cols; m_selectedResult = std::min(((static_cast<int>(total) - 1) / cols) * cols + c, static_cast<int>(total) - 1); }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
        int next = m_selectedResult + cols;
        m_selectedResult = (static_cast<size_t>(next) < total) ? next : m_selectedResult % cols;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true)) {
        int visRows = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().y / (kSymbolCellSize * m_dpiScale)) - 2);
        m_selectedResult = std::max(0, m_selectedResult - visRows * cols);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true)) {
        int visRows = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().y / (kSymbolCellSize * m_dpiScale)) - 2);
        m_selectedResult = std::min(static_cast<int>(total) - 1, m_selectedResult + visRows * cols);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Home, true)) m_selectedResult = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_End, true)) m_selectedResult = static_cast<int>(total) - 1;
}

} // namespace ssp
