#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/TubeStage.h"
#include "dsp/VariMu.h"
#include "dsp/Loudness.h"

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
    ppm::CompSettings getCompSettings (int ch) const;          // honours stereo link
    int getCompOrder() const { return (int) std::lround (orderParam->load()); }
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
        std::atomic<float> gr[2]      { {0.0f}, {0.0f} };   // compressor gain reduction, dB
        std::atomic<int>   numChannels { 2 };
    } meters;

    // Output audio for the analyzer / oscilloscope (audio thread writes, GUI reads)
    struct ScopeFifo
    {
        static constexpr int kSize = 32768;
        juce::AbstractFifo fifo { kSize };
        std::vector<float> l = std::vector<float> (kSize), r = std::vector<float> (kSize);
        std::atomic<double> sampleRate { 48000.0 };

        void push (const float* left, const float* right, int n)
        {
            const auto w = fifo.write (juce::jmin (n, fifo.getFreeSpace()));   // drop if the GUI is not reading
            auto copy = [&] (int start, int size, int offset)
            {
                if (size <= 0) return;
                std::copy (left  + offset, left  + offset + size, l.begin() + start);
                std::copy (right + offset, right + offset + size, r.begin() + start);
            };
            copy (w.startIndex1, w.blockSize1, 0);
            copy (w.startIndex2, w.blockSize2, w.blockSize1);
        }

        template <typename Fn> void pull (Fn&& fn)   // fn (const float* l, const float* r, int n)
        {
            const auto rd = fifo.read (fifo.getNumReady());
            if (rd.blockSize1 > 0) fn (l.data() + rd.startIndex1, r.data() + rd.startIndex1, rd.blockSize1);
            if (rd.blockSize2 > 0) fn (l.data() + rd.startIndex2, r.data() + rd.startIndex2, rd.blockSize2);
        }
    } scope;

    // Loudness (EBU R128) readings of the output, published every block
    struct LoudnessShared
    {
        std::atomic<float> momentary { -200.0f }, shortTerm { -200.0f }, integrated { -200.0f }, lra { 0.0f };
        std::atomic<float> momentaryMax { -200.0f }, shortTermMax { -200.0f }, truePeakMax { -200.0f }, truePeak3s { -200.0f };
        std::atomic<bool>  resetRequested { false };
    } loudness;

    juce::AudioProcessorValueTreeState apvts;

private:
    ppm::params::ChannelParamPtrs chanParams[2];
    ppm::params::CompParamPtrs compParams[2];
    std::atomic<float>* orderParam = nullptr;
    std::atomic<float>* linkParam  = nullptr;
    std::atomic<float>* tubeOnParam = nullptr;
    std::atomic<float>* driveParam  = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int osLog2 = 0;
    int maxBlock = 512;
    int currentProgram = 0;
    ppm::ChannelEngine engines[2];
    ppm::StereoVariMu comp;
    ppm::LoudnessMeter loudnessMeter;
    int    activeOrder = 0;        // order currently running
    double orderGain = 1.0, orderStep = 0.001;   // fade used to swap order without a click

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PassiveMasterEQProcessor)
};
