#include "open_url_dialog.hpp"
#include "../../core/utils.hpp"
#include <windowsx.h>

namespace rx::ui {

static const wchar_t* kOpenUrlDialogClass = L"RXOpenUrlDialogClass";
static bool s_urlDialogClassRegistered = false;

constexpr UINT IDC_URL_EDIT        = 8001;
constexpr UINT IDC_URL_COMBO_CACHE = 8002;
constexpr UINT IDC_URL_COMBO_BUFF  = 8003;
constexpr UINT IDC_URL_BTN_PLAY    = 8004;
constexpr UINT IDC_URL_BTN_CANCEL  = 8005;

RXOpenUrlDialog::RXOpenUrlDialog() = default;

RXOpenUrlDialog::~RXOpenUrlDialog() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_boldFont) DeleteObject(m_boldFont);
}

void RXOpenUrlDialog::close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool RXOpenUrlDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_urlDialogClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXOpenUrlDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kOpenUrlDialogClass;
        RegisterClassExW(&wc);
        s_urlDialogClassRegistered = true;
    }

    int width = 500;
    int height = 240;
    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kOpenUrlDialogClass, L"Open Network Stream / URL",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);
    setupControls();
    checkClipboardForUrl();

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    SetFocus(m_editUrl);
    return true;
}

void RXOpenUrlDialog::setupControls() {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    m_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_titleFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_boldFont = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int curY = 16;
    int padX = 20;
    int fullW = 500 - padX * 2;

    HWND lblPrompt = CreateWindowExW(0, L"STATIC",
                                     L"Enter HTTP, HTTPS, HLS (.m3u8), DASH (.mpd), or RTSP streaming address:",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT,
                                     padX, curY, fullW, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblPrompt, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 26;

    m_editUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                                padX, curY, fullW, 26, m_hwnd, (HMENU)(INT_PTR)IDC_URL_EDIT, hInst, nullptr);
    SendMessageW(m_editUrl, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 38;

    // Buffer Readahead Combo
    HWND lblCache = CreateWindowExW(0, L"STATIC", L"Stream Cache:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblCache, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboCache = CreateWindowExW(0, L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   padX + 95, curY, 150, 140, m_hwnd, (HMENU)(INT_PTR)IDC_URL_COMBO_CACHE, hInst, nullptr);
    SendMessageW(m_comboCache, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_comboCache, CB_ADDSTRING, 0, (LPARAM)L"5s (Low Latency)");
    SendMessageW(m_comboCache, CB_ADDSTRING, 0, (LPARAM)L"15s (Recommended)");
    SendMessageW(m_comboCache, CB_ADDSTRING, 0, (LPARAM)L"30s (Unstable)");
    SendMessageW(m_comboCache, CB_ADDSTRING, 0, (LPARAM)L"60s (Maximum)");
    SendMessageW(m_comboCache, CB_SETCURSEL, 1, 0); // 15s default

    // Buffer Max Memory Size Combo
    HWND lblBuff = CreateWindowExW(0, L"STATIC", L"Max Buffer:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   padX + 260, curY + 2, 75, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblBuff, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboBuffer = CreateWindowExW(0, L"COMBOBOX", L"",
                                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                    padX + 340, curY, fullW - 340, 140, m_hwnd, (HMENU)(INT_PTR)IDC_URL_COMBO_BUFF, hInst, nullptr);
    SendMessageW(m_comboBuffer, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_comboBuffer, CB_ADDSTRING, 0, (LPARAM)L"50 MB");
    SendMessageW(m_comboBuffer, CB_ADDSTRING, 0, (LPARAM)L"100 MB (Default)");
    SendMessageW(m_comboBuffer, CB_ADDSTRING, 0, (LPARAM)L"250 MB");
    SendMessageW(m_comboBuffer, CB_SETCURSEL, 1, 0); // 100 MB default
    curY += 46;

    // Action buttons
    m_btnPlay = CreateWindowExW(0, L"BUTTON", L"▶ Play Stream", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                               padX + fullW - 220, curY, 115, 30, m_hwnd, (HMENU)(INT_PTR)IDC_URL_BTN_PLAY, hInst, nullptr);
    SendMessageW(m_btnPlay, WM_SETFONT, (WPARAM)m_boldFont, TRUE);

    m_btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 padX + fullW - 95, curY, 95, 30, m_hwnd, (HMENU)(INT_PTR)IDC_URL_BTN_CANCEL, hInst, nullptr);
    SendMessageW(m_btnCancel, WM_SETFONT, (WPARAM)m_font, TRUE);
}

void RXOpenUrlDialog::checkClipboardForUrl() {
    if (OpenClipboard(m_hwnd)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            auto* pText = static_cast<const wchar_t*>(GlobalLock(hData));
            if (pText) {
                std::wstring str = pText;
                if (str.find(L"http://") == 0 || str.find(L"https://") == 0 ||
                    str.find(L"rtsp://") == 0 || str.find(L"rtmp://") == 0) {
                    SetWindowTextW(m_editUrl, str.c_str());
                    SendMessageW(m_editUrl, EM_SETSEL, 0, -1);
                }
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

void RXOpenUrlDialog::submit() {
    wchar_t buf[2048] = { 0 };
    GetWindowTextW(m_editUrl, buf, 2048);
    std::string url = rx::utils::wideToUtf8(buf);

    if (url.empty()) return;

    int cacheSel = static_cast<int>(SendMessageW(m_comboCache, CB_GETCURSEL, 0, 0));
    int cacheSec = 15;
    if (cacheSel == 0) cacheSec = 5;
    else if (cacheSel == 1) cacheSec = 15;
    else if (cacheSel == 2) cacheSec = 30;
    else if (cacheSel == 3) cacheSec = 60;

    int buffSel = static_cast<int>(SendMessageW(m_comboBuffer, CB_GETCURSEL, 0, 0));
    int maxBufferMb = 100;
    if (buffSel == 0) maxBufferMb = 50;
    else if (buffSel == 1) maxBufferMb = 100;
    else if (buffSel == 2) maxBufferMb = 250;

    if (m_callback) {
        m_callback(url, cacheSec, maxBufferMb);
    }
    close();
}

void RXOpenUrlDialog::onCommand(WORD id, WORD /*code*/) {
    if (id == IDC_URL_BTN_PLAY) {
        submit();
    } else if (id == IDC_URL_BTN_CANCEL) {
        close();
    }
}

void RXOpenUrlDialog::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
}

LRESULT CALLBACK RXOpenUrlDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXOpenUrlDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXOpenUrlDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXOpenUrlDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_COMMAND:
                pThis->onCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;
            case WM_CTLCOLORSTATIC: {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetBkColor(hdc, theme::kPanelBg);
                SetTextColor(hdc, theme::kTextPrimary);
                static HBRUSH s_brush = CreateSolidBrush(theme::kPanelBg);
                return reinterpret_cast<LRESULT>(s_brush);
            }
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                pThis->onPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_CLOSE:
                pThis->close();
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace rx::ui
