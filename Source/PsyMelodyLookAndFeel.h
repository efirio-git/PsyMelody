#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

class PsyMelodyLookAndFeel : public juce::LookAndFeel_V4 {
public:
    // Custom fonts
    juce::Font titleFont;       // Orbitron Bold - for "PsyMelody"
    juce::Font sectionFont;     // Orbitron Regular - for section headers
    juce::Font uiFont;          // Inter Medium - for labels, buttons
    juce::Font uiFontBold;      // Inter Bold - for emphasis
    juce::Font uiFontRegular;   // Inter Regular - for values

    PsyMelodyLookAndFeel()
    {
        // Load custom fonts from BinaryData
        orbitronBoldTf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::OrbitronBold_ttf, BinaryData::OrbitronBold_ttfSize);
        orbitronRegTf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::OrbitronRegular_ttf, BinaryData::OrbitronRegular_ttfSize);
        interRegTf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
        interMedTf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::InterMedium_ttf, BinaryData::InterMedium_ttfSize);
        interBoldTf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);

        if (orbitronBoldTf) titleFont = juce::Font(orbitronBoldTf).withHeight(24.0f);
        else titleFont = juce::Font(24.0f, juce::Font::bold);

        if (orbitronRegTf) sectionFont = juce::Font(orbitronRegTf).withHeight(11.0f);
        else sectionFont = juce::Font(11.0f, juce::Font::bold);

        if (interMedTf) uiFont = juce::Font(interMedTf).withHeight(14.0f);
        else uiFont = juce::Font(14.0f);

        if (interBoldTf) uiFontBold = juce::Font(interBoldTf).withHeight(14.0f);
        else uiFontBold = juce::Font(14.0f, juce::Font::bold);

        if (interRegTf) uiFontRegular = juce::Font(interRegTf).withHeight(13.0f);
        else uiFontRegular = juce::Font(13.0f);

        // Set Inter as default typeface for the whole LookAndFeel
        if (interRegTf)
            setDefaultSansSerifTypeface(interRegTf);
        // Color scheme
        setColour(juce::ResizableWindow::backgroundColourId, bg);
        setColour(juce::ComboBox::backgroundColourId, panelBg);
        setColour(juce::ComboBox::outlineColourId, border);
        setColour(juce::ComboBox::textColourId, textLight);
        setColour(juce::ComboBox::arrowColourId, accent);
        setColour(juce::PopupMenu::backgroundColourId, panelBg);
        setColour(juce::PopupMenu::textColourId, textLight);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.3f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::PopupMenu::headerTextColourId, accent);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::TextButton::textColourOffId, textLight);
        setColour(juce::Label::textColourId, textDim);
        setColour(juce::ScrollBar::thumbColourId, accent.withAlpha(0.4f));
        setColour(juce::ScrollBar::trackColourId, juce::Colour(0x00000000));
    }

    // ============================================================
    // Rotary Slider (Knob)
    // ============================================================
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 4.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Background circle
        g.setColour(knobBg);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // Arc track
        juce::Path arcTrack;
        arcTrack.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(border);
        g.strokePath(arcTrack, juce::PathStrokeType(2.5f));

        // Active arc
        if (sliderPos > 0.0f) {
            juce::Path arcActive;
            arcActive.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                     0.0f, rotaryStartAngle, angle, true);
            g.setColour(accent);
            g.strokePath(arcActive, juce::PathStrokeType(2.5f));
        }

        // Pointer line
        juce::Path pointer;
        auto pointerLen = radius * 0.6f;
        pointer.addRoundedRectangle(-1.5f, -pointerLen, 3.0f, pointerLen, 1.5f);
        g.setColour(juce::Colours::white);
        g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        // Glow on the tip
        float tipX = centreX + std::sin(angle) * (radius - 6.0f);
        float tipY = centreY - std::cos(angle) * (radius - 6.0f);
        g.setColour(accent.withAlpha(0.5f));
        g.fillEllipse(tipX - 3.0f, tipY - 3.0f, 6.0f, 6.0f);
    }

    // ============================================================
    // Linear Slider
    // ============================================================
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal) {
            auto trackY = (float)y + (float)height * 0.5f;
            auto trackH = 4.0f;

            // Track background
            g.setColour(border);
            g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, (float)width, trackH, 2.0f);

            // Active portion
            float activeW = sliderPos - (float)x;
            if (activeW > 0) {
                g.setColour(accent);
                g.fillRoundedRectangle((float)x, trackY - trackH * 0.5f, activeW, trackH, 2.0f);
            }

            // Thumb
            float thumbSize = 12.0f;
            g.setColour(juce::Colours::white);
            g.fillEllipse(sliderPos - thumbSize * 0.5f, trackY - thumbSize * 0.5f, thumbSize, thumbSize);
            g.setColour(accent.withAlpha(0.3f));
            g.fillEllipse(sliderPos - thumbSize, trackY - thumbSize, thumbSize * 2.0f, thumbSize * 2.0f);
        } else {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
        }
    }

    // ============================================================
    // Buttons
    // ============================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.brighter(0.2f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.1f);

        // Gradient background
        g.setGradientFill(juce::ColourGradient(
            baseColour.brighter(0.1f), bounds.getX(), bounds.getY(),
            baseColour.darker(0.1f), bounds.getX(), bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, 5.0f);

        // Subtle border
        g.setColour(baseColour.brighter(0.2f).withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        // Glow effect when highlighted
        if (shouldDrawButtonAsHighlighted) {
            g.setColour(accent.withAlpha(0.1f));
            g.fillRoundedRectangle(bounds.expanded(1.0f), 6.0f);
        }
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return uiFont.withHeight(std::min(15.0f, (float)buttonHeight * 0.65f));
    }

    // ============================================================
    // ComboBox
    // ============================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);

        g.setColour(panelBg);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(border);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        // Arrow
        auto arrowArea = bounds.removeFromRight(20.0f).reduced(6.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowArea.getX(), arrowArea.getCentreY() - 2.0f,
                          arrowArea.getRight(), arrowArea.getCentreY() - 2.0f,
                          arrowArea.getCentreX(), arrowArea.getCentreY() + 4.0f);
        g.setColour(accent);
        g.fillPath(arrow);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return uiFont.withHeight(14.0f); }
    juce::Font getPopupMenuFont() override { return uiFontRegular.withHeight(14.0f); }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(panelBg);
        g.setColour(border);
        g.drawRect(0, 0, width, height);
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        g.setColour(accent.withAlpha(0.7f));
        g.setFont(uiFontBold.withHeight(13.0f));
        g.drawText(sectionName, area.reduced(8, 0), juce::Justification::centredLeft);
        g.setColour(border);
        g.drawLine((float)area.getX() + 8, (float)area.getBottom() - 1,
                   (float)area.getRight() - 8, (float)area.getBottom() - 1, 0.5f);
    }

    // ============================================================
    // Toggle Button
    // ============================================================
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto toggleArea = bounds.removeFromLeft(bounds.getHeight()).reduced(4.0f);

        // Toggle background
        g.setColour(button.getToggleState() ? accent : border);
        g.fillRoundedRectangle(toggleArea, 3.0f);

        // Check mark
        if (button.getToggleState()) {
            g.setColour(juce::Colours::white);
            auto tick = toggleArea.reduced(3.0f);
            juce::Path check;
            check.startNewSubPath(tick.getX(), tick.getCentreY());
            check.lineTo(tick.getCentreX() - 1, tick.getBottom() - 2);
            check.lineTo(tick.getRight(), tick.getY() + 2);
            g.strokePath(check, juce::PathStrokeType(2.0f));
        }

        // Label
        g.setColour(textLight);
        g.setFont(uiFont.withHeight(13.0f));
        g.drawText(button.getButtonText(), bounds.withTrimmedLeft(4), juce::Justification::centredLeft);
    }

    // ============================================================
    // Label
    // ============================================================
    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(getLabelFont(label));
        g.drawText(label.getText(), label.getLocalBounds(),
                   label.getJustificationType(), true);
    }

    // ============================================================
    // ScrollBar
    // ============================================================
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                        int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool /*isMouseDown*/) override
    {
        auto thumbColour = accent.withAlpha(isMouseOver ? 0.5f : 0.3f);

        if (isScrollbarVertical) {
            g.setColour(thumbColour);
            g.fillRoundedRectangle((float)x + 1, (float)thumbStartPosition,
                                    (float)width - 2, (float)thumbSize, 3.0f);
        } else {
            g.setColour(thumbColour);
            g.fillRoundedRectangle((float)thumbStartPosition, (float)y + 1,
                                    (float)thumbSize, (float)height - 2, 3.0f);
        }
    }

    // ============================================================
    // Slider textbox
    // ============================================================
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox(slider);
        l->setColour(juce::Label::textColourId, textLight);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setFont(uiFontRegular.withHeight(12.0f));
        return l;
    }

    // ============================================================
    // Helper: Draw panel background
    // ============================================================
    static void drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds,
                           float cornerSize = 6.0f)
    {
        g.setColour(juce::Colour(0xff161630));
        g.fillRoundedRectangle(bounds, cornerSize);
        g.setColour(juce::Colour(0xff2a2a50).withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }

    void drawSectionHeader(juce::Graphics& g, int x, int y, int width,
                            const juce::String& text)
    {
        g.setColour(juce::Colour(0xff00dda0));
        g.setFont(sectionFont);
        g.drawText(text, x, y, width, 14, juce::Justification::centredLeft);
        // Gradient line
        juce::ColourGradient grad(juce::Colour(0xff00dda0).withAlpha(0.6f), (float)x, 0,
                                   juce::Colour(0xff00dda0).withAlpha(0.0f), (float)(x + width), 0, false);
        g.setGradientFill(grad);
        g.fillRect(x, y + 14, width, 1);
    }

    // Color palette
    juce::Colour bg       {0xff0e0e20};
    juce::Colour panelBg  {0xff18183a};
    juce::Colour border   {0xff2a2a55};
    juce::Colour accent   {0xff00dda0};  // Teal/cyan
    juce::Colour accent2  {0xff7744ff};  // Purple
    juce::Colour textLight{0xffe0e0e8};
    juce::Colour textDim  {0xff8888aa};
    juce::Colour knobBg   {0xff1e1e40};

    // Typeface storage
    juce::Typeface::Ptr orbitronBoldTf, orbitronRegTf;
    juce::Typeface::Ptr interRegTf, interMedTf, interBoldTf;
};
