#include "PluginEditor.h"
#include <cstdlib>
#include "PluginProcessor.h"

// [Claude 2026-10-29] Review renders only (WLA_FOCUS=MENU): pop-up menus open in their own window, which an offline
// render cannot capture, so this draws the ☰ menu's items in place with the same look, for a still
struct MenuPreview : juce::Component
{
  juce::StringArray items; juce::Array<bool> active;
  void paint(juce::Graphics& g) override
  {
    auto& look = HouseMenuLookAndFeel::get();
    look.drawPopupMenuBackground(g, getWidth(), getHeight());
    int y = 4;
    for (int i = 0; i < items.size(); ++i)
    {
      const bool sep = items[i].isEmpty();
      const int h = sep ? 9 : 22;
      look.drawPopupMenuItem(g, { 4, y, getWidth() - 8, h }, sep, active[i], i == 0, false, false, items[i], {}, nullptr, nullptr);
      y += h;
    }
  }
};

namespace audio_plugin {

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      aboutWindow(audioProcessor, *this)  // <-- added here explicitly
{
      // [Claude 2026-10-29] everything below is a child of `content`, which the window scales (see the header)
      addAndMakeVisible(content);
      content.onPaint = [this](juce::Graphics& g) { paintContent(g); };

      // [Claude 2026-10-29] every font that does not name a family uses the house typeface
      juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName(houseTypeface());
    checkForSerialKeyOnStartup();


		/* Used for Hi-Res Screen Capture... for graphics*/
		fullScreenScope = false;

      /* Container class for top banner */
      if (!fullScreenScope)
          content.addAndMakeVisible(topBanner);

      topBanner.addMouseListener(this, true);

      /* Set UI is Resizable for Hi-Res Screen Capturing, etc */
      // [Claude 2026-10-29] Resizable from 75 % to 250 %, always the same shape; it opens at the size last used
      // (the ☰ menu has 100 / 125 / 150 / 200 %). The window was 750 x 515 before the shorter layout.
      setResizable(true, false);   // no corner grip: it sat over the right meter; hosts and the standalone resize from their window edges
      setResizeLimits(baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 5 / 2, baseHeight * 5 / 2);
      getConstrainer()->setFixedAspectRatio((double) baseWidth / (double) baseHeight);
      uiScale = loadUiScale();
      setSize(juce::roundToInt(baseWidth * uiScale), juce::roundToInt(baseHeight * uiScale));

      // Scope Channels listens to Band Controls
      globalControls.getLowControl().addActionListener(
          &wrapperOscilloscope.getOscilloscopeLow());
      globalControls.getMidControl().addActionListener(
          &wrapperOscilloscope.getOscilloscopeMid());
      globalControls.getHighControl().addActionListener(
          &wrapperOscilloscope.getOscilloscopeHigh());

      // Band Controls listen to Scope Channels
      wrapperOscilloscope.getOscilloscopeLow().addActionListener(
          &globalControls.getLowControl());
      wrapperOscilloscope.getOscilloscopeMid().addActionListener(
          &globalControls.getMidControl());
      wrapperOscilloscope.getOscilloscopeHigh().addActionListener(
          &globalControls.getHighControl());

      // Frequency Response listens to Controls
      globalControls.getLowControl().addActionListener(
          &wrapperFrequency.getFrequencyResponse());
      globalControls.getMidControl().addActionListener(
          &wrapperFrequency.getFrequencyResponse());
      globalControls.getHighControl().addActionListener(
          &wrapperFrequency.getFrequencyResponse());

      // Scope Channels listen to Global Controls
      // globalControls.addActionListener    (
      // &wrapperOscilloscope.getOscilloscopeLow()     );
      // globalControls.addActionListener    (
      // &wrapperOscilloscope.getOscilloscopeMid()     );
      // globalControls.addActionListener    (
      // &wrapperOscilloscope.getOscilloscopeHigh()    );

      globalControls.getLowControl().broadcastInitialParameters();
      globalControls.getMidControl().broadcastInitialParameters();
      globalControls.getHighControl().broadcastInitialParameters();

      // Frequency Response listens to Global Controls
      globalControls.addActionListener(&wrapperFrequency.getFrequencyResponse());

      globalControls.addActionListener(&wrapperOscilloscope.getOscilloscopeLow());
      globalControls.addActionListener(&wrapperOscilloscope.getOscilloscopeMid());
      globalControls.addActionListener(&wrapperOscilloscope.getOscilloscopeHigh());

      /* Container class for all parameter controls */
	      if (!fullScreenScope)
      content.addAndMakeVisible(globalControls);

      /* Time-Domain Display */
      content.addAndMakeVisible(wrapperOscilloscope);

      /* Frequency-Domain & Crossover Display */
	  if (!fullScreenScope)
      content.addAndMakeVisible(wrapperFrequency);

	  if (!fullScreenScope) {
            content.addAndMakeVisible(aboutWindow);
            aboutWindow.setVisible(false);
            // [Claude 2026-10-29] review renders only: WLA_FOCUS=ABOUT shows the About window, for a still
            if (const char* f = std::getenv("WLA_FOCUS")) if (juce::String(f).contains("ABOUT")) aboutWindow.setVisible(true);
      }

      // [Claude 2026-10-27] Presets, over the left of the banner
      if (!fullScreenScope)
        content.addAndMakeVisible(presetBar);
        // [Claude 2026-10-29] Activate and the menu in the new header open the About window, where activation is
        topBanner.onActivate = [this] { aboutWindow.setVisible(true); aboutWindow.toFront(false); };
        aboutWindow.toFront(false);   // [Claude 2026-10-29] above the preset bar, which was added after it
        topBanner.onMenu = [this] { showMainMenu(); };
        globalControls.onDemoBypassChanged = [this](bool bypassed)   // [Claude 2026-10-29] one bypassed state for the whole window
        {
          wrapperOscilloscope.setAlpha(bypassed ? 0.35f : 1.f);
          wrapperFrequency.setAlpha(bypassed ? 0.35f : 1.f);
          topBanner.setDemoBypassed(bypassed);
        };   // [Claude 2026-10-29] a real menu (was: toggle the About window)
        if (const char* f = std::getenv("WLA_FOCUS")) if (juce::String(f).contains("MENU"))
        {
          auto* m = new MenuPreview();
          mMenuPreview.reset(m);
          m->items = { audioProcessor.getDemoState() ? "Activate Tertiary..." : "About Tertiary...",
                       "Tertiary " + juce::String(JucePlugin_VersionString) + " is up to date", "Open presets folder", "", "WonderlandAudio.com" };
          m->active = { true, false, true, true, true };
          m->setBounds(750 - 14 - 200, 44, 200, 4 + 22 * 4 + 9 + 4);
          content.addAndMakeVisible(m);   // a still only
          m->toFront(false);
        }

      // [Claude 2026-10-27] Weekly serial-key re-check and update check, on the processor's own thread
      audioProcessor.licenseCheck.addChangeListener(this);
      audioProcessor.startLicenseCheck();
      showLicenseCheckResult();   // In case it already finished while the editor was closed


    }



    //==============================================================================
    AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
      saveUiScale();   // [Claude 2026-10-29] the next window opens at this size
      audioProcessor.licenseCheck.removeChangeListener(this);   // [Claude 2026-10-27]
    }

    // [Claude 2026-10-27] Called on the message thread when the license check has finished
    //==============================================================================
    void AudioPluginAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source) {
      showLicenseCheckResult();
    }

    // [Claude 2026-10-27] Back to demo if the key was revoked (and open the About window to say why),
    // a quiet note if offline too long, and the "Version X is available" link
    //==============================================================================
    void AudioPluginAudioProcessorEditor::showLicenseCheckResult() {
      auto result = audioProcessor.licenseCheck.getResult();

      if (!result.finished)
        return;

      updateDemoState(audioProcessor.getDemoState());
      aboutWindow.showLicenseCheckResult(result);
      topBanner.setUpdateVersion(result.newVersion);

      if (result.revoked && !fullScreenScope)
        aboutWindow.setVisible(true);
    }

	//==============================================================================
	void AudioPluginAudioProcessorEditor::checkForSerialKeyOnStartup()
	{
    	// [1] Get state from Processor
        bool mIsDemoVersion = audioProcessor.getDemoState();

		// Propagate state to rest of GUI
		updateDemoState(mIsDemoVersion);
		
	}

	//==============================================================================
	void AudioPluginAudioProcessorEditor::updateDemoState(bool newDemoState)
	{
		// [1] Pass state to Top Banner
        topBanner.setDemoState(newDemoState);

		// [2] Pass state to Global Controls
        globalControls.setDemoState(newDemoState);

		// [3] Pass state to About Window
        aboutWindow.setDemoState(newDemoState);
		
		repaint();
	}

    //==============================================================================
    #if JUCE_WINDOWS
    void AudioPluginAudioProcessorEditor::parentHierarchyChanged()
    {
        // Force JUCE's Software Renderer on Windows.
        // This is primarily a compatibility workaround for older Intel iGPU/driver
        // combinations where Direct2D can produce missing fills / corruption.
        if (auto* peer = getPeer())
            peer->setCurrentRenderingEngine(0); // 0 = "Software Renderer"
    }
    #endif

    //==============================================================================
    void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
      g.fillAll(juce::Colours::black);   // [Claude 2026-10-29] under the scaled content; it covers the window
    }

    // [Claude 2026-10-29] What the editor used to paint, now painted by the content at the 750 x 478 layout
    void AudioPluginAudioProcessorEditor::paintContent(juce::Graphics& g) {
      /* Set the Gradient */
      g.setGradientFill(AllColors::PluginEditorColors::BACKGROUND_GRADIENT(
          content.getLocalBounds().toFloat()));
      g.fillAll();

      // [Claude 2026-10-29] The strip between the scope and the crossover is the frames' platform grey, so the two
      // recesses sit in one surface (Joe: "restore the plateau on this section")
      const int x0 = wrapperOscilloscope.getRight(), x1 = wrapperFrequency.getX();
      if (x1 > x0)
      {
          // [Claude 2026-10-29] the frame grey itself, not the second step's shade, so the strip and the two outer
          // steps beside it are one even plateau rather than a lighter-darker-lighter ridge
          g.setColour(ColorScheme::WindowBorders::getWindowBorderColor());
          g.fillRect(snapToScreenPixels(g, juce::Rectangle<float>((float) x0 - 1.f, (float) wrapperOscilloscope.getY(),
                                                                  (float) (x1 - x0 + 2), (float) wrapperOscilloscope.getHeight()), &content));
      }
    }

    //==============================================================================
    void AudioPluginAudioProcessorEditor::resized() {
      using namespace juce;

      //const float aspectRatio = 750.f / 515.f;  // Width / Height
		  
	  //DBG("Updated DPI Scale Factor: " + juce::String(Desktop::getInstance().getGlobalScaleFactor()));


      /* Enforce Aspect Ratio */
      //auto availableArea = getLocalBounds();
      //int newWidth = availableArea.getWidth();
      //int newHeight = roundToInt(newWidth / aspectRatio);

      //if (newHeight > availableArea.getHeight()) {
      //  // Height is constrained by availableArea
      //  newHeight = availableArea.getHeight();
      //  newWidth = roundToInt(newHeight * aspectRatio);
      //}

      // Set the new size and center the component
      //setSize(newWidth, newHeight);
      //centreWithSize(newWidth, newHeight);

      // [Claude 2026-10-29] the content keeps its 750 x 478 layout and is scaled to the window's width
      uiScale = (float) getWidth() / (float) baseWidth;
      content.setTransform({});
      content.setBounds(0, 0, baseWidth, baseHeight);
      content.setTransform(juce::AffineTransform::scale(uiScale));

      buildFlexboxLayout();

      auto bounds = content.getLocalBounds();

      auto aboutWidth = 360;   // [Claude 2026-10-29] the rebuilt panel; was 400 x 450
      auto aboutHeight = 300;
      auto aboutX = bounds.getCentreX() - aboutWidth / 2;
      auto aboutY = bounds.getCentreY() - aboutHeight / 2;

      aboutWindow.setBounds(aboutX, aboutY, aboutWidth, aboutHeight);
      aboutWindow.toFront(false);
    }

    /* Builds the UI layout */
    //==============================================================================
    void AudioPluginAudioProcessorEditor::buildFlexboxLayout() {
      using namespace juce;

      auto bounds = content.getLocalBounds();
      bounds.reduce(0, 0);  // [Claude 2026-10-29] edge to edge (Joe: "expanding the sections all the way to the left/right edges"); was 5

      /* Create a horizontal FlexBox container to arrange the Oscilloscope and
       * Frequency Response */
      juce::FlexBox flexBoxRow;
      flexBoxRow.flexDirection = FlexBox::Direction::row;
      flexBoxRow.flexWrap = FlexBox::Wrap::noWrap;

      //auto spacerH = FlexItem().withWidth(5);  // Gap between O-Scope and Freq Resp
      auto spacerH = FlexItem().withWidth(4);  // [Claude 2026-10-29] a narrow separator between the scope and the crossover (Joe: "this little gap is awkward", then "widen this separator just a bit"); was 1 % of the width
      //auto spacerH = FlexItem().withWidth(0);

      flexBoxRow.items.add(
          FlexItem(wrapperOscilloscope).withFlex(1.f));  // Insert Scope Wrapper
      flexBoxRow.items.add(spacerH);                     // Insert Spacer
      flexBoxRow.items.add(
          FlexItem(wrapperFrequency).withFlex(1.f));  // Insert Freq Wrapper

      /* Create a vertical flexBox container to arrange the Row and Global Controls
       */

      juce::FlexBox flexBox;
      flexBox.flexDirection = FlexBox::Direction::column;
      flexBox.flexWrap = FlexBox::Wrap::noWrap;

      //flexBox.items.add(FlexItem(topBanner).withHeight(50)); <<<<<<<<<<
      const float unit = content.getWidth() / 750.f;  // [Claude 2026-10-29] the header and the displays keep their size; only the controls area got shorter
      flexBox.items.add(FlexItem(topBanner).withHeight(51.5f * unit));
      flexBox.items.add(FlexItem(flexBoxRow).withHeight(206.f * unit));
      flexBox.items.add(FlexItem(globalControls).withFlex(1.f));

      flexBox.performLayout(bounds);

      // [Claude 2026-10-27] The preset bar sits on the left of the banner; the line under it is the
      // banner's own (the demo note, or the update link)
      // [Claude 2026-10-29] the preset sits in the middle of the new header
      presetBar.setBounds(topBanner.getX() + 248, topBanner.getY() + (topBanner.getHeight() - 22) / 2, 172, 22);   // ends one gap (16 px) before DEMO

	  	if (fullScreenScope)
		  wrapperOscilloscope.setBounds(content.getLocalBounds());

    }

    // [Claude 2026-10-29] The ☰ menu: only what Tertiary can back today (Joe: "yeah let's update it")
    //==============================================================================
    juce::PopupMenu AudioPluginAudioProcessorEditor::buildMainMenu()
    {
      juce::PopupMenu menu;
      menu.setLookAndFeel(&HouseMenuLookAndFeel::get());
      const bool demo = audioProcessor.getDemoState();
      menu.addItem(1, demo ? "Activate Tertiary..." : "About Tertiary...");
      const auto result = audioProcessor.licenseCheck.getResult();
      if (result.newVersion.isNotEmpty())
        menu.addItem(2, "Get version " + result.newVersion + "...");
      else
        menu.addItem(2, "Tertiary " + juce::String(JucePlugin_VersionString) + " is up to date", false);
     #if ! JUCE_EMSCRIPTEN
      menu.addItem(3, "Open presets folder");
     #endif
      // [Claude 2026-10-29] window size; the window can also be dragged to any size between 75 % and 250 %
      juce::PopupMenu size;
      for (int pct : { 100, 125, 150, 200 })
        size.addItem(100 + pct, juce::String(pct) + " %", true, std::abs(uiScale * 100.f - (float) pct) < 1.f);
      menu.addSubMenu("Window size", size);
      menu.addSeparator();
      menu.addItem(4, "WonderlandAudio.com");
      return menu;
    }

    // [Claude 2026-10-29] Window size, kept in Tertiary's settings file beside the licence
    //==============================================================================
    static juce::PropertiesFile::Options tertiarySettings()
    {
      juce::PropertiesFile::Options options;
      options.applicationName = "Tertiary";
      options.filenameSuffix = "settings";
      options.osxLibrarySubFolder = "Application Support";
      options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML;
      return options;
    }

    float AudioPluginAudioProcessorEditor::loadUiScale()
    {
     #if WLA_REVIEW_RENDER_BUILD
      // review renders are laid out at 100 % (their --scale sets the picture's size); WLA_UISCALE tries a window size
      if (const char* v = std::getenv("WLA_UISCALE")) return juce::jlimit(0.75f, 2.5f, juce::String(v).getFloatValue());
      return 1.f;
     #else
      juce::PropertiesFile props(tertiarySettings());
      if (props.containsKey("uiScale"))
        return juce::jlimit(0.75f, 2.5f, (float) props.getDoubleValue("uiScale", 1.0));
      // first open: 125 % where the screen has room for it (Joe: "yes 125% by default"), else 100 %
      if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        if (display->userArea.getWidth() >= 1700 && display->userArea.getHeight() >= 1000)
          return 1.25f;
      return 1.f;
     #endif
    }

    void AudioPluginAudioProcessorEditor::saveUiScale()
    {
     #if ! WLA_REVIEW_RENDER_BUILD
      juce::PropertiesFile props(tertiarySettings());
      props.setValue("uiScale", (double) uiScale);
      props.saveIfNeeded();
     #endif
    }

    void AudioPluginAudioProcessorEditor::setUiScale(float scale)
    {
      setSize(juce::roundToInt(baseWidth * scale), juce::roundToInt(baseHeight * scale));
      saveUiScale();
    }

    void AudioPluginAudioProcessorEditor::showMainMenu()
    {
      juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safeThis(this);
      buildMainMenu().showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&topBanner).withTargetScreenArea(topBanner.localAreaToGlobal(topBanner.getLocalBounds().removeFromRight(40)))   /* [Claude 2026-10-29] through the banner, so the window's scale is counted (it opened in the 100 % spot) */
                                                               .withMinimumWidth(190),
        [safeThis](int item)
        {
          if (safeThis == nullptr || item == 0) return;
          auto& self = *safeThis;
          if (item == 1) { self.aboutWindow.setVisible(true); self.aboutWindow.toFront(false); }
          else if (item == 2 || item == 4) juce::URL("https://www.wonderlandaudio.com").launchInDefaultBrowser();
          else if (item > 100) self.setUiScale((float) (item - 100) / 100.f);
          else if (item == 3)
          {
            self.audioProcessor.presets.getUserFolder().createDirectory();
            self.audioProcessor.presets.getUserFolder().startAsProcess();
          }
        });
    }

    /* Temporary Double-Click Callback*/
    void AudioPluginAudioProcessorEditor::mouseDoubleClick(
        const juce::MouseEvent& event) {
      if (topBanner.isMouseOver()) {
        if (!aboutWindow.isVisible()) {
          aboutWindow.setVisible(true);
          // aboutWindow.toFront(false);
        } else
          aboutWindow.setVisible(false);
      }
    }








} // namespace audio_plugin