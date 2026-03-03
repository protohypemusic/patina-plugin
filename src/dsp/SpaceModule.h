#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

/**
 * SpaceModule — Short plate/room reverb based on Dattorro topology.
 *
 * Dattorro plate reverb architecture:
 *   - 4 cascaded input allpass diffusers
 *   - 2 cross-coupled feedback loops (the "tank")
 *   - Each loop: allpass -> delay -> damping lowpass -> delay
 *   - Multi-tap output for stereo width
 *
 * Tuned for short decays (plate/room character, not hall).
 * Amount controls wet level. Decay controlled by separate parameter.
 * Pre-delay fixed at ~15ms for clarity.
 *
 * Based on Jon Dattorro, "Effect Design Part 1: Reverberator and
 * Other Filters" (JAES, 1997).
 */
class SpaceModule
{
public:
    SpaceModule();
    ~SpaceModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process stereo buffer — adds reverb wet signal */
    void process(juce::AudioBuffer<float>& buffer, float amount, float decayParam);

private:
    // ---- Allpass Filter ----
    class AllpassFilter
    {
    public:
        void setDelay(int delaySamples);
        void reset();
        float process(float input, float gain);
        float readAt(int offset) const;

    private:
        std::vector<float> buffer;
        int writeIndex = 0;
        int delayLength = 1;
    };

    // ---- Simple Delay Line ----
    class SimpleDelay
    {
    public:
        void setMaxDelay(int maxSamples);
        void setDelay(int delaySamples);
        void reset();
        void write(float sample);
        float read() const;
        float readAt(int offset) const;
        float readFractional(float offset) const;
        int getDelayLength() const { return delayLength; }

    private:
        std::vector<float> buffer;
        int writeIndex = 0;
        int delayLength = 1;
    };

    // ---- Input Diffusion ----
    std::array<AllpassFilter, 4> inputDiffusers;

    // ---- Tank (two cross-coupled loops) ----
    // Loop 1
    AllpassFilter tankAllpass1;
    SimpleDelay tankDelay1a;
    SimpleDelay tankDelay1b;
    float damping1_z1 = 0.0f;

    // Loop 2
    AllpassFilter tankAllpass2;
    SimpleDelay tankDelay2a;
    SimpleDelay tankDelay2b;
    float damping2_z1 = 0.0f;

    // Cross-coupling feedback
    float tank1bOutput = 0.0f;
    float tank2bOutput = 0.0f;

    // ---- Pre-delay ----
    SimpleDelay preDelay;
    int preDelaySamples = 0;

    // ---- Output taps ----
    // Tap positions for stereo output (scaled from Dattorro's paper)
    struct OutputTaps
    {
        int leftTaps[7] = {};
        int rightTaps[7] = {};
    };

    OutputTaps taps;

    // ---- Tank modulation LFOs (smears comb filter resonances) ----
    float lfoPhase1 = 0.0f;
    float lfoPhase2 = 0.0f;
    float lfoRate1 = 0.0f;
    float lfoRate2 = 0.0f;
    float modExcursion = 0.0f;  // Modulation depth in samples

    // ---- Parameters ----
    double sampleRate = 44100.0;
    float decay = 0.5f;
    float damping = 0.5f;

    // Smoothed parameter values
    juce::SmoothedValue<float> smoothedDecay;
    juce::SmoothedValue<float> smoothedDamping;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpaceModule)
};
