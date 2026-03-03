#include "ResonatorModule.h"
#include <cmath>
#include <algorithm>

// Modal harmonic ratios (slightly inharmonic for organic character)
static constexpr float kModeRatios[6] = { 1.0f, 2.0f, 3.01f, 4.7f, 6.28f, 8.54f };
static constexpr float kModeGains[6]  = { 1.0f, 0.7f, 0.45f, 0.3f, 0.2f, 0.12f };

// Formant vowel data: F1, F2, F3 in Hz for /a/ /e/ /i/ /o/ /u/
static constexpr float kVowelF1[5] = { 800, 400, 250, 450, 325 };
static constexpr float kVowelF2[5] = { 1150, 1600, 1750, 800, 700 };
static constexpr float kVowelF3[5] = { 2900, 2700, 2600, 2830, 2530 };

ResonatorModule::ResonatorModule() {}

void ResonatorModule::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;
    reset();
}

void ResonatorModule::reset()
{
    for (auto& ch : combBuf) ch.fill(0);
    combWrite.fill(0);
    combDamp.fill(0);
    for (auto& ch : modal)   for (auto& b : ch) b = {};
    for (auto& ch : formant) for (auto& b : ch) b = {};
    dcX.fill(0);
    dcY.fill(0);
    sAmount = sFreq = sReso = 0;
}

// ── Biquad bandpass coefficient calculation ──────────────────────────
void ResonatorModule::calcBandpass(Biquad& b, double f, double q, double sampleRate)
{
    f = std::clamp(f, 20.0, sampleRate * 0.45);
    q = std::max(q, 0.5);

    double w0    = 2.0 * 3.14159265358979323846 * f / sampleRate;
    double alpha = std::sin(w0) / (2.0 * q);
    double a0    = 1.0 + alpha;

    b.b0 = float( alpha / a0);
    b.b1 = 0.0f;
    b.b2 = float(-alpha / a0);
    b.a1 = float(-2.0 * std::cos(w0) / a0);
    b.a2 = float((1.0 - alpha) / a0);
}

float ResonatorModule::tick(Biquad& b, float in)
{
    float out = b.b0 * in + b.b1 * b.x1 + b.b2 * b.x2
              - b.a1 * b.y1 - b.a2 * b.y2;
    b.x2 = b.x1;  b.x1 = in;
    b.y2 = b.y1;  b.y1 = out;
    return out;
}

// ── Comb filter with damped feedback ─────────────────────────────────
float ResonatorModule::tickComb(int ch, float in, float delaySamples,
                                float feedback, float dampCoeff)
{
    // Linear-interpolation read
    float readPos = float(combWrite[ch]) - delaySamples;
    if (readPos < 0.0f) readPos += float(kMaxDelay);

    int   idx0 = int(readPos) % kMaxDelay;
    int   idx1 = (idx0 + 1) % kMaxDelay;
    float frac = readPos - std::floor(readPos);

    float delayed = combBuf[ch][idx0] + frac * (combBuf[ch][idx1] - combBuf[ch][idx0]);

    // One-pole lowpass in feedback path (warmth)
    combDamp[ch] += dampCoeff * (delayed - combDamp[ch]);

    // Write new sample into buffer
    combBuf[ch][combWrite[ch]] = in + feedback * combDamp[ch];
    combWrite[ch] = (combWrite[ch] + 1) % kMaxDelay;

    return delayed;
}

// ── DC blocker ───────────────────────────────────────────────────────
float ResonatorModule::tickDC(int ch, float in)
{
    float out = in - dcX[ch] + 0.995f * dcY[ch];
    dcX[ch] = in;
    dcY[ch] = out;
    return out;
}

// ── Main process ─────────────────────────────────────────────────────
void ResonatorModule::process(juce::AudioBuffer<float>& buffer,
                              float amount, float freq, float reso, int type)
{
    if (amount < 0.001f && sAmount < 0.001f) return;

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const float smooth   = 1.0f - std::exp(-1.0f / (float(sr) * 0.005f)); // ~5 ms

    // Coefficient update interval (every 32 samples for modal/formant)
    constexpr int kCoeffBlock = 32;
    int coeffCounter = 0;

    for (int s = 0; s < numSamples; ++s)
    {
        // Smooth parameters
        sAmount += smooth * (amount - sAmount);
        sFreq   += smooth * (freq   - sFreq);
        sReso   += smooth * (reso   - sReso);

        // Update biquad coefficients periodically (expensive trig)
        bool updateCoeffs = (coeffCounter == 0);
        coeffCounter = (coeffCounter + 1) % kCoeffBlock;

        if (updateCoeffs)
        {
            if (type == 1) // Modal
            {
                float fundamental = 50.0f * std::pow(100.0f, sFreq); // 50-5000 Hz
                float q = 5.0f + sReso * 95.0f; // Q: 5-100
                for (int ch = 0; ch < numCh; ++ch)
                    for (int m = 0; m < kNumModes; ++m)
                    {
                        float mf = fundamental * kModeRatios[m];
                        if (mf < float(sr) * 0.45f)
                            calcBandpass(modal[ch][m], mf, q / (1.0f + m * 0.3f), sr);
                    }
            }
            else if (type == 2) // Formant
            {
                float morph = sFreq * 4.0f; // 0-4 across 5 vowels
                int   v0 = std::min(int(morph), 3);
                int   v1 = v0 + 1;
                float t  = morph - float(v0);

                float f1 = kVowelF1[v0] + t * (kVowelF1[v1] - kVowelF1[v0]);
                float f2 = kVowelF2[v0] + t * (kVowelF2[v1] - kVowelF2[v0]);
                float f3 = kVowelF3[v0] + t * (kVowelF3[v1] - kVowelF3[v0]);

                float q = 3.0f + sReso * 27.0f; // Q: 3-30

                for (int ch = 0; ch < numCh; ++ch)
                {
                    calcBandpass(formant[ch][0], f1, q,        sr);
                    calcBandpass(formant[ch][1], f2, q * 0.8,  sr);
                    calcBandpass(formant[ch][2], f3, q * 0.6,  sr);
                }
            }
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            float dry = buffer.getSample(ch, s);
            float wet = 0.0f;

            switch (type)
            {
                case 0: // ── Comb ──
                {
                    float freqHz = 50.0f * std::pow(100.0f, sFreq);
                    float delaySamples = float(sr) / std::max(freqHz, 20.0f);
                    delaySamples = std::clamp(delaySamples, 1.0f, float(kMaxDelay - 2));

                    float fb   = 0.3f + sReso * 0.65f;          // 0.3 - 0.95
                    float damp = 0.3f + (1.0f - sReso) * 0.5f;  // warmth at high reso
                    wet = tickComb(ch, dry, delaySamples, fb, damp);
                    break;
                }

                case 1: // ── Modal ──
                {
                    float sum = 0.0f;
                    for (int m = 0; m < kNumModes; ++m)
                    {
                        float mf = 50.0f * std::pow(100.0f, sFreq) * kModeRatios[m];
                        if (mf < float(sr) * 0.45f)
                            sum += tick(modal[ch][m], dry) * kModeGains[m];
                    }

                    // Narrow bandpass filters at high Q capture very little energy
                    // from broadband input. Compensate with sqrt(Q) gain so the
                    // effect gets LOUDER as resonance increases.
                    float q = 5.0f + sReso * 95.0f;
                    float qComp = std::sqrt(q) * 0.7f; // ~1.6x at Q=5, ~7x at Q=100
                    wet = std::tanh(sum * qComp);       // Soft limit prevents blowup
                    break;
                }

                case 2: // ── Formant ──
                {
                    float raw = tick(formant[ch][0], dry) * 0.50f
                              + tick(formant[ch][1], dry) * 0.35f
                              + tick(formant[ch][2], dry) * 0.15f;

                    // Same Q-dependent gain compensation as modal
                    float q = 3.0f + sReso * 27.0f;
                    float qComp = std::sqrt(q) * 0.9f; // ~1.6x at Q=3, ~4.9x at Q=30
                    wet = std::tanh(raw * qComp);       // Soft limit prevents blowup
                    break;
                }
            }

            wet = tickDC(ch, wet);
            buffer.setSample(ch, s, dry + sAmount * (wet - dry));
        }
    }
}
