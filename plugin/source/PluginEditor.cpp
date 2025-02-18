#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace audio_plugin {

    AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
        AudioPluginAudioProcessor& p)
        : AudioProcessorEditor(&p), audioProcessor(p) {

		// Force DPI Awareness
		//#if JUCE_WINDOWS
  //         SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		//#endif

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
      setSize(750 * scale, 515 * scale);

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
          }
    }

    //==============================================================================
    AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}


    //==============================================================================
    void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
      /* Set the Gradient */
      g.setGradientFill(AllColors::PluginEditorColors::BACKGROUND_GRADIENT(
          getLocalBounds().toFloat()));
      g.fillAll();
    }

    //==============================================================================
    void AudioPluginAudioProcessorEditor::resized() {
      using namespace juce;

      //const float aspectRatio = 750.f / 515.f;  // Width / Height
		  
	  DBG("Updated DPI Scale Factor: " + juce::String(Desktop::getInstance().getGlobalScaleFactor()));


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

      auto aboutWidth = 400;
      auto aboutHeight = 300;
      auto aboutX = bounds.getCentreX() - aboutWidth / 2;
      auto aboutY = bounds.getCentreY() - aboutHeight / 2;

      aboutWindow.setBounds(aboutX, aboutY, aboutWidth, aboutHeight);
      // aboutWindow.toFront(false);
    }

    /* Builds the UI layout */
    //==============================================================================
    void AudioPluginAudioProcessorEditor::buildFlexboxLayout() {
      using namespace juce;

      auto bounds = getLocalBounds();
      bounds.reduce(5, 0);

      /* Create a horizontal FlexBox container to arrange the Oscilloscope and
       * Frequency Response */
      juce::FlexBox flexBoxRow;
      flexBoxRow.flexDirection = FlexBox::Direction::row;
      flexBoxRow.flexWrap = FlexBox::Wrap::noWrap;

      //auto spacerH = FlexItem().withWidth(5);  // Gap between O-Scope and Freq Resp
      auto spacerH = FlexItem().withWidth(getWidth()*0.01f);  // Gap between O-Scope and Freq Resp
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
      flexBox.items.add(FlexItem(topBanner).withHeight(getHeight() * 0.1f));
      flexBox.items.add(FlexItem(flexBoxRow).withHeight(getHeight() * 0.4f));
      flexBox.items.add(FlexItem(globalControls).withFlex(1.f));

      flexBox.performLayout(bounds);

	  	if (fullScreenScope)
		  wrapperOscilloscope.setBounds(getLocalBounds());

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