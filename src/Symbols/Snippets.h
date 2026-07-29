#pragma once
#include "Core/Types.h"
#include <vector>
#include <string>
#include <filesystem>

namespace ssp {

// ============================================================================
// SnippetManager — manages built-in and user snippets
// ============================================================================
class SnippetManager {
public:
    SnippetManager();
    ~SnippetManager() = default;

    // Load from bundled + user data. Call once at startup.
    // bundledPath: path to data/snippets.json in app dir (bundled)
    // userDataDir: %APPDATA%/ScientificSymbolPanel (user overrides)
    bool Load(const std::filesystem::path& bundledPath, const std::filesystem::path& userDataDir);

    // Search snippets by name, aliases, or description.
    // Case-insensitive substring match.
    std::vector<Snippet> Search(const std::wstring& query) const;

    // Return all snippets (merged built-in + user).
    const std::vector<Snippet>& GetSnippets() const { return m_snippets; }

private:
    std::vector<Snippet> m_snippets;

    void LoadBuiltinDefaults();

    static std::vector<Snippet> LoadSnippetsFromJson(const std::filesystem::path& path);
    static void MergeSnippets(std::vector<Snippet>& base, const std::vector<Snippet>& user);

    static bool Matches(const std::wstring& haystack, const std::wstring& needle);
    static std::wstring ToLower(std::wstring s);
};

} // namespace ssp
