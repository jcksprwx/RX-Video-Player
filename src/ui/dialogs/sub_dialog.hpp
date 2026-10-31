#pragma once

#include "../theme.hpp"
#include "../../engine/mpv_backend.hpp"
#include <rx/types.hpp>
#include <windows.h>
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
    void onCommand(WORD id, WORD code);
    void onHScroll(HWND tb, int pos);
    void onPaint(HDC hdc);
    void applyChanges();
    void resetDefaults();
    void openFilePicker();
    void updateLabels();

    MpvBackend& m_engine;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;

    // Track selection
    HWND m_comboTrack = nullptr;
    HWND m_btnLoadExt = nullptr;

    // Delay
    HWND m_tbDelay = nullptr;
    HWND m_lblValDelay = nullptr;
    HWND m_btnMinusDelay = nullptr;
    HWND m_btnPlusDelay = nullptr;

    // Font & Styling
    HWND m_comboFont = nullptr;
    HWND m_tbFontSize = nullptr;
    HWND m_lblValFontSize = nullptr;
    HWND m_chkBold = nullptr;
    HWND m_chkItalic = nullptr;

    // Colors & Borders
    HWND m_comboTextColor = nullptr;
    HWND m_comboBorderColor = nullptr;
    HWND m_tbBorderSize = nullptr;
    HWND m_lblValBorderSize = nullptr;
    HWND m_tbShadow = nullptr;
    HWND m_lblValShadow = nullptr;
    HWND m_tbMargin = nullptr;
    HWND m_lblValMargin = nullptr;

    // Actions
    HWND m_btnApply = nullptr;
    HWND m_btnReset = nullptr;
    HWND m_btnClose = nullptr;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_boldFont = nullptr;

    SubtitleStyle m_style;
    std::vector<MediaTrack> m_tracks;
    StyleChangedCallback m_callback;
};

} // namespace rx::ui
