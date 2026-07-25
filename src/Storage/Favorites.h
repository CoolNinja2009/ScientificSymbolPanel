#pragma once
#include "Core/Types.h"
#include <vector>

namespace ssp {

class SymbolDatabase;
class Config;

class FavoritesManager {
public:
    FavoritesManager(SymbolDatabase* db, const Config& config);
    ~FavoritesManager() = default;

    // Prevent copy
    FavoritesManager(const FavoritesManager&) = delete;
    FavoritesManager& operator=(const FavoritesManager&) = delete;

    // Add a symbol to the end of favorites (no-op if already favorited).
    void Add(const Symbol& sym);

    // Remove by codepoint (no-op if not present).
    void Remove(const Symbol& sym);

    // Returns references into the database.
    const std::vector<const Symbol*>& GetFavorites() const { return m_favorites; }

    // Check if a symbol is currently favorited.
    bool IsFavorite(const Symbol& sym) const;

    // Clear the in-memory list (does not touch disk).
    void Clear();

    // Persist to / load from favorites.json (path from Config).
    bool Load();
    bool Save() const;

private:
    SymbolDatabase* m_db;
    const Config& m_config;

    // Maintains insertion order.
    std::vector<const Symbol*> m_favorites;
};

} // namespace ssp
