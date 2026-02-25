#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// ============================================================
// PatinaLookAndFeel
//
// Brand-consistent L&F for the Patina plugin.
// Colours:
//   Background  #000000
//   Purple      #a373f8
//   Deep purple #5d2da8
//   White       #ffffff
//   Subtle      #ffffff26
//
// Knob style: black fill, purple arc, small indicator dot
// Lock button: circular toggle — grey when unlocked, purple when locked
// Randomize button: pill-shaped, glows purple
// ============================================================

class PatinaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ---- Brand colours ----
    static constexpr uint32_t colourBackground = 0xff000000;
    static constexpr uint32_t colourPurple     = 0xffa373f8;
    static constexpr uint32_t colourDeepPurple = 0xff5d2da8;
    static constexpr uint32_t colourWhite      = 0xffffffff;
    static constexpr uint32_t colourSubtle     = 0x26ffffff;
    static constexpr uint32_t colourDark       = 0xff1c1429;

    PatinaLookAndFeel();

    // Rotary knob — black fill, purple arc, dot indicator
    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    // Text label colours
    juce::Label* createSliderTextBox(juce::Slider& slider) override;

    // Button — handles both lock toggle and RANDOMIZE
    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // Combo box (preset selector)
    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive,
                           bool isHighlighted, bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;

private:
    juce::Font getModuleFont(float height);
};
