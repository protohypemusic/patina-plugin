#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================
// PresetManager
//
// Manages factory presets for Patina.
// Lives in PluginProcessor so the current index persists
// when the editor is closed/reopened, and saves with DAW state.
//
// All parameter values are in raw [0, 1] range matching APVTS.
// ============================================================

class PresetManager
{
public:
    struct Preset
    {
        const char* name;
        float noise;       // noise_amount
        float wobble;      // wobble_amount
        float distort;     // distort_amount
        float space;       // space_amount
        float flux;        // flux_amount
        float filter;      // filter_amount  (0.5 = flat)
        float mix;         // mix master
        float wobbleRate;  // wobble_rate  (0.0–1.0, controls tremolo speed)
        float spaceDecay;  // space_decay  (0.0–1.0, controls reverb tail)
    };

    static constexpr int kNumPresets = 12;
    static const Preset kPresets[kNumPresets];

    PresetManager();

    int         getCurrentIndex() const { return currentIndex; }
    const char* getCurrentName()  const { return kPresets[currentIndex].name; }

    // Load preset and apply to APVTS (notifies host, updates all knobs)
    void loadPreset(int index, juce::AudioProcessorValueTreeState& apvts);
    void nextPreset(juce::AudioProcessorValueTreeState& apvts);
    void prevPreset(juce::AudioProcessorValueTreeState& apvts);

    // Restore index only (used in setStateInformation — params already restored from XML)
    void setCurrentIndexOnly(int index);

private:
    int currentIndex = 0;

    static void applyPreset(const Preset& p, juce::AudioProcessorValueTreeState& apvts);
    static void setParam(juce::AudioProcessorValueTreeState& apvts, const char* id, float value);
};
