#include "sub_dialog.hpp"
#include "../../core/utils.hpp"
#include "../../core/settings.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace rx::ui {

static const wchar_t* kSubDialogClass = L"RXSubtitleDialogClass";
static bool s_subDialogClassRegistered = false;

// Control IDs
constexpr UINT IDC_SUB_TAB_CTRL           = 5001;

// Tab 0
constexpr UINT IDC_SUB_COMBO_TRACK        = 5010;
constexpr UINT IDC_SUB_BTN_LOAD_EXT       = 5011;
constexpr UINT IDC_SUB_COMBO_FONT         = 5012;
constexpr UINT IDC_SUB_TB_FONT_SIZE       = 5013;
constexpr UINT IDC_SUB_CHK_BOLD           = 5014;
constexpr UINT IDC_SUB_CHK_ITALIC         = 5015;

// Tab 1
constexpr UINT IDC_SUB_EDIT_TEXT_COLOR    = 5020;
constexpr UINT IDC_SUB_BTN_PICK_TEXT_COL  = 5021;
constexpr UINT IDC_SUB_BTN_RESET_TEXT_COL = 5022;
constexpr UINT IDC_SUB_SWATCH_TEXT_COL    = 5023;
constexpr UINT IDC_SUB_CHK_OUTLINE        = 5024;
constexpr UINT IDC_SUB_EDIT_OUTLINE_COL   = 5025;
constexpr UINT IDC_SUB_BTN_PICK_OUTLINE   = 5026;
constexpr UINT IDC_SUB_SWATCH_OUTLINE_COL = 5027;
constexpr UINT IDC_SUB_TB_BORDER_SIZE     = 5028;
constexpr UINT IDC_SUB_CHK_SHADOW         = 5029;
constexpr UINT IDC_SUB_EDIT_SHADOW_COL    = 5030;
constexpr UINT IDC_SUB_BTN_PICK_SHADOW    = 5031;
constexpr UINT IDC_SUB_SWATCH_SHADOW_COL  = 5032;
constexpr UINT IDC_SUB_TB_SHADOW          = 5033;

// Tab 2
constexpr UINT IDC_SUB_CHK_BACKGROUND     = 5040;
constexpr UINT IDC_SUB_EDIT_BG_COLOR      = 5041;
constexpr UINT IDC_SUB_BTN_PICK_BG_COL    = 5042;
constexpr UINT IDC_SUB_SWATCH_BG_COL      = 5043;
constexpr UINT IDC_SUB_TB_BG_OPACITY      = 5044;
constexpr UINT IDC_SUB_TB_MARGIN          = 5045;

// Tab 3
constexpr UINT IDC_SUB_TB_POSITION        = 5050;
constexpr UINT IDC_SUB_COMBO_ALIGNMENT    = 5051;

// Tab 4
constexpr UINT IDC_SUB_BTN_MINUS_250      = 5060;
constexpr UINT IDC_SUB_BTN_MINUS_100      = 5061;
constexpr UINT IDC_SUB_BTN_PLUS_100       = 5062;
constexpr UINT IDC_SUB_BTN_PLUS_250       = 5063;
constexpr UINT IDC_SUB_BTN_RESET_DELAY    = 5064;
constexpr UINT IDC_SUB_TB_DELAY           = 5065;
constexpr UINT IDC_SUB_CHK_AUTOLOAD       = 5066;
constexpr UINT IDC_SUB_CHK_OVERRIDE_ASS   = 5067;
constexpr UINT IDC_SUB_EDIT_LANGS         = 5068;

// Actions
constexpr UINT IDC_SUB_BTN_APPLY          = 5080;
constexpr UINT IDC_SUB_BTN_OK             = 5081;
constexpr UINT IDC_SUB_BTN_CANCEL         = 5082;
constexpr UINT IDC_SUB_BTN_RESET_TAB      = 5083;
constexpr UINT IDC_SUB_BTN_RESET_ALL      = 5084;

// Color Helpers
static std::string colorRefToHex(COLORREF cr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(cr), GetGValue(cr), GetBValue(cr));
    return std::string(buf);
}

static COLORREF hexToColorRef(const std::string& hex, COLORREF defaultColor = RGB(255, 255, 255)) {
    if (hex.empty() || hex[0] != '#') return defaultColor;
    try {
        unsigned long val = std::stoul(hex.substr(1), nullptr, 16);
        if (hex.length() == 7) {
            int r = (val >> 16) & 0xFF;
            int g = (val >> 8) & 0xFF;
            int b = val & 0xFF;
            return RGB(r, g, b);
        }
    } catch (...) {}
    return defaultColor;
}

static int CALLBACK enumFontProc(ENUMLOGFONTEXW* lpelfe, NEWTEXTMETRICEXW*, DWORD, LPARAM lParam) {
    auto* list = reinterpret_cast<std::vector<std::wstring>*>(lParam);
    if (!list || !lpelfe) return 1;
    std::wstring face = lpelfe->elfLogFont.lfFaceName;
    if (face.empty() || face[0] == L'@') return 1;
    if (std::find(list->begin(), list->end(), face) == list->end()) {
        list->push_back(face);
    }
    return 1;
}

static std::vector<std::wstring> getInstalledFonts() {
    std::vector<std::wstring> fonts;
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        LOGFONTW lf{};
        lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)enumFontProc, (LPARAM)&fonts, 0);
        ReleaseDC(nullptr, hdc);
    }
    std::sort(fonts.begin(), fonts.end());
    std::vector<std::wstring> priority = {
        L"Segoe UI", L"Arial", L"Calibri", L"Tahoma", L"Verdana",
        L"Trebuchet MS", L"Consolas", L"Georgia", L"Times New Roman"
    };
    for (auto it = priority.rbegin(); it != priority.rend(); ++it) {
        auto found = std::find(fonts.begin(), fonts.end(), *it);
        if (found != fonts.end()) {
            fonts.erase(found);
            fonts.insert(fonts.begin(), *it);
        } else {
            fonts.insert(fonts.begin(), *it);
        }
    }
    return fonts;
}

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

    int width = 640;
    int height = 720;
    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kSubDialogClass, L"Subtitle Settings — RX Media Player",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);
    setupControls();
    refreshTracks();

    m_originalStyle = m_engine.getSubtitleStyle();
    setStyle(m_originalStyle);

    switchTab(0);
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

    // 1. Create Tab Control below the 105px Preview
    m_tabHwnd = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_TABS | TCS_FOCUSNEVER,
        20, 130, 595, 510,
        m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TAB_CTRL, hInst, nullptr
    );
    SendMessageW(m_tabHwnd, WM_SETFONT, (WPARAM)m_font, TRUE);

    TCITEMW tie{};
    tie.mask = TCIF_TEXT;
    const wchar_t* tabTitles[] = {
        L"Font & Text",
        L"Colors & Outline",
        L"Background & Layout",
        L"Position & Alignment",
        L"Sync & Advanced"
    };
    for (int i = 0; i < 5; ++i) {
        tie.pszText = const_cast<wchar_t*>(tabTitles[i]);
        TabCtrl_InsertItem(m_tabHwnd, i, &tie);
    }

    // Setup child controls for each tab
    setupTab0Font();
    setupTab1Colors();
    setupTab2Background();
    setupTab3Position();
    setupTab4Sync();

    // Bottom Action Buttons
    int btnY = 648;
    m_btnResetTab = CreateWindowExW(0, L"BUTTON", L"Reset Tab", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   20, btnY, 95, 28, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_RESET_TAB, hInst, nullptr);
    SendMessageW(m_btnResetTab, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnResetAll = CreateWindowExW(0, L"BUTTON", L"Reset All", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   122, btnY, 95, 28, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_RESET_ALL, hInst, nullptr);
    SendMessageW(m_btnResetAll, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnOk = CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                              335, btnY, 85, 28, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_OK, hInst, nullptr);
    SendMessageW(m_btnOk, WM_SETFONT, (WPARAM)m_boldFont, TRUE);

    m_btnApply = CreateWindowExW(0, L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 428, btnY, 85, 28, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_APPLY, hInst, nullptr);
    SendMessageW(m_btnApply, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnCancel = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  520, btnY, 85, 28, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_CANCEL, hInst, nullptr);
    SendMessageW(m_btnCancel, WM_SETFONT, (WPARAM)m_font, TRUE);
}

void RXSubtitleDialog::setupTab0Font() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int padX = 35;
    int curY = 170;
    int fullW = 560;

    // Active track
    HWND lblTrack = CreateWindowExW(0, L"STATIC", L"Active Subtitle Stream:", WS_CHILD,
                                    padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblTrack, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[0].push_back(lblTrack);
    curY += 24;

    m_comboTrack = CreateWindowExW(0, L"COMBOBOX", L"",
                                  WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  padX, curY, fullW - 140, 160, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_TRACK, hInst, nullptr);
    SendMessageW(m_comboTrack, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(m_comboTrack);

    m_btnLoadExt = CreateWindowExW(0, L"BUTTON", L"Load File...", WS_CHILD | BS_PUSHBUTTON,
                                  padX + fullW - 130, curY, 130, 26, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_LOAD_EXT, hInst, nullptr);
    SendMessageW(m_btnLoadExt, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(m_btnLoadExt);
    curY += 40;

    // Font Family
    HWND lblFont = CreateWindowExW(0, L"STATIC", L"Font Family:", WS_CHILD,
                                   padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblFont, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(lblFont);

    m_comboFont = CreateWindowExW(0, L"COMBOBOX", L"",
                                  WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                  padX + 95, curY, 260, 200, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_FONT, hInst, nullptr);
    SendMessageW(m_comboFont, WM_SETFONT, (WPARAM)m_font, TRUE);
    auto installedFonts = getInstalledFonts();
    for (const auto& fn : installedFonts) {
        SendMessageW(m_comboFont, CB_ADDSTRING, 0, (LPARAM)fn.c_str());
    }
    SendMessageW(m_comboFont, CB_SETCURSEL, 0, 0);
    m_tabControls[0].push_back(m_comboFont);

    m_chkBold = CreateWindowExW(0, L"BUTTON", L"Bold", WS_CHILD | BS_AUTOCHECKBOX,
                                padX + 375, curY + 2, 70, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_BOLD, hInst, nullptr);
    SendMessageW(m_chkBold, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(m_chkBold);

    m_chkItalic = CreateWindowExW(0, L"BUTTON", L"Italic", WS_CHILD | BS_AUTOCHECKBOX,
                                  padX + 455, curY + 2, 70, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_ITALIC, hInst, nullptr);
    SendMessageW(m_chkItalic, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(m_chkItalic);
    curY += 42;

    // Font Size
    HWND lblFontSize = CreateWindowExW(0, L"STATIC", L"Font Size:", WS_CHILD,
                                       padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblFontSize, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(lblFontSize);

    m_tbFontSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                   padX + 95, curY, fullW - 170, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_FONT_SIZE, hInst, nullptr);
    SendMessageW(m_tbFontSize, TBM_SETRANGE, TRUE, MAKELPARAM(14, 60));
    SendMessageW(m_tbFontSize, TBM_SETPOS, TRUE, 28);
    m_tabControls[0].push_back(m_tbFontSize);

    m_lblValFontSize = CreateWindowExW(0, L"STATIC", L"28 pt", WS_CHILD | SS_RIGHT,
                                       padX + fullW - 65, curY + 2, 65, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValFontSize, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(m_lblValFontSize);
    curY += 50;

    // Explanatory note
    HWND lblNote = CreateWindowExW(0, L"STATIC",
        L"Tip: Font customization applies to text-based subtitle streams (SRT, ASS, VTT, SUB).\r\n"
        L"Bitmap-based subtitles (PGS, VobSub) use pre-rendered graphics embedded in the video.",
        WS_CHILD | SS_LEFT, padX, curY, fullW, 40, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblNote, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[0].push_back(lblNote);
}

void RXSubtitleDialog::setupTab1Colors() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int padX = 35;
    int curY = 170;
    int fullW = 560;

    // 1. Primary Text Color
    HWND lblText = CreateWindowExW(0, L"STATIC", L"Subtitle Text Color:", WS_CHILD,
                                  padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblText, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[1].push_back(lblText);
    curY += 24;

    m_swatchTextColor = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_BLACKFRAME,
                                        padX, curY, 26, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_SWATCH_TEXT_COL, hInst, nullptr);
    m_tabControls[1].push_back(m_swatchTextColor);

    m_editTextColor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"#FFFFFF", WS_CHILD | ES_AUTOHSCROLL,
                                      padX + 35, curY, 90, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_EDIT_TEXT_COLOR, hInst, nullptr);
    SendMessageW(m_editTextColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_editTextColor);

    m_btnPickTextColor = CreateWindowExW(0, L"BUTTON", L"Choose Color...", WS_CHILD | BS_PUSHBUTTON,
                                         padX + 135, curY, 115, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PICK_TEXT_COL, hInst, nullptr);
    SendMessageW(m_btnPickTextColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_btnPickTextColor);

    m_btnResetTextColor = CreateWindowExW(0, L"BUTTON", L"Default White", WS_CHILD | BS_PUSHBUTTON,
                                          padX + 260, curY, 105, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_RESET_TEXT_COL, hInst, nullptr);
    SendMessageW(m_btnResetTextColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_btnResetTextColor);
    curY += 45;

    // 2. Outline Section
    m_chkOutline = CreateWindowExW(0, L"BUTTON", L"Enable Text Outline", WS_CHILD | BS_AUTOCHECKBOX,
                                   padX, curY, 200, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_OUTLINE, hInst, nullptr);
    SendMessageW(m_chkOutline, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[1].push_back(m_chkOutline);
    curY += 26;

    m_swatchOutlineColor = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_BLACKFRAME,
                                           padX, curY, 26, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_SWATCH_OUTLINE_COL, hInst, nullptr);
    m_tabControls[1].push_back(m_swatchOutlineColor);

    m_editOutlineColor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"#000000", WS_CHILD | ES_AUTOHSCROLL,
                                         padX + 35, curY, 90, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_EDIT_OUTLINE_COL, hInst, nullptr);
    SendMessageW(m_editOutlineColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_editOutlineColor);

    m_btnPickOutlineColor = CreateWindowExW(0, L"BUTTON", L"Choose Color...", WS_CHILD | BS_PUSHBUTTON,
                                            padX + 135, curY, 115, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PICK_OUTLINE, hInst, nullptr);
    SendMessageW(m_btnPickOutlineColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_btnPickOutlineColor);
    curY += 32;

    HWND lblBorderSize = CreateWindowExW(0, L"STATIC", L"Outline Width:", WS_CHILD,
                                         padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblBorderSize, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(lblBorderSize);

    m_tbBorderSize = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                     padX + 95, curY, fullW - 170, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_BORDER_SIZE, hInst, nullptr);
    SendMessageW(m_tbBorderSize, TBM_SETRANGE, TRUE, MAKELPARAM(0, 8));
    SendMessageW(m_tbBorderSize, TBM_SETPOS, TRUE, 3);
    m_tabControls[1].push_back(m_tbBorderSize);

    m_lblValBorderSize = CreateWindowExW(0, L"STATIC", L"3 px", WS_CHILD | SS_RIGHT,
                                         padX + fullW - 65, curY + 2, 65, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValBorderSize, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_lblValBorderSize);
    curY += 45;

    // 3. Shadow Section
    m_chkShadow = CreateWindowExW(0, L"BUTTON", L"Enable Drop Shadow", WS_CHILD | BS_AUTOCHECKBOX,
                                  padX, curY, 200, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_SHADOW, hInst, nullptr);
    SendMessageW(m_chkShadow, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[1].push_back(m_chkShadow);
    curY += 26;

    m_swatchShadowColor = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_BLACKFRAME,
                                          padX, curY, 26, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_SWATCH_SHADOW_COL, hInst, nullptr);
    m_tabControls[1].push_back(m_swatchShadowColor);

    m_editShadowColor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"#000000", WS_CHILD | ES_AUTOHSCROLL,
                                        padX + 35, curY, 90, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_EDIT_SHADOW_COL, hInst, nullptr);
    SendMessageW(m_editShadowColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_editShadowColor);

    m_btnPickShadowColor = CreateWindowExW(0, L"BUTTON", L"Choose Color...", WS_CHILD | BS_PUSHBUTTON,
                                           padX + 135, curY, 115, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PICK_SHADOW, hInst, nullptr);
    SendMessageW(m_btnPickShadowColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_btnPickShadowColor);
    curY += 32;

    HWND lblShadow = CreateWindowExW(0, L"STATIC", L"Shadow Offset:", WS_CHILD,
                                     padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblShadow, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(lblShadow);

    m_tbShadow = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                 padX + 95, curY, fullW - 170, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_SHADOW, hInst, nullptr);
    SendMessageW(m_tbShadow, TBM_SETRANGE, TRUE, MAKELPARAM(0, 6));
    SendMessageW(m_tbShadow, TBM_SETPOS, TRUE, 1);
    m_tabControls[1].push_back(m_tbShadow);

    m_lblValShadow = CreateWindowExW(0, L"STATIC", L"1 px", WS_CHILD | SS_RIGHT,
                                     padX + fullW - 65, curY + 2, 65, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValShadow, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[1].push_back(m_lblValShadow);
}

void RXSubtitleDialog::setupTab2Background() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int padX = 35;
    int curY = 170;
    int fullW = 560;

    m_chkBackground = CreateWindowExW(0, L"BUTTON", L"Enable Subtitle Background Box", WS_CHILD | BS_AUTOCHECKBOX,
                                     padX, curY, 280, 20, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_BACKGROUND, hInst, nullptr);
    SendMessageW(m_chkBackground, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[2].push_back(m_chkBackground);
    curY += 30;

    HWND lblBgColor = CreateWindowExW(0, L"STATIC", L"Box Color:", WS_CHILD,
                                     padX, curY + 2, 80, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblBgColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(lblBgColor);

    m_swatchBgColor = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_BLACKFRAME,
                                      padX + 85, curY, 26, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_SWATCH_BG_COL, hInst, nullptr);
    m_tabControls[2].push_back(m_swatchBgColor);

    m_editBgColor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"#000000", WS_CHILD | ES_AUTOHSCROLL,
                                    padX + 120, curY, 90, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_EDIT_BG_COLOR, hInst, nullptr);
    SendMessageW(m_editBgColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(m_editBgColor);

    m_btnPickBgColor = CreateWindowExW(0, L"BUTTON", L"Choose Color...", WS_CHILD | BS_PUSHBUTTON,
                                       padX + 220, curY, 115, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PICK_BG_COL, hInst, nullptr);
    SendMessageW(m_btnPickBgColor, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(m_btnPickBgColor);
    curY += 40;

    HWND lblOpacity = CreateWindowExW(0, L"STATIC", L"Box Opacity:", WS_CHILD,
                                     padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblOpacity, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(lblOpacity);

    m_tbBgOpacity = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                    padX + 95, curY, fullW - 170, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_BG_OPACITY, hInst, nullptr);
    SendMessageW(m_tbBgOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(m_tbBgOpacity, TBM_SETPOS, TRUE, 60);
    m_tabControls[2].push_back(m_tbBgOpacity);

    m_lblValBgOpacity = CreateWindowExW(0, L"STATIC", L"60 %", WS_CHILD | SS_RIGHT,
                                        padX + fullW - 65, curY + 2, 65, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValBgOpacity, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(m_lblValBgOpacity);
    curY += 45;

    HWND lblMargin = CreateWindowExW(0, L"STATIC", L"Bottom Margin:", WS_CHILD,
                                    padX, curY + 2, 90, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblMargin, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(lblMargin);

    m_tbMargin = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                 padX + 95, curY, fullW - 170, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_MARGIN, hInst, nullptr);
    SendMessageW(m_tbMargin, TBM_SETRANGE, TRUE, MAKELPARAM(10, 120));
    SendMessageW(m_tbMargin, TBM_SETPOS, TRUE, 36);
    m_tabControls[2].push_back(m_tbMargin);

    m_lblValMargin = CreateWindowExW(0, L"STATIC", L"36 px", WS_CHILD | SS_RIGHT,
                                     padX + fullW - 65, curY + 2, 65, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValMargin, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[2].push_back(m_lblValMargin);
}

void RXSubtitleDialog::setupTab3Position() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int padX = 35;
    int curY = 170;
    int fullW = 560;

    HWND lblPos = CreateWindowExW(0, L"STATIC", L"Vertical Position (% of screen height):", WS_CHILD,
                                  padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblPos, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[3].push_back(lblPos);
    curY += 24;

    m_tbPosition = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                   padX, curY, fullW - 80, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_POSITION, hInst, nullptr);
    SendMessageW(m_tbPosition, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessageW(m_tbPosition, TBM_SETPOS, TRUE, 100);
    m_tabControls[3].push_back(m_tbPosition);

    m_lblValPosition = CreateWindowExW(0, L"STATIC", L"100 %", WS_CHILD | SS_RIGHT,
                                       padX + fullW - 75, curY + 2, 75, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValPosition, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[3].push_back(m_lblValPosition);
    curY += 45;

    HWND lblAlign = CreateWindowExW(0, L"STATIC", L"Horizontal Alignment:", WS_CHILD,
                                    padX, curY + 2, 140, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblAlign, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[3].push_back(lblAlign);

    m_comboAlignment = CreateWindowExW(0, L"COMBOBOX", L"",
                                       WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
                                       padX + 150, curY, 260, 140, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_COMBO_ALIGNMENT, hInst, nullptr);
    SendMessageW(m_comboAlignment, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_comboAlignment, CB_ADDSTRING, 0, (LPARAM)L"Bottom Center (Default)");
    SendMessageW(m_comboAlignment, CB_ADDSTRING, 0, (LPARAM)L"Top Center");
    SendMessageW(m_comboAlignment, CB_ADDSTRING, 0, (LPARAM)L"Center");
    SendMessageW(m_comboAlignment, CB_ADDSTRING, 0, (LPARAM)L"Bottom Left");
    SendMessageW(m_comboAlignment, CB_ADDSTRING, 0, (LPARAM)L"Bottom Right");
    SendMessageW(m_comboAlignment, CB_SETCURSEL, 0, 0);
    m_tabControls[3].push_back(m_comboAlignment);
    curY += 50;

    HWND lblSafe = CreateWindowExW(0, L"STATIC",
        L"Subtitle Safe Area:\r\n"
        L"Subtitles are kept within safety bounds to ensure readability on high-DPI displays.\r\n"
        L"When the fullscreen control bar is revealed, subtitles are raised automatically.",
        WS_CHILD | SS_LEFT, padX, curY, fullW, 60, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblSafe, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[3].push_back(lblSafe);
}

void RXSubtitleDialog::setupTab4Sync() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    int padX = 35;
    int curY = 170;
    int fullW = 560;

    HWND lblSync = CreateWindowExW(0, L"STATIC", L"Subtitle Timing Synchronization:", WS_CHILD,
                                  padX, curY, 240, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblSync, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[4].push_back(lblSync);

    m_lblValDelay = CreateWindowExW(0, L"STATIC", L"Subtitle Delay: +0 ms", WS_CHILD | SS_RIGHT,
                                    padX + 250, curY, fullW - 250, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblValDelay, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    m_tabControls[4].push_back(m_lblValDelay);
    curY += 26;

    m_btnMinus250 = CreateWindowExW(0, L"BUTTON", L"-250ms", WS_CHILD | BS_PUSHBUTTON,
                                    padX, curY, 75, 25, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_MINUS_250, hInst, nullptr);
    SendMessageW(m_btnMinus250, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_btnMinus250);

    m_btnMinus100 = CreateWindowExW(0, L"BUTTON", L"-100ms", WS_CHILD | BS_PUSHBUTTON,
                                    padX + 80, curY, 75, 25, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_MINUS_100, hInst, nullptr);
    SendMessageW(m_btnMinus100, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_btnMinus100);

    m_tbDelay = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | TBS_AUTOTICKS | TBS_NOTICKS,
                                padX + 160, curY, fullW - 395, 25, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_TB_DELAY, hInst, nullptr);
    SendMessageW(m_tbDelay, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200)); // 0=-5000ms, 100=0ms, 200=+5000ms
    SendMessageW(m_tbDelay, TBM_SETPOS, TRUE, 100);
    m_tabControls[4].push_back(m_tbDelay);

    m_btnPlus100 = CreateWindowExW(0, L"BUTTON", L"+100ms", WS_CHILD | BS_PUSHBUTTON,
                                   padX + fullW - 230, curY, 75, 25, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PLUS_100, hInst, nullptr);
    SendMessageW(m_btnPlus100, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_btnPlus100);

    m_btnPlus250 = CreateWindowExW(0, L"BUTTON", L"+250ms", WS_CHILD | BS_PUSHBUTTON,
                                   padX + fullW - 150, curY, 75, 25, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_PLUS_250, hInst, nullptr);
    SendMessageW(m_btnPlus250, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_btnPlus250);

    m_btnResetDelay = CreateWindowExW(0, L"BUTTON", L"Reset (0ms)", WS_CHILD | BS_PUSHBUTTON,
                                     padX + fullW - 70, curY, 70, 25, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_BTN_RESET_DELAY, hInst, nullptr);
    SendMessageW(m_btnResetDelay, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_btnResetDelay);
    curY += 45;

    // Advanced Checkboxes
    m_chkAutoLoad = CreateWindowExW(0, L"BUTTON", L"Automatically load matching external subtitle files",
                                    WS_CHILD | BS_AUTOCHECKBOX,
                                    padX, curY, fullW, 22, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_AUTOLOAD, hInst, nullptr);
    SendMessageW(m_chkAutoLoad, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_chkAutoLoad);
    curY += 28;

    m_chkOverrideAss = CreateWindowExW(0, L"BUTTON", L"Override embedded styling (forces user fonts on SSA / ASS)",
                                      WS_CHILD | BS_AUTOCHECKBOX,
                                      padX, curY, fullW, 22, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_CHK_OVERRIDE_ASS, hInst, nullptr);
    SendMessageW(m_chkOverrideAss, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_chkOverrideAss);
    curY += 36;

    HWND lblLangs = CreateWindowExW(0, L"STATIC", L"Preferred Subtitle Languages (comma-separated ISO-639 codes):", WS_CHILD,
                                    padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblLangs, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(lblLangs);
    curY += 22;

    m_editLangs = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"en,hi", WS_CHILD | ES_AUTOHSCROLL,
                                  padX, curY, 260, 24, m_hwnd, (HMENU)(INT_PTR)IDC_SUB_EDIT_LANGS, hInst, nullptr);
    SendMessageW(m_editLangs, WM_SETFONT, (WPARAM)m_font, TRUE);
    m_tabControls[4].push_back(m_editLangs);
}

void RXSubtitleDialog::switchTab(int tabIndex) {
    m_activeTab = rx::utils::clamp(tabIndex, 0, 4);
    for (int t = 0; t < 5; ++t) {
        bool show = (t == m_activeTab);
        for (HWND h : m_tabControls[t]) {
            ShowWindow(h, show ? SW_SHOW : SW_HIDE);
        }
    }
}

void RXSubtitleDialog::refreshTracks() {
    if (!m_comboTrack) return;
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
    syncUiFromStyle();
    updateLabels();
    updateSwatches();
    RECT rcPreview{ 20, 15, 615, 120 };
    InvalidateRect(m_hwnd, &rcPreview, FALSE);
}

void RXSubtitleDialog::syncUiFromStyle() {
    // Tab 0
    std::wstring fontW = rx::utils::utf8ToWide(m_style.font);
    int fontIdx = static_cast<int>(SendMessageW(m_comboFont, CB_FINDSTRINGEXACT, -1, (LPARAM)fontW.c_str()));
    SendMessageW(m_comboFont, CB_SETCURSEL, (fontIdx >= 0) ? fontIdx : 0, 0);
    SendMessageW(m_tbFontSize, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.fontSize, 14, 60));
    SendMessageW(m_chkBold, BM_SETCHECK, m_style.bold ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkItalic, BM_SETCHECK, m_style.italic ? BST_CHECKED : BST_UNCHECKED, 0);

    // Tab 1
    SetWindowTextW(m_editTextColor, rx::utils::utf8ToWide(m_style.textColor).c_str());
    SendMessageW(m_chkOutline, BM_SETCHECK, m_style.outlineEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_editOutlineColor, rx::utils::utf8ToWide(m_style.borderColor).c_str());
    SendMessageW(m_tbBorderSize, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.borderSize, 0, 8));

    SendMessageW(m_chkShadow, BM_SETCHECK, m_style.shadowEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_editShadowColor, rx::utils::utf8ToWide(m_style.shadowColor).c_str());
    SendMessageW(m_tbShadow, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.shadowOffset, 0, 6));

    // Tab 2
    SendMessageW(m_chkBackground, BM_SETCHECK, m_style.backgroundEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_editBgColor, rx::utils::utf8ToWide(m_style.backgroundColor).c_str());
    SendMessageW(m_tbBgOpacity, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.backgroundOpacity, 0, 100));
    SendMessageW(m_tbMargin, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.marginVertical, 10, 120));

    // Tab 3
    SendMessageW(m_tbPosition, TBM_SETPOS, TRUE, rx::utils::clamp(m_style.positionPercent, 0, 100));
    int alignIdx = 0;
    if (m_style.alignment == "top-center") alignIdx = 1;
    else if (m_style.alignment == "center") alignIdx = 2;
    else if (m_style.alignment == "bottom-left") alignIdx = 3;
    else if (m_style.alignment == "bottom-right") alignIdx = 4;
    SendMessageW(m_comboAlignment, CB_SETCURSEL, alignIdx, 0);

    // Tab 4
    int delayPos = rx::utils::clamp(static_cast<int>((m_style.delaySec * 20.0) + 100.5), 0, 200);
    SendMessageW(m_tbDelay, TBM_SETPOS, TRUE, delayPos);
    SendMessageW(m_chkAutoLoad, BM_SETCHECK, m_style.autoLoadExternal ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkOverrideAss, BM_SETCHECK, m_style.overrideAss ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_editLangs, rx::utils::utf8ToWide(m_style.preferredLanguages).c_str());
}

void RXSubtitleDialog::syncStyleFromUi() {
    // Tab 0
    wchar_t fBuf[128] = {0};
    int selFont = static_cast<int>(SendMessageW(m_comboFont, CB_GETCURSEL, 0, 0));
    if (selFont >= 0) {
        SendMessageW(m_comboFont, CB_GETLBTEXT, selFont, (LPARAM)fBuf);
        m_style.font = rx::utils::wideToUtf8(fBuf);
    }
    m_style.fontSize = static_cast<int>(SendMessageW(m_tbFontSize, TBM_GETPOS, 0, 0));
    m_style.bold = (SendMessageW(m_chkBold, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_style.italic = (SendMessageW(m_chkItalic, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // Tab 1
    wchar_t colBuf[32] = {0};
    GetWindowTextW(m_editTextColor, colBuf, 32);
    m_style.textColor = rx::utils::wideToUtf8(colBuf);

    m_style.outlineEnabled = (SendMessageW(m_chkOutline, BM_GETCHECK, 0, 0) == BST_CHECKED);
    GetWindowTextW(m_editOutlineColor, colBuf, 32);
    m_style.borderColor = rx::utils::wideToUtf8(colBuf);
    m_style.borderSize = static_cast<int>(SendMessageW(m_tbBorderSize, TBM_GETPOS, 0, 0));

    m_style.shadowEnabled = (SendMessageW(m_chkShadow, BM_GETCHECK, 0, 0) == BST_CHECKED);
    GetWindowTextW(m_editShadowColor, colBuf, 32);
    m_style.shadowColor = rx::utils::wideToUtf8(colBuf);
    m_style.shadowOffset = static_cast<int>(SendMessageW(m_tbShadow, TBM_GETPOS, 0, 0));

    // Tab 2
    m_style.backgroundEnabled = (SendMessageW(m_chkBackground, BM_GETCHECK, 0, 0) == BST_CHECKED);
    GetWindowTextW(m_editBgColor, colBuf, 32);
    m_style.backgroundColor = rx::utils::wideToUtf8(colBuf);
    m_style.backgroundOpacity = static_cast<int>(SendMessageW(m_tbBgOpacity, TBM_GETPOS, 0, 0));
    m_style.marginVertical = static_cast<int>(SendMessageW(m_tbMargin, TBM_GETPOS, 0, 0));

    // Tab 3
    m_style.positionPercent = static_cast<int>(SendMessageW(m_tbPosition, TBM_GETPOS, 0, 0));
    int alignSel = static_cast<int>(SendMessageW(m_comboAlignment, CB_GETCURSEL, 0, 0));
    if (alignSel == 1) m_style.alignment = "top-center";
    else if (alignSel == 2) m_style.alignment = "center";
    else if (alignSel == 3) m_style.alignment = "bottom-left";
    else if (alignSel == 4) m_style.alignment = "bottom-right";
    else m_style.alignment = "bottom-center";

    // Tab 4
    m_style.autoLoadExternal = (SendMessageW(m_chkAutoLoad, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_style.overrideAss = (SendMessageW(m_chkOverrideAss, BM_GETCHECK, 0, 0) == BST_CHECKED);
    wchar_t langBuf[64] = {0};
    GetWindowTextW(m_editLangs, langBuf, 64);
    m_style.preferredLanguages = rx::utils::wideToUtf8(langBuf);
}

void RXSubtitleDialog::updateLabels() {
    if (m_lblValFontSize) {
        std::wstring s = std::to_wstring(m_style.fontSize) + L" pt";
        SetWindowTextW(m_lblValFontSize, s.c_str());
    }
    if (m_lblValBorderSize) {
        std::wstring s = std::to_wstring(m_style.borderSize) + L" px";
        SetWindowTextW(m_lblValBorderSize, s.c_str());
    }
    if (m_lblValShadow) {
        std::wstring s = std::to_wstring(m_style.shadowOffset) + L" px";
        SetWindowTextW(m_lblValShadow, s.c_str());
    }
    if (m_lblValBgOpacity) {
        std::wstring s = std::to_wstring(m_style.backgroundOpacity) + L" %";
        SetWindowTextW(m_lblValBgOpacity, s.c_str());
    }
    if (m_lblValMargin) {
        std::wstring s = std::to_wstring(m_style.marginVertical) + L" px";
        SetWindowTextW(m_lblValMargin, s.c_str());
    }
    if (m_lblValPosition) {
        std::wstring s = std::to_wstring(m_style.positionPercent) + L" %";
        SetWindowTextW(m_lblValPosition, s.c_str());
    }
    if (m_lblValDelay) {
        int ms = static_cast<int>(std::round(m_style.delaySec * 1000.0));
        wchar_t buf[64];
        swprintf_s(buf, L"Subtitle Delay: %+d ms", ms);
        SetWindowTextW(m_lblValDelay, buf);
    }
}

void RXSubtitleDialog::updateSwatches() {
    if (m_swatchTextColor) InvalidateRect(m_swatchTextColor, nullptr, TRUE);
    if (m_swatchOutlineColor) InvalidateRect(m_swatchOutlineColor, nullptr, TRUE);
    if (m_swatchShadowColor) InvalidateRect(m_swatchShadowColor, nullptr, TRUE);
    if (m_swatchBgColor) InvalidateRect(m_swatchBgColor, nullptr, TRUE);
}

void RXSubtitleDialog::applyChanges(bool showOsd) {
    syncStyleFromUi();
    m_engine.applySubtitleStyle(m_style);
    Settings::instance().subtitle.style = m_style;
    Settings::instance().save();

    if (m_callback) m_callback(m_style);

    RECT rcPreview{ 20, 15, 615, 120 };
    InvalidateRect(m_hwnd, &rcPreview, FALSE);

    if (showOsd) {
        m_engine.showOsd("Subtitle Settings Applied");
    }
}

void RXSubtitleDialog::resetTab(int tabIndex) {
    SubtitleStyle defStyle;
    defStyle.reset();

    if (tabIndex == 0) {
        m_style.font = defStyle.font;
        m_style.fontSize = defStyle.fontSize;
        m_style.bold = defStyle.bold;
        m_style.italic = defStyle.italic;
    } else if (tabIndex == 1) {
        m_style.textColor = defStyle.textColor;
        m_style.outlineEnabled = defStyle.outlineEnabled;
        m_style.borderColor = defStyle.borderColor;
        m_style.borderSize = defStyle.borderSize;
        m_style.shadowEnabled = defStyle.shadowEnabled;
        m_style.shadowColor = defStyle.shadowColor;
        m_style.shadowOffset = defStyle.shadowOffset;
    } else if (tabIndex == 2) {
        m_style.backgroundEnabled = defStyle.backgroundEnabled;
        m_style.backgroundColor = defStyle.backgroundColor;
        m_style.backgroundOpacity = defStyle.backgroundOpacity;
        m_style.marginVertical = defStyle.marginVertical;
    } else if (tabIndex == 3) {
        m_style.positionPercent = defStyle.positionPercent;
        m_style.alignment = defStyle.alignment;
    } else if (tabIndex == 4) {
        m_style.delaySec = 0.0;
        m_style.autoLoadExternal = defStyle.autoLoadExternal;
        m_style.overrideAss = defStyle.overrideAss;
        m_style.preferredLanguages = defStyle.preferredLanguages;
    }
    setStyle(m_style);
    m_engine.applySubtitleStyle(m_style);
}

void RXSubtitleDialog::resetAll() {
    m_style.reset();
    setStyle(m_style);
    m_engine.applySubtitleStyle(m_style);
    Settings::instance().subtitle.style = m_style;
    Settings::instance().save();
    m_engine.showOsd("Subtitle Settings Reset");
}

void RXSubtitleDialog::pickColor(std::string& targetHex, HWND editBox, HWND swatchHwnd) {
    COLORREF initial = hexToColorRef(targetHex);
    CHOOSECOLORW cc{};
    static COLORREF customCols[16] = {0};
    cc.lStructSize = sizeof(CHOOSECOLORW);
    cc.hwndOwner = m_hwnd;
    cc.rgbResult = initial;
    cc.lpCustColors = customCols;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        targetHex = colorRefToHex(cc.rgbResult);
        SetWindowTextW(editBox, rx::utils::utf8ToWide(targetHex).c_str());
        InvalidateRect(swatchHwnd, nullptr, TRUE);
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    }
}

void RXSubtitleDialog::openFilePicker() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"Subtitle Files (*.srt;*.ass;*.ssa;*.vtt;*.sub)\0*.srt;*.ass;*.ssa;*.vtt;*.sub\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        std::string pathUtf8 = rx::utils::wideToUtf8(filename);
        if (m_engine.loadSubtitle(pathUtf8)) {
            refreshTracks();
            std::filesystem::path p(filename);
            m_engine.showOsd("Subtitle Loaded: " + p.filename().string());
        }
    }
}

void RXSubtitleDialog::onHScroll(HWND tb, int pos) {
    if (tb == m_tbFontSize) {
        m_style.fontSize = pos;
    } else if (tb == m_tbBorderSize) {
        m_style.borderSize = pos;
    } else if (tb == m_tbShadow) {
        m_style.shadowOffset = pos;
    } else if (tb == m_tbBgOpacity) {
        m_style.backgroundOpacity = pos;
    } else if (tb == m_tbMargin) {
        m_style.marginVertical = pos;
    } else if (tb == m_tbPosition) {
        m_style.positionPercent = pos;
    } else if (tb == m_tbDelay) {
        // 0 to 200, 100 = 0.0s, step = 50ms
        m_style.delaySec = (pos - 100) * 0.050;
        m_engine.setSubtitleDelay(m_style.delaySec);
    }

    updateLabels();
    RECT rcPreview{ 20, 15, 615, 120 };
    InvalidateRect(m_hwnd, &rcPreview, FALSE);
}

void RXSubtitleDialog::onCommand(WORD id, WORD code) {
    if (id == IDC_SUB_COMBO_TRACK && code == CBN_SELCHANGE) {
        int sel = static_cast<int>(SendMessageW(m_comboTrack, CB_GETCURSEL, 0, 0));
        if (sel == 0) {
            m_engine.selectSubtitleTrack(0);
            m_engine.showOsd("Subtitles: Off");
        } else if (sel > 0 && sel - 1 < static_cast<int>(m_tracks.size())) {
            int trackId = m_tracks[sel - 1].id;
            m_engine.selectSubtitleTrack(trackId);
            m_engine.showOsd("Subtitle Track: " + m_tracks[sel - 1].title);
        }
    } else if (id == IDC_SUB_BTN_LOAD_EXT) {
        openFilePicker();
    } else if (id == IDC_SUB_COMBO_FONT && code == CBN_SELCHANGE) {
        syncStyleFromUi();
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_CHK_BOLD || id == IDC_SUB_CHK_ITALIC) {
        syncStyleFromUi();
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_BTN_PICK_TEXT_COL) {
        pickColor(m_style.textColor, m_editTextColor, m_swatchTextColor);
    } else if (id == IDC_SUB_BTN_RESET_TEXT_COL) {
        m_style.textColor = "#FFFFFF";
        SetWindowTextW(m_editTextColor, L"#FFFFFF");
        InvalidateRect(m_swatchTextColor, nullptr, TRUE);
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_CHK_OUTLINE) {
        m_style.outlineEnabled = (SendMessageW(m_chkOutline, BM_GETCHECK, 0, 0) == BST_CHECKED);
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_BTN_PICK_OUTLINE) {
        pickColor(m_style.borderColor, m_editOutlineColor, m_swatchOutlineColor);
    } else if (id == IDC_SUB_CHK_SHADOW) {
        m_style.shadowEnabled = (SendMessageW(m_chkShadow, BM_GETCHECK, 0, 0) == BST_CHECKED);
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_BTN_PICK_SHADOW) {
        pickColor(m_style.shadowColor, m_editShadowColor, m_swatchShadowColor);
    } else if (id == IDC_SUB_CHK_BACKGROUND) {
        m_style.backgroundEnabled = (SendMessageW(m_chkBackground, BM_GETCHECK, 0, 0) == BST_CHECKED);
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_BTN_PICK_BG_COL) {
        pickColor(m_style.backgroundColor, m_editBgColor, m_swatchBgColor);
    } else if (id == IDC_SUB_COMBO_ALIGNMENT && code == CBN_SELCHANGE) {
        syncStyleFromUi();
        RECT rcPreview{ 20, 15, 615, 120 };
        InvalidateRect(m_hwnd, &rcPreview, FALSE);
    } else if (id == IDC_SUB_BTN_MINUS_250) {
        m_style.delaySec -= 0.250;
        setStyle(m_style);
        m_engine.setSubtitleDelay(m_style.delaySec);
    } else if (id == IDC_SUB_BTN_MINUS_100) {
        m_style.delaySec -= 0.100;
        setStyle(m_style);
        m_engine.setSubtitleDelay(m_style.delaySec);
    } else if (id == IDC_SUB_BTN_PLUS_100) {
        m_style.delaySec += 0.100;
        setStyle(m_style);
        m_engine.setSubtitleDelay(m_style.delaySec);
    } else if (id == IDC_SUB_BTN_PLUS_250) {
        m_style.delaySec += 0.250;
        setStyle(m_style);
        m_engine.setSubtitleDelay(m_style.delaySec);
    } else if (id == IDC_SUB_BTN_RESET_DELAY) {
        m_style.delaySec = 0.0;
        setStyle(m_style);
        m_engine.setSubtitleDelay(0.0);
    } else if (id == IDC_SUB_BTN_APPLY) {
        applyChanges(true);
    } else if (id == IDC_SUB_BTN_OK) {
        applyChanges(true);
        close();
    } else if (id == IDC_SUB_BTN_CANCEL) {
        m_engine.applySubtitleStyle(m_originalStyle);
        close();
    } else if (id == IDC_SUB_BTN_RESET_TAB) {
        resetTab(m_activeTab);
    } else if (id == IDC_SUB_BTN_RESET_ALL) {
        resetAll();
    }
}

void RXSubtitleDialog::drawPreview(HDC hdc, const RECT& rc) {
    // 1. Draw outer simulated video canvas
    HBRUSH bgBrush = CreateSolidBrush(RGB(15, 18, 22));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(55, 60, 68));
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);

    // 2. Prepare text and font
    std::wstring line1 = L"RX Media Player — Professional Subtitle Preview";
    std::wstring line2 = L"Where precision meets cinematic playback.";
    std::wstring fullText = line1 + L"\r\n" + line2;

    int scaledFontSize = rx::utils::clamp(m_style.fontSize * 7 / 10, 12, 34);
    HFONT prevFont = CreateFontW(
        -scaledFontSize, 0, 0, 0,
        m_style.bold ? FW_BOLD : FW_NORMAL,
        m_style.italic ? TRUE : FALSE,
        FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        rx::utils::utf8ToWide(m_style.font).c_str()
    );

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, prevFont));
    SetBkMode(hdc, TRANSPARENT);

    RECT rcCalc = rc;
    DrawTextW(hdc, fullText.c_str(), -1, &rcCalc, DT_CALCRECT | DT_CENTER);
    int textW = rcCalc.right - rcCalc.left;
    int textH = rcCalc.bottom - rcCalc.top;

    // Horizontal position based on alignment
    int textX = rc.left + (rc.right - rc.left - textW) / 2;
    if (m_style.alignment == "bottom-left") {
        textX = rc.left + 24;
    } else if (m_style.alignment == "bottom-right") {
        textX = rc.right - textW - 24;
    }

    // Vertical position based on positionPercent (0=top, 100=bottom)
    int availH = (rc.bottom - rc.top) - textH - 16;
    if (availH < 0) availH = 0;
    int textY = rc.top + 8 + (availH * m_style.positionPercent) / 100;
    if (m_style.alignment == "top-center") {
        textY = rc.top + 10;
    } else if (m_style.alignment == "center") {
        textY = rc.top + (rc.bottom - rc.top - textH) / 2;
    }

    RECT rcText{ textX, textY, textX + textW, textY + textH };

    // 3. Draw background box if enabled
    if (m_style.backgroundEnabled && m_style.backgroundOpacity > 0) {
        RECT rcBg = rcText;
        InflateRect(&rcBg, 8, 4);
        COLORREF bgCol = hexToColorRef(m_style.backgroundColor, RGB(0, 0, 0));
        HBRUSH boxBrush = CreateSolidBrush(bgCol);
        FillRect(hdc, &rcBg, boxBrush);
        DeleteObject(boxBrush);
    }

    // 4. Draw Drop Shadow
    if (m_style.shadowEnabled && m_style.shadowOffset > 0) {
        RECT rcShadow = rcText;
        OffsetRect(&rcShadow, m_style.shadowOffset, m_style.shadowOffset);
        COLORREF shCol = hexToColorRef(m_style.shadowColor, RGB(0, 0, 0));
        SetTextColor(hdc, shCol);
        DrawTextW(hdc, fullText.c_str(), -1, &rcShadow, DT_CENTER);
    }

    // 5. Draw Outline
    if (m_style.outlineEnabled && m_style.borderSize > 0) {
        COLORREF outCol = hexToColorRef(m_style.borderColor, RGB(0, 0, 0));
        SetTextColor(hdc, outCol);
        int b = m_style.borderSize;
        int offsets[8][2] = {
            {-b, 0}, {b, 0}, {0, -b}, {0, b},
            {-b, -b}, {b, b}, {-b, b}, {b, -b}
        };
        for (int i = 0; i < 8; ++i) {
            RECT rcOut = rcText;
            OffsetRect(&rcOut, offsets[i][0], offsets[i][1]);
            DrawTextW(hdc, fullText.c_str(), -1, &rcOut, DT_CENTER);
        }
    }

    // 6. Draw Primary Text
    COLORREF textCol = hexToColorRef(m_style.textColor, RGB(255, 255, 255));
    SetTextColor(hdc, textCol);
    DrawTextW(hdc, fullText.c_str(), -1, &rcText, DT_CENTER);

    SelectObject(hdc, oldFont);
    DeleteObject(prevFont);
}

void RXSubtitleDialog::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw Live Preview Box at x: 20, y: 15, w: 595, h: 105
    RECT rcPreview{ 20, 15, 615, 120 };
    drawPreview(hdc, rcPreview);
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
            case WM_NOTIFY: {
                auto* hdr = reinterpret_cast<NMHDR*>(lParam);
                if (hdr->idFrom == IDC_SUB_TAB_CTRL && hdr->code == TCN_SELCHANGE) {
                    int curSel = TabCtrl_GetCurSel(pThis->m_tabHwnd);
                    pThis->switchTab(curSel);
                    return 0;
                }
                break;
            }
            case WM_COMMAND:
                pThis->onCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;
            case WM_HSCROLL:
                pThis->onHScroll(reinterpret_cast<HWND>(lParam),
                                 static_cast<int>(SendMessageW(reinterpret_cast<HWND>(lParam), TBM_GETPOS, 0, 0)));
                return 0;
            case WM_CTLCOLORSTATIC: {
                HWND ctrl = reinterpret_cast<HWND>(lParam);
                HDC hdc = reinterpret_cast<HDC>(wParam);

                // Check Color Swatches
                if (ctrl == pThis->m_swatchTextColor) {
                    COLORREF cr = hexToColorRef(pThis->m_style.textColor);
                    static HBRUSH s_br1 = nullptr;
                    if (s_br1) DeleteObject(s_br1);
                    s_br1 = CreateSolidBrush(cr);
                    return reinterpret_cast<LRESULT>(s_br1);
                }
                if (ctrl == pThis->m_swatchOutlineColor) {
                    COLORREF cr = hexToColorRef(pThis->m_style.borderColor);
                    static HBRUSH s_br2 = nullptr;
                    if (s_br2) DeleteObject(s_br2);
                    s_br2 = CreateSolidBrush(cr);
                    return reinterpret_cast<LRESULT>(s_br2);
                }
                if (ctrl == pThis->m_swatchShadowColor) {
                    COLORREF cr = hexToColorRef(pThis->m_style.shadowColor);
                    static HBRUSH s_br3 = nullptr;
                    if (s_br3) DeleteObject(s_br3);
                    s_br3 = CreateSolidBrush(cr);
                    return reinterpret_cast<LRESULT>(s_br3);
                }
                if (ctrl == pThis->m_swatchBgColor) {
                    COLORREF cr = hexToColorRef(pThis->m_style.backgroundColor);
                    static HBRUSH s_br4 = nullptr;
                    if (s_br4) DeleteObject(s_br4);
                    s_br4 = CreateSolidBrush(cr);
                    return reinterpret_cast<LRESULT>(s_br4);
                }

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
