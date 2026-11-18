#include "Application.h"
#include "core/Logger.h"

#include <Windows.h>
#include <filesystem>

using namespace PixelMotion;

/**
 * Application entry point
 */
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    // Set working directory to the application executable directory
    // (Crucial when launched at Windows startup from Registry Run key, where CWD defaults to C:\Windows\System32)
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileName(nullptr, exePath, MAX_PATH) > 0) {
        std::filesystem::path appDir = std::filesystem::path(exePath).parent_path();
        SetCurrentDirectoryW(appDir.c_str());
    }

    // Ensure single instance (allow up to 5s for previous instance to exit during update/restart)
    HANDLE hMutex = CreateMutex(nullptr, TRUE, L"PixelMotion_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        DWORD waitResult = WaitForSingleObject(hMutex, 5000);
        if (waitResult != WAIT_OBJECT_0 && waitResult != WAIT_ABANDONED) {
            MessageBox(nullptr,
                L"Pixel Motion is already running.\nCheck the system tray.",
                L"Pixel Motion",
                MB_OK | MB_ICONINFORMATION);
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
    }

    // Initialize logger
    Logger::Initialize();
    Logger::Info("=== Pixel Motion Starting ===");

    // Create and run application
    int exitCode = 0;
    {
        Application app;
        if (app.Initialize()) {
            exitCode = app.Run();
        } else {
            Logger::Error("Application initialization failed");
            MessageBox(nullptr,
                L"Failed to initialize Pixel Motion.\nCheck logs for details.",
                L"Pixel Motion - Error",
                MB_OK | MB_ICONERROR);
            exitCode = -1;
        }
    }

    Logger::Info("=== Pixel Motion Exited ===");
    Logger::Shutdown();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return exitCode;
}
