#pragma once

#include "MonitorInfo.h"
#include <Windows.h>
#include <memory>
#include <string>
#include <chrono>

namespace PixelMotion {

class RendererContext;
class VideoDecoder;

/**
 * Per-monitor wallpaper window
 * Attached as child to WorkerW
 */
class WallpaperWindow {
public:
    WallpaperWindow();
    ~WallpaperWindow();

    bool Create(HWND parentWorkerW, const MonitorInfo& monitor);
    void Destroy();

    bool LoadVideo(const std::wstring& videoPath);
    void UnloadVideo();

    void Update();
    void Render();

    void SetScalingMode(int mode); // 0=Fill, 1=Fit, 2=Stretch, 3=Center

    HWND GetHandle() const { return m_hwnd; }
    const MonitorInfo& GetMonitor() const { return m_monitor; }
    bool HasVideo() const { return m_videoDecoder != nullptr; }
    
    // Optimization methods
    bool NeedsRepaint() const { return m_needsRepaint; }
    double GetTimeToNextFrame() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool RegisterWindowClass();

    HWND m_hwnd;
    HWND m_parent;
    MonitorInfo m_monitor;
    std::unique_ptr<RendererContext> m_renderer;
    std::unique_ptr<VideoDecoder> m_videoDecoder;

    // Video playback timing
    std::chrono::steady_clock::time_point m_lastFrameTime;
    double m_frameInterval; // Time between frames in seconds
    bool m_needsRepaint;

    static const wchar_t* s_className;
    static bool s_classRegistered;
};

} // namespace PixelMotion
