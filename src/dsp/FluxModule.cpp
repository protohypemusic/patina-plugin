#include "FluxModule.h"
#include <cmath>
#include <algorithm>

// ============================================================
// SmoothRandom — Catmull-Rom cubic interpolated random
// ============================================================

void FluxModule::SmoothRandom::prepare(double sampleRate, float frequencyHz, uint32_t seed)
{
    rngState = seed;
    samplesPerSegment = std::max(1, static_cast<int>(sampleRate / static_cast<double>(frequencyHz)));
    sampleCounter = 0;

    // Initialize 4 history values for Catmull-Rom
    for (int i = 0; i < 4; ++i)
        values[static_cast<size_t>(i)] = nextRandom() * 2.0f - 1.0f;
}

void FluxModule::SmoothRandom::reset()
{
    sampleCounter = 0;
}

float FluxModule::SmoothRandom::getNextValue()
{
    // Fractional position within current segment
    float t = static_cast<float>(sampleCounter) / static_cast<float>(samplesPerSegment);

    // Catmull-Rom cubic interpolation (C1 continuous)
    float t2 = t * t;
    float t3 = t2 * t;

    float result = 0.5f * (
        (2.0f * values[1]) +
        (-values[0] + values[2]) * t +
        (2.0f * values[0] - 5.0f * values[1] + 4.0f * values[2] - values[3]) * t2 +
        (-values[0] + 3.0f * values[1] - 3.0f * values[2] + values[3]) * t3
    );

    // Advance counter
    if (++sampleCounter >= samplesPerSegment)
    {
        sampleCounter = 0;

        // Shift history and generate new target
        values[0] = values[1];
        values[1] = values[2];
        values[2] = values[3];
        values[3] = nextRandom() * 2.0f - 1.0f;
    }

    return result;
}

float FluxModule::SmoothRandom::nextRandom()
{
    // xorshift32 — each SmoothRandom has its own seed for decorrelation
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(rngState) / static_cast<float>(UINT32_MAX);
}

// ============================================================
// FluxModule — Beat Repeat / Glitch
// ============================================================

FluxModule::FluxModule()
{
}

void FluxModule::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    // 3 modulation sources at different rates for cross-module drift
    modSources[0].prepare(sampleRate, 0.1f,  0xA1B2C3D4);  // Very slow — filter drift
    modSources[1].prepare(sampleRate, 0.5f,  0xE5F6A7B8);  // Slow — wobble drift
    modSources[2].prepare(sampleRate, 2.0f,  0x12345678);   // Medium — noise flutter

    // Allocate 2 seconds of stereo stutter buffer (L/R interleaved)
    stutterBufFrames = static_cast<int>(sampleRate * 2.0);
    stutterBuffer.assign(static_cast<size_t>(stutterBufFrames) * 2, 0.0f);

    // 2ms fade for grain edges
    fadeSamples = static_cast<int>(sampleRate * 0.002);

    reset();
}

void FluxModule::reset()
{
    for (auto& src : modSources)
        src.reset();

    filterMod = 0.0f;
    wobbleMod = 0.0f;
    noiseMod = 0.0f;

    // Reset stutter state
    if (!stutterBuffer.empty())
        std::fill(stutterBuffer.begin(), stutterBuffer.end(), 0.0f);

    stutterWriteFrame = 0;
    stutterBufFull    = false;
    stutterReadFrame  = 0;
    stutterGrainStart = 0;
    stutterLength     = 0;
    stutterCountdown  = 0;
    isStuttering      = false;
}

void FluxModule::update(float amount)
{
    if (amount < 0.001f)
    {
        filterMod = 0.0f;
        wobbleMod = 0.0f;
        noiseMod = 0.0f;
        return;
    }

    // Quadratic depth for better control at low settings
    float depth = amount * amount;

    // Advance all modulation sources
    float raw[3];
    for (int i = 0; i < 3; ++i)
        raw[i] = modSources[i].getNextValue();

    // Map to modulation outputs with cross-blending for complexity
    filterMod = raw[0] * depth;
    wobbleMod = (0.7f * raw[1] + 0.3f * raw[2]) * depth;
    noiseMod = std::abs((0.5f * raw[2] + 0.5f * raw[1]) * depth * 0.5f);
}

void FluxModule::process(juce::AudioBuffer<float>& buffer, float amount)
{
    if (amount < 0.001f)
    {
        isStuttering = false;
        return;
    }

    auto numSamples    = buffer.getNumSamples();
    auto* leftChannel  = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Stutter active from 0.05 to 1.0 — full range beat repeat
    // Maps 0.05 → 0.0 intensity and 1.0 → 1.0 intensity
    const float stutterIntensity = (amount > 0.05f)
        ? juce::jlimit(0.0f, 1.0f, (amount - 0.05f) / 0.95f)
        : 0.0f;

    // Wet mix scales with intensity: 15% at low → 85% at max
    const float wetAmount = 0.15f + stutterIntensity * 0.70f;

    // Grain size: long at low intensity, short at high
    // Low (0.05): 200–500ms, High (1.0): 10–50ms
    const int minGrainMs = static_cast<int>(200.0f - 190.0f * stutterIntensity); // 200→10
    const int maxGrainMs = static_cast<int>(500.0f - 450.0f * stutterIntensity); // 500→50
    const int minGrainFrames = std::max(1, static_cast<int>(sampleRate * static_cast<double>(minGrainMs) / 1000.0));
    const int maxGrainFrames = std::max(minGrainFrames + 1, static_cast<int>(sampleRate * static_cast<double>(maxGrainMs) / 1000.0));

    // Trigger interval: ~2000ms at low → ~80ms at max
    const int triggerInterval = static_cast<int>(
        sampleRate * (2.0 - 1.92 * static_cast<double>(stutterIntensity))
    );
    const int safeTriggerInterval = std::max(1, triggerInterval);

    // Trigger probability: 0.6 at low → 1.0 at max
    const float triggerProb = 0.6f + 0.4f * stutterIntensity;

    for (int blockStart = 0; blockStart < numSamples; blockStart += MOD_BLOCK_SIZE)
    {
        const int blockEnd = std::min(blockStart + MOD_BLOCK_SIZE, numSamples);

        // Update modulation LFOs once per mod-block
        update(amount);

        for (int i = blockStart; i < blockEnd; ++i)
        {
            const float liveL = leftChannel[i];
            const float liveR = rightChannel[i];

            // ---- Always write live audio into circular stutter buffer ----
            if (!stutterBuffer.empty())
            {
                stutterBuffer[static_cast<size_t>(stutterWriteFrame) * 2]     = liveL;
                stutterBuffer[static_cast<size_t>(stutterWriteFrame) * 2 + 1] = liveR;

                if (++stutterWriteFrame >= stutterBufFrames)
                {
                    stutterWriteFrame = 0;
                    stutterBufFull    = true;
                }
            }

            float outL = liveL;
            float outR = liveR;

            // ---- Beat repeat stutter ----
            if (stutterIntensity > 0.001f && !stutterBuffer.empty())
            {
                // Tick down to the next trigger evaluation
                if (--stutterCountdown <= 0)
                {
                    stutterCountdown = safeTriggerInterval;

                    const float roll = static_cast<float>(stutterRng.nextInt(10000)) / 10000.0f;
                    const int writtenFrames = stutterBufFull ? stutterBufFrames : stutterWriteFrame;

                    if (roll < triggerProb && writtenFrames > minGrainFrames)
                    {
                        // Choose grain length and start position from recent audio
                        const int grainLen = minGrainFrames
                            + stutterRng.nextInt(std::max(1, maxGrainFrames - minGrainFrames));

                        // Pick from recent buffer (last 0.5 sec for freshness)
                        const int recentFrames = std::min(writtenFrames, static_cast<int>(sampleRate * 0.5));
                        const int maxStart = recentFrames - grainLen;

                        if (maxStart > 0)
                        {
                            // Offset from current write position
                            int start = stutterWriteFrame - recentFrames + stutterRng.nextInt(maxStart);
                            if (start < 0) start += stutterBufFrames;

                            stutterGrainStart = start;
                            stutterLength     = grainLen;
                            stutterReadFrame  = stutterGrainStart;
                            isStuttering      = true;
                        }
                    }
                    else
                    {
                        isStuttering = false;
                    }
                }

                // Output from the looping grain with fade-in/out
                if (isStuttering)
                {
                    const float grainL = stutterBuffer[static_cast<size_t>(stutterReadFrame) * 2];
                    const float grainR = stutterBuffer[static_cast<size_t>(stutterReadFrame) * 2 + 1];

                    // Position within grain for fade envelope
                    int posInGrain = stutterReadFrame - stutterGrainStart;
                    if (posInGrain < 0) posInGrain += stutterBufFrames;

                    // 2ms fade-in at start, 2ms fade-out at end
                    float grainEnv = 1.0f;
                    if (fadeSamples > 0)
                    {
                        if (posInGrain < fadeSamples)
                            grainEnv = static_cast<float>(posInGrain) / static_cast<float>(fadeSamples);
                        else if (posInGrain > stutterLength - fadeSamples)
                            grainEnv = static_cast<float>(stutterLength - posInGrain) / static_cast<float>(fadeSamples);
                    }

                    // Advance read pointer, looping within the grain
                    if (++stutterReadFrame >= stutterGrainStart + stutterLength)
                    {
                        // Wrap within circular buffer
                        if (stutterReadFrame >= stutterBufFrames)
                            stutterReadFrame -= stutterBufFrames;
                        else
                            stutterReadFrame = stutterGrainStart;
                    }

                    // Blend: live signal at (1 - wet), frozen grain at wet
                    outL = outL * (1.0f - wetAmount) + grainL * grainEnv * wetAmount;
                    outR = outR * (1.0f - wetAmount) + grainR * grainEnv * wetAmount;
                }
            }
            else
            {
                isStuttering = false;
            }

            leftChannel[i]  = outL;
            rightChannel[i] = outR;
        }
    }
}
