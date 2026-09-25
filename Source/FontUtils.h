#pragma once
#include <juce_graphics/juce_graphics.h>

namespace PsyMelody {

// JUCE 8 deprecated the juce::Font(float ...), Font(name, ...) and
// Font(Typeface::Ptr) constructors. Internally they build a FontOptions with
// TypefaceMetricsKind::legacy, whereas FontOptions defaults to the "portable"
// metrics, which lays text out at a different size. These helpers keep the
// legacy metrics so text renders exactly as before without the deprecated API.

inline juce::Font legacyFont(float height, int styleFlags = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(height, styleFlags)
                          .withMetricsKind(juce::TypefaceMetricsKind::legacy));
}

inline juce::Font legacyFont(const juce::String& typefaceName, float height, int styleFlags)
{
    return juce::Font(juce::FontOptions(typefaceName, height, styleFlags)
                          .withMetricsKind(juce::TypefaceMetricsKind::legacy));
}

inline juce::Font legacyFont(const juce::Typeface::Ptr& typeface)
{
    return juce::Font(juce::FontOptions(typeface)
                          .withMetricsKind(juce::TypefaceMetricsKind::legacy));
}

// Same measurement as the deprecated Font::getStringWidthFloat: the sum of the
// glyph advances. The replacements JUCE suggests (GlyphArrangement / TextLayout
// getStringWidth) return the shaped bounding-box width instead, which would
// shift layouts that were tuned against the advance width.
inline float advanceWidth(const juce::Font& font, const juce::String& text)
{
    if (auto typeface = font.getTypefacePtr())
        return typeface->getStringWidth(font.getMetricsKind(), text, font.getHeight(),
                                        font.getHorizontalScale())
             + font.getHeight() * font.getHorizontalScale() * font.getExtraKerningFactor()
                   * (float) text.length();
    return 0.0f;
}

} // namespace PsyMelody
