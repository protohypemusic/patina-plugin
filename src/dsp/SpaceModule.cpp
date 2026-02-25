#include "SpaceModule.h"
#include <cmath>
#include <algorithm>

// ============================================================
// AllpassFilter
// ============================================================

void SpaceModule::AllpassFilter::setDelay(int delaySamples)
{
    delayLength = std::max(1, delaySamples);
    buffer.resize(static_cast<size_t>(delayLength), 0.0f);
    writeIndex = 0;
}

void SpaceModule::AllpassFilter::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

float SpaceModule::AllpassFilter::process(float input, float gain)
{
    float delayed = buffer[static_cast<size_t>(writeIndex)];
    float output = -gain * input + delayed;
    buffer[static_cast<size_t>(writeIndex)] = input + gain * delayed;

    if (++writeIndex >= delayLength)
        writeIndex = 0;

    return output;
}

float SpaceModule::AllpassFilter::readAt(int offset) const
{
    int idx = writeIndex - offset;
    if (idx < 0) idx += delayLength;
    return buffer[static_cast<size_t>(idx)];
}

// ============================================================
// SimpleDelay
// ============================================================

void SpaceModule::SimpleDelay::setMaxDelay(int maxSamples)
{
    buffer.resize(static_cast<size_t>(std::max(1, maxSamples)), 0.0f);
    delayLength = maxSamples;
    writeIndex = 0;
}

void SpaceModule::SimpleDelay::setDelay(int delaySamples)
{
    delayLength = std::max(1, std::min(delaySamples, static_cast<int>(buffer.size())));
}

void SpaceModule::SimpleDelay::reset()
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
}

void SpaceModule::SimpleDelay::write(float sample)
{
    buffer[static_cast<size_t>(writeIndex)] = sample;
    if (++writeIndex >= static_cast<int>(buffer.size()))
        writeIndex = 0;
}

float SpaceModule::SimpleDelay::read() const
{
    int readIdx = writeIndex - delayLength;
    if (readIdx < 0) readIdx += static_cast<int>(buffer.size());
    return buffer[static_cast<size_t>(readIdx)];
}

float SpaceModule::SimpleDelay::readAt(int offset) const
{
    int idx = writeIndex - offset;
    while (idx < 0) idx += static_cast<int>(buffer.size());
    return buffer[static_cast<size_t>(idx % static_cast<int>(buffer.size()))];
}

// ============================================================
// SpaceModule
// ============================================================

SpaceModule::SpaceModule()
{
}

void SpaceModule::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    // Scale delay lengths from Dattorro's original (designed at 29761 Hz)
    float scale = static_cast<float>(sampleRate) / 29761.0f;

    // Input diffusers (delay lengths from Dattorro's paper)
    inputDiffusers[0].setDelay(static_cast<int>(142.0f * scale));
    inputDiffusers[1].setDelay(static_cast<int>(107.0f * scale));
    inputDiffusers[2].setDelay(static_cast<int>(379.0f * scale));
    inputDiffusers[3].setDelay(static_cast<int>(277.0f * scale));

    // Tank delays (Loop 1)
    tankDelay1a.setMaxDelay(static_cast<int>(672.0f * scale) + 1);
    tankDelay1a.setDelay(static_cast<int>(672.0f * scale));
    tankAllpass1.setDelay(static_cast<int>(908.0f * scale));
    tankDelay1b.setMaxDelay(static_cast<int>(4453.0f * scale) + 1);
    tankDelay1b.setDelay(static_cast<int>(4453.0f * scale));

    // Tank delays (Loop 2)
    tankDelay2a.setMaxDelay(static_cast<int>(1800.0f * scale) + 1);
    tankDelay2a.setDelay(static_cast<int>(1800.0f * scale));
    tankAllpass2.setDelay(static_cast<int>(672.0f * scale));
    tankDelay2b.setMaxDelay(static_cast<int>(3720.0f * scale) + 1);
    tankDelay2b.setDelay(static_cast<int>(3720.0f * scale));

    // Pre-delay: fixed ~15ms
    int preDelayMax = static_cast<int>(0.025f * static_cast<float>(sampleRate));
    preDelaySamples = static_cast<int>(0.015f * static_cast<float>(sampleRate));
    preDelay.setMaxDelay(preDelayMax + 1);
    preDelay.setDelay(preDelaySamples);

    // Output taps for stereo decorrelation (scaled from Dattorro)
    taps.leftTaps[0] = static_cast<int>(266.0f * scale);
    taps.leftTaps[1] = static_cast<int>(2974.0f * scale);
    taps.leftTaps[2] = static_cast<int>(1913.0f * scale);
    taps.leftTaps[3] = static_cast<int>(1996.0f * scale);
    taps.leftTaps[4] = static_cast<int>(1990.0f * scale);
    taps.leftTaps[5] = static_cast<int>(187.0f * scale);
    taps.leftTaps[6] = static_cast<int>(1066.0f * scale);

    taps.rightTaps[0] = static_cast<int>(353.0f * scale);
    taps.rightTaps[1] = static_cast<int>(3627.0f * scale);
    taps.rightTaps[2] = static_cast<int>(1228.0f * scale);
    taps.rightTaps[3] = static_cast<int>(2673.0f * scale);
    taps.rightTaps[4] = static_cast<int>(2111.0f * scale);
    taps.rightTaps[5] = static_cast<int>(335.0f * scale);
    taps.rightTaps[6] = static_cast<int>(121.0f * scale);

    // Smoothed parameters
    smoothedDecay.reset(sampleRate, 0.05); // 50ms ramp
    smoothedDamping.reset(sampleRate, 0.05);
    smoothedDecay.setCurrentAndTargetValue(0.5f);
    smoothedDamping.setCurrentAndTargetValue(0.5f);

    reset();
}

void SpaceModule::reset()
{
    for (auto& d : inputDiffusers)
        d.reset();

    tankAllpass1.reset();
    tankAllpass2.reset();
    tankDelay1a.reset();
    tankDelay1b.reset();
    tankDelay2a.reset();
    tankDelay2b.reset();
    preDelay.reset();

    damping1_z1 = 0.0f;
    damping2_z1 = 0.0f;
    tank1bOutput = 0.0f;
    tank2bOutput = 0.0f;
}

void SpaceModule::process(juce::AudioBuffer<float>& buffer, float amount, float decayParam)
{
    if (amount < 0.001f)
        return;

    auto numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Decay parameter controls reverb tail independently from wet/dry
    // decayParam 0.0 → tight room (0.2), 1.0 → long ambient wash (0.95)
    float targetDecay = 0.2f + 0.75f * decayParam;
    // Damping tied to decay: brighter at low decay, darker at high
    float targetDamping = 0.3f + 0.4f * decayParam;
    smoothedDecay.setTargetValue(targetDecay);
    smoothedDamping.setTargetValue(targetDamping);

    // Wet/dry balance — wet + dry always = 1.0 to prevent clipping.
    // Previous formula (wet=amount, dry=1-amount*0.5) could sum to 1.40 at amount=0.80.
    // New formula: wet caps at 0.70 (full reverb wash), dry fills the rest.
    float wet = amount * 0.7f;
    float dry = 1.0f - wet;

    for (int i = 0; i < numSamples; ++i)
    {
        float currentDecay = smoothedDecay.getNextValue();
        float currentDamping = smoothedDamping.getNextValue();

        // Sum to mono for reverb input
        float monoInput = (leftChannel[i] + rightChannel[i]) * 0.5f;

        // Pre-delay
        preDelay.write(monoInput);
        float preDelayed = preDelay.read();

        // Input diffusion: 4 cascaded allpass filters
        float diffused = preDelayed;
        diffused = inputDiffusers[0].process(diffused, 0.75f);
        diffused = inputDiffusers[1].process(diffused, 0.75f);
        diffused = inputDiffusers[2].process(diffused, 0.625f);
        diffused = inputDiffusers[3].process(diffused, 0.625f);

        // ---- Tank Loop 1 ----
        float tankIn1 = diffused + tank2bOutput * currentDecay;

        // Allpass in loop 1
        float ap1Out = tankAllpass1.process(tankIn1, -0.7f);

        // Delay 1a
        tankDelay1a.write(ap1Out);
        float d1aOut = tankDelay1a.read();

        // Damping lowpass (one-pole)
        damping1_z1 = d1aOut * (1.0f - currentDamping) + damping1_z1 * currentDamping;
        float damped1 = damping1_z1;

        // Delay 1b
        tankDelay1b.write(damped1 * currentDecay);
        tank1bOutput = tankDelay1b.read();

        // ---- Tank Loop 2 ----
        float tankIn2 = diffused + tank1bOutput * currentDecay;

        // Allpass in loop 2
        float ap2Out = tankAllpass2.process(tankIn2, -0.7f);

        // Delay 2a
        tankDelay2a.write(ap2Out);
        float d2aOut = tankDelay2a.read();

        // Damping lowpass (one-pole)
        damping2_z1 = d2aOut * (1.0f - currentDamping) + damping2_z1 * currentDamping;
        float damped2 = damping2_z1;

        // Delay 2b
        tankDelay2b.write(damped2 * currentDecay);
        tank2bOutput = tankDelay2b.read();

        // ---- Multi-tap stereo output ----
        // Simplified tap reading from tank delays
        float reverbL = 0.0f;
        float reverbR = 0.0f;

        // Tap from various points in the tank for decorrelated stereo
        reverbL += tankDelay1a.readAt(std::min(taps.leftTaps[0], static_cast<int>(672.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbL += tankDelay1b.readAt(std::min(taps.leftTaps[1], static_cast<int>(4453.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbL -= tankAllpass2.readAt(std::min(taps.leftTaps[2], static_cast<int>(672.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbL += tankDelay2a.readAt(std::min(taps.leftTaps[3], static_cast<int>(1800.0f * static_cast<float>(sampleRate) / 29761.0f)));

        reverbR += tankDelay2a.readAt(std::min(taps.rightTaps[0], static_cast<int>(1800.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbR += tankDelay2b.readAt(std::min(taps.rightTaps[1], static_cast<int>(3720.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbR -= tankAllpass1.readAt(std::min(taps.rightTaps[2], static_cast<int>(908.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbR += tankDelay1a.readAt(std::min(taps.rightTaps[3], static_cast<int>(672.0f * static_cast<float>(sampleRate) / 29761.0f)));

        reverbL *= 0.25f; // Normalize tap sum
        reverbR *= 0.25f;

        // Mix dry + wet
        leftChannel[i] = leftChannel[i] * dry + reverbL * wet;
        rightChannel[i] = rightChannel[i] * dry + reverbR * wet;
    }
}
