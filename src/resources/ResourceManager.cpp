#include "ResourceManager.h"
#include "GameModeDetector.h"
#include "BatteryMonitor.h"
#include "core/Logger.h"

namespace PixelMotion {

ResourceManager::ResourceManager()
    : m_paused(false)
    , m_manualPause(false)
    , m_fpsMultiplier(1.0f)
    , m_initialized(false)
{
}

ResourceManager::~ResourceManager() {
    Shutdown();
}

bool ResourceManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing Resource Manager...");

    // Initialize Game Mode detector
    m_gameModeDetector = std::make_unique<GameModeDetector>();
    if (!m_gameModeDetector->Initialize()) {
        Logger::Error("Failed to initialize Game Mode detector");
        return false;
    }

    // Initialize Battery monitor
    m_batteryMonitor = std::make_unique<BatteryMonitor>();
    if (!m_batteryMonitor->Initialize()) {
        Logger::Error("Failed to initialize Battery monitor");
        return false;
    }

    m_initialized = true;
    Logger::Info("Resource Manager initialized successfully");
    return true;
}

void ResourceManager::SetProcessBlocklist(const std::vector<std::string>& list) {
    if (m_gameModeDetector) {
        m_gameModeDetector->SetProcessBlocklist(list);
    }
}

void ResourceManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("Shutting down Resource Manager...");

    m_batteryMonitor.reset();
    m_gameModeDetector.reset();

    m_initialized = false;
}

void ResourceManager::Update() {
    if (!m_initialized) {
        return;
    }

    // Update detectors
    m_gameModeDetector->Update();
    m_batteryMonitor->Update();

    // Update pause state based on detectors
    UpdatePauseState();
}

void ResourceManager::UpdatePauseState() {
    bool wasPaused = m_paused;

    // Manual pause takes precedence
    if (m_manualPause) {
        m_paused = true;
        m_fpsMultiplier = 0.0f;
        return;
    }

    // Pause if fullscreen game detected
    if (m_gameModeDetector->IsFullscreenAppActive()) {
        m_paused = true;
        m_fpsMultiplier = 0.0f;
        
        if (!wasPaused) {
            Logger::Info("Game Mode activated - pausing wallpapers");
        }
        return;
    }

    // Adjust FPS based on battery status
    if (m_batteryMonitor->IsOnBattery()) {
        m_paused = false;
        
        int batteryPercent = m_batteryMonitor->GetBatteryPercent();
        if (batteryPercent < 20) {
            m_fpsMultiplier = 0.0f; // Pause completely
            m_paused = true;
            
            if (!wasPaused) {
                Logger::Info("Low battery - pausing wallpapers");
            }
        } else if (batteryPercent < 50) {
            m_fpsMultiplier = 0.25f; // 15 FPS
            
            if (wasPaused) {
                Logger::Info("Battery mode - reduced FPS");
            }
        } else {
            m_fpsMultiplier = 0.5f; // 30 FPS
            
            if (wasPaused) {
                Logger::Info("Battery mode - moderate FPS");
            }
        }
    } else {
        // On AC power - full speed
        m_paused = false;
        m_fpsMultiplier = 1.0f;
        
        if (wasPaused) {
            Logger::Info("AC power - resuming wallpapers");
        }
    }
}

} // namespace PixelMotion
