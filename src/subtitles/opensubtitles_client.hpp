#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace rx::subtitles {

struct ParsedMediaInfo {
    std::string cleanTitle;
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

class OpenSubtitlesClient {
public:
    // Calculates the official 64-bit OpenSubtitles MovieHash
    static uint64_t computeMovieHash(const std::string& filePath, uint64_t& outFileSize);
    static std::string formatMovieHash(uint64_t hash);

    // Extracts clean title, year, season, and episode from a video filename
    static ParsedMediaInfo parseFilename(const std::string& filePath);

    // Performs online search via official OpenSubtitles.com REST API v1
    static std::vector<SubtitleSearchResult> searchSubtitles(
        const std::string& apiKey,
        const std::string& query,
        const std::string& languages = "en",
        const std::string& movieHash = "",
        int season = 0,
        int episode = 0,
        int year = 0,
        std::string* outError = nullptr
    );

    // Requests download link and saves subtitle to destination path
    static bool downloadSubtitle(
        const std::string& apiKey,
        int fileId,
        const std::string& destFilePath,
        std::string* outError = nullptr
    );

    // Checks if an API key looks valid (32 characters alphanumeric)
    static bool isValidApiKey(const std::string& key);
};

} // namespace rx::subtitles
