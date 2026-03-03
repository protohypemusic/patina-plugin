#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * WobbleModule -- Volume tremolo with adjustable rate.
 *
 * Sine-LFO amplitude modulation:
 *   - Rate knob (0-1) maps to 0.5-12 Hz (exponential)
 *   - Amount knob (0-1) controls depth from none to full
 *   - Mono: both channels use same LFO phase (volume wobble, not pan)
 *   - Amount changes smoothed over ~10ms to avoid clicks
 */
class WobbleModule
{
public:
    WobbleModule() = default;
    ~WobbleModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process stereo buffer -- applies volume tremolo */
    void process(juce::AudioBuffer<float>& buffer, float amount, float rate);

private:
    double sampleRate = 44100.0;

    // LFO phase accumulators (0-1 range)
    float phaseL = 0.0f;
    float phaseR = 0.0f;

    // Smoothed amount to avoid clicks
    float smoothedAmount = 0.0f;
    float smoothingCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WobbleModule)
};
