#pragma once

#include "../theme.hpp"
#include "../../engine/mpv_backend.hpp"
#include <rx/types.hpp>
#include <windows.h>
#include <commctrl.h>
#include <functional>
#include <vector>
#include <string>

namespace rx::ui {

class RXSubtitleDialog {
public:
    using StyleChangedCallback = std::function<void(const SubtitleStyle&)>;

    RXSubtitleDialog(MpvBackend& engine);
    ~RXSubtitleDialog();

    bool show(HWND parentHwnd);
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

    void refreshTracks();
    void setStyle(const SubtitleStyle& style);
    const SubtitleStyle& getStyle() const { return m_style; }
    void setCallback(StyleChangedCallback cb) { m_callback = std::move(cb); }

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void setupControls();
    void setupTab0Font();
    void setupTab1Colors();
    void setupTab2Background();
    void setupTab3Position();
    void setupTab4Sync();

    void switchTab(int tabIndex);
    void onCommand(WORD id, WORD code);
    void onHScroll(HWND tb, int pos);
    void onPaint(HDC hdc);
    void drawPreview(HDC hdc, const RECT& rc);

    void updateLabels();
    void updateSwatches();
    void syncUiFromStyle();
    void syncStyleFromUi();

    void applyChanges(bool showOsd = false);
    void resetTab(int tabIndex);
    void resetAll();
    void openFilePicker();
    void pickColor(std::string& targetHex, HWND editBox, HWND swatchHwnd);

    MpvBackend& m_engine;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_tabHwnd = nullptr;

    // Fonts
    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_boldFont = nullptr;

    // Tab control groupings
    std::vector<HWND> m_tabControls[5];

    // Tab 0: Font & Text
    HWND m_comboTrack = nullptr;
    HWND m_btnLoadExt = nullptr;
    HWND m_comboFont = nullptr;
    HWND m_tbFontSize = nullptr;
    HWND m_lblValFontSize = nullptr;
    HWND m_chkBold = nullptr;
    HWND m_chkItalic = nullptr;

    // Tab 1: Colors & Outline
    HWND m_editTextColor = nullptr;
    HWND m_btnPickTextColor = nullptr;
    HWND m_btnResetTextColor = nullptr;
    HWND m_swatchTextColor = nullptr;

    HWND m_chkOutline = nullptr;
    HWND m_editOutlineColor = nullptr;
    HWND m_btnPickOutlineColor = nullptr;
    HWND m_swatchOutlineColor = nullptr;
    HWND m_tbBorderSize = nullptr;
    HWND m_lblValBorderSize = nullptr;

    HWND m_chkShadow = nullptr;
    HWND m_editShadowColor = nullptr;
    HWND m_btnPickShadowColor = nullptr;
    HWND m_swatchShadowColor = nullptr;
    HWND m_tbShadow = nullptr;
    HWND m_lblValShadow = nullptr;

    // Tab 2: Background & Layout
    HWND m_chkBackground = nullptr;
    HWND m_editBgColor = nullptr;
    HWND m_btnPickBgColor = nullptr;
    HWND m_swatchBgColor = nullptr;
    HWND m_tbBgOpacity = nullptr;
    HWND m_lblValBgOpacity = nullptr;
    HWND m_tbMargin = nullptr;
    HWND m_lblValMargin = nullptr;

    // Tab 3: Position & Alignment
    HWND m_tbPosition = nullptr;
    HWND m_lblValPosition = nullptr;
    HWND m_comboAlignment = nullptr;

    // Tab 4: Sync & Advanced
    HWND m_lblValDelay = nullptr;
    HWND m_btnMinus250 = nullptr;
    HWND m_btnMinus100 = nullptr;
    HWND m_btnPlus100 = nullptr;
    HWND m_btnPlus250 = nullptr;
    HWND m_btnResetDelay = nullptr;
    HWND m_tbDelay = nullptr;
    HWND m_chkAutoLoad = nullptr;
    HWND m_chkOverrideAss = nullptr;
    HWND m_editLangs = nullptr;

    // Bottom Action Buttons
    HWND m_btnApply = nullptr;
    HWND m_btnOk = nullptr;
    HWND m_btnCancel = nullptr;
    HWND m_btnResetTab = nullptr;
    HWND m_btnResetAll = nullptr;

    SubtitleStyle m_style;
    SubtitleStyle m_originalStyle;
    std::vector<MediaTrack> m_tracks;
    StyleChangedCallback m_callback;
    int m_activeTab = 0;
};

} // namespace rx::ui
