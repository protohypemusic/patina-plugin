#include "RandomizeSystem.h"
#include <cmath>

// Module name strings used as save/load keys
const char* RandomizeSystem::moduleNames[NUM_MODULES] = {
    "noise", "wobble", "distort", "resonator", "space", "mix"
};

// Flat parameter table -- each entry tagged with its owner module.
// Ranges are tuned for musically useful randomization results.
const RandomizeSystem::ParamEntry RandomizeSystem::params[NUM_PARAMS] = {
    // Noise
    { "noise_amount", 0.00f, 0.45f, false, ModuleId::Noise },
    { "noise_tone",   0.20f, 0.80f, false, ModuleId::Noise },
    { "noise_type",   0.00f, 2.00f, true,  ModuleId::Noise },    // White/Pink/Brown

    // Wobble
    { "wobble_amount", 0.00f, 0.50f, false, ModuleId::Wobble },
    { "wobble_rate",   0.10f, 0.70f, false, ModuleId::Wobble },

    // Distort
    { "distort_amount", 0.00f, 0.60f, false, ModuleId::Distort },
    { "distort_tone",   0.20f, 0.80f, false, ModuleId::Distort },
    { "distort_type",   0.00f, 4.00f, true,  ModuleId::Distort }, // Soft/Hard/Diode/Fold/Crush

    // Resonator
    { "resonator_amount", 0.00f, 0.55f, false, ModuleId::Resonator },
    { "resonator_freq",   0.10f, 0.80f, false, ModuleId::Resonator },
    { "resonator_reso",   0.10f, 0.60f, false, ModuleId::Resonator },
    { "resonator_type",   0.00f, 2.00f, true,  ModuleId::Resonator }, // Comb/Modal/Formant

    // Space
    { "space_amount", 0.00f, 0.70f, false, ModuleId::Space },
    { "space_decay",  0.10f, 0.80f, false, ModuleId::Space },

    // Mix
    { "mix",          0.50f, 1.00f, false, ModuleId::Mix },
};

RandomizeSystem::RandomizeSystem()
{
    locked.fill(false);
}

bool RandomizeSystem::isLocked(ModuleId m) const
{
    return locked[static_cast<int>(m)];
}

void RandomizeSystem::setLocked(ModuleId m, bool lock)
{
    locked[static_cast<int>(m)] = lock;
}

void RandomizeSystem::toggleLocked(ModuleId m)
{
    int idx = static_cast<int>(m);
    locked[idx] = !locked[idx];
}

void RandomizeSystem::randomize(juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        auto& entry = params[i];

        // Skip if this parameter's owner module is locked
        if (locked[static_cast<int>(entry.owner)])
            continue;

        auto* param = apvts.getParameter(entry.id);
        if (param == nullptr)
            continue;

        if (entry.isInt)
        {
            // Integer parameter: pick a random int in [min, max]
            int intMin = static_cast<int>(entry.minVal);
            int intMax = static_cast<int>(entry.maxVal);
            int val = intMin + rng.nextInt(intMax - intMin + 1);
            param->setValueNotifyingHost(
                param->convertTo0to1(static_cast<float>(val)));
        }
        else
        {
            // Float parameter: random in [min, max]
            float val = entry.minVal + rng.nextFloat() * (entry.maxVal - entry.minVal);
            param->setValueNotifyingHost(param->convertTo0to1(val));
        }
    }
}

void RandomizeSystem::saveToValueTree(juce::ValueTree& state) const
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        juce::String key = juce::String("lock_") + moduleNames[i];
        state.setProperty(key, locked[static_cast<size_t>(i)], nullptr);
    }
}

void RandomizeSystem::loadFromValueTree(const juce::ValueTree& state)
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        juce::String key = juce::String("lock_") + moduleNames[i];
        if (state.hasProperty(key))
            locked[static_cast<size_t>(i)] = static_cast<bool>(state.getProperty(key));
    }
}
