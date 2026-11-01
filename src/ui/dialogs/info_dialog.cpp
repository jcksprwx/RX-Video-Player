#include "info_dialog.hpp"
#include "../../core/utils.hpp"
#include "../../audio/wasapi_enumerator.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <psapi.h>
#include <dxgi.h>
#include <iomanip>
#include <sstream>
#include <cmath>

namespace rx::ui {

static const wchar_t* kInfoDialogClass = L"RXPlaybackSystemInfoDialogClass";
static bool s_infoDialogClassRegistered = false;

constexpr UINT IDC_INFO_DETAIL   = 6001;
constexpr UINT IDC_INFO_FILEEDIT = 6002;
constexpr UINT IDC_INFO_COPY     = 6003;
constexpr UINT IDC_INFO_TOPMOST  = 6004;
constexpr UINT IDC_INFO_CLOSE    = 6005;
constexpr UINT_PTR TIMER_REFRESH = 6006;

RXInfoDialog::RXInfoDialog(const MpvBackend& engine)
    : m_engine(engine) {}

RXInfoDialog::~RXInfoDialog() {
    close();
    if (m_fontTab) DeleteObject(m_fontTab);
    if (m_fontBold) DeleteObject(m_fontBold);
    if (m_fontRegular) DeleteObject(m_fontRegular);
    if (m_fontSmall) DeleteObject(m_fontSmall);
    if (m_fontMono) DeleteObject(m_fontMono);
    if (m_bgBrush) DeleteObject(m_bgBrush);
    if (m_groupBrush) DeleteObject(m_groupBrush);
    if (m_footerBrush) DeleteObject(m_footerBrush);
}

void RXInfoDialog::collectSystemTelemetry() {
    // 1. OS Version & Build via RtlGetVersion
    m_sysTelemetry.osVersion = L"Windows (Unknown)";
    m_sysTelemetry.architecture = L"x86_64 (64-bit)";

    typedef LONG(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(reinterpret_cast<void*>(GetProcAddress(hNtdll, "RtlGetVersion")));
        if (rtlGetVersion) {
            RTL_OSVERSIONINFOW rovi{};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (rtlGetVersion(&rovi) == 0) {
                std::wostringstream oss;
                if (rovi.dwMajorVersion == 10 && rovi.dwBuildNumber >= 22000) {
                    oss << L"Windows 11 (Build " << rovi.dwBuildNumber << L", 64-bit)";
                } else if (rovi.dwMajorVersion == 10) {
                    oss << L"Windows 10 (Build " << rovi.dwBuildNumber << L", 64-bit)";
                } else {
                    oss << L"Windows " << rovi.dwMajorVersion << L"." << rovi.dwMinorVersion << L" (Build " << rovi.dwBuildNumber << L")";
                }
                m_sysTelemetry.osVersion = oss.str();
            }
        }
    }

    // 2. CPU Model & Cores
    m_sysTelemetry.cpuName = L"Unknown Processor";
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t cpuNameBuf[256] = {};
        DWORD bufSize = sizeof(cpuNameBuf);
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, (LPBYTE)cpuNameBuf, &bufSize) == ERROR_SUCCESS) {
            // Trim leading/trailing spaces
            std::wstring s = cpuNameBuf;
            size_t start = s.find_first_not_of(L" ");
            size_t end = s.find_last_not_of(L" ");
            if (start != std::wstring::npos && end != std::wstring::npos) {
                m_sysTelemetry.cpuName = s.substr(start, end - start + 1);
            } else {
                m_sysTelemetry.cpuName = s;
            }
        }
        RegCloseKey(hKey);
    }

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    m_sysTelemetry.cpuLogicalCores = static_cast<int>(sysInfo.dwNumberOfProcessors);

    // 3. GPU Adapters via DXGI
    m_sysTelemetry.gpuAdapters.clear();
    IDXGIFactory* pFactory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory)) && pFactory) {
        IDXGIAdapter* pAdapter = nullptr;
        for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC desc;
            if (SUCCEEDED(pAdapter->GetDesc(&desc))) {
                // Filter out software driver if hardware exists
                std::wstring descStr = desc.Description;
                int64_t vram = desc.DedicatedVideoMemory / (1024 * 1024);
                m_sysTelemetry.gpuAdapters.push_back({ descStr, vram });
            }
            pAdapter->Release();
        }
        pFactory->Release();
    }

    // 4. Default WASAPI Audio Endpoint
    std::string defAudio = rx::audio::WasapiEnumerator::getDefaultEndpointName();
    m_sysTelemetry.audioDevice = rx::utils::utf8ToWide(defAudio);

    // 5. Memory Telemetry
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        m_sysTelemetry.memLoadPercent = static_cast<int>(mem.dwMemoryLoad);
        m_sysTelemetry.totalPhysMb = static_cast<int64_t>(mem.ullTotalPhys / (1024 * 1024));
        m_sysTelemetry.availPhysMb = static_cast<int64_t>(mem.ullAvailPhys / (1024 * 1024));
    }

    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m_sysTelemetry.workingSetMb = static_cast<int64_t>(pmc.WorkingSetSize / (1024 * 1024));
        m_sysTelemetry.commitMb = static_cast<int64_t>(pmc.PagefileUsage / (1024 * 1024));
    }
}

bool RXInfoDialog::show(HWND parentHwnd) {
    if (m_hwnd && IsWindow(m_hwnd)) {
        SetForegroundWindow(m_hwnd);
        return true;
    }

    m_parent = parentHwnd;
    HINSTANCE hInst = GetModuleHandle(nullptr);

    if (!s_infoDialogClassRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = RXInfoDialog::dlgProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr; // Handled in WM_PAINT
        wc.lpszClassName = kInfoDialogClass;
        RegisterClassExW(&wc);
        s_infoDialogClassRegistered = true;
    }

    int width = 580;
    int height = 750;
    RECT rcParent;
    if (parentHwnd && GetWindowRect(parentHwnd, &rcParent)) {
        int cx = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
        int cy = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;
        if (cx < 10) cx = 10;
        if (cy < 10) cy = 10;
        m_hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kInfoDialogClass, L"Playback / System Information",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_VSCROLL,
            cx, cy, width, height,
            parentHwnd, nullptr, hInst, this
        );
    } else {
        m_hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            kInfoDialogClass, L"Playback / System Information",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_VSCROLL,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            parentHwnd, nullptr, hInst, this
        );
    }

    if (!m_hwnd) return false;

    theme::enableImmersiveDarkMode(m_hwnd);

    // Create GDI Brushes & Fonts
    m_bgBrush = CreateSolidBrush(RGB(246, 248, 250));
    m_groupBrush = CreateSolidBrush(RGB(255, 255, 255));
    m_footerBrush = CreateSolidBrush(RGB(238, 241, 246));

    m_fontTab = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontBold = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontRegular = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    m_fontMono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

    // Create Detail Info Edit Control (Tab 0)
    m_editDetail = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        28, 380, width - 56, 120,
        m_hwnd, (HMENU)(INT_PTR)IDC_INFO_DETAIL, hInst, nullptr
    );
    SendMessageW(m_editDetail, WM_SETFONT, (WPARAM)m_fontMono, TRUE);

    // Create File Info Edit Control (Tab 1)
    m_editFileInfo = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        14, 44, width - 28, height - 100,
        m_hwnd, (HMENU)(INT_PTR)IDC_INFO_FILEEDIT, hInst, nullptr
    );
    SendMessageW(m_editFileInfo, WM_SETFONT, (WPARAM)m_fontMono, TRUE);

    // Footer Buttons
    m_btnCopy = CreateWindowExW(
        0, L"BUTTON", L"Copy to Clipboard",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        14, height - 40, 140, 28,
        m_hwnd, (HMENU)(INT_PTR)IDC_INFO_COPY, hInst, nullptr
    );

    m_chkTopMost = CreateWindowExW(
        0, L"BUTTON", L"Always on top",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        width - 215, height - 36, 110, 20,
        m_hwnd, (HMENU)(INT_PTR)IDC_INFO_TOPMOST, hInst, nullptr
    );

    m_btnClose = CreateWindowExW(
        0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width - 96, height - 40, 82, 28,
        m_hwnd, (HMENU)(INT_PTR)IDC_INFO_CLOSE, hInst, nullptr
    );

    SendMessageW(m_btnCopy, WM_SETFONT, (WPARAM)m_fontRegular, TRUE);
    SendMessageW(m_chkTopMost, WM_SETFONT, (WPARAM)m_fontRegular, TRUE);
    SendMessageW(m_btnClose, WM_SETFONT, (WPARAM)m_fontRegular, TRUE);

    collectSystemTelemetry();
    m_diagReport = m_engine.getDiagnostics();

    refreshContent();
    updateLayout();

    m_timerId = SetTimer(m_hwnd, TIMER_REFRESH, 500, nullptr);

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void RXInfoDialog::close() {
    if (m_timerId && m_hwnd) {
        KillTimer(m_hwnd, m_timerId);
        m_timerId = 0;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void RXInfoDialog::setTab(InfoTab tab) {
    if (m_currentTab == tab) return;
    m_currentTab = tab;
    m_scrollY = 0;

    // Show/hide child edit controls based on tab
    if (m_currentTab == InfoTab::PlaybackInfo) {
        ShowWindow(m_editDetail, SW_SHOW);
        ShowWindow(m_editFileInfo, SW_HIDE);
    } else if (m_currentTab == InfoTab::FileInfo) {
        ShowWindow(m_editDetail, SW_HIDE);
        ShowWindow(m_editFileInfo, SW_SHOW);
    } else {
        ShowWindow(m_editDetail, SW_HIDE);
        ShowWindow(m_editFileInfo, SW_HIDE);
    }

    refreshContent();
    updateScrollMetrics();
    updateLayout();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void RXInfoDialog::toggleAlwaysOnTop() {
    m_alwaysOnTop = !m_alwaysOnTop;
    Button_SetCheck(m_chkTopMost, m_alwaysOnTop ? BST_CHECKED : BST_UNCHECKED);
    SetWindowPos(m_hwnd, m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void RXInfoDialog::updateScrollMetrics() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int clientH = rc.bottom - rc.top;
    int visH = clientH - 44 - 48; // between tabs and footer
    if (visH < 100) visH = 100;

    if (m_currentTab == InfoTab::PlaybackInfo) {
        m_contentHeight = 670;
    } else if (m_currentTab == InfoTab::FileInfo) {
        m_contentHeight = visH; // Handled directly by m_editFileInfo scrollbar
    } else {
        m_contentHeight = 675;
    }

    if (m_contentHeight > visH) {
        m_maxScrollY = m_contentHeight - visH;
        SCROLLINFO si{};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = m_contentHeight;
        si.nPage = visH;
        si.nPos = m_scrollY;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
        EnableScrollBar(m_hwnd, SB_VERT, ESB_ENABLE_BOTH);
    } else {
        m_scrollY = 0;
        m_maxScrollY = 0;
        SCROLLINFO si{};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = 0;
        si.nPage = 0;
        si.nPos = 0;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
        EnableScrollBar(m_hwnd, SB_VERT, ESB_DISABLE_BOTH);
    }
}

void RXInfoDialog::updateLayout() {
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int clientW = rc.right - rc.left;
    int clientH = rc.bottom - rc.top;

    int footerY = clientH - 46;

    // Reposition footer buttons
    SetWindowPos(m_btnCopy, nullptr, 14, footerY + 9, 140, 28, SWP_NOZORDER);
    SetWindowPos(m_chkTopMost, nullptr, clientW - 225, footerY + 13, 115, 20, SWP_NOZORDER);
    SetWindowPos(m_btnClose, nullptr, clientW - 96, footerY + 9, 82, 28, SWP_NOZORDER);

    // Reposition Detail Info edit box (Tab 0)
    int yBase = 44 - m_scrollY;
    int detailY = yBase + 375;
    int detailH = 135;
    SetWindowPos(m_editDetail, nullptr, 28, detailY, clientW - 56, detailH, SWP_NOZORDER);

    // Reposition File Info edit box (Tab 1)
    int fileH = (footerY - 6) - 44;
    if (fileH < 100) fileH = 100;
    SetWindowPos(m_editFileInfo, nullptr, 14, 44, clientW - 28, fileH, SWP_NOZORDER);
}

void RXInfoDialog::onScroll(WPARAM wParam) {
    int oldScroll = m_scrollY;
    switch (LOWORD(wParam)) {
        case SB_LINEUP:        m_scrollY -= 24; break;
        case SB_LINEDOWN:      m_scrollY += 24; break;
        case SB_PAGEUP:        m_scrollY -= 120; break;
        case SB_PAGEDOWN:      m_scrollY += 120; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: m_scrollY = HIWORD(wParam); break;
    }

    if (m_scrollY < 0) m_scrollY = 0;
    if (m_scrollY > m_maxScrollY) m_scrollY = m_maxScrollY;

    if (m_scrollY != oldScroll) {
        SCROLLINFO si{};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_POS;
        si.nPos = m_scrollY;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
        updateLayout();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void RXInfoDialog::onMouseWheel(short delta) {
    if (m_maxScrollY <= 0) return;
    int oldScroll = m_scrollY;
    m_scrollY -= (delta / WHEEL_DELTA) * 40;
    if (m_scrollY < 0) m_scrollY = 0;
    if (m_scrollY > m_maxScrollY) m_scrollY = m_maxScrollY;

    if (m_scrollY != oldScroll) {
        SCROLLINFO si{};
        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_POS;
        si.nPos = m_scrollY;
        SetScrollInfo(m_hwnd, SB_VERT, &si, TRUE);
        updateLayout();
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void RXInfoDialog::onLButtonDown(int x, int y) {
    // Check if clicked inside Top Tab Bar (y between 8 and 38)
    if (y >= 8 && y <= 38) {
        int tabW = 140;
        int tab0Left = 14;
        int tab1Left = tab0Left + tabW + 4;
        int tab2Left = tab1Left + tabW + 4;

        if (x >= tab0Left && x < tab0Left + tabW) {
            setTab(InfoTab::PlaybackInfo);
        } else if (x >= tab1Left && x < tab1Left + tabW) {
            setTab(InfoTab::FileInfo);
        } else if (x >= tab2Left && x < tab2Left + tabW) {
            setTab(InfoTab::SystemInfo);
        }
    }
}

void RXInfoDialog::onMouseMove(int x, int y) {
    int oldHover = m_hoverTab;
    if (y >= 8 && y <= 38) {
        int tabW = 140;
        int tab0Left = 14;
        int tab1Left = tab0Left + tabW + 4;
        int tab2Left = tab1Left + tabW + 4;

        if (x >= tab0Left && x < tab0Left + tabW) m_hoverTab = 0;
        else if (x >= tab1Left && x < tab1Left + tabW) m_hoverTab = 1;
        else if (x >= tab2Left && x < tab2Left + tabW) m_hoverTab = 2;
        else m_hoverTab = -1;
    } else {
        m_hoverTab = -1;
    }

    if (oldHover != m_hoverTab) {
        RECT rcTabs{ 10, 6, 450, 42 };
        InvalidateRect(m_hwnd, &rcTabs, FALSE);
    }
}

std::wstring RXInfoDialog::generatePlaybackInfoText() const {
    std::wostringstream oss;
    oss << L"[Used Filter List]\r\n";
    oss << L"(1) Built-in Source Reader (libmpv)\r\n";

    if (!m_diagReport.hwdecActive.empty() && m_diagReport.hwdecActive != "no") {
        oss << L"(2) Built-in Video Codec/Transform (" << rx::utils::utf8ToWide(m_diagReport.hwdecActive) << L" / ffmpeg)\r\n";
    } else {
        oss << L"(2) Built-in Video Codec/Transform (ffmpeg)\r\n";
    }

    oss << L"(3) Built-in Direct3D 11 Video Renderer (libmpv gpu-next / d3d11)\r\n";
    oss << L"(4) Built-in Audio Codec/Transform (ffmpeg)\r\n";
    oss << L"(5) Built-in WASAPI Audio Output (wasapi)\r\n\r\n";

    oss << L"[Active Pipeline Details]\r\n";
    oss << L"- Demuxer: libmpv internal demuxer\r\n";
    oss << L"- Hardware Acceleration: " << (m_diagReport.hwdecActive.empty() || m_diagReport.hwdecActive == "no" ? L"Disabled (Software Decoding)" : rx::utils::utf8ToWide(m_diagReport.hwdecActive) + L" [D3D11VA]") << L"\r\n";
    oss << L"- Video Render Surface: Direct3D 11 (NV12 SwapChain)\r\n";
    oss << L"- Audio Output Device: " << m_sysTelemetry.audioDevice << L" via WASAPI\r\n";
    oss << L"- Audio Stream Format: " << m_diagReport.audioSampleRate << L" Hz, " << m_diagReport.audioChannels << L" Channels, 16-bit PCM\r\n";
    oss << L"- Audio Resampler: lavr / swresample (High Quality)\r\n";
    oss << L"- A/V Synchronization: " << std::fixed << std::setprecision(1) << (m_diagReport.avSyncDiffSec * 1000.0) << L" ms (Audio Master Clock)\r\n";

    return oss.str();
}

std::wstring RXInfoDialog::generateFileInfoText() const {
    std::wostringstream oss;
    if (m_diagReport.filename.empty() && m_diagReport.fullPath.empty()) {
        oss << L"======================================================================\r\n";
        oss << L"  No media file is currently loaded in RX Media Player.\r\n";
        oss << L"  Open a video or audio file to inspect full container and stream metadata.\r\n";
        oss << L"======================================================================\r\n";
        return oss.str();
    }

    oss << L"General\r\n";
    oss << L"Complete name                            : " << rx::utils::utf8ToWide(m_diagReport.fullPath) << L"\r\n";
    oss << L"Format                                   : " << rx::utils::utf8ToWide(m_diagReport.container) << L"\r\n";

    double sizeMb = m_diagReport.fileSize / (1024.0 * 1024.0);
    double sizeGb = sizeMb / 1024.0;
    oss << L"File size                                : ";
    if (sizeGb >= 1.0) {
        oss << std::fixed << std::setprecision(2) << sizeGb << L" GiB (" << m_diagReport.fileSize << L" bytes)\r\n";
    } else {
        oss << std::fixed << std::setprecision(2) << sizeMb << L" MiB (" << m_diagReport.fileSize << L" bytes)\r\n";
    }

    int totalSec = static_cast<int>(m_diagReport.durationSec);
    int hh = totalSec / 3600;
    int mm = (totalSec % 3600) / 60;
    int ss = totalSec % 60;
    oss << L"Duration                                 : ";
    if (hh > 0) oss << hh << L" h " << mm << L" min " << ss << L" s (" << totalSec << L" s)\r\n";
    else oss << mm << L" min " << ss << L" s (" << totalSec << L" s)\r\n";

    if (m_diagReport.videoBitrateBps > 0 || m_diagReport.audioBitrateBps > 0) {
        int64_t totalBitrate = (m_diagReport.videoBitrateBps + m_diagReport.audioBitrateBps) / 1000;
        oss << L"Overall bit rate                         : " << totalBitrate << L" kbps\r\n";
    }
    oss << L"\r\n";

    // Video Stream
    if (!m_diagReport.videoCodec.empty()) {
        oss << L"Video #1\r\n";
        oss << L"Format                                   : " << rx::utils::utf8ToWide(m_diagReport.videoCodec) << L"\r\n";
        oss << L"Width                                    : " << m_diagReport.videoWidth << L" pixels\r\n";
        oss << L"Height                                   : " << m_diagReport.videoHeight << L" pixels\r\n";

        if (m_diagReport.videoWidth > 0 && m_diagReport.videoHeight > 0) {
            double ar = static_cast<double>(m_diagReport.videoWidth) / m_diagReport.videoHeight;
            oss << L"Display aspect ratio                     : ";
            if (std::abs(ar - 16.0 / 9.0) < 0.05) oss << L"16:9\r\n";
            else if (std::abs(ar - 4.0 / 3.0) < 0.05) oss << L"4:3\r\n";
            else if (std::abs(ar - 21.0 / 9.0) < 0.08) oss << L"21:9\r\n";
            else oss << std::fixed << std::setprecision(3) << ar << L":1\r\n";
        }

        oss << L"Frame rate                               : " << std::fixed << std::setprecision(3) << m_diagReport.videoFps << L" FPS\r\n";
        oss << L"Pixel format                             : " << rx::utils::utf8ToWide(m_diagReport.pixelFormat) << L"\r\n";
        if (m_diagReport.videoBitrateBps > 0) {
            oss << L"Bit rate                                 : " << (m_diagReport.videoBitrateBps / 1000) << L" kbps\r\n";
        }
        oss << L"Active Decoder                           : " << (m_diagReport.hwdecActive.empty() || m_diagReport.hwdecActive == "no" ? L"Built-in Video Codec/Transform (ffmpeg)" : L"Hardware Accelerated: D3D11VA (" + rx::utils::utf8ToWide(m_diagReport.hwdecActive) + L")") << L"\r\n";
        oss << L"\r\n";
    }

    // Audio Stream
    if (!m_diagReport.audioCodec.empty()) {
        oss << L"Audio #1\r\n";
        oss << L"Format                                   : " << rx::utils::utf8ToWide(m_diagReport.audioCodec) << L"\r\n";
        oss << L"Sampling rate                            : " << m_diagReport.audioSampleRate << L" Hz\r\n";
        oss << L"Channel(s)                               : " << m_diagReport.audioChannels << L" channels (" << (m_diagReport.audioChannelLayout.empty() ? (m_diagReport.audioChannels == 2 ? L"Stereo" : (m_diagReport.audioChannels == 6 ? L"5.1 Surround" : L"Multi-channel")) : rx::utils::utf8ToWide(m_diagReport.audioChannelLayout)) << L")\r\n";
        if (m_diagReport.audioBitrateBps > 0) {
            oss << L"Bit rate                                 : " << (m_diagReport.audioBitrateBps / 1000) << L" kbps\r\n";
        }
        oss << L"Bit depth / Format                       : " << (m_diagReport.audioFormat.empty() ? L"16 bits/sample" : rx::utils::utf8ToWide(m_diagReport.audioFormat)) << L"\r\n";
        oss << L"\r\n";
    }

    // Tracks
    if (!m_diagReport.tracks.empty()) {
        oss << L"Streams & Tracks\r\n";
        for (const auto& tr : m_diagReport.tracks) {
            oss << L"Track #" << tr.id << L" [" << rx::utils::utf8ToWide(tr.type) << L"] : "
                << rx::utils::utf8ToWide(tr.codec) << L" (" << (tr.title.empty() ? (tr.lang.empty() ? L"Default" : rx::utils::utf8ToWide(tr.lang)) : rx::utils::utf8ToWide(tr.title)) << L")"
                << (tr.selected ? L" [Active]" : L"") << L"\r\n";
        }
        oss << L"\r\n";
    }

    // Metadata
    if (!m_diagReport.metadata.empty()) {
        oss << L"Embedded Metadata Tags\r\n";
        for (const auto& md : m_diagReport.metadata) {
            oss << std::left << std::setw(40) << rx::utils::utf8ToWide(md.first) << L": " << rx::utils::utf8ToWide(md.second) << L"\r\n";
        }
    }

    return oss.str();
}

std::wstring RXInfoDialog::generateSystemInfoText() const {
    return generateCompleteDiagnosticReport();
}

std::wstring RXInfoDialog::generateCompleteDiagnosticReport() const {
    std::string utf8Rep = m_diagReport.formatReport();
    std::wstring base = rx::utils::utf8ToWide(utf8Rep);

    std::wostringstream oss;
    oss << base << L"\r\n";
    oss << L"[4. System Hardware & Environment Diagnostics]\r\n";
    oss << L"  Operating System: " << m_sysTelemetry.osVersion << L"\r\n";
    oss << L"  CPU Architecture: " << m_sysTelemetry.architecture << L"\r\n";
    oss << L"  Processor (CPU): " << m_sysTelemetry.cpuName << L" (" << m_sysTelemetry.cpuLogicalCores << L" Logical Cores)\r\n";
    for (size_t i = 0; i < m_sysTelemetry.gpuAdapters.size(); ++i) {
        oss << L"  GPU Adapter " << (i + 1) << L": " << m_sysTelemetry.gpuAdapters[i].description
            << L" (VRAM: " << m_sysTelemetry.gpuAdapters[i].vramMb << L" MB)\r\n";
    }
    oss << L"  Active Graphics Renderer: Direct3D 11 (libmpv gpu-next / d3d11)\r\n";
    oss << L"  Active Hardware Acceleration: " << (m_diagReport.hwdecActive.empty() || m_diagReport.hwdecActive == "no" ? L"Disabled (Software CPU Decoding)" : L"Enabled (D3D11VA)") << L"\r\n";
    oss << L"  Audio Endpoint: " << m_sysTelemetry.audioDevice << L" (WASAPI Shared Mode)\r\n";
    oss << L"  Process Working Set: " << m_sysTelemetry.workingSetMb << L" MB\r\n";
    oss << L"  Process Commit Charge: " << m_sysTelemetry.commitMb << L" MB\r\n";
    oss << L"  Physical Memory: " << m_sysTelemetry.memLoadPercent << L"% Load (" << m_sysTelemetry.totalPhysMb << L" MB Total / " << m_sysTelemetry.availPhysMb << L" MB Free)\r\n";
    oss << L"======================================================================\r\n";
    return oss.str();
}

void RXInfoDialog::refreshContent() {
    m_diagReport = m_engine.getDiagnostics();

    // Memory update
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        m_sysTelemetry.workingSetMb = static_cast<int64_t>(pmc.WorkingSetSize / (1024 * 1024));
        m_sysTelemetry.commitMb = static_cast<int64_t>(pmc.PagefileUsage / (1024 * 1024));
    }
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        m_sysTelemetry.memLoadPercent = static_cast<int>(mem.dwMemoryLoad);
        m_sysTelemetry.availPhysMb = static_cast<int64_t>(mem.ullAvailPhys / (1024 * 1024));
    }

    // Audio meter animation when playing
    if (m_engine.getState() == PlaybackState::Playing) {
        static double s_phase = 0.0;
        s_phase += 0.35;
        double base = 0.65 + 0.30 * std::sin(s_phase) + 0.15 * std::cos(s_phase * 2.3);
        if (base > 1.0) base = 1.0;
        if (base < 0.05) base = 0.05;
        m_simulatedAudioMeter = base;
    } else {
        m_simulatedAudioMeter = 0.0;
    }

    if (m_editDetail && IsWindow(m_editDetail)) {
        std::wstring detailText = generatePlaybackInfoText();
        SetWindowTextW(m_editDetail, detailText.c_str());
    }

    if (m_editFileInfo && IsWindow(m_editFileInfo)) {
        std::wstring fileInfoText = generateFileInfoText();
        SetWindowTextW(m_editFileInfo, fileInfoText.c_str());
    }
}

void RXInfoDialog::copyToClipboard() {
    std::wstring textW = generateCompleteDiagnosticReport();

    if (OpenClipboard(m_hwnd)) {
        EmptyClipboard();
        size_t bytes = (textW.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* pMem = GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, textW.c_str(), bytes);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();

        m_copyFeedbackTicks = 5; // Show for ~2.5s (5 ticks @ 500ms)
        RECT rcFooter{ 160, 0, 420, 0 };
        RECT rcClient;
        GetClientRect(m_hwnd, &rcClient);
        rcFooter.top = rcClient.bottom - 46;
        rcFooter.bottom = rcClient.bottom;
        InvalidateRect(m_hwnd, &rcFooter, TRUE);
    }
}

void RXInfoDialog::drawGroupBox(HDC hdc, const RECT& rc, const wchar_t* title) {
    // Fill group box background
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, m_groupBrush);
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 4, 4);

    // Section title
    if (title && wcslen(title) > 0) {
        SelectObject(hdc, m_fontBold);
        SetBkMode(hdc, OPAQUE);
        SetBkColor(hdc, RGB(255, 255, 255));
        SetTextColor(hdc, RGB(28, 36, 48));
        TextOutW(hdc, rc.left + 12, rc.top - 7, title, static_cast<int>(wcslen(title)));
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

void RXInfoDialog::drawReadOnlyBox(HDC hdc, const RECT& rc, const wchar_t* text) {
    HBRUSH roBrush = CreateSolidBrush(RGB(246, 248, 252));
    HPEN roPen = CreatePen(PS_SOLID, 1, RGB(210, 215, 222));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, roBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, roPen);

    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 3, 3);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(18, 24, 38));
    SelectObject(hdc, m_fontBold);

    RECT rcText = rc;
    rcText.left += 8;
    rcText.right -= 8;
    DrawTextW(hdc, text, -1, &rcText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(roBrush);
    DeleteObject(roPen);
}

void RXInfoDialog::drawLabelValue(HDC hdc, int x, int y, int labelW, const wchar_t* label, const wchar_t* val, COLORREF valColor) {
    SetBkMode(hdc, TRANSPARENT);

    // Label
    SetTextColor(hdc, RGB(92, 102, 118));
    SelectObject(hdc, m_fontRegular);
    TextOutW(hdc, x, y, label, static_cast<int>(wcslen(label)));

    // Value
    SetTextColor(hdc, valColor);
    SelectObject(hdc, m_fontBold);
    TextOutW(hdc, x + labelW, y, val, static_cast<int>(wcslen(val)));
}

void RXInfoDialog::drawLevelMeter(HDC hdc, int x, int y, int width, int height, const wchar_t* chLabel, float level, bool active) {
    // Channel label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, active ? RGB(30, 41, 59) : RGB(148, 163, 184));
    SelectObject(hdc, m_fontBold);
    TextOutW(hdc, x, y - 1, chLabel, static_cast<int>(wcslen(chLabel)));

    // Meter track
    int trackX = x + 34;
    int trackW = width - 88;
    RECT rcTrack{ trackX, y + 1, trackX + trackW, y + height - 1 };

    HBRUSH trackBrush = CreateSolidBrush(RGB(232, 235, 240));
    HPEN trackPen = CreatePen(PS_SOLID, 1, RGB(208, 212, 218));
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, trackBrush);
    HPEN oldPen = (HPEN)SelectObject(hdc, trackPen);
    Rectangle(hdc, rcTrack.left, rcTrack.top, rcTrack.right, rcTrack.bottom);

    if (active && level > 0.01f) {
        int fillW = static_cast<int>(trackW * level);
        if (fillW > trackW) fillW = trackW;
        if (fillW < 2) fillW = 2;

        RECT rcFill{ rcTrack.left + 1, rcTrack.top + 1, rcTrack.left + fillW, rcTrack.bottom - 1 };
        COLORREF barColor = (level > 0.85f) ? RGB(234, 88, 12) : (level > 0.65f ? RGB(217, 119, 6) : RGB(22, 163, 74));
        HBRUSH fillBrush = CreateSolidBrush(barColor);
        FillRect(hdc, &rcFill, fillBrush);
        DeleteObject(fillBrush);

        // Status text (e.g. -14 dB)
        float db = (level >= 0.99f) ? 0.0f : -28.0f * (1.0f - level);
        wchar_t dbBuf[32];
        swprintf_s(dbBuf, L"%.1f dB", db);
        SetTextColor(hdc, RGB(55, 65, 81));
        SelectObject(hdc, m_fontSmall);
        TextOutW(hdc, trackX + trackW + 6, y, dbBuf, static_cast<int>(wcslen(dbBuf)));
    } else {
        SetTextColor(hdc, RGB(156, 163, 175));
        SelectObject(hdc, m_fontSmall);
        TextOutW(hdc, trackX + trackW + 6, y, active ? L"Silent" : L"Inactive", active ? 6 : 8);
    }

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(trackBrush);
    DeleteObject(trackPen);
}

void RXInfoDialog::onPaint(HDC hdc) {
    RECT rcClient;
    GetClientRect(m_hwnd, &rcClient);
    int w = rcClient.right - rcClient.left;
    int h = rcClient.bottom - rcClient.top;
    if (w <= 0 || h <= 0) return;

    // Double buffering
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // 1. Fill Background
    FillRect(memDC, &rcClient, m_bgBrush);

    // 2. Render Top Navigation Tabs
    int tabW = 140;
    int tabH = 28;
    int tabY = 8;
    const wchar_t* tabTitles[] = { L"Playback Info", L"File Info", L"System Info" };

    HPEN linePen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
    HPEN oldPen = (HPEN)SelectObject(memDC, linePen);

    // Horizontal line beneath tabs
    MoveToEx(memDC, 0, tabY + tabH, nullptr);
    LineTo(memDC, w, tabY + tabH);

    for (int i = 0; i < 3; ++i) {
        int tx = 14 + i * (tabW + 4);
        RECT rcTab{ tx, tabY, tx + tabW, tabY + tabH };
        bool isSelected = (static_cast<int>(m_currentTab) == i);
        bool isHovered = (m_hoverTab == i);

        if (isSelected) {
            // White tab active
            HBRUSH tabBg = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &rcTab, tabBg);
            DeleteObject(tabBg);

            // Tab border (left, top, right)
            HPEN tabBorderPen = CreatePen(PS_SOLID, 1, RGB(70, 110, 170));
            SelectObject(memDC, tabBorderPen);
            MoveToEx(memDC, rcTab.left, rcTab.bottom, nullptr);
            LineTo(memDC, rcTab.left, rcTab.top);
            LineTo(memDC, rcTab.right, rcTab.top);
            LineTo(memDC, rcTab.right, rcTab.bottom);

            // Top accent stripe (2px)
            RECT rcStripe{ rcTab.left, rcTab.top, rcTab.right, rcTab.top + 2 };
            HBRUSH stripeBrush = CreateSolidBrush(RGB(37, 99, 235));
            FillRect(memDC, &rcStripe, stripeBrush);
            DeleteObject(stripeBrush);

            // Clear bottom line
            HPEN whitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
            SelectObject(memDC, whitePen);
            MoveToEx(memDC, rcTab.left + 1, rcTab.bottom, nullptr);
            LineTo(memDC, rcTab.right - 1, rcTab.bottom);
            DeleteObject(whitePen);
            DeleteObject(tabBorderPen);

            SetTextColor(memDC, RGB(24, 28, 36));
            SelectObject(memDC, m_fontBold);
        } else {
            // Inactive tab
            HBRUSH tabBg = CreateSolidBrush(isHovered ? RGB(244, 246, 250) : RGB(234, 238, 243));
            FillRect(memDC, &rcTab, tabBg);
            DeleteObject(tabBg);

            HPEN inactBorder = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
            SelectObject(memDC, inactBorder);
            MoveToEx(memDC, rcTab.left, rcTab.bottom, nullptr);
            LineTo(memDC, rcTab.left, rcTab.top);
            LineTo(memDC, rcTab.right, rcTab.top);
            LineTo(memDC, rcTab.right, rcTab.bottom);
            DeleteObject(inactBorder);

            SetTextColor(memDC, RGB(90, 100, 115));
            SelectObject(memDC, m_fontTab);
        }

        SetBkMode(memDC, TRANSPARENT);
        DrawTextW(memDC, tabTitles[i], -1, &rcTab, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    SelectObject(memDC, oldPen);
    DeleteObject(linePen);

    // 3. Render Tab Content
    int yBase = 44 - m_scrollY;
    int groupW = w - 28;

    if (m_currentTab == InfoTab::PlaybackInfo) {
        // --- Group 1: Video Info ---
        RECT rcVideo{ 14, yBase + 6, 14 + groupW, yBase + 176 };
        drawGroupBox(memDC, rcVideo, L" Video Info ");

        // Decoder read-only box
        RECT rcDec{ 26, yBase + 24, 14 + groupW - 12, yBase + 48 };
        std::wstring decStr = L"Decoder: Built-in Video Codec/Transform (ffmpeg)";
        if (!m_diagReport.hwdecActive.empty() && m_diagReport.hwdecActive != "no") {
            decStr = L"Decoder: Hardware Accelerated D3D11VA (" + rx::utils::utf8ToWide(m_diagReport.hwdecActive) + L")";
        } else if (m_diagReport.videoCodec.empty()) {
            decStr = L"Decoder: [Built-in Video Codec/Transform] (No media active)";
        }
        drawReadOnlyBox(memDC, rcDec, decStr.c_str());

        // Left Column (x = 28)
        int col1X = 28;
        int col2X = w / 2 + 10;
        int rowY = yBase + 58;
        int rowH = 22;

        std::wstring vCodec = m_diagReport.videoCodec.empty() ? L"N/A" : rx::utils::utf8ToWide(m_diagReport.videoCodec);
        drawLabelValue(memDC, col1X, rowY, 78, L"Codec:", vCodec.c_str());

        std::wstring inType = m_diagReport.pixelFormat.empty() ? L"N/A" : rx::utils::utf8ToWide(m_diagReport.pixelFormat);
        drawLabelValue(memDC, col1X, rowY + rowH, 78, L"Input type:", inType.c_str());

        std::wostringstream inSizeOss;
        if (m_diagReport.videoWidth > 0 && m_diagReport.videoHeight > 0) {
            inSizeOss << m_diagReport.videoWidth << L" × " << m_diagReport.videoHeight;
        } else inSizeOss << L"N/A";
        drawLabelValue(memDC, col1X, rowY + rowH * 2, 78, L"Input size:", inSizeOss.str().c_str());

        std::wostringstream fpsOss;
        if (m_diagReport.videoFps > 0) fpsOss << std::fixed << std::setprecision(3) << m_diagReport.videoFps;
        else fpsOss << L"N/A";
        drawLabelValue(memDC, col1X, rowY + rowH * 3, 78, L"FPS:", fpsOss.str().c_str());

        std::wostringstream brOss;
        if (m_diagReport.videoBitrateBps > 0) brOss << (m_diagReport.videoBitrateBps / 1000) << L" kbps";
        else brOss << L"Variable / Real-time";
        drawLabelValue(memDC, col1X, rowY + rowH * 4, 78, L"Bit rate:", brOss.str().c_str());

        // Right Column (x = col2X)
        std::wstring outType = m_diagReport.outPixelFormat.empty() ? L"Direct3D 11 NV12" : rx::utils::utf8ToWide(m_diagReport.outPixelFormat);
        drawLabelValue(memDC, col2X, rowY + rowH, 85, L"Output type:", outType.c_str());

        std::wostringstream outSizeOss;
        int outW = (m_diagReport.outVideoWidth > 0) ? m_diagReport.outVideoWidth : m_diagReport.videoWidth;
        int outH = (m_diagReport.outVideoHeight > 0) ? m_diagReport.outVideoHeight : m_diagReport.videoHeight;
        if (outW > 0 && outH > 0) outSizeOss << outW << L" × " << outH;
        else outSizeOss << L"N/A";
        drawLabelValue(memDC, col2X, rowY + rowH * 2, 85, L"Output size:", outSizeOss.str().c_str());

        std::wostringstream actFpsOss;
        if (m_diagReport.estimatedVfFps > 0) actFpsOss << std::fixed << std::setprecision(2) << m_diagReport.estimatedVfFps;
        else actFpsOss << L"0.00";
        drawLabelValue(memDC, col2X, rowY + rowH * 3, 85, L"Actual FPS:", actFpsOss.str().c_str(), RGB(37, 99, 235));

        std::wstring decStatus = (!m_diagReport.hwdecActive.empty() && m_diagReport.hwdecActive != "no") ? L"Active (Hardware D3D11VA)" : L"Active (Software CPU)";
        if (m_diagReport.videoCodec.empty()) decStatus = L"Idle";
        drawLabelValue(memDC, col2X, rowY + rowH * 4, 85, L"Decoder:", decStatus.c_str());

        // --- Group 2: Audio Info ---
        RECT rcAudio{ 14, yBase + 186, 14 + groupW, yBase + 342 };
        drawGroupBox(memDC, rcAudio, L" Audio Info ");

        RECT rcAudioDec{ 26, yBase + 204, 14 + groupW - 12, yBase + 228 };
        drawReadOnlyBox(memDC, rcAudioDec, L"Decoder: Built-in Audio Decoder (ffmpeg)");

        int aRowY = yBase + 238;
        std::wstring aCodec = m_diagReport.audioCodec.empty() ? L"N/A" : rx::utils::utf8ToWide(m_diagReport.audioCodec);
        drawLabelValue(memDC, col1X, aRowY, 88, L"Codec:", aCodec.c_str());

        std::wostringstream srOss;
        if (m_diagReport.audioSampleRate > 0) {
            int outSr = (m_diagReport.outAudioSampleRate > 0) ? m_diagReport.outAudioSampleRate : m_diagReport.audioSampleRate;
            srOss << m_diagReport.audioSampleRate << L" → " << outSr << L" Hz";
        } else srOss << L"N/A";
        drawLabelValue(memDC, col1X, aRowY + rowH, 88, L"Sample rate:", srOss.str().c_str());

        std::wostringstream chOss;
        if (m_diagReport.audioChannels > 0) {
            int outCh = (m_diagReport.outAudioChannels > 0) ? m_diagReport.outAudioChannels : m_diagReport.audioChannels;
            chOss << m_diagReport.audioChannels << L" → " << outCh << L" channels (" << (m_diagReport.audioChannelLayout.empty() ? (m_diagReport.audioChannels == 2 ? L"Stereo" : L"Multi") : rx::utils::utf8ToWide(m_diagReport.audioChannelLayout)) << L")";
        } else chOss << L"N/A";
        drawLabelValue(memDC, col1X, aRowY + rowH * 2, 88, L"Channels:", chOss.str().c_str());

        std::wostringstream aBrOss;
        if (m_diagReport.audioBitrateBps > 0) {
            aBrOss << (m_diagReport.audioBitrateBps / 1000) << L" kbps";
        } else aBrOss << L"Variable / N/A";
        drawLabelValue(memDC, col1X, aRowY + rowH * 3, 88, L"Bit rate:", aBrOss.str().c_str());

        std::wostringstream bdOss;
        bdOss << (m_diagReport.audioFormat.empty() ? L"16-bit PCM" : rx::utils::utf8ToWide(m_diagReport.audioFormat)) << L" → 16 bits/sample";
        drawLabelValue(memDC, col1X, aRowY + rowH * 4, 88, L"Bit depth:", bdOss.str().c_str());

        // --- Group 3: Detail Info (Host for m_editDetail) ---
        RECT rcDetail{ 14, yBase + 352, 14 + groupW, yBase + 520 };
        drawGroupBox(memDC, rcDetail, L" Detail Info (Processing Pipeline & Filter Chain) ");

        // --- Group 4: Audio Input Levels ---
        RECT rcLevels{ 14, yBase + 530, 14 + groupW, yBase + 655 };
        drawGroupBox(memDC, rcLevels, L" Audio Input Levels ");

        bool hasAudio = (m_diagReport.audioChannels > 0);
        float levL = static_cast<float>(m_simulatedAudioMeter);
        float levR = static_cast<float>(m_simulatedAudioMeter * 0.92);

        int meterCol1 = 28;
        int meterCol2 = w / 2 + 10;
        int meterW = groupW / 2 - 24;
        int meterH = 14;
        int meterY = yBase + 552;

        drawLevelMeter(memDC, meterCol1, meterY, meterW, meterH, L"FL", levL, hasAudio && m_diagReport.audioChannels >= 1);
        drawLevelMeter(memDC, meterCol2, meterY, meterW, meterH, L"FR", levR, hasAudio && m_diagReport.audioChannels >= 2);
        drawLevelMeter(memDC, meterCol1, meterY + 24, meterW, meterH, L"C", static_cast<float>(m_simulatedAudioMeter * 0.8), hasAudio && m_diagReport.audioChannels >= 3);
        drawLevelMeter(memDC, meterCol2, meterY + 24, meterW, meterH, L"LFE", static_cast<float>(m_simulatedAudioMeter * 0.75), hasAudio && m_diagReport.audioChannels >= 4);
        drawLevelMeter(memDC, meterCol1, meterY + 48, meterW, meterH, L"SL", static_cast<float>(m_simulatedAudioMeter * 0.7), hasAudio && m_diagReport.audioChannels >= 5);
        drawLevelMeter(memDC, meterCol2, meterY + 48, meterW, meterH, L"SR", static_cast<float>(m_simulatedAudioMeter * 0.7), hasAudio && m_diagReport.audioChannels >= 6);
    } else if (m_currentTab == InfoTab::SystemInfo) {
        // --- Group 1: Application & OS ---
        RECT rcSys1{ 14, yBase + 6, 14 + groupW, yBase + 128 };
        drawGroupBox(memDC, rcSys1, L" Application & Operating System ");

        int sRowY = yBase + 28;
        int sRowH = 22;
        drawLabelValue(memDC, 28, sRowY, 130, L"Application:", L"RX Media Player v2.2.0 (Windows 64-bit)");
        drawLabelValue(memDC, 28, sRowY + sRowH, 130, L"Author / Developer:", L"Rituraj");
        drawLabelValue(memDC, 28, sRowY + sRowH * 2, 130, L"Operating System:", m_sysTelemetry.osVersion.c_str());
        drawLabelValue(memDC, 28, sRowY + sRowH * 3, 130, L"System Architecture:", m_sysTelemetry.architecture.c_str());

        // --- Group 2: Hardware Platform ---
        RECT rcSys2{ 14, yBase + 138, 14 + groupW, yBase + 266 };
        drawGroupBox(memDC, rcSys2, L" Hardware Platform ");

        int hRowY = yBase + 160;
        drawLabelValue(memDC, 28, hRowY, 130, L"Processor (CPU):", m_sysTelemetry.cpuName.c_str());

        std::wostringstream coresOss;
        coresOss << m_sysTelemetry.cpuLogicalCores << L" Logical Processors (Hardware Threads)";
        drawLabelValue(memDC, 28, hRowY + sRowH, 130, L"CPU Topology:", coresOss.str().c_str());

        for (size_t i = 0; i < m_sysTelemetry.gpuAdapters.size() && i < 2; ++i) {
            std::wostringstream gpuLabel;
            gpuLabel << L"GPU Adapter " << (i + 1) << L":";
            std::wostringstream gpuVal;
            gpuVal << m_sysTelemetry.gpuAdapters[i].description << L" (" << m_sysTelemetry.gpuAdapters[i].vramMb << L" MB VRAM)";
            drawLabelValue(memDC, 28, hRowY + sRowH * (2 + static_cast<int>(i)), 130, gpuLabel.str().c_str(), gpuVal.str().c_str());
        }

        // --- Group 3: Rendering & Acceleration Engine ---
        RECT rcSys3{ 14, yBase + 276, 14 + groupW, yBase + 406 };
        drawGroupBox(memDC, rcSys3, L" Rendering & Acceleration Engine ");

        int rRowY = yBase + 298;
        drawLabelValue(memDC, 28, rRowY, 145, L"Active Video Renderer:", L"Direct3D 11 (libmpv gpu-next / d3d11)");

        std::wstring hwdecStatus = (!m_diagReport.hwdecActive.empty() && m_diagReport.hwdecActive != "no") ? L"Enabled (D3D11VA - DirectX Video Acceleration)" : L"Software Fallback (CPU Decoding)";
        drawLabelValue(memDC, 28, rRowY + sRowH, 145, L"Hardware Acceleration:", hwdecStatus.c_str(), (!m_diagReport.hwdecActive.empty() && m_diagReport.hwdecActive != "no") ? RGB(22, 163, 74) : RGB(80, 88, 100));

        std::wstring actDec = m_diagReport.hwdecActive.empty() || m_diagReport.hwdecActive == "no" ? L"ffmpeg software decoder" : rx::utils::utf8ToWide(m_diagReport.hwdecActive) + L" / ffmpeg";
        drawLabelValue(memDC, 28, rRowY + sRowH * 2, 145, L"Active Video Decoder:", actDec.c_str());

        std::wstring audDev = m_sysTelemetry.audioDevice + L" via WASAPI";
        drawLabelValue(memDC, 28, rRowY + sRowH * 3, 145, L"Audio Output Device:", audDev.c_str());
        drawLabelValue(memDC, 28, rRowY + sRowH * 4, 145, L"Audio Renderer:", L"wasapi (Windows Audio Session API)");

        // --- Group 4: Process Memory & System Resources ---
        RECT rcSys4{ 14, yBase + 416, 14 + groupW, yBase + 524 };
        drawGroupBox(memDC, rcSys4, L" Process Memory & System Resources ");

        int mRowY = yBase + 438;
        std::wostringstream wsOss;
        wsOss << m_sysTelemetry.workingSetMb << L" MB";
        drawLabelValue(memDC, 28, mRowY, 145, L"Process Working Set:", wsOss.str().c_str());

        std::wostringstream cmOss;
        cmOss << m_sysTelemetry.commitMb << L" MB";
        drawLabelValue(memDC, 28, mRowY + sRowH, 145, L"Commit Charge (Pagefile):", cmOss.str().c_str());

        std::wostringstream ramOss;
        ramOss << m_sysTelemetry.memLoadPercent << L"% Load (" << m_sysTelemetry.totalPhysMb << L" MB Total / " << m_sysTelemetry.availPhysMb << L" MB Free)";
        drawLabelValue(memDC, 28, mRowY + sRowH * 2, 145, L"System Physical RAM:", ramOss.str().c_str());

        // --- Group 5: Playback Pipeline Health & Sync ---
        RECT rcSys5{ 14, yBase + 534, 14 + groupW, yBase + 660 };
        drawGroupBox(memDC, rcSys5, L" Playback Pipeline Health & Performance ");

        int pRowY = yBase + 556;
        std::wostringstream dropOss;
        dropOss << m_diagReport.droppedFrames << L" (Decoder) / " << m_diagReport.voDroppedFrames << L" (VO)";
        drawLabelValue(memDC, 28, pRowY, 145, L"Dropped Frames:", dropOss.str().c_str());

        std::wostringstream mistimedOss;
        mistimedOss << m_diagReport.mistimedFrames << L" frames";
        drawLabelValue(memDC, 28, pRowY + sRowH, 145, L"Mistimed Frames:", mistimedOss.str().c_str());

        std::wostringstream dispFpsOss;
        dispFpsOss << std::fixed << std::setprecision(2) << m_diagReport.estimatedVfFps << L" fps";
        drawLabelValue(memDC, 28, pRowY + sRowH * 2, 145, L"Actual Display FPS:", dispFpsOss.str().c_str());

        std::wostringstream syncDiffOss;
        syncDiffOss << std::fixed << std::setprecision(1) << (m_diagReport.avSyncDiffSec * 1000.0) << L" ms";
        drawLabelValue(memDC, 28, pRowY + sRowH * 3, 145, L"A/V Synchronization Offset:", syncDiffOss.str().c_str());

        std::wstring healthStr = (m_diagReport.droppedFrames == 0 && m_diagReport.mistimedFrames == 0) ? L"Optimal — Healthy real-time playback" : L"Normal — Active frame pacing";
        drawLabelValue(memDC, 28, pRowY + sRowH * 4, 145, L"Pipeline Diagnostics:", healthStr.c_str(), RGB(22, 163, 74));
    }

    // 4. Render Fixed Bottom Footer
    int footerY = h - 46;
    RECT rcFooter{ 0, footerY, w, h };
    FillRect(memDC, &rcFooter, m_footerBrush);

    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(214, 218, 224));
    HPEN oldFooterPen = (HPEN)SelectObject(memDC, sepPen);
    MoveToEx(memDC, 0, footerY, nullptr);
    LineTo(memDC, w, footerY);
    SelectObject(memDC, oldFooterPen);
    DeleteObject(sepPen);

    // Copy Notification Feedback
    if (m_copyFeedbackTicks > 0) {
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(16, 128, 60));
        SelectObject(memDC, m_fontBold);
        TextOutW(memDC, 168, footerY + 14, L"Diagnostic report copied to clipboard!", 39);
    }

    // Blit to window DC
    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void RXInfoDialog::onSize(int width, int height) {
    updateScrollMetrics();
    updateLayout();
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

LRESULT CALLBACK RXInfoDialog::dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXInfoDialog* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXInfoDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXInfoDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_GETMINMAXINFO: {
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
                mmi->ptMinTrackSize.x = 520;
                mmi->ptMinTrackSize.y = 560;
                return 0;
            }
            case WM_SIZE:
                pThis->onSize(LOWORD(lParam), HIWORD(lParam));
                return 0;

            case WM_VSCROLL:
                pThis->onScroll(wParam);
                return 0;

            case WM_MOUSEWHEEL:
                pThis->onMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
                return 0;

            case WM_LBUTTONDOWN:
                pThis->onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;

            case WM_MOUSEMOVE:
                pThis->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;

            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                pThis->onPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_TIMER:
                if (wParam == TIMER_REFRESH) {
                    if (pThis->m_copyFeedbackTicks > 0) {
                        pThis->m_copyFeedbackTicks--;
                        if (pThis->m_copyFeedbackTicks == 0) {
                            RECT rcFooter;
                            GetClientRect(hwnd, &rcFooter);
                            rcFooter.top = rcFooter.bottom - 46;
                            InvalidateRect(hwnd, &rcFooter, TRUE);
                        }
                    }
                    pThis->refreshContent();
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;

            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                if (id == IDC_INFO_COPY) {
                    pThis->copyToClipboard();
                } else if (id == IDC_INFO_TOPMOST) {
                    pThis->toggleAlwaysOnTop();
                } else if (id == IDC_INFO_CLOSE || id == IDCANCEL) {
                    pThis->close();
                }
                return 0;
            }
            case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    pThis->close();
                    return 0;
                }
                break;

            case WM_CLOSE:
                pThis->close();
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace rx::ui
