#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * FilterModule — Combined high-pass and low-pass filter with single knob.
 *
 * Uses two juce::dsp::StateVariableTPTFilter instances:
 *   - Highpass: sweeps up as amount goes below 0.5
 *   - Lowpass: sweeps down as amount goes above 0.5
 *
 * Amount mapping:
 *   0.0 = HP at 500 Hz, LP at 20 kHz (strong bass cut)
 *   0.5 = HP at 20 Hz,  LP at 20 kHz (flat / no filtering)
 *   1.0 = HP at 20 Hz,  LP at 800 Hz (muffled / treble cut)
 *
 * Exponential frequency mapping for perceptually uniform knob response.
 * Multiplicative SmoothedValue for click-free filter sweeps.
 * Resonance at 0.6 for subtle vintage character (softer than Butterworth).
 *
 * Also accepts external modulation offset from FluxModule.
 */
class FilterModule
{
public:
    FilterModule();
    ~FilterModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process stereo buffer with filter amount and optional modulation */
    void process(juce::AudioBuffer<float>& buffer, float amount, float modOffset = 0.0f);

private:
    /** Exponential frequency mapping for perceptual linearity */
    static float mapFrequency(float normalized, float minFreq, float maxFreq);

    // Stereo filter pairs
    juce::dsp::StateVariableTPTFilter<float> highpassL, highpassR;
    juce::dsp::StateVariableTPTFilter<float> lowpassL, lowpassR;

    // Multiplicative smoothing for frequency parameters (superior to linear for audio)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedHPFreq;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedLPFreq;

    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterModule)
};
