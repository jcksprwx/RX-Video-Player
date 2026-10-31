#include "playlist_manager.hpp"
#include "../core/utils.hpp"
#include <fstream>
#include <algorithm>
#include <random>

namespace rx {

void PlaylistManager::addItem(const std::string& path, const std::string& title) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    PlaylistItem item;
    item.path = path;
    if (!title.empty()) {
        item.title = title;
    } else {
        std::filesystem::path p(path);
        item.title = p.stem().string();
    }
    m_items.push_back(item);
    if (m_currentIndex < 0) {
        m_currentIndex = 0;
    }
    notifyChange();
}

void PlaylistManager::addFiles(const std::vector<std::string>& paths) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    for (const auto& path : paths) {
        std::filesystem::path p(path);
        if (rx::utils::isSupportedMediaFile(p)) {
            m_items.push_back(PlaylistItem::fromFile(p));
        } else if (rx::utils::isPlaylistFile(p)) {
            // Load playlist file
            loadM3U(p.string());
        }
    }
    if (m_currentIndex < 0 && !m_items.empty()) {
        m_currentIndex = 0;
    }
    notifyChange();
}

void PlaylistManager::addFolder(const std::string& dirPath, bool recursive) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    try {
        std::filesystem::path root(dirPath);
        if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root)) return;

        auto processFile = [&](const std::filesystem::path& p) {
            if (rx::utils::isSupportedMediaFile(p)) {
                m_items.push_back(PlaylistItem::fromFile(p));
            }
        };

        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
                if (entry.is_regular_file()) processFile(entry.path());
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(root)) {
                if (entry.is_regular_file()) processFile(entry.path());
            }
        }
    } catch (...) {}

    if (m_currentIndex < 0 && !m_items.empty()) {
        m_currentIndex = 0;
    }
    notifyChange();
}

void PlaylistManager::removeItem(size_t index) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (index >= m_items.size()) return;
    m_items.erase(m_items.begin() + index);

    if (m_items.empty()) {
        m_currentIndex = -1;
    } else if (m_currentIndex >= static_cast<int>(m_items.size())) {
        m_currentIndex = static_cast<int>(m_items.size()) - 1;
    }
    notifyChange();
}

void PlaylistManager::clear() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_items.clear();
    m_currentIndex = -1;
    notifyChange();
}

void PlaylistManager::moveItem(size_t fromIndex, size_t toIndex) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (fromIndex >= m_items.size() || toIndex >= m_items.size() || fromIndex == toIndex) return;

    PlaylistItem item = m_items[fromIndex];
    m_items.erase(m_items.begin() + fromIndex);
    m_items.insert(m_items.begin() + toIndex, item);

    if (m_currentIndex == static_cast<int>(fromIndex)) {
        m_currentIndex = static_cast<int>(toIndex);
    } else if (fromIndex < toIndex && m_currentIndex > static_cast<int>(fromIndex) && m_currentIndex <= static_cast<int>(toIndex)) {
        m_currentIndex--;
    } else if (fromIndex > toIndex && m_currentIndex >= static_cast<int>(toIndex) && m_currentIndex < static_cast<int>(fromIndex)) {
        m_currentIndex++;
    }
    notifyChange();
}

size_t PlaylistManager::size() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_items.size();
}

bool PlaylistManager::empty() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_items.empty();
}

std::optional<PlaylistItem> PlaylistManager::getItem(size_t index) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (index < m_items.size()) return m_items[index];
    return std::nullopt;
}

std::optional<PlaylistItem> PlaylistManager::getCurrentItem() const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_items.size())) {
        return m_items[m_currentIndex];
    }
    return std::nullopt;
}

void PlaylistManager::setCurrentIndex(int index) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_items.size())) {
        m_currentIndex = index;
        notifyChange();
    }
}

std::optional<PlaylistItem> PlaylistManager::next(bool userExplicit) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_items.empty()) return std::nullopt;

    if (!userExplicit && m_repeatMode == RepeatMode::RepeatOne) {
        if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_items.size())) {
            return m_items[m_currentIndex];
        }
    }

    if (m_currentIndex + 1 < static_cast<int>(m_items.size())) {
        m_currentIndex++;
        notifyChange();
        return m_items[m_currentIndex];
    } else if (m_repeatMode == RepeatMode::RepeatAll || userExplicit) {
        m_currentIndex = 0;
        notifyChange();
        return m_items[0];
    }

    return std::nullopt;
}

std::optional<PlaylistItem> PlaylistManager::previous() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_items.empty()) return std::nullopt;

    if (m_currentIndex > 0) {
        m_currentIndex--;
        notifyChange();
        return m_items[m_currentIndex];
    } else if (m_repeatMode == RepeatMode::RepeatAll) {
        m_currentIndex = static_cast<int>(m_items.size()) - 1;
        notifyChange();
        return m_items[m_currentIndex];
    }

    return std::nullopt;
}

void PlaylistManager::shuffle() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_items.size() <= 1) return;

    std::string currentPath;
    if (m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_items.size())) {
        currentPath = m_items[m_currentIndex].path;
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(m_items.begin(), m_items.end(), g);

    if (!currentPath.empty()) {
        for (size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].path == currentPath) {
                m_currentIndex = static_cast<int>(i);
                break;
            }
        }
    }
    notifyChange();
}

bool PlaylistManager::saveM3U(const std::string& m3uPath) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::ofstream out(m3uPath);
    if (!out.is_open()) return false;

    out << "#EXTM3U\n";
    for (const auto& item : m_items) {
        out << "#EXTINF:-1," << item.title << "\n";
        out << item.path << "\n";
    }
    return true;
}

bool PlaylistManager::loadM3U(const std::string& m3uPath) {
    std::ifstream in(m3uPath);
    if (!in.is_open()) return false;

    std::vector<PlaylistItem> loaded;
    std::string line;
    std::string pendingTitle;

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (line.rfind("#EXTINF:", 0) == 0) {
            size_t comma = line.find(',');
            if (comma != std::string::npos) {
                pendingTitle = line.substr(comma + 1);
            }
        } else if (line[0] != '#') {
            PlaylistItem item;
            item.path = line;
            if (!pendingTitle.empty()) {
                item.title = pendingTitle;
                pendingTitle.clear();
            } else {
                std::filesystem::path p(line);
                item.title = p.stem().string();
            }
            loaded.push_back(item);
        }
    }

    if (loaded.empty()) return false;

    {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_items = std::move(loaded);
        m_currentIndex = 0;
    }
    notifyChange();
    return true;
}

void PlaylistManager::notifyChange() {
    if (m_changeCb) {
        m_changeCb();
    }
}

} // namespace rx
