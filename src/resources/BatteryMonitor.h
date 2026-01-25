#pragma once

#include <Windows.h>

namespace PixelMotion {

/**
 * Battery monitor
 * Monitors system power status
 */
class BatteryMonitor {
public:
    BatteryMonitor();
    ~BatteryMonitor();

    bool Initialize();
    void Shutdown();

    void Update();

    bool IsOnBattery() const { return m_onBattery; }
    int GetBatteryPercent() const { return m_batteryPercent; }
    bool IsLowBattery() const { return m_batteryPercent < 20; }

private:
    bool m_onBattery;
    int m_batteryPercent;
    bool m_initialized;
};

} // namespace PixelMotion
