#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdint>
#include <array>
#include <vector>

/**
 * FluxModule — Beat-repeat / glitch with cross-module modulation.
 *
 * Full-range beat repeat (0.05 to 1.0):
 *   - Low amount: rare, long-grain stutters at low wet mix
 *   - Mid amount: frequent stutters with medium grains
 *   - High amount: rapid-fire short-grain glitch chaos
 *
 * Also generates 3 smooth random modulation signals (Catmull-Rom)
 * that other modules read for organic drift:
 *   - filterMod  (very slow, ~0.1 Hz)
 *   - wobbleMod  (slow, ~0.5 Hz)
 *   - noiseMod   (medium, ~2.0 Hz)
 *
 * Grain playback loops within grain length with 2ms fade-in/out.
 */
class FluxModule
{
public:
    FluxModule();
    ~FluxModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Update modulation values — call once per mod-block */
    void update(float amount);

    /** Get current modulation values (already scaled by amount) */
    float getFilterMod() const { return filterMod; }
    float getWobbleMod() const { return wobbleMod; }
    float getNoiseMod() const { return noiseMod; }

    /** Process a buffer — applies beat-repeat stutter */
    void process(juce::AudioBuffer<float>& buffer, float amount);

    // Block rate for modulation computation
    static constexpr int MOD_BLOCK_SIZE = 32;

private:
    // ---- Smooth Random Generator ----
    class SmoothRandom
    {
    public:
        void prepare(double sampleRate, float frequencyHz, uint32_t seed);
        void reset();
        float getNextValue();

    private:
        std::array<float, 4> values = {};
        int samplesPerSegment = 44100;
        int sampleCounter = 0;
        uint32_t rngState;

        float nextRandom();
    };

    // 3 independent modulation sources (filter, wobble, noise)
    SmoothRandom modSources[3];

    // Current modulation output values
    float filterMod = 0.0f;   // For filter cutoff offset
    float wobbleMod = 0.0f;   // For wobble rate/depth offset
    float noiseMod = 0.0f;    // For noise level offset

    double sampleRate = 44100.0;

    // ---- Granular stutter ----
    // Circular buffer holds 2 seconds of stereo audio (L/R interleaved).
    std::vector<float> stutterBuffer;   // L/R interleaved: [L0, R0, L1, R1, ...]
    int  stutterBufFrames  = 0;         // capacity in frames (buffer.size() / 2)
    int  stutterWriteFrame = 0;         // current write position (in frames)
    bool stutterBufFull    = false;     // true once write position has wrapped once
    int  stutterReadFrame  = 0;         // current read position within grain (frames)
    int  stutterGrainStart = 0;         // where the current grain starts (frames)
    int  stutterLength     = 0;         // grain length in frames
    int  stutterCountdown  = 0;         // samples until next trigger check
    bool isStuttering      = false;
    int  fadeSamples       = 0;         // 2ms fade length in samples
    juce::Random stutterRng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FluxModule)
};
