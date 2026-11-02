#pragma once

#include "rx/types.hpp"
#include "../audio/equalizer.hpp"
#include <string>
#include <mutex>

namespace rx {

struct WindowSettings {
    int x = 100;
    int y = 100;
    int width = 1280;
    int height = 720;
    bool maximized = false;
};

struct AudioSettings {
    int volume = 80;
    bool muted = false;
    std::string deviceId = "auto";
    bool exclusiveMode = false;
    double delayMs = 0.0;
    audio::EqualizerState equalizer;
    StereoDownmixConfig downmix;
};

struct VideoSettings {
    bool hwdecEnabled = true;
    std::string hwdecApi = "auto-safe";
    std::string renderer = "gpu";
    VideoAdjustmentParams adjustments;
};

struct PlaybackSettings {
    bool rememberPosition = true;
    double seekStepSec = 5.0;
    double speed = 1.0;
    bool autoPlayNext = true;
    int repeatMode = 0; // 0=Off, 1=All, 2=One
    bool shuffle = false;
};

struct SubtitleSettings {
    bool autoLoad = true;
    SubtitleStyle style;
    int fontSize = 28;
    std::string fontName = "Segoe UI";
    std::string primaryColor = "#FFFFFF";
    std::string outlineColor = "#000000";
    double delayMs = 0.0;
};

struct OpenSubtitlesSettings {
    std::string apiKey = "";
    std::string username = "";
    std::string userToken = "";
    std::string preferredLanguages = "en,hi";
    bool autoSearch = true;
    bool applyAfterDownload = true;
};

class Settings {
public:
    static Settings& instance();

    void load(const std::string& path = "");
    void save(const std::string& path = "");

    std::string getFilePath() const { return m_filePath; }

    WindowSettings window;
    AudioSettings audio;
    AudioChannelMode channelMode = AudioChannelMode::Both;
    VideoSettings video;
    PlaybackSettings playback;
    SubtitleSettings subtitle;
    OpenSubtitlesSettings opensubtitles;
    NetworkStreamConfig network;

    std::string lastOpenDirectory;
    bool showPlaylistPanel = true;

private:
    Settings();
    ~Settings() = default;

    std::mutex m_mutex;
    std::string m_filePath;
};

} // namespace rx
