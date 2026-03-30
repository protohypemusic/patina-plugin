#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "../PatinaLookAndFeel.h"

// ============================================================
// SpectralBloomComponent
//
// Real-time FFT spectrum visualizer with module-reactive effects.
// Reads the post-processing FFT data from PatinaProcessor and
// renders logarithmically-spaced frequency bars with trails,
// peaks, particles, and per-module visual modulations.
//
// Effects by active module:
//   NOISE     — particles spawn from bar peaks
//   WOBBLE    — sine-wave x-offset on bars
//   DISTORT   — height jitter + white color shift
//   RESONATOR — harmonic shimmer on bars
//   SPACE     — trail persists longer
// ============================================================

class SpectralBloomComponent : public juce::Component,
                               private juce::Timer
{
public:
    explicit SpectralBloomComponent(PatinaProcessor& processor);
    ~SpectralBloomComponent() override;

    /** Clear all visual state so the visualizer is idle until new audio arrives. */
    void resetVisualState();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // ---- FFT → display bins ----
    static constexpr int kNumBins   = 128;
    static constexpr int kFPS       = 30;

    // Smoothing factors
    static constexpr float kSmoothDown  = 0.82f;   // bar decay
    static constexpr float kSmoothUp    = 0.40f;   // bar attack
    static constexpr float kTrailDecay  = 0.94f;   // trail decay (base)
    static constexpr float kPeakDecay   = 0.97f;   // peak dot decay

    // dB range (post-normalization: 0 dB = full-scale sine)
    static constexpr float kMinDb = -50.0f;
    static constexpr float kMaxDb =  10.0f;

    // ---- Particle system ----
    static constexpr int kMaxParticles = 120;

    struct Particle
    {
        float x  = 0.0f;   // normalised [0, 1] across width
        float y  = 0.0f;   // normalised [0, 1] bottom = 1
        float vx = 0.0f;
        float vy = 0.0f;
        float life     = 0.0f;   // 1 → 0
        float size     = 2.0f;
        float hue      = 0.0f;   // 0 = purple, shifts toward white with distort
    };

    // ---- Data ----
    PatinaProcessor& processorRef;

    std::array<float, kNumBins> displayBins {};   // smoothed bar heights [0,1]
    std::array<float, kNumBins> trailBins {};     // slower-decay trail
    std::array<float, kNumBins> peakBins {};      // peak-hold dots
    std::array<float, kNumBins> distortJitter {}; // pre-computed per-frame

    std::array<Particle, kMaxParticles> particles {};
    int nextParticle = 0;

    float phaseAccum     = 0.0f;   // for wobble sine offset
    float resonatorPhase = 0.0f;   // for resonator shimmer

    // Cached module amounts (read each frame)
    float effNoise     = 0.0f;
    float effWobble    = 0.0f;
    float effDistort   = 0.0f;
    float effResonator = 0.0f;
    float effSpace     = 0.0f;

    // ---- Helpers ----
    void updateBinsFromFFT();
    void updateParticles();
    void spawnParticle(float normX, float normY);

    // Map FFT bin index → display bin (logarithmic)
    int fftBinForDisplayBin(int displayBin) const;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralBloomComponent)
};
