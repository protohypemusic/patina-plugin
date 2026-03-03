#include "NoiseModule.h"
#include <cmath>
#include <algorithm>

NoiseModule::NoiseModule() {}

void NoiseModule::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;

    // Envelope follower: ~1 ms attack, ~50 ms release
    envAttack  = std::exp(-1.0f / (0.001f * float(sr)));
    envRelease = std::exp(-1.0f / (0.050f * float(sr)));
    envState   = 0.0f;

    reset();
}

void NoiseModule::reset()
{
    for (auto& p : pink) p = {};
    for (auto& c : crackle) c = {};
    lpZ.fill(0);
    hpZ.fill(0);
    dcX.fill(0);
    dcY.fill(0);
    envState = 0.0f;
    sAmount = sTone = 0.0f;
}

// ── PRNG ─────────────────────────────────────────────────────────────

float NoiseModule::nextRandom()
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return float(rngState) / float(UINT32_MAX);
}

float NoiseModule::nextRandomBipolar()
{
    return nextRandom() * 2.0f - 1.0f;
}

// ── Noise generators ─────────────────────────────────────────────────

float NoiseModule::generateWhite()
{
    return nextRandomBipolar();
}

float NoiseModule::generatePink(int ch)
{
    // Paul Kellet's economy method (perf-friendly pink noise filter)
    float white = nextRandomBipolar();
    auto& p = pink[ch];

    p.b0 = 0.99886f * p.b0 + white * 0.0555179f;
    p.b1 = 0.99332f * p.b1 + white * 0.0750759f;
    p.b2 = 0.96900f * p.b2 + white * 0.1538520f;
    p.b3 = 0.86650f * p.b3 + white * 0.3104856f;
    p.b4 = 0.55000f * p.b4 + white * 0.5329522f;
    p.b5 = -0.7616f * p.b5 - white * 0.0168980f;

    float out = p.b0 + p.b1 + p.b2 + p.b3 + p.b4 + p.b5 + p.b6 + white * 0.5362f;
    p.b6 = white * 0.115926f;

    return out * 0.11f; // Normalize to roughly -1..1
}

float NoiseModule::generateCrackle(int ch)
{
    // Vinyl-style noise: bandpassed surface hiss + random pops/clicks.
    // Unique character that White and Pink don't have.
    auto& c = crackle[ch];
    float white = nextRandomBipolar();
    float dt = 1.0f / float(sr);

    // --- Surface noise: bandpass via LP then HP ---

    // One-pole LP at ~3 kHz (removes harsh highs)
    float lpCutoff = 3000.0f;
    float lpRc     = 1.0f / (2.0f * 3.14159265f * lpCutoff);
    float lpAlpha  = dt / (lpRc + dt);
    c.surfaceLp   += lpAlpha * (white - c.surfaceLp);

    // One-pole HP at ~300 Hz (removes rumble, keeps mid-range sizzle)
    float hpCutoff = 300.0f;
    float hpRc     = 1.0f / (2.0f * 3.14159265f * hpCutoff);
    float hpAlpha  = hpRc / (hpRc + dt);
    float hpOut    = hpAlpha * (c.surfaceHpOut + c.surfaceLp - c.surfaceHpIn);
    c.surfaceHpIn  = c.surfaceLp;
    c.surfaceHpOut = hpOut;

    // --- Random pops: sparse impulses ~3-4 per second ---
    float pop = 0.0f;
    if (nextRandom() < 3.5f * dt)
    {
        // New pop: random intensity 0.5 - 1.0
        c.popEnv = 0.5f + nextRandom() * 0.5f;
    }

    if (c.popEnv > 0.001f)
    {
        pop = c.popEnv;
        c.popEnv *= 0.85f; // Fast decay (~2ms at 44.1k)
    }

    // Mix surface hiss with pops
    return hpOut * 0.5f + pop * 0.9f;
}

// ── Tone control ─────────────────────────────────────────────────────

float NoiseModule::applyTone(int ch, float in, float tone)
{
    // tone 0.0 = dark (LP at ~200 Hz)
    // tone 0.5 = flat (no filtering)
    // tone 1.0 = bright (HP at ~2000 Hz)

    if (tone < 0.48f)
    {
        // Lowpass: cutoff sweeps 200 Hz (tone=0) to 20 kHz (tone=0.5)
        float t = tone / 0.48f; // 0 to 1
        float cutoff = 200.0f * std::pow(100.0f, t); // 200 Hz - 20 kHz
        float rc = 1.0f / (2.0f * 3.14159265f * cutoff);
        float dt = 1.0f / float(sr);
        float alpha = dt / (rc + dt);

        lpZ[ch] += alpha * (in - lpZ[ch]);
        return lpZ[ch];
    }
    else if (tone > 0.52f)
    {
        // Highpass: cutoff sweeps 20 Hz (tone=0.52) to 2000 Hz (tone=1.0)
        float t = (tone - 0.52f) / 0.48f; // 0 to 1
        float cutoff = 20.0f * std::pow(100.0f, t); // 20 Hz - 2 kHz
        float rc = 1.0f / (2.0f * 3.14159265f * cutoff);
        float dt = 1.0f / float(sr);
        float alpha = rc / (rc + dt);

        float out = alpha * (hpZ[ch] + in - lpZ[ch]);
        lpZ[ch] = in;    // Store previous input for HP
        hpZ[ch] = out;
        return out;
    }

    // Flat zone (0.48 - 0.52): no filtering
    return in;
}

// ── DC Blocker ───────────────────────────────────────────────────────

float NoiseModule::tickDC(int ch, float in)
{
    float out = in - dcX[ch] + 0.995f * dcY[ch];
    dcX[ch] = in;
    dcY[ch] = out;
    return out;
}

// ── Main process ─────────────────────────────────────────────────────

void NoiseModule::process(juce::AudioBuffer<float>& buffer,
                          float amount, float tone, int type)
{
    if (amount < 0.001f && sAmount < 0.001f) return;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const float smooth   = 1.0f - std::exp(-1.0f / (float(sr) * 0.005f)); // ~5 ms

    for (int s = 0; s < numSamples; ++s)
    {
        sAmount += smooth * (amount - sAmount);
        sTone   += smooth * (tone   - sTone);

        // Envelope follower: track input level to gate noise
        float inLevel = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            inLevel += std::abs(buffer.getSample(ch, s));

        float coeff = (inLevel > envState) ? envAttack : envRelease;
        envState = coeff * envState + (1.0f - coeff) * inLevel;

        // Gate: ramps from 0 to 1 as input level rises
        float gate = std::min(envState * 8.0f, 1.0f);

        for (int ch = 0; ch < numCh; ++ch)
        {
            // Generate noise based on type
            // Per-type scaling normalizes all three to roughly equal perceived level (~0.2 RMS)
            float noise = 0.0f;
            switch (type)
            {
                case 0:  noise = generateWhite() * 0.3f;      break; // Raw ±1.0, tame to ~0.17 RMS
                case 1:  noise = generatePink(ch);             break; // Already ~0.2 RMS from 0.11 normalizer
                case 2:  noise = generateCrackle(ch) * 2.0f;  break; // Bandpassed is quiet, boost to ~0.2 RMS
            }

            // Apply tone filter
            noise = applyTone(ch, noise, sTone);

            // DC block
            noise = tickDC(ch, noise);

            // Scale, gate, and add to signal
            // 0.4f scaling (up from 0.15f) so distortion can grab the noise
            float sample = buffer.getSample(ch, s);
            sample += noise * sAmount * 0.4f * gate;
            buffer.setSample(ch, s, sample);
        }
    }
}
