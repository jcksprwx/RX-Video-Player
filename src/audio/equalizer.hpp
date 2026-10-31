#pragma once

#include <vector>
#include <string>
#include <array>

namespace rx::audio {

constexpr size_t kEqualizerBandCount = 10;
constexpr std::array<int, kEqualizerBandCount> kBandFrequencies = {
    31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};

enum class EqualizerPreset {
    Flat,
    Rock,
    Pop,
    Jazz,
    Classical,
    Speech,
    BassBoost,
    TrebleBoost,
    Movie,
    Custom
};

struct EqualizerState {
    bool enabled = false;
    EqualizerPreset preset = EqualizerPreset::Flat;
    std::array<double, kEqualizerBandCount> gains{}; // in dB (-12.0 to +12.0)
    double bass = 0.0;    // -10.0 to +10.0 dB
    double treble = 0.0;  // -10.0 to +10.0 dB
    bool normalize = false; // EBU R128 Loudness Normalization
    bool stereoDownmix = false;
    double leftRightBalance = 0.0; // -1.0 (Full Left) to +1.0 (Full Right)

    void reset() {
        enabled = false;
        preset = EqualizerPreset::Flat;
        gains.fill(0.0);
        bass = 0.0;
        treble = 0.0;
        normalize = false;
        stereoDownmix = false;
        leftRightBalance = 0.0;
    }
};

class Equalizer {
public:
    static void applyPreset(EqualizerState& state, EqualizerPreset preset);
    static std::string buildFilterString(const EqualizerState& state);
    static const char* getPresetName(EqualizerPreset preset);
};

} // namespace rx::audio
