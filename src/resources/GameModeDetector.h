#pragma once

#include <Windows.h>
#include <string>
#include <vector>
#include <Psapi.h>

namespace PixelMotion {

/**
 * Game Mode detector
 * Detects fullscreen applications using window heuristics
 */
class GameModeDetector {
public:
    GameModeDetector();
    ~GameModeDetector();

    bool Initialize();
    void Shutdown();

    void Update();

    bool IsFullscreenAppActive() const { return m_fullscreenDetected; }
    
    void SetProcessBlocklist(const std::vector<std::string>& blocklist) { m_processBlocklist = blocklist; }

private:
    bool IsWindowFullscreen(HWND hwnd);
    std::string GetProcessName(HWND hwnd); // Helper for internal use

    bool m_fullscreenDetected;
    HWND m_lastForegroundWindow;
    bool m_initialized;
    
    // Hysteresis
    bool m_pendingState = false;
    int m_consecutiveFrames = 0;
    
    std::vector<std::string> m_processBlocklist;
};

} // namespace PixelMotion
