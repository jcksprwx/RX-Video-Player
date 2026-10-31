#include "equalizer.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace rx::audio {

const char* Equalizer::getPresetName(EqualizerPreset preset) {
    switch (preset) {
        case EqualizerPreset::Flat:        return "Flat";
        case EqualizerPreset::Rock:        return "Rock";
        case EqualizerPreset::Pop:         return "Pop";
        case EqualizerPreset::Jazz:        return "Jazz";
        case EqualizerPreset::Classical:   return "Classical";
        case EqualizerPreset::Speech:      return "Speech / Vocal";
        case EqualizerPreset::BassBoost:   return "Bass Boost";
        case EqualizerPreset::TrebleBoost: return "Treble Boost";
        case EqualizerPreset::Movie:       return "Movie / Cinema";
        case EqualizerPreset::Custom:      return "Custom";
    }
    return "Custom";
}

void Equalizer::applyPreset(EqualizerState& state, EqualizerPreset preset) {
    state.preset = preset;
    state.bass = 0.0;
    state.treble = 0.0;

    switch (preset) {
        case EqualizerPreset::Flat:
            state.gains = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
            break;
        case EqualizerPreset::Rock:
            state.gains = { 4.5, 3.5, 1.0, -1.5, -2.0, 0.0, 2.0, 3.5, 4.5, 5.0 };
            break;
        case EqualizerPreset::Pop:
            state.gains = { -1.5, 0.0, 2.0, 3.5, 4.0, 3.0, 1.0, -0.5, -1.0, -1.5 };
            break;
        case EqualizerPreset::Jazz:
            state.gains = { 3.5, 2.5, 1.0, 1.5, -1.5, -1.5, 0.0, 1.5, 3.0, 4.0 };
            break;
        case EqualizerPreset::Classical:
            state.gains = { 4.5, 3.5, 2.5, 2.0, -1.5, -1.5, 0.0, 2.0, 3.5, 4.0 };
            break;
        case EqualizerPreset::Speech:
            state.gains = { -3.5, -1.0, 0.0, 2.5, 4.5, 4.0, 3.5, 1.5, 0.0, -2.0 };
            break;
        case EqualizerPreset::BassBoost:
            state.gains = { 7.0, 6.0, 4.5, 2.5, 0.5, 0.0, 0.0, 0.0, 0.0, 0.0 };
            break;
        case EqualizerPreset::TrebleBoost:
            state.gains = { 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.5, 4.5, 6.5, 8.0 };
            break;
        case EqualizerPreset::Movie:
            state.gains = { 4.0, 2.5, 0.0, -1.0, 1.5, 2.5, 1.0, 2.0, 3.5, 4.5 };
            break;
        case EqualizerPreset::Custom:
            break;
    }
}

std::string Equalizer::buildFilterString(const EqualizerState& state) {
    std::vector<std::string> filters;

    // 1. Equalizer bands using lavfi equalizer filters
    if (state.enabled) {
        bool hasNonZeroGain = false;
        for (double g : state.gains) {
            if (std::abs(g) > 0.05) {
                hasNonZeroGain = true;
                break;
            }
        }

        if (hasNonZeroGain) {
            for (size_t i = 0; i < kEqualizerBandCount; ++i) {
                double g = state.gains[i];
                if (std::abs(g) > 0.05) {
                    std::ostringstream ss;
                    ss << "equalizer=f=" << kBandFrequencies[i] << ":width_type=o:w=1:g=" << std::fixed << std::setprecision(1) << g;
                    filters.push_back(ss.str());
                }
            }
        }
    }

    // 2. Bass & Treble tone controls
    if (std::abs(state.bass) > 0.05) {
        std::ostringstream ss;
        ss << "bass=g=" << std::fixed << std::setprecision(1) << state.bass;
        filters.push_back(ss.str());
    }
    if (std::abs(state.treble) > 0.05) {
        std::ostringstream ss;
        ss << "treble=g=" << std::fixed << std::setprecision(1) << state.treble;
        filters.push_back(ss.str());
    }

    // 3. Stereo downmix if multichannel
    if (state.stereoDownmix) {
        filters.push_back("pan=stereo|c0=0.5*c2+0.707*c0+0.707*c4+0.5*c3|c1=0.5*c2+0.707*c1+0.707*c5+0.5*c3");
    }

    // 4. Loudness Normalization (dynaudnorm)
    if (state.normalize) {
        filters.push_back("dynaudnorm=f=150:g=15");
    }

    // 5. Left / Right balance
    if (std::abs(state.leftRightBalance) > 0.05) {
        double bal = state.leftRightBalance;
        double leftVol = (bal <= 0.0) ? 1.0 : (1.0 - bal);
        double rightVol = (bal >= 0.0) ? 1.0 : (1.0 + bal);
        std::ostringstream ss;
        ss << "volume=volume=" << leftVol << ":precision=fixed,volume=volume=" << rightVol << ":precision=fixed";
        // Or simpler balance via pan filter:
        std::ostringstream panSs;
        panSs << "pan=stereo|c0=" << leftVol << "*c0|c1=" << rightVol << "*c1";
        filters.push_back(panSs.str());
    }

    if (filters.empty()) return "";

    std::ostringstream finalFilter;
    for (size_t i = 0; i < filters.size(); ++i) {
        if (i > 0) finalFilter << ",";
        finalFilter << filters[i];
    }
    return "lavfi=[" + finalFilter.str() + "]";
}

} // namespace rx::audio
