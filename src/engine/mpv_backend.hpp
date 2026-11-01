#pragma once

#include "rx/types.hpp"
#include <windows.h>
#include <mpv/client.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

namespace rx {

class MpvBackend {
public:
    using StateCallback = std::function<void(PlaybackState)>;
    using ProgressCallback = std::function<void(double posSec, double durSec)>;
    using MediaLoadedCallback = std::function<void(const MediaInfo&)>;
    using TrackListCallback = std::function<void(const std::vector<MediaTrack>&)>;
    using EndFileCallback = std::function<void(int reason)>;
    using ErrorCallback = std::function<void(const std::string& errorMsg)>;

    MpvBackend();
    ~MpvBackend();

    // Initialization with host window handle for video rendering
    bool initialize(HWND videoHostHwnd);
    void shutdown();

    // Playback control
    bool loadFile(const std::string& path, bool autoPlay = true);
    void play();
    void pause();
    void togglePause();
    void stop();
    void stepForward();
    void stepBackward();
    void seek(double targetSec, bool exact = true);
    void seekRelative(double deltaSec);

    // Audio controls
    void setVolume(int volumePercent); // 0 - 100
    int getVolume() const { return m_volume; }
    void setMute(bool mute);
    bool isMuted() const { return m_muted; }
    void setAudioDelay(double delaySec);
    double getAudioDelay() const { return m_audioDelay; }
    void selectAudioTrack(int trackId);
    void setAudioDevice(const std::string& deviceId);
    void setAudioExclusive(bool exclusive);
    void setAudioFilter(const std::string& filterStr);
    std::string getAudioFilter() const { return m_audioFilter; }

    // Speed controls (0.25x - 4.0x) with pitch correction
    void setSpeed(double speed);
    double getSpeed() const { return m_speed; }

    // Subtitles
    bool loadSubtitle(const std::string& subPath);
    void selectSubtitleTrack(int trackId);
    void setSubtitleDelay(double delaySec);
    double getSubtitleDelay() const { return m_subDelay; }
    void setSubtitlesVisible(bool visible);
    void applySubtitleStyle(const SubtitleStyle& style);
    const SubtitleStyle& getSubtitleStyle() const { return m_subStyle; }
    std::vector<MediaTrack> getSubtitleTracks() const;
    int getSelectedSubtitleTrack() const { return m_selectedSubTrack; }

    // Streaming & URLs
    bool openUrl(const std::string& url, int cacheSec = 15, int maxBufferMb = 100);

    // Video adjustments
    void setVideoAdjustments(double brightness, double contrast, double saturation, double gamma, double hue);
    void applyVideoAdjustments(const VideoAdjustmentParams& params);
    const VideoAdjustmentParams& getVideoAdjustments() const { return m_adjParams; }
    void setAspectRatio(const std::string& ratio); // "auto", "16:9", "4:3", "21:9", "2.35:1", "1:1"
    void setVideoRotate(int degrees); // 0, 90, 180, 270
    void setDeinterlace(bool enable);
    void setFlip(bool hFlip, bool vFlip);
    void setZoom(double factor);
    void setPan(double x, double y);
    void setVideoDelay(double delaySec);
    double getVideoDelay() const { return m_videoDelay; }

    // Diagnostics & Telemetry
    DiagnosticsReport getDiagnostics() const;
    void toggleOsdStats();
    void showOsd(const std::string& text, int durationMs = 2000);
    std::string cycleAudioTrack();
    std::string cycleSubtitleTrack();
    bool toggleSubtitleVisibility();
    bool isSubtitlesVisible() const { return m_subtitlesVisible; }
    std::vector<MediaTrack> getAudioTracks() const;

    // Hardware acceleration
    void setHardwareAcceleration(bool enabled, const std::string& api = "auto-safe");
    std::string getActiveHwdec() const { return m_activeHwdec; }

    // Screenshot capture
    bool takeScreenshot(const std::string& destPath, bool includeSubtitles = true);

    // State queries
    PlaybackState getState() const { return m_state; }
    double getCurrentPosition() const { return m_position; }
    double getDuration() const { return m_duration; }
    const MediaInfo& getMediaInfo() const { return m_mediaInfo; }

    // Callbacks
    void setStateCallback(StateCallback cb) { m_stateCb = std::move(cb); }
    void setProgressCallback(ProgressCallback cb) { m_progressCb = std::move(cb); }
    void setMediaLoadedCallback(MediaLoadedCallback cb) { m_mediaLoadedCb = std::move(cb); }
    void setTrackListCallback(TrackListCallback cb) { m_trackListCb = std::move(cb); }
    void setEndFileCallback(EndFileCallback cb) { m_endFileCb = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { m_errorCb = std::move(cb); }

    // Direct mpv_handle accessor for advanced capabilities
    mpv_handle* getHandle() const { return m_mpv; }

private:
    void eventLoop();
    void handleMpvEvent(mpv_event* event);
    void handlePropertyChange(mpv_event_property* prop);
    void updateMediaInfo();
    void queryTrackList();

    mpv_handle* m_mpv = nullptr;
    HWND m_videoHwnd = nullptr;

    std::thread m_eventThread;
    std::atomic<bool> m_running{false};

    PlaybackState m_state = PlaybackState::Idle;
    double m_position = 0.0;
    double m_duration = 0.0;
    int m_volume = 80;
    bool m_muted = false;
    double m_speed = 1.0;
    double m_subDelay = 0.0;
    SubtitleStyle m_subStyle;
    int m_selectedSubTrack = 1;
    bool m_subtitlesVisible = true;
    double m_audioDelay = 0.0;
    double m_videoDelay = 0.0;
    std::string m_audioFilter;
    bool m_audioExclusive = false;
    std::string m_activeHwdec = "no";
    MediaInfo m_mediaInfo;
    VideoAdjustmentParams m_adjParams;

    mutable std::mutex m_mutex;

    StateCallback m_stateCb;
    ProgressCallback m_progressCb;
    MediaLoadedCallback m_mediaLoadedCb;
    TrackListCallback m_trackListCb;
    EndFileCallback m_endFileCb;
    ErrorCallback m_errorCb;
};

} // namespace rx
