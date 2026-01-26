#include "GameModeDetector.h"
#include "core/Logger.h"

namespace PixelMotion {

GameModeDetector::GameModeDetector()
    : m_fullscreenDetected(false)
    , m_lastForegroundWindow(nullptr)
    , m_initialized(false)
{
}

GameModeDetector::~GameModeDetector() {
    Shutdown();
}

bool GameModeDetector::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing Game Mode detector...");

    m_initialized = true;
    return true;
}

void GameModeDetector::Shutdown() {
    m_initialized = false;
}

void GameModeDetector::Update() {
    if (!m_initialized) {
        return;
    }

    // Get foreground window
    HWND foregroundWindow = GetForegroundWindow();
    
    // Check if it changed
    if (foregroundWindow != m_lastForegroundWindow) {
        m_lastForegroundWindow = foregroundWindow;
        
        // Check if it's fullscreen
        bool wasFullscreen = m_fullscreenDetected;
        m_fullscreenDetected = IsWindowFullscreen(foregroundWindow);
        
        if (m_fullscreenDetected != wasFullscreen) {
            if (m_fullscreenDetected) {
                Logger::Info("Fullscreen application detected");
            } else {
                Logger::Info("Fullscreen application closed");
            }
        }
    }
}

bool GameModeDetector::IsWindowFullscreen(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    // Get window class name
    wchar_t className[256];
    if (GetClassName(hwnd, className, 256) == 0) {
        return false;
    }

    // Ignore Desktop and Taskbar
    if (wcscmp(className, L"WorkerW") == 0 || 
        wcscmp(className, L"Progman") == 0 || 
        wcscmp(className, L"Shell_TrayWnd") == 0) {
        return false;
    }

    // Get window rectangle
    RECT windowRect;
    if (!GetWindowRect(hwnd, &windowRect)) {
        return false;
    }

    // Get monitor info for the window's monitor
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(MONITORINFO);
    
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return false;
    }

    // Check if window covers entire monitor (with small tolerance)
    const int tolerance = 2; // Allow small mismatch
    bool coversMonitor = 
        (windowRect.left <= monitorInfo.rcMonitor.left + tolerance) &&
        (windowRect.top <= monitorInfo.rcMonitor.top + tolerance) &&
        (windowRect.right >= monitorInfo.rcMonitor.right - tolerance) &&
        (windowRect.bottom >= monitorInfo.rcMonitor.bottom - tolerance);

    if (!coversMonitor) {
        return false;
    }

    // Check window style
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    // Fullscreen windows typically:
    // - Don't have WS_OVERLAPPEDWINDOW style (or at least no caption/thickframe)
    // - Have WS_POPUP style
    // - Often have WS_EX_TOPMOST extended style
    
    bool hasPopupStyle = (style & WS_POPUP) != 0;
    bool noCaption = (style & WS_CAPTION) != WS_CAPTION;
    bool isTopmost = (exStyle & WS_EX_TOPMOST) != 0;

    // Consider it fullscreen if it covers the monitor and has appropriate styles
    // Relaxed check: just covering monitor + popup OR no caption is usually enough
    return coversMonitor && (hasPopupStyle || noCaption || isTopmost);
}

} // namespace PixelMotion
