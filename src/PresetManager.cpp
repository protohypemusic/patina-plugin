#include "PresetManager.h"

// ================================================================
// Factory Presets
//
// Ordered from subtle → extreme for a nice browsing experience.
// All values are raw [0, 1] matching APVTS parameter range.
// filter_amount: 0.5 = flat, <0.5 = high-pass, >0.5 = low-pass
// ================================================================

const PresetManager::Preset PresetManager::kPresets[PresetManager::kNumPresets] =
{
    //  name               noise  wobble  distort  space  flux   filter  mix    wRate  sDecay
    { "Init",              0.00f, 0.00f,  0.00f,  0.00f,  0.00f, 0.50f, 1.00f, 0.30f, 0.30f },
    { "Subtle Warmth",     0.10f, 0.15f,  0.04f,  0.08f,  0.00f, 0.46f, 1.00f, 0.25f, 0.25f },
    { "Tape Hiss",         0.35f, 0.25f,  0.05f,  0.12f,  0.00f, 0.47f, 1.00f, 0.35f, 0.30f },
    { "Vinyl Crackle",     0.65f, 0.10f,  0.12f,  0.08f,  0.10f, 0.50f, 1.00f, 0.20f, 0.25f },
    { "Cassette Dreams",   0.50f, 0.55f,  0.08f,  0.18f,  0.05f, 0.44f, 0.90f, 0.40f, 0.35f },
    { "Dreamscape",        0.08f, 0.20f,  0.04f,  0.65f,  0.08f, 0.50f, 0.95f, 0.15f, 0.70f },
    { "Haunted Room",      0.15f, 0.12f,  0.08f,  0.75f,  0.12f, 0.50f, 0.90f, 0.10f, 0.80f },
    { "Ghost Signal",      0.12f, 0.30f,  0.04f,  0.60f,  0.15f, 0.52f, 0.80f, 0.50f, 0.65f },
    { "Warped",            0.18f, 0.70f,  0.25f,  0.28f,  0.50f, 0.50f, 0.90f, 0.60f, 0.40f },
    { "Telephone",         0.25f, 0.08f,  0.45f,  0.04f,  0.05f, 0.22f, 1.00f, 0.30f, 0.20f },
    { "Wrecked Radio",     0.65f, 0.35f,  0.72f,  0.08f,  0.35f, 0.32f, 0.85f, 0.45f, 0.25f },
    { "Dark Matter",       0.48f, 0.40f,  0.55f,  0.48f,  0.30f, 0.40f, 0.85f, 0.35f, 0.55f },
};

// ================================================================

PresetManager::PresetManager() = default;

void PresetManager::setParam(juce::AudioProcessorValueTreeState& apvts,
                              const char* id, float value)
{
    if (auto* param = apvts.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void PresetManager::applyPreset(const Preset& p,
                                 juce::AudioProcessorValueTreeState& apvts)
{
    setParam(apvts, "noise_amount",   p.noise);
    setParam(apvts, "wobble_amount",  p.wobble);
    setParam(apvts, "distort_amount", p.distort);
    setParam(apvts, "space_amount",   p.space);
    setParam(apvts, "flux_amount",    p.flux);
    setParam(apvts, "filter_amount",  p.filter);
    setParam(apvts, "mix",            p.mix);
    setParam(apvts, "wobble_rate",    p.wobbleRate);
    setParam(apvts, "space_decay",    p.spaceDecay);
}

void PresetManager::loadPreset(int index, juce::AudioProcessorValueTreeState& apvts)
{
    currentIndex = juce::jlimit(0, kNumPresets - 1, index);
    applyPreset(kPresets[currentIndex], apvts);
}

void PresetManager::nextPreset(juce::AudioProcessorValueTreeState& apvts)
{
    loadPreset((currentIndex + 1) % kNumPresets, apvts);
}

void PresetManager::prevPreset(juce::AudioProcessorValueTreeState& apvts)
{
    loadPreset((currentIndex - 1 + kNumPresets) % kNumPresets, apvts);
}

void PresetManager::setCurrentIndexOnly(int index)
{
    currentIndex = juce::jlimit(0, kNumPresets - 1, index);
}
