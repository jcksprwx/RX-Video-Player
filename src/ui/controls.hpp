#pragma once

#include "theme.hpp"
#include <windows.h>
#include <string>
#include <functional>

namespace rx::ui {

class RXSeekBar {
public:
    using SeekCallback = std::function<void(double targetSec)>;

    RXSeekBar() = default;
    ~RXSeekBar();

    bool create(HWND parentHwnd, int x, int y, int width, int height, UINT id);
    void setPosition(double currentSec, double durationSec);
    void setSeekCallback(SeekCallback cb) { m_seekCb = std::move(cb); }
    HWND getHwnd() const { return m_hwnd; }
    void move(int x, int y, int width, int height);

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onLButtonDown(int x, int y);
    void onMouseMove(int x, int y, WPARAM flags);
    void onLButtonUp(int x, int y);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    double m_currentSec = 0.0;
    double m_durationSec = 0.0;
    bool m_dragging = false;
    double m_dragSec = 0.0;
    bool m_hovering = false;
    int m_hoverX = 0;
    SeekCallback m_seekCb;
};

class RXIconButton {
public:
    using ClickCallback = std::function<void()>;

    RXIconButton() = default;
    ~RXIconButton();

    bool create(HWND parentHwnd, int x, int y, int width, int height, const std::wstring& iconText, UINT id, const std::wstring& tooltip = L"");
    void setText(const std::wstring& text);
    void setTooltip(const std::wstring& tooltip);
    void setCallback(ClickCallback cb) { m_clickCb = std::move(cb); }
    HWND getHwnd() const { return m_hwnd; }
    void move(int x, int y, int width, int height);
    void setAccent(bool isAccent);

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
    ClickCallback m_clickCb;
};

} // namespace rx::ui
