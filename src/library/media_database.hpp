#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

struct sqlite3;

namespace rx::library {

struct HistoryEntry {
    int id = 0;
    std::string filePath;
    std::string title;
    double duration = 0.0;
    double lastPosition = 0.0;
    int64_t lastPlayed = 0;
    int playCount = 0;
};

struct BookmarkEntry {
    int id = 0;
    std::string filePath;
    double timestampSec = 0.0;
    std::string note;
    int64_t createdAt = 0;
};

struct LibraryItem {
    int id = 0;
    std::string filePath;
    std::string title;
    double duration = 0.0;
    int64_t fileSize = 0;
    std::string format;
    int64_t lastScanned = 0;
};

class MediaDatabase {
public:
    static MediaDatabase& instance();

    bool initialize(const std::string& dbPath = "");
    void close();
    bool isOpen() const { return m_db != nullptr; }

    // Playback History & Resume
    void recordPlaybackProgress(const std::string& path, const std::string& title, double positionSec, double durationSec);
    double getLastPosition(const std::string& path);
    std::vector<HistoryEntry> getRecentHistory(int limit = 50);
    void clearHistory();

    // Bookmarks
    bool addBookmark(const std::string& filePath, double timestampSec, const std::string& note = "");
    bool deleteBookmark(int bookmarkId);
    std::vector<BookmarkEntry> getBookmarksForFile(const std::string& filePath);
    std::vector<BookmarkEntry> getAllBookmarks();

    // Watched Folders & Library
    bool addWatchedFolder(const std::string& folderPath);
    std::vector<std::string> getWatchedFolders();
    int scanFolder(const std::string& folderPath);
    std::vector<LibraryItem> searchLibrary(const std::string& query = "", int limit = 100);

private:
    MediaDatabase();
    ~MediaDatabase();

    bool execute(const char* sql);
    std::string getDefaultDbPath();

    sqlite3* m_db = nullptr;
    std::mutex m_mutex;
    std::string m_dbPath;
};

} // namespace rx::library
