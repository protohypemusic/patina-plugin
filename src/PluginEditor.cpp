#include "PluginEditor.h"
#include <cmath>

// ----------------------------------------------------------------
// Module metadata — matches APVTS parameter IDs in PluginProcessor
// ----------------------------------------------------------------
const char* const PatinaEditor::kModuleNames[6] = {
    "NOISE", "WOBBLE", "DISTORT", "SPACE", "FLUX", "FILTER"
};

const char* const PatinaEditor::kModuleParamIds[6] = {
    "noise_amount", "wobble_amount", "distort_amount",
    "space_amount", "flux_amount", "filter_amount"
};

const RandomizeSystem::ModuleId PatinaEditor::kModuleIds[6] = {
    RandomizeSystem::ModuleId::Noise,
    RandomizeSystem::ModuleId::Wobble,
    RandomizeSystem::ModuleId::Distort,
    RandomizeSystem::ModuleId::Space,
    RandomizeSystem::ModuleId::Flux,
    RandomizeSystem::ModuleId::Filter
};

const char* const PatinaEditor::kModuleTooltips[6] = {
    "NOISE — adds vinyl crackle, tape hiss, or static. Double-click to reset.",
    "WOBBLE — volume tremolo from slow pulse to fast flutter. Double-click to reset.",
    "DISTORT — continuous saturation from warm to aggressive. Double-click to reset.",
    "SPACE — plate reverb adds depth and air. Double-click to reset.",
    "FLUX — beat repeat glitch from subtle stutters to rapid-fire chaos. Double-click to reset.",
    "FILTER — below centre cuts bass; above centre cuts treble. Double-click to reset."
};

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------
PatinaEditor::PatinaEditor(PatinaProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), spectralBloom(p)
{
    setLookAndFeel(&lookAndFeel);
    setSize(kEditorWidth, kEditorHeight);

    // ---- Visualizer ----
    addAndMakeVisible(spectralBloom);

    // ---- Randomize button ----
    randomizeButton.setName("randomize");
    randomizeButton.onClick = [this]
    {
        processorRef.getRandomizeSystem().randomize(processorRef.getAPVTS());
        syncLockButtons();
    };
    addAndMakeVisible(randomizeButton);

    // ---- Preset selector ----
    presetPrevButton.setName("preset_nav");
    presetNextButton.setName("preset_nav");

    presetPrevButton.onClick = [this]
    {
        processorRef.getPresetManager().prevPreset(processorRef.getAPVTS());
        updatePresetLabel();
    };
    presetNextButton.onClick = [this]
    {
        processorRef.getPresetManager().nextPreset(processorRef.getAPVTS());
        updatePresetLabel();
    };

    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    presetNameLabel.setColour(juce::Label::textColourId,
                              juce::Colour(PatinaLookAndFeel::colourWhite));
    presetNameLabel.setColour(juce::Label::backgroundColourId,
                              juce::Colours::transparentBlack);

    updatePresetLabel();

    addAndMakeVisible(presetPrevButton);
    addAndMakeVisible(presetNextButton);
    addAndMakeVisible(presetNameLabel);

    // ---- Module strips ----
    auto& apvts = processorRef.getAPVTS();
    auto& rs    = processorRef.getRandomizeSystem();

    for (int i = 0; i < 6; ++i)
    {
        auto& strip = moduleStrips[i];

        // Knob
        strip.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        strip.knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
        strip.knob.setNumDecimalPlacesToDisplay(2);
        addAndMakeVisible(strip.knob);

        strip.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, kModuleParamIds[i], strip.knob);

        // Double-click resets to default; tooltip describes the module
        strip.knob.setDoubleClickReturnValue(true, static_cast<double>(kModuleDefaults[i]));
        strip.knob.setTooltip(kModuleTooltips[i]);

        // Label
        strip.label.setText(kModuleNames[i], juce::dontSendNotification);
        strip.label.setJustificationType(juce::Justification::centred);
        strip.label.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
        strip.label.setColour(juce::Label::textColourId,
                              juce::Colour(PatinaLookAndFeel::colourWhite).withAlpha(0.75f));
        addAndMakeVisible(strip.label);

        // Lock button
        strip.lockButton.setName("lock");
        strip.lockButton.setClickingTogglesState(true);
        strip.lockButton.setToggleState(rs.isLocked(kModuleIds[i]),
                                        juce::dontSendNotification);
        strip.lockButton.onClick = [this, i]
        {
            auto& strip2 = moduleStrips[i];
            processorRef.getRandomizeSystem().setLocked(kModuleIds[i],
                                                        strip2.lockButton.getToggleState());
        };
        addAndMakeVisible(strip.lockButton);
    }

    // ---- Secondary knobs (Wobble Rate, Space Decay) ----
    {
        auto setupSecondaryKnob = [this, &apvts](
            juce::Slider& knob, juce::Label& label,
            const juce::String& labelText,
            const juce::String& paramId,
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
            double defaultValue,
            const juce::String& tooltip)
        {
            knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
            knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            knob.setNumDecimalPlacesToDisplay(2);
            knob.setDoubleClickReturnValue(true, defaultValue);
            knob.setTooltip(tooltip);
            addAndMakeVisible(knob);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, paramId, knob);

            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setFont(juce::Font(juce::FontOptions(9.0f).withStyle("Bold")));
            label.setColour(juce::Label::textColourId,
                            juce::Colour(PatinaLookAndFeel::colourWhite).withAlpha(0.55f));
            addAndMakeVisible(label);
        };

        setupSecondaryKnob(wobbleRateKnob, wobbleRateLabel, "RATE", "wobble_rate",
                           wobbleRateAttachment, 0.3,
                           "RATE \xe2\x80\x94 tremolo speed from slow pulse to fast ring-mod. Double-click to reset.");

        setupSecondaryKnob(spaceDecayKnob, spaceDecayLabel, "DECAY", "space_decay",
                           spaceDecayAttachment, 0.3,
                           "DECAY \xe2\x80\x94 reverb tail from tight room to long ambient wash. Double-click to reset.");
    }

    // ---- Master knobs ----
    setupMasterKnob(mixKnob, mixLabel, "MIX", apvts, "mix", mixAttachment,
                    1.0, "MIX \xe2\x80\x94 dry/wet blend. Full right = 100% processed signal. Double-click to reset.");

    // Sync lock button visuals once on open
    syncLockButtons();

    // Poll for external lock-state changes every 200ms
    startTimer(200);
}

PatinaEditor::~PatinaEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ----------------------------------------------------------------
// Timer — keeps lock button toggle state in sync with RandomizeSystem
// ----------------------------------------------------------------
void PatinaEditor::timerCallback()
{
    syncLockButtons();
}

void PatinaEditor::updatePresetLabel()
{
    const auto& pm = processorRef.getPresetManager();
    const int total = PresetManager::kNumPresets;
    const int idx   = pm.getCurrentIndex() + 1;   // 1-based display

    presetNameLabel.setText(
        juce::String(idx) + " / " + juce::String(total) + "  " + pm.getCurrentName(),
        juce::dontSendNotification);
}

void PatinaEditor::syncLockButtons()
{
    auto& rs = processorRef.getRandomizeSystem();
    for (int i = 0; i < 6; ++i)
    {
        const bool locked = rs.isLocked(kModuleIds[i]);
        if (moduleStrips[i].lockButton.getToggleState() != locked)
            moduleStrips[i].lockButton.setToggleState(locked, juce::dontSendNotification);
    }
}

// ----------------------------------------------------------------
// Master knob helper
// ----------------------------------------------------------------
void PatinaEditor::setupMasterKnob(juce::Slider& knob, juce::Label& label,
                                    const juce::String& labelText,
                                    juce::AudioProcessorValueTreeState& apvts,
                                    const juce::String& paramId,
                                    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& attachment,
                                    double defaultValue,
                                    const juce::String& tooltip)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    knob.setNumDecimalPlacesToDisplay(2);
    knob.setDoubleClickReturnValue(true, defaultValue);
    knob.setTooltip(tooltip);
    addAndMakeVisible(knob);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, paramId, knob);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    label.setColour(juce::Label::textColourId,
                    juce::Colour(PatinaLookAndFeel::colourWhite).withAlpha(0.75f));
    addAndMakeVisible(label);
}

// ----------------------------------------------------------------
// Paint
// ----------------------------------------------------------------
void PatinaEditor::paint(juce::Graphics& g)
{
    // Fill entire background
    g.fillAll(juce::Colour(PatinaLookAndFeel::colourBackground));

    // ---- Header background ----
    const auto headerArea = juce::Rectangle<int>(0, 0, kEditorWidth, kHeaderH);
    g.setColour(juce::Colour(PatinaLookAndFeel::colourDark));
    g.fillRect(headerArea);

    // Subtle bottom border on header
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.drawHorizontalLine(kHeaderH - 1, 0.0f, static_cast<float>(kEditorWidth));

    // ---- Header text ----
    // "PATINA" — large, white
    g.setColour(juce::Colour(PatinaLookAndFeel::colourWhite));
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("PATINA",
               juce::Rectangle<int>(20, 0, 120, kHeaderH),
               juce::Justification::centredLeft, false);

    // "by Futureproof" — small, purple
    g.setColour(juce::Colour(PatinaLookAndFeel::colourPurple));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("by Futureproof",
               juce::Rectangle<int>(20, 28, 120, 16),
               juce::Justification::centredLeft, false);

    // ---- Visualizer ----
    // (SpectralBloomComponent renders itself as a child component)

    // ---- Module strip background ----
    const auto stripArea = juce::Rectangle<int>(0, kHeaderH + kVisuH, kEditorWidth, kStripH);

    // Top divider
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.drawHorizontalLine(stripArea.getY(), 0.0f, static_cast<float>(kEditorWidth));

    // Column dividers between modules
    const int moduleW = kEditorWidth / 6;
    for (int i = 1; i < 6; ++i)
    {
        const float divX = static_cast<float>(i * moduleW);
        g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
        g.drawVerticalLine(static_cast<int>(divX),
                           static_cast<float>(stripArea.getY() + 12),
                           static_cast<float>(stripArea.getBottom() - 12));
    }

    // ---- Master bar background ----
    const auto masterArea = juce::Rectangle<int>(0, kHeaderH + kVisuH + kStripH, kEditorWidth, kMasterH);
    g.setColour(juce::Colour(PatinaLookAndFeel::colourDark));
    g.fillRect(masterArea);

    // Top divider
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.drawHorizontalLine(masterArea.getY(), 0.0f, static_cast<float>(kEditorWidth));

    // Master labels (MIX drawn by the Label component; add "MASTER" caption)
    g.setColour(juce::Colour(PatinaLookAndFeel::colourWhite).withAlpha(0.20f));
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawText("MASTER",
               juce::Rectangle<int>(0, masterArea.getY() + 6, kEditorWidth, 14),
               juce::Justification::centred, false);
}

// ----------------------------------------------------------------
// resized — lay out all components
// ----------------------------------------------------------------
void PatinaEditor::resized()
{
    // ---- Visualizer ----
    spectralBloom.setBounds(0, kHeaderH, kEditorWidth, kVisuH);

    // ---- Header ----
    // RANDOMIZE button: 120×30, right side
    const int btnW = 120;
    const int btnH = 30;
    const int btnX = kEditorWidth - btnW - 20;
    const int btnY = (kHeaderH - btnH) / 2;
    randomizeButton.setBounds(btnX, btnY, btnW, btnH);

    // Preset selector: centered between wordmark and RANDOMIZE button
    // ◄ (28px) + name label (240px) + ► (28px) = 296px total
    const int navW     = 28;
    const int nameW    = 240;
    const int selectorW = navW * 2 + nameW;
    const int selectorX = (kEditorWidth - selectorW) / 2;
    const int navY     = (kHeaderH - btnH) / 2;

    presetPrevButton.setBounds(selectorX,               navY, navW, btnH);
    presetNameLabel .setBounds(selectorX + navW,        navY, nameW, btnH);
    presetNextButton.setBounds(selectorX + navW + nameW, navY, navW, btnH);

    // ---- Module strip ----
    const int stripTop = kHeaderH + kVisuH;
    const int moduleW  = kEditorWidth / 6;

    for (int i = 0; i < 6; ++i)
    {
        auto& strip = moduleStrips[i];
        const int colX = i * moduleW;

        // Label at top of column
        strip.label.setBounds(colX, stripTop + 8, moduleW, 16);

        // Primary knob: 100px, aligned across all columns
        const int knobSize = 100;
        const int knobX = colX + (moduleW - knobSize) / 2;
        const int knobY = stripTop + 28;
        strip.knob.setBounds(knobX, knobY, knobSize, knobSize);

        // Lock button: near bottom of column
        const int lockW = 28;
        const int lockH = 22;
        const int lockX = colX + (moduleW - lockW) / 2;
        const int lockY = stripTop + kStripH - lockH - 8;
        strip.lockButton.setBounds(lockX, lockY, lockW, lockH);
    }

    // ---- Secondary knobs (below WOBBLE and SPACE primary knobs) ----
    {
        const int secKnobSize = 44;
        const int secLabelH   = 14;
        const int secY        = stripTop + 136;   // below primary knob + text box

        // Wobble Rate — column 1 (WOBBLE)
        {
            const int colX = 1 * moduleW;
            const int knobX = colX + (moduleW - secKnobSize) / 2;
            wobbleRateKnob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            wobbleRateLabel.setBounds(colX, secY + secKnobSize + 2, moduleW, secLabelH);
        }

        // Space Decay — column 3 (SPACE)
        {
            const int colX = 3 * moduleW;
            const int knobX = colX + (moduleW - secKnobSize) / 2;
            spaceDecayKnob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            spaceDecayLabel.setBounds(colX, secY + secKnobSize + 2, moduleW, secLabelH);
        }
    }

    // ---- Master bar ----
    const int masterTop = kHeaderH + kVisuH + kStripH;
    const int masterKnobSize = 64;
    const int masterKnobY    = masterTop + (kMasterH - masterKnobSize - 18) / 2 + 10;
    const int labelH         = 14;

    // MIX: centered in master bar
    const int mixCX = kEditorWidth / 2;
    mixKnob.setBounds(mixCX - masterKnobSize / 2, masterKnobY, masterKnobSize, masterKnobSize);
    mixLabel.setBounds(mixCX - 40, masterKnobY + masterKnobSize + 2, 80, labelH);
}
