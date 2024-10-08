#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LFO.h"
#include "Params.h"

namespace audio_plugin {

    /* Audio-Path-Wrapper holding the parameters unique to the multiple effect bands */
    // ============================================================================================
    struct TremoloBand {
      /* Pointers to the APVTS */
      juce::AudioParameterBool* bypassParam{nullptr};
      juce::AudioParameterBool* muteParam{nullptr};
      juce::AudioParameterBool* soloParam{nullptr};

      juce::AudioParameterFloat* bandGainParam{nullptr};

      juce::dsp::Gain<float> bandGain;

      void updateTremoloSettings() {
        bandGain.setRampDurationSeconds(0.05);           // Reduce Artifacts
        bandGain.setGainDecibels(bandGainParam->get());  // Update Gain
      }
    };

    // ============================================================================================


    class AudioPluginAudioProcessor :   public juce::AudioProcessor,
                                        public juce::AudioProcessorValueTreeState::Listener
    {
    public:
        //==============================================================================
        AudioPluginAudioProcessor();
        ~AudioPluginAudioProcessor() override;

        juce::String mNameSpace{"PluginProcessor"};
        bool setDebug{true};

        //==============================================================================
        void prepareToPlay(double sampleRate, int samplesPerBlock) override;
        void releaseResources() override;

        #ifndef JucePlugin_PreferredChannelConfigurations
            bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
        #endif

        void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        //==============================================================================
        juce::AudioProcessorEditor* createEditor() override;
        bool hasEditor() const override;

        //==============================================================================
        const juce::String getName() const override;

        bool acceptsMidi() const override;
        bool producesMidi() const override;
        bool isMidiEffect() const override;
        double getTailLengthSeconds() const override;

        //==============================================================================
        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram(int index) override;
        const juce::String getProgramName(int index) override;
        void changeProgramName(int index, const juce::String& newName) override;

        //==============================================================================
        void getStateInformation(juce::MemoryBlock& destData) override;
        void setStateInformation(const void* data, int sizeInBytes) override;

        using APVTS = juce::AudioProcessorValueTreeState;  // Alias
        static APVTS::ParameterLayout
        createParameterLayout();  // Parameter Layout Function

        void parameterChanged(const juce::String& parameterID,
                            float newValue) override;

        void setFftPickoffPoint(int point);

        APVTS apvts{*this,                     // Audio Processor
                    nullptr,                   // Undo Manager
                    "Parameters",              // Identifier &valueTreeType
                    createParameterLayout()};  // parameterLayout

        juce::AudioParameterBool* getShowLowBandParam() { return showLowBandParam; }
        juce::AudioParameterBool* getShowMidBandParam() { return showMidBandParam; }
        juce::AudioParameterBool* getShowHighBandParam() { return showHighBandParam; }
        juce::AudioParameterBool* getStackBandsParam() { return stackBandsParam; }
        juce::AudioParameterFloat* getScopePoint1Param() { return scopePoint1Param; }
        juce::AudioParameterFloat* getScopePoint2Param() { return scopePoint2Param; }
        juce::AudioParameterBool* getShowFftParam() { return showFftParam; }
        juce::AudioParameterChoice* getFftPickofIdParam() { return fftPickoffParam; }

        #ifdef DEMO_VERSION
		   bool getIsDemoModeEnabled() { return effectEnabled; }
        #endif
        
        
        /* Input & Output Meters */
        // =========================================================================
        float getRmsValue(const int pickOffPoint, const int channel) const;

        /* LFO */
        // =========================================================================
        LFO lowLFO, midLFO, highLFO;  // Instances of LFO

        /* Oscilloscope */
        // =========================================================================
        std::atomic<double> getPlayPosition() { return playPosition; }
        juce::Atomic<bool> paramChangedForScope{false};

        /* Frequency Response */
        // =========================================================================
        void pushNextSampleIntoFifo(float sample);

        enum { fftOrder = 12, fftSize = 1 << fftOrder, scopeSize = 512 };

        float fifo[fftSize];
        float fftData[2 * fftSize];
        float scopeData[scopeSize];
        bool nextFFTBlockReady = false;
        int fifoIndex = 0;

    private:
        bool parameterChangedLfoLow{true};
        bool parameterChangedLfoMid{true};
        bool parameterChangedLfoHigh{true};

        /* Main DSP */
        void applyAmplitudeModulation();
        void sumAudioBands(juce::AudioBuffer<float>& buffer);

        void updateState();  // Process Block, Update State

        void forceSynchronization(LFO& lfo);
        double playPosition;
        
		/* Demo Version */
        // =========================================================================
        bool effectEnabled = true;
        
		#ifdef DEMO_VERSION
        int demoActiveTimeLimit{50};
        int demoBypassTimeLimit{10};
		int64_t sampleCounter = 0;
		int64_t samplesUntilDisable;
		int64_t samplesUntilEnable;
		#endif

        /* Parameter Change Detection Flags */
        // =========================================================================
        float multLow{1.f}, multMid{1.f}, multHigh{1.f};
        float oldMultLow{0.f}, oldMultMid{0.f}, oldMultHigh{0.f};
        bool hitPlay{false};
        bool setLatchPlay{false};
        bool setLatchStop{true};
        bool rateChanged{true};
        bool multChanged{true};

        void attachParametersToLayout();
        void addParameterListeners();

        /* FrequencyResponse */
        // =========================================================================
        bool fftPickoffPointIsInput{false};
        void pushSignalToFFT(juce::AudioBuffer<float>& buffer);

        /* BPM and Host Info */
        // =========================================================================
        juce::AudioPlayHead::CurrentPositionInfo hostInfo;  // To get BPM
        double lastSampleRate;                              // To get Sample Rate
        juce::AudioPlayHead* playHead;                      // i To get BPM

        /* Crossover */
        // =========================================================================
        void applyCrossover(
            const juce::AudioBuffer<float>& inputBuffer);  // Crossover Function
        using Filter = juce::dsp::LinkwitzRileyFilter<float>;
        Filter LP1, AP2, HP1, LP2, HP2;
        juce::AudioParameterFloat* lowMidCrossover{
            nullptr};  // Pointer to the APVTS
        juce::AudioParameterFloat* midHighCrossover{
            nullptr};  // Pointer to the APVTS

        /* Input & Output Gain */
        // =========================================================================
        juce::dsp::Gain<float> inputGain, outputGain;
        juce::AudioParameterFloat* inputGainParam{
            nullptr};  // Pointer to the APVTS
        juce::AudioParameterFloat* outputGainParam{
            nullptr};  // Pointer to the APVTS

        /* TEST: GUI PARAMS */
        juce::AudioParameterBool* showLowBandParam{nullptr};   // BOOL
        juce::AudioParameterBool* showMidBandParam{nullptr};   // BOOL
        juce::AudioParameterBool* showHighBandParam{nullptr};  // BOOL
        juce::AudioParameterBool* stackBandsParam{nullptr};    // BOOL

        juce::AudioParameterFloat* scopePoint1Param{nullptr};
        juce::AudioParameterFloat* scopePoint2Param{nullptr};

        juce::AudioParameterBool* showFftParam{nullptr};
        juce::AudioParameterChoice* fftPickoffParam{nullptr};

        /* Input & Output Meters */
        // =========================================================================
        juce::LinearSmoothedValue<float> rmsLevelInputLeft, rmsLevelInputRight,
            rmsLevelOutputLeft, rmsLevelOutputRight;
        void updateMeterLevels(juce::LinearSmoothedValue<float>& left,
                                juce::LinearSmoothedValue<float>& right,
                                juce::AudioBuffer<float>& buffer);

        // Function to apply input/output gain
        template <typename T, typename U>
        void applyGain(T& buffer, U& gain) {
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
        gain.process(ctx);
        }

        /* Tremolo-Bands */
        // =========================================================================
        std::array<TremoloBand, 3>
            tremolos;  // Array of param-wrappers containing byp/mute/solo for
                        // each band
        TremoloBand& lowBandTrem = tremolos[0];   // Low Band
        TremoloBand& midBandTrem = tremolos[1];   // Mid Band
        TremoloBand& highBandTrem = tremolos[2];  // High Band
        std::array<juce::AudioBuffer<float>, 3> filterBuffers;  // An array of 3 buffers which contain the filtered audio

      JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)

    };

} // namespace audio_plugin
