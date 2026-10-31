#include "sub_dialog.hpp"
#include "../../core/utils.hpp"
#include <commctrl.h>
#include <windowsx.h>
#include <commdlg.h>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace rx::ui {

static const wchar_t* kSubDialogClass = L"RXSubtitleDialogClass";
static bool s_subDialogClassRegistered = false;

constexpr UINT IDC_SUB_COMBO_TRACK       = 7001;
constexpr UINT IDC_SUB_BTN_LOAD_EXT      = 7002;
constexpr UINT IDC_SUB_TB_DELAY          = 7003;
constexpr UINT IDC_SUB_BTN_MINUS_DELAY   = 7004;
constexpr UINT IDC_SUB_BTN_PLUS_DELAY    = 7005;
constexpr UINT IDC_SUB_COMBO_FONT        = 7006;
constexpr UINT IDC_SUB_TB_FONT_SIZE      = 7007;
constexpr UINT IDC_SUB_CHK_BOLD          = 7008;
constexpr UINT IDC_SUB_CHK_ITALIC        = 7009;
constexpr UINT IDC_SUB_COMBO_TEXT_COLOR  = 7010;
constexpr UINT IDC_SUB_COMBO_BORDER_COLOR= 7011;
constexpr UINT IDC_SUB_TB_BORDER_SIZE    = 7012;
constexpr UINT IDC_SUB_TB_SHADOW         = 7013;
constexpr UINT IDC_SUB_TB_MARGIN         = 7014;
constexpr UINT IDC_SUB_BTN_APPLY         = 7015;
constexpr UINT IDC_SUB_BTN_RESET         = 7016;
constexpr UINT IDC_SUB_BTN_CLOSE         = 7017;

static const wchar_t* kFontList[] = {
    L"Segoe UI", L"Arial", L"Roboto", L"Verdana",
    L"Trebuchet MS", L"Georgia", L"Consolas", L"Tahoma"
};

struct ColorPreset {
    const wchar_t* name;
    const char* hex;
};

static const ColorPreset kTextColors[] = {
    { L"White (#FFFFFF)", "#FFFFFF" },
    { L"Yellow (#FFFF00)", "#FFFF00" },
    { L"Cyan (#00FFFF)", "#00FFFF" },
    { L"Green (#00FF66)", "#00FF66" },
    { L"Amber (#FFC000)", "#FFC000" }
};

static const ColorPreset kBorderColors[] = {
    { L"Black (#000000)", "#000000" },
    { L"Dark Gray (#222222)", "#222222" },
    { L"Navy (#000044)", "#000044" },
    { L"None", "#00000000" }
};

RXSubtitleDialog::RXSubtitleDialog(MpvBackend& engine)
    : m_engine(engine) {}

RXSubtitleDialog::~RXSubtitleDialog() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_boldFont) DeleteObject(m_boldFont);
}

void RXSubtitleDialog::close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool RXSubtitleDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        refreshTracks();
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_subDialogClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXSubtitleDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kSubDialogClass;
        RegisterClassExW(&wc);
        s_subDialogClassRegistered = true;
    }

    int width = 520;
    int height = 620;
    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kSubDialogClass, L"Subtitle Settings & Styling",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);
    setupControls();
    refreshTracks();
    setStyle(m_engine.getSubtitleStyle());

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void RXSubtitleDialog::setupControls() {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    m_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_titleFont = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_boldFont = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    int curY = 14;
    int padX = 18;
    int fullW = 520 - padX * 2;

    // --- Section 1: Active Subtitle Track ---
    HWND lblSection1 = CreateWindowExW(0, L"STATIC", L"Active Subtitle Stream:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblSection1, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 24;

    m_comboTrack = CreateWindowExW(0, L"COMBOBOX", L"",
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  padX, curY, fullW - 140, 140, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_TRACK, hInst, nullptr);
    SendMessageW(m_comboTrack, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnLoadExt = CreateWindowExW(0, L"BUTTON", L"Load File...",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  padX + fullW - 130, curY, 130, 26, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_LOAD_EXT, hInst, nullptr);
    SendMessageW(m_btnLoadExt, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 36;

    // --- Section 2: Delay & Sync ---
    HWND lblSection2 = CreateWindowExW(0, L"STATIC", L"Synchronization & Delay:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblSection2, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 24;

    m_btnMinusDelay = CreateWindowExW(0, L"BUTTON", L"-100ms", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      padX, curY, 68, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_MINUS_DELAY, hInst, nullptr);
    SendMessageW(m_btnMinusDelay, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbDelay = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                padX + 74, curY, fullW - 220, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_DELAY, hInst, nullptr);
    SendMessageW(m_tbDelay, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100)); // 0 to 100, 50 = 0.0s, step 100ms
    SendMessageW(m_tbDelay, TBM_SETPOS, TRUE, 50);

    m_btnPlusDelay = CreateWindowExW(0, L"BUTTON", L"+100ms", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     padX + fullW - 140, curY, 68, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PLUS_DELAY, hInst, nullptr);
    SendMessageW(m_btnPlusDelay, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_lblValDelay = CreateWindowExW(0, L"STATIC", L"0 ms", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                    padX + fullW - 66, curY + 2, 66, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValDelay, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 36;

    // --- Section 3: Font & Typography ---
    HWND lblSection3 = CreateWindowExW(0, L"STATIC", L"Font & Typography:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblSection3, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 24;

    HWND lblFontName = CreateWindowExW(0, L"STATIC", L"Font Family:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                      padX, curY + 2, 80, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblFontName, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboFont = CreateWindowExW(0, L"COMBOBOX", L"",
                                  WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  padX + 85, curY, 180, 160, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_FONT, hInst, nullptr);
    SendMessageW(m_comboFont, WM_SETFONT, (WPARAM)m_font, TRUE);
    for (const auto* fn : kFontList) {
        SendMessageW(m_comboFont, CB_ADDSTRING, 0, (LPARAM)fn);
    }
    SendMessageW(m_comboFont, CB_SETCURSEL, 0, 0);

    m_chkBold = CreateWindowExW(0, L"BUTTON", L"Bold", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                padX + 280, curY + 2, 65, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_BOLD, hInst, nullptr);
    SendMessageW(m_chkBold, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_chkItalic = CreateWindowExW(0, L"BUTTON", L"Italic", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                  padX + 355, curY + 2, 65, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_ITALIC, hInst, nullptr);
    SendMessageW(m_chkItalic, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 32;

    HWND lblFontSize = CreateWindowExW(0, L"STATIC", L"Font Size:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                      padX, curY + 2, 80, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblFontSize, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbFontSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                   padX + 85, curY, fullW - 150, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_FONT_SIZE, hInst, nullptr);
    SendMessageW(m_tbFontSize, TBM_SETRANGE, TRUE, MAKELPARAM(14, 60));
    SendMessageW(m_tbFontSize, TBM_SETPOS, TRUE, 28);

    m_lblValFontSize = CreateWindowExW(0, L"STATIC", L"28 pt", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                       padX + fullW - 60, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValFontSize, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 36;

    // --- Section 4: Colors & Outline ---
    HWND lblSection4 = CreateWindowExW(0, L"STATIC", L"Color & Visual Effects:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblSection4, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 24;

    HWND lblTextColor = CreateWindowExW(0, L"STATIC", L"Text Color:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                       padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblTextColor, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboTextColor = CreateWindowExW(0, L"COMBOBOX", L"",
                                      WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                      padX + 95, curY, 140, 140, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_TEXT_COLOR, hInst, nullptr);
    SendMessageW(m_comboTextColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    for (const auto& c : kTextColors) {
        SendMessageW(m_comboTextColor, CB_ADDSTRING, 0, (LPARAM)c.name);
    }
    SendMessageW(m_comboTextColor, CB_SETCURSEL, 0, 0);

    HWND lblBorderColor = CreateWindowExW(0, L"STATIC", L"Outline Color:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                         padX + 250, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblBorderColor, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_comboBorderColor = CreateWindowExW(0, L"COMBOBOX", L"",
                                        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                        padX + 345, curY, 135, 140, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_BORDER_COLOR, hInst, nullptr);
    SendMessageW(m_comboBorderColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    for (const auto& c : kBorderColors) {
        SendMessageW(m_comboBorderColor, CB_ADDSTRING, 0, (LPARAM)c.name);
    }
    SendMessageW(m_comboBorderColor, CB_SETCURSEL, 0, 0);
    curY += 32;

    HWND lblBorderSize = CreateWindowExW(0, L"STATIC", L"Outline Width:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                        padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblBorderSize, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbBorderSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                     padX + 95, curY, fullW - 160, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_BORDER_SIZE, hInst, nullptr);
    SendMessageW(m_tbBorderSize, TBM_SETRANGE, TRUE, MAKELPARAM(0, 8));
    SendMessageW(m_tbBorderSize, TBM_SETPOS, TRUE, 3);

    m_lblValBorderSize = CreateWindowExW(0, L"STATIC", L"3 px", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                         padX + fullW - 60, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValBorderSize, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 30;

    HWND lblShadow = CreateWindowExW(0, L"STATIC", L"Shadow Offset:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblShadow, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbShadow = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                 padX + 95, curY, fullW - 160, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_SHADOW, hInst, nullptr);
    SendMessageW(m_tbShadow, TBM_SETRANGE, TRUE, MAKELPARAM(0, 6));
    SendMessageW(m_tbShadow, TBM_SETPOS, TRUE, 1);

    m_lblValShadow = CreateWindowExW(0, L"STATIC", L"1 px", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                     padX + fullW - 60, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValShadow, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 30;

    HWND lblMargin = CreateWindowExW(0, L"STATIC", L"Bottom Margin:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblMargin, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_tbMargin = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_NOTICKS,
                                 padX + 95, curY, fullW - 160, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_MARGIN, hInst, nullptr);
    SendMessageW(m_tbMargin, TBM_SETRANGE, TRUE, MAKELPARAM(10, 120));
    SendMessageW(m_tbMargin, TBM_SETPOS, TRUE, 36);

    m_lblValMargin = CreateWindowExW(0, L"STATIC", L"36 px", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                     padX + fullW - 60, curY + 2, 60, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValMargin, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 46;

    // --- Action Buttons ---
    m_btnApply = CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                padX + fullW - 270, curY, 85, 30, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_APPLY, hInst, nullptr);
    SendMessageW(m_btnApply, WM_SETFONT, (WPARAM)m_boldFont, TRUE);

    m_btnReset = CreateWindowExW(0, L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                padX + fullW - 175, curY, 80, 30, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_RESET, hInst, nullptr);
    SendMessageW(m_btnReset, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                padX + fullW - 85, curY, 85, 30, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_CLOSE, hInst, nullptr);
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_font, TRUE);
}

void RXSubtitleDialog::refreshTracks() {
    SendMessageW(m_comboTrack, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_comboTrack, CB_ADDSTRING, 0, (LPARAM)L"[No Subtitles]");

    m_tracks = m_engine.getSubtitleTracks();
    int curSel = 0;
    int activeId = m_engine.getSelectedSubtitleTrack();

    for (size_t i = 0; i < m_tracks.size(); ++i) {
        const auto& t = m_tracks[i];
        std::wostringstream wss;
        wss << L"Track " << t.id << L": " << rx::utils::utf8ToWide(t.title.empty() ? "Embedded Subtitle" : t.title);
        if (!t.lang.empty()) {
            wss << L" [" << rx::utils::utf8ToWide(t.lang) << L"]";
        }
        if (!t.codec.empty()) {
            wss << L" (" << rx::utils::utf8ToWide(t.codec) << L")";
        }
        SendMessageW(m_comboTrack, CB_ADDSTRING, 0, (LPARAM)wss.str().c_str());

        if (t.id == activeId) {
            curSel = static_cast<int>(i) + 1;
        }
    }

    SendMessageW(m_comboTrack, CB_SETCURSEL, curSel, 0);
}

void RXSubtitleDialog::setStyle(const SubtitleStyle& style) {
    m_style = style;

    // Delay: -5.0s to +5.0s -> pos = (delaySec + 5.0) * 10
    int delayPos = rx::utils::clamp(static_cast<int>((m_style.delaySec + 5.0) * 10.0 + 0.5), 0, 100);
    SendMessageW(m_tbDelay, TBM_SETPOS, TRUE, delayPos);

    // Font Size
    SendMessageW(m_tbFontSize, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.fontSize, 14, 60));

    // Bold / Italic
    SendMessageW(m_chkBold, BM_SETCHECK, m_style.bold ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkItalic, BM_SETCHECK, m_style.italic ? BST_CHECKED : BST_UNCHECKED, 0);

    // Font Family
    std::wstring fontW = rx::utils::utf8ToWide(m_style.font);
    int fontIdx = static_cast<int>(SendMessageW(m_comboFont, CB_FINDSTRINGEXACT, -1, (LPARAM)fontW.c_str()));
    SendMessageW(m_comboFont, CB_SETCURSEL, (fontIdx >= 0) ? fontIdx : 0, 0);

    // Text Color
    int textIdx = 0;
    for (int i = 0; i < 5; ++i) {
        if (m_style.textColor == kTextColors[i].hex) { textIdx = i; break; }
    }
    SendMessageW(m_comboTextColor, CB_SETCURSEL, textIdx, 0);

    // Border Color
    int borderIdx = 0;
    for (int i = 0; i < 4; ++i) {
        if (m_style.borderColor == kBorderColors[i].hex) { borderIdx = i; break; }
    }
    SendMessageW(m_comboBorderColor, CB_SETCURSEL, borderIdx, 0);

    // Border Size, Shadow, Margin
    SendMessageW(m_tbBorderSize, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.borderSize, 0, 8));
    SendMessageW(m_tbShadow, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.shadowOffset, 0, 6));
    SendMessageW(m_tbMargin, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.marginVertical, 10, 120));

    updateLabels();
}

void RXSubtitleDialog::updateLabels() {
    int delayMs = static_cast<int>(m_style.delaySec * 1000.0);
    std::wstring delayStr = (delayMs >= 0 ? L"+" : L"") + std::to_wstring(delayMs) + L" ms";
    SetWindowTextW(m_lblValDelay, delayStr.c_str());

    SetWindowTextW(m_lblValFontSize, (std::to_wstring(m_style.fontSize) + L" pt").c_str());
    SetWindowTextW(m_lblValBorderSize, (std::to_wstring(m_style.borderSize) + L" px").c_str());
    SetWindowTextW(m_lblValShadow, (std::to_wstring(m_style.shadowOffset) + L" px").c_str());
    SetWindowTextW(m_lblValMargin, (std::to_wstring(m_style.marginVertical) + L" px").c_str());
}

void RXSubtitleDialog::applyChanges() {
    // Read Font Family
    int fontSel = static_cast<int>(SendMessageW(m_comboFont, CB_GETCURSEL, 0, 0));
    if (fontSel >= 0 && fontSel < 8) {
        m_style.font = rx::utils::wideToUtf8(kFontList[fontSel]);
    }

    // Read Colors
    int textSel = static_cast<int>(SendMessageW(m_comboTextColor, CB_GETCURSEL, 0, 0));
    if (textSel >= 0 && textSel < 5) {
        m_style.textColor = kTextColors[textSel].hex;
    }
    int borderSel = static_cast<int>(SendMessageW(m_comboBorderColor, CB_GETCURSEL, 0, 0));
    if (borderSel >= 0 && borderSel < 4) {
        m_style.borderColor = kBorderColors[borderSel].hex;
    }

    // Read Checks
    m_style.bold = (SendMessageW(m_chkBold, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_style.italic = (SendMessageW(m_chkItalic, BM_GETCHECK, 0, 0) == BST_CHECKED);

    m_engine.applySubtitleStyle(m_style);
    if (m_callback) {
        m_callback(m_style);
    }
}

void RXSubtitleDialog::resetDefaults() {
    m_style.reset();
    setStyle(m_style);
    applyChanges();
}

void RXSubtitleDialog::openFilePicker() {
    wchar_t filename[MAX_PATH] = { 0 };
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Subtitle Files\0*.srt;*.ass;*.ssa;*.vtt;*.sub;*.idx;*.smi\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        std::string path = rx::utils::wideToUtf8(filename);
        if (m_engine.loadSubtitle(path)) {
            refreshTracks();
        }
    }
}

void RXSubtitleDialog::onHScroll(HWND tb, int pos) {
    if (tb == m_tbDelay) {
        // 0 to 100, 50 = 0.0s -> (pos - 50) * 0.1s
        m_style.delaySec = static_cast<double>(pos - 50) * 0.1;
        m_engine.setSubtitleDelay(m_style.delaySec);
    } else if (tb == m_tbFontSize) {
        m_style.fontSize = pos;
    } else if (tb == m_tbBorderSize) {
        m_style.borderSize = pos;
    } else if (tb == m_tbShadow) {
        m_style.shadowOffset = pos;
    } else if (tb == m_tbMargin) {
        m_style.marginVertical = pos;
    }
    updateLabels();
    applyChanges();
}

void RXSubtitleDialog::onCommand(WORD id, WORD code) {
    if (id == IDC_SUB_COMBO_TRACK && code == CBN_SELCHANGE) {
        int sel = static_cast<int>(SendMessageW(m_comboTrack, CB_GETCURSEL, 0, 0));
        if (sel == 0) {
            m_engine.selectSubtitleTrack(0);
        } else if (sel > 0 && sel - 1 < static_cast<int>(m_tracks.size())) {
            m_engine.selectSubtitleTrack(m_tracks[sel - 1].id);
        }
    } else if (id == IDC_SUB_BTN_LOAD_EXT) {
        openFilePicker();
    } else if (id == IDC_SUB_BTN_MINUS_DELAY) {
        m_style.delaySec -= 0.1;
        setStyle(m_style);
        applyChanges();
    } else if (id == IDC_SUB_BTN_PLUS_DELAY) {
        m_style.delaySec += 0.1;
        setStyle(m_style);
        applyChanges();
    } else if ((id == IDC_SUB_COMBO_FONT || id == IDC_SUB_COMBO_TEXT_COLOR || id == IDC_SUB_COMBO_BORDER_COLOR) && code == CBN_SELCHANGE) {
        applyChanges();
    } else if (id == IDC_SUB_CHK_BOLD || id == IDC_SUB_CHK_ITALIC) {
        applyChanges();
    } else if (id == IDC_SUB_BTN_APPLY) {
        applyChanges();
    } else if (id == IDC_SUB_BTN_RESET) {
        resetDefaults();
    } else if (id == IDC_SUB_BTN_CLOSE) {
        close();
    }
}

void RXSubtitleDialog::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
}

LRESULT CALLBACK RXSubtitleDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXSubtitleDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXSubtitleDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXSubtitleDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_COMMAND:
                pThis->onCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;
            case WM_HSCROLL:
                pThis->onHScroll(reinterpret_cast<HWND>(lParam),
                                 static_cast<int>(SendMessageW(reinterpret_cast<HWND>(lParam), TBM_GETPOS, 0, 0)));
                return 0;
            case WM_CTLCOLORSTATIC: {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetBkColor(hdc, theme::kPanelBg);
                SetTextColor(hdc, theme::kTextPrimary);
                static HBRUSH s_brush = CreateSolidBrush(theme::kPanelBg);
                return reinterpret_cast<LRESULT>(s_brush);
            }
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX: {
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetBkColor(hdc, theme::kPanelBg);
                SetTextColor(hdc, theme::kTextPrimary);
                static HBRUSH s_boxBrush = CreateSolidBrush(theme::kPanelBg);
                return reinterpret_cast<LRESULT>(s_boxBrush);
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
