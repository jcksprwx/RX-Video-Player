#include "main_window.hpp"
#include "../core/logger.hpp"
#include "../core/settings.hpp"
#include "../core/utils.hpp"
#include "../core/track_formatter.hpp"
#include "../library/media_database.hpp"
#include <windowsx.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace rx::ui {

static const wchar_t* kMainClassName   = L"RXMediaPlayerMainClass";
static const wchar_t* kVideoClassName  = L"RXVideoViewportClass";
static const wchar_t* kControlBarClass = L"RXControlBarClass";

constexpr UINT_PTR TIMER_AUTOHIDE = 2001;
constexpr UINT_PTR TIMER_OSD      = 2002;

constexpr UINT IDC_MAIN_PLAY          = 3001;
constexpr UINT IDC_MAIN_STOP          = 3002;
constexpr UINT IDC_MAIN_PREV          = 3003;
constexpr UINT IDC_MAIN_NEXT          = 3004;
constexpr UINT IDC_MAIN_OPEN          = 3005;
constexpr UINT IDC_MAIN_VIDEO         = 3006;
constexpr UINT IDC_MAIN_360           = 3007;
constexpr UINT IDC_MAIN_3D            = 3008;
constexpr UINT IDC_MAIN_SUBTITLES     = 3009;
constexpr UINT IDC_MAIN_AUDIO         = 3010;
constexpr UINT IDC_MAIN_PLAYLIST      = 3011;
constexpr UINT IDC_MAIN_SETTINGS      = 3012;
constexpr UINT IDC_MAIN_MENU          = 3013;
constexpr UINT IDC_MAIN_FULLSCREEN    = 3014;
constexpr UINT IDC_MAIN_TIMELINE      = 3015;
constexpr UINT IDC_MAIN_TITLEBAR      = 3016;

// Menu Command IDs
constexpr UINT IDM_OPEN_FILE         = 9001;
constexpr UINT IDM_OPEN_FOLDER       = 9002;
constexpr UINT IDM_OPEN_URL          = 9003;
constexpr UINT IDM_EXIT              = 9004;
constexpr UINT IDM_PLAY_PAUSE        = 9010;
constexpr UINT IDM_STOP              = 9011;
constexpr UINT IDM_PREV_TRACK        = 9012;
constexpr UINT IDM_NEXT_TRACK        = 9013;
constexpr UINT IDM_SPEED_050         = 9014;
constexpr UINT IDM_SPEED_075         = 9015;
constexpr UINT IDM_SPEED_100         = 9016;
constexpr UINT IDM_SPEED_125         = 9017;
constexpr UINT IDM_SPEED_150         = 9018;
constexpr UINT IDM_SPEED_200         = 9019;
constexpr UINT IDM_REPEAT_OFF        = 9020;
constexpr UINT IDM_REPEAT_ALL        = 9021;
constexpr UINT IDM_REPEAT_ONE        = 9022;
constexpr UINT IDM_SEEK_FWD_5        = 9030;
constexpr UINT IDM_SEEK_BACK_5       = 9031;
constexpr UINT IDM_SEEK_FWD_30       = 9032;
constexpr UINT IDM_SEEK_BACK_30      = 9033;
constexpr UINT IDM_BOOKMARKS         = 9034;
constexpr UINT IDM_ADD_BOOKMARK      = 9035;
constexpr UINT IDM_AUDIO_TRACK_MENU   = 9036;
constexpr UINT IDM_AUDIO_CHAN_BOTH    = 9037;
constexpr UINT IDM_AUDIO_CHAN_LEFT    = 9038;
constexpr UINT IDM_AUDIO_CHAN_RIGHT   = 9039;
constexpr UINT IDM_AUDIO_EQUALIZER   = 9040;
constexpr UINT IDM_MUTE              = 9041;
constexpr UINT IDM_VOL_UP            = 9042;
constexpr UINT IDM_VOL_DOWN          = 9043;
constexpr UINT IDM_VIDEO_ADJUST      = 9050;
constexpr UINT IDM_ASPECT_AUTO       = 9051;
constexpr UINT IDM_ASPECT_16_9       = 9052;
constexpr UINT IDM_ASPECT_4_3        = 9053;
constexpr UINT IDM_ASPECT_21_9       = 9054;
constexpr UINT IDM_ROTATE_90         = 9055;
constexpr UINT IDM_FLIP_HORZ         = 9056;
constexpr UINT IDM_FLIP_VERT         = 9057;
constexpr UINT IDM_HWDEC_TOGGLE      = 9058;
constexpr UINT IDM_SCREENSHOT        = 9059;
constexpr UINT IDM_SUBTITLE_SETTINGS = 9060;
constexpr UINT IDM_SUBTITLE_LOAD     = 9061;
constexpr UINT IDM_SUBTITLE_TRACK_MENU = 9062;
constexpr UINT IDM_SUBTITLE_ONLINE     = 9063;
constexpr UINT IDM_PLAYLIST_TOGGLE   = 9070;
constexpr UINT IDM_PLAYLIST_SHUFFLE  = 9071;
constexpr UINT IDM_PLAYLIST_CLEAR    = 9072;
constexpr UINT IDM_FULLSCREEN        = 9080;
constexpr UINT IDM_COMPACT_MODE      = 9081;
constexpr UINT IDM_ALWAYS_ON_TOP     = 9082;
constexpr UINT IDM_MEDIA_INFO        = 9083;
constexpr UINT IDM_PREFERENCES       = 9090;
constexpr UINT IDM_ABOUT             = 9091;
constexpr UINT IDM_SHORTCUT_HELP     = 9092;
constexpr UINT IDM_RECENT_BASE       = 9200;
constexpr UINT IDM_AUDIO_TRACK_BASE   = 9300;
constexpr UINT IDM_SUBTITLE_TRACK_BASE = 9400;

RXMainWindow::RXMainWindow() = default;

RXMainWindow::~RXMainWindow() {
    if (m_uiFont) DeleteObject(m_uiFont);
    if (m_uiBoldFont) DeleteObject(m_uiBoldFont);
    if (m_osdFont) DeleteObject(m_osdFont);
    if (m_brandFont) DeleteObject(m_brandFont);
    if (m_brandSubFont) DeleteObject(m_brandSubFont);
    if (m_recentFont) DeleteObject(m_recentFont);
    if (m_hBrandIcon) DestroyIcon(m_hBrandIcon);
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
    wc.hIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
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
    cbc.hbrBackground = CreateSolidBrush(theme::kToolbarBg);
    cbc.lpszClassName = kControlBarClass;
    RegisterClassExW(&cbc);

    m_uiFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_uiBoldFont = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_osdFont = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_brandFont = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_brandSubFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_recentFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    m_hBrandIcon = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, 96, 96, LR_DEFAULTCOLOR);

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
    MARGINS margins = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(m_mainHwnd, &margins);

    if (wc.hIcon) {
        SendMessageW(m_mainHwnd, WM_SETICON, ICON_BIG, (LPARAM)wc.hIcon);
    }
    if (wc.hIconSm) {
        SendMessageW(m_mainHwnd, WM_SETICON, ICON_SMALL, (LPARAM)wc.hIconSm);
    }

    // 1. Create Top Title Bar (Section A: ~38px)
    m_titleBar.create(m_mainHwnd, ww, 38, IDC_MAIN_TITLEBAR);
    m_titleBar.setMenuCallback([this](int screenX, int screenY) {
        showMainMenu(screenX, screenY);
    });
    m_titleBar.setPinCallback([this]() {
        toggleAlwaysOnTop();
    });
    m_titleBar.setMinCallback([this]() {
        ShowWindow(m_mainHwnd, SW_MINIMIZE);
    });
    m_titleBar.setMaxCallback([this]() {
        if (IsZoomed(m_mainHwnd)) ShowWindow(m_mainHwnd, SW_RESTORE);
        else ShowWindow(m_mainHwnd, SW_MAXIMIZE);
    });
    m_titleBar.setCloseCallback([this]() {
        PostMessageW(m_mainHwnd, WM_CLOSE, 0, 0);
    });

    // 2. Create Video Viewport Child Window (Section B: flexible height)
    m_videoHwnd = CreateWindowExW(
        0, kVideoClassName, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 38, ww, wh - 38 - 26 - 48,
        m_mainHwnd, nullptr, hInstance, this
    );

    // 3. Create Timeline & Volume Strip (Section C: ~26px)
    m_timelineStrip.create(m_mainHwnd, 0, wh - 74, ww, 26, IDC_MAIN_TIMELINE);
    m_timelineStrip.setSeekCallback([this](double targetSec) {
        m_engine.seek(targetSec, true);
    });
    m_timelineStrip.setVolumeCallback([this](int volPercent) {
        m_engine.setVolume(volPercent);
        Settings::instance().audio.volume = volPercent;
        showOsdMessage(L"Volume: " + std::to_wstring(volPercent) + L"%");
    });
    m_timelineStrip.setMuteCallback([this]() {
        toggleMute();
    });

    // 4. Create Bottom Control Bar (Section D: ~48px)
    m_controlBarHwnd = CreateWindowExW(
        0, kControlBarClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, wh - 48, ww, 48,
        m_mainHwnd, nullptr, hInstance, this
    );

    // Create Side Panels
    m_playlistPanel = std::make_unique<RXPlaylistPanel>(m_playlistMgr);
    m_playlistPanel->create(m_mainHwnd, ww - 280, 38, 280, wh - 38 - 74);
    m_playlistPanel->setVisible(settings.showPlaylistPanel);

    m_videoPanel = std::make_unique<RXVideoPanel>();
    m_videoPanel->create(m_mainHwnd, 0, 38, 300, wh - 38 - 74);
    m_videoPanel->setParams(settings.video.adjustments);
    m_videoPanel->setCallback([this](const VideoAdjustmentParams& params) {
        m_engine.applyVideoAdjustments(params);
        Settings::instance().video.adjustments = params;
    });

    m_infoDialog = std::make_unique<RXInfoDialog>(m_engine);

    m_subDialog = std::make_unique<RXSubtitleDialog>(m_engine);
    m_subDialog->setStyle(settings.subtitle.style);
    m_subDialog->setCallback([](const SubtitleStyle& st) {
        Settings::instance().subtitle.style = st;
    });

    m_urlDialog = std::make_unique<RXOpenUrlDialog>();
    m_urlDialog->setCallback([this](const std::string& url, int cacheSec, int maxBufferMb) {
        m_playlistMgr.addItem(url);
        m_playlistPanel->refresh();
        m_engine.openUrl(url, cacheSec, maxBufferMb);
        showOsdMessage(L"Opening Stream: " + rx::utils::utf8ToWide(url));
    });

    library::MediaDatabase::instance().initialize();
    m_bmDialog = std::make_unique<RXBookmarkDialog>(m_engine);
    m_prefsDialog = std::make_unique<RXPreferencesDialog>();
    ShortcutManager::instance().loadFromSettings(settings);

    m_audioPanel = std::make_unique<RXAudioPanel>(m_engine);
    m_audioPanel->create(m_mainHwnd, ww - 340, 38, 340, wh - 38 - 74);
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

    // Setup Child Controls
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
    m_engine.setVolume(settings.audio.volume);
    m_engine.setMute(settings.audio.muted);
    m_engine.setAudioDevice(settings.audio.deviceId);
    m_engine.setAudioExclusive(settings.audio.exclusiveMode);
    m_engine.setAudioDelay(settings.audio.delayMs / 1000.0);
    m_engine.setAudioFilter(audio::Equalizer::buildFilterString(settings.audio.equalizer));

    m_timelineStrip.setVolume(settings.audio.volume, settings.audio.muted);

    // Setup engine callbacks
    m_engine.setProgressCallback([this](double posSec, double durSec) {
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
        if (m_controlBarHwnd) InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
    });

    m_engine.setMediaLoadedCallback([this](const MediaInfo& info) {
        std::wstring filenameW = rx::utils::utf8ToWide(info.filename);
        std::wstring titleW = L"RX Media Player — " + filenameW;
        SetWindowTextW(m_mainHwnd, titleW.c_str());

        // Extract container / format badge
        std::string ext = "";
        try {
            ext = std::filesystem::path(info.fullPath).extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        } catch (...) {}
        if (ext.empty() && !info.container.empty()) {
            ext = info.container;
            size_t comma = ext.find(',');
            if (comma != std::string::npos) ext = ext.substr(0, comma);
        }
        std::wstring badgeW = rx::utils::utf8ToWide(ext);
        std::transform(badgeW.begin(), badgeW.end(), badgeW.begin(), ::towupper);
        if (badgeW.length() > 5) badgeW = badgeW.substr(0, 5);

        m_titleBar.setMediaInfo(badgeW, filenameW);

        // Auto-load matching subtitles if configured
        if (Settings::instance().subtitle.autoLoad && !info.fullPath.empty()) {
            auto subs = rx::utils::findMatchingSubtitles(info.fullPath);
            if (!subs.empty()) {
                m_engine.loadSubtitle(subs[0].string());
                showOsdMessage(L"Loaded Subtitle: " + subs[0].filename().wstring());
            }
        }

        if (m_audioPanel) m_audioPanel->refreshTracks();
        if (m_subDialog) m_subDialog->refreshTracks();

        // Playback Position Resume from SQLite Database
        if (Settings::instance().playback.rememberPosition && !info.fullPath.empty()) {
            double lastPos = library::MediaDatabase::instance().getLastPosition(info.fullPath);
            if (lastPos > 5.0 && (info.durationSec <= 0.0 || lastPos < (info.durationSec - 10.0))) {
                m_engine.seek(lastPos, true);
                showOsdMessage(L"Resumed at: " + rx::utils::utf8ToWide(rx::utils::formatTime(lastPos)));
            }
        }

        if (m_bmDialog) m_bmDialog->refreshList();
        if (m_controlBarHwnd) InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
    });

    m_engine.setEndFileCallback([this](int /*reason*/) {
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

    // Initial Layout
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
    // Left playback controls (height = 32px, y = 8px)
    m_btnPlay.create(m_controlBarHwnd, 8, 8, 34, 32, L"►", IDC_MAIN_PLAY, L"Play / Pause (Space)");
    m_btnPlay.setCallback([this]() {
        m_engine.togglePause();
    });

    m_btnStop.create(m_controlBarHwnd, 44, 8, 30, 32, L"■", IDC_MAIN_STOP, L"Stop (S)");
    m_btnStop.setCallback([this]() {
        m_engine.stop();
        m_btnPlay.setText(L"►");
        m_titleBar.setMediaInfo(L"", L"RX Media Player");
        if (m_videoHwnd) InvalidateRect(m_videoHwnd, nullptr, TRUE);
        if (m_controlBarHwnd) InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
    });

    m_btnPrev.create(m_controlBarHwnd, 76, 8, 30, 32, L"⏮", IDC_MAIN_PREV, L"Previous Track (PgUp / Ctrl+Left)");
    m_btnPrev.setCallback([this]() {
        auto prevItem = m_playlistMgr.previous();
        if (prevItem) {
            m_engine.loadFile(prevItem->path, true);
            m_playlistPanel->refresh();
        }
    });

    m_btnNext.create(m_controlBarHwnd, 108, 8, 30, 32, L"⏭", IDC_MAIN_NEXT, L"Next Track (PgDn / Ctrl+Right)");
    m_btnNext.setCallback([this]() {
        auto nextItem = m_playlistMgr.next(true);
        if (nextItem) {
            m_engine.loadFile(nextItem->path, true);
            m_playlistPanel->refresh();
        }
    });

    m_btnOpen.create(m_controlBarHwnd, 140, 8, 32, 32, L"📂", IDC_MAIN_OPEN, L"Open Media File (Ctrl+O)");
    m_btnOpen.setCallback([this]() {
        openFileDialog();
    });

    // Right feature controls (height = 32px, y = 8px)
    m_btnVideo.create(m_controlBarHwnd, 0, 8, 34, 32, L"🎨", IDC_MAIN_VIDEO, L"Picture Adjustments (V)");
    m_btnVideo.setCallback([this]() {
        toggleVideoPanel();
    });

    m_btn360.create(m_controlBarHwnd, 0, 8, 38, 32, L"360°", IDC_MAIN_360, L"Aspect Ratio / 360 Mode");
    m_btn360.setCallback([this]() {
        cycleAspectRatio();
    });

    m_btn3D.create(m_controlBarHwnd, 0, 8, 34, 32, L"3D", IDC_MAIN_3D, L"3D Video / Deinterlace Mode");
    m_btn3D.setCallback([this]() {
        toggleDeinterlace();
    });

    m_btnSubtitles.create(m_controlBarHwnd, 0, 8, 32, 32, L"CC", IDC_MAIN_SUBTITLES, L"Subtitle Stream & Tracks (L)");
    m_btnSubtitles.setCallback([this]() {
        showSubtitleTrackMenu(0, 0);
    });

    m_btnAudio.create(m_controlBarHwnd, 0, 8, 32, 32, L"🎵", IDC_MAIN_AUDIO, L"Audio Stream & Tracks (Alt+A)");
    m_btnAudio.setCallback([this]() {
        showAudioTrackMenu(0, 0);
    });

    m_btnPlaylist.create(m_controlBarHwnd, 0, 8, 36, 32, L"☰", IDC_MAIN_PLAYLIST, L"Toggle Playlist Panel (P / Ctrl+P)");
    m_btnPlaylist.setAccent(m_playlistPanel && m_playlistPanel->isVisible());
    m_btnPlaylist.setCallback([this]() {
        togglePlaylistPanel();
    });

    m_btnSettings.create(m_controlBarHwnd, 0, 8, 32, 32, L"⚙", IDC_MAIN_SETTINGS, L"Preferences Center (Ctrl+P / Ctrl+,)");
    m_btnSettings.setCallback([this]() {
        showPreferencesDialog();
    });

    m_btnMenu.create(m_controlBarHwnd, 0, 8, 32, 32, L"≡", IDC_MAIN_MENU, L"Main Application Menu");
    m_btnMenu.setCallback([this]() {
        RECT btnRc;
        GetWindowRect(m_btnMenu.getHwnd(), &btnRc);
        showMainMenu(btnRc.left, btnRc.top);
    });

    m_btnFullscreen.create(m_controlBarHwnd, 0, 8, 34, 32, L"⛶", IDC_MAIN_FULLSCREEN, L"Toggle Fullscreen (F / F11)");
    m_btnFullscreen.setCallback([this]() {
        toggleFullscreen();
    });
}

void RXMainWindow::onResize(int width, int height) {
    if (width <= 0 || height <= 0) return;

    int topBarHeight = 38;
    int timelineHeight = 26;
    int controlBarHeight = 48;

    int leftPanelWidth = (m_videoPanel && m_videoPanel->isVisible()) ? 300 : 0;
    int rightPanelWidth = 0;
    if (m_playlistPanel && m_playlistPanel->isVisible()) rightPanelWidth += 280;
    if (m_audioPanel && m_audioPanel->isVisible()) rightPanelWidth += 340;

    if (m_isFullscreen) {
        if (m_controlsHidden) {
            m_titleBar.move(-1000, -1000, 0, 0);
            m_timelineStrip.move(-1000, -1000, 0, 0);
            if (m_controlBarHwnd) MoveWindow(m_controlBarHwnd, -1000, -1000, 0, 0, FALSE);
            if (m_videoHwnd) MoveWindow(m_videoHwnd, 0, 0, width, height, TRUE);
            return;
        } else {
            m_titleBar.move(0, 0, width, topBarHeight);
            int barY = height - controlBarHeight;
            int timeY = barY - timelineHeight;
            m_timelineStrip.move(0, timeY, width, timelineHeight);
            if (m_controlBarHwnd) {
                MoveWindow(m_controlBarHwnd, 0, barY, width, controlBarHeight, TRUE);
                layoutControls();
            }
            if (m_videoHwnd) MoveWindow(m_videoHwnd, 0, 0, width, height, TRUE);
            return;
        }
    }

    // Normal windowed layout
    int videoX = leftPanelWidth;
    int videoY = topBarHeight;
    int videoWidth = width - leftPanelWidth - rightPanelWidth;
    if (videoWidth < 100) videoWidth = 100;
    int videoHeight = height - topBarHeight - timelineHeight - controlBarHeight;
    if (videoHeight < 50) videoHeight = 50;

    m_titleBar.move(0, 0, width, topBarHeight);

    if (m_videoPanel) {
        m_videoPanel->move(0, topBarHeight, leftPanelWidth, height - topBarHeight);
    }

    if (m_videoHwnd) {
        MoveWindow(m_videoHwnd, videoX, videoY, videoWidth, videoHeight, TRUE);
    }

    int currentRight = width;
    if (m_playlistPanel && m_playlistPanel->isVisible()) {
        currentRight -= 280;
        m_playlistPanel->move(currentRight, topBarHeight, 280, height - topBarHeight);
    } else if (m_playlistPanel) {
        m_playlistPanel->move(-1000, -1000, 0, 0);
    }

    if (m_audioPanel && m_audioPanel->isVisible()) {
        currentRight -= 340;
        m_audioPanel->move(currentRight, topBarHeight, 340, height - topBarHeight);
    } else if (m_audioPanel) {
        m_audioPanel->move(-1000, -1000, 0, 0);
    }

    int timeStripY = height - controlBarHeight - timelineHeight;
    m_timelineStrip.move(videoX, timeStripY, videoWidth, timelineHeight);

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
    int height = rc.bottom - rc.top;

    int btnY = (height - 30) / 2;
    int btnH = 30;

    // Left playback controls (5 compact buttons)
    // 8, 42, 72, 102, 132 -> Play (32), Stop (28), Prev (28), Next (28), Open (30)
    // End of Open button: 132 + 30 = 162.
    m_btnPlay.move(8, btnY, 32, btnH);
    m_btnStop.move(42, btnY, 28, btnH);
    m_btnPrev.move(72, btnY, 28, btnH);
    m_btnNext.move(102, btnY, 28, btnH);
    m_btnOpen.move(132, btnY, 30, btnH);

    // Right feature controls
    int right = width - 8;
    right -= 32; m_btnFullscreen.move(right, btnY, 32, btnH);
    right -= 2;
    right -= 28; m_btnMenu.move(right, btnY, 28, btnH);
    right -= 2;
    right -= 28; m_btnSettings.move(right, btnY, 28, btnH);
    right -= 2;
    right -= 32; m_btnPlaylist.move(right, btnY, 32, btnH);
    right -= 2;
    right -= 28; m_btnAudio.move(right, btnY, 28, btnH);
    right -= 2;
    right -= 28; m_btnSubtitles.move(right, btnY, 28, btnH);
    right -= 2;
    right -= 28; m_btnVideo.move(right, btnY, 28, btnH);

    bool showSecondary = (width >= 880);
    if (showSecondary) {
        ShowWindow(m_btn360.getHwnd(), SW_SHOW);
        ShowWindow(m_btn3D.getHwnd(), SW_SHOW);
        right -= 2;
        right -= 30; m_btn360.move(right, btnY, 30, btnH);
        right -= 2;
        right -= 28; m_btn3D.move(right, btnY, 28, btnH);
    } else {
        ShowWindow(m_btn3D.getHwnd(), SW_HIDE);
        ShowWindow(m_btn360.getHwnd(), SW_HIDE);
        m_btn3D.move(-1000, -1000, 0, 0);
        m_btn360.move(-1000, -1000, 0, 0);
    }
    m_rightControlsStart = right;
}

void RXMainWindow::onPaintControlBar(HDC hdc) {
    RECT rc;
    GetClientRect(m_controlBarHwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    if (width <= 0 || height <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDC, memBmp));

    // Clear Background (#202020)
    HBRUSH bgBrush = CreateSolidBrush(theme::kToolbarBg);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Top border separator line (#414141)
    HPEN borderPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
    HPEN oldPen = static_cast<HPEN>(SelectObject(memDC, borderPen));
    MoveToEx(memDC, 0, 0, nullptr);
    LineTo(memDC, width, 0);

    // Left separator line after Left playback buttons (x = 168)
    MoveToEx(memDC, 168, 8, nullptr);
    LineTo(memDC, 168, height - 8);

    // Right separator line before Right feature buttons
    int rightControlsStart = (m_rightControlsStart > 0) ? m_rightControlsStart : (width - 240);
    MoveToEx(memDC, rightControlsStart - 8, 8, nullptr);
    LineTo(memDC, rightControlsStart - 8, height - 8);

    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    // Center playback information: 00:33:58 / 00:41:16 | H/W | H.265 | AAC | 2.0
    SetBkMode(memDC, TRANSPARENT);

    std::string curTime = rx::utils::formatTime(m_cachedPos);
    std::string durTime = rx::utils::formatTime(m_cachedDur);
    std::wstring curTimeW = rx::utils::utf8ToWide(curTime);
    std::wstring durTimeW = rx::utils::utf8ToWide(durTime);

    int curX = 176;
    HFONT oldFont = static_cast<HFONT>(SelectObject(memDC, m_uiBoldFont));
    SetTextColor(memDC, theme::kCurrentTimeAccent); // #83C6FF subtle blue accent

    SIZE szCur{};
    GetTextExtentPoint32W(memDC, curTimeW.c_str(), static_cast<int>(curTimeW.length()), &szCur);
    RECT rcTime{ curX, 0, curX + szCur.cx + 4, height };
    DrawTextW(memDC, curTimeW.c_str(), -1, &rcTime, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    curX += szCur.cx + 5;

    // Separator /
    SelectObject(memDC, m_uiFont);
    SetTextColor(memDC, theme::kTextSecondary);
    RECT rcSlash{ curX, 0, curX + 12, height };
    DrawTextW(memDC, L"/", -1, &rcSlash, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    curX += 13;

    // Total Duration #F2F2F2
    SetTextColor(memDC, theme::kTextPrimary);
    SIZE szDur{};
    GetTextExtentPoint32W(memDC, durTimeW.c_str(), static_cast<int>(durTimeW.length()), &szDur);
    RECT rcDur{ curX, 0, curX + szDur.cx + 4, height };
    DrawTextW(memDC, durTimeW.c_str(), -1, &rcDur, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    curX += szDur.cx + 12;

    // Reset badge rects before painting
    m_rcHwdecBadge = {0, 0, 0, 0};
    m_rcVideoCodecBadge = {0, 0, 0, 0};
    m_rcAudioCodecBadge = {0, 0, 0, 0};
    m_rcChannelsBadge = {0, 0, 0, 0};

    // Telemetry Badges Helper
    HFONT bFont = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT fO = static_cast<HFONT>(SelectObject(memDC, bFont));

    auto drawBadge = [&](int x, const std::wstring& text, bool isAccent) -> RECT {
        SIZE szBadge{};
        GetTextExtentPoint32W(memDC, text.c_str(), static_cast<int>(text.length()), &szBadge);
        int badgeW = szBadge.cx + 10;
        if (badgeW < 30) badgeW = 30;

        if (x + badgeW >= rightControlsStart - 8) {
            return RECT{0, 0, 0, 0};
        }

        RECT r{ x, (height - 20) / 2, x + badgeW, (height + 20) / 2 };
        HBRUSH bBg = CreateSolidBrush(theme::kButtonBg);
        HPEN bPen = CreatePen(PS_SOLID, 1, isAccent ? theme::kAccentColor : theme::kBorderColor);
        HPEN pO = static_cast<HPEN>(SelectObject(memDC, bPen));
        HBRUSH bO = static_cast<HBRUSH>(SelectObject(memDC, bBg));
        RoundRect(memDC, r.left, r.top, r.right, r.bottom, 4, 4);
        SelectObject(memDC, bO);
        SelectObject(memDC, pO);
        DeleteObject(bBg);
        DeleteObject(bPen);

        SetTextColor(memDC, isAccent ? theme::kAccentColor : theme::kTextSecondary);
        DrawTextW(memDC, text.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return r;
    };

    const auto& info = m_engine.getMediaInfo();
    bool isHw = (m_engine.getActiveHwdec() != "no" && !m_engine.getActiveHwdec().empty());

    // 1. [ H/W ] or [ S/W ] badge
    RECT rcHw = drawBadge(curX, isHw ? L"H/W" : L"S/W", isHw);
    if (rcHw.right > rcHw.left) {
        m_rcHwdecBadge = rcHw;
        curX = rcHw.right + 6;
    }

    // 2. [ H.265 ] Video Codec Badge
    if (!info.videoCodec.empty()) {
        std::wstring vCodecW = rx::utils::utf8ToWide(info.videoCodec);
        std::transform(vCodecW.begin(), vCodecW.end(), vCodecW.begin(), ::towupper);
        if (vCodecW == L"HEVC") vCodecW = L"H.265";
        else if (vCodecW == L"H264") vCodecW = L"H.264";

        RECT rcV = drawBadge(curX, vCodecW, false);
        if (rcV.right > rcV.left) {
            m_rcVideoCodecBadge = rcV;
            curX = rcV.right + 6;
        }
    }

    // 3. [ AAC ] Audio Codec Badge
    if (!info.audioCodec.empty()) {
        std::wstring aCodecW = rx::utils::utf8ToWide(info.audioCodec);
        std::transform(aCodecW.begin(), aCodecW.end(), aCodecW.begin(), ::towupper);

        RECT rcA = drawBadge(curX, aCodecW, false);
        if (rcA.right > rcA.left) {
            m_rcAudioCodecBadge = rcA;
            curX = rcA.right + 6;
        }
    }

    // 4. [ 2.0 ] Channels Badge
    if (info.audioChannels > 0) {
        std::wstring chStr = (info.audioChannels == 6) ? L"5.1" :
                             (info.audioChannels == 8) ? L"7.1" :
                             (info.audioChannels == 2) ? L"2.0" :
                             (info.audioChannels == 1) ? L"1.0" :
                             (std::to_wstring(info.audioChannels) + L"ch");

        RECT rcCh = drawBadge(curX, chStr, false);
        if (rcCh.right > rcCh.left) {
            m_rcChannelsBadge = rcCh;
            curX = rcCh.right + 6;
        }
    }

    SelectObject(memDC, fO);
    DeleteObject(bFont);
    SelectObject(memDC, oldFont);

    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void RXMainWindow::updateTimeDisplay(double posSec, double durSec) {
    m_cachedPos = posSec;
    m_cachedDur = durSec;
    m_timelineStrip.setPosition(posSec, durSec);
    if (m_controlBarHwnd) InvalidateRect(m_controlBarHwnd, nullptr, FALSE);
}

void RXMainWindow::showOsdMessage(const std::wstring& msg, int durationMs) {
    m_osdMessage = msg;
    if (m_osdTimerId) KillTimer(m_mainHwnd, m_osdTimerId);
    m_osdTimerId = SetTimer(m_mainHwnd, TIMER_OSD, durationMs, nullptr);
    m_engine.showOsd(rx::utils::wideToUtf8(msg), durationMs);
    if (m_videoHwnd) InvalidateRect(m_videoHwnd, nullptr, FALSE);
}

void RXMainWindow::cycleAspectRatio() {
    static const char* s_aspects[] = { "-1", "16:9", "4:3", "21:9", "2.35:1", "1:1" };
    static const wchar_t* s_names[] = { L"Auto / Original", L"16:9 Widescreen", L"4:3 Standard", L"21:9 Cinemascope", L"2.35:1 Anamorphic", L"1:1 Square" };
    static int s_idx = 0;
    s_idx = (s_idx + 1) % 6;
    m_engine.setAspectRatio(s_aspects[s_idx]);
    showOsdMessage(std::wstring(L"Aspect Ratio: ") + s_names[s_idx]);
}

void RXMainWindow::toggleDeinterlace() {
    bool d = !m_engine.getVideoAdjustments().deinterlace;
    m_engine.setDeinterlace(d);
    showOsdMessage(d ? L"Deinterlacing: ON" : L"Deinterlacing: OFF");
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
        openFile(path);
    }
}

void RXMainWindow::openFolderDialog() {
    BROWSEINFOW bi{};
    bi.hwndOwner = m_mainHwnd;
    bi.lpszTitle = L"Select Folder to Play with RX Media Player";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t folderPath[MAX_PATH] = {0};
        if (SHGetPathFromIDListW(pidl, folderPath)) {
            std::string folderStr = rx::utils::wideToUtf8(folderPath);
            size_t prevSize = m_playlistMgr.size();
            m_playlistMgr.addFolder(folderStr);
            m_playlistPanel->refresh();
            size_t added = m_playlistMgr.size() - prevSize;
            if (added > 0) {
                auto current = m_playlistMgr.getCurrentItem();
                if (current) {
                    m_engine.loadFile(current->path, true);
                }
                showOsdMessage(L"Loaded " + std::to_wstring(added) + L" files from folder");
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
    const auto& info = m_engine.getMediaInfo();
    if (info.fullPath.empty()) {
        showOsdMessage(L"No active media to bookmark");
        return;
    }
    double curPos = m_engine.getCurrentPosition();
    std::string title = rx::utils::formatTime(curPos);
    library::MediaDatabase::instance().addBookmark(info.fullPath, curPos, title);
    showOsdMessage(L"Bookmark Added: " + rx::utils::utf8ToWide(title));
    if (m_bmDialog) m_bmDialog->refreshList();
}

void RXMainWindow::playItemAtIndex(int index) {
    auto item = m_playlistMgr.getItem(index);
    if (item) {
        m_playlistMgr.setCurrentIndex(index);
        m_engine.loadFile(item->path, true);
        m_playlistPanel->refresh();
    }
}

void RXMainWindow::toggleFullscreen() {
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen) {
        GetWindowPlacement(m_mainHwnd, &m_prevPlacement);
        SetWindowLongPtrW(m_mainHwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

        HMONITOR hMon = MonitorFromWindow(m_mainHwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(hMon, &mi);

        SetWindowPos(m_mainHwnd, HWND_TOPMOST,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        startAutoHideTimer();
    } else {
        SetWindowLongPtrW(m_mainHwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN);
        SetWindowPlacement(m_mainHwnd, &m_prevPlacement);
        SetWindowPos(m_mainHwnd, m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        m_controlsHidden = false;
        m_titleBar.setMaximized(IsZoomed(m_mainHwnd));
    }

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
    showOsdMessage(m_isFullscreen ? L"Fullscreen" : L"Windowed Mode");
}

void RXMainWindow::togglePlaylistPanel() {
    if (!m_playlistPanel) return;
    bool vis = !m_playlistPanel->isVisible();
    m_playlistPanel->setVisible(vis);
    Settings::instance().showPlaylistPanel = vis;
    m_btnPlaylist.setAccent(vis);

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::toggleVideoPanel() {
    if (!m_videoPanel) return;
    bool vis = !m_videoPanel->isVisible();
    m_videoPanel->setVisible(vis);
    m_btnVideo.setAccent(vis);

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::toggleAudioPanel() {
    if (!m_audioPanel) return;
    bool vis = !m_audioPanel->isVisible();
    m_audioPanel->setVisible(vis);
    m_btnAudio.setAccent(vis);

    RECT rc;
    GetClientRect(m_mainHwnd, &rc);
    onResize(rc.right - rc.left, rc.bottom - rc.top);
}

void RXMainWindow::showInfoDialog() {
    if (!m_infoDialog) m_infoDialog = std::make_unique<RXInfoDialog>(m_engine);
    m_infoDialog->show(m_mainHwnd);
}

void RXMainWindow::showPreferencesDialog() {
    if (!m_prefsDialog) m_prefsDialog = std::make_unique<RXPreferencesDialog>();
    m_prefsDialog->show(m_mainHwnd);
}

void RXMainWindow::showShortcutHelpDialog() {
    if (!m_shortcutHelpDialog) m_shortcutHelpDialog = std::make_unique<RXShortcutHelpDialog>();
    m_shortcutHelpDialog->show(m_mainHwnd);
}

void RXMainWindow::showAboutDialog() {
    std::wstring msg =
        L"RX Media Player v2.4.1 (Windows 64-bit)\n\n"
        L"Author & Lead Architect: Rituraj\n"
        L"Repository: https://github.com/jcksprwx/RX-Video-Player\n\n"
        L"Architecture & Subsystems:\n"
        L"  • Modern C++20 Core Application\n"
        L"  • Direct3D 11 GPU Rendering (D3D11VA / DXVA2)\n"
        L"  • Direct WASAPI Audio with 10-Band Graphic Equalizer\n"
        L"  • Professional Audio & Subtitle Track Selection\n"
        L"  • OpenSubtitles.com Online Subtitle Integration\n"
        L"  • Multimedia Engine: libmpv & FFmpeg Backends\n"
        L"  • Library & History: Embedded SQLite 3 Database\n\n"
        L"Engineered for high performance, smooth playback, and precision control.\n"
        L"Copyright (c) 2026 RX Media Player. All rights reserved.";

    MessageBoxW(m_mainHwnd, msg.c_str(), L"About RX Media Player", MB_ICONINFORMATION | MB_OK);
}

void RXMainWindow::showAudioTrackMenu(int screenX, int screenY) {
    if (screenX == 0 && screenY == 0) {
        RECT rc{};
        GetWindowRect(m_btnAudio.getHwnd(), &rc);
        screenX = rc.left;
        screenY = rc.top;
    }

    auto tracks = m_engine.getAudioTracks();
    int curTrackId = m_engine.getSelectedAudioTrack();
    AudioChannelMode curMode = m_engine.getAudioChannelMode();

    std::vector<rx::ui::TrackMenuItem> items;

    // Header
    rx::ui::TrackMenuItem hdr;
    hdr.id = 1001;
    hdr.text = L"Cycle Audio Stream";
    hdr.shortcut = L"Alt+A";
    hdr.isHeader = true;
    items.push_back(hdr);

    if (tracks.empty()) {
        rx::ui::TrackMenuItem none;
        none.id = 0;
        none.text = L"No Audio Tracks Available";
        none.isEnabled = false;
        items.push_back(none);
    } else {
        for (size_t i = 0; i < tracks.size(); ++i) {
            rx::ui::TrackMenuItem it;
            it.id = 2000 + tracks[i].id;
            it.text = rx::track_formatter::formatAudioTrackMenuLabel(tracks[i], static_cast<int>(i + 1));
            it.isRadio = true;
            it.isSelected = (tracks[i].id == curTrackId);
            items.push_back(it);
        }
    }

    // Separator
    rx::ui::TrackMenuItem sep1;
    sep1.isSeparator = true;
    items.push_back(sep1);

    // Audio channel selection
    rx::ui::TrackMenuItem mBoth;
    mBoth.id = 1010;
    mBoth.text = L"Both";
    mBoth.isRadio = true;
    mBoth.isSelected = (curMode == AudioChannelMode::Both);
    items.push_back(mBoth);

    rx::ui::TrackMenuItem mLeft;
    mLeft.id = 1011;
    mLeft.text = L"Left channel";
    mLeft.isRadio = true;
    mLeft.isSelected = (curMode == AudioChannelMode::Left);
    items.push_back(mLeft);

    rx::ui::TrackMenuItem mRight;
    mRight.id = 1012;
    mRight.text = L"Right channel";
    mRight.isRadio = true;
    mRight.isSelected = (curMode == AudioChannelMode::Right);
    items.push_back(mRight);

    // Separator
    rx::ui::TrackMenuItem sep2;
    sep2.isSeparator = true;
    items.push_back(sep2);

    rx::ui::TrackMenuItem mEq;
    mEq.id = 1020;
    mEq.text = L"10-Band Equalizer & WASAPI...";
    mEq.shortcut = L"A";
    items.push_back(mEq);

    int choice = m_trackMenu.trackPopupMenu(m_mainHwnd, screenX, screenY, items);
    if (choice == 1001) {
        executeAction(PlayerAction::CycleAudioTrack);
    } else if (choice >= 2000) {
        int trackId = choice - 2000;
        m_engine.selectAudioTrack(trackId);
        for (const auto& t : tracks) {
            if (t.id == trackId) {
                showOsdMessage(L"Audio Track: " + rx::track_formatter::formatAudioTrackOsd(t));
                break;
            }
        }
    } else if (choice == 1010) {
        m_engine.setAudioChannelMode(AudioChannelMode::Both);
        showOsdMessage(L"Audio Mode: Both");
    } else if (choice == 1011) {
        m_engine.setAudioChannelMode(AudioChannelMode::Left);
        showOsdMessage(L"Audio Mode: Left Channel");
    } else if (choice == 1012) {
        m_engine.setAudioChannelMode(AudioChannelMode::Right);
        showOsdMessage(L"Audio Mode: Right Channel");
    } else if (choice == 1020) {
        toggleAudioPanel();
    }
}

void RXMainWindow::showSubtitleTrackMenu(int screenX, int screenY) {
    if (screenX == 0 && screenY == 0) {
        RECT rc{};
        GetWindowRect(m_btnSubtitles.getHwnd(), &rc);
        screenX = rc.left;
        screenY = rc.top;
    }

    auto subs = m_engine.getSubtitleTracks();
    int curSubId = m_engine.getSelectedSubtitleTrack();

    std::vector<rx::ui::TrackMenuItem> items;

    // Header
    rx::ui::TrackMenuItem hdr;
    hdr.id = 3001;
    hdr.text = L"Subtitle Stream";
    hdr.shortcut = L"L";
    hdr.isHeader = true;
    items.push_back(hdr);

    // Disable subtitles
    rx::ui::TrackMenuItem dis;
    dis.id = 3002;
    dis.text = L"Disable Subtitles";
    dis.isRadio = true;
    dis.isSelected = (curSubId == 0);
    items.push_back(dis);

    // Cycle
    rx::ui::TrackMenuItem cyc;
    cyc.id = 3003;
    cyc.text = L"Cycle Subtitle Stream";
    items.push_back(cyc);

    // Separator
    rx::ui::TrackMenuItem sep1;
    sep1.isSeparator = true;
    items.push_back(sep1);

    if (subs.empty()) {
        rx::ui::TrackMenuItem none;
        none.id = 0;
        none.text = L"No Subtitle Tracks Available";
        none.isEnabled = false;
        items.push_back(none);
    } else {
        for (size_t i = 0; i < subs.size(); ++i) {
            rx::ui::TrackMenuItem it;
            it.id = 4000 + subs[i].id;
            it.text = rx::track_formatter::formatSubtitleTrackMenuLabel(subs[i], static_cast<int>(i + 1));
            it.isRadio = true;
            it.isSelected = (subs[i].id == curSubId);
            items.push_back(it);
        }
    }

    // Separator
    rx::ui::TrackMenuItem sep2;
    sep2.isSeparator = true;
    items.push_back(sep2);

    // Utility actions
    rx::ui::TrackMenuItem itExt;
    itExt.id = 3010;
    itExt.text = L"Load External Subtitle...";
    items.push_back(itExt);

    rx::ui::TrackMenuItem itOnline;
    itOnline.id = 3011;
    itOnline.text = L"Download Subtitles Online...";
    itOnline.shortcut = L"Ctrl+D";
    items.push_back(itOnline);

    rx::ui::TrackMenuItem itSync;
    itSync.id = 3012;
    itSync.text = L"Subtitle Synchronization...";
    items.push_back(itSync);

    rx::ui::TrackMenuItem itApp;
    itApp.id = 3013;
    itApp.text = L"Subtitle Appearance...";
    items.push_back(itApp);

    rx::ui::TrackMenuItem itSet;
    itSet.id = 3014;
    itSet.text = L"Subtitle Settings...";
    items.push_back(itSet);

    int choice = m_trackMenu.trackPopupMenu(m_mainHwnd, screenX, screenY, items);
    if (choice == 3001 || choice == 3003) {
        executeAction(PlayerAction::CycleSubtitleTrack);
    } else if (choice == 3002) {
        m_engine.selectSubtitleTrack(0);
        showOsdMessage(L"Subtitles: Off");
    } else if (choice >= 4000) {
        int subId = choice - 4000;
        m_engine.selectSubtitleTrack(subId);
        for (const auto& s : subs) {
            if (s.id == subId) {
                showOsdMessage(L"Subtitle Track: " + rx::track_formatter::formatSubtitleTrackOsd(s));
                break;
            }
        }
    } else if (choice == 3010) {
        openSubtitleDialog();
    } else if (choice == 3011) {
        showOnlineSubtitlesDialog();
    } else if (choice == 3012 || choice == 3013 || choice == 3014) {
        openSubtitleDialog();
    }
}

void RXMainWindow::showOnlineSubtitlesDialog() {
    if (!m_onlineSubsDialog) {
        m_onlineSubsDialog = std::make_unique<rx::ui::RXOnlineSubtitlesDialog>();
        m_onlineSubsDialog->setDownloadCallback([this](const std::string& filePath, bool applyNow) {
            if (!filePath.empty() && applyNow) {
                m_engine.loadSubtitle(filePath);
                std::wstring fn = std::filesystem::path(filePath).filename().wstring();
                showOsdMessage(L"Subtitle Loaded: " + fn);
            }
        });
    }
    m_onlineSubsDialog->show(m_mainHwnd, m_engine.getMediaInfo().fullPath);
}

void RXMainWindow::toggleCompactMode() {
    m_isCompactMode = !m_isCompactMode;
    if (m_isCompactMode) {
        if (m_playlistPanel) m_playlistPanel->setVisible(false);
        if (m_videoPanel) m_videoPanel->setVisible(false);
        if (m_audioPanel) m_audioPanel->setVisible(false);
        m_btnPlaylist.setAccent(false);
        m_btnVideo.setAccent(false);
        m_btnAudio.setAccent(false);
        SetWindowPos(m_mainHwnd, nullptr, 0, 0, 520, 360, SWP_NOMOVE | SWP_NOZORDER);
        showOsdMessage(L"Compact Mini Mode");
    } else {
        SetWindowPos(m_mainHwnd, nullptr, 0, 0, 960, 580, SWP_NOMOVE | SWP_NOZORDER);
        showOsdMessage(L"Standard Window Mode");
    }
}

void RXMainWindow::toggleAlwaysOnTop() {
    m_alwaysOnTop = !m_alwaysOnTop;
    SetWindowPos(m_mainHwnd, m_alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE);
    m_titleBar.setPinned(m_alwaysOnTop);
    showOsdMessage(m_alwaysOnTop ? L"Always on Top: On" : L"Always on Top: Off");
}

void RXMainWindow::cyclePlaybackSpeed() {
    double speeds[] = { 1.0, 1.25, 1.5, 2.0, 0.5, 0.75 };
    double current = m_engine.getSpeed();
    double nextSpeed = 1.0;
    for (size_t i = 0; i < 6; ++i) {
        if (std::abs(current - speeds[i]) < 0.05) {
            nextSpeed = speeds[(i + 1) % 6];
            break;
        }
    }
    m_engine.setSpeed(nextSpeed);
    wchar_t buf[32];
    swprintf_s(buf, L"Speed: %.2fx", nextSpeed);
    showOsdMessage(buf);
}

void RXMainWindow::showMainMenu(int screenX, int screenY) {
    showContextMenu(screenX, screenY);
}

void RXMainWindow::showContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    HMENU hSubFile = CreatePopupMenu();
    HMENU hSubPlayback = CreatePopupMenu();
    HMENU hSubSpeed = CreatePopupMenu();
    HMENU hSubRepeat = CreatePopupMenu();
    HMENU hSubNav = CreatePopupMenu();
    HMENU hSubAudio = CreatePopupMenu();
    HMENU hSubVideo = CreatePopupMenu();
    HMENU hSubAspect = CreatePopupMenu();
    HMENU hSubSubs = CreatePopupMenu();
    HMENU hSubPlaylist = CreatePopupMenu();
    HMENU hSubView = CreatePopupMenu();
    HMENU hSubRecent = CreatePopupMenu();

    // 1. File
    AppendMenuW(hSubFile, MF_STRING, IDM_OPEN_FILE, L"Open File...\tCtrl+O");
    AppendMenuW(hSubFile, MF_STRING, IDM_OPEN_FOLDER, L"Open Folder...\tCtrl+F");
    AppendMenuW(hSubFile, MF_STRING, IDM_OPEN_URL, L"Open Network Stream...\tCtrl+U");

    auto recents = library::MediaDatabase::instance().getRecentHistory(8);
    if (!recents.empty()) {
        AppendMenuW(hSubFile, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < recents.size() && i < 8; ++i) {
            std::wstring itemTitle = rx::utils::utf8ToWide(recents[i].title);
            AppendMenuW(hSubRecent, MF_STRING, IDM_RECENT_BASE + (UINT)i, itemTitle.c_str());
        }
        AppendMenuW(hSubFile, MF_POPUP, (UINT_PTR)hSubRecent, L"Recent Media");
    }
    AppendMenuW(hSubFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubFile, MF_STRING, IDM_EXIT, L"Exit\tAlt+F4");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubFile, L"File");

    // 2. Playback
    bool isPlaying = (m_engine.getState() == PlaybackState::Playing);
    AppendMenuW(hSubPlayback, MF_STRING, IDM_PLAY_PAUSE, isPlaying ? L"Pause\tSpace" : L"Play\tSpace");
    AppendMenuW(hSubPlayback, MF_STRING, IDM_STOP, L"Stop\tS");
    AppendMenuW(hSubPlayback, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubPlayback, MF_STRING, IDM_PREV_TRACK, L"Previous Track\tPgUp / Ctrl+Left");
    AppendMenuW(hSubPlayback, MF_STRING, IDM_NEXT_TRACK, L"Next Track\tPgDn / Ctrl+Right");
    AppendMenuW(hSubPlayback, MF_SEPARATOR, 0, nullptr);

    // Speed submenu
    AppendMenuW(hSubSpeed, MF_STRING, IDM_SPEED_050, L"0.5x");
    AppendMenuW(hSubSpeed, MF_STRING, IDM_SPEED_075, L"0.75x");
    AppendMenuW(hSubSpeed, MF_STRING, IDM_SPEED_100, L"1.0x (Normal)");
    AppendMenuW(hSubSpeed, MF_STRING, IDM_SPEED_125, L"1.25x");
    AppendMenuW(hSubSpeed, MF_STRING, IDM_SPEED_150, L"1.5x");
    AppendMenuW(hSubSpeed, MF_STRING, IDM_SPEED_200, L"2.0x");
    AppendMenuW(hSubPlayback, MF_POPUP, (UINT_PTR)hSubSpeed, L"Playback Speed");

    // Repeat submenu
    AppendMenuW(hSubRepeat, MF_STRING, IDM_REPEAT_OFF, L"Off");
    AppendMenuW(hSubRepeat, MF_STRING, IDM_REPEAT_ALL, L"Repeat All");
    AppendMenuW(hSubRepeat, MF_STRING, IDM_REPEAT_ONE, L"Repeat Current");
    AppendMenuW(hSubPlayback, MF_POPUP, (UINT_PTR)hSubRepeat, L"Repeat Mode");
    AppendMenuW(hSubPlayback, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubPlayback, MF_STRING, IDM_MEDIA_INFO, L"Playback / System Information...\tCtrl+I");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubPlayback, L"Playback");

    // 3. Navigation
    AppendMenuW(hSubNav, MF_STRING, IDM_SEEK_FWD_5, L"Jump Forward 5s\tRight");
    AppendMenuW(hSubNav, MF_STRING, IDM_SEEK_BACK_5, L"Jump Backward 5s\tLeft");
    AppendMenuW(hSubNav, MF_STRING, IDM_SEEK_FWD_30, L"Jump Forward 30s\tShift+Right");
    AppendMenuW(hSubNav, MF_STRING, IDM_SEEK_BACK_30, L"Jump Backward 30s\tShift+Left");
    AppendMenuW(hSubNav, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubNav, MF_STRING, IDM_BOOKMARKS, L"Scene Bookmarks...\tCtrl+B");
    AppendMenuW(hSubNav, MF_STRING, IDM_ADD_BOOKMARK, L"Add Quick Bookmark\tB");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubNav, L"Navigation");

    // 4. Video
    AppendMenuW(hSubVideo, MF_STRING, IDM_VIDEO_ADJUST, L"Picture Adjustments...\tV");
    AppendMenuW(hSubAspect, MF_STRING, IDM_ASPECT_AUTO, L"Auto / Original");
    AppendMenuW(hSubAspect, MF_STRING, IDM_ASPECT_16_9, L"16:9 Widescreen");
    AppendMenuW(hSubAspect, MF_STRING, IDM_ASPECT_4_3, L"4:3 Standard");
    AppendMenuW(hSubAspect, MF_STRING, IDM_ASPECT_21_9, L"21:9 Cinemascope");
    AppendMenuW(hSubVideo, MF_POPUP, (UINT_PTR)hSubAspect, L"Aspect Ratio");
    AppendMenuW(hSubVideo, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubVideo, MF_STRING, IDM_ROTATE_90, L"Rotate 90° Clockwise");
    AppendMenuW(hSubVideo, MF_STRING, IDM_FLIP_HORZ, L"Flip Horizontal");
    AppendMenuW(hSubVideo, MF_STRING, IDM_FLIP_VERT, L"Flip Vertical");
    AppendMenuW(hSubVideo, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubVideo, MF_STRING, IDM_HWDEC_TOGGLE, (m_engine.getActiveHwdec() != "no") ? L"Disable Hardware Decoding (D3D11VA)" : L"Enable Hardware Decoding (D3D11VA)");
    AppendMenuW(hSubVideo, MF_STRING, IDM_SCREENSHOT, L"Take Screenshot\tS");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubVideo, L"Video");

    // 5. Audio
    AppendMenuW(hSubAudio, MF_STRING, IDM_AUDIO_TRACK_MENU, L"Audio Stream Selection...\tAlt+A");

    HMENU hSubAudioTracks = CreatePopupMenu();
    auto audioTracks = m_engine.getAudioTracks();
    int curAudioId = m_engine.getSelectedAudioTrack();
    if (audioTracks.empty()) {
        AppendMenuW(hSubAudioTracks, MF_GRAYED | MF_STRING, 0, L"No Audio Tracks Available");
    } else {
        for (size_t i = 0; i < audioTracks.size(); ++i) {
            std::wstring label = rx::track_formatter::formatAudioTrackMenuLabel(audioTracks[i], static_cast<int>(i + 1));
            UINT flags = MF_STRING;
            if (audioTracks[i].id == curAudioId) {
                flags |= MF_CHECKED;
            }
            AppendMenuW(hSubAudioTracks, flags, IDM_AUDIO_TRACK_BASE + audioTracks[i].id, label.c_str());
        }
    }
    AppendMenuW(hSubAudio, MF_POPUP, (UINT_PTR)hSubAudioTracks, L"Audio Streams");

    HMENU hSubChannelMode = CreatePopupMenu();
    AudioChannelMode curMode = m_engine.getAudioChannelMode();
    AppendMenuW(hSubChannelMode, MF_STRING | (curMode == AudioChannelMode::Both ? MF_CHECKED : 0), IDM_AUDIO_CHAN_BOTH, L"Both");
    AppendMenuW(hSubChannelMode, MF_STRING | (curMode == AudioChannelMode::Left ? MF_CHECKED : 0), IDM_AUDIO_CHAN_LEFT, L"Left channel");
    AppendMenuW(hSubChannelMode, MF_STRING | (curMode == AudioChannelMode::Right ? MF_CHECKED : 0), IDM_AUDIO_CHAN_RIGHT, L"Right channel");
    AppendMenuW(hSubAudio, MF_POPUP, (UINT_PTR)hSubChannelMode, L"Audio Channel");

    AppendMenuW(hSubAudio, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubAudio, MF_STRING, IDM_AUDIO_EQUALIZER, L"10-Band Equalizer & WASAPI...\tA");
    AppendMenuW(hSubAudio, MF_STRING, IDM_MUTE, m_engine.isMuted() ? L"Unmute\tM" : L"Mute\tM");
    AppendMenuW(hSubAudio, MF_STRING, IDM_VOL_UP, L"Volume Up (5%)\tUp");
    AppendMenuW(hSubAudio, MF_STRING, IDM_VOL_DOWN, L"Volume Down (5%)\tDown");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubAudio, L"Audio");

    // 6. Subtitles
    AppendMenuW(hSubSubs, MF_STRING, IDM_SUBTITLE_TRACK_MENU, L"Subtitle Stream Selection...\tL");

    HMENU hSubSubTracks = CreatePopupMenu();
    auto subTracks = m_engine.getSubtitleTracks();
    int curSubId = m_engine.getSelectedSubtitleTrack();
    AppendMenuW(hSubSubTracks, MF_STRING | (curSubId == 0 ? MF_CHECKED : 0), IDM_SUBTITLE_TRACK_BASE, L"Disable Subtitles");
    AppendMenuW(hSubSubTracks, MF_SEPARATOR, 0, nullptr);
    if (subTracks.empty()) {
        AppendMenuW(hSubSubTracks, MF_GRAYED | MF_STRING, 0, L"No Subtitle Tracks Available");
    } else {
        for (size_t i = 0; i < subTracks.size(); ++i) {
            std::wstring label = rx::track_formatter::formatSubtitleTrackMenuLabel(subTracks[i], static_cast<int>(i + 1));
            UINT flags = MF_STRING;
            if (subTracks[i].id == curSubId) {
                flags |= MF_CHECKED;
            }
            AppendMenuW(hSubSubTracks, flags, IDM_SUBTITLE_TRACK_BASE + subTracks[i].id, label.c_str());
        }
    }
    AppendMenuW(hSubSubs, MF_POPUP, (UINT_PTR)hSubSubTracks, L"Subtitle Streams");
    AppendMenuW(hSubSubs, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubSubs, MF_STRING, IDM_SUBTITLE_ONLINE, L"Download Subtitles Online...\tCtrl+D");
    AppendMenuW(hSubSubs, MF_STRING, IDM_SUBTITLE_LOAD, L"Load External Subtitle File...");
    AppendMenuW(hSubSubs, MF_STRING, IDM_SUBTITLE_SETTINGS, L"Subtitle Explorer & Sync...\tCtrl+S");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubSubs, L"Subtitles");

    // 7. Playlist
    AppendMenuW(hSubPlaylist, MF_STRING, IDM_PLAYLIST_TOGGLE, L"Toggle Playlist Panel\tP");
    AppendMenuW(hSubPlaylist, MF_STRING, IDM_PLAYLIST_SHUFFLE, L"Shuffle Playlist");
    AppendMenuW(hSubPlaylist, MF_STRING, IDM_PLAYLIST_CLEAR, L"Clear Playlist");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubPlaylist, L"Playlist");

    // 8. View
    AppendMenuW(hSubView, MF_STRING, IDM_FULLSCREEN, m_isFullscreen ? L"Exit Fullscreen\tF / Esc" : L"Enter Fullscreen\tF / F11");
    AppendMenuW(hSubView, MF_STRING, IDM_COMPACT_MODE, m_isCompactMode ? L"Standard Window Mode\tCtrl+M" : L"Compact Mini Mode\tCtrl+M");
    AppendMenuW(hSubView, MF_STRING, IDM_ALWAYS_ON_TOP, m_alwaysOnTop ? L"Always On Top [Enabled]\tCtrl+T" : L"Always On Top [Disabled]\tCtrl+T");
    AppendMenuW(hSubView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hSubView, MF_STRING, IDM_MEDIA_INFO, L"Playback / System Information...\tCtrl+I");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubView, L"View");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_PREFERENCES, L"Preferences...\tF5");
    AppendMenuW(hMenu, MF_STRING, IDM_SHORTCUT_HELP, L"Keyboard Shortcuts...\tF1");
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"About RX Media Player...");

    TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, screenX, screenY, m_mainHwnd, nullptr);
    DestroyMenu(hMenu);
}

void RXMainWindow::drawEmptyState(HDC hdc, const RECT& rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    // Deep black video canvas background #000000
    HBRUSH bgBrush = CreateSolidBrush(theme::kVideoBg);
    FillRect(memDC, &rc, bgBrush);
    DeleteObject(bgBrush);

    int cx = w / 2;
    int cy = h / 2;
    int logoY = cy - 140;

    // Concentric circular motifs
    HPEN ringPen1 = CreatePen(PS_SOLID, 1, RGB(35, 34, 18));
    HPEN ringPen2 = CreatePen(PS_SOLID, 1, RGB(55, 50, 22));
    HPEN oldPen = (HPEN)SelectObject(memDC, ringPen1);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, nullBrush);

    Ellipse(memDC, cx - 110, logoY + 48 - 110, cx + 110, logoY + 48 + 110);
    SelectObject(memDC, ringPen2);
    Ellipse(memDC, cx - 75, logoY + 48 - 75, cx + 75, logoY + 48 + 75);

    // 2. RX Brand Icon (96x96)
    if (m_hBrandIcon) {
        DrawIconEx(memDC, cx - 48, logoY, m_hBrandIcon, 96, 96, 0, nullptr, DI_NORMAL);
    }

    // 3. Brand Title & Subtitle
    SetBkMode(memDC, TRANSPARENT);
    SetTextColor(memDC, theme::kTextPrimary);
    SelectObject(memDC, m_brandFont);

    RECT rcTitle{ cx - 250, logoY + 105, cx + 250, logoY + 140 };
    DrawTextW(memDC, L"RX MEDIA PLAYER", -1, &rcTitle, DT_CENTER | DT_SINGLELINE);

    SetTextColor(memDC, theme::kAccentColor);
    SelectObject(memDC, m_brandSubFont);
    RECT rcSub{ cx - 250, logoY + 142, cx + 250, logoY + 165 };
    DrawTextW(memDC, L"Open a media file to start", -1, &rcSub, DT_CENTER | DT_SINGLELINE);

    // 4. Interactive Quick Action Buttons
    int btnY = logoY + 180;
    int btnW = 125;
    int btnH = 34;
    int spacing = 14;
    int totalBtnsW = (btnW * 3) + (spacing * 2);
    int startBtnX = cx - (totalBtnsW / 2);

    m_rcEmptyOpenFile = { startBtnX, btnY, startBtnX + btnW, btnY + btnH };
    m_rcEmptyOpenFolder = { startBtnX + btnW + spacing, btnY, startBtnX + (btnW * 2) + spacing, btnY + btnH };
    m_rcEmptyOpenUrl = { startBtnX + (btnW * 2) + (spacing * 2), btnY, startBtnX + (btnW * 3) + (spacing * 2), btnY + btnH };

    auto drawPill = [&](const RECT& r, const wchar_t* label, bool hovered) {
        HBRUSH pillBrush = CreateSolidBrush(hovered ? RGB(45, 45, 45) : theme::kButtonBg);
        HPEN pillPen = CreatePen(PS_SOLID, 1, hovered ? theme::kAccentColor : theme::kBorderColor);
        SelectObject(memDC, pillBrush);
        SelectObject(memDC, pillPen);
        RoundRect(memDC, r.left, r.top, r.right, r.bottom, 6, 6);
        DeleteObject(pillBrush);
        DeleteObject(pillPen);

        SetTextColor(memDC, hovered ? RGB(255, 255, 255) : theme::kTextPrimary);
        SelectObject(memDC, m_uiFont);
        RECT textR = r;
        DrawTextW(memDC, label, -1, &textR, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    drawPill(m_rcEmptyOpenFile, L"📁  Open File", m_hoveringOpenFile);
    drawPill(m_rcEmptyOpenFolder, L"📂  Open Folder", m_hoveringOpenFolder);
    drawPill(m_rcEmptyOpenUrl, L"🌐  Open Stream", m_hoveringOpenUrl);

    // 5. Drag & Drop Hint
    SetTextColor(memDC, theme::kTextSecondary);
    RECT rcHint{ cx - 250, btnY + 45, cx + 250, btnY + 65 };
    DrawTextW(memDC, L"or drag & drop media files anywhere to start playback", -1, &rcHint, DT_CENTER | DT_SINGLELINE);

    // 6. Recent Media from SQLite Database
    auto recents = library::MediaDatabase::instance().getRecentHistory(4);
    m_recentItemRects.clear();

    if (!recents.empty() && h > 480) {
        int recY = btnY + 75;

        HPEN sepPen = CreatePen(PS_SOLID, 1, theme::kBorderColor);
        SelectObject(memDC, sepPen);
        MoveToEx(memDC, cx - 180, recY, nullptr);
        LineTo(memDC, cx + 180, recY);
        DeleteObject(sepPen);

        recY += 12;
        SetTextColor(memDC, theme::kTextSecondary);
        SelectObject(memDC, m_uiFont);
        RECT rcRecHeader{ cx - 180, recY, cx + 180, recY + 20 };
        DrawTextW(memDC, L"Recent Media:", -1, &rcRecHeader, DT_LEFT | DT_SINGLELINE);

        recY += 24;
        SelectObject(memDC, m_recentFont);

        for (size_t i = 0; i < recents.size() && i < 4; ++i) {
            RECT itemR{ cx - 180, recY, cx + 180, recY + 24 };
            m_recentItemRects.push_back({ itemR, recents[i].filePath });

            bool isHov = ((int)i == m_hoveredRecentIdx);
            if (isHov) {
                HBRUSH itemBg = CreateSolidBrush(theme::kHoverBg);
                FillRect(memDC, &itemR, itemBg);
                DeleteObject(itemBg);
            }

            SetTextColor(memDC, isHov ? theme::kAccentColor : theme::kTextPrimary);
            std::wstring line = L"▶  " + rx::utils::utf8ToWide(recents[i].title);
            RECT textR{ itemR.left + 8, itemR.top, itemR.right - 8, itemR.bottom };
            DrawTextW(memDC, line.c_str(), -1, &textR, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            recY += 26;
        }
    }

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(ringPen1);
    DeleteObject(ringPen2);

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
}

void RXMainWindow::toggleMute() {
    bool mute = !m_engine.isMuted();
    m_engine.setMute(mute);
    m_timelineStrip.setVolume(m_engine.getVolume(), mute);
    if (mute) {
        showOsdMessage(L"Muted");
    } else {
        showOsdMessage(L"Volume: " + std::to_wstring(m_engine.getVolume()) + L"%");
    }
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
    m_engine.loadFile(paths[0], true);
}

void RXMainWindow::onKeyDown(WPARAM key) {
    // Suppress shortcuts when an edit box or text field has focus
    HWND focused = GetFocus();
    if (focused) {
        wchar_t clsName[64] = {0};
        GetClassNameW(focused, clsName, 64);
        if (_wcsicmp(clsName, L"EDIT") == 0 || _wcsicmp(clsName, L"RichEdit") == 0 || _wcsicmp(clsName, L"RICHEDIT50W") == 0) {
            return;
        }
    }

    if (key == VK_ESCAPE) {
        if (m_isFullscreen) {
            toggleFullscreen();
            return;
        }
    }

    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;

    PlayerAction action = ShortcutManager::instance().findAction(static_cast<WORD>(key), ctrl, shift, alt);
    if (action != PlayerAction::None) {
        executeAction(action);
        return;
    }

    // Fallback for non-configurable legacy keys
    switch (key) {
        case VK_OEM_4: // [
            m_engine.setSubtitleDelay(m_engine.getSubtitleDelay() - 0.1);
            showOsdMessage(L"Subtitle Delay: -100 ms");
            break;
        case VK_OEM_6: // ]
            m_engine.setSubtitleDelay(m_engine.getSubtitleDelay() + 0.1);
            showOsdMessage(L"Subtitle Delay: +100 ms");
            break;
        case VK_OEM_COMMA:
            if (ctrl) showPreferencesDialog();
            else m_engine.stepBackward();
            break;
        case VK_OEM_PERIOD:
            m_engine.stepForward();
            break;
        case 'B':
            if (ctrl) openBookmarkDialog();
            else addQuickBookmark();
            break;
        case 'U':
            if (ctrl) openUrlDialog();
            break;
        case 'V':
            toggleVideoPanel();
            break;
        case VK_F11:
            toggleFullscreen();
            break;
    }
}

void RXMainWindow::executeAction(PlayerAction action) {
    switch (action) {
        case PlayerAction::PlayPause: {
            if (m_engine.getState() == PlaybackState::Playing) {
                m_engine.pause();
                m_btnPlay.setText(L"►");
                showOsdMessage(L"Paused");
            } else {
                m_engine.play();
                m_btnPlay.setText(L"⏸");
                showOsdMessage(L"Playing");
            }
            break;
        }
        case PlayerAction::Stop: {
            m_engine.stop();
            m_btnPlay.setText(L"►");
            m_titleBar.setMediaInfo(L"", L"RX Media Player");
            if (m_videoHwnd) InvalidateRect(m_videoHwnd, nullptr, TRUE);
            showOsdMessage(L"Stopped");
            break;
        }
        case PlayerAction::SeekBeginning: {
            m_engine.seek(0.0, true);
            showOsdMessage(L"Beginning");
            break;
        }
        case PlayerAction::SeekEnd: {
            double dur = m_engine.getDuration();
            if (dur > 1.0) m_engine.seek(dur - 0.5, true);
            showOsdMessage(L"End of Video");
            break;
        }
        case PlayerAction::SeekBackward5: {
            m_engine.seekRelative(-5.0);
            showOsdMessage(L"Backward 5 seconds");
            break;
        }
        case PlayerAction::SeekForward5: {
            m_engine.seekRelative(5.0);
            showOsdMessage(L"Forward 5 seconds");
            break;
        }
        case PlayerAction::SeekBackward30: {
            m_engine.seekRelative(-30.0);
            showOsdMessage(L"Backward 30 seconds");
            break;
        }
        case PlayerAction::SeekForward30: {
            m_engine.seekRelative(30.0);
            showOsdMessage(L"Forward 30 seconds");
            break;
        }
        case PlayerAction::SeekBackward60: {
            m_engine.seekRelative(-60.0);
            showOsdMessage(L"Backward 60 seconds");
            break;
        }
        case PlayerAction::SeekForward60: {
            m_engine.seekRelative(60.0);
            showOsdMessage(L"Forward 60 seconds");
            break;
        }
        case PlayerAction::PreviousFile: {
            auto prevItem = m_playlistMgr.previous();
            if (prevItem) {
                m_engine.loadFile(prevItem->path, true);
                if (m_playlistPanel) m_playlistPanel->refresh();
                showOsdMessage(L"Previous: " + rx::utils::utf8ToWide(prevItem->title));
            }
            break;
        }
        case PlayerAction::NextFile:
        case PlayerAction::NextPlaylistItem: {
            auto nextItem = m_playlistMgr.next(true);
            if (nextItem) {
                m_engine.loadFile(nextItem->path, true);
                if (m_playlistPanel) m_playlistPanel->refresh();
                showOsdMessage(L"Next: " + rx::utils::utf8ToWide(nextItem->title));
            }
            break;
        }
        case PlayerAction::TogglePlaylist: {
            togglePlaylistPanel();
            showOsdMessage((m_playlistPanel && m_playlistPanel->isVisible()) ? L"Playlist Opened" : L"Playlist Closed");
            break;
        }
        case PlayerAction::ToggleRepeat: {
            auto mode = m_playlistMgr.getRepeatMode();
            if (mode == RepeatMode::Off) {
                m_playlistMgr.setRepeatMode(RepeatMode::RepeatAll);
                showOsdMessage(L"Repeat: On");
            } else if (mode == RepeatMode::RepeatAll) {
                m_playlistMgr.setRepeatMode(RepeatMode::RepeatOne);
                showOsdMessage(L"Repeat: Single Track");
            } else {
                m_playlistMgr.setRepeatMode(RepeatMode::Off);
                showOsdMessage(L"Repeat: Off");
            }
            break;
        }
        case PlayerAction::ToggleShuffle: {
            m_playlistMgr.shuffle();
            if (m_playlistPanel) m_playlistPanel->refresh();
            static bool s_shuffled = false;
            s_shuffled = !s_shuffled;
            showOsdMessage(s_shuffled ? L"Shuffle: On" : L"Shuffle: Off");
            break;
        }
        case PlayerAction::VolumeUp5: {
            int vol = rx::utils::clamp(m_engine.getVolume() + 5, 0, 100);
            m_engine.setVolume(vol);
            Settings::instance().audio.volume = vol;
            m_timelineStrip.setVolume(vol, m_engine.isMuted());
            showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            break;
        }
        case PlayerAction::VolumeDown5: {
            int vol = rx::utils::clamp(m_engine.getVolume() - 5, 0, 100);
            m_engine.setVolume(vol);
            Settings::instance().audio.volume = vol;
            m_timelineStrip.setVolume(vol, m_engine.isMuted());
            showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            break;
        }
        case PlayerAction::VolumeUp1: {
            int vol = rx::utils::clamp(m_engine.getVolume() + 1, 0, 100);
            m_engine.setVolume(vol);
            Settings::instance().audio.volume = vol;
            m_timelineStrip.setVolume(vol, m_engine.isMuted());
            showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            break;
        }
        case PlayerAction::VolumeDown1: {
            int vol = rx::utils::clamp(m_engine.getVolume() - 1, 0, 100);
            m_engine.setVolume(vol);
            Settings::instance().audio.volume = vol;
            m_timelineStrip.setVolume(vol, m_engine.isMuted());
            showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            break;
        }
        case PlayerAction::MuteToggle: {
            toggleMute();
            break;
        }
        case PlayerAction::SpeedDown: {
            double spd = m_engine.getSpeed() - 0.10;
            if (spd < 0.25) spd = 0.25;
            m_engine.setSpeed(spd);
            wchar_t buf[64];
            if (std::abs(spd - 1.0) < 0.01) {
                swprintf_s(buf, L"Playback Speed: Normal (1.00x)");
            } else {
                swprintf_s(buf, L"Playback Speed: %.2fx", spd);
            }
            showOsdMessage(buf);
            break;
        }
        case PlayerAction::SpeedUp: {
            double spd = m_engine.getSpeed() + 0.10;
            if (spd > 4.0) spd = 4.0;
            m_engine.setSpeed(spd);
            wchar_t buf[64];
            if (std::abs(spd - 1.0) < 0.01) {
                swprintf_s(buf, L"Playback Speed: Normal (1.00x)");
            } else {
                swprintf_s(buf, L"Playback Speed: %.2fx", spd);
            }
            showOsdMessage(buf);
            break;
        }
        case PlayerAction::SpeedReset: {
            m_engine.setSpeed(1.0);
            showOsdMessage(L"Playback Speed: Normal (1.00x)");
            break;
        }
        case PlayerAction::ToggleFullscreen: {
            toggleFullscreen();
            break;
        }
        case PlayerAction::ToggleAlwaysOnTop: {
            toggleAlwaysOnTop();
            break;
        }
        case PlayerAction::ZoomReset: {
            m_engine.setZoom(1.0);
            m_engine.setPan(0, 0);
            showOsdMessage(L"Zoom Reset");
            break;
        }
        case PlayerAction::ZoomIn: {
            double curZoom = m_engine.getVideoAdjustments().zoom;
            curZoom += 0.10;
            if (curZoom > 4.0) curZoom = 4.0;
            m_engine.setZoom(curZoom);
            wchar_t buf[64];
            swprintf_s(buf, L"Zoom: %d%%", static_cast<int>(std::round(curZoom * 100.0)));
            showOsdMessage(buf);
            break;
        }
        case PlayerAction::ZoomOut: {
            double curZoom = m_engine.getVideoAdjustments().zoom;
            curZoom -= 0.10;
            if (curZoom < 0.20) curZoom = 0.20;
            m_engine.setZoom(curZoom);
            wchar_t buf[64];
            swprintf_s(buf, L"Zoom: %d%%", static_cast<int>(std::round(curZoom * 100.0)));
            showOsdMessage(buf);
            break;
        }
        case PlayerAction::Rotate90: {
            int rot = (m_engine.getVideoAdjustments().rotate + 90) % 360;
            m_engine.setVideoRotate(rot);
            showOsdMessage(L"Rotation: " + std::to_wstring(rot) + L"°");
            break;
        }
        case PlayerAction::OpenFile: {
            openFileDialog();
            break;
        }
        case PlayerAction::OpenFolder: {
            openFolderDialog();
            break;
        }
        case PlayerAction::CycleAudioTrack: {
            std::string name = m_engine.cycleAudioTrack();
            if (name.empty()) {
                showOsdMessage(L"No Audio Tracks Available");
            } else if (name == "SINGLE_TRACK") {
                auto tracks = m_engine.getAudioTracks();
                if (!tracks.empty()) {
                    showOsdMessage(L"Audio Track: " + rx::track_formatter::formatAudioTrackOsd(tracks[0]));
                } else {
                    showOsdMessage(L"No Audio Tracks Available");
                }
            } else {
                showOsdMessage(L"Audio Track: " + rx::utils::utf8ToWide(name));
            }
            break;
        }
        case PlayerAction::OpenAudioMenu: {
            showAudioTrackMenu(0, 0);
            break;
        }
        case PlayerAction::AudioChannelBoth: {
            m_engine.setAudioChannelMode(AudioChannelMode::Both);
            showOsdMessage(L"Audio Mode: Both");
            break;
        }
        case PlayerAction::AudioChannelLeft: {
            m_engine.setAudioChannelMode(AudioChannelMode::Left);
            showOsdMessage(L"Audio Mode: Left Channel");
            break;
        }
        case PlayerAction::AudioChannelRight: {
            m_engine.setAudioChannelMode(AudioChannelMode::Right);
            showOsdMessage(L"Audio Mode: Right Channel");
            break;
        }
        case PlayerAction::CycleSubtitleTrack: {
            std::string name = m_engine.cycleSubtitleTrack();
            if (name.empty()) {
                showOsdMessage(L"No Subtitles Available");
            } else if (name == "OFF") {
                showOsdMessage(L"Subtitles: Off");
            } else {
                showOsdMessage(L"Subtitle Track: " + rx::utils::utf8ToWide(name));
            }
            break;
        }
        case PlayerAction::ToggleSubtitles: {
            int cur = m_engine.getSelectedSubtitleTrack();
            if (cur > 0 && m_engine.isSubtitlesVisible()) {
                m_engine.selectSubtitleTrack(0);
                showOsdMessage(L"Subtitles: Off");
            } else {
                auto tracks = m_engine.getSubtitleTracks();
                if (tracks.empty()) {
                    showOsdMessage(L"No Subtitles Available");
                } else {
                    m_engine.selectSubtitleTrack(tracks[0].id);
                    showOsdMessage(L"Subtitle Track: " + rx::track_formatter::formatSubtitleTrackOsd(tracks[0]));
                }
            }
            break;
        }
        case PlayerAction::OpenSubtitleMenu: {
            showSubtitleTrackMenu(0, 0);
            break;
        }
        case PlayerAction::DownloadOnlineSubtitles: {
            showOnlineSubtitlesDialog();
            break;
        }
        case PlayerAction::SubtitleDelayDown: {
            double delay = m_engine.getSubtitleDelay() - 0.10;
            m_engine.setSubtitleDelay(delay);
            int delayMs = static_cast<int>(std::round(delay * 1000.0));
            wchar_t buf[64];
            swprintf_s(buf, L"Subtitle Delay: %+d ms", delayMs);
            showOsdMessage(buf);
            break;
        }
        case PlayerAction::SubtitleDelayUp: {
            double delay = m_engine.getSubtitleDelay() + 0.10;
            m_engine.setSubtitleDelay(delay);
            int delayMs = static_cast<int>(std::round(delay * 1000.0));
            wchar_t buf[64];
            swprintf_s(buf, L"Subtitle Delay: %+d ms", delayMs);
            showOsdMessage(buf);
            break;
        }
        case PlayerAction::LoadSubtitles: {
            openSubtitleDialog();
            break;
        }
        case PlayerAction::ShowDetailedInfo: {
            m_engine.toggleOsdStats();
            break;
        }
        case PlayerAction::ShowBriefInfo: {
            double pos = m_engine.getCurrentPosition();
            double dur = m_engine.getDuration();
            std::string brief = rx::utils::formatTime(pos) + " / " + rx::utils::formatTime(dur);
            showOsdMessage(rx::utils::utf8ToWide(brief));
            break;
        }
        case PlayerAction::OpenPlaybackSystemInfo:
        case PlayerAction::ShowFileInfo: {
            showInfoDialog();
            break;
        }
        case PlayerAction::OpenPreferences: {
            showPreferencesDialog();
            break;
        }
        case PlayerAction::CaptureScreenshot: {
            takeScreenshot();
            break;
        }
        case PlayerAction::OpenShortcutHelp: {
            showShortcutHelpDialog();
            break;
        }
        default:
            break;
    }
}

void RXMainWindow::startAutoHideTimer() {
    m_autoHideTimerId = SetTimer(m_mainHwnd, TIMER_AUTOHIDE, 2500, nullptr);
}

void RXMainWindow::resetAutoHide() {
    if (m_controlsHidden) {
        m_controlsHidden = false;
        m_titleBar.move(0, 0, 0, 0); // Trigger resize update
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
            case WM_NCCALCSIZE: {
                if (wParam) {
                    NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    if (IsZoomed(hwnd)) {
                        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
                        MONITORINFO mi{};
                        mi.cbSize = sizeof(MONITORINFO);
                        if (GetMonitorInfoW(hMon, &mi)) {
                            params->rgrc[0] = mi.rcWork;
                        }
                    }
                    return 0; // Seamless borderless custom titlebar
                }
                return 0;
            }
            case WM_NCHITTEST: {
                LRESULT hit = DefWindowProcW(hwnd, msg, wParam, lParam);
                if (!pThis->m_isFullscreen && !IsZoomed(hwnd)) {
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    ScreenToClient(hwnd, &pt);
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    int border = 6;
                    if (pt.y < border && pt.x < border) return HTTOPLEFT;
                    if (pt.y < border && pt.x >= rc.right - border) return HTTOPRIGHT;
                    if (pt.y >= rc.bottom - border && pt.x < border) return HTBOTTOMLEFT;
                    if (pt.y >= rc.bottom - border && pt.x >= rc.right - border) return HTBOTTOMRIGHT;
                    if (pt.y < border) return HTTOP;
                    if (pt.y >= rc.bottom - border) return HTBOTTOM;
                    if (pt.x < border) return HTLEFT;
                    if (pt.x >= rc.right - border) return HTRIGHT;
                }
                return hit;
            }
            case WM_SIZE: {
                pThis->m_titleBar.setMaximized(wParam == SIZE_MAXIMIZED);
                pThis->onResize(LOWORD(lParam), HIWORD(lParam));
                return 0;
            }
            case WM_DROPFILES:
                pThis->onDropFiles(reinterpret_cast<HDROP>(wParam));
                return 0;
            case WM_KEYDOWN:
                pThis->onKeyDown(wParam);
                return 0;
            case WM_COMMAND: {
                WORD id = LOWORD(wParam);
                if (id >= IDM_RECENT_BASE && id < IDM_RECENT_BASE + 20) {
                    auto recents = library::MediaDatabase::instance().getRecentHistory(8);
                    size_t idx = id - IDM_RECENT_BASE;
                    if (idx < recents.size()) {
                        pThis->openFile(recents[idx].filePath);
                    }
                    return 0;
                }

                if (id >= IDM_AUDIO_TRACK_BASE && id < IDM_SUBTITLE_TRACK_BASE) {
                    int trackId = id - IDM_AUDIO_TRACK_BASE;
                    pThis->m_engine.selectAudioTrack(trackId);
                    auto tracks = pThis->m_engine.getAudioTracks();
                    for (const auto& t : tracks) {
                        if (t.id == trackId) {
                            pThis->showOsdMessage(L"Audio Track: " + rx::track_formatter::formatAudioTrackOsd(t));
                            break;
                        }
                    }
                    return 0;
                }
                if (id >= IDM_SUBTITLE_TRACK_BASE && id < IDM_SUBTITLE_TRACK_BASE + 200) {
                    int trackId = id - IDM_SUBTITLE_TRACK_BASE;
                    if (trackId == 0) {
                        pThis->m_engine.selectSubtitleTrack(0);
                        pThis->showOsdMessage(L"Subtitles: Off");
                    } else {
                        pThis->m_engine.selectSubtitleTrack(trackId);
                        auto subs = pThis->m_engine.getSubtitleTracks();
                        for (const auto& s : subs) {
                            if (s.id == trackId) {
                                pThis->showOsdMessage(L"Subtitle Track: " + rx::track_formatter::formatSubtitleTrackOsd(s));
                                break;
                            }
                        }
                    }
                    return 0;
                }

                switch (id) {
                    case IDM_OPEN_FILE:         pThis->openFileDialog(); break;
                    case IDM_OPEN_FOLDER:       pThis->openFolderDialog(); break;
                    case IDM_OPEN_URL:          pThis->openUrlDialog(); break;
                    case IDM_EXIT:              PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
                    case IDM_PLAY_PAUSE:        pThis->m_engine.togglePause(); break;
                    case IDM_STOP:              pThis->m_engine.stop(); pThis->m_btnPlay.setText(L"►"); if (pThis->m_videoHwnd) InvalidateRect(pThis->m_videoHwnd, nullptr, TRUE); break;
                    case IDM_PREV_TRACK: {
                        auto prevItem = pThis->m_playlistMgr.previous();
                        if (prevItem) {
                            pThis->m_engine.loadFile(prevItem->path, true);
                            pThis->m_playlistPanel->refresh();
                        }
                        break;
                    }
                    case IDM_NEXT_TRACK: {
                        auto nextItem = pThis->m_playlistMgr.next(true);
                        if (nextItem) {
                            pThis->m_engine.loadFile(nextItem->path, true);
                            pThis->m_playlistPanel->refresh();
                        }
                        break;
                    }
                    case IDM_SPEED_050:         pThis->m_engine.setSpeed(0.5); pThis->showOsdMessage(L"Speed: 0.5x"); break;
                    case IDM_SPEED_075:         pThis->m_engine.setSpeed(0.75); pThis->showOsdMessage(L"Speed: 0.75x"); break;
                    case IDM_SPEED_100:         pThis->m_engine.setSpeed(1.0); pThis->showOsdMessage(L"Speed: 1.0x (Normal)"); break;
                    case IDM_SPEED_125:         pThis->m_engine.setSpeed(1.25); pThis->showOsdMessage(L"Speed: 1.25x"); break;
                    case IDM_SPEED_150:         pThis->m_engine.setSpeed(1.5); pThis->showOsdMessage(L"Speed: 1.5x"); break;
                    case IDM_SPEED_200:         pThis->m_engine.setSpeed(2.0); pThis->showOsdMessage(L"Speed: 2.0x"); break;
                    case IDM_REPEAT_OFF:        pThis->m_playlistMgr.setRepeatMode(RepeatMode::Off); pThis->showOsdMessage(L"Repeat: Off"); break;
                    case IDM_REPEAT_ALL:        pThis->m_playlistMgr.setRepeatMode(RepeatMode::RepeatAll); pThis->showOsdMessage(L"Repeat: All"); break;
                    case IDM_REPEAT_ONE:        pThis->m_playlistMgr.setRepeatMode(RepeatMode::RepeatOne); pThis->showOsdMessage(L"Repeat: Single"); break;
                    case IDM_SEEK_FWD_5:        pThis->m_engine.seekRelative(5.0); break;
                    case IDM_SEEK_BACK_5:       pThis->m_engine.seekRelative(-5.0); break;
                    case IDM_SEEK_FWD_30:       pThis->m_engine.seekRelative(30.0); break;
                    case IDM_SEEK_BACK_30:      pThis->m_engine.seekRelative(-30.0); break;
                    case IDM_BOOKMARKS:         pThis->openBookmarkDialog(); break;
                    case IDM_ADD_BOOKMARK:      pThis->addQuickBookmark(); break;
                    case IDM_AUDIO_TRACK_MENU:  pThis->showAudioTrackMenu(0, 0); break;
                    case IDM_AUDIO_CHAN_BOTH:
                        pThis->m_engine.setAudioChannelMode(AudioChannelMode::Both);
                        pThis->showOsdMessage(L"Audio Mode: Both");
                        break;
                    case IDM_AUDIO_CHAN_LEFT:
                        pThis->m_engine.setAudioChannelMode(AudioChannelMode::Left);
                        pThis->showOsdMessage(L"Audio Mode: Left Channel");
                        break;
                    case IDM_AUDIO_CHAN_RIGHT:
                        pThis->m_engine.setAudioChannelMode(AudioChannelMode::Right);
                        pThis->showOsdMessage(L"Audio Mode: Right Channel");
                        break;
                    case IDM_AUDIO_EQUALIZER:   pThis->toggleAudioPanel(); break;
                    case IDM_MUTE:              pThis->toggleMute(); break;
                    case IDM_VOL_UP: {
                        int vol = rx::utils::clamp(pThis->m_engine.getVolume() + 5, 0, 100);
                        pThis->m_engine.setVolume(vol);
                        pThis->m_timelineStrip.setVolume(vol, pThis->m_engine.isMuted());
                        pThis->showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
                        break;
                    }
                    case IDM_VOL_DOWN: {
                        int vol = rx::utils::clamp(pThis->m_engine.getVolume() - 5, 0, 100);
                        pThis->m_engine.setVolume(vol);
                        pThis->m_timelineStrip.setVolume(vol, pThis->m_engine.isMuted());
                        pThis->showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
                        break;
                    }
                    case IDM_VIDEO_ADJUST:      pThis->toggleVideoPanel(); break;
                    case IDM_ASPECT_AUTO:       pThis->m_engine.setAspectRatio("-1"); pThis->showOsdMessage(L"Aspect Ratio: Auto"); break;
                    case IDM_ASPECT_16_9:       pThis->m_engine.setAspectRatio("16:9"); pThis->showOsdMessage(L"Aspect Ratio: 16:9"); break;
                    case IDM_ASPECT_4_3:        pThis->m_engine.setAspectRatio("4:3"); pThis->showOsdMessage(L"Aspect Ratio: 4:3"); break;
                    case IDM_ASPECT_21_9:       pThis->m_engine.setAspectRatio("21:9"); pThis->showOsdMessage(L"Aspect Ratio: 21:9"); break;
                    case IDM_ROTATE_90: {
                        int curRot = pThis->m_engine.getVideoAdjustments().rotate;
                        int nextRot = (curRot + 90) % 360;
                        pThis->m_engine.setVideoRotate(nextRot);
                        pThis->showOsdMessage(L"Rotation: " + std::to_wstring(nextRot) + L"°");
                        break;
                    }
                    case IDM_FLIP_HORZ: {
                        bool f = !pThis->m_engine.getVideoAdjustments().flipHorizontal;
                        pThis->m_engine.setFlip(f, pThis->m_engine.getVideoAdjustments().flipVertical);
                        pThis->showOsdMessage(f ? L"Flip Horizontal: ON" : L"Flip Horizontal: OFF");
                        break;
                    }
                    case IDM_FLIP_VERT: {
                        bool v = !pThis->m_engine.getVideoAdjustments().flipVertical;
                        pThis->m_engine.setFlip(pThis->m_engine.getVideoAdjustments().flipHorizontal, v);
                        pThis->showOsdMessage(v ? L"Flip Vertical: ON" : L"Flip Vertical: OFF");
                        break;
                    }
                    case IDM_HWDEC_TOGGLE: {
                        bool en = (pThis->m_engine.getActiveHwdec() == "no");
                        pThis->m_engine.setHardwareAcceleration(en, "auto-safe");
                        pThis->showOsdMessage(en ? L"Hardware Acceleration: Enabled" : L"Hardware Acceleration: Disabled");
                        if (pThis->m_controlBarHwnd) InvalidateRect(pThis->m_controlBarHwnd, nullptr, FALSE);
                        break;
                    }
                    case IDM_SCREENSHOT:        pThis->takeScreenshot(); break;
                    case IDM_SUBTITLE_TRACK_MENU: pThis->showSubtitleTrackMenu(0, 0); break;
                    case IDM_SUBTITLE_ONLINE:     pThis->showOnlineSubtitlesDialog(); break;
                    case IDM_SUBTITLE_SETTINGS: pThis->openSubtitleDialog(); break;
                    case IDM_SUBTITLE_LOAD:     pThis->openSubtitleDialog(); break;
                    case IDM_PLAYLIST_TOGGLE:   pThis->togglePlaylistPanel(); break;
                    case IDM_PLAYLIST_SHUFFLE:  pThis->m_playlistMgr.shuffle(); pThis->m_playlistPanel->refresh(); pThis->showOsdMessage(L"Playlist Shuffled"); break;
                    case IDM_PLAYLIST_CLEAR:    pThis->m_playlistMgr.clear(); pThis->m_playlistPanel->refresh(); pThis->showOsdMessage(L"Playlist Cleared"); break;
                    case IDM_FULLSCREEN:        pThis->toggleFullscreen(); break;
                    case IDM_COMPACT_MODE:      pThis->toggleCompactMode(); break;
                    case IDM_ALWAYS_ON_TOP:     pThis->toggleAlwaysOnTop(); break;
                    case IDM_MEDIA_INFO:        pThis->showInfoDialog(); break;
                    case IDM_PREFERENCES:       pThis->showPreferencesDialog(); break;
                    case IDM_SHORTCUT_HELP:     pThis->showShortcutHelpDialog(); break;
                    case IDM_ABOUT:             pThis->showAboutDialog(); break;
                }
                return 0;
            }
            case WM_TIMER: {
                if (wParam == TIMER_AUTOHIDE) {
                    if (pThis->m_isFullscreen && pThis->m_engine.getState() == PlaybackState::Playing) {
                        pThis->m_controlsHidden = true;
                        RECT rc;
                        GetClientRect(hwnd, &rc);
                        pThis->onResize(rc.right - rc.left, rc.bottom - rc.top);
                    }
                } else if (wParam == TIMER_OSD) {
                    pThis->m_osdMessage.clear();
                    KillTimer(hwnd, TIMER_OSD);
                    pThis->m_osdTimerId = 0;
                    if (pThis->m_videoHwnd) InvalidateRect(pThis->m_videoHwnd, nullptr, FALSE);
                }
                return 0;
            }
            case WM_DESTROY: {
                auto& s = Settings::instance();
                RECT wr;
                GetWindowRect(hwnd, &wr);
                s.window.x = wr.left;
                s.window.y = wr.top;
                s.window.width = wr.right - wr.left;
                s.window.height = wr.bottom - wr.top;
                s.window.maximized = IsZoomed(hwnd);
                ShortcutManager::instance().saveToSettings(s);
                s.save();

                PostQuitMessage(0);
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK RXMainWindow::videoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RXMainWindow* pThis = reinterpret_cast<RXMainWindow*>(GetWindowLongPtrW(GetParent(hwnd), GWLP_USERDATA));

    if (pThis) {
        switch (msg) {
            case WM_LBUTTONDBLCLK:
                pThis->toggleFullscreen();
                return 0;
            case WM_RBUTTONUP: {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                pThis->showContextMenu(pt.x, pt.y);
                return 0;
            }
            case WM_LBUTTONDOWN: {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                // Handle interactive empty state clicks
                if (pThis->m_engine.getState() == PlaybackState::Stopped || pThis->m_engine.getState() == PlaybackState::Idle) {
                    if (PtInRect(&pThis->m_rcEmptyOpenFile, pt)) {
                        pThis->openFileDialog();
                        return 0;
                    }
                    if (PtInRect(&pThis->m_rcEmptyOpenFolder, pt)) {
                        pThis->openFolderDialog();
                        return 0;
                    }
                    if (PtInRect(&pThis->m_rcEmptyOpenUrl, pt)) {
                        pThis->openUrlDialog();
                        return 0;
                    }

                    for (const auto& item : pThis->m_recentItemRects) {
                        if (PtInRect(&item.first, pt)) {
                            pThis->openFile(item.second);
                            return 0;
                        }
                    }
                }
                pThis->resetAutoHide();
                return 0;
            }
            case WM_MOUSEMOVE: {
                pThis->resetAutoHide();

                if (pThis->m_engine.getState() == PlaybackState::Stopped || pThis->m_engine.getState() == PlaybackState::Idle) {
                    POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    bool hovFile = PtInRect(&pThis->m_rcEmptyOpenFile, pt);
                    bool hovFolder = PtInRect(&pThis->m_rcEmptyOpenFolder, pt);
                    bool hovUrl = PtInRect(&pThis->m_rcEmptyOpenUrl, pt);

                    int hovRecent = -1;
                    for (size_t i = 0; i < pThis->m_recentItemRects.size(); ++i) {
                        if (PtInRect(&pThis->m_recentItemRects[i].first, pt)) {
                            hovRecent = (int)i;
                            break;
                        }
                    }

                    bool needRedraw = (hovFile != pThis->m_hoveringOpenFile) ||
                                      (hovFolder != pThis->m_hoveringOpenFolder) ||
                                      (hovUrl != pThis->m_hoveringOpenUrl) ||
                                      (hovRecent != pThis->m_hoveredRecentIdx);

                    pThis->m_hoveringOpenFile = hovFile;
                    pThis->m_hoveringOpenFolder = hovFolder;
                    pThis->m_hoveringOpenUrl = hovUrl;
                    pThis->m_hoveredRecentIdx = hovRecent;

                    if (hovFile || hovFolder || hovUrl || hovRecent >= 0) {
                        SetCursor(LoadCursor(nullptr, IDC_HAND));
                    }

                    if (needRedraw) {
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                break;
            }
            case WM_MOUSEWHEEL: {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int change = (delta > 0) ? 5 : -5;
                int vol = rx::utils::clamp(pThis->m_engine.getVolume() + change, 0, 100);
                pThis->m_engine.setVolume(vol);
                pThis->m_timelineStrip.setVolume(vol, pThis->m_engine.isMuted());
                pThis->showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
                return 0;
            }
            case WM_DROPFILES:
                pThis->onDropFiles(reinterpret_cast<HDROP>(wParam));
                return 0;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                if (pThis->m_engine.getState() == PlaybackState::Stopped || pThis->m_engine.getState() == PlaybackState::Idle) {
                    RECT rc;
                    GetClientRect(hwnd, &rc);
                    pThis->drawEmptyState(hdc, rc);
                }

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
        case WM_SETCURSOR: {
            if (pThis) {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                if (PtInRect(&pThis->m_rcHwdecBadge, pt) ||
                    PtInRect(&pThis->m_rcVideoCodecBadge, pt) ||
                    PtInRect(&pThis->m_rcAudioCodecBadge, pt) ||
                    PtInRect(&pThis->m_rcChannelsBadge, pt)) {
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            if (pThis) {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&pThis->m_rcHwdecBadge, pt)) {
                    bool en = (pThis->m_engine.getActiveHwdec() == "no");
                    pThis->m_engine.setHardwareAcceleration(en, "auto-safe");
                    pThis->showOsdMessage(en ? L"Hardware Acceleration: Enabled" : L"Hardware Acceleration: Disabled");
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&pThis->m_rcVideoCodecBadge, pt)) {
                    const auto& mi = pThis->m_engine.getMediaInfo();
                    std::wstring msg = L"Video: " + rx::utils::utf8ToWide(mi.videoCodec);
                    if (mi.videoWidth > 0 && mi.videoHeight > 0) {
                        msg += L" (" + std::to_wstring(mi.videoWidth) + L"x" + std::to_wstring(mi.videoHeight) + L")";
                    }
                    if (mi.videoFps > 0.0) {
                        wchar_t fpsBuf[32];
                        swprintf_s(fpsBuf, L" @ %.2f fps", mi.videoFps);
                        msg += fpsBuf;
                    }
                    pThis->showOsdMessage(msg);
                    return 0;
                }
                if (PtInRect(&pThis->m_rcAudioCodecBadge, pt)) {
                    const auto& mi = pThis->m_engine.getMediaInfo();
                    std::wstring msg = L"Audio: " + rx::utils::utf8ToWide(mi.audioCodec);
                    if (mi.audioSampleRate > 0) {
                        msg += L" (" + std::to_wstring(mi.audioSampleRate / 1000) + L"." +
                               std::to_wstring((mi.audioSampleRate % 1000) / 100) + L" kHz)";
                    }
                    pThis->showOsdMessage(msg);
                    return 0;
                }
                if (PtInRect(&pThis->m_rcChannelsBadge, pt)) {
                    const auto& mi = pThis->m_engine.getMediaInfo();
                    std::wstring chDesc = (mi.audioChannels == 6) ? L"5.1 Surround" :
                                         (mi.audioChannels == 8) ? L"7.1 Surround" :
                                         (mi.audioChannels == 2) ? L"2.0 Stereo" :
                                         (mi.audioChannels == 1) ? L"1.0 Mono" :
                                         (std::to_wstring(mi.audioChannels) + L" Channels");
                    pThis->showOsdMessage(L"Channels: " + chDesc);
                    return 0;
                }
            }
            break;
        }
        case WM_MOUSEWHEEL: {
            if (pThis) {
                short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                int change = (delta > 0) ? 5 : -5;
                int vol = rx::utils::clamp(pThis->m_engine.getVolume() + change, 0, 100);
                pThis->m_engine.setVolume(vol);
                pThis->m_timelineStrip.setVolume(vol, pThis->m_engine.isMuted());
                pThis->showOsdMessage(L"Volume: " + std::to_wstring(vol) + L"%");
            }
            return 0;
        }
        case WM_RBUTTONUP: {
            if (pThis) {
                POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ClientToScreen(hwnd, &pt);
                pThis->showContextMenu(pt.x, pt.y);
            }
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
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
