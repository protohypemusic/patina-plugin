#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <array>

/**
 * SpaceModule — Multi-algorithm spatial effects.
 *
 * Types:
 *   0 = PLATE  — Dattorro plate reverb (~15ms pre-delay, medium density)
 *   1 = ROOM   — Tighter Dattorro variant (5ms pre-delay, heavy damping)
 *   2 = HALL   — Spacious Dattorro variant (30ms pre-delay, less damping)
 *   3 = TONAL  — Short comb-filter delay (20-30ms) with filtered feedback
 *
 * Dattorro reverb architecture (types 0-2):
 *   - 4 cascaded input allpass diffusers
 *   - 2 cross-coupled feedback loops (the "tank")
 *   - Each loop: allpass -> delay -> damping lowpass -> delay
 *   - Multi-tap output for stereo width
 *
 * Tape delay (type 3):
 *   - Short stereo delay (20-30ms) creating comb-filter coloring
 *   - One-pole lowpass on feedback path for warmth
 *   - Decay controls feedback/resonance intensity
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

    /** Process stereo buffer — adds reverb/delay wet signal.
     *  type: 0=Plate, 1=Room, 2=Hall, 3=Tape
     */
    void process(juce::AudioBuffer<float>& buffer, float amount, float decayParam,
                 int type = 0);

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

    /** Internal: tonal delay processing path (type 3) */
    void processTonal(juce::AudioBuffer<float>& buffer, float amount, float decayParam);

    // ---- Tonal Delay (type 3) ----
    SimpleDelay tonalDelayL;
    SimpleDelay tonalDelayR;
    float tonalFeedbackL = 0.0f;
    float tonalFeedbackR = 0.0f;
    float tonalLpfL_z1 = 0.0f;
    float tonalLpfR_z1 = 0.0f;

    // ---- Parameters ----
    double sampleRate = 44100.0;
    float decay = 0.5f;
    float damping = 0.5f;

    // Smoothed parameter values
    juce::SmoothedValue<float> smoothedDecay;
    juce::SmoothedValue<float> smoothedDamping;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpaceModule)
};
