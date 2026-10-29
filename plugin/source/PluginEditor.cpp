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
      // [Claude 2026-10-29] every font that does not name a family uses the house typeface
      juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName(houseTypeface());
    checkForSerialKeyOnStartup();


		/* Used for Hi-Res Screen Capture... for graphics*/
		fullScreenScope = false;

      /* Container class for top banner */
      if (!fullScreenScope)
          addAndMakeVisible(topBanner);

      topBanner.addMouseListener(this, true);

      /* Set UI is Resizable for Hi-Res Screen Capturing, etc */
      //setResizable(true, false);
      //setResizeLimits(750, 515, 10000, 10000);
      
      
      /* Sets window size */
      float scale = 1.f;
      setSize(750 * scale, 478 * scale);  // [Claude 2026-10-29] shorter, the padding taken out (Joe); was 515

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
      addAndMakeVisible(globalControls);

      /* Time-Domain Display */
      addAndMakeVisible(wrapperOscilloscope);

      /* Frequency-Domain & Crossover Display */
	  if (!fullScreenScope)
      addAndMakeVisible(wrapperFrequency);

	  if (!fullScreenScope) {
            addAndMakeVisible(aboutWindow);
            aboutWindow.setVisible(false);
            // [Claude 2026-10-29] review renders only: WLA_FOCUS=ABOUT shows the About window, for a still
            if (const char* f = std::getenv("WLA_FOCUS")) if (juce::String(f).contains("ABOUT")) aboutWindow.setVisible(true);
      }

      // [Claude 2026-10-27] Presets, over the left of the banner
      if (!fullScreenScope)
        addAndMakeVisible(presetBar);
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
          addAndMakeVisible(m);   // a still only
          m->toFront(false);
        }

      // [Claude 2026-10-27] Weekly serial-key re-check and update check, on the processor's own thread
      audioProcessor.licenseCheck.addChangeListener(this);
      audioProcessor.startLicenseCheck();
      showLicenseCheckResult();   // In case it already finished while the editor was closed


    }



    //==============================================================================
    AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
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
      /* Set the Gradient */
      g.setGradientFill(AllColors::PluginEditorColors::BACKGROUND_GRADIENT(
          getLocalBounds().toFloat()));
      g.fillAll();

      // [Claude 2026-10-29] The strip between the scope and the crossover is the frames' platform grey, so the two
      // recesses sit in one surface (Joe: "restore the plateau on this section")
      const int x0 = wrapperOscilloscope.getRight(), x1 = wrapperFrequency.getX();
      if (x1 > x0)
      {
          // [Claude 2026-10-29] the frame grey itself, not the second step's shade, so the strip and the two outer
          // steps beside it are one even plateau rather than a lighter-darker-lighter ridge
          g.setColour(ColorScheme::WindowBorders::getWindowBorderColor());
          g.fillRect(x0 - 1, wrapperOscilloscope.getY(), x1 - x0 + 2, wrapperOscilloscope.getHeight());
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

      buildFlexboxLayout();

      auto bounds = getLocalBounds();

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

      auto bounds = getLocalBounds();
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
      const float unit = getWidth() / 750.f;  // [Claude 2026-10-29] the header and the displays keep their size; only the controls area got shorter
      flexBox.items.add(FlexItem(topBanner).withHeight(51.5f * unit));
      flexBox.items.add(FlexItem(flexBoxRow).withHeight(206.f * unit));
      flexBox.items.add(FlexItem(globalControls).withFlex(1.f));

      flexBox.performLayout(bounds);

      // [Claude 2026-10-27] The preset bar sits on the left of the banner; the line under it is the
      // banner's own (the demo note, or the update link)
      // [Claude 2026-10-29] the preset sits in the middle of the new header
      presetBar.setBounds(topBanner.getX() + 248, topBanner.getY() + (topBanner.getHeight() - 22) / 2, 160, 22);

	  	if (fullScreenScope)
		  wrapperOscilloscope.setBounds(getLocalBounds());

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
      menu.addSeparator();
      menu.addItem(4, "WonderlandAudio.com");
      return menu;
    }

    void AudioPluginAudioProcessorEditor::showMainMenu()
    {
      juce::Component::SafePointer<AudioPluginAudioProcessorEditor> safeThis(this);
      buildMainMenu().showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(localAreaToGlobal(topBanner.getBounds().removeFromRight(40)))
                                                               .withMinimumWidth(190),
        [safeThis](int item)
        {
          if (safeThis == nullptr || item == 0) return;
          auto& self = *safeThis;
          if (item == 1) { self.aboutWindow.setVisible(true); self.aboutWindow.toFront(false); }
          else if (item == 2 || item == 4) juce::URL("https://www.wonderlandaudio.com").launchInDefaultBrowser();
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