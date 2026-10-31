#include "media_database.hpp"
#include "../../core/logger.hpp"
#include "../../core/utils.hpp"
#include "sqlite3.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <chrono>

namespace rx::library {

MediaDatabase& MediaDatabase::instance() {
    static MediaDatabase s_instance;
    return s_instance;
}

MediaDatabase::MediaDatabase() = default;

MediaDatabase::~MediaDatabase() {
    close();
}

std::string MediaDatabase::getDefaultDbPath() {
    PWSTR appDataPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
        std::filesystem::path dir = appDataPath;
        CoTaskMemFree(appDataPath);
        dir /= "RXMediaPlayer";
        std::filesystem::create_directories(dir);
        return (dir / "rx_library.db").string();
    }
    return "rx_library.db";
}

bool MediaDatabase::execute(const char* sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) {
            RX_LOG_ERROR("SQLite error: " + std::string(errMsg));
            sqlite3_free(errMsg);
        }
        return false;
    }
    return true;
}

bool MediaDatabase::initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) return true;

    m_dbPath = dbPath.empty() ? getDefaultDbPath() : dbPath;

    int rc = sqlite3_open_v2(m_dbPath.c_str(), &m_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);
    if (rc != SQLITE_OK) {
        RX_LOG_ERROR("Failed to open SQLite database: " + m_dbPath);
        m_db = nullptr;
        return false;
    }

    // Enable WAL mode for high concurrency & performance
    execute("PRAGMA journal_mode=WAL;");
    execute("PRAGMA synchronous=NORMAL;");

    // 1. Playback History & Resume Table
    const char* kHistoryTableSql =
        "CREATE TABLE IF NOT EXISTS playback_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path TEXT UNIQUE NOT NULL,"
        "  title TEXT,"
        "  duration REAL DEFAULT 0.0,"
        "  last_position REAL DEFAULT 0.0,"
        "  last_played INTEGER DEFAULT 0,"
        "  play_count INTEGER DEFAULT 1"
        ");";
    execute(kHistoryTableSql);

    // 2. Bookmarks Table
    const char* kBookmarksTableSql =
        "CREATE TABLE IF NOT EXISTS bookmarks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path TEXT NOT NULL,"
        "  timestamp_sec REAL NOT NULL,"
        "  note TEXT,"
        "  created_at INTEGER DEFAULT 0"
        ");";
    execute(kBookmarksTableSql);

    // 3. Watched Folders Table
    const char* kFoldersTableSql =
        "CREATE TABLE IF NOT EXISTS watched_folders ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  folder_path TEXT UNIQUE NOT NULL,"
        "  auto_scan INTEGER DEFAULT 1"
        ");";
    execute(kFoldersTableSql);

    // 4. Media Library Table
    const char* kLibraryTableSql =
        "CREATE TABLE IF NOT EXISTS media_library ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path TEXT UNIQUE NOT NULL,"
        "  title TEXT,"
        "  duration REAL DEFAULT 0.0,"
        "  file_size INTEGER DEFAULT 0,"
        "  format TEXT,"
        "  last_scanned INTEGER DEFAULT 0"
        ");";
    execute(kLibraryTableSql);

    RX_LOG_INFO("SQLite Media Database initialized: " + m_dbPath);
    return true;
}

void MediaDatabase::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
}

void MediaDatabase::recordPlaybackProgress(const std::string& path, const std::string& title,
                                          double positionSec, double durationSec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || path.empty()) return;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql =
        "INSERT INTO playback_history (file_path, title, duration, last_position, last_played, play_count) "
        "VALUES (?, ?, ?, ?, ?, 1) "
        "ON CONFLICT(file_path) DO UPDATE SET "
        "  title = excluded.title, "
        "  duration = excluded.duration, "
        "  last_position = excluded.last_position, "
        "  last_played = excluded.last_played, "
        "  play_count = playback_history.play_count + 1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, durationSec);
        sqlite3_bind_double(stmt, 4, positionSec);
        sqlite3_bind_int64(stmt, 5, now);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

double MediaDatabase::getLastPosition(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || path.empty()) return 0.0;

    const char* sql = "SELECT last_position FROM playback_history WHERE file_path = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    double pos = 0.0;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            pos = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return pos;
}

std::vector<HistoryEntry> MediaDatabase::getRecentHistory(int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HistoryEntry> list;
    if (!m_db) return list;

    const char* sql = "SELECT id, file_path, title, duration, last_position, last_played, play_count "
                      "FROM playback_history ORDER BY last_played DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            HistoryEntry e;
            e.id = sqlite3_column_int(stmt, 0);
            const auto* pPath = sqlite3_column_text(stmt, 1);
            if (pPath) e.filePath = reinterpret_cast<const char*>(pPath);
            const auto* pTitle = sqlite3_column_text(stmt, 2);
            if (pTitle) e.title = reinterpret_cast<const char*>(pTitle);
            e.duration = sqlite3_column_double(stmt, 3);
            e.lastPosition = sqlite3_column_double(stmt, 4);
            e.lastPlayed = sqlite3_column_int64(stmt, 5);
            e.playCount = sqlite3_column_int(stmt, 6);
            list.push_back(e);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

void MediaDatabase::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return;
    execute("DELETE FROM playback_history;");
}

bool MediaDatabase::addBookmark(const std::string& filePath, double timestampSec, const std::string& note) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || filePath.empty()) return false;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* sql = "INSERT INTO bookmarks (file_path, timestamp_sec, note, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 2, timestampSec);
        sqlite3_bind_text(stmt, 3, note.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 4, now);

        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

bool MediaDatabase::deleteBookmark(int bookmarkId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return false;

    const char* sql = "DELETE FROM bookmarks WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, bookmarkId);
        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

std::vector<BookmarkEntry> MediaDatabase::getBookmarksForFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BookmarkEntry> list;
    if (!m_db || filePath.empty()) return list;

    const char* sql = "SELECT id, file_path, timestamp_sec, note, created_at FROM bookmarks "
                      "WHERE file_path = ? ORDER BY timestamp_sec ASC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BookmarkEntry b;
            b.id = sqlite3_column_int(stmt, 0);
            const auto* pPath = sqlite3_column_text(stmt, 1);
            if (pPath) b.filePath = reinterpret_cast<const char*>(pPath);
            b.timestampSec = sqlite3_column_double(stmt, 2);
            const auto* pNote = sqlite3_column_text(stmt, 3);
            if (pNote) b.note = reinterpret_cast<const char*>(pNote);
            b.createdAt = sqlite3_column_int64(stmt, 4);
            list.push_back(b);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

std::vector<BookmarkEntry> MediaDatabase::getAllBookmarks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<BookmarkEntry> list;
    if (!m_db) return list;

    const char* sql = "SELECT id, file_path, timestamp_sec, note, created_at FROM bookmarks "
                      "ORDER BY created_at DESC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BookmarkEntry b;
            b.id = sqlite3_column_int(stmt, 0);
            const auto* pPath = sqlite3_column_text(stmt, 1);
            if (pPath) b.filePath = reinterpret_cast<const char*>(pPath);
            b.timestampSec = sqlite3_column_double(stmt, 2);
            const auto* pNote = sqlite3_column_text(stmt, 3);
            if (pNote) b.note = reinterpret_cast<const char*>(pNote);
            b.createdAt = sqlite3_column_int64(stmt, 4);
            list.push_back(b);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

bool MediaDatabase::addWatchedFolder(const std::string& folderPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || folderPath.empty()) return false;

    const char* sql = "INSERT OR IGNORE INTO watched_folders (folder_path, auto_scan) VALUES (?, 1);";
    sqlite3_stmt* stmt = nullptr;
    bool success = false;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, folderPath.c_str(), -1, SQLITE_TRANSIENT);
        success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
    }
    return success;
}

std::vector<std::string> MediaDatabase::getWatchedFolders() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> folders;
    if (!m_db) return folders;

    const char* sql = "SELECT folder_path FROM watched_folders ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const auto* p = sqlite3_column_text(stmt, 0);
            if (p) folders.push_back(reinterpret_cast<const char*>(p));
        }
        sqlite3_finalize(stmt);
    }
    return folders;
}

int MediaDatabase::scanFolder(const std::string& folderPath) {
    if (folderPath.empty() || !std::filesystem::exists(folderPath)) return 0;
    addWatchedFolder(folderPath);

    int count = 0;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    const char* insertSql =
        "INSERT INTO media_library (file_path, title, duration, file_size, format, last_scanned) "
        "VALUES (?, ?, 0.0, ?, ?, ?) "
        "ON CONFLICT(file_path) DO UPDATE SET "
        "  file_size = excluded.file_size, "
        "  last_scanned = excluded.last_scanned;";

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr) != SQLITE_OK) {
        return 0;
    }

    execute("BEGIN TRANSACTION;");

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 folderPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            auto ext = entry.path().extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(c));

            if (ext == ".mp4" || ext == ".mkv" || ext == ".mov" || ext == ".avi" ||
                ext == ".webm" || ext == ".flv" || ext == ".ts" || ext == ".mp3" ||
                ext == ".flac" || ext == ".wav" || ext == ".aac" || ext == ".ogg") {

                std::string pathStr = entry.path().string();
                std::string titleStr = entry.path().stem().string();
                int64_t size = entry.file_size();
                std::string fmt = ext.substr(1);

                sqlite3_reset(stmt);
                sqlite3_bind_text(stmt, 1, pathStr.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, titleStr.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 3, size);
                sqlite3_bind_text(stmt, 4, fmt.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(stmt, 5, now);

                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    count++;
                }
            }
        }
    } catch (...) {}

    sqlite3_finalize(stmt);
    execute("COMMIT;");
    return count;
}

std::vector<LibraryItem> MediaDatabase::searchLibrary(const std::string& query, int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LibraryItem> list;
    if (!m_db) return list;

    std::string sql;
    if (query.empty()) {
        sql = "SELECT id, file_path, title, duration, file_size, format, last_scanned "
              "FROM media_library ORDER BY id DESC LIMIT ?;";
    } else {
        sql = "SELECT id, file_path, title, duration, file_size, format, last_scanned "
              "FROM media_library WHERE title LIKE ? OR file_path LIKE ? ORDER BY id DESC LIMIT ?;";
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (query.empty()) {
            sqlite3_bind_int(stmt, 1, limit);
        } else {
            std::string q = "%" + query + "%";
            sqlite3_bind_text(stmt, 1, q.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, q.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 3, limit);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            LibraryItem item;
            item.id = sqlite3_column_int(stmt, 0);
            const auto* pPath = sqlite3_column_text(stmt, 1);
            if (pPath) item.filePath = reinterpret_cast<const char*>(pPath);
            const auto* pTitle = sqlite3_column_text(stmt, 2);
            if (pTitle) item.title = reinterpret_cast<const char*>(pTitle);
            item.duration = sqlite3_column_double(stmt, 3);
            item.fileSize = sqlite3_column_int64(stmt, 4);
            const auto* pFmt = sqlite3_column_text(stmt, 5);
            if (pFmt) item.format = reinterpret_cast<const char*>(pFmt);
            item.lastScanned = sqlite3_column_int64(stmt, 6);
            list.push_back(item);
        }
        sqlite3_finalize(stmt);
    }
    return list;
}

} // namespace rx::library
