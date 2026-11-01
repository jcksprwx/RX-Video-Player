#include "opensubtitles_client.hpp"
#include "../core/track_formatter.hpp"
#include "../core/utils.hpp"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <algorithm>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")

namespace rx::subtitles {

namespace {

// Lightweight JSON token and value extractor
std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";

    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }

    if (pos < json.length() && json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end != std::string::npos) {
            std::string val = json.substr(pos, end - pos);
            // Replace escaped quotes
            size_t slash = 0;
            while ((slash = val.find("\\\"", slash)) != std::string::npos) {
                val.replace(slash, 2, "\"");
                slash++;
            }
            return val;
        }
    }
    return "";
}

int extractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n' || json[pos] == '"')) {
        pos++;
    }

    size_t end = pos;
    while (end < json.length() && (isdigit(json[end]) || json[end] == '-')) {
        end++;
    }
    if (end > pos) {
        try {
            return std::stoi(json.substr(pos, end - pos));
        } catch (...) {}
    }
    return defaultVal;
}

double extractJsonDouble(const std::string& json, const std::string& key, double defaultVal = 0.0) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n' || json[pos] == '"')) {
        pos++;
    }

    size_t end = pos;
    while (end < json.length() && (isdigit(json[end]) || json[end] == '.' || json[end] == '-')) {
        end++;
    }
    if (end > pos) {
        try {
            return std::stod(json.substr(pos, end - pos));
        } catch (...) {}
    }
    return defaultVal;
}

bool extractJsonBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    std::string needle = "\"" + key + "\":";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return defaultVal;

    pos += needle.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\r' || json[pos] == '\n')) {
        pos++;
    }

    if (json.compare(pos, 4, "true") == 0) return true;
    if (json.compare(pos, 5, "false") == 0) return false;
    return defaultVal;
}

// WinHTTP HTTPS Request Helper
bool httpGetRequest(const std::wstring& host, const std::wstring& path,
                    const std::string& apiKey, std::string& outBody, int& outStatusCode, std::string* outError) {
    outBody.clear();
    outStatusCode = 0;

    HINTERNET hSession = WinHttpOpen(L"RXMediaPlayer/2.4.0 (Windows NT 10.0; Win64; x64)",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        if (outError) *outError = "Failed to initialize WinHTTP session (Error: " + std::to_string(GetLastError()) + ")";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        if (outError) *outError = "Failed to connect to host: " + rx::utils::wideToUtf8(host);
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        if (outError) *outError = "Failed to create HTTP request";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set headers
    std::wstring headers = L"User-Agent: RXMediaPlayer v2.4.0\r\nAccept: application/json\r\n";
    if (!apiKey.empty()) {
        headers += L"Api-Key: " + rx::utils::utf8ToWide(apiKey) + L"\r\n";
    }

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()),
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent) {
        DWORD err = GetLastError();
        if (outError) *outError = "Failed to send HTTP request (Error " + std::to_string(err) + ")";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        if (outError) *outError = "Failed to receive HTTP response from server";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outStatusCode = static_cast<int>(status);

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            outBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

bool httpPostRequest(const std::wstring& host, const std::wstring& path,
                     const std::string& apiKey, const std::string& postJson,
                     std::string& outBody, int& outStatusCode, std::string* outError) {
    outBody.clear();
    outStatusCode = 0;

    HINTERNET hSession = WinHttpOpen(L"RXMediaPlayer/2.4.0 (Windows NT 10.0; Win64; x64)",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        if (outError) *outError = "Failed to initialize WinHTTP session";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        if (outError) *outError = "Failed to connect to host: " + rx::utils::wideToUtf8(host);
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        if (outError) *outError = "Failed to create HTTP POST request";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json\r\nUser-Agent: RXMediaPlayer v2.4.0\r\n";
    if (!apiKey.empty()) {
        headers += L"Api-Key: " + rx::utils::utf8ToWide(apiKey) + L"\r\n";
    }

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()),
                                  (LPVOID)postJson.c_str(), static_cast<DWORD>(postJson.length()),
                                  static_cast<DWORD>(postJson.length()), 0);
    if (!sent) {
        if (outError) *outError = "Failed to send POST request";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        if (outError) *outError = "Failed to receive POST response";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outStatusCode = static_cast<int>(status);

    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead) && bytesRead > 0) {
            outBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// URL-encoder helper
std::string urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

} // anonymous namespace

uint64_t OpenSubtitlesClient::computeMovieHash(const std::string& filePath, uint64_t& outFileSize) {
    outFileSize = 0;
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return 0;

    file.seekg(0, std::ios::end);
    std::streampos endPos = file.tellg();
    if (endPos <= 0) return 0;

    uint64_t fileSize = static_cast<uint64_t>(endPos);
    outFileSize = fileSize;

    if (fileSize < 65536) return 0;

    uint64_t hash = fileSize;

    // Read first 64 KB (8192 64-bit uints)
    file.seekg(0, std::ios::beg);
    uint64_t buffer[8192];
    file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    std::streamsize bytesRead = file.gcount();
    size_t count = bytesRead / sizeof(uint64_t);
    for (size_t i = 0; i < count; ++i) {
        hash += buffer[i];
    }

    // Read last 64 KB (8192 64-bit uints)
    uint64_t lastOffset = fileSize - 65536;
    file.seekg(lastOffset, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    bytesRead = file.gcount();
    count = bytesRead / sizeof(uint64_t);
    for (size_t i = 0; i < count; ++i) {
        hash += buffer[i];
    }

    return hash;
}

std::string OpenSubtitlesClient::formatMovieHash(uint64_t hash) {
    if (hash == 0) return "";
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return ss.str();
}

ParsedMediaInfo OpenSubtitlesClient::parseFilename(const std::string& filePath) {
    ParsedMediaInfo info;
    std::filesystem::path p(filePath);
    std::string stem = p.stem().string();
    info.rawFilename = p.filename().string();

    // Check for TV episode pattern: S01E02, s1e2, 1x02
    std::smatch match;
    std::regex tvRegex(R"([sS](\d{1,2})[eE](\d{1,2}))");
    if (std::regex_search(stem, match, tvRegex)) {
        info.season = std::stoi(match[1].str());
        info.episode = std::stoi(match[2].str());
        stem = stem.substr(0, match.position());
    } else {
        std::regex altTvRegex(R"((\d{1,2})[xX](\d{1,2}))");
        if (std::regex_search(stem, match, altTvRegex)) {
            info.season = std::stoi(match[1].str());
            info.episode = std::stoi(match[2].str());
            stem = stem.substr(0, match.position());
        }
    }

    // Check for 4-digit Year: (19xx or 20xx)
    std::regex yearRegex(R"([\.\s\(\[_](19\d\d|20\d\d)[\.\s\)\]_])");
    if (std::regex_search(stem, match, yearRegex)) {
        info.year = std::stoi(match[1].str());
        stem = stem.substr(0, match.position());
    }

    // Replace dots, underscores, dashes with spaces
    for (char& c : stem) {
        if (c == '.' || c == '_' || c == '-') c = ' ';
    }

    // Remove common release junk tags
    std::vector<std::string> junk = {
        "1080p", "720p", "2160p", "4k", "bluray", "blu ray", "webrip", "web rip", "web dl",
        "webdl", "hdrip", "x264", "x265", "hevc", "dvdrip", "proper", "repack", "ac3", "dts"
    };

    std::string lower = stem;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto& j : junk) {
        size_t pos = lower.find(j);
        if (pos != std::string::npos) {
            stem = stem.substr(0, pos);
            lower = lower.substr(0, pos);
        }
    }

    // Clean up whitespace and edge punctuation
    size_t first = stem.find_first_not_of(" \t\r\n-_.");
    if (first == std::string::npos) {
        stem = "";
    } else {
        size_t last = stem.find_last_not_of(" \t\r\n-_.");
        stem = stem.substr(first, (last - first + 1));
    }
    info.cleanTitle = stem.empty() ? p.stem().string() : stem;
    return info;
}

bool OpenSubtitlesClient::isValidApiKey(const std::string& key) {
    if (key.length() < 16) return false;
    for (char c : key) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
    }
    return true;
}

std::vector<SubtitleSearchResult> OpenSubtitlesClient::searchSubtitles(
    const std::string& apiKey,
    const std::string& query,
    const std::string& languages,
    const std::string& movieHash,
    int season,
    int episode,
    int year,
    std::string* outError
) {
    std::vector<SubtitleSearchResult> results;

    if (apiKey.empty()) {
        if (outError) *outError = "OpenSubtitles API Key is required. Please set your key in Preferences or dialog.";
        return results;
    }

    std::string path = "/api/v1/subtitles?";
    bool hasParam = false;

    if (!movieHash.empty()) {
        path += "moviehash=" + urlEncode(movieHash);
        hasParam = true;
    }

    if (!query.empty()) {
        if (hasParam) path += "&";
        path += "query=" + urlEncode(query);
        hasParam = true;
    }

    if (!languages.empty() && languages != "all") {
        if (hasParam) path += "&";
        path += "languages=" + urlEncode(languages);
        hasParam = true;
    }

    if (season > 0) {
        if (hasParam) path += "&";
        path += "season_number=" + std::to_string(season);
        hasParam = true;
    }

    if (episode > 0) {
        if (hasParam) path += "&";
        path += "episode_number=" + std::to_string(episode);
        hasParam = true;
    }

    if (year > 0) {
        if (hasParam) path += "&";
        path += "year=" + std::to_string(year);
        hasParam = true;
    }

    std::string responseBody;
    int statusCode = 0;
    std::wstring host = L"api.opensubtitles.com";
    std::wstring pathW = rx::utils::utf8ToWide(path);

    bool ok = httpGetRequest(host, pathW, apiKey, responseBody, statusCode, outError);
    if (!ok) return results;

    if (statusCode == 401 || statusCode == 403) {
        if (outError) *outError = "Invalid or expired OpenSubtitles API Key (HTTP " + std::to_string(statusCode) + ")";
        return results;
    }
    if (statusCode == 406) {
        if (outError) *outError = "OpenSubtitles request rejected (HTTP 406 User-Agent missing/invalid)";
        return results;
    }
    if (statusCode == 429) {
        if (outError) *outError = "OpenSubtitles rate limit exceeded. Please wait a minute.";
        return results;
    }
    if (statusCode != 200) {
        if (outError) *outError = "Server returned HTTP " + std::to_string(statusCode);
        return results;
    }

    // Parse array of items under `"data": [ { ... }, { ... } ]`
    size_t dataPos = responseBody.find("\"data\":[");
    if (dataPos == std::string::npos) {
        dataPos = responseBody.find("\"data\": [");
    }
    if (dataPos == std::string::npos) {
        return results;
    }

    // Split objects in data array
    size_t start = responseBody.find('{', dataPos);
    while (start != std::string::npos) {
        // Find matching closing brace
        int depth = 0;
        size_t end = std::string::npos;
        for (size_t i = start; i < responseBody.length(); ++i) {
            if (responseBody[i] == '{') depth++;
            else if (responseBody[i] == '}') {
                depth--;
                if (depth == 0) {
                    end = i;
                    break;
                }
            }
        }
        if (end == std::string::npos) break;

        std::string itemJson = responseBody.substr(start, end - start + 1);

        SubtitleSearchResult res;
        res.id = extractJsonString(itemJson, "id");
        res.language = extractJsonString(itemJson, "language");
        if (res.language.empty()) res.language = "en";
        res.languageName = track_formatter::getLanguageDisplayName(res.language);
        res.format = extractJsonString(itemJson, "format");
        if (res.format.empty()) res.format = "SRT";
        std::transform(res.format.begin(), res.format.end(), res.format.begin(), ::toupper);

        res.release = extractJsonString(itemJson, "release");
        res.fileName = extractJsonString(itemJson, "file_name");
        if (res.fileName.empty()) {
            res.fileName = extractJsonString(itemJson, "movie_name");
            if (res.fileName.empty()) res.fileName = res.release;
        }

        res.fileId = extractJsonInt(itemJson, "file_id");
        res.downloadCount = extractJsonInt(itemJson, "download_count");
        res.rating = extractJsonDouble(itemJson, "ratings");
        res.hearingImpaired = extractJsonBool(itemJson, "hearing_impaired");
        res.hd = extractJsonBool(itemJson, "hd");

        if (res.fileId > 0 || !res.id.empty()) {
            results.push_back(res);
        }

        // Advance to next object
        start = responseBody.find('{', end + 1);
    }

    return results;
}

bool OpenSubtitlesClient::downloadSubtitle(
    const std::string& apiKey,
    int fileId,
    const std::string& destFilePath,
    std::string* outError
) {
    if (apiKey.empty()) {
        if (outError) *outError = "OpenSubtitles API Key is required";
        return false;
    }
    if (fileId <= 0) {
        if (outError) *outError = "Invalid file identifier";
        return false;
    }

    // Step 1: POST to /api/v1/download
    std::string postJson = "{\"file_id\":" + std::to_string(fileId) + "}";
    std::string responseBody;
    int statusCode = 0;

    bool ok = httpPostRequest(L"api.opensubtitles.com", L"/api/v1/download",
                              apiKey, postJson, responseBody, statusCode, outError);
    if (!ok) return false;

    if (statusCode == 401 || statusCode == 403) {
        if (outError) *outError = "Unauthorized API key or quota exceeded";
        return false;
    }
    if (statusCode != 200) {
        if (outError) *outError = "Download request failed (HTTP " + std::to_string(statusCode) + ")";
        return false;
    }

    std::string downloadUrl = extractJsonString(responseBody, "link");
    if (downloadUrl.empty()) {
        if (outError) *outError = "Server did not return a valid download link";
        return false;
    }

    // Step 2: Download raw subtitle content from downloadUrl
    // downloadUrl format: https://<cdn-host>/<path>
    std::string hostStr;
    std::string pathStr;
    if (downloadUrl.rfind("https://", 0) == 0) {
        size_t slashPos = downloadUrl.find('/', 8);
        if (slashPos != std::string::npos) {
            hostStr = downloadUrl.substr(8, slashPos - 8);
            pathStr = downloadUrl.substr(slashPos);
        } else {
            hostStr = downloadUrl.substr(8);
            pathStr = "/";
        }
    } else if (downloadUrl.rfind("http://", 0) == 0) {
        size_t slashPos = downloadUrl.find('/', 7);
        if (slashPos != std::string::npos) {
            hostStr = downloadUrl.substr(7, slashPos - 7);
            pathStr = downloadUrl.substr(slashPos);
        } else {
            hostStr = downloadUrl.substr(7);
            pathStr = "/";
        }
    } else {
        if (outError) *outError = "Malformed download URL";
        return false;
    }

    std::string subContent;
    int dlStatus = 0;
    std::wstring hostW = rx::utils::utf8ToWide(hostStr);
    std::wstring pathW = rx::utils::utf8ToWide(pathStr);

    ok = httpGetRequest(hostW, pathW, "", subContent, dlStatus, outError);
    if (!ok || dlStatus != 200 || subContent.empty()) {
        if (outError && outError->empty()) {
            *outError = "Failed to download subtitle file from CDN (HTTP " + std::to_string(dlStatus) + ")";
        }
        return false;
    }

    // Create parent directory if needed
    std::filesystem::path destP(destFilePath);
    if (destP.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(destP.parent_path(), ec);
    }

    // Save to disk
    std::ofstream out(destFilePath, std::ios::binary);
    if (!out.is_open()) {
        if (outError) *outError = "Failed to create destination file: " + destFilePath;
        return false;
    }

    out.write(subContent.data(), subContent.size());
    out.close();
    return true;
}

} // namespace rx::subtitles
