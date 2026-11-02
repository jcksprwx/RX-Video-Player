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

#ifndef WINHTTP_OPTION_DECOMPRESSION
#define WINHTTP_OPTION_DECOMPRESSION 118
#define WINHTTP_DECOMPRESSION_FLAG_GZIP 0x00000001
#define WINHTTP_DECOMPRESSION_FLAG_DEFLATE 0x00000002
#endif

namespace rx::subtitles {

namespace {

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

// Universal WinHTTP request helper
bool executeHttpRequest(
    const std::wstring& verb,
    const std::wstring& host,
    const std::wstring& path,
    const std::string& apiKey,
    const std::string& userToken,
    const std::string& postBody,
    std::string& outBody,
    int& outStatusCode,
    std::string* outError
) {
    outBody.clear();
    outStatusCode = 0;

    HINTERNET hSession = WinHttpOpen(
        L"RXMediaPlayer/2.5.0 (Windows NT 10.0; Win64; x64)",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0
    );
    if (!hSession) {
        if (outError) *outError = "Failed to initialize WinHTTP session (Error: " + std::to_string(GetLastError()) + ")";
        return false;
    }

    WinHttpSetTimeouts(hSession, 10000, 10000, 15000, 25000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        if (outError) *outError = "Failed to connect to host: " + rx::utils::wideToUtf8(host);
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, verb.c_str(), path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );
    if (!hRequest) {
        if (outError) *outError = "Failed to create HTTP request";
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Enable transparent gzip/deflate decompression in Windows HTTP engine
    DWORD decomp = WINHTTP_DECOMPRESSION_FLAG_GZIP | WINHTTP_DECOMPRESSION_FLAG_DEFLATE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_DECOMPRESSION, &decomp, sizeof(decomp));

    std::wstring headers = L"User-Agent: RXMediaPlayer v2.5.0\r\nAccept: application/json\r\n";
    if (!apiKey.empty()) {
        headers += L"Api-Key: " + rx::utils::utf8ToWide(apiKey) + L"\r\n";
    }
    if (!userToken.empty()) {
        headers += L"Authorization: Bearer " + rx::utils::utf8ToWide(userToken) + L"\r\n";
    }
    if (verb == L"POST" || verb == L"PUT") {
        headers += L"Content-Type: application/json\r\n";
    }

    LPVOID pData = postBody.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)postBody.c_str();
    DWORD dataLen = static_cast<DWORD>(postBody.length());

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.length()),
                                   pData, dataLen, dataLen, 0);
    if (!sent) {
        if (outError) *outError = "Network connection failed. Could not reach server.";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        if (outError) *outError = "Server did not respond to request.";
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

    file.seekg(0, std::ios::beg);
    uint64_t buffer[8192];
    file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
    std::streamsize bytesRead = file.gcount();
    size_t count = bytesRead / sizeof(uint64_t);
    for (size_t i = 0; i < count; ++i) {
        hash += buffer[i];
    }

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

    std::regex yearRegex(R"([\.\s\(\[_](19\d\d|20\d\d)[\.\s\)\]_])");
    if (std::regex_search(stem, match, yearRegex)) {
        info.year = std::stoi(match[1].str());
        stem = stem.substr(0, match.position());
    }

    for (char& c : stem) {
        if (c == '.' || c == '_' || c == '-') c = ' ';
    }

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

    size_t first = stem.find_first_not_of(" \t\r\n-_.");
    if (first == std::string::npos) {
        stem = "";
    } else {
        size_t last = stem.find_last_not_of(" \t\r\n-_.");
        stem = stem.substr(first, (last - first + 1));
    }
    info.cleanTitle = stem.empty() ? p.stem().string() : stem;
    info.title = info.cleanTitle;
    return info;
}

bool OpenSubtitlesClient::isValidApiKey(const std::string& key) {
    if (key.length() < 16) return false;
    for (char c : key) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
    }
    return true;
}

bool OpenSubtitlesClient::testConnection(
    const std::string& apiKey,
    const std::string& userToken,
    UserAccountInfo* outInfo,
    std::string* outError
) {
    if (apiKey.empty()) {
        if (outError) *outError = "OpenSubtitles API key is required";
        return false;
    }

    std::string responseBody;
    int statusCode = 0;

    if (!userToken.empty()) {
        // Authenticated user check: GET /api/v1/infos/user
        bool ok = executeHttpRequest(L"GET", L"api.opensubtitles.com", L"/api/v1/infos/user",
                                     apiKey, userToken, "", responseBody, statusCode, outError);
        if (!ok) return false;

        if (statusCode == 200) {
            if (outInfo) {
                outInfo->isLoggedIn = true;
                outInfo->token = userToken;
                outInfo->remainingDownloads = extractJsonInt(responseBody, "remaining_downloads", 0);
                outInfo->allowedDownloads = extractJsonInt(responseBody, "allowed_downloads", 0);
                outInfo->isVip = extractJsonBool(responseBody, "vip", false);
                outInfo->statusMessage = "Authenticated (" + std::to_string(outInfo->remainingDownloads) +
                                         "/" + std::to_string(outInfo->allowedDownloads) + " downloads remaining)";
            }
            return true;
        } else if (statusCode == 401 || statusCode == 403) {
            if (outError) *outError = "User session expired or unauthorized. Please log in again.";
            return false;
        } else {
            if (outError) *outError = "OpenSubtitles returned HTTP " + std::to_string(statusCode);
            return false;
        }
    } else {
        // Anonymous API Key check: GET /api/v1/formats
        bool ok = executeHttpRequest(L"GET", L"api.opensubtitles.com", L"/api/v1/formats",
                                     apiKey, "", "", responseBody, statusCode, outError);
        if (!ok) return false;

        if (statusCode == 200) {
            if (outInfo) {
                outInfo->isLoggedIn = false;
                outInfo->statusMessage = "Connected anonymously to OpenSubtitles.com (API Key Valid)";
            }
            return true;
        } else if (statusCode == 401 || statusCode == 403) {
            if (outError) *outError = "Invalid OpenSubtitles API key (HTTP " + std::to_string(statusCode) + ")";
            return false;
        } else {
            if (outError) *outError = "Server returned HTTP " + std::to_string(statusCode);
            return false;
        }
    }
}

bool OpenSubtitlesClient::login(
    const std::string& apiKey,
    const std::string& username,
    const std::string& password,
    UserAccountInfo* outInfo,
    std::string* outError
) {
    if (apiKey.empty()) {
        if (outError) *outError = "OpenSubtitles API key is required";
        return false;
    }
    if (username.empty() || password.empty()) {
        if (outError) *outError = "Username and password cannot be empty";
        return false;
    }

    std::string postJson = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    std::string responseBody;
    int statusCode = 0;

    bool ok = executeHttpRequest(L"POST", L"api.opensubtitles.com", L"/api/v1/login",
                                 apiKey, "", postJson, responseBody, statusCode, outError);
    if (!ok) return false;

    if (statusCode == 200) {
        std::string token = extractJsonString(responseBody, "token");
        if (token.empty()) {
            if (outError) *outError = "Failed to extract session token from response";
            return false;
        }

        if (outInfo) {
            outInfo->isLoggedIn = true;
            outInfo->username = username;
            outInfo->token = token;
            outInfo->remainingDownloads = extractJsonInt(responseBody, "remaining_downloads", 20);
            outInfo->allowedDownloads = extractJsonInt(responseBody, "allowed_downloads", 20);
            outInfo->isVip = extractJsonBool(responseBody, "vip", false);
            outInfo->statusMessage = "Logged in as " + username + (outInfo->isVip ? " [VIP]" : " [Standard]");
        }
        return true;
    } else if (statusCode == 401 || statusCode == 400) {
        if (outError) *outError = "Invalid username or password";
        return false;
    } else {
        if (outError) *outError = "Login failed (HTTP " + std::to_string(statusCode) + ")";
        return false;
    }
}

bool OpenSubtitlesClient::logout(
    const std::string& apiKey,
    const std::string& userToken,
    std::string* outError
) {
    if (userToken.empty()) return true;

    std::string responseBody;
    int statusCode = 0;
    executeHttpRequest(L"DELETE", L"api.opensubtitles.com", L"/api/v1/logout",
                       apiKey, userToken, "", responseBody, statusCode, outError);
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
    const std::string& userToken,
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
    std::wstring pathW = rx::utils::utf8ToWide(path);

    bool ok = executeHttpRequest(L"GET", L"api.opensubtitles.com", pathW,
                                 apiKey, userToken, "", responseBody, statusCode, outError);
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

    size_t dataPos = responseBody.find("\"data\":[");
    if (dataPos == std::string::npos) {
        dataPos = responseBody.find("\"data\": [");
    }
    if (dataPos == std::string::npos) {
        return results;
    }

    size_t start = responseBody.find('{', dataPos);
    while (start != std::string::npos) {
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

        start = responseBody.find('{', end + 1);
    }

    return results;
}

bool OpenSubtitlesClient::downloadSubtitle(
    const std::string& apiKey,
    int fileId,
    const std::string& destFilePath,
    const std::string& userToken,
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

    bool ok = executeHttpRequest(L"POST", L"api.opensubtitles.com", L"/api/v1/download",
                                 apiKey, userToken, postJson, responseBody, statusCode, outError);
    if (!ok) return false;

    if (statusCode == 401 || statusCode == 403) {
        if (outError) *outError = "Unauthorized API key or download quota exceeded";
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

    ok = executeHttpRequest(L"GET", hostW, pathW, "", "", "", subContent, dlStatus, outError);
    if (!ok || dlStatus != 200 || subContent.empty()) {
        if (outError && outError->empty()) {
            *outError = "Failed to download subtitle file from CDN (HTTP " + std::to_string(dlStatus) + ")";
        }
        return false;
    }

    // Determine target save path
    std::filesystem::path destP(destFilePath);
    if (destP.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(destP.parent_path(), ec);
    }

    // Save to disk
    std::ofstream out(destFilePath, std::ios::binary);
    if (!out.is_open()) {
        // Fallback to AppData/Subtitles if folder was not writeable
        wchar_t appData[MAX_PATH];
        GetEnvironmentVariableW(L"LOCALAPPDATA", appData, MAX_PATH);
        std::filesystem::path fallbackP = std::filesystem::path(appData) / "RX Media Player" / "Subtitles" / destP.filename();
        std::filesystem::create_directories(fallbackP.parent_path());
        out.open(fallbackP, std::ios::binary);
        if (!out.is_open()) {
            if (outError) *outError = "Failed to create destination file: " + destFilePath;
            return false;
        }
    }

    out.write(subContent.data(), subContent.size());
    out.close();
    return true;
}

} // namespace rx::subtitles
