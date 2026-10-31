#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace rx::utils {

// Time formatting: e.g. 75.0 -> "01:15", 3661.0 -> "01:01:01"
std::string formatTime(double seconds);

// Byte size formatting: e.g. 1048576 -> "1.00 MB"
std::string formatBytes(int64_t bytes);

// UTF-8 <-> UTF-16 (Windows Wide String) conversions
std::wstring utf8ToWide(std::string_view utf8Str);
std::string wideToUtf8(std::wstring_view wideStr);

// File classification
bool isSupportedMediaFile(const std::filesystem::path& path);
bool isSubtitleFile(const std::filesystem::path& path);
bool isPlaylistFile(const std::filesystem::path& path);

// Fuzzy subtitle detection: looks for files in same directory with same stem and subtitle extensions
std::vector<std::filesystem::path> findMatchingSubtitles(const std::filesystem::path& mediaPath);

// Clamp helper
template <typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

} // namespace rx::utils
