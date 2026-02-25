#include "NoiseModule.h"
#include <cmath>

NoiseModule::NoiseModule()
{
}

void NoiseModule::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;

    // Vinyl crackle rates
    // Surface crackle: ~200-800 impulses/sec at full amount
    crackleRate = 400.0f / static_cast<float>(sampleRate);
    // Dust pops: ~2-8 per second
    popRate = 4.0f / static_cast<float>(sampleRate);

    // Envelope decays
    // Surface crackle: ~0.5ms decay
    float surfaceDecayMs = 0.5f;
    float surfaceDecaySamples = surfaceDecayMs * 0.001f * static_cast<float>(sampleRate);
    crackleL.surfaceDecay = std::exp(-1.0f / surfaceDecaySamples);
    crackleR.surfaceDecay = crackleL.surfaceDecay;

    // Dust pop: ~3ms decay
    float popDecayMs = 3.0f;
    float popDecaySamples = popDecayMs * 0.001f * static_cast<float>(sampleRate);
    crackleL.popDecay = std::exp(-1.0f / popDecaySamples);
    crackleR.popDecay = crackleL.popDecay;

    // Revolution modulator: ~0.55 Hz (33 RPM)
    revolutionRate = 0.55f / static_cast<float>(sampleRate);
    revolutionPhase = 0.0f;

    // Tape hiss high-shelf filter
    // Shelf at ~4kHz with ~6dB boost for tape character
    float shelfFreq = 4000.0f;
    float wc = 2.0f * juce::MathConstants<float>::pi * shelfFreq / static_cast<float>(sampleRate);
    float alpha = std::exp(-wc);
    hissL.shelfCoeff = alpha;
    hissR.shelfCoeff = alpha;
    hissL.shelfGain = 2.0f;  // ~6dB boost above shelf freq
    hissR.shelfGain = 2.0f;

    // Static modulation: slow amplitude modulation at ~0.3 Hz
    staticState.modRate = 0.3f / static_cast<float>(sampleRate);
    staticState.modPhase = 0.0f;

    reset();
}

void NoiseModule::reset()
{
    // Save computed coefficients before resetting structs
    float savedSurfaceDecay = crackleL.surfaceDecay;
    float savedPopDecay = crackleL.popDecay;
    float savedShelfCoeff = hissL.shelfCoeff;
    float savedShelfGain = hissL.shelfGain;

    // Reset runtime state
    crackleL = {};
    crackleR = {};
    hissL = {};
    hissR = {};
    dcL = {};
    dcR = {};

    // Restore computed coefficients
    crackleL.surfaceDecay = savedSurfaceDecay;
    crackleR.surfaceDecay = savedSurfaceDecay;
    crackleL.popDecay = savedPopDecay;
    crackleR.popDecay = savedPopDecay;
    hissL.shelfCoeff = savedShelfCoeff;
    hissR.shelfCoeff = savedShelfCoeff;
    hissL.shelfGain = savedShelfGain;
    hissR.shelfGain = savedShelfGain;
}

void NoiseModule::process(juce::AudioBuffer<float>& buffer, float amount)
{
    if (amount < 0.001f)
        return;

    auto numSamples = buffer.getNumSamples();
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    // Scale sub-components based on amount
    // Low amounts: mostly hiss. High amounts: crackle + static come in
    float hissLevel = amount * 0.025f;                    // Subtle hiss throughout
    float crackleLevel = amount * amount * 0.05f;         // Crackle ramps up quadratically
    float staticLevel = amount * amount * amount * 0.02f; // Static at high amounts only

    for (int i = 0; i < numSamples; ++i)
    {
        // Revolution modulator — modulates crackle density
        revolutionPhase += revolutionRate;
        if (revolutionPhase >= 1.0f)
            revolutionPhase -= 1.0f;

        float revolutionMod = 0.7f + 0.3f * std::sin(revolutionPhase * juce::MathConstants<float>::twoPi);

        // Generate noise components
        float crackL = generateCrackle(crackleL, revolutionMod) * crackleLevel;
        float crackR = generateCrackle(crackleR, revolutionMod) * crackleLevel;

        float hL = generateHiss(hissL) * hissLevel;
        float hR = generateHiss(hissR) * hissLevel;

        float stat = generateStatic() * staticLevel;

        // Combine noise components
        float noiseL = crackL + hL + stat;
        float noiseR = crackR + hR + stat * 0.8f; // Slightly decorrelate static

        // Soft-clip limiter — rounds off harsh crackle peaks
        // tanh(x*k)/k ≈ x for small signals, clamps to ±1/k for peaks
        constexpr float k = 4.0f;
        noiseL = std::tanh(noiseL * k) / k;
        noiseR = std::tanh(noiseR * k) / k;

        // DC block
        noiseL = blockDC(dcL, noiseL);
        noiseR = blockDC(dcR, noiseR);

        leftChannel[i] += noiseL;
        rightChannel[i] += noiseR;
    }
}

// ---- PRNG ----

float NoiseModule::nextRandom()
{
    // xorshift32
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return static_cast<float>(rngState) / static_cast<float>(UINT32_MAX);
}

float NoiseModule::nextRandomBipolar()
{
    return nextRandom() * 2.0f - 1.0f;
}

float NoiseModule::nextGaussian()
{
    // Fast Gaussian approximation: sum of 4 uniform randoms
    // Central limit theorem — produces approximately N(0, 1/sqrt(3))
    float sum = nextRandomBipolar() + nextRandomBipolar() +
                nextRandomBipolar() + nextRandomBipolar();
    return sum * 0.5f; // Scale for reasonable range
}

// ---- Vinyl Crackle ----

float NoiseModule::generateCrackle(CrackleState& state, float revolutionMod)
{
    // Surface crackle: Poisson-triggered impulses
    if (nextRandom() < crackleRate * revolutionMod)
    {
        // Trigger new surface crackle — random amplitude
        state.surfaceEnvelope = 0.3f + 0.7f * nextRandom();
    }

    // Dust pop: rare, louder impulses
    if (nextRandom() < popRate)
    {
        state.popEnvelope = 0.6f + 0.4f * nextRandom();
    }

    // Decay envelopes
    state.surfaceEnvelope *= state.surfaceDecay;
    state.popEnvelope *= state.popDecay;

    // Generate noise burst shaped by envelopes
    float surfaceNoise = nextRandomBipolar() * state.surfaceEnvelope;
    float popNoise = nextRandomBipolar() * state.popEnvelope;

    // Simple bandpass for crackle character (~1-8kHz range)
    float input = surfaceNoise + popNoise * 2.0f;

    // 2nd order bandpass — wide bandwidth to avoid tonal ringing
    float freq = 3000.0f / static_cast<float>(sampleRate);
    float bw = 2.5f; // Very wide — shapes spectrum without audible pitch
    float R = 1.0f - (juce::MathConstants<float>::pi * bw * freq);
    R = juce::jmax(R, 0.0f);
    float cosFreq = std::cos(juce::MathConstants<float>::twoPi * freq);

    float output = input + 2.0f * R * cosFreq * state.bp_z1 - R * R * state.bp_z2;
    state.bp_z2 = state.bp_z1;
    state.bp_z1 = output;

    return output * 0.5f; // Normalize
}

// ---- Tape Hiss ----

float NoiseModule::generateHiss(HissFilter& filter)
{
    // White noise source
    float noise = nextRandomBipolar();

    // One-pole high shelf: boosts high frequencies for tape character
    // z1 stores the high-pass state (NOT the full output) to keep feedback
    // coefficient = shelfCoeff ≈ 0.566 < 1.0 → stable pole at z = -shelfCoeff.
    // Storing output instead would give feedback = shelfGain*shelfCoeff ≈ 1.13 → unstable.
    float highPass = noise - filter.shelfCoeff * filter.z1;
    float output = noise + filter.shelfGain * highPass;
    filter.z1 = highPass;  // stable: feedback coeff = shelfCoeff < 1

    return output * 0.3f; // Level normalize
}

// ---- Static ----

float NoiseModule::generateStatic()
{
    // Broadband noise with slow amplitude modulation
    staticState.modPhase += staticState.modRate;
    if (staticState.modPhase >= 1.0f)
        staticState.modPhase -= 1.0f;

    float modulation = 0.5f + 0.5f * std::sin(staticState.modPhase * juce::MathConstants<float>::twoPi);

    return nextGaussian() * modulation;
}

// ---- DC Blocker ----

float NoiseModule::blockDC(DCBlocker& state, float input)
{
    // Standard DC blocking filter: y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
    float output = input - state.x1 + 0.995f * state.y1;
    state.x1 = input;
    state.y1 = output;
    return output;
}
