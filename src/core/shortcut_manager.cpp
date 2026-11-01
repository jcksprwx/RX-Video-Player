#include "shortcut_manager.hpp"
#include "settings.hpp"
#include "utils.hpp"
#include <algorithm>
#include <sstream>

namespace rx {

std::wstring ShortcutBinding::toString() const {
    if (vk == 0) return L"None";

    std::wstring result;
    if (ctrl) result += L"Ctrl + ";
    if (shift) result += L"Shift + ";
    if (alt) result += L"Alt + ";

    switch (vk) {
        case VK_SPACE:      result += L"Space"; break;
        case VK_RETURN:     result += L"Enter"; break;
        case VK_LEFT:       result += L"Left"; break;
        case VK_RIGHT:      result += L"Right"; break;
        case VK_UP:         result += L"Up"; break;
        case VK_DOWN:       result += L"Down"; break;
        case VK_PRIOR:      result += L"Page Up"; break;
        case VK_NEXT:       result += L"Page Down"; break;
        case VK_HOME:       result += L"Home"; break;
        case VK_END:        result += L"End"; break;
        case VK_TAB:        result += L"Tab"; break;
        case VK_ESCAPE:     result += L"Esc"; break;
        case VK_BACK:       result += L"Backspace"; break;
        case VK_DELETE:     result += L"Delete"; break;
        case VK_OEM_PLUS:
        case VK_ADD:        result += L"Plus"; break;
        case VK_OEM_MINUS:
        case VK_SUBTRACT:   result += L"Minus"; break;
        case VK_OEM_4:      result += L"["; break;
        case VK_OEM_6:      result += L"]"; break;
        case VK_OEM_COMMA:  result += L","; break;
        case VK_OEM_PERIOD: result += L"."; break;
        default:
            if (vk >= VK_F1 && vk <= VK_F12) {
                result += L"F" + std::to_wstring(vk - VK_F1 + 1);
            } else if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
                result += static_cast<wchar_t>(vk);
            } else {
                wchar_t keyName[32] = {0};
                UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
                if (GetKeyNameTextW(scanCode << 16, keyName, 32) > 0) {
                    result += keyName;
                } else {
                    result += L"Key(" + std::to_wstring(vk) + L")";
                }
            }
            break;
    }
    return result;
}

ShortcutBinding ShortcutBinding::fromString(const std::wstring& str) {
    ShortcutBinding b;
    if (str.empty() || str == L"None") return b;

    std::wstringstream ss(str);
    std::wstring token;
    std::vector<std::wstring> tokens;
    while (std::getline(ss, token, L'+')) {
        size_t first = token.find_first_not_of(L" \t");
        size_t last = token.find_last_not_of(L" \t");
        if (first != std::wstring::npos && last != std::wstring::npos) {
            tokens.push_back(token.substr(first, last - first + 1));
        }
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
        std::wstring t = tokens[i];
        std::wstring lowerT = t;
        std::transform(lowerT.begin(), lowerT.end(), lowerT.begin(), ::towlower);

        if (lowerT == L"ctrl" || lowerT == L"control") {
            b.ctrl = true;
        } else if (lowerT == L"shift") {
            b.shift = true;
        } else if (lowerT == L"alt") {
            b.alt = true;
        } else {
            // Key part
            if (lowerT == L"space") b.vk = VK_SPACE;
            else if (lowerT == L"enter" || lowerT == L"return") b.vk = VK_RETURN;
            else if (lowerT == L"left") b.vk = VK_LEFT;
            else if (lowerT == L"right") b.vk = VK_RIGHT;
            else if (lowerT == L"up") b.vk = VK_UP;
            else if (lowerT == L"down") b.vk = VK_DOWN;
            else if (lowerT == L"page up" || lowerT == L"pageup" || lowerT == L"pgup") b.vk = VK_PRIOR;
            else if (lowerT == L"page down" || lowerT == L"pagedown" || lowerT == L"pgdn") b.vk = VK_NEXT;
            else if (lowerT == L"home") b.vk = VK_HOME;
            else if (lowerT == L"end") b.vk = VK_END;
            else if (lowerT == L"tab") b.vk = VK_TAB;
            else if (lowerT == L"esc" || lowerT == L"escape") b.vk = VK_ESCAPE;
            else if (lowerT == L"plus" || lowerT == L"=") b.vk = VK_OEM_PLUS;
            else if (lowerT == L"minus" || lowerT == L"-") b.vk = VK_OEM_MINUS;
            else if (lowerT == L"[") b.vk = VK_OEM_4;
            else if (lowerT == L"]") b.vk = VK_OEM_6;
            else if (lowerT.size() >= 2 && lowerT[0] == L'f' && iswdigit(lowerT[1])) {
                int fNum = std::stoi(lowerT.substr(1));
                if (fNum >= 1 && fNum <= 12) b.vk = static_cast<WORD>(VK_F1 + fNum - 1);
            } else if (t.size() == 1) {
                wchar_t ch = towupper(t[0]);
                if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
                    b.vk = static_cast<WORD>(ch);
                }
            }
        }
    }
    return b;
}

ShortcutManager::ShortcutManager() {
    initialize();
}

ShortcutManager& ShortcutManager::instance() {
    static ShortcutManager s_inst;
    return s_inst;
}

void ShortcutManager::registerAction(PlayerAction action, const std::string& internalName, const std::wstring& label,
                                     const std::wstring& category, const std::vector<ShortcutBinding>& defaults,
                                     const std::wstring& desc) {
    ActionInfo info;
    info.action = action;
    info.internalName = internalName;
    info.label = label;
    info.category = category;
    info.defaultBindings = defaults;
    info.description = desc;

    m_actions.push_back(info);
    m_currentBindings[action] = defaults;

    for (const auto& b : defaults) {
        if (!b.empty()) {
            m_lookupMap[b] = action;
        }
    }
}

void ShortcutManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_actions.clear();
    m_currentBindings.clear();
    m_lookupMap.clear();

    // 1. Playback
    registerAction(PlayerAction::PlayPause, "PlayPause", L"Play / Pause", L"Playback",
                   { { VK_SPACE, false, false, false } }, L"Toggle play and pause");
    registerAction(PlayerAction::Stop, "Stop", L"Stop Playback", L"Playback",
                   { { 'S', false, false, false } }, L"Stop playback and reset position");
    registerAction(PlayerAction::SeekBeginning, "SeekBeginning", L"Seek to Beginning", L"Playback",
                   { { VK_HOME, false, false, false } }, L"Jump to start of media");
    registerAction(PlayerAction::SeekEnd, "SeekEnd", L"Seek to End", L"Playback",
                   { { VK_END, false, false, false } }, L"Jump to end of media");

    // 2. Seeking
    registerAction(PlayerAction::SeekBackward5, "SeekBackward5", L"Seek Backward 5s", L"Seeking",
                   { { VK_LEFT, false, false, false } }, L"Jump backward 5 seconds");
    registerAction(PlayerAction::SeekForward5, "SeekForward5", L"Seek Forward 5s", L"Seeking",
                   { { VK_RIGHT, false, false, false } }, L"Jump forward 5 seconds");
    registerAction(PlayerAction::SeekBackward30, "SeekBackward30", L"Seek Backward 30s", L"Seeking",
                   { { VK_LEFT, true, false, false } }, L"Jump backward 30 seconds");
    registerAction(PlayerAction::SeekForward30, "SeekForward30", L"Seek Forward 30s", L"Seeking",
                   { { VK_RIGHT, true, false, false } }, L"Jump forward 30 seconds");
    registerAction(PlayerAction::SeekBackward60, "SeekBackward60", L"Seek Backward 1m", L"Seeking",
                   { { VK_LEFT, false, true, false } }, L"Jump backward 60 seconds");
    registerAction(PlayerAction::SeekForward60, "SeekForward60", L"Seek Forward 1m", L"Seeking",
                   { { VK_RIGHT, false, true, false } }, L"Jump forward 60 seconds");

    // 3. Playlist Navigation
    registerAction(PlayerAction::PreviousFile, "PreviousFile", L"Previous File", L"Playlist",
                   { { VK_PRIOR, false, false, false } }, L"Play previous item in playlist");
    registerAction(PlayerAction::NextFile, "NextFile", L"Next File", L"Playlist",
                   { { VK_NEXT, false, false, false } }, L"Play next item in playlist");
    registerAction(PlayerAction::NextPlaylistItem, "NextPlaylistItem", L"Open Next Item", L"Playlist",
                   { { 'N', true, false, false } }, L"Advance to next item");
    registerAction(PlayerAction::TogglePlaylist, "TogglePlaylist", L"Toggle Playlist", L"Playlist",
                   { { 'P', true, false, false }, { VK_F6, false, false, false } }, L"Show or hide playlist drawer");
    registerAction(PlayerAction::ToggleRepeat, "ToggleRepeat", L"Toggle Repeat", L"Playlist",
                   { { 'B', true, false, false } }, L"Cycle repeat off, all, or current track");
    registerAction(PlayerAction::ToggleShuffle, "ToggleShuffle", L"Toggle Shuffle", L"Playlist",
                   { { 'H', true, false, false } }, L"Shuffle playlist items");

    // 4. Volume
    registerAction(PlayerAction::VolumeUp5, "VolumeUp5", L"Volume Up (5%)", L"Volume",
                   { { VK_UP, false, false, false } }, L"Increase volume by 5%");
    registerAction(PlayerAction::VolumeDown5, "VolumeDown5", L"Volume Down (5%)", L"Volume",
                   { { VK_DOWN, false, false, false } }, L"Decrease volume by 5%");
    registerAction(PlayerAction::VolumeUp1, "VolumeUp1", L"Volume Up (1%)", L"Volume",
                   { { VK_UP, true, false, false } }, L"Fine volume increase by 1%");
    registerAction(PlayerAction::VolumeDown1, "VolumeDown1", L"Volume Down (1%)", L"Volume",
                   { { VK_DOWN, true, false, false } }, L"Fine volume decrease by 1%");
    registerAction(PlayerAction::MuteToggle, "MuteToggle", L"Mute / Unmute", L"Volume",
                   { { 'M', false, false, false } }, L"Toggle audio mute state");

    // 5. Playback Speed
    registerAction(PlayerAction::SpeedDown, "SpeedDown", L"Decrease Speed", L"Speed",
                   { { 'X', false, false, false } }, L"Decrease playback speed by 0.10x");
    registerAction(PlayerAction::SpeedUp, "SpeedUp", L"Increase Speed", L"Speed",
                   { { 'C', false, false, false } }, L"Increase playback speed by 0.10x");
    registerAction(PlayerAction::SpeedReset, "SpeedReset", L"Reset Speed (1.0x)", L"Speed",
                   { { 'Z', false, false, false } }, L"Restore normal 1.00x playback speed");

    // 6. Video
    registerAction(PlayerAction::ZoomReset, "ZoomReset", L"Reset Zoom", L"Video",
                   { { '0', true, false, false } }, L"Reset canvas zoom to 100%");
    registerAction(PlayerAction::ZoomIn, "ZoomIn", L"Zoom In", L"Video",
                   { { VK_OEM_PLUS, true, false, false }, { VK_ADD, true, false, false } }, L"Zoom canvas by +10%");
    registerAction(PlayerAction::ZoomOut, "ZoomOut", L"Zoom Out", L"Video",
                   { { VK_OEM_MINUS, true, false, false }, { VK_SUBTRACT, true, false, false } }, L"Zoom canvas by -10%");
    registerAction(PlayerAction::Rotate90, "Rotate90", L"Rotate 90° Clockwise", L"Video",
                   { { 'R', true, false, false } }, L"Rotate video 90 degrees");

    // 7. Window
    registerAction(PlayerAction::ToggleFullscreen, "ToggleFullscreen", L"Toggle Fullscreen", L"Window",
                   { { VK_RETURN, false, false, false }, { 'F', false, false, false } }, L"Toggle fullscreen presentation");
    registerAction(PlayerAction::ToggleAlwaysOnTop, "ToggleAlwaysOnTop", L"Toggle Always on Top", L"Window",
                   { { 'T', true, false, false } }, L"Pin player window on top of others");

    // 8. File Operations
    registerAction(PlayerAction::OpenFile, "OpenFile", L"Open Media File", L"File",
                   { { 'O', true, false, false } }, L"Show open file dialog");
    registerAction(PlayerAction::OpenFolder, "OpenFolder", L"Open Folder", L"File",
                   { { 'O', true, true, false } }, L"Open entire folder into playlist");

    // 9. Audio
    registerAction(PlayerAction::CycleAudioTrack, "CycleAudioTrack", L"Cycle Audio Tracks", L"Audio",
                   { { 'A', false, false, true }, { 'A', false, false, false } }, L"Switch to next audio stream (Alt+A)");
    registerAction(PlayerAction::OpenAudioMenu, "OpenAudioMenu", L"Open Audio Stream Menu", L"Audio",
                   { { 'A', true, true, false } }, L"Open popup audio track selection menu");
    registerAction(PlayerAction::AudioChannelBoth, "AudioChannelBoth", L"Channel Mode: Both", L"Audio",
                   {}, L"Stereo/multichannel audio output");
    registerAction(PlayerAction::AudioChannelLeft, "AudioChannelLeft", L"Channel Mode: Left", L"Audio",
                   {}, L"Output left channel to both speakers");
    registerAction(PlayerAction::AudioChannelRight, "AudioChannelRight", L"Channel Mode: Right", L"Audio",
                   {}, L"Output right channel to both speakers");

    // 10. Subtitles
    registerAction(PlayerAction::CycleSubtitleTrack, "CycleSubtitleTrack", L"Cycle Subtitle Tracks", L"Subtitles",
                   { { 'L', false, false, false } }, L"Switch to next subtitle stream");
    registerAction(PlayerAction::ToggleSubtitles, "ToggleSubtitles", L"Toggle Subtitles", L"Subtitles",
                   { { 'L', false, false, true } }, L"Turn subtitles on or off");
    registerAction(PlayerAction::SubtitleDelayDown, "SubtitleDelayDown", L"Subtitle Delay (-100ms)", L"Subtitles",
                   { { VK_LEFT, false, false, true } }, L"Advance subtitles by 100ms");
    registerAction(PlayerAction::SubtitleDelayUp, "SubtitleDelayUp", L"Subtitle Delay (+100ms)", L"Subtitles",
                   { { VK_RIGHT, false, false, true } }, L"Delay subtitles by 100ms");
    registerAction(PlayerAction::LoadSubtitles, "LoadSubtitles", L"Load External Subtitles", L"Subtitles",
                   { { 'S', true, true, false } }, L"Load external SRT, ASS, or VTT file");
    registerAction(PlayerAction::OpenSubtitleMenu, "OpenSubtitleMenu", L"Open Subtitle Stream Menu", L"Subtitles",
                   { { 'L', true, true, false } }, L"Open popup subtitle track selection menu");
    registerAction(PlayerAction::DownloadOnlineSubtitles, "DownloadOnlineSubtitles", L"Download Subtitles Online", L"Subtitles",
                   { { 'D', true, false, false } }, L"Search and download subtitles from OpenSubtitles.com");

    // 11. Information
    registerAction(PlayerAction::ShowDetailedInfo, "ShowDetailedInfo", L"Detailed Playback Info", L"Information",
                   { { VK_TAB, false, false, false } }, L"Toggle on-screen telemetry stats");
    registerAction(PlayerAction::ShowBriefInfo, "ShowBriefInfo", L"Brief Playback Info", L"Information",
                   { { VK_TAB, false, true, false } }, L"Display current timestamp and duration");
    registerAction(PlayerAction::OpenPlaybackSystemInfo, "OpenPlaybackSystemInfo", L"Playback / System Info Dialog", L"Information",
                   { { VK_F1, true, false, false } }, L"Open 3-tab technical diagnostics utility");
    registerAction(PlayerAction::ShowFileInfo, "ShowFileInfo", L"File Information", L"Information",
                   { { 'I', true, false, false } }, L"Open media metadata dialog");

    // 12. Application
    registerAction(PlayerAction::OpenPreferences, "OpenPreferences", L"Open Preferences", L"Application",
                   { { VK_F5, false, false, false } }, L"Show preferences dialog");
    registerAction(PlayerAction::CaptureScreenshot, "CaptureScreenshot", L"Capture Screenshot", L"Application",
                   { { 'C', true, false, false } }, L"Save current video frame to PNG");
    registerAction(PlayerAction::OpenShortcutHelp, "OpenShortcutHelp", L"Keyboard Shortcuts Help", L"Application",
                   { { VK_F1, false, false, false } }, L"Display searchable shortcut reference");
}

const ActionInfo* ShortcutManager::getActionInfo(PlayerAction action) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& a : m_actions) {
        if (a.action == action) return &a;
    }
    return nullptr;
}

PlayerAction ShortcutManager::findAction(WORD vk, bool ctrl, bool shift, bool alt) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    ShortcutBinding b{ vk, ctrl, shift, alt };
    auto it = m_lookupMap.find(b);
    if (it != m_lookupMap.end()) {
        return it->second;
    }
    return PlayerAction::None;
}

std::vector<ShortcutBinding> ShortcutManager::getBindings(PlayerAction action) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_currentBindings.find(action);
    if (it != m_currentBindings.end()) {
        return it->second;
    }
    return {};
}

std::wstring ShortcutManager::getBindingString(PlayerAction action) const {
    auto bindings = getBindings(action);
    if (bindings.empty() || bindings[0].empty()) return L"None";

    std::wstring str;
    for (size_t i = 0; i < bindings.size(); ++i) {
        if (i > 0) str += L", ";
        str += bindings[i].toString();
    }
    return str;
}

PlayerAction ShortcutManager::checkConflict(const ShortcutBinding& binding, PlayerAction targetAction) const {
    if (binding.empty()) return PlayerAction::None;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_lookupMap.find(binding);
    if (it != m_lookupMap.end() && it->second != targetAction) {
        return it->second;
    }
    return PlayerAction::None;
}

bool ShortcutManager::setPrimaryBinding(PlayerAction action, const ShortcutBinding& binding, std::wstring* outConflictDesc) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!binding.empty()) {
        auto it = m_lookupMap.find(binding);
        if (it != m_lookupMap.end() && it->second != action) {
            PlayerAction conflictingAction = it->second;
            std::wstring actName = L"Another Action";
            for (const auto& a : m_actions) {
                if (a.action == conflictingAction) {
                    actName = a.label;
                    break;
                }
            }
            if (outConflictDesc) {
                *outConflictDesc = L"Shortcut '" + binding.toString() + L"' is already assigned to '" + actName + L"'.";
            }
            return false;
        }
    }

    // Remove old bindings from lookup
    auto oldIt = m_currentBindings.find(action);
    if (oldIt != m_currentBindings.end()) {
        for (const auto& b : oldIt->second) {
            m_lookupMap.erase(b);
        }
    }

    // Set new binding
    if (binding.empty()) {
        m_currentBindings[action] = {};
    } else {
        m_currentBindings[action] = { binding };
        m_lookupMap[binding] = action;
    }

    return true;
}

void ShortcutManager::clearBinding(PlayerAction action) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_currentBindings.find(action);
    if (it != m_currentBindings.end()) {
        for (const auto& b : it->second) {
            m_lookupMap.erase(b);
        }
        m_currentBindings[action] = {};
    }
}

void ShortcutManager::restoreDefaults() {
    initialize();
}

void ShortcutManager::loadFromSettings(const Settings& /*settings*/) {
    // In future expansions, custom user overrides loaded from settings.ini
}

void ShortcutManager::saveToSettings(Settings& /*settings*/) const {
    // In future expansions, custom user overrides saved to settings.ini
}

} // namespace rx
