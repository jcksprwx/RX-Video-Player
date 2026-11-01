#include "shortcut_help_dialog.hpp"
#include "../../core/utils.hpp"
#include <commctrl.h>
#include <algorithm>

namespace rx::ui {

static const wchar_t* kShortcutHelpClass = L"RXShortcutHelpDialogClass";
static bool s_shortcutHelpClassRegistered = false;

constexpr UINT IDC_HELP_SEARCH = 7001;
constexpr UINT IDC_HELP_LIST   = 7002;
constexpr UINT IDC_HELP_CLOSE  = 7003;

RXShortcutHelpDialog::RXShortcutHelpDialog() = default;

RXShortcutHelpDialog::~RXShortcutHelpDialog() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_fontBold) DeleteObject(m_fontBold);
    if (m_fontMono) DeleteObject(m_fontMono);
}

bool RXShortcutHelpDialog::show(HWND parentHwnd) {
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

    if (!s_shortcutHelpClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXShortcutHelpDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(246, 248, 250));
        wc.lpszClassName = kShortcutHelpClass;
        RegisterClassExW(&wc);
        s_shortcutHelpClassRegistered = true;
    }

    int width = 760;
    int height = 560;
    RECT rcParent;
    if (parentHwnd && GetWindowRect(parentHwnd, &rcParent)) {
        int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;
        m_hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kShortcutHelpClass, L"Keyboard Shortcuts — RX Media Player",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
            x, y, width, height,
            parentHwnd, nullptr, hInst, this
        );
    } else {
        m_hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kShortcutHelpClass, L"Keyboard Shortcuts — RX Media Player",
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
    m_fontBold = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontMono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    onInitDialog();

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void RXShortcutHelpDialog::close() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXShortcutHelpDialog::onInitDialog() {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Search Label & Edit box
    HWND hLblSearch = CreateWindowExW(0, L"STATIC", L"Search Shortcuts:", WS_CHILD | WS_VISIBLE,
                                      16, 18, 120, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(hLblSearch, WM_SETFONT, (WPARAM)m_fontBold, TRUE);

    m_editSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                   140, 15, 340, 26,
                                   m_hwnd, (HMENU)(INT_PTR)IDC_HELP_SEARCH, hInst, nullptr);
    SendMessageW(m_editSearch, WM_SETFONT, (WPARAM)m_font, TRUE);

    // Report ListView
    m_listShortcuts = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        16, 52, 712, 420,
        m_hwnd, (HMENU)(INT_PTR)IDC_HELP_LIST, hInst, nullptr
    );
    SendMessageW(m_listShortcuts, WM_SETFONT, (WPARAM)m_font, TRUE);
    ListView_SetExtendedListViewStyle(m_listShortcuts, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    // Columns
    LVCOLUMNW lvc{};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.cx = 110;
    lvc.pszText = const_cast<wchar_t*>(L"Category");
    ListView_InsertColumn(m_listShortcuts, 0, &lvc);

    lvc.cx = 210;
    lvc.pszText = const_cast<wchar_t*>(L"Action");
    ListView_InsertColumn(m_listShortcuts, 1, &lvc);

    lvc.cx = 160;
    lvc.pszText = const_cast<wchar_t*>(L"Shortcut");
    ListView_InsertColumn(m_listShortcuts, 2, &lvc);

    lvc.cx = 210;
    lvc.pszText = const_cast<wchar_t*>(L"Description");
    ListView_InsertColumn(m_listShortcuts, 3, &lvc);

    // Close button
    m_btnClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 638, 484, 90, 28, m_hwnd, (HMENU)(INT_PTR)IDC_HELP_CLOSE, hInst, nullptr);
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_font, TRUE);

    refreshList();
}

void RXShortcutHelpDialog::onSearchChanged() {
    wchar_t buf[128] = {0};
    GetWindowTextW(m_editSearch, buf, 128);
    m_filterText = buf;
    std::transform(m_filterText.begin(), m_filterText.end(), m_filterText.begin(), ::towlower);
    refreshList();
}

void RXShortcutHelpDialog::refreshList() {
    if (!m_listShortcuts || !IsWindow(m_listShortcuts)) return;
    ListView_DeleteAllItems(m_listShortcuts);

    const auto& actions = ShortcutManager::instance().getActionDefinitions();
    int itemIdx = 0;

    for (const auto& a : actions) {
        std::wstring bindStr = ShortcutManager::instance().getBindingString(a.action);

        if (!m_filterText.empty()) {
            std::wstring catL = a.category;
            std::wstring actL = a.label;
            std::wstring bndL = bindStr;
            std::wstring dscL = a.description;
            std::transform(catL.begin(), catL.end(), catL.begin(), ::towlower);
            std::transform(actL.begin(), actL.end(), actL.begin(), ::towlower);
            std::transform(bndL.begin(), bndL.end(), bndL.begin(), ::towlower);
            std::transform(dscL.begin(), dscL.end(), dscL.begin(), ::towlower);

            if (catL.find(m_filterText) == std::wstring::npos &&
                actL.find(m_filterText) == std::wstring::npos &&
                bndL.find(m_filterText) == std::wstring::npos &&
                dscL.find(m_filterText) == std::wstring::npos) {
                continue;
            }
        }

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = itemIdx;
        lvi.iSubItem = 0;
        lvi.pszText = const_cast<wchar_t*>(a.category.c_str());
        ListView_InsertItem(m_listShortcuts, &lvi);

        ListView_SetItemText(m_listShortcuts, itemIdx, 1, const_cast<wchar_t*>(a.label.c_str()));
        ListView_SetItemText(m_listShortcuts, itemIdx, 2, const_cast<wchar_t*>(bindStr.c_str()));
        ListView_SetItemText(m_listShortcuts, itemIdx, 3, const_cast<wchar_t*>(a.description.c_str()));

        itemIdx++;
    }
}

LRESULT CALLBACK RXShortcutHelpDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXShortcutHelpDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXShortcutHelpDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXShortcutHelpDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                if (id == IDC_HELP_SEARCH && HIWORD(wParam) == EN_CHANGE) {
                    pThis->onSearchChanged();
                    return 0;
                } else if (id == IDC_HELP_CLOSE || id == IDCANCEL) {
                    pThis->close();
                    return 0;
                }
                break;
            }
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    pThis->close();
                    return 0;
                }
                break;
            case WM_CLOSE:
                pThis->close();
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace rx::ui
