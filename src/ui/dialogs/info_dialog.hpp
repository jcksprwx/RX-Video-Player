#pragma once

#include "../theme.hpp"
#include "../../engine/mpv_backend.hpp"
#include <windows.h>
#include <string>
#include <vector>

namespace rx::ui {

enum class InfoTab {
    PlaybackInfo = 0,
    FileInfo = 1,
    SystemInfo = 2
};

struct GpuAdapterTelemetry {
    std::wstring description;
    int64_t vramMb = 0;
};

struct SystemTelemetry {
    std::wstring osVersion;
    std::wstring architecture;
    std::wstring cpuName;
    int cpuLogicalCores = 0;
    std::vector<GpuAdapterTelemetry> gpuAdapters;
    std::wstring audioDevice;
    int memLoadPercent = 0;
    int64_t workingSetMb = 0;
    int64_t commitMb = 0;
    int64_t totalPhysMb = 0;
    int64_t availPhysMb = 0;
};

class RXInfoDialog {
public:
    explicit RXInfoDialog(const MpvBackend& engine);
    ~RXInfoDialog();

    bool show(HWND parentHwnd);
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onSize(int width, int height);
    void onScroll(WPARAM wParam);
    void onMouseWheel(short delta);
    void onLButtonDown(int x, int y);
    void onMouseMove(int x, int y);
    void setTab(InfoTab tab);
    void updateScrollMetrics();
    void updateLayout();
    void refreshContent();
    void copyToClipboard();
    void toggleAlwaysOnTop();

    // Text generators
    std::wstring generatePlaybackInfoText() const;
    std::wstring generateFileInfoText() const;
    std::wstring generateSystemInfoText() const;
    std::wstring generateCompleteDiagnosticReport() const;
    void collectSystemTelemetry();

    // Painting helpers
    void drawGroupBox(HDC hdc, const RECT& rc, const wchar_t* title);
    void drawReadOnlyBox(HDC hdc, const RECT& rc, const wchar_t* text);
    void drawLabelValue(HDC hdc, int x, int y, int labelW, const wchar_t* label, const wchar_t* val, COLORREF valColor = RGB(20, 24, 32));
    void drawLevelMeter(HDC hdc, int x, int y, int width, int height, const wchar_t* chLabel, float level, bool active);

    const MpvBackend& m_engine;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;

    // Child Controls
    HWND m_editDetail = nullptr;     // Tab 0 Detail Info / Used Filter List
    HWND m_editFileInfo = nullptr;   // Tab 1 File Info View
    HWND m_btnCopy = nullptr;        // Footer Copy to Clipboard Button
    HWND m_chkTopMost = nullptr;     // Footer Always On Top Checkbox
    HWND m_btnClose = nullptr;       // Footer Close Button

    // Fonts & GDI Resources
    HFONT m_fontTab = nullptr;
    HFONT m_fontBold = nullptr;
    HFONT m_fontRegular = nullptr;
    HFONT m_fontSmall = nullptr;
    HFONT m_fontMono = nullptr;
    HBRUSH m_bgBrush = nullptr;
    HBRUSH m_groupBrush = nullptr;
    HBRUSH m_footerBrush = nullptr;

    // View & Interaction State
    InfoTab m_currentTab = InfoTab::PlaybackInfo;
    int m_hoverTab = -1;
    bool m_alwaysOnTop = false;
    UINT_PTR m_timerId = 0;
    int m_scrollY = 0;
    int m_maxScrollY = 0;
    int m_contentHeight = 0;
    int m_copyFeedbackTicks = 0;

    // Telemetry Caches
    SystemTelemetry m_sysTelemetry;
    DiagnosticsReport m_diagReport;
    double m_simulatedAudioMeter = 0.0;
};

} // namespace rx::ui
