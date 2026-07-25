#include "JsonStore.h"
#include "Core/Log.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cwctype>

namespace ssp {

// ============================================================================
// JsonValue sentinel
// ============================================================================
const JsonValue& JsonValue::NullSentinel() {
    static const JsonValue s_null;
    return s_null;
}

// ============================================================================
// JsonValue mutators
// ============================================================================
void JsonValue::SetNull()  { *this = JsonValue{}; }
void JsonValue::SetBool(bool v) { *this = JsonValue(v); }
void JsonValue::SetNumber(double v) { *this = JsonValue(v); }
void JsonValue::SetString(std::wstring v) { *this = JsonValue(std::move(v)); }
void JsonValue::SetArray() { m_type = Array; m_arr.clear(); }
void JsonValue::SetObject() { m_type = Object; m_obj.clear(); }

JsonValue& JsonValue::operator[](const std::wstring& key) {
    if (m_type != Object) { SetObject(); }
    return m_obj[key];
}

void JsonValue::Push(JsonValue v) {
    if (m_type != Array) { SetArray(); }
    m_arr.push_back(std::move(v));
}

// ============================================================================
// JsonValue accessors
// ============================================================================
bool JsonValue::AsBool() const {
    switch (m_type) {
    case Bool:   return m_bool;
    case Number: return m_num != 0.0;
    case String: return !m_str.empty();
    default:     return false;
    }
}

int64_t JsonValue::AsInt() const {
    if (m_type == Number) return static_cast<int64_t>(m_num);
    if (m_type == Bool) return m_bool ? 1 : 0;
    return 0;
}

double JsonValue::AsDouble() const {
    if (m_type == Number) return m_num;
    if (m_type == Bool) return m_bool ? 1.0 : 0.0;
    return 0.0;
}

std::wstring JsonValue::AsString() const {
    if (m_type == String) return m_str;
    if (m_type == Number) return std::to_wstring(m_num);
    if (m_type == Bool) return m_bool ? L"true" : L"false";
    return {};
}

const std::vector<JsonValue>& JsonValue::AsArray() const {
    static const std::vector<JsonValue> s_empty;
    return (m_type == Array) ? m_arr : s_empty;
}

const std::unordered_map<std::wstring, JsonValue>& JsonValue::AsObject() const {
    static const std::unordered_map<std::wstring, JsonValue> s_empty;
    return (m_type == Object) ? m_obj : s_empty;
}

bool JsonValue::Has(const std::wstring& key) const {
    return m_type == Object && m_obj.contains(key);
}

const JsonValue& JsonValue::operator[](const std::wstring& key) const {
    if (m_type == Object) {
        auto it = m_obj.find(key);
        if (it != m_obj.end()) return it->second;
    }
    return NullSentinel();
}

const JsonValue& JsonValue::operator[](size_t idx) const {
    if (m_type == Array && idx < m_arr.size()) return m_arr[idx];
    return NullSentinel();
}

size_t JsonValue::Size() const {
    switch (m_type) {
    case Array:  return m_arr.size();
    case Object: return m_obj.size();
    case String: return m_str.size();
    default:     return 0;
    }
}

// ============================================================================
// JsonStore
// ============================================================================
JsonStore::JsonStore(std::filesystem::path path) : m_path(std::move(path)) {}

// ---- forward declarations for parser ----
namespace {
    struct PState {
        std::wstring_view input;
        size_t pos = 0;
    };

    void SkipWS(PState& s);
    std::optional<wchar_t> PeekChar(const PState& s);
    wchar_t NextChar(PState& s);
    void ExpectChar(PState& s, wchar_t c);
    JsonValue ParseVal(PState& s);
    JsonValue ParseObj(PState& s);
    JsonValue ParseArr(PState& s);
    std::wstring ParseStr(PState& s);
    JsonValue ParseNum(PState& s);

    std::wstring SerializeValue(const JsonValue& v, int indent);
    std::wstring EscapeJson(const std::wstring& s);
    std::wstring IndentStr(int level);
}

// ---- file I/O (no Windows API conflicts) ----
static std::wstring LoadFileUtf8(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return {};

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size <= 0) return {};
    file.seekg(0, std::ios::beg);

    std::string utf8(static_cast<size_t>(size), '\0');
    file.read(utf8.data(), size);
    file.close();

    const char* data = utf8.data();
    size_t len = utf8.size();
    if (len >= 3 && static_cast<uint8_t>(data[0]) == 0xEF &&
        static_cast<uint8_t>(data[1]) == 0xBB &&
        static_cast<uint8_t>(data[2]) == 0xBF) {
        data += 3;
        len -= 3;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, static_cast<int>(len), nullptr, 0);
    if (wlen <= 0) {
        SSP_LOG_DEBUG("JsonStore: UTF-8 decode failed for %ls (error %lu)", path.c_str(), GetLastError());
        return {};
    }
    std::wstring result(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, data, static_cast<int>(len), result.data(), wlen);
    return result;
}

static bool SaveFileUtf8(const std::filesystem::path& path, const std::wstring& content) {
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, content.data(), static_cast<int>(content.size()),
                                       nullptr, 0, nullptr, nullptr);
    if (utf8len <= 0) return false;
    std::string utf8(utf8len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, content.data(), static_cast<int>(content.size()),
                         utf8.data(), utf8len, nullptr, nullptr);

    auto tmp = path;
    tmp += L".tmp";
    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        file.close();
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

// ---- public Load/Save ----
JsonValue JsonStore::Load() {
    auto content = LoadFileUtf8(m_path);
    if (content.empty()) {
        SSP_LOG_DEBUG("JsonStore: no file at %ls", m_path.c_str());
        return {};
    }
    PState s{content, 0};
    try {
        SkipWS(s);
        auto root = ParseVal(s);
        SkipWS(s);
        SSP_LOG_DEBUG("JsonStore: loaded %ls", m_path.c_str());
        return root;
    } catch (const std::exception& e) {
        SSP_LOG_DEBUG("JsonStore: parse error in %ls: %hs", m_path.c_str(), e.what());
        return {};
    }
}

bool JsonStore::Save(const JsonValue& root) {
    std::wstring content = SerializeValue(root, 0);
    bool ok = SaveFileUtf8(m_path, content);
    SSP_LOG_DEBUG("JsonStore: saved %ls (%s)", m_path.c_str(), ok ? "ok" : "failed");
    return ok;
}

// ============================================================================
// JSON Parser (anonymous namespace)
// ============================================================================
namespace {

void SkipWS(PState& s) {
    while (s.pos < s.input.size() && std::iswspace(s.input[s.pos])) s.pos++;
}

std::optional<wchar_t> PeekChar(const PState& s) {
    if (s.pos >= s.input.size()) return std::nullopt;
    return s.input[s.pos];
}

wchar_t NextChar(PState& s) {
    assert(s.pos < s.input.size());
    return s.input[s.pos++];
}

void ExpectChar(PState& s, wchar_t c) {
    if (s.pos >= s.input.size() || s.input[s.pos] != c)
        throw std::runtime_error("Unexpected character in JSON");
    s.pos++;
}

JsonValue ParseVal(PState& s) {
    SkipWS(s);
    auto c = PeekChar(s);
    if (!c) throw std::runtime_error("Unexpected end of JSON");

    switch (*c) {
    case L'{': return ParseObj(s);
    case L'[': return ParseArr(s);
    case L'"': return JsonValue(ParseStr(s));
    case L't':
        if (s.input.substr(s.pos, 4) == L"true") { s.pos += 4; return JsonValue(true); }
        throw std::runtime_error("Invalid literal");
    case L'f':
        if (s.input.substr(s.pos, 5) == L"false") { s.pos += 5; return JsonValue(false); }
        throw std::runtime_error("Invalid literal");
    case L'n':
        if (s.input.substr(s.pos, 4) == L"null") { s.pos += 4; return {}; }
        throw std::runtime_error("Invalid literal");
    default: return ParseNum(s);
    }
}

JsonValue ParseObj(PState& s) {
    ExpectChar(s, L'{');
    JsonValue obj; obj.SetObject();

    SkipWS(s);
    if (auto n = PeekChar(s); n && *n == L'}') { s.pos++; return obj; }

    while (true) {
        SkipWS(s);
        auto key = ParseStr(s);
        SkipWS(s);
        ExpectChar(s, L':');
        obj[key] = ParseVal(s);

        SkipWS(s);
        auto n = PeekChar(s);
        if (!n) throw std::runtime_error("Unterminated object");
        if (*n == L'}') { s.pos++; break; }
        if (*n == L',') { s.pos++; }
        else throw std::runtime_error("Expected ',' or '}' in object");
    }
    return obj;
}

JsonValue ParseArr(PState& s) {
    ExpectChar(s, L'[');
    JsonValue arr; arr.SetArray();

    SkipWS(s);
    if (auto n = PeekChar(s); n && *n == L']') { s.pos++; return arr; }

    while (true) {
        arr.Push(ParseVal(s));
        SkipWS(s);
        auto n = PeekChar(s);
        if (!n) throw std::runtime_error("Unterminated array");
        if (*n == L']') { s.pos++; break; }
        if (*n == L',') { s.pos++; }
        else throw std::runtime_error("Expected ',' or ']' in array");
    }
    return arr;
}

std::wstring ParseStr(PState& s) {
    ExpectChar(s, L'"');
    std::wstring result;
    result.reserve(64);

    while (s.pos < s.input.size()) {
        wchar_t c = NextChar(s);
        if (c == L'"') return result;
        if (c == L'\\') {
            if (s.pos >= s.input.size()) throw std::runtime_error("Unterminated string escape");
            wchar_t esc = NextChar(s);
            switch (esc) {
            case L'"':  result += L'"';  break;
            case L'\\': result += L'\\'; break;
            case L'/':  result += L'/';  break;
            case L'b':  result += L'\b'; break;
            case L'f':  result += L'\f'; break;
            case L'n':  result += L'\n'; break;
            case L'r':  result += L'\r'; break;
            case L't':  result += L'\t'; break;
            case L'u': {
                if (s.pos + 4 > s.input.size()) throw std::runtime_error("Unterminated unicode escape");
                wchar_t buf[5] = {};
                for (int i = 0; i < 4; i++) buf[i] = s.input[s.pos++];
                result += static_cast<wchar_t>(std::wcstoul(buf, nullptr, 16));
                break;
            }
            default: result += esc; break;
            }
        } else {
            result += c;
        }
    }
    throw std::runtime_error("Unterminated string");
}

JsonValue ParseNum(PState& s) {
    size_t start = s.pos;
    bool isFloat = false;

    if (s.pos < s.input.size() && s.input[s.pos] == L'-') s.pos++;
    while (s.pos < s.input.size() && std::iswdigit(s.input[s.pos])) s.pos++;

    if (s.pos < s.input.size() && s.input[s.pos] == L'.') {
        isFloat = true; s.pos++;
        while (s.pos < s.input.size() && std::iswdigit(s.input[s.pos])) s.pos++;
    }
    if (s.pos < s.input.size() && (s.input[s.pos] == L'e' || s.input[s.pos] == L'E')) {
        isFloat = true; s.pos++;
        if (s.pos < s.input.size() && (s.input[s.pos] == L'+' || s.input[s.pos] == L'-')) s.pos++;
        while (s.pos < s.input.size() && std::iswdigit(s.input[s.pos])) s.pos++;
    }

    std::wstring numStr(s.input.substr(start, s.pos - start));
    if (isFloat) return JsonValue(std::wcstod(numStr.c_str(), nullptr));
    return JsonValue(static_cast<int64_t>(std::wcstoll(numStr.c_str(), nullptr, 10)));
}

// ============================================================================
// JSON Serializer (anonymous namespace)
// ============================================================================
std::wstring EscapeJson(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 2);
    for (wchar_t c : s) {
        switch (c) {
        case L'"':  out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\b': out += L"\\b";  break;
        case L'\f': out += L"\\f";  break;
        case L'\n': out += L"\\n";  break;
        case L'\r': out += L"\\r";  break;
        case L'\t': out += L"\\t";  break;
        default:
            if (c < 0x20) {
                wchar_t buf[8];
                swprintf_s(buf, L"\\u%04x", static_cast<int>(c));
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

std::wstring IndentStr(int level) { return std::wstring(level * 2, L' '); }

std::wstring SerializeValue(const JsonValue& v, int indent) {
    switch (v.type()) {
    case JsonValue::Null: return L"null";
    case JsonValue::Bool: return v.AsBool() ? L"true" : L"false";
    case JsonValue::Number: {
        double d = v.AsDouble();
        if (d == static_cast<int64_t>(d) && !std::isinf(d))
            return std::to_wstring(v.AsInt());
        return std::to_wstring(d);
    }
    case JsonValue::String:
        return L"\"" + EscapeJson(v.AsString()) + L"\"";

    case JsonValue::Array: {
        const auto& arr = v.AsArray();
        if (arr.empty()) return L"[]";
        std::wstring out = L"[\n";
        for (size_t i = 0; i < arr.size(); i++) {
            out += IndentStr(indent + 1) + SerializeValue(arr[i], indent + 1);
            if (i + 1 < arr.size()) out += L",";
            out += L"\n";
        }
        out += IndentStr(indent) + L"]";
        return out;
    }
    case JsonValue::Object: {
        const auto& obj = v.AsObject();
        if (obj.empty()) return L"{}";
        std::wstring out = L"{\n";
        size_t i = 0;
        for (const auto& [key, val] : obj) {
            out += IndentStr(indent + 1) + L"\"" + EscapeJson(key) + L"\": " + SerializeValue(val, indent + 1);
            if (++i < obj.size()) out += L",";
            out += L"\n";
        }
        out += IndentStr(indent) + L"}";
        return out;
    }
    }
    return L"null";
}

} // anonymous namespace

} // namespace ssp
