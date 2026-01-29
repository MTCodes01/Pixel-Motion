#include "DesktopManager.h"
#include "WallpaperWindow.h"
#include "MonitorInfo.h"
#include "core/Logger.h"
#include "core/Configuration.h"

#include <algorithm>

namespace PixelMotion {

// Undocumented message to spawn WorkerW
constexpr UINT WM_SPAWN_WORKER = 0x052C;

DesktopManager::DesktopManager()
    : m_progman(nullptr)
    , m_workerW(nullptr)
    , m_config(nullptr)
    , m_initialized(false)
{
}

DesktopManager::~DesktopManager() {
    Shutdown();
}

bool DesktopManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing Desktop Manager...");

    // Find WorkerW window
    if (!FindWorkerW()) {
        Logger::Error("Failed to find WorkerW window");
        return false;
    }

    // Create wallpaper windows for each monitor
    if (!CreateWallpaperWindows()) {
        Logger::Error("Failed to create wallpaper windows");
        return false;
    }

    m_initialized = true;
    Logger::Info("Desktop Manager initialized successfully");
    return true;
}

void DesktopManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("Shutting down Desktop Manager...");
    
    DestroyWallpaperWindows();
    
    m_workerW = nullptr;
    m_progman = nullptr;
    m_initialized = false;
}

bool DesktopManager::FindWorkerW() {
    // Step 1: Find Progman window
    m_progman = FindWindow(L"Progman", nullptr);
    if (!m_progman) {
        Logger::Error("Failed to find Progman window");
        return false;
    }

    Logger::Info("Found Progman window");

    // Step 2: Send undocumented message to spawn WorkerW
    SendMessageTimeout(m_progman, WM_SPAWN_WORKER, 0xD, 0x1,
        SMTO_NORMAL, 1000, nullptr);

    // Step 3: Enumerate windows to find WorkerW behind SHELLDLL_DefView
    m_workerW = nullptr;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(this));

    if (!m_workerW) {
        Logger::Error("Failed to find WorkerW window");
        return false;
    }

    Logger::Info("Found WorkerW window");
    return true;
}

BOOL CALLBACK DesktopManager::EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* manager = reinterpret_cast<DesktopManager*>(lParam);

    // Find WorkerW window that has SHELLDLL_DefView as a child
    HWND defView = FindWindowEx(hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr) {
        // This WorkerW contains the desktop icons (SHELLDLL_DefView)
        // We want to use THIS WorkerW as our parent
        manager->m_workerW = hwnd;
        
        Logger::Info("Found WorkerW with SHELLDLL_DefView, HWND: " + 
                     std::to_string(reinterpret_cast<uintptr_t>(hwnd)));
        
        return FALSE; // Stop enumeration
    }

    return TRUE; // Continue enumeration
}

bool DesktopManager::CreateWallpaperWindows() {
    // Get all monitors
    std::vector<MonitorInfo> monitors = MonitorInfo::EnumerateMonitors();
    
    if (monitors.empty()) {
        Logger::Error("No monitors found");
        return false;
    }

    Logger::Info("Creating wallpaper windows for " + std::to_string(monitors.size()) + " monitor(s)");

    // Create a wallpaper window for each monitor
    for (const auto& monitor : monitors) {
        auto wallpaperWindow = std::make_unique<WallpaperWindow>();
        
        if (!wallpaperWindow->Create(m_workerW, monitor)) {
            std::wstring wDeviceName = monitor.deviceName;
            std::string deviceName(wDeviceName.begin(), wDeviceName.end());
            Logger::Error("Failed to create wallpaper window for monitor: " + deviceName);
            continue;
        }

        m_wallpaperWindows.push_back(std::move(wallpaperWindow));
    }

    return !m_wallpaperWindows.empty();
}

void DesktopManager::DestroyWallpaperWindows() {
    m_wallpaperWindows.clear();
}

bool DesktopManager::SetWallpaper(int monitorIndex, const std::wstring& videoPath) {
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(m_wallpaperWindows.size())) {
        Logger::Error("Invalid monitor index: " + std::to_string(monitorIndex));
        return false;
    }

    std::wstring wPath = videoPath;
    std::string path(wPath.begin(), wPath.end());
    Logger::Info("Setting wallpaper for monitor " + std::to_string(monitorIndex) + ": " + path);

    auto* window = m_wallpaperWindows[monitorIndex].get();
    
    // Get scaling mode from configuration
    if (m_config) {
        const auto& monitor = window->GetMonitor();
        auto* monitorConfig = m_config->GetMonitorConfig(monitor.deviceName);
        if (monitorConfig) {
            int scalingMode = monitorConfig->scalingMode;
            window->SetScalingMode(scalingMode);
            Logger::Info("Applied scaling mode: " + std::to_string(scalingMode));
        }
    }

    return window->LoadVideo(videoPath);
}

void DesktopManager::RestoreWallpapers() {
    if (!m_config) return;

    Logger::Info("Restoring saved wallpapers...");

    for (size_t i = 0; i < m_wallpaperWindows.size(); i++) {
        auto* window = m_wallpaperWindows[i].get();
        const auto& monitor = window->GetMonitor();
        
        auto* monitorConfig = m_config->GetMonitorConfig(monitor.deviceName);
        if (monitorConfig) {
            // Apply scaling mode regardless of wallpaper (in case they want to set it before invalid wallpaper)
            window->SetScalingMode(monitorConfig->scalingMode);

            // Restore wallpaper if enabled and path exists
            if (monitorConfig->enabled && !monitorConfig->wallpaperPath.empty()) {
                if (std::filesystem::exists(monitorConfig->wallpaperPath)) {
                    // Use SetWallpaper to handle logging and any future logic
                    SetWallpaper(static_cast<int>(i), monitorConfig->wallpaperPath);
                } else {
                    std::string pathUtf8(monitorConfig->wallpaperPath.begin(), monitorConfig->wallpaperPath.end());
                    Logger::Warning("Saved wallpaper path not found: " + pathUtf8);
                }
            }
        }
    }
}

void DesktopManager::Update() {
    // Check if WorkerW still exists (Windows might recreate it)
    if (!IsWindow(m_workerW)) {
        Logger::Warning("WorkerW window lost, attempting to reattach...");
        
        DestroyWallpaperWindows();
        
        if (FindWorkerW()) {
            CreateWallpaperWindows();
        }
    }

    // Update each wallpaper window
    for (auto& window : m_wallpaperWindows) {
        window->Update();
    }
}

double DesktopManager::GetTimeToNextUpdate() const {
    double minTime = 1.0; // Default max wait
    
    for (const auto& window : m_wallpaperWindows) {
        double t = window->GetTimeToNextFrame();
        if (t < minTime) {
            minTime = t;
        }
    }
    
    return minTime;
}

void DesktopManager::Render() {
    // Render each wallpaper window
    for (auto& window : m_wallpaperWindows) {
        if (window->NeedsRepaint()) {
            window->Render();
        }
    }
}

} // namespace PixelMotion
