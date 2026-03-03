#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

/**
 * RandomizeSystem -- Per-module lock/randomize system.
 *
 * Locking a module locks ALL of its parameters together
 * (e.g., locking "Noise" locks noise_amount, noise_tone, AND noise_type).
 *
 * Modules: Noise, Wobble, Distort, Resonator, Space, Mix
 */
class RandomizeSystem
{
public:
    enum class ModuleId
    {
        Noise = 0,
        Wobble,
        Distort,
        Resonator,
        Space,
        Mix,
        Count
    };

    static constexpr int NUM_MODULES = static_cast<int>(ModuleId::Count);

    RandomizeSystem();

    bool isLocked(ModuleId m) const;
    void setLocked(ModuleId m, bool locked);
    void toggleLocked(ModuleId m);

    /** Randomize all unlocked parameters. */
    void randomize(juce::AudioProcessorValueTreeState& apvts);

    /** Save lock states into a ValueTree (for preset recall). */
    void saveToValueTree(juce::ValueTree& state) const;

    /** Load lock states from a ValueTree. */
    void loadFromValueTree(const juce::ValueTree& state);

    /** Module name strings for save/load keys. */
    static const char* moduleNames[NUM_MODULES];

private:
    struct ParamEntry
    {
        const char* id;       // APVTS parameter ID
        float minVal;         // Randomize min
        float maxVal;         // Randomize max
        bool  isInt;          // true = AudioParameterInt
        ModuleId owner;       // Which module owns this param
    };

    static constexpr int NUM_PARAMS = 15;
    static const ParamEntry params[NUM_PARAMS];

    std::array<bool, NUM_MODULES> locked{};

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RandomizeSystem)
};
