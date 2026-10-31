#include "playlist_panel.hpp"
#include "../../core/utils.hpp"
#include <windowsx.h>
#include <commctrl.h>

namespace rx::ui {

static const wchar_t* kPlaylistPanelClass = L"RXPlaylistPanelClass";
static bool s_playlistPanelClassRegistered = false;

constexpr UINT IDC_PLAYLIST_LISTBOX    = 1001;
constexpr UINT IDC_PLAYLIST_ADD        = 1002;
constexpr UINT IDC_PLAYLIST_ADDFOLDER  = 1003;
constexpr UINT IDC_PLAYLIST_CLEAR      = 1004;
constexpr UINT IDC_PLAYLIST_REPEAT     = 1005;
constexpr UINT IDC_PLAYLIST_SHUFFLE    = 1006;

RXPlaylistPanel::RXPlaylistPanel(PlaylistManager& playlistMgr)
    : m_playlistMgr(playlistMgr) {}

RXPlaylistPanel::~RXPlaylistPanel() {
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool RXPlaylistPanel::create(HWND parentHwnd, int x, int y, int width, int height) {
    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_playlistPanelClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXPlaylistPanel::wndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kPlaylistPanelClass;
        RegisterClassExW(&wc);
        s_playlistPanelClassRegistered = true;
    }

    m_hwnd = CreateWindowExW(
        0, kPlaylistPanelClass, L"Playlist",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    m_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_titleFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Header buttons
    m_btnAdd = CreateWindowExW(0, L"BUTTON", L"+ Files", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               10, 10, 58, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PLAYLIST_ADD, hInst, nullptr);
    SendMessageW(m_btnAdd, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnAddFolder = CreateWindowExW(0, L"BUTTON", L"+ Dir", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     72, 10, 52, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PLAYLIST_ADDFOLDER, hInst, nullptr);
    SendMessageW(m_btnAddFolder, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClear = CreateWindowExW(0, L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 128, 10, 48, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PLAYLIST_CLEAR, hInst, nullptr);
    SendMessageW(m_btnClear, WM_SETFONT, (WPARAM)m_font, TRUE);

    // ListBox
    m_listBox = CreateWindowExW(
        0, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT,
        10, 45, width - 20, height - 90,
        m_hwnd, (HMENU)(INT_PTR)IDC_PLAYLIST_LISTBOX, hInst, nullptr
    );
    SendMessageW(m_listBox, WM_SETFONT, (WPARAM)m_font, TRUE);

    // Footer buttons: Repeat, Shuffle
    m_btnRepeat = CreateWindowExW(0, L"BUTTON", L"Repeat: Off", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  10, height - 36, 110, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PLAYLIST_REPEAT, hInst, nullptr);
    SendMessageW(m_btnRepeat, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnShuffle = CreateWindowExW(0, L"BUTTON", L"Shuffle", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   126, height - 36, 75, 26, m_hwnd, (HMENU)(INT_PTR)IDC_PLAYLIST_SHUFFLE, hInst, nullptr);
    SendMessageW(m_btnShuffle, WM_SETFONT, (WPARAM)m_font, TRUE);

    refresh();
    return true;
}

void RXPlaylistPanel::move(int x, int y, int width, int height) {
    if (m_hwnd) {
        MoveWindow(m_hwnd, x, y, width, height, TRUE);
        onSize(width, height);
    }
}

void RXPlaylistPanel::setVisible(bool visible) {
    m_visible = visible;
    if (m_hwnd) {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void RXPlaylistPanel::refresh() {
    if (!m_listBox) return;
    SendMessageW(m_listBox, LB_RESETCONTENT, 0, 0);

    const auto& items = m_playlistMgr.getItems();
    for (size_t i = 0; i < items.size(); ++i) {
        std::wstring titleW = rx::utils::utf8ToWide(items[i].title);
        SendMessageW(m_listBox, LB_ADDSTRING, 0, (LPARAM)titleW.c_str());
    }

    int curIdx = m_playlistMgr.getCurrentIndex();
    if (curIdx >= 0 && curIdx < static_cast<int>(items.size())) {
        SendMessageW(m_listBox, LB_SETCURSEL, (WPARAM)curIdx, 0);
    }
    InvalidateRect(m_listBox, nullptr, TRUE);
}

void RXPlaylistPanel::onSize(int width, int height) {
    if (!m_listBox) return;
    int headerHeight = 45;
    int footerHeight = 45;
    int listHeight = height - headerHeight - footerHeight;
    if (listHeight < 50) listHeight = 50;

    MoveWindow(m_listBox, 10, headerHeight, width - 20, listHeight, TRUE);
    MoveWindow(m_btnRepeat, 10, height - 36, 110, 26, TRUE);
    MoveWindow(m_btnShuffle, 126, height - 36, 75, 26, TRUE);
}

LRESULT CALLBACK RXPlaylistPanel::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXPlaylistPanel* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXPlaylistPanel*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXPlaylistPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
            case WM_MEASUREITEM: {
                auto* pmi = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
                if (pmi->CtlID == IDC_PLAYLIST_LISTBOX) {
                    pmi->itemHeight = 28;
                    return TRUE;
                }
                break;
            }
            case WM_DRAWITEM: {
                auto* pdi = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                if (pdi->CtlID == IDC_PLAYLIST_LISTBOX && pdi->itemID != (UINT)-1) {
                    int index = static_cast<int>(pdi->itemID);
                    bool isPlaying = (index == pThis->m_playlistMgr.getCurrentIndex());
                    bool isSelected = (pdi->itemState & ODS_SELECTED);

                    COLORREF bg = isPlaying ? theme::kActiveItem : (isSelected ? theme::kPanelBgHover : theme::kBgColor);
                    HBRUSH hBrush = CreateSolidBrush(bg);
                    FillRect(pdi->hDC, &pdi->rcItem, hBrush);
                    DeleteObject(hBrush);

                    wchar_t textBuf[512] = {0};
                    SendMessageW(pThis->m_listBox, LB_GETTEXT, index, (LPARAM)textBuf);

                    SetBkMode(pdi->hDC, TRANSPARENT);
                    COLORREF textColor = isPlaying ? theme::kAccentHover : (isSelected ? theme::kTextPrimary : theme::kTextMuted);
                    SetTextColor(pdi->hDC, textColor);

                    RECT textRect = pdi->rcItem;
                    textRect.left += 8;
                    textRect.right -= 8;

                    std::wstring displayText;
                    if (isPlaying) {
                        displayText = L"► " + std::to_wstring(index + 1) + L". " + textBuf;
                    } else {
                        displayText = L"   " + std::to_wstring(index + 1) + L". " + textBuf;
                    }

                    DrawTextW(pdi->hDC, displayText.c_str(), -1, &textRect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    return TRUE;
                }
                break;
            }
            case WM_COMMAND:
                pThis->onCommand(LOWORD(wParam), HIWORD(wParam));
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXPlaylistPanel::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Left border separator
    HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    MoveToEx(hdc, 0, 0, nullptr);
    LineTo(hdc, 0, rc.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

void RXPlaylistPanel::onCommand(WORD id, WORD code) {
    if (id == IDC_PLAYLIST_LISTBOX && code == LBN_DBLCLK) {
        int sel = static_cast<int>(SendMessageW(m_listBox, LB_GETCURSEL, 0, 0));
        if (sel >= 0 && m_playCb) {
            m_playCb(sel);
        }
    } else if (id == IDC_PLAYLIST_ADD) {
        if (m_addFilesCb) m_addFilesCb();
    } else if (id == IDC_PLAYLIST_ADDFOLDER) {
        if (m_addFolderCb) m_addFolderCb();
    } else if (id == IDC_PLAYLIST_CLEAR) {
        m_playlistMgr.clear();
        refresh();
    } else if (id == IDC_PLAYLIST_REPEAT) {
        RepeatMode cur = m_playlistMgr.getRepeatMode();
        if (cur == RepeatMode::Off) {
            m_playlistMgr.setRepeatMode(RepeatMode::RepeatAll);
            SetWindowTextW(m_btnRepeat, L"Repeat: All");
        } else if (cur == RepeatMode::RepeatAll) {
            m_playlistMgr.setRepeatMode(RepeatMode::RepeatOne);
            SetWindowTextW(m_btnRepeat, L"Repeat: One");
        } else {
            m_playlistMgr.setRepeatMode(RepeatMode::Off);
            SetWindowTextW(m_btnRepeat, L"Repeat: Off");
        }
    } else if (id == IDC_PLAYLIST_SHUFFLE) {
        m_playlistMgr.shuffle();
        refresh();
    }
}

} // namespace rx::ui
