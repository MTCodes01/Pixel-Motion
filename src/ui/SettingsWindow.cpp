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

    // --- Japanese Festival Theme ---
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Soft Pink/Purple Background
    colors[ImGuiCol_WindowBg]       = ImVec4(1.00f, 0.86f, 0.90f, 1.00f); // Soft pink
    colors[ImGuiCol_ChildBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.90f); // White cards
    colors[ImGuiCol_PopupBg]        = ImVec4(1.00f, 0.95f, 0.97f, 1.00f);
    
    // Dark Text
    colors[ImGuiCol_Text]           = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TextDisabled]   = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Borders
    colors[ImGuiCol_Border]         = ImVec4(0.77f, 0.12f, 0.23f, 0.30f); // Red tint
    colors[ImGuiCol_BorderShadow]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Headers & Accents (Festival Red)
    colors[ImGuiCol_Header]         = ImVec4(0.77f, 0.12f, 0.23f, 0.40f);
    colors[ImGuiCol_HeaderHovered]  = ImVec4(0.77f, 0.12f, 0.23f, 0.60f);
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.77f, 0.12f, 0.23f, 0.80f);

    // Buttons (Bold Red)
    colors[ImGuiCol_Button]         = ImVec4(0.77f, 0.12f, 0.23f, 1.00f);
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.87f, 0.22f, 0.33f, 1.00f);
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.67f, 0.02f, 0.13f, 1.00f);

    // Frame/Inputs
    colors[ImGuiCol_FrameBg]        = ImVec4(1.00f, 1.00f, 1.00f, 0.80f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.95f);
    colors[ImGuiCol_FrameBgActive]  = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg]    = ImVec4(1.00f, 0.86f, 0.90f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]  = ImVec4(0.77f, 0.12f, 0.23f, 0.30f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.77f, 0.12f, 0.23f, 0.50f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.77f, 0.12f, 0.23f, 0.70f);

    // Checkmark/Slider (Festival Red)
    colors[ImGuiCol_CheckMark]      = ImVec4(0.77f, 0.12f, 0.23f, 1.00f);
    colors[ImGuiCol_SliderGrab]     = ImVec4(0.77f, 0.12f, 0.23f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.87f, 0.22f, 0.33f, 1.00f);

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

    // Load initial monitor settings
    if (m_monitorManager && m_monitorManager->GetMonitorCount() > 0) {
        // Validation of index
        if (m_selectedMonitorIndex >= m_monitorManager->GetMonitorCount()) 
            m_selectedMonitorIndex = 0;
            
        LoadSettingsForMonitor(m_selectedMonitorIndex);
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
    const float clear_color_with_alpha[4] = { 1.00f, 0.86f, 0.90f, 1.00f }; // Soft pink
    m_context->OMSetRenderTargets(1, m_mainRenderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_mainRenderTargetView.Get(), clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0); // Enable VSync
}

void SettingsWindow::DrawUI() {
    // Modern Clean Color Palette
    const ImU32 COL_GRADIENT_TOP    = IM_COL32(255, 235, 245, 255); // Lighter pink
    const ImU32 COL_GRADIENT_BOTTOM = IM_COL32(235, 220, 250, 255); // Lighter purple
    const ImU32 COL_PRIMARY_RED     = IM_COL32(220, 53, 69, 255);   // Modern red
    const ImU32 COL_PRIMARY_DARK    = IM_COL32(196, 30, 58, 255);   // Darker red
    const ImU32 COL_CARD_BG         = IM_COL32(255, 255, 255, 250); // White card
    const ImU32 COL_TEXT_PRIMARY    = IM_COL32(33, 37, 41, 255);    // Dark text
    const ImU32 COL_TEXT_SECONDARY  = IM_COL32(108, 117, 125, 255); // Gray text
    const ImU32 COL_TEXT_LIGHT      = IM_COL32(255, 255, 255, 255); // White text
    const ImU32 COL_HOVER           = IM_COL32(255, 255, 255, 40);  // White overlay
    const ImU32 COL_SHADOW          = IM_COL32(0, 0, 0, 20);        // Subtle shadow
    const ImU32 COL_BORDER          = IM_COL32(222, 226, 230, 255); // Light border
    
    // Setup Window
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | 
                                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | 
                                     ImGuiWindowFlags_NoBackground;
    
    if (ImGui::Begin("Settings", nullptr, window_flags)) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();

        // === 1. CLEAN GRADIENT BACKGROUND ===
        draw_list->AddRectFilledMultiColor(
            win_pos, 
            ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y),
            COL_GRADIENT_TOP, COL_GRADIENT_TOP, COL_GRADIENT_BOTTOM, COL_GRADIENT_BOTTOM
        );

        // Optional: Very subtle noise overlay (single layer, not pattern)
        for (int i = 0; i < 100; i++) {
            float x = win_pos.x + (rand() % (int)win_size.x);
            float y = win_pos.y + (rand() % (int)win_size.y);
            draw_list->AddCircleFilled(ImVec2(x, y), 1.0f, IM_COL32(255, 255, 255, 10), 4);
        }

        // === 2. MODERN HEADER BAR ===
        float header_height = 70.0f;
        draw_list->AddRectFilled(
            win_pos,
            ImVec2(win_pos.x + win_size.x, win_pos.y + header_height),
            COL_PRIMARY_RED
        );
        
        // Clean title (no shadows)
        const char* title = "PIXEL MOTION SETTINGS";
        ImFont* font = ImGui::GetFont();
        float font_size = 20.0f;
        ImVec2 title_pos = ImVec2(win_pos.x + 30, win_pos.y + 25);
        draw_list->AddText(font, font_size, title_pos, COL_TEXT_LIGHT, title);

        // Subtle separator line
        draw_list->AddLine(
            ImVec2(win_pos.x, win_pos.y + header_height),
            ImVec2(win_pos.x + win_size.x, win_pos.y + header_height),
            IM_COL32(255, 255, 255, 30),
            2.0f
        );

        // Draggable title bar
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##TitleBarDrag", ImVec2(win_size.x - 50, header_height));
        if (ImGui::IsItemActive()) {
            SendMessage(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // Modern close button
        ImGui::SetCursorPos(ImVec2(win_size.x - 45, 20));
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_LIGHT);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 30));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 50));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
        if (ImGui::Button("✕", ImVec2(35, 35))) {
            Hide();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        // === 3. CLEAN SIDEBAR ===
        float sidebar_width = 220.0f;
        ImVec2 sidebar_p1 = ImVec2(win_pos.x, win_pos.y + header_height);
        ImVec2 sidebar_p2 = ImVec2(win_pos.x + sidebar_width, win_pos.y + win_size.y);
        
        draw_list->AddRectFilled(sidebar_p1, sidebar_p2, COL_PRIMARY_RED);
        
        // Sidebar content
        ImGui::SetCursorPos(ImVec2(20, header_height + 30));
        ImGui::BeginGroup();
        
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 200, 210, 255));
        ImGui::TextUnformatted("DISPLAYS");
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::Spacing();

        if (m_monitorManager) {
            int monitorCount = m_monitorManager->GetMonitorCount();
            for (int i = 0; i < monitorCount; i++) {
                char label[32];
                sprintf_s(label, "Monitor %d", i + 1);
                
                bool is_selected = (m_selectedMonitorIndex == i);
                
                // Modern button style with smooth hover
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, is_selected ? COL_HOVER : IM_COL32(0,0,0,0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_HOVER);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 60));
                ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_LIGHT);
                
                if (ImGui::Button(label, ImVec2(180, 44))) {
                    m_selectedMonitorIndex = i;
                    LoadSettingsForMonitor(i);
                }
                
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
                
                ImGui::Spacing();
            }
        }
        ImGui::EndGroup();

        // === 4. SPACIOUS CONTENT CARD ===
        float content_x = sidebar_width + 40;
        float content_y = header_height + 40;
        float content_w = win_size.x - sidebar_width - 80;
        float content_h = win_size.y - header_height - 80;
        
        ImVec2 card_p1 = ImVec2(win_pos.x + content_x, win_pos.y + content_y);
        ImVec2 card_p2 = ImVec2(card_p1.x + content_w, card_p1.y + content_h);
        
        // Soft shadow
        draw_list->AddRectFilled(
            ImVec2(card_p1.x + 4, card_p1.y + 4),
            ImVec2(card_p2.x + 4, card_p2.y + 4),
            COL_SHADOW,
            12.0f
        );
        
        // Card background with rounded corners
        draw_list->AddRectFilled(card_p1, card_p2, COL_CARD_BG, 12.0f);

        // === 5. CONTENT AREA ===
        ImGui::SetCursorPos(ImVec2(content_x + 40, content_y + 30));
        
        // Use a child window for scrollable content
        ImGui::BeginChild("ContentScroll", ImVec2(content_w - 80, content_h - 60), false, ImGuiWindowFlags_NoBackground);
        
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_PRIMARY);

        if (m_monitorManager && m_selectedMonitorIndex >= 0) {
            
            // Section title helper
            auto SectionTitle = [&](const char* text) {
                ImGui::PushStyleColor(ImGuiCol_Text, COL_PRIMARY_RED);
                ImGui::TextUnformatted(text);
                ImGui::PopStyleColor();
                ImGui::Spacing();
            };

            // Calculate available width for inputs
            float available_width = content_w - 160; // Account for card padding and margins
            float input_width = available_width - 120; // Leave room for browse button
            float list_width = available_width;

            // === WALLPAPER SECTION ===
            SectionTitle("Wallpaper");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_SECONDARY);
            ImGui::TextUnformatted("Select a video or image for this display");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(248, 249, 250, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(241, 243, 245, 255));
            ImGui::SetNextItemWidth(input_width);
            ImGui::InputText("##Path", m_wallpaperPathBuffer, MAX_PATH, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            
            ImGui::SameLine();
            
            // Modern button
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_DARK);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 25, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_LIGHT);
            
            if (ImGui::Button("Browse", ImVec2(100, 36))) {
                std::wstring path = OpenFileDialog();
                if (!path.empty()) {
                    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, m_wallpaperPathBuffer, MAX_PATH, nullptr, nullptr);
                }
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            // Separator
            ImGui::PushStyleColor(ImGuiCol_Separator, COL_BORDER);
            ImGui::Separator();
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();

            // === BEHAVIOR SECTION ===
            SectionTitle("Behavior");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_SECONDARY);
            ImGui::TextUnformatted("Configure when the wallpaper should pause");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            // Modern checkboxes with better spacing
            ImGui::PushStyleColor(ImGuiCol_CheckMark, COL_PRIMARY_RED);
            ImGui::Checkbox("Start with Windows", &m_autoStart);
            ImGui::Spacing();
            ImGui::Checkbox("Pause on Battery", &m_batteryPause);
            ImGui::Spacing();
            ImGui::Checkbox("Pause on Fullscreen", &m_fullscreenPause);
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            // Separator
            ImGui::PushStyleColor(ImGuiCol_Separator, COL_BORDER);
            ImGui::Separator();
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // === BLOCKLIST SECTION ===
            SectionTitle("Application Blocklist");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_SECONDARY);
            ImGui::TextUnformatted("Pause wallpaper when these applications are in focus");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            // Modern list box
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(248, 249, 250, 255));
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(220, 53, 69, 40));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(220, 53, 69, 60));
            
            if (ImGui::BeginListBox("##AppList", ImVec2(list_width, 150))) {
                for (int i = 0; i < m_blockedApps.size(); i++) {
                    const std::string& app = m_blockedApps[i];
                    if (ImGui::Selectable(app.c_str(), m_selectedAppIndex == i)) {
                        m_selectedAppIndex = i;
                    }
                }
                ImGui::EndListBox();
            }
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            
            ImGui::Spacing();
            
            // Action buttons
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            
            // Add button
            ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_DARK);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 25, 50, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_LIGHT);
            
            if (ImGui::Button("+ Add Application", ImVec2(150, 36))) {
                std::wstring exePath = OpenFileDialog();
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
                            for(const auto& existing : m_blockedApps) { 
                                if (_stricmp(existing.c_str(), asciiFilename.c_str()) == 0) exists = true; 
                            }
                            if (!exists) m_blockedApps.push_back(asciiFilename);
                        }
                    }
                }
            }
            
            ImGui::PopStyleColor(4);
            
            ImGui::SameLine();
            
            // Remove button (secondary style)
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(248, 249, 250, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(233, 236, 239, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(222, 226, 230, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_PRIMARY);
            
            if (ImGui::Button("Remove", ImVec2(100, 36))) {
                if (m_selectedAppIndex >= 0 && m_selectedAppIndex < m_blockedApps.size()) {
                    m_blockedApps.erase(m_blockedApps.begin() + m_selectedAppIndex);
                    m_selectedAppIndex = -1;
                }
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            
            // Add some bottom padding
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_SECONDARY);
            ImGui::TextUnformatted("Select a display from the sidebar to configure settings");
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor(); // Pop main text color
        ImGui::EndChild();
        
        // === 6. FLOATING SAVE BUTTON ===
        ImGui::SetCursorPos(ImVec2(win_size.x - 160, win_size.y - 80));
        
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 24.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_DARK);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 25, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_LIGHT);
        
        if (ImGui::Button("Save Changes", ImVec2(140, 48))) {
            ApplySettings();
            Hide();
        }
        
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

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
