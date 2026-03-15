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

float SpaceModule::SimpleDelay::readFractional(float offset) const
{
    int bufSize = static_cast<int>(buffer.size());
    float readPos = static_cast<float>(writeIndex) - offset;
    while (readPos < 0.0f) readPos += static_cast<float>(bufSize);

    int idx0 = static_cast<int>(readPos) % bufSize;
    int idx1 = (idx0 + 1) % bufSize;
    float frac = readPos - std::floor(readPos);

    return buffer[static_cast<size_t>(idx0)] * (1.0f - frac)
         + buffer[static_cast<size_t>(idx1)] * frac;
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

    // Extra buffer space for modulated delay reads (±modExcursion + safety)
    int modExtra = static_cast<int>(16.0f * scale) + 8;

    // Tank delays (Loop 1)
    tankDelay1a.setMaxDelay(static_cast<int>(672.0f * scale) + modExtra);
    tankDelay1a.setDelay(static_cast<int>(672.0f * scale));
    tankAllpass1.setDelay(static_cast<int>(908.0f * scale));
    tankDelay1b.setMaxDelay(static_cast<int>(4453.0f * scale) + 1);
    tankDelay1b.setDelay(static_cast<int>(4453.0f * scale));

    // Tank delays (Loop 2)
    tankDelay2a.setMaxDelay(static_cast<int>(1800.0f * scale) + modExtra);
    tankDelay2a.setDelay(static_cast<int>(1800.0f * scale));
    tankAllpass2.setDelay(static_cast<int>(672.0f * scale));
    tankDelay2b.setMaxDelay(static_cast<int>(3720.0f * scale) + 1);
    tankDelay2b.setDelay(static_cast<int>(3720.0f * scale));

    // Pre-delay: max ~40ms (type-dependent actual value set in process)
    int preDelayMax = static_cast<int>(0.040f * static_cast<float>(sampleRate));
    preDelaySamples = static_cast<int>(0.015f * static_cast<float>(sampleRate));
    preDelay.setMaxDelay(preDelayMax + 1);
    preDelay.setDelay(preDelaySamples);

    // Tonal delay: max ~35ms (covers 20-30ms range with headroom)
    int tonalMax = static_cast<int>(0.035f * static_cast<float>(sampleRate));
    tonalDelayL.setMaxDelay(tonalMax + 1);
    tonalDelayR.setMaxDelay(tonalMax + 1);
    tonalDelayL.setDelay(static_cast<int>(0.025f * static_cast<float>(sampleRate)));
    tonalDelayR.setDelay(static_cast<int>(0.027f * static_cast<float>(sampleRate)));  // Slight offset for stereo

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

    // Tank modulation LFOs — different rates to avoid phase-locking
    // These slowly vary the delay read positions, smearing comb filter peaks
    // that cause the "tonal" coloration in fixed-delay reverbs
    lfoRate1 = 0.5f / static_cast<float>(sampleRate);    // 0.50 Hz
    lfoRate2 = 0.31f / static_cast<float>(sampleRate);   // 0.31 Hz
    lfoPhase1 = 0.0f;
    lfoPhase2 = 0.0f;
    modExcursion = 16.0f * scale;  // ±16 samples at reference rate, scaled

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
    lfoPhase1 = 0.0f;
    lfoPhase2 = 0.0f;

    tonalDelayL.reset();
    tonalDelayR.reset();
    tonalFeedbackL = 0.0f;
    tonalFeedbackR = 0.0f;
    tonalLpfL_z1 = 0.0f;
    tonalLpfR_z1 = 0.0f;
}

// Soft-clip to prevent energy blowup in feedback loops
static inline float softClip(float x)
{
    // Fast tanh approximation: keeps signal in (-1, 1)
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

void SpaceModule::process(juce::AudioBuffer<float>& buffer, float amount, float decayParam,
                          int type)
{
    if (amount < 0.001f)
        return;

    // Type 3 = TONAL: separate processing path
    if (type == 3)
    {
        processTonal(buffer, amount, decayParam);
        return;
    }

    auto numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Type-dependent parameter scaling
    float decayScale = 1.0f;
    float dampingBias = 0.0f;
    float diffGain1 = 0.60f;
    float diffGain2 = 0.50f;
    float modScale = 1.0f;

    switch (type)
    {
        case 1: // ROOM: tighter, more damped, less modulation
            preDelay.setDelay(static_cast<int>(0.005f * static_cast<float>(sampleRate)));
            decayScale = 0.80f;
            dampingBias = 0.15f;
            diffGain1 = 0.70f;
            diffGain2 = 0.60f;
            modScale = 0.5f;
            break;
        case 2: // HALL: spacious, less damped, more modulation
            preDelay.setDelay(static_cast<int>(0.030f * static_cast<float>(sampleRate)));
            decayScale = 1.10f;
            dampingBias = -0.12f;
            diffGain1 = 0.55f;
            diffGain2 = 0.45f;
            modScale = 1.5f;
            break;
        default: // PLATE: original tuning
            preDelay.setDelay(static_cast<int>(0.015f * static_cast<float>(sampleRate)));
            break;
    }

    // Decay parameter controls reverb tail independently from wet/dry
    float targetDecay = (0.2f + 0.70f * decayParam) * decayScale;
    targetDecay = std::min(targetDecay, 0.92f); // Safety cap
    float targetDamping = std::max(0.0f, std::min(1.0f, 0.45f + 0.35f * decayParam + dampingBias));
    smoothedDecay.setTargetValue(targetDecay);
    smoothedDamping.setTargetValue(targetDamping);

    // Wet/dry balance
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
        diffused = inputDiffusers[0].process(diffused, diffGain1);
        diffused = inputDiffusers[1].process(diffused, diffGain1);
        diffused = inputDiffusers[2].process(diffused, diffGain2);
        diffused = inputDiffusers[3].process(diffused, diffGain2);

        // ---- Advance tank modulation LFOs ----
        lfoPhase1 += lfoRate1;
        if (lfoPhase1 >= 1.0f) lfoPhase1 -= 1.0f;
        lfoPhase2 += lfoRate2;
        if (lfoPhase2 >= 1.0f) lfoPhase2 -= 1.0f;

        float mod1 = std::sin(lfoPhase1 * 6.2831853f) * modExcursion * modScale;
        float mod2 = std::sin(lfoPhase2 * 6.2831853f) * modExcursion * modScale;

        // ---- Tank Loop 1 ----
        float tankIn1 = diffused + softClip(tank2bOutput) * currentDecay;
        float ap1Out = tankAllpass1.process(tankIn1, -0.5f);

        tankDelay1a.write(ap1Out);
        float d1aDelay = static_cast<float>(tankDelay1a.getDelayLength());
        float d1aOut = tankDelay1a.readFractional(std::max(1.0f, d1aDelay + mod1));

        damping1_z1 = d1aOut * (1.0f - currentDamping) + damping1_z1 * currentDamping;
        float damped1 = softClip(damping1_z1);

        tankDelay1b.write(damped1 * currentDecay);
        tank1bOutput = tankDelay1b.read();

        // ---- Tank Loop 2 ----
        float tankIn2 = diffused + softClip(tank1bOutput) * currentDecay;
        float ap2Out = tankAllpass2.process(tankIn2, -0.5f);

        tankDelay2a.write(ap2Out);
        float d2aDelay = static_cast<float>(tankDelay2a.getDelayLength());
        float d2aOut = tankDelay2a.readFractional(std::max(1.0f, d2aDelay + mod2));

        damping2_z1 = d2aOut * (1.0f - currentDamping) + damping2_z1 * currentDamping;
        float damped2 = softClip(damping2_z1);

        tankDelay2b.write(damped2 * currentDecay);
        tank2bOutput = tankDelay2b.read();

        // ---- Multi-tap stereo output ----
        float reverbL = 0.0f;
        float reverbR = 0.0f;

        reverbL += tankDelay1a.readAt(std::min(taps.leftTaps[0], static_cast<int>(672.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbL += tankDelay1b.readAt(std::min(taps.leftTaps[1], static_cast<int>(4453.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbL -= tankAllpass2.readAt(std::min(taps.leftTaps[2], static_cast<int>(672.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbL += tankDelay2a.readAt(std::min(taps.leftTaps[3], static_cast<int>(1800.0f * static_cast<float>(sampleRate) / 29761.0f)));

        reverbR += tankDelay2a.readAt(std::min(taps.rightTaps[0], static_cast<int>(1800.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbR += tankDelay2b.readAt(std::min(taps.rightTaps[1], static_cast<int>(3720.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbR -= tankAllpass1.readAt(std::min(taps.rightTaps[2], static_cast<int>(908.0f * static_cast<float>(sampleRate) / 29761.0f)));
        reverbR += tankDelay1a.readAt(std::min(taps.rightTaps[3], static_cast<int>(672.0f * static_cast<float>(sampleRate) / 29761.0f)));

        reverbL *= 0.25f;
        reverbR *= 0.25f;

        leftChannel[i]  = juce::jlimit(-2.0f, 2.0f, leftChannel[i] * dry + reverbL * wet);
        rightChannel[i] = juce::jlimit(-2.0f, 2.0f, rightChannel[i] * dry + reverbR * wet);
    }
}

// ============================================================
// Tonal Delay (type 3) -- short comb-filter delay (20-30ms)
// ============================================================
void SpaceModule::processTonal(juce::AudioBuffer<float>& buffer, float amount, float decayParam)
{
    auto numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Delay time: 20ms (decay=0) to 30ms (decay=1)
    float delayMs = 20.0f + 10.0f * decayParam;
    float delaySamplesF = delayMs * 0.001f * static_cast<float>(sampleRate);
    int delaySamplesL = static_cast<int>(delaySamplesF);
    int delaySamplesR = static_cast<int>(delaySamplesF * 1.08f); // ~8% offset for stereo width

    tonalDelayL.setDelay(delaySamplesL);
    tonalDelayR.setDelay(delaySamplesR);

    // Feedback: moderate to high for tonal resonance
    // Higher decay = more feedback = more pronounced comb-filter tone
    float feedback = 0.3f + 0.45f * decayParam; // 0.3 to 0.75

    // Lowpass coefficient for warm feedback (cuts harsh highs)
    float lpfCoeff = 0.35f;

    // Wet/dry: amount controls blend
    float wet = amount * 0.65f;
    float dry = 1.0f - wet * 0.5f; // Keep dry signal strong

    for (int i = 0; i < numSamples; ++i)
    {
        // Read delayed signals
        float delayedL = tonalDelayL.read();
        float delayedR = tonalDelayR.read();

        // Lowpass filter on feedback for warmth
        tonalLpfL_z1 = delayedL * (1.0f - lpfCoeff) + tonalLpfL_z1 * lpfCoeff;
        tonalLpfR_z1 = delayedR * (1.0f - lpfCoeff) + tonalLpfR_z1 * lpfCoeff;

        // Write input + filtered feedback into delay
        tonalDelayL.write(leftChannel[i]  + tonalLpfL_z1 * feedback);
        tonalDelayR.write(rightChannel[i] + tonalLpfR_z1 * feedback);

        // Mix
        leftChannel[i]  = juce::jlimit(-2.0f, 2.0f, leftChannel[i] * dry + delayedL * wet);
        rightChannel[i] = juce::jlimit(-2.0f, 2.0f, rightChannel[i] * dry + delayedR * wet);
    }
}
