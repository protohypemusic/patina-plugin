#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>

// ============================================================
// RandomizeSystem
//
// Generates randomized parameter values for all unlocked modules.
// Weighted ranges are tuned to produce musically useful patches
// rather than purely random noise.
//
// Lock state is stored here (not in the UI) so it persists
// through plugin state save/load.
// ============================================================

class RandomizeSystem
{
public:
    enum class ModuleId
    {
        Noise = 0,
        Wobble,
        Distort,
        Space,
        Flux,
        Filter,
        Mix,
        WobbleRate,
        SpaceDecay,
        Count
    };

    static constexpr int NUM_MODULES = static_cast<int>(ModuleId::Count);

    RandomizeSystem();

    // Randomize all unlocked parameters via APVTS
    void randomize(juce::AudioProcessorValueTreeState& apvts);

    // Lock / unlock individual modules
    void setLocked(ModuleId id, bool locked);
    bool isLocked(ModuleId id) const;
    void toggleLock(ModuleId id);

    // Persist lock state inside the plugin's ValueTree
    void saveToValueTree(juce::ValueTree& tree) const;
    void loadFromValueTree(const juce::ValueTree& tree);

private:
    struct Range { float minVal, maxVal; };

    // Parameter IDs must match PluginProcessor's createParameterLayout()
    static const char* paramIds[NUM_MODULES];

    // Weighted ranges: tuned to stay in musically useful territory
    static const Range ranges[NUM_MODULES];

    bool lockState[NUM_MODULES] = {};

    std::mt19937 rng;

    float randomInRange(float min, float max);
};
