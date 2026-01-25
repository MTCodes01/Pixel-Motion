#include "SettingsWindow.h"
#include "resource.h"
#include "core/Logger.h"
#include "core/Configuration.h"
#include "desktop/MonitorManager.h"
#include "desktop/DesktopManager.h"
#include "Application.h"

#include <CommCtrl.h>
#include <commdlg.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

namespace PixelMotion {

SettingsWindow::SettingsWindow()
    : m_hwnd(nullptr)
    , m_initialized(false)
    , m_settingsChanged(false)
    , m_config(nullptr)
    , m_monitorManager(nullptr)
    , m_currentMonitorIndex(0)
{
}

SettingsWindow::~SettingsWindow() {
    Shutdown();
}

bool SettingsWindow::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing settings window...");

    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    m_initialized = true;
    return true;
}

void SettingsWindow::Shutdown() {
    if (!m_initialized) {
        return;
    }

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    m_initialized = false;
}

void SettingsWindow::Show() {
    if (!m_initialized) {
        Logger::Error("SettingsWindow not initialized");
        return;
    }

    if (m_hwnd && IsWindow(m_hwnd)) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        return;
    }

    // Create modeless dialog
    m_hwnd = CreateDialogParam(
        GetModuleHandle(nullptr),
        MAKEINTRESOURCE(IDD_SETTINGS_DIALOG),
        nullptr,
        DialogProc,
        reinterpret_cast<LPARAM>(this)
    );

    if (!m_hwnd) {
        Logger::Error("Failed to create settings dialog");
        return;
    }

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void SettingsWindow::Hide() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

bool SettingsWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void SettingsWindow::SetConfiguration(Configuration* config) {
    m_config = config;
}

void SettingsWindow::SetMonitorManager(MonitorManager* monitorMgr) {
    m_monitorManager = monitorMgr;
}

INT_PTR CALLBACK SettingsWindow::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsWindow* pThis = nullptr;

    if (msg == WM_INITDIALOG) {
        pThis = reinterpret_cast<SettingsWindow*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        return pThis->OnInitDialog(hwnd);
    } else {
        pThis = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!pThis) {
        return FALSE;
    }

    switch (msg) {
        case WM_COMMAND:
            return pThis->OnCommand(wParam, lParam);
        case WM_NOTIFY:
            return pThis->OnNotify(wParam, lParam);
        case WM_CLOSE:
            return pThis->OnClose();
        default:
            return FALSE;
    }
}

INT_PTR SettingsWindow::OnInitDialog(HWND hwnd) {
    m_hwnd = hwnd;

    // Set dialog title
    SetWindowText(m_hwnd, L"Pixel Motion - Settings");

    // Center the dialog
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(m_hwnd, nullptr,
        (screenWidth - width) / 2,
        (screenHeight - height) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);

    // Populate monitor list
    PopulateMonitorList();

    // Load settings for first monitor
    if (m_monitorManager && m_monitorManager->GetMonitorCount() > 0) {
        LoadMonitorSettings(0);
    }

    // Update resource management settings
    UpdateResourceSettings();

    return TRUE;
}

INT_PTR SettingsWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    int controlId = LOWORD(wParam);
    int notifyCode = HIWORD(wParam);

    switch (controlId) {
        case IDC_BROWSE_BUTTON:
            OnBrowseWallpaper();
            return TRUE;

        case IDC_SCALING_COMBO:
            if (notifyCode == CBN_SELCHANGE) {
                OnScalingChanged();
            }
            return TRUE;

        case IDC_GAMEMODE_CHECK:
        case IDC_BATTERY_CHECK:
        case IDC_STARTUP_CHECK:
            m_settingsChanged = true;
            UpdateControlStates();
            return TRUE;

        case IDC_APPLY_BUTTON:
            OnApply();
            return TRUE;

        case IDC_CANCEL_BUTTON:
        case IDCANCEL:
            OnCancel();
            return TRUE;

        default:
            return FALSE;
    }
}

INT_PTR SettingsWindow::OnNotify(WPARAM wParam, LPARAM lParam) {
    LPNMHDR pnmh = reinterpret_cast<LPNMHDR>(lParam);

    if (pnmh->idFrom == IDC_MONITOR_LIST && pnmh->code == LVN_ITEMCHANGED) {
        LPNMLISTVIEW pnmlv = reinterpret_cast<LPNMLISTVIEW>(lParam);
        if (pnmlv->uNewState & LVIS_SELECTED) {
            OnMonitorSelectionChanged();
        }
    }

    return FALSE;
}

INT_PTR SettingsWindow::OnClose() {
    Hide();
    return TRUE;
}

void SettingsWindow::PopulateMonitorList() {
    HWND hList = GetDlgItem(m_hwnd, IDC_MONITOR_LIST);
    if (!hList) return;

    // Set up list view columns
    LVCOLUMN lvc = {};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH;
    lvc.cx = 80;
    lvc.pszText = const_cast<LPWSTR>(L"Monitor");
    ListView_InsertColumn(hList, 0, &lvc);

    lvc.cx = 150;
    lvc.pszText = const_cast<LPWSTR>(L"Resolution");
    ListView_InsertColumn(hList, 1, &lvc);

    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"Primary");
    ListView_InsertColumn(hList, 2, &lvc);

    // Populate monitor information
    if (!m_monitorManager) return;

    int monitorCount = m_monitorManager->GetMonitorCount();
    m_tempSettings.resize(monitorCount);

    for (int i = 0; i < monitorCount; ++i) {
        // Get monitor info (placeholder - would come from MonitorManager)
        std::wstringstream ss;
        ss << L"Monitor " << (i + 1);

        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<LPWSTR>(ss.str().c_str());
        ListView_InsertItem(hList, &lvi);

        // Resolution (placeholder)
        ListView_SetItemText(hList, i, 1, const_cast<LPWSTR>(L"1920x1080"));

        // Primary indicator
        ListView_SetItemText(hList, i, 2, i == 0 ? const_cast<LPWSTR>(L"Yes") : const_cast<LPWSTR>(L"No"));
    }

    // Select first monitor
    ListView_SetItemState(hList, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void SettingsWindow::UpdateWallpaperPath() {
    if (m_currentMonitorIndex < 0 || m_currentMonitorIndex >= static_cast<int>(m_tempSettings.size())) {
        return;
    }

    HWND hEdit = GetDlgItem(m_hwnd, IDC_WALLPAPER_PATH);
    if (hEdit) {
        SetWindowText(hEdit, m_tempSettings[m_currentMonitorIndex].wallpaperPath.c_str());
    }
}

void SettingsWindow::UpdateScalingMode() {
    if (m_currentMonitorIndex < 0 || m_currentMonitorIndex >= static_cast<int>(m_tempSettings.size())) {
        return;
    }

    HWND hCombo = GetDlgItem(m_hwnd, IDC_SCALING_COMBO);
    if (hCombo) {
        SendMessage(hCombo, CB_SETCURSEL, m_tempSettings[m_currentMonitorIndex].scalingMode, 0);
    }
}

void SettingsWindow::UpdateResourceSettings() {
    if (!m_config) return;

    // Game Mode checkbox
    HWND hGameMode = GetDlgItem(m_hwnd, IDC_GAMEMODE_CHECK);
    if (hGameMode) {
        SendMessage(hGameMode, BM_SETCHECK, m_config->GetGameModeEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    // Battery-Aware checkbox
    HWND hBattery = GetDlgItem(m_hwnd, IDC_BATTERY_CHECK);
    if (hBattery) {
        SendMessage(hBattery, BM_SETCHECK, m_config->GetBatteryAwareEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    // Startup checkbox
    HWND hStartup = GetDlgItem(m_hwnd, IDC_STARTUP_CHECK);
    if (hStartup) {
        SendMessage(hStartup, BM_SETCHECK, m_config->GetStartWithWindows() ? BST_CHECKED : BST_UNCHECKED, 0);
    }

    UpdateControlStates();
}

void SettingsWindow::UpdateControlStates() {
    // Enable/disable battery threshold based on Battery-Aware checkbox
    HWND hBatteryCheck = GetDlgItem(m_hwnd, IDC_BATTERY_CHECK);
    HWND hThreshold = GetDlgItem(m_hwnd, IDC_BATTERY_THRESHOLD);
    
    if (hBatteryCheck && hThreshold) {
        BOOL enabled = (SendMessage(hBatteryCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
        EnableWindow(hThreshold, enabled);
    }

    // Enable Apply button if settings changed
    HWND hApply = GetDlgItem(m_hwnd, IDC_APPLY_BUTTON);
    if (hApply) {
        EnableWindow(hApply, m_settingsChanged);
    }
}

void SettingsWindow::OnMonitorSelectionChanged() {
    int newIndex = GetSelectedMonitorIndex();
    if (newIndex < 0) return;

    // Save current monitor settings before switching
    SaveCurrentMonitorSettings();

    // Load new monitor settings
    m_currentMonitorIndex = newIndex;
    LoadMonitorSettings(newIndex);
}

void SettingsWindow::OnBrowseWallpaper() {
    std::wstring filePath = OpenFileDialog();
    if (filePath.empty()) return;

    // Update current monitor's wallpaper path
    if (m_currentMonitorIndex >= 0 && m_currentMonitorIndex < static_cast<int>(m_tempSettings.size())) {
        m_tempSettings[m_currentMonitorIndex].wallpaperPath = filePath;
        UpdateWallpaperPath();
        m_settingsChanged = true;
        UpdateControlStates();
    }
}

void SettingsWindow::OnScalingChanged() {
    HWND hCombo = GetDlgItem(m_hwnd, IDC_SCALING_COMBO);
    if (!hCombo) return;

    int selection = static_cast<int>(SendMessage(hCombo, CB_GETCURSEL, 0, 0));
    if (selection != CB_ERR && m_currentMonitorIndex >= 0 && m_currentMonitorIndex < static_cast<int>(m_tempSettings.size())) {
        m_tempSettings[m_currentMonitorIndex].scalingMode = selection;
        m_settingsChanged = true;
        UpdateControlStates();
    }
}

void SettingsWindow::OnApply() {
    if (!m_config) return;

    // Save current monitor settings
    SaveCurrentMonitorSettings();

    // Apply all monitor settings to configuration
    for (size_t i = 0; i < m_tempSettings.size(); ++i) {
        // TODO: Save to configuration
        Logger::Info("Applying settings for monitor " + std::to_string(i));
        
        // Load wallpaper if path is set
        if (!m_tempSettings[i].wallpaperPath.empty()) {
            // Get Application instance to access DesktopManager
            auto* desktopMgr = Application::GetInstance().GetDesktopManager();
            if (desktopMgr) {
                if (desktopMgr->SetWallpaper(static_cast<int>(i), m_tempSettings[i].wallpaperPath)) {
                    std::wstring wPath = m_tempSettings[i].wallpaperPath;
                    std::string path(wPath.begin(), wPath.end());
                    Logger::Info("Loaded wallpaper: " + path);
                } else {
                    Logger::Error("Failed to load wallpaper for monitor " + std::to_string(i));
                }
            }
        }
    }

    // Apply resource management settings
    HWND hGameMode = GetDlgItem(m_hwnd, IDC_GAMEMODE_CHECK);
    if (hGameMode) {
        bool enabled = (SendMessage(hGameMode, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_config->SetGameModeEnabled(enabled);
    }

    HWND hBattery = GetDlgItem(m_hwnd, IDC_BATTERY_CHECK);
    if (hBattery) {
        bool enabled = (SendMessage(hBattery, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_config->SetBatteryAwareEnabled(enabled);
    }

    HWND hStartup = GetDlgItem(m_hwnd, IDC_STARTUP_CHECK);
    if (hStartup) {
        bool enabled = (SendMessage(hStartup, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_config->SetStartWithWindows(enabled);
    }

    // Save configuration
    m_config->Save();

    m_settingsChanged = false;
    UpdateControlStates();

    SetDlgItemText(m_hwnd, IDC_STATUS_TEXT, L"Settings applied successfully!");
    Logger::Info("Settings applied");
}

void SettingsWindow::OnCancel() {
    // Reload settings from configuration (discard changes)
    if (m_config) {
        UpdateResourceSettings();
    }

    if (m_monitorManager && m_monitorManager->GetMonitorCount() > 0) {
        LoadMonitorSettings(m_currentMonitorIndex);
    }

    m_settingsChanged = false;
    UpdateControlStates();
    Hide();
}

std::wstring SettingsWindow::OpenFileDialog() {
    OPENFILENAME ofn = {};
    wchar_t szFile[MAX_PATH] = {};

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Video Files\0*.mp4;*.mkv;*.avi;*.mov;*.webm\0Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"Select Wallpaper File";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileName(&ofn)) {
        return std::wstring(szFile);
    }

    return L"";
}

int SettingsWindow::GetSelectedMonitorIndex() {
    HWND hList = GetDlgItem(m_hwnd, IDC_MONITOR_LIST);
    if (!hList) return -1;

    return ListView_GetNextItem(hList, -1, LVNI_SELECTED);
}

void SettingsWindow::SaveCurrentMonitorSettings() {
    if (m_currentMonitorIndex < 0 || m_currentMonitorIndex >= static_cast<int>(m_tempSettings.size())) {
        return;
    }

    // Wallpaper path is already saved in m_tempSettings when changed
    // Scaling mode is already saved in m_tempSettings when changed
}

void SettingsWindow::LoadMonitorSettings(int monitorIndex) {
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(m_tempSettings.size())) {
        return;
    }

    m_currentMonitorIndex = monitorIndex;

    // Update UI with monitor's settings
    UpdateWallpaperPath();
    UpdateScalingMode();

    // Populate scaling combo if not already done
    HWND hCombo = GetDlgItem(m_hwnd, IDC_SCALING_COMBO);
    if (hCombo && SendMessage(hCombo, CB_GETCOUNT, 0, 0) == 0) {
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Fill"));
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Fit"));
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Stretch"));
        SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Center"));
        SendMessage(hCombo, CB_SETCURSEL, 0, 0);
    }
}

} // namespace PixelMotion
