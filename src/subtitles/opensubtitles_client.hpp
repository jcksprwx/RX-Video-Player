#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace rx::subtitles {

struct ParsedMediaInfo {
    std::string cleanTitle;
    std::string title; // Alias/compatibility
    int year = 0;
    int season = 0;
    int episode = 0;
    std::string rawFilename;
};

struct SubtitleSearchResult {
    std::string id;
    int fileId = 0;
    std::string fileName;
    std::string language;
    std::wstring languageName;
    std::string format;
    std::string release;
    int downloadCount = 0;
    double rating = 0.0;
    bool hearingImpaired = false;
    bool hd = false;
};

struct UserAccountInfo {
    bool isLoggedIn = false;
    std::string username;
    std::string token;
    int remainingDownloads = 0;
    int allowedDownloads = 0;
    bool isVip = false;
    std::string statusMessage;
};

class OpenSubtitlesClient {
public:
    // Calculates the official 64-bit OpenSubtitles MovieHash
    static uint64_t computeMovieHash(const std::string& filePath, uint64_t& outFileSize);
    static std::string formatMovieHash(uint64_t hash);

    // Extracts clean title, year, season, and episode from a video filename
    static ParsedMediaInfo parseFilename(const std::string& filePath);

    // Checks if an API key looks valid (alphanumeric, at least 16 chars)
    static bool isValidApiKey(const std::string& key);

    // Tests connectivity to OpenSubtitles REST API v1
    static bool testConnection(
        const std::string& apiKey,
        const std::string& userToken = "",
        UserAccountInfo* outInfo = nullptr,
        std::string* outError = nullptr
    );

    // Authenticates user with username & password to obtain JWT token
    static bool login(
        const std::string& apiKey,
        const std::string& username,
        const std::string& password,
        UserAccountInfo* outInfo,
        std::string* outError = nullptr
    );

    // Invalidate user session
    static bool logout(
        const std::string& apiKey,
        const std::string& userToken,
        std::string* outError = nullptr
    );

    // Performs online search via official OpenSubtitles.com REST API v1
    static std::vector<SubtitleSearchResult> searchSubtitles(
        const std::string& apiKey,
        const std::string& query,
        const std::string& languages = "en",
        const std::string& movieHash = "",
        int season = 0,
        int episode = 0,
        int year = 0,
        const std::string& userToken = "",
        std::string* outError = nullptr
    );

    // Requests download link and saves subtitle to destination path
    static bool downloadSubtitle(
        const std::string& apiKey,
        int fileId,
        const std::string& destFilePath,
        const std::string& userToken = "",
        std::string* outError = nullptr
    );
};

} // namespace rx::subtitles
