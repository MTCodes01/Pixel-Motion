#include "WallpaperWindow.h"
#include "rendering/RendererContext.h"
#include "video/VideoDecoder.h"
#include "core/Logger.h"

namespace PixelMotion {

const wchar_t* WallpaperWindow::s_className = L"PixelMotionWallpaperWindow";
bool WallpaperWindow::s_classRegistered = false;

WallpaperWindow::WallpaperWindow()
    : m_hwnd(nullptr)
    , m_parent(nullptr)
    , m_frameInterval(1.0 / 30.0) // Default 30 FPS
    , m_needsRepaint(false)
{
    m_lastFrameTime = std::chrono::steady_clock::now();
}

WallpaperWindow::~WallpaperWindow() {
    Destroy();
}

bool WallpaperWindow::Create(HWND parentWorkerW, const MonitorInfo& monitor) {
    m_parent = parentWorkerW;
    m_monitor = monitor;

    Logger::Info("Creating wallpaper window for monitor: " + 
                 std::to_string(monitor.width) + "x" + std::to_string(monitor.height));
    Logger::Info("Parent WorkerW HWND: " + std::to_string(reinterpret_cast<uintptr_t>(parentWorkerW)));

    // Register window class
    if (!RegisterWindowClass()) {
        Logger::Error("Failed to register wallpaper window class");
        return false;
    }

    // Get WorkerW window rect to calculate positioning
    RECT workerRect;
    GetWindowRect(parentWorkerW, &workerRect);
    
    Logger::Info("WorkerW rect: (" + std::to_string(workerRect.left) + ", " + 
                 std::to_string(workerRect.top) + ", " + 
                 std::to_string(workerRect.right) + ", " + 
                 std::to_string(workerRect.bottom) + ")");
    Logger::Info("Monitor absolute: (" + std::to_string(monitor.bounds.left) + ", " + 
                 std::to_string(monitor.bounds.top) + ")");

    // Create window as a popup first (not as child)
    m_hwnd = CreateWindowEx(
        WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,   // Extended style - no taskbar button
        s_className,                            // Class name
        L"Pixel Motion Wallpaper",             // Window name
        WS_POPUP | WS_VISIBLE,                 // Popup style
        monitor.bounds.left,                   // X (screen coords)
        monitor.bounds.top,                    // Y (screen coords)
        monitor.width,                         // Width
        monitor.height,                        // Height
        nullptr,                               // No parent initially
        nullptr,                               // Menu
        GetModuleHandle(nullptr),              // Instance
        this                                   // lParam
    );

    if (!m_hwnd) {
        DWORD error = GetLastError();
        Logger::Error("Failed to create wallpaper window, error: " + std::to_string(error));
        return false;
    }

    Logger::Info("Wallpaper window created, HWND: " + std::to_string(reinterpret_cast<uintptr_t>(m_hwnd)));

    // Now attach to WorkerW using SetParent
    HWND oldParent = SetParent(m_hwnd, parentWorkerW);
    if (oldParent == nullptr && GetLastError() != 0) {
        Logger::Error("SetParent failed, error: " + std::to_string(GetLastError()));
    } else {
        Logger::Info("Successfully attached to WorkerW");
    }
    
    // Verify parent
    HWND actualParent = GetParent(m_hwnd);
    Logger::Info("Actual parent HWND: " + std::to_string(reinterpret_cast<uintptr_t>(actualParent)));
    
    // Position relative to WorkerW after SetParent
    int relativeX = monitor.bounds.left - workerRect.left;
    int relativeY = monitor.bounds.top - workerRect.top;
    Logger::Info("Relative position: (" + std::to_string(relativeX) + ", " + std::to_string(relativeY) + ")");
    
    // Move window to correct position relative to parent
    SetWindowPos(m_hwnd, nullptr, relativeX, relativeY, monitor.width, monitor.height, 
                 SWP_NOZORDER | SWP_NOACTIVATE);
    
    // Get window rect
    RECT rect;
    GetWindowRect(m_hwnd, &rect);
    Logger::Info("Window rect: (" + std::to_string(rect.left) + ", " + std::to_string(rect.top) + 
                 ", " + std::to_string(rect.right) + ", " + std::to_string(rect.bottom) + ")");

    // Make sure window is visible
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);

    // Initialize renderer for this window
    m_renderer = std::make_unique<RendererContext>();
    if (!m_renderer->Initialize(m_hwnd, monitor.width, monitor.height)) {
        Logger::Error("Failed to initialize renderer for wallpaper window");
        Destroy();
        return false;
    }
    m_renderer->SetMonitorBounds(monitor.bounds);

    std::string deviceName;
    int len = WideCharToMultiByte(CP_UTF8, 0, monitor.deviceName.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len > 1) {
        deviceName.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, monitor.deviceName.c_str(), -1, &deviceName[0], len, nullptr, nullptr);
    }
    Logger::Info("Created wallpaper window for monitor: " + deviceName);
    return true;
}

void WallpaperWindow::Destroy() {
    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    if (m_videoDecoder) {
        m_videoDecoder.reset();
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool WallpaperWindow::LoadVideo(const std::wstring& videoPath) {
    Logger::Info("Loading video for wallpaper...");

    // Shut down previous video decoder and release old GPU/RAM resources
    if (m_videoDecoder) {
        m_videoDecoder->Shutdown();
        m_videoDecoder.reset();
    }
    if (m_renderer) {
        m_renderer->ClearVideoResources();
    }

    // Create video decoder
    m_videoDecoder = std::make_unique<VideoDecoder>();

    // Get D3D11 device from renderer
    if (!m_renderer) {
        Logger::Error("Renderer not initialized");
        return false;
    }

    ID3D11Device* device = m_renderer->GetDevice();
    if (!device) {
        Logger::Error("Could not get D3D11 device");
        return false;
    }

    // Initialize decoder
    if (!m_videoDecoder->Initialize(videoPath, device)) {
        Logger::Error("Failed to initialize video decoder");
        m_videoDecoder.reset();
        return false;
    }

    // Update frame interval based on video frame rate (cap wallpaper presentation at 30 FPS for optimal battery and low GPU usage)
    double fps = m_videoDecoder->GetFrameRate();
    if (fps > 30.0) {
        m_frameInterval = 1.0 / 30.0;
    } else if (fps > 0) {
        m_frameInterval = 1.0 / fps;
    }

    // Decode first frame (allow several attempts for initial keyframe / GOP headers)
    bool firstFrameDecoded = false;
    for (int attempt = 0; attempt < 30; ++attempt) {
        if (m_videoDecoder->DecodeNextFrame()) {
            firstFrameDecoded = true;
            break;
        }
    }

    if (!firstFrameDecoded) {
        Logger::Error("Failed to decode first frame after multiple attempts");
        m_videoDecoder.reset();
        return false;
    }

    m_lastFrameTime = std::chrono::steady_clock::now();
    m_needsRepaint = true;

    // Immediately present the initial frame
    Render();

    // Reclaim transient decoding/decompression memory buffers back to the OS immediately
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    std::string path;
    int pathLen = WideCharToMultiByte(CP_UTF8, 0, videoPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (pathLen > 1) {
        path.resize(pathLen - 1);
        WideCharToMultiByte(CP_UTF8, 0, videoPath.c_str(), -1, &path[0], pathLen, nullptr, nullptr);
    }
    Logger::Info("Wallpaper media loaded successfully: " + path);
    return true;
}

void WallpaperWindow::UnloadVideo() {
    if (m_videoDecoder) {
        m_videoDecoder->Shutdown();
        m_videoDecoder.reset();
        Logger::Info("Video unloaded");
    }
    if (m_renderer) {
        m_renderer->ClearVideoResources();
    }
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
}

bool WallpaperWindow::RegisterWindowClass() {
    if (s_classRegistered) {
        return true;
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = s_className;

    if (!RegisterClassEx(&wc)) {
        return false;
    }

    s_classRegistered = true;
    return true;
}

LRESULT CALLBACK WallpaperWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WallpaperWindow* window = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<WallpaperWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<WallpaperWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            if (window) {
                window->m_needsRepaint = true;
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            return 0;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

void WallpaperWindow::Update() {
    if (!m_videoDecoder) {
        return;
    }

    // For static images, no need to decode frames
    if (m_videoDecoder->IsImage()) {
        return;
    }

    // Check if it's time for the next frame
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - m_lastFrameTime;

    if (elapsed.count() >= m_frameInterval) {
        // Decode next frame
        if (!m_videoDecoder->DecodeNextFrame()) {
            // End of file - loop back to beginning
            if (m_videoDecoder->IsEndOfFile()) {
                m_videoDecoder->Reset();
                m_videoDecoder->DecodeNextFrame();
            }
        }
        
        m_lastFrameTime = now;
        m_needsRepaint = true;
    }
}

double WallpaperWindow::GetTimeToNextFrame() const {
    if (!m_videoDecoder || m_videoDecoder->IsImage()) {
        return 1.0; // Static content, check infrequently
    }
    
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - m_lastFrameTime;
    double remaining = m_frameInterval - elapsed.count();
    
    return (remaining > 0.0) ? remaining : 0.0;
}

void WallpaperWindow::Render() {
    if (!m_renderer) {
        return;
    }

    if (m_videoDecoder) {
        // Get current frame texture and array index
        ID3D11Texture2D* frameTexture = m_videoDecoder->GetFrameTexture();
        int arrayIndex = m_videoDecoder->GetFrameArrayIndex();
        if (frameTexture) {
            m_renderer->SetVideoTexture(frameTexture, arrayIndex, m_videoDecoder->GetWidth(), m_videoDecoder->GetHeight());
        }
    }

    m_renderer->Render();
    m_renderer->Present(); // Display the frame
    m_needsRepaint = false;
}

void WallpaperWindow::SetVirtualDesktopBounds(const RECT& bounds) {
    if (m_renderer) {
        m_renderer->SetVirtualDesktopBounds(bounds);
    }
}

void WallpaperWindow::SetScalingMode(int mode) {
    if (m_renderer) {
        m_renderer->SetScalingMode(mode);
        if (m_videoDecoder) {
            m_renderer->Render();
            m_renderer->Present();
            m_needsRepaint = false;
        } else {
            m_needsRepaint = true;
        }
    }
}

} // namespace PixelMotion
