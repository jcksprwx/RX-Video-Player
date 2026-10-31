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
#include "../engine/mpv_backend.hpp"
#include "../playlist/playlist_manager.hpp"
#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <string>

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

    // Controls
    RXSeekBar m_seekBar;
    RXIconButton m_btnPlay;
    RXIconButton m_btnStop;
    RXIconButton m_btnPrev;
    RXIconButton m_btnNext;
    RXIconButton m_btnMute;
    RXIconButton m_btnSubtitles;
    RXIconButton m_btnHwdec;
    RXIconButton m_btnVideoAdjust;
    RXIconButton m_btnAudio;
    RXIconButton m_btnInfo;
    RXIconButton m_btnPlaylist;
    RXIconButton m_btnFullscreen;
    HWND m_lblTime = nullptr;

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

    // State
    bool m_isFullscreen = false;
    WINDOWPLACEMENT m_prevPlacement{};
    bool m_controlsHidden = false;
    UINT_PTR m_autoHideTimerId = 0;
    std::wstring m_osdMessage;
    UINT_PTR m_osdTimerId = 0;
    HFONT m_uiFont = nullptr;
    HFONT m_osdFont = nullptr;
};

} // namespace rx::ui
