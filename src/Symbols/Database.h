#pragma once
#include "Core/Types.h"
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace ssp {

class SymbolDatabase {
public:
    SymbolDatabase() = default;
    ~SymbolDatabase() = default;

    // Load from the default location (data/symbols.json relative to executable)
    // Returns true if any symbols were loaded; false on error or empty file.
    bool Load();

    // Query interface
    const std::vector<Symbol>& GetSymbols() const { return m_symbols; }
    std::vector<const Symbol*> GetByCategory(Category cat) const;
    const Symbol* FindByCodepoint(char32_t cp) const;
    size_t GetCount() const { return m_symbols.size(); }

private:
    void BuildIndices();
    static std::filesystem::path DefaultPath();
    bool LoadBinary(const std::vector<uint8_t>& data);
    bool LoadJson(const std::vector<uint8_t>& data);
    std::vector<Symbol> m_symbols;
    std::unordered_map<Category, std::vector<size_t>> m_categoryIndex;
    std::unordered_map<char32_t, size_t> m_codepointIndex;
};

} // namespace ssp
