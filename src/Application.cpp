#include "Application.h"
#include "core/Logger.h"
#include "core/Configuration.h"
#include "desktop/DesktopManager.h"
#include "desktop/MonitorManager.h"
#include "resources/ResourceManager.h"
#include "ui/TrayIcon.h"
#include "ui/SettingsWindow.h"

#include <Windows.h>
#include <objbase.h>

namespace PixelMotion {

Application* Application::s_instance = nullptr;

Application::Application()
    : m_running(false)
    , m_initialized(false)
{
    s_instance = this;
}

Application::~Application() {
    Shutdown();
    s_instance = nullptr;
}

Application& Application::GetInstance() {
    return *s_instance;
}

bool Application::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing Pixel Motion...");

    // Initialize COM for shell integration
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        Logger::Error("Failed to initialize COM");
        return false;
    }

    // Load configuration
    m_config = std::make_unique<Configuration>();
    if (!m_config->Load()) {
        Logger::Warning("Failed to load configuration, using defaults");
    }

    // Initialize subsystems
    if (!InitializeSubsystems()) {
        Logger::Error("Failed to initialize subsystems");
        return false;
    }

    m_initialized = true;
    Logger::Info("Pixel Motion initialized successfully");
    return true;
}

bool Application::InitializeSubsystems() {
    // Monitor manager (enumerate displays)
    m_monitorManager = std::make_unique<MonitorManager>();
    if (!m_monitorManager->Initialize()) {
        Logger::Error("Failed to initialize monitor manager");
        return false;
    }

    // Desktop manager (WorkerW integration)
    m_desktopManager = std::make_unique<DesktopManager>();
    if (!m_desktopManager->Initialize()) {
        Logger::Error("Failed to initialize desktop manager");
        return false;
    }

    // Resource manager (Game Mode, Battery-Aware)
    m_resourceManager = std::make_unique<ResourceManager>();
    if (!m_resourceManager->Initialize()) {
        Logger::Error("Failed to initialize resource manager");
        return false;
    }
    Logger::Info("Resource Manager initialized successfully");

    // System tray UI
    m_trayIcon = std::make_unique<TrayIcon>();
    if (!m_trayIcon->Initialize()) {
        Logger::Error("Failed to initialize tray icon");
        return false;
    }
    Logger::Info("Tray Icon initialized successfully");

    // Settings window
    m_settingsWindow = std::make_unique<SettingsWindow>();
    if (!m_settingsWindow->Initialize()) {
        Logger::Error("Failed to initialize settings window");
        return false;
    }
    Logger::Info("Settings Window initialized successfully");

    // Connect settings window to configuration and monitor manager
    m_settingsWindow->SetConfiguration(m_config.get());
    m_settingsWindow->SetMonitorManager(m_monitorManager.get());
    Logger::Info("All subsystems connected successfully");

    return true;
}

int Application::Run() {
    if (!m_initialized) {
        Logger::Error("Application not initialized");
        return -1;
    }

    m_running = true;
    Logger::Info("Entering main loop");

    MSG msg = {};
    while (m_running) {
        // Process Windows messages
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!m_running) break;

        // Update subsystems
        Update();

        // Render wallpapers
        Render();

        // Yield to prevent 100% CPU usage
        Sleep(1);
    }

    Logger::Info("Exiting main loop");
    return static_cast<int>(msg.wParam);
}

void Application::Update() {
    // Update resource manager (check for fullscreen apps, battery status)
    if (m_resourceManager) {
        m_resourceManager->Update();
    }

    // Update desktop manager (handle monitor changes)
    if (m_desktopManager) {
        m_desktopManager->Update();
    }
}

void Application::Render() {
    // Render is handled by individual monitor renderers in DesktopManager
    if (m_desktopManager && !m_resourceManager->IsPaused()) {
        m_desktopManager->Render();
    }
}

void Application::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("Shutting down Pixel Motion...");

    m_running = false;

    // Shutdown in reverse order
    m_trayIcon.reset();
    m_resourceManager.reset();
    m_desktopManager.reset();

    // Save configuration
    if (m_config) {
        m_config->Save();
        m_config.reset();
    }

    CoUninitialize();

    m_initialized = false;
    Logger::Info("Shutdown complete");
}

void Application::RequestExit() {
    Logger::Info("Exit requested");
    m_running = false;
    PostQuitMessage(0);
}

void Application::ShowSettings() {
    if (m_settingsWindow) {
        m_settingsWindow->Show();
    }
}

} // namespace PixelMotion
