/*
  ==============================================================================

    MyLookAndFeel.cpp
    Created: 19 Jan 2022 4:55:59pm
    Author:  Joe

  ==============================================================================
*/

#include "MyLookAndFeel.h"


CustomSlider::CustomSlider()
{
    buildLabel();
}

CustomSlider::~CustomSlider()
{
	slider.setLookAndFeel(nullptr);
}

void CustomSlider::resized()
{
	auto bounds = getLocalBounds();

	auto labelScale = 40.f / 180.f;
    auto labelWidth = getWidth() * labelScale;

    label.setSize(labelWidth, getHeight());
    auto x = getWidth();
    //label.setSize(getWidth() * 0.25f, getHeight());

    label.setTopRightPosition(bounds.getRight(), 0);

	//slider.setBounds(bounds.withTrimmedRight(45));
    slider.setBounds(bounds.withTrimmedRight(labelWidth * 1.05f));

	buildLabel();
}

void CustomSlider::paintOverChildren(juce::Graphics& g)
{
    auto bounds = slider.getLocalBounds();
    // [Claude 2026-10-29] one crisp dark outline on every key, menu and slider (Joe chose it over the grey frames)
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRect(bounds.toFloat(), 1.f);
    
//    g.setColour(juce::Colours::hotpink);
//    g.fillRect(label.getBounds());
    
    //if (JUCE_ENABLE_LIVE_CONSTANT_EDITOR)
    if (false)
        buildLabel();
}

void CustomSlider::setStyleStandard(juce::String suffix)
{
	slider.setLookAndFeel(&standardSliderLookAndFeel);
	valueSuffix = suffix;
	updateStringText();
}

void CustomSlider::setStyleCenter(juce::String suffix)
{
	slider.setLookAndFeel(&centerSliderLookAndFeel);
	valueSuffix = suffix;
	updateStringText();
}

void CustomSlider::setBandMode(int mode)
{
	standardSliderLookAndFeel.setBandMode(mode);
	centerSliderLookAndFeel.setBandMode(mode);
}

void CustomSlider::labelTextChanged(juce::Label* labelThatHasChanged)
{
	juce::String entryString = labelThatHasChanged->getText();

	// Remove Non-Numerical Text ==================================================

	bool containsNum{ false };

	for (int i = 0; i <= 127; i++)
	{
		// Remove all characters not '0-9' or '-' or '.'
		if ((i < 48 || i > 57) && i !=46 && i != 45)
			entryString = entryString.removeCharacters(juce::String::charToString(char(i)));

		// Entry must contain at least one numeric value
		if ((i >= 48 || i <= 57))
		{
			if (entryString.contains(juce::String::charToString(char(i))))
				containsNum = true;
		}
	}

	if (containsNum)
		slider.setValue((float)entryString.getFloatValue());

	updateStringText();
}

void CustomSlider::sliderValueChanged(juce::Slider* slider)
{
	updateStringText();
}

void CustomSlider::updateStringText()
{
	using namespace FontEditor::ParameterLabels;

	juce::String sliderValue;

	sliderValue = (juce::String)slider.getValue();
	sliderValue.append(valueSuffix, 5);

	label.setText(sliderValue, juce::NotificationType::dontSendNotification);

	auto fontSize = slider.getHeight() * 0.83f;  // the same size as before the sliders got shorter
    auto titleFont = juce::Font(getTypeface(), fontSize, getFontStyle());

    // Set the initial font size
    label.setFont(titleFont);

    // Measure the text width and adjust font size if needed
    auto labelWidth = label.getWidth();
    auto textWidth = label.getFont().getStringWidth(label.getText());

    while (textWidth > labelWidth && fontSize > 1.0f) {
        fontSize -= 2.5f;  // Reduce font size
        titleFont.setHeight(fontSize);
        label.setFont(titleFont);  // Update label's font
        textWidth = label.getFont().getStringWidth( label.getText());  // Recalculate text width
    }

}

void CustomSlider::buildLabel()
{
    using namespace FontEditor::ParameterLabels;
    
    // SLIDER ======================================================
    slider.setSliderStyle(juce::Slider::SliderStyle::LinearBar);
    slider.addListener(this);
    addAndMakeVisible(slider);

	// LABEL ======================================================
	label.setEditable(false, true);
	label.setColour(juce::Label::ColourIds::backgroundColourId, juce::Colours::white.withAlpha(0.f));
	label.setColour(juce::Label::ColourIds::outlineWhenEditingColourId, juce::Colours::white.withAlpha(0.f));
	label.setColour(juce::Label::ColourIds::outlineColourId, juce::Colours::white.withAlpha(0.f));

	label.setColour(juce::Label::ColourIds::textColourId, getFontColor());
	label.setColour(juce::Label::ColourIds::textWhenEditingColourId, juce::Colours::white);
	label.setColour(juce::Label::ColourIds::backgroundWhenEditingColourId, juce::Colours::black);

	label.setJustificationType(getJustification());

	auto fontSize = slider.getHeight() * 0.83f;  // the same size as before the sliders got shorter
	auto titleFont = juce::Font(getTypeface(), fontSize, getFontStyle());

	// Set the initial font size
	label.setFont(titleFont);

	// Measure the text width and adjust font size if needed
	auto labelWidth = label.getWidth();
	auto textWidth = label.getFont().getStringWidth(label.getText());

	while (textWidth > labelWidth && fontSize > 1.0f)
	{
		fontSize -= 2.5f; // Reduce font size
		titleFont.setHeight(fontSize);
		label.setFont(titleFont); // Update label's font
		textWidth = label.getFont().getStringWidth(label.getText()); // Recalculate text width
	}

	label.addListener(this);
	addAndMakeVisible(label);
}

/* =========================================================================== */
/* =========================================================================== */

// Draw the Background Appearance of Option Buttons
void drawOptionButtonBackground(	juce::Graphics& g, 
									juce::Rectangle<int> bounds, 
									bool selected, 
									juce::Slider& slider, 
									int bandMode )
{
	using namespace AllColors::SliderColors;
    using namespace Gradients::Icons;

	auto sliderBounds = slider.getLocalBounds().toFloat();

	// Set Color-Coded Gradient
	if (slider.isEnabled())
	{
		switch (bandMode)
		{
		case 0: {g.setGradientFill(makeIconGradient(sliderBounds.toFloat(), "LOW"    ));	break; }
		case 1: {g.setGradientFill(makeIconGradient(sliderBounds.toFloat(), "MID"    ));	break; }
		case 2: {g.setGradientFill(makeIconGradient(sliderBounds.toFloat(), "HIGH"   ));    break; }
		}
	}
	else
		g.setGradientFill(OPTION_SLIDER_GRADIENT_BYPASS(sliderBounds.toFloat()));

	// Fill Rectangle With Color-Coded Gradient
	g.setOpacity(0.65f);
	g.fillRoundedRectangle(bounds.toFloat(), 2);

	// Highlight Icon If Selected
	if (selected && slider.isEnabled())
	{
		g.setColour(juce::Colours::white);
		g.setOpacity(0.65f);
		g.fillRoundedRectangle(bounds.toFloat(), 2);
	}

	g.setOpacity(1.f);
}

/* =========================================================================== */
/* =========================================================================== */

void WaveLookAndFeel::drawLinearSlider(	juce::Graphics& g, int x, int y, int width, int height,
										float sliderPos, float minSliderPos, float maxSliderPos,
										const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{
	using namespace juce;

	slider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);

	Rectangle<int> bounds = slider.getLocalBounds();		// Bounding Box
	Rectangle<int> sliderBounds = { x, y, width, height };	// Slider

	auto iconSize = abs((sliderBounds.getX() - bounds.getX()) * 2.f);

	// SET ICON SIZE ================================================================
	wave0.setSize(iconSize, iconSize);
	wave1.setSize(iconSize, iconSize);
	wave2.setSize(iconSize, iconSize);
	wave3.setSize(iconSize, iconSize);
	wave4.setSize(iconSize, iconSize);
	wave5.setSize(iconSize, iconSize);
	wave6.setSize(iconSize, iconSize);

	// SET ICON POSITIONS ============================================================
	auto spacing = sliderBounds.getWidth() / 6.f;

	wave0.setCentre(sliderBounds.getX() + 0 * spacing, sliderBounds.getCentreY());
	wave1.setCentre(sliderBounds.getX() + 1 * spacing, sliderBounds.getCentreY());
	wave2.setCentre(sliderBounds.getX() + 2 * spacing, sliderBounds.getCentreY());
	wave3.setCentre(sliderBounds.getX() + 3 * spacing, sliderBounds.getCentreY());
	wave4.setCentre(sliderBounds.getX() + 4 * spacing, sliderBounds.getCentreY());
	wave5.setCentre(sliderBounds.getX() + 5 * spacing, sliderBounds.getCentreY());
	wave6.setCentre(sliderBounds.getX() + 6 * spacing, sliderBounds.getCentreY());

	// DRAW ICONS ====================================================================
	int select = slider.getValue();
	
	drawWave0(g, wave0, select, slider);
	drawWave1(g, wave1, select, slider);
	drawWave2(g, wave2, select, slider);
	drawWave3(g, wave3, select, slider);
	drawWave4(g, wave4, select, slider);
	drawWave5(g, wave5, select, slider);
	drawWave6(g, wave6, select, slider);
}

void WaveLookAndFeel::drawWave0(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 0) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon ===============================
	//auto x =		bounds.getX();
	auto y =		bounds.getY();
	//auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	auto xC =		bounds.getCentreX();
	//auto yC =		bounds.getCentreY();


	juce::Line<float> arrow;
	arrow.setEnd(xC, y + height-3);
	arrow.setStart(xC, y+3);

	g.setColour(juce::Colours::black);
	g.drawArrow(arrow, 3.f, bounds.getWidth() * 0.65f, bounds.getHeight() * 0.45f);
}

void WaveLookAndFeel::drawWave1(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 1) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon
	//auto x =		bounds.getX();
	auto y =		bounds.getY();
	//auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	auto xC =		bounds.getCentreX();
	//auto yC =		bounds.getCentreY();
	
	juce::Line<float> arrow;
	arrow.setStart(xC, y + height-3);
	arrow.setEnd(xC, y+3);
	
	g.setColour(juce::Colours::black);
	g.drawArrow(arrow, 3.f, bounds.getWidth()*0.65f, bounds.getHeight()*0.45f);
}

void WaveLookAndFeel::drawWave2(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 2) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	//auto x =		bounds.getX();
	//auto y =		bounds.getY();
	auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	auto xC =		bounds.getCentreX();
	auto yC =		bounds.getCentreY();

	juce::Rectangle<float> square;
	square.setSize(width-8, height-8);
	square.setCentre(xC, yC);
	
	g.setColour(juce::Colours::black);
	g.fillRect(square);
}

void WaveLookAndFeel::drawWave3(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 3) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	auto x =		bounds.getX();
	auto y =		bounds.getY();
	auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	auto xC =		bounds.getCentreX();
	//auto yC =		bounds.getCentreY();

	juce::Path triangle;
	triangle.startNewSubPath(xC, y+4);
	triangle.lineTo(x + width - 4, y + height - 4);
	triangle.lineTo(x + 4, y + height - 4);
	triangle.closeSubPath();

	g.setColour(juce::Colours::black);
	g.fillPath(triangle);
}

void WaveLookAndFeel::drawWave4(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 4) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	auto x =		bounds.getX();
	auto y =		bounds.getY();
	auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	//auto xC =		bounds.getCentreX();
	//auto yC =		bounds.getCentreY();


	juce::Path p;

	// Second Half Of Sine Wave
	p.addArc(	0, 0, 
				20, 20, 
				-1.f * juce::MathConstants<float>::pi / 2.f,
				juce::MathConstants<float>::pi / 2.f, true);

	p.applyTransform(juce::AffineTransform().rotated(juce::MathConstants<float>::pi, 20, 10));

	// First Half Of Sine Wave
	p.addArc(	0, 0, 
				20, 20, 
				-1.f * juce::MathConstants<float>::pi / 2.f, 
				juce::MathConstants<float>::pi / 2.f, true);

	p.scaleToFit(x+4, y+4, width-8, height-8, false);

	g.setColour(juce::Colours::black);
	g.strokePath(p, juce::PathStrokeType(2.f));
}

void WaveLookAndFeel::drawWave5(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 5) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	auto x =		bounds.getX();
	auto y =		bounds.getY();
	auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	auto xC =		bounds.getCentreX();
	//auto yC =		bounds.getCentreY();

	g.setColour(juce::Colours::black);

	// Add Arc
	juce::Path p;
	p.addCentredArc(	xC, 
						y + height, 
						width, 
						height, 
						0.f, 
						-1.f * juce::MathConstants<float>::pi / 2.f, 
						juce::MathConstants<float>::pi / 2.f, 
						true);

	p.scaleToFit(x+5, y+5, width-10, height- 10, false);
	g.strokePath(p, juce::PathStrokeType(2.f));
}

void WaveLookAndFeel::drawWave6(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 6) 
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	auto x =		bounds.getX();
	auto y =		bounds.getY();
	auto width =	bounds.getWidth();
	auto height =	bounds.getHeight();
	auto xC =		bounds.getCentreX();
	//auto yC =		bounds.getCentreY();

	g.setColour(juce::Colours::black);

	// Add Arc
	juce::Path p;
	p.addCentredArc(	xC, 
						y + height, 
						width, 
						height,
						juce::MathConstants<float>::pi, 
						-1.f * juce::MathConstants<float>::pi / 2.f, 
						juce::MathConstants<float>::pi / 2.f, 
						true);

	p.scaleToFit(x+5, y+5, width-10, height- 10, false);
	g.strokePath(p, juce::PathStrokeType(2.f));
}

void WaveLookAndFeel::setBandMode(int bandMode)
{
	mode = bandMode;
}

/* =========================================================================== */
/* =========================================================================== */

MultLookAndFeel::MultLookAndFeel()
{
	//imageMultIcon0 = juce::ImageCache::getFromMemory(BinaryData::Mult0_png, BinaryData::Mult0_pngSize);
	//imageMultIcon1 = juce::ImageCache::getFromMemory(BinaryData::Mult1_png, BinaryData::Mult1_pngSize);
	//imageMultIcon2 = juce::ImageCache::getFromMemory(BinaryData::Mult2_png, BinaryData::Mult2_pngSize);
	//imageMultIcon3 = juce::ImageCache::getFromMemory(BinaryData::Mult3_png, BinaryData::Mult3_pngSize);
	//imageMultIcon4 = juce::ImageCache::getFromMemory(BinaryData::Mult4_png, BinaryData::Mult4_pngSize);
	//imageMultIcon5 = juce::ImageCache::getFromMemory(BinaryData::Mult5_png, BinaryData::Mult5_pngSize);
}

void MultLookAndFeel::drawLinearSlider(	juce::Graphics& g, int x, int y, int width, int height,
										float sliderPos, float minSliderPos, float maxSliderPos,
										const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{
	using namespace juce;

	slider.setTextBoxStyle(Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
	Rectangle<int> bounds = slider.getLocalBounds();		// Bounding Box
	Rectangle<int> sliderBounds = { x, y, width, height };	// Slider

	auto iconSize = abs((sliderBounds.getX() - bounds.getX()) * 2.f);

	// SET ICON SIZE ================================================================
	mult0.setSize(iconSize, iconSize);
	mult1.setSize(iconSize, iconSize);
	mult2.setSize(iconSize, iconSize);
	mult3.setSize(iconSize, iconSize);
	mult4.setSize(iconSize, iconSize);
	mult5.setSize(iconSize, iconSize);

	// SET ICON POSITIONS ============================================================
	auto spacing = sliderBounds.getWidth() / 5.f;

	mult0.setCentre(sliderBounds.getX() + 0 * spacing, sliderBounds.getCentreY());
	mult1.setCentre(sliderBounds.getX() + 1 * spacing, sliderBounds.getCentreY());
	mult2.setCentre(sliderBounds.getX() + 2 * spacing, sliderBounds.getCentreY());
	mult3.setCentre(sliderBounds.getX() + 3 * spacing, sliderBounds.getCentreY());
	mult4.setCentre(sliderBounds.getX() + 4 * spacing, sliderBounds.getCentreY());
	mult5.setCentre(sliderBounds.getX() + 5 * spacing, sliderBounds.getCentreY());

	// DRAW ICONS ====================================================================
	int select = slider.getValue();
	
	g.setOpacity(1.f);
	drawMult0(g, mult0, select, slider);
	drawMult1(g, mult1, select, slider);
	drawMult2(g, mult2, select, slider);
	drawMult3(g, mult3, select, slider);
	drawMult4(g, mult4, select, slider);
	drawMult5(g, mult5, select, slider);
}

void MultLookAndFeel::drawMult0(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 0)
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon
	g.drawImage(imageMultIcon0, bounds.toFloat());
}

void MultLookAndFeel::drawMult1(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 1)
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon
	g.drawImage(imageMultIcon1, bounds.toFloat());
}

void MultLookAndFeel::drawMult2(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 2)
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon
	g.drawImage(imageMultIcon2, bounds.toFloat());
}

void MultLookAndFeel::drawMult3(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 3)
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon
	g.drawImage(imageMultIcon3, bounds.toFloat());
}

void MultLookAndFeel::drawMult4(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 4)
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);

	// Draw Icon
	g.drawImage(imageMultIcon4, bounds.toFloat());
}

void MultLookAndFeel::drawMult5(juce::Graphics& g, juce::Rectangle<int> bounds, int selection, juce::Slider& slider)
{
	bool selected{ false };

	if (selection == 5)
		selected = true;

	// Draw Icon Background
	drawOptionButtonBackground(g, bounds, selected, slider, mode);
	
	// Draw Icon
	g.drawImage(imageMultIcon5, bounds.toFloat());
}

void MultLookAndFeel::setBandMode(int bandMode)
{
	mode = bandMode;
}

/* =========================================================================== */
/* =========================================================================== */

// [Claude 2026-10-29] The bar sliders, reworked (Joe: "Slider"): a recessed track, the band's fill, and a bright line
// at the value, so a slider at centre (Skew, Phase) still shows where it is instead of two empty boxes.
static juce::Colour bandColourFor(int mode, bool enabled)
{
	using namespace ColorScheme::BandColors;
	if (! enabled) return getBypassBaseColor();
	return mode == 0 ? getLowBandBaseColor() : mode == 1 ? getMidBandBaseColor() : getHighBandBaseColor();
}

static void drawRecessedTrack(juce::Graphics& g, juce::Rectangle<float> r)
{
	// one smooth fall of light, darker at the top, and nothing drawn along the edges but the outline (Joe: the banded
	// version "looks poorly rendered and clunky")
	juce::ColourGradient grad(juce::Colour(0xff1b1c21), 0.f, r.getY(), juce::Colour(0xff2c2e34), 0.f, r.getBottom(), false);  // a mid grey, so the empty track stands off the dark column (Joe: "dark slider space on dark background"), like his original
	grad.addColour(0.35, juce::Colour(0xff222328));
	g.setGradientFill(grad);
	g.fillRect(r);
}

static void drawValueLine(juce::Graphics& g, juce::Rectangle<float> r, float x, juce::Colour c)
{
	// a little width: a soft glow either side of a bright core (Joe: "add a pixel or two of width to it with a teeny
	// gradient")
	const float cx = juce::jlimit(r.getX() + 1.f, r.getRight() - 1.f, x);
	const auto glow = c.brighter(0.2f);
	juce::ColourGradient grad(glow.withAlpha(0.f), cx - 3.f, 0.f, glow.withAlpha(0.f), cx + 3.f, 0.f, false);
	grad.addColour(0.5, glow.withAlpha(0.4f));  // quieter, so the bold core reads in the middle (Joe: "leave the bold part in the center")
	g.setGradientFill(grad);
	g.fillRect(juce::Rectangle<float>(cx - 3.f, r.getY(), 6.f, r.getHeight()).getIntersection(r));
	g.setColour(c.brighter(0.4f));
	g.fillRect(juce::Rectangle<float>(cx - 1.f, r.getY(), 2.f, r.getHeight()));
}

// A thin dark edge where the fill ends, with a little shadow falling onto the empty track, as in Joe's original
// (Joe: "a little bit of a dark line or kind of recessed shadow where the end cap is")
static void drawEndShadow(juce::Graphics& g, juce::Rectangle<float> r, float x, int direction)
{
	if (x <= r.getX() + 0.5f || x >= r.getRight() - 0.5f) return;
	const float w = 2.5f, x2 = x + direction * w;  // a short, sharp shade (Joe: "make the edge sharper")
	juce::ColourGradient sh(juce::Colours::black.withAlpha(0.45f), x, 0.f, juce::Colours::black.withAlpha(0.f), x2, 0.f, false);
	g.setGradientFill(sh);
	g.fillRect(juce::Rectangle<float>(juce::jmin(x, x2), r.getY(), w, r.getHeight()).getIntersection(r));
	g.setColour(juce::Colours::black.withAlpha(0.8f));
	g.fillRect(juce::Rectangle<float>(x - 0.5f, r.getY(), 1.f, r.getHeight()));
}

void StandardSliderLookAndFeel::drawLinearSlider(	juce::Graphics& g, int x, int y, int width, int height,
													float sliderPos, float minSliderPos, float maxSliderPos,
													const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{
	using namespace AllColors::SliderColors;
    using namespace Gradients::BarSliders;

	slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
	slider.setColour(juce::Slider::ColourIds::textBoxOutlineColourId, juce::Colours::black.withAlpha(0.f));

	juce::Rectangle<float> bounds = { (float)x, (float)y, (float)width, (float)height };
	auto valuePosition = juce::jmap((sliderPos / bounds.getWidth()), bounds.getX(), bounds.getRight() - 1);

	drawRecessedTrack(g, bounds);

	juce::Rectangle<float> valueBar = bounds.withRight(valuePosition);
	if (slider.isEnabled())
	{
		switch (mode)
		{
		case 0: {g.setGradientFill( makeStandardSliderGradient  (bounds, "LOW"  ));	break; }
        case 1: {g.setGradientFill( makeStandardSliderGradient  (bounds, "MID"  )); break; }
		case 2: {g.setGradientFill( makeStandardSliderGradient  (bounds, "HIGH" )); break; }
		}
	}
	else
		g.setGradientFill(BASIC_SLIDER_GRADIENT_BYPASS(bounds));
	g.fillRect(valueBar);
	// no bright cap at the value: the fill simply ends, as in Joe's original (Joe: "I don't want that big bright cap")
	drawEndShadow(g, bounds, valuePosition, -1);  // the shade falls back into the fill, where it shows
}

void StandardSliderLookAndFeel::setBandMode(int bandMode)
{
	mode = bandMode;
}

/* =========================================================================== */
/* =========================================================================== */

void CenterSliderLookAndFeel::drawLinearSlider(	juce::Graphics& g, int x, int y, int width, int height,
												float sliderPos, float minSliderPos, float maxSliderPos,
												const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{
	using namespace AllColors::SliderColors;
    using namespace Gradients::BarSliders;

	slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
	slider.setColour(juce::Slider::ColourIds::textBoxOutlineColourId, juce::Colours::black.withAlpha(0.f));

	juce::Rectangle<float> bounds = { (float)x, (float)y, (float)width, (float)height };
	auto valuePosition = juce::jmap((sliderPos / bounds.getWidth()), bounds.getX(), bounds.getRight() - 1);

	drawRecessedTrack(g, bounds);

	// The fill runs from the centre to the value
	juce::Rectangle<float> valueBar = juce::Rectangle<float>::leftTopRightBottom(
		juce::jmin(bounds.getCentreX(), valuePosition), bounds.getY(), juce::jmax(bounds.getCentreX(), valuePosition), bounds.getBottom());
	switch (mode)
	{
    case 0: g.setGradientFill( makeCenterSliderGradient  (bounds, "LOW"  ));    break;
    case 1: g.setGradientFill( makeCenterSliderGradient  (bounds, "MID"  ));    break;
    case 2: g.setGradientFill( makeCenterSliderGradient  (bounds, "HIGH" ));    break;
	}
	if (slider.isEnabled())
		g.fillRect(valueBar);
	if (std::abs(valuePosition - bounds.getCentreX()) >= 2.f)
		drawEndShadow(g, bounds, valuePosition, valuePosition > bounds.getCentreX() ? -1 : +1);

	// A fine centre mark in place of the old 7 px groove, then the value line on top
	g.setColour(juce::Colours::white.withAlpha(0.14f));
	g.fillRect(juce::Rectangle<float>(bounds.getCentreX() - 0.5f, bounds.getY(), 1.f, bounds.getHeight()));
	// the marker only where there is no fill to show the value, at the centre; otherwise the fill ends plainly
	if (std::abs(valuePosition - bounds.getCentreX()) < 2.f)
		drawValueLine(g, bounds, valuePosition, bandColourFor(mode, slider.isEnabled()));
}

void CenterSliderLookAndFeel::setBandMode(int bandMode)
{
	mode = bandMode;
}

/* =========================================================================== */
/* =========================================================================== */

void ButtonOptionsLookAndFeel::drawButtonBackground(juce::Graphics& g,
													juce::Button& button,
													const juce::Colour& backgroundColour,
													bool shouldDrawButtonAsHighlighted,
													bool shouldDrawButtonAsDown)
{
	// [Claude 2026-10-29] The Options key in the header's style (Joe: "What about the Options menu that shows up in the
	// corner?"): a small raised grey key with a lit top edge, pressed darker. Was a white bevelled block.
	auto r = button.getLocalBounds().toFloat().reduced(0.5f);
	const auto top = shouldDrawButtonAsDown ? juce::Colour(0xff26282e) : juce::Colour(0xff34363e);
	const auto bot = shouldDrawButtonAsDown ? juce::Colour(0xff1f2126) : juce::Colour(0xff25272d);
	g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
	g.fillRoundedRectangle(r, 1.5f);
	g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.16f : 0.1f));
	g.drawLine(r.getX() + 1.5f, r.getY() + 0.75f, r.getRight() - 1.5f, r.getY() + 0.75f, 1.f);
	g.setColour(juce::Colours::black.withAlpha(0.7f));
	g.drawRoundedRectangle(r, 1.5f, 1.f);
}

void ButtonOptionsLookAndFeel::drawButtonText(	juce::Graphics& g,
												juce::TextButton& button,
												bool shouldDrawButtonAsHighlighted,
												bool shouldDrawButtonAsDown)
{
	auto bounds = button.getLocalBounds().toFloat();
	g.setColour(juce::Colour(0xffc9cbd1));
	g.setFont(juce::Font(11.f));
	g.drawText("Options", bounds.withTrimmedRight(11.f), juce::Justification::centred, false);
	// a small chevron
	const float cx = bounds.getRight() - 9.f, cy = bounds.getCentreY();
	juce::Path chevron;
	chevron.startNewSubPath(cx - 2.5f, cy - 1.25f);
	chevron.lineTo(cx, cy + 1.25f);
	chevron.lineTo(cx + 2.5f, cy - 1.25f);
	g.setColour(juce::Colour(0xff7d808a));
	g.strokePath(chevron, juce::PathStrokeType(1.2f));
}

void ButtonOptionsLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
												bool shouldDrawButtonAsHighlighted, bool)
{
	auto r = button.getLocalBounds().toFloat();
	if (shouldDrawButtonAsHighlighted)
	{
		g.setColour(juce::Colours::white.withAlpha(0.07f));
		g.fillRect(r.reduced(1.f, 0.f));
	}
	// a small square box at the left: empty when off, filled light with a dark tick when on (Joe: "show the unchecked
	// as empty toggles")
	const juce::Rectangle<float> box { r.getX() + 4.f, r.getCentreY() - 4.5f, 9.f, 9.f };
	if (button.getToggleState())
	{
		g.setColour(juce::Colour(0xffe3e4e8));
		g.fillRoundedRectangle(box, 1.f);
		juce::Path tick;
		const float x = box.getCentreX(), y = box.getCentreY();
		tick.startNewSubPath(x - 2.5f, y);
		tick.lineTo(x - 0.75f, y + 1.75f);
		tick.lineTo(x + 2.5f, y - 2.f);
		g.setColour(juce::Colour(0xff1d1f25));
		g.strokePath(tick, juce::PathStrokeType(1.25f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
	}
	else
	{
		g.setColour(juce::Colours::white.withAlpha(0.38f));
		g.drawRoundedRectangle(box.reduced(0.5f), 1.f, 1.f);
	}
	g.setColour(juce::Colour(button.getToggleState() ? 0xffe3e4e8 : 0xff9a9da6));
	g.setFont(juce::Font(11.f));
	g.drawText(button.getButtonText(), r.withTrimmedLeft(17.f), juce::Justification::centredLeft, false);
}

/* =========================================================================== */
/* =========================================================================== */

void ScrollLookAndFeel::drawLinearSlider(	juce::Graphics& g, int x, int y, int width, int height,
											float sliderPos, float minSliderPos, float maxSliderPos,
											const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{

}

/* =========================================================================== */
/* =========================================================================== */

InOutLookAndFeel::InOutLookAndFeel()
{
	imageFader = juce::ImageCache::getFromMemory(BinaryData::FaderKnob_png, BinaryData::FaderKnob_pngSize);
}

void InOutLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
	float sliderPos, float minSliderPos, float maxSliderPos,
	const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{
	// Set Textbox Style
	slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
	slider.setColour(juce::Slider::ColourIds::textBoxOutlineColourId, juce::Colours::hotpink.withAlpha(0.f));

	// Establish Bounds
	juce::Rectangle<float> bounds = { (float)x, (float)y, (float)width, (float)height };

	// Draw Track & Fader
	auto faderWidth = bounds.getWidth();
	auto faderHeight = faderWidth*2.f;

	auto sliderPosScaled = (sliderPos - bounds.getY()) / bounds.getHeight();

	auto faderRange = juce::jmap(sliderPosScaled, bounds.getY() + 25, bounds.getBottom() - 25);

	juce::Path track;
	track.startNewSubPath(bounds.getCentreX(), bounds.getY() + 10);
	track.lineTo(bounds.getCentreX(), bounds.getBottom() - 10);
	g.setColour(juce::Colours::black.withAlpha(0.85f));
	g.strokePath(track, juce::PathStrokeType(	3.f, juce::PathStrokeType::JointStyle::beveled, 
												juce::PathStrokeType::EndCapStyle::rounded));

	// [Claude 2026-10-29] A drawn cap in place of the chrome picture (FaderKnob.png): a short dark key, lit along its
	// top edge, with one white index line at the value.
	const float capW = faderWidth * 0.86f, capH = faderWidth * 0.62f;
	juce::Rectangle<float> cap { bounds.getCentreX() - capW / 2.f, faderRange - capH / 2.f, capW, capH };
	g.setColour(juce::Colours::black.withAlpha(0.6f));
	g.fillRoundedRectangle(cap.translated(0.f, 1.5f), 2.f);
	g.setGradientFill(juce::ColourGradient(juce::Colour(0xff40434b), cap.getX(), cap.getY(),
										   juce::Colour(0xff24262c), cap.getX(), cap.getBottom(), false));
	g.fillRoundedRectangle(cap, 2.f);
	g.setColour(juce::Colours::white.withAlpha(0.12f));
	g.drawLine(cap.getX() + 2.f, cap.getY() + 0.75f, cap.getRight() - 2.f, cap.getY() + 0.75f, 1.f);
	g.setColour(juce::Colours::black.withAlpha(0.8f));
	g.drawRoundedRectangle(cap, 2.f, 1.f);
	g.setColour(juce::Colours::white.withAlpha(0.9f));
	g.drawLine(cap.getX() + 2.f, cap.getCentreY(), cap.getRight() - 2.f, cap.getCentreY(), 1.25f);
}

/* =========================================================================== */
/* =========================================================================== */


void CrossoverLookAndFeel::drawRotarySlider(	juce::Graphics& g, int x, int y, int width, int height,
												float sliderPosProportional, float rotaryStartAngle,
												float rotaryEndAngle, juce::Slider& slider)
{
	using namespace juce;

	slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, false, 0, 0);

	auto bounds = Rectangle<float>(x, y, width, height).toFloat();
	bounds.removeFromTop(10);
	bounds.removeFromBottom(5);

	// Use parent bounds as bounds of slider.  Square off and center.
	juce::Rectangle<float> ellipseBounds = { bounds.getX(), bounds.getY(), bounds.getHeight(), bounds.getHeight() };
	ellipseBounds.setCentre(bounds.getCentreX(), ellipseBounds.getCentreY());

	// Draw Outer Ring (Fixed Background)
	Path p1;

	p1.addArc(	ellipseBounds.getX(), 
				ellipseBounds.getY(), 
				ellipseBounds.getWidth(), 
				ellipseBounds.getHeight(), 
				rotaryStartAngle, rotaryEndAngle,
				true);

	g.setColour(juce::Colours::grey);
	g.strokePath(p1, juce::PathStrokeType(	4.f, 
											juce::PathStrokeType::JointStyle::beveled, 
											juce::PathStrokeType::EndCapStyle::rounded ) );

	// Draw Outer Ring (Value-Based)
	Path p2;

	// Current Angle of Parameter Selection
	auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

	p2.addArc(	ellipseBounds.getX(),
				ellipseBounds.getY(), 
				ellipseBounds.getWidth(), 
				ellipseBounds.getHeight(), 
				rotaryStartAngle, sliderAngRad,
				true);

	g.setColour(juce::Colour(0xff2530bd));
	g.strokePath(p2, juce::PathStrokeType(	5.f,
											juce::PathStrokeType::JointStyle::beveled, 
											juce::PathStrokeType::EndCapStyle::rounded ) );
}




// [Claude 2026-10-29] A medium weight for the button words, between plain and bold (Joe: "a little less bold"):
// Segoe UI Semibold on Windows, Helvetica Neue Medium on the Mac.
static juce::Font buttonWordFont(float size)
{
   #if JUCE_WINDOWS
    return juce::Font("Segoe UI", size, juce::Font::plain).withTypefaceStyle("Semibold");
   #else
    return juce::Font("Helvetica Neue", size, juce::Font::plain).withTypefaceStyle("Medium");
   #endif
}

void ToggleLookAndFeel::drawToggleButton (  juce::Graphics& g, juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    using namespace ColorScheme::BandColors;
    using namespace AllColors::SliderColors;

    auto bounds = button.getLocalBounds();
    
    drawTickBox(    g, button,
                    bounds.getX(), bounds.getY(),
                    bounds.getWidth(), bounds.getHeight(),
                    button.getToggleState(), button.isEnabled(),
                    false, button.getState());
}

// [Claude 2026-10-29] Keys (Joe: "Buttons"): a lit key is the band colour lit from the top with a highlight along its top
// edge and light text; an unlit key is a dark grey key, not pitch black. Shared by the toggles and the choice menus.
static void drawLitKey(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
	g.setGradientFill(juce::ColourGradient(c.brighter(0.12f), 0.f, r.getY(), c.darker(0.28f), 0.f, r.getBottom(), false));
	g.fillRect(r);
	g.setColour(juce::Colours::white.withAlpha(0.22f));
	g.fillRect(r.withHeight(1.f).reduced(1.f, 0.f));
}

static void drawDarkKey(juce::Graphics& g, juce::Rectangle<float> r)
{
	g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2d33), 0.f, r.getY(), juce::Colour(0xff1d1f24), 0.f, r.getBottom(), false));
	g.fillRect(r);
	g.setColour(juce::Colours::white.withAlpha(0.07f));
	g.fillRect(r.withHeight(1.f).reduced(1.f, 0.f));
}

static const juce::Colour litKeyText { 0xff0e0f12 }, darkKeyText { 0xffa9abb3 };  // dark text on lit keys (Joe: "the white text is terrible")

void ToggleLookAndFeel::drawTickBox (   juce::Graphics& g, juce::Component& button,
                                        float x, float y, float w, float h,
                                        bool ticked, bool isEnabled,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    auto bounds = juce::Rectangle<float>{x, y, w, h};
    
    using namespace ColorScheme::BandColors;
	using namespace AllColors::SliderColors;
    using namespace juce;
    
    juce::Colour baseColor;
    
	if (isEnabled)
	{
		switch (mode)
		{
			case 0: {baseColor = getLowBandBaseColor();  break;}
			case 1: {baseColor = getMidBandBaseColor();  break;}
			case 2: {baseColor = getHighBandBaseColor(); break;}
		}
	} 
	else
          baseColor = ColorScheme::BandColors::getBypassBaseColor();

    if (ticked)
        drawLitKey(g, bounds, baseColor);
    else
        drawDarkKey(g, bounds);

    g.setColour(ticked ? litKeyText : darkKeyText);

    // Start with a font size based on height (INV)
	// [Claude 2026-10-29] Button words as small spaced capitals, not big type (Joe: "Now the INV etc seem too large").
	if (mLabel == "INV")
	{
		float fontSize = bounds.getHeight() * 0.43f;
		// a touch lighter: the font's semibold face where there is one, else plain (Joe: "a little less bold")
		g.setFont(buttonWordFont(fontSize).withExtraKerningFactor(0.12f));

		auto textWidth = g.getCurrentFont().getStringWidth(mLabel);

		// Adjust the font size if the text width exceeds the bounds width
		while (textWidth > bounds.getWidth()*0.9f && fontSize > 1.0f) {
			fontSize -= 1.5f; // Reduce the font size
			g.setFont(fontSize); // Set the new font size
			textWidth = g.getCurrentFont().getStringWidth(mLabel); // Re-measure the text width
		}
	}


    // Adjust the font size if the text width exceeds the bounds width (Byp, Solo, Mute buttons)
    if (mLabel != "INV") {
		float fontSize = bounds.getHeight() * 0.37f;
		g.setFont(buttonWordFont(fontSize).withExtraKerningFactor(0.12f));
    }

    // Draw the text centered within the bounds
    g.drawFittedText(mLabel, bounds.toNearestInt(), juce::Justification::centred, 1);

    // [Claude 2026-10-29] one crisp dark outline, as on the menus and sliders
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRect(bounds, 1.f);
}

void ToggleLookAndFeel::setBandMode(int bandMode, juce::String label)
{
    mode = bandMode;
    mLabel = label;
}

CustomToggle::CustomToggle()
{
    mToggleButton.setLookAndFeel(&toggleLookAndFeel);
    addAndMakeVisible(mToggleButton);
}

CustomToggle::~CustomToggle()
{
    mToggleButton.setLookAndFeel(nullptr);
}

void CustomToggle::resized()
{
    mToggleButton.setSize(getWidth(), getHeight());
    mToggleButton.setTopLeftPosition(0,0);
}

void CustomToggle::setBandMode(int bandMode, juce::String label)
{
    toggleLookAndFeel.setBandMode(bandMode, label);
}

MultComboLookAndFeel::MultComboLookAndFeel()
{
    //imageMultIcon0 = juce::ImageCache::getFromMemory(BinaryData::Mult0_png, BinaryData::Mult0_pngSize);
    //imageMultIcon1 = juce::ImageCache::getFromMemory(BinaryData::Mult1_png, BinaryData::Mult1_pngSize);
    //imageMultIcon2 = juce::ImageCache::getFromMemory(BinaryData::Mult2_png, BinaryData::Mult2_pngSize);
    //imageMultIcon3 = juce::ImageCache::getFromMemory(BinaryData::Mult3_png, BinaryData::Mult3_pngSize);
    //imageMultIcon4 = juce::ImageCache::getFromMemory(BinaryData::Mult4_png, BinaryData::Mult4_pngSize);
    //imageMultIcon5 = juce::ImageCache::getFromMemory(BinaryData::Mult5_png, BinaryData::Mult5_pngSize);
}

void MultComboLookAndFeel::drawComboBox ( juce::Graphics& g,
                                    int width, int height,
                                    bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH,
                                    juce::ComboBox& comboBox)
{
    using namespace juce;
    using namespace ColorScheme::BandColors;
	using namespace AllColors::SliderColors;
    using namespace Gradients::Icons;

    auto bounds = comboBox.getLocalBounds();
    //auto menuBounds = bounds.withTrimmedRight(5);

	auto trimScale = 42 / 180.f;
    auto trimWidth = bounds.getWidth() * trimScale;

	auto menuBounds = bounds.withTrimmedRight(trimWidth);

    /* Provide a "Recessed" appearance */
    
    juce::Colour baseColour;
    
	if (comboBox.isEnabled())
	{
		switch (mode)
		{
			case 0: baseColour = getLowBandBaseColor(); break;
			case 1: baseColour = getMidBandBaseColor(); break;
			case 2: baseColour = getHighBandBaseColor(); break;
		}
	}
    else
	    g.setGradientFill(OPTION_SLIDER_GRADIENT_BYPASS(bounds.toFloat()));

    drawLitKey(g, menuBounds.toFloat(), comboBox.isEnabled() ? baseColour : ColorScheme::BandColors::getBypassBaseColor());
    
    // [Claude 2026-10-29] one crisp dark outline, like the keys, in place of the stacked grey bevel (Joe: "Let's try it")
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRect(menuBounds, 1);
    menuBounds.reduce(1, 1);
    
	g.setColour(litKeyText);
    
    juce::String selection = comboBox.getItemText(comboBox.getSelectedItemIndex());
    g.setFont(height * 0.56f);
    g.drawFittedText(selection, menuBounds, juce::Justification::centred, 1);
}

void MultComboLookAndFeel::setMode(int bandMode)
{
    mode = bandMode;
}






void WaveComboLookAndFeel::drawComboBox ( juce::Graphics& g,
                                    int width, int height,
                                    bool isButtonDown,
                                    int buttonX, int buttonY, int buttonW, int buttonH,
                                    juce::ComboBox& comboBox)
{
    using namespace juce;
    using namespace ColorScheme::BandColors;
	using namespace AllColors::SliderColors;
    using namespace Gradients::Icons;
    
    auto bounds = comboBox.getLocalBounds();
    /* Provide a "Recessed" appearance */
    
    juce::Colour baseColour;
    
	if (comboBox.isEnabled())
	{
		switch (mode)
		{
			case 0: baseColour = getLowBandBaseColor(); break;
			case 1: baseColour = getMidBandBaseColor(); break;
			case 2: baseColour = getHighBandBaseColor(); break;
		}
	}
    else
	    g.setGradientFill(OPTION_SLIDER_GRADIENT_BYPASS(bounds.toFloat()));
    
    drawLitKey(g, bounds.toFloat(), comboBox.isEnabled() ? baseColour : ColorScheme::BandColors::getBypassBaseColor());
    
    // [Claude 2026-10-29] one crisp dark outline, like the keys, in place of the stacked grey bevel
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.drawRect(bounds, 1);
    bounds.reduce(1, 1);
    
	g.setColour(litKeyText);
    
    juce::String selection = comboBox.getItemText(comboBox.getSelectedItemIndex());
    g.setFont(height * 0.56f);
    g.drawFittedText(selection, bounds, juce::Justification::centred, 1);
    
}

void WaveComboLookAndFeel::setMode(int bandMode)
{
    mode = bandMode;
}
