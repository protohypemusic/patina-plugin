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

void WobbleModule::process(juce::AudioBuffer<float>& buffer, float amount, float rate)
{
    if (amount < 0.001f)
        return;

    auto numSamples = buffer.getNumSamples();
    auto* leftChannel  = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Rate mapping: 0.0 -> 0.5 Hz, 1.0 -> 12 Hz (exponential)
    // freq = 0.5 * pow(24, rate)
    float freq = 0.5f * std::pow(24.0f, rate);
    float phaseInc = freq / static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        // Smooth amount to avoid clicks when user turns knob
        smoothedAmount += smoothingCoeff * (amount - smoothedAmount);

        // LFO: sin oscillation mapped to gain
        // gain = 1.0 - amount * (0.5 - 0.5 * sin(phase))
        // At amount=0: gain=1.0 (no effect)
        // At amount=1: gain swings 0.0 to 1.0
        float sinL = std::sin(phaseL * juce::MathConstants<float>::twoPi);
        float sinR = std::sin(phaseR * juce::MathConstants<float>::twoPi);

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
