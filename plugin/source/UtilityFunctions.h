/*
  ==============================================================================

    UtilityFunctions.h
    Created: 16 Jan 2022 2:26:42pm
    Author:  Joe

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "avir.h"

#if JUCE_WINDOWS
    #include "avir_float4_sse.h"
#endif

void paintBorder(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds, const juce::Component* owner = nullptr);

// [Claude 2026-10-29] A rectangle in `owner`'s coordinates moved to the nearest edges of real screen pixels. The window
// scales the whole layout (125 %, 150 % ...), so whole layout pixels are not whole screen pixels; frames snapped only to
// layout pixels showed seams again at 125 % (Joe). Returns the rectangle unchanged without an owner.
juce::Rectangle<float> snapToScreenPixels(juce::Graphics& g, juce::Rectangle<float> r, const juce::Component* owner);

// [Claude 2026-10-29] Drawing on the screen's pixels. The window scales the 750 x 478 layout (100 / 125 / 150 / 200 %),
// so a layout pixel is `scale` screen pixels and anything placed on whole layout pixels lands between screen pixels at
// 125 % and 150 %: seams, soft doubled hairlines, soft text, uneven repeated patterns. A PixelGrid knows the scale and
// where `owner` sits in the scaled layout, and hands back positions in the owner's own coordinates that fall on whole
// screen pixels. Without an owner the origin is taken as the window's (pop-up menus).
struct PixelGrid
{
	PixelGrid(juce::Graphics& g, const juce::Component* owner);

	float scale { 1.f };            // screen pixels per layout pixel
	juce::Point<float> origin;      // the owner's origin in the scaled layout

	float px() const { return 1.f / scale; }                                      // one screen pixel, in layout units
	int rule() const { return juce::jmax(1, juce::roundToInt(scale)); }          // a hairline: 1 screen pixel up to 125 %, 2 at 150 and 200 %
	int thick(float layoutPx) const { return juce::jmax(1, juce::roundToInt(layoutPx * scale)); }   // a layout thickness in whole screen pixels
	float rulePx() const { return (float) rule() * px(); }                        // the hairline's thickness, in layout units

	float snapX(float x) const;     // the nearest screen-pixel edge, in the owner's coordinates
	float snapY(float y) const;
	juce::Rectangle<float> snap(juce::Rectangle<float> r) const;                 // every edge on a screen pixel
	// a rectangle to stroke with rulePx() so the stroke covers whole screen pixels just inside `r`
	juce::Rectangle<float> strokeRect(juce::Rectangle<float> r) const { return snap(r).reduced(rulePx() / 2.f); }

	// a horizontal rule whose top edge is the nearest screen pixel to `top`, `pixels` screen pixels tall (0: rule()),
	// from x0 to x1; and the same standing up
	juce::Rectangle<float> hLine(float top, float x0, float x1, int pixels = 0) const;
	juce::Rectangle<float> vLine(float left, float y0, float y1, int pixels = 0) const;
};

void fillHLine(juce::Graphics& g, const PixelGrid& grid, float top, float x0, float x1, int pixels = 0);
void fillVLine(juce::Graphics& g, const PixelGrid& grid, float left, float y0, float y1, int pixels = 0);

// [Claude 2026-10-29] One line of text in the current font and colour, placed as drawText would (left, centred or
// right in `area`, and top, centred or bottom), then moved so its baseline and its first glyph's left edge sit on whole
// screen pixels: the same word then rasterises the same in every column and at every window size. Text wider than
// the area is squeezed to fit, as drawFittedText does.
void drawTextOnPixels(juce::Graphics& g, const PixelGrid& grid, const juce::String& text, juce::Rectangle<float> area, juce::Justification justification);

void buildLabelFont(    juce::Label& label,
                        juce::Justification justification,
                        juce::String typeface,
                        float fontSize,
                        juce::Font::FontStyleFlags fontStyle,
                        juce::Colour color,
                        float alpha);


juce::Image applyResize(const juce::Image& src, int width, int height);

float getWindowsDPIScale(juce::Component* component);
