#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/NoiseModule.h"
#include "dsp/WobbleModule.h"
#include "dsp/DistortModule.h"
#include "dsp/ResonatorModule.h"
#include "dsp/SpaceModule.h"
#include "RandomizeSystem.h"
#include "PresetManager.h"

class PatinaProcessor : public juce::AudioProcessor
{
public:
    PatinaProcessor();
    ~PatinaProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    RandomizeSystem& getRandomizeSystem() { return randomizeSystem; }
    PresetManager&   getPresetManager()   { return presetManager; }

    // FFT data for visualizer
    const float* getFFTData() const { return fftOutputBuffer.data(); }
    static constexpr int FFT_SIZE = 2048;

private:
    juce::AudioProcessorValueTreeState apvts;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- Randomize & Lock System ----
    RandomizeSystem randomizeSystem;

    // ---- Preset System ----
    PresetManager presetManager;

    // ---- DSP Modules ----
    // Signal chain: Noise -> Wobble -> Distort -> Resonator -> Space
    NoiseModule     noiseModule;
    WobbleModule    wobbleModule;
    DistortModule   distortModule;
    ResonatorModule resonatorModule;
    SpaceModule     spaceModule;

    // ---- FFT for Visualizer ----
    juce::dsp::FFT fft { 11 }; // 2^11 = 2048
    std::array<float, FFT_SIZE * 2> fftInputBuffer {};
    std::array<float, FFT_SIZE * 2> fftOutputBuffer {};
    int fftWriteIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatinaProcessor)
};
