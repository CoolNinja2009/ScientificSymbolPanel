#include <windows.h>
#include "Snippets.h"
#include "Core/Log.h"
#include "Storage/JsonStore.h"
#include <algorithm>
#include <cwctype>

namespace ssp {

SnippetManager::SnippetManager() = default;

// ============================================================================
// Built-in defaults
// ============================================================================
void SnippetManager::LoadBuiltinDefaults() {
    m_snippets = {
        {L"Ohm's Law", L"V = IR", L"Voltage equals current times resistance", {L"ohm", L"resistance", L"voltage", L"current"}},
        {L"Euler's Identity", L"e^(i\u03C0) + 1 = 0", L"The most beautiful equation in mathematics", {L"euler", L"identity", L"e"}},
        {L"Quadratic Formula", L"x = (-b \u00B1 \u221A(b\u00B2 \u2212 4ac)) / 2a", L"Solution to ax\u00B2 + bx + c = 0", {L"quadratic", L"formula", L"roots", L"polynomial"}},
        {L"Ideal Gas Law", L"PV = nRT", L"Pressure, volume, moles, and temperature relationship", {L"gas", L"ideal", L"pressure", L"volume", L"temperature"}},
        {L"Pythagorean Theorem", L"a\u00B2 + b\u00B2 = c\u00B2", L"Right triangle hypotenuse relationship", {L"pythagorean", L"pythagoras", L"triangle", L"hypotenuse"}},
        {L"Einstein's Equation", L"E = mc\u00B2", L"Mass-energy equivalence", {L"einstein", L"relativity", L"energy", L"mass"}},
        {L"Newton's Second Law", L"F = ma", L"Force equals mass times acceleration", {L"newton", L"force", L"mass", L"acceleration", L"motion"}},
        {L"Kinetic Energy", L"KE = \u00BDmv\u00B2", L"Energy of motion", {L"kinetic", L"energy", L"velocity", L"speed"}},
        {L"Work-Energy", L"W = Fd cos(\u03B8)", L"Work equals force times displacement times cosine of angle", {L"work", L"energy", L"force", L"displacement", L"angle"}},
        {L"Coulomb's Law", L"F = k(q\u2081q\u2082)/r\u00B2", L"Electrostatic force between charges", {L"coulomb", L"charge", L"electrostatic", L"electric"}},
        {L"Power", L"P = IV", L"Power equals current times voltage", {L"power", L"current", L"voltage", L"watt"}},
        {L"Wave Equation", L"v = f\u03BB", L"Wave speed equals frequency times wavelength", {L"wave", L"frequency", L"wavelength", L"speed"}},
        {L"Hooke's Law", L"F = \u2212kx", L"Spring force proportional to displacement", {L"hooke", L"spring", L"elastic", L"displacement"}},
        {L"Bernoulli's Equation", L"P + \u00BD\u03C1v\u00B2 + \u03C1gh = constant", L"Energy conservation in fluid flow", {L"bernoulli", L"fluid", L"pressure", L"density", L"flow"}},
        {L"Faraday's Law", L"\u03B5 = \u2212N(d\u03A6/dt)", L"Induced EMF proportional to rate of flux change", {L"faraday", L"induction", L"emf", L"flux", L"magnetic"}},
    };
    SSP_LOG_DEBUG("SnippetManager: %zu built-in snippets loaded", m_snippets.size());
}

// ============================================================================
// Case-insensitive helpers
// ============================================================================
std::wstring SnippetManager::ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

bool SnippetManager::Matches(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    auto lower = ToLower(haystack);
    return lower.find(needle) != std::wstring::npos;
}

// ============================================================================
// JSON loading
// ============================================================================
std::vector<Snippet> SnippetManager::LoadSnippetsFromJson(const std::filesystem::path& path) {
    std::vector<Snippet> result;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        SSP_LOG_DEBUG("SnippetManager: no file at %ls", path.c_str());
        return result;
    }

    JsonStore store(path);
    JsonValue root = store.Load();
    if (root.type() != JsonValue::Object) {
        SSP_LOG_DEBUG("SnippetManager: invalid root in %ls", path.c_str());
        return result;
    }

    if (!root.Has(L"snippets")) {
        SSP_LOG_DEBUG("SnippetManager: missing 'snippets' key in %ls", path.c_str());
        return result;
    }

    const auto& arr = root[L"snippets"].AsArray();
    result.reserve(arr.size());

    for (size_t i = 0; i < arr.size(); i++) {
        const auto& obj = arr[i];
        if (obj.type() != JsonValue::Object) continue;

        Snippet snip;
        if (obj.Has(L"name"))  snip.name = obj[L"name"].AsString();
        if (obj.Has(L"text"))  snip.text = obj[L"text"].AsString();
        if (obj.Has(L"description")) snip.description = obj[L"description"].AsString();

        if (obj.Has(L"aliases")) {
            const auto& aliases = obj[L"aliases"].AsArray();
            snip.aliases.reserve(aliases.size());
            for (size_t j = 0; j < aliases.size(); j++) {
                snip.aliases.push_back(aliases[j].AsString());
            }
        }

        if (!snip.name.empty()) {
            result.push_back(std::move(snip));
        }
    }

    SSP_LOG_DEBUG("SnippetManager: loaded %zu snippets from %ls", result.size(), path.c_str());
    return result;
}

// ============================================================================
// Merge: user overrides built-in by name
// ============================================================================
void SnippetManager::MergeSnippets(std::vector<Snippet>& base, const std::vector<Snippet>& user) {
    for (const auto& userSnip : user) {
        auto it = std::find_if(base.begin(), base.end(),
            [&](const Snippet& b) { return b.name == userSnip.name; });

        if (it != base.end()) {
            // Override existing
            *it = userSnip;
            SSP_LOG_DEBUG("SnippetManager: user override '%ls'", userSnip.name.c_str());
        } else {
            // Append new
            base.push_back(userSnip);
            SSP_LOG_DEBUG("SnippetManager: user added '%ls'", userSnip.name.c_str());
        }
    }
}

// ============================================================================
// Load
// ============================================================================
bool SnippetManager::Load(const std::filesystem::path& bundledPath,
                           const std::filesystem::path& userDataDir) {
    // Start with built-in defaults
    LoadBuiltinDefaults();

    // Load bundled snippets.json (shipped with the app)
    auto bundled = LoadSnippetsFromJson(bundledPath);
    if (!bundled.empty()) {
        MergeSnippets(m_snippets, bundled);
    }

    // Load user snippets (from %APPDATA%)
    auto userPath = userDataDir / kSnippetsFile;
    auto user = LoadSnippetsFromJson(userPath);
    if (!user.empty()) {
        MergeSnippets(m_snippets, user);
    }

    SSP_LOG_DEBUG("SnippetManager: total %zu snippets after merge", m_snippets.size());
    return true;
}

// ============================================================================
// Search
// ============================================================================
std::vector<Snippet> SnippetManager::Search(const std::wstring& query) const {
    if (query.empty()) return m_snippets;

    auto needle = ToLower(query);
    std::vector<Snippet> results;

    for (const auto& snip : m_snippets) {
        if (Matches(snip.name, needle) || Matches(snip.description, needle)) {
            results.push_back(snip);
            continue;
        }
        // Check aliases
        for (const auto& alias : snip.aliases) {
            if (Matches(alias, needle)) {
                results.push_back(snip);
                break;
            }
        }
    }

    // Sort by match quality: exact name match first, then prefix match, then substring
    auto score = [&](const Snippet& s) -> int {
        auto lowerName = ToLower(s.name);
        if (lowerName == needle) return 3;
        if (lowerName.starts_with(needle)) return 2;
        return 1;
    };

    std::stable_sort(results.begin(), results.end(),
        [&](const Snippet& a, const Snippet& b) { return score(a) > score(b); });

    SSP_LOG_DEBUG("SnippetManager: search '%ls' found %zu results", query.c_str(), results.size());
    return results;
}

} // namespace ssp
