#pragma once

#include <Windows.h>
#include <vector>
#include <string>

namespace PixelMotion {

/**
 * Monitor information structure
 */
struct MonitorInfo {
    HMONITOR hMonitor;
    std::wstring deviceName;
    RECT bounds;
    int width;
    int height;
    int refreshRate;
    bool isPrimary;
};

/**
 * Monitor manager
 * Enumerates and tracks connected monitors
 */
class MonitorManager {
public:
    MonitorManager();
    ~MonitorManager();

    bool Initialize();
    void Shutdown();
    void Update();

    int GetMonitorCount() const;
    const MonitorInfo* GetMonitor(int index) const;
    const MonitorInfo* GetPrimaryMonitor() const;

private:
    static BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
    void EnumerateMonitors();

    std::vector<MonitorInfo> m_monitors;
    bool m_initialized;
};

} // namespace PixelMotion
