#include "utils.hpp"
#include <windows.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <unordered_set>

namespace rx::utils {

std::string formatTime(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    int totalSec = static_cast<int>(seconds);
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;

    std::ostringstream oss;
    if (h > 0) {
        oss << std::setfill('0') << std::setw(2) << h << ":"
            << std::setfill('0') << std::setw(2) << m << ":"
            << std::setfill('0') << std::setw(2) << s;
    } else {
        oss << std::setfill('0') << std::setw(2) << m << ":"
            << std::setfill('0') << std::setw(2) << s;
    }
    return oss.str();
}

std::string formatBytes(int64_t bytes) {
    if (bytes < 0) bytes = 0;
    constexpr double KB = 1024.0;
    constexpr double MB = KB * 1024.0;
    constexpr double GB = MB * 1024.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    if (bytes >= GB) {
        oss << (bytes / GB) << " GB";
    } else if (bytes >= MB) {
        oss << (bytes / MB) << " MB";
    } else if (bytes >= KB) {
        oss << (bytes / KB) << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

std::wstring utf8ToWide(std::string_view utf8Str) {
    if (utf8Str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring result(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), result.data(), len);
    return result;
}

std::string wideToUtf8(std::wstring_view wideStr) {
    if (wideStr.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), static_cast<int>(wideStr.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), static_cast<int>(wideStr.size()), result.data(), len, nullptr, nullptr);
    return result;
}

namespace {
    std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return str;
    }

    const std::unordered_set<std::string> kVideoExtensions = {
        ".mp4", ".mkv", ".mov", ".avi", ".wmv", ".webm", ".flv",
        ".mpeg", ".mpg", ".ts", ".mts", ".m2ts", ".vob", ".ogv",
        ".m4v", ".3gp", ".f4v"
    };

    const std::unordered_set<std::string> kAudioExtensions = {
        ".mp3", ".aac", ".flac", ".wav", ".wma", ".ogg", ".opus",
        ".aiff", ".alac", ".ape", ".ac3", ".eac3", ".dts", ".m4a"
    };

    const std::unordered_set<std::string> kSubtitleExtensions = {
        ".srt", ".ass", ".ssa", ".vtt", ".sub", ".idx", ".smi"
    };

    const std::unordered_set<std::string> kPlaylistExtensions = {
        ".m3u", ".m3u8", ".pls"
    };
}

bool isSupportedMediaFile(const std::filesystem::path& path) {
    std::string ext = toLower(path.extension().string());
    return kVideoExtensions.contains(ext) || kAudioExtensions.contains(ext);
}

bool isSubtitleFile(const std::filesystem::path& path) {
    std::string ext = toLower(path.extension().string());
    return kSubtitleExtensions.contains(ext);
}

bool isPlaylistFile(const std::filesystem::path& path) {
    std::string ext = toLower(path.extension().string());
    return kPlaylistExtensions.contains(ext);
}

std::vector<std::filesystem::path> findMatchingSubtitles(const std::filesystem::path& mediaPath) {
    std::vector<std::filesystem::path> matches;
    try {
        if (!std::filesystem::exists(mediaPath)) return matches;
        auto parent = mediaPath.parent_path();
        std::string stem = toLower(mediaPath.stem().string());

        for (const auto& entry : std::filesystem::directory_iterator(parent)) {
            if (!entry.is_regular_file()) continue;
            auto entryExt = toLower(entry.path().extension().string());
            if (!kSubtitleExtensions.contains(entryExt)) continue;

            std::string entryStem = toLower(entry.path().stem().string());
            // Match if entry stem starts with media stem (e.g. Movie.en.srt matches Movie.mp4)
            if (entryStem.find(stem) == 0) {
                matches.push_back(entry.path());
            }
        }
    } catch (...) {}
    return matches;
}

} // namespace rx::utils
