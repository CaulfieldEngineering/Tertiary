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
void paintBorder(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds)
{
	// [Claude 2026-10-29] Joe's stepped bevel, tightened: the same four steps from the frame grey down into the recess,
	// 1 px each instead of 2, and near-square corners (Joe: "revert the framing to its original state and just
	// tighten it up", "a little thinner and sharper"). The boxes it frames are unchanged.
	auto border = bounds.reduced(0.5f);
	const float steps[] = { 1.f, 0.8f, 0.6f, 0.3f };
	for (float k : steps)
	{
		g.setColour(baseColor.withMultipliedBrightness(k));
		g.drawRoundedRectangle(border, 0.75f, 1.f);
		border.reduce(1.f, 1.f);
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