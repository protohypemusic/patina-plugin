#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

/**
 * DistortModule -- Continuous saturation from subtle warmth to hard clipping.
 *
 * Full 0.0-1.0 range is oversampled asymmetric tanh waveshaping:
 *   - Drive: 1x at 0.0 -> 31x at 1.0 (quadratic ramp)
 *   - Even harmonics from asymmetric transfer function
 *   - 2x oversampling for anti-aliasing
 *   - Pre-filter darkens from 12 kHz -> 4 kHz as drive increases
 *   - Output is always safe (tanh clamps to +/-1)
 */
class DistortModule
{
public:
    DistortModule();
    ~DistortModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process stereo buffer -- applies saturation in-place */
    void process(juce::AudioBuffer<float>& buffer, float amount);

private:
    // ---- Saturation ----
    float waveshape(float x, float drive, float asymmetry);

    // ---- Oversampling (2x) ----
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // ---- Pre-filter (darkens with drive) ----
    juce::dsp::StateVariableTPTFilter<float> preFilterL, preFilterR;

    // ---- DC Blocker (removes DC from asymmetric saturation) ----
    struct DCBlocker
    {
        float x1 = 0.0f;
        float y1 = 0.0f;
    };

    DCBlocker dcL, dcR;
    float blockDC(DCBlocker& state, float input);

    // ---- General ----
    double sampleRate = 44100.0;
    int numChannels = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortModule)
};
