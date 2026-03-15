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
//   Header bar       52px  -- wordmark + preset nav + RANDOMIZE
//   Visualizer area  228px -- SpectralBloomComponent
//   Module strip     280px -- 5 module columns (knobs, type
//                             selectors, secondary knobs, locks)
//   Master bar       80px  -- MIX knob (centered)
//
// Modules (left to right):
//   NOISE | WOBBLE | DISTORT | RESONATOR | SPACE
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
    // ---- Timer (sync lock + type button state) ----
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
    void syncTypeButtons();
    void updatePresetLabel();

    // ---- Preset dropdown + save ----
    void showPresetMenu();
    void showSavePresetDialog();

    // ---- References ----
    PatinaProcessor& processorRef;

    // ---- Look and feel (must outlive all components) ----
    PatinaLookAndFeel lookAndFeel;

    // ---- Header ----
    juce::TextButton initButton      { "INIT" };
    juce::TextButton randomizeButton { "RANDOMIZE" };

    // ---- Preset selector (centered in header) ----
    juce::TextButton presetPrevButton { "<" };
    juce::TextButton presetNextButton { ">" };
    juce::TextButton presetNameButton;     // clickable -- opens dropdown
    juce::TextButton presetSaveButton { "SAVE" };

    // ---- Module strips (5 modules) ----
    struct ModuleStrip
    {
        juce::Slider     knob;
        juce::Label      label;
        juce::TextButton lockButton;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    static constexpr int kNumModules = 5;
    std::array<ModuleStrip, kNumModules> moduleStrips;

    // ---- Secondary knobs (6 total) ----
    struct SecondaryKnob
    {
        juce::Slider knob;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    SecondaryKnob noiseTone;
    SecondaryKnob wobbleRate;
    SecondaryKnob wobbleShape;
    SecondaryKnob distortTone;
    SecondaryKnob resonatorFreq;
    SecondaryKnob resonatorReso;
    SecondaryKnob spaceDecay;

    void setupSecondaryKnob(SecondaryKnob& sec, const juce::String& labelText,
                            const juce::String& paramId, double defaultValue,
                            const juce::String& tooltip);

    // ---- Type selector buttons (3 modules + wobble sync) ----
    juce::TextButton noiseTypeButton;
    juce::TextButton wobbleSyncButton;
    juce::TextButton distortTypeButton;
    juce::TextButton resonatorTypeButton;
    juce::TextButton spaceTypeButton;

    static const char* const kNoiseTypeNames[3];
    static const char* const kWobbleRateModeNames[9];
    static const char* const kDistortTypeNames[5];
    static const char* const kResonatorTypeNames[3];
    static const char* const kSpaceTypeNames[4];

    // ---- Master controls ----
    juce::Slider mixKnob;
    juce::Label  mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    // ---- Visualizer ----
    SpectralBloomComponent spectralBloom;

    // ---- Module metadata ----
    static const char* const kModuleNames[kNumModules];
    static const char* const kModuleParamIds[kNumModules];
    static const RandomizeSystem::ModuleId kModuleIds[kNumModules];
    static constexpr float kModuleDefaults[kNumModules] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    static const char* const kModuleTooltips[kNumModules];

    // ---- Branding ----
    // Panchang Bold is loaded in PatinaLookAndFeel and accessed via getPanchangFont()

    // ---- Tooltip window ----
    juce::TooltipWindow tooltipWindow { this, 600 };

    // ---- Layout constants ----
    static constexpr int kEditorWidth  = 900;
    static constexpr int kEditorHeight = 640;
    static constexpr int kHeaderH      = 80;
    static constexpr int kVisuH        = 200;
    static constexpr int kStripH       = 280;
    static constexpr int kMasterH      = 80;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatinaEditor)
};
