#include "Config.h"
#include "Log.h"
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>
#include <cwctype>
#include <sstream>

namespace ssp {

// Minimal JSON writer for our flat settings structure
static std::wstring EscapeJson(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 2);
    for (wchar_t c : s) {
        switch (c) {
        case L'"':  out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n";  break;
        case L'\r': out += L"\\r";  break;
        case L'\t': out += L"\\t";  break;
        default:    out += c;       break;
        }
    }
    return out;
}

// Minimal JSON reader — handles our config format only
static std::wstring ReadFileContents(const std::filesystem::path& path) {
    std::wifstream file(path, std::ios::binary);
    if (!file.is_open()) return {};
    file.imbue(std::locale("en_US.UTF-8"));
    std::wstringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static bool WriteFileContents(const std::filesystem::path& path, const std::wstring& content) {
    // Atomic write: temp file then rename
    auto tmp = path;
    tmp += L".tmp";
    {
        std::wofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        file.imbue(std::locale("en_US.UTF-8"));
        // Write UTF-8 BOM
        file.put(L'\xFEFF');
        file << content;
        file.close();
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

static int64_t JsonGetInt(const std::wstring& json, const std::wstring& key, int64_t def = 0) {
    std::wstring search = L"\"" + key + L"\":";
    auto pos = json.find(search);
    if (pos == std::wstring::npos) return def;
    pos += search.size();
    while (pos < json.size() && std::iswspace(json[pos])) pos++;
    if (pos >= json.size()) return def;
    // Handle negative
    int sign = 1;
    if (json[pos] == L'-') { sign = -1; pos++; }
    if (!std::iswdigit(json[pos])) return def;
    int64_t val = 0;
    while (pos < json.size() && std::iswdigit(json[pos])) {
        val = val * 10 + (json[pos] - L'0');
        pos++;
    }
    return val * sign;
}

static bool JsonGetBool(const std::wstring& json, const std::wstring& key, bool def = false) {
    std::wstring search = L"\"" + key + L"\":";
    auto pos = json.find(search);
    if (pos == std::wstring::npos) return def;
    pos += search.size();
    while (pos < json.size() && std::iswspace(json[pos])) pos++;
    if (json.substr(pos, 4) == L"true") return true;
    if (json.substr(pos, 5) == L"false") return false;
    return def;
}

static std::wstring JsonGetString(const std::wstring& json, const std::wstring& key, const std::wstring& def = {}) {
    std::wstring search = L"\"" + key + L"\":";
    auto pos = json.find(search);
    if (pos == std::wstring::npos) return def;
    pos += search.size();
    while (pos < json.size() && std::iswspace(json[pos])) pos++;
    if (pos >= json.size() || json[pos] != L'"') return def;
    pos++; // skip opening quote
    std::wstring val;
    while (pos < json.size()) {
        if (json[pos] == L'"') break;
        if (json[pos] == L'\\' && pos + 1 < json.size()) {
            pos++;
            switch (json[pos]) {
            case L'"':  val += L'"'; break;
            case L'\\': val += L'\\'; break;
            case L'n':  val += L'\n'; break;
            case L'r':  val += L'\r'; break;
            case L't':  val += L'\t'; break;
            default:    val += json[pos]; break;
            }
        } else {
            val += json[pos];
        }
        pos++;
    }
    return val;
}

// ============================================================================
// Config implementation
// ============================================================================

Config::Config() {
    m_dataDir = DataDir();
}

std::filesystem::path Config::DataDir() {
    wchar_t localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        std::filesystem::path p(localAppData);
        p /= kDataSubdir;
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }
    return std::filesystem::current_path() / kDataSubdir;
}

std::filesystem::path Config::SettingsPath() const  { return m_dataDir / kSettingsFile; }
std::filesystem::path Config::RecentPath() const    { return m_dataDir / kRecentFile; }
std::filesystem::path Config::FavoritesPath() const { return m_dataDir / kFavoritesFile; }

bool Config::Load() {
    auto path = SettingsPath();
    auto content = ReadFileContents(path);
    if (content.empty()) {
        SSP_LOG_DEBUG("Config: no settings file, using defaults");
        return false;
    }

    m_settings.hotkeyModifiers = static_cast<uint32_t>(JsonGetInt(content, L"hotkeyModifiers", m_settings.hotkeyModifiers));
    m_settings.hotkeyVk        = static_cast<uint32_t>(JsonGetInt(content, L"hotkeyVk", m_settings.hotkeyVk));
    m_settings.windowX         = static_cast<int32_t>(JsonGetInt(content, L"windowX", m_settings.windowX));
    m_settings.windowY         = static_cast<int32_t>(JsonGetInt(content, L"windowY", m_settings.windowY));
    m_settings.windowWidth     = static_cast<int32_t>(JsonGetInt(content, L"windowWidth", m_settings.windowWidth));
    m_settings.windowHeight    = static_cast<int32_t>(JsonGetInt(content, L"windowHeight", m_settings.windowHeight));
    m_settings.theme           = static_cast<ThemeMode>(JsonGetInt(content, L"theme", static_cast<int64_t>(m_settings.theme)));
    m_settings.animations      = JsonGetBool(content, L"animations", m_settings.animations);
    m_settings.startWithWindows = JsonGetBool(content, L"startWithWindows", m_settings.startWithWindows);
    m_settings.maxRecent       = static_cast<int32_t>(JsonGetInt(content, L"maxRecent", m_settings.maxRecent));
    m_settings.fuzzySearch     = JsonGetBool(content, L"fuzzySearch", m_settings.fuzzySearch);

    SSP_LOG_DEBUG("Config: loaded from %ls", path.c_str());
    return true;
}

bool Config::Save() const {
    auto path = SettingsPath();
    auto& s = m_settings;

    std::wstring json;
    json += L"{\n";
    json += L"  \"hotkeyModifiers\": " + std::to_wstring(s.hotkeyModifiers) + L",\n";
    json += L"  \"hotkeyVk\": " + std::to_wstring(s.hotkeyVk) + L",\n";
    json += L"  \"windowX\": " + std::to_wstring(s.windowX) + L",\n";
    json += L"  \"windowY\": " + std::to_wstring(s.windowY) + L",\n";
    json += L"  \"windowWidth\": " + std::to_wstring(s.windowWidth) + L",\n";
    json += L"  \"windowHeight\": " + std::to_wstring(s.windowHeight) + L",\n";
    json += L"  \"theme\": " + std::to_wstring(static_cast<int>(s.theme)) + L",\n";
    json += L"  \"animations\": " + std::wstring(s.animations ? L"true" : L"false") + L",\n";
    json += L"  \"startWithWindows\": " + std::wstring(s.startWithWindows ? L"true" : L"false") + L",\n";
    json += L"  \"maxRecent\": " + std::to_wstring(s.maxRecent) + L",\n";
    json += L"  \"fuzzySearch\": " + std::wstring(s.fuzzySearch ? L"true" : L"false") + L"\n";
    json += L"}\n";

    bool ok = WriteFileContents(path, json);
    SSP_LOG_DEBUG("Config: saved to %ls (%s)", path.c_str(), ok ? "ok" : "failed");
    return ok;
}

void Config::SetHotkey(uint32_t mods, uint32_t vk) {
    m_settings.hotkeyModifiers = mods;
    m_settings.hotkeyVk = vk;
    Save();
}

void Config::SetTheme(ThemeMode mode) {
    m_settings.theme = mode;
    Save();
}

void Config::SetWindowSize(int32_t w, int32_t h) {
    m_settings.windowWidth = w;
    m_settings.windowHeight = h;
    Save();
}

void Config::SetWindowPos(int32_t x, int32_t y) {
    m_settings.windowX = x;
    m_settings.windowY = y;
    Save();
}

void Config::SetAnimations(bool enabled) {
    m_settings.animations = enabled;
    Save();
}

void Config::SetStartWithWindows(bool enabled) {
    m_settings.startWithWindows = enabled;
    Save();
}

void Config::Reset() {
    m_settings = AppSettings{};
    Save();
}

} // namespace ssp
