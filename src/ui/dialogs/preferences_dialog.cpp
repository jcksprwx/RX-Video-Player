#include "preferences_dialog.hpp"
#include "../../core/utils.hpp"
#include <commctrl.h>
#include <algorithm>

namespace rx::ui {

static const wchar_t* kPreferencesDialogClass = L"RXPreferencesDialogClass";
static bool s_prefClassRegistered = false;

constexpr UINT IDC_PREF_CATEGORIES    = 4001;
constexpr UINT IDC_PREF_AUTONEXT      = 4002;
constexpr UINT IDC_PREF_REMEMBERPOS   = 4003;
constexpr UINT IDC_PREF_HWDEC         = 4004;
constexpr UINT IDC_PREF_EXCLUSIVE     = 4005;
constexpr UINT IDC_PREF_AUTOSUBS      = 4006;
constexpr UINT IDC_PREF_SAVE          = 4007;
constexpr UINT IDC_PREF_RESET         = 4008;
constexpr UINT IDC_PREF_CLOSE         = 4009;

// Shortcut controls IDs
constexpr UINT IDC_PREF_SC_SEARCH     = 4010;
constexpr UINT IDC_PREF_SC_LIST       = 4011;
constexpr UINT IDC_PREF_SC_CAPTURE    = 4012;
constexpr UINT IDC_PREF_SC_ASSIGN     = 4013;
constexpr UINT IDC_PREF_SC_CLEAR      = 4014;
constexpr UINT IDC_PREF_SC_RESET      = 4015;

RXPreferencesDialog::RXPreferencesDialog() = default;

RXPreferencesDialog::~RXPreferencesDialog() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_boldFont) DeleteObject(m_boldFont);
    if (m_titleFont) DeleteObject(m_titleFont);
}

bool RXPreferencesDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    if (!s_prefClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXPreferencesDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kPreferencesDialogClass;
        RegisterClassExW(&wc);
        s_prefClassRegistered = true;
    }

    int width = 780;
    int height = 560;
    RECT rcParent;
    if (parentHwnd && GetWindowRect(parentHwnd, &rcParent)) {
        int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;
        m_hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kPreferencesDialogClass, L"Preferences — RX Media Player",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
            x, y, width, height,
            parentHwnd, nullptr, hInst, this
        );
    } else {
        m_hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kPreferencesDialogClass, L"Preferences — RX Media Player",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            parentHwnd, nullptr, hInst, this
        );
    }

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);

    m_font = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_boldFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
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

LRESULT CALLBACK RXPreferencesDialog::captureEditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData) {
    auto* pThis = reinterpret_cast<RXPreferencesDialog*>(dwRefData);

    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
        WORD vk = static_cast<WORD>(wParam);

        // Ignore bare modifiers
        if (vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_CAPITAL || vk == VK_LWIN || vk == VK_RWIN) {
            return 0;
        }

        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

        ShortcutBinding b{ vk, ctrl, shift, alt };
        pThis->m_capturedBinding = b;
        SetWindowTextW(hwnd, b.toString().c_str());

        // Check conflict in real-time
        if (pThis->m_selectedAction != PlayerAction::None) {
            PlayerAction conf = ShortcutManager::instance().checkConflict(b, pThis->m_selectedAction);
            if (conf != PlayerAction::None) {
                const auto* cInfo = ShortcutManager::instance().getActionInfo(conf);
                std::wstring msgStr = L"Warning: Shortcut already assigned to '" + (cInfo ? cInfo->label : L"Another Action") + L"'!";
                SetWindowTextW(pThis->m_lblConflict, msgStr.c_str());
            } else {
                SetWindowTextW(pThis->m_lblConflict, L"Shortcut combination is available.");
            }
        }
        return 0;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void RXPreferencesDialog::onInitDialog() {
    HINSTANCE hInst = GetModuleHandle(nullptr);
    auto& s = Settings::instance();

    // 1. Category Sidebar (Listbox)
    m_listCategories = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER,
        15, 15, 165, 470,
        m_hwnd, (HMENU)(INT_PTR)IDC_PREF_CATEGORIES, hInst, nullptr
    );
    SendMessageW(m_listCategories, WM_SETFONT, (WPARAM)m_font, TRUE);

    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ⚙  General");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ▶  Playback & Resume");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  🎨  Video & D3D11");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  🔊  Audio & WASAPI");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  💬  Subtitles");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ⌨  Keyboard Shortcuts");
    SendMessageW(m_listCategories, LB_ADDSTRING, 0, (LPARAM)L"  ℹ  About RX Player");
    SendMessageW(m_listCategories, LB_SETCURSEL, 0, 0);

    // 2. Setting controls on right panel (x = 200, y = 55, w = 540)
    m_chkAutoPlayNext = CreateWindowExW(
        0, L"BUTTON", L"Automatically play next file in playlist",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 60, 520, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_AUTONEXT, hInst, nullptr
    );
    SendMessageW(m_chkAutoPlayNext, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkAutoPlayNext, BM_SETCHECK, s.playback.autoPlayNext ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkRememberPos = CreateWindowExW(
        0, L"BUTTON", L"Remember and resume playback position for each media file (SQLite)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 95, 520, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_REMEMBERPOS, hInst, nullptr
    );
    SendMessageW(m_chkRememberPos, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkRememberPos, BM_SETCHECK, s.playback.rememberPosition ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkHwdec = CreateWindowExW(
        0, L"BUTTON", L"Enable Direct3D 11 hardware video decoding (D3D11VA)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 130, 520, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_HWDEC, hInst, nullptr
    );
    SendMessageW(m_chkHwdec, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkHwdec, BM_SETCHECK, s.video.hwdecEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkWASAPIExclusive = CreateWindowExW(
        0, L"BUTTON", L"Enable WASAPI Exclusive Mode for bit-perfect audio output",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 165, 520, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_EXCLUSIVE, hInst, nullptr
    );
    SendMessageW(m_chkWASAPIExclusive, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkWASAPIExclusive, BM_SETCHECK, s.audio.exclusiveMode ? BST_CHECKED : BST_UNCHECKED, 0);

    m_chkAutoLoadSubs = CreateWindowExW(
        0, L"BUTTON", L"Auto-detect and load companion subtitle files (.srt, .ass, .vtt)",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        200, 200, 520, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_AUTOSUBS, hInst, nullptr
    );
    SendMessageW(m_chkAutoLoadSubs, WM_SETFONT, (WPARAM)m_font, TRUE);
    SendMessageW(m_chkAutoLoadSubs, BM_SETCHECK, s.subtitle.autoLoad ? BST_CHECKED : BST_UNCHECKED, 0);

    // 3. Shortcuts Manager Controls (Category 5)
    m_lblSearch = CreateWindowExW(0, L"STATIC", L"Search Shortcuts:", WS_CHILD,
                                  200, 55, 110, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblSearch, WM_SETFONT, (WPARAM)m_boldFont, TRUE);

    m_editShortcutSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                           WS_CHILD | ES_AUTOHSCROLL,
                                           320, 52, 240, 24, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SC_SEARCH, hInst, nullptr);
    SendMessageW(m_editShortcutSearch, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_listShortcutKeys = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        200, 84, 550, 280,
        m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SC_LIST, hInst, nullptr
    );
    SendMessageW(m_listShortcutKeys, WM_SETFONT, (WPARAM)m_font, TRUE);
    ListView_SetExtendedListViewStyle(m_listShortcutKeys, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.cx = 210;
    lvc.pszText = const_cast<wchar_t*>(L"Action Name");
    ListView_InsertColumn(m_listShortcutKeys, 0, &lvc);

    lvc.cx = 190;
    lvc.pszText = const_cast<wchar_t*>(L"Assigned Shortcut");
    ListView_InsertColumn(m_listShortcutKeys, 1, &lvc);

    lvc.cx = 130;
    lvc.pszText = const_cast<wchar_t*>(L"Category");
    ListView_InsertColumn(m_listShortcutKeys, 2, &lvc);

    m_lblCapture = CreateWindowExW(0, L"STATIC", L"Press Key Combination:", WS_CHILD,
                                   200, 376, 150, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblCapture, WM_SETFONT, (WPARAM)m_boldFont, TRUE);

    m_editKeyCapture = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Click here & press keys...",
                                       WS_CHILD | ES_AUTOHSCROLL,
                                       360, 373, 190, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SC_CAPTURE, hInst, nullptr);
    SendMessageW(m_editKeyCapture, WM_SETFONT, (WPARAM)m_font, TRUE);
    SetWindowSubclass(m_editKeyCapture, RXPreferencesDialog::captureEditSubclass, 1, reinterpret_cast<DWORD_PTR>(this));

    m_btnAssignShortcut = CreateWindowExW(0, L"BUTTON", L"Assign Shortcut", WS_CHILD | BS_PUSHBUTTON,
                                         560, 372, 110, 28, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SC_ASSIGN, hInst, nullptr);
    SendMessageW(m_btnAssignShortcut, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClearShortcut = CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | BS_PUSHBUTTON,
                                        678, 372, 72, 28, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SC_CLEAR, hInst, nullptr);
    SendMessageW(m_btnClearShortcut, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnResetShortcuts = CreateWindowExW(0, L"BUTTON", L"Restore Defaults", WS_CHILD | BS_PUSHBUTTON,
                                         560, 408, 190, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SC_RESET, hInst, nullptr);
    SendMessageW(m_btnResetShortcuts, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_lblConflict = CreateWindowExW(0, L"STATIC", L"", WS_CHILD,
                                    200, 412, 350, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_lblConflict, WM_SETFONT, (WPARAM)m_font, TRUE);

    // 4. Bottom Action Buttons
    m_btnSave = CreateWindowExW(
        0, L"BUTTON", L"Save & Apply",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        430, 498, 110, 30, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_SAVE, hInst, nullptr
    );
    SendMessageW(m_btnSave, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnReset = CreateWindowExW(
        0, L"BUTTON", L"Reset Defaults",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        550, 498, 110, 30, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_RESET, hInst, nullptr
    );
    SendMessageW(m_btnReset, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClose = CreateWindowExW(
        0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        670, 498, 80, 30, m_hwnd, (HMENU)(INT_PTR)IDC_PREF_CLOSE, hInst, nullptr
    );
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_font, TRUE);

    refreshShortcutsList();
    updateCategoryPanels(0);
}

void RXPreferencesDialog::onCategoryChanged(int categoryIndex) {
    m_currentCategory = categoryIndex;
    updateCategoryPanels(categoryIndex);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void RXPreferencesDialog::updateCategoryPanels(int categoryIndex) {
    bool showPlay = (categoryIndex == 1);
    bool showVid = (categoryIndex == 2);
    bool showAud = (categoryIndex == 3);
    bool showSub = (categoryIndex == 4);
    bool showShortcuts = (categoryIndex == 5);

    if (categoryIndex == 0) {
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

    // Shortcuts controls
    ShowWindow(m_lblSearch, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_editShortcutSearch, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_listShortcutKeys, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_lblCapture, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_editKeyCapture, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_btnAssignShortcut, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_btnClearShortcut, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_btnResetShortcuts, showShortcuts ? SW_SHOW : SW_HIDE);
    ShowWindow(m_lblConflict, showShortcuts ? SW_SHOW : SW_HIDE);

    if (showShortcuts) {
        refreshShortcutsList();
    }
}

void RXPreferencesDialog::onShortcutSearch() {
    wchar_t buf[128] = {0};
    GetWindowTextW(m_editShortcutSearch, buf, 128);
    m_shortcutFilter = buf;
    std::transform(m_shortcutFilter.begin(), m_shortcutFilter.end(), m_shortcutFilter.begin(), ::towlower);
    refreshShortcutsList();
}

void RXPreferencesDialog::refreshShortcutsList() {
    if (!m_listShortcutKeys || !IsWindow(m_listShortcutKeys)) return;
    ListView_DeleteAllItems(m_listShortcutKeys);

    const auto& actions = ShortcutManager::instance().getActionDefinitions();
    int itemIdx = 0;

    for (const auto& a : actions) {
        std::wstring bindStr = ShortcutManager::instance().getBindingString(a.action);

        if (!m_shortcutFilter.empty()) {
            std::wstring catL = a.category;
            std::wstring actL = a.label;
            std::wstring bndL = bindStr;
            std::transform(catL.begin(), catL.end(), catL.begin(), ::towlower);
            std::transform(actL.begin(), actL.end(), actL.begin(), ::towlower);
            std::transform(bndL.begin(), bndL.end(), bndL.begin(), ::towlower);

            if (catL.find(m_shortcutFilter) == std::wstring::npos &&
                actL.find(m_shortcutFilter) == std::wstring::npos &&
                bndL.find(m_shortcutFilter) == std::wstring::npos) {
                continue;
            }
        }

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT | LVIF_PARAM;
        lvi.iItem = itemIdx;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<wchar_t*>(a.label.c_str());
        lvi.lParam = static_cast<LPARAM>(a.action);
        ListView_InsertItem(m_listShortcutKeys, &lvi);

        ListView_SetItemText(m_listShortcutKeys, itemIdx, 1, const_cast<wchar_t*>(bindStr.c_str()));
        ListView_SetItemText(m_listShortcutKeys, itemIdx, 2, const_cast<wchar_t*>(a.category.c_str()));

        itemIdx++;
    }
}

void RXPreferencesDialog::onShortcutSelected() {
    int sel = ListView_GetNextItem(m_listShortcutKeys, -1, LVNI_SELECTED);
    if (sel >= 0) {
        LVITEMW lvi{};
        lvi.mask = LVIF_PARAM;
        lvi.iItem = sel;
        if (ListView_GetItem(m_listShortcutKeys, &lvi)) {
            m_selectedAction = static_cast<PlayerAction>(lvi.lParam);
            const auto* info = ShortcutManager::instance().getActionInfo(m_selectedAction);
            if (info) {
                std::wstring curBind = ShortcutManager::instance().getBindingString(m_selectedAction);
                SetWindowTextW(m_editKeyCapture, curBind.c_str());
                SetWindowTextW(m_lblConflict, (L"Selected action: " + info->label).c_str());
            }
        }
    }
}

void RXPreferencesDialog::onAssignShortcut() {
    if (m_selectedAction == PlayerAction::None) {
        SetWindowTextW(m_lblConflict, L"Please select an action in the list first.");
        return;
    }

    if (m_capturedBinding.empty()) {
        SetWindowTextW(m_lblConflict, L"Please press a key combination in the capture box.");
        return;
    }

    std::wstring conflictMsg;
    if (!ShortcutManager::instance().setPrimaryBinding(m_selectedAction, m_capturedBinding, &conflictMsg)) {
        SetWindowTextW(m_lblConflict, conflictMsg.c_str());
        return;
    }

    SetWindowTextW(m_lblConflict, L"Shortcut assigned successfully!");
    refreshShortcutsList();
}

void RXPreferencesDialog::onClearShortcut() {
    if (m_selectedAction == PlayerAction::None) return;
    ShortcutManager::instance().clearBinding(m_selectedAction);
    SetWindowTextW(m_editKeyCapture, L"None");
    SetWindowTextW(m_lblConflict, L"Shortcut cleared.");
    refreshShortcutsList();
}

void RXPreferencesDialog::onResetShortcuts() {
    ShortcutManager::instance().restoreDefaults();
    SetWindowTextW(m_editKeyCapture, L"");
    SetWindowTextW(m_lblConflict, L"Shortcuts restored to defaults.");
    refreshShortcutsList();
}

void RXPreferencesDialog::saveAndApply() {
    auto& s = Settings::instance();
    s.playback.autoPlayNext = (SendMessageW(m_chkAutoPlayNext, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.playback.rememberPosition = (SendMessageW(m_chkRememberPos, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.video.hwdecEnabled = (SendMessageW(m_chkHwdec, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.audio.exclusiveMode = (SendMessageW(m_chkWASAPIExclusive, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s.subtitle.autoLoad = (SendMessageW(m_chkAutoLoadSubs, BM_GETCHECK, 0, 0) == BST_CHECKED);

    s.save();
    MessageBoxW(m_hwnd, L"Preferences & keyboard shortcuts saved successfully!", L"Preferences", MB_ICONINFORMATION | MB_OK);
    close();
}

void RXPreferencesDialog::resetDefaults() {
    if (MessageBoxW(m_hwnd, L"Reset all application settings and keyboard shortcuts to defaults?", L"Confirm Reset", MB_ICONQUESTION | MB_YESNO) == IDYES) {
        auto& s = Settings::instance();
        s.playback.autoPlayNext = true;
        s.playback.rememberPosition = true;
        s.video.hwdecEnabled = true;
        s.audio.exclusiveMode = false;
        s.subtitle.autoLoad = true;

        SendMessageW(m_chkAutoPlayNext, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(m_chkRememberPos, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(m_chkHwdec, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(m_chkWASAPIExclusive, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(m_chkAutoLoadSubs, BM_SETCHECK, BST_CHECKED, 0);

        ShortcutManager::instance().restoreDefaults();
        refreshShortcutsList();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
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
            case WM_NOTIFY: {
                auto* nmhdr = reinterpret_cast<NMHDR*>(lParam);
                if (nmhdr->idFrom == IDC_PREF_SC_LIST && (nmhdr->code == LVN_ITEMCHANGED || nmhdr->code == NM_CLICK)) {
                    pThis->onShortcutSelected();
                }
                break;
            }
            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                WORD code = HIWORD(wParam);
                if (id == IDC_PREF_CATEGORIES && code == LBN_SELCHANGE) {
                    int sel = static_cast<int>(SendMessageW(pThis->m_listCategories, LB_GETCURSEL, 0, 0));
                    pThis->onCategoryChanged(sel);
                } else if (id == IDC_PREF_SC_SEARCH && code == EN_CHANGE) {
                    pThis->onShortcutSearch();
                } else if (id == IDC_PREF_SC_ASSIGN) {
                    pThis->onAssignShortcut();
                } else if (id == IDC_PREF_SC_CLEAR) {
                    pThis->onClearShortcut();
                } else if (id == IDC_PREF_SC_RESET) {
                    pThis->onResetShortcuts();
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
                RECT rcHeader{ 200, 15, rcClient.right - 15, 45 };
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, theme::kTextPrimary);
                HFONT oldFont = (HFONT)SelectObject(hdc, pThis->m_titleFont);

                const wchar_t* titles[] = {
                    L"General Application Settings",
                    L"Playback & Position Resume",
                    L"Video Acceleration & D3D11 Engine",
                    L"Audio Architecture & WASAPI",
                    L"Subtitle Rendering & Sync",
                    L"Keyboard Shortcuts & Key Mappings",
                    L"About RX Media Player"
                };

                int cat = pThis->m_currentCategory;
                if (cat >= 0 && cat < 7) {
                    DrawTextW(hdc, titles[cat], -1, &rcHeader, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }

                // If on About category (cat == 6), draw rich about details
                if (cat == 6) {
                    SelectObject(hdc, pThis->m_font);
                    SetTextColor(hdc, theme::kTextMuted);

                    RECT rcAbout{ 200, 60, rcClient.right - 20, rcClient.bottom - 60 };
                    std::wstring aboutText =
                        L"RX Media Player — Professional Desktop Multimedia Engine\n\n"
                        L"Version: 2.3.0 (Windows 64-bit)\n"
                        L"Author & Lead Architect: Rituraj\n"
                        L"Repository: https://github.com/jcksprwx/RX-Video-Player\n\n"
                        L"Core Architecture:\n"
                        L"  • Language: C++20 Standard\n"
                        L"  • Rendering: Direct3D 11 / D3D11VA Hardware Acceleration\n"
                        L"  • Audio: Direct Windows Audio Session API (WASAPI) with 10-Band EQ\n"
                        L"  • Shortcuts: Configurable Centralized Shortcut Dispatcher & OSD Engine\n"
                        L"  • Multimedia Engine: libmpv & FFmpeg Backends\n"
                        L"  • Metadata & History: Embedded SQLite 3 Database\n\n"
                        L"Designed and developed with pride for Windows 10 & 11.\n"
                        L"Copyright (c) 2026 Rituraj. All rights reserved.";

                    DrawTextW(hdc, aboutText.c_str(), -1, &rcAbout, DT_LEFT | DT_TOP);
                }

                // Vertical separator line between sidebar and content
                HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
                HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
                MoveToEx(hdc, 190, 15, nullptr);
                LineTo(hdc, 190, rcClient.bottom - 60);

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
