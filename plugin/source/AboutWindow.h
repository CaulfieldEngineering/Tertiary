/*
  ==============================================================================

    ActivatorWindow.h
    Created: 17 Jan 2023 6:19:28pm
    Author:  Joe Caulfield

  ==============================================================================
*/

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include "UtilityFunctions.h"
#include <map>
#include <ctime>
#include <juce_core/juce_core.h>
#include "PluginProcessor.h"
#include "LicenseServer.h"   // [Claude 2026-10-27]
#include "LicenseCheck.h"    // [Claude 2026-10-27]

// Forward declaration goes exactly here:
namespace audio_plugin {
    class AudioPluginAudioProcessorEditor;
}

struct AboutWindow : juce::Component, juce::Button::Listener
{
public:
	AboutWindow(audio_plugin::AudioPluginAudioProcessor& p, audio_plugin::AudioPluginAudioProcessorEditor& e);
    ~AboutWindow();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void mouseExit(const juce::MouseEvent& event) override;

	/* Tells this class whether or not the app is in Demo Version */
    void setDemoState(bool demoState){mIsDemoVersion = demoState;}

	/* [Claude 2026-10-27] Shows what the weekly re-check found: a message, and a newer version if any */
	void showLicenseCheckResult(const LicenseCheck::Result& result);

	/* [Claude 2026-10-27] Re-reads the key fields and button after the license state changed elsewhere */
	void refreshActivationFields();

private:

	audio_plugin::AudioPluginAudioProcessor& audioProcessor;
	audio_plugin::AudioPluginAudioProcessorEditor& pluginEditor;
	
    juce::String formatBuildDateTime();
  
	bool mIsDemoVersion {true};

    juce::TextButton mButtonClose;

    juce::Image imagePluginTitle;
    juce::Image imageCompanyTitle;

	juce::TextEditor mTextEditorSerialKey;
    void setUpSerialKeyEditor();

	juce::TextEditor mTextEditorEmailAddress;
    void setUpEmailEditor();

	juce::TextButton mActionButton;
    void setUpActionButton();

	juce::Label mLabelStatus;
    void setUpStatusLabel();

	void attemptSerialKeyValidation();
	void attemptSerialKeyActivation();

	void attemptSerialKeyDeactivation();


	void updateAppOnActivationChange(bool isActivating);
	void updateLocalKeyData(bool isActivating);

    float targetTitleWidth{ 350.f };
    float targetTitleHeight{ 60.f };

    float targetCompanyWidth{ 400.f };
    float targetCompanyHeight{ 50.f };

	// Cached scaled images
    juce::Image scaledCompanyTitle;
    juce::Image scaledPluginTitle;
    
    // Scale tracking
    float lastPluginTitleScale = 0.0f;
    float lastCompanyTitleScale = 0.0f;
    
    // Cached aspect ratios
    float pluginTitleOriginalAspect = 1.0f;
    float companyTitleOriginalAspect = 1.0f;

    juce::HyperlinkButton mButtonWebsiteLink;

	// [Claude 2026-10-27] The server calls, behind an interface (LicenseServer.h). Shared with the request threads.
	std::shared_ptr<LicenseServer> licenseServer { std::make_shared<WonderlandLicenseServer>() };

};
