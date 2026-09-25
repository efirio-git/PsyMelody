#pragma once
#include <juce_graphics/juce_graphics.h>

namespace PsyMelody {

// Font construction for the whole UI. JUCE 8 deprecated the juce::Font(float),
// Font(name, ...) and Font(Typeface::Ptr) constructors; these helpers build the
// font from FontOptions with the "portable" metrics kind instead.
//
// Metrics kind matters for Windows. The deprecated constructors used the legacy
// kind, which takes metrics from the platform: on Windows the embedded Inter
// and Yu Gothic then render about 15% smaller than on macOS. Portable metrics
// are the same on every platform, and on macOS they are identical to the
// legacy ones for every font this UI uses (Inter, Orbitron, Lucida Grande,
// Hiragino), so macOS - the reference look - is unchanged.

inline juce::Font makeFont(float height, int styleFlags = juce::Font::plain)
{
    return juce::Font(juce::FontOptions(height, styleFlags)
                          .withMetricsKind(juce::TypefaceMetricsKind::portable));
}

inline juce::Font makeFont(const juce::String& typefaceName, float height, int styleFlags)
{
    return juce::Font(juce::FontOptions(typefaceName, height, styleFlags)
                          .withMetricsKind(juce::TypefaceMetricsKind::portable));
}

inline juce::Font makeFont(const juce::Typeface::Ptr& typeface)
{
    return juce::Font(juce::FontOptions(typeface)
                          .withMetricsKind(juce::TypefaceMetricsKind::portable));
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
