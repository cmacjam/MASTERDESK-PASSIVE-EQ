#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/TubeStage.h"

class PassiveMasterEQProcessor : public juce::AudioProcessor
{
public:
    PassiveMasterEQProcessor();
    ~PassiveMasterEQProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return (int) ppm::presets::all().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // GUI helpers ---------------------------------------------------------
    ppm::ChannelSettings getChannelSettings (int ch) const;   // honours stereo link
    ppm::ChannelSettings getOwnChannelSettings (int ch) const { return chanParams[ch].read(); }
    bool isLinked() const { return linkParam->load() > 0.5f; }
    void copyChannel (int fromCh, int toCh);
    int  getOversamplingFactor() const { return 1 << osLog2; }

    // Meters (written by the audio thread, read by the GUI) ---------------
    struct Meters
    {
        std::atomic<float> outRms[2]  { {0.0f}, {0.0f} };   // last block RMS, linear
        std::atomic<float> outPeak[2] { {0.0f}, {0.0f} };   // peak hold, GUI resets
        std::atomic<float> thd[2]     { {0.0f}, {0.0f} };   // tube harmonics ratio
        std::atomic<float> inPeak     { 0.0f };             // input peak hold, GUI resets
        std::atomic<int>   numChannels { 2 };
    } meters;

    juce::AudioProcessorValueTreeState apvts;

private:
    ppm::params::ChannelParamPtrs chanParams[2];
    std::atomic<float>* linkParam  = nullptr;
    std::atomic<float>* tubeOnParam = nullptr;
    std::atomic<float>* driveParam  = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int osLog2 = 0;
    int currentProgram = 0;
    ppm::ChannelEngine engines[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PassiveMasterEQProcessor)
};
