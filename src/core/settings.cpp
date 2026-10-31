#include "settings.hpp"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

namespace rx {

namespace {
    std::string getDefaultSettingsPath() {
        PWSTR appDataPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appDataPath))) {
            std::filesystem::path dir(appDataPath);
            CoTaskMemFree(appDataPath);
            dir /= "RXMediaPlayer";
            std::filesystem::create_directories(dir);
            return (dir / "rx_settings.ini").string();
        }
        return "rx_settings.ini";
    }

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
}

Settings::Settings() {
    m_filePath = getDefaultSettingsPath();
}

Settings& Settings::instance() {
    static Settings s_inst;
    return s_inst;
}

void Settings::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!path.empty()) m_filePath = path;

    std::ifstream file(m_filePath);
    if (!file.is_open()) return;

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eqPos));
        std::string val = trim(line.substr(eqPos + 1));

        if (section == "Window") {
            if (key == "x") window.x = std::stoi(val);
            else if (key == "y") window.y = std::stoi(val);
            else if (key == "width") window.width = std::stoi(val);
            else if (key == "height") window.height = std::stoi(val);
            else if (key == "maximized") window.maximized = (val == "1" || val == "true");
        } else if (section == "Audio") {
            if (key == "volume") audio.volume = std::stoi(val);
            else if (key == "muted") audio.muted = (val == "1" || val == "true");
            else if (key == "deviceId") audio.deviceId = val;
            else if (key == "exclusiveMode") audio.exclusiveMode = (val == "1" || val == "true");
            else if (key == "delayMs") audio.delayMs = std::stod(val);
            else if (key == "eqEnabled") audio.equalizer.enabled = (val == "1" || val == "true");
            else if (key == "eqPreset") audio.equalizer.preset = static_cast<audio::EqualizerPreset>(std::stoi(val));
            else if (key == "eqBass") audio.equalizer.bass = std::stod(val);
            else if (key == "eqTreble") audio.equalizer.treble = std::stod(val);
            else if (key == "eqNormalize") audio.equalizer.normalize = (val == "1" || val == "true");
            else if (key == "eqDownmix") audio.equalizer.stereoDownmix = (val == "1" || val == "true");
            else if (key == "eqBalance") audio.equalizer.leftRightBalance = std::stod(val);
            else if (key == "eqGains") {
                std::stringstream ss(val);
                std::string token;
                size_t idx = 0;
                while (std::getline(ss, token, ',') && idx < audio::kEqualizerBandCount) {
                    audio.equalizer.gains[idx++] = std::stod(trim(token));
                }
            }
        } else if (section == "Video") {
            if (key == "hwdecEnabled") video.hwdecEnabled = (val == "1" || val == "true");
            else if (key == "hwdecApi") video.hwdecApi = val;
            else if (key == "renderer") video.renderer = val;
            else if (key == "aspectRatio") video.adjustments.aspectRatio = val;
            else if (key == "brightness") video.adjustments.brightness = std::stoi(val);
            else if (key == "contrast") video.adjustments.contrast = std::stoi(val);
            else if (key == "saturation") video.adjustments.saturation = std::stoi(val);
            else if (key == "gamma") video.adjustments.gamma = std::stoi(val);
            else if (key == "hue") video.adjustments.hue = std::stoi(val);
            else if (key == "sharpness") video.adjustments.sharpness = std::stoi(val);
            else if (key == "rotate") video.adjustments.rotate = std::stoi(val);
            else if (key == "flipHorizontal") video.adjustments.flipHorizontal = (val == "1" || val == "true");
            else if (key == "flipVertical") video.adjustments.flipVertical = (val == "1" || val == "true");
            else if (key == "deinterlace") video.adjustments.deinterlace = (val == "1" || val == "true");
            else if (key == "zoom") video.adjustments.zoom = std::stod(val);
            else if (key == "panX") video.adjustments.panX = std::stod(val);
            else if (key == "panY") video.adjustments.panY = std::stod(val);
        } else if (section == "Playback") {
            if (key == "rememberPosition") playback.rememberPosition = (val == "1" || val == "true");
            else if (key == "seekStepSec") playback.seekStepSec = std::stod(val);
            else if (key == "speed") playback.speed = std::stod(val);
            else if (key == "autoPlayNext") playback.autoPlayNext = (val == "1" || val == "true");
            else if (key == "repeatMode") playback.repeatMode = std::stoi(val);
            else if (key == "shuffle") playback.shuffle = (val == "1" || val == "true");
        } else if (section == "Subtitle") {
            if (key == "autoLoad") subtitle.autoLoad = (val == "1" || val == "true");
            else if (key == "fontSize") { subtitle.fontSize = std::stoi(val); subtitle.style.fontSize = std::stoi(val); }
            else if (key == "fontName") { subtitle.fontName = val; subtitle.style.font = val; }
            else if (key == "primaryColor") { subtitle.primaryColor = val; subtitle.style.textColor = val; }
            else if (key == "outlineColor") { subtitle.outlineColor = val; subtitle.style.borderColor = val; }
            else if (key == "borderSize") subtitle.style.borderSize = std::stoi(val);
            else if (key == "shadowOffset") subtitle.style.shadowOffset = std::stoi(val);
            else if (key == "marginVertical") subtitle.style.marginVertical = std::stoi(val);
            else if (key == "bold") subtitle.style.bold = (val == "1" || val == "true");
            else if (key == "italic") subtitle.style.italic = (val == "1" || val == "true");
            else if (key == "delayMs") { subtitle.delayMs = std::stod(val); subtitle.style.delaySec = std::stod(val) / 1000.0; }
        } else if (section == "Network") {
            if (key == "cacheSec") network.cacheSec = std::stoi(val);
            else if (key == "maxBufferMb") network.maxBufferMb = std::stoi(val);
        } else if (section == "General") {
            if (key == "lastOpenDirectory") lastOpenDirectory = val;
            else if (key == "showPlaylistPanel") showPlaylistPanel = (val == "1" || val == "true");
        }
    }
}

void Settings::save(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!path.empty()) m_filePath = path;

    std::ofstream file(m_filePath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return;

    file << "; RX Media Player Settings File\n\n";

    file << "[General]\n";
    file << "lastOpenDirectory=" << lastOpenDirectory << "\n";
    file << "showPlaylistPanel=" << (showPlaylistPanel ? "1" : "0") << "\n\n";

    file << "[Window]\n";
    file << "x=" << window.x << "\n";
    file << "y=" << window.y << "\n";
    file << "width=" << window.width << "\n";
    file << "height=" << window.height << "\n";
    file << "maximized=" << (window.maximized ? "1" : "0") << "\n\n";

    file << "[Audio]\n";
    file << "volume=" << audio.volume << "\n";
    file << "muted=" << (audio.muted ? "1" : "0") << "\n";
    file << "deviceId=" << audio.deviceId << "\n";
    file << "exclusiveMode=" << (audio.exclusiveMode ? "1" : "0") << "\n";
    file << "delayMs=" << audio.delayMs << "\n";
    file << "eqEnabled=" << (audio.equalizer.enabled ? "1" : "0") << "\n";
    file << "eqPreset=" << static_cast<int>(audio.equalizer.preset) << "\n";
    file << "eqBass=" << audio.equalizer.bass << "\n";
    file << "eqTreble=" << audio.equalizer.treble << "\n";
    file << "eqNormalize=" << (audio.equalizer.normalize ? "1" : "0") << "\n";
    file << "eqDownmix=" << (audio.equalizer.stereoDownmix ? "1" : "0") << "\n";
    file << "eqBalance=" << audio.equalizer.leftRightBalance << "\n";
    file << "eqGains=";
    for (size_t i = 0; i < audio::kEqualizerBandCount; ++i) {
        if (i > 0) file << ",";
        file << audio.equalizer.gains[i];
    }
    file << "\n\n";

    file << "[Video]\n";
    file << "hwdecEnabled=" << (video.hwdecEnabled ? "1" : "0") << "\n";
    file << "hwdecApi=" << video.hwdecApi << "\n";
    file << "renderer=" << video.renderer << "\n";
    file << "aspectRatio=" << video.adjustments.aspectRatio << "\n";
    file << "brightness=" << video.adjustments.brightness << "\n";
    file << "contrast=" << video.adjustments.contrast << "\n";
    file << "saturation=" << video.adjustments.saturation << "\n";
    file << "gamma=" << video.adjustments.gamma << "\n";
    file << "hue=" << video.adjustments.hue << "\n";
    file << "sharpness=" << video.adjustments.sharpness << "\n";
    file << "rotate=" << video.adjustments.rotate << "\n";
    file << "flipHorizontal=" << (video.adjustments.flipHorizontal ? "1" : "0") << "\n";
    file << "flipVertical=" << (video.adjustments.flipVertical ? "1" : "0") << "\n";
    file << "deinterlace=" << (video.adjustments.deinterlace ? "1" : "0") << "\n";
    file << "zoom=" << video.adjustments.zoom << "\n";
    file << "panX=" << video.adjustments.panX << "\n";
    file << "panY=" << video.adjustments.panY << "\n\n";

    file << "[Playback]\n";
    file << "rememberPosition=" << (playback.rememberPosition ? "1" : "0") << "\n";
    file << "seekStepSec=" << playback.seekStepSec << "\n";
    file << "speed=" << playback.speed << "\n";
    file << "autoPlayNext=" << (playback.autoPlayNext ? "1" : "0") << "\n";
    file << "repeatMode=" << playback.repeatMode << "\n";
    file << "shuffle=" << (playback.shuffle ? "1" : "0") << "\n\n";

    file << "[Subtitle]\n";
    file << "autoLoad=" << (subtitle.autoLoad ? "1" : "0") << "\n";
    file << "fontSize=" << subtitle.style.fontSize << "\n";
    file << "fontName=" << subtitle.style.font << "\n";
    file << "primaryColor=" << subtitle.style.textColor << "\n";
    file << "outlineColor=" << subtitle.style.borderColor << "\n";
    file << "borderSize=" << subtitle.style.borderSize << "\n";
    file << "shadowOffset=" << subtitle.style.shadowOffset << "\n";
    file << "marginVertical=" << subtitle.style.marginVertical << "\n";
    file << "bold=" << (subtitle.style.bold ? "1" : "0") << "\n";
    file << "italic=" << (subtitle.style.italic ? "1" : "0") << "\n";
    file << "delayMs=" << (subtitle.style.delaySec * 1000.0) << "\n\n";

    file << "[Network]\n";
    file << "cacheSec=" << network.cacheSec << "\n";
    file << "maxBufferMb=" << network.maxBufferMb << "\n";

    file.flush();
}

} // namespace rx
