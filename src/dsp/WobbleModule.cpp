#include "WobbleModule.h"
#include <cmath>

// ============================================================
// WobbleModule -- Volume Tremolo
// ============================================================

void WobbleModule::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    // Smoothing coefficient: ~10ms attack for amount changes
    float smoothTimeSeconds = 0.01f;
    smoothingCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * smoothTimeSeconds));

    reset();
}

void WobbleModule::reset()
{
    phaseL = 0.0f;
    phaseR = 0.0f;  // Same phase as L — mono volume tremolo, not stereo pan
    smoothedAmount = 0.0f;
}

// Beat multipliers for tempo-synced modes (index 1-8).
// Multiplied by (BPM / 60) to get LFO frequency in Hz.
// 1/1=0.25, 1/2=0.5, 1/4=1, 1/8=2, 1/8T=3, 1/16=4, 1/16T=6, 1/32=8
static constexpr float kSyncMultipliers[8] = {
    0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f
};

void WobbleModule::process(juce::AudioBuffer<float>& buffer, float amount, float rate,
                           int rateMode, double bpm, float shape)
{
    if (amount < 0.001f)
        return;

    auto numSamples = buffer.getNumSamples();
    auto* leftChannel  = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    float freq;
    if (rateMode > 0 && rateMode <= 8)
    {
        // Tempo-synced: calculate frequency from BPM and note division
        float beatsPerSec = static_cast<float>(bpm) / 60.0f;
        freq = beatsPerSec * kSyncMultipliers[rateMode - 1];
    }
    else
    {
        // Free mode: 0.0 -> 0.5 Hz, 1.0 -> 12 Hz (exponential)
        freq = 0.5f * std::pow(24.0f, rate);
    }
    float phaseInc = freq / static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        // Smooth amount to avoid clicks when user turns knob
        smoothedAmount += smoothingCoeff * (amount - smoothedAmount);

        // LFO waveform: blend from sine (shape=0) to square (shape=1)
        float sinL = std::sin(phaseL * juce::MathConstants<float>::twoPi);
        float sinR = std::sin(phaseR * juce::MathConstants<float>::twoPi);

        // Shape: raise sine to a power to steepen it toward square
        // At shape=0: pure sine. At shape=1: hard square.
        if (shape > 0.001f)
        {
            // Steepen: use tanh-based shaping for smooth sine-to-square morph
            // Drive increases with shape: 1x (sine) to 20x (near-square)
            float drive = 1.0f + shape * 19.0f;
            sinL = std::tanh(sinL * drive) / std::tanh(drive);
            sinR = std::tanh(sinR * drive) / std::tanh(drive);
        }

        // Gain: 1.0 - amount * (0.5 - 0.5 * lfo)
        // At amount=0: gain=1.0 (no effect)
        // At amount=1: gain swings 0.0 to 1.0
        float gainL = 1.0f - smoothedAmount * (0.5f - 0.5f * sinL);
        float gainR = 1.0f - smoothedAmount * (0.5f - 0.5f * sinR);

        leftChannel[i]  *= gainL;
        rightChannel[i] *= gainR;

        // Advance phases
        phaseL += phaseInc;
        phaseR += phaseInc;

        if (phaseL >= 1.0f) phaseL -= 1.0f;
        if (phaseR >= 1.0f) phaseR -= 1.0f;
    }
}
