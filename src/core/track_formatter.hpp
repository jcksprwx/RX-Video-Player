#pragma once

#include "rx/types.hpp"
#include <string>

namespace rx::track_formatter {

// Converts language code (ISO 639-1 or 639-2) to full display name (e.g. "hin" -> "Hindi")
std::wstring getLanguageDisplayName(const std::string& langCode);

// Converts codec string to friendly name (e.g. "eac3" -> "Dolby Digital Plus")
std::wstring getCodecDisplayName(const std::string& codec);

// Short badge (e.g. "DDP 5.1", "AAC", "FLAC")
std::wstring getCodecShortBadge(const std::string& codec, int channels);

// Format channel count (e.g. 6 -> "5.1 Ch", 2 -> "2.0 Ch")
std::wstring getChannelsDisplayName(int channels);

// Format sample rate (e.g. 48000 -> "48.0 kHz")
std::wstring getSampleRateDisplayName(int sampleRate);

// Format bitrate (e.g. 192000 -> "192 kbit/s")
std::wstring getBitrateDisplayName(int bitrate);

// Format audio track label for popup menu:
// Language, Track Title [Codec] (Audio N) — Codec, Sample Rate, Channels, Bitrate
std::wstring formatAudioTrackMenuLabel(const MediaTrack& track, int index1Based);

// Format audio track label for OSD:
// Audio Track: Hindi — Dolby Digital Plus 5.1
std::wstring formatAudioTrackOsd(const MediaTrack& track);

// Format subtitle track label for popup menu:
// Language, Track Title — Format (Subtitle N) [Default] [Forced]
std::wstring formatSubtitleTrackMenuLabel(const MediaTrack& track, int index1Based);

// Format subtitle track label for OSD:
// Subtitle Track: English — SRT
std::wstring formatSubtitleTrackOsd(const MediaTrack& track);

} // namespace rx::track_formatter
