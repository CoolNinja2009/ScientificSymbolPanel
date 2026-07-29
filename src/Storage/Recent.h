#pragma once
#include "Core/Types.h"
#include <vector>

namespace ssp {

class SymbolDatabase;
class Config;

class RecentManager {
public:
    RecentManager(SymbolDatabase* db, const Config& config);
    ~RecentManager() = default;

    // Prevent copy — owns no external resources but semantically singleton
    RecentManager(const RecentManager&) = delete;
    RecentManager& operator=(const RecentManager&) = delete;

    // Add a symbol to recents (or move to front if already present).
    // Respects cap from config (maxRecent).
    void Add(const Symbol& sym);

    // Returns references into the database — valid for the database's lifetime.
    const std::vector<const Symbol*>& GetRecent() const { return m_recent; }

    // Clear the in-memory list (does not touch disk).
    void Clear();

    // Persist to / load from recent.json (path from Config).
    bool Load();
    bool Save() const;

private:
    SymbolDatabase* m_db;
    const Config& m_config;

    // Most-recently-used at index 0.
    std::vector<const Symbol*> m_recent;
};

} // namespace ssp
