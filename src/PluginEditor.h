#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PatinaLookAndFeel.h"
#include "RandomizeSystem.h"
#include "visualizer/SpectralBloomComponent.h"

// ============================================================
// PatinaEditor
//
// Layout (900 x 640):
//   Header bar       52px  — wordmark + RANDOMIZE button
//   Visualizer area  228px — SpectralBloomComponent
//   Module strip     260px — 6 module knobs + lock buttons
//                            + secondary knobs (WOBBLE rate, SPACE decay)
//   Master bar       100px — MIX knob (centered)
// ============================================================

class PatinaEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PatinaEditor(PatinaProcessor& processor);
    ~PatinaEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // ---- Timer (sync lock button visual state) ----
    void timerCallback() override;

    // ---- Helpers ----
    void setupMasterKnob(juce::Slider& knob, juce::Label& label,
                         const juce::String& labelText,
                         juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& paramId,
                         std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                         double defaultValue,
                         const juce::String& tooltip);

    void syncLockButtons();

    // ---- References ----
    PatinaProcessor& processorRef;

    // ---- Look and feel (must outlive all components) ----
    PatinaLookAndFeel lookAndFeel;

    // ---- Header ----
    juce::TextButton randomizeButton { "RANDOMIZE" };

    // ---- Preset selector (centered in header) ----
    juce::TextButton presetPrevButton { "<" };
    juce::TextButton presetNextButton { ">" };
    juce::Label      presetNameLabel;
    void             updatePresetLabel();

    // ---- Module strips (6 modules) ----
    struct ModuleStrip
    {
        juce::Slider     knob;
        juce::Label      label;
        juce::TextButton lockButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    std::array<ModuleStrip, 6> moduleStrips;

    // ---- Master controls ----
    juce::Slider mixKnob;
    juce::Label  mixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    // ---- Secondary controls (below WOBBLE and SPACE) ----
    juce::Slider wobbleRateKnob;
    juce::Label  wobbleRateLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wobbleRateAttachment;

    juce::Slider spaceDecayKnob;
    juce::Label  spaceDecayLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spaceDecayAttachment;

    // ---- Visualizer ----
    SpectralBloomComponent spectralBloom;

    // ---- Module metadata ----
    static const char* const kModuleNames[6];
    static const char* const kModuleParamIds[6];
    static const RandomizeSystem::ModuleId kModuleIds[6];
    static constexpr float kModuleDefaults[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f };
    static const char* const kModuleTooltips[6];

    // ---- Tooltip window (must be a member; shows knob/button hints) ----
    juce::TooltipWindow tooltipWindow { this, 600 };  // 600 ms delay

    // ---- Layout constants ----
    static constexpr int kEditorWidth  = 900;
    static constexpr int kEditorHeight = 640;
    static constexpr int kHeaderH      = 52;
    static constexpr int kVisuH        = 228;
    static constexpr int kStripH       = 260;
    static constexpr int kMasterH      = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatinaEditor)
};
