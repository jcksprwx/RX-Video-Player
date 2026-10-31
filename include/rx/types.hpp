#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

namespace rx {

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

struct MediaTrack {
    int id = 0;
    std::string type;      // "video", "audio", "sub"
    std::string title;
    std::string lang;
    std::string codec;
    bool selected = false;
    bool external = false;
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
    std::string borderColor = "#000000";  // Hex RGB
    int borderSize = 3;                  // 0 to 8 px
    int shadowOffset = 1;                // 0 to 6 px
    int marginVertical = 36;             // 10 to 120 px
    bool bold = true;
    bool italic = false;
    double delaySec = 0.0;               // -10.0 to +10.0 s

    void reset() {
        font = "Segoe UI";
        fontSize = 28;
        textColor = "#FFFFFF";
        borderColor = "#000000";
        borderSize = 3;
        shadowOffset = 1;
        marginVertical = 36;
        bold = true;
        italic = false;
        delaySec = 0.0;
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

    // Video
    std::string videoCodec;
    int videoWidth = 0;
    int videoHeight = 0;
    double videoFps = 0.0;
    std::string pixelFormat;
    int64_t videoBitrateBps = 0;
    std::string hwdecActive;
    std::string voDriver;

    // Audio
    std::string audioCodec;
    int audioSampleRate = 0;
    int audioChannels = 0;
    int64_t audioBitrateBps = 0;
    std::string aoDriver;

    // Performance & Stats
    int64_t droppedFrames = 0;
    int64_t voDroppedFrames = 0;
    int64_t mistimedFrames = 0;
    double avSyncDiffSec = 0.0;
    double estimatedVfFps = 0.0;

    std::string formatReport() const {
        std::ostringstream ss;
        ss << "==================================================\n";
        ss << "          RX MEDIA PLAYER DIAGNOSTIC REPORT       \n";
        ss << "==================================================\n\n";
        ss << "[File Information]\n";
        ss << "  File: " << filename << "\n";
        ss << "  Path: " << fullPath << "\n";
        ss << "  Container: " << container << "\n";
        ss << "  Size: " << (fileSize / (1024.0 * 1024.0)) << " MB\n";
        ss << "  Duration: " << durationSec << " s (Pos: " << currentPositionSec << " s)\n\n";

        ss << "[Video Stream]\n";
        ss << "  Codec: " << videoCodec << "\n";
        ss << "  Resolution: " << videoWidth << " x " << videoHeight << "\n";
        ss << "  Frame Rate: " << videoFps << " fps\n";
        ss << "  Pixel Format: " << pixelFormat << "\n";
        ss << "  Bitrate: " << (videoBitrateBps / 1000) << " kbps\n";
        ss << "  Active Decoder: " << hwdecActive << "\n";
        ss << "  Video Output: " << voDriver << "\n\n";

        ss << "[Audio Stream]\n";
        ss << "  Codec: " << audioCodec << "\n";
        ss << "  Channels: " << audioChannels << "\n";
        ss << "  Sample Rate: " << audioSampleRate << " Hz\n";
        ss << "  Bitrate: " << (audioBitrateBps / 1000) << " kbps\n";
        ss << "  Audio Output: " << aoDriver << "\n\n";

        ss << "[Performance & Sync]\n";
        ss << "  Decoder Dropped Frames: " << droppedFrames << "\n";
        ss << "  VO Dropped Frames: " << voDroppedFrames << "\n";
        ss << "  Mistimed Frames: " << mistimedFrames << "\n";
        ss << "  A/V Sync Offset: " << (avSyncDiffSec * 1000.0) << " ms\n";
        ss << "  Display Frame Rate: " << estimatedVfFps << " fps\n";
        ss << "==================================================\n";
        return ss.str();
    }
};

} // namespace rx
