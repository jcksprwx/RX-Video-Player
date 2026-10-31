#pragma once

#include "playlist_item.hpp"
#include "rx/types.hpp"
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include <functional>

namespace rx {

class PlaylistManager {
public:
    using ChangeCallback = std::function<void()>;

    PlaylistManager() = default;

    void addItem(const std::string& path, const std::string& title = "");
    void addFiles(const std::vector<std::string>& paths);
    void addFolder(const std::string& dirPath, bool recursive = true);
    void removeItem(size_t index);
    void clear();
    void moveItem(size_t fromIndex, size_t toIndex);

    size_t size() const;
    bool empty() const;
    const std::vector<PlaylistItem>& getItems() const { return m_items; }

    std::optional<PlaylistItem> getItem(size_t index) const;
    std::optional<PlaylistItem> getCurrentItem() const;

    int getCurrentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);

    // Navigation logic considering repeat & shuffle
    std::optional<PlaylistItem> next(bool userExplicit = false);
    std::optional<PlaylistItem> previous();

    void shuffle();
    void setRepeatMode(RepeatMode mode) { m_repeatMode = mode; }
    RepeatMode getRepeatMode() const { return m_repeatMode; }

    bool saveM3U(const std::string& m3uPath) const;
    bool loadM3U(const std::string& m3uPath);

    void setChangeCallback(ChangeCallback cb) { m_changeCb = std::move(cb); }

private:
    void notifyChange();

    mutable std::recursive_mutex m_mutex;
    std::vector<PlaylistItem> m_items;
    int m_currentIndex = -1;
    RepeatMode m_repeatMode = RepeatMode::Off;
    ChangeCallback m_changeCb;
};

} // namespace rx
