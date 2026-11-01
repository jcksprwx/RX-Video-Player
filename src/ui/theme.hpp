#pragma once

#include <windows.h>
#include <dwmapi.h>

namespace rx::theme {

// Color definitions matching professional PotPlayer-inspired palette
constexpr COLORREF kBgColor           = RGB(27, 27, 27);    // #1B1B1B - Main window surface
constexpr COLORREF kToolbarBg         = RGB(32, 32, 32);    // #202020 - Title bar & toolbars
constexpr COLORREF kPanelBg           = RGB(32, 32, 32);    // #202020 - Panels & sidebar
constexpr COLORREF kButtonBg          = RGB(36, 36, 36);    // #242424 - Flat buttons & badges
constexpr COLORREF kHoverBg           = RGB(56, 56, 56);    // #383838 - Button hover
constexpr COLORREF kPanelBgHover      = RGB(56, 56, 56);    // #383838 - List/panel hover
constexpr COLORREF kPressedBg         = RGB(24, 24, 24);    // #181818 - Button pressed
constexpr COLORREF kBorderColor       = RGB(65, 65, 65);    // #414141 - Subtle dividers & borders
constexpr COLORREF kTextPrimary       = RGB(242, 242, 242); // #F2F2F2 - Crisp primary text
constexpr COLORREF kTextSecondary     = RGB(165, 165, 165); // #A5A5A5 - Muted metadata text
constexpr COLORREF kTextMuted         = RGB(165, 165, 165); // #A5A5A5
constexpr COLORREF kIconsPrimary      = RGB(216, 216, 216); // #D8D8D8 - Monochrome icons
constexpr COLORREF kAccentColor       = RGB(232, 208, 0);   // #E8D000 - Bright golden yellow playback accent
constexpr COLORREF kPlaybackAccent    = RGB(232, 208, 0);   // #E8D000
constexpr COLORREF kAccentHover       = RGB(255, 235, 59);  // #FFEB3B - Bright yellow hover
constexpr COLORREF kCurrentTimeAccent = RGB(131, 198, 255); // #83C6FF - Subtle blue timecode accent
constexpr COLORREF kErrorAccent       = RGB(229, 72, 77);   // #E5484D - Close button hover & error accent
constexpr COLORREF kVideoBg           = RGB(0, 0, 0);       // #000000 - Pure black video canvas
constexpr COLORREF kSeekBg            = RGB(45, 45, 45);    // #2D2D2D - Progress track background
constexpr COLORREF kSeekHover         = RGB(65, 65, 65);    // #414141
constexpr COLORREF kActiveItem        = RGB(48, 44, 22);    // Dark gold/charcoal active selection

// Enable Windows 10/11 Dark Titlebar & modern DWM frame
inline void enableImmersiveDarkMode(HWND hwnd) {
    BOOL useDarkMode = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE is 20 in Windows 11 / Windows 10 20H1+
    DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    // Also try legacy attribute 19 for older Windows 10 builds
    DwmSetWindowAttribute(hwnd, 19, &useDarkMode, sizeof(useDarkMode));
}

} // namespace rx::theme
