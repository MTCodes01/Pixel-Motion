#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <string>

namespace PixelMotion {

// Forward declaration
class ResourceManager;

/**
 * System tray icon
 * Provides minimal UI for controlling the application
 */
class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    bool Initialize();
    void Shutdown();

    void UpdateIcon();
    void ShowNotification(const std::wstring& title, const std::wstring& message);


    void SetResourceManager(ResourceManager* resourceMgr) { m_resourceManager = resourceMgr; }
    bool IsPaused() const { return m_paused; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void ShowContextMenu();
    void OnCommand(UINT command);

    HWND m_hwnd;
    NOTIFYICONDATA m_nid;
    bool m_initialized;

    static const UINT WM_TRAYICON = WM_USER + 1;
    static const UINT CMD_PAUSE = 1001;
    static const UINT CMD_SETTINGS = 1002;
    static const UINT CMD_EXIT = 1003;

    // Dependencies
    ResourceManager* m_resourceManager;
    bool m_paused;
};

} // namespace PixelMotion
