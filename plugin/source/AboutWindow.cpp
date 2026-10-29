/*
  ==============================================================================

    ActivatorWindow.cpp
    Created: 17 Jan 2023 6:19:28pm
    Author:  Joe Caulfield

  ==============================================================================
*/


#include "AboutWindow.h"
#include "PluginEditor.h"




// [Claude 2026-10-29] The house style: keys like the header's (green when they activate), fields as recessed wells
//==============================================================================
void AboutWindow::Look::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
	auto r = b.getLocalBounds().toFloat().reduced(0.5f);
	const bool primary = b.getProperties()["primary"];
	juce::Colour top = primary ? juce::Colour(0xff5bbd7d) : juce::Colour(0xff34363e);
	juce::Colour bot = primary ? juce::Colour(0xff3f9a5f) : juce::Colour(0xff25272d);
	if (down) { top = top.darker(0.25f); bot = bot.darker(0.25f); }
	else if (highlighted) { top = top.brighter(0.08f); bot = bot.brighter(0.08f); }
	g.setGradientFill(juce::ColourGradient(top, 0.f, r.getY(), bot, 0.f, r.getBottom(), false));
	g.fillRoundedRectangle(r, 1.5f);
	g.setColour(juce::Colours::white.withAlpha(primary ? 0.25f : 0.1f));
	g.fillRect(r.withHeight(1.f).reduced(1.5f, 0.f));
	g.setColour(juce::Colours::black.withAlpha(0.7f));
	g.drawRoundedRectangle(r, 1.5f, 1.f);
}

void AboutWindow::Look::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool highlighted, bool)
{
	const bool primary = b.getProperties()["primary"];
	if (b.getButtonText() == "X")
	{
		// the close key: a fine cross
		auto c = b.getLocalBounds().toFloat().getCentre();
		g.setColour(juce::Colour(highlighted ? 0xffd9dade : 0xffa9abb3));
		g.drawLine(c.x - 3.5f, c.y - 3.5f, c.x + 3.5f, c.y + 3.5f, 1.3f);
		g.drawLine(c.x - 3.5f, c.y + 3.5f, c.x + 3.5f, c.y - 3.5f, 1.3f);
		return;
	}
	g.setColour(primary ? juce::Colour(0xff0f1a12) : juce::Colour(highlighted ? 0xffd9dade : 0xffb4b6bc));
	g.setFont(juce::Font(13.f));
	g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

void AboutWindow::Look::fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor&)
{
	juce::Rectangle<float> r { 0.5f, 0.5f, (float) w - 1.f, (float) h - 1.f };
	g.setGradientFill(juce::ColourGradient(juce::Colour(0xff0b0c0f), 0.f, r.getY(), juce::Colour(0xff15161a), 0.f, r.getBottom(), false));
	g.fillRoundedRectangle(r, 1.5f);
	g.setColour(juce::Colours::black.withAlpha(0.6f));
	g.fillRect(r.withHeight(1.5f).reduced(1.5f, 0.f));
}

void AboutWindow::Look::drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
	g.setColour(e.hasKeyboardFocus(true) ? juce::Colour(0xff5bbd7d).withAlpha(0.7f) : juce::Colours::black.withAlpha(0.7f));
	g.drawRoundedRectangle(juce::Rectangle<float>(0.5f, 0.5f, (float) w - 1.f, (float) h - 1.f), 1.5f, 1.f);
	g.setColour(juce::Colours::white.withAlpha(0.06f));
	g.fillRect(juce::Rectangle<float>(2.f, (float) h - 1.f, (float) w - 4.f, 1.f));
}

void AboutWindow::applyLook()
{
	for (juce::Component* c : { (juce::Component*) &mButtonClose, (juce::Component*) &mActionButton,
	                            (juce::Component*) &mTextEditorEmailAddress, (juce::Component*) &mTextEditorSerialKey })
		c->setLookAndFeel(&look);

	for (auto* e : { &mTextEditorEmailAddress, &mTextEditorSerialKey })
	{
		e->setFont(juce::Font(13.f));
		e->setJustification(juce::Justification::centredLeft);
		e->setIndents(10, 0);
		e->setColour(juce::TextEditor::textColourId, juce::Colour(0xffd6d7dc));
		e->setColour(juce::TextEditor::highlightColourId, juce::Colour(0xff3f9a5f).withAlpha(0.5f));
		e->setColour(juce::CaretComponent::caretColourId, juce::Colour(0xffd6d7dc));
		e->setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
		e->setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
		e->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
	}
	mTextEditorEmailAddress.setTextToShowWhenEmpty("Email address", juce::Colour(0xff5d606a));
	mTextEditorSerialKey.setTextToShowWhenEmpty("Serial key", juce::Colour(0xff5d606a));

	// the demo shows the fields to activate; the full version shows who it is activated to instead
	mTextEditorEmailAddress.setVisible(mIsDemoVersion);
	mTextEditorSerialKey.setVisible(mIsDemoVersion);
	mActionButton.getProperties().set("primary", mIsDemoVersion);
	mActionButton.setButtonText(mIsDemoVersion ? "Activate" : "Deactivate this computer");

	mButtonWebsiteLink.setFont(juce::Font(12.f), false, juce::Justification::centred);
	mButtonWebsiteLink.setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xff8b8e97));
	mLabelStatus.setFont(juce::Font(12.f, juce::Font::italic));
	mLabelStatus.setColour(juce::Label::textColourId, juce::Colour(0xffa9abb3));
	resized();
	repaint();
}

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
    applyLook();
    setAlpha(1.f);
}

AboutWindow::~AboutWindow()
{
	for (juce::Component* c : { (juce::Component*) &mButtonClose, (juce::Component*) &mActionButton,
	                            (juce::Component*) &mTextEditorEmailAddress, (juce::Component*) &mTextEditorSerialKey })
		c->setLookAndFeel(nullptr);

}

void AboutWindow::paint(juce::Graphics& g)
{
    // [Claude 2026-10-29] A panel in the header's style: top-lit grey, a thin border, the wordmark and version at the
    // top, then the licence state (Joe: "yes rebuild it"). Was the title pictures on black in a thick grey frame.
    auto r = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff222329), 0.f, 0.f, juce::Colour(0xff17181c), 0.f, r.getBottom(), false));
    g.fillRect(r);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillRect(r.withHeight(1.f).reduced(1.f, 0.f));
    g.setColour(juce::Colours::black);
    g.drawRect(r, 1.f);

    const float x = 20.f;

    // the wordmark and version
    auto nameFont = juce::Font(17.f, juce::Font::bold);
    nameFont.setExtraKerningFactor(0.22f);
    g.setFont(nameFont);
    g.setColour(juce::Colour(0xffe9e9ec));
    const juce::String name("TERTIARY");
    const float nameW = (float) nameFont.getStringWidth(name);
    g.drawText(name, juce::Rectangle<float>(x, 20.f, nameW + 8.f, 22.f), juce::Justification::centredLeft, false);
    g.setFont(juce::Font(12.5f));
    g.setColour(juce::Colour(0xff8b8e97));
    g.drawText("Multiband Tremolo", juce::Rectangle<float>(x, 44.f, 200.f, 16.f), juce::Justification::centredLeft, false);
    g.drawText("Version " + juce::String(JucePlugin_VersionString), juce::Rectangle<float>(x + 150.f, 44.f, r.getWidth() - x * 2.f - 150.f, 16.f),
               juce::Justification::centredRight, false);

    // the logo, small, at the top right beside the close key
    if (! imageCompanyTitle.isNull())
    {
        const float h = 12.f, w = h * imageCompanyTitle.getWidth() / (float) imageCompanyTitle.getHeight();
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(imageCompanyTitle, juce::Rectangle<float>(r.getRight() - x - 26.f - w, 25.f, w, h));
    }

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRect(juce::Rectangle<float>(x, 72.f, r.getWidth() - x * 2.f, 1.f));
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRect(juce::Rectangle<float>(x, 73.f, r.getWidth() - x * 2.f, 1.f));

    // the licence state
    auto line = juce::Rectangle<float>(x, 86.f, r.getWidth() - x * 2.f, 18.f);
    if (mIsDemoVersion)
    {
        g.setColour(juce::Colour(0xffc886dd));
        g.setFont(juce::Font(12.f, juce::Font::bold).withExtraKerningFactor(0.12f));
        g.drawText("DEMO", line, juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xffa9abb3));
        g.setFont(juce::Font(12.5f));
        g.drawFittedText("The effect bypasses for 10 seconds of every minute.\nActivate with the email and serial key from your purchase.",
                         line.translated(0.f, 20.f).withHeight(34.f).toNearestInt(), juce::Justification::topLeft, 2);
    }
    else
    {
        g.setColour(juce::Colour(0xff5bbd7d));
        g.setFont(juce::Font(12.f, juce::Font::bold).withExtraKerningFactor(0.12f));
        g.drawText("ACTIVATED", line, juce::Justification::centredLeft, false);
        const auto email = mTextEditorEmailAddress.getText().fromFirstOccurrenceOf("Email: ", false, true).trim();
        const auto key = mTextEditorSerialKey.getText().fromFirstOccurrenceOf("Key: ", false, true).trim();
        g.setColour(juce::Colour(0xffd6d7dc));
        g.setFont(juce::Font(13.f));
        g.drawText(email.isNotEmpty() ? email : juce::String("This computer"), line.translated(0.f, 22.f), juce::Justification::centredLeft, true);
        if (key.isNotEmpty())
        {
            g.setColour(juce::Colour(0xff8b8e97));
            g.setFont(juce::Font(12.f));
            g.drawText("Key ending " + key.getLastCharacters(4), line.translated(0.f, 40.f), juce::Justification::centredLeft, false);
        }
        g.setColour(juce::Colour(0xffa9abb3));
        g.setFont(juce::Font(12.5f));
        g.drawText("Thank you for your purchase.", line.translated(0.f, 64.f), juce::Justification::centredLeft, false);
    }
}

void AboutWindow::resized()
{
    // [Claude 2026-10-29] laid out for the rebuilt panel
    auto bounds = getLocalBounds();
    const int x = 20, w = bounds.getWidth() - x * 2;

    mButtonClose.setBounds(bounds.getRight() - x - 18, 22, 18, 18);
    mButtonWebsiteLink.setBounds(bounds.getX(), bounds.getBottom() - 30, bounds.getWidth(), 20);

    if (mIsDemoVersion)
    {
        mTextEditorEmailAddress.setBounds(x, 146, w, 26);
        mTextEditorSerialKey.setBounds(x, 178, w, 26);
        mActionButton.setBounds(x, 214, w, 28);
        mLabelStatus.setBounds(x, 246, w, 28);
    }
    else
    {
        mActionButton.setBounds(x, 194, w, 26);
        mLabelStatus.setBounds(x, 224, w, 28);
    }
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





// [Claude 2026-10-27] The three server calls below are the same requests as in 1.0.2, now made through
// licenseServer (LicenseServer.h). Changes: a SafePointer, so a reply that arrives after the window is closed
// no longer touches a deleted window (that could crash the host); the email is sent in lower case (the server
// compares it in lower case); the 5 s timeout now applies (it was set on a copy before); and the instance is a
// real machine ID (LicenseCheck::getMachineInstance) instead of the computer's model name.
void AboutWindow::attemptSerialKeyValidation()
{
    auto serialKey = mTextEditorSerialKey.getText().trim();
    auto email = LicenseCheck::normaliseEmail(mTextEditorEmailAddress.getText());

    if (serialKey.isEmpty())
    {
        mLabelStatus.setText("Enter a serial key.", juce::dontSendNotification);
        return;
    }

    mLabelStatus.setText("Validating...", juce::dontSendNotification);

    juce::Component::SafePointer<AboutWindow> safeThis(this);
    auto server = licenseServer;

    juce::Thread::launch([safeThis, server, serialKey, email]()
    {
        auto reply = server->validate(serialKey, email);

        juce::MessageManager::callAsync([safeThis, reply]()
        {
            if (safeThis == nullptr)
                return;

            if (!reply.reachable)
                safeThis->mLabelStatus.setText("Connection to server failed.", juce::dontSendNotification);

            // Key is confirmed to be valid
            else if (reply.code == "key_valid")
            {
                safeThis->mLabelStatus.setText("Serial key valid!", juce::dontSendNotification);
                safeThis->attemptSerialKeyActivation();
            }
            else
            {
                safeThis->mLabelStatus.setText("Error: " + reply.message, juce::dontSendNotification);
                DBG("API Error: " + reply.code + " - " + reply.message);
            }
        });
    });
}



void AboutWindow::attemptSerialKeyActivation()
{
    auto serialKey = mTextEditorSerialKey.getText().trim();
    auto email = LicenseCheck::normaliseEmail(mTextEditorEmailAddress.getText());

    juce::PropertiesFile props(LicenseCheck::getSettingsOptions());
    juce::String instance = LicenseCheck::getMachineInstance(props);
    juce::String platform = juce::SystemStats::getOperatingSystemName();

    mLabelStatus.setText("Activating...", juce::dontSendNotification);

    juce::Component::SafePointer<AboutWindow> safeThis(this);
    auto server = licenseServer;

    juce::Thread::launch([safeThis, server, serialKey, email, instance, platform]()
    {
        auto reply = server->activate(serialKey, email, instance, platform);

        juce::MessageManager::callAsync([safeThis, reply]()
        {
            if (safeThis == nullptr)
                return;

            if (!reply.reachable)
                safeThis->mLabelStatus.setText("Activation connection failed.", juce::dontSendNotification);

            else if (reply.code == "key_activated" || reply.code == "instance_already_activated")
            {
                if (reply.code == "instance_already_activated")
                    safeThis->mLabelStatus.setText("Instance was already activated.  Reactivation successful!", juce::dontSendNotification);
                else
                    safeThis->mLabelStatus.setText("Activation successful!", juce::dontSendNotification);

                safeThis->updateAppOnActivationChange(true);
            }
            else
            {
                safeThis->mLabelStatus.setText("Activation error: " + reply.message, juce::dontSendNotification);
                DBG("API Error: " + reply.code + " - " + reply.message);
            }
        });
    });
}

void AboutWindow::attemptSerialKeyDeactivation()
{
	auto serialKey = mTextEditorSerialKey.getText().fromFirstOccurrenceOf("Key: ", false, true).trim();
	auto email = LicenseCheck::normaliseEmail(mTextEditorEmailAddress.getText().fromFirstOccurrenceOf("Email: ", false, true));

    // The instance it was activated with: the machine ID, or for a 1.0.x activation the device description
    juce::PropertiesFile props(LicenseCheck::getSettingsOptions());
    juce::String instance = LicenseCheck::getActivatedInstance(props);
    juce::String platform = juce::SystemStats::getOperatingSystemName();

    mLabelStatus.setText("Deactivating...", juce::dontSendNotification);

    juce::Component::SafePointer<AboutWindow> safeThis(this);
    auto server = licenseServer;

    juce::Thread::launch([safeThis, server, serialKey, email, instance, platform]()
    {
        auto reply = server->deactivate(serialKey, email, instance, platform);

        juce::MessageManager::callAsync([safeThis, reply]()
        {
            if (safeThis == nullptr)
                return;

            if (!reply.reachable)
                safeThis->mLabelStatus.setText("Deactivation connection failed.", juce::dontSendNotification);

            else if (reply.code == "key_deactivated" || reply.code == "instance_deactivated")
            {
                safeThis->mLabelStatus.setText("Deactivation successful.", juce::dontSendNotification);

                // Optionally clear local props or revert UI to demo mode
                safeThis->updateAppOnActivationChange(false);
            }
            else
            {
                safeThis->mLabelStatus.setText("Deactivation error: " + reply.message, juce::dontSendNotification);
                DBG("API Error: " + reply.code + " - " + reply.message);
            }
        });
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
	mIsDemoVersion = !isActivating;
	setUpSerialKeyEditor();
	setUpEmailEditor();
	setUpActionButton();
	applyLook();

}


void AboutWindow::updateLocalKeyData(bool isActivating)
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Tertiary"; // or whatever your plugin name is
    options.filenameSuffix = "settings";
    options.osxLibrarySubFolder = "Application Support"; // macOS-specific
    options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML; // Easier to read

    juce::PropertiesFile props(options);

	juce::String email = mTextEditorEmailAddress.getText().fromFirstOccurrenceOf("Email: ", false, true).trim();  // [Claude 2026-10-27] drop the "Email: " shown when activated

	juce::String serialKey = "";

	if (isActivating)
      serialKey = mTextEditorSerialKey.getText().trim();

    props.setValue("licenseKey", serialKey);
    props.setValue("licenseEmail", email);

    // [Claude 2026-10-27] For the weekly re-check: which machine ID was activated, and when it was last confirmed
    auto now = juce::String(juce::Time::currentTimeMillis());
    props.setValue("licenseInstance", isActivating ? LicenseCheck::getMachineInstance(props) : juce::String());
    props.setValue("licenseLastCheck", isActivating ? now : juce::String());
    props.setValue("licenseLastValid", isActivating ? now : juce::String());

    props.saveIfNeeded();

    juce::File file = props.getFile();
    DBG("Stored license data at: " + file.getFullPathName());
}



// [Claude 2026-10-27] Called by the editor when the weekly re-check has finished (LicenseCheck.h).
// A message goes in the status line; a newer version turns the website link into "Version X is available".
//==============================================================================
void AboutWindow::showLicenseCheckResult(const LicenseCheck::Result& result)
{
    if (result.message.isNotEmpty())
    {
        mLabelStatus.setText(result.message, juce::dontSendNotification);
    }

    if (result.newVersion.isNotEmpty())
        mButtonWebsiteLink.setButtonText("Version " + result.newVersion + " is available at WonderlandAudio.com");

    if (result.revoked)
        refreshActivationFields();
}

// [Claude 2026-10-27] Same as the end of updateAppOnActivationChange(), for a change made by the re-check
//==============================================================================
void AboutWindow::refreshActivationFields()
{
	mIsDemoVersion = audioProcessor.getDemoState();
	setUpSerialKeyEditor();
	setUpEmailEditor();
	setUpActionButton();
	applyLook();
}
