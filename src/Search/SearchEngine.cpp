#include "SearchEngine.h"
#include "Core/Log.h"
#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace ssp {

// ============================================================================
// Helpers
// ============================================================================

std::wstring SearchEngine::Normalize(std::wstring_view sv) {
    std::wstring result;
    result.reserve(sv.size());
    for (wchar_t ch : sv) {
        result.push_back(static_cast<wchar_t>(std::towlower(ch)));
    }
    return result;
}

void SearchEngine::SplitWords(std::wstring_view sv, std::vector<std::wstring>& out) {
    std::wstring word;
    word.reserve(sv.size());
    for (wchar_t ch : sv) {
        if (std::iswspace(ch) || ch == L'-' || ch == L'_' ||
            ch == L',' || ch == L';' || ch == L'/' || ch == L'(' || ch == L')') {
            if (!word.empty()) {
                out.push_back(std::move(word));
                word.clear();
            }
        } else {
            word.push_back(static_cast<wchar_t>(std::towlower(ch)));
        }
    }
    if (!word.empty()) {
        out.push_back(std::move(word));
    }
}

std::vector<SearchResult> SearchEngine::FlattenScores(
    const std::unordered_map<const Symbol*, int32_t>& scoreMap) {
    std::vector<SearchResult> results;
    results.reserve(scoreMap.size());
    for (const auto& [sym, score] : scoreMap) {
        results.push_back({sym, score});
    }
    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.score > b.score;
        });
    return results;
}

// ============================================================================
// Build
// ============================================================================

void SearchEngine::Build(const std::vector<Symbol>& symbols) {
    m_symbols = &symbols;

    SSP_LOG_DEBUG("SearchEngine::Build - indexing %zu symbols", symbols.size());

    // Clear previous state
    m_exactName.clear();
    m_exactAlias.clear();
    m_exactKeyword.clear();
    m_exactLatex.clear();
    m_wordIndex.clear();
    m_categoryNames.clear();
    m_categoryIndices.clear();
    m_trie = Trie();

    // Reserve reasonable capacity
    m_exactName.reserve(symbols.size());
    m_exactAlias.reserve(symbols.size() * 2);
    m_exactKeyword.reserve(symbols.size() * 2);
    m_exactLatex.reserve(symbols.size());

    // Build category index once
    constexpr size_t catCount = static_cast<size_t>(Category::COUNT);
    m_categoryNames.resize(catCount);
    m_categoryIndices.resize(catCount);
    for (size_t c = 0; c < catCount; ++c) {
        // Convert narrow category name to wide
        const char* narrow = CategoryNames[c];
        std::wstring& wide = m_categoryNames[c];
        while (*narrow) {
            wide.push_back(static_cast<wchar_t>(*narrow++));
        }
        // Normalize to lowercase for matching
        for (auto& ch : wide) {
            ch = static_cast<wchar_t>(std::towlower(ch));
        }
    }

    // Index each symbol
    for (size_t i = 0; i < symbols.size(); ++i) {
        const auto& sym = symbols[i];

        // --- Name ---
        {
            std::wstring nameLower = Normalize(sym.name);
            m_exactName[nameLower].push_back(i);
            m_trie.Insert(nameLower, i);

            // Split name into words for contains/word matching
            std::vector<std::wstring> words;
            SplitWords(sym.name, words);
            for (auto& word : words) {
                m_wordIndex[std::move(word)].push_back(i);
            }
        }

        // --- Aliases ---
        for (const auto& alias : sym.aliases) {
            std::wstring aliasLower = Normalize(alias);
            m_exactAlias[aliasLower].push_back(i);
            m_trie.Insert(aliasLower, i);

            std::vector<std::wstring> words;
            SplitWords(alias, words);
            for (auto& word : words) {
                m_wordIndex[std::move(word)].push_back(i);
            }
        }

        // --- Keywords ---
        for (const auto& kw : sym.keywords) {
            std::wstring kwLower = Normalize(kw);
            m_exactKeyword[kwLower].push_back(i);
            m_trie.Insert(kwLower, i);
        }

        // --- LaTeX ---
        if (!sym.latex.empty()) {
            std::wstring latexLower = Normalize(sym.latex);
            m_exactLatex[latexLower].push_back(i);
            m_trie.Insert(latexLower, i);
        }

        // --- Category ---
        size_t catIdx = static_cast<size_t>(sym.category);
        if (catIdx < catCount) {
            m_categoryIndices[catIdx].push_back(i);
        }
    }

    SSP_LOG_DEBUG("SearchEngine::Build - complete");
}

// ============================================================================
// Phase 1: Exact
// ============================================================================

void SearchEngine::SearchExact(std::wstring_view query,
                               std::unordered_map<const Symbol*, int32_t>& scores) const {
    // We need a std::wstring key for map lookup (heterogeneous lookup not set up)
    std::wstring key(query);

    auto addResults = [&](const std::unordered_map<std::wstring, std::vector<size_t>>& map,
                          int32_t score) {
        auto it = map.find(key);
        if (it != map.end()) {
            for (size_t idx : it->second) {
                const Symbol* sym = &(*m_symbols)[idx];
                auto& s = scores[sym];
                if (score > s) s = score;
            }
        }
    };

    addResults(m_exactName, 100);
    addResults(m_exactAlias, 90);
    addResults(m_exactKeyword, 80);
    addResults(m_exactLatex, 85);
}

// ============================================================================
// Phase 1: Contains (word index + substring scan)
// ============================================================================

void SearchEngine::SearchContains(std::wstring_view query,
                                  std::unordered_map<const Symbol*, int32_t>& scores) const {
    std::wstring key(query);

    // 1. Word-index lookup: query matches a whole word from a name/alias
    {
        auto it = m_wordIndex.find(key);
        if (it != m_wordIndex.end()) {
            for (size_t idx : it->second) {
                const Symbol* sym = &(*m_symbols)[idx];
                auto& s = scores[sym];
                if (50 > s) s = 50;
            }
        }
    }

    // 2. Substring scan: query appears anywhere inside a name or alias.
    //    Only for queries ≥ 2 chars; single-char would match too broadly.
    if (query.size() >= 2) {
        for (size_t i = 0; i < m_symbols->size(); ++i) {
            const auto& sym = (*m_symbols)[i];

            auto contains = [&](const std::wstring& str) -> bool {
                // Case-insensitive substring search
                if (str.size() < query.size()) return false;
                auto it = std::search(
                    str.begin(), str.end(),
                    query.begin(), query.end(),
                    [](wchar_t a, wchar_t b) {
                        return std::towlower(a) == std::towlower(b);
                    });
                return it != str.end();
            };

            bool found = contains(sym.name);
            if (!found) {
                for (const auto& alias : sym.aliases) {
                    if (contains(alias)) { found = true; break; }
                }
            }

            if (found) {
                const Symbol* symPtr = &sym;
                auto& s = scores[symPtr];
                if (50 > s) s = 50;
            }
        }
    }
}

// ============================================================================
// Phase 1: Category
// ============================================================================

void SearchEngine::SearchCategory(std::wstring_view query,
                                  std::unordered_map<const Symbol*, int32_t>& scores) const {
    if (query.size() < 2) return; // too short to be a category name

    for (size_t c = 0; c < m_categoryNames.size(); ++c) {
        const auto& catName = m_categoryNames[c];
        // Check if query is a substring of the category name (e.g. "greek" in "greek letters")
        if (catName.find(query) != std::wstring::npos) {
            for (size_t idx : m_categoryIndices[c]) {
                const Symbol* sym = &(*m_symbols)[idx];
                auto& s = scores[sym];
                if (30 > s) s = 30;
            }
        }
    }
}

// ============================================================================
// Phase 2: Prefix (trie)
// ============================================================================

std::vector<SearchResult> SearchEngine::SearchByPrefix(std::wstring_view query) const {
    auto indices = m_trie.SearchPrefix(query);
    if (indices.empty()) return {};

    // Deduplicate: a symbol may appear multiple times in the trie
    std::unordered_set<size_t> seen;
    std::vector<SearchResult> results;
    results.reserve(indices.size());

    for (size_t idx : indices) {
        if (seen.insert(idx).second) {
            results.push_back({&(*m_symbols)[idx], 60});
        }
    }

    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.score > b.score;
        });
    return results;
}

// ============================================================================
// Phase 2: Levenshtein fuzzy
// ============================================================================

int SearchEngine::Levenshtein(std::wstring_view a, std::wstring_view b, int maxDist) {
    size_t n = a.size();
    size_t m = b.size();

    if (n == 0) return static_cast<int>(m);
    if (m == 0) return static_cast<int>(n);

    // Ensure a is the shorter string (space optimization)
    if (n > m) {
        std::swap(a, b);
        std::swap(n, m);
    }

    std::vector<int> prev(n + 1);
    std::vector<int> curr(n + 1);

    for (size_t i = 0; i <= n; ++i) {
        prev[i] = static_cast<int>(i);
    }

    for (size_t j = 1; j <= m; ++j) {
        curr[0] = static_cast<int>(j);
        int rowMin = curr[0];

        for (size_t i = 1; i <= n; ++i) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            curr[i] = std::min({
                prev[i] + 1,        // deletion
                curr[i - 1] + 1,    // insertion
                prev[i - 1] + cost  // substitution
            });
            if (curr[i] < rowMin) rowMin = curr[i];
        }

        // Early exit: entire row exceeds maxDist
        if (rowMin > maxDist) {
            return maxDist + 1;
        }

        std::swap(prev, curr);
    }

    return prev[n];
}

std::vector<SearchResult> SearchEngine::SearchByLevenshtein(std::wstring_view query) const {
    if (!m_symbols || query.size() < 2) return {};

    // Max Levenshtein distance we'll consider: ~1/3 of query length, min 2
    int maxDist = std::max(2, static_cast<int>(query.size()) / 3);

    std::vector<SearchResult> results;

    for (size_t i = 0; i < m_symbols->size(); ++i) {
        const auto& sym = (*m_symbols)[i];

        // Find best (minimum) distance against name and aliases
        int bestDist = maxDist + 1;

        // Compare against name
        {
            std::wstring target = Normalize(sym.name);
            int d = Levenshtein(query, target, maxDist);
            if (d < bestDist) bestDist = d;
        }

        // Compare against aliases
        for (const auto& alias : sym.aliases) {
            std::wstring target = Normalize(alias);
            int d = Levenshtein(query, target, maxDist);
            if (d < bestDist) bestDist = d;
            if (bestDist == 0) break; // can't beat perfect (shouldn't happen — exact match already tried)
        }

        if (bestDist <= maxDist) {
            // Score: 30 for distance 0, down to 10 for distance == maxDist
            int32_t score = 30 - (bestDist * 20) / maxDist;
            if (score < 10) score = 10;
            results.push_back({&sym, score});
        }
    }

    std::sort(results.begin(), results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.score > b.score;
        });
    return results;
}

// ============================================================================
// Public API
// ============================================================================

std::vector<SearchResult> SearchEngine::Search(std::wstring_view query) const {
    if (!m_symbols || query.empty()) return {};

    std::wstring normalizedQuery = Normalize(query);

    // Handle "latex:" prefix — search only in LaTeX field
    constexpr std::wstring_view latexPrefix = L"latex:";
    bool latexOnly = false;
    std::wstring_view effectiveQuery = normalizedQuery;

    if (normalizedQuery.size() > latexPrefix.size() &&
        std::wstring_view(normalizedQuery).substr(0, latexPrefix.size()) == latexPrefix) {
        latexOnly = true;
        effectiveQuery = std::wstring_view(normalizedQuery).substr(latexPrefix.size());
    }

    std::unordered_map<const Symbol*, int32_t> scoreMap;

    if (latexOnly) {
        // LaTeX-prefixed search: look up the remainder in the latex map
        std::wstring key(effectiveQuery);
        auto it = m_exactLatex.find(key);
        if (it != m_exactLatex.end()) {
            for (size_t idx : it->second) {
                const Symbol* sym = &(*m_symbols)[idx];
                scoreMap[sym] = 90;
            }
        }
    } else {
        // Phase 1: primary search
        SearchExact(effectiveQuery, scoreMap);
        SearchContains(effectiveQuery, scoreMap);
        SearchCategory(effectiveQuery, scoreMap);
    }

    if (!scoreMap.empty()) {
        return FlattenScores(scoreMap);
    }

    // Phase 2: fuzzy fallback
    // Try prefix first, then Levenshtein
    auto prefixResults = SearchByPrefix(effectiveQuery);
    if (!prefixResults.empty()) {
        return prefixResults;
    }

    return SearchByLevenshtein(effectiveQuery);
}

std::vector<SearchResult> SearchEngine::SearchFuzzy(std::wstring_view query) const {
    if (!m_symbols || query.empty()) return {};

    std::wstring normalizedQuery = Normalize(query);

    auto prefixResults = SearchByPrefix(normalizedQuery);
    if (!prefixResults.empty()) {
        return prefixResults;
    }

    return SearchByLevenshtein(normalizedQuery);
}

} // namespace ssp
