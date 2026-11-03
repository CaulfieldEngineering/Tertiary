/*
  ==============================================================================

    MyLookAndFeel.cpp
    Created: 19 Jan 2022 4:55:59pm
    Author:  Joe

  ==============================================================================
*/

#include "MyLookAndFeel.h"
#include "UtilityFunctions.h"

static void drawOutline(juce::Graphics& g, juce::Rectangle<float> snapped);   // [Claude 2026-10-29] defined with the keys


CustomSlider::CustomSlider()
{
    buildLabel();
}

CustomSlider::~CustomSlider()
{
	slider.setLookAndFeel(nullptr);
	label.setLookAndFeel(nullptr);   // [Claude 2026-10-29]
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
    // [Claude 2026-10-29] one crisp dark outline on every key, menu and slider (Joe chose it over the grey frames), on
    // screen pixels, round the same snapped rectangle the track fills
    drawOutline(g, snapToScreenPixels(g, slider.getBounds().toFloat(), this));
    
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

	label.setLookAndFeel(&HouseLabelLookAndFeel::get());   // [Claude 2026-10-29] the value on whole screen pixels
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

// [Claude 2026-10-29] The lines are laid on whole screen pixels (PixelGrid): the 2 px core and the 1 px edge
// straddled two screen pixels at 125 % and came out soft
static void drawValueLine(juce::Graphics& g, const PixelGrid& grid, juce::Rectangle<float> r, float x, juce::Colour c)
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
	fillVLine(g, grid, cx - 1.f, r.getY(), r.getBottom(), grid.thick(2.f));
}

// A thin dark edge where the fill ends, with a little shadow falling onto the empty track, as in Joe's original
// (Joe: "a little bit of a dark line or kind of recessed shadow where the end cap is")
static void drawEndShadow(juce::Graphics& g, const PixelGrid& grid, juce::Rectangle<float> r, float x, int direction)
{
	if (x <= r.getX() + 0.5f || x >= r.getRight() - 0.5f) return;
	const float w = 2.5f, x2 = x + direction * w;  // a short, sharp shade (Joe: "make the edge sharper")
	juce::ColourGradient sh(juce::Colours::black.withAlpha(0.45f), x, 0.f, juce::Colours::black.withAlpha(0.f), x2, 0.f, false);
	g.setGradientFill(sh);
	g.fillRect(juce::Rectangle<float>(juce::jmin(x, x2), r.getY(), w, r.getHeight()).getIntersection(r));
	g.setColour(juce::Colours::black.withAlpha(0.8f));
	// the edge sits just inside the fill: on the pixel before `x` when the fill is to the left of it
	fillVLine(g, grid, direction < 0 ? x - grid.rulePx() : x, r.getY(), r.getBottom());
}

// [Claude 2026-10-29] Where the fill ends, from the value itself: JUCE's sliderPos is a position inside its
// thumb-indented range, and read as 0..width it left a gap at the ends (Joe: the centre sliders "don't reach the left
// end of the track" at full negative). On a screen pixel, like the track's edges.
static float valueEdge(const PixelGrid& grid, juce::Rectangle<float> track, juce::Slider& slider)
{
	const float p = (float) slider.valueToProportionOfLength(slider.getValue());
	return grid.snapX(track.getX() + juce::jlimit(0.f, 1.f, p) * track.getWidth());
}

void StandardSliderLookAndFeel::drawLinearSlider(	juce::Graphics& g, int x, int y, int width, int height,
													float sliderPos, float minSliderPos, float maxSliderPos,
													const juce::Slider::SliderStyle sliderStyle, juce::Slider& slider)
{
	using namespace AllColors::SliderColors;
    using namespace Gradients::BarSliders;

	slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
	slider.setColour(juce::Slider::ColourIds::textBoxOutlineColourId, juce::Colours::black.withAlpha(0.f));

	// [Claude 2026-10-29] the whole slider, not the area JUCE hands in, which is inset by a layout pixel: at 150 and
	// 200 % that inset became a screen pixel or two of background between the outline and the track
	juce::Rectangle<float> bounds = slider.getLocalBounds().toFloat();
	const PixelGrid grid(g, &slider);   // [Claude 2026-10-29]

	bounds = grid.snap(bounds);   // [Claude 2026-10-29] the fill ends where the outline is
	const float valuePosition = valueEdge(grid, bounds, slider);
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
	drawEndShadow(g, grid, bounds, valuePosition, -1);  // the shade falls back into the fill, where it shows
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

	// [Claude 2026-10-29] the whole slider, as in the standard slider above
	juce::Rectangle<float> bounds = slider.getLocalBounds().toFloat();
	const PixelGrid grid(g, &slider);   // [Claude 2026-10-29]

	bounds = grid.snap(bounds);   // [Claude 2026-10-29] the fill ends where the outline is
	const float valuePosition = valueEdge(grid, bounds, slider);
	const float centreX = grid.snapX(bounds.getCentreX());   // [Claude 2026-10-29] the centre mark and the fill's start on one screen pixel
	drawRecessedTrack(g, bounds);

	// The fill runs from the centre to the value
	juce::Rectangle<float> valueBar = juce::Rectangle<float>::leftTopRightBottom(
		juce::jmin(centreX, valuePosition), bounds.getY(), juce::jmax(centreX, valuePosition), bounds.getBottom());
	switch (mode)
	{
    case 0: g.setGradientFill( makeCenterSliderGradient  (bounds, "LOW"  ));    break;
    case 1: g.setGradientFill( makeCenterSliderGradient  (bounds, "MID"  ));    break;
    case 2: g.setGradientFill( makeCenterSliderGradient  (bounds, "HIGH" ));    break;
	}
	if (slider.isEnabled())
		g.fillRect(valueBar);
	if (std::abs(valuePosition - centreX) >= 2.f)
		drawEndShadow(g, grid, bounds, valuePosition, valuePosition > centreX ? -1 : +1);

	// A fine centre mark in place of the old 7 px groove, then the value line on top
	g.setColour(juce::Colours::white.withAlpha(0.14f));
	fillVLine(g, grid, bounds.getCentreX() - 0.5f, bounds.getY(), bounds.getBottom());
	// the marker only where there is no fill to show the value, at the centre; otherwise the fill ends plainly
	if (std::abs(valuePosition - centreX) < 2.f)
		drawValueLine(g, grid, bounds, valuePosition, bandColourFor(mode, slider.isEnabled()));
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
	const PixelGrid grid(g, &button);   // [Claude 2026-10-29] the edge and its highlight on whole screen pixels
	auto r = grid.strokeRect(button.getLocalBounds().toFloat());
	const auto top = shouldDrawButtonAsDown ? juce::Colour(0xff26282e) : juce::Colour(0xff34363e);
	const auto bot = shouldDrawButtonAsDown ? juce::Colour(0xff1f2126) : juce::Colour(0xff25272d);
	g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
	g.fillRoundedRectangle(r, 1.5f);
	g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.16f : 0.1f));
	fillHLine(g, grid, r.getY() + grid.rulePx() / 2.f, r.getX() + 1.5f, r.getRight() - 1.5f);
	g.setColour(juce::Colours::black.withAlpha(0.7f));
	g.drawRoundedRectangle(r, 1.5f, grid.rulePx());
}

void ButtonOptionsLookAndFeel::drawButtonText(	juce::Graphics& g,
												juce::TextButton& button,
												bool shouldDrawButtonAsHighlighted,
												bool shouldDrawButtonAsDown)
{
	auto bounds = button.getLocalBounds().toFloat();
	g.setColour(juce::Colour(0xffc9cbd1));
	g.setFont(juce::Font(11.f));
	drawTextOnPixels(g, PixelGrid(g, &button), "Options", bounds.withTrimmedRight(11.f), juce::Justification::centred);   // [Claude 2026-10-29]
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
	const PixelGrid grid(g, &button);   // [Claude 2026-10-29] the box on whole screen pixels
	const auto box = grid.snap({ r.getX() + 4.f, r.getCentreY() - 4.5f, 9.f, 9.f });
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
		g.drawRoundedRectangle(grid.strokeRect(box), 1.f, grid.rulePx());
	}
	g.setColour(juce::Colour(button.getToggleState() ? 0xffe3e4e8 : 0xff9a9da6));
	g.setFont(juce::Font(11.f));
	drawTextOnPixels(g, PixelGrid(g, &button), button.getButtonText(), r.withTrimmedLeft(17.f), juce::Justification::centredLeft);   // [Claude 2026-10-29]
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
	// [Claude 2026-10-29] centred on the slider itself (which is centred on the meter), not on the area JUCE hands
	// in, which can sit off to one side (Joe: "get these faders and fader track centered on the meter")
	const auto ledCentre = slider.getProperties()["ledCentreX"];
	const float centreX = ledCentre.isVoid() ? slider.getLocalBounds().toFloat().getCentreX() : (float) (double) ledCentre;
	bounds.setX(centreX - bounds.getWidth() / 2.f);

	// Draw Track & Fader
	auto faderWidth = bounds.getWidth();
	auto faderHeight = faderWidth*2.f;

	// [Claude 2026-10-29] the cap sits right at JUCE's position, which already runs the length of the slot less half a
	// cap at each end (getSliderThumbRadius); it stopped 25 px short of both ends before
	auto faderRange = sliderPos;

	// the track runs the whole slot
	const auto whole = slider.getLocalBounds().toFloat();
	juce::Path track;
	track.startNewSubPath(bounds.getCentreX(), whole.getY() + 3.f);
	track.lineTo(bounds.getCentreX(), whole.getBottom() - 3.f);
	g.setColour(juce::Colours::black.withAlpha(0.85f));
	g.strokePath(track, juce::PathStrokeType(	3.f, juce::PathStrokeType::JointStyle::beveled, 
												juce::PathStrokeType::EndCapStyle::rounded));

	// [Claude 2026-10-29] A drawn cap in place of the chrome picture (FaderKnob.png): a short dark key, lit along its
	// top edge, with one white index line at the value.
	// [Claude 2026-10-29] the cap itself on whole screen pixels, and its highlight, outline and index line with it,
	// so the cap is one crisp key wherever it stops
	const PixelGrid grid(g, &slider);
	const float capW = faderWidth * 0.86f, capH = faderWidth * 0.62f;
	const auto cap = grid.snap({ bounds.getCentreX() - capW / 2.f, faderRange - capH / 2.f, capW, capH });
	g.setColour(juce::Colours::black.withAlpha(0.6f));
	g.fillRoundedRectangle(cap.translated(0.f, 1.5f), 2.f);
	g.setGradientFill(juce::ColourGradient(juce::Colour(0xff40434b), cap.getX(), cap.getY(),
										   juce::Colour(0xff24262c), cap.getX(), cap.getBottom(), false));
	g.fillRoundedRectangle(cap, 2.f);
	g.setColour(juce::Colours::white.withAlpha(0.12f));
	fillHLine(g, grid, cap.getY() + grid.rulePx(), cap.getX() + 2.f, cap.getRight() - 2.f);
	g.setColour(juce::Colours::black.withAlpha(0.8f));
	g.drawRoundedRectangle(grid.strokeRect(cap), 2.f, grid.rulePx());
	g.setColour(juce::Colours::white.withAlpha(0.9f));
	fillHLine(g, grid, cap.getCentreY() - 0.625f, cap.getX() + 2.f, cap.getRight() - 2.f, grid.thick(1.25f));
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
// [Claude 2026-10-29] Text centred on a whole-pixel baseline and left edge. Centring by itself put small text on half
// pixels, which smeared it a pixel up and down (Joe: "the word "Square" on the blue dropdown menu" looked blurry at
// 100 %). Falls back to fitting when the text is wider than its box.
// [Claude 2026-10-29] Now whole *screen* pixels, through drawTextOnPixels (UtilityFunctions), so it holds at 125 and
// 150 % too; `owner` is the key or menu being drawn.
static void drawCrispText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Component* owner)
{
	drawTextOnPixels(g, PixelGrid(g, owner), text, r, juce::Justification::centred);
}

// [Claude 2026-10-29] The keys', menus' and sliders' outline: one screen pixel, on a rectangle already snapped to screen
// pixels with snapToScreenPixels. A 1-layout-pixel line covered only part of a screen pixel at 125 %, and the fill
// showed past it (Joe: "The fill slightly exceeds the black border")
static void drawOutline(juce::Graphics& g, juce::Rectangle<float> snapped)
{
	g.setColour(juce::Colours::black.withAlpha(0.75f));
	g.drawRect(snapped, 1.f / (float) g.getInternalContext().getPhysicalPixelScaleFactor());
}

// [Claude 2026-10-29] the keys' lit top edge: a hairline on whole screen pixels (1 layout pixel covered a pixel and a
// quarter at 125 %)
static void drawLitKey(juce::Graphics& g, const PixelGrid& grid, juce::Rectangle<float> r, juce::Colour c)
{
	c = ColorScheme::BandColors::matchLuminance(c, ColorScheme::BandColors::getHighBandBaseColor());  // [Claude 2026-10-29] even bands
	g.setGradientFill(juce::ColourGradient(c.brighter(0.12f), 0.f, r.getY(), c.darker(0.28f), 0.f, r.getBottom(), false));
	g.fillRect(r);
	g.setColour(juce::Colours::white.withAlpha(0.22f));
	fillHLine(g, grid, r.getY(), r.getX() + 1.f, r.getRight() - 1.f);
}

static void drawDarkKey(juce::Graphics& g, const PixelGrid& grid, juce::Rectangle<float> r)
{
	g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2b2d33), 0.f, r.getY(), juce::Colour(0xff1d1f24), 0.f, r.getBottom(), false));
	g.fillRect(r);
	g.setColour(juce::Colours::white.withAlpha(0.07f));
	fillHLine(g, grid, r.getY(), r.getX() + 1.f, r.getRight() - 1.f);
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

    const PixelGrid grid(g, &button);   // [Claude 2026-10-29]
    bounds = grid.snap(bounds);   // [Claude 2026-10-29] the fill ends where the outline is
    if (ticked)
        drawLitKey(g, grid, bounds, baseColor);
    else
        drawDarkKey(g, grid, bounds);

    g.setColour(ticked ? litKeyText : darkKeyText);

    // Start with a font size based on height (INV)
	// [Claude 2026-10-29] Button words as small spaced capitals, not big type (Joe: "Now the INV etc seem too large").
	if (mLabel == "INV")
	{
		float fontSize = bounds.getHeight() * 0.6f;
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
		float fontSize = bounds.getHeight() * 0.51f;
		g.setFont(buttonWordFont(fontSize).withExtraKerningFactor(0.12f));
    }

    // Draw the text centered within the bounds
    drawCrispText(g, mLabel, bounds, &button);

    // [Claude 2026-10-29] one crisp dark outline, as on the menus and sliders
    drawOutline(g, bounds);
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

    const PixelGrid grid(g, &comboBox);   // [Claude 2026-10-29]
    const auto keyArea = grid.snap(menuBounds.toFloat());   // [Claude 2026-10-29] fill and outline on one snapped edge
    drawLitKey(g, grid, keyArea, comboBox.isEnabled() ? baseColour : ColorScheme::BandColors::getBypassBaseColor());

    // [Claude 2026-10-29] one crisp dark outline, like the keys, in place of the stacked grey bevel (Joe: "Let's try it")
    drawOutline(g, keyArea);
    menuBounds.reduce(1, 1);

	g.setColour(litKeyText);

    juce::String selection = comboBox.getItemText(comboBox.getSelectedItemIndex());
    g.setFont((float) juce::roundToInt(height * 0.78f));  // the same size as before the menus got shorter, on a whole pixel
    drawCrispText(g, selection, menuBounds.toFloat(), &comboBox);
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
    
    const PixelGrid grid(g, &comboBox);   // [Claude 2026-10-29]
    const auto keyArea = grid.snap(bounds.toFloat());   // [Claude 2026-10-29] fill and outline on one snapped edge
    drawLitKey(g, grid, keyArea, comboBox.isEnabled() ? baseColour : ColorScheme::BandColors::getBypassBaseColor());

    // [Claude 2026-10-29] one crisp dark outline, like the keys, in place of the stacked grey bevel
    drawOutline(g, keyArea);
    bounds.reduce(1, 1);

	g.setColour(litKeyText);

    juce::String selection = comboBox.getItemText(comboBox.getSelectedItemIndex());
    g.setFont((float) juce::roundToInt(height * 0.78f));  // the same size as before the menus got shorter, on a whole pixel
    drawCrispText(g, selection, bounds.toFloat(), &comboBox);

}

void WaveComboLookAndFeel::setMode(int bandMode)
{
    mode = bandMode;
}


/* =========================================================================== */
// [Claude 2026-10-29] The house label look: JUCE's own drawLabel, with the text placed on whole screen pixels
void HouseLabelLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
	g.fillAll(label.findColour(juce::Label::backgroundColourId));

	if (! label.isBeingEdited())
	{
		const float alpha = label.isEnabled() ? 1.0f : 0.5f;
		const juce::Font font(getLabelFont(label));
		g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
		g.setFont(font);
		const auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());
		const int maxLines = juce::jmax(1, (int) ((float) textArea.getHeight() / font.getHeight()));
		// a label with room for more lines wraps a long text as it always did (the About window's status line);
		// otherwise the one line goes on screen pixels (Codex)
		if (maxLines > 1 && font.getStringWidthFloat(label.getText()) > (float) textArea.getWidth())
			g.drawFittedText(label.getText(), textArea, label.getJustificationType(), maxLines, label.getMinimumHorizontalScale());
		else
			drawTextOnPixels(g, PixelGrid(g, &label), label.getText(), textArea.toFloat(), label.getJustificationType());
		g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
	}
	else if (label.isEnabled())
		g.setColour(label.findColour(juce::Label::outlineColourId));

	g.drawRect(label.getLocalBounds());
}

/* =========================================================================== */
// [Claude 2026-10-29] The house menu look
void HouseMenuLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
	// [Claude 2026-10-29] the menu's window is scaled like the plugin window, so its hairlines go on screen pixels too;
	// the window's origin is a screen pixel, so no owner is needed
	const PixelGrid grid(g, nullptr);
	juce::Rectangle<float> r { 0.f, 0.f, (float) width, (float) height };
	g.setGradientFill(juce::ColourGradient(juce::Colour(0xff26282f), 0.f, 0.f, juce::Colour(0xff1d1f25), 0.f, r.getBottom(), false));
	g.fillRect(r);
	g.setColour(juce::Colours::white.withAlpha(0.08f));
	fillHLine(g, grid, r.getY(), r.getX() + 1.f, r.getRight() - 1.f);
	g.setColour(juce::Colours::black.withAlpha(0.85f));
	g.drawRect(grid.snap(r), grid.rulePx());
}

void HouseMenuLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight)
{
	// [Claude 2026-10-29] Rows 24 and separators 8 (were 22 and 9), with the 4 px border: multiples of 4, so every
	// row's own origin lands on a whole screen pixel at 125, 150 and 200 %. JUCE paints each row in the row's own
	// coordinates, with nothing to tell the painter where the row sits, so the rows have to sit well by construction
	// (Codex: "popup items use the wrong origin").
	if (isSeparator) { idealWidth = 50; idealHeight = 8; return; }
	idealHeight = 24;
	idealWidth = getPopupMenuFont().getStringWidth(text) + 44;
}

void HouseMenuLookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
	auto f = juce::Font(10.f, juce::Font::bold).withExtraKerningFactor(0.12f);
	g.setFont(f);
	g.setColour(juce::Colour(0xff6c6f78));
	drawTextOnPixels(g, PixelGrid(g, nullptr), name.toUpperCase(), area.withTrimmedLeft(10).withTrimmedTop(4).toFloat(), juce::Justification::centredLeft);   // [Claude 2026-10-29]
}

void HouseMenuLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                             bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*)
{
	auto r = area.toFloat();
	const PixelGrid grid(g, nullptr);   // [Claude 2026-10-29] each row's own origin is a screen pixel (getIdealPopupMenuItemSize)
	if (isSeparator)
	{
		const float y = std::floor(r.getCentreY());
		g.setColour(juce::Colours::black.withAlpha(0.55f));
		const auto dark = grid.hLine(y, r.getX() + 6.f, r.getRight() - 6.f);
		g.fillRect(dark);
		g.setColour(juce::Colours::white.withAlpha(0.05f));
		g.fillRect(dark.translated(0.f, dark.getHeight()));
		return;
	}
	if (isHighlighted && isActive)
	{
		g.setColour(juce::Colours::white.withAlpha(0.08f));
		g.fillRect(r.reduced(2.f, 0.f));
	}
	if (isTicked)
	{
		const float x = r.getX() + 12.f, y = r.getCentreY();
		juce::Path tick;
		tick.startNewSubPath(x - 3.f, y);
		tick.lineTo(x - 1.f, y + 2.f);
		tick.lineTo(x + 3.f, y - 2.5f);
		g.setColour(juce::Colour(0xffe3e4e8));
		g.strokePath(tick, juce::PathStrokeType(1.25f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
	}
	g.setFont(getPopupMenuFont());
	g.setColour(! isActive ? juce::Colour(0xff5d606a) : isHighlighted ? juce::Colour(0xfff0f1f4) : juce::Colour(0xffc9cbd1));
	drawTextOnPixels(g, grid, text, r.withTrimmedLeft(24.f).withTrimmedRight(12.f), juce::Justification::centredLeft);   // [Claude 2026-10-29]
	if (shortcutKeyText.isNotEmpty())
	{
		g.setColour(juce::Colour(0xff6c6f78));
		drawTextOnPixels(g, grid, shortcutKeyText, r.withTrimmedRight(10.f), juce::Justification::centredRight);   // [Claude 2026-10-29]
	}
	if (hasSubMenu)
	{
		const float cx = r.getRight() - 10.f, cy = r.getCentreY();
		juce::Path chev;
		chev.startNewSubPath(cx - 1.5f, cy - 3.f);
		chev.lineTo(cx + 1.5f, cy);
		chev.lineTo(cx - 1.5f, cy + 3.f);
		g.setColour(juce::Colour(0xff8b8e97));
		g.strokePath(chev, juce::PathStrokeType(1.2f));
	}
}
