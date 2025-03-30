/*
  ==============================================================================

    ActivatorWindow.cpp
    Created: 17 Jan 2023 6:19:28pm
    Author:  Joe Caulfield

  ==============================================================================
*/


#include "AboutWindow.h"
#include "PluginEditor.h"




AboutWindow::AboutWindow(audio_plugin::AudioPluginAudioProcessor& p, audio_plugin::AudioPluginAudioProcessorEditor& e)
    : audioProcessor(p), pluginEditor(e)
{
    setAlpha(0.95f);


    mButtonClose.addListener(this);
    addAndMakeVisible(mButtonClose);

    addMouseListener(this, true);

    mButtonClose.setColour(juce::TextButton::ColourIds::buttonColourId, juce::Colours::black.withAlpha(0.f));
    mButtonClose.setButtonText("X");

    imagePluginTitle = juce::ImageCache::getFromMemory(BinaryData::PluginTitle_png, BinaryData::PluginTitle_pngSize);
    imageCompanyTitle = juce::ImageCache::getFromMemory(BinaryData::CompanyTitle_png, BinaryData::CompanyTitle_pngSize);

    mButtonWebsiteLink.setButtonText("WonderlandAudio.com");
    mButtonWebsiteLink.setURL(juce::URL("http://www.wonderlandaudio.com"));
    mButtonWebsiteLink.setColour(juce::HyperlinkButton::textColourId, juce::Colours::oldlace);
    addAndMakeVisible(mButtonWebsiteLink);

	mIsDemoVersion = audioProcessor.getDemoState();

	setUpSerialKeyEditor();
    setUpEmailEditor();
    setUpActionButton();
    setUpStatusLabel();

}

AboutWindow::~AboutWindow()
{

}

void AboutWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    auto bounds = getLocalBounds();
    float scale = getWindowsDPIScale(this);
    
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // Draw Company Title (Wonderland Audio logo)
	// ==========================================================
    float companyTitleWidth = bounds.getWidth() * 0.75f;
    float aspectRatio = static_cast<float>(targetCompanyHeight) / static_cast<float>(targetCompanyWidth);
    float companyTitleHeight = companyTitleWidth * aspectRatio;
    
    if (std::abs(lastCompanyTitleScale - scale) > 0.01f || scaledCompanyTitle.isNull())
    {
        scaledCompanyTitle = applyResize(imageCompanyTitle, 
                                       juce::roundToInt(companyTitleWidth * scale), 
                                       juce::roundToInt(companyTitleHeight * scale));
        lastCompanyTitleScale = scale;
    }

    juce::Rectangle<float> companyBounds{
        bounds.getCentreX() - companyTitleWidth / 2,
        25,
        companyTitleWidth,
        companyTitleHeight
    };
    
    if (!scaledCompanyTitle.isNull())
    {
        g.drawImage(scaledCompanyTitle, companyBounds);
    }

    // Draw Plugin Title (TERTIARY)
	// ==========================================================
    juce::Rectangle<float> titleBounds{
        bounds.getCentreX() - targetTitleWidth / 2,
        companyBounds.getBottom() + 5,
        static_cast<float>(targetTitleWidth),
        static_cast<float>(targetTitleHeight)
    };

    if (std::abs(lastPluginTitleScale - scale) > 0.01f || scaledPluginTitle.isNull())
    {
        scaledPluginTitle = applyResize(imagePluginTitle, 
                                      juce::roundToInt(targetTitleWidth * scale), 
                                      juce::roundToInt(targetTitleHeight * scale));
        lastPluginTitleScale = scale;
    }

    if (!scaledPluginTitle.isNull())
    {
        g.drawImage(scaledPluginTitle, titleBounds);
    }

    // Set up text drawing with DPI scaling
    float fontSize = 20.0f;
    g.setFont(fontSize);

    // Draw Version and Release info
	// ==========================================================
    g.setColour(juce::Colours::white);
    #ifdef DEBUG
        g.setColour(juce::Colours::red);
    #endif

    juce::Rectangle<int> versionBounds = bounds.removeFromTop(static_cast<int>(titleBounds.getBottom()) + 60)
                                               .removeFromBottom(60);
    juce::String versionNumber = JucePlugin_VersionString;
    
    juce::String releaseDate = formatBuildDateTime();
    g.drawFittedText("Version: " + versionNumber, versionBounds.removeFromTop(30), juce::Justification::centred, 1);
    g.drawFittedText("Release: " + releaseDate, versionBounds, juce::Justification::centred, 1);

    // Draw Demo Version Disclaimer
	// ==========================================================
	if (mIsDemoVersion)
	{
		g.setFont(fontSize * 0.8f);
		g.setColour(juce::Colours::lightgrey);
		juce::Rectangle<int> disclaimerBounds = bounds.removeFromTop(60).reduced(5, 0);
		g.drawFittedText("Demo Version: Effect is disabled once every\n50 seconds, for 10 seconds.",
						 disclaimerBounds, juce::Justification::centred, 2);
	}
	else
	{
		g.setFont(fontSize * 0.8f);
		g.setColour(juce::Colour(0xff409963)); // 0xff indicates full opacity
		juce::Rectangle<int> thanksBounds = bounds.removeFromTop(60).reduced(5, 0);
		g.drawFittedText("Full Version Activated\nThank you for your purchase!",
						 thanksBounds, juce::Justification::centred, 1);


	}

    // Draw Window Border
	// ==========================================================
    paintBorder(g, juce::Colours::grey, getLocalBounds().toFloat());
}

void AboutWindow::resized()
{
    auto bounds = getLocalBounds();

    // Position the Close ("X") button in the top-right corner
    mButtonClose.setBounds(bounds.getRight() - 30, 5, 25, 25);

    // Position the website link button at the bottom center
    mButtonWebsiteLink.setBounds(bounds.getX(), bounds.getBottom() - 50, bounds.getWidth(), 30);

	// ===============================
	// Position Activation UI Elements
	// ===============================

	auto editorArea = bounds.withTrimmedTop(bounds.getHeight() * 0.575f).reduced(40, 0);

	// Email Address Field
	mTextEditorEmailAddress.setBounds(editorArea.removeFromTop(30));

	// Serial Key Field
	editorArea.removeFromTop(10); // spacing
	mTextEditorSerialKey.setBounds(editorArea.removeFromTop(30));

	// Action Button
	auto buttonArea = editorArea.removeFromTop(30).translated(0, 10);
	mActionButton.setBounds(buttonArea);

	// Status Label
	auto statusArea = buttonArea.translated(0, 35);
	statusArea.setHeight(20);
	mLabelStatus.setBounds(statusArea);
}


void AboutWindow::setUpSerialKeyEditor()
{
    addAndMakeVisible(mTextEditorSerialKey);

    mTextEditorSerialKey.setMultiLine(false);
    mTextEditorSerialKey.setTextToShowWhenEmpty("Enter Serial Key...", juce::Colours::darkgrey);
    mTextEditorSerialKey.setFont(juce::Font(16.0f));
    mTextEditorSerialKey.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.3f));
    mTextEditorSerialKey.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    mTextEditorSerialKey.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey.withAlpha(0.6f));
    mTextEditorSerialKey.setJustification(juce::Justification::centred);

    // Load key from disk
    juce::PropertiesFile::Options options;
    options.applicationName = "Tertiary";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile props(options);



    if (mIsDemoVersion)
    {
        mTextEditorSerialKey.setReadOnly(false);
        mTextEditorSerialKey.setText("");
    }
    else
    {
        mTextEditorSerialKey.setReadOnly(true);

		juce::String savedKey = props.getValue("licenseKey", "");
		if (savedKey.isNotEmpty())
			savedKey = "Key: " + savedKey;

        mTextEditorSerialKey.setText(savedKey);
    }
}

void AboutWindow::setUpEmailEditor()
{
    addAndMakeVisible(mTextEditorEmailAddress);

    mTextEditorEmailAddress.setMultiLine(false);
    mTextEditorEmailAddress.setTextToShowWhenEmpty("Enter Email Address...", juce::Colours::darkgrey);
    mTextEditorEmailAddress.setFont(juce::Font(16.0f));
    mTextEditorEmailAddress.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.3f));
    mTextEditorEmailAddress.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    mTextEditorEmailAddress.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey.withAlpha(0.6f));
    mTextEditorEmailAddress.setJustification(juce::Justification::centred);

    // Load email from disk
    juce::PropertiesFile::Options options;
    options.applicationName = "Tertiary";
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support";
    juce::PropertiesFile props(options);



    if (mIsDemoVersion)
    {
        mTextEditorEmailAddress.setReadOnly(false);
        mTextEditorEmailAddress.setText("");
    }
    else
    {

		mTextEditorEmailAddress.setReadOnly(true);

		juce::String savedEmail = props.getValue("licenseEmail", "");
		if (savedEmail.isNotEmpty())
			savedEmail = "Email: " + savedEmail;

        mTextEditorEmailAddress.setText(savedEmail);
    }
}



void AboutWindow::setUpActionButton()
{
    addAndMakeVisible(mActionButton);

    // Custom green: #409963 = RGB(64, 153, 99)
    juce::Colour customGreen(0xff409963);
    juce::Colour grey = juce::Colours::darkgrey;

    if (mIsDemoVersion)
    {
        mActionButton.setEnabled(true);
        mActionButton.setButtonText("Activate Serial Key");

        mActionButton.setColour(juce::TextButton::buttonColourId, customGreen);
        mActionButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        mActionButton.setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);

        mActionButton.onClick = [this]()
        {
            juce::String enteredKey = mTextEditorSerialKey.getText().trim();

            if (enteredKey.isNotEmpty())
            {
                DBG("Attempting to activate serial key: " + enteredKey);
                attemptSerialKeyValidation();
            }
            else
            {
                DBG("No serial key entered.");
                mLabelStatus.setText("Enter a serial key.", juce::dontSendNotification);
            }
        };
    }
    else
    {
        mActionButton.setEnabled(true);
        mActionButton.setButtonText("Deactivate Key");

        mActionButton.setColour(juce::TextButton::buttonColourId, grey);
        mActionButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        mActionButton.setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey);

        mActionButton.onClick = [this]()
        {
            DBG("Attempting to deactivate serial key");
            attemptSerialKeyDeactivation();
        };
    }
}






void AboutWindow::setUpStatusLabel()
{
    addAndMakeVisible(mLabelStatus);

    mLabelStatus.setText("", juce::dontSendNotification);
    mLabelStatus.setJustificationType(juce::Justification::centred);
    mLabelStatus.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    mLabelStatus.setFont(juce::Font(14.0f, juce::Font::italic));
}


juce::String AboutWindow::formatBuildDateTime()
{
    // Map month abbreviations to numbers
    std::map<juce::String, int> months = {
        {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
        {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
        {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
    };

    // Extract components from __DATE__ and __TIME__
    juce::String date(__DATE__);  // "MMM DD YYYY"
    juce::String time(__TIME__);  // "HH:MM:SS"

    // Parse the date string
    juce::String monthAbbr = date.substring(0, 3);
    int day = date.substring(4, 6).getIntValue();
    int year = date.substring(7, 11).getIntValue();
    int month = months[monthAbbr];

    // Parse the time string
    int hour = time.substring(0, 2).getIntValue();
    int minute = time.substring(3, 5).getIntValue();
    int second = time.substring(6, 8).getIntValue();

    // Convert to UTC (Assumes build system is in local time)
    struct tm buildTime = {};
    buildTime.tm_year = year - 1900;
    buildTime.tm_mon = month - 1;
    buildTime.tm_mday = day;
    buildTime.tm_hour = hour;
    buildTime.tm_min = minute;
    buildTime.tm_sec = second;

    time_t buildTimestamp = mktime(&buildTime); // Converts to system's local time
    if (buildTimestamp == -1)
        return "Invalid time";

    // Convert to UTC
    struct tm *utcTime = gmtime(&buildTimestamp);

    // Convert UTC to EST (UTC-5) or EDT (UTC-4)
    int estOffset = -5; // EST is UTC-5, EDT is UTC-4
    struct tm estTime = *utcTime;
    estTime.tm_hour += estOffset;

    // Normalize time (handles underflows or overflows in hour field)
    mktime(&estTime);

    // Format the date and time as "YYYY.MM.DD HH:MM EST"
    return juce::String::formatted("%04d.%02d.%02d %02d:%02d EST",
                                   estTime.tm_year + 1900, estTime.tm_mon + 1, estTime.tm_mday,
                                   estTime.tm_hour, estTime.tm_min);
}


void AboutWindow::buttonClicked(juce::Button* button)
{
    if (button == &mButtonClose)
    {
        setVisible(false);
    }

}

void AboutWindow::mouseExit(const juce::MouseEvent& event)
{
    //if (!isMouseOver())
    //{
    //    if (!mButtonClose.isMouseOver() && !mButtonWebsiteLink.isMouseOver())
    //        setVisible(false);
    //}

}





void AboutWindow::attemptSerialKeyValidation()
{
    auto serialKey = mTextEditorSerialKey.getText().trim();
    auto email = mTextEditorEmailAddress.getText().trim();

    if (serialKey.isEmpty())
    {
        mLabelStatus.setText("Enter a serial key.", juce::dontSendNotification);
        return;
    }

    mLabelStatus.setText("Validating...", juce::dontSendNotification);

    juce::Thread::launch([this, serialKey, email]()
    {
        juce::URL apiURL("https://wonderlandaudio.com/wp-json/wcsn/validate");
        apiURL = apiURL.withParameter("product_id", "45")
                       .withParameter("serial_key", serialKey)
                       .withParameter("request", "validate");

        if (email.isNotEmpty())
            apiURL = apiURL.withParameter("email", email);

        juce::URL::InputStreamOptions options(juce::URL::ParameterHandling::inAddress);
        options.withConnectionTimeoutMs(5000);

        std::unique_ptr<juce::InputStream> stream(apiURL.createInputStream(options));

        if (stream != nullptr)
        {
            auto responseStr = stream->readEntireStreamAsString();

            auto json = juce::JSON::parse(responseStr);
            if (auto* obj = json.getDynamicObject())
            {
                auto code = obj->getProperty("code").toString();
                auto message = obj->getProperty("message").toString();

                juce::MessageManager::callAsync([this, code, message, obj]()
                {
					// Key is confirmed to be valid
                    if (code == "key_valid")
                    {
                        mLabelStatus.setText("Serial key valid!", juce::dontSendNotification);

                        attemptSerialKeyActivation();
                    }
                    else
                    {
                        mLabelStatus.setText("Error: " + message, juce::dontSendNotification);
                        DBG("API Error: " + code + " - " + message);
                    }
                });
            }
            else
            {
                juce::MessageManager::callAsync([this]()
                {
                    mLabelStatus.setText("Invalid JSON from server.", juce::dontSendNotification);
                });
            }
        }
        else
        {
            juce::MessageManager::callAsync([this]()
            {
                mLabelStatus.setText("Connection to server failed.", juce::dontSendNotification);
            });
        }
    });
}



void AboutWindow::attemptSerialKeyActivation()
{
    auto serialKey = mTextEditorSerialKey.getText().trim();
    auto email = mTextEditorEmailAddress.getText().trim();
    juce::String instance = juce::SystemStats::getDeviceDescription();
    juce::String platform = juce::SystemStats::getOperatingSystemName();

    mLabelStatus.setText("Activating...", juce::dontSendNotification);

    juce::Thread::launch([this, serialKey, email, instance, platform]()
    {
        juce::URL activateURL("https://wonderlandaudio.com/wp-json/wcsn/activate");
        activateURL = activateURL.withParameter("product_id", "45")
                                 .withParameter("serial_key", serialKey)
                                 .withParameter("instance", instance)
                                 .withParameter("platform", platform);

        if (email.isNotEmpty())
            activateURL = activateURL.withParameter("email", email);

        auto stream = activateURL.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(5000));

        if (stream)
        {
            auto responseStr = stream->readEntireStreamAsString();
            auto json = juce::JSON::parse(responseStr);

            if (auto* obj = json.getDynamicObject())
            {
                auto code = obj->getProperty("code").toString();
                auto message = obj->getProperty("message").toString();

				juce::MessageManager::callAsync([this, code, message]()
				{
					if (code == "key_activated" || code == "instance_already_activated")
					{
						if (code == "instance_already_activated")
							mLabelStatus.setText("Instance was already activated.  Reactivation successful!", juce::dontSendNotification);
						else
							mLabelStatus.setText("Activation successful!", juce::dontSendNotification);

						updateAppOnActivationChange(true);
					}
					else
					{
						mLabelStatus.setText("Activation error: " + message, juce::dontSendNotification);
						DBG("API Error: " + code + " - " + message);
					}
				});
            }
            else
            {
                juce::MessageManager::callAsync([this]()
                {
                    mLabelStatus.setText("Invalid activation response.", juce::dontSendNotification);
                });
            }
        }
        else
        {
            juce::MessageManager::callAsync([this]()
            {
                mLabelStatus.setText("Activation connection failed.", juce::dontSendNotification);
            });
        }
    });
}

void AboutWindow::attemptSerialKeyDeactivation()
{
	auto serialKey = mTextEditorSerialKey.getText().fromFirstOccurrenceOf("Key: ", false, true).trim();
	auto email = mTextEditorEmailAddress.getText().fromFirstOccurrenceOf("Email: ", false, true).trim();

    juce::String instance = juce::SystemStats::getDeviceDescription();
    juce::String platform = juce::SystemStats::getOperatingSystemName();

    mLabelStatus.setText("Deactivating...", juce::dontSendNotification);

    juce::Thread::launch([this, serialKey, email, instance, platform]()
    {
        juce::URL deactivateURL("https://wonderlandaudio.com/wp-json/wcsn/deactivate");
        deactivateURL = deactivateURL.withParameter("product_id", "45")
                                     .withParameter("serial_key", serialKey)
                                     .withParameter("instance", instance)
                                     .withParameter("platform", platform);

        if (email.isNotEmpty())
            deactivateURL = deactivateURL.withParameter("email", email);

        auto stream = deactivateURL.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(5000));

        if (stream)
        {
            auto responseStr = stream->readEntireStreamAsString();
            auto json = juce::JSON::parse(responseStr);

            if (auto* obj = json.getDynamicObject())
            {
                auto code = obj->getProperty("code").toString();
                auto message = obj->getProperty("message").toString();

                juce::MessageManager::callAsync([this, code, message]()
                {
                    if (code == "key_deactivated" || code == "instance_deactivated")
                    {
                        mLabelStatus.setText("Deactivation successful.", juce::dontSendNotification);

                        // Optionally clear local props or revert UI to demo mode
                        updateAppOnActivationChange(false);
                    }
                    else
                    {
                        mLabelStatus.setText("Deactivation error: " + message, juce::dontSendNotification);
                        DBG("API Error: " + code + " - " + message);
                    }
                });
            }
            else
            {
                juce::MessageManager::callAsync([this]()
                {
                    mLabelStatus.setText("Invalid deactivation response.", juce::dontSendNotification);
                });
            }
        }
        else
        {
            juce::MessageManager::callAsync([this]()
            {
                mLabelStatus.setText("Deactivation connection failed.", juce::dontSendNotification);
            });
        }
    });
}



void AboutWindow::updateAppOnActivationChange(bool isActivating)
{

// Store Local Key
	if (isActivating)
         updateLocalKeyData(true);
	else
	     updateLocalKeyData(false);


	// Notify Processor of Change
	audioProcessor.setDemoState(!isActivating);
  
	// Notify GUI of Change, which will propagate changes downward
	pluginEditor.updateDemoState(!isActivating);


	// Reload local changes
	setUpSerialKeyEditor();
	setUpEmailEditor();
	setUpActionButton();

	repaint();

}


void AboutWindow::updateLocalKeyData(bool isActivating)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Tertiary"; // or whatever your plugin name is
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support"; // macOS-specific
    options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML; // Easier to read

    juce::PropertiesFile props(options);

	juce::String email = mTextEditorEmailAddress.getText().trim();

	juce::String serialKey = "";

	if (isActivating)
      serialKey = mTextEditorSerialKey.getText().trim();

    props.setValue("licenseKey", serialKey);
    props.setValue("licenseEmail", email);
    props.saveIfNeeded();

    juce::File file = props.getFile();
    DBG("Stored license data at: " + file.getFullPathName());
}


