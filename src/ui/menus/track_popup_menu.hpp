#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

namespace rx::ui {

struct TrackMenuItem {
    int id = 0;
    std::wstring text;
    std::wstring shortcut;
    bool isSeparator = false;
    bool isHeader = false;
    bool isRadio = false;
    bool isSelected = false;
    bool isEnabled = true;
};

class RXTrackMenu {
public:
    RXTrackMenu();
    ~RXTrackMenu();

    // Displays the popup menu at (screenX, screenY).
    // Returns the selected item ID (or 0 if dismissed/cancelled).
    int trackPopupMenu(HWND parentHwnd, int screenX, int screenY,
                       const std::vector<TrackMenuItem>& items);

    void close();
    bool isOpen() const { return m_isOpen; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    int hitTest(int y) const;

    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    HFONT m_font = nullptr;
    HFONT m_fontBold = nullptr;

    std::vector<TrackMenuItem> m_items;
    int m_hoveredIndex = -1;
    int m_selectedIndex = -1;
    bool m_isOpen = false;
    int m_chosenId = 0;

    int m_width = 380;
    int m_height = 200;
};

} // namespace rx::ui
