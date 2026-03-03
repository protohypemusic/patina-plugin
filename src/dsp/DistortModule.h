#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <array>

/**
 * DistortModule -- Five distortion types with post-distortion tone control.
 *
 * Types (designed with increasing aggression):
 *   0 = Soft clip   (tanh -- warm, musical, gentle compression)
 *   1 = Hard clip   (flat clamp -- buzzy, aggressive, loud)
 *   2 = Diode       (asymmetric rectifier -- super heavy, massive even harmonics)
 *   3 = Wavefold    (triangle folding -- harmonically rich, synth-like)
 *   4 = Bitcrush    (sample-rate + bit reduction -- lo-fi digital)
 *
 * Parameters:
 *   amount : 0-1 drive level (0 = bypass)
 *   tone   : 0-1 post-distortion filter (0 = dark, 0.5 = flat, 1 = bright)
 *   type   : 0-4
 *
 * All types run through 2x oversampling to control aliasing.
 */
class DistortModule
{
public:
    DistortModule();
    ~DistortModule() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    void process(juce::AudioBuffer<float>& buffer,
                 float amount, float tone, int type);

private:
    double sr = 44100.0;
    int numChannels = 2;

    // 2x oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // Pre-filter (gentle HF taming before distortion)
    juce::dsp::StateVariableTPTFilter<float> preFilterL, preFilterR;

    // Post-distortion tone filter (one-pole LP + HP, per channel)
    std::array<float, 2> toneLpZ{};
    std::array<float, 2> toneHpIn{};
    std::array<float, 2> toneHpZ{};

    // DC blocker
    std::array<float, 2> dcX{}, dcY{};

    // Bitcrush state (per-channel to avoid stereo offset)
    std::array<float, 2> crushHold{};
    std::array<float, 2> crushPhase{};

    // Waveshapers
    static float softClip(float x, float drive);
    static float hardClip(float x, float drive);
    static float diodeClip(float x, float drive);
    static float waveFold(float x, float drive);
    float bitCrush(float x, float amount, int ch);

    // Tone + DC
    float applyTone(int ch, float in, float tone);
    float tickDC(int ch, float in);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DistortModule)
};
