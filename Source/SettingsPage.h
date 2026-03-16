#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Localization.h"

namespace PsyMelody {

class JapaneseLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void setJapaneseFont(const juce::Font& f) { jaFont = f; }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return jaFont; }
private:
    juce::Font jaFont{13.0f};
};

// Inner component that draws the manual/about text (placed inside a Viewport)
class SettingsContent : public juce::Component {
public:
    juce::String text;
    juce::String title;
    juce::Font textFont{11.5f};
    juce::Font titleFont{15.0f, juce::Font::bold};
    bool isManual = true;

    void updateHeight()
    {
        if (text.isEmpty()) { setSize(getWidth(), 100); return; }
        // Estimate height needed
        int lineH = isManual ? 14 : 16;
        int numLines = 1;
        for (int i = 0; i < text.length(); ++i)
            if (text[i] == '\n') numLines++;
        int h = numLines * lineH + 60;
        setSize(getWidth(), std::max(h, 100));
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(12, 8);

        if (isManual && title.isNotEmpty()) {
            g.setColour(juce::Colour(0xff00cc88));
            g.setFont(titleFont);
            g.drawText(title, area.removeFromTop(22), juce::Justification::centredLeft);
            area.removeFromTop(6);
        }

        g.setColour(juce::Colour(0xffcccccc));
        g.setFont(textFont);
        g.drawFittedText(text, area, juce::Justification::topLeft, 200);
    }
};

class SettingsPage : public juce::Component {
public:
    enum class Tab { Manual, About };

    SettingsPage()
    {
        langSelector.addItem("English", 1);
        langSelector.addItem("Japanese", 2);
        langSelector.setSelectedId(1, juce::dontSendNotification);
        langSelector.onChange = [this] {
            currentLang = langSelector.getSelectedId() == 2 ? Lang::JA : Lang::EN;
            updateButtonLabels();
            updateContent();
            if (onLanguageChanged) onLanguageChanged(currentLang);
        };
        addAndMakeVisible(langSelector);

        manualBtn.onClick = [this] { currentTab = Tab::Manual; updateContent(); };
        aboutBtn.onClick = [this] { currentTab = Tab::About; updateContent(); };
        manualBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334466));
        aboutBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff334466));
        addAndMakeVisible(manualBtn);
        addAndMakeVisible(aboutBtn);

        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        addAndMakeVisible(viewport);

        updateButtonLabels();
        updateContent();
    }

    void setLanguage(Lang lang)
    {
        currentLang = lang;
        langSelector.setSelectedId(lang == Lang::JA ? 2 : 1, juce::dontSendNotification);
        updateButtonLabels();
        updateContent();
    }

    Lang getLanguage() const { return currentLang; }
    std::function<void(Lang)> onLanguageChanged;

    juce::Font getJapaneseFont(float size, int style = juce::Font::plain) const
    {
        juce::StringArray jaFonts = {
            "Hiragino Kaku Gothic ProN", "Hiragino Sans",
            "Yu Gothic", "Meiryo", "MS Gothic",
            "Noto Sans CJK JP", "Arial Unicode MS"
        };
        for (auto& name : jaFonts) {
            juce::Font f(name, size, style);
            if (f.getTypefaceName().isNotEmpty() &&
                f.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
                return f;
        }
        return juce::Font(size, style);
    }

    juce::Font getFontForLang(float size, int style = juce::Font::plain) const
    {
        return currentLang == Lang::JA ? getJapaneseFont(size, style) : juce::Font(size, style);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff1a1a2e));

        g.setColour(juce::Colour(0xff00ff88));
        g.setFont(getFontForLang(20.0f, juce::Font::bold));
        g.drawText(tr(currentLang, Str::Settings),
                   getLocalBounds().removeFromTop(36), juce::Justification::centred);

        g.setColour(juce::Colour(0xffcccccc));
        g.setFont(getFontForLang(13.0f));
        g.drawText(tr(currentLang, Str::Language) + ":",
                   20, 50, 80, 24, juce::Justification::centredLeft);

        // Content background
        auto contentArea = getLocalBounds().reduced(16);
        contentArea.removeFromTop(110);
        g.setColour(juce::Colour(0xff12122a));
        g.fillRoundedRectangle(contentArea.toFloat(), 6.0f);
    }

    void resized() override
    {
        langSelector.setBounds(110, 50, 140, 24);

        int btnW = 90, btnH = 28;
        manualBtn.setBounds(20, 86, btnW, btnH);
        aboutBtn.setBounds(120, 86, btnW, btnH);

        auto contentArea = getLocalBounds().reduced(16);
        contentArea.removeFromTop(114);
        viewport.setBounds(contentArea);
        content.setSize(contentArea.getWidth() - 14, content.getHeight());
        content.updateHeight();
    }

    void updateButtonLabels()
    {
        manualBtn.setButtonText(tr(currentLang, Str::Manual));
        aboutBtn.setButtonText(tr(currentLang, Str::About));

        if (currentLang == Lang::JA) {
            jaLookAndFeel.setJapaneseFont(getJapaneseFont(12.0f));
            manualBtn.setLookAndFeel(&jaLookAndFeel);
            aboutBtn.setLookAndFeel(&jaLookAndFeel);
        } else {
            manualBtn.setLookAndFeel(nullptr);
            aboutBtn.setLookAndFeel(nullptr);
        }
    }

    void updateContent()
    {
        if (currentTab == Tab::Manual) {
            content.isManual = true;
            content.title = tr(currentLang, Str::ManualTitle);
            content.text = tr(currentLang, Str::ManualOverview) + "\n\n"
                         + tr(currentLang, Str::ManualPresets) + "\n\n"
                         + tr(currentLang, Str::ManualPianoRoll) + "\n\n"
                         + tr(currentLang, Str::ManualParams) + "\n\n"
                         + tr(currentLang, Str::ManualExport);
            content.titleFont = getFontForLang(15.0f, juce::Font::bold);
            content.textFont = getFontForLang(11.5f);
        } else {
            content.isManual = false;
            content.title = {};
            content.text = tr(currentLang, Str::AboutText);
            content.textFont = getFontForLang(13.0f);
        }
        content.updateHeight();
        viewport.setViewPosition(0, 0);
        content.repaint();
    }

    ~SettingsPage() override
    {
        manualBtn.setLookAndFeel(nullptr);
        aboutBtn.setLookAndFeel(nullptr);
    }

private:
    JapaneseLookAndFeel jaLookAndFeel;
    Lang currentLang = Lang::EN;
    Tab currentTab = Tab::Manual;
    juce::ComboBox langSelector;
    juce::TextButton manualBtn{"Manual"};
    juce::TextButton aboutBtn{"About"};
    juce::Viewport viewport;
    SettingsContent content;
};

} // namespace PsyMelody
