/*
  ==============================================================================

	ScrollPad.h
	Created: 5 Oct 2022 7:30:04pm
	Author:  Joe

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct Cursor : juce::Component,
				juce::ActionBroadcaster,
				juce::ActionListener,
				juce::Timer
{
public:

	Cursor();
	~Cursor();

	void mouseEnter(const juce::MouseEvent& event) override {};
	void mouseExit(const juce::MouseEvent& event) override {};

	void paint(juce::Graphics& g) override;
	void resized() override;

	void setHorizontalOrientation();

	// [Claude 2026-10-29] Where on a crossover cursor the two bands meet (local y), for its handle
	void setMarkerY(float y) { mMarkerY = y; repaint(); }

	void setFocus(bool hasFocus);

	void timerCallback();

	float getFadeValue() { return fadeValue; };
	float getFadeValueMin() { return fadeValueMin; };
	float getFadeValueMax() { return fadeValueMax; };

	void actionListenerCallback(const juce::String& message);
	void sendBroadcast(juce::String parameterName, juce::String parameterValue);

private:

	juce::Line<float> mCursor;

	juce::String mNameSpace{ "Cursor" };
	bool setDebug{ true };

	bool mIsVertical{ true };
	float mMarkerY{ -1.f };

	bool mHasFocus{ false };
	bool mForceFocus{ false };

	int timerCounter{ 0 };
	int timerCounterMin{ 0 };
	int timerCounterMax{ 14 };

	float fadeValue{ 1.f };
	float fadeValueMin{ 0.25f };
	float fadeValueMax{ 1.f };

};
