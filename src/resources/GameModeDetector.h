#pragma once

#include <Windows.h>

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

private:
    bool IsWindowFullscreen(HWND hwnd);

    bool m_fullscreenDetected;
    HWND m_lastForegroundWindow;
    bool m_initialized;
};

} // namespace PixelMotion
