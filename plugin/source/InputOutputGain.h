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

	void buildGrill(juce::Rectangle<float> bounds);
    
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
	float leftLevelPixel;
	float rightLevelPixel;

	// Grill Drawing Parameters
	int margin = 0;				// Amount of Pixels removed from Top/Bottom before grill
	float spacing = 3.5f;			// Vertical Distance between LEDs
	float ledWidth = 10.f;
	float ledHeight = 1.5f;
	juce::Array<float> ledThresholds;

	juce::Rectangle<float> leftBounds, rightBounds;

	juce::String pickOffPoint = "NA"; // 0 = INPUT, 1 = OUTPUT

	juce::ColourGradient ledOffGradient, ledOnGradient{};

	juce::Image grill;
};
