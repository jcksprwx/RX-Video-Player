#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>

namespace rx {

class Settings;

enum class PlayerAction {
    None = 0,

    // Playback
    PlayPause,
    Stop,
    SeekBeginning,
    SeekEnd,

    // Seeking
    SeekBackward5,
    SeekForward5,
    SeekBackward30,
    SeekForward30,
    SeekBackward60,
    SeekForward60,

    // Playlist Navigation
    PreviousFile,
    NextFile,
    NextPlaylistItem,
    TogglePlaylist,
    ToggleRepeat,
    ToggleShuffle,

    // Volume
    VolumeUp5,
    VolumeDown5,
    VolumeUp1,
    VolumeDown1,
    MuteToggle,

    // Speed
    SpeedDown,
    SpeedUp,
    SpeedReset,

    // Video & Window
    ToggleFullscreen,
    ToggleAlwaysOnTop,
    ZoomReset,
    ZoomIn,
    ZoomOut,
    Rotate90,

    // File
    OpenFile,
    OpenFolder,

    // Audio & Subtitles
    CycleAudioTrack,
    CycleSubtitleTrack,
    ToggleSubtitles,
    SubtitleDelayDown,
    SubtitleDelayUp,
    LoadSubtitles,
    OpenAudioMenu,
    OpenSubtitleMenu,
    DownloadOnlineSubtitles,
    AudioChannelBoth,
    AudioChannelLeft,
    AudioChannelRight,

    // Information & Settings
    ShowDetailedInfo,
    ShowBriefInfo,
    OpenPlaybackSystemInfo,
    OpenPreferences,
    ShowFileInfo,
    CaptureScreenshot,
    OpenShortcutHelp
};

struct ShortcutBinding {
    WORD vk = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    bool empty() const { return vk == 0; }

    bool operator==(const ShortcutBinding& o) const {
        return vk == o.vk && ctrl == o.ctrl && shift == o.shift && alt == o.alt;
    }

    bool operator<(const ShortcutBinding& o) const {
        if (vk != o.vk) return vk < o.vk;
        if (ctrl != o.ctrl) return ctrl < o.ctrl;
        if (shift != o.shift) return shift < o.shift;
        return alt < o.alt;
    }

    std::wstring toString() const;
    static ShortcutBinding fromString(const std::wstring& str);
};

struct ActionInfo {
    PlayerAction action = PlayerAction::None;
    std::string internalName;
    std::wstring label;
    std::wstring category;
    std::vector<ShortcutBinding> defaultBindings;
    std::wstring description;
};

class ShortcutManager {
public:
    static ShortcutManager& instance();

    // Initialization & Action Definitions
    void initialize();
    const std::vector<ActionInfo>& getActionDefinitions() const { return m_actions; }
    const ActionInfo* getActionInfo(PlayerAction action) const;

    // Fast Action Lookup from key event
    PlayerAction findAction(WORD vk, bool ctrl, bool shift, bool alt) const;

    // Binding queries
    std::vector<ShortcutBinding> getBindings(PlayerAction action) const;
    std::wstring getBindingString(PlayerAction action) const;

    // Customization & Conflict Management
    PlayerAction checkConflict(const ShortcutBinding& binding, PlayerAction targetAction) const;
    bool setPrimaryBinding(PlayerAction action, const ShortcutBinding& binding, std::wstring* outConflictDesc = nullptr);
    void clearBinding(PlayerAction action);
    void restoreDefaults();

    // Persistence with Settings
    void loadFromSettings(const Settings& settings);
    void saveToSettings(Settings& settings) const;

private:
    ShortcutManager();
    void registerAction(PlayerAction action, const std::string& internalName, const std::wstring& label,
                        const std::wstring& category, const std::vector<ShortcutBinding>& defaults,
                        const std::wstring& desc);

    mutable std::mutex m_mutex;
    std::vector<ActionInfo> m_actions;
    std::map<PlayerAction, std::vector<ShortcutBinding>> m_currentBindings;
    std::map<ShortcutBinding, PlayerAction> m_lookupMap;
};

} // namespace rx
