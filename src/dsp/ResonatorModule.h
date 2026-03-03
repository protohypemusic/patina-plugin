#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

class ResonatorModule
{
public:
    ResonatorModule();
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    /** Process audio through the resonator.
        amount: wet/dry mix (0=bypass, 1=full wet)
        freq:   raw 0-1 param (each type maps differently)
        reso:   resonance/feedback 0-1
        type:   0=Comb, 1=Modal, 2=Formant */
    void process(juce::AudioBuffer<float>& buffer,
                 float amount, float freq, float reso, int type);

private:
    double sr = 44100.0;

    // --- Comb filter ---
    static constexpr int kMaxDelay = 4096;
    std::array<std::array<float, kMaxDelay>, 2> combBuf{};
    std::array<int, 2> combWrite{};
    std::array<float, 2> combDamp{};

    // --- Biquad bandpass (shared by Modal + Formant) ---
    struct Biquad
    {
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    };

    // Modal: 6 resonant modes per channel
    static constexpr int kNumModes = 6;
    std::array<std::array<Biquad, kNumModes>, 2> modal{};

    // Formant: 3 bandpass filters per channel
    static constexpr int kNumFormants = 3;
    std::array<std::array<Biquad, kNumFormants>, 2> formant{};

    // --- DC blocker ---
    std::array<float, 2> dcX{}, dcY{};

    // --- Smoothing ---
    float sAmount = 0, sFreq = 0, sReso = 0;

    // --- Helpers ---
    static void calcBandpass(Biquad& b, double freq, double q, double sampleRate);
    static float tick(Biquad& b, float in);
    float tickComb(int ch, float in, float delaySamples, float feedback, float dampCoeff);
    float tickDC(int ch, float in);
};
