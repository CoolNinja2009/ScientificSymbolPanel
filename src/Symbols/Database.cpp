#include <windows.h>
#include "Database.h"
#include "Core/Log.h"
#include <fstream>
#include <cstring>

namespace ssp {

// ============================================================================
// UTF-8 conversion helpers
// ============================================================================

static std::wstring Utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), len);
    return result;
}

// ============================================================================
// Category name lookup
// ============================================================================

static Category CategoryFromName(std::string_view name) {
    for (size_t i = 0; i < static_cast<size_t>(Category::COUNT); ++i) {
        if (name == CategoryNames[i]) {
            return static_cast<Category>(i);
        }
    }
    return Category::Miscellaneous;
}

// ============================================================================
// Minimal streaming JSON reader — handles UTF-8 symbol array format
// ============================================================================

class JsonReader {
public:
    explicit JsonReader(std::string_view src)
        : m_cur(src.data()), m_end(src.data() + src.size()) {}

    bool ParseSymbols(std::vector<Symbol>& out) {
        SkipBOM();
        SkipWS();
        if (!Expect('[')) {
            SSP_LOG_DEBUG("SymbolDatabase: expected '[' at start of JSON");
            return false;
        }

        while (true) {
            SkipWS();
            if (AtEnd()) break;
            if (Peek() == ']') { m_cur++; break; }

            Symbol sym;
            if (ParseSymbolObject(sym)) {
                out.push_back(std::move(sym));
            }

            SkipWS();
            if (Peek() == ',') { m_cur++; continue; }
            if (Peek() == ']') { m_cur++; break; }
            SSP_LOG_DEBUG("SymbolDatabase: unexpected char '%c' in array, stopping", Peek());
            break;
        }

        return !out.empty();
    }

private:
    const char* m_cur;
    const char* m_end;

    bool AtEnd() const { return m_cur >= m_end; }
    char Peek() const { return AtEnd() ? '\0' : *m_cur; }

    void SkipBOM() {
        if (m_end - m_cur >= 3 &&
            static_cast<unsigned char>(m_cur[0]) == 0xEF &&
            static_cast<unsigned char>(m_cur[1]) == 0xBB &&
            static_cast<unsigned char>(m_cur[2]) == 0xBF) {
            m_cur += 3;
        }
    }

    void SkipWS() {
        while (m_cur < m_end) {
            char c = *m_cur;
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') m_cur++;
            else break;
        }
    }

    bool Expect(char c) {
        SkipWS();
        if (Peek() == c) { m_cur++; return true; }
        return false;
    }

    // Read a JSON string, return raw UTF-8 bytes (unescaped).
    // Returns empty string on parse failure.
    std::string ReadString() {
        if (!Expect('"')) return {};

        std::string out;
        out.reserve(64);

        while (m_cur < m_end) {
            char c = *m_cur++;
            if (c == '"') return out;
            if (c == '\\' && m_cur < m_end) {
                char esc = *m_cur++;
                switch (esc) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case 'u': {
                    uint32_t cp = 0;
                    for (int i = 0; i < 4 && m_cur < m_end; i++) {
                        char h = *m_cur;
                        cp <<= 4;
                        if      (h >= '0' && h <= '9') cp |= (h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                        else break;
                        m_cur++;
                    }
                    if (cp <= 0x7F) {
                        out += static_cast<char>(cp);
                    } else if (cp <= 0x7FF) {
                        out += static_cast<char>(0xC0 | (cp >> 6));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (cp >> 12));
                        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (cp & 0x3F));
                    }
                    break;
                }
                default: out += esc; break;
                }
            } else {
                out += c;
            }
        }
        return out; // unterminated — return what we have
    }

    int64_t ReadInt() {
        SkipWS();
        int64_t sign = 1;
        if (Peek() == '-') { sign = -1; m_cur++; }
        if (m_cur >= m_end || (*m_cur < '0' || *m_cur > '9')) return 0;
        int64_t val = 0;
        while (m_cur < m_end && *m_cur >= '0' && *m_cur <= '9') {
            val = val * 10 + (*m_cur - '0');
            m_cur++;
        }
        return val * sign;
    }

    std::vector<std::string> ReadStringArray() {
        std::vector<std::string> result;
        if (!Expect('[')) return result;
        while (true) {
            SkipWS();
            if (Peek() == ']') { m_cur++; break; }
            if (Peek() == '"') {
                result.push_back(ReadString());
            } else {
                break; // malformed — expected string
            }
            SkipWS();
            if (Peek() == ',') { m_cur++; continue; }
            if (Peek() == ']') { m_cur++; break; }
            break;
        }
        return result;
    }

    // Skip any JSON value, handling nesting
    void SkipValue() {
        SkipWS();
        if (AtEnd()) return;
        char c = Peek();
        if (c == '"') {
            m_cur++;
            bool esc = false;
            while (m_cur < m_end) {
                char ch = *m_cur++;
                if (esc) { esc = false; continue; }
                if (ch == '\\') { esc = true; continue; }
                if (ch == '"') return;
            }
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            if (c == '-') m_cur++;
            while (m_cur < m_end && *m_cur >= '0' && *m_cur <= '9') m_cur++;
        } else if (c == 't') { m_cur += 4; }
        else if (c == 'f') { m_cur += 5; }
        else if (c == 'n') { m_cur += 4; }
        else if (c == '{') { SkipBraceBlock('{', '}'); }
        else if (c == '[') { SkipBraceBlock('[', ']'); }
    }

    void SkipBraceBlock(char open, char close) {
        m_cur++; // past open
        int depth = 1;
        while (m_cur < m_end && depth > 0) {
            char ch = *m_cur++;
            if (ch == open) depth++;
            else if (ch == close) depth--;
            else if (ch == '"') {
                bool esc = false;
                while (m_cur < m_end) {
                    char s = *m_cur++;
                    if (esc) { esc = false; continue; }
                    if (s == '\\') { esc = true; continue; }
                    if (s == '"') break;
                }
            }
        }
    }

    bool ParseSymbolObject(Symbol& sym) {
        if (!Expect('{')) return false;

        while (true) {
            SkipWS();
            if (Peek() == '}') { m_cur++; return true; }

            // Read key
            auto keyUtf8 = ReadString();
            if (keyUtf8.empty() && Peek() != ':') {
                SSP_LOG_DEBUG("SymbolDatabase: expected string key in object");
                return false;
            }

            if (!Expect(':')) {
                SSP_LOG_DEBUG("SymbolDatabase: expected ':' after key");
                return false;
            }

            // Dispatch on key
            if (keyUtf8 == "symbol") {
                sym.symbol = Utf8ToWide(ReadString());
            } else if (keyUtf8 == "codepoint") {
                sym.codepoint = static_cast<char32_t>(ReadInt());
            } else if (keyUtf8 == "name") {
                sym.name = Utf8ToWide(ReadString());
            } else if (keyUtf8 == "aliases") {
                auto arr = ReadStringArray();
                sym.aliases.reserve(arr.size());
                for (auto& s : arr) sym.aliases.push_back(Utf8ToWide(s));
            } else if (keyUtf8 == "keywords") {
                auto arr = ReadStringArray();
                sym.keywords.reserve(arr.size());
                for (auto& s : arr) sym.keywords.push_back(Utf8ToWide(s));
            } else if (keyUtf8 == "category") {
                sym.category = CategoryFromName(ReadString());
            } else if (keyUtf8 == "latex") {
                sym.latex = Utf8ToWide(ReadString());
            } else if (keyUtf8 == "htmlEntity") {
                sym.htmlEntity = Utf8ToWide(ReadString());
            } else if (keyUtf8 == "description") {
                sym.description = Utf8ToWide(ReadString());
            } else {
                // Unknown key: skip its value
                SkipValue();
            }

            SkipWS();
            if (Peek() == ',') { m_cur++; continue; }
            if (Peek() == '}') { m_cur++; return true; }
            SSP_LOG_DEBUG("SymbolDatabase: unexpected char '%c' in object", Peek());
            return false;
        }
    }
};

// ============================================================================
// SymbolDatabase implementation
// ============================================================================

std::filesystem::path SymbolDatabase::DefaultPath() {
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::vector<std::filesystem::path> candidates;

    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path exePath(buf);
        // 1. Next to the exe: build/Release/data/symbols.json
        candidates.push_back(exePath.parent_path() / L"data" / L"symbols.json");
        // 2. Two levels up from exe (project root): data/symbols.json
        candidates.push_back(exePath.parent_path().parent_path().parent_path() / L"data" / L"symbols.json");
        // 3. Parent of exe parent (for out-of-source builds): ../data/symbols.json
        candidates.push_back(exePath.parent_path().parent_path() / L"data" / L"symbols.json");
    }
    // 4. Current working directory
    candidates.push_back(std::filesystem::current_path() / L"data" / L"symbols.json");
    // 5. Raw relative path fallback
    candidates.push_back(L"data\\symbols.json");

    for (auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            SSP_LOG_DEBUG("SymbolDatabase: found at %ls", p.c_str());
            return p;
        }
    }

    SSP_LOG_DEBUG("SymbolDatabase: symbols.json not found in any candidate path");
    return candidates.empty() ? L"data\\symbols.json" : candidates[0];
}

bool SymbolDatabase::Load() {
    auto path = DefaultPath();
    SSP_LOG_DEBUG("SymbolDatabase: loading from %ls", path.c_str());

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SSP_LOG_DEBUG("SymbolDatabase: failed to open %ls", path.c_str());
        return false;
    }

    auto size = file.tellg();
    if (size <= 0) {
        SSP_LOG_DEBUG("SymbolDatabase: empty file");
        return false;
    }

    std::string content(static_cast<size_t>(size), '\0');
    file.seekg(0);
    file.read(content.data(), size);
    file.close();

    JsonReader reader(content);
    if (!reader.ParseSymbols(m_symbols)) {
        SSP_LOG_DEBUG("SymbolDatabase: no symbols parsed");
        m_symbols.clear();
        return false;
    }

    SSP_LOG_DEBUG("SymbolDatabase: loaded %zu symbols", m_symbols.size());
    BuildIndices();
    return true;
}

void SymbolDatabase::BuildIndices() {
    m_categoryIndex.clear();
    m_codepointIndex.clear();

    for (size_t i = 0; i < m_symbols.size(); ++i) {
        const auto& sym = m_symbols[i];
        m_categoryIndex[sym.category].push_back(i);
        if (sym.codepoint != 0) {
            m_codepointIndex[sym.codepoint] = i;
        }
    }
}

std::vector<const Symbol*> SymbolDatabase::GetByCategory(Category cat) const {
    std::vector<const Symbol*> result;
    auto it = m_categoryIndex.find(cat);
    if (it != m_categoryIndex.end()) {
        result.reserve(it->second.size());
        for (size_t idx : it->second) {
            result.push_back(&m_symbols[idx]);
        }
    }
    return result;
}

const Symbol* SymbolDatabase::FindByCodepoint(char32_t cp) const {
    auto it = m_codepointIndex.find(cp);
    if (it != m_codepointIndex.end()) {
        return &m_symbols[it->second];
    }
    return nullptr;
}

} // namespace ssp
