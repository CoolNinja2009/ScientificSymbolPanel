#include "JsonStore.h"
#include "Core/Log.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cwctype>

namespace ssp {

// ============================================================================
// UTF-8 <-> wstring conversion (cross-platform)
// ============================================================================

static std::wstring Utf8ToWide(std::string_view u8) {
    std::wstring result;
    result.reserve(u8.size());
    for (size_t i = 0; i < u8.size(); ) {
        char32_t cp;
        uint8_t b = static_cast<uint8_t>(u8[i]);
        if (b < 0x80) { cp = b; i += 1; }
        else if ((b & 0xE0) == 0xC0 && i + 1 < u8.size()) {
            cp = ((b & 0x1F) << 6) | (static_cast<uint8_t>(u8[i+1]) & 0x3F); i += 2;
        }
        else if ((b & 0xF0) == 0xE0 && i + 2 < u8.size()) {
            cp = ((b & 0x0F) << 12) | ((static_cast<uint8_t>(u8[i+1]) & 0x3F) << 6) | (static_cast<uint8_t>(u8[i+2]) & 0x3F); i += 3;
        }
        else if ((b & 0xF8) == 0xF0 && i + 3 < u8.size()) {
            cp = ((b & 0x07) << 18) | ((static_cast<uint8_t>(u8[i+1]) & 0x3F) << 12) | ((static_cast<uint8_t>(u8[i+2]) & 0x3F) << 6) | (static_cast<uint8_t>(u8[i+3]) & 0x3F); i += 4;
        }
        else { i++; continue; }
        if (cp <= 0xFFFF) {
            result += static_cast<wchar_t>(cp);
        } else {
            cp -= 0x10000;
            result += static_cast<wchar_t>(0xD800 | (cp >> 10));
            result += static_cast<wchar_t>(0xDC00 | (cp & 0x3FF));
        }
    }
    return result;
}

static std::string WideToUtf8(std::wstring_view ws) {
    std::string result;
    result.reserve(ws.size() * 3);
    for (size_t i = 0; i < ws.size(); ) {
        char32_t cp = static_cast<char32_t>(ws[i]);
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < ws.size()) {
            char32_t lo = static_cast<char32_t>(ws[i+1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = ((cp - 0xD800) << 10) | (lo - 0xDC00);
                cp += 0x10000;
                i++;
            }
        }
        i++;
        if (cp < 0x80) { result += static_cast<char>(cp); }
        else if (cp < 0x800) { result += static_cast<char>(0xC0 | (cp >> 6)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { result += static_cast<char>(0xE0 | (cp >> 12)); result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { result += static_cast<char>(0xF0 | (cp >> 18)); result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); result += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    return result;
}

// ============================================================================
// JsonValue sentinel
// ============================================================================

const JsonValue& JsonValue::NullSentinel() {
    static JsonValue s_null;
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
    case String: return !m_str.empty() && m_str != L"false" && m_str != L"0";
    default:     return false;
    }
}

int64_t JsonValue::AsInt() const {
    return (m_type == Number) ? static_cast<int64_t>(m_num) : 0;
}

double JsonValue::AsDouble() const {
    return (m_type == Number) ? m_num : 0.0;
}

std::wstring JsonValue::AsString() const {
    if (m_type == String) return m_str;
    if (m_type == Number) {
        wchar_t buf[64];
        std::swprintf(buf, 64, L"%g", m_num);
        return buf;
    }
    if (m_type == Bool) return m_bool ? L"true" : L"false";
    return {};
}

bool JsonValue::Has(const std::wstring& key) const {
    return m_type == Object && m_obj.contains(key);
}

const std::vector<JsonValue>& JsonValue::AsArray() const {
    static std::vector<JsonValue> s_empty;
    return (m_type == Array) ? m_arr : s_empty;
}

const std::unordered_map<std::wstring, JsonValue>& JsonValue::AsObject() const {
    static std::unordered_map<std::wstring, JsonValue> s_empty;
    return (m_type == Object) ? m_obj : s_empty;
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
    if (m_type == Array) return m_arr.size();
    if (m_type == Object) return m_obj.size();
    return 0;
}

// ============================================================================
// JsonStore
// ============================================================================

JsonStore::JsonStore(std::filesystem::path path) : m_path(std::move(path)) {}

// ---- File I/O ----

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

    return Utf8ToWide(std::string_view(data, len));
}

static bool SaveFileUtf8(const std::filesystem::path& path, const std::wstring& content) {
    std::string utf8 = WideToUtf8(content);
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


// Forward declarations for parser (defined below)
namespace {
struct PState { std::wstring src; size_t pos; };
static void SkipWS(PState& s);
static JsonValue ParseVal(PState& s);
}
static std::wstring SerializeValue(const JsonValue& v, int indent);
// ---- Public Load/Save ----

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

static void SkipWS(PState& s) {
    while (s.pos < s.src.size() && std::iswspace(s.src[s.pos])) s.pos++;
}

static wchar_t Peek(PState& s) {
    SkipWS(s);
    return (s.pos < s.src.size()) ? s.src[s.pos] : L'\0';
}

static wchar_t Next(PState& s) {
    SkipWS(s);
    return (s.pos < s.src.size()) ? s.src[s.pos++] : L'\0';
}

static std::wstring ParseString(PState& s) {
    if (Next(s) != L'"') throw std::runtime_error("Expected '\"'");
    std::wstring val;
    while (s.pos < s.src.size()) {
        wchar_t c = s.src[s.pos++];
        if (c == L'"') return val;
        if (c == L'\\' && s.pos < s.src.size()) {
            c = s.src[s.pos++];
            switch (c) {
            case L'"':  val += L'"'; break;
            case L'\\': val += L'\\'; break;
            case L'/':  val += L'/'; break;
            case L'b':  val += L'\b'; break;
            case L'f':  val += L'\f'; break;
            case L'n':  val += L'\n'; break;
            case L'r':  val += L'\r'; break;
            case L't':  val += L'\t'; break;
            case L'u': {
                if (s.pos + 4 > s.src.size()) throw std::runtime_error("Incomplete \\u escape");
                wchar_t cp = 0;
                for (int i = 0; i < 4; i++) {
                    cp <<= 4;
                    wchar_t h = s.src[s.pos++];
                    if (h >= L'0' && h <= L'9') cp |= (h - L'0');
                    else if (h >= L'a' && h <= L'f') cp |= (h - L'a' + 10);
                    else if (h >= L'A' && h <= L'F') cp |= (h - L'A' + 10);
                    else throw std::runtime_error("Invalid hex in \\u escape");
                }
                val += cp;
                break;
            }
            default: val += c; break;
            }
        } else {
            val += c;
        }
    }
    throw std::runtime_error("Unterminated string");
}

static JsonValue ParseVal(PState& s);

static JsonValue ParseObject(PState& s) {
    JsonValue obj;
    obj.SetObject();
    if (Next(s) != L'{') throw std::runtime_error("Expected '{'");
    SkipWS(s);
    if (s.pos < s.src.size() && s.src[s.pos] == L'}') { s.pos++; return obj; }
    while (s.pos < s.src.size()) {
        SkipWS(s);
        std::wstring key = ParseString(s);
        SkipWS(s);
        if (Next(s) != L':') throw std::runtime_error("Expected ':'");
        obj[key] = ParseVal(s);
        SkipWS(s);
        if (s.pos >= s.src.size()) throw std::runtime_error("Unterminated object");
        if (s.src[s.pos] == L'}') { s.pos++; return obj; }
        if (s.src[s.pos] == L',') { s.pos++; continue; }
        throw std::runtime_error("Expected ',' or '}' in object");
    }
    return obj;
}

static JsonValue ParseArray(PState& s) {
    JsonValue arr;
    arr.SetArray();
    if (Next(s) != L'[') throw std::runtime_error("Expected '['");
    SkipWS(s);
    if (s.pos < s.src.size() && s.src[s.pos] == L']') { s.pos++; return arr; }
    while (s.pos < s.src.size()) {
        arr.Push(ParseVal(s));
        SkipWS(s);
        if (s.pos >= s.src.size()) throw std::runtime_error("Unterminated array");
        if (s.src[s.pos] == L']') { s.pos++; return arr; }
        if (s.src[s.pos] == L',') { s.pos++; continue; }
        throw std::runtime_error("Expected ',' or ']' in array");
    }
    return arr;
}

static JsonValue ParseVal(PState& s) {
    SkipWS(s);
    if (s.pos >= s.src.size()) throw std::runtime_error("Unexpected end of input");
    wchar_t c = s.src[s.pos];
    if (c == L'{') return ParseObject(s);
    if (c == L'[') return ParseArray(s);
    if (c == L'"') {
        JsonValue v;
        v.SetString(ParseString(s));
        return v;
    }
    if (c == L't' && s.src.substr(s.pos, 4) == L"true") { s.pos += 4; return JsonValue(true); }
    if (c == L'f' && s.src.substr(s.pos, 5) == L"false") { s.pos += 5; return JsonValue(false); }
    if (c == L'n' && s.src.substr(s.pos, 4) == L"null") { s.pos += 4; return JsonValue(); }
    if (c == L'-' || (c >= L'0' && c <= L'9')) {
        size_t start = s.pos;
        if (c == L'-') s.pos++;
        while (s.pos < s.src.size() && std::iswdigit(s.src[s.pos])) s.pos++;
        if (s.pos < s.src.size() && s.src[s.pos] == L'.') {
            s.pos++;
            while (s.pos < s.src.size() && std::iswdigit(s.src[s.pos])) s.pos++;
        }
        if (s.pos < s.src.size() && (s.src[s.pos] == L'e' || s.src[s.pos] == L'E')) {
            s.pos++;
            if (s.pos < s.src.size() && (s.src[s.pos] == L'+' || s.src[s.pos] == L'-')) s.pos++;
            while (s.pos < s.src.size() && std::iswdigit(s.src[s.pos])) s.pos++;
        }
        std::wstring numStr = s.src.substr(start, s.pos - start);
        return JsonValue(std::wcstod(numStr.c_str(), nullptr));
    }
    throw std::runtime_error("Unexpected character in JSON");
}

} // anonymous namespace

// ============================================================================
// JSON Serializer
// ============================================================================

static std::wstring EscapeJsonString(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 2);
    for (wchar_t c : s) {
        switch (c) {
        case L'"':  out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\b': out += L"\\b"; break;
        case L'\f': out += L"\\f"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default:
            if (c < 0x20) {
                wchar_t buf[8];
                std::swprintf(buf, 8, L"\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out += c;
            }
            break;
        }
    }
    return out;
}

static std::wstring SerializeValue(const JsonValue& v, int indent) {
    std::wstring pad(indent * 2, L' ');
    std::wstring padInner((indent + 1) * 2, L' ');

    switch (v.type()) {
    case JsonValue::Null:
        return L"null";
    case JsonValue::Bool:
        return v.AsBool() ? L"true" : L"false";
    case JsonValue::Number: {
        wchar_t buf[64];
        std::swprintf(buf, 64, L"%g", v.AsDouble());
        return buf;
    }
    case JsonValue::String:
        return L"\"" + EscapeJsonString(v.AsString()) + L"\"";
    case JsonValue::Array: {
        const auto& arr = v.AsArray();
        if (arr.empty()) return L"[]";
        std::wstring out = L"[\n";
        for (size_t i = 0; i < arr.size(); i++) {
            out += padInner + SerializeValue(arr[i], indent + 1);
            if (i + 1 < arr.size()) out += L",";
            out += L"\n";
        }
        out += pad + L"]";
        return out;
    }
    case JsonValue::Object: {
        const auto& obj = v.AsObject();
        if (obj.empty()) return L"{}";
        std::wstring out = L"{\n";
        size_t i = 0;
        for (const auto& [key, val] : obj) {
            out += padInner + L"\"" + EscapeJsonString(key) + L"\": " + SerializeValue(val, indent + 1);
            if (++i < obj.size()) out += L",";
            out += L"\n";
        }
        out += pad + L"}";
        return out;
    }
    }
    return L"null";
}

} // namespace ssp
