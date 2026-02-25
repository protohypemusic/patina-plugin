#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>

/**
 * NoiseModule — Generates vinyl crackle, tape hiss, and static noise.
 *
 * Three noise types blended together:
 *   - Vinyl crackle: Poisson-triggered filtered impulses (surface crackle + dust pops)
 *   - Tape hiss: Shaped white noise with high-shelf character
 *   - Static: Broadband noise with subtle modulation
 *
 * Amount (0.0-1.0) controls overall noise level mixed into signal.
 * At low amounts, mostly subtle hiss. At high amounts, heavy crackle and static.
 */
class NoiseModule
{
public:
    NoiseModule();
    ~NoiseModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process stereo buffer — adds noise to existing signal */
    void process(juce::AudioBuffer<float>& buffer, float amount);

private:
    // ---- Fast PRNG (xorshift32) ----
    uint32_t rngState = 0x12345678;

    float nextRandom();           // Returns [0, 1)
    float nextRandomBipolar();    // Returns [-1, 1)
    float nextGaussian();         // Approximate Gaussian via sum-of-4-uniform

    // ---- Vinyl Crackle ----
    struct CrackleState
    {
        // Surface crackle — frequent small impulses
        float surfaceEnvelope = 0.0f;
        float surfaceDecay = 0.0f;

        // Dust pop — infrequent louder impulses
        float popEnvelope = 0.0f;
        float popDecay = 0.0f;

        // Bandpass filter state for crackle shaping
        float bp_z1 = 0.0f;
        float bp_z2 = 0.0f;
    };

    CrackleState crackleL, crackleR;
    float crackleRate = 0.0f;       // Surface crackle density (per sample probability)
    float popRate = 0.0f;           // Dust pop density (per sample probability)

    // Revolution modulator (simulates groove rotation)
    float revolutionPhase = 0.0f;
    float revolutionRate = 0.0f;    // ~0.55 Hz for 33 RPM

    float generateCrackle(CrackleState& state, float revolutionMod);

    // ---- Tape Hiss ----
    struct HissFilter
    {
        // One-pole high shelf for tape hiss character
        float z1 = 0.0f;
        float shelfCoeff = 0.0f;
        float shelfGain = 0.0f;
    };

    HissFilter hissL, hissR;

    float generateHiss(HissFilter& filter);

    // ---- Static ----
    struct StaticState
    {
        float modPhase = 0.0f;
        float modRate = 0.0f;
    };

    StaticState staticState;

    float generateStatic();

    // ---- DC Blocker ----
    struct DCBlocker
    {
        float x1 = 0.0f;
        float y1 = 0.0f;
    };

    DCBlocker dcL, dcR;

    float blockDC(DCBlocker& state, float input);

    // ---- General ----
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseModule)
};
