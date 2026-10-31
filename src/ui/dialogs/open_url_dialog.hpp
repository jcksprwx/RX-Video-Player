#pragma once

#include "../theme.hpp"
#include <windows.h>
#include <string>
#include <functional>

namespace rx::ui {

class RXOpenUrlDialog {
public:
    using OpenUrlCallback = std::function<void(const std::string& url, int cacheSec, int maxBufferMb)>;

    RXOpenUrlDialog();
    ~RXOpenUrlDialog();

    bool show(HWND parentHwnd);
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

    void setCallback(OpenUrlCallback cb) { m_callback = std::move(cb); }

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void setupControls();
    void onCommand(WORD id, WORD code);
    void onPaint(HDC hdc);
    void submit();
    void checkClipboardForUrl();

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_editUrl = nullptr;
    HWND m_comboCache = nullptr;
    HWND m_comboBuffer = nullptr;
    HWND m_btnPlay = nullptr;
    HWND m_btnCancel = nullptr;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_boldFont = nullptr;

    OpenUrlCallback m_callback;
};

} // namespace rx::ui
