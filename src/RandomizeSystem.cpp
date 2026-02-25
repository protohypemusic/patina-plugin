#include "RandomizeSystem.h"
#include <chrono>

// Parameter IDs — must match createParameterLayout() in PluginProcessor.cpp
const char* RandomizeSystem::paramIds[NUM_MODULES] = {
    "noise_amount",   // Noise
    "wobble_amount",  // Wobble
    "distort_amount", // Distort
    "space_amount",   // Space
    "flux_amount",    // Flux
    "filter_amount",  // Filter
    "mix",            // Mix
    "wobble_rate",    // WobbleRate
    "space_decay"     // SpaceDecay
};

// Weighted ranges — designed to produce usable patches, not just chaos
// All parameters have range [0, 1] in APVTS, so these are actual values
const RandomizeSystem::Range RandomizeSystem::ranges[NUM_MODULES] = {
    { 0.00f, 0.45f },  // Noise:      subtle to moderate — above 0.5 is too harsh
    { 0.00f, 0.50f },  // Wobble:     up to medium tremolo
    { 0.00f, 0.60f },  // Distort:    full saturation range
    { 0.00f, 0.70f },  // Space:      reverb is pleasant across a wide range
    { 0.00f, 0.55f },  // Flux:       heavy glitch is disorienting above ~0.6
    { 0.30f, 0.70f },  // Filter:     keep near center — extremes are too coloring
    { 0.50f, 1.00f },  // Mix:        keep reasonably wet
    { 0.10f, 0.70f },  // WobbleRate: avoid extremes (very slow / ring-mod)
    { 0.10f, 0.80f },  // SpaceDecay: avoid extremes (too tight / infinite wash)
};

RandomizeSystem::RandomizeSystem()
    : rng(static_cast<unsigned>(
          std::chrono::high_resolution_clock::now().time_since_epoch().count()))
{
    for (int i = 0; i < NUM_MODULES; ++i)
        lockState[i] = false;
}

float RandomizeSystem::randomInRange(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

void RandomizeSystem::randomize(juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        if (lockState[i])
            continue;

        float val = randomInRange(ranges[i].minVal, ranges[i].maxVal);

        if (auto* param = apvts.getParameter(paramIds[i]))
        {
            // All our params have range [0,1], so actual value == normalized value.
            // convertTo0to1 makes this explicit and safe for future range changes.
            param->setValueNotifyingHost(param->convertTo0to1(val));
        }
    }
}

void RandomizeSystem::setLocked(ModuleId id, bool locked)
{
    lockState[static_cast<int>(id)] = locked;
}

bool RandomizeSystem::isLocked(ModuleId id) const
{
    return lockState[static_cast<int>(id)];
}

void RandomizeSystem::toggleLock(ModuleId id)
{
    int idx = static_cast<int>(id);
    lockState[idx] = !lockState[idx];
}

void RandomizeSystem::saveToValueTree(juce::ValueTree& tree) const
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        juce::String key = "lock_" + juce::String(paramIds[i]);
        tree.setProperty(key, lockState[i], nullptr);
    }
}

void RandomizeSystem::loadFromValueTree(const juce::ValueTree& tree)
{
    for (int i = 0; i < NUM_MODULES; ++i)
    {
        juce::String key = "lock_" + juce::String(paramIds[i]);
        if (tree.hasProperty(key))
            lockState[i] = static_cast<bool>(tree.getProperty(key));
    }
}
