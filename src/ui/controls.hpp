#pragma once

#include "theme.hpp"
#include <windows.h>
#include <string>
#include <functional>
#include <vector>

namespace rx::ui {

// -------------------------------------------------------------------------
// RXIconButton: High-DPI, flat dark button with hover/active states
// -------------------------------------------------------------------------
class RXIconButton {
public:
    using ClickCallback = std::function<void()>;

    RXIconButton() = default;
    ~RXIconButton();

    bool create(HWND parentHwnd, int x, int y, int width, int height,
                const std::wstring& iconText, UINT id, const std::wstring& tooltip = L"");
    void setText(const std::wstring& text);
    void setTooltip(const std::wstring& tooltip);
    void setCallback(ClickCallback cb) { m_clickCb = std::move(cb); }
    HWND getHwnd() const { return m_hwnd; }
    void move(int x, int y, int width, int height);
    void setAccent(bool isAccent);
    void setCloseButton(bool isClose) { m_isClose = isClose; }
    void setPinButton(bool isPin) { m_isPin = isPin; }
    void setFlatTitleBar(bool isTitleBar) { m_isTitleBar = isTitleBar; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    std::wstring m_text;
    std::wstring m_tooltip;
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_accent = false;
    bool m_isClose = false;
    bool m_isPin = false;
    bool m_isTitleBar = false;
    ClickCallback m_clickCb;
};

// -------------------------------------------------------------------------
// RXTimelineStrip: Timeline & Volume Strip (26-28px height)
// Left: Seek track with yellow progress & draggable handle + hover timecode
// Right: Compact speaker button & volume slider with circular handle & % tooltip
// -------------------------------------------------------------------------
class RXTimelineStrip {
public:
    using SeekCallback = std::function<void(double targetSec)>;
    using VolumeCallback = std::function<void(int volPercent)>;
    using MuteCallback = std::function<void()>;

    RXTimelineStrip() = default;
    ~RXTimelineStrip();

    bool create(HWND parentHwnd, int x, int y, int width, int height, UINT id);
    void setPosition(double currentSec, double durationSec);
    void setVolume(int volumePercent, bool muted);
    void setSeekCallback(SeekCallback cb) { m_seekCb = std::move(cb); }
    void setVolumeCallback(VolumeCallback cb) { m_volumeCb = std::move(cb); }
    void setMuteCallback(MuteCallback cb) { m_muteCb = std::move(cb); }
    HWND getHwnd() const { return m_hwnd; }
    void move(int x, int y, int width, int height);
    bool isDragging() const { return m_timelineDragging || m_volumeDragging; }
    bool isHovered() const { return m_timelineHover || m_volumeHover || m_speakerHover; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onLButtonDown(int x, int y);
    void onMouseMove(int x, int y, WPARAM flags);
    void onLButtonUp(int x, int y);
    void onMouseWheel(short delta);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    double m_currentSec = 0.0;
    double m_durationSec = 0.0;
    int m_volume = 100;
    bool m_muted = false;

    // Timeline state
    bool m_timelineDragging = false;
    double m_dragSec = 0.0;
    bool m_timelineHover = false;
    int m_hoverX = 0;

    // Volume state
    bool m_volumeDragging = false;
    bool m_volumeHover = false;
    bool m_speakerHover = false;

    SeekCallback m_seekCb;
    VolumeCallback m_volumeCb;
    MuteCallback m_muteCb;
};

// For backward compatibility alias
using RXSeekBar = RXTimelineStrip;

// -------------------------------------------------------------------------
// RXTitleBar: Custom Top Title Bar (38-40px height)
// Logo + App Name + Dropdown Menu Arrow + Format Badge + Filename + Pin/Min/Max/Close
// -------------------------------------------------------------------------
class RXTitleBar {
public:
    using MenuClickCallback = std::function<void(int screenX, int screenY)>;
    using ActionCallback = std::function<void()>;

    RXTitleBar() = default;
    ~RXTitleBar();

    bool create(HWND parentHwnd, int width, int height, UINT id);
    void setMediaInfo(const std::wstring& formatBadge, const std::wstring& title);
    void setPinned(bool pinned);
    void setMaximized(bool maximized);
    void move(int x, int y, int width, int height);
    HWND getHwnd() const { return m_hwnd; }
    bool isHovered() const { return m_hoverBtn >= 0 || m_pressedBtn >= 0; }

    void setMenuCallback(MenuClickCallback cb) { m_menuCb = std::move(cb); }
    void setPinCallback(ActionCallback cb) { m_pinCb = std::move(cb); }
    void setMinCallback(ActionCallback cb) { m_minCb = std::move(cb); }
    void setMaxCallback(ActionCallback cb) { m_maxCb = std::move(cb); }
    void setCloseCallback(ActionCallback cb) { m_closeCb = std::move(cb); }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onMouseMove(int x, int y);
    void onLButtonDown(int x, int y);
    void onLButtonUp(int x, int y);
    void onLButtonDblClk(int x, int y);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    std::wstring m_formatBadge;
    std::wstring m_title = L"RX Media Player";
    bool m_isPinned = false;
    bool m_isMaximized = false;

    // Button hit testing
    int m_hoverBtn = -1; // 0=Menu, 1=Pin, 2=Min, 3=Max, 4=Close
    int m_pressedBtn = -1;

    MenuClickCallback m_menuCb;
    ActionCallback m_pinCb;
    ActionCallback m_minCb;
    ActionCallback m_maxCb;
    ActionCallback m_closeCb;
};

} // namespace rx::ui
