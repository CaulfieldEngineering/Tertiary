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