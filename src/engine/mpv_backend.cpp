#include "mpv_backend.hpp"
#include "../core/logger.hpp"
#include <iostream>
#include <sstream>
#include <cmath>

namespace rx {

MpvBackend::MpvBackend() = default;

MpvBackend::~MpvBackend() {
    shutdown();
}

bool MpvBackend::initialize(HWND videoHostHwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_videoHwnd = videoHostHwnd;

    m_mpv = mpv_create();
    if (!m_mpv) {
        RX_LOG_ERROR("Failed to create libmpv context");
        return false;
    }

    // Configure native Windows Direct3D 11 video output attached to HWND
    int64_t wid = reinterpret_cast<intptr_t>(m_videoHwnd);
    mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid);
    mpv_set_option_string(m_mpv, "vo", "gpu");
    mpv_set_option_string(m_mpv, "gpu-context", "d3d11");
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "ao", "wasapi");
    mpv_set_option_string(m_mpv, "audio-pitch-correction", "yes");
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "idle", "yes");
    mpv_set_option_string(m_mpv, "subs-with-matching-audio", "yes");
    mpv_set_option_string(m_mpv, "sub-auto", "fuzzy");

    int status = mpv_initialize(m_mpv);
    if (status < 0) {
        RX_LOG_ERROR(std::string("mpv_initialize failed: ") + mpv_error_string(status));
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }

    // Observe properties
    mpv_observe_property(m_mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 4, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 5, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 6, "hwdec-current", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 7, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 8, "sub-delay", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 9, "audio-delay", MPV_FORMAT_DOUBLE);

    // Initial audio setup
    double vol = static_cast<double>(m_volume);
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &vol);

    m_running = true;
    m_eventThread = std::thread(&MpvBackend::eventLoop, this);

    RX_LOG_INFO("libmpv initialized successfully with D3D11 & WASAPI backend");
    return true;
}

void MpvBackend::shutdown() {
    if (m_running.exchange(false)) {
        if (m_mpv) {
            mpv_wakeup(m_mpv);
        }
        if (m_eventThread.joinable()) {
            m_eventThread.join();
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_mpv) {
        mpv_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

bool MpvBackend::loadFile(const std::string& path, bool autoPlay) {
    if (!m_mpv) return false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = PlaybackState::Opening;
        m_position = 0.0;
        m_duration = 0.0;
        m_mediaInfo = MediaInfo{};
        m_mediaInfo.fullPath = path;
    }

    if (m_stateCb) m_stateCb(PlaybackState::Opening);

    const char* cmd[] = { "loadfile", path.c_str(), "replace", nullptr };
    int err = mpv_command_async(m_mpv, 0, cmd);
    if (err < 0) {
        RX_LOG_ERROR(std::string("loadfile async error: ") + mpv_error_string(err));
        if (m_errorCb) m_errorCb(std::string("Failed to load: ") + path);
        return false;
    }

    int pauseVal = autoPlay ? 0 : 1;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseVal);
    return true;
}

void MpvBackend::play() {
    if (!m_mpv) return;
    int pauseVal = 0;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseVal);
}

void MpvBackend::pause() {
    if (!m_mpv) return;
    int pauseVal = 1;
    mpv_set_property_async(m_mpv, 0, "pause", MPV_FORMAT_FLAG, &pauseVal);
}

void MpvBackend::togglePause() {
    if (!m_mpv) return;
    const char* cmd[] = { "cycle", "pause", nullptr };
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvBackend::stop() {
    if (!m_mpv) return;
    const char* cmd[] = { "stop", nullptr };
    mpv_command_async(m_mpv, 0, cmd);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_state = PlaybackState::Stopped;
        m_position = 0.0;
    }
    if (m_stateCb) m_stateCb(PlaybackState::Stopped);
    if (m_progressCb) m_progressCb(0.0, m_duration);
}

void MpvBackend::stepForward() {
    if (!m_mpv) return;
    const char* cmd[] = { "frame-step", nullptr };
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvBackend::stepBackward() {
    if (!m_mpv) return;
    const char* cmd[] = { "frame-back-step", nullptr };
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvBackend::seek(double targetSec, bool exact) {
    if (!m_mpv) return;
    std::string targetStr = std::to_string(targetSec);
    const char* flag = exact ? "exact" : "keyframes";
    const char* cmd[] = { "seek", targetStr.c_str(), "absolute", flag, nullptr };
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvBackend::seekRelative(double deltaSec) {
    if (!m_mpv) return;
    std::string deltaStr = std::to_string(deltaSec);
    const char* cmd[] = { "seek", deltaStr.c_str(), "relative", "exact", nullptr };
    mpv_command_async(m_mpv, 0, cmd);
}

void MpvBackend::setVolume(int volumePercent) {
    if (!m_mpv) return;
    if (volumePercent < 0) volumePercent = 0;
    if (volumePercent > 100) volumePercent = 100;
    m_volume = volumePercent;
    double vol = static_cast<double>(m_volume);
    mpv_set_property_async(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE, &vol);
}

void MpvBackend::setMute(bool mute) {
    if (!m_mpv) return;
    m_muted = mute;
    int flag = mute ? 1 : 0;
    mpv_set_property_async(m_mpv, 0, "mute", MPV_FORMAT_FLAG, &flag);
}

void MpvBackend::setAudioDelay(double delaySec) {
    if (!m_mpv) return;
    m_audioDelay = delaySec;
    mpv_set_property_async(m_mpv, 0, "audio-delay", MPV_FORMAT_DOUBLE, &delaySec);
}

void MpvBackend::selectAudioTrack(int trackId) {
    if (!m_mpv) return;
    std::string idStr = (trackId == 0) ? "no" : std::to_string(trackId);
    mpv_set_property_string(m_mpv, "aid", idStr.c_str());
}

void MpvBackend::setAudioDevice(const std::string& deviceId) {
    if (!m_mpv) return;
    std::string dev = deviceId.empty() ? "auto" : deviceId;
    mpv_set_property_string(m_mpv, "audio-device", dev.c_str());
}

void MpvBackend::setAudioExclusive(bool exclusive) {
    if (!m_mpv) return;
    m_audioExclusive = exclusive;
    const char* flag = exclusive ? "yes" : "no";
    mpv_set_property_string(m_mpv, "audio-exclusive", flag);
}

void MpvBackend::setAudioFilter(const std::string& filterStr) {
    if (!m_mpv) return;
    m_audioFilter = filterStr;
    mpv_set_property_string(m_mpv, "af", filterStr.c_str());
}

void MpvBackend::setSpeed(double speed) {
    if (!m_mpv) return;
    if (speed < 0.25) speed = 0.25;
    if (speed > 4.0) speed = 4.0;
    m_speed = speed;
    mpv_set_property_async(m_mpv, 0, "speed", MPV_FORMAT_DOUBLE, &speed);
}

bool MpvBackend::loadSubtitle(const std::string& subPath) {
    if (!m_mpv) return false;
    const char* cmd[] = { "sub-add", subPath.c_str(), "select", nullptr };
    int err = mpv_command_async(m_mpv, 0, cmd);
    return (err >= 0);
}

void MpvBackend::selectSubtitleTrack(int trackId) {
    if (!m_mpv) return;
    m_selectedSubTrack = trackId;
    std::string idStr = (trackId == 0) ? "no" : std::to_string(trackId);
    mpv_set_property_string(m_mpv, "sid", idStr.c_str());
}

void MpvBackend::setSubtitleDelay(double delaySec) {
    if (!m_mpv) return;
    m_subDelay = delaySec;
    mpv_set_property_async(m_mpv, 0, "sub-delay", MPV_FORMAT_DOUBLE, &delaySec);
}

void MpvBackend::setSubtitlesVisible(bool visible) {
    if (!m_mpv) return;
    int flag = visible ? 1 : 0;
    mpv_set_property_async(m_mpv, 0, "sub-visibility", MPV_FORMAT_FLAG, &flag);
}

void MpvBackend::applySubtitleStyle(const SubtitleStyle& style) {
    if (!m_mpv) return;
    m_subStyle = style;

    mpv_set_property_string(m_mpv, "sub-font", style.font.c_str());
    mpv_set_property_string(m_mpv, "sub-font-size", std::to_string(style.fontSize).c_str());
    mpv_set_property_string(m_mpv, "sub-color", style.textColor.c_str());
    mpv_set_property_string(m_mpv, "sub-border-color", style.borderColor.c_str());
    mpv_set_property_string(m_mpv, "sub-border-size", std::to_string(style.borderSize).c_str());
    mpv_set_property_string(m_mpv, "sub-shadow-offset", std::to_string(style.shadowOffset).c_str());
    mpv_set_property_string(m_mpv, "sub-margin-y", std::to_string(style.marginVertical).c_str());
    mpv_set_property_string(m_mpv, "sub-bold", style.bold ? "yes" : "no");
    mpv_set_property_string(m_mpv, "sub-italic", style.italic ? "yes" : "no");
    setSubtitleDelay(style.delaySec);
}

std::vector<MediaTrack> MpvBackend::getSubtitleTracks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<MediaTrack> res;
    for (const auto& t : m_mediaInfo.tracks) {
        if (t.type == "sub") {
            res.push_back(t);
        }
    }
    return res;
}

bool MpvBackend::openUrl(const std::string& url, int cacheSec, int maxBufferMb) {
    if (!m_mpv || url.empty()) return false;

    int64_t maxBytes = static_cast<int64_t>(maxBufferMb) * 1024 * 1024;
    mpv_set_property_string(m_mpv, "demuxer-max-bytes", std::to_string(maxBytes).c_str());
    mpv_set_property_string(m_mpv, "demuxer-readahead-secs", std::to_string(cacheSec).c_str());
    mpv_set_property_string(m_mpv, "network-timeout", "30");
    mpv_set_property_string(m_mpv, "user-agent", "RXMediaPlayer/1.0 (Windows 64-bit; D3D11/WASAPI)");

    return loadFile(url, true);
}

void MpvBackend::setVideoAdjustments(double brightness, double contrast, double saturation, double gamma, double hue) {
    VideoAdjustmentParams params;
    params.brightness = static_cast<int>(brightness * 100.0);
    params.contrast = static_cast<int>(contrast * 100.0);
    params.saturation = static_cast<int>(saturation * 100.0);
    params.gamma = static_cast<int>(gamma * 100.0);
    params.hue = static_cast<int>(hue * 100.0);
    applyVideoAdjustments(params);
}

void MpvBackend::applyVideoAdjustments(const VideoAdjustmentParams& params) {
    if (!m_mpv) return;
    m_adjParams = params;

    mpv_set_property_string(m_mpv, "brightness", std::to_string(params.brightness).c_str());
    mpv_set_property_string(m_mpv, "contrast", std::to_string(params.contrast).c_str());
    mpv_set_property_string(m_mpv, "saturation", std::to_string(params.saturation).c_str());
    mpv_set_property_string(m_mpv, "gamma", std::to_string(params.gamma).c_str());
    mpv_set_property_string(m_mpv, "hue", std::to_string(params.hue).c_str());

    setAspectRatio(params.aspectRatio);
    setVideoRotate(params.rotate);
    setDeinterlace(params.deinterlace);
    setZoom(params.zoom);
    setPan(params.panX, params.panY);
    setFlip(params.flipHorizontal, params.flipVertical);
}

void MpvBackend::setDeinterlace(bool enable) {
    if (!m_mpv) return;
    m_adjParams.deinterlace = enable;
    const char* flag = enable ? "yes" : "no";
    mpv_set_property_string(m_mpv, "deinterlace", flag);
}

void MpvBackend::setFlip(bool hFlip, bool vFlip) {
    if (!m_mpv) return;
    m_adjParams.flipHorizontal = hFlip;
    m_adjParams.flipVertical = vFlip;
    std::string vfStr;
    if (hFlip && vFlip) {
        vfStr = "hflip,vflip";
    } else if (hFlip) {
        vfStr = "hflip";
    } else if (vFlip) {
        vfStr = "vflip";
    }
    mpv_set_property_string(m_mpv, "vf", vfStr.c_str());
}

void MpvBackend::setZoom(double factor) {
    if (!m_mpv) return;
    if (factor < 0.2) factor = 0.2;
    if (factor > 4.0) factor = 4.0;
    m_adjParams.zoom = factor;
    // mpv video-zoom is logarithmic (0 = 1x, 1 = 2x, -1 = 0.5x)
    double logZoom = std::log2(factor);
    mpv_set_property(m_mpv, "video-zoom", MPV_FORMAT_DOUBLE, &logZoom);
}

void MpvBackend::setPan(double x, double y) {
    if (!m_mpv) return;
    m_adjParams.panX = x;
    m_adjParams.panY = y;
    mpv_set_property(m_mpv, "video-pan-x", MPV_FORMAT_DOUBLE, &x);
    mpv_set_property(m_mpv, "video-pan-y", MPV_FORMAT_DOUBLE, &y);
}

void MpvBackend::setVideoDelay(double delaySec) {
    if (!m_mpv) return;
    m_videoDelay = delaySec;
    mpv_set_property_async(m_mpv, 0, "video-delay", MPV_FORMAT_DOUBLE, &delaySec);
}

void MpvBackend::toggleOsdStats() {
    if (!m_mpv) return;
    const char* cmd[] = { "script-binding", "stats/display-stats-toggle", nullptr };
    mpv_command_async(m_mpv, 0, cmd);
}

DiagnosticsReport MpvBackend::getDiagnostics() const {
    DiagnosticsReport r;
    if (!m_mpv) return r;

    r.filename = m_mediaInfo.filename;
    r.fullPath = m_mediaInfo.fullPath;
    r.durationSec = m_duration;
    r.currentPositionSec = m_position;

    char* fmt = mpv_get_property_string(m_mpv, "file-format");
    if (fmt) { r.container = fmt; mpv_free(fmt); }

    int64_t size = 0;
    if (mpv_get_property(m_mpv, "file-size", MPV_FORMAT_INT64, &size) >= 0) r.fileSize = size;

    char* vc = mpv_get_property_string(m_mpv, "video-codec");
    if (vc) { r.videoCodec = vc; mpv_free(vc); }

    int64_t w = 0, h = 0;
    if (mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w) >= 0) r.videoWidth = static_cast<int>(w);
    if (mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h) >= 0) r.videoHeight = static_cast<int>(h);

    double fps = 0.0;
    if (mpv_get_property(m_mpv, "container-fps", MPV_FORMAT_DOUBLE, &fps) >= 0) r.videoFps = fps;

    char* pix = mpv_get_property_string(m_mpv, "video-params/pixelformat");
    if (pix) { r.pixelFormat = pix; mpv_free(pix); }

    int64_t vBitrate = 0;
    if (mpv_get_property(m_mpv, "video-bitrate", MPV_FORMAT_INT64, &vBitrate) >= 0) r.videoBitrateBps = vBitrate;

    char* hw = mpv_get_property_string(m_mpv, "hwdec-current");
    if (hw) { r.hwdecActive = hw; mpv_free(hw); }

    char* vo = mpv_get_property_string(m_mpv, "current-vo");
    if (vo) { r.voDriver = vo; mpv_free(vo); }

    char* ac = mpv_get_property_string(m_mpv, "audio-codec-name");
    if (ac) { r.audioCodec = ac; mpv_free(ac); }

    int64_t sr = 0, ch = 0;
    if (mpv_get_property(m_mpv, "audio-params/samplerate", MPV_FORMAT_INT64, &sr) >= 0) r.audioSampleRate = static_cast<int>(sr);
    if (mpv_get_property(m_mpv, "audio-params/channel-count", MPV_FORMAT_INT64, &ch) >= 0) r.audioChannels = static_cast<int>(ch);

    int64_t aBitrate = 0;
    if (mpv_get_property(m_mpv, "audio-bitrate", MPV_FORMAT_INT64, &aBitrate) >= 0) r.audioBitrateBps = aBitrate;

    char* ao = mpv_get_property_string(m_mpv, "current-ao");
    if (ao) { r.aoDriver = ao; mpv_free(ao); }

    int64_t drops = 0, voDrops = 0, mistimed = 0;
    if (mpv_get_property(m_mpv, "drop-frame-count", MPV_FORMAT_INT64, &drops) >= 0) r.droppedFrames = drops;
    if (mpv_get_property(m_mpv, "vo-drop-frame-count", MPV_FORMAT_INT64, &voDrops) >= 0) r.voDroppedFrames = voDrops;
    if (mpv_get_property(m_mpv, "mistimed-frame-count", MPV_FORMAT_INT64, &mistimed) >= 0) r.mistimedFrames = mistimed;

    double avSync = 0.0, dispFps = 0.0;
    if (mpv_get_property(m_mpv, "avsync", MPV_FORMAT_DOUBLE, &avSync) >= 0) r.avSyncDiffSec = avSync;
    if (mpv_get_property(m_mpv, "estimated-vf-fps", MPV_FORMAT_DOUBLE, &dispFps) >= 0) r.estimatedVfFps = dispFps;

    return r;
}

void MpvBackend::setAspectRatio(const std::string& ratio) {
    if (!m_mpv) return;
    std::string val = (ratio == "auto" || ratio == "-1") ? "-1" : ratio;
    mpv_set_property_string(m_mpv, "video-aspect-override", val.c_str());
}

void MpvBackend::setVideoRotate(int degrees) {
    if (!m_mpv) return;
    mpv_set_property_string(m_mpv, "video-rotate", std::to_string(degrees).c_str());
}

void MpvBackend::setHardwareAcceleration(bool enabled, const std::string& api) {
    if (!m_mpv) return;
    std::string hw = enabled ? api : "no";
    mpv_set_property_string(m_mpv, "hwdec", hw.c_str());
}

bool MpvBackend::takeScreenshot(const std::string& destPath, bool includeSubtitles) {
    if (!m_mpv) return false;
    const char* subArg = includeSubtitles ? "subtitles" : "video";
    const char* cmd[] = { "screenshot-to-file", destPath.c_str(), subArg, nullptr };
    int err = mpv_command(m_mpv, cmd);
    return (err >= 0);
}

void MpvBackend::eventLoop() {
    while (m_running) {
        mpv_event* event = mpv_wait_event(m_mpv, 0.05);
        if (!event || event->event_id == MPV_EVENT_NONE) continue;
        if (event->event_id == MPV_EVENT_SHUTDOWN) break;
        handleMpvEvent(event);
    }
}

void MpvBackend::handleMpvEvent(mpv_event* event) {
    switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED: {
            updateMediaInfo();
            queryTrackList();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state = PlaybackState::Playing;
            }
            if (m_stateCb) m_stateCb(PlaybackState::Playing);
            if (m_mediaLoadedCb) m_mediaLoadedCb(m_mediaInfo);
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto* endFile = static_cast<mpv_event_end_file*>(event->data);
            int reason = endFile ? endFile->reason : 0;
            if (endFile && endFile->error < 0) {
                RX_LOG_ERROR(std::string("Playback failed: ") + mpv_error_string(endFile->error));
                if (m_errorCb) m_errorCb(std::string("Playback error: ") + mpv_error_string(endFile->error));
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_state = PlaybackState::Stopped;
            }
            if (m_stateCb) m_stateCb(PlaybackState::Stopped);
            if (m_endFileCb) m_endFileCb(reason);
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto* prop = static_cast<mpv_event_property*>(event->data);
            if (prop) handlePropertyChange(prop);
            break;
        }
        default:
            break;
    }
}

void MpvBackend::handlePropertyChange(mpv_event_property* prop) {
    if (!prop || !prop->name) return;
    std::string name = prop->name;

    if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
        double pos = *static_cast<double*>(prop->data);
        m_position = pos;
        if (m_progressCb) m_progressCb(m_position, m_duration);
    } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
        double dur = *static_cast<double*>(prop->data);
        m_duration = dur;
        if (m_progressCb) m_progressCb(m_position, m_duration);
    } else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
        int paused = *static_cast<int*>(prop->data);
        PlaybackState newState = (paused ? PlaybackState::Paused : PlaybackState::Playing);
        if (m_state != PlaybackState::Idle && m_state != PlaybackState::Stopped) {
            m_state = newState;
            if (m_stateCb) m_stateCb(m_state);
        }
    } else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
        m_volume = static_cast<int>(*static_cast<double*>(prop->data));
    } else if (name == "mute" && prop->format == MPV_FORMAT_FLAG) {
        m_muted = (*static_cast<int*>(prop->data) != 0);
    } else if (name == "hwdec-current" && prop->format == MPV_FORMAT_STRING) {
        char* str = *static_cast<char**>(prop->data);
        m_activeHwdec = str ? str : "no";
        m_mediaInfo.hwdecActive = m_activeHwdec;
    } else if (name == "sub-delay" && prop->format == MPV_FORMAT_DOUBLE) {
        m_subDelay = *static_cast<double*>(prop->data);
    } else if (name == "audio-delay" && prop->format == MPV_FORMAT_DOUBLE) {
        m_audioDelay = *static_cast<double*>(prop->data);
    }
}

void MpvBackend::updateMediaInfo() {
    if (!m_mpv) return;

    char* title = mpv_get_property_string(m_mpv, "media-title");
    if (title) {
        m_mediaInfo.filename = title;
        mpv_free(title);
    }

    char* path = mpv_get_property_string(m_mpv, "path");
    if (path) {
        m_mediaInfo.fullPath = path;
        mpv_free(path);
    }

    char* fmt = mpv_get_property_string(m_mpv, "file-format");
    if (fmt) {
        m_mediaInfo.container = fmt;
        mpv_free(fmt);
    }

    int64_t w = 0, h = 0;
    if (mpv_get_property(m_mpv, "video-params/w", MPV_FORMAT_INT64, &w) >= 0) m_mediaInfo.videoWidth = static_cast<int>(w);
    if (mpv_get_property(m_mpv, "video-params/h", MPV_FORMAT_INT64, &h) >= 0) m_mediaInfo.videoHeight = static_cast<int>(h);

    double fps = 0.0;
    if (mpv_get_property(m_mpv, "container-fps", MPV_FORMAT_DOUBLE, &fps) >= 0) m_mediaInfo.videoFps = fps;

    char* vCodec = mpv_get_property_string(m_mpv, "video-codec");
    if (vCodec) {
        m_mediaInfo.videoCodec = vCodec;
        mpv_free(vCodec);
    }

    char* aCodec = mpv_get_property_string(m_mpv, "audio-codec-name");
    if (aCodec) {
        m_mediaInfo.audioCodec = aCodec;
        mpv_free(aCodec);
    }

    int64_t ch = 0;
    if (mpv_get_property(m_mpv, "audio-params/channel-count", MPV_FORMAT_INT64, &ch) >= 0) {
        m_mediaInfo.audioChannels = static_cast<int>(ch);
    }

    char* hwdec = mpv_get_property_string(m_mpv, "hwdec-current");
    if (hwdec) {
        m_mediaInfo.hwdecActive = hwdec;
        m_activeHwdec = hwdec;
        mpv_free(hwdec);
    }
}

void MpvBackend::queryTrackList() {
    if (!m_mpv) return;

    int64_t count = 0;
    if (mpv_get_property(m_mpv, "track-list/count", MPV_FORMAT_INT64, &count) < 0) return;

    std::vector<MediaTrack> tracks;
    for (int64_t i = 0; i < count; ++i) {
        std::string prefix = "track-list/" + std::to_string(i) + "/";

        MediaTrack track;
        int64_t id = 0;
        if (mpv_get_property(m_mpv, (prefix + "id").c_str(), MPV_FORMAT_INT64, &id) >= 0) track.id = static_cast<int>(id);

        char* type = mpv_get_property_string(m_mpv, (prefix + "type").c_str());
        if (type) { track.type = type; mpv_free(type); }

        char* title = mpv_get_property_string(m_mpv, (prefix + "title").c_str());
        if (title) { track.title = title; mpv_free(title); }

        char* lang = mpv_get_property_string(m_mpv, (prefix + "lang").c_str());
        if (lang) { track.lang = lang; mpv_free(lang); }

        char* codec = mpv_get_property_string(m_mpv, (prefix + "codec").c_str());
        if (codec) { track.codec = codec; mpv_free(codec); }

        int selected = 0;
        if (mpv_get_property(m_mpv, (prefix + "selected").c_str(), MPV_FORMAT_FLAG, &selected) >= 0) track.selected = (selected != 0);

        int external = 0;
        if (mpv_get_property(m_mpv, (prefix + "external").c_str(), MPV_FORMAT_FLAG, &external) >= 0) track.external = (external != 0);

        tracks.push_back(track);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_mediaInfo.tracks = tracks;
    }

    if (m_trackListCb) m_trackListCb(tracks);
}

} // namespace rx
