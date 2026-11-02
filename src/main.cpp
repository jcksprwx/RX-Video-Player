#include "core/logger.hpp"
#include "core/settings.hpp"
#include "core/utils.hpp"
#include "core/association_manager.hpp"
#include "ui/main_window.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <filesystem>
#include <vector>
#include <string>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow) {
    // Enable Per-Monitor High-DPI Awareness v2
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    // Parse command line arguments
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    // Check for registration and command-line flags
    if (argv && argc > 1) {
        if (_wcsicmp(argv[1], L"--register") == 0 || _wcsicmp(argv[1], L"/register") == 0 ||
            _wcsicmp(argv[1], L"--repair") == 0 || _wcsicmp(argv[1], L"/repair") == 0 ||
            _wcsicmp(argv[1], L"--register-associations") == 0) {
            bool ok = rx::AssociationManager::registerFileAssociations(true);
            MessageBoxW(nullptr,
                ok ? L"RX Media Player has been registered in Windows 'Open with' and Default Apps for all supported video and audio formats."
                   : L"Failed to register Windows file associations.",
                L"RX Media Player — File Associations",
                ok ? (MB_ICONINFORMATION | MB_OK) : (MB_ICONERROR | MB_OK));
            LocalFree(argv);
            return ok ? 0 : 1;
        }

        if (_wcsicmp(argv[1], L"--unregister") == 0 || _wcsicmp(argv[1], L"/unregister") == 0) {
            bool ok = rx::AssociationManager::unregisterFileAssociations(true);
            MessageBoxW(nullptr,
                ok ? L"RX Media Player file associations have been removed from Windows Registry."
                   : L"Failed to unregister file associations.",
                L"RX Media Player — File Associations",
                ok ? (MB_ICONINFORMATION | MB_OK) : (MB_ICONERROR | MB_OK));
            LocalFree(argv);
            return ok ? 0 : 1;
        }
    }

    // Single-Instance Check & Inter-Process Communication (IPC)
    HWND hExisting = FindWindowW(L"RXMediaPlayerClass", nullptr);
    if (hExisting && IsWindow(hExisting)) {
        if (argv && argc > 1) {
            std::wstring combinedPaths;
            for (int i = 1; i < argc; ++i) {
                if (argv[i][0] == L'-' || argv[i][0] == L'/') continue;
                if (!combinedPaths.empty()) combinedPaths += L"\n";
                combinedPaths += argv[i];
            }

            if (!combinedPaths.empty()) {
                COPYDATASTRUCT cds{};
                cds.dwData = 0x5258504C; // 'RXPL'
                cds.cbData = static_cast<DWORD>((combinedPaths.length() + 1) * sizeof(wchar_t));
                cds.lpData = const_cast<wchar_t*>(combinedPaths.c_str());
                SendMessageW(hExisting, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
            }
        }

        if (IsIconic(hExisting)) ShowWindow(hExisting, SW_RESTORE);
        SetForegroundWindow(hExisting);

        if (argv) LocalFree(argv);
        return 0;
    }

    // Automatic self-registration for user if not yet registered in Windows
    if (!rx::AssociationManager::isRegistered(true)) {
        rx::AssociationManager::registerFileAssociations(true);
    }

    // Initialize Logger
    rx::Logger::instance().init("rx_player.log");
    RX_LOG_INFO("==================================================");
    RX_LOG_INFO(std::string("RX Media Player ") + rx::kAppVersion + " (Windows 64-bit) starting up");
    RX_LOG_INFO("==================================================");

    // Create Main Window
    rx::ui::RXMainWindow window;
    if (!window.create(hInstance, nCmdShow)) {
        RX_LOG_ERROR("Failed to initialize RX Media Player main window");
        MessageBoxW(nullptr, L"Failed to initialize RX Media Player window.", L"RX Media Player Error", MB_ICONERROR | MB_OK);
        if (argv) LocalFree(argv);
        return 1;
    }

    // Process incoming media files
    if (argv && argc > 1) {
        std::vector<std::string> validFiles;
        for (int i = 1; i < argc; ++i) {
            if (argv[i][0] == L'-' || argv[i][0] == L'/') continue;

            std::wstring wpath = argv[i];
            std::string pathUtf8 = rx::utils::wideToUtf8(wpath);

            std::error_code ec;
            if (std::filesystem::exists(std::filesystem::path(wpath), ec)) {
                validFiles.push_back(pathUtf8);
            } else if (pathUtf8.rfind("http://", 0) == 0 || pathUtf8.rfind("https://", 0) == 0) {
                validFiles.push_back(pathUtf8);
            } else {
                RX_LOG_WARN("File not found or inaccessible: " + pathUtf8);
                std::wstring err = L"The specified media file does not exist:\n\n" + wpath;
                MessageBoxW(window.getHwnd(), err.c_str(), L"File Not Found — RX Media Player", MB_ICONWARNING | MB_OK);
            }
        }

        if (!validFiles.empty()) {
            RX_LOG_INFO("Opening primary file: " + validFiles[0]);
            window.openFile(validFiles[0]);
            for (size_t i = 1; i < validFiles.size(); ++i) {
                window.addToPlaylist(validFiles[i]);
            }
        }
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
