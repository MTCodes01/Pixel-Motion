#include "core/Configuration.h"
#include "SettingsWindow.h"
#include "resource.h"
#include "core/Logger.h"
#include "desktop/MonitorManager.h"
#include "desktop/DesktopManager.h"
#include "core/UpdateManager.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <tchar.h>
#include <commdlg.h> // For GetOpenFileName
#include <wincodec.h>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    // Elegant Dark Theme Color Palette
    const ImU32 COL_BG_DARK         = IM_COL32(19, 21, 26, 255);    // Deep Charcoal Background
    const ImU32 COL_SIDEBAR_BG      = IM_COL32(15, 17, 21, 255);    // Darker Sidebar
    const ImU32 COL_HEADER_BG       = IM_COL32(23, 26, 32, 255);    // Header Bar
    const ImU32 COL_CARD_BG         = IM_COL32(26, 29, 37, 255);    // Card Surface
    const ImU32 COL_CARD_BORDER     = IM_COL32(42, 47, 60, 255);    // Card Border
    
    const ImU32 COL_PRIMARY_RED     = IM_COL32(227, 46, 79, 255);   // Crimson Accent
    const ImU32 COL_PRIMARY_HOVER   = IM_COL32(250, 69, 102, 255);  // Hover Crimson
    const ImU32 COL_PRIMARY_DARK    = IM_COL32(189, 29, 58, 255);   // Active Crimson
    
    const ImU32 COL_TEXT_WHITE      = IM_COL32(248, 250, 252, 255); // White Text
    const ImU32 COL_TEXT_MUTED      = IM_COL32(148, 163, 184, 255); // Slate Gray Text
    const ImU32 COL_TEXT_HINT       = IM_COL32(100, 116, 139, 255); // Muted Hint
    const ImU32 COL_BORDER_DIVIDER  = IM_COL32(36, 41, 52, 255);    // Divider
}

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
    int winW = 880;
    int winH = 640;
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;

    m_hwnd = CreateWindow(_T("PixelMotionSettings"), _T("Pixel Motion Settings"), 
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, 
        x, y, winW, winH, nullptr, nullptr, wc.hInstance, this);

    // Set Window icons (Taskbar and Alt-Tab)
    HICON hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APP_ICON));
    if (hIcon) {
        SendMessage(m_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(m_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

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

    // Load App Logo Texture for UI
    LoadLogo();

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
    m_logoSRV.Reset();
    m_mainRenderTargetView.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool SettingsWindow::LoadLogo() {
    if (!m_device) return false;

    // Locate logo.png
    wchar_t exePath[MAX_PATH];
    GetModuleFileName(nullptr, exePath, MAX_PATH);
    std::filesystem::path appDir = std::filesystem::path(exePath).parent_path();
    
    std::vector<std::filesystem::path> candidates = {
        appDir / "logo.png",
        appDir / "logos" / "logo_light_512.png",
        appDir / "logos" / "logo_color_512.png",
        std::filesystem::path("d:\\Github\\Pixel-Motion\\src\\ui\\logo.png"),
        std::filesystem::path("d:\\Github\\Pixel-Motion\\logos\\logo_light_512.png")
    };

    std::filesystem::path chosen;
    for (const auto& p : candidates) {
        if (std::filesystem::exists(p)) {
            chosen = p;
            break;
        }
    }

    if (chosen.empty()) {
        Logger::Warning("Could not find logo.png for Settings UI");
        return false;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(chosen.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;

    UINT width = 0, height = 0;
    frame->GetSize(&width, &height);
    if (width == 0 || height == 0) return false;

    ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return false;

    UINT stride = width * 4;
    UINT bufferSize = stride * height;
    std::vector<BYTE> pixelBuffer(bufferSize);
    hr = converter->CopyPixels(nullptr, stride, bufferSize, pixelBuffer.data());
    if (FAILED(hr)) return false;

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixelBuffer.data();
    initData.SysMemPitch = stride;

    ComPtr<ID3D11Texture2D> texture;
    hr = m_device->CreateTexture2D(&texDesc, &initData, &texture);
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = m_device->CreateShaderResourceView(texture.Get(), &srvDesc, &m_logoSRV);
    if (SUCCEEDED(hr)) {
        Logger::Info("Loaded logo texture for Settings UI (" + std::to_string(width) + "x" + std::to_string(height) + ")");
        return true;
    }
    return false;
}

bool SettingsWindow::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load High-Quality Segoe UI Fonts for Windows
    if (std::filesystem::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        m_fontRegular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f);
    }
    if (std::filesystem::exists("C:\\Windows\\Fonts\\segoeuib.ttf")) {
        m_fontBold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 18.0f);
        m_fontTitle = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 22.0f);
    }
    if (!m_fontRegular) m_fontRegular = io.Fonts->AddFontDefault();
    if (!m_fontBold)    m_fontBold = m_fontRegular;
    if (!m_fontTitle)   m_fontTitle = m_fontRegular;

    // --- Sleek Fluent Dark Theme ---
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    const ImVec4 bgWindow       = ImVec4(0.09f, 0.10f, 0.13f, 1.00f); // #171A21 Deep Dark
    const ImVec4 bgCard         = ImVec4(0.13f, 0.14f, 0.18f, 1.00f); // #21242E Elevated Surface
    const ImVec4 bgPopup        = ImVec4(0.14f, 0.16f, 0.20f, 0.98f); // #242933
    const ImVec4 borderSubtle   = ImVec4(0.22f, 0.25f, 0.32f, 0.70f); // #384052
    
    const ImVec4 textPrimary    = ImVec4(0.95f, 0.96f, 0.98f, 1.00f); // #F2F5FA Crisp White
    const ImVec4 textMuted      = ImVec4(0.60f, 0.65f, 0.72f, 1.00f); // #99A6B8 Slate
    
    const ImVec4 accentCrimson  = ImVec4(0.89f, 0.18f, 0.31f, 1.00f); // #E32E4F Radiant Crimson
    const ImVec4 accentHover    = ImVec4(0.98f, 0.27f, 0.40f, 1.00f); // #FA4566
    const ImVec4 accentActive   = ImVec4(0.75f, 0.12f, 0.23f, 1.00f); // #BF1F3B
    const ImVec4 inputFieldBg   = ImVec4(0.08f, 0.09f, 0.11f, 0.85f); // #14171C

    colors[ImGuiCol_WindowBg]             = bgWindow;
    colors[ImGuiCol_ChildBg]              = bgCard;
    colors[ImGuiCol_PopupBg]              = bgPopup;
    colors[ImGuiCol_Border]               = borderSubtle;
    colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_Text]                 = textPrimary;
    colors[ImGuiCol_TextDisabled]         = textMuted;

    colors[ImGuiCol_Header]               = ImVec4(accentCrimson.x, accentCrimson.y, accentCrimson.z, 0.25f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(accentCrimson.x, accentCrimson.y, accentCrimson.z, 0.45f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(accentCrimson.x, accentCrimson.y, accentCrimson.z, 0.70f);

    colors[ImGuiCol_Button]               = accentCrimson;
    colors[ImGuiCol_ButtonHovered]        = accentHover;
    colors[ImGuiCol_ButtonActive]         = accentActive;

    colors[ImGuiCol_FrameBg]              = inputFieldBg;
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.16f, 0.18f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.23f, 0.29f, 1.00f);

    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.35f, 0.39f, 0.47f, 0.40f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.50f, 0.60f, 0.70f);
    colors[ImGuiCol_ScrollbarGrabActive]  = accentCrimson;

    colors[ImGuiCol_CheckMark]            = accentCrimson;
    colors[ImGuiCol_SliderGrab]           = accentCrimson;
    colors[ImGuiCol_SliderGrabActive]     = accentHover;
    colors[ImGuiCol_Separator]            = borderSubtle;

    // Modern Rounding & Dimensions
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.GrabRounding      = 4.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 6.0f;
    style.ScrollbarSize     = 8.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;

    style.ItemSpacing       = ImVec2(10, 8);
    style.FramePadding      = ImVec2(10, 8);
    style.WindowPadding     = ImVec2(16, 16);

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
        case WM_SIZE:
            // Handle window resize: recreate DX11 render target
            if (window->m_swapChain && wParam != SIZE_MINIMIZED) {
                window->m_mainRenderTargetView.Reset();
                window->m_swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
                ID3D11Texture2D* pBackBuffer = nullptr;
                window->m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
                if (pBackBuffer) {
                    window->m_device->CreateRenderTargetView(pBackBuffer, nullptr, &window->m_mainRenderTargetView);
                    pBackBuffer->Release();
                }
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
                return 0;
            break;
        case WM_DPICHANGED:
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
    const float clear_color_with_alpha[4] = { 0.09f, 0.10f, 0.13f, 1.00f }; // Deep Charcoal Dark
    m_context->OMSetRenderTargets(1, m_mainRenderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_mainRenderTargetView.Get(), clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0); // Enable VSync
}

void SettingsWindow::DrawUI() {
    // Setup Fullscreen ImGui Window
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

        // 1. Sleek Gradient Dark Canvas
        draw_list->AddRectFilledMultiColor(
            win_pos, 
            ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y),
            COL_BG_DARK, COL_BG_DARK,
            IM_COL32(14, 15, 19, 255), IM_COL32(14, 15, 19, 255)
        );

        // 2. Modern Header Bar
        float header_height = 62.0f;
        draw_list->AddRectFilled(
            win_pos,
            ImVec2(win_pos.x + win_size.x, win_pos.y + header_height),
            COL_HEADER_BG
        );
        
        // Brand Logo Image & Title
        if (m_logoSRV) {
            float logo_size = 32.0f;
            float logo_y = win_pos.y + (header_height - logo_size) * 0.5f;
            draw_list->AddImage(
                (ImTextureID)m_logoSRV.Get(),
                ImVec2(win_pos.x + 20, logo_y),
                ImVec2(win_pos.x + 20 + logo_size, logo_y + logo_size)
            );
            
            if (m_fontTitle) ImGui::PushFont(m_fontTitle);
            draw_list->AddText(ImVec2(win_pos.x + 60, win_pos.y + 18), COL_TEXT_WHITE, "PIXEL MOTION");
            if (m_fontTitle) ImGui::PopFont();
            
            // Subtitle badge
            draw_list->AddRectFilled(
                ImVec2(win_pos.x + 218, win_pos.y + 20),
                ImVec2(win_pos.x + 276, win_pos.y + 42),
                IM_COL32(227, 46, 79, 35), 6.0f
            );
            draw_list->AddText(ImVec2(win_pos.x + 227, win_pos.y + 23), COL_PRIMARY_HOVER, "v1.3.6");
        } else {
            draw_list->AddCircleFilled(ImVec2(win_pos.x + 28, win_pos.y + header_height * 0.5f), 5.0f, COL_PRIMARY_RED);
            
            if (m_fontTitle) ImGui::PushFont(m_fontTitle);
            draw_list->AddText(ImVec2(win_pos.x + 42, win_pos.y + 18), COL_TEXT_WHITE, "PIXEL MOTION");
            if (m_fontTitle) ImGui::PopFont();
            
            // Subtitle badge
            draw_list->AddRectFilled(
                ImVec2(win_pos.x + 200, win_pos.y + 20),
                ImVec2(win_pos.x + 258, win_pos.y + 42),
                IM_COL32(227, 46, 79, 35), 6.0f
            );
            draw_list->AddText(ImVec2(win_pos.x + 209, win_pos.y + 23), COL_PRIMARY_HOVER, "v1.3.6");
        }

        // Subtle header bottom line
        draw_list->AddLine(
            ImVec2(win_pos.x, win_pos.y + header_height),
            ImVec2(win_pos.x + win_size.x, win_pos.y + header_height),
            COL_BORDER_DIVIDER, 1.0f
        );

        // Draggable Titlebar Area
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##TitleBarDrag", ImVec2(win_size.x, header_height));
        if (ImGui::IsItemActive()) {
            SendMessage(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // 3. Modern Dark Sidebar (Displays)
        float sidebar_width = 220.0f;
        ImVec2 sidebar_p1 = ImVec2(win_pos.x, win_pos.y + header_height);
        ImVec2 sidebar_p2 = ImVec2(win_pos.x + sidebar_width, win_pos.y + win_size.y);
        
        draw_list->AddRectFilled(sidebar_p1, sidebar_p2, COL_SIDEBAR_BG);
        draw_list->AddLine(
            ImVec2(win_pos.x + sidebar_width, win_pos.y + header_height),
            ImVec2(win_pos.x + sidebar_width, win_pos.y + win_size.y),
            COL_BORDER_DIVIDER, 1.0f
        );
        
        // Sidebar Content
        ImGui::SetCursorPos(ImVec2(16, header_height + 24));
        ImGui::BeginGroup();
        
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_HINT);
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextUnformatted("DISPLAYS");
        if (m_fontBold) ImGui::PopFont();
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::Spacing();

        if (m_monitorManager) {
            int monitorCount = m_monitorManager->GetMonitorCount();
            for (int i = 0; i < monitorCount; i++) {
                char label[64];
                auto* mon = m_monitorManager->GetMonitor(i);
                if (mon) {
                    sprintf_s(label, "Monitor %d\n%dx%d", i + 1, mon->width, mon->height);
                } else {
                    sprintf_s(label, "Monitor %d", i + 1);
                }
                
                bool is_selected = (m_selectedMonitorIndex == i);
                
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                if (is_selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_HOVER);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, COL_PRIMARY_DARK);
                    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(24, 27, 34, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(34, 38, 48, 255));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 45, 58, 255));
                    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
                }
                
                if (ImGui::Button(label, ImVec2(188, 54))) {
                    m_selectedMonitorIndex = i;
                    LoadSettingsForMonitor(i);
                }
                
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
                
                ImGui::Spacing();
            }
        }
        ImGui::EndGroup();

        // Sidebar Bottom Updates Widget (Minimal & User Friendly)
        float update_y = win_pos.y + win_size.y - 48.0f;
        draw_list->AddLine(
            ImVec2(win_pos.x + 16, update_y),
            ImVec2(win_pos.x + sidebar_width - 16, update_y),
            COL_BORDER_DIVIDER, 1.0f
        );

        ImGui::SetCursorPos(ImVec2(16, update_y - win_pos.y + 12));
        DrawUpdateWidget();

        // 4. Main Content Area Card & Dedicated Footer
        float content_x = sidebar_width + 24.0f;
        float content_y = header_height + 20.0f;
        float content_w = win_size.x - content_x - 24.0f;
        float footer_h = 64.0f;
        float content_h = win_size.y - content_y - footer_h - 16.0f;
        
        ImVec2 card_p1 = ImVec2(win_pos.x + content_x, win_pos.y + content_y);
        ImVec2 card_p2 = ImVec2(card_p1.x + content_w, card_p1.y + content_h);
        
        // Card surface with subtle border
        draw_list->AddRectFilled(card_p1, card_p2, COL_CARD_BG, 10.0f);
        draw_list->AddRect(card_p1, card_p2, COL_CARD_BORDER, 10.0f);

        // 5. Scrollable Content Area inside Card
        ImGui::SetCursorPos(ImVec2(content_x + 24, content_y + 18));
        ImGui::BeginChild("ContentScroll", ImVec2(content_w - 48, content_h - 36), false, ImGuiWindowFlags_NoBackground);
        
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);

        if (m_monitorManager && m_selectedMonitorIndex >= 0) {
            
            auto SectionTitle = [&](const char* text) {
                if (m_fontBold) ImGui::PushFont(m_fontBold);
                ImGui::PushStyleColor(ImGuiCol_Text, COL_PRIMARY_HOVER);
                ImGui::TextUnformatted(text);
                ImGui::PopStyleColor();
                if (m_fontBold) ImGui::PopFont();
                ImGui::Spacing();
            };

            float available_width = content_w - 72;
            float input_width = available_width - 110;

            // === WALLPAPER MEDIA SECTION ===
            SectionTitle("Wallpaper Media");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            ImGui::TextUnformatted("Select an animated video (MP4, MKV) or high-res image (PNG, JPG, BMP, WebP)");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(18, 20, 26, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(24, 27, 35, 255));
            ImGui::SetNextItemWidth(input_width);
            ImGui::InputText("##Path", m_wallpaperPathBuffer, MAX_PATH, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            
            ImGui::SameLine();
            
            // Browse button
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_HOVER);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, COL_PRIMARY_DARK);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
            
            if (ImGui::Button("Browse...", ImVec2(100, 36))) {
                std::wstring path = OpenFileDialog();
                if (!path.empty()) {
                    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, m_wallpaperPathBuffer, MAX_PATH, nullptr, nullptr);
                    m_statusMessage = "Wallpaper selected. Click Apply to preview.";
                }
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();

            ImGui::Spacing();
            ImGui::Spacing();

            // Separator
            ImGui::PushStyleColor(ImGuiCol_Separator, COL_BORDER_DIVIDER);
            ImGui::Separator();
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();

            // === SCALING SECTION ===
            SectionTitle("Scaling Mode");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            ImGui::TextUnformatted("Choose how the media adapts to this monitor's resolution");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            const char* scalingItems[] = { 
                "Fill (Crop to cover screen)", 
                "Fit (Contain entire image)", 
                "Stretch (Match screen dimensions)", 
                "Center (Original resolution)",
                "Tile (Repeat across screen)",
                "Span (Spread across monitors)"
            };

            const char* scalingDescriptions[] = {
                "Fills the screen while maintaining aspect ratio; crops excess portions.",
                "Displays the entire image without cropping; leaves empty space if needed.",
                "Stretches the image to match screen dimensions, ignoring aspect ratio.",
                "Displays the image at its original resolution, centered on the screen.",
                "Repeats the image to cover the entire screen.",
                "Spreads one wallpaper continuously across multiple monitors."
            };
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(18, 20, 26, 255));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(24, 27, 35, 255));
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 27, 35, 255));
            ImGui::SetNextItemWidth(available_width);
            ImGui::Combo("##Scaling", &m_scalingMode, scalingItems, IM_ARRAYSIZE(scalingItems));
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            // Scaling mode explanation helper text
            if (m_scalingMode >= 0 && m_scalingMode < static_cast<int>(IM_ARRAYSIZE(scalingDescriptions))) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, COL_PRIMARY_HOVER);
                ImGui::TextWrapped("%s", scalingDescriptions[m_scalingMode]);
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Spacing();

            // Separator
            ImGui::PushStyleColor(ImGuiCol_Separator, COL_BORDER_DIVIDER);
            ImGui::Separator();
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();

            // === BEHAVIOR & AUTOMATION SECTION ===
            SectionTitle("Behavior & Automation");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            ImGui::TextUnformatted("Intelligent resource-aware playback pausing");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::Checkbox("Start with Windows (Launch on system boot)", &m_autoStart);
            ImGui::Spacing();
            ImGui::Checkbox("Pause on Battery (Conserve laptop battery power)", &m_batteryPause);
            ImGui::Spacing();
            ImGui::Checkbox("Pause on Fullscreen (Game Mode automatically pauses playback)", &m_fullscreenPause);

            ImGui::Spacing();
            ImGui::Spacing();

            // Separator
            ImGui::PushStyleColor(ImGuiCol_Separator, COL_BORDER_DIVIDER);
            ImGui::Separator();
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Spacing();
            
            // === APPLICATION BLOCKLIST SECTION ===
            SectionTitle("Application Blocklist");
            
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            ImGui::TextUnformatted("Pause wallpaper whenever these processes are focused");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(18, 20, 26, 255));
            ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(227, 46, 79, 80));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(227, 46, 79, 120));
            
            if (ImGui::BeginListBox("##AppList", ImVec2(available_width, 120))) {
                for (int i = 0; i < (int)m_blockedApps.size(); i++) {
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
            
            // Action buttons for blocklist
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 46, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(42, 47, 60, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 56, 72, 255));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
            
            if (ImGui::Button("+ Add Executable...", ImVec2(160, 34))) {
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
            
            ImGui::SameLine();
            
            if (ImGui::Button("Remove", ImVec2(100, 34))) {
                if (m_selectedAppIndex >= 0 && m_selectedAppIndex < (int)m_blockedApps.size()) {
                    m_blockedApps.erase(m_blockedApps.begin() + m_selectedAppIndex);
                    m_selectedAppIndex = -1;
                }
            }
            
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            
            ImGui::Spacing();
            ImGui::Spacing();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            ImGui::TextUnformatted("Select a display from the sidebar to configure wallpaper settings.");
            ImGui::PopStyleColor();
        }

        ImGui::PopStyleColor();
        ImGui::EndChild();
        
        // 6. Dedicated Bottom Action Footer (Clean Alignment, Zero Overlap)
        float footer_y = win_size.y - footer_h;
        
        // Status indicator text
        ImGui::SetCursorPos(ImVec2(content_x, footer_y + 14));
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
        ImGui::Text("%s", m_statusMessage.c_str());
        ImGui::PopStyleColor();
        
        // Secondary "Apply" Button
        ImGui::SetCursorPos(ImVec2(win_size.x - 24.0f - 140.0f - 12.0f - 110.0f, footer_y + 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 36, 46, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(44, 49, 62, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(52, 58, 74, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
        
        if (ImGui::Button("Apply", ImVec2(110, 42))) {
            ApplySettings();
            m_statusMessage = "Settings applied successfully.";
        }
        
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
        
        // Primary "Save Changes" Button
        ImGui::SetCursorPos(ImVec2(win_size.x - 24.0f - 140.0f, footer_y + 8));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, COL_PRIMARY_DARK);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
        
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        if (ImGui::Button("Save Changes", ImVec2(140, 42))) {
            ApplySettings();
            Hide();
        }
        if (m_fontBold) ImGui::PopFont();
        
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();

    }
    ImGui::End();
}

void SettingsWindow::DrawUpdateWidget() {
    auto& updater = UpdateManager::GetInstance();
    UpdateState state = updater.GetState();

    switch (state) {
        case UpdateState::Idle:
        case UpdateState::UpToDate: {
            std::string ver = updater.GetCurrentVersion();
            while (ver.rfind("v", 0) == 0 || ver.rfind("V", 0) == 0) {
                ver.erase(0, 1);
            }
            ver = "v" + ver;
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_HINT);
            ImGui::TextUnformatted(ver.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 10);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(35, 170, 85, 25));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(35, 170, 85, 55));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(35, 170, 85, 85));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(45, 200, 100, 90));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(75, 225, 125, 255));
            if (ImGui::SmallButton("Check for update")) {
                updater.CheckForUpdates(false);
            }
            ImGui::PopStyleColor(5);
            ImGui::PopStyleVar(2);
            break;
        }

        case UpdateState::Checking: {
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            ImGui::TextUnformatted("Checking for update...");
            ImGui::PopStyleColor();
            break;
        }

        case UpdateState::UpdateAvailable: {
            ImGui::PushStyleColor(ImGuiCol_Text, COL_PRIMARY_HOVER);
            ImGui::TextUnformatted("Update available!");
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_HOVER);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, COL_PRIMARY_DARK);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
            if (ImGui::SmallButton("Update")) {
                updater.StartDownload();
            }
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            break;
        }

        case UpdateState::Downloading: {
            float progress = updater.GetDownloadProgress();
            char buf[32];
            sprintf_s(buf, "Updating... %d%%", (int)(progress * 100.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_PRIMARY_HOVER);
            ImGui::TextUnformatted(buf);
            ImGui::PopStyleColor();

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COL_PRIMARY_RED);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(24, 28, 36, 255));
            ImGui::ProgressBar(progress, ImVec2(188, 3), "");
            ImGui::PopStyleColor(2);
            break;
        }

        case UpdateState::ReadyToInstall: {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(75, 225, 125, 255));
            ImGui::TextUnformatted("Update ready!");
            ImGui::PopStyleColor();

            ImGui::SameLine(0, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, COL_PRIMARY_RED);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_PRIMARY_HOVER);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, COL_PRIMARY_DARK);
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_WHITE);
            if (ImGui::SmallButton("Restart")) {
                updater.LaunchInstaller();
            }
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar();
            break;
        }

        case UpdateState::Error: {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(235, 85, 85, 255));
            ImGui::TextUnformatted("Check failed.");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", updater.GetErrorMessage().c_str());
            }

            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 15));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 25));
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_MUTED);
            if (ImGui::SmallButton("Retry")) {
                updater.CheckForUpdates(false);
            }
            ImGui::PopStyleColor(4);
            break;
        }
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
    
    // Check if any monitor was currently in Span mode (mode 5)
    bool wasInSpanMode = false;
    for (size_t i = 0; i < m_monitorManager->GetMonitorCount(); i++) {
        auto* mon = m_monitorManager->GetMonitor(static_cast<int>(i));
        if (mon) {
            auto* cfg = m_config->GetMonitorConfig(mon->deviceName);
            if (cfg && cfg->scalingMode == 5) {
                wasInSpanMode = true;
                break;
            }
        }
    }

    // Create monitor config
    if (m_scalingMode == 5) {
        // Span mode: apply across ALL monitors continuously
        for (size_t i = 0; i < m_monitorManager->GetMonitorCount(); i++) {
            auto* mon = m_monitorManager->GetMonitor(static_cast<int>(i));
            if (mon) {
                Configuration::MonitorConfig monCfg;
                monCfg.wallpaperPath = wPath;
                monCfg.enabled = true;
                monCfg.scalingMode = 5;
                m_config->SetMonitorConfig(mon->deviceName, monCfg);
            }
        }
    } else if (wasInSpanMode) {
        // Exiting Span mode: transition all monitors that were spanned to the new scaling mode
        for (size_t i = 0; i < m_monitorManager->GetMonitorCount(); i++) {
            int idx = static_cast<int>(i);
            auto* mon = m_monitorManager->GetMonitor(idx);
            if (!mon) continue;

            auto* existingCfg = m_config->GetMonitorConfig(mon->deviceName);
            Configuration::MonitorConfig monCfg;
            monCfg.enabled = true;
            monCfg.scalingMode = m_scalingMode;

            if (idx == m_selectedMonitorIndex) {
                monCfg.wallpaperPath = wPath;
            } else {
                monCfg.wallpaperPath = (existingCfg && !existingCfg->wallpaperPath.empty()) ? existingCfg->wallpaperPath : wPath;
            }
            m_config->SetMonitorConfig(mon->deviceName, monCfg);
        }
    } else {
        Configuration::MonitorConfig config;
        config.wallpaperPath = wPath;
        config.enabled = true;
        config.scalingMode = m_scalingMode;
        m_config->SetMonitorConfig(monitor->deviceName, config);
    }
    
    // Save global settings
    m_config->SetGameModeEnabled(m_fullscreenPause);
    m_config->SetBatteryAwareEnabled(m_batteryPause);
    m_config->SetStartWithWindows(m_autoStart);
    m_config->SetProcessBlocklist(m_blockedApps);
    
    // Persist to disk
    if (m_config->Save()) {
        Logger::Info("Settings saved successfully");
    } else {
        Logger::Error("Failed to save settings");
    }
    
    // Apply to desktop manager
    if (m_desktopManager) {
        if (m_scalingMode == 5) {
            m_desktopManager->SetWallpaperSpan(wPath);
            Logger::Info("Spanned wallpaper applied across all monitors");
        } else if (wasInSpanMode) {
            // Apply new scaling mode to all monitors that were previously spanned
            for (size_t i = 0; i < m_monitorManager->GetMonitorCount(); i++) {
                int idx = static_cast<int>(i);
                m_desktopManager->SetScalingMode(idx, m_scalingMode);
            }
            if (wcslen(wPath) > 0) {
                m_desktopManager->SetWallpaper(m_selectedMonitorIndex, wPath);
            }
            Logger::Info("Exited span mode: all monitors synchronized to mode " + std::to_string(m_scalingMode));
        } else {
            if (wcslen(wPath) > 0) {
                m_desktopManager->SetWallpaper(m_selectedMonitorIndex, wPath);
                Logger::Info("Wallpaper applied to monitor");
            } else {
                m_desktopManager->SetScalingMode(m_selectedMonitorIndex, m_scalingMode);
                Logger::Info("Scaling mode applied to monitor");
            }
        }
    }

    // Reclaim working set memory after applying changes
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

std::wstring SettingsWindow::OpenFileDialog() {
    OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd; // Modal to settings window
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Media Files\0*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.jpg;*.jpeg;*.png;*.bmp;*.webp;*.tiff\0All Files\0*.*\0";
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
