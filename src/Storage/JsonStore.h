#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace ssp {

// ============================================================================
// JsonValue — a JSON value that can be null, bool, number, string, array, or object.
// Strings are stored as std::wstring internally.
// ============================================================================
class JsonValue {
public:
    enum Type : uint8_t { Null, Bool, Number, String, Array, Object };

    // --- Construction ---
    JsonValue() : m_type(Null) {}
    explicit JsonValue(bool v) : m_type(Bool), m_bool(v) {}
    explicit JsonValue(int64_t v) : m_type(Number), m_num(static_cast<double>(v)) {}
    explicit JsonValue(double v) : m_type(Number), m_num(v) {}
    explicit JsonValue(std::wstring v) : m_type(String), m_str(std::move(v)) {}
    explicit JsonValue(const wchar_t* v) : m_type(String), m_str(v) {}

    // --- Type query ---
    Type type() const { return m_type; }

    // --- Const accessors ---
    bool AsBool() const;
    int64_t AsInt() const;
    double AsDouble() const;
    std::wstring AsString() const;
    const std::vector<JsonValue>& AsArray() const;
    const std::unordered_map<std::wstring, JsonValue>& AsObject() const;

    bool Has(const std::wstring& key) const;
    const JsonValue& operator[](const std::wstring& key) const;
    const JsonValue& operator[](size_t idx) const;
    size_t Size() const;

    // --- Mutators (for building JSON trees) ---
    void SetNull();
    void SetBool(bool v);
    void SetNumber(double v);
    void SetString(std::wstring v);
    void SetArray();
    void SetObject();

    JsonValue& operator[](const std::wstring& key); // inserts if missing
    void Push(JsonValue v);

    // --- Sentinel ---
    static const JsonValue& NullSentinel();

private:
    Type m_type = Null;
    bool m_bool = false;
    double m_num = 0.0;
    std::wstring m_str;
    std::vector<JsonValue> m_arr;
    std::unordered_map<std::wstring, JsonValue> m_obj;
};

// ============================================================================
// JsonStore — atomic file-backed JSON storage.
// ============================================================================
class JsonStore {
public:
    explicit JsonStore(std::filesystem::path path);

    // Returns the parsed JSON root, or Null on failure.
    JsonValue Load();

    // Writes atomically: temp file, then rename. Returns true on success.
    bool Save(const JsonValue& root);

private:
    std::filesystem::path m_path;
};

} // namespace ssp
