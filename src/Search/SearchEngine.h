#pragma once
#include "Core/Types.h"
#include "Trie.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>

namespace ssp {

// Two-phase search engine for ScientificSymbolPanel.
// Phase 1 (primary): exact match on name/alias/keyword/latex via hash maps,
//   substring/word-boundary contains match, and category match.
// Phase 2 (secondary, only when Phase 1 returns empty): trie prefix + Levenshtein fuzzy.
//
// Build() must be called once (from App::Initialize) and completes in <1ms for 2000 symbols.
// Search() must return in <1ms.
class SearchEngine {
public:
    void Build(const std::vector<Symbol>& symbols);
    std::vector<SearchResult> Search(std::wstring_view query) const;
    std::vector<SearchResult> SearchFuzzy(std::wstring_view query) const;

private:
    // --- Helpers ---

    static std::wstring Normalize(std::wstring_view sv);
    static void SplitWords(std::wstring_view sv, std::vector<std::wstring>& out);

    // Convert score map to sorted, deduplicated result vector.
    static std::vector<SearchResult> FlattenScores(
        const std::unordered_map<const Symbol*, int32_t>& scoreMap);

    // --- Phase 1: primary search (exact, contains, category) ---

    // Exact matches: name (100), alias (90), keyword (80), latex (85).
    void SearchExact(std::wstring_view query,
                     std::unordered_map<const Symbol*, int32_t>& scores) const;

    // Contains: word-index lookup (50) plus substring scan of names/aliases (50).
    void SearchContains(std::wstring_view query,
                        std::unordered_map<const Symbol*, int32_t>& scores) const;

    // Category: query substring of a category name -> all symbols in that category (30).
    void SearchCategory(std::wstring_view query,
                        std::unordered_map<const Symbol*, int32_t>& scores) const;

    // --- Phase 2: fuzzy fallback ---

    // Trie prefix walk, score 60.
    std::vector<SearchResult> SearchByPrefix(std::wstring_view query) const;

    // Levenshtein edit distance on all symbol names/aliases, score 10–30.
    std::vector<SearchResult> SearchByLevenshtein(std::wstring_view query) const;

    static int Levenshtein(std::wstring_view a, std::wstring_view b, int maxDist);

    // --- Index data ---

    const std::vector<Symbol>* m_symbols = nullptr;

    // Exact-match maps: lowercase string -> vector of indices into m_symbols
    std::unordered_map<std::wstring, std::vector<size_t>> m_exactName;
    std::unordered_map<std::wstring, std::vector<size_t>> m_exactAlias;
    std::unordered_map<std::wstring, std::vector<size_t>> m_exactKeyword;
    std::unordered_map<std::wstring, std::vector<size_t>> m_exactLatex;

    // Word index: individual lowercase words from names/aliases -> symbol indices
    std::unordered_map<std::wstring, std::vector<size_t>> m_wordIndex;

    // Category index: lowercase category name -> symbol indices
    std::vector<std::wstring> m_categoryNames;               // parallel arrays
    std::vector<std::vector<size_t>> m_categoryIndices;

    Trie m_trie;
};

} // namespace ssp
