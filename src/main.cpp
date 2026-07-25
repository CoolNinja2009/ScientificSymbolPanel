#include "App/App.h"
#include <windows.h>

int APIENTRY wWinMain(
    HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/,
    LPWSTR    /*lpCmdLine*/,
    int       nCmdShow)
{
    // Prevent multiple instances
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ScientificSymbolPanel_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        // TODO: Send WM_SSP_ACTIVATE to existing instance
        return 0;
    }

    // Set DPI awareness (Per-Monitor v2 for best scaling)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize COM
    ssp::App app;
    if (!app.Initialize(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize Scientific Symbol Panel.",
            L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Run message loop
    int result = app.Run();

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }

    return result;
}
