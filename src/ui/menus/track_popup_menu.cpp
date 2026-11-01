#include "track_popup_menu.hpp"
#include <windowsx.h>
#include <algorithm>

namespace rx::ui {

static const wchar_t* kTrackMenuClass = L"RXTrackPopupMenuClass";
constexpr int kRowHeight = 32;
constexpr int kSeparatorHeight = 8;
constexpr COLORREF kBgCharcoal = RGB(65, 65, 65);       // #414141
constexpr COLORREF kBgSelected = RGB(25, 25, 25);       // #191919
constexpr COLORREF kTextPrimary = RGB(208, 208, 208);   // #D0D0D0
constexpr COLORREF kTextActive = RGB(255, 255, 0);      // #FFFF00 (bright yellow)
constexpr COLORREF kTextShortcut = RGB(144, 144, 144);  // #909090
constexpr COLORREF kTextDisabled = RGB(110, 110, 110);  // #6E6E6E
constexpr COLORREF kRadioBlue = RGB(40, 140, 240);      // Blue radio indicator
constexpr COLORREF kBorder = RGB(85, 85, 85);           // #555555
constexpr COLORREF kSeparator = RGB(48, 48, 48);        // #303030

RXTrackMenu::RXTrackMenu() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_DROPSHADOW | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kTrackMenuClass;
    RegisterClassExW(&wc);

    m_font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    m_fontBold = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

RXTrackMenu::~RXTrackMenu() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_fontBold) DeleteObject(m_fontBold);
}

void RXTrackMenu::close() {
    m_isOpen = false;
    if (m_hwnd && IsWindow(m_hwnd)) {
        ReleaseCapture();
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

int RXTrackMenu::hitTest(int y) const {
    int curY = 1; // 1px border
    for (size_t i = 0; i < m_items.size(); ++i) {
        int h = m_items[i].isSeparator ? kSeparatorHeight : kRowHeight;
        if (y >= curY && y < curY + h) {
            if (m_items[i].isSeparator || !m_items[i].isEnabled) return -1;
            return static_cast<int>(i);
        }
        curY += h;
    }
    return -1;
}

int RXTrackMenu::trackPopupMenu(HWND parentHwnd, int screenX, int screenY,
                                const std::vector<TrackMenuItem>& items) {
    if (items.empty()) return 0;
    m_parent = parentHwnd;
    m_items = items;
    m_hoveredIndex = -1;
    m_selectedIndex = -1;
    m_chosenId = 0;
    m_isOpen = true;

    // Measure widths and height
    HDC hdc = GetDC(parentHwnd);
    HFONT oldFont = (HFONT)SelectObject(hdc, m_font);

    int maxTextWidth = 260;
    int maxShortcutWidth = 0;
    int totalHeight = 2; // 2px for top and bottom border

    for (const auto& it : m_items) {
        if (it.isSeparator) {
            totalHeight += kSeparatorHeight;
        } else {
            totalHeight += kRowHeight;
            SIZE szText{ 0, 0 };
            GetTextExtentPoint32W(hdc, it.text.c_str(), static_cast<int>(it.text.length()), &szText);
            if (szText.cx > maxTextWidth) maxTextWidth = szText.cx;

            if (!it.shortcut.empty()) {
                SIZE szSc{ 0, 0 };
                GetTextExtentPoint32W(hdc, it.shortcut.c_str(), static_cast<int>(it.shortcut.length()), &szSc);
                if (szSc.cx > maxShortcutWidth) maxShortcutWidth = szSc.cx;
            }
        }
    }

    SelectObject(hdc, oldFont);
    ReleaseDC(parentHwnd, hdc);

    // Padding: 32px left (radio indicator) + 24px between text and shortcut + 16px right
    int menuWidth = 32 + maxTextWidth + (maxShortcutWidth > 0 ? (24 + maxShortcutWidth) : 0) + 20;
    if (menuWidth < 340) menuWidth = 340;
    if (menuWidth > 750) menuWidth = 750;

    m_width = menuWidth;
    m_height = totalHeight;

    // Clamp to screen bounds
    POINT pt{ screenX, screenY };
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(MONITORINFO);
    GetMonitorInfoW(hMon, &mi);

    int posX = screenX;
    int posY = screenY;

    if (posX + m_width > mi.rcWork.right) {
        posX = mi.rcWork.right - m_width - 4;
    }
    if (posX < mi.rcWork.left) {
        posX = mi.rcWork.left + 4;
    }

    if (posY + m_height > mi.rcWork.bottom) {
        // Pop upward if not enough room downward
        posY = screenY - m_height;
        if (posY < mi.rcWork.top) posY = mi.rcWork.bottom - m_height - 4;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kTrackMenuClass, L"",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        posX, posY, m_width, m_height,
        parentHwnd, nullptr, GetModuleHandleW(nullptr), this
    );

    if (!m_hwnd) return 0;

    SetCapture(m_hwnd);

    // Modal message loop
    MSG msg;
    while (m_isOpen && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                close();
                break;
            } else if (msg.wParam == VK_UP) {
                int next = m_hoveredIndex - 1;
                while (next >= 0 && (m_items[next].isSeparator || !m_items[next].isEnabled)) next--;
                if (next >= 0) {
                    m_hoveredIndex = next;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
                continue;
            } else if (msg.wParam == VK_DOWN) {
                int next = m_hoveredIndex + 1;
                while (next < static_cast<int>(m_items.size()) && (m_items[next].isSeparator || !m_items[next].isEnabled)) next++;
                if (next < static_cast<int>(m_items.size())) {
                    m_hoveredIndex = next;
                    InvalidateRect(m_hwnd, nullptr, FALSE);
                }
                continue;
            } else if (msg.wParam == VK_RETURN) {
                if (m_hoveredIndex >= 0 && m_hoveredIndex < static_cast<int>(m_items.size())) {
                    m_chosenId = m_items[m_hoveredIndex].id;
                    close();
                    break;
                }
            }
        } else if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN || msg.message == WM_NCLBUTTONDOWN) {
            POINT clickPt = msg.pt;
            RECT rcWindow;
            GetWindowRect(m_hwnd, &rcWindow);
            if (!PtInRect(&rcWindow, clickPt)) {
                // Click outside closes menu
                close();
                break;
            }
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return m_chosenId;
}

void RXTrackMenu::onPaint(HDC hdc) {
    RECT clientR;
    GetClientRect(m_hwnd, &clientR);
    int w = clientR.right - clientR.left;
    int h = clientR.bottom - clientR.top;

    // Double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Background: #414141
    HBRUSH bgBrush = CreateSolidBrush(kBgCharcoal);
    FillRect(memDC, &clientR, bgBrush);
    DeleteObject(bgBrush);

    // Border: #555555
    HPEN borderPen = CreatePen(PS_SOLID, 1, kBorder);
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, nullBrush);
    Rectangle(memDC, 0, 0, w, h);

    SetBkMode(memDC, TRANSPARENT);

    int curY = 1;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const auto& item = m_items[i];
        if (item.isSeparator) {
            // Draw separator line
            HPEN sepPen = CreatePen(PS_SOLID, 1, kSeparator);
            SelectObject(memDC, sepPen);
            MoveToEx(memDC, 8, curY + (kSeparatorHeight / 2), nullptr);
            LineTo(memDC, w - 8, curY + (kSeparatorHeight / 2));
            DeleteObject(sepPen);
            curY += kSeparatorHeight;
            continue;
        }

        RECT rowR{ 1, curY, w - 1, curY + kRowHeight };
        bool isHovered = (static_cast<int>(i) == m_hoveredIndex);

        // Hover background: #191919
        if (isHovered && item.isEnabled) {
            HBRUSH hoverBrush = CreateSolidBrush(kBgSelected);
            FillRect(memDC, &rowR, hoverBrush);
            DeleteObject(hoverBrush);
        }

        // Radio indicator
        if (item.isRadio) {
            int cx = 18;
            int cy = curY + (kRowHeight / 2);
            int r = 5;

            HPEN radioPen = CreatePen(PS_SOLID, 1, item.isSelected ? kRadioBlue : RGB(100, 100, 100));
            SelectObject(memDC, radioPen);

            if (item.isSelected) {
                // Filled blue circular radio bullet
                HBRUSH radioBrush = CreateSolidBrush(kRadioBlue);
                HBRUSH prevB = (HBRUSH)SelectObject(memDC, radioBrush);
                Ellipse(memDC, cx - r, cy - r, cx + r + 1, cy + r + 1);
                SelectObject(memDC, prevB);
                DeleteObject(radioBrush);

                // Bright center dot
                HBRUSH dotBrush = CreateSolidBrush(RGB(255, 255, 255));
                HBRUSH prevDot = (HBRUSH)SelectObject(memDC, dotBrush);
                Ellipse(memDC, cx - 2, cy - 2, cx + 3, cy + 3);
                SelectObject(memDC, prevDot);
                DeleteObject(dotBrush);
            } else {
                Ellipse(memDC, cx - r, cy - r, cx + r + 1, cy + r + 1);
            }
            DeleteObject(radioPen);
        }

        // Text color
        COLORREF txtColor = kTextPrimary;
        if (!item.isEnabled) {
            txtColor = kTextDisabled;
        } else if (item.isSelected) {
            txtColor = kTextActive; // Bright yellow #FFFF00
        }
        SetTextColor(memDC, txtColor);

        HFONT fontToUse = (item.isSelected || item.isHeader) ? m_fontBold : m_font;
        HFONT prevFont = (HFONT)SelectObject(memDC, fontToUse);

        // Text rect
        int textLeft = item.isRadio ? 32 : 14;
        RECT textR{ textLeft, curY, w - 16, curY + kRowHeight };

        if (!item.shortcut.empty()) {
            // Draw right-aligned shortcut text
            SetTextColor(memDC, kTextShortcut);
            RECT scR{ w - 140, curY, w - 16, curY + kRowHeight };
            DrawTextW(memDC, item.shortcut.c_str(), -1, &scR, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            textR.right = w - 144;
            SetTextColor(memDC, txtColor);
        }

        DrawTextW(memDC, item.text.c_str(), -1, &textR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SelectObject(memDC, prevFont);
        curY += kRowHeight;
    }

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(borderPen);

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

LRESULT CALLBACK RXTrackMenu::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXTrackMenu* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXTrackMenu*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
        pThis = reinterpret_cast<RXTrackMenu*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!pThis) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            pThis->onPaint(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int y = GET_Y_LPARAM(lParam);
            int idx = pThis->hitTest(y);
            if (idx != pThis->m_hoveredIndex) {
                pThis->m_hoveredIndex = idx;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int y = GET_Y_LPARAM(lParam);
            int idx = pThis->hitTest(y);
            if (idx >= 0 && idx < static_cast<int>(pThis->m_items.size())) {
                pThis->m_chosenId = pThis->m_items[idx].id;
                pThis->close();
            }
            return 0;
        }
        case WM_KILLFOCUS: {
            pThis->close();
            return 0;
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace rx::ui
