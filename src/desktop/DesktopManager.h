#pragma once

#include <Windows.h>
#include <vector>
#include <memory>
#include <string>

namespace PixelMotion {

class WallpaperWindow;
class RendererContext;

/**
 * Manages Windows desktop integration
 * Handles WorkerW discovery and wallpaper window attachment
 */
class DesktopManager {
public:
    DesktopManager();
    ~DesktopManager();

    bool Initialize();
    void Shutdown();

    bool SetWallpaper(int monitorIndex, const std::wstring& videoPath);

    void Update();
    void Render();

private:
    bool FindWorkerW();
    bool CreateWallpaperWindows();
    void DestroyWallpaperWindows();
    
    static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);

    HWND m_progman;
    HWND m_workerW;
    
    std::vector<std::unique_ptr<WallpaperWindow>> m_wallpaperWindows;
    bool m_initialized;
};

} // namespace PixelMotion
