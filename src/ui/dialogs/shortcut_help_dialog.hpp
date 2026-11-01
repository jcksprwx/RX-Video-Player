#pragma once

#include "../theme.hpp"
#include "../../core/shortcut_manager.hpp"
#include <windows.h>
#include <string>
#include <vector>

namespace rx::ui {

class RXShortcutHelpDialog {
public:
    RXShortcutHelpDialog();
    ~RXShortcutHelpDialog();

    bool show(HWND parentHwnd);
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onInitDialog();
    void onSearchChanged();
    void refreshList();

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_editSearch = nullptr;
    HWND m_listShortcuts = nullptr;
    HWND m_btnClose = nullptr;

    HFONT m_font = nullptr;
    HFONT m_fontBold = nullptr;
    HFONT m_fontMono = nullptr;

    std::wstring m_filterText;
};

} // namespace rx::ui
