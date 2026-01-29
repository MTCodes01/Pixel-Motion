#include "core/Configuration.h"
#include "SettingsWindow.h"
#include "core/Logger.h"
#include "desktop/MonitorManager.h"
#include "desktop/DesktopManager.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <tchar.h>
#include <commdlg.h> // For GetOpenFileName

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace PixelMotion {

SettingsWindow::SettingsWindow()
    : m_hwnd(nullptr)
    , m_visible(false)
    , m_initialized(false)
    , m_config(nullptr)
    , m_monitorManager(nullptr)
    , m_desktopManager(nullptr)
    , m_selectedMonitorIndex(0)
    , m_batteryPause(true)
    , m_fullscreenPause(true)
    , m_scalingMode(0)
    , m_selectedAppIndex(-1)
{
    memset(m_wallpaperPathBuffer, 0, sizeof(m_wallpaperPathBuffer));
}

SettingsWindow::~SettingsWindow() {
    Shutdown();
}

void SettingsWindow::SetConfiguration(Configuration* config) {
    m_config = config;
}

void SettingsWindow::SetMonitorManager(MonitorManager* monitorMgr) {
    m_monitorManager = monitorMgr;
}

void SettingsWindow::SetDesktopManager(DesktopManager* desktopMgr) {
    m_desktopManager = desktopMgr;
}

void SettingsWindow::Shutdown() {
    if (m_initialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        CleanupDX11();
        
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            UnregisterClass(_T("PixelMotionSettings"), GetModuleHandle(nullptr));
            m_hwnd = nullptr;
        }
        
        m_initialized = false;
    }
}

bool SettingsWindow::Initialize() {
    if (m_initialized) return true;

    // Create application window
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, SettingsWindow::WindowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("PixelMotionSettings"), nullptr };
    RegisterClassEx(&wc);
    
    // Calculate center of screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winW = 800;
    int winH = 600;
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    m_hwnd = CreateWindow(_T("PixelMotionSettings"), _T("Pixel Motion Settings"), 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
        x, y, winW, winH, nullptr, nullptr, wc.hInstance, this);

    // Initialize Direct3D
    if (!InitializeDX11()) {
        CleanupDX11();
        UnregisterClass(_T("PixelMotionSettings"), wc.hInstance);
        return false;
    }

    // Initialize ImGui
    if (!InitializeImGui()) {
        CleanupDX11();
        UnregisterClass(_T("PixelMotionSettings"), wc.hInstance);
        return false;
    }

    m_initialized = true;
    return true;
}

bool SettingsWindow::InitializeDX11() {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_device, &featureLevel, &m_context);
    if (hr == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP if hardware not available
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_swapChain, &m_device, &featureLevel, &m_context);
    
    if (FAILED(hr)) return false;

    // Create Render Target View
    ID3D11Texture2D* pBackBuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_mainRenderTargetView);
    pBackBuffer->Release();

    return true;
}

void SettingsWindow::CleanupDX11() {
    m_mainRenderTargetView.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool SettingsWindow::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style - Dark Theme with customizations
    ImGui::StyleColorsDark();
    
    // Custom Style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.37f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.50f, 0.85f, 1.00f);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

    return true;
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    SettingsWindow* window = nullptr;
    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lParam;
        window = (SettingsWindow*)lpcs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
    } else {
        window = (SettingsWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (window) {
        switch (msg) {
        case WM_CLOSE:
            window->Hide(); // Don't destroy, just hide
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DPICHANGED:
            if (ImGui::GetIO().ConfigFlags) { // & ImGuiConfigFlags_DpiEnableScaleViewports) {
                //const RECT* suggested_rect = (RECT*)lParam;
                //SetWindowPos(hwnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
            }
            break;
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SettingsWindow::Show() {
    if (!m_initialized) Initialize();
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    m_visible = true;
    
    // Refresh monitor list on show
    // Refresh monitor list on show
    if (m_monitorManager) {
        m_monitorManager->Update();
    }
    
    // Load global settings
    if (m_config) {
        const auto& settings = m_config->GetSettings();
        m_batteryPause = settings.batteryAwareEnabled;
        m_fullscreenPause = settings.gameModeEnabled;
        m_blockedApps = settings.processBlocklist;
    }
}

void SettingsWindow::Hide() {
    ShowWindow(m_hwnd, SW_HIDE);
    m_visible = false;
}

bool SettingsWindow::IsVisible() const {
    return m_visible;
}

void SettingsWindow::Render() {
    if (!m_visible) return;

    // Handle window resize (DX11 swapchain resize) if needed
    // For now we assume fixed size or handled by DX11 init logic if we added resize handler

    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Create the UI
    DrawUI();

    // Rendering
    ImGui::Render();
    const float clear_color_with_alpha[4] = { 0.12f, 0.12f, 0.14f, 1.00f };
    m_context->OMSetRenderTargets(1, m_mainRenderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_mainRenderTargetView.Get(), clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0); // Enable VSync
}

void SettingsWindow::DrawUI() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    
    if (ImGui::Begin("Settings", nullptr, window_flags)) {
        
        // Split into two panels: Monitor List (Left) and Settings (Right)
        ImGui::Columns(2, "SettingsColumns", true);
        
        // --- Left Panel: Monitors ---
        ImGui::Text("Displays");
        ImGui::Separator();
        
        if (m_monitorManager) {
            int monitorCount = m_monitorManager->GetMonitorCount();
            
            // Sync monitor list if needed (simplified)
            m_monitorIds.resize(monitorCount);
            
            for (int i = 0; i < monitorCount; i++) {
                char label[32];
                sprintf_s(label, "Monitor %d", i + 1);
                
                if (ImGui::Selectable(label, m_selectedMonitorIndex == i)) {
                    m_selectedMonitorIndex = i;
                    LoadSettingsForMonitor(i);
                }
            }
        }
        
        ImGui::NextColumn();
        
        // --- Right Panel: Settings ---
        if (m_monitorManager && m_selectedMonitorIndex >= 0 && m_selectedMonitorIndex < m_monitorManager->GetMonitorCount()) {
            ImGui::Text("Wallpaper Settings");
            ImGui::Separator();
            
            ImGui::Spacing();
            ImGui::Text("File Path:");
            ImGui::InputText("##WallpaperPath", m_wallpaperPathBuffer, MAX_PATH, ImGuiInputTextFlags_ReadOnly);
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                std::wstring path = OpenFileDialog();
                if (!path.empty()) {
                   WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, m_wallpaperPathBuffer, MAX_PATH, nullptr, nullptr);
                   // Auto-apply on selection? Or wait for Apply button?
                   // ApplySettings(); 
                }
            }
            
            ImGui::Spacing();
            // Scaling Mode
            const char* items[] = { "Fill", "Fit", "Stretch", "Center" };
            ImGui::Combo("Scaling", &m_scalingMode, items, IM_ARRAYSIZE(items));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Performance");
            ImGui::Checkbox("Start with Windows", &m_autoStart);
            ImGui::Checkbox("Pause on Battery Power", &m_batteryPause);
            ImGui::Checkbox("Pause when Fullscreen App Detected", &m_fullscreenPause);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("App Detection (Pause when foreground)");
            
            // List box for blocked apps
            if (ImGui::BeginListBox("##AppList", ImVec2(-FLT_MIN, 100))) {
                for (int i = 0; i < m_blockedApps.size(); i++) {
                    const std::string& app = m_blockedApps[i];
                    bool isSelected = (m_selectedAppIndex == i);
                    if (ImGui::Selectable(app.c_str(), isSelected)) {
                        m_selectedAppIndex = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndListBox();
            }

            // Buttons
            if (ImGui::Button("Add .exe")) {
                std::wstring exePath = OpenFileDialog();
                if (!exePath.empty()) {
                    // Extract filename only
                    size_t lastSlash = exePath.find_last_of(L"\\/");
                    if (lastSlash != std::wstring::npos) {
                        std::wstring filename = exePath.substr(lastSlash + 1);
                        int len = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0, nullptr, nullptr);
                        if (len > 0) {
                            std::string asciiFilename(len, '\0');
                            WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, &asciiFilename[0], len, nullptr, nullptr);
                            asciiFilename.resize(len - 1);
                            
                            // Check if exists
                            bool exists = false;
                            for(const auto& existing : m_blockedApps) {
                                if (_stricmp(existing.c_str(), asciiFilename.c_str()) == 0) exists = true;
                            }
                            
                            if (!exists) {
                                m_blockedApps.push_back(asciiFilename);
                            }
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Selected")) {
                if (m_selectedAppIndex >= 0 && m_selectedAppIndex < m_blockedApps.size()) {
                    m_blockedApps.erase(m_blockedApps.begin() + m_selectedAppIndex);
                    m_selectedAppIndex = -1;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Apply", ImVec2(120, 0))) {
                ApplySettings();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                Hide();
            }
            
        } else {
            ImGui::Text("No monitor selected or detected.");
        }
        
        ImGui::End();
    }
}

void SettingsWindow::LoadSettingsForMonitor(int monitorIndex) {
    if (!m_monitorManager) return;
    auto* monitor = m_monitorManager->GetMonitor(monitorIndex);
    if (!monitor) return;
    
    // Clear buffer default
    m_wallpaperPathBuffer[0] = '\0';
    m_scalingMode = 0;

    // Load from global configuration
    if (m_config) {
        const auto& settings = m_config->GetSettings();
        auto it = settings.monitors.find(monitor->deviceName);
        if (it != settings.monitors.end()) {
            std::wstring wpath = it->second.wallpaperPath;
            WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, m_wallpaperPathBuffer, MAX_PATH, nullptr, nullptr);
            
            m_scalingMode = it->second.scalingMode;
        }
        
        // Load global settings
        m_batteryPause = settings.batteryAwareEnabled;
        m_fullscreenPause = settings.gameModeEnabled;
        m_autoStart = settings.autoStart;
    }
}

void SettingsWindow::ApplySettings() {
    if (!m_monitorManager || !m_config) return;
    
    auto* monitor = m_monitorManager->GetMonitor(m_selectedMonitorIndex);
    if (!monitor) return;
    
    // Convert UTF-8 buffer to wstring
    wchar_t wPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, m_wallpaperPathBuffer, -1, wPath, MAX_PATH);
    
    // Create monitor config
    Configuration::MonitorConfig config;
    config.wallpaperPath = wPath;
    config.enabled = true;
    config.scalingMode = m_scalingMode;
    
    // Save to configuration
    m_config->SetMonitorConfig(monitor->deviceName, config);
    
    // Save global settings
    m_config->SetGameModeEnabled(m_fullscreenPause);
    m_config->SetBatteryAwareEnabled(m_batteryPause);
    m_config->SetStartWithWindows(m_autoStart);
    m_config->SetProcessBlocklist(m_blockedApps);
    m_config->SetProcessBlocklist(m_blockedApps);
    
    // Persist to disk
    if (m_config->Save()) {
        Logger::Info("Settings saved successfully");
    } else {
        Logger::Error("Failed to save settings");
    }
    
    // Apply to desktop manager
    if (m_desktopManager && wcslen(wPath) > 0) {
        m_desktopManager->SetWallpaper(m_selectedMonitorIndex, wPath);
        Logger::Info("Wallpaper applied to monitor");
    }
}

std::wstring SettingsWindow::OpenFileDialog() {
    OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd; // Modal to settings window
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Media Files\0*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.jpg;*.jpeg;*.png;*.bmp\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileName(&ofn)) {
        return std::wstring(ofn.lpstrFile);
    }
    return std::wstring();
}
} // namespace PixelMotion
