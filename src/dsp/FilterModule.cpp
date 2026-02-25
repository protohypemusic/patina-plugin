#include "FilterModule.h"
#include <cmath>
#include <algorithm>

FilterModule::FilterModule()
{
}

float FilterModule::mapFrequency(float normalized, float minFreq, float maxFreq)
{
    // Exponential mapping: freq = min * (max/min)^normalized
    // Perceptually uniform — equal knob travel = equal perceived change
    return minFreq * std::pow(maxFreq / minFreq, normalized);
}

void FilterModule::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    // Prepare filters
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1; // We process L/R separately

    highpassL.prepare(spec);
    highpassR.prepare(spec);
    lowpassL.prepare(spec);
    lowpassR.prepare(spec);

    // Set filter types
    highpassL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    highpassR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lowpassL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    lowpassR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    // Resonance at 0.6 — subtle warmth, softer than Butterworth (0.707)
    // Gives a gentle bump near cutoff for vintage character
    const float resonance = 0.6f;
    highpassL.setResonance(resonance);
    highpassR.setResonance(resonance);
    lowpassL.setResonance(resonance);
    lowpassR.setResonance(resonance);

    // Initialize smoothed values with multiplicative smoothing
    // 20ms ramp time — fast enough for responsiveness, slow enough for no clicks
    smoothedHPFreq.reset(sampleRate, 0.02);
    smoothedLPFreq.reset(sampleRate, 0.02);

    // Start at neutral position (flat response)
    smoothedHPFreq.setCurrentAndTargetValue(20.0f);
    smoothedLPFreq.setCurrentAndTargetValue(20000.0f);

    reset();
}

void FilterModule::reset()
{
    highpassL.reset();
    highpassR.reset();
    lowpassL.reset();
    lowpassR.reset();
}

void FilterModule::process(juce::AudioBuffer<float>& buffer, float amount, float modOffset)
{
    auto numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Apply modulation offset from FluxModule
    // Clamp to valid range after applying
    float modulatedAmount = std::clamp(amount + modOffset * 0.1f, 0.0f, 1.0f);

    // ---- Frequency Mapping ----
    // Amount 0.0 → 0.5: Highpass sweeps from 500 Hz down to 20 Hz
    // Amount 0.5 → 1.0: Lowpass sweeps from 20 kHz down to 800 Hz
    //
    // At 0.5 both filters are at their neutral positions (flat response)

    float hpFreq, lpFreq;

    if (modulatedAmount < 0.5f)
    {
        // Highpass active: map [0.0, 0.5] → HP [500 Hz, 20 Hz]
        // Invert so 0.0 = max HP, 0.5 = min HP
        float hpNormalized = 1.0f - (modulatedAmount * 2.0f); // 1.0 at amount=0, 0.0 at amount=0.5
        hpFreq = mapFrequency(hpNormalized, 20.0f, 500.0f);
        lpFreq = 20000.0f; // Lowpass fully open
    }
    else
    {
        // Lowpass active: map [0.5, 1.0] → LP [20000 Hz, 800 Hz]
        // 0.5 = fully open, 1.0 = most closed
        float lpNormalized = (modulatedAmount - 0.5f) * 2.0f; // 0.0 at amount=0.5, 1.0 at amount=1.0
        lpFreq = mapFrequency(1.0f - lpNormalized, 800.0f, 20000.0f); // Invert: higher normalized = lower freq
        hpFreq = 20.0f; // Highpass fully open
    }

    // Clamp frequencies to safe range
    hpFreq = std::clamp(hpFreq, 20.0f, 2000.0f);
    lpFreq = std::clamp(lpFreq, 200.0f, 20000.0f);

    // Set smoothed targets
    smoothedHPFreq.setTargetValue(hpFreq);
    smoothedLPFreq.setTargetValue(lpFreq);

    // Process sample-by-sample with smoothed frequency updates
    for (int i = 0; i < numSamples; ++i)
    {
        float currentHP = smoothedHPFreq.getNextValue();
        float currentLP = smoothedLPFreq.getNextValue();

        // Update filter cutoff frequencies
        highpassL.setCutoffFrequency(currentHP);
        highpassR.setCutoffFrequency(currentHP);
        lowpassL.setCutoffFrequency(currentLP);
        lowpassR.setCutoffFrequency(currentLP);

        // Process left channel: HP → LP
        float sampleL = leftChannel[i];
        sampleL = highpassL.processSample(0, sampleL);
        sampleL = lowpassL.processSample(0, sampleL);
        leftChannel[i] = sampleL;

        // Process right channel: HP → LP
        float sampleR = rightChannel[i];
        sampleR = highpassR.processSample(0, sampleR);
        sampleR = lowpassR.processSample(0, sampleR);
        rightChannel[i] = sampleR;
    }
}
