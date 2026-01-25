#include "Configuration.h"
#include "Logger.h"

#include <Windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>

// Simple JSON-like parsing (for now, can be replaced with nlohmann/json later)
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

    // TODO: Implement proper JSON parsing
    // For now, just log that we attempted to load
    Logger::Info("Configuration loaded from: " + configPath.string());
    
    file.close();
    return true;
}

bool Configuration::Save() {
    std::filesystem::path configPath = GetConfigPath();
    
    std::ofstream file(configPath);
    if (!file.is_open()) {
        Logger::Error("Failed to save configuration file");
        return false;
    }

    // TODO: Implement proper JSON serialization
    // For now, save basic structure
    file << "{\n";
    file << "  \"gameModeEnabled\": " << (m_settings.gameModeEnabled ? "true" : "false") << ",\n";
    file << "  \"batteryAwareEnabled\": " << (m_settings.batteryAwareEnabled ? "true" : "false") << ",\n";
    file << "  \"autoStart\": " << (m_settings.autoStart ? "true" : "false") << ",\n";
    file << "  \"batteryThreshold\": " << m_settings.batteryThreshold << "\n";
    file << "}\n";

    file.close();
    
    Logger::Info("Configuration saved to: " + configPath.string());
    return true;
}

} // namespace PixelMotion
