/*
  ==============================================================================

    TopBanner.cpp
    Created: 29 Jan 2023 1:32:49pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#include "TopBanner.h"


#if JUCE_INTEL
 #define USE_SSE 1
#endif

TopBanner::TopBanner()
{
    imageCompanyTitle = juce::ImageCache::getFromMemory(BinaryData::CompanyTitle_png, BinaryData::CompanyTitle_pngSize);
    imagePluginTitle = juce::ImageCache::getFromMemory(BinaryData::PluginTitle_png, BinaryData::PluginTitle_pngSize);
}

TopBanner::~TopBanner()
{
    
}

void TopBanner::resized()
{
    // Any layout code can go here, if needed
}
void TopBanner::paint(juce::Graphics& g)
{
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality); // Ensure high-quality resampling

    auto bounds = getLocalBounds().toFloat();
    auto boundsHeight = bounds.getHeight();

    // Draw the plugin title centered in the bounds
    if (!imagePluginTitle.isNull()) {
        float aspectRatio = imagePluginTitle.getWidth() / (float)imagePluginTitle.getHeight();
        float newWidth = boundsHeight * aspectRatio;

        auto pluginTitleBounds = juce::Rectangle<float>(
            bounds.getCentreX() - (newWidth / 2.0f),
            bounds.getY(),
            newWidth,
            boundsHeight
        );

        // Draw the image with the new scaled dimensions
        g.drawImage(imagePluginTitle, pluginTitleBounds,
                    juce::RectanglePlacement::centred);
    }
    
    // Draw the company title image with resizing
    if (!imageCompanyTitle.isNull())
    {
        float aspectRatio = imageCompanyTitle.getWidth() / (float)imageCompanyTitle.getHeight();
        float companyHeight = boundsHeight * 0.65f; // 75% of the bounds height
        int newWidth = juce::roundToInt(companyHeight * aspectRatio);
        int newHeight = juce::roundToInt(companyHeight);

        // Call applyResize directly to get the resized image
        juce::Image resizedImage = applyResize(imageCompanyTitle, newWidth, newHeight);

        if (!resizedImage.isNull())
        {
            // Calculate where to position the image
            auto xPos = juce::roundToInt(bounds.getRight() - newWidth);
            auto yPos = juce::roundToInt(bounds.getCentreY() - (newHeight / 2.0f));

            // Draw the resized image at the calculated position
            g.drawImageAt(resizedImage, xPos, yPos);
        }
        else
        {
            DBG("Resized image is invalid.");
        }
    }
}

void TopBanner::paintRescaledImage(juce::Graphics& g, juce::Rectangle<int> src, juce::Rectangle<int> dest, juce::Image originalImgToDraw)
{
    // Simplified approach to ensure basic image rendering works first
    auto cutImage = originalImgToDraw.getClippedImage(src);

    // Check if the clipped image is valid
    if (!cutImage.isNull())
    {
        // Directly draw the clipped image to check if it’s correct
        g.drawImage(cutImage, dest.toFloat());
    }
    else
    {
        DBG("Clipped image is invalid.");
    }

    // Now apply resizing and render the resized image
    const auto pxFactor = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int width = juce::roundToInt(pxFactor * dest.getWidth());
    const int height = juce::roundToInt(pxFactor * dest.getHeight());

    auto finalImgToDraw = applyResize(cutImage, width, height);

    if (!finalImgToDraw.isNull())
    {
        g.drawImage(finalImgToDraw, dest.toFloat());
    }
    else
    {
        DBG("Resized image is invalid.");
    }
}

juce::Image TopBanner::applyResize(const juce::Image& src, int width, int height)
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

