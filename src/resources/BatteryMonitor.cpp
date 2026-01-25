#include "BatteryMonitor.h"
#include "core/Logger.h"

namespace PixelMotion {

BatteryMonitor::BatteryMonitor()
    : m_onBattery(false)
    , m_batteryPercent(100)
    , m_initialized(false)
{
}

BatteryMonitor::~BatteryMonitor() {
    Shutdown();
}

bool BatteryMonitor::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing Battery monitor...");

    // Initial update
    Update();

    m_initialized = true;
    return true;
}

void BatteryMonitor::Shutdown() {
    m_initialized = false;
}

void BatteryMonitor::Update() {
    if (!m_initialized) {
        return;
    }

    SYSTEM_POWER_STATUS powerStatus;
    if (!GetSystemPowerStatus(&powerStatus)) {
        return;
    }

    bool wasOnBattery = m_onBattery;
    int oldPercent = m_batteryPercent;

    // Update battery status
    m_onBattery = (powerStatus.ACLineStatus == 0); // 0 = offline (on battery)
    m_batteryPercent = (powerStatus.BatteryLifePercent == 255) ? 
        100 : powerStatus.BatteryLifePercent;

    // Log changes
    if (wasOnBattery != m_onBattery) {
        if (m_onBattery) {
            Logger::Info("Switched to battery power (" + 
                std::to_string(m_batteryPercent) + "%)");
        } else {
            Logger::Info("Switched to AC power");
        }
    } else if (m_onBattery && abs(oldPercent - m_batteryPercent) >= 10) {
        Logger::Info("Battery level: " + std::to_string(m_batteryPercent) + "%");
    }
}

} // namespace PixelMotion
