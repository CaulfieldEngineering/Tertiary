/*
  ==============================================================================

    UtilityFunctions.cpp
    Created: 16 Jan 2022 2:26:42pm
    Author:  Joe

  ==============================================================================
*/

#include "UtilityFunctions.h"

#if JUCE_WINDOWS
    #include <Windows.h>
    #include <ShellScalingApi.h>
#endif

/* Paints a shaded border around specified object.*/
// ==================================================================
// where `c`'s origin sits in the scaled layout: its position summed up to the scaled content (the one transformed parent)
static juce::Point<float> originInLayout(const juce::Component* c)
{
	juce::Point<float> o;
	for (auto* p = c; p != nullptr && ! p->isTransformed(); p = p->getParentComponent())
		o += p->getPosition().toFloat();
	return o;
}

juce::Rectangle<float> snapToScreenPixels(juce::Graphics& g, juce::Rectangle<float> r, const juce::Component* owner)
{
	if (owner == nullptr)
		return r;
	const float sc = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
	const auto o = originInLayout(owner);
	const auto snap = [sc](float v) { return std::round(v * sc) / sc; };
	return juce::Rectangle<float>::leftTopRightBottom(snap(o.x + r.getX()) - o.x, snap(o.y + r.getY()) - o.y,
	                                                   snap(o.x + r.getRight()) - o.x, snap(o.y + r.getBottom()) - o.y);
}

// [Claude 2026-10-29] The pixel grid (see the header)
// ==================================================================
PixelGrid::PixelGrid(juce::Graphics& g, const juce::Component* owner)
	: scale((float) g.getInternalContext().getPhysicalPixelScaleFactor()),
	  origin(owner != nullptr ? originInLayout(owner) : juce::Point<float>())
{
	if (! (scale > 0.f)) scale = 1.f;
}

float PixelGrid::snapX(float x) const { return std::round((origin.x + x) * scale) / scale - origin.x; }
float PixelGrid::snapY(float y) const { return std::round((origin.y + y) * scale) / scale - origin.y; }

juce::Rectangle<float> PixelGrid::snap(juce::Rectangle<float> r) const
{
	return juce::Rectangle<float>::leftTopRightBottom(snapX(r.getX()), snapY(r.getY()), snapX(r.getRight()), snapY(r.getBottom()));
}

juce::Rectangle<float> PixelGrid::hLine(float top, float x0, float x1, int pixels) const
{
	const float y = snapY(top);
	return juce::Rectangle<float>::leftTopRightBottom(snapX(x0), y, snapX(x1), y + (float) (pixels > 0 ? pixels : rule()) * px());
}

juce::Rectangle<float> PixelGrid::vLine(float left, float y0, float y1, int pixels) const
{
	const float x = snapX(left);
	return juce::Rectangle<float>::leftTopRightBottom(x, snapY(y0), x + (float) (pixels > 0 ? pixels : rule()) * px(), snapY(y1));
}

void fillHLine(juce::Graphics& g, const PixelGrid& grid, float top, float x0, float x1, int pixels)
{
	g.fillRect(grid.hLine(top, x0, x1, pixels));
}

void fillVLine(juce::Graphics& g, const PixelGrid& grid, float left, float y0, float y1, int pixels)
{
	g.fillRect(grid.vLine(left, y0, y1, pixels));
}

void drawTextOnPixels(juce::Graphics& g, const PixelGrid& grid, const juce::String& text, juce::Rectangle<float> area, juce::Justification justification)
{
	if (text.isEmpty())
		return;
	auto font = g.getCurrentFont();
	float w = font.getStringWidthFloat(text);
	// too wide: squeezed as drawFittedText squeezes a single line (to 70 % at most), then cut with an ellipsis; still
	// placed on screen pixels (Codex: the fallback to drawFittedText lost the snapping for long preset names)
	bool curtail = false;
	if (w > area.getWidth() + 0.01f)
	{
		font = font.withHorizontalScale(juce::jmax(0.7f, area.getWidth() / w));
		w = font.getStringWidthFloat(text);
		if (w > area.getWidth() + 0.01f) { curtail = true; w = area.getWidth(); }
	}
	using J = juce::Justification;
	float x = justification.testFlags(J::right) ? area.getRight() - w
	        : justification.testFlags(J::horizontallyCentred) ? area.getCentreX() - w / 2.f
	        : area.getX();
	// the line box drawText centres is ascent + descent tall, so its baseline sits ascent below the box's top
	float baseline = justification.testFlags(J::top) ? area.getY() + font.getAscent()
	               : justification.testFlags(J::bottom) ? area.getBottom() - font.getDescent()
	               : area.getY() + (area.getHeight() + font.getAscent() - font.getDescent()) / 2.f;
	// the renderer truncates the baseline to a screen row (so text at 100 % keeps the row it always had); here the
	// row is chosen the same way, then nudged a hair below its edge so the truncation cannot drop it a row
	x = grid.snapX(x);
	baseline = (std::floor((grid.origin.y + baseline) * grid.scale) + 0.05f) / grid.scale - grid.origin.y;
	juce::GlyphArrangement glyphs;
	if (curtail)
		glyphs.addCurtailedLineOfText(font, text, x, baseline, area.getWidth(), true);
	else
		glyphs.addLineOfText(font, text, x, baseline);
	glyphs.draw(g);
}

void paintBorder(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds, const juce::Component* owner)
{
	// [Claude 2026-10-29] With an owner, every step lands on whole screen pixels at any window size, one screen pixel per
	// step (two at 200 %), so abutting frames tile into one plateau at 125 % and 150 % too (Joe: "at 125, those seams
	// reappear"). Without one, the 1-layout-pixel version below.
	if (owner != nullptr)
	{
		const float sc = (float) g.getInternalContext().getPhysicalPixelScaleFactor();
		const float step = (float) juce::jmax(1, juce::roundToInt(sc)) / sc;
		auto ring = snapToScreenPixels(g, bounds, owner);
		const float steps[] = { 1.f, 0.8f, 0.6f, 0.3f };
		for (float k : steps)
		{
			g.setColour(baseColor.withMultipliedBrightness(k));
			g.drawRect(ring, step);
			ring.reduce(step, step);
		}
		return;
	}

	// [Claude 2026-10-29] Joe's stepped bevel, tightened: the same four steps from the frame grey down into the recess,
	// 1 px each instead of 2 (Joe: "revert the framing to its original state and just tighten it up", "a little
	// thinner and sharper"). The boxes it frames are unchanged.
	// [Claude 2026-10-29] Each step is a square-cornered ring on whole pixels, not a rounded stroke on a half-pixel
	// inset: the rounded corner left its apex pixel part-covered over black, a dark speck at every corner that read as
	// a notch wherever two frames meet; and the band frames sit on fractional x (the band width is 203.67), so at 1x
	// their strokes smeared over two pixels and the LOW/MID and MID/HIGH pillars came out uneven (Joe: "non-homogenous
	// and black spots where the painting isn't hitting"). Snapped to the nearest pixel edges, neighbouring frames tile
	// into one plateau of the frame grey.
	auto border = bounds.toNearestIntEdges();
	const float steps[] = { 1.f, 0.8f, 0.6f, 0.3f };
	for (float k : steps)
	{
		g.setColour(baseColor.withMultipliedBrightness(k));
		g.drawRect(border, 1);
		border.reduce(1, 1);
	}
}

/* Conditions All Font & Text Parameters */
// ==================================================================
void buildLabelFont(    juce::Label& label,
                        juce::Justification justification,
                        juce::String typeface,
                        float fontSize,
                        juce::Font::FontStyleFlags fontStyle,
                        juce::Colour color,
                        float alpha)
{
    label.setJustificationType(justification);
    label.setFont(juce::Font(typeface, fontSize, fontStyle));
    label.setColour(juce::Label::textColourId, color);
    label.setAlpha(alpha);
}

/* Specialized Image Resize Function */
// ==================================================================
juce::Image applyResize(const juce::Image& src, int width, int height)
{
    if (width <= 0 || height <= 0 || src.isNull())
    {
        DBG("Invalid source image or target dimensions for resizing.");
        return juce::Image(); // Return an empty image if the dimensions or source are invalid
    }

    juce::Image dst(src.getFormat(), width, height, true);

    juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dstData(dst, juce::Image::BitmapData::readWrite);

    int channels = 0;
    if (src.getFormat() == juce::Image::ARGB)               channels = 4;
    else if (src.getFormat() == juce::Image::RGB)           channels = 3;
    else if (src.getFormat() == juce::Image::SingleChannel) channels = 1;
    else                                                    return juce::Image();

    juce::HeapBlock<uint8_t> srcPacked(src.getWidth() * src.getHeight() * channels);
    juce::HeapBlock<uint8_t> dstPacked(dst.getWidth() * dst.getHeight() * channels);

    uint8_t* rawSrc = srcPacked.getData();
    uint8_t* rawDst = dstPacked.getData();

    for (int y = 0; y < src.getHeight(); y++)
        memcpy(rawSrc + y * src.getWidth() * channels,
               srcData.getLinePointer(y),
               (size_t)(src.getWidth() * channels));

   #if USE_SSE
    avir::CImageResizer<avir::fpclass_float4> imageResizer(8);
    imageResizer.resizeImage(rawSrc, src.getWidth(), src.getHeight(), 0,
                             rawDst, dst.getWidth(), dst.getHeight(), channels, 0);
   #else
    avir::CImageResizer<> imageResizer(8);
    imageResizer.resizeImage(rawSrc, src.getWidth(), src.getHeight(), 0,
                             rawDst, dst.getWidth(), dst.getHeight(), channels, 0);
   #endif

    for (int y = 0; y < dst.getHeight(); y++)
        memcpy(dstData.getLinePointer(y),
               rawDst + y * dst.getWidth() * channels,
               (size_t)(dst.getWidth() * channels));

    return dst;
}


float getWindowsDPIScale(juce::Component* component)
{
    #if JUCE_WINDOWS
    if (auto* peer = component->getPeer())
    {
        if (auto* hwnd = (HWND)peer->getNativeHandle())
        {
            UINT dpi = GetDpiForWindow(hwnd);
            return static_cast<float>(dpi) / 96.0f;
        }
    }
    #endif
    return 1.0f;
}