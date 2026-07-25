#include "Storage/Favorites.h"
#include "Core/Config.h"
#include "Core/Log.h"
#include "Storage/JsonStore.h"
#include "Symbols/Database.h"
#include <algorithm>
#include <cstdio>

namespace ssp {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::wstring CodepointToHex(char32_t cp) {
    wchar_t buf[9];
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%04X", static_cast<unsigned>(cp));
    return buf;
}

static char32_t HexToCodepoint(const std::wstring& s) {
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(s.c_str(), &end, 16);
    if (end == s.c_str() || v > 0x10FFFF) return 0;
    return static_cast<char32_t>(v);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

FavoritesManager::FavoritesManager(SymbolDatabase* db, const Config& config)
    : m_db(db)
    , m_config(config)
{
}

void FavoritesManager::Add(const Symbol& sym) {
    // No-op if already favorited.
    auto it = std::find_if(m_favorites.begin(), m_favorites.end(),
        [&](const Symbol* s) { return s->codepoint == sym.codepoint; });
    if (it != m_favorites.end()) return;

    m_favorites.push_back(&sym);
}

void FavoritesManager::Remove(const Symbol& sym) {
    auto it = std::find_if(m_favorites.begin(), m_favorites.end(),
        [&](const Symbol* s) { return s->codepoint == sym.codepoint; });
    if (it != m_favorites.end()) {
        m_favorites.erase(it);
    }
}

bool FavoritesManager::IsFavorite(const Symbol& sym) const {
    return std::any_of(m_favorites.begin(), m_favorites.end(),
        [&](const Symbol* s) { return s->codepoint == sym.codepoint; });
}

void FavoritesManager::Clear() {
    m_favorites.clear();
}

bool FavoritesManager::Load() {
    JsonStore store(m_config.FavoritesPath());
    JsonValue root = store.Load();

    if (root.type() != JsonValue::Object) {
        SSP_LOG_DEBUG("FavoritesManager: no favorites data (root not an object), starting empty");
        m_favorites.clear();
        return false;
    }

    if (!root.Has(L"favorites")) {
        m_favorites.clear();
        return false;
    }

    const auto& arr = root[L"favorites"].AsArray();
    m_favorites.clear();
    m_favorites.reserve(arr.size());

    for (size_t i = 0; i < arr.size(); ++i) {
        std::wstring hex = arr[i].AsString();
        char32_t cp = HexToCodepoint(hex);
        if (cp == 0) continue;

        const Symbol* sym = m_db->FindByCodepoint(cp);
        if (sym) {
            m_favorites.push_back(sym);
        } else {
            SSP_LOG_DEBUG("FavoritesManager: codepoint %ls not found in database, skipping", hex.c_str());
        }
    }

    SSP_LOG_DEBUG("FavoritesManager: loaded %zu favorites", m_favorites.size());
    return true;
}

bool FavoritesManager::Save() const {
    JsonValue root;
    root.SetObject();
    root[L"favorites"].SetArray();

    for (const Symbol* sym : m_favorites) {
        root[L"favorites"].Push(JsonValue(CodepointToHex(sym->codepoint)));
    }

    JsonStore store(m_config.FavoritesPath());
    bool ok = store.Save(root);
    SSP_LOG_DEBUG("FavoritesManager: saved %zu favorites (%ls)",
        m_favorites.size(), ok ? L"ok" : L"failed");
    return ok;
}

} // namespace ssp
