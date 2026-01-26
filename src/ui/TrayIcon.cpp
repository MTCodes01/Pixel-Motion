#include "TrayIcon.h"
#include "Application.h"
#include "core/Logger.h"
#include "resources/ResourceManager.h"

#include <shellapi.h>

namespace PixelMotion {

const UINT TrayIcon::WM_TRAYICON;
const UINT TrayIcon::CMD_PAUSE;
const UINT TrayIcon::CMD_SETTINGS;
const UINT TrayIcon::CMD_EXIT;

TrayIcon::TrayIcon()
    : m_hwnd(nullptr)
    , m_initialized(false)
    , m_resourceManager(nullptr)
    , m_paused(false)
{
    ZeroMemory(&m_nid, sizeof(m_nid));
}

TrayIcon::~TrayIcon() {
    Shutdown();
}

bool TrayIcon::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing tray icon...");

    // Create hidden window for tray icon messages
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"PixelMotionTrayWindow";

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(
        0, L"PixelMotionTrayWindow", L"Pixel Motion Tray",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
        GetModuleHandle(nullptr), this);

    if (!m_hwnd) {
        Logger::Error("Failed to create tray window");
        return false;
    }

    // Setup tray icon
    m_nid.cbSize = sizeof(NOTIFYICONDATA);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // TODO: Load custom icon
    wcscpy_s(m_nid.szTip, ARRAYSIZE(m_nid.szTip), L"Pixel Motion");

    if (!Shell_NotifyIcon(NIM_ADD, &m_nid)) {
        Logger::Error("Failed to add tray icon");
        DestroyWindow(m_hwnd);
        return false;
    }

    m_initialized = true;
    Logger::Info("Tray icon initialized successfully");
    return true;
}

void TrayIcon::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("Shutting down tray icon...");

    Shell_NotifyIcon(NIM_DELETE, &m_nid);

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    m_initialized = false;
}

LRESULT CALLBACK TrayIcon::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TrayIcon* tray = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        tray = reinterpret_cast<TrayIcon*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tray));
    } else {
        tray = reinterpret_cast<TrayIcon*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            if (tray) {
                tray->ShowContextMenu();
            }
        }
        return 0;
    }

    if (msg == WM_COMMAND) {
        if (tray) {
            tray->OnCommand(LOWORD(wParam));
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void TrayIcon::ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    
    // Dynamic menu text based on pause state
    const wchar_t* pauseText = m_paused ? L"Resume" : L"Pause";
    AppendMenu(hMenu, MF_STRING, CMD_PAUSE, pauseText);
    
    AppendMenu(hMenu, MF_STRING, CMD_SETTINGS, L"Settings...");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, CMD_EXIT, L"Exit");

    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        pt.x, pt.y, 0, m_hwnd, nullptr);
    DestroyMenu(hMenu);
}

void TrayIcon::OnCommand(UINT command) {
    switch (command) {
        case CMD_PAUSE:
            if (m_resourceManager) {
                m_paused = !m_paused;
                m_resourceManager->SetPaused(m_paused);
                
                if (m_paused) {
                    Logger::Info("Wallpapers paused by user");
                    ShowNotification(L"Pixel Motion", L"Wallpapers paused");
                } else {
                    Logger::Info("Wallpapers resumed by user");
                    ShowNotification(L"Pixel Motion", L"Wallpapers resumed");
                }
                
                UpdateIcon();
            } else {
                Logger::Warning("ResourceManager not set - cannot pause/resume");
            }
            break;

        case CMD_SETTINGS:
            Logger::Info("Settings clicked");
            Application::GetInstance().ShowSettings();
            break;

        case CMD_EXIT:
            Logger::Info("Exit clicked");
            Application::GetInstance().RequestExit();
            break;
    }
}

void TrayIcon::UpdateIcon() {
    // TODO: Update icon based on state (paused, battery, etc.)
}

void TrayIcon::ShowNotification(const std::wstring& title, const std::wstring& message) {
    m_nid.uFlags = NIF_INFO;
    wcscpy_s(m_nid.szInfoTitle, ARRAYSIZE(m_nid.szInfoTitle), title.c_str());
    wcscpy_s(m_nid.szInfo, ARRAYSIZE(m_nid.szInfo), message.c_str());
    m_nid.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIcon(NIM_MODIFY, &m_nid);
}

} // namespace PixelMotion
