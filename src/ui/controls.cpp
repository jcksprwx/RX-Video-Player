#include "controls.hpp"
#include "../core/utils.hpp"
#include <windowsx.h>
#include <cmath>

namespace rx::ui {

// -------------------------------------------------------------
// RXSeekBar Implementation
// -------------------------------------------------------------

static const wchar_t* kSeekBarClass = L"RXSeekBarClass";
static bool s_seekBarClassRegistered = false;

bool RXSeekBar::create(HWND parentHwnd, int x, int y, int width, int height, UINT id) {
    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_seekBarClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXSeekBar::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.lpszClassName = kSeekBarClass;
        RegisterClassExW(&wc);
        s_seekBarClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kSeekBarClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, width, height,
        parentHwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, this
    );

    return (m_hwnd != nullptr);
}

RXSeekBar::~RXSeekBar() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXSeekBar::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void RXSeekBar::setPosition(double currentSec, double durationSec) {
    if (!m_dragging) {
        m_currentSec = currentSec;
        m_durationSec = durationSec;
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

LRESULT CALLBACK RXSeekBar::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXSeekBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXSeekBar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXSeekBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                pThis->onPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1; // Double buffered in onPaint
            case WM_LBUTTONDOWN:
                pThis->onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSEMOVE:
                pThis->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
                return 0;
            case WM_LBUTTONUP:
                pThis->onLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSELEAVE:
                pThis->m_hovering = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXSeekBar::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Clear background
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Track dimensions
    int trackHeight = m_hovering || m_dragging ? 6 : 4;
    int trackY = (height - trackHeight) / 2;

    RECT trackRect{ 0, trackY, width, trackY + trackHeight };
    HBRUSH trackBgBrush = CreateSolidBrush(theme::kSeekBg);
    FillRect(memDC, &trackRect, trackBgBrush);
    DeleteObject(trackBgBrush);

    // Played portion
    double activeSec = m_dragging ? m_dragSec : m_currentSec;
    double progress = (m_durationSec > 0.0) ? (activeSec / m_durationSec) : 0.0;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    int playedWidth = static_cast<int>(width * progress);
    RECT playedRect{ 0, trackY, playedWidth, trackY + trackHeight };
    HBRUSH playedBrush = CreateSolidBrush(theme::kAccentColor);
    FillRect(memDC, &playedRect, playedBrush);
    DeleteObject(playedBrush);

    // Scrubber thumb
    if (m_hovering || m_dragging) {
        int thumbRadius = 6;
        int thumbX = playedWidth;
        int thumbY = height / 2;

        HBRUSH thumbBrush = CreateSolidBrush(RGB(255, 255, 255));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC, thumbBrush));
        HPEN thumbPen = CreatePen(PS_SOLID, 1, theme::kAccentHover);
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, thumbPen));

        Ellipse(memDC, thumbX - thumbRadius, thumbY - thumbRadius, thumbX + thumbRadius, thumbY + thumbRadius);

        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(thumbBrush);
        DeleteObject(thumbPen);
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void RXSeekBar::onLButtonDown(int x, int y) {
    SetCapture(m_hwnd);
    m_dragging = true;

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    if (width > 0 && m_durationSec > 0.0) {
        double ratio = static_cast<double>(x) / width;
        if (ratio < 0.0) ratio = 0.0;
        if (ratio > 1.0) ratio = 1.0;
        m_dragSec = ratio * m_durationSec;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void RXSeekBar::onMouseMove(int x, int y, WPARAM flags) {
    if (!m_hovering) {
        m_hovering = true;
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
        TrackMouseEvent(&tme);
    }

    m_hoverX = x;

    if (m_dragging) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int width = rc.right - rc.left;
        if (width > 0 && m_durationSec > 0.0) {
            double ratio = static_cast<double>(x) / width;
            if (ratio < 0.0) ratio = 0.0;
            if (ratio > 1.0) ratio = 1.0;
            m_dragSec = ratio * m_durationSec;
        }
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXSeekBar::onLButtonUp(int x, int y) {
    if (m_dragging) {
        m_dragging = false;
        ReleaseCapture();

        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int width = rc.right - rc.left;
        if (width > 0 && m_durationSec > 0.0) {
            double ratio = static_cast<double>(x) / width;
            if (ratio < 0.0) ratio = 0.0;
            if (ratio > 1.0) ratio = 1.0;
            m_currentSec = ratio * m_durationSec;
            if (m_seekCb) m_seekCb(m_currentSec);
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

// -------------------------------------------------------------
// RXIconButton Implementation
// -------------------------------------------------------------

static const wchar_t* kButtonClass = L"RXIconButtonClass";
static bool s_buttonClassRegistered = false;

bool RXIconButton::create(HWND parentHwnd, int x, int y, int width, int height, const std::wstring& iconText, UINT id, const std::wstring& tooltip) {
    m_parent = parentHwnd;
    m_text = iconText;
    m_tooltip = tooltip;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_buttonClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXIconButton::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.lpszClassName = kButtonClass;
        RegisterClassExW(&wc);
        s_buttonClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kButtonClass, iconText.c_str(),
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, width, height,
        parentHwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, this
    );

    return (m_hwnd != nullptr);
}

RXIconButton::~RXIconButton() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXIconButton::setText(const std::wstring& text) {
    m_text = text;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXIconButton::setTooltip(const std::wstring& tooltip) {
    m_tooltip = tooltip;
}

void RXIconButton::setAccent(bool isAccent) {
    m_accent = isAccent;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXIconButton::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

LRESULT CALLBACK RXIconButton::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXIconButton* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXIconButton*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXIconButton*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                pThis->onPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_ERASEBKGND:
                return 1;
            case WM_MOUSEMOVE: {
                if (!pThis->m_hovered) {
                    pThis->m_hovered = true;
                    TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
                    TrackMouseEvent(&tme);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_MOUSELEAVE: {
                pThis->m_hovered = false;
                pThis->m_pressed = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                pThis->m_pressed = true;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            case WM_LBUTTONUP: {
                if (pThis->m_pressed) {
                    pThis->m_pressed = false;
                    ReleaseCapture();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    if (pThis->m_clickCb) pThis->m_clickCb();
                }
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXIconButton::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    COLORREF bgColor = theme::kPanelBg;
    if (m_pressed) {
        bgColor = theme::kBorderColor;
    } else if (m_hovered) {
        bgColor = theme::kPanelBgHover;
    }

    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Text & icon
    SetBkMode(memDC, TRANSPARENT);
    COLORREF textColor = m_accent ? theme::kAccentColor : theme::kTextPrimary;
    if (m_hovered && m_accent) textColor = theme::kAccentHover;
    SetTextColor(memDC, textColor);

    HFONT hFont = CreateFontW(
        -14, 0, 0, 0, m_accent ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, hFont));

    DrawTextW(memDC, m_text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDC, oldFont);
    DeleteObject(hFont);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

} // namespace rx::ui
