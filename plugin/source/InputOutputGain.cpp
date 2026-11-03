/*
  ==============================================================================

    InputOutputGain.cpp
    Created: 10 Jan 2022 8:19:16pm
    Author:  Joe

  ==============================================================================
*/

#include "InputOutputGain.h"
#include "UtilityFunctions.h"

InputOutputGain::InputOutputGain(audio_plugin::AudioPluginAudioProcessor& p) :
	audioProcessor(p)
{

	//setSize(40, 150);

	sliderGain.setSliderStyle(juce::Slider::SliderStyle::LinearVertical);
	sliderGain.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);  // [Claude 2026-10-29] before the first layout, so no space is kept for one
	sliderGain.setLookAndFeel(&inOutLookAndFeel);
	sliderGain.addListener(this);
	sliderValue = sliderGain.getValue();
	addAndMakeVisible(sliderGain);

	startTimerHz(30);
    // [Claude 2026-10-29] not opaque and not cached: at a scaled window size (125 %, 150 %) the edges fall between
    // screen pixels, so an opaque, cached meter left stray pixels round its label (Joe: "little singular pixel
    // artifacts behind the input and output gain labels")

}

InputOutputGain::~InputOutputGain()
{
	sliderGain.setLookAndFeel(nullptr);
}

void InputOutputGain::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds().toFloat();
	bounds.reduce(4, 0);

	// [Claude 2026-10-29] Each LED drawn as its own rectangle on screen pixels, unlit or lit, on black. It was colour
	// under a black picture with LED holes cut in it; scaled to 125 % or 150 % the picture's edges let the colour show
	// through as stray red, yellow and green pixels (Joe)
	// [Claude 2026-10-29] The grid itself is laid out in screen pixels for the window's scale (buildLeds), so every
	// LED and every gap is the same whole number of pixels and the two columns line up row for row (Joe: "even at
	// large scalings the meters look pretty bad compared to how they did previously")
	const PixelGrid grid(g, this);
	if (builtScale != grid.scale)
		buildLeds(grid);
	g.setColour(juce::Colours::black);
	g.fillRect(ledGrill);
	for (int column = 0; column < 2; ++column)
	{
		const auto& leds = column == 0 ? leftLeds : rightLeds;
		const float level = column == 0 ? leftLevelPixel : rightLevelPixel;
		for (int i = 0; i < leds.size(); ++i)
		{
			// lit when its top is within the level, measured up from the grill's bottom (the same rule getLevels
			// quantises to, so the meter steps one whole LED at a time)
			g.setGradientFill(ledThresholds[i] <= level + 0.001f ? ledOnGradient : ledOffGradient);
			g.fillRect(leds[i]);
		}
	}

	// Draw the Slider Value Background
	g.setColour(juce::Colours::black);  // Migrate to AllColors
	g.fillRect(labelBounds);

	juce::String valueString;

	if (sliderValue > 0.f)
		valueString = "+";

	valueString = valueString << juce::String(sliderValue);

	//if (sliderValue == 0)
	//	valueString = valueString << " dB";

	//g.drawImage(grill, grillBounds);

	//g.setColour(juce::Colours::white.withAlpha(0.65f));
	//g.setFont(13);
	//g.drawFittedText(valueString, labelBounds.toNearestInt(), juce::Justification::centred, 1);

	if (sliderValue == 0)
		valueString = valueString << " dB";

	// (the LEDs are drawn above, one by one; the grill picture is no longer drawn)

	// Initial font size based on height of labelBounds or a fixed size
	auto initialFontSize = 13.0f;
	g.setFont(initialFontSize);

	// Measure the text width
	auto textWidth = g.getCurrentFont().getStringWidth(valueString);
	auto labelWidth = labelBounds.getWidth();

	// Reduce font size if the text doesn't fit within the label bounds
	while (textWidth > labelWidth && initialFontSize > 1.0f) {
		initialFontSize -= 1.0f; // Reduce the font size
		g.setFont(initialFontSize); // Set the new font size
		textWidth = g.getCurrentFont().getStringWidth(valueString); // Re-measure the text width
	}

	// Set color and draw the text
	g.setColour(juce::Colours::white.withAlpha(0.65f));
	drawTextOnPixels(g, grid, valueString, labelBounds, juce::Justification::centred);   // [Claude 2026-10-29] on whole screen pixels

	g.setColour(juce::Colours::hotpink);
}

void InputOutputGain::resized()
{

	auto bounds = getLocalBounds().toFloat();

	grillBounds = {	bounds.getX(), bounds.getY(),
					bounds.getWidth(), bounds.getHeight() - 15};
	
	labelBounds = { bounds.getX(), grillBounds.getBottom(), bounds.getWidth(), 15 };

	ledWidth = bounds.getWidth() * 0.45f;
	//spacing = bo unds.getHeight()*0.014f;			// Vertical Distance between LEDs
	//ledHeight = bounds.getHeight()*0.006f;

	ledOffGradient = makeMeterGradient(grillBounds, 0.3f);  // (Joe: "Meters still a little too dark" at 0.2)  // [Claude 2026-10-29] visible again on Joe's screen ("I can't see the off meter LEDs"); 0.1 was too dark  // [Claude 2026-10-29] darker unlit LEDs, a quiet meter at rest (was 0.22)
	ledOnGradient = makeMeterGradient(grillBounds, 1.f);

	// The LEDs are laid out at the next paint, for the screen's scale (buildLeds)
	builtScale = 0.f;

	// Place The Slider
	// ===========================================================
	// [Claude 2026-10-29] the slider covers the whole meter slot above the value, so the cap can reach both ends
	sliderGain.setBounds(grillBounds.toNearestInt());
}

juce::ColourGradient InputOutputGain::makeMeterGradient(juce::Rectangle<float> bounds, float brightness)
{
	// [Claude 2026-10-29] The meters in the product's own colours (Joe: "Yeah meters?"): the Wonderland green, lighter
	// toward the top, and red only for the last segments. Was green, yellow and red at full strength.
	juce::ColourGradient gradient = juce::ColourGradient {	
									juce::Colour(0xff2a7346).withMultipliedBrightness(brightness),	
									bounds.getBottomLeft(),
									juce::Colour(0xffd0574f).withMultipliedBrightness(brightness),	
									bounds.getTopLeft(), false };

	gradient.addColour(0.55f, juce::Colour(0xff3c9b5d).withMultipliedBrightness(brightness));	// Wonderland green
	gradient.addColour(0.84f, juce::Colour(0xff8fd3a6).withMultipliedBrightness(brightness));	// toward 0 dB
	gradient.addColour(0.905f, juce::Colour(0xffcfe9d8).withMultipliedBrightness(brightness));
	gradient.addColour(0.915f, juce::Colour(0xffd0574f).withMultipliedBrightness(brightness));	// over

	return gradient;
}

void InputOutputGain::timerCallback()
{
	getLevels();

	//repaint(1,1,getWidth()-2,getHeight()-2);
	repaint(grillBounds.toNearestInt());
}

void InputOutputGain::getLevels()
{

	// Pull current audio-level values from processor

	if (pickOffPoint == "INPUT")
	{
		leftLevel = audioProcessor.getRmsValue(0, 0);
		rightLevel = audioProcessor.getRmsValue(0, 1);
	}

	if (pickOffPoint == "OUTPUT")
	{
		leftLevel = audioProcessor.getRmsValue(1, 0);
		rightLevel = audioProcessor.getRmsValue(1, 1);
	}

	if (pickOffPoint == "NA")
	{
		leftLevel = 0.f;
		rightLevel = 0.f;
	}

	//auto bounds = getLocalBounds().toFloat();
	// [Claude 2026-10-29] over the grill as drawn (on screen pixels), the height the LED thresholds are measured in;
	// over the unsnapped height the top LED could stay off at 0 dB (Codex)
	auto bounds = ledGrill.isEmpty() ? grillBounds : ledGrill;

	leftLevelPixel = bounds.getHeight() * juce::jmap(leftLevel, -60.f, 0.f, 0.f, 1.f);
	rightLevelPixel = bounds.getHeight() * juce::jmap(rightLevel, -60.f, 0.f, 0.f, 1.f);

	// Quantize levels to discrete LED steps: down to the top of the highest LED the level reaches
	// [Claude 2026-10-29] the thresholds are the LEDs' tops measured up from the grill's bottom, as paint lights them
	for (float* level : { &leftLevelPixel, &rightLevelPixel })
	{
		float quantised = 0.f;
		for (float threshold : ledThresholds)
		{
			if (threshold > *level) break;
			quantised = threshold;
		}
		*level = quantised;
	}
}

void InputOutputGain::setPickOffPoint(juce::String pickoff)
{
	pickOffPoint = pickoff;
}

// [Claude 2026-10-29] The LED grid in screen pixels. The 100 % meter (the original grill picture: LEDs 2 px tall, 1 px
// apart, 11 px wide, the two columns 1 px apart) is the pattern; at 125, 150 and 200 % each of those is rounded to
// whole screen pixels and the grid is laid out from the grill's middle in those units, so every LED and every gap is
// the same height, the columns align row for row, and the edges are crisp. Rebuilt when the window's scale changes.
void InputOutputGain::buildLeds(const PixelGrid& grid)
{
	ledThresholds.clear();
	leftLeds.clear();
	rightLeds.clear();
	builtScale = grid.scale;

	const float sc = grid.scale;
	const auto toDevX = [&](float x) { return (grid.origin.x + x) * sc; };
	const auto toDevY = [&](float y) { return (grid.origin.y + y) * sc; };
	const auto fromDevX = [&](float X) { return X / sc - grid.origin.x; };
	const auto fromDevY = [&](float Y) { return Y / sc - grid.origin.y; };

	ledGrill = grid.snap(grillBounds);

	// sizes, in screen pixels
	const int ledPx = juce::jmax(1, juce::roundToInt(ledHeightAt1x * sc));
	const int gapPx = juce::jmax(1, juce::roundToInt(ledGapAt1x * sc));
	const int pitchPx = ledPx + gapPx;
	const int widthPx = juce::jmax(1, juce::roundToInt(ledWidth * sc));
	const int columnGapPx = juce::jmax(1, juce::roundToInt(columnGapAt1x * sc));

	// the two columns, centred on the grill
	const float centreDevX = std::round(toDevX(grillBounds.getCentreX()));
	const float leftDev = std::round(centreDevX - (2 * widthPx + columnGapPx) / 2.f);
	const float rightDev = leftDev + (float) (widthPx + columnGapPx);

	// the rows, from the middle outward (the component's middle, as the grill picture was built), whole LEDs inside
	// the grill only
	const float topDev = std::round(toDevY(grillBounds.getY())), bottomDev = std::round(toDevY(grillBounds.getBottom()));
	const float middleTop = std::round(toDevY(getLocalBounds().toFloat().getCentreY()) - ledPx / 2.f);
	const int above = (int) std::floor((middleTop - topDev) / pitchPx);
	const int below = (int) std::floor((bottomDev - ledPx - middleTop) / pitchPx);
	for (int k = -above; k <= below; ++k)
	{
		const float top = middleTop + (float) (k * pitchPx);
		const auto row = juce::Rectangle<float>::leftTopRightBottom(0.f, fromDevY(top), 0.f, fromDevY(top + ledPx));
		leftLeds.add(row.withLeft(fromDevX(leftDev)).withRight(fromDevX(leftDev + widthPx)));
		rightLeds.add(row.withLeft(fromDevX(rightDev)).withRight(fromDevX(rightDev + widthPx)));
		ledThresholds.add((bottomDev - top) / sc);   // its top, up from the grill's bottom, in layout pixels
	}
	// bottom to top, as the levels rise
	for (int i = 0, j = leftLeds.size() - 1; i < j; ++i, --j)
	{
		leftLeds.swap(i, j);
		rightLeds.swap(i, j);
		ledThresholds.swap(i, j);
	}

	// The fader is drawn on the gap between the columns, which can sit off the component's centre by a screen
	// pixel (Joe: "the track is to the left of center on the meter")
	const double ledCentreX = (double) fromDevX(leftDev + widthPx + columnGapPx / 2.f);
	if (sliderGain.getProperties()["ledCentreX"] != juce::var(ledCentreX))
	{
		sliderGain.getProperties().set("ledCentreX", ledCentreX);
		sliderGain.repaint();
	}
}

void InputOutputGain::sliderValueChanged(juce::Slider* slider)
{
	//if (setDebug)
	//	WLDebugger::getInstance().printMessage(mNameSpace, __func__, getName());

	sliderValue = slider->getValue();
	repaint();
}
