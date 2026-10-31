#pragma once

#include "../theme.hpp"
#include "../../audio/equalizer.hpp"
#include "../../audio/wasapi_enumerator.hpp"
#include "../../engine/mpv_backend.hpp"
#include <windows.h>
#include <vector>
#include <functional>

namespace rx::ui {

class RXAudioPanel {
public:
    using AudioChangeCallback = std::function<void(const audio::EqualizerState&, const std::string& deviceId, double delayMs, bool exclusive)>;

    RXAudioPanel(MpvBackend& engine);
    ~RXAudioPanel();

    bool create(HWND parentHwnd, int x, int y, int width, int height);
    void move(int x, int y, int width, int height);
    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    void refreshDevices();
    void refreshTracks();
    void setEqualizerState(const audio::EqualizerState& state);
    const audio::EqualizerState& getEqualizerState() const { return m_eqState; }

    void setCallback(AudioChangeCallback cb) { m_callback = std::move(cb); }
    HWND getHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void onPaint(HDC hdc);
    void onVScroll(HWND trackbarHwnd, int pos);
    void onHScroll(HWND trackbarHwnd, int pos);
    void onCommand(WORD id, WORD code);
    void notifyChange();
    void updateLabels();

    MpvBackend& m_engine;
    HWND m_hwnd = nullptr;
    HWND m_parent = nullptr;
    bool m_visible = false;

    // Devices & Output
    HWND m_comboDevice = nullptr;
    HWND m_chkExclusive = nullptr;
    HWND m_chkNormalize = nullptr;
    HWND m_chkDownmix = nullptr;

    // Delay
    HWND m_tbDelay = nullptr;
    HWND m_lblValDelay = nullptr;

    // EQ Preset & Bands
    HWND m_chkEqEnable = nullptr;
    HWND m_comboPreset = nullptr;
    HWND m_btnResetEq = nullptr;
    std::array<HWND, audio::kEqualizerBandCount> m_tbBands{};
    std::array<HWND, audio::kEqualizerBandCount> m_lblBands{};

    // Tone & Balance
    HWND m_tbBass = nullptr;
    HWND m_lblValBass = nullptr;
    HWND m_tbTreble = nullptr;
    HWND m_lblValTreble = nullptr;
    HWND m_tbBalance = nullptr;
    HWND m_lblValBalance = nullptr;

    // Tracks
    HWND m_comboTracks = nullptr;

    HFONT m_font = nullptr;
    HFONT m_titleFont = nullptr;
    HFONT m_smallFont = nullptr;

    audio::EqualizerState m_eqState;
    std::string m_selectedDeviceId = "auto";
    bool m_exclusiveMode = false;
    double m_audioDelayMs = 0.0;
    std::vector<audio::AudioEndpoint> m_endpoints;

    AudioChangeCallback m_callback;
};

} // namespace rx::ui
