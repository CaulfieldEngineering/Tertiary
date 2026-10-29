/*
  ==============================================================================

    InputOutputGain.h
    Created: 10 Jan 2022 8:19:16pm
    Author:  Joe

  ==============================================================================
*/

#pragma once
#include "PluginProcessor.h"
#include "MyLookAndFeel.h"

struct PixelGrid;   // [Claude 2026-10-29] UtilityFunctions.h

struct InputOutputGain : juce::Component, juce::Timer, juce::Slider::Listener
{
	InputOutputGain(audio_plugin::AudioPluginAudioProcessor& p);
	~InputOutputGain() override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	void sliderValueChanged(juce::Slider* slider) override;

	void timerCallback() override;

	void setPickOffPoint(juce::String pickoff);

	audio_plugin::AudioPluginAudioProcessor& audioProcessor;
	juce::Slider sliderGain;

	InOutLookAndFeel inOutLookAndFeel;

	// [Claude 2026-10-29] The LED grid is laid out in screen pixels when painted (see buildLeds); it was a picture with
	// holes cut on layout pixels, which came out with uneven LEDs and gaps at 125 and 150 %
	void buildLeds(const PixelGrid& grid);

	void getLevels();

	float sliderValue;

	juce::Rectangle<float> grillBounds;
	juce::Rectangle<float> labelBounds;

	juce::ColourGradient makeMeterGradient(juce::Rectangle<float> bounds, float brightness);



private:

	juce::String mNameSpace{ "InputOutputGain" };
	bool setDebug{ false };

	float leftLevel = -60.f;
	float rightLevel = -60.f;
	float leftLevelPixel { 0.f };
	float rightLevelPixel { 0.f };

	// Grill Drawing Parameters
	float ledWidth = 10.f;
	// [Claude 2026-10-29] the 100 % look, which the other sizes reproduce in whole screen pixels: LEDs 2 px tall and
	// 1 px apart, the two columns 1 px apart
	static constexpr float ledHeightAt1x = 2.f, ledGapAt1x = 1.f, columnGapAt1x = 1.f;
	juce::Array<float> ledThresholds;                       // each LED's top, measured up from the grill's bottom (layout px)
	juce::Array<juce::Rectangle<float>> leftLeds, rightLeds; // in this component's coordinates, on whole screen pixels
	juce::Rectangle<float> ledGrill;                        // the black behind them, on whole screen pixels
	float builtScale { 0.f };                               // the screen scale the LEDs were laid out for

	juce::String pickOffPoint = "NA"; // 0 = INPUT, 1 = OUTPUT

	juce::ColourGradient ledOffGradient, ledOnGradient{};
};
