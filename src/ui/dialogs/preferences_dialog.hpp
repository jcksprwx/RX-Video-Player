#pragma once

#include "../theme.hpp"
#include "../../core/settings.hpp"
#include "../../core/shortcut_manager.hpp"
#include <windows.h>
#include <string>
#include <memory>
#include <vector>

namespace rx::ui {

class RXPreferencesDialog {
public:
    RXPreferencesDialog();
    ~RXPreferencesDialog();

    bool show(HWND parentHwnd);
    void close();

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK captureEditSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    void onInitDialog();
    void onCategoryChanged(int categoryIndex);
    void saveAndApply();
    void resetDefaults();
    void updateCategoryPanels(int categoryIndex);

    // Shortcuts panel methods
    void onShortcutSearch();
    void refreshShortcutsList();
    void onShortcutSelected();
    void onAssignShortcut();
    void onClearShortcut();
    void onResetShortcuts();

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

    // Shortcuts Controls
    HWND m_lblSearch = nullptr;
    HWND m_editShortcutSearch = nullptr;
    HWND m_listShortcutKeys = nullptr;
    HWND m_lblCapture = nullptr;
    HWND m_editKeyCapture = nullptr;
    HWND m_btnAssignShortcut = nullptr;
    HWND m_btnClearShortcut = nullptr;
    HWND m_btnResetShortcuts = nullptr;
    HWND m_lblConflict = nullptr;

    // Bottom Action Buttons
    HWND m_btnSave = nullptr;
    HWND m_btnReset = nullptr;
    HWND m_btnClose = nullptr;

    int m_currentCategory = 0;
    std::wstring m_shortcutFilter;
    PlayerAction m_selectedAction = PlayerAction::None;
    ShortcutBinding m_capturedBinding;
};

} // namespace rx::ui
