#pragma once

#include <memory>

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
    float GetFPSMultiplier() const { return m_fpsMultiplier; }

private:
    void UpdatePauseState();

    std::unique_ptr<GameModeDetector> m_gameModeDetector;
    std::unique_ptr<BatteryMonitor> m_batteryMonitor;

    bool m_paused;
    float m_fpsMultiplier;
    bool m_initialized;
};

} // namespace PixelMotion
