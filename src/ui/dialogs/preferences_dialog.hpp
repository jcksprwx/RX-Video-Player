#pragma once

#include "../theme.hpp"
#include "../../core/settings.hpp"
#include <windows.h>
#include <string>
#include <memory>

namespace rx::ui {

class RXPreferencesDialog {
public:
    RXPreferencesDialog();
    ~RXPreferencesDialog();

    bool show(HWND parentHwnd);
    void close();

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onInitDialog();
    void onCategoryChanged(int categoryIndex);
    void saveAndApply();
    void resetDefaults();
    void updateCategoryPanels(int categoryIndex);

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_listCategories = nullptr;
    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_boldFont = nullptr;

    // Category container controls
    HWND m_chkAutoPlayNext = nullptr;
    HWND m_chkRememberPos = nullptr;
    HWND m_chkHwdec = nullptr;
    HWND m_chkWASAPIExclusive = nullptr;
    HWND m_chkAutoLoadSubs = nullptr;

    // Buttons
    HWND m_btnSave = nullptr;
    HWND m_btnReset = nullptr;
    HWND m_btnClose = nullptr;

    int m_currentCategory = 0;
};

} // namespace rx::ui
