#pragma once

#include <Windows.h>
#include <string>
#include <vector>

namespace PixelMotion {

/**
 * Monitor information structure
 */
struct MonitorInfo {
    HMONITOR handle;
    std::wstring deviceName;
    RECT bounds;
    int width;
    int height;
    int refreshRate;
    bool isPrimary;

    static std::vector<MonitorInfo> EnumerateMonitors();

private:
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, 
        LPRECT lprcMonitor, LPARAM dwData);
};

} // namespace PixelMotion
