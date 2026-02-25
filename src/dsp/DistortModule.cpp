#include "DistortModule.h"
#include <cmath>

DistortModule::DistortModule()
{
}

void DistortModule::prepare(double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;
    numChannels = 2;

    // 2x oversampling (IIR halfband filter)
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels,
        1, // order = 1 means 2^1 = 2x oversampling
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true // isMaxQuality
    );
    oversampling->initProcessing(static_cast<size_t>(samplesPerBlock));

    // Pre-filter: lowpass that darkens with distortion amount
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = newSampleRate;
    spec.maximumBlockSize = static_cast<uint32_t>(samplesPerBlock);
    spec.numChannels = 1;

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
    if (oversampling)
        oversampling->reset();

    preFilterL.reset();
    preFilterR.reset();
    dcL = {};
    dcR = {};
}

void DistortModule::process(juce::AudioBuffer<float>& buffer, float amount)
{
    if (amount < 0.001f)
        return;

    auto numSamples = buffer.getNumSamples();

    // Pre-filter cutoff: 12 kHz at 0.0 -> 4 kHz at 1.0
    float preFilterCutoff = 12000.0f - 8000.0f * amount;
    preFilterL.setCutoffFrequency(preFilterCutoff);
    preFilterR.setCutoffFrequency(preFilterCutoff);

    // Drive: quadratic ramp from 1x to 31x
    // At 0.3: drive ~3.7 (warm)
    // At 0.7: drive ~15.7 (crunchy)
    // At 1.0: drive ~31 (hard clip)
    float drive = 1.0f + amount * amount * 30.0f;
    float asymmetry = 0.15f * amount; // Even harmonics increase with amount

    // Pre-filter
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        leftChannel[i] = preFilterL.processSample(0, leftChannel[i]);
        rightChannel[i] = preFilterR.processSample(0, rightChannel[i]);
    }

    // Upsample
    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampling->processSamplesUp(block);

    // Process at 2x sample rate
    auto oversampledNumSamples = static_cast<int>(oversampledBlock.getNumSamples());
    auto* osLeft = oversampledBlock.getChannelPointer(0);
    auto* osRight = oversampledBlock.getChannelPointer(1);

    for (int i = 0; i < oversampledNumSamples; ++i)
    {
        osLeft[i] = waveshape(osLeft[i], drive, asymmetry);
        osRight[i] = waveshape(osRight[i], drive, asymmetry);
    }

    // Downsample back
    oversampling->processSamplesDown(block);

    // DC block (removes DC from asymmetric waveshaping)
    leftChannel = buffer.getWritePointer(0);
    rightChannel = buffer.getWritePointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        leftChannel[i] = blockDC(dcL, leftChannel[i]);
        rightChannel[i] = blockDC(dcR, rightChannel[i]);
    }
}

// ============================================================
// Waveshaping
// ============================================================

float DistortModule::waveshape(float x, float drive, float asymmetry)
{
    float driven = x * drive;

    // Add asymmetry for even harmonic content (tube warmth)
    float biased = driven + asymmetry * driven * driven;

    // Soft clip — std::tanh is safe for all input magnitudes
    float clipped = std::tanh(biased);

    // Level compensation: normalize so output ~= 1.0 at max drive
    float compensation = std::tanh(drive);
    if (compensation > 0.001f)
        clipped /= compensation;

    return clipped;
}

// ============================================================
// DC Blocker
// ============================================================

float DistortModule::blockDC(DCBlocker& state, float input)
{
    float output = input - state.x1 + 0.9995f * state.y1;
    state.x1 = input;
    state.y1 = output;
    return output;
}
