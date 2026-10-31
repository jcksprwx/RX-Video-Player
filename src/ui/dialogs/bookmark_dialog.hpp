#pragma once

#include "../theme.hpp"
#include "../../engine/mpv_backend.hpp"
#include "../../library/media_database.hpp"
#include <windows.h>
#include <vector>
#include <string>

namespace rx::ui {

class RXBookmarkDialog {
public:
    RXBookmarkDialog(MpvBackend& engine);
    ~RXBookmarkDialog();

    bool show(HWND parentHwnd);
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

    void refreshList();
    void addCurrentBookmark();

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void setupControls();
    void onCommand(WORD id, WORD code);
    void onPaint(HDC hdc);
    void jumpToSelected();
    void deleteSelected();
    void addBookmarkFromInput();

    MpvBackend& m_engine;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;

    HWND m_listBookmarks = nullptr;
    HWND m_editNote = nullptr;
    HWND m_btnAdd = nullptr;
    HWND m_btnJump = nullptr;
    HWND m_btnDelete = nullptr;
    HWND m_btnClose = nullptr;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_boldFont = nullptr;

    std::vector<library::BookmarkEntry> m_entries;
};

} // namespace rx::ui
