#pragma once

#include <string>
#include <map>
#include <filesystem>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

// Use nlohmann json
using json = nlohmann::json;

namespace PixelMotion {

/**
 * Configuration manager
 * Handles loading/saving settings from JSON
 */
class Configuration {
public:
    struct MonitorConfig {
        std::wstring wallpaperPath;
        bool enabled = true;
        int scalingMode = 0; // 0=Fill, 1=Fit, 2=Stretch, 3=Tile
    };

    struct Settings {
        bool gameModeEnabled = true;
        bool batteryAwareEnabled = true;
        bool autoStart = false;
        int batteryThreshold = 20; // Percentage
        std::map<std::wstring, MonitorConfig> monitors; // Key: monitor device name
        std::vector<std::string> processBlocklist;
    };

    Configuration();
    ~Configuration();

    bool Load();
    bool Save();

    Settings& GetSettings() { return m_settings; }
    const Settings& GetSettings() const { return m_settings; }

    // Convenience getters/setters for UI
    bool GetGameModeEnabled() const { return m_settings.gameModeEnabled; }
    void SetGameModeEnabled(bool enabled) { m_settings.gameModeEnabled = enabled; }

    bool GetBatteryAwareEnabled() const { return m_settings.batteryAwareEnabled; }
    void SetBatteryAwareEnabled(bool enabled) { m_settings.batteryAwareEnabled = enabled; }

    bool GetStartWithWindows() const { return m_settings.autoStart; }
    void SetStartWithWindows(bool enabled) { m_settings.autoStart = enabled; }

    int GetBatteryThreshold() const { return m_settings.batteryThreshold; }
    void SetBatteryThreshold(int threshold) { m_settings.batteryThreshold = threshold; }

    // Monitor-specific configuration
    MonitorConfig* GetMonitorConfig(const std::wstring& deviceName);
    void SetMonitorConfig(const std::wstring& deviceName, const MonitorConfig& config);
    
    // Process Blocklist
    void SetProcessBlocklist(const std::vector<std::string>& list);

private:
    std::filesystem::path GetConfigPath() const;

    Settings m_settings;
};

} // namespace PixelMotion
