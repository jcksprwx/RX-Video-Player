#pragma once

#include "../theme.hpp"
#include "rx/types.hpp"
#include <windows.h>
#include <functional>

namespace rx::ui {

class RXVideoPanel {
public:
    using AdjustmentCallback = std::function<void(const VideoAdjustmentParams&)>;

    RXVideoPanel();
    ~RXVideoPanel();

    bool create(HWND parentHwnd, int x, int y, int width, int height);
    void move(int x, int y, int width, int height);
    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    void setParams(const VideoAdjustmentParams& params);
    const VideoAdjustmentParams& getParams() const { return m_params; }

    void setCallback(AdjustmentCallback cb) { m_callback = std::move(cb); }
    HWND getHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onHScroll(HWND trackbarHwnd, int pos);
    void onCommand(WORD id, WORD code);
    void notifyChange();
    void updateLabels();

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    bool m_visible = false;

    // Trackbars
    HWND m_tbBrightness = nullptr;
    HWND m_tbContrast = nullptr;
    HWND m_tbSaturation = nullptr;
    HWND m_tbGamma = nullptr;
    HWND m_tbHue = nullptr;
    HWND m_tbZoom = nullptr;

    // Value Labels
    HWND m_lblValBrightness = nullptr;
    HWND m_lblValContrast = nullptr;
    HWND m_lblValSaturation = nullptr;
    HWND m_lblValGamma = nullptr;
    HWND m_lblValHue = nullptr;
    HWND m_lblValZoom = nullptr;

    // Transform Buttons & Checkboxes
    HWND m_btnRotate0 = nullptr;
    HWND m_btnRotate90 = nullptr;
    HWND m_btnRotate180 = nullptr;
    HWND m_btnRotate270 = nullptr;
    HWND m_chkFlipH = nullptr;
    HWND m_chkFlipV = nullptr;
    HWND m_chkDeinterlace = nullptr;

    // Aspect Ratio Buttons
    HWND m_btnAspectAuto = nullptr;
    HWND m_btnAspect169 = nullptr;
    HWND m_btnAspect43 = nullptr;
    HWND m_btnAspect219 = nullptr;

    // Reset Button
    HWND m_btnReset = nullptr;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;

    VideoAdjustmentParams m_params;
    AdjustmentCallback m_callback;
};

} // namespace rx::ui
