#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

namespace rx {

inline constexpr const char* kAppVersion = "2.7.0";
inline constexpr const char* kAppAuthor  = "Rituraj";
inline constexpr const char* kAppName    = "RX Media Player";

enum class PlaybackState {
    Idle,
    Opening,
    Playing,
    Paused,
    Stopped,
    Error
};

enum class RepeatMode {
    Off,
    RepeatAll,
    RepeatOne
};

enum class AudioChannelMode {
    Both = 0,
    Left = 1,
    Right = 2
};

enum class AudioOutputMode {
    Auto = 0,               // Auto (Follow Source / Device)
    Stereo = 1,             // Stereo (2.0)
    Mono = 2,               // Mono (1.0)
    Original = 3            // Original Multichannel Output
};

struct StereoDownmixConfig {
    AudioOutputMode outputMode = AudioOutputMode::Auto;
    double centerMixLevel = 0.707;     // 0.0 to 2.0 (dialogue level, default 0.707 = -3dB)
    double surroundMixLevel = 0.707;   // 0.0 to 1.5 (surround level, default 0.707 = -3dB)
    bool includeLfe = false;           // Include LFE subwoofer channel (default false)
    double lfeMixLevel = 0.5;          // 0.0 to 1.0 (default 0.5 = -6dB)
    bool normalizeDownmix = true;      // Clipping protection (normalize coefficients)
    int outputSampleRate = 0;          // 0 = Auto, 44100, 48000, 96000, 192000
    bool globalSetting = true;         // Persist across sessions

    void reset() {
        outputMode = AudioOutputMode::Auto;
        centerMixLevel = 0.707;
        surroundMixLevel = 0.707;
        includeLfe = false;
        lfeMixLevel = 0.5;
        normalizeDownmix = true;
        outputSampleRate = 0;
        globalSetting = true;
    }
};

struct MediaTrack {
    int id = 0;
    std::string type;      // "video", "audio", "sub"
    std::string title;
    std::string lang;
    std::string codec;
    std::string codecDesc;
    int channels = 0;
    int sampleRate = 0;
    int bitrate = 0;
    bool selected = false;
    bool external = false;
    bool isDefault = false;
    bool isForced = false;
    std::string externalFilename;

    MediaTrack() = default;
    MediaTrack(int id_, std::string type_, std::string title_, std::string lang_, std::string codec_, bool sel = false, bool ext = false)
        : id(id_), type(std::move(type_)), title(std::move(title_)), lang(std::move(lang_)), codec(std::move(codec_)), selected(sel), external(ext) {}
};

struct MediaInfo {
    std::string filename;
    std::string fullPath;
    std::string container;
    double durationSec = 0.0;
    int videoWidth = 0;
    int videoHeight = 0;
    double videoFps = 0.0;
    std::string videoCodec;
    std::string videoPixelFormat;
    std::string audioCodec;
    int audioSampleRate = 0;
    int audioChannels = 0;
    int outAudioChannels = 0;
    std::string outAudioFormat;
    int outAudioSampleRate = 0;
    int64_t fileSize = 0;
    std::string hwdecActive = "no";
    std::vector<MediaTrack> tracks;
};

struct PlaybackProgress {
    double positionSec = 0.0;
    double durationSec = 0.0;
    double percent = 0.0;
    bool isSeeking = false;
};

struct VideoAdjustmentParams {
    int brightness = 0;    // -100 to 100
    int contrast = 0;      // -100 to 100
    int saturation = 0;    // -100 to 100
    int gamma = 0;         // -100 to 100
    int hue = 0;           // -100 to 100
    int sharpness = 0;     // -10 to 10
    int rotate = 0;        // 0, 90, 180, 270
    bool flipHorizontal = false;
    bool flipVertical = false;
    bool deinterlace = false;
    std::string aspectRatio = "auto"; // "auto", "16:9", "4:3", "21:9", "2.35:1", "1:1"
    double zoom = 1.0;     // 0.2 to 3.0
    double panX = 0.0;     // -1.0 to 1.0
    double panY = 0.0;     // -1.0 to 1.0

    void reset() {
        brightness = 0;
        contrast = 0;
        saturation = 0;
        gamma = 0;
        hue = 0;
        sharpness = 0;
        rotate = 0;
        flipHorizontal = false;
        flipVertical = false;
        deinterlace = false;
        aspectRatio = "auto";
        zoom = 1.0;
        panX = 0.0;
        panY = 0.0;
    }
};

struct SubtitleStyle {
    std::string font = "Segoe UI";
    int fontSize = 28;                   // 14 to 60 pt
    std::string textColor = "#FFFFFF";    // Hex RGB
    bool outlineEnabled = true;
    std::string borderColor = "#000000";  // Hex RGB
    int borderSize = 3;                  // 0 to 8 px
    bool shadowEnabled = true;
    std::string shadowColor = "#000000";  // Hex RGB
    int shadowOffset = 1;                // 0 to 6 px
    bool backgroundEnabled = false;
    std::string backgroundColor = "#000000"; // Hex RGB
    int backgroundOpacity = 60;          // 0 to 100 %
    int marginVertical = 36;             // 10 to 120 px
    int positionPercent = 100;           // 0 to 100 (mpv sub-pos)
    std::string alignment = "bottom-center"; // bottom-center, top-center, center, bottom-left, bottom-right
    bool bold = true;
    bool italic = false;
    double delaySec = 0.0;               // -10.0 to +10.0 s
    bool overrideAss = false;            // override embedded ASS/SSA styles
    bool autoLoadExternal = true;        // auto-load matching external subtitles
    std::string preferredLanguages = "en,hi"; // preferred languages

    void reset() {
        font = "Segoe UI";
        fontSize = 28;
        textColor = "#FFFFFF";
        outlineEnabled = true;
        borderColor = "#000000";
        borderSize = 3;
        shadowEnabled = true;
        shadowColor = "#000000";
        shadowOffset = 1;
        backgroundEnabled = false;
        backgroundColor = "#000000";
        backgroundOpacity = 60;
        marginVertical = 36;
        positionPercent = 100;
        alignment = "bottom-center";
        bold = true;
        italic = false;
        delaySec = 0.0;
        overrideAss = false;
        autoLoadExternal = true;
        preferredLanguages = "en,hi";
    }
};

struct NetworkStreamConfig {
    std::string url;
    int cacheSec = 15;                   // Demuxer readahead in seconds (5, 15, 30, 60)
    int maxBufferMb = 100;               // Demuxer max buffer in MB (50, 100, 250)
    int networkTimeoutSec = 30;
    std::string userAgent = "RXMediaPlayer/1.0 (Windows 64-bit)";
};

struct DiagnosticsReport {
    std::string filename;
    std::string fullPath;
    std::string container;
    int64_t fileSize = 0;
    double durationSec = 0.0;
    double currentPositionSec = 0.0;

    // Video Input
    std::string videoCodec;
    int videoWidth = 0;
    int videoHeight = 0;
    double videoFps = 0.0;
    std::string pixelFormat;
    int64_t videoBitrateBps = 0;
    std::string hwdecActive;
    std::string voDriver;

    // Video Output / Renderer
    int outVideoWidth = 0;
    int outVideoHeight = 0;
    std::string outPixelFormat;

    // Audio Input
    std::string audioCodec;
    int audioSampleRate = 0;
    int audioChannels = 0;
    std::string audioChannelLayout;
    std::string audioFormat;
    int64_t audioBitrateBps = 0;
    std::string aoDriver;

    // Audio Output / Renderer
    int outAudioSampleRate = 0;
    int outAudioChannels = 0;
    std::string outAudioChannelLayout;
    std::string outAudioFormat;
    int64_t outAudioBitrateBps = 0;

    // Performance & Stats
    int64_t droppedFrames = 0;
    int64_t voDroppedFrames = 0;
    int64_t mistimedFrames = 0;
    double avSyncDiffSec = 0.0;
    double estimatedVfFps = 0.0;

    // Active tracks & metadata
    std::vector<MediaTrack> tracks;
    std::vector<std::pair<std::string, std::string>> metadata;

    std::string formatReport() const {
        std::ostringstream ss;
        ss << "======================================================================\n";
        ss << "  " << kAppName << " v" << kAppVersion << " Diagnostics & Media Information\n";
        ss << "  Author: " << kAppAuthor << "\n";
        ss << "======================================================================\n\n";

        ss << "[1. Playback Information]\n";
        ss << "  Video Decoder: " << (hwdecActive.empty() || hwdecActive == "no" ? "Built-in Video Codec/Transform (ffmpeg)" : "Hardware Accelerated: D3D11VA (" + hwdecActive + ")") << "\n";
        ss << "  Video Codec: " << (videoCodec.empty() ? "N/A" : videoCodec) << "\n";
        ss << "  Video Input Type: " << (pixelFormat.empty() ? "N/A" : pixelFormat) << "\n";
        ss << "  Video Input Size: " << videoWidth << " x " << videoHeight << "\n";
        ss << "  Video Output Type: " << (outPixelFormat.empty() ? "Direct3D 11 NV12" : outPixelFormat) << "\n";
        ss << "  Video Output Size: " << (outVideoWidth > 0 ? outVideoWidth : videoWidth) << " x " << (outVideoHeight > 0 ? outVideoHeight : videoHeight) << "\n";
        ss << "  Source FPS: " << videoFps << " fps\n";
        ss << "  Actual FPS: " << estimatedVfFps << " fps\n";
        ss << "  Video Bitrate: " << (videoBitrateBps > 0 ? std::to_string(videoBitrateBps / 1000) + " kbps" : "Variable / N/A") << "\n";
        ss << "  Active Decoder: " << hwdecActive << "\n";
        ss << "  Video Output: " << voDriver << "\n\n";

        ss << "  Audio Decoder: Built-in Audio Decoder (ffmpeg)\n";
        ss << "  Audio Codec: " << (audioCodec.empty() ? "N/A" : audioCodec) << "\n";
        ss << "  Sample Rate: " << audioSampleRate << " Hz -> " << (outAudioSampleRate > 0 ? outAudioSampleRate : audioSampleRate) << " Hz\n";
        ss << "  Channels: " << audioChannels << " (" << (audioChannelLayout.empty() ? (audioChannels == 2 ? "Stereo" : (audioChannels == 6 ? "5.1" : "Multi")) : audioChannelLayout) << ") -> " << (outAudioChannels > 0 ? outAudioChannels : audioChannels) << " channels\n";
        ss << "  Audio Bitrate: " << (audioBitrateBps > 0 ? std::to_string(audioBitrateBps / 1000) + " kbps" : "N/A") << "\n";
        ss << "  Audio Format: " << (audioFormat.empty() ? "16-bit PCM" : audioFormat) << "\n";
        ss << "  Audio Output: " << aoDriver << "\n\n";

        ss << "[2. File Information]\n";
        ss << "  File: " << filename << "\n";
        ss << "  Path: " << fullPath << "\n";
        ss << "  Container: " << container << "\n";
        ss << "  Size: " << (fileSize / (1024.0 * 1024.0)) << " MB (" << fileSize << " bytes)\n";
        ss << "  Duration: " << durationSec << " s (Pos: " << currentPositionSec << " s)\n";
        if (!tracks.empty()) {
            ss << "  Streams:\n";
            for (const auto& tr : tracks) {
                ss << "    Stream #" << tr.id << " [" << tr.type << "]: " << tr.codec << " (" << (tr.title.empty() ? tr.lang : tr.title) << ")" << (tr.selected ? " [Selected]" : "") << "\n";
            }
        }
        if (!metadata.empty()) {
            ss << "  Tags:\n";
            for (const auto& md : metadata) {
                ss << "    " << md.first << ": " << md.second << "\n";
            }
        }
        ss << "\n";

        ss << "[3. Performance & System Telemetry]\n";
        ss << "  Decoder Dropped Frames: " << droppedFrames << "\n";
        ss << "  VO Dropped Frames: " << voDroppedFrames << "\n";
        ss << "  Mistimed Frames: " << mistimedFrames << "\n";
        ss << "  A/V Sync Offset: " << (avSyncDiffSec * 1000.0) << " ms\n";
        ss << "  Display Frame Rate: " << estimatedVfFps << " fps\n";
        ss << "======================================================================\n";
        return ss.str();
    }
};

} // namespace rx
