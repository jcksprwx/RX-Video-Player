#pragma once

#include <string>
#include <vector>

namespace rx {

struct FileFormatInfo {
    const wchar_t* ext;
    const wchar_t* mime;
    bool isVideo;
};

class AssociationManager {
public:
    static const std::vector<FileFormatInfo>& getSupportedFormats();

    // Registers RX Media Player in Windows registry for "Open with" and Default Apps.
    // userOnly = true registers under HKEY_CURRENT_USER\Software\Classes (no elevation required).
    // userOnly = false registers under HKEY_LOCAL_MACHINE\Software\Classes (requires elevation).
    static bool registerFileAssociations(bool userOnly = true, const std::wstring& customExePath = L"");

    // Removes registrations created by RX Media Player
    static bool unregisterFileAssociations(bool userOnly = true);

    // Checks if RX Media Player is registered in Windows "Open with" / Applications
    static bool isRegistered(bool userOnly = true);

    // Gets the current executable path
    static std::wstring getExecutablePath();
};

} // namespace rx
