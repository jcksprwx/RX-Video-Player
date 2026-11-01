#pragma once

#include "../../subtitles/opensubtitles_client.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>

namespace rx::ui {

class RXOnlineSubtitlesDialog {
public:
    using DownloadCallback = std::function<void(const std::string& filePath, bool applyNow)>;

    RXOnlineSubtitlesDialog();
    ~RXOnlineSubtitlesDialog();

    bool show(HWND parentHwnd, const std::string& currentMediaPath = "");
    void close();
    bool isVisible() const { return (m_hwnd != nullptr && IsWindow(m_hwnd)); }

    void setDownloadCallback(DownloadCallback cb) { m_downloadCb = std::move(cb); }

private:
    static LRESULT CALLBACK dlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onInitDialog();
    void onResize(int width, int height);
    void onSearchClicked();
    void onDownloadClicked(bool applyNow);
    void updateStatus(const std::wstring& status);
    void populateResults();

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;

    HWND m_lblFile = nullptr;
    HWND m_editTitle = nullptr;
    HWND m_editYear = nullptr;
    HWND m_editSeason = nullptr;
    HWND m_editEpisode = nullptr;
    HWND m_comboLang = nullptr;
    HWND m_btnSearch = nullptr;

    HWND m_lblApiKey = nullptr;
    HWND m_editApiKey = nullptr;
    HWND m_btnSaveKey = nullptr;

    HWND m_listResults = nullptr;
    HWND m_lblStatus = nullptr;
    HWND m_btnDownloadApply = nullptr;
    HWND m_btnDownloadOnly = nullptr;
    HWND m_btnClose = nullptr;

    HFONT m_font = nullptr;
    HFONT m_fontBold = nullptr;

    std::string m_currentMediaPath;
    subtitles::ParsedMediaInfo m_mediaInfo;
    std::string m_movieHash;

    std::vector<subtitles::SubtitleSearchResult> m_results;
    std::thread m_workerThread;
    std::atomic<bool> m_isBusy{false};
    DownloadCallback m_downloadCb;
};

} // namespace rx::ui
