/*
  ==============================================================================

    ScrollPad.h
    Created: 5 Oct 2022 7:30:04pm
    Author:  Joe

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "AllColors.h"
#include "Params.h"

struct ScrollPad :	juce::Component,
					juce::Slider::Listener,
					juce::ActionBroadcaster
{
public:

	ScrollPad(audio_plugin::AudioPluginAudioProcessor& p);
	~ScrollPad();

	void mouseDown(const juce::MouseEvent& event) override;
	void mouseDrag(const juce::MouseEvent& event) override;
	void mouseDoubleClick(const juce::MouseEvent& event) override;

	void paint(juce::Graphics& g) override;
	void resized() override;

	void sliderValueChanged(juce::Slider* slider) override;

	void calculateZoomFactor();

	void forceNewPointValues(float newP1 = 0.f, float newP2 = 0.f);
	void updatePoints(int x1, int y1);
	void makeDefaultPoints();

	//void setCenter(float panPercentage);
	float getScrollCenter();
	float getScrollZoom();

	int getMaxWidth() { return maxWidth; }
	int getMinWidth() { return minWidth; }
	float getMinZoomFactor() { return minZoomFactor; }
	float getMaxZoomFactor() { return maxZoomFactor; }
	float getPoint1() { return point1; }
	float getPoint2() { return point2; }

	//void setZoom(float zoom);

	audio_plugin::AudioPluginAudioProcessor& audioProcessor;

	void sendBroadcast(juce::String parameterName, juce::String parameterValue);



private:
	float point1;	// Value & Position of Left Thumb
	float point2;	// Value & Position of Right Thumb

	float centerScaled; // Value returned to ScopeChannel

	float p01, p02;

	bool leftHit{ false }, rightHit{ false }, midHit{ false };

	float xDown, x0, x1;		// Determines Mouse Movement, X-Direction
	float yDown, y0, y1;		// Determines Mouse Movement, Y-Direction

	bool shouldUpdatePoint1Only, shouldUpdatePoint2Only, shouldCheckPanOrZoom;

	int currentCenter{ 0 };
	int minWidth{ 80 };
	int defaultWidth{ 160 };
	float currentWidth{ 100 };
	int maxWidth{ 360 };

	float minZoomFactor{ 0.5f };
	float currentZoomFactor{ 1.f };
	float maxZoomFactor{ 10.f };


	int thumbTolerance = 75;
	int zoomScale{ 1 };
	float zoomIncrement = 10;

	float convertSliderToPixelValue(float sliderValue);
	float convertPixelToSliderValue(float pixelValue);

	juce::Slider point1Slider, point2Slider;
	using sliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
	std::unique_ptr<sliderAttachment> point1Attachment, point2Attachment;
};
