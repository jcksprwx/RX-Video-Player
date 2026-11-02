#include "association_manager.hpp"
#include "logger.hpp"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace rx {

static const std::vector<FileFormatInfo> s_formats = {
    // Video formats requested
    { L".mkv",  L"video/x-matroska", true },
    { L".mp4",  L"video/mp4",        true },
    { L".avi",  L"video/x-msvideo",  true },
    { L".mov",  L"video/quicktime",  true },
    { L".wmv",  L"video/x-ms-wmv",   true },
    { L".webm", L"video/webm",       true },
    { L".m4v",  L"video/x-m4v",      true },
    { L".mpeg", L"video/mpeg",       true },
    { L".mpg",  L"video/mpeg",       true },
    { L".ts",   L"video/mp2t",       true },
    { L".m2ts", L"video/mp2t",       true },
    { L".flv",  L"video/x-flv",      true },

    // Audio formats requested
    { L".mp3",  L"audio/mpeg",       false },
    { L".wav",  L"audio/wav",        false },
    { L".flac", L"audio/flac",       false },
    { L".aac",  L"audio/aac",        false },
    { L".m4a",  L"audio/mp4",        false },
    { L".ogg",  L"audio/ogg",        false },
    { L".opus", L"audio/opus",       false },
    { L".wma",  L"audio/x-ms-wma",   false }
};

const std::vector<FileFormatInfo>& AssociationManager::getSupportedFormats() {
    return s_formats;
}

std::wstring AssociationManager::getExecutablePath() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::wstring(path);
}

static bool setRegString(HKEY root, const std::wstring& subkey, const std::wstring& valueName, const std::wstring& valueData) {
    HKEY hKey = nullptr;
    LSTATUS status = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr,
                                     REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                                     nullptr, &hKey, nullptr);
    if (status != ERROR_SUCCESS) return false;

    status = RegSetValueExW(hKey, valueName.empty() ? nullptr : valueName.c_str(), 0,
                            REG_SZ, reinterpret_cast<const BYTE*>(valueData.c_str()),
                            static_cast<DWORD>((valueData.length() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return (status == ERROR_SUCCESS);
}

static bool deleteRegKeyTree(HKEY root, const std::wstring& subkey) {
    return (RegDeleteTreeW(root, subkey.c_str()) == ERROR_SUCCESS);
}

bool AssociationManager::registerFileAssociations(bool userOnly, const std::wstring& customExePath) {
    std::wstring exePath = customExePath.empty() ? getExecutablePath() : customExePath;
    if (exePath.empty()) return false;

    HKEY root = userOnly ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring classesRoot = L"Software\\Classes";
    std::wstring commandStr = L"\"" + exePath + L"\" \"%1\"";
    std::wstring iconStr = L"\"" + exePath + L"\",0";

    // 1. Register Application: Software\Classes\Applications\rx_player.exe
    std::wstring appKey = classesRoot + L"\\Applications\\rx_player.exe";
    setRegString(root, appKey, L"", L"RX Media Player");
    setRegString(root, appKey, L"FriendlyAppName", L"RX Media Player");
    setRegString(root, appKey, L"ApplicationCompany", L"RX Media Player");
    setRegString(root, appKey + L"\\DefaultIcon", L"", iconStr);
    setRegString(root, appKey + L"\\shell\\open", L"", L"Play with &RX Media Player");
    setRegString(root, appKey + L"\\shell\\open", L"FriendlyAppName", L"RX Media Player");
    setRegString(root, appKey + L"\\shell\\open\\command", L"", commandStr);

    // Register SupportedTypes under Applications\rx_player.exe
    std::wstring suppTypesKey = appKey + L"\\SupportedTypes";
    for (const auto& fmt : s_formats) {
        setRegString(root, suppTypesKey, fmt.ext, L"");
    }

    // 2. Register ProgID: Software\Classes\RXMediaPlayer.AssocFile
    std::wstring progIdKey = classesRoot + L"\\RXMediaPlayer.AssocFile";
    setRegString(root, progIdKey, L"", L"RX Media Player Media File");
    setRegString(root, progIdKey, L"FriendlyTypeName", L"RX Media Player Media File");
    setRegString(root, progIdKey + L"\\DefaultIcon", L"", iconStr);
    setRegString(root, progIdKey + L"\\shell\\open", L"", L"Play with &RX Media Player");
    setRegString(root, progIdKey + L"\\shell\\open\\command", L"", commandStr);

    // 3. Register OpenWithProgids & OpenWithList for each supported extension
    for (const auto& fmt : s_formats) {
        std::wstring extKey = classesRoot + L"\\" + fmt.ext;
        setRegString(root, extKey + L"\\OpenWithProgids", L"RXMediaPlayer.AssocFile", L"");
        setRegString(root, extKey + L"\\OpenWithList\\rx_player.exe", L"", L"");
    }

    // 4. Register Capabilities for Windows Default Apps / RegisteredApplications
    std::wstring capsKey = L"Software\\RX Media Player\\Capabilities";
    setRegString(root, capsKey, L"ApplicationName", L"RX Media Player");
    setRegString(root, capsKey, L"ApplicationDescription", L"RX Media Player - Native 64-bit Multimedia Player");
    setRegString(root, capsKey, L"ApplicationIcon", iconStr);

    std::wstring fileAssocKey = capsKey + L"\\FileAssociations";
    for (const auto& fmt : s_formats) {
        setRegString(root, fileAssocKey, fmt.ext, L"RXMediaPlayer.AssocFile");
    }

    setRegString(root, L"Software\\RegisteredApplications", L"RX Media Player", capsKey);

    // 5. Notify Windows Shell of association change
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    RX_LOG_INFO("File associations successfully registered in Windows Registry for " + std::to_string(s_formats.size()) + " formats");
    return true;
}

bool AssociationManager::unregisterFileAssociations(bool userOnly) {
    HKEY root = userOnly ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring classesRoot = L"Software\\Classes";

    // 1. Remove OpenWithProgids and OpenWithList entries for our extensions
    for (const auto& fmt : s_formats) {
        HKEY hKey = nullptr;
        std::wstring openWithProgids = classesRoot + L"\\" + fmt.ext + L"\\OpenWithProgids";
        if (RegOpenKeyExW(root, openWithProgids.c_str(), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            RegDeleteValueW(hKey, L"RXMediaPlayer.AssocFile");
            RegCloseKey(hKey);
        }

        std::wstring openWithList = classesRoot + L"\\" + fmt.ext + L"\\OpenWithList\\rx_player.exe";
        RegDeleteKeyW(root, openWithList.c_str());
    }

    // 2. Remove ProgID and Application entries
    deleteRegKeyTree(root, classesRoot + L"\\RXMediaPlayer.AssocFile");
    deleteRegKeyTree(root, classesRoot + L"\\Applications\\rx_player.exe");

    // 3. Remove Capabilities & RegisteredApplications
    deleteRegKeyTree(root, L"Software\\RX Media Player\\Capabilities");
    HKEY hRegApp = nullptr;
    if (RegOpenKeyExW(root, L"Software\\RegisteredApplications", 0, KEY_SET_VALUE, &hRegApp) == ERROR_SUCCESS) {
        RegDeleteValueW(hRegApp, L"RX Media Player");
        RegCloseKey(hRegApp);
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    RX_LOG_INFO("File associations successfully unregistered from Windows Registry");
    return true;
}

bool AssociationManager::isRegistered(bool userOnly) {
    HKEY root = userOnly ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
    std::wstring appKey = L"Software\\Classes\\Applications\\rx_player.exe\\shell\\open\\command";
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(root, appKey.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

} // namespace rx
