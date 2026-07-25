#include "Storage/Recent.h"
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

RecentManager::RecentManager(SymbolDatabase* db, const Config& config)
    : m_db(db)
    , m_config(config)
{
}

void RecentManager::Add(const Symbol& sym) {
    // Remove existing entry if present (by codepoint).
    auto it = std::find_if(m_recent.begin(), m_recent.end(),
        [&](const Symbol* s) { return s->codepoint == sym.codepoint; });
    if (it != m_recent.end()) {
        m_recent.erase(it);
    }

    // Insert at front (most recent).
    m_recent.insert(m_recent.begin(), &sym);

    // Trim to cap.
    int32_t cap = m_config.Get().maxRecent;
    if (cap < 1) cap = 100;
    while (static_cast<int32_t>(m_recent.size()) > cap) {
        m_recent.pop_back();
    }
}

void RecentManager::Clear() {
    m_recent.clear();
}

bool RecentManager::Load() {
    JsonStore store(m_config.RecentPath());
    JsonValue root = store.Load();

    if (root.type() != JsonValue::Object) {
        SSP_LOG_DEBUG("RecentManager: no recent data (root not an object), starting empty");
        m_recent.clear();
        return false;
    }

    if (!root.Has(L"recent")) {
        m_recent.clear();
        return false;
    }

    const auto& arr = root[L"recent"].AsArray();
    m_recent.clear();
    m_recent.reserve(arr.size());

    for (size_t i = 0; i < arr.size(); ++i) {
        std::wstring hex = arr[i].AsString();
        char32_t cp = HexToCodepoint(hex);
        if (cp == 0) continue;

        const Symbol* sym = m_db->FindByCodepoint(cp);
        if (sym) {
            m_recent.push_back(sym);
        } else {
            SSP_LOG_DEBUG("RecentManager: codepoint %ls not found in database, skipping", hex.c_str());
        }
    }

    SSP_LOG_DEBUG("RecentManager: loaded %zu recent symbols", m_recent.size());
    return true;
}

bool RecentManager::Save() const {
    JsonValue root;
    root.SetObject();
    root[L"recent"].SetArray();

    for (const Symbol* sym : m_recent) {
        root[L"recent"].Push(JsonValue(CodepointToHex(sym->codepoint)));
    }

    JsonStore store(m_config.RecentPath());
    bool ok = store.Save(root);
    SSP_LOG_DEBUG("RecentManager: saved %zu recent symbols (%ls)",
        m_recent.size(), ok ? L"ok" : L"failed");
    return ok;
}

} // namespace ssp
