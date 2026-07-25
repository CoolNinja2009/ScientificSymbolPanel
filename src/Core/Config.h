#pragma once
#include "Types.h"
#include <string>
#include <filesystem>

namespace ssp {

class Config {
public:
    Config();
    ~Config() = default;

    // Paths
    static std::filesystem::path DataDir();
    std::filesystem::path SettingsPath() const;
    std::filesystem::path RecentPath() const;
    std::filesystem::path FavoritesPath() const;

    // Load/Save
    bool Load();
    bool Save() const;

    // Access
    const AppSettings& Get() const { return m_settings; }
    AppSettings& GetMutable() { return m_settings; }

    void SetHotkey(uint32_t mods, uint32_t vk);
    void SetTheme(ThemeMode mode);
    void SetWindowSize(int32_t w, int32_t h);
    void SetWindowPos(int32_t x, int32_t y);
    void SetAnimations(bool enabled);
    void SetStartWithWindows(bool enabled);
    void Reset();

private:
    AppSettings m_settings;
    std::filesystem::path m_dataDir;
};

} // namespace ssp
