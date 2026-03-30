#include "PluginEditor.h"
#include <cmath>

// ----------------------------------------------------------------
// Module metadata -- matches APVTS parameter IDs in PluginProcessor
// ----------------------------------------------------------------
const char* const PatinaEditor::kModuleNames[kNumModules] = {
    "NOISE", "WOBBLE", "DISTORT", "RESONATOR", "SPACE"
};

const char* const PatinaEditor::kModuleParamIds[kNumModules] = {
    "noise_amount", "wobble_amount", "distort_amount",
    "resonator_amount", "space_amount"
};

const RandomizeSystem::ModuleId PatinaEditor::kModuleIds[kNumModules] = {
    RandomizeSystem::ModuleId::Noise,
    RandomizeSystem::ModuleId::Wobble,
    RandomizeSystem::ModuleId::Distort,
    RandomizeSystem::ModuleId::Resonator,
    RandomizeSystem::ModuleId::Space
};

const char* const PatinaEditor::kModuleTooltips[kNumModules] = {
    "NOISE -- adds vinyl crackle, tape hiss, or rumble. Double-click to reset.",
    "WOBBLE -- volume tremolo from slow pulse to fast flutter. Double-click to reset.",
    "DISTORT -- saturation from warm to aggressive. Double-click to reset.",
    "RESONATOR -- comb, modal, or formant coloring. Double-click to reset.",
    "SPACE -- plate reverb adds depth and air. Double-click to reset."
};

// ----------------------------------------------------------------
// Type name arrays
// ----------------------------------------------------------------
const char* const PatinaEditor::kNoiseTypeNames[3] = {
    "WHITE", "PINK", "CRACKLE"
};

const char* const PatinaEditor::kWobbleRateModeNames[9] = {
    "FREE", "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"
};

const char* const PatinaEditor::kDistortTypeNames[5] = {
    "SOFT", "HARD", "DIODE", "FOLD", "CRUSH"
};

const char* const PatinaEditor::kResonatorTypeNames[3] = {
    "COMB", "MODAL", "FORMANT"
};

const char* const PatinaEditor::kSpaceTypeNames[4] = {
    "PLATE", "ROOM", "HALL", "TONAL"
};

// ----------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------
PatinaEditor::PatinaEditor(PatinaProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p), spectralBloom(p)
{
    setLookAndFeel(&lookAndFeel);
    setSize(kEditorWidth, kEditorHeight);

    // Panchang Bold is loaded in LookAndFeel and available via getPanchangFont()

    // ---- Visualizer ----
    addAndMakeVisible(spectralBloom);

    // ---- Init button (reset all knobs to defaults) ----
    initButton.setName("preset_nav");   // reuse subtle header-button style
    initButton.setTooltip("Reset all parameters to their default values.");
    initButton.onClick = [this]
    {
        // Reset every APVTS parameter to its default
        for (auto* param : processorRef.getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
                ranged->setValueNotifyingHost(ranged->getDefaultValue());
        }

        // Unlock all modules
        auto& rs = processorRef.getRandomizeSystem();
        for (int i = 0; i < kNumModules; ++i)
            rs.setLocked(kModuleIds[i], false);

        syncLockButtons();
        syncTypeButtons();
        spectralBloom.resetVisualState();
    };
    addAndMakeVisible(initButton);

    // ---- Randomize button ----
    randomizeButton.setName("randomize");
    randomizeButton.onClick = [this]
    {
        processorRef.getRandomizeSystem().randomize(processorRef.getAPVTS());
        syncLockButtons();
        syncTypeButtons();
        spectralBloom.resetVisualState();
    };
    addAndMakeVisible(randomizeButton);

    // ---- Preset selector ----
    presetPrevButton.setName("preset_nav");
    presetNextButton.setName("preset_nav");

    presetPrevButton.onClick = [this]
    {
        processorRef.getPresetManager().prevPresetGlobal(processorRef.getAPVTS());
        updatePresetLabel();
        syncTypeButtons();
        spectralBloom.resetVisualState();
    };
    presetNextButton.onClick = [this]
    {
        processorRef.getPresetManager().nextPresetGlobal(processorRef.getAPVTS());
        updatePresetLabel();
        syncTypeButtons();
        spectralBloom.resetVisualState();
    };

    // Preset name button -- clickable, opens dropdown
    presetNameButton.setName("preset_nav");
    presetNameButton.setTooltip("Click to browse presets.");
    presetNameButton.onClick = [this] { showPresetMenu(); };

    // Save button
    presetSaveButton.setName("preset_nav");
    presetSaveButton.setTooltip("Save current settings as a user preset.");
    presetSaveButton.onClick = [this] { showSavePresetDialog(); };

    updatePresetLabel();

    addAndMakeVisible(presetPrevButton);
    addAndMakeVisible(presetNextButton);
    addAndMakeVisible(presetNameButton);
    addAndMakeVisible(presetSaveButton);

    // ---- Module strips (5 modules) ----
    auto& apvts = processorRef.getAPVTS();
    auto& rs    = processorRef.getRandomizeSystem();

    for (int i = 0; i < kNumModules; ++i)
    {
        auto& strip = moduleStrips[static_cast<size_t>(i)];

        // Knob
        strip.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        strip.knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        strip.knob.setDoubleClickReturnValue(true, static_cast<double>(kModuleDefaults[i]));
        strip.knob.setTooltip(kModuleTooltips[i]);
        addAndMakeVisible(strip.knob);

        strip.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, kModuleParamIds[i], strip.knob);

        // Label
        strip.label.setText(kModuleNames[i], juce::dontSendNotification);
        strip.label.setJustificationType(juce::Justification::centred);
        strip.label.setFont(lookAndFeel.getPanchangFont(11.0f));
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
            auto& s = moduleStrips[static_cast<size_t>(i)];
            processorRef.getRandomizeSystem().setLocked(kModuleIds[i],
                                                        s.lockButton.getToggleState());
        };
        addAndMakeVisible(strip.lockButton);
    }

    // ---- Secondary knobs (6 total) ----
    setupSecondaryKnob(noiseTone,     "TONE",  "noise_tone",     0.5,
                       "TONE -- sweeps noise colour from dark to bright. Double-click to reset.");
    setupSecondaryKnob(wobbleRate,    "RATE",  "wobble_rate",    0.3,
                       "RATE -- tremolo speed from slow pulse to fast ring-mod. Double-click to reset.");
    setupSecondaryKnob(wobbleShape,  "SHAPE", "wobble_shape",   0.0,
                       "SHAPE -- LFO waveform from smooth sine to hard square chop. Double-click to reset.");
    setupSecondaryKnob(distortTone,   "TONE",  "distort_tone",   0.5,
                       "TONE -- post-distortion EQ from dark to bright. Double-click to reset.");
    setupSecondaryKnob(resonatorFreq, "FREQ",  "resonator_freq", 0.3,
                       "FREQ -- resonator centre frequency. Double-click to reset.");
    setupSecondaryKnob(resonatorReso, "RESO",  "resonator_reso", 0.3,
                       "RESO -- resonator feedback / resonance. Double-click to reset.");
    setupSecondaryKnob(spaceDecay,    "DECAY", "space_decay",    0.3,
                       "DECAY -- reverb tail from tight room to long ambient wash. Double-click to reset.");

    // ---- Type selector buttons (3 modules) ----
    auto setupTypeButton = [this, &apvts](juce::TextButton& btn,
                                           const char* paramId,
                                           int numTypes)
    {
        btn.setName("type_selector");
        btn.onClick = [&btn, &apvts, paramId, numTypes]
        {
            if (auto* param = apvts.getParameter(paramId))
            {
                int current = static_cast<int>(param->convertFrom0to1(param->getValue()));
                int next = (current + 1) % numTypes;
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(next)));
            }
        };
        addAndMakeVisible(btn);
    };

    setupTypeButton(noiseTypeButton,     "noise_type",     3);
    setupTypeButton(distortTypeButton,   "distort_type",   5);
    setupTypeButton(resonatorTypeButton, "resonator_type", 3);
    setupTypeButton(spaceTypeButton,     "space_type",     4);

    // Wobble sync button -- cycles through FREE, 1/1, 1/2, ... 1/32
    wobbleSyncButton.setName("type_selector");
    wobbleSyncButton.onClick = [this, &apvts]
    {
        if (auto* param = apvts.getParameter("wobble_rate_mode"))
        {
            int current = static_cast<int>(param->convertFrom0to1(param->getValue()));
            int next = (current + 1) % 9;
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(next)));
        }
    };
    addAndMakeVisible(wobbleSyncButton);

    // ---- Master knobs ----
    setupMasterKnob(mixKnob, mixLabel, "MIX", apvts, "mix", mixAttachment,
                    1.0, "MIX -- dry/wet blend. Full right = 100% processed signal. Double-click to reset.");

    // Sync visuals on open
    syncLockButtons();
    syncTypeButtons();

    // Poll for external state changes every 200ms
    startTimer(200);
}

PatinaEditor::~PatinaEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ----------------------------------------------------------------
// Timer -- keeps lock + type button state in sync
// ----------------------------------------------------------------
void PatinaEditor::timerCallback()
{
    syncLockButtons();
    syncTypeButtons();
}

void PatinaEditor::updatePresetLabel()
{
    auto& pm = processorRef.getPresetManager();
    presetNameButton.setButtonText(pm.getDisplayName());
}

void PatinaEditor::syncLockButtons()
{
    auto& rs = processorRef.getRandomizeSystem();
    for (int i = 0; i < kNumModules; ++i)
    {
        const bool locked = rs.isLocked(kModuleIds[i]);
        if (moduleStrips[static_cast<size_t>(i)].lockButton.getToggleState() != locked)
            moduleStrips[static_cast<size_t>(i)].lockButton.setToggleState(locked, juce::dontSendNotification);
    }
}

void PatinaEditor::syncTypeButtons()
{
    auto& apvts = processorRef.getAPVTS();

    // Noise type
    {
        int val = static_cast<int>(apvts.getRawParameterValue("noise_type")->load());
        val = juce::jlimit(0, 2, val);
        noiseTypeButton.setButtonText(kNoiseTypeNames[val]);
    }

    // Wobble rate mode (sync)
    {
        int val = static_cast<int>(apvts.getRawParameterValue("wobble_rate_mode")->load());
        val = juce::jlimit(0, 8, val);
        wobbleSyncButton.setButtonText(kWobbleRateModeNames[val]);

        // Swap rate/shape knob based on mode:
        // FREE: show rate knob, hide shape knob
        // Synced: show shape knob, hide rate knob
        bool isFree = (val == 0);
        wobbleRate.knob.setVisible(isFree);
        wobbleRate.label.setVisible(isFree);
        wobbleShape.knob.setVisible(!isFree);
        wobbleShape.label.setVisible(!isFree);
    }

    // Distort type
    {
        int val = static_cast<int>(apvts.getRawParameterValue("distort_type")->load());
        val = juce::jlimit(0, 4, val);
        distortTypeButton.setButtonText(kDistortTypeNames[val]);
    }

    // Resonator type
    {
        int val = static_cast<int>(apvts.getRawParameterValue("resonator_type")->load());
        val = juce::jlimit(0, 2, val);
        resonatorTypeButton.setButtonText(kResonatorTypeNames[val]);
    }

    // Space type
    {
        int val = static_cast<int>(apvts.getRawParameterValue("space_type")->load());
        val = juce::jlimit(0, 3, val);
        spaceTypeButton.setButtonText(kSpaceTypeNames[val]);
    }
}

// ----------------------------------------------------------------
// Preset dropdown menu
// ----------------------------------------------------------------
void PatinaEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    auto& pm = processorRef.getPresetManager();

    // ---- Stock presets submenu ----
    juce::PopupMenu stockMenu;
    for (int i = 0; i < PresetManager::kNumPresets; ++i)
    {
        bool isCurrent = !pm.isUserPreset() && pm.getCurrentIndex() == i;
        stockMenu.addItem(100 + i, PresetManager::kPresets[i].name, true, isCurrent);
    }
    menu.addSubMenu("Stock Presets", stockMenu);

    // ---- User presets submenu ----
    auto userNames = pm.getUserPresetNames();
    juce::PopupMenu userMenu;

    if (userNames.isEmpty())
    {
        userMenu.addItem(-1, "(no saved presets)", false);
    }
    else
    {
        for (int i = 0; i < userNames.size(); ++i)
        {
            bool isCurrent = pm.isUserPreset() && pm.getCurrentUserPresetName() == userNames[i];

            // Main item for loading
            juce::PopupMenu itemMenu;
            itemMenu.addItem(500 + i, "Load");
            itemMenu.addItem(700 + i, "Delete");

            userMenu.addSubMenu(userNames[i], itemMenu, true, nullptr, isCurrent);
        }
    }

    userMenu.addSeparator();
    userMenu.addItem(900, "Save Current...");
    userMenu.addItem(901, "Open Presets Folder...");

    menu.addSubMenu("User Presets", userMenu);

    // Show the menu
    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetComponent(&presetNameButton)
        .withMinimumWidth(180),
        [this, userNames](int result)
        {
            if (result == 0) return; // dismissed

            auto& pm2  = processorRef.getPresetManager();
            auto& apvts2 = processorRef.getAPVTS();

            if (result >= 100 && result < 100 + PresetManager::kNumPresets)
            {
                // Load factory preset
                pm2.loadPreset(result - 100, apvts2);
                updatePresetLabel();
                syncTypeButtons();
                spectralBloom.resetVisualState();
            }
            else if (result >= 500 && result < 700)
            {
                // Load user preset
                int idx = result - 500;
                if (idx >= 0 && idx < userNames.size())
                {
                    pm2.loadUserPreset(userNames[idx], apvts2);
                    updatePresetLabel();
                    syncTypeButtons();
                    spectralBloom.resetVisualState();
                }
            }
            else if (result >= 700 && result < 900)
            {
                // Delete user preset
                int idx = result - 700;
                if (idx >= 0 && idx < userNames.size())
                {
                    auto nameToDelete = userNames[idx];

                    // Confirm deletion with async dialog
                    auto options = juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::WarningIcon)
                        .withTitle("Delete Preset")
                        .withMessage("Delete \"" + nameToDelete + "\"?\nThis cannot be undone.")
                        .withButton("Delete")
                        .withButton("Cancel")
                        .withAssociatedComponent(this);

                    juce::AlertWindow::showAsync(options,
                        [this, nameToDelete](int choice)
                        {
                            if (choice == 1)
                            {
                                processorRef.getPresetManager().deleteUserPreset(nameToDelete);
                                updatePresetLabel();
                            }
                        });
                }
            }
            else if (result == 900)
            {
                showSavePresetDialog();
            }
            else if (result == 901)
            {
                // Open the presets folder in the OS file manager
                PresetManager::getUserPresetsDir().startAsProcess();
            }
        });
}

// ----------------------------------------------------------------
// Save preset dialog
// ----------------------------------------------------------------
void PatinaEditor::showSavePresetDialog()
{
    auto& pm = processorRef.getPresetManager();

    // Pre-fill with current name if it's a user preset
    juce::String defaultName = pm.isUserPreset() ? pm.getCurrentUserPresetName() : "";

    auto* dialog = new juce::AlertWindow("Save Preset",
                                          "Enter a name for your preset:",
                                          juce::MessageBoxIconType::NoIcon,
                                          this);
    dialog->addTextEditor("presetName", defaultName, "Preset Name");
    dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    // Style the dialog
    if (auto* editor = dialog->getTextEditor("presetName"))
    {
        editor->setJustification(juce::Justification::centredLeft);
    }

    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [this, dialog](int result)
            {
                if (result == 1)
                {
                    auto name = dialog->getTextEditorContents("presetName").trim();

                    if (name.isNotEmpty())
                    {
                        // Sanitize: remove characters that are bad for filenames
                        name = name.replaceCharacters("\\/:*?\"<>|", "_________");

                        auto& pm2 = processorRef.getPresetManager();

                        // Check if it already exists
                        auto existingFile = PresetManager::getUserPresetsDir()
                                               .getChildFile(name + ".xml");

                        if (existingFile.existsAsFile())
                        {
                            auto opts = juce::MessageBoxOptions()
                                .withIconType(juce::MessageBoxIconType::WarningIcon)
                                .withTitle("Overwrite Preset")
                                .withMessage("A preset named \"" + name + "\" already exists.\nOverwrite it?")
                                .withButton("Overwrite")
                                .withButton("Cancel")
                                .withAssociatedComponent(this);

                            juce::AlertWindow::showAsync(opts,
                                [this, name](int choice)
                                {
                                    if (choice == 1)
                                    {
                                        processorRef.getPresetManager().saveUserPreset(
                                            name, processorRef.getAPVTS());
                                        updatePresetLabel();
                                    }
                                });
                        }
                        else
                        {
                            pm2.saveUserPreset(name, processorRef.getAPVTS());
                            updatePresetLabel();
                        }
                    }
                }

                delete dialog;
            }),
        true);
}

// ----------------------------------------------------------------
// Secondary knob helper
// ----------------------------------------------------------------
void PatinaEditor::setupSecondaryKnob(SecondaryKnob& sec,
                                       const juce::String& labelText,
                                       const juce::String& paramId,
                                       double defaultValue,
                                       const juce::String& tooltip)
{
    auto& apvts = processorRef.getAPVTS();

    sec.knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    sec.knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sec.knob.setNumDecimalPlacesToDisplay(2);
    sec.knob.setDoubleClickReturnValue(true, defaultValue);
    sec.knob.setTooltip(tooltip);
    addAndMakeVisible(sec.knob);

    sec.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, paramId, sec.knob);

    sec.label.setText(labelText, juce::dontSendNotification);
    sec.label.setJustificationType(juce::Justification::centred);
    sec.label.setFont(lookAndFeel.getPanchangFont(9.0f));
    sec.label.setColour(juce::Label::textColourId,
                        juce::Colour(PatinaLookAndFeel::colourWhite).withAlpha(0.55f));
    addAndMakeVisible(sec.label);
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
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setDoubleClickReturnValue(true, defaultValue);
    knob.setTooltip(tooltip);
    addAndMakeVisible(knob);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, paramId, knob);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(lookAndFeel.getPanchangFont(11.0f));
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

    // Purple gradient accent from left side
    {
        juce::ColourGradient headerGlow(
            juce::Colour(PatinaLookAndFeel::colourDeepPurple).withAlpha(0.30f),
            0.0f, static_cast<float>(kHeaderH) * 0.5f,
            juce::Colours::transparentBlack,
            static_cast<float>(kEditorWidth) * 0.4f, static_cast<float>(kHeaderH) * 0.5f,
            false);
        g.setGradientFill(headerGlow);
        g.fillRect(headerArea);
    }

    // Subtle bottom border on header
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.drawHorizontalLine(kHeaderH - 1, 0.0f, static_cast<float>(kEditorWidth));

    // ---- Branding (top left, text-rendered in Panchang Bold) ----
    {
        const float brandX = 14.0f;

        g.setColour(juce::Colour(PatinaLookAndFeel::colourWhite));
        g.setFont(lookAndFeel.getPanchangFont(22.0f));
        const int patinaY = (kHeaderH - 36) / 2;
        g.drawText("PATINA",
                   juce::Rectangle<int>(static_cast<int>(brandX), patinaY, 250, 26),
                   juce::Justification::centredLeft, false);

        g.setFont(lookAndFeel.getPanchangFont(8.0f));
        g.drawText("by FUTUREPROOF",
                   juce::Rectangle<int>(static_cast<int>(brandX), patinaY + 24, 250, 14),
                   juce::Justification::centredLeft, false);
    }

    // ---- Module strip background ----
    const auto stripArea = juce::Rectangle<int>(0, kHeaderH + kVisuH, kEditorWidth, kStripH);

    // Top divider
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.drawHorizontalLine(stripArea.getY(), 0.0f, static_cast<float>(kEditorWidth));

    // Module card panels (5 columns)
    {
        const int moduleW = kEditorWidth / kNumModules;
        const int cardInsetX = 4;
        const int cardInsetY = 8;
        const float cornerRadius = 8.0f;

        for (int i = 0; i < kNumModules; ++i)
        {
            const auto cardRect = juce::Rectangle<float>(
                static_cast<float>(i * moduleW + cardInsetX),
                static_cast<float>(stripArea.getY() + cardInsetY),
                static_cast<float>(moduleW - cardInsetX * 2),
                static_cast<float>(kStripH - cardInsetY * 2));

            // Card fill (very dark purple-black)
            g.setColour(juce::Colour(0xff0d0b14));
            g.fillRoundedRectangle(cardRect, cornerRadius);

            // Card border
            g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
            g.drawRoundedRectangle(cardRect, cornerRadius, 1.0f);
        }
    }

    // ---- Master bar background ----
    const auto masterArea = juce::Rectangle<int>(0, kHeaderH + kVisuH + kStripH, kEditorWidth, kMasterH);
    {
        juce::ColourGradient masterGrad(
            juce::Colour(0xff201a2e),
            0.0f, static_cast<float>(masterArea.getY()),
            juce::Colour(PatinaLookAndFeel::colourBackground),
            0.0f, static_cast<float>(masterArea.getBottom()),
            false);
        g.setGradientFill(masterGrad);
        g.fillRect(masterArea);
    }

    // Top divider
    g.setColour(juce::Colour(PatinaLookAndFeel::colourSubtle));
    g.drawHorizontalLine(masterArea.getY(), 0.0f, static_cast<float>(kEditorWidth));


}

// ----------------------------------------------------------------
// resized -- lay out all components
// ----------------------------------------------------------------
void PatinaEditor::resized()
{
    // ---- Visualizer ----
    spectralBloom.setBounds(0, kHeaderH, kEditorWidth, kVisuH);

    // ---- Header ----
    const int btnW = 120;
    const int btnH = 30;
    const int btnX = kEditorWidth - btnW - 20;
    const int btnY = (kHeaderH - btnH) / 2;
    randomizeButton.setBounds(btnX, btnY, btnW, btnH);

    // INIT button: to the left of RANDOMIZE
    const int initW = 70;
    const int initX = btnX - initW - 10;
    initButton.setBounds(initX, btnY, initW, btnH);

    // Preset selector: centered in the available space
    // Layout: [SAVE] [<] [  preset name  ] [>]
    const int navW     = 28;
    const int saveW    = 50;
    const int nameW    = 180;
    const int gap      = 4;
    const int selectorW = saveW + gap + navW + gap + nameW + gap + navW;
    const int selectorX = (kEditorWidth - selectorW) / 2;
    const int navY     = (kHeaderH - btnH) / 2;

    int cx = selectorX;
    presetSaveButton.setBounds(cx, navY, saveW, btnH);
    cx += saveW + gap;
    presetPrevButton.setBounds(cx, navY, navW, btnH);
    cx += navW + gap;
    presetNameButton.setBounds(cx, navY, nameW, btnH);
    cx += nameW + gap;
    presetNextButton.setBounds(cx, navY, navW, btnH);

    // ---- Module strip (5 columns, 180px each) ----
    const int stripTop = kHeaderH + kVisuH;
    const int moduleW  = kEditorWidth / kNumModules;   // 180

    for (int i = 0; i < kNumModules; ++i)
    {
        auto& strip = moduleStrips[static_cast<size_t>(i)];
        const int colX = i * moduleW;

        // Label at top
        strip.label.setBounds(colX, stripTop + 10, moduleW, 16);

        // Primary knob: 100x100, centered
        const int knobSize = 100;
        const int knobX = colX + (moduleW - knobSize) / 2;
        const int knobY = stripTop + 30;
        strip.knob.setBounds(knobX, knobY, knobSize, knobSize);

        // Lock button: near bottom (above card's rounded corner)
        const int lockW = 28;
        const int lockH = 22;
        const int lockX = colX + (moduleW - lockW) / 2;
        const int lockY = stripTop + 242;
        strip.lockButton.setBounds(lockX, lockY, lockW, lockH);
    }

    // ---- Type selector buttons (all 5 columns) ----
    {
        const int typeW = 100;
        const int typeH = 22;
        const int typeY = stripTop + 136;

        // Noise type -- column 0
        {
            const int colX = 0 * moduleW;
            noiseTypeButton.setBounds(colX + (moduleW - typeW) / 2, typeY, typeW, typeH);
        }

        // Wobble sync -- column 1
        {
            const int colX = 1 * moduleW;
            wobbleSyncButton.setBounds(colX + (moduleW - typeW) / 2, typeY, typeW, typeH);
        }

        // Distort type -- column 2
        {
            const int colX = 2 * moduleW;
            distortTypeButton.setBounds(colX + (moduleW - typeW) / 2, typeY, typeW, typeH);
        }

        // Resonator type -- column 3
        {
            const int colX = 3 * moduleW;
            resonatorTypeButton.setBounds(colX + (moduleW - typeW) / 2, typeY, typeW, typeH);
        }

        // Space type -- column 4
        {
            const int colX = 4 * moduleW;
            spaceTypeButton.setBounds(colX + (moduleW - typeW) / 2, typeY, typeW, typeH);
        }
    }

    // ---- Secondary knobs ----
    {
        const int secKnobSize = 44;
        const int secLabelH   = 14;
        const int secY        = stripTop + 164;
        const int labelY      = secY + secKnobSize + 2;

        // Column 0 (Noise) -- TONE
        {
            const int colX = 0 * moduleW;
            const int knobX = colX + (moduleW - secKnobSize) / 2;
            noiseTone.knob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            noiseTone.label.setBounds(colX, labelY, moduleW, secLabelH);
        }

        // Column 1 (Wobble) -- RATE or SHAPE (same position, swapped by visibility)
        {
            const int colX = 1 * moduleW;
            const int knobX = colX + (moduleW - secKnobSize) / 2;
            wobbleRate.knob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            wobbleRate.label.setBounds(colX, labelY, moduleW, secLabelH);
            wobbleShape.knob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            wobbleShape.label.setBounds(colX, labelY, moduleW, secLabelH);
        }

        // Column 2 (Distort) -- TONE
        {
            const int colX = 2 * moduleW;
            const int knobX = colX + (moduleW - secKnobSize) / 2;
            distortTone.knob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            distortTone.label.setBounds(colX, labelY, moduleW, secLabelH);
        }

        // Column 3 (Resonator) -- FREQ + RESO side by side
        {
            const int colX = 3 * moduleW;
            const int dualKnobSize = 42;
            const int dualgap = 4;
            const int totalW = dualKnobSize * 2 + dualgap;  // 88
            const int startX = colX + (moduleW - totalW) / 2;

            resonatorFreq.knob.setBounds(startX, secY, dualKnobSize, dualKnobSize);
            resonatorReso.knob.setBounds(startX + dualKnobSize + dualgap, secY, dualKnobSize, dualKnobSize);

            // Labels centered under each knob
            const int lblW = 50;
            resonatorFreq.label.setBounds(startX + dualKnobSize / 2 - lblW / 2,
                                          labelY, lblW, secLabelH);
            resonatorReso.label.setBounds(startX + dualKnobSize + dualgap + dualKnobSize / 2 - lblW / 2,
                                          labelY, lblW, secLabelH);
        }

        // Column 4 (Space) -- DECAY
        {
            const int colX = 4 * moduleW;
            const int knobX = colX + (moduleW - secKnobSize) / 2;
            spaceDecay.knob.setBounds(knobX, secY, secKnobSize, secKnobSize);
            spaceDecay.label.setBounds(colX, labelY, moduleW, secLabelH);
        }
    }

    // ---- Master bar ----
    const int masterTop = kHeaderH + kVisuH + kStripH;
    const int masterKnobSize = 60;
    const int labelH         = 14;
    const int totalContent   = masterKnobSize + 2 + labelH;               // 76
    const int masterKnobY    = masterTop + (kMasterH - totalContent) / 2;  // centered

    // MIX: centered
    const int mixCX = kEditorWidth / 2;
    mixKnob.setBounds(mixCX - masterKnobSize / 2, masterKnobY, masterKnobSize, masterKnobSize);
    mixLabel.setBounds(mixCX - 40, masterKnobY + masterKnobSize + 2, 80, labelH);
}
