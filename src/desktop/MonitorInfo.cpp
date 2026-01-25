#include "MonitorInfo.h"
#include "core/Logger.h"

namespace PixelMotion {

std::vector<MonitorInfo> MonitorInfo::EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 
        reinterpret_cast<LPARAM>(&monitors));
    
    Logger::Info("Enumerated " + std::to_string(monitors.size()) + " monitor(s)");
    return monitors;
}

BOOL CALLBACK MonitorInfo::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor,
    LPRECT lprcMonitor, LPARAM dwData)
{
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);

    MONITORINFOEX monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    
    if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
        return TRUE; // Continue enumeration
    }

    MonitorInfo info = {};
    info.handle = hMonitor;
    info.deviceName = monitorInfo.szDevice;
    info.bounds = monitorInfo.rcMonitor;
    info.width = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    info.height = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
    info.isPrimary = (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0;

    // Get refresh rate
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(DEVMODE);
    if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
        info.refreshRate = devMode.dmDisplayFrequency;
    } else {
        info.refreshRate = 60; // Default
    }

    monitors->push_back(info);
    
    return TRUE; // Continue enumeration
}

} // namespace PixelMotion
