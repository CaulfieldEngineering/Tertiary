/*
  ==============================================================================

    TopBanner.cpp
    Created: 29 Jan 2023 1:32:49pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#include "TopBanner.h"

TopBanner::TopBanner()
{
    imageTopBanner = juce::ImageCache::getFromMemory(BinaryData::TopBanner_Black_png, BinaryData::TopBanner_Black_pngSize);
}

TopBanner::~TopBanner()
{
    
}

void TopBanner::paint(juce::Graphics& g)
{
  auto bounds = getLocalBounds().toFloat();

  if (!imageTopBanner.isNull()) {
    // Rescale the image each time it's painted, based on the current bounds
    g.drawImage(imageTopBanner, bounds,
                juce::RectanglePlacement::fillDestination);
  }
}

void TopBanner::resized()
{

}