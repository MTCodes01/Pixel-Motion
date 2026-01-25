#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include <memory>

namespace PixelMotion {

class Configuration;
class MonitorManager;

/**
 * Settings dialog window
 * Provides UI for configuring wallpapers, monitors, and resource management
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

    void SetConfiguration(Configuration* config);
    void SetMonitorManager(MonitorManager* monitorMgr);

private:
    // Dialog procedure
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Message handlers
    INT_PTR OnInitDialog(HWND hwnd);
    INT_PTR OnCommand(WPARAM wParam, LPARAM lParam);
    INT_PTR OnNotify(WPARAM wParam, LPARAM lParam);
    INT_PTR OnClose();
    
    // UI update methods
    void PopulateMonitorList();
    void UpdateWallpaperPath();
    void UpdateScalingMode();
    void UpdateResourceSettings();
    void UpdateControlStates();
    
    // Event handlers
    void OnMonitorSelectionChanged();
    void OnBrowseWallpaper();
    void OnScalingChanged();
    void OnApply();
    void OnCancel();
    
    // Helper methods
    std::wstring OpenFileDialog();
    int GetSelectedMonitorIndex();
    void SaveCurrentMonitorSettings();
    void LoadMonitorSettings(int monitorIndex);
    
    HWND m_hwnd;
    bool m_initialized;
    bool m_settingsChanged;
    
    // References to application components
    Configuration* m_config;
    MonitorManager* m_monitorManager;
    
    // Current selection
    int m_currentMonitorIndex;
    
    // Temporary settings (before Apply)
    struct MonitorSettings {
        std::wstring wallpaperPath;
        int scalingMode; // 0=Fill, 1=Fit, 2=Stretch, 3=Center
    };
    std::vector<MonitorSettings> m_tempSettings;
};

} // namespace PixelMotion
