#include "video_panel.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <iomanip>
#include <sstream>

namespace rx::ui {

static const wchar_t* kVideoPanelClass = L"RXVideoPanelClass";
static bool s_videoPanelClassRegistered = false;

constexpr UINT IDC_VP_TB_BRIGHTNESS   = 4001;
constexpr UINT IDC_VP_TB_CONTRAST     = 4002;
constexpr UINT IDC_VP_TB_SATURATION   = 4003;
constexpr UINT IDC_VP_TB_GAMMA        = 4004;
constexpr UINT IDC_VP_TB_HUE          = 4005;
constexpr UINT IDC_VP_TB_ZOOM         = 4006;

constexpr UINT IDC_VP_BTN_ROT0        = 4010;
constexpr UINT IDC_VP_BTN_ROT90       = 4011;
constexpr UINT IDC_VP_BTN_ROT180      = 4012;
constexpr UINT IDC_VP_BTN_ROT270      = 4013;

constexpr UINT IDC_VP_CHK_FLIPH       = 4020;
constexpr UINT IDC_VP_CHK_FLIPV       = 4021;
constexpr UINT IDC_VP_CHK_DEINT       = 4022;

constexpr UINT IDC_VP_BTN_ASP_AUTO    = 4030;
constexpr UINT IDC_VP_BTN_ASP_169     = 4031;
constexpr UINT IDC_VP_BTN_ASP_43      = 4032;
constexpr UINT IDC_VP_BTN_ASP_219     = 4033;

constexpr UINT IDC_VP_BTN_RESET       = 4040;

RXVideoPanel::RXVideoPanel() = default;

RXVideoPanel::~RXVideoPanel() {
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool RXVideoPanel::create(HWND parentHwnd, int x, int y, int width, int height) {
    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_videoPanelClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXVideoPanel::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kVideoPanelClass;
        RegisterClassExW(&wc);
        s_videoPanelClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kVideoPanelClass, L"Video Adjustments",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    m_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_titleFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int curY = 12;
    int labelW = 75;
    int trackbarW = width - labelW - 55;
    int valW = 35;

    auto createSliderRow = [&](const wchar_t* labelText, UINT id, HWND& tb, HWND& valLbl, int minVal, int maxVal, int curVal) {
        HWND lbl = CreateWindowExW(0, L"STATIC", labelText, WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   12, curY + 2, labelW, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessageW(lbl, WM_SETFONT, (WPARAM)m_font, TRUE);

        tb = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                             12 + labelW, curY, trackbarW, 24, m_hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessageW(tb, TBM_SETRANGE, TRUE, MAKELPARAM(minVal, maxVal));
        SendMessageW(tb, TBM_SETPOS, TRUE, curVal);

        valLbl = CreateWindowExW(0, L"STATIC", L"0", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                 12 + labelW + trackbarW + 5, curY + 2, valW, 20, m_hwnd, nullptr, hInst, nullptr);
        SendMessageW(valLbl, WM_SETFONT, (WPARAM)m_font, TRUE);

        curY += 28;
    };

    createSliderRow(L"Brightness", IDC_VP_TB_BRIGHTNESS, m_tbBrightness, m_lblValBrightness, 0, 200, 100);
    createSliderRow(L"Contrast",   IDC_VP_TB_CONTRAST,   m_tbContrast,   m_lblValContrast,   0, 200, 100);
    createSliderRow(L"Saturation", IDC_VP_TB_SATURATION, m_tbSaturation, m_lblValSaturation, 0, 200, 100);
    createSliderRow(L"Gamma",      IDC_VP_TB_GAMMA,      m_tbGamma,      m_lblValGamma,      0, 200, 100);
    createSliderRow(L"Hue",        IDC_VP_TB_HUE,        m_tbHue,        m_lblValHue,        0, 200, 100);
    createSliderRow(L"Zoom",       IDC_VP_TB_ZOOM,       m_tbZoom,       m_lblValZoom,       20, 300, 100);

    curY += 8;

    // Aspect Ratio section
    HWND lblAsp = CreateWindowExW(0, L"STATIC", L"Aspect Ratio:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  12, curY, width - 24, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblAsp, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 22;

    int btnW = (width - 24 - 15) / 4;
    m_btnAspectAuto = CreateWindowExW(0, L"BUTTON", L"Auto", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      12, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ASP_AUTO, hInst, nullptr);
    m_btnAspect169  = CreateWindowExW(0, L"BUTTON", L"16:9", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      12 + btnW + 5, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ASP_169, hInst, nullptr);
    m_btnAspect43   = CreateWindowExW(0, L"BUTTON", L"4:3",  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      12 + (btnW + 5) * 2, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ASP_43, hInst, nullptr);
    m_btnAspect219  = CreateWindowExW(0, L"BUTTON", L"21:9", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      12 + (btnW + 5) * 3, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ASP_219, hInst, nullptr);

    SendMessageW(m_btnAspectAuto, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_btnAspect169,  WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_btnAspect43,   WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_btnAspect219,  WM_SETFONT, (WPARAM)m_font, TRUE);

    curY += 32;

    // Rotation section
    HWND lblRot = CreateWindowExW(0, L"STATIC", L"Rotation:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                  12, curY, width - 24, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblRot, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 22;

    m_btnRotate0   = CreateWindowExW(0, L"BUTTON", L"0°",   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     12, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ROT0, hInst, nullptr);
    m_btnRotate90  = CreateWindowExW(0, L"BUTTON", L"90°",  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     12 + btnW + 5, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ROT90, hInst, nullptr);
    m_btnRotate180 = CreateWindowExW(0, L"BUTTON", L"180°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     12 + (btnW + 5) * 2, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ROT180, hInst, nullptr);
    m_btnRotate270 = CreateWindowExW(0, L"BUTTON", L"270°", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     12 + (btnW + 5) * 3, curY, btnW, 24, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_ROT270, hInst, nullptr);

    SendMessageW(m_btnRotate0,   WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_btnRotate90,  WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_btnRotate180, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_btnRotate270, WM_SETFONT, (WPARAM)m_font, TRUE);

    curY += 32;

    // Checkboxes: Flip H, Flip V, Deinterlace
    m_chkFlipH = CreateWindowExW(0, L"BUTTON", L"Flip Horizontal", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                 12, curY, 130, 22, m_hwnd, (HMENU)(INT_PTR)IDC_VP_CHK_FLIPH, hInst, nullptr);
    m_chkFlipV = CreateWindowExW(0, L"BUTTON", L"Flip Vertical", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                 145, curY, 130, 22, m_hwnd, (HMENU)(INT_PTR)IDC_VP_CHK_FLIPV, hInst, nullptr);
    curY += 24;

    m_chkDeinterlace = CreateWindowExW(0, L"BUTTON", L"Deinterlacing", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       12, curY, 130, 22, m_hwnd, (HMENU)(INT_PTR)IDC_VP_CHK_DEINT, hInst, nullptr);
    curY += 32;

    SendMessageW(m_chkFlipH, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkFlipV, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkDeinterlace, WM_SETFONT, (WPARAM)m_font, TRUE);

    // Reset button
    m_btnReset = CreateWindowExW(0, L"BUTTON", L"Reset All Picture Adjustments", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 12, curY, width - 24, 28, m_hwnd, (HMENU)(INT_PTR)IDC_VP_BTN_RESET, hInst, nullptr);
    SendMessageW(m_btnReset, WM_SETFONT, (WPARAM)m_titleFont, TRUE);

    updateLabels();
    return true;
}

void RXVideoPanel::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
    }
}

void RXVideoPanel::setVisible(bool visible) {
    m_visible = visible;
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void RXVideoPanel::setParams(const VideoAdjustmentParams& params) {
    m_params = params;
    if (!m_hwnd) return;

    SendMessageW(m_tbBrightness, TBM_SETPOS, TRUE, params.brightness + 100);
    SendMessageW(m_tbContrast,   TBM_SETPOS, TRUE, params.contrast + 100);
    SendMessageW(m_tbSaturation, TBM_SETPOS, TRUE, params.saturation + 100);
    SendMessageW(m_tbGamma,      TBM_SETPOS, TRUE, params.gamma + 100);
    SendMessageW(m_tbHue,        TBM_SETPOS, TRUE, params.hue + 100);
    SendMessageW(m_tbZoom,       TBM_SETPOS, TRUE, static_cast<int>(params.zoom * 100.0));

    SendMessageW(m_chkFlipH, BM_SETCHECK, params.flipHorizontal ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkFlipV, BM_SETCHECK, params.flipVertical ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkDeinterlace, BM_SETCHECK, params.deinterlace ? BST_CHECKED : BST_UNCHECKED, 0);

    updateLabels();
}

void RXVideoPanel::updateLabels() {
    auto formatVal = [](int val) {
        if (val > 0) return L"+" + std::to_wstring(val);
        return std::to_wstring(val);
    };

    SetWindowTextW(m_lblValBrightness, formatVal(m_params.brightness).c_str());
    SetWindowTextW(m_lblValContrast,   formatVal(m_params.contrast).c_str());
    SetWindowTextW(m_lblValSaturation, formatVal(m_params.saturation).c_str());
    SetWindowTextW(m_lblValGamma,      formatVal(m_params.gamma).c_str());
    SetWindowTextW(m_lblValHue,        formatVal(m_params.hue).c_str());

    std::wostringstream ss;
    ss << std::fixed << std::setprecision(1) << m_params.zoom << L"x";
    SetWindowTextW(m_lblValZoom, ss.str().c_str());
}

void RXVideoPanel::notifyChange() {
    updateLabels();
    if (m_callback) {
        m_callback(m_params);
    }
}

LRESULT CALLBACK RXVideoPanel::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXVideoPanel* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXVideoPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXVideoPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
            case WM_HSCROLL: {
                HWND tb = reinterpret_cast<HWND>(lParam);
                int pos = static_cast<int>(SendMessageW(tb, TBM_GETPOS, 0, 0));
                pThis->onHScroll(tb, pos);
                return 0;
            }
            case WM_COMMAND:
                pThis->onCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXVideoPanel::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Right border separator
    HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    MoveToEx(hdc, rc.right - 1, 0, nullptr);
    LineTo(hdc, rc.right - 1, rc.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

void RXVideoPanel::onHScroll(HWND tb, int pos) {
    if (tb == m_tbBrightness) {
        m_params.brightness = pos - 100;
    } else if (tb == m_tbContrast) {
        m_params.contrast = pos - 100;
    } else if (tb == m_tbSaturation) {
        m_params.saturation = pos - 100;
    } else if (tb == m_tbGamma) {
        m_params.gamma = pos - 100;
    } else if (tb == m_tbHue) {
        m_params.hue = pos - 100;
    } else if (tb == m_tbZoom) {
        m_params.zoom = pos / 100.0;
    }
    notifyChange();
}

void RXVideoPanel::onCommand(WORD id, WORD code) {
    switch (id) {
        case IDC_VP_BTN_ROT0:
            m_params.rotate = 0;
            notifyChange();
            break;
        case IDC_VP_BTN_ROT90:
            m_params.rotate = 90;
            notifyChange();
            break;
        case IDC_VP_BTN_ROT180:
            m_params.rotate = 180;
            notifyChange();
            break;
        case IDC_VP_BTN_ROT270:
            m_params.rotate = 270;
            notifyChange();
            break;

        case IDC_VP_CHK_FLIPH:
            m_params.flipHorizontal = (SendMessageW(m_chkFlipH, BM_GETCHECK, 0, 0) == BST_CHECKED);
            notifyChange();
            break;
        case IDC_VP_CHK_FLIPV:
            m_params.flipVertical = (SendMessageW(m_chkFlipV, BM_GETCHECK, 0, 0) == BST_CHECKED);
            notifyChange();
            break;
        case IDC_VP_CHK_DEINT:
            m_params.deinterlace = (SendMessageW(m_chkDeinterlace, BM_GETCHECK, 0, 0) == BST_CHECKED);
            notifyChange();
            break;

        case IDC_VP_BTN_ASP_AUTO:
            m_params.aspectRatio = "auto";
            notifyChange();
            break;
        case IDC_VP_BTN_ASP_169:
            m_params.aspectRatio = "16:9";
            notifyChange();
            break;
        case IDC_VP_BTN_ASP_43:
            m_params.aspectRatio = "4:3";
            notifyChange();
            break;
        case IDC_VP_BTN_ASP_219:
            m_params.aspectRatio = "21:9";
            notifyChange();
            break;

        case IDC_VP_BTN_RESET:
            m_params.reset();
            setParams(m_params);
            notifyChange();
            break;
    }
}

} // namespace rx::ui
