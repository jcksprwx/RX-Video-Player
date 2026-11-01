#include "track_formatter.hpp"
#include "utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <unordered_map>

namespace rx::track_formatter {

std::wstring getLanguageDisplayName(const std::string& langCode) {
    if (langCode.empty()) return L"Unknown";

    std::string lower = langCode;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    static const std::unordered_map<std::string, std::wstring> s_langs = {
        { "hin", L"Hindi" }, { "hi", L"Hindi" },
        { "tel", L"Telugu" }, { "te", L"Telugu" },
        { "tam", L"Tamil" }, { "ta", L"Tamil" },
        { "mal", L"Malayalam" }, { "ml", L"Malayalam" },
        { "ben", L"Bengali" }, { "bn", L"Bengali" },
        { "kan", L"Kannada" }, { "kn", L"Kannada" },
        { "mar", L"Marathi" }, { "mr", L"Marathi" },
        { "pan", L"Punjabi" }, { "pa", L"Punjabi" },
        { "guj", L"Gujarati" }, { "gu", L"Gujarati" },
        { "urd", L"Urdu" }, { "ur", L"Urdu" },
        { "eng", L"English" }, { "en", L"English" },
        { "spa", L"Spanish" }, { "es", L"Spanish" },
        { "fra", L"French" }, { "fre", L"French" }, { "fr", L"French" },
        { "deu", L"German" }, { "ger", L"German" }, { "de", L"German" },
        { "ita", L"Italian" }, { "it", L"Italian" },
        { "jpn", L"Japanese" }, { "ja", L"Japanese" },
        { "kor", L"Korean" }, { "ko", L"Korean" },
        { "zho", L"Chinese" }, { "chi", L"Chinese" }, { "zh", L"Chinese" },
        { "rus", L"Russian" }, { "ru", L"Russian" },
        { "por", L"Portuguese" }, { "pt", L"Portuguese" },
        { "ara", L"Arabic" }, { "ar", L"Arabic" },
        { "tur", L"Turkish" }, { "tr", L"Turkish" },
        { "nld", L"Dutch" }, { "dut", L"Dutch" }, { "nl", L"Dutch" },
        { "pol", L"Polish" }, { "pl", L"Polish" },
        { "swe", L"Swedish" }, { "sv", L"Swedish" },
        { "nor", L"Norwegian" }, { "no", L"Norwegian" },
        { "dan", L"Danish" }, { "da", L"Danish" },
        { "fin", L"Finnish" }, { "fi", L"Finnish" },
        { "heb", L"Hebrew" }, { "he", L"Hebrew" },
        { "und", L"Undetermined" }
    };

    auto it = s_langs.find(lower);
    if (it != s_langs.end()) return it->second;

    // Fallback: capitalize first letter
    std::wstring w = utils::utf8ToWide(langCode);
    if (!w.empty()) w[0] = towupper(w[0]);
    return w;
}

std::wstring getCodecDisplayName(const std::string& codec) {
    if (codec.empty()) return L"";

    std::string lower = codec;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "eac3" || lower == "ec-3") return L"Dolby Digital Plus";
    if (lower == "ac3") return L"Dolby Digital";
    if (lower == "truehd") return L"Dolby TrueHD";
    if (lower == "dts" || lower == "dca") return L"DTS";
    if (lower == "dts-hd ma" || lower == "dtshd") return L"DTS-HD Master Audio";
    if (lower == "aac") return L"AAC";
    if (lower == "flac") return L"FLAC";
    if (lower == "opus") return L"Opus";
    if (lower == "vorbis") return L"Vorbis";
    if (lower == "mp3") return L"MP3";
    if (lower == "pcm_s16le" || lower == "pcm_s24le") return L"PCM";
    if (lower == "subrip" || lower == "srt") return L"SRT";
    if (lower == "ass" || lower == "ssa") return L"ASS";
    if (lower == "hdmv_pgs_subtitle" || lower == "pgs") return L"PGS";
    if (lower == "webvtt" || lower == "vtt") return L"WebVTT";
    if (lower == "mov_text") return L"TX3G";
    if (lower == "dvd_subtitle") return L"VobSub";

    std::wstring w = utils::utf8ToWide(codec);
    std::transform(w.begin(), w.end(), w.begin(), ::towupper);
    return w;
}

std::wstring getCodecShortBadge(const std::string& codec, int channels) {
    std::string lower = codec;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "eac3" || lower == "ec-3") {
        return (channels >= 6) ? L"DDP 5.1" : L"DDP";
    }
    if (lower == "ac3") {
        return (channels >= 6) ? L"DD 5.1" : L"DD";
    }
    if (lower == "truehd") return L"TrueHD";
    if (lower == "dts" || lower == "dca") return (channels >= 6) ? L"DTS 5.1" : L"DTS";
    if (lower == "dtshd") return L"DTS-HD";
    if (lower == "aac") return L"AAC";
    if (lower == "flac") return L"FLAC";
    if (lower == "opus") return L"Opus";
    if (lower == "mp3") return L"MP3";

    std::wstring w = utils::utf8ToWide(codec);
    std::transform(w.begin(), w.end(), w.begin(), ::towupper);
    return w;
}

std::wstring getChannelsDisplayName(int channels) {
    if (channels <= 0) return L"";
    if (channels == 1) return L"1.0 Ch (Mono)";
    if (channels == 2) return L"2.0 Ch (Stereo)";
    if (channels == 6) return L"5.1 Ch";
    if (channels == 8) return L"7.1 Ch";
    return std::to_wstring(channels) + L" Ch";
}

std::wstring getSampleRateDisplayName(int sampleRate) {
    if (sampleRate <= 0) return L"";
    double khz = static_cast<double>(sampleRate) / 1000.0;
    std::wostringstream wss;
    wss << std::fixed << std::setprecision(1) << khz << L" kHz";
    return wss.str();
}

std::wstring getBitrateDisplayName(int bitrate) {
    if (bitrate <= 0) return L"";
    if (bitrate >= 1000000) {
        double mbps = static_cast<double>(bitrate) / 1000000.0;
        std::wostringstream wss;
        wss << std::fixed << std::setprecision(1) << mbps << L" Mbit/s";
        return wss.str();
    }
    int kbps = bitrate / 1000;
    return std::to_wstring(kbps) + L" kbit/s";
}

std::wstring formatAudioTrackMenuLabel(const MediaTrack& track, int index1Based) {
    // Expected format:
    // Language, Track Title [Codec] (Audio N) — Codec, Sample Rate, Channels, Bitrate
    // e.g.: Hindi, Main Audio [DDP 5.1] (Audio 3) — Dolby Digital Plus, 48.0 kHz, 5.1 Ch, 192 kbit/s
    std::wstring lang = getLanguageDisplayName(track.lang);
    std::wstring title = utils::utf8ToWide(track.title);
    std::wstring codecBadge = getCodecShortBadge(track.codec, track.channels);
    std::wstring codecName = !track.codecDesc.empty() ? utils::utf8ToWide(track.codecDesc) : getCodecDisplayName(track.codec);
    std::wstring channels = getChannelsDisplayName(track.channels);
    std::wstring srate = getSampleRateDisplayName(track.sampleRate);
    std::wstring brate = getBitrateDisplayName(track.bitrate);

    std::wstring leftSide;
    leftSide += lang;
    if (!title.empty() && title != lang) {
        leftSide += L", " + title;
    }
    if (!codecBadge.empty()) {
        leftSide += L" [" + codecBadge + L"]";
    }
    leftSide += L" (Audio " + std::to_wstring(index1Based) + L")";

    std::wstring rightSide;
    if (!codecName.empty()) {
        if (!rightSide.empty()) rightSide += L", ";
        rightSide += codecName;
    }
    if (!srate.empty()) {
        if (!rightSide.empty()) rightSide += L", ";
        rightSide += srate;
    }
    if (!channels.empty()) {
        if (!rightSide.empty()) rightSide += L", ";
        rightSide += channels;
    }
    if (!brate.empty()) {
        if (!rightSide.empty()) rightSide += L", ";
        rightSide += brate;
    }

    if (!rightSide.empty()) {
        return leftSide + L" \x2014 " + rightSide; // em dash
    }
    return leftSide;
}

std::wstring formatAudioTrackOsd(const MediaTrack& track) {
    // Example: Hindi — Dolby Digital Plus 5.1
    std::wstring lang = getLanguageDisplayName(track.lang);
    std::wstring codecName = getCodecDisplayName(track.codec);
    std::wstring ch = (track.channels == 6) ? L"5.1" : ((track.channels == 2) ? L"Stereo" : ((track.channels == 8) ? L"7.1" : L""));

    std::wstring result = lang;
    if (!codecName.empty()) {
        result += L" \x2014 " + codecName;
        if (!ch.empty()) result += L" " + ch;
    }
    return result;
}

std::wstring formatSubtitleTrackMenuLabel(const MediaTrack& track, int index1Based) {
    // Expected format:
    // Language, Track Title — Format (Subtitle N) [Default] [Forced]
    // e.g.: English — SRT (Subtitle 1)
    std::wstring lang = getLanguageDisplayName(track.lang);
    std::wstring title = utils::utf8ToWide(track.title);
    std::wstring format = getCodecDisplayName(track.codec);
    if (format.empty()) format = L"Text";

    std::wstring result = lang;
    if (!title.empty() && title != lang) {
        result += L", " + title;
    }
    result += L" \x2014 " + format + L" (Subtitle " + std::to_wstring(index1Based) + L")";
    if (track.isForced) result += L" [Forced]";
    if (track.isDefault) result += L" [Default]";
    if (track.external) result += L" [External]";

    return result;
}

std::wstring formatSubtitleTrackOsd(const MediaTrack& track) {
    std::wstring lang = getLanguageDisplayName(track.lang);
    std::wstring format = getCodecDisplayName(track.codec);
    if (!format.empty()) {
        return lang + L" \x2014 " + format;
    }
    return lang;
}

} // namespace rx::track_formatter
