/*
  ==============================================================================

	FrequencyResponse.cpp
	Created: 30 Dec 2021 11:38:20am
	Author:  Joe Caulfield

	Class to display and control the frequency response of the crossover

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "UtilityFunctions.h"
#include "AllColors.h"
#include "Cursor.h"
#include "FreqLabel.h"


/* ============================================================== */
struct FrequencyResponse :	juce::Component,
							juce::Slider::Listener,
							juce::ActionListener
{

	FrequencyResponse(audio_plugin::AudioPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& apv);
	~FrequencyResponse();

	void paint(juce::Graphics& g) override;

	void drawAxis(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::Colour color) {};

	void sliderValueChanged(juce::Slider* slider) override;

    void paintResponseRegions(juce::Graphics& g);

	void paintLowRegion(	juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            float gainPixel,
                            float freq1Pixel,
                            float alpha);

	void paintMidRegion(	juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            float gainPixel,
                            float freq1Pixel,
                            float freq2Pixel,
                            float alpha);

	void paintHighRegion(	juce::Graphics& g,
							juce::Rectangle<float> bounds,
							float gainPixel,
							float freq2Pixel,
							float alpha);

	void paintGridGain(juce::Graphics& g);

	void paintGridFrequency(juce::Graphics& g);
    
	void resized() override;

	void makeAttachments();

	void mouseEnter(const juce::MouseEvent& event) override;
	void mouseExit(const juce::MouseEvent& event) override;
	void mouseMove(const juce::MouseEvent& event) override;
    
	void updateResponse();



	void checkCursorFocus(const juce::MouseEvent& event);

	float mapLog2(float freq);

	void checkSolos();

	audio_plugin::AudioPluginAudioProcessor& audioProcessor;
	//GlobalControls& globalControls;

	// Frequency Response Parameters
	juce::AudioParameterFloat* lowMidCutoff{ nullptr };     // Pointer to the APVTS
	juce::AudioParameterFloat* midHighCutoff{ nullptr };    // Pointer to the APVTS
	juce::AudioParameterFloat* lowGain{ nullptr };			// Pointer to the APVTS
	juce::AudioParameterFloat* midGain{ nullptr };			// Pointer to the APVTS
	juce::AudioParameterFloat* highGain{ nullptr };         // Pointer to the APVTS

	// Solo Params
	bool lowBandSolo{ false },		midBandSolo{ false },	highBandSolo{ false };
	bool lowBandMute{ false },		midBandMute{ false },	highBandMute{ false };
	bool lowBandSoloMute{ false },	midBandSoloMute{ false }, highBandSoloMute{ false };

	bool lowBandBypass{ false },	midBandBypass{ false }, highBandBypass{ false };
	//bool mLowBypass{ false },		mMidBypass{ false },	mHighBypass{ false };

	void actionListenerCallback(const juce::String& message);
    
private:
    
	juce::AudioProcessorValueTreeState& apvts;

	juce::String mNameSpace{ "FrequencyResponse" };
	bool setDebug{ false };

	void buildLowMidFreqSlider();
	void buildMidHighFreqSlider();

	void buildLowGainSlider();
	void buildMidGainSlider();
	void buildHighGainSlider();

	Cursor newCursorLM, newCursorMH, newCursorLowGain, newCursorMidGain, newCursorHighGain;
	float focusAlpha = 0.85f;
	float noFocusAlpha = 0.65f;
	float lowCursorFadeValue{ 1.f };
	float highCursorFadeValue{ 1.f };


	//void setLabelOpacity();
	float labelFadeMin{ 0.f };
	float labelFadeMax{ 1.f };
	void drawLabels();
	void buildLabels();
	float lowFreqLabelFadeValue{ 1.f };
	float highFreqLabelFadeValue{ 1.f };

	FreqLabel newFreqLabelLow, newFreqLabelHigh;

	float mLowMidCutoff, mMidHighCutoff, mLowGain, mMidGain, mHighGain;
	bool mLowFocus{ false }, mMidFocus{ false }, mHighFocus{ false }, mLowMidFocus, mMidHighFocus;



	float freq1Pixel, freq2Pixel, gainLowPixel, gainMidPixel, gainHighPixel;

	juce::Rectangle<float> responseArea;

    juce::Array<float> freqs { 20, 40, 80, 160, 320, 640, 1300, 2500, 5100, 10000, 20000 };
	juce::Array<float> gain { -30, -24, -18, -12, -6, 0, 6, 12, 18, 24, 30 };

	juce::Slider	sliderLowMidCutoff,
					sliderMidHighCutoff,
					sliderLowMidInterface,
					sliderMidHighInterface,
					sliderLowGain,
					sliderMidGain,
					sliderHighGain;

	using sliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

	std::unique_ptr<sliderAttachment>	lowMidAttachment,
										midHighAttachment,
										lowGainAttachment,
										midGainAttachment,
										highGainAttachment;

	using buttonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

};
