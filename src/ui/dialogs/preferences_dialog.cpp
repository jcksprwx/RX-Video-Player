#include "preferences_dialog.hpp"
#include "../../core/utils.hpp"
#include <windowsx.h>

namespace rx::ui {

static const wchar_t* kPreferencesDialogClass = L"RXPreferencesDialogClass";
static bool s_prefsClassRegistered = false;

constexpr UINT IDC_PREF_CATEGORIES    = 7001;
constexpr UINT IDC_PREF_AUTONEXT      = 7002;
constexpr UINT IDC_PREF_REMEMBERPOS   = 7003;
constexpr UINT IDC_PREF_HWDEC         = 7004;
constexpr UINT IDC_PREF_EXCLUSIVE     = 7005;
constexpr UINT IDC_PREF_AUTOSUBS      = 7006;
constexpr UINT IDC_PREF_SAVE          = 7007;
constexpr UINT IDC_PREF_RESET         = 7008;
constexpr UINT IDC_PREF_CLOSE         = 7009;

RXPreferencesDialog::RXPreferencesDialog() = default;

RXPreferencesDialog::~RXPreferencesDialog() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_boldFont) DeleteObject(m_boldFont);
}

bool RXPreferencesDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_prefsClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXPreferencesDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kPreferencesDialogClass;
        RegisterClassExW(&wc);
        s_prefsClassRegistered = true;
    }

    int width = 680;
    int height = 500;
    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kPreferencesDialogClass, L"Preferences — RX Media Player",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);

    m_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_boldFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_titleFont = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    onInitDialog();

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void RXPreferencesDialog::close() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXPreferencesDialog::onInitDialog() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    auto& s = Settings::instance();

    // 1. Category Sidebar (Listbox)
    m_listCategories = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER,
        15, 15, 160, 410,
        m_hwnd, (HMENU)(INT_PTR)IDC_PREF_CATEGORIES, hInst, nullptr
    );
    SendMessageW(m_listCategories, WM_SETFONT, (WPARAM)m_font, TRUE);

    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ⚙  General");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ▶  Playback & Resume");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  🎨  Video & D3D11");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  🔊  Audio & WASAPI");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  💬  Subtitles");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ℹ  About RX Player");
    SendMessageW(m_listCategories, LB_SETCURSEL, 0, 0);

    // 2. Setting controls on right panel (x = 195, y = 50, w = 460)
    m_chkAutoPlayNext = CreateWindowExW(
        0, L"BUTTON", L"Automatically play next file in playlist",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 60, 440, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_AUTONEXT, hInst, nullptr
    );
    SendMessageW(m_chkAutoPlayNext, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkAutoPlayNext, BM_SETCHECK, s.playback.autoPlayNext ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkRememberPos = CreateWindowExW(
        0, L"BUTTON", L"Remember and resume playback position for each media file (SQLite)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 95, 450, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_REMEMBERPOS, hInst, nullptr
    );
    SendMessageW(m_chkRememberPos, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkRememberPos, BM_SETCHECK, s.playback.rememberPosition ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkHwdec = CreateWindowExW(
        0, L"BUTTON", L"Enable Direct3D 11 hardware video decoding (D3D11VA)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 130, 440, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_HWDEC, hInst, nullptr
    );
    SendMessageW(m_chkHwdec, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkHwdec, BM_SETCHECK, s.video.hwdecEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkWASAPIExclusive = CreateWindowExW(
        0, L"BUTTON", L"Enable WASAPI Exclusive Mode for bit-perfect audio output",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 165, 440, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_EXCLUSIVE, hInst, nullptr
    );
    SendMessageW(m_chkWASAPIExclusive, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkWASAPIExclusive, BM_SETCHECK, s.audio.exclusiveMode ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkAutoLoadSubs = CreateWindowExW(
        0, L"BUTTON", L"Auto-detect and load companion subtitle files (.srt, .ass, .vtt)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 200, 440, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_AUTOSUBS, hInst, nullptr
    );
    SendMessageW(m_chkAutoLoadSubs, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkAutoLoadSubs, BM_SETCHECK, s.subtitle.autoLoad ? BST_CHECKED : BST_UNCHECKED, 0);

    // 3. Bottom Action Buttons
    m_btnSave = CreateWindowExW(
        0, L"BUTTON", L"Save & Apply",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        340, 435, 110, 30, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SAVE, hInst, nullptr
    );
    SendMessageW(m_btnSave, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnReset = CreateWindowExW(
        0, L"BUTTON", L"Reset Defaults",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        460, 435, 110, 30, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_RESET, hInst, nullptr
    );
    SendMessageW(m_btnReset, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClose = CreateWindowExW(
        0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        580, 435, 80, 30, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_CLOSE, hInst, nullptr
    );
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_font, TRUE);

    updateCategoryPanels(0);
}

void RXPreferencesDialog::onCategoryChanged(int categoryIndex) {
    m_currentCategory = categoryIndex;
    updateCategoryPanels(categoryIndex);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void RXPreferencesDialog::updateCategoryPanels(int categoryIndex) {
    // Show/hide relevant checkboxes according to category
    bool showGen = (categoryIndex == 0);
    bool showPlay = (categoryIndex == 1);
    bool showVid = (categoryIndex == 2);
    bool showAud = (categoryIndex == 3);
    bool showSub = (categoryIndex == 4);

    if (categoryIndex == 0) {
        // General: show all general toggles
        ShowWindow(m_chkAutoPlayNext, SW_SHOW);
        ShowWindow(m_chkRememberPos, SW_SHOW);
        ShowWindow(m_chkHwdec, SW_SHOW);
        ShowWindow(m_chkWASAPIExclusive, SW_SHOW);
        ShowWindow(m_chkAutoLoadSubs, SW_SHOW);
    } else {
        ShowWindow(m_chkAutoPlayNext, showPlay ? SW_SHOW : SW_HIDE);
        ShowWindow(m_chkRememberPos, showPlay ? SW_SHOW : SW_HIDE);
        ShowWindow(m_chkHwdec, showVid ? SW_SHOW : SW_HIDE);
        ShowWindow(m_chkWASAPIExclusive, showAud ? SW_SHOW : SW_HIDE);
        ShowWindow(m_chkAutoLoadSubs, showSub ? SW_SHOW : SW_HIDE);
    }
}

void RXPreferencesDialog::saveAndApply() {
    auto& s = Settings::instance();
    s.playback.autoPlayNext = (SendMessageW(m_chkAutoPlayNext, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.playback.rememberPosition = (SendMessageW(m_chkRememberPos, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.video.hwdecEnabled = (SendMessageW(m_chkHwdec, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.audio.exclusiveMode = (SendMessageW(m_chkWASAPIExclusive, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.subtitle.autoLoad = (SendMessageW(m_chkAutoLoadSubs, BM_GETCHECK, 0, 0) == BST_CHECKED);

    s.save();
    MessageBoxW(m_hwnd, L"Preferences saved successfully!", L"RX Media Player", MB_ICONINFORMATION | MB_OK);
}

void RXPreferencesDialog::resetDefaults() {
    SendMessageW(m_chkAutoPlayNext, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_chkRememberPos, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_chkHwdec, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(m_chkWASAPIExclusive, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(m_chkAutoLoadSubs, BM_SETCHECK, BST_CHECKED, 0);
}

LRESULT CALLBACK RXPreferencesDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXPreferencesDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXPreferencesDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXPreferencesDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                WORD code = HIWORD(wParam);
                if (id == IDC_PREF_CATEGORIES && code == LBN_SELCHANGE) {
                    int sel = (int)SendMessageW(pThis->m_listCategories, LB_GETCURSEL, 0, 0);
                    pThis->onCategoryChanged(sel);
                } else if (id == IDC_PREF_SAVE) {
                    pThis->saveAndApply();
                } else if (id == IDC_PREF_RESET) {
                    pThis->resetDefaults();
                } else if (id == IDC_PREF_CLOSE || id == IDCANCEL) {
                    pThis->close();
                }
                return 0;
            }
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT rcClient;
                GetClientRect(hwnd, &rcClient);

                // Right panel header area
                RECT rcHeader{ 195, 15, rcClient.right - 15, 45 };
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, theme::kTextPrimary);
                HFONT oldFont = (HFONT)SelectObject(hdc, pThis->m_titleFont);

                const wchar_t* titles[] = {
                    L"General Application Settings",
                    L"Playback & Position Resume",
                    L"Video Acceleration & D3D11 Engine",
                    L"Audio Architecture & WASAPI",
                    L"Subtitle Rendering & Sync",
                    L"About RX Media Player"
                };

                int cat = pThis->m_currentCategory;
                if (cat >= 0 && cat < 6) {
                    DrawTextW(hdc, titles[cat], -1, &rcHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }

                // If on About category (cat == 5), draw rich about details
                if (cat == 5) {
                    SelectObject(hdc, pThis->m_font);
                    SetTextColor(hdc, theme::kTextMuted);

                    RECT rcAbout{ 200, 60, rcClient.right - 20, rcClient.bottom - 60 };
                    std::wstring aboutText =
                        L"RX Media Player — Professional Desktop Multimedia Engine\n\n"
                        L"Version: 2.0.1 (Windows 64-bit)\n"
                        L"Author & Lead Architect: Rituraj\n"
                        L"Repository: https://github.com/jcksprwx/RX-Video-Player\n\n"
                        L"Core Architecture:\n"
                        L"  • Language: C++20 Standard\n"
                        L"  • Rendering: Direct3D 11 / D3D11VA Hardware Acceleration\n"
                        L"  • Audio: Direct Windows Audio Session API (WASAPI) with 10-Band EQ\n"
                        L"  • Multimedia Engine: libmpv & FFmpeg Backends\n"
                        L"  • Metadata & History: Embedded SQLite 3 Database\n\n"
                        L"Designed and developed with pride for Windows 10 & 11.\n"
                        L"Copyright (c) 2026 Rituraj. All rights reserved.";

                    DrawTextW(hdc, aboutText.c_str(), -1, &rcAbout, DT_LEFT | DT_TOP);
                }

                // Vertical separator line between sidebar and content
                HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
                HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
                MoveToEx(hdc, 185, 15, nullptr);
                LineTo(hdc, 185, rcClient.bottom - 60);

                // Horizontal separator above action buttons
                MoveToEx(hdc, 15, rcClient.bottom - 55, nullptr);
                LineTo(hdc, rcClient.right - 15, rcClient.bottom - 55);

                SelectObject(hdc, oldPen);
                DeleteObject(borderPen);
                SelectObject(hdc, oldFont);

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
