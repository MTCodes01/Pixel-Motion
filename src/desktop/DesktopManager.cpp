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
    // Step 1: Find Progman window (or fallback to GetShellWindow)
    m_progman = FindWindow(L"Progman", nullptr);
    if (!m_progman) {
        m_progman = GetShellWindow();
    }

    if (m_progman) {
        Logger::Info("Found Progman/Shell window");

        // Step 2: Send message to spawn WorkerW
        SendMessageTimeout(m_progman, WM_SPAWN_WORKER, 0xD, 0x1,
            SMTO_NORMAL, 1000, nullptr);
    } else {
        Logger::Warning("Progman not found via FindWindow/GetShellWindow, checking existing WorkerW windows...");
    }

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
            std::string deviceName;
            int len = WideCharToMultiByte(CP_UTF8, 0, monitor.deviceName.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (len > 1) {
                deviceName.resize(len - 1);
                WideCharToMultiByte(CP_UTF8, 0, monitor.deviceName.c_str(), -1, &deviceName[0], len, nullptr, nullptr);
            }
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

RECT DesktopManager::GetVirtualDesktopBounds() const {
    RECT vRect = { 0, 0, 0, 0 };
    if (!m_wallpaperWindows.empty()) {
        vRect = m_wallpaperWindows[0]->GetMonitor().bounds;
        for (size_t i = 1; i < m_wallpaperWindows.size(); i++) {
            const RECT& b = m_wallpaperWindows[i]->GetMonitor().bounds;
            vRect.left = std::min(vRect.left, b.left);
            vRect.top = std::min(vRect.top, b.top);
            vRect.right = std::max(vRect.right, b.right);
            vRect.bottom = std::max(vRect.bottom, b.bottom);
        }
    } else {
        vRect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        vRect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        vRect.right = vRect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        vRect.bottom = vRect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
    return vRect;
}

void DesktopManager::SetScalingMode(int monitorIndex, int mode) {
    if (mode == 5) { // Span
        SetWallpaperSpan(L"");
        return;
    }

    if (monitorIndex >= 0 && monitorIndex < static_cast<int>(m_wallpaperWindows.size())) {
        m_wallpaperWindows[monitorIndex]->SetVirtualDesktopBounds(GetVirtualDesktopBounds());
        m_wallpaperWindows[monitorIndex]->SetScalingMode(mode);

        // If any OTHER monitor is still in Span mode (5), it cannot span alone.
        // Transition it out of Span mode so it doesn't display a cut-off half image.
        for (size_t i = 0; i < m_wallpaperWindows.size(); i++) {
            if (static_cast<int>(i) != monitorIndex) {
                if (m_config) {
                    const auto& mon = m_wallpaperWindows[i]->GetMonitor();
                    auto* cfg = m_config->GetMonitorConfig(mon.deviceName);
                    if (cfg && cfg->scalingMode == 5) {
                        cfg->scalingMode = mode;
                        m_wallpaperWindows[i]->SetScalingMode(mode);
                    }
                }
            }
        }
    }
}

void DesktopManager::SetWallpaperSpan(const std::wstring& videoPath) {
    Logger::Info("Applying spanned wallpaper across all monitors");
    RECT vRect = GetVirtualDesktopBounds();
    for (auto& window : m_wallpaperWindows) {
        window->SetVirtualDesktopBounds(vRect);
        window->SetScalingMode(5);
        if (!videoPath.empty()) {
            window->LoadVideo(videoPath);
        }
    }
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

bool DesktopManager::SetWallpaper(int monitorIndex, const std::wstring& videoPath) {
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(m_wallpaperWindows.size())) {
        Logger::Error("Invalid monitor index: " + std::to_string(monitorIndex));
        return false;
    }

    std::string path;
    int pathLen = WideCharToMultiByte(CP_UTF8, 0, videoPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (pathLen > 1) {
        path.resize(pathLen - 1);
        WideCharToMultiByte(CP_UTF8, 0, videoPath.c_str(), -1, &path[0], pathLen, nullptr, nullptr);
    }
    Logger::Info("Setting wallpaper for monitor " + std::to_string(monitorIndex) + ": " + path);

    auto* window = m_wallpaperWindows[monitorIndex].get();
    
    // Get scaling mode from configuration
    int scalingMode = 0;
    if (m_config) {
        const auto& monitor = window->GetMonitor();
        auto* monitorConfig = m_config->GetMonitorConfig(monitor.deviceName);
        if (monitorConfig) {
            scalingMode = monitorConfig->scalingMode;
        }
    }

    if (scalingMode == 5) {
        SetWallpaperSpan(videoPath);
        return true;
    }

    window->SetVirtualDesktopBounds(GetVirtualDesktopBounds());
    window->SetScalingMode(scalingMode);
    Logger::Info("Applied scaling mode: " + std::to_string(scalingMode));

    bool result = window->LoadVideo(videoPath);
    if (result) {
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    }
    return result;
}

void DesktopManager::RestoreWallpapers() {
    if (!m_config) return;

    Logger::Info("Restoring saved wallpapers...");
    RECT vRect = GetVirtualDesktopBounds();

    for (size_t i = 0; i < m_wallpaperWindows.size(); i++) {
        auto* window = m_wallpaperWindows[i].get();
        window->SetVirtualDesktopBounds(vRect);
        const auto& monitor = window->GetMonitor();
        
        auto* monitorConfig = m_config->GetMonitorConfig(monitor.deviceName);
        if (monitorConfig) {
            window->SetScalingMode(monitorConfig->scalingMode);

            // Restore wallpaper if enabled and path exists
            if (monitorConfig->enabled && !monitorConfig->wallpaperPath.empty()) {
                if (std::filesystem::exists(monitorConfig->wallpaperPath)) {
                    if (monitorConfig->scalingMode == 5) {
                        SetWallpaperSpan(monitorConfig->wallpaperPath);
                        break; // Spanned wallpaper applies to all monitors
                    } else {
                        SetWallpaper(static_cast<int>(i), monitorConfig->wallpaperPath);
                    }
                } else {
                    std::string pathUtf8;
                    int pLen = WideCharToMultiByte(CP_UTF8, 0, monitorConfig->wallpaperPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (pLen > 1) {
                        pathUtf8.resize(pLen - 1);
                        WideCharToMultiByte(CP_UTF8, 0, monitorConfig->wallpaperPath.c_str(), -1, &pathUtf8[0], pLen, nullptr, nullptr);
                    }
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
            // Restore wallpapers after reattach
            RestoreWallpapers();
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
