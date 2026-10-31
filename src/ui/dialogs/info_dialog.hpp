#pragma once

#include "../theme.hpp"
#include "../../engine/mpv_backend.hpp"
#include <windows.h>
#include <string>

namespace rx::ui {

class RXInfoDialog {
public:
    RXInfoDialog(const MpvBackend& engine);
    ~RXInfoDialog();

    bool show(HWND parentHwnd);
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void refreshContent();
    void copyToClipboard();

    const MpvBackend& m_engine;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_editReport = nullptr;
    HWND m_btnCopy = nullptr;
    HWND m_btnClose = nullptr;
    HFONT m_font = nullptr;
    UINT_PTR m_timerId = 0;
};

} // namespace rx::ui
