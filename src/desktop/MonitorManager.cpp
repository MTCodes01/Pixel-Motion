#include "MonitorManager.h"
#include "core/Logger.h"

namespace PixelMotion {

MonitorManager::MonitorManager()
    : m_initialized(false)
{
}

MonitorManager::~MonitorManager() {
    Shutdown();
}

bool MonitorManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing monitor manager...");

    EnumerateMonitors();

    if (m_monitors.empty()) {
        Logger::Error("No monitors detected");
        return false;
    }

    Logger::Info("Detected " + std::to_string(m_monitors.size()) + " monitor(s)");

    m_initialized = true;
    return true;
}

void MonitorManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_monitors.clear();
    m_initialized = false;
}

void MonitorManager::Update() {
    // Re-enumerate monitors to detect changes
    std::vector<MonitorInfo> oldMonitors = m_monitors;
    EnumerateMonitors();

    // Check if monitor configuration changed
    if (oldMonitors.size() != m_monitors.size()) {
        Logger::Info("Monitor configuration changed: " + 
                    std::to_string(oldMonitors.size()) + " -> " + 
                    std::to_string(m_monitors.size()));
    }
}

int MonitorManager::GetMonitorCount() const {
    return static_cast<int>(m_monitors.size());
}

const MonitorInfo* MonitorManager::GetMonitor(int index) const {
    if (index < 0 || index >= static_cast<int>(m_monitors.size())) {
        return nullptr;
    }
    return &m_monitors[index];
}

const MonitorInfo* MonitorManager::GetPrimaryMonitor() const {
    for (const auto& monitor : m_monitors) {
        if (monitor.isPrimary) {
            return &monitor;
        }
    }
    return m_monitors.empty() ? nullptr : &m_monitors[0];
}

BOOL CALLBACK MonitorManager::MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MonitorManager* pThis = reinterpret_cast<MonitorManager*>(dwData);

    MONITORINFOEX mi = {};
    mi.cbSize = sizeof(MONITORINFOEX);

    if (!GetMonitorInfo(hMonitor, &mi)) {
        return TRUE; // Continue enumeration
    }

    MonitorInfo info = {};
    info.hMonitor = hMonitor;
    info.deviceName = mi.szDevice;
    info.bounds = mi.rcMonitor;
    info.width = mi.rcMonitor.right - mi.rcMonitor.left;
    info.height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    // Get refresh rate
    DEVMODE dm = {};
    dm.dmSize = sizeof(DEVMODE);
    if (EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
        info.refreshRate = dm.dmDisplayFrequency;
    } else {
        info.refreshRate = 60; // Default fallback
    }

    pThis->m_monitors.push_back(info);

    return TRUE; // Continue enumeration
}

void MonitorManager::EnumerateMonitors() {
    m_monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(this));
}

} // namespace PixelMotion
