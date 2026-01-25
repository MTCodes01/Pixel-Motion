#pragma once

#include <memory>
#include <string>

namespace PixelMotion {

// Forward declarations
class DesktopManager;
class MonitorManager;
class ResourceManager;
class TrayIcon;
class Configuration;
class SettingsWindow;


/**
 * Main application controller
 * Manages lifecycle and coordinates all subsystems
 */
class Application {
public:
    Application();
    ~Application();

    // Non-copyable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /**
     * Initialize all subsystems
     * @return true if successful
     */
    bool Initialize();

    /**
     * Main application run loop
     * @return exit code
     */
    int Run();

    /**
     * Graceful shutdown
     */
    void Shutdown();

    /**
     * Get singleton instance
     */
    static Application& GetInstance();

    /**
     * Request application exit
     */
    void RequestExit();

    /**
     * Show settings window
     */
    void ShowSettings();

    /**
     * Get desktop manager for wallpaper control
     */
    DesktopManager* GetDesktopManager() { return m_desktopManager.get(); }

private:
    bool InitializeSubsystems();
    void ProcessMessages();
    void Update();
    void Render();

    std::unique_ptr<Configuration> m_config;
    std::unique_ptr<DesktopManager> m_desktopManager;
    std::unique_ptr<MonitorManager> m_monitorManager;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<TrayIcon> m_trayIcon;
    std::unique_ptr<SettingsWindow> m_settingsWindow;


    bool m_running;
    bool m_initialized;

    static Application* s_instance;
};

} // namespace PixelMotion
