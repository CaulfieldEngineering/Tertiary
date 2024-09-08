/*
  ==============================================================================

    UtilityFunctions.cpp
    Created: 16 Jan 2022 2:26:42pm
    Author:  Joe

  ==============================================================================
*/

#include "UtilityFunctions.h"

/* Paints a shaded border around specified object.*/
// ==================================================================
void paintBorder(juce::Graphics& g, juce::Colour baseColor, juce::Rectangle<float> bounds)
{
	using namespace juce;

	//auto baseColor = juce::Colours::purple;

	//color = juce::Colours::purple;

	// DRAW BORDER 1 ====================
	juce::Rectangle<float> border;
	border.setBounds(bounds.getTopLeft().x, bounds.getTopLeft().y, bounds.getWidth(), bounds.getHeight());
	g.setColour(baseColor);
	g.drawRoundedRectangle(border, 1.f, 2.f);

	// DRAW BORDER 2 =====
	border.reduce(1, 1);
	g.setColour(baseColor.withMultipliedBrightness(0.8f));
	g.drawRoundedRectangle(border, 2.f, 2.f);

	// DRAW BORDER 3 =====
	border.reduce(1, 1);
	g.setColour(baseColor.withMultipliedBrightness(0.6f));
	g.drawRoundedRectangle(border, 2.f, 2.f);

	// DRAW BORDER 4 =====
	border.reduce(1, 1);
	g.setColour(baseColor.withMultipliedBrightness(0.3f));
	g.drawRoundedRectangle(border, 2.f, 2.f);
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