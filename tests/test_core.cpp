#include "../src/core/utils.hpp"
#include "../src/core/settings.hpp"
#include "../src/playlist/playlist_manager.hpp"
#include "../src/audio/equalizer.hpp"
#include "../src/library/media_database.hpp"
#include "../src/core/shortcut_manager.hpp"
#include "../src/core/track_formatter.hpp"
#include "../src/subtitles/opensubtitles_client.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>

void testUtils() {
    std::cout << "[TEST] Running testUtils..." << std::endl;

    // Time formatting
    assert(rx::utils::formatTime(0.0) == "00:00");
    assert(rx::utils::formatTime(65.0) == "01:05");
    assert(rx::utils::formatTime(3665.0) == "01:01:05");

    // Byte formatting
    assert(rx::utils::formatBytes(500).find("500") != std::string::npos);
    assert(rx::utils::formatBytes(1048576).find("1.00 MB") != std::string::npos);

    // UTF conversions
    std::string testUtf8 = "Hello RX Media Player 🎵";
    std::wstring wide = rx::utils::utf8ToWide(testUtf8);
    std::string back = rx::utils::wideToUtf8(wide);
    assert(back == testUtf8);

    // Extensions
    assert(rx::utils::isSupportedMediaFile("video.mp4"));
    assert(rx::utils::isSupportedMediaFile("music.flac"));
    assert(!rx::utils::isSupportedMediaFile("doc.pdf"));
    assert(rx::utils::isSubtitleFile("sub.srt"));
    assert(rx::utils::isPlaylistFile("list.m3u"));

    std::cout << "  -> testUtils PASSED!" << std::endl;
}

void testPlaylist() {
    std::cout << "[TEST] Running testPlaylist..." << std::endl;

    rx::PlaylistManager pm;
    assert(pm.empty());
    assert(pm.size() == 0);

    pm.addItem("C:/Videos/Movie1.mp4", "Movie 1");
    pm.addItem("C:/Videos/Movie2.mkv", "Movie 2");
    pm.addItem("C:/Videos/Song.mp3", "Song");

    assert(pm.size() == 3);
    assert(pm.getCurrentIndex() == 0);
    assert(pm.getCurrentItem()->title == "Movie 1");

    // Next navigation
    auto next = pm.next(true);
    assert(next.has_value());
    assert(next->title == "Movie 2");
    assert(pm.getCurrentIndex() == 1);

    // Previous navigation
    auto prev = pm.previous();
    assert(prev.has_value());
    assert(prev->title == "Movie 1");
    assert(pm.getCurrentIndex() == 0);

    // Move item
    pm.moveItem(0, 2);
    assert(pm.getItem(2)->title == "Movie 1");

    // M3U export & import
    std::string testM3u = "test_playlist.m3u";
    bool saved = pm.saveM3U(testM3u);
    assert(saved);

    rx::PlaylistManager pm2;
    bool loaded = pm2.loadM3U(testM3u);
    assert(loaded);
    assert(pm2.size() == pm.size());

    std::filesystem::remove(testM3u);
    std::cout << "  -> testPlaylist PASSED!" << std::endl;
}

void testSettings() {
    std::cout << "[TEST] Running testSettings..." << std::endl;

    std::string testIni = "test_settings.ini";
    auto& s = rx::Settings::instance();
    s.audio.volume = 92;
    s.video.hwdecEnabled = true;
    s.video.adjustments.aspectRatio = "16:9";
    s.video.adjustments.brightness = 15;
    s.video.adjustments.contrast = -10;
    s.video.adjustments.rotate = 90;
    s.save(testIni);

    assert(std::filesystem::exists(testIni));

    // Reset and reload
    s.audio.volume = 50;
    s.video.adjustments.brightness = 0;
    s.load(testIni);
    assert(s.audio.volume == 92);
    assert(s.video.hwdecEnabled == true);
    assert(s.video.adjustments.aspectRatio == "16:9");
    assert(s.video.adjustments.brightness == 15);
    assert(s.video.adjustments.contrast == -10);
    assert(s.video.adjustments.rotate == 90);

    std::filesystem::remove(testIni);
    std::cout << "  -> testSettings PASSED!" << std::endl;
}

void testVideoAdjustments() {
    std::cout << "[TEST] Running testVideoAdjustments..." << std::endl;

    rx::VideoAdjustmentParams p;
    assert(p.brightness == 0);
    assert(p.contrast == 0);
    assert(p.zoom == 1.0);
    assert(p.rotate == 0);

    p.brightness = 25;
    p.contrast = -15;
    p.rotate = 180;
    p.flipHorizontal = true;
    p.zoom = 1.5;

    assert(p.brightness == 25);
    assert(p.rotate == 180);
    assert(p.flipHorizontal);

    p.reset();
    assert(p.brightness == 0);
    assert(p.contrast == 0);
    assert(p.zoom == 1.0);
    assert(p.rotate == 0);
    assert(!p.flipHorizontal);

    std::cout << "  -> testVideoAdjustments PASSED!" << std::endl;
}

void testDiagnostics() {
    std::cout << "[TEST] Running testDiagnostics..." << std::endl;

    rx::DiagnosticsReport r;
    r.filename = "sample_4k.mkv";
    r.fullPath = "D:/Videos/sample_4k.mkv";
    r.container = "matroska";
    r.fileSize = 104857600;
    r.durationSec = 120.0;
    r.currentPositionSec = 45.5;
    r.videoCodec = "hevc";
    r.videoWidth = 3840;
    r.videoHeight = 2160;
    r.videoFps = 60.0;
    r.pixelFormat = "yuv420p10le";
    r.hwdecActive = "d3d11va";
    r.audioCodec = "flac";
    r.audioChannels = 6;
    r.audioSampleRate = 48000;
    r.droppedFrames = 0;
    r.outVideoWidth = 3840;
    r.outVideoHeight = 2160;
    r.outPixelFormat = "Direct3D 11 NV12";
    r.audioChannelLayout = "5.1(side)";
    r.tracks.push_back({ 1, "video", "4K HDR", "eng", "hevc", true, false });
    r.tracks.push_back({ 2, "audio", "Surround 5.1", "eng", "flac", true, false });
    r.metadata.push_back({ "title", "Test Video 4K" });

    std::string report = r.formatReport();
    assert(report.find("sample_4k.mkv") != std::string::npos);
    assert(report.find("3840 x 2160") != std::string::npos);
    assert(report.find("d3d11va") != std::string::npos);
    assert(report.find("flac") != std::string::npos);
    assert(report.find("Direct3D 11 NV12") != std::string::npos);
    assert(report.find("Test Video 4K") != std::string::npos);

    std::cout << "  -> testDiagnostics PASSED!" << std::endl;
}

void testAudioEqualizer() {
    std::cout << "[TEST] Running testAudioEqualizer..." << std::endl;

    rx::audio::EqualizerState state;
    assert(state.preset == rx::audio::EqualizerPreset::Flat);
    assert(!state.enabled);
    assert(state.bass == 0.0);
    assert(state.treble == 0.0);
    assert(!state.normalize);
    assert(!state.stereoDownmix);

    // Initial filter string when disabled and zeroed should be empty
    std::string filterEmpty = rx::audio::Equalizer::buildFilterString(state);
    assert(filterEmpty.empty());

    // Enable Equalizer
    state.enabled = true;
    rx::audio::Equalizer::applyPreset(state, rx::audio::EqualizerPreset::Rock);
    assert(state.preset == rx::audio::EqualizerPreset::Rock);
    assert(state.gains[0] > 0); // Rock has bass boost

    std::string filterRock = rx::audio::Equalizer::buildFilterString(state);
    assert(filterRock.find("equalizer=f=") != std::string::npos);
    assert(filterRock.find("lavfi=[") != std::string::npos);

    // Tone controls
    state.bass = 6.0;
    state.treble = -3.0;
    std::string filterTone = rx::audio::Equalizer::buildFilterString(state);
    assert(filterTone.find("bass=g=6") != std::string::npos);
    assert(filterTone.find("treble=g=-3") != std::string::npos);

    // Normalization & stereo downmix
    state.normalize = true;
    state.stereoDownmix = true;
    std::string filterFull = rx::audio::Equalizer::buildFilterString(state);
    assert(filterFull.find("dynaudnorm") != std::string::npos);
    assert(filterFull.find("pan=stereo") != std::string::npos);

    // Reset
    state.reset();
    assert(state.preset == rx::audio::EqualizerPreset::Flat);
    assert(state.bass == 0.0);
    assert(!state.enabled);

    std::cout << "  -> testAudioEqualizer PASSED!" << std::endl;
}

void testSubtitleAndStreaming() {
    std::cout << "[TEST] Running testSubtitleAndStreaming..." << std::endl;

    // 1. SubtitleStyle defaults
    rx::SubtitleStyle style;
    assert(style.font == "Segoe UI");
    assert(style.fontSize == 28);
    assert(style.textColor == "#FFFFFF");
    assert(style.borderColor == "#000000");
    assert(style.borderSize == 3);
    assert(style.shadowOffset == 1);
    assert(style.marginVertical == 36);
    assert(style.bold == true);
    assert(style.italic == false);
    assert(style.delaySec == 0.0);

    // 2. Custom modifications
    style.font = "Roboto";
    style.fontSize = 36;
    style.textColor = "#FFFF00";
    style.borderColor = "#222222";
    style.borderSize = 5;
    style.shadowOffset = 2;
    style.marginVertical = 48;
    style.bold = false;
    style.italic = true;
    style.delaySec = 1.2;

    assert(style.font == "Roboto");
    assert(style.fontSize == 36);
    assert(style.textColor == "#FFFF00");
    assert(style.borderColor == "#222222");
    assert(style.borderSize == 5);
    assert(style.marginVertical == 48);
    assert(!style.bold);
    assert(style.italic);
    assert(style.delaySec == 1.2);

    // 3. Reset
    style.reset();
    assert(style.font == "Segoe UI");
    assert(style.fontSize == 28);
    assert(style.textColor == "#FFFFFF");
    assert(style.bold == true);
    assert(style.delaySec == 0.0);

    // 4. NetworkStreamConfig
    rx::NetworkStreamConfig net;
    assert(net.cacheSec == 15);
    assert(net.maxBufferMb == 100);
    assert(net.networkTimeoutSec == 30);
    assert(!net.userAgent.empty());

    net.url = "https://test-streams.mux.dev/x36xhzz/x36xhzz.m3u8";
    net.cacheSec = 30;
    net.maxBufferMb = 250;
    assert(net.url.find("m3u8") != std::string::npos);
    assert(net.cacheSec == 30);
    assert(net.maxBufferMb == 250);

    // 5. Persistence verification
    std::string testIni = "test_sub_settings.ini";
    auto& s = rx::Settings::instance();
    s.subtitle.style.font = "Arial";
    s.subtitle.style.fontSize = 32;
    s.subtitle.style.textColor = "#00FFFF";
    s.subtitle.style.borderSize = 4;
    s.subtitle.style.delaySec = -0.5;
    s.network.cacheSec = 60;
    s.network.maxBufferMb = 250;
    s.save(testIni);

    assert(std::filesystem::exists(testIni));

    // Reset and reload
    s.subtitle.style.font = "Default";
    s.subtitle.style.fontSize = 12;
    s.network.cacheSec = 10;
    s.load(testIni);

    assert(s.subtitle.style.font == "Arial");
    assert(s.subtitle.style.fontSize == 32);
    assert(s.subtitle.style.textColor == "#00FFFF");
    assert(s.subtitle.style.borderSize == 4);
    assert(s.subtitle.style.delaySec == -0.5);
    assert(s.network.cacheSec == 60);
    assert(s.network.maxBufferMb == 250);

    std::filesystem::remove(testIni);
    std::cout << "  -> testSubtitleAndStreaming PASSED!" << std::endl;
}

void testMediaDatabase() {
    std::cout << "[TEST] Running testMediaDatabase..." << std::endl;

    std::string testDb = "test_media.db";
    if (std::filesystem::exists(testDb)) std::filesystem::remove(testDb);

    auto& db = rx::library::MediaDatabase::instance();
    bool init = db.initialize(testDb);
    assert(init);
    assert(db.isOpen());

    // 1. Playback History & Resume
    db.recordPlaybackProgress("D:/Videos/Movie1.mp4", "Movie1.mp4", 125.5, 3600.0);
    double pos1 = db.getLastPosition("D:/Videos/Movie1.mp4");
    assert(pos1 == 125.5);

    // Update progress
    db.recordPlaybackProgress("D:/Videos/Movie1.mp4", "Movie1.mp4", 300.0, 3600.0);
    double pos2 = db.getLastPosition("D:/Videos/Movie1.mp4");
    assert(pos2 == 300.0);

    auto history = db.getRecentHistory(10);
    assert(!history.empty());
    assert(history[0].filePath == "D:/Videos/Movie1.mp4");
    assert(history[0].lastPosition == 300.0);
    assert(history[0].playCount == 2);

    // 2. Bookmarks
    bool b1 = db.addBookmark("D:/Videos/Movie1.mp4", 45.0, "Intro Scene");
    bool b2 = db.addBookmark("D:/Videos/Movie1.mp4", 120.0, "Action Chase");
    assert(b1 && b2);

    auto bms = db.getBookmarksForFile("D:/Videos/Movie1.mp4");
    assert(bms.size() == 2);
    assert(bms[0].timestampSec == 45.0);
    assert(bms[0].note == "Intro Scene");
    assert(bms[1].timestampSec == 120.0);

    // Delete one bookmark
    int idToDelete = bms[0].id;
    bool del = db.deleteBookmark(idToDelete);
    assert(del);

    auto bmsAfter = db.getBookmarksForFile("D:/Videos/Movie1.mp4");
    assert(bmsAfter.size() == 1);
    assert(bmsAfter[0].note == "Action Chase");

    // Close and remove db
    db.close();
    std::filesystem::remove(testDb);
    // Also remove WAL / SHM files if created
    std::filesystem::remove(testDb + "-wal");
    std::filesystem::remove(testDb + "-shm");

    std::cout << "  -> testMediaDatabase PASSED!" << std::endl;
}

void testShortcuts() {
    std::cout << "[TEST] Running testShortcuts..." << std::endl;

    auto& sm = rx::ShortcutManager::instance();
    sm.restoreDefaults();

    // Verify lookup of default actions
    assert(sm.findAction(VK_SPACE, false, false, false) == rx::PlayerAction::PlayPause);
    assert(sm.findAction('S', false, false, false) == rx::PlayerAction::Stop);
    assert(sm.findAction(VK_LEFT, false, false, false) == rx::PlayerAction::SeekBackward5);
    assert(sm.findAction(VK_RIGHT, false, false, false) == rx::PlayerAction::SeekForward5);
    assert(sm.findAction(VK_LEFT, true, false, false) == rx::PlayerAction::SeekBackward30);
    assert(sm.findAction(VK_RIGHT, true, false, false) == rx::PlayerAction::SeekForward30);
    assert(sm.findAction(VK_LEFT, false, true, false) == rx::PlayerAction::SeekBackward60);
    assert(sm.findAction(VK_RIGHT, false, true, false) == rx::PlayerAction::SeekForward60);
    assert(sm.findAction(VK_UP, false, false, false) == rx::PlayerAction::VolumeUp5);
    assert(sm.findAction(VK_DOWN, false, false, false) == rx::PlayerAction::VolumeDown5);
    assert(sm.findAction(VK_UP, true, false, false) == rx::PlayerAction::VolumeUp1);
    assert(sm.findAction(VK_DOWN, true, false, false) == rx::PlayerAction::VolumeDown1);
    assert(sm.findAction('M', false, false, false) == rx::PlayerAction::MuteToggle);
    assert(sm.findAction('X', false, false, false) == rx::PlayerAction::SpeedDown);
    assert(sm.findAction('C', false, false, false) == rx::PlayerAction::SpeedUp);
    assert(sm.findAction('Z', false, false, false) == rx::PlayerAction::SpeedReset);
    assert(sm.findAction('F', false, false, false) == rx::PlayerAction::ToggleFullscreen);
    assert(sm.findAction(VK_RETURN, false, false, false) == rx::PlayerAction::ToggleFullscreen);
    assert(sm.findAction('T', true, false, false) == rx::PlayerAction::ToggleAlwaysOnTop);
    assert(sm.findAction('O', true, false, false) == rx::PlayerAction::OpenFile);
    assert(sm.findAction('O', true, true, false) == rx::PlayerAction::OpenFolder);
    assert(sm.findAction('P', true, false, false) == rx::PlayerAction::TogglePlaylist);
    assert(sm.findAction(VK_F6, false, false, false) == rx::PlayerAction::TogglePlaylist);
    assert(sm.findAction('B', true, false, false) == rx::PlayerAction::ToggleRepeat);
    assert(sm.findAction('H', true, false, false) == rx::PlayerAction::ToggleShuffle);
    assert(sm.findAction('A', false, false, false) == rx::PlayerAction::CycleAudioTrack);
    assert(sm.findAction('L', false, false, false) == rx::PlayerAction::CycleSubtitleTrack);
    assert(sm.findAction('L', false, false, true) == rx::PlayerAction::ToggleSubtitles);
    assert(sm.findAction(VK_LEFT, false, false, true) == rx::PlayerAction::SubtitleDelayDown);
    assert(sm.findAction(VK_RIGHT, false, false, true) == rx::PlayerAction::SubtitleDelayUp);
    assert(sm.findAction('S', true, true, false) == rx::PlayerAction::LoadSubtitles);
    assert(sm.findAction(VK_TAB, false, false, false) == rx::PlayerAction::ShowDetailedInfo);
    assert(sm.findAction(VK_TAB, false, true, false) == rx::PlayerAction::ShowBriefInfo);
    assert(sm.findAction(VK_F1, true, false, false) == rx::PlayerAction::OpenPlaybackSystemInfo);
    assert(sm.findAction(VK_F5, false, false, false) == rx::PlayerAction::OpenPreferences);
    assert(sm.findAction(VK_F1, false, false, false) == rx::PlayerAction::OpenShortcutHelp);

    // Serialization & Parsing
    rx::ShortcutBinding b1{ 'K', true, false, false };
    std::wstring b1Str = b1.toString();
    assert(b1Str == L"Ctrl + K");
    rx::ShortcutBinding b1Parsed = rx::ShortcutBinding::fromString(b1Str);
    assert(b1Parsed == b1);
    (void)b1Parsed;

    // Conflict detection
    rx::ShortcutBinding spaceBinding{ VK_SPACE, false, false, false };
    rx::PlayerAction conflict = sm.checkConflict(spaceBinding, rx::PlayerAction::Stop);
    assert(conflict == rx::PlayerAction::PlayPause);
    (void)conflict;

    // Custom re-binding
    rx::ShortcutBinding customBinding{ 'K', true, true, false };
    bool ok = sm.setPrimaryBinding(rx::PlayerAction::PlayPause, customBinding);
    assert(ok);
    assert(sm.findAction('K', true, true, false) == rx::PlayerAction::PlayPause);
    (void)ok;

    // Reset to defaults
    sm.restoreDefaults();
    assert(sm.findAction(VK_SPACE, false, false, false) == rx::PlayerAction::PlayPause);

    std::cout << "  -> testShortcuts PASSED!" << std::endl;
}

void testAudioTracksAndOpenSubtitles() {
    std::cout << "[TEST] Running testAudioTracksAndOpenSubtitles..." << std::endl;

    // 1. Language code resolution
    assert(rx::track_formatter::getLanguageDisplayName("hin") == L"Hindi");
    assert(rx::track_formatter::getLanguageDisplayName("tel") == L"Telugu");
    assert(rx::track_formatter::getLanguageDisplayName("tam") == L"Tamil");
    assert(rx::track_formatter::getLanguageDisplayName("mal") == L"Malayalam");
    assert(rx::track_formatter::getLanguageDisplayName("spa") == L"Spanish");
    assert(rx::track_formatter::getLanguageDisplayName("jpn") == L"Japanese");
    assert(rx::track_formatter::getLanguageDisplayName("eng") == L"English");

    // 2. Audio track menu label & OSD
    rx::MediaTrack audioTrack;
    audioTrack.id = 1;
    audioTrack.type = "audio";
    audioTrack.lang = "hin";
    audioTrack.title = "Main Audio";
    audioTrack.codec = "eac3";
    audioTrack.channels = 6;
    audioTrack.sampleRate = 48000;
    audioTrack.bitrate = 192000;

    std::wstring audioMenuLabel = rx::track_formatter::formatAudioTrackMenuLabel(audioTrack, 1);
    assert(audioMenuLabel.find(L"Hindi") != std::wstring::npos);
    assert(audioMenuLabel.find(L"Main Audio") != std::wstring::npos);
    assert(audioMenuLabel.find(L"DDP 5.1") != std::wstring::npos);
    assert(audioMenuLabel.find(L"Audio 1") != std::wstring::npos);
    assert(audioMenuLabel.find(L"Dolby Digital Plus") != std::wstring::npos);
    assert(audioMenuLabel.find(L"48.0 kHz") != std::wstring::npos);
    assert(audioMenuLabel.find(L"5.1 Ch") != std::wstring::npos);
    assert(audioMenuLabel.find(L"192 kbit/s") != std::wstring::npos);

    std::wstring audioOsd = rx::track_formatter::formatAudioTrackOsd(audioTrack);
    assert(audioOsd.find(L"Hindi") != std::wstring::npos);
    assert(audioOsd.find(L"Dolby Digital Plus") != std::wstring::npos);
    assert(audioOsd.find(L"5.1") != std::wstring::npos);

    // 3. Subtitle track menu label & OSD
    rx::MediaTrack subTrack;
    subTrack.id = 2;
    subTrack.type = "sub";
    subTrack.lang = "eng";
    subTrack.title = "Full Subtitles";
    subTrack.codec = "subrip";
    subTrack.isDefault = true;
    subTrack.isForced = false;

    std::wstring subMenuLabel = rx::track_formatter::formatSubtitleTrackMenuLabel(subTrack, 1);
    assert(subMenuLabel.find(L"English") != std::wstring::npos);
    assert(subMenuLabel.find(L"Full Subtitles") != std::wstring::npos);
    assert(subMenuLabel.find(L"SRT") != std::wstring::npos);
    assert(subMenuLabel.find(L"Subtitle 1") != std::wstring::npos);
    assert(subMenuLabel.find(L"[Default]") != std::wstring::npos);

    std::wstring subOsd = rx::track_formatter::formatSubtitleTrackOsd(subTrack);
    assert(subOsd.find(L"English") != std::wstring::npos);
    assert(subOsd.find(L"SRT") != std::wstring::npos);

    // 4. OpenSubtitles Filename Parsing
    std::string movieFile = "Dunkirk.2017.1080p.BluRay.x264-SPARKS.mkv";
    auto movieMeta = rx::subtitles::OpenSubtitlesClient::parseFilename(movieFile);
    assert(movieMeta.title == "Dunkirk");
    assert(movieMeta.year == 2017);
    assert(movieMeta.season == 0);
    assert(movieMeta.episode == 0);

    std::string tvFile = "Breaking.Bad.S05E16.Felina.720p.HDTV.mkv";
    auto tvMeta = rx::subtitles::OpenSubtitlesClient::parseFilename(tvFile);
    assert(tvMeta.title == "Breaking Bad");
    assert(tvMeta.season == 5);
    assert(tvMeta.episode == 16);

    // 5. OpenSubtitles MovieHash Formatting
    uint64_t hashVal = 0x8e245d9679d31e12ULL;
    std::string hashStr = rx::subtitles::OpenSubtitlesClient::formatMovieHash(hashVal);
    assert(hashStr == "8e245d9679d31e12");

    // 6. Channel mode in settings persistence
    auto& s = rx::Settings::instance();
    s.channelMode = rx::AudioChannelMode::Left;
    s.opensubtitles.apiKey = "rx-sub-test-api-key";
    s.opensubtitles.preferredLanguages = "eng,hin";
    s.opensubtitles.autoSearch = true;
    s.opensubtitles.applyAfterDownload = true;
    s.save();

    s.channelMode = rx::AudioChannelMode::Both;
    s.opensubtitles.apiKey.clear();
    s.load();

    assert(s.channelMode == rx::AudioChannelMode::Left);
    assert(s.opensubtitles.apiKey == "rx-sub-test-api-key");
    assert(s.opensubtitles.preferredLanguages == "eng,hin");
    assert(s.opensubtitles.autoSearch == true);
    assert(s.opensubtitles.applyAfterDownload == true);

    std::cout << "  -> testAudioTracksAndOpenSubtitles PASSED!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RX Media Player Automated Test Runner" << std::endl;
    std::cout << "========================================" << std::endl;

    testUtils();
    testPlaylist();
    testSettings();
    testVideoAdjustments();
    testDiagnostics();
    testAudioEqualizer();
    testSubtitleAndStreaming();
    testMediaDatabase();
    testShortcuts();
    testAudioTracksAndOpenSubtitles();

    std::cout << "========================================" << std::endl;
    std::cout << "ALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
