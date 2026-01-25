#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace PixelMotion {

class Configuration;
class MonitorManager;
class DesktopManager;

/**
 * Modern ImGui-based Settings Window
 */
class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    bool Initialize();
    void Shutdown();
    
    void Show();
    void Hide();
    bool IsVisible() const;
    
    void Render(); // Called every frame when visible

    void SetConfiguration(Configuration* config);
    void SetMonitorManager(MonitorManager* monitorMgr);
    void SetDesktopManager(DesktopManager* desktopMgr);

    // Window Procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool InitializeDX11();
    bool InitializeImGui();
    void CleanupDX11();
    
    void DrawUI(); // Draw ImGui widgets
    void ApplySettings(); // Save and apply changes
    void LoadSettingsForMonitor(int monitorIndex);
    
    // Windows helpers
    std::wstring OpenFileDialog();

private:
    HWND m_hwnd;
    bool m_visible;
    bool m_initialized;
    
    // Dependencies
    Configuration* m_config;
    MonitorManager* m_monitorManager;
    DesktopManager* m_desktopManager;
    
    // DX11 Resources
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_mainRenderTargetView;
    
    // UI State
    int m_selectedMonitorIndex;
    char m_wallpaperPathBuffer[MAX_PATH];
    const char* m_monitorNames[10]; // Simplified for now
    std::vector<std::string> m_monitorIds;
    bool m_batteryPause;
    bool m_fullscreenPause;
};

} // namespace PixelMotion
