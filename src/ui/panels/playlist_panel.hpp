#pragma once

#include "../theme.hpp"
#include "../../playlist/playlist_manager.hpp"
#include <windows.h>
#include <memory>
#include <functional>

namespace rx::ui {

class RXPlaylistPanel {
public:
    using PlayItemCallback = std::function<void(int index)>;
    using AddFilesCallback = std::function<void()>;
    using AddFolderCallback = std::function<void()>;
    using SaveM3UCallback = std::function<void()>;
    using LoadM3UCallback = std::function<void()>;

    RXPlaylistPanel(PlaylistManager& playlistMgr);
    ~RXPlaylistPanel();

    bool create(HWND parentHwnd, int x, int y, int width, int height);
    void move(int x, int y, int width, int height);
    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }
    void refresh();

    void setPlayItemCallback(PlayItemCallback cb) { m_playCb = std::move(cb); }
    void setAddFilesCallback(AddFilesCallback cb) { m_addFilesCb = std::move(cb); }
    void setAddFolderCallback(AddFolderCallback cb) { m_addFolderCb = std::move(cb); }
    void setSaveM3UCallback(SaveM3UCallback cb) { m_saveM3uCb = std::move(cb); }
    void setLoadM3UCallback(LoadM3UCallback cb) { m_loadM3uCb = std::move(cb); }

    HWND getHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onSize(int width, int height);
    void onCommand(WORD id, WORD code);

    PlaylistManager& m_playlistMgr;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HWND m_listBox = nullptr;
    HWND m_btnAdd = nullptr;
    HWND m_btnAddFolder = nullptr;
    HWND m_btnClear = nullptr;
    HWND m_btnRepeat = nullptr;
    HWND m_btnShuffle = nullptr;
    bool m_visible = true;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;

    PlayItemCallback m_playCb;
    AddFilesCallback m_addFilesCb;
    AddFolderCallback m_addFolderCb;
    SaveM3UCallback m_saveM3uCb;
    LoadM3UCallback m_loadM3uCb;
};

} // namespace rx::ui
