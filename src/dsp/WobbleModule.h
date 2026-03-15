#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * WobbleModule -- Volume tremolo with adjustable rate.
 *
 * Sine-LFO amplitude modulation:
 *   - Rate knob (0-1) maps to 0.5-12 Hz (exponential) in free mode
 *   - Tempo sync mode locks LFO to DAW tempo at note divisions
 *   - Amount knob (0-1) controls depth from none to full
 *   - Mono: both channels use same LFO phase (volume wobble, not pan)
 *   - Amount changes smoothed over ~10ms to avoid clicks
 *
 * Rate modes: 0=Free, 1=1/1, 2=1/2, 3=1/4, 4=1/8, 5=1/8T, 6=1/16, 7=1/16T, 8=1/32
 */
class WobbleModule
{
public:
    WobbleModule() = default;
    ~WobbleModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process stereo buffer -- applies volume tremolo.
     *  rateMode: 0=free (uses rate knob), 1-8=tempo-synced note division
     *  bpm: current DAW tempo (only used when rateMode > 0)
     *  shape: 0=sine (smooth), 1=square (hard chop)
     */
    void process(juce::AudioBuffer<float>& buffer, float amount, float rate,
                 int rateMode = 0, double bpm = 120.0, float shape = 0.0f);

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
