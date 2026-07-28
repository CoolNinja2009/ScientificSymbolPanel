#include <windows.h>
#include "Platform/Platform.h"
#include "Database.h"
#include "Core/Log.h"
#include <fstream>
#include <cstring>

namespace ssp {

// ============================================================================
// Binary reader helpers
// ============================================================================

static uint32_t ReadU32(const uint8_t*& p) { uint32_t v = *reinterpret_cast<const uint32_t*>(p); p += 4; return v; }
static uint16_t ReadU16(const uint8_t*& p) { uint16_t v = *reinterpret_cast<const uint16_t*>(p); p += 2; return v; }
static uint8_t  ReadU8 (const uint8_t*& p) { uint8_t  v = *p; p += 1; return v; }

static std::wstring ReadWStr(const uint8_t*& p) {
    uint16_t len = ReadU16(p);
    std::wstring s(reinterpret_cast<const wchar_t*>(p), len);
    p += len * 2;
    return s;
}

static std::vector<std::wstring> ReadWStrVec(const uint8_t*& p) {
    uint16_t count = ReadU16(p);
    std::vector<std::wstring> vec;
    vec.reserve(count);
    for (uint16_t i = 0; i < count; i++)
        vec.push_back(ReadWStr(p));
    return vec;
}

// ============================================================================
// Path resolution
// ============================================================================

std::filesystem::path SymbolDatabase::DefaultPath() {
    auto exeDir = Platform::ExeDir();
    std::vector<std::filesystem::path> candidates;
        candidates.push_back(exeDir / L"data" / L"symbols.bin");
        candidates.push_back(exeDir / L"data" / L"symbols.json");
        candidates.push_back(exeDir.parent_path().parent_path().parent_path() / L"data" / L"symbols.bin");
        candidates.push_back(exeDir.parent_path().parent_path() / L"data" / L"symbols.bin");
    candidates.push_back(std::filesystem::current_path() / L"data" / L"symbols.bin");
    candidates.push_back(std::filesystem::current_path() / L"data" / L"symbols.json");
    candidates.push_back(L"data\\symbols.bin");
    candidates.push_back(L"data\\symbols.json");

    for (auto& p : candidates) {
        if (std::filesystem::exists(p)) return p;
    }
    return candidates[0];
}

// ============================================================================
// Load
// ============================================================================

bool SymbolDatabase::Load() {
    auto path = DefaultPath();
    SSP_LOG_DEBUG("SymbolDatabase: loading from %ls", path.c_str());

    // Read entire file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        SSP_LOG_DEBUG("SymbolDatabase: failed to open %ls", path.c_str());
        return false;
    }

    auto size = file.tellg();
    if (size <= 0) return false;

    std::vector<uint8_t> data(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);
    file.close();

    // Detect format: binary starts with "SSPD" magic
    bool isBinary = (data.size() >= 4 &&
        data[0] == 'S' && data[1] == 'S' && data[2] == 'P' && data[3] == 'D');

    if (isBinary) {
        return LoadBinary(data);
    } else {
        return LoadJson(data);
    }
}

bool SymbolDatabase::LoadBinary(const std::vector<uint8_t>& data) {
    const uint8_t* p = data.data();

    uint32_t magic   = ReadU32(p);
    uint32_t version = ReadU32(p);
    uint32_t count   = ReadU32(p);
    if (magic != 0x44505353) return false; // "SSPD" in LE


    SSP_LOG_DEBUG("SymbolDatabase: binary v%u, %u symbols", version, count);

    m_symbols.clear();
    m_symbols.reserve(count);

    for (uint32_t i = 0; i < count; i++) {
        Symbol sym;
        sym.codepoint    = ReadU32(p);
        sym.category     = static_cast<Category>(ReadU8(p));
        sym.symbol       = ReadWStr(p);
        sym.name         = ReadWStr(p);
        sym.aliases      = ReadWStrVec(p);
        sym.keywords     = ReadWStrVec(p);
        sym.latex        = ReadWStr(p);
        sym.htmlEntity   = ReadWStr(p);
        sym.description  = ReadWStr(p);
        m_symbols.push_back(std::move(sym));
    }

    SSP_LOG_DEBUG("SymbolDatabase: loaded %zu symbols (binary)", m_symbols.size());
    BuildIndices();
    return !m_symbols.empty();
}

bool SymbolDatabase::LoadJson(const std::vector<uint8_t>& data) {
    // Minimal JSON fallback parser for symbols.json
    // This is a simplified parser for the known format
    SSP_LOG_DEBUG("SymbolDatabase: falling back to JSON parser");

    std::string_view json(reinterpret_cast<const char*>(data.data()), data.size());

    // Skip to first '['
    auto pos = json.find('[');
    if (pos == std::string_view::npos) return false;
    pos++;

    m_symbols.clear();

    // Parse each symbol object
    while (pos < json.size()) {
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
            pos++;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] == ',') { pos++; continue; }
        if (json[pos] != '{') break;

        Symbol sym;
        pos++; // skip '{'

        while (pos < json.size()) {
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t' || json[pos] == ','))
                pos++;
            if (pos >= json.size() || json[pos] == '}') { pos++; break; }
            if (json[pos] != '"') break;

            // Read key
            pos++;
            auto keyEnd = json.find('"', pos);
            if (keyEnd == std::string_view::npos) break;
            std::string_view key(json.data() + pos, keyEnd - pos);
            pos = keyEnd + 1;

            // Skip ':'
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) pos++;

            if (key == "codepoint") {
                auto end = json.find_first_of(",}\n\r \t", pos);
                sym.codepoint = static_cast<char32_t>(strtoul(json.data() + pos, nullptr, 10));
                pos = end;
            } else if (key == "category") {
                auto end = json.find('"', pos + 1);
                std::string cat(json.data() + pos + 1, end - pos - 1);
                for (int i = 0; i < static_cast<int>(Category::COUNT); i++)
                    if (cat == CategoryNames[i]) { sym.category = static_cast<Category>(i); break; }
                pos = end + 1;
            } else if (key == "symbol" || key == "name" || key == "latex" || key == "htmlEntity" || key == "description") {
                auto end = json.find('"', pos + 1);
                std::string val(json.data() + pos + 1, end - pos - 1);
                std::wstring wval(val.begin(), val.end());
                if (key == "symbol") sym.symbol = wval;
                else if (key == "name") sym.name = wval;
                else if (key == "latex") sym.latex = wval;
                else if (key == "htmlEntity") sym.htmlEntity = wval;
                else sym.description = wval;
                pos = end + 1;
            } else if (key == "aliases" || key == "keywords") {
                while (pos < json.size() && json[pos] != '[') pos++;
                pos++; // skip '['
                std::vector<std::wstring> vec;
                while (pos < json.size()) {
                    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t' || json[pos] == ','))
                        pos++;
                    if (pos >= json.size() || json[pos] == ']') { pos++; break; }
                    if (json[pos] != '"') break;
                    auto end = json.find('"', pos + 1);
                    std::string val(json.data() + pos + 1, end - pos - 1);
                    vec.push_back(std::wstring(val.begin(), val.end()));
                    pos = end + 1;
                }
                if (key == "aliases") sym.aliases = std::move(vec);
                else sym.keywords = std::move(vec);
            } else {
                // Skip unknown value
                if (json[pos] == '"') { pos = json.find('"', pos + 1) + 1; }
                else if (json[pos] == '[') { int depth = 1; pos++; while (pos < json.size() && depth > 0) { if (json[pos] == '[') depth++; if (json[pos] == ']') depth--; pos++; } }
                else if (json[pos] == '{') { int depth = 1; pos++; while (pos < json.size() && depth > 0) { if (json[pos] == '{') depth++; if (json[pos] == '}') depth--; pos++; } }
                else { while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != '\n') pos++; }
            }
        }

        m_symbols.push_back(std::move(sym));
    }

    SSP_LOG_DEBUG("SymbolDatabase: loaded %zu symbols (JSON fallback)", m_symbols.size());
    BuildIndices();
    return !m_symbols.empty();
}

// ============================================================================
// Indices
// ============================================================================

void SymbolDatabase::BuildIndices() {
    m_categoryIndex.clear();
    m_codepointIndex.clear();
    for (size_t i = 0; i < m_symbols.size(); ++i) {
        const auto& sym = m_symbols[i];
        m_categoryIndex[sym.category].push_back(i);
        if (sym.codepoint != 0) m_codepointIndex[sym.codepoint] = i;
    }
}

std::vector<const Symbol*> SymbolDatabase::GetByCategory(Category cat) const {
    std::vector<const Symbol*> result;
    auto it = m_categoryIndex.find(cat);
    if (it != m_categoryIndex.end()) {
        result.reserve(it->second.size());
        for (size_t idx : it->second) result.push_back(&m_symbols[idx]);
    }
    return result;
}

const Symbol* SymbolDatabase::FindByCodepoint(char32_t cp) const {
    auto it = m_codepointIndex.find(cp);
    return (it != m_codepointIndex.end()) ? &m_symbols[it->second] : nullptr;
}

} // namespace ssp
