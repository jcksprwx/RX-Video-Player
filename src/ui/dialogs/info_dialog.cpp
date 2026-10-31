#include "info_dialog.hpp"
#include "../../core/utils.hpp"
#include <windowsx.h>

namespace rx::ui {

static const wchar_t* kInfoDialogClass = L"RXInfoDialogClass";
static bool s_infoDialogClassRegistered = false;

constexpr UINT IDC_INFO_EDIT    = 5001;
constexpr UINT IDC_INFO_COPY    = 5002;
constexpr UINT IDC_INFO_CLOSE   = 5003;
constexpr UINT_PTR TIMER_REFRESH = 5004;

RXInfoDialog::RXInfoDialog(const MpvBackend& engine)
    : m_engine(engine) {}

RXInfoDialog::~RXInfoDialog() {
    close();
    if (m_font) DeleteObject(m_font);
}

bool RXInfoDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_infoDialogClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXInfoDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kInfoDialogClass;
        RegisterClassExW(&wc);
        s_infoDialogClassRegistered = true;
    }

    int width = 560;
    int height = 520;
    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kInfoDialogClass, L"Media Information & Diagnostics",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);

    m_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    // Multiline Read-Only Edit Control
    m_editReport = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        15, 15, width - 30, height - 80,
        m_hwnd, (HMENU)(INT_PTR)IDC_INFO_EDIT, hInst, nullptr
    );
    SendMessageW(m_editReport, WM_SETFONT, (WPARAM)m_font, TRUE);

    // Buttons
    m_btnCopy = CreateWindowExW(0, L"BUTTON", L"Copy Diagnostics to Clipboard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                15, height - 55, 230, 30, m_hwnd, (HMENU)(INT_PTR)IDC_INFO_COPY, hInst, nullptr);

    m_btnClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 width - 115, height - 55, 100, 30, m_hwnd, (HMENU)(INT_PTR)IDC_INFO_CLOSE, hInst, nullptr);

    HFONT btnFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(m_btnCopy, WM_SETFONT, (WPARAM)btnFont, TRUE);
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)btnFont, TRUE);

    refreshContent();
    m_timerId = SetTimer(m_hwnd, TIMER_REFRESH, 1000, nullptr);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void RXInfoDialog::close() {
    if (m_timerId && m_hwnd) {
        KillTimer(m_hwnd, m_timerId);
        m_timerId = 0;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXInfoDialog::refreshContent() {
    if (!m_editReport || !IsWindow(m_editReport)) return;

    DiagnosticsReport report = m_engine.getDiagnostics();
    std::string text = report.formatReport();
    std::wstring textW = rx::utils::utf8ToWide(text);

    // Convert \n to \r\n for Win32 EDIT control
    std::wstring winText;
    for (wchar_t ch : textW) {
        if (ch == L'\n') winText += L"\r\n";
        else winText += ch;
    }

    SetWindowTextW(m_editReport, winText.c_str());
}

void RXInfoDialog::copyToClipboard() {
    DiagnosticsReport report = m_engine.getDiagnostics();
    std::string text = report.formatReport();
    std::wstring textW = rx::utils::utf8ToWide(text);

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t bytes = (textW.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, textW.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
        MessageBoxW(m_hwnd, L"Diagnostic report copied to clipboard!", L"Diagnostics", MB_ICONINFORMATION | MB_OK);
    }
}

LRESULT CALLBACK RXInfoDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXInfoDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXInfoDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXInfoDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_TIMER:
                if (wParam == TIMER_REFRESH) {
                    pThis->refreshContent();
                }
                return 0;
            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                if (id == IDC_INFO_COPY) {
                    pThis->copyToClipboard();
                } else if (id == IDC_INFO_CLOSE || id == IDCANCEL) {
                    pThis->close();
                }
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
