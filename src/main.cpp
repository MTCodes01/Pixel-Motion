#include "Application.h"
#include "core/Logger.h"

#include <Windows.h>

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
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    // Ensure single instance
    HANDLE hMutex = CreateMutex(nullptr, TRUE, L"PixelMotion_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(nullptr,
            L"Pixel Motion is already running.\nCheck the system tray.",
            L"Pixel Motion",
            MB_OK | MB_ICONINFORMATION);
        if (hMutex) CloseHandle(hMutex);
        return 0;
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
