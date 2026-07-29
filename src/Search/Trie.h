#pragma once
#include <vector>
#include <unordered_map>
#include <string_view>
#include <cwctype>

namespace ssp {

// Prefix tree (trie) for fast prefix matching and fuzzy search fallback.
// Flat storage: all nodes in a single vector; root is at index 0.
// Case-insensitive: all chars normalized to lowercase on insert/search.
class Trie {
public:
    Trie() { m_nodes.emplace_back(); }

    // Insert a word and associate it with a symbol index.
    // Word is normalized to lowercase.
    void Insert(std::wstring_view word, size_t symbolIndex) {
        size_t nodeIdx = 0;
        for (wchar_t ch : word) {
            ch = Normalize(ch);
            auto& children = m_nodes[nodeIdx].children;
            auto it = children.find(ch);
            if (it != children.end()) {
                nodeIdx = it->second;
            } else {
                size_t newIdx = m_nodes.size();
                children[ch] = newIdx;
                m_nodes.emplace_back();
                nodeIdx = newIdx;
            }
        }
        m_nodes[nodeIdx].symbolIndices.push_back(symbolIndex);
    }

    // Return all symbol indices whose associated words start with prefix.
    // Prefix is normalized to lowercase. Returns empty if prefix not found.
    std::vector<size_t> SearchPrefix(std::wstring_view prefix) const {
        size_t nodeIdx = 0;
        for (wchar_t ch : prefix) {
            ch = Normalize(ch);
            const auto& children = m_nodes[nodeIdx].children;
            auto it = children.find(ch);
            if (it == children.end()) {
                return {};
            }
            nodeIdx = it->second;
        }
        return CollectSubtree(nodeIdx);
    }

private:
    struct Node {
        std::unordered_map<wchar_t, size_t> children;
        std::vector<size_t> symbolIndices;
    };

    std::vector<Node> m_nodes;

    static wchar_t Normalize(wchar_t c) {
        return std::towlower(c);
    }

    // Iterative DFS to collect all symbol indices from a subtree.
    std::vector<size_t> CollectSubtree(size_t rootIdx) const {
        std::vector<size_t> result;
        std::vector<size_t> stack;
        stack.reserve(64);
        stack.push_back(rootIdx);

        while (!stack.empty()) {
            size_t idx = stack.back();
            stack.pop_back();
            const auto& node = m_nodes[idx];
            result.insert(result.end(), node.symbolIndices.begin(), node.symbolIndices.end());
            for (const auto& [ch, childIdx] : node.children) {
                stack.push_back(childIdx);
            }
        }
        return result;
    }
};

} // namespace ssp
