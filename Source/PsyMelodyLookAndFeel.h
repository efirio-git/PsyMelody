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

        if (orbitronBoldTf) titleFont = juce::Font(orbitronBoldTf).withHeight(22.0f);
        else titleFont = juce::Font(22.0f, juce::Font::bold);

        if (orbitronRegTf) sectionFont = juce::Font(orbitronRegTf).withHeight(10.0f);
        else sectionFont = juce::Font(10.0f, juce::Font::bold);

        if (interMedTf) uiFont = juce::Font(interMedTf).withHeight(13.0f);
        else uiFont = juce::Font(13.0f);

        if (interBoldTf) uiFontBold = juce::Font(interBoldTf).withHeight(13.0f);
        else uiFontBold = juce::Font(13.0f, juce::Font::bold);

        if (interRegTf) uiFontRegular = juce::Font(interRegTf).withHeight(12.0f);
        else uiFontRegular = juce::Font(12.0f);

        if (interRegTf)
            setDefaultSansSerifTypeface(interRegTf);

        // Color scheme - Neon Architect palette
        setColour(juce::ResizableWindow::backgroundColourId, surface);
        setColour(juce::ComboBox::backgroundColourId, surfaceLowest);
        setColour(juce::ComboBox::outlineColourId, outlineVariant.withAlpha(0.15f));
        setColour(juce::ComboBox::textColourId, primary);
        setColour(juce::ComboBox::arrowColourId, primary);
        setColour(juce::PopupMenu::backgroundColourId, surfaceContainerHigh);
        setColour(juce::PopupMenu::textColourId, onSurface);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, primary.withAlpha(0.2f));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::PopupMenu::headerTextColourId, primary);
        setColour(juce::TextButton::textColourOnId, onSurface);
        setColour(juce::TextButton::textColourOffId, onSurfaceVariant);
        setColour(juce::Label::textColourId, onSurfaceVariant);
        setColour(juce::ScrollBar::thumbColourId, primary.withAlpha(0.3f));
        setColour(juce::ScrollBar::trackColourId, juce::Colour(0x00000000));
    }

    // ============================================================
    // Rotary Slider (Knob) - Neon Architect style
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

        // Background circle - dark recessed
        g.setColour(surfaceLowest);
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // Arc track
        juce::Path arcTrack;
        arcTrack.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(surfaceContainerHigh);
        g.strokePath(arcTrack, juce::PathStrokeType(2.5f));

        // Active arc with glow
        if (sliderPos > 0.0f) {
            juce::Path arcActive;
            arcActive.addCentredArc(centreX, centreY, radius - 2.0f, radius - 2.0f,
                                     0.0f, rotaryStartAngle, angle, true);
            g.setColour(primary);
            g.strokePath(arcActive, juce::PathStrokeType(2.5f));

            // Glow
            g.setColour(primary.withAlpha(0.15f));
            juce::Path arcGlow;
            arcGlow.addCentredArc(centreX, centreY, radius - 1.0f, radius - 1.0f,
                                   0.0f, rotaryStartAngle, angle, true);
            g.strokePath(arcGlow, juce::PathStrokeType(6.0f));
        }

        // Pointer - laser-wire style
        auto pointerLen = radius * 0.65f;
        float tipX = centreX + std::sin(angle) * pointerLen;
        float tipY = centreY - std::cos(angle) * pointerLen;
        g.setColour(primaryFixed);
        g.drawLine(centreX, centreY, tipX, tipY, 1.5f);

        // Glow dot at tip
        g.setColour(primary.withAlpha(0.6f));
        g.fillEllipse(tipX - 2.5f, tipY - 2.5f, 5.0f, 5.0f);
    }

    // ============================================================
    // Linear Slider - Laser-wire style
    // ============================================================
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearHorizontal) {
            auto trackY = (float)y + (float)height * 0.5f;
            auto trackH = 2.0f;

            // Track background - recessed
            g.setColour(surfaceLowest);
            g.fillRect((float)x, trackY - trackH * 0.5f, (float)width, trackH);

            // Active fill with gradient glow
            float activeW = sliderPos - (float)x;
            if (activeW > 0) {
                juce::ColourGradient grad(primaryDim, (float)x, trackY,
                                           primary, sliderPos, trackY, false);
                g.setGradientFill(grad);
                g.fillRect((float)x, trackY - trackH * 0.5f, activeW, trackH);

                // Glow
                g.setColour(primary.withAlpha(0.2f));
                g.fillRect((float)x, trackY - 3.0f, activeW, 6.0f);
            }

            // Handle - 1px laser-wire
            g.setColour(primaryFixed);
            g.fillRect(sliderPos - 0.5f, trackY - 6.0f, 1.0f, 12.0f);
        } else if (style == juce::Slider::LinearBarVertical) {
            // Draggable number display (Stitch style)
            auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
            g.setColour(surfaceLowest);
            g.fillRect(bounds);

            // Split text into number and unit, draw with different colours/sizes
            auto fullText = slider.getTextFromValue(slider.getValue());
            auto spaceIdx = fullText.indexOfChar(' ');
            if (spaceIdx >= 0) {
                auto numPart = fullText.substring(0, spaceIdx);
                auto unitPart = fullText.substring(spaceIdx);
                auto numFont = uiFontBold.withHeight(16.0f);
                auto unitFont = uiFontRegular.withHeight(11.0f);
                float numW = numFont.getStringWidthFloat(numPart);
                float unitW = unitFont.getStringWidthFloat(unitPart);
                float startX = bounds.getX() + 6.0f;  // Left-aligned with padding
                // Number in cyan (large)
                g.setColour(primary);
                g.setFont(numFont);
                g.drawText(numPart, (int)startX, (int)bounds.getY(), (int)numW + 1, (int)bounds.getHeight(),
                           juce::Justification::centredLeft);
                // Unit in light grey (smaller)
                g.setColour(onSurfaceVariant);
                g.setFont(unitFont);
                g.drawText(unitPart, (int)(startX + numW + 1), (int)bounds.getY(), (int)unitW + 2, (int)bounds.getHeight(),
                           juce::Justification::centredLeft);
            } else {
                g.setColour(primary);
                g.setFont(uiFontBold.withHeight(16.0f));
                g.drawText(fullText, bounds.toNearestInt(), juce::Justification::centred);
            }
        } else {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
        }
    }

    // ============================================================
    // Buttons - Sharp corners (0px radius)
    // ============================================================
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto baseColour = backgroundColour;

        if (shouldDrawButtonAsDown)
            baseColour = baseColour.brighter(0.15f);
        else if (shouldDrawButtonAsHighlighted)
            baseColour = baseColour.brighter(0.08f);

        // Flat background, sharp corners
        g.setColour(baseColour);
        g.fillRect(bounds);

        // Cyan border for active subgenre pills; ghost border for other buttons
        auto textColour = button.findColour(juce::TextButton::textColourOffId);
        if (textColour == primary && button.getComponentID() == "subgenre_pill") {
            g.setColour(primary.withAlpha(0.6f));
            g.drawRect(bounds, 1.0f);
        } else if (backgroundColour.getAlpha() > 0 && backgroundColour != surfaceContainerLow) {
            g.setColour(outlineVariant.withAlpha(0.15f));
            g.drawRect(bounds, 1.0f);
        }

        // Cyan aura on hover
        if (shouldDrawButtonAsHighlighted) {
            g.setColour(primary.withAlpha(0.06f));
            g.fillRect(bounds.expanded(1.0f));
        }
    }

    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        return uiFontBold.withHeight(std::min(13.0f, (float)buttonHeight * 0.6f));
    }

    // ============================================================
    // ComboBox - Sharp corners, recessed
    // ============================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);

        // Recessed input field
        g.setColour(surfaceLowest);
        g.fillRect(bounds);

        // Ghost border on hover
        if (box.isMouseOver())
            g.setColour(primary.withAlpha(0.3f));
        else
            g.setColour(outlineVariant.withAlpha(0.0f));
        g.drawRect(bounds, 1.0f);

        // Arrow
        auto arrowArea = bounds.removeFromRight(20.0f).reduced(6.0f);
        juce::Path arrow;
        arrow.addTriangle(arrowArea.getX(), arrowArea.getCentreY() - 2.0f,
                          arrowArea.getRight(), arrowArea.getCentreY() - 2.0f,
                          arrowArea.getCentreX(), arrowArea.getCentreY() + 4.0f);
        g.setColour(primary.withAlpha(0.6f));
        g.fillPath(arrow);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return uiFont.withHeight(12.0f); }
    juce::Font getPopupMenuFont() override { return uiFontRegular.withHeight(13.0f); }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight, int& idealWidth, int& idealHeight) override
    {
        juce::LookAndFeel_V4::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight, idealWidth, idealHeight);
        if (!isSeparator)
            idealHeight = 28;
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(surfaceContainerHigh);
        g.setColour(primary.withAlpha(0.08f));
        g.drawRect(0, 0, width, height, 1);
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        g.setColour(primary.withAlpha(0.7f));
        g.setFont(sectionFont.withHeight(10.0f));
        g.drawText(sectionName.toUpperCase(), area.reduced(8, 0), juce::Justification::centredLeft);
    }

    // ============================================================
    // Toggle Button
    // ============================================================
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool /*shouldDrawButtonAsDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        bool isPreview = button.getComponentID() == "preview_toggle";
        float reduce = isPreview ? 7.0f : 4.0f;
        auto toggleArea = bounds.removeFromLeft(bounds.getHeight()).reduced(reduce);

        g.setColour(button.getToggleState() ? primary : surfaceContainerHigh);
        g.fillRect(toggleArea);

        if (button.getToggleState()) {
            g.setColour(juce::Colour(0xff005762));
            auto tick = toggleArea.reduced(isPreview ? 2.0f : 3.0f);
            juce::Path check;
            check.startNewSubPath(tick.getX(), tick.getCentreY());
            check.lineTo(tick.getCentreX() - 1, tick.getBottom() - 2);
            check.lineTo(tick.getRight(), tick.getY() + 2);
            g.strokePath(check, juce::PathStrokeType(isPreview ? 1.5f : 2.0f));
        }

        g.setColour(onSurfaceVariant);
        g.setFont(uiFont.withHeight(12.0f));
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
        auto thumbColour = primary.withAlpha(isMouseOver ? 0.4f : 0.2f);

        if (isScrollbarVertical) {
            g.setColour(thumbColour);
            g.fillRect((float)x + 1, (float)thumbStartPosition,
                       (float)width - 2, (float)thumbSize);
        } else {
            g.setColour(thumbColour);
            g.fillRect((float)thumbStartPosition, (float)y + 1,
                       (float)thumbSize, (float)height - 2);
        }
    }

    // ============================================================
    // Slider textbox
    // ============================================================
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox(slider);
        l->setColour(juce::Label::textColourId, primary);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setFont(uiFontRegular.withHeight(10.0f));
        return l;
    }

    // ============================================================
    // Helpers
    // ============================================================
    static void drawPanel(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        g.setColour(colour);
        g.fillRect(bounds);
    }

    void drawSectionHeader(juce::Graphics& g, int x, int y, int width,
                            const juce::String& text)
    {
        g.setColour(primary);
        g.setFont(sectionFont.withHeight(10.0f));
        g.drawText(text.toUpperCase(), x, y, width, 12, juce::Justification::centredLeft);
    }

    void drawParamCard(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour(surfaceContainerHigh);
        g.fillRect(bounds);
    }

    // ============================================================
    // Neon Architect Color Palette
    // ============================================================
    // Surface hierarchy
    juce::Colour surface             {0xff0e0e13};
    juce::Colour surfaceContainerLow {0xff131319};
    juce::Colour surfaceContainer    {0xff19191f};
    juce::Colour surfaceContainerHigh{0xff1f1f26};
    juce::Colour surfaceHighest      {0xff25252d};
    juce::Colour surfaceBright       {0xff2c2b33};
    juce::Colour surfaceLowest       {0xff000000};

    // Neon accent colors
    juce::Colour primary      {0xff81ecff};  // Cyan
    juce::Colour primaryDim   {0xff00d4ec};
    juce::Colour primaryFixed {0xff00e3fd};
    juce::Colour secondary    {0xffff59e3};  // Magenta
    juce::Colour secondaryDim {0xffad009b};
    juce::Colour tertiary     {0xffba84ff};  // Purple
    juce::Colour tertiaryDim  {0xff993fff};

    // Text / on-surface
    juce::Colour onSurface        {0xfff9f5fd};
    juce::Colour onSurfaceVariant {0xffacaab1};
    juce::Colour outline          {0xff76747b};
    juce::Colour outlineVariant   {0xff48474d};

    // Error
    juce::Colour error {0xffff716c};

    // Legacy aliases for compatibility
    juce::Colour bg       = surface;
    juce::Colour panelBg  = surfaceContainerLow;
    juce::Colour border   = outlineVariant;
    juce::Colour accent   = primary;
    juce::Colour accent2  = tertiary;
    juce::Colour textLight = onSurface;
    juce::Colour textDim  = onSurfaceVariant;
    juce::Colour knobBg   = surfaceLowest;

    // Typeface storage
    juce::Typeface::Ptr orbitronBoldTf, orbitronRegTf;
    juce::Typeface::Ptr interRegTf, interMedTf, interBoldTf;
};
