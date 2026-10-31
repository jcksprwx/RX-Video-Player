#pragma once

#include <windows.h>
#include <dwmapi.h>

namespace rx::theme {

// Color definitions (BGR for Windows COLORREF / RGB macro)
constexpr COLORREF kBgColor        = RGB(20, 21, 24);    // #141518
constexpr COLORREF kPanelBg        = RGB(30, 32, 36);    // #1e2024
constexpr COLORREF kPanelBgHover   = RGB(42, 45, 52);    // #2a2d34
constexpr COLORREF kBorderColor    = RGB(45, 49, 57);    // #2d3139
constexpr COLORREF kTextPrimary    = RGB(240, 242, 245); // #f0f2f5
constexpr COLORREF kTextMuted      = RGB(154, 160, 166); // #9aa0a6
constexpr COLORREF kAccentColor    = RGB(59, 130, 246);  // #3b82f6 (RX Blue)
constexpr COLORREF kAccentHover    = RGB(96, 165, 250);  // #60a5fa
constexpr COLORREF kSeekBg         = RGB(55, 65, 81);    // #374151
constexpr COLORREF kSeekHover      = RGB(75, 85, 99);
constexpr COLORREF kActiveItem     = RGB(37, 50, 75);    // Playlist active selection

// Enable Windows 10/11 Dark Titlebar via DWM
inline void enableImmersiveDarkMode(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE is 20 in Windows 11 / Windows 10 20H1+
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    // Also try legacy attribute 19 for older Windows 10 builds
    DwmSetWindowAttribute(hwnd, 19, &useDarkMode, sizeof(useDarkMode));
}

} // namespace rx::theme
