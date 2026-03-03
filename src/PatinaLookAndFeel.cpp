#include "PatinaLookAndFeel.h"

PatinaLookAndFeel::PatinaLookAndFeel()
{
    // Load Panchang Bold from embedded binary data
    panchangTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::PanchangBold_otf,
        BinaryData::PanchangBold_otfSize);

    // Global colour overrides used by JUCE internals
    setColour(juce::Slider::thumbColourId,              juce::Colour(colourPurple));
    setColour(juce::Slider::rotarySliderFillColourId,   juce::Colour(colourPurple));
    setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(colourSubtle));
    setColour(juce::Slider::textBoxTextColourId,        juce::Colour(colourWhite));
    setColour(juce::Slider::textBoxBackgroundColourId,  juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);

    setColour(juce::Label::textColourId,                juce::Colour(colourWhite));
    setColour(juce::Label::backgroundColourId,          juce::Colours::transparentBlack);

    setColour(juce::TextButton::buttonColourId,         juce::Colour(colourDark));
    setColour(juce::TextButton::buttonOnColourId,       juce::Colour(colourPurple));
    setColour(juce::TextButton::textColourOffId,        juce::Colour(colourWhite));
    setColour(juce::TextButton::textColourOnId,         juce::Colour(colourWhite));

    setColour(juce::ComboBox::backgroundColourId,       juce::Colour(colourDark));
    setColour(juce::ComboBox::outlineColourId,          juce::Colour(colourSubtle));
    setColour(juce::ComboBox::textColourId,             juce::Colour(colourWhite));
    setColour(juce::ComboBox::arrowColourId,            juce::Colour(colourPurple));

    setColour(juce::PopupMenu::backgroundColourId,      juce::Colour(colourDark));
    setColour(juce::PopupMenu::textColourId,            juce::Colour(colourWhite));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(colourDeepPurple));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(colourWhite));
}

// ---------------------------------------------------------------
// Rotary knob
// ---------------------------------------------------------------
void PatinaLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider& /*slider*/)
{
    const float cx = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
    const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float radius = (juce::jmin(width, height) * 0.5f) - 4.0f;

    if (radius <= 0.0f) return;

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // ---- Active glow (purple halo when knob value > 0) ----
    if (sliderPos > 0.01f)
    {
        const float glowIntensity = sliderPos * 0.12f;
        for (int ring = 3; ring >= 1; --ring)
        {
            const float rr = radius + static_cast<float>(ring) * 2.5f;
            const float alpha = glowIntensity / static_cast<float>(ring);
            g.setColour(juce::Colour(colourPurple).withAlpha(alpha));
            g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 2.0f);
        }
    }

    // ---- Background circle (radial gradient for concave 3D look) ----
    {
        juce::ColourGradient bgGrad(juce::Colour(0xff1e1e1e), cx, cy,
                                     juce::Colour(0xff080808), cx, cy - radius, true);
        g.setGradientFill(bgGrad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    // ---- Outer ring (subtle border) ----
    g.setColour(juce::Colour(colourSubtle));
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // ---- Arc track (background) ----
    const float arcThickness = radius * 0.18f;
    const float arcRadius = radius - arcThickness * 0.5f - 2.0f;

    {
        juce::Path trackArc;
        trackArc.addArc(cx - arcRadius, cy - arcRadius,
                        arcRadius * 2.0f, arcRadius * 2.0f,
                        rotaryStartAngle, rotaryEndAngle, true);

        juce::PathStrokeType stroke(arcThickness,
                                    juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded);

        g.setColour(juce::Colour(0xff2a2a2a));
        g.strokePath(trackArc, stroke);
    }

    // ---- Purple filled arc (gradient: deep purple at start -> bright purple at tip) ----
    if (sliderPos > 0.001f)
    {
        juce::Path filledArc;
        filledArc.addArc(cx - arcRadius, cy - arcRadius,
                         arcRadius * 2.0f, arcRadius * 2.0f,
                         rotaryStartAngle, angle, true);

        juce::PathStrokeType stroke(arcThickness,
                                    juce::PathStrokeType::curved,
                                    juce::PathStrokeType::butt);

        // Gradient along the arc: deep purple at start, bright purple at current position
        const float startX = cx + std::sin(rotaryStartAngle) * arcRadius;
        const float startY = cy - std::cos(rotaryStartAngle) * arcRadius;
        const float endX   = cx + std::sin(angle) * arcRadius;
        const float endY   = cy - std::cos(angle) * arcRadius;

        juce::ColourGradient arcGrad(juce::Colour(colourDeepPurple), startX, startY,
                                      juce::Colour(colourPurple), endX, endY, false);
        g.setGradientFill(arcGrad);
        g.strokePath(filledArc, stroke);

        // Subtle glow behind arc
        juce::PathStrokeType glowStroke(arcThickness + 3.0f,
                                        juce::PathStrokeType::curved,
                                        juce::PathStrokeType::butt);

        g.setColour(juce::Colour(colourPurple).withAlpha(0.25f));
        g.strokePath(filledArc, glowStroke);
    }

    // ---- Indicator dot ----
    const float dotRadius = arcThickness * 0.5f;
    const float dotX = cx + std::sin(angle) * arcRadius;
    const float dotY = cy - std::cos(angle) * arcRadius;

    // Purple glow behind the dot
    g.setColour(juce::Colour(colourPurple).withAlpha(0.4f));
    g.fillEllipse(dotX - dotRadius * 1.8f, dotY - dotRadius * 1.8f,
                  dotRadius * 3.6f, dotRadius * 3.6f);

    // White dot
    g.setColour(juce::Colour(colourWhite));
    g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

// ---------------------------------------------------------------
// Slider text box — transparent, white text, small font
// ---------------------------------------------------------------
juce::Label* PatinaLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setFont(getPanchangFont(10.0f));
    label->setColour(juce::Label::textColourId, juce::Colour(colourWhite).withAlpha(0.7f));
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId,    juce::Colours::transparentBlack);
    return label;
}

// ---------------------------------------------------------------
// Button background
// Two modes:
//   1. Lock button (small square/circle) — grey outline / purple fill when on
//   2. RANDOMIZE button — pill shape, always purple-tinted
// ---------------------------------------------------------------
void PatinaLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                              juce::Button& button,
                                              const juce::Colour& /*backgroundColour*/,
                                              bool shouldDrawButtonAsHighlighted,
                                              bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const float cornerRadius = bounds.getHeight() * 0.5f; // Pill shape for all buttons

    const bool isToggled      = button.getToggleState();
    const bool isRandomize    = (button.getName() == "randomize");
    const bool isPresetNav    = (button.getName() == "preset_nav");
    const bool isTypeSelector = (button.getName() == "type_selector");

    juce::Colour bgColour;
    juce::Colour borderColour;

    if (isRandomize)
    {
        bgColour     = shouldDrawButtonAsDown
                           ? juce::Colour(colourDeepPurple)
                           : (shouldDrawButtonAsHighlighted
                              ? juce::Colour(colourPurple).brighter(0.15f)
                              : juce::Colour(colourPurple));
        borderColour = juce::Colours::transparentBlack;
    }
    else if (isPresetNav)
    {
        // Subtle dark pill with border -- highlights purple on hover
        bgColour     = shouldDrawButtonAsDown
                           ? juce::Colour(colourDark).brighter(0.12f)
                           : (shouldDrawButtonAsHighlighted
                              ? juce::Colour(colourDark).brighter(0.06f)
                              : juce::Colours::transparentBlack);
        borderColour = shouldDrawButtonAsHighlighted
                           ? juce::Colour(colourPurple).withAlpha(0.55f)
                           : juce::Colour(colourSubtle);
    }
    else if (isTypeSelector)
    {
        // Dark filled pill -- click to cycle through types
        bgColour     = shouldDrawButtonAsDown
                           ? juce::Colour(colourDark).brighter(0.15f)
                           : (shouldDrawButtonAsHighlighted
                              ? juce::Colour(colourDark).brighter(0.08f)
                              : juce::Colour(colourDark));
        borderColour = shouldDrawButtonAsHighlighted
                           ? juce::Colour(colourPurple).withAlpha(0.55f)
                           : juce::Colour(colourSubtle);
    }
    else
    {
        // Lock button
        bgColour     = isToggled ? juce::Colour(colourPurple).withAlpha(0.25f)
                                 : juce::Colours::transparentBlack;
        borderColour = isToggled ? juce::Colour(colourPurple)
                                 : juce::Colour(colourSubtle);

        if (shouldDrawButtonAsDown)
            bgColour = bgColour.brighter(0.1f);
        else if (shouldDrawButtonAsHighlighted && !isToggled)
            borderColour = juce::Colour(colourWhite).withAlpha(0.4f);
    }

    g.setColour(bgColour);
    g.fillRoundedRectangle(bounds.reduced(0.5f), cornerRadius);

    g.setColour(borderColour);
    g.drawRoundedRectangle(bounds.reduced(0.75f), cornerRadius, 1.5f);

    // Glow effect for randomize button
    if (isRandomize && !shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colour(colourPurple).withAlpha(0.2f));
        g.drawRoundedRectangle(bounds.expanded(2.0f), cornerRadius + 2.0f, 3.0f);
    }
}

void PatinaLookAndFeel::drawButtonText(juce::Graphics& g,
                                        juce::TextButton& button,
                                        bool /*shouldDrawButtonAsHighlighted*/,
                                        bool /*shouldDrawButtonAsDown*/)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isLock = (button.getName() == "lock");

    if (isLock)
    {
        // Path-drawn padlock icon (no emoji dependency)
        const bool locked = button.getToggleState();
        auto colour = locked ? juce::Colour(colourPurple)
                             : juce::Colour(colourWhite).withAlpha(0.4f);
        g.setColour(colour);

        const float s = juce::jmin(bounds.getWidth(), bounds.getHeight());
        const float iconSize = s * 0.6f;
        const float iconCX = bounds.getCentreX();
        const float iconCY = bounds.getCentreY();

        // Body: filled rounded rect (lower portion)
        const float bodyW = iconSize * 0.65f;
        const float bodyH = iconSize * 0.45f;
        const float bodyTop = iconCY + iconSize * 0.05f;
        g.fillRoundedRectangle(iconCX - bodyW * 0.5f, bodyTop, bodyW, bodyH, 1.5f);

        // Keyhole: small dark dot on the body
        {
            auto holeCol = locked ? juce::Colour(colourDark) : juce::Colour(colourBackground);
            g.setColour(holeCol);
            const float hr = iconSize * 0.05f;
            g.fillEllipse(iconCX - hr, bodyTop + bodyH * 0.38f - hr, hr * 2.0f, hr * 2.0f);
            g.setColour(colour);
        }

        // Shackle: U-shape (two vertical legs + semicircle over top)
        const float shackleR = bodyW * 0.30f;
        const float shackleStroke = 1.8f;

        juce::Path shackle;
        shackle.startNewSubPath(iconCX - shackleR, bodyTop + 1.0f);
        shackle.lineTo(iconCX - shackleR, bodyTop - shackleR * 0.4f);
        shackle.addArc(iconCX - shackleR, bodyTop - shackleR * 1.4f,
                       shackleR * 2.0f, shackleR * 2.0f,
                       -juce::MathConstants<float>::pi, 0.0f, false);

        if (locked)
            shackle.lineTo(iconCX + shackleR, bodyTop + 1.0f);
        else
            shackle.lineTo(iconCX + shackleR, bodyTop - shackleR * 0.7f);

        g.strokePath(shackle, juce::PathStrokeType(shackleStroke,
                     juce::PathStrokeType::curved,
                     juce::PathStrokeType::rounded));
    }
    else
    {
        // RANDOMIZE or any other button — Panchang Bold caps
        g.setColour(juce::Colour(colourWhite));
        g.setFont(getPanchangFont(bounds.getHeight() * 0.38f));
        g.drawText(button.getButtonText(),
                   button.getLocalBounds(), juce::Justification::centred, false);
    }
}

// ---------------------------------------------------------------
// ComboBox
// ---------------------------------------------------------------
void PatinaLookAndFeel::drawComboBox(juce::Graphics& g,
                                      int width, int height,
                                      bool /*isButtonDown*/,
                                      int /*buttonX*/, int /*buttonY*/,
                                      int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& /*box*/)
{
    const auto bounds = juce::Rectangle<float>(0, 0,
                                               static_cast<float>(width),
                                               static_cast<float>(height));
    const float corner = static_cast<float>(height) * 0.3f;

    g.setColour(juce::Colour(colourDark));
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(juce::Colour(colourSubtle));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

    // Arrow
    const float arrowX = static_cast<float>(width) - 18.0f;
    const float arrowY = static_cast<float>(height) * 0.5f;
    juce::Path arrow;
    arrow.addTriangle(arrowX, arrowY - 3.0f,
                      arrowX + 6.0f, arrowY - 3.0f,
                      arrowX + 3.0f, arrowY + 3.0f);
    g.setColour(juce::Colour(colourPurple));
    g.fillPath(arrow);
}

void PatinaLookAndFeel::drawPopupMenuItem(juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator,
                                           bool /*isActive*/,
                                           bool isHighlighted,
                                           bool /*isTicked*/,
                                           bool /*hasSubMenu*/,
                                           const juce::String& text,
                                           const juce::String& /*shortcutKeyText*/,
                                           const juce::Drawable* /*icon*/,
                                           const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour(juce::Colour(colourSubtle));
        g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX() + 5),
                             static_cast<float>(area.getRight() - 5));
        return;
    }

    if (isHighlighted)
    {
        g.setColour(juce::Colour(colourDeepPurple));
        g.fillRect(area);
    }

    g.setColour(juce::Colour(colourWhite));
    g.setFont(getPanchangFont(12.0f));
    g.drawText(text, area.reduced(8, 0), juce::Justification::centredLeft, true);
}

juce::Font PatinaLookAndFeel::getPanchangFont(float height) const
{
    if (panchangTypeface != nullptr)
        return juce::Font(juce::FontOptions(panchangTypeface)).withHeight(height);

    return juce::Font(juce::FontOptions(height));
}
