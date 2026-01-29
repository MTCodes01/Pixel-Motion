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

    // --- Japanese Vintage Theme ---
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Soft Cream Background
    colors[ImGuiCol_WindowBg]       = ImVec4(0.96f, 0.95f, 0.92f, 1.00f);
    colors[ImGuiCol_ChildBg]        = ImVec4(0.96f, 0.95f, 0.92f, 1.00f);
    colors[ImGuiCol_PopupBg]        = ImVec4(0.96f, 0.95f, 0.92f, 1.00f);
    
    // Sumi Ink Text
    colors[ImGuiCol_Text]           = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Borders (Hand-drawn look)
    colors[ImGuiCol_Border]         = ImVec4(0.35f, 0.35f, 0.35f, 0.60f);
    colors[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Headers & Accents (Vermilion/Indigo)
    colors[ImGuiCol_Header]         = ImVec4(0.83f, 0.35f, 0.33f, 0.20f); // Light Vermilion
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.83f, 0.35f, 0.33f, 0.40f);
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.83f, 0.35f, 0.33f, 0.60f);

    // Buttons
    colors[ImGuiCol_Button]         = ImVec4(0.90f, 0.88f, 0.85f, 1.00f); // Slightly darker cream
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.83f, 0.35f, 0.33f, 0.20f); // Vermilion tint
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.83f, 0.35f, 0.33f, 0.50f);

    // Frame/Inputs
    colors[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.60f); // White-ish
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.80f);
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]    = ImVec4(0.96f, 0.95f, 0.92f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.35f, 0.35f, 0.35f, 0.30f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 0.50f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.35f, 0.35f, 0.35f, 0.70f);

    // Checkmark/Slider
    colors[ImGuiCol_CheckMark]      = ImVec4(0.23f, 0.34f, 0.51f, 1.00f); // Indigo
    colors[ImGuiCol_SliderGrab]     = ImVec4(0.23f, 0.34f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.23f, 0.34f, 0.51f, 1.00f);

    // Style Rules
    style.WindowRounding    = 0.0f; // Sharp corners for paper feel
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 0.0f;
    style.GrabRounding      = 0.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.ItemSpacing       = ImVec2(8, 6);
    style.WindowPadding     = ImVec2(12, 12);

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
    const float clear_color_with_alpha[4] = { 0.96f, 0.95f, 0.92f, 1.00f };
    m_context->OMSetRenderTargets(1, m_mainRenderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_mainRenderTargetView.Get(), clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0); // Enable VSync
}

void SettingsWindow::DrawUI() {
    // Custom Style Constants
    const ImU32 COL_BG          = IM_COL32(244, 241, 234, 255); // Cream Paper
    const ImU32 COL_TEXT        = IM_COL32(43, 43, 43, 255);    // Sumi Ink
    const ImU32 COL_ACCENT_RED  = IM_COL32(211, 89, 85, 255);   // Vermilion
    const ImU32 COL_ACCENT_BLUE = IM_COL32(58, 86, 131, 255);   // Indigo
    const ImU32 COL_SIDEBAR     = IM_COL32(40, 44, 52, 255);    // Dark slate/Indigo mix
    const ImU32 COL_BORDER      = IM_COL32(80, 70, 60, 100);    // Soft brown border
    
    // Setup Window
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    // No decorations, we draw our own
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
    
    if (ImGui::Begin("Settings", nullptr, window_flags)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();

        // --- 1. Draw Washi Background ---
        // Solid Cream Base
        draw_list->AddRectFilled(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y), COL_BG);
        
        // Subtle Noise/Grain (Simulated with lines or dots for now)
        // In a real shader we'd use a texture, but here we can add a subtle heavy border
        draw_list->AddRect(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y), COL_BORDER, 0.0f, 0, 2.0f);

        // --- 2. Custom Layout: Sidebar (Obi) ---
        float sidebar_width = 200.0f;
        ImVec2 sidebar_p1 = win_pos;
        ImVec2 sidebar_p2 = ImVec2(win_pos.x + sidebar_width, win_pos.y + win_size.y);
        
        // Draw Sidebar Background (Indigo/Slate)
        draw_list->AddRectFilled(sidebar_p1, sidebar_p2, COL_SIDEBAR);
        
        // --- 3. Custom Title Bar Area ---
        float title_height = 50.0f;
        
        // Draggable Area Logic (Simulated)
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##TitleBarDrag", ImVec2(win_size.x - 40, title_height)); // Leave room for close button
        if (ImGui::IsItemActive()) {
            // Send drag message to OS window
            SendMessage(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // Draw App Title in Sidebar
        draw_list->AddText(ImGui::GetFont(), 24.0f, ImVec2(win_pos.x + 20, win_pos.y + 25), IM_COL32(255, 255, 255, 255), "Pixel Motion");
        
        // --- 4. Sidebar Content ---
        ImGui::SetCursorPos(ImVec2(10, 80));
        ImGui::BeginGroup();
            
            // Monitor List in Sidebar
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
            ImGui::Text("SCREENS");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            if (m_monitorManager) {
                int monitorCount = m_monitorManager->GetMonitorCount();
                for (int i = 0; i < monitorCount; i++) {
                    char label[32];
                    sprintf_s(label, "Monitor %d", i + 1);
                    
                    bool is_selected = (m_selectedMonitorIndex == i);
                    
                    // Custom Sidebar Button Style
                    ImGui::PushStyleColor(ImGuiCol_Button, is_selected ? COL_ACCENT_RED : IM_COL32(0,0,0,0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 20));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, COL_ACCENT_RED);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
                    
                    if (ImGui::Button(label, ImVec2(180, 40))) {
                        m_selectedMonitorIndex = i;
                        LoadSettingsForMonitor(i);
                    }
                    
                    ImGui::PopStyleColor(4);
                }
            }
        ImGui::EndGroup();

        // --- 5. Main Content Area ---
        ImGui::SetCursorPos(ImVec2(sidebar_width + 40, 60));
        ImGui::BeginGroup();
            
            // Header
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]); // Use default font for now, scaled up if possible
            ImGui::TextColored(ImVec4(0.16f, 0.16f, 0.16f, 1.0f), "SETTINGS");
            ImGui::PopFont(); // FIX: Added missing PopFont
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            if (m_monitorManager && m_selectedMonitorIndex >= 0) {
                
                // Helper for Hanko Label
                auto SectionTitle = [&](const char* text) {
                    ImGui::TextColored(ImVec4(0.83f, 0.35f, 0.33f, 1.0f), text);
                    // Draw a small line under?
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    draw_list->AddLine(ImVec2(p.x, p.y - 2), ImVec2(p.x + 20, p.y - 2), COL_ACCENT_RED, 2.0f);
                };

                // Wallpaper Section
                SectionTitle("WALLPAPER");
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0,0,0,0)); // Transparent bg for input
                ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT);
                
                // Custom Input Look: Just text with a bottom border drawn manually?
                // For now, standard input but styled cleanly
                ImGui::SetNextItemWidth(300);
                ImGui::InputText("##Path", m_wallpaperPathBuffer, MAX_PATH, ImGuiInputTextFlags_ReadOnly);
                
                // Underline the input
                ImVec2 input_rect_min = ImGui::GetItemRectMin();
                ImVec2 input_rect_max = ImGui::GetItemRectMax();
                draw_list->AddLine(ImVec2(input_rect_min.x, input_rect_max.y), ImVec2(input_rect_max.x, input_rect_max.y), COL_TEXT, 1.0f);

                ImGui::SameLine();
                
                // Hanko Button (Browse)
                ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT_RED);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,255));
                if (ImGui::Button("BROWSE", ImVec2(80, 0))) {
                     std::wstring path = OpenFileDialog();
                     if (!path.empty()) {
                        WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, m_wallpaperPathBuffer, MAX_PATH, nullptr, nullptr);
                     }
                }
                ImGui::PopStyleColor(4); // Pop FrameBg, Text, Button, Text
                ImGui::PopStyleVar();

                ImGui::Spacing();
                ImGui::Spacing();

                // Options Section
                SectionTitle("BEHAVIOR");
                // Custom Checkbox Style? ImGui's default with our colors is actually okay, 
                // but let's just use standard for functionality with the new palette.
                // The global style we set in InitializeImGui handles the colors (Indigo checkmark).
                
                ImGui::Checkbox("Start with Windows", &m_autoStart);
                ImGui::Checkbox("Pause on Battery", &m_batteryPause);
                ImGui::Checkbox("Pause on Fullscreen", &m_fullscreenPause);

                ImGui::Spacing();
                ImGui::Spacing();
                
                SectionTitle("BLOCKLIST");
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Pause when these apps are focused:");
                
                ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 255, 255, 150));
                if (ImGui::BeginListBox("##AppList", ImVec2(400, 150))) {
                    for (int i = 0; i < m_blockedApps.size(); i++) {
                         const std::string& app = m_blockedApps[i];
                         if (ImGui::Selectable(app.c_str(), m_selectedAppIndex == i)) {
                             m_selectedAppIndex = i;
                         }
                    }
                    ImGui::EndListBox();
                }
                ImGui::PopStyleColor();
                
                ImGui::Spacing();
                
                // Action Buttons
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(230, 230, 230, 255));
                ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT);
                if (ImGui::Button("+ ADD", ImVec2(80, 30))) {
                     std::wstring exePath = OpenFileDialog();
                     // ... (omitted logic for brevity, assuming copy paste of logic or same file diff)
                     // RE-INSERTING LOGIC CAREFULLY
                     if (!exePath.empty()) {
                         size_t lastSlash = exePath.find_last_of(L"\\/");
                         if (lastSlash != std::wstring::npos) {
                             std::wstring filename = exePath.substr(lastSlash + 1);
                             int len = WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, nullptr, 0, nullptr, nullptr);
                             if (len > 0) {
                                 std::string asciiFilename(len, '\0');
                                 WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, &asciiFilename[0], len, nullptr, nullptr);
                                 asciiFilename.resize(len - 1);
                                 bool exists = false;
                                 for(const auto& existing : m_blockedApps) { if (_stricmp(existing.c_str(), asciiFilename.c_str()) == 0) exists = true; }
                                 if (!exists) m_blockedApps.push_back(asciiFilename);
                             }
                         }
                     }
                }
                ImGui::SameLine();
                if (ImGui::Button("- REMOVE", ImVec2(80, 30))) {
                    if (m_selectedAppIndex >= 0 && m_selectedAppIndex < m_blockedApps.size()) {
                        m_blockedApps.erase(m_blockedApps.begin() + m_selectedAppIndex);
                        m_selectedAppIndex = -1;
                    }
                }
                ImGui::PopStyleColor(2);

            } else {
                ImGui::Text("Select a monitor to configure.");
            }

        ImGui::EndGroup();
        
        // --- 6. Footer / Close ---
        // Floating "Close" Hanko at bottom right
        ImGui::SetCursorPos(ImVec2(win_size.x - 100, win_size.y - 60));
        ImGui::PushStyleColor(ImGuiCol_Button, COL_ACCENT_RED);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,255,255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f); // Round circle/capsule
        
        if (ImGui::Button("SAVE", ImVec2(80, 40))) {
            ApplySettings();
            Hide();
        }
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        
        // Close X at top right
        ImGui::SetCursorPos(ImVec2(win_size.x - 40, 10));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
        if (ImGui::Button("X", ImVec2(30, 30))) {
            Hide();
        }
        ImGui::PopStyleColor();

    }
    ImGui::End();
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
