#include "SpectralBloomComponent.h"
#include <cmath>
#include <algorithm>

// ================================================================
// Construction / Destruction
// ================================================================

SpectralBloomComponent::SpectralBloomComponent(PatinaProcessor& p)
    : processorRef(p)
{
    setOpaque(true);
    startTimerHz(kFPS);
}

SpectralBloomComponent::~SpectralBloomComponent()
{
    stopTimer();
}

// ================================================================
// FFT bin → display bin (logarithmic mapping)
//
// Maps kNumBins display bins onto the FFT output using a log scale
// so low frequencies get more visual resolution than high ones.
// ================================================================

int SpectralBloomComponent::fftBinForDisplayBin(int displayBin) const
{
    // Map display bin [0, kNumBins) → frequency bin [1, FFT_SIZE/2)
    // using exponential mapping: more bins at the low end
    const float minFreqBin = 1.0f;
    const float maxFreqBin = static_cast<float>(PatinaProcessor::FFT_SIZE) / 2.0f;

    const float t = static_cast<float>(displayBin) / static_cast<float>(kNumBins);

    // Logarithmic: bin = min * (max/min)^t
    const float bin = minFreqBin * std::pow(maxFreqBin / minFreqBin, t);
    return juce::jlimit(1, static_cast<int>(maxFreqBin) - 1, static_cast<int>(bin));
}

// ================================================================
// Timer callback — runs at 30 fps on the message thread
// ================================================================

void SpectralBloomComponent::timerCallback()
{
    // ---- Read module parameters ----
    auto& apvts = processorRef.getAPVTS();

    effNoise     = apvts.getRawParameterValue("noise_amount")->load();
    effWobble    = apvts.getRawParameterValue("wobble_amount")->load();
    effDistort   = apvts.getRawParameterValue("distort_amount")->load();
    effResonator = apvts.getRawParameterValue("resonator_amount")->load();
    effSpace     = apvts.getRawParameterValue("space_amount")->load();

    // ---- Update FFT display data ----
    updateBinsFromFFT();

    // ---- Pre-compute distort jitter (avoid randomness inside paint) ----
    if (effDistort > 0.01f)
    {
        for (int i = 0; i < kNumBins; ++i)
            distortJitter[i] = (rng.nextFloat() - 0.5f) * 2.0f * effDistort * 0.15f;
    }
    else
    {
        distortJitter.fill(0.0f);
    }

    // ---- Advance phase accumulators ----
    phaseAccum     += 0.04f;       // wobble visual sine
    resonatorPhase += 0.09f;       // resonator shimmer

    // ---- Update particles ----
    updateParticles();

    // ---- Spawn particles from peaks when Noise is active ----
    if (effNoise > 0.05f)
    {
        // Spawn from a few random high bars each frame
        for (int attempt = 0; attempt < 3; ++attempt)
        {
            const int bin = rng.nextInt(kNumBins);
            if (displayBins[bin] > 0.25f && rng.nextFloat() < effNoise * 0.6f)
            {
                const float normX = (static_cast<float>(bin) + 0.5f) / static_cast<float>(kNumBins);
                const float normY = 1.0f - displayBins[bin]; // top of bar
                spawnParticle(normX, normY);
            }
        }
    }

    repaint();
}

// ================================================================
// updateBinsFromFFT — read FFT data, map to display bins, smooth
// ================================================================

void SpectralBloomComponent::updateBinsFromFFT()
{
    const float* fftData = processorRef.getFFTData();
    const int fftHalf = PatinaProcessor::FFT_SIZE / 2;

    // Normalize raw FFT magnitudes so a full-scale sine = 1.0 (0 dB).
    // JUCE's unnormalized FFT produces magnitudes proportional to N/2,
    // so a full-scale sine at any bin gives magnitude ~1024 for N=2048.
    // Dividing by N/2 brings that back to 1.0.
    static constexpr float fftNorm = 2.0f / static_cast<float>(PatinaProcessor::FFT_SIZE);

    for (int i = 0; i < kNumBins; ++i)
    {
        // Logarithmic mapping: get the FFT bin range for this display bin
        const int fftBinLo = fftBinForDisplayBin(i);
        const int fftBinHi = fftBinForDisplayBin(i + 1);

        // Average magnitudes across the FFT bin range
        float sumMag = 0.0f;
        int count = 0;

        for (int b = fftBinLo; b < fftBinHi && b < fftHalf; ++b)
        {
            sumMag += fftData[b];
            ++count;
        }

        if (count == 0)
        {
            sumMag = fftData[fftBinLo];
            count = 1;
        }

        const float avgMag = (sumMag / static_cast<float>(count)) * fftNorm;

        // Convert to dB
        const float db = (avgMag > 1.0e-10f)
                            ? 20.0f * std::log10(avgMag)
                            : kMinDb;

        // Normalise to [0, 1]
        float normLevel = (db - kMinDb) / (kMaxDb - kMinDb);
        normLevel = juce::jlimit(0.0f, 1.0f, normLevel);

        // Smooth: fast attack, slow release
        if (normLevel > displayBins[i])
            displayBins[i] += (normLevel - displayBins[i]) * (1.0f - kSmoothUp);
        else
            displayBins[i] *= kSmoothDown;

        // ---- Trail (slower decay, Space makes it persist longer) ----
        float trailFactor = kTrailDecay;
        if (effSpace > 0.01f)
            trailFactor = kTrailDecay + (0.99f - kTrailDecay) * effSpace;

        if (normLevel > trailBins[i])
            trailBins[i] = normLevel;
        else
            trailBins[i] *= trailFactor;

        // ---- Peak hold ----
        if (normLevel > peakBins[i])
            peakBins[i] = normLevel;
        else
            peakBins[i] *= kPeakDecay;
    }
}

// ================================================================
// Particle system
// ================================================================

void SpectralBloomComponent::spawnParticle(float normX, float normY)
{
    auto& p = particles[nextParticle];
    nextParticle = (nextParticle + 1) % kMaxParticles;

    p.x    = normX;
    p.y    = normY;
    p.vx   = (rng.nextFloat() - 0.5f) * 0.012f;
    p.vy   = -(rng.nextFloat() * 0.015f + 0.005f);   // drift upward
    p.life = 1.0f;
    p.size = 1.5f + rng.nextFloat() * 2.5f;
    p.hue  = 0.0f;
}

void SpectralBloomComponent::updateParticles()
{
    const float dt = 1.0f / static_cast<float>(kFPS);

    for (auto& p : particles)
    {
        if (p.life <= 0.0f) continue;

        p.x += p.vx;
        p.y += p.vy;
        p.vy -= 0.0002f;  // subtle upward acceleration
        p.life -= dt * 1.5f;

        if (p.life < 0.0f) p.life = 0.0f;
    }
}

// ================================================================
// resized
// ================================================================

void SpectralBloomComponent::resized()
{
    // Nothing to lay out — everything is painted
}

// ================================================================
// paint — the main visual rendering
// ================================================================

void SpectralBloomComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // ---- 1. Black background ----
    g.fillAll(juce::Colour(PatinaLookAndFeel::colourBackground));

    // ---- 2. Radial purple ambience (subtle) ----
    {
        const float cx = w * 0.5f;
        const float cy = h * 0.55f;
        const float radius = w * 0.5f;

        for (int ring = 8; ring >= 0; --ring)
        {
            const float t = static_cast<float>(ring) / 8.0f;
            const float alpha = t * 0.04f * (1.0f + effSpace * 0.6f);
            const float r = radius * (1.0f - t * 0.4f);
            g.setColour(juce::Colour(PatinaLookAndFeel::colourPurple).withAlpha(alpha));
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
    }

    // ---- Bar geometry ----
    const float barGap    = 1.0f;
    const float totalBarW = (w - barGap * static_cast<float>(kNumBins - 1)) / static_cast<float>(kNumBins);
    const float barW      = std::max(totalBarW, 1.5f);
    const float baseY     = h - 2.0f;  // 2px above bottom for base line

    // Colours
    const auto purpleBright = juce::Colour(PatinaLookAndFeel::colourPurple);
    const auto purpleDeep   = juce::Colour(PatinaLookAndFeel::colourDeepPurple);
    const auto white        = juce::Colour(PatinaLookAndFeel::colourWhite);

    // ---- 3. Trail layer ----
    for (int i = 0; i < kNumBins; ++i)
    {
        const float barH = trailBins[i] * (baseY - 4.0f);
        if (barH < 1.0f) continue;

        float x = static_cast<float>(i) * (barW + barGap);

        // WOBBLE: sine offset on x position
        if (effWobble > 0.01f)
            x += std::sin(phaseAccum + static_cast<float>(i) * 0.15f) * effWobble * 10.0f;

        const float trailAlpha = 0.08f + effSpace * 0.10f;
        g.setColour(purpleDeep.withAlpha(trailAlpha));
        g.fillRect(x, baseY - barH, barW, barH);
    }

    // ---- 4. Main bars ----
    for (int i = 0; i < kNumBins; ++i)
    {
        float barH = displayBins[i] * (baseY - 4.0f);

        // DISTORT: height jitter
        barH += distortJitter[i] * (baseY - 4.0f);
        barH = std::max(barH, 0.0f);

        if (barH < 0.5f) continue;

        float x = static_cast<float>(i) * (barW + barGap);

        // WOBBLE: same sine offset as trail
        if (effWobble > 0.01f)
            x += std::sin(phaseAccum + static_cast<float>(i) * 0.15f) * effWobble * 10.0f;

        const float barTop = baseY - barH;

        // RESONATOR: harmonic shimmer — alternating bars pulse in brightness
        float resoMod = 1.0f;
        if (effResonator > 0.01f)
        {
            resoMod = 1.0f - effResonator * 0.25f
                      * (0.5f + 0.5f * std::sin(resonatorPhase + static_cast<float>(i) * 0.5f));
        }

        // Gradient: deep purple at bottom → bright purple at top
        const float normH = barH / (baseY - 4.0f);
        auto barColour = purpleDeep.interpolatedWith(purpleBright, normH * 0.8f + 0.2f);

        // DISTORT: shift toward white at peaks
        if (effDistort > 0.01f && normH > 0.5f)
        {
            const float whiteShift = effDistort * (normH - 0.5f) * 0.6f;
            barColour = barColour.interpolatedWith(white, whiteShift);
        }

        barColour = barColour.withMultipliedAlpha(resoMod);

        g.setColour(barColour);
        g.fillRect(x, barTop, barW, barH);

        // ---- 5. Glow overlay (wider, lower alpha) ----
        if (barH > 10.0f)
        {
            const float glowAlpha = 0.10f * normH * resoMod;
            g.setColour(purpleBright.withAlpha(glowAlpha));
            g.fillRect(x - 1.5f, barTop, barW + 3.0f, barH);
        }
    }

    // ---- 6. Peak dots ----
    for (int i = 0; i < kNumBins; ++i)
    {
        if (peakBins[i] < 0.02f) continue;

        float x = static_cast<float>(i) * (barW + barGap) + barW * 0.5f;

        if (effWobble > 0.01f)
            x += std::sin(phaseAccum + static_cast<float>(i) * 0.15f) * effWobble * 10.0f;

        const float peakY = baseY - peakBins[i] * (baseY - 4.0f);
        const float dotAlpha = peakBins[i] * 0.8f;

        g.setColour(white.withAlpha(dotAlpha));
        g.fillEllipse(x - 1.5f, peakY - 1.5f, 3.0f, 3.0f);
    }

    // ---- 7. Particles ----
    for (const auto& p : particles)
    {
        if (p.life <= 0.0f) continue;

        const float px = p.x * w;
        const float py = p.y * h;
        const float alpha = p.life * 0.7f;

        // Distort shifts particle colour toward white
        auto particleColour = purpleBright.interpolatedWith(white, effDistort * 0.4f);
        g.setColour(particleColour.withAlpha(alpha));
        g.fillEllipse(px - p.size * 0.5f, py - p.size * 0.5f, p.size, p.size);
    }

    // ---- 8. Base line (thin, subtle) ----
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.fillRect(0.0f, baseY, w, 1.5f);
}
