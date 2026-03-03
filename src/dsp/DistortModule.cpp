#include "DistortModule.h"
#include <cmath>
#include <algorithm>

DistortModule::DistortModule() {}

void DistortModule::prepare(double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    numChannels = 2;

    // 2x oversampling (IIR halfband)
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels,
        1, // 2^1 = 2x
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true
    );
    oversampling->initProcessing(size_t(samplesPerBlock));

    // Pre-filter (gentle HF taming before distortion)
    juce::dsp::ProcessSpec spec;
    spec.sampleRate   = sampleRate;
    spec.maximumBlockSize = uint32_t(samplesPerBlock);
    spec.numChannels  = 1;

    preFilterL.prepare(spec);
    preFilterR.prepare(spec);
    preFilterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    preFilterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    preFilterL.setResonance(0.5f);
    preFilterR.setResonance(0.5f);

    reset();
}

void DistortModule::reset()
{
    if (oversampling) oversampling->reset();
    preFilterL.reset();
    preFilterR.reset();
    toneLpZ.fill(0.0f);
    toneHpIn.fill(0.0f);
    toneHpZ.fill(0.0f);
    dcX.fill(0.0f);
    dcY.fill(0.0f);
    crushHold.fill(0.0f);
    crushPhase.fill(0.0f);
}

// ── Waveshaping algorithms ───────────────────────────────────────────
//
// Each type is designed to sound DISTINCTLY different.
// NO gain compensation: all types clip to roughly [-1, 1] naturally.
// Higher drive = louder + more saturated (as expected from distortion).
// The waveshaper ceilings create natural loudness at max.

float DistortModule::softClip(float x, float drive)
{
    // Tanh saturation: warm, musical, gentle compression.
    // Even at high drive it stays smooth and rounded.
    // Output bounded to [-1, 1] by tanh. No gain compensation needed.
    float d = x * drive;
    return std::tanh(d);
}

float DistortModule::hardClip(float x, float drive)
{
    // Flat clamp: buzzy, aggressive, loud.
    // Square-wave-like at high drive. Much brighter than soft.
    // Output bounded to [-1, 1] by clamp. No gain compensation needed.
    float d = x * drive;
    return std::clamp(d, -1.0f, 1.0f);
}

float DistortModule::diodeClip(float x, float drive)
{
    // Asymmetric rectifier: super heavy, massive even harmonics.
    // Positive half saturates hard, negative half is nearly blocked.
    // Like a real diode half-wave rectifier with leakage.
    // Output bounded to roughly [-0.2, 1.0]. No gain compensation - this should SMASH.
    float d = x * drive;

    if (d >= 0.0f)
    {
        // Forward bias: hard exponential saturation (clips to ~1.0)
        return 1.0f - std::exp(-d * 1.5f);
    }
    else
    {
        // Reverse bias: heavily attenuated (nearly rectified)
        // Only lets through ~20% of negative signal
        return -0.2f * (1.0f - std::exp(d * 0.8f));
    }
}

float DistortModule::waveFold(float x, float drive)
{
    // Triangle wave folding: harmonically rich, synth-like.
    // Folds signal back when it exceeds bounds. Gets wilder with drive.
    // Output bounded to [-1, 1] by asin(sin()). No gain compensation needed.
    float d = x * drive;
    return std::asin(std::sin(d * 1.5707963f)) * 0.6366198f; // (2/pi)*asin(sin(x*pi/2))
}

float DistortModule::bitCrush(float x, float amount, int ch)
{
    // Sample-rate + bit reduction: lo-fi digital.
    // amount: 0-1 from the UI knob directly.

    // Bit reduction: 16 bits (amount=0) down to ~2 bits (amount=1)
    float bits = 16.0f - amount * 14.0f;
    bits = std::max(bits, 2.0f);
    float levels = std::pow(2.0f, bits);
    float quantized = std::round(x * levels) / levels;

    // Sample rate reduction: hold samples for longer at higher amount
    // Range: 1 to 41 oversampled samples (~1 to ~20 at original rate)
    float holdLen = 1.0f + amount * amount * 40.0f;

    // Per-channel phase tracking (fixes stereo offset bug)
    crushPhase[ch] += 1.0f;
    if (crushPhase[ch] >= holdLen)
    {
        crushPhase[ch] -= holdLen;
        crushHold[ch] = quantized;
    }

    return crushHold[ch];
}

// ── Post-distortion tone ─────────────────────────────────────────────

float DistortModule::applyTone(int ch, float in, float tone)
{
    if (tone < 0.48f)
    {
        float t = tone / 0.48f;
        float cutoff = 400.0f * std::pow(50.0f, t); // 400 Hz - 20 kHz
        float rc = 1.0f / (2.0f * 3.14159265f * cutoff);
        float dt = 1.0f / float(sr);
        float alpha = dt / (rc + dt);
        toneLpZ[ch] += alpha * (in - toneLpZ[ch]);
        return toneLpZ[ch];
    }
    else if (tone > 0.52f)
    {
        float t = (tone - 0.52f) / 0.48f;
        float cutoff = 40.0f * std::pow(50.0f, t); // 40 Hz - 2 kHz
        float rc = 1.0f / (2.0f * 3.14159265f * cutoff);
        float dt = 1.0f / float(sr);
        float alpha = rc / (rc + dt);
        float out = alpha * (toneHpZ[ch] + in - toneHpIn[ch]);
        toneHpIn[ch] = in;
        toneHpZ[ch] = out;
        return out;
    }

    return in; // Flat zone
}

float DistortModule::tickDC(int ch, float in)
{
    float out = in - dcX[ch] + 0.9995f * dcY[ch];
    dcX[ch] = in;
    dcY[ch] = out;
    return out;
}

// ── Main process ─────────────────────────────────────────────────────

void DistortModule::process(juce::AudioBuffer<float>& buffer,
                            float amount, float tone, int type)
{
    if (amount < 0.001f) return;

    auto numSamples = buffer.getNumSamples();

    // Drive: quadratic ramp 1x to 61x (much more range than before)
    float drive = 1.0f + amount * amount * 60.0f;

    // Pre-filter cutoff: 18 kHz at low drive, 10 kHz at max drive
    // Much gentler than before (was 12k->4k) so character frequencies survive
    float preFilterCutoff = 18000.0f - 8000.0f * amount;
    preFilterL.setCutoffFrequency(preFilterCutoff);
    preFilterR.setCutoffFrequency(preFilterCutoff);

    // Pre-filter pass
    auto* left  = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        left[i]  = preFilterL.processSample(0, left[i]);
        right[i] = preFilterR.processSample(0, right[i]);
    }

    // Upsample
    juce::dsp::AudioBlock<float> block(buffer);
    auto osBlock = oversampling->processSamplesUp(block);

    auto osNumSamples = int(osBlock.getNumSamples());
    auto* osL = osBlock.getChannelPointer(0);
    auto* osR = osBlock.getChannelPointer(1);

    // Waveshape at 2x sample rate
    for (int i = 0; i < osNumSamples; ++i)
    {
        switch (type)
        {
            case 0:
                osL[i] = softClip(osL[i], drive);
                osR[i] = softClip(osR[i], drive);
                break;
            case 1:
                osL[i] = hardClip(osL[i], drive);
                osR[i] = hardClip(osR[i], drive);
                break;
            case 2:
                osL[i] = diodeClip(osL[i], drive);
                osR[i] = diodeClip(osR[i], drive);
                break;
            case 3:
                osL[i] = waveFold(osL[i], drive);
                osR[i] = waveFold(osR[i], drive);
                break;
            case 4:
                osL[i] = bitCrush(osL[i], amount, 0);
                osR[i] = bitCrush(osR[i], amount, 1);
                break;
        }
    }

    // Downsample
    oversampling->processSamplesDown(block);

    // Output gain compensation: reduce level proportionally to drive
    // so distortion adds character without massive volume jumps.
    // Allows ~3-4 dB of natural loudness increase at max, not 15-20 dB.
    float compensation = 1.0f / std::pow(drive, 0.4f);

    // Post-distortion tone filter + DC block
    left  = buffer.getWritePointer(0);
    right = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        left[i]  *= compensation;
        right[i] *= compensation;
        left[i]  = applyTone(0, left[i],  tone);
        right[i] = applyTone(1, right[i], tone);
        left[i]  = tickDC(0, left[i]);
        right[i] = tickDC(1, right[i]);
    }
}
