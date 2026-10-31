#include "main_window.hpp"
#include "../core/logger.hpp"
#include "../core/settings.hpp"
#include "../core/utils.hpp"
#include "../library/media_database.hpp"
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>

namespace rx::ui {

static const wchar_t* kMainClassName   = L"RXMediaPlayerMainClass";
static const wchar_t* kVideoClassName  = L"RXVideoViewportClass";
static const wchar_t* kControlBarClass = L"RXControlBarClass";

constexpr UINT_PTR TIMER_AUTOHIDE = 2001;
constexpr UINT_PTR TIMER_OSD      = 2002;

constexpr UINT IDC_MAIN_PLAY       = 3001;
constexpr UINT IDC_MAIN_STOP       = 3002;
constexpr UINT IDC_MAIN_PREV       = 3003;
constexpr UINT IDC_MAIN_NEXT       = 3004;
constexpr UINT IDC_MAIN_MUTE       = 3005;
constexpr UINT IDC_MAIN_SUBTITLES  = 3006;
constexpr UINT IDC_MAIN_HWDEC      = 3007;
constexpr UINT IDC_MAIN_PLAYLIST   = 3008;
constexpr UINT IDC_MAIN_FULLSCREEN = 3009;
constexpr UINT IDC_MAIN_SEEKBAR    = 3010;
constexpr UINT IDC_MAIN_VIDEO_ADJUST = 3011;
constexpr UINT IDC_MAIN_INFO       = 3012;
constexpr UINT IDC_MAIN_AUDIO_PANEL = 3013;

RXMainWindow::RXMainWindow() = default;

RXMainWindow::~RXMainWindow() {
    if (m_uiFont) DeleteObject(m_uiFont);
    if (m_osdFont) DeleteObject(m_osdFont);
    m_engine.shutdown();
}

bool RXMainWindow::create(HINSTANCE hInstance, int nCmdShow) {
    auto& settings = Settings::instance();
    settings.load();

    // Register Window Classes
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = RXMainWindow::mainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(theme::kBgColor);
    wc.lpszClassName = kMainClassName;
    RegisterClassExW(&wc);

    WNDCLASSEXW vc{};
    vc.cbSize = sizeof(WNDCLASSEXW);
    vc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    vc.lpfnWndProc = RXMainWindow::videoWndProc;
    vc.hInstance = hInstance;
    vc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    vc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
    vc.lpszClassName = kVideoClassName;
    RegisterClassExW(&vc);

    WNDCLASSEXW cbc{};
    cbc.cbSize = sizeof(WNDCLASSEXW);
    cbc.style = CS_HREDRAW | CS_VREDRAW;
    cbc.lpfnWndProc = RXMainWindow::controlBarWndProc;
    cbc.hInstance = hInstance;
    cbc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cbc.hbrBackground = CreateSolidBrush(theme::kPanelBg);
    cbc.lpszClassName = kControlBarClass;
    RegisterClassExW(&cbc);

    m_uiFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_osdFont = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Create Main Window
    int wx = settings.window.x;
    int wy = settings.window.y;
    int ww = settings.window.width;
    int wh = settings.window.height;

    m_mainHwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        kMainClassName, L"RX Media Player",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        wx, wy, ww, wh,
        nullptr, nullptr, hInstance, this
    );

    if (!m_mainHwnd) {
        RX_LOG_ERROR("Failed to create main window");
        return false;
    }

    theme::enableImmersiveDarkMode(m_mainHwnd);

    // Create Video Viewport Child Window
    m_videoHwnd = CreateWindowExW(
        0, kVideoClassName, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, ww, wh,
        m_mainHwnd, nullptr, hInstance, this
    );

    // Create Control Bar
    m_controlBarHwnd = CreateWindowExW(
        0, kControlBarClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, wh - 68, ww, 68,
        m_mainHwnd, nullptr, hInstance, this
    );

    // Create Playlist Panel
    m_playlistPanel = std::make_unique<RXPlaylistPanel>(m_playlistMgr);
    m_playlistPanel->create(m_mainHwnd, ww - 280, 0, 280, wh - 68);
    m_playlistPanel->setVisible(settings.showPlaylistPanel);

    // Create Video Adjustments Panel
    m_videoPanel = std::make_unique<RXVideoPanel>();
    m_videoPanel->create(m_mainHwnd, 0, 0, 300, wh - 68);
    m_videoPanel->setParams(settings.video.adjustments);
    m_videoPanel->setCallback([this](const VideoAdjustmentParams& params) {
        m_engine.applyVideoAdjustments(params);
        Settings::instance().video.adjustments = params;
    });

    // Create Diagnostics Dialog
    m_infoDialog = std::make_unique<RXInfoDialog>(m_engine);

    // Create Subtitle Dialog
    m_subDialog = std::make_unique<RXSubtitleDialog>(m_engine);
    m_subDialog->setStyle(settings.subtitle.style);
    m_subDialog->setCallback([](const SubtitleStyle& st) {
        Settings::instance().subtitle.style = st;
    });

    // Create Open URL Dialog
    m_urlDialog = std::make_unique<RXOpenUrlDialog>();
    m_urlDialog->setCallback([this](const std::string& url, int cacheSec, int maxBufferMb) {
        m_playlistMgr.addItem(url);
        m_playlistPanel->refresh();
        m_engine.openUrl(url, cacheSec, maxBufferMb);
        showOsdMessage(L"Opening Stream: " + rx::utils::utf8ToWide(url));
    });

    // Initialize SQLite Media Database & Bookmarks Dialog
    library::MediaDatabase::instance().initialize();
    m_bmDialog = std::make_unique<RXBookmarkDialog>(m_engine);

    // Create Audio Panel
    m_audioPanel = std::make_unique<RXAudioPanel>(m_engine);
    m_audioPanel->create(m_mainHwnd, ww - 340, 0, 340, wh - 68);
    m_audioPanel->setEqualizerState(settings.audio.equalizer);
    m_audioPanel->setCallback([this](const audio::EqualizerState& eq, const std::string& dev, double delayMs, bool excl) {
        m_engine.setAudioDevice(dev);
        m_engine.setAudioExclusive(excl);
        m_engine.setAudioDelay(delayMs / 1000.0);
        m_engine.setAudioFilter(audio::Equalizer::buildFilterString(eq));
        auto& s = Settings::instance();
        s.audio.deviceId = dev;
        s.audio.exclusiveMode = excl;
        s.audio.delayMs = delayMs;
        s.audio.equalizer = eq;
    });

    // Setup child controls
    setupChildControls();

    // Enable Drag and Drop
    DragAcceptFiles(m_mainHwnd, TRUE);
    DragAcceptFiles(m_videoHwnd, TRUE);

    // Initialize libmpv with the video viewport HWND
    if (!m_engine.initialize(m_videoHwnd)) {
        RX_LOG_ERROR("Engine initialization failed");
    }

    // Apply initial settings
    m_engine.applyVideoAdjustments(settings.video.adjustments);
    m_engine.applySubtitleStyle(settings.subtitle.style);

    // Set initial audio settings
    m_engine.setVolume(settings.audio.volume);
    m_engine.setMute(settings.audio.muted);
    m_engine.setAudioDevice(settings.audio.deviceId);
    m_engine.setAudioExclusive(settings.audio.exclusiveMode);
    m_engine.setAudioDelay(settings.audio.delayMs / 1000.0);
    m_engine.setAudioFilter(audio::Equalizer::buildFilterString(settings.audio.equalizer));

    // Setup engine callbacks
    m_engine.setProgressCallback([this](double posSec, double durSec) {
        m_seekBar.setPosition(posSec, durSec);
        updateTimeDisplay(posSec, durSec);

        static double s_lastRecordedPos = 0.0;
        if (std::abs(posSec - s_lastRecordedPos) >= 5.0 && posSec > 1.0) {
            s_lastRecordedPos = posSec;
            const auto& info = m_engine.getMediaInfo();
            if (!info.fullPath.empty()) {
                library::MediaDatabase::instance().recordPlaybackProgress(info.fullPath, info.filename, posSec, durSec);
            }
        }
    });

    m_engine.setStateCallback([this](PlaybackState state) {
        if (state == PlaybackState::Playing) {
            m_btnPlay.setText(L"❚❚");
            m_btnPlay.setTooltip(L"Pause (Space)");
        } else {
            m_btnPlay.setText(L"►");
            m_btnPlay.setTooltip(L"Play (Space)");
        }
    });

    m_engine.setMediaLoadedCallback([this](const MediaInfo& info) {
        std::wstring titleW = L"RX Media Player — " + rx::utils::utf8ToWide(info.filename);
        SetWindowTextW(m_mainHwnd, titleW.c_str());

        std::string hwdec = info.hwdecActive;
        std::wstring hwdecW = (hwdec != "no" && !hwdec.empty()) ? (L"HW: " + rx::utils::utf8ToWide(hwdec)) : L"SW";
        m_btnHwdec.setText(hwdecW);
        m_btnHwdec.setAccent(hwdec != "no" && !hwdec.empty());

        // Auto-load matching subtitles if configured
        if (Settings::instance().subtitle.autoLoad && !info.fullPath.empty()) {
            auto subs = rx::utils::findMatchingSubtitles(info.fullPath);
            if (!subs.empty()) {
                m_engine.loadSubtitle(subs[0].string());
                showOsdMessage(L"Loaded Subtitle: " + subs[0].filename().wstring());
            }
        }

        if (m_audioPanel) {
            m_audioPanel->refreshTracks();
        }
        if (m_subDialog) {
            m_subDialog->refreshTracks();
        }

        // Playback Position Resume from SQLite Database
        if (Settings::instance().playback.rememberPosition && !info.fullPath.empty()) {
            double lastPos = library::MediaDatabase::instance().getLastPosition(info.fullPath);
            if (lastPos > 5.0 && (info.durationSec <= 0.0 || lastPos < (info.durationSec - 10.0))) {
                m_engine.seek(lastPos, true);
                showOsdMessage(L"Resumed at: " + rx::utils::utf8ToWide(rx::utils::formatTime(lastPos)));
            }
        }

        if (m_bmDialog) {
            m_bmDialog->refreshList();
        }
    });

    m_engine.setEndFileCallback([this](int /*reason*/) {
        // Auto-play next item in playlist if available
        if (Settings::instance().playback.autoPlayNext) {
            auto nextItem = m_playlistMgr.next(false);
            if (nextItem) {
                m_engine.loadFile(nextItem->path, true);
                m_playlistPanel->refresh();
            }
        }
    });

    m_engine.setErrorCallback([this](const std::string& err) {
        showOsdMessage(rx::utils::utf8ToWide(err), 4000);
    });

    // Setup playlist callbacks
    m_playlistPanel->setPlayItemCallback([this](int index) {
        playItemAtIndex(index);
    });

    m_playlistPanel->setAddFilesCallback([this]() {
        openFileDialog();
    });

    m_playlistPanel->setAddFolderCallback([this]() {
        openFolderDialog();
    });

    // Layout
    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);

    if (settings.window.maximized) {
        ShowWindow(m_mainHwnd, SW_MAXIMIZE);
    } else {
        ShowWindow(m_mainHwnd, nCmdShow);
    }

    startAutoHideTimer();
    Settings::instance().save();
    return true;
}

void RXMainWindow::openFile(const std::string& path) {
    m_playlistMgr.addItem(path);
    if (m_playlistPanel) m_playlistPanel->refresh();
    m_engine.loadFile(path, true);
}

void RXMainWindow::setupChildControls() {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Seek bar (spans top of control bar)
    m_seekBar.create(m_controlBarHwnd, 0, 0, 800, 14, IDC_MAIN_SEEKBAR);
    m_seekBar.setSeekCallback([this](double targetSec) {
        m_engine.seek(targetSec, true);
    });

    // Controls on the bar (Y = 18, height = 36)
    m_btnPlay.create(m_controlBarHwnd, 10, 20, 38, 36, L"►", IDC_MAIN_PLAY, L"Play / Pause (Space)");
    m_btnPlay.setCallback([this]() {
        m_engine.togglePause();
    });

    m_btnStop.create(m_controlBarHwnd, 52, 20, 36, 36, L"■", IDC_MAIN_STOP, L"Stop");
    m_btnStop.setCallback([this]() {
        m_engine.stop();
        m_btnPlay.setText(L"►");
    });

    m_btnPrev.create(m_controlBarHwnd, 92, 20, 36, 36, L"⏮", IDC_MAIN_PREV, L"Previous Track");
    m_btnPrev.setCallback([this]() {
        auto prevItem = m_playlistMgr.previous();
        if (prevItem) {
            m_engine.loadFile(prevItem->path, true);
            m_playlistPanel->refresh();
        }
    });

    m_btnNext.create(m_controlBarHwnd, 132, 20, 36, 36, L"⏭", IDC_MAIN_NEXT, L"Next Track");
    m_btnNext.setCallback([this]() {
        auto nextItem = m_playlistMgr.next(true);
        if (nextItem) {
            m_engine.loadFile(nextItem->path, true);
            m_playlistPanel->refresh();
        }
    });

    m_btnMute.create(m_controlBarHwnd, 172, 20, 36, 36, L"🔊", IDC_MAIN_MUTE, L"Mute / Unmute (M)");
    m_btnMute.setCallback([this]() {
        toggleMute();
    });

    // Time Label
    m_lblTime = CreateWindowExW(
        0, L"STATIC", L"00:00 / 00:00",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        215, 20, 140, 36,
        m_controlBarHwnd, nullptr, hInst, nullptr
    );
    SendMessageW(m_lblTime, WM_SETFONT, (WPARAM)m_uiFont, TRUE);

    // Right-side controls
    m_btnHwdec.create(m_controlBarHwnd, 400, 20, 90, 36, L"HW: D3D11", IDC_MAIN_HWDEC, L"Hardware Acceleration Status");
    m_btnHwdec.setAccent(true);
    m_btnHwdec.setCallback([this]() {
        bool enabled = (m_engine.getActiveHwdec() == "no");
        m_engine.setHardwareAcceleration(enabled, "auto-safe");
        showOsdMessage(enabled ? L"Hardware Acceleration: Enabled" : L"Hardware Acceleration: Disabled");
    });

    m_btnSubtitles.create(m_controlBarHwnd, 495, 20, 48, 36, L"CC", IDC_MAIN_SUBTITLES, L"Load Subtitle File");
    m_btnSubtitles.setCallback([this]() {
        openSubtitleDialog();
    });

    m_btnVideoAdjust.create(m_controlBarHwnd, 545, 20, 68, 36, L"🎨 Video", IDC_MAIN_VIDEO_ADJUST, L"Picture Adjustments (V)");
    m_btnVideoAdjust.setCallback([this]() {
        toggleVideoPanel();
    });

    m_btnAudio.create(m_controlBarHwnd, 615, 20, 68, 36, L"🎵 Audio", IDC_MAIN_AUDIO_PANEL, L"Audio & Equalizer (A)");
    m_btnAudio.setCallback([this]() {
        toggleAudioPanel();
    });

    m_btnInfo.create(m_controlBarHwnd, 685, 20, 56, 36, L"ℹ Info", IDC_MAIN_INFO, L"Diagnostics (I / Tab)");
    m_btnInfo.setCallback([this]() {
        showInfoDialog();
    });

    m_btnPlaylist.create(m_controlBarHwnd, 745, 20, 80, 36, L"☰ Playlist", IDC_MAIN_PLAYLIST, L"Toggle Playlist Panel (P)");
    m_btnPlaylist.setCallback([this]() {
        togglePlaylistPanel();
    });

    m_btnFullscreen.create(m_controlBarHwnd, 830, 20, 40, 36, L"⛶", IDC_MAIN_FULLSCREEN, L"Fullscreen (F / F11)");
    m_btnFullscreen.setCallback([this]() {
        toggleFullscreen();
    });
}

void RXMainWindow::onResize(int width, int height) {
    if (width <= 0 || height <= 0) return;

    int leftPanelWidth = (m_videoPanel && m_videoPanel->isVisible()) ? 300 : 0;
    int rightPanelWidth = 0;
    if (m_playlistPanel && m_playlistPanel->isVisible()) rightPanelWidth += 280;
    if (m_audioPanel && m_audioPanel->isVisible()) rightPanelWidth += 340;
    int controlBarHeight = 68;

    int videoX = leftPanelWidth;
    int videoWidth = width - leftPanelWidth - rightPanelWidth;
    if (videoWidth < 100) videoWidth = 100;

    int videoHeight = m_controlsHidden ? height : (height - controlBarHeight);

    if (m_videoPanel) {
        m_videoPanel->move(0, 0, leftPanelWidth, height);
    }

    if (m_videoHwnd) {
        MoveWindow(m_videoHwnd, videoX, 0, videoWidth, videoHeight, TRUE);
    }

    int currentRight = width;
    if (m_playlistPanel && m_playlistPanel->isVisible()) {
        currentRight -= 280;
        m_playlistPanel->move(currentRight, 0, 280, height);
    } else if (m_playlistPanel) {
        m_playlistPanel->move(-1000, -1000, 0, 0);
    }

    if (m_audioPanel && m_audioPanel->isVisible()) {
        currentRight -= 340;
        m_audioPanel->move(currentRight, 0, 340, height);
    } else if (m_audioPanel) {
        m_audioPanel->move(-1000, -1000, 0, 0);
    }

    if (m_controlBarHwnd) {
        int barY = height - controlBarHeight;
        MoveWindow(m_controlBarHwnd, videoX, barY, videoWidth, controlBarHeight, TRUE);
        layoutControls();
    }
}

void RXMainWindow::layoutControls() {
    RECT rc;
    GetClientRect(m_controlBarHwnd, &rc);
    int width = rc.right - rc.left;

    // Seek bar full width at top
    m_seekBar.move(0, 0, width, 14);

    // Left controls fixed
    m_btnPlay.move(10, 20, 38, 36);
    m_btnStop.move(52, 20, 36, 36);
    m_btnPrev.move(92, 20, 36, 36);
    m_btnNext.move(132, 20, 36, 36);
    m_btnMute.move(172, 20, 36, 36);
    MoveWindow(m_lblTime, 215, 20, 130, 36, TRUE);

    // Right controls aligned to right
    int right = width - 10;
    right -= 40; m_btnFullscreen.move(right, 20, 40, 36);
    right -= 85; m_btnPlaylist.move(right, 20, 80, 36);
    right -= 60; m_btnInfo.move(right, 20, 56, 36);
    right -= 72; m_btnAudio.move(right, 20, 68, 36);
    right -= 72; m_btnVideoAdjust.move(right, 20, 68, 36);
    right -= 52; m_btnSubtitles.move(right, 20, 48, 36);
    right -= 95; m_btnHwdec.move(right, 20, 90, 36);
}

void RXMainWindow::updateTimeDisplay(double posSec, double durSec) {
    if (!m_lblTime) return;
    std::string timeStr = rx::utils::formatTime(posSec) + " / " + rx::utils::formatTime(durSec);
    std::wstring timeW = rx::utils::utf8ToWide(timeStr);
    SetWindowTextW(m_lblTime, timeW.c_str());
}

void RXMainWindow::showOsdMessage(const std::wstring& msg, int durationMs) {
    m_osdMessage = msg;
    if (m_osdTimerId) KillTimer(m_mainHwnd, m_osdTimerId);
    m_osdTimerId = SetTimer(m_mainHwnd, TIMER_OSD, durationMs, nullptr);
    if (m_videoHwnd) InvalidateRect(m_videoHwnd, nullptr, FALSE);
}

void RXMainWindow::openFileDialog() {
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = m_mainHwnd;
    ofn.lpstrFilter = L"All Media Files\0*.mp4;*.mkv;*.mov;*.avi;*.webm;*.flv;*.ts;*.mp3;*.flac;*.wav;*.aac;*.ogg;*.m4v\0"
                      L"Video Files\0*.mp4;*.mkv;*.mov;*.avi;*.webm;*.flv;*.ts;*.m4v\0"
                      L"Audio Files\0*.mp3;*.flac;*.wav;*.aac;*.ogg;*.wma\0"
                      L"All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        std::string path = rx::utils::wideToUtf8(filename);
        m_playlistMgr.addItem(path);
        m_playlistPanel->refresh();
        m_engine.loadFile(path, true);
    }
}

void RXMainWindow::openFolderDialog() {
    BROWSEINFOW bi{};
    bi.hwndOwner = m_mainHwnd;
    bi.lpszTitle = L"Select Media Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t folderPath[MAX_PATH] = {0};
        if (SHGetPathFromIDListW(pidl, folderPath)) {
            std::string dir = rx::utils::wideToUtf8(folderPath);
            m_playlistMgr.addFolder(dir, true);
            m_playlistPanel->refresh();
            if (m_engine.getState() == PlaybackState::Idle || m_engine.getState() == PlaybackState::Stopped) {
                auto first = m_playlistMgr.getCurrentItem();
                if (first) m_engine.loadFile(first->path, true);
            }
        }
        CoTaskMemFree(pidl);
    }
}

void RXMainWindow::openSubtitleDialog() {
    if (m_subDialog) {
        m_subDialog->show(m_mainHwnd);
    }
}

void RXMainWindow::openUrlDialog() {
    if (m_urlDialog) {
        m_urlDialog->show(m_mainHwnd);
    }
}

void RXMainWindow::openBookmarkDialog() {
    if (m_bmDialog) {
        m_bmDialog->show(m_mainHwnd);
    }
}

void RXMainWindow::addQuickBookmark() {
    double pos = m_engine.getCurrentPosition();
    const auto& info = m_engine.getMediaInfo();
    if (!info.fullPath.empty()) {
        library::MediaDatabase::instance().addBookmark(info.fullPath, pos, "Bookmark at " + rx::utils::formatTime(pos));
        showOsdMessage(L"Bookmark Added: " + rx::utils::utf8ToWide(rx::utils::formatTime(pos)));
        if (m_bmDialog) {
            m_bmDialog->refreshList();
        }
    } else {
        showOsdMessage(L"No active media to bookmark");
    }
}

void RXMainWindow::playItemAtIndex(int index) {
    m_playlistMgr.setCurrentIndex(index);
    auto item = m_playlistMgr.getCurrentItem();
    if (item) {
        m_engine.loadFile(item->path, true);
        m_playlistPanel->refresh();
    }
}

void RXMainWindow::toggleFullscreen() {
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen) {
        GetWindowPlacement(m_mainHwnd, &m_prevPlacement);
        LONG style = GetWindowLongW(m_mainHwnd, GWL_STYLE);
        SetWindowLongW(m_mainHwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

        HMONITOR hMon = MonitorFromWindow(m_mainHwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(hMon, &mi);

        SetWindowPos(m_mainHwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        m_btnFullscreen.setText(L"🗗");
        m_btnFullscreen.setTooltip(L"Exit Fullscreen (Esc)");
    } else {
        SetWindowLongW(m_mainHwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(m_mainHwnd, &m_prevPlacement);
        SetWindowPos(m_mainHwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        m_btnFullscreen.setText(L"⛶");
        m_btnFullscreen.setTooltip(L"Fullscreen (F / F11)");
    }

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::togglePlaylistPanel() {
    if (!m_playlistPanel) return;
    bool show = !m_playlistPanel->isVisible();
    m_playlistPanel->setVisible(show);

    Settings::instance().showPlaylistPanel = show;

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::toggleVideoPanel() {
    if (!m_videoPanel) return;
    bool show = !m_videoPanel->isVisible();
    m_videoPanel->setVisible(show);

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::toggleAudioPanel() {
    if (!m_audioPanel) return;
    bool show = !m_audioPanel->isVisible();
    m_audioPanel->setVisible(show);
    if (show) {
        m_audioPanel->refreshDevices();
        m_audioPanel->refreshTracks();
    }

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::showInfoDialog() {
    if (m_infoDialog) {
        m_infoDialog->show(m_mainHwnd);
    }
}

void RXMainWindow::toggleMute() {
    bool mute = !m_engine.isMuted();
    m_engine.setMute(mute);
    m_btnMute.setText(mute ? L"🔇" : L"🔊");
    showOsdMessage(mute ? L"Muted" : L"Unmuted");
}

void RXMainWindow::takeScreenshot() {
    PWSTR picPath = nullptr;
    std::filesystem::path destDir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &picPath))) {
        destDir = picPath;
        CoTaskMemFree(picPath);
        destDir /= "RX Screenshots";
        std::filesystem::create_directories(destDir);
    } else {
        destDir = ".";
    }

    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << "rx_capture_" << timeT << ".png";
    destDir /= ss.str();

    if (m_engine.takeScreenshot(destDir.string(), true)) {
        showOsdMessage(L"Screenshot Saved: " + destDir.filename().wstring());
    } else {
        showOsdMessage(L"Screenshot Failed");
    }
}

void RXMainWindow::onDropFiles(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::string> paths;

    for (UINT i = 0; i < count; ++i) {
        wchar_t buf[MAX_PATH] = {0};
        if (DragQueryFileW(hDrop, i, buf, MAX_PATH)) {
            paths.push_back(rx::utils::wideToUtf8(buf));
        }
    }
    DragFinish(hDrop);

    if (paths.empty()) return;

    m_playlistMgr.addFiles(paths);
    m_playlistPanel->refresh();

    // Play first dropped item
    m_engine.loadFile(paths[0], true);
}

void RXMainWindow::onKeyDown(WPARAM key) {
    switch (key) {
        case VK_SPACE:
            m_engine.togglePause();
            break;
        case VK_LEFT:
            m_engine.seekRelative(-5.0);
            showOsdMessage(L"Seek: -5s");
            break;
        case VK_RIGHT:
            m_engine.seekRelative(5.0);
            showOsdMessage(L"Seek: +5s");
            break;
        case VK_UP: {
            int vol = rx::utils::clamp(m_engine.getVolume() + 5, 0, 100);
            m_engine.setVolume(vol);
            showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            break;
        }
        case VK_DOWN: {
            int vol = rx::utils::clamp(m_engine.getVolume() - 5, 0, 100);
            m_engine.setVolume(vol);
            showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            break;
        }
        case 'M':
            toggleMute();
            break;
        case 'F':
        case VK_F11:
            toggleFullscreen();
            break;
        case VK_ESCAPE:
            if (m_isFullscreen) toggleFullscreen();
            break;
        case 'S':
            takeScreenshot();
            break;
        case 'B':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                openBookmarkDialog();
            } else {
                addQuickBookmark();
            }
            break;
        case 'V':
            toggleVideoPanel();
            break;
        case 'A':
            toggleAudioPanel();
            break;
        case 'P':
            togglePlaylistPanel();
            break;
        case 'I':
        case VK_TAB:
            showInfoDialog();
            break;
        case 'U':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                openUrlDialog();
            }
            break;
        case 'O':
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                openFileDialog();
            } else {
                m_engine.toggleOsdStats();
            }
            break;
        case VK_OEM_4: { // '[' -> Subtitle delay -100ms
            double delay = m_engine.getSubtitleDelay() - 0.1;
            m_engine.setSubtitleDelay(delay);
            showOsdMessage(L"Subtitle Delay: " + std::to_wstring(static_cast<int>(delay * 1000.0)) + L" ms");
            break;
        }
        case VK_OEM_6: { // ']' -> Subtitle delay +100ms
            double delay = m_engine.getSubtitleDelay() + 0.1;
            m_engine.setSubtitleDelay(delay);
            showOsdMessage(L"Subtitle Delay: " + std::to_wstring(static_cast<int>(delay * 1000.0)) + L" ms");
            break;
        }
        case VK_OEM_PERIOD: // '.' -> Step forward
            m_engine.stepForward();
            break;
        case VK_OEM_COMMA: // ',' -> Step backward
            m_engine.stepBackward();
            break;
    }
}

void RXMainWindow::startAutoHideTimer() {
    m_autoHideTimerId = SetTimer(m_mainHwnd, TIMER_AUTOHIDE, 2500, nullptr);
}

void RXMainWindow::resetAutoHide() {
    if (m_controlsHidden) {
        m_controlsHidden = false;
        ShowWindow(m_controlBarHwnd, SW_SHOW);
        RECT rc;
        GetClientRect(m_mainHwnd, &rc);
        onResize(rc.right - rc.left, rc.bottom - rc.top);
    }
    if (m_autoHideTimerId) {
        KillTimer(m_mainHwnd, m_autoHideTimerId);
    }
    startAutoHideTimer();
}

LRESULT CALLBACK RXMainWindow::mainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXMainWindow* pThis = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        pThis = static_cast<RXMainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_mainHwnd = hwnd;
    } else {
        pThis = reinterpret_cast<RXMainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (msg) {
            case WM_SIZE:
                pThis->onResize(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_DROPFILES:
                pThis->onDropFiles(reinterpret_cast<HDROP>(wParam));
                return 0;
            case WM_KEYDOWN:
                pThis->onKeyDown(wParam);
                return 0;
            case WM_TIMER:
                if (wParam == TIMER_AUTOHIDE) {
                    if (pThis->m_engine.getState() == PlaybackState::Playing && !pThis->m_controlsHidden) {
                        pThis->m_controlsHidden = true;
                        ShowWindow(pThis->m_controlBarHwnd, SW_HIDE);
                        RECT rc;
                        GetClientRect(hwnd, &rc);
                        pThis->onResize(rc.right - rc.left, rc.bottom - rc.top);
                    }
                } else if (wParam == TIMER_OSD) {
                    KillTimer(hwnd, TIMER_OSD);
                    pThis->m_osdTimerId = 0;
                    pThis->m_osdMessage.clear();
                    if (pThis->m_videoHwnd) InvalidateRect(pThis->m_videoHwnd, nullptr, FALSE);
                }
                return 0;
            case WM_CLOSE: {
                auto& s = Settings::instance();
                if (!pThis->m_isFullscreen) {
                    RECT rc;
                    GetWindowRect(hwnd, &rc);
                    s.window.x = rc.left;
                    s.window.y = rc.top;
                    s.window.width = rc.right - rc.left;
                    s.window.height = rc.bottom - rc.top;
                    s.window.maximized = (IsZoomed(hwnd) != 0);
                }
                s.audio.volume = pThis->m_engine.getVolume();
                s.audio.muted = pThis->m_engine.isMuted();
                s.save();
                DestroyWindow(hwnd);
                return 0;
            }
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK RXMainWindow::videoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXMainWindow* pThis = reinterpret_cast<RXMainWindow*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));

    if (pThis) {
        switch (msg) {
            case WM_LBUTTONDOWN:
                pThis->resetAutoHide();
                pThis->m_engine.togglePause();
                return 0;
            case WM_LBUTTONDBLCLK:
                pThis->toggleFullscreen();
                return 0;
            case WM_MOUSEMOVE:
                pThis->resetAutoHide();
                break;
            case WM_MOUSEWHEEL: {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int change = (delta > 0) ? 5 : -5;
                int vol = rx::utils::clamp(pThis->m_engine.getVolume() + change, 0, 100);
                pThis->m_engine.setVolume(vol);
                pThis->showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
                return 0;
            }
            case WM_DROPFILES:
                pThis->onDropFiles(reinterpret_cast<HDROP>(wParam));
                return 0;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                // Draw OSD overlay if present
                if (!pThis->m_osdMessage.empty()) {
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(255, 255, 255));
                    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, pThis->m_osdFont));

                    RECT osdRc{ 20, 20, rc.right - 20, 80 };
                    DrawTextW(hdc, pThis->m_osdMessage.c_str(), -1, &osdRc, DT_LEFT | DT_TOP | DT_SINGLELINE);

                    SelectObject(hdc, oldFont);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK RXMainWindow::controlBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXMainWindow* pThis = reinterpret_cast<RXMainWindow*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (pThis) pThis->onPaintControlBar(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (pThis) pThis->resetAutoHide();
            break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void RXMainWindow::onPaintControlBar(HDC hdc) {
    RECT rc;
    GetClientRect(m_controlBarHwnd, &rc);

    HBRUSH bgBrush = CreateSolidBrush(theme::kPanelBg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Top border separator line
    HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, borderPen));
    MoveToEx(hdc, 0, 0, nullptr);
    LineTo(hdc, rc.right, 0);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

int RXMainWindow::runEventLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

} // namespace rx::ui
