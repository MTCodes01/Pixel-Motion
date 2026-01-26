#include "Configuration.h"
#include "Logger.h"

#include <Windows.h>
#include <shlobj.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PixelMotion {

Configuration::Configuration() {
    // Default settings are initialized in struct
}

Configuration::~Configuration() {
    // Auto-save on destruction
}

std::filesystem::path Configuration::GetConfigPath() const {
    wchar_t* localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData))) {
        std::filesystem::path configDir = std::filesystem::path(localAppData) / L"PixelMotion";
        CoTaskMemFree(localAppData);
        
        std::filesystem::create_directories(configDir);
        return configDir / L"config.json";
    }
    return L"config.json";
}

bool Configuration::Load() {
    std::filesystem::path configPath = GetConfigPath();
    
    if (!std::filesystem::exists(configPath)) {
        Logger::Info("No configuration file found, using defaults");
        return false;
    }

    std::ifstream file(configPath);
    if (!file.is_open()) {
        Logger::Error("Failed to open configuration file");
        return false;
    }

    try {
        json j = json::parse(file);
        
        // Load global settings
        if (j.contains("gameModeEnabled")) {
            m_settings.gameModeEnabled = j["gameModeEnabled"].get<bool>();
        }
        if (j.contains("batteryAwareEnabled")) {
            m_settings.batteryAwareEnabled = j["batteryAwareEnabled"].get<bool>();
        }
        if (j.contains("autoStart")) {
            m_settings.autoStart = j["autoStart"].get<bool>();
        }
        if (j.contains("batteryThreshold")) {
            m_settings.batteryThreshold = j["batteryThreshold"].get<int>();
        }
        
        // Load monitor configurations
        if (j.contains("monitors")) {
            for (auto& [key, value] : j["monitors"].items()) {
                MonitorConfig config;
                
                if (value.contains("wallpaperPath")) {
                    std::string pathUtf8 = value["wallpaperPath"].get<std::string>();
                    int wideLen = MultiByteToWideChar(CP_UTF8, 0, pathUtf8.c_str(), -1, nullptr, 0);
                    if (wideLen > 0) {
                        std::wstring widePath(wideLen, L'\0');
                        MultiByteToWideChar(CP_UTF8, 0, pathUtf8.c_str(), -1, &widePath[0], wideLen);
                        widePath.resize(wideLen - 1); // Remove null terminator
                        config.wallpaperPath = widePath;
                    }
                }
                
                if (value.contains("enabled")) {
                    config.enabled = value["enabled"].get<bool>();
                }
                if (value.contains("loop")) {
                    config.loop = value["loop"].get<bool>();
                }
                if (value.contains("volume")) {
                    config.volume = value["volume"].get<float>();
                }
                if (value.contains("scalingMode")) {
                    config.scalingMode = value["scalingMode"].get<int>();
                }
                
                // Convert key from UTF-8 to wide string
                int wideLen = MultiByteToWideChar(CP_UTF8, 0, key.c_str(), -1, nullptr, 0);
                if (wideLen > 0) {
                    std::wstring wideKey(wideLen, L'\0');
                    MultiByteToWideChar(CP_UTF8, 0, key.c_str(), -1, &wideKey[0], wideLen);
                    wideKey.resize(wideLen - 1); // Remove null terminator
                    m_settings.monitors[wideKey] = config;
                }
            }
        }
        
        Logger::Info("Configuration loaded from: " + configPath.string());
        file.close();
        return true;
        
    } catch (const json::exception& e) {
        Logger::Error("Failed to parse configuration JSON: " + std::string(e.what()));
        file.close();
        return false;
    }
}

bool Configuration::Save() {
    std::filesystem::path configPath = GetConfigPath();
    
    std::ofstream file(configPath);
    if (!file.is_open()) {
        Logger::Error("Failed to save configuration file");
        return false;
    }

    try {
        json j;
        
        // Save global settings
        j["gameModeEnabled"] = m_settings.gameModeEnabled;
        j["batteryAwareEnabled"] = m_settings.batteryAwareEnabled;
        j["autoStart"] = m_settings.autoStart;
        j["batteryThreshold"] = m_settings.batteryThreshold;
        
        // Save monitor configurations
        json monitorsJson = json::object();
        for (const auto& [deviceName, config] : m_settings.monitors) {
            json monitorJson;
            
            // Convert wide string to UTF-8
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, config.wallpaperPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                std::string pathUtf8(utf8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, config.wallpaperPath.c_str(), -1, &pathUtf8[0], utf8Len, nullptr, nullptr);
                pathUtf8.resize(utf8Len - 1); // Remove null terminator
                monitorJson["wallpaperPath"] = pathUtf8;
            } else {
                monitorJson["wallpaperPath"] = "";
            }
            
            monitorJson["enabled"] = config.enabled;
            monitorJson["loop"] = config.loop;
            monitorJson["volume"] = config.volume;
            monitorJson["scalingMode"] = config.scalingMode;
            
            // Convert device name to UTF-8 for JSON key
            int keyLen = WideCharToMultiByte(CP_UTF8, 0, deviceName.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (keyLen > 0) {
                std::string keyUtf8(keyLen, '\0');
                WideCharToMultiByte(CP_UTF8, 0, deviceName.c_str(), -1, &keyUtf8[0], keyLen, nullptr, nullptr);
                keyUtf8.resize(keyLen - 1); // Remove null terminator
                monitorsJson[keyUtf8] = monitorJson;
            }
        }
        j["monitors"] = monitorsJson;
        
        // Write with pretty formatting
        file << j.dump(2);
        file.close();
        
        Logger::Info("Configuration saved to: " + configPath.string());
        return true;
        
    } catch (const json::exception& e) {
        Logger::Error("Failed to serialize configuration JSON: " + std::string(e.what()));
        file.close();
        return false;
    }
}

Configuration::MonitorConfig* Configuration::GetMonitorConfig(const std::wstring& deviceName) {
    auto it = m_settings.monitors.find(deviceName);
    if (it != m_settings.monitors.end()) {
        return &it->second;
    }
    return nullptr;
}

void Configuration::SetMonitorConfig(const std::wstring& deviceName, const MonitorConfig& config) {
    m_settings.monitors[deviceName] = config;
}

} // namespace PixelMotion
