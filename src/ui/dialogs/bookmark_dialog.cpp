#include "bookmark_dialog.hpp"
#include "../../core/utils.hpp"
#include <windowsx.h>
#include <sstream>

namespace rx::ui {

static const wchar_t* kBookmarkDlgClass = L"RXBookmarkDialogClass";
static bool s_bmDialogClassRegistered = false;

constexpr UINT IDC_BM_LIST    = 9001;
constexpr UINT IDC_BM_EDIT    = 9002;
constexpr UINT IDC_BM_ADD     = 9003;
constexpr UINT IDC_BM_JUMP    = 9004;
constexpr UINT IDC_BM_DELETE  = 9005;
constexpr UINT IDC_BM_CLOSE   = 9006;

RXBookmarkDialog::RXBookmarkDialog(MpvBackend& engine)
    : m_engine(engine) {}

RXBookmarkDialog::~RXBookmarkDialog() {
    close();
    if (m_font) DeleteObject(m_font);
    if (m_titleFont) DeleteObject(m_titleFont);
    if (m_boldFont) DeleteObject(m_boldFont);
}

void RXBookmarkDialog::close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool RXBookmarkDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        refreshList();
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_bmDialogClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXBookmarkDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
        wc.lpszClassName = kBookmarkDlgClass;
        RegisterClassExW(&wc);
        s_bmDialogClassRegistered = true;
    }

    int width = 520;
    int height = 480;
    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kBookmarkDlgClass, L"Scene Bookmarks (Ctrl+B)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        x, y, width, height,
        parentHwnd, nullptr, hInst, this
    );

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);
    setupControls();
    refreshList();

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void RXBookmarkDialog::setupControls() {
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
    int fullW = 520 - padX * 2;

    // Header info
    std::string filename = m_engine.getMediaInfo().filename;
    std::wstring headerText = filename.empty()
        ? L"Saved Scene Bookmarks"
        : (L"Bookmarks for: " + rx::utils::utf8ToWide(filename));

    HWND lblHeader = CreateWindowExW(0, L"STATIC", headerText.c_str(),
                                     WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                                     padX, curY, fullW, 20, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblHeader, WM_SETFONT, (WPARAM)m_titleFont, TRUE);
    curY += 26;

    // Bookmarks ListBox
    m_listBookmarks = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                                      padX, curY, fullW, 240, m_hwnd, (HMENU)(INT_PTR)IDC_BM_LIST, hInst, nullptr);
    SendMessageW(m_listBookmarks, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 250;

    // Add Bookmark Section
    HWND lblNote = CreateWindowExW(0, L"STATIC", L"Add Note / Bookmark at Current Timestamp:",
                                   WS_CHILD | WS_VISIBLE | SS_LEFT,
                                   padX, curY, fullW, 18, m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(lblNote, WM_SETFONT, (WPARAM)m_font, TRUE);
    curY += 22;

    m_editNote = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
                                 padX, curY, fullW - 145, 26, m_hwnd, (HMENU)(INT_PTR)IDC_BM_EDIT, hInst, nullptr);
    SendMessageW(m_editNote, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnAdd = CreateWindowExW(0, L"BUTTON", L"+ Add Bookmark", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               padX + fullW - 135, curY, 135, 26, m_hwnd, (HMENU)(INT_PTR)IDC_BM_ADD, hInst, nullptr);
    SendMessageW(m_btnAdd, WM_SETFONT, (WPARAM)m_boldFont, TRUE);
    curY += 38;

    // Action buttons
    m_btnJump = CreateWindowExW(0, L"BUTTON", L"▶ Jump to Selected", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                padX, curY, 150, 30, m_hwnd, (HMENU)(INT_PTR)IDC_BM_JUMP, hInst, nullptr);
    SendMessageW(m_btnJump, WM_SETFONT, (WPARAM)m_boldFont, TRUE);

    m_btnDelete = CreateWindowExW(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  padX + 160, curY, 80, 30, m_hwnd, (HMENU)(INT_PTR)IDC_BM_DELETE, hInst, nullptr);
    SendMessageW(m_btnDelete, WM_SETFONT, (WPARAM)m_font, TRUE);

    m_btnClose = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 padX + fullW - 85, curY, 85, 30, m_hwnd, (HMENU)(INT_PTR)IDC_BM_CLOSE, hInst, nullptr);
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_font, TRUE);
}

void RXBookmarkDialog::refreshList() {
    SendMessageW(m_listBookmarks, LB_RESETCONTENT, 0, 0);

    std::string path = m_engine.getMediaInfo().fullPath;
    if (!path.empty()) {
        m_entries = library::MediaDatabase::instance().getBookmarksForFile(path);
    } else {
        m_entries = library::MediaDatabase::instance().getAllBookmarks();
    }

    if (m_entries.empty()) {
        SendMessageW(m_listBookmarks, LB_ADDSTRING, 0, (LPARAM)L"[No bookmarks recorded for this media yet]");
        return;
    }

    for (const auto& b : m_entries) {
        std::wostringstream wss;
        wss << L"⏱ [" << rx::utils::utf8ToWide(rx::utils::formatTime(b.timestampSec)) << L"] ";
        if (!b.note.empty()) {
            wss << rx::utils::utf8ToWide(b.note);
        } else {
            wss << L"Scene Bookmark";
        }
        SendMessageW(m_listBookmarks, LB_ADDSTRING, 0, (LPARAM)wss.str().c_str());
    }
}

void RXBookmarkDialog::addCurrentBookmark() {
    std::string path = m_engine.getMediaInfo().fullPath;
    if (path.empty()) return;

    double pos = m_engine.getCurrentPosition();
    library::MediaDatabase::instance().addBookmark(path, pos, "Bookmark at " + rx::utils::formatTime(pos));
    if (m_hwnd && IsWindow(m_hwnd)) {
        refreshList();
    }
}

void RXBookmarkDialog::addBookmarkFromInput() {
    std::string path = m_engine.getMediaInfo().fullPath;
    if (path.empty()) return;

    wchar_t buf[512] = { 0 };
    GetWindowTextW(m_editNote, buf, 512);
    std::string note = rx::utils::wideToUtf8(buf);
    if (note.empty()) {
        note = "Bookmark at " + rx::utils::formatTime(m_engine.getCurrentPosition());
    }

    double pos = m_engine.getCurrentPosition();
    library::MediaDatabase::instance().addBookmark(path, pos, note);
    SetWindowTextW(m_editNote, L"");
    refreshList();
}

void RXBookmarkDialog::jumpToSelected() {
    int sel = static_cast<int>(SendMessageW(m_listBookmarks, LB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < static_cast<int>(m_entries.size())) {
        m_engine.seek(m_entries[sel].timestampSec, true);
    }
}

void RXBookmarkDialog::deleteSelected() {
    int sel = static_cast<int>(SendMessageW(m_listBookmarks, LB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < static_cast<int>(m_entries.size())) {
        library::MediaDatabase::instance().deleteBookmark(m_entries[sel].id);
        refreshList();
    }
}

void RXBookmarkDialog::onCommand(WORD id, WORD code) {
    if (id == IDC_BM_ADD) {
        addBookmarkFromInput();
    } else if (id == IDC_BM_JUMP) {
        jumpToSelected();
    } else if (id == IDC_BM_DELETE) {
        deleteSelected();
    } else if (id == IDC_BM_CLOSE) {
        close();
    } else if (id == IDC_BM_LIST && code == LBN_DBLCLK) {
        jumpToSelected();
    }
}

void RXBookmarkDialog::onPaint(HDC hdc) {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
}

LRESULT CALLBACK RXBookmarkDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXBookmarkDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXBookmarkDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXBookmarkDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
