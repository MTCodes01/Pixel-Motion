#pragma once

#include <memory>
#include <vector>
#include <string>

namespace PixelMotion {

class GameModeDetector;
class BatteryMonitor;

/**
 * Resource manager
 * Coordinates Game Mode and Battery-Aware features
 */
class ResourceManager {
public:
    ResourceManager();
    ~ResourceManager();

    bool Initialize();
    void Shutdown();

    void Update();

    bool IsPaused() const { return m_paused; }
    void SetPauseOnBattery(bool enabled) { m_pauseOnBattery = enabled; }
    void SetPauseOnFullscreen(bool enabled) { m_pauseOnFullscreen = enabled; }
    void SetProcessBlocklist(const std::vector<std::string>& list);
    
    // Manual pause override
    void SetPaused(bool paused) { m_manualPause = paused; }
    float GetFPSMultiplier() const { return m_fpsMultiplier; }

private:
    void UpdatePauseState();

    std::unique_ptr<GameModeDetector> m_gameModeDetector;
    std::unique_ptr<BatteryMonitor> m_batteryMonitor;

    bool m_paused;
    bool m_manualPause;
    bool m_pauseOnBattery = true;
    bool m_pauseOnFullscreen = true;
    
    float m_fpsMultiplier;
    bool m_initialized;
};

} // namespace PixelMotion
