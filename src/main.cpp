#include "core/logger.hpp"
#include "core/settings.hpp"
#include "core/utils.hpp"
#include "ui/main_window.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow) {
    // Enable Per-Monitor High-DPI Awareness v2
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // Initialize Logger
    rx::Logger::instance().init("rx_player.log");
    RX_LOG_INFO("==================================================");
    RX_LOG_INFO("RX Media Player 1.0.0 (Windows 64-bit) starting up");
    RX_LOG_INFO("==================================================");

    // Create Main Window
    rx::ui::RXMainWindow window;
    if (!window.create(hInstance, nCmdShow)) {
        RX_LOG_ERROR("Failed to initialize RX Media Player main window");
        MessageBoxW(nullptr, L"Failed to initialize RX Media Player window.", L"RX Media Player Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Process command line arguments for initial media file
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        // First argument after executable
        std::wstring initialPath = argv[1];
        std::string pathUtf8 = rx::utils::wideToUtf8(initialPath);
        RX_LOG_INFO("Opening command-line file: " + pathUtf8);
        window.openFile(pathUtf8);
    }
    if (argv) LocalFree(argv);

    // Run Message Loop
    int result = window.runEventLoop();

    RX_LOG_INFO("RX Media Player terminated cleanly");
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR /*lpCmdLine*/, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
