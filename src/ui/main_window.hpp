#pragma once

#include "theme.hpp"
#include "controls.hpp"
#include "panels/playlist_panel.hpp"
#include "panels/video_panel.hpp"
#include "panels/audio_panel.hpp"
#include "dialogs/info_dialog.hpp"
#include "dialogs/sub_dialog.hpp"
#include "dialogs/open_url_dialog.hpp"
#include "dialogs/bookmark_dialog.hpp"
#include "dialogs/preferences_dialog.hpp"
#include "dialogs/shortcut_help_dialog.hpp"
#include "../core/shortcut_manager.hpp"
#include "../engine/mpv_backend.hpp"
#include "../playlist/playlist_manager.hpp"
#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <string>
#include <vector>

namespace rx::ui {

class RXMainWindow {
public:
    RXMainWindow();
    ~RXMainWindow();

    bool create(HINSTANCE hInstance, int nCmdShow);
    int runEventLoop();
    void openFile(const std::string& path);

    HWND getHwnd() const { return m_mainHwnd; }

private:
    static LRESULT CALLBACK mainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK videoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK controlBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void onResize(int width, int height);
    void onPaintControlBar(HDC hdc);
    void onKeyDown(WPARAM key);
    void onDropFiles(HDROP hDrop);

    // Actions
    void openFileDialog();
    void openFolderDialog();
    void openSubtitleDialog();
    void openUrlDialog();
    void openBookmarkDialog();
    void addQuickBookmark();
    void playItemAtIndex(int index);
    void toggleFullscreen();
    void togglePlaylistPanel();
    void toggleVideoPanel();
    void toggleAudioPanel();
    void showInfoDialog();
    void showPreferencesDialog();
    void showShortcutHelpDialog();
    void showAboutDialog();
    void executeAction(PlayerAction action);
    void showContextMenu(int screenX, int screenY);
    void showMainMenu(int screenX, int screenY);
    void toggleCompactMode();
    void toggleAlwaysOnTop();
    void cyclePlaybackSpeed();
    void cycleAspectRatio();
    void toggleDeinterlace();
    void drawEmptyState(HDC hdc, const RECT& rc);
    void toggleMute();
    void updateTimeDisplay(double posSec, double durSec);
    void showOsdMessage(const std::wstring& msg, int durationMs = 2000);
    void takeScreenshot();

    // Controls setup
    void setupChildControls();
    void layoutControls();
    void startAutoHideTimer();
    void resetAutoHide();

    HWND m_mainHwnd = nullptr;
    HWND m_videoHwnd = nullptr;
    HWND m_controlBarHwnd = nullptr;

    // Four horizontal sections
    RXTitleBar m_titleBar;             // A. Top Title Bar (38px)
    RXTimelineStrip m_timelineStrip;   // C. Timeline & Volume Strip (26px)

    // D. Bottom Control Toolbar Controls (48px)
    // Left playback group
    RXIconButton m_btnPlay;
    RXIconButton m_btnStop;
    RXIconButton m_btnPrev;
    RXIconButton m_btnNext;
    RXIconButton m_btnOpen;

    // Right feature controls group
    RXIconButton m_btnVideo;
    RXIconButton m_btn360;
    RXIconButton m_btn3D;
    RXIconButton m_btnSubtitles;
    RXIconButton m_btnAudio;
    RXIconButton m_btnPlaylist;
    RXIconButton m_btnSettings;
    RXIconButton m_btnMenu;
    RXIconButton m_btnFullscreen;

    // Subsystems
    MpvBackend m_engine;
    PlaylistManager m_playlistMgr;
    std::unique_ptr<RXPlaylistPanel> m_playlistPanel;
    std::unique_ptr<RXVideoPanel> m_videoPanel;
    std::unique_ptr<RXAudioPanel> m_audioPanel;
    std::unique_ptr<RXInfoDialog> m_infoDialog;
    std::unique_ptr<RXSubtitleDialog> m_subDialog;
    std::unique_ptr<RXOpenUrlDialog> m_urlDialog;
    std::unique_ptr<RXBookmarkDialog> m_bmDialog;
    std::unique_ptr<RXPreferencesDialog> m_prefsDialog;
    std::unique_ptr<RXShortcutHelpDialog> m_shortcutHelpDialog;

    // State
    bool m_isFullscreen = false;
    bool m_isCompactMode = false;
    bool m_alwaysOnTop = false;
    WINDOWPLACEMENT m_prevPlacement{};
    bool m_controlsHidden = false;
    UINT_PTR m_autoHideTimerId = 0;
    std::wstring m_osdMessage;
    UINT_PTR m_osdTimerId = 0;
    HFONT m_uiFont = nullptr;
    HFONT m_uiBoldFont = nullptr;
    HFONT m_osdFont = nullptr;
    HFONT m_brandFont = nullptr;
    HFONT m_brandSubFont = nullptr;
    HFONT m_recentFont = nullptr;
    HICON m_hBrandIcon = nullptr;

    // Playback telemetry cache for bottom bar
    double m_cachedPos = 0.0;
    double m_cachedDur = 0.0;
    RECT m_rcHwdecBadge{};

    // Empty state interactive regions
    RECT m_rcEmptyOpenFile{};
    RECT m_rcEmptyOpenFolder{};
    RECT m_rcEmptyOpenUrl{};
    std::vector<std::pair<RECT, std::string>> m_recentItemRects;
    int m_hoveredRecentIdx = -1;
    bool m_hoveringOpenFile = false;
    bool m_hoveringOpenFolder = false;
    bool m_hoveringOpenUrl = false;
};

} // namespace rx::ui
