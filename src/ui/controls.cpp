#include "controls.hpp"
#include "../core/utils.hpp"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

namespace rx::ui {

// -------------------------------------------------------------
// RXIconButton Implementation
// -------------------------------------------------------------

static const wchar_t* kButtonClass = L"RXIconButtonClass";
static bool s_buttonClassRegistered = false;

bool RXIconButton::create(HWND parentHwnd, int x, int y, int width, int height,
                          const std::wstring& iconText, UINT id, const std::wstring& tooltip) {
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

    COLORREF bgColor = m_isTitleBar ? theme::kBgColor : theme::kButtonBg;
    if (m_pressed) {
        bgColor = theme::kPressedBg;
    } else if (m_hovered) {
        bgColor = m_isClose ? theme::kErrorAccent : theme::kHoverBg;
    }

    HBRUSH bgBrush = CreateSolidBrush(bgColor);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Optional border for standard buttons
    if (!m_isTitleBar && !m_isClose && !m_hovered && !m_pressed) {
        HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
        HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, borderPen));
        HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC, GetStockObject(NULL_BRUSH)));
        RoundRect(memDC, rc.left, rc.top, rc.right, rc.bottom, 4, 4);
        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(borderPen);
    }

    // Text & icon
    SetBkMode(memDC, TRANSPARENT);
    COLORREF textColor = theme::kIconsPrimary;
    if (m_isClose && m_hovered) {
        textColor = RGB(255, 255, 255);
    } else if (m_accent || (m_isPin && m_accent)) {
        textColor = theme::kAccentColor;
    } else if (m_hovered) {
        textColor = RGB(255, 255, 255);
    }
    SetTextColor(memDC, textColor);

    HFONT hFont = CreateFontW(
        -13, 0, 0, 0, m_accent ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, hFont));

    DrawTextW(memDC, m_text.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Active bottom accent line for toolbar toggles
    if (m_accent && !m_isTitleBar) {
        RECT rcAccent{ rc.left + 4, rc.bottom - 2, rc.right - 4, rc.bottom };
        HBRUSH accBrush = CreateSolidBrush(theme::kAccentColor);
        FillRect(memDC, &rcAccent, accBrush);
        DeleteObject(accBrush);
    }

    SelectObject(memDC, oldFont);
    DeleteObject(hFont);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

// -------------------------------------------------------------
// RXTimelineStrip Implementation (Timeline + Volume Strip, 26px)
// -------------------------------------------------------------

static const wchar_t* kTimelineStripClass = L"RXTimelineStripClass";
static bool s_timelineStripClassRegistered = false;

bool RXTimelineStrip::create(HWND parentHwnd, int x, int y, int width, int height, UINT id) {
    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_timelineStripClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXTimelineStrip::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_HAND);
        wc.lpszClassName = kTimelineStripClass;
        RegisterClassExW(&wc);
        s_timelineStripClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kTimelineStripClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, width, height,
        parentHwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, this
    );

    return (m_hwnd != nullptr);
}

RXTimelineStrip::~RXTimelineStrip() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXTimelineStrip::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void RXTimelineStrip::setPosition(double currentSec, double durationSec) {
    if (!m_timelineDragging) {
        m_currentSec = currentSec;
        m_durationSec = durationSec;
        if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void RXTimelineStrip::setVolume(int volumePercent, bool muted) {
    m_volume = std::clamp(volumePercent, 0, 100);
    m_muted = muted;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT CALLBACK RXTimelineStrip::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXTimelineStrip* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXTimelineStrip*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXTimelineStrip*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
            case WM_LBUTTONDOWN:
                pThis->onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSEMOVE:
                pThis->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), wParam);
                return 0;
            case WM_LBUTTONUP:
                pThis->onLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSEWHEEL:
                pThis->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
                return 0;
            case WM_MOUSELEAVE:
                pThis->m_timelineHover = false;
                pThis->m_volumeHover = false;
                pThis->m_speakerHover = false;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXTimelineStrip::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Clear background (#202020)
    HBRUSH bgBrush = CreateSolidBrush(theme::kToolbarBg);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // 1px top border separator (#414141)
    HPEN topPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, topPen));
    MoveToEx(memDC, 0, 0, nullptr);
    LineTo(memDC, width, 0);
    SelectObject(memDC, oldPen);
    DeleteObject(topPen);

    // Volume Area Bounds
    int volAreaWidth = 140;
    int volTrackLeft = width - 110;
    int volTrackWidth = 85;
    int volTrackY = (height - 4) / 2;

    // Speaker Icon at (width - 135)
    RECT rcSpeaker{ width - 138, 2, width - 114, height - 2 };
    SetBkMode(memDC, TRANSPARENT);
    COLORREF speakerColor = m_muted ? theme::kAccentColor : (m_speakerHover ? RGB(255, 255, 255) : theme::kIconsPrimary);
    SetTextColor(memDC, speakerColor);

    HFONT iconFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, iconFont));
    DrawTextW(memDC, m_muted ? L"🔇" : L"🔊", -1, &rcSpeaker, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Volume Track (#2D2D2D)
    RECT rcVolTrack{ volTrackLeft, volTrackY, volTrackLeft + volTrackWidth, volTrackY + 4 };
    HBRUSH volTrackBg = CreateSolidBrush(theme::kSeekBg);
    FillRect(memDC, &rcVolTrack, volTrackBg);
    DeleteObject(volTrackBg);

    // Volume Fill (#E8D000)
    double volRatio = m_muted ? 0.0 : (m_volume / 100.0);
    int volFilledWidth = static_cast<int>(volTrackWidth * volRatio);
    RECT rcVolFill{ volTrackLeft, volTrackY, volTrackLeft + volFilledWidth, volTrackY + 4 };
    HBRUSH volFillBrush = CreateSolidBrush(theme::kAccentColor);
    FillRect(memDC, &rcVolFill, volFillBrush);
    DeleteObject(volFillBrush);

    // Volume Thumb (Circle radius 4px)
    int volThumbX = volTrackLeft + volFilledWidth;
    int volThumbY = height / 2;
    HBRUSH volThumbBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(memDC, volThumbBrush));
    HPEN volThumbPen = CreatePen(PS_SOLID, 1, theme::kAccentColor);
    oldPen = static_cast<HPEN>(SelectObject(memDC, volThumbPen));
    Ellipse(memDC, volThumbX - 4, volThumbY - 4, volThumbX + 4, volThumbY + 4);
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    DeleteObject(volThumbBrush);
    DeleteObject(volThumbPen);

    // Divider line between Timeline and Volume section
    HPEN divPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    oldPen = static_cast<HPEN>(SelectObject(memDC, divPen));
    MoveToEx(memDC, width - volAreaWidth - 10, 4, nullptr);
    LineTo(memDC, width - volAreaWidth - 10, height - 4);
    SelectObject(memDC, oldPen);
    DeleteObject(divPen);

    // Timeline Area Bounds
    int timelineLeft = 14;
    int timelineRight = width - volAreaWidth - 25;
    int timelineWidth = timelineRight - timelineLeft;

    if (timelineWidth > 20) {
        int trackHeight = (m_timelineHover || m_timelineDragging) ? 5 : 4;
        int trackY = (height - trackHeight) / 2;

        // Background progress track (#2D2D2D)
        RECT rcTrack{ timelineLeft, trackY, timelineRight, trackY + trackHeight };
        HBRUSH trackBgBrush = CreateSolidBrush(theme::kSeekBg);
        FillRect(memDC, &rcTrack, trackBgBrush);
        DeleteObject(trackBgBrush);

        // Played progress line (#E8D000 - Bright Golden Yellow)
        double activeSec = m_timelineDragging ? m_dragSec : m_currentSec;
        double progress = (m_durationSec > 0.0) ? (activeSec / m_durationSec) : 0.0;
        progress = std::clamp(progress, 0.0, 1.0);

        int playedWidth = static_cast<int>(timelineWidth * progress);
        RECT rcPlayed{ timelineLeft, trackY, timelineLeft + playedWidth, trackY + trackHeight };
        HBRUSH playedBrush = CreateSolidBrush(theme::kAccentColor);
        FillRect(memDC, &rcPlayed, playedBrush);
        DeleteObject(playedBrush);

        // Seek Handle (Circular Yellow Thumb radius 5px)
        int thumbRadius = 5;
        int thumbX = timelineLeft + playedWidth;
        int thumbY = height / 2;

        HBRUSH thumbBrush = CreateSolidBrush(theme::kAccentColor);
        oldBrush = static_cast<HBRUSH>(SelectObject(memDC, thumbBrush));
        HPEN thumbPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
        oldPen = static_cast<HPEN>(SelectObject(memDC, thumbPen));

        Ellipse(memDC, thumbX - thumbRadius, thumbY - thumbRadius, thumbX + thumbRadius, thumbY + thumbRadius);

        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(thumbBrush);
        DeleteObject(thumbPen);

        // Floating hover timecode tooltip
        if ((m_timelineHover || m_timelineDragging) && m_durationSec > 0.0) {
            double hoverSec = m_timelineDragging ? m_dragSec : 0.0;
            if (!m_timelineDragging) {
                double r = static_cast<double>(m_hoverX - timelineLeft) / timelineWidth;
                hoverSec = std::clamp(r, 0.0, 1.0) * m_durationSec;
            }

            std::string timeStr = rx::utils::formatTime(hoverSec);
            std::wstring timeW = rx::utils::utf8ToWide(timeStr);

            HFONT tipFont = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT prevFont = static_cast<HFONT>(SelectObject(memDC, tipFont));

            int tipX = std::clamp(m_hoverX - 25, timelineLeft, timelineRight - 55);
            RECT rcTip{ tipX, 1, tipX + 50, 16 };

            HBRUSH tipBg = CreateSolidBrush(RGB(20, 20, 20));
            HPEN tipBorder = CreatePen(PS_SOLID, 1, theme::kBorderColor);
            HPEN pOld = static_cast<HPEN>(SelectObject(memDC, tipBorder));
            HBRUSH bOld = static_cast<HBRUSH>(SelectObject(memDC, tipBg));
            RoundRect(memDC, rcTip.left, rcTip.top, rcTip.right, rcTip.bottom, 4, 4);

            SetTextColor(memDC, theme::kCurrentTimeAccent);
            DrawTextW(memDC, timeW.c_str(), -1, &rcTip, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(memDC, bOld);
            SelectObject(memDC, pOld);
            SelectObject(memDC, prevFont);
            DeleteObject(tipBg);
            DeleteObject(tipBorder);
            DeleteObject(tipFont);
        }
    }

    SelectObject(memDC, oldFont);
    DeleteObject(iconFont);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void RXTimelineStrip::onLButtonDown(int x, int y) {
    SetCapture(m_hwnd);

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;

    // Check Speaker Icon
    if (x >= width - 138 && x <= width - 114) {
        if (m_muteCb) m_muteCb();
        return;
    }

    // Check Volume Track
    int volTrackLeft = width - 110;
    int volTrackWidth = 85;
    if (x >= volTrackLeft - 5 && x <= volTrackLeft + volTrackWidth + 5) {
        m_volumeDragging = true;
        double ratio = static_cast<double>(x - volTrackLeft) / volTrackWidth;
        int newVol = static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 100.0);
        setVolume(newVol, false);
        if (m_volumeCb) m_volumeCb(newVol);
        return;
    }

    // Check Timeline Track
    int timelineLeft = 14;
    int timelineRight = width - 165;
    int timelineWidth = timelineRight - timelineLeft;
    if (timelineWidth > 20 && m_durationSec > 0.0) {
        m_timelineDragging = true;
        double ratio = static_cast<double>(x - timelineLeft) / timelineWidth;
        m_dragSec = std::clamp(ratio, 0.0, 1.0) * m_durationSec;
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void RXTimelineStrip::onMouseMove(int x, int y, WPARAM flags) {
    if (!m_timelineHover && !m_volumeHover) {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
        TrackMouseEvent(&tme);
    }

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;

    m_hoverX = x;
    m_speakerHover = (x >= width - 138 && x <= width - 114);
    m_volumeHover = (x >= width - 115 && x <= width - 15);
    m_timelineHover = (x >= 10 && x <= width - 165);

    if (m_timelineDragging && m_durationSec > 0.0) {
        int timelineLeft = 14;
        int timelineWidth = (width - 165) - timelineLeft;
        if (timelineWidth > 0) {
            double ratio = static_cast<double>(x - timelineLeft) / timelineWidth;
            m_dragSec = std::clamp(ratio, 0.0, 1.0) * m_durationSec;
        }
    } else if (m_volumeDragging) {
        int volTrackLeft = width - 110;
        int volTrackWidth = 85;
        double ratio = static_cast<double>(x - volTrackLeft) / volTrackWidth;
        int newVol = static_cast<int>(std::clamp(ratio, 0.0, 1.0) * 100.0);
        setVolume(newVol, false);
        if (m_volumeCb) m_volumeCb(newVol);
    }

    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXTimelineStrip::onLButtonUp(int x, int y) {
    ReleaseCapture();

    if (m_timelineDragging) {
        m_timelineDragging = false;
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int timelineLeft = 14;
        int timelineWidth = (rc.right - rc.left - 165) - timelineLeft;
        if (timelineWidth > 0 && m_durationSec > 0.0) {
            double ratio = static_cast<double>(x - timelineLeft) / timelineWidth;
            m_currentSec = std::clamp(ratio, 0.0, 1.0) * m_durationSec;
            if (m_seekCb) m_seekCb(m_currentSec);
        }
    }

    m_volumeDragging = false;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXTimelineStrip::onMouseWheel(short delta) {
    int step = (delta > 0) ? 5 : -5;
    int newVol = std::clamp(m_volume + step, 0, 100);
    setVolume(newVol, false);
    if (m_volumeCb) m_volumeCb(newVol);
}

// -------------------------------------------------------------
// RXTitleBar Implementation (Top Title Bar, 38px)
// -------------------------------------------------------------

static const wchar_t* kTitleBarClass = L"RXTitleBarClass";
static bool s_titleBarClassRegistered = false;

bool RXTitleBar::create(HWND parentHwnd, int width, int height, UINT id) {
    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_titleBarClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = RXTitleBar::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kTitleBarClass;
        RegisterClassExW(&wc);
        s_titleBarClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kTitleBarClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, width, height,
        parentHwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, this
    );

    return (m_hwnd != nullptr);
}

RXTitleBar::~RXTitleBar() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXTitleBar::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void RXTitleBar::setMediaInfo(const std::wstring& formatBadge, const std::wstring& title) {
    m_formatBadge = formatBadge;
    m_title = title.empty() ? L"RX Media Player" : title;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXTitleBar::setPinned(bool pinned) {
    m_isPinned = pinned;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXTitleBar::setMaximized(bool maximized) {
    m_isMaximized = maximized;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT CALLBACK RXTitleBar::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXTitleBar* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXTitleBar*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXTitleBar*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
            case WM_MOUSEMOVE:
                pThis->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_LBUTTONDOWN:
                pThis->onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_LBUTTONUP:
                pThis->onLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_LBUTTONDBLCLK:
                pThis->onLButtonDblClk(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSELEAVE:
                pThis->m_hoverBtn = -1;
                pThis->m_pressedBtn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXTitleBar::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Clear background (#1B1B1B)
    HBRUSH bgBrush = CreateSolidBrush(theme::kBgColor);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Bottom border separator (#414141)
    HPEN btmPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, btmPen));
    MoveToEx(memDC, 0, height - 1, nullptr);
    LineTo(memDC, width, height - 1);
    SelectObject(memDC, oldPen);
    DeleteObject(btmPen);

    SetBkMode(memDC, TRANSPARENT);

    // 1. Official RX Logo Emblem (Gold circular badge at x=12, y=9, size=20)
    int iconX = 12;
    int iconY = 9;
    int iconSize = 20;

    HBRUSH iconBg = CreateSolidBrush(RGB(38, 34, 12));
    HPEN iconPen = CreatePen(PS_SOLID, 1, theme::kAccentColor);
    HPEN pOld = static_cast<HPEN>(SelectObject(memDC, iconPen));
    HBRUSH bOld = static_cast<HBRUSH>(SelectObject(memDC, iconBg));
    Ellipse(memDC, iconX, iconY, iconX + iconSize, iconY + iconSize);

    // Draw miniature play triangle inside emblem
    POINT pts[3] = {
        { iconX + 8,  iconY + 5 },
        { iconX + 8,  iconY + 15 },
        { iconX + 15, iconY + 10 }
    };
    HBRUSH triBrush = CreateSolidBrush(theme::kAccentColor);
    SelectObject(memDC, triBrush);
    Polygon(memDC, pts, 3);
    SelectObject(memDC, bOld);
    SelectObject(memDC, pOld);
    DeleteObject(triBrush);
    DeleteObject(iconBg);
    DeleteObject(iconPen);

    // 2. Application Name: "RX Media Player"
    HFONT fontBold = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, fontBold));
    SetTextColor(memDC, theme::kTextPrimary);

    RECT rcName{ 38, 0, 142, height };
    DrawTextW(memDC, L"RX Media Player", -1, &rcName, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 3. Dropdown Menu Arrow Button (x=144, y=7, w=18, h=24)
    RECT rcMenuArrow{ 142, 8, 160, height - 8 };
    if (m_hoverBtn == 0) {
        HBRUSH hbr = CreateSolidBrush(theme::kHoverBg);
        FillRect(memDC, &rcMenuArrow, hbr);
        DeleteObject(hbr);
    }
    SetTextColor(memDC, (m_hoverBtn == 0) ? RGB(255, 255, 255) : theme::kIconsPrimary);
    DrawTextW(memDC, L"▼", -1, &rcMenuArrow, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // 4. Current Media Format Badge (e.g. MKV, MP4, AVI, FLAC)
    int currentX = 168;
    if (!m_formatBadge.empty()) {
        HFONT badgeFont = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SelectObject(memDC, badgeFont);

        RECT rcBadge{ currentX, 9, currentX + 42, 29 };
        HBRUSH badgeBg = CreateSolidBrush(theme::kButtonBg);
        HPEN badgePen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
        pOld = static_cast<HPEN>(SelectObject(memDC, badgePen));
        bOld = static_cast<HBRUSH>(SelectObject(memDC, badgeBg));
        RoundRect(memDC, rcBadge.left, rcBadge.top, rcBadge.right, rcBadge.bottom, 4, 4);

        SetTextColor(memDC, theme::kAccentColor);
        DrawTextW(memDC, m_formatBadge.c_str(), -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(memDC, bOld);
        SelectObject(memDC, pOld);
        DeleteObject(badgeBg);
        DeleteObject(badgePen);
        DeleteObject(badgeFont);

        currentX += 48;
    }

    // 5. Filename (Intelligently truncated with ellipsis)
    HFONT fontNormal = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(memDC, fontNormal);
    SetTextColor(memDC, theme::kTextSecondary);

    int rightBtnsWidth = 145; // 30 (pin) + 36 (min) + 36 (max) + 42 (close)
    RECT rcTitle{ currentX, 0, width - rightBtnsWidth - 10, height };
    if (rcTitle.right > rcTitle.left) {
        DrawTextW(memDC, m_title.c_str(), -1, &rcTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    DeleteObject(fontNormal);

    // 6. Right Window Control Buttons: Pin (📌), Min (—), Max (🗖/❐), Close (✕)
    int btnY = 5;
    int btnH = 28;

    // Pin Button (Btn 1)
    RECT rcPin{ width - 146, btnY, width - 114, btnY + btnH };
    if (m_hoverBtn == 1 || m_isPinned) {
        HBRUSH hbr = CreateSolidBrush((m_hoverBtn == 1) ? theme::kHoverBg : theme::kButtonBg);
        FillRect(memDC, &rcPin, hbr);
        DeleteObject(hbr);
    }
    SetTextColor(memDC, m_isPinned ? theme::kAccentColor : ((m_hoverBtn == 1) ? RGB(255, 255, 255) : theme::kIconsPrimary));
    DrawTextW(memDC, L"📌", -1, &rcPin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Minimize Button (Btn 2)
    RECT rcMin{ width - 114, btnY, width - 78, btnY + btnH };
    if (m_hoverBtn == 2) {
        HBRUSH hbr = CreateSolidBrush(theme::kHoverBg);
        FillRect(memDC, &rcMin, hbr);
        DeleteObject(hbr);
    }
    SetTextColor(memDC, (m_hoverBtn == 2) ? RGB(255, 255, 255) : theme::kIconsPrimary);
    DrawTextW(memDC, L"🗕", -1, &rcMin, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Maximize / Restore Button (Btn 3)
    RECT rcMax{ width - 78, btnY, width - 42, btnY + btnH };
    if (m_hoverBtn == 3) {
        HBRUSH hbr = CreateSolidBrush(theme::kHoverBg);
        FillRect(memDC, &rcMax, hbr);
        DeleteObject(hbr);
    }
    SetTextColor(memDC, (m_hoverBtn == 3) ? RGB(255, 255, 255) : theme::kIconsPrimary);
    DrawTextW(memDC, m_isMaximized ? L"❐" : L"🗖", -1, &rcMax, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Close Button (Btn 4)
    RECT rcClose{ width - 42, btnY, width - 2, btnY + btnH };
    if (m_hoverBtn == 4) {
        HBRUSH hbr = CreateSolidBrush(theme::kErrorAccent);
        FillRect(memDC, &rcClose, hbr);
        DeleteObject(hbr);
        SetTextColor(memDC, RGB(255, 255, 255));
    } else {
        SetTextColor(memDC, theme::kIconsPrimary);
    }
    DrawTextW(memDC, L"✕", -1, &rcClose, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(memDC, oldFont);
    DeleteObject(fontBold);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void RXTitleBar::onMouseMove(int x, int y) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;

    int prevHover = m_hoverBtn;
    m_hoverBtn = -1;

    if (x >= 142 && x <= 160) {
        m_hoverBtn = 0; // Menu Arrow
    } else if (x >= width - 146 && x < width - 114) {
        m_hoverBtn = 1; // Pin
    } else if (x >= width - 114 && x < width - 78) {
        m_hoverBtn = 2; // Min
    } else if (x >= width - 78 && x < width - 42) {
        m_hoverBtn = 3; // Max
    } else if (x >= width - 42 && x <= width - 2) {
        m_hoverBtn = 4; // Close
    }

    if (m_hoverBtn != prevHover) {
        TRACKMOUSEEVENT tme{ sizeof(TRACKMOUSEEVENT), TME_LEAVE, m_hwnd, 0 };
        TrackMouseEvent(&tme);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void RXTitleBar::onLButtonDown(int x, int y) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;

    if (x >= 142 && x <= 160) {
        POINT pt{ 142, rc.bottom };
        ClientToScreen(m_hwnd, &pt);
        if (m_menuCb) m_menuCb(pt.x, pt.y);
        return;
    } else if (x >= width - 146 && x < width - 114) {
        if (m_pinCb) m_pinCb();
        return;
    } else if (x >= width - 114 && x < width - 78) {
        if (m_minCb) m_minCb();
        return;
    } else if (x >= width - 78 && x < width - 42) {
        if (m_maxCb) m_maxCb();
        return;
    } else if (x >= width - 42 && x <= width - 2) {
        if (m_closeCb) m_closeCb();
        return;
    }

    // Drag the window by its titlebar
    ReleaseCapture();
    SendMessageW(m_parent, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void RXTitleBar::onLButtonUp(int x, int y) {
    m_pressedBtn = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void RXTitleBar::onLButtonDblClk(int x, int y) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int width = rc.right - rc.left;

    // Only double click if not on buttons
    if (x < width - 146 && (x < 142 || x > 160)) {
        if (m_maxCb) m_maxCb();
    }
}

} // namespace rx::ui
