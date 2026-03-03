#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdint>
#include <array>

/**
 * NoiseModule -- White, Pink, and Crackle noise with tone control.
 *
 * Types:
 *   0 = White   (flat spectrum)
 *   1 = Pink    (1/f -- Paul Kellet approximation)
 *   2 = Crackle (vinyl surface noise + random pops)
 *
 * Parameters:
 *   amount : 0-1 noise level added to signal
 *   tone   : 0-1 (0 = dark/LP, 0.5 = flat, 1.0 = bright/HP)
 *   type   : 0/1/2
 *
 * Noise is gated by an envelope follower so it only sounds when
 * audio signal is present (musical, not a constant hiss).
 *
 * Noise is routed FIRST in the chain so downstream distortion
 * squishes noise and signal together.
 */
class NoiseModule
{
public:
    NoiseModule();
    ~NoiseModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    void process(juce::AudioBuffer<float>& buffer,
                 float amount, float tone, int type);

private:
    double sr = 44100.0;

    // Fast PRNG (xorshift32)
    uint32_t rngState = 0x12345678;
    float nextRandom();         // [0, 1)
    float nextRandomBipolar();  // [-1, 1)

    // Pink noise state (Paul Kellet method, per channel)
    struct PinkState
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    };
    std::array<PinkState, 2> pink{};

    // Crackle noise state (vinyl surface + pops, per channel)
    struct CrackleState
    {
        float surfaceLp    = 0.0f;   // LP filter state for surface noise
        float surfaceHpOut = 0.0f;   // HP filter output
        float surfaceHpIn  = 0.0f;   // HP filter previous input
        float popEnv       = 0.0f;   // Pop impulse decay envelope
    };
    std::array<CrackleState, 2> crackle{};

    // Tone filter state (one-pole LP + HP, per channel)
    std::array<float, 2> lpZ{};
    std::array<float, 2> hpZ{};

    // DC blocker
    std::array<float, 2> dcX{}, dcY{};

    // Envelope follower (gates noise by input level)
    float envState = 0.0f;
    float envAttack  = 0.0f;
    float envRelease = 0.0f;

    // Smoothing
    float sAmount = 0.0f;
    float sTone   = 0.0f;

    // Helpers
    float generateWhite();
    float generatePink(int ch);
    float generateCrackle(int ch);
    float applyTone(int ch, float in, float tone);
    float tickDC(int ch, float in);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseModule)
};
