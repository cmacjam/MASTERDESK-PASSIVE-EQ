#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "Presets.h"
#include "dsp/TubeStage.h"
#include "dsp/VariMu.h"
#include "dsp/Loudness.h"
#include "dsp/KeyShift.h"
#include "dsp/Maximizer.h"
#include "dsp/AutoMaster.h"

class PassiveMasterEQProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PassiveMasterEQProcessor();
    ~PassiveMasterEQProcessor() override;

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
    bool   isKeyShiftOn() const      { return keyOnParam->load() > 0.5f; }
    double getKeyShiftSemitones() const { return (double) std::lround (keySemisParam->load()) + std::lround (keyCentsParam->load()) / 100.0; }
    bool isLinked() const { return linkParam->load() > 0.5f; }
    ppm::ClassASettings getClassASettings() const { return classAParams.read(); }
    ppm::sontec::Settings getParametricSettings() const { return pqParams.read (isLinked()); }
    void copyChannel (int fromCh, int toCh);
    int  getOversamplingFactor() const { return 1 << osLog2; }

    // Meters (written by the audio thread, read by the GUI) ---------------
    struct Meters
    {
        std::atomic<float> outRms[2]  { {0.0f}, {0.0f} };   // last block RMS, linear
        std::atomic<float> outPeak[2] { {0.0f}, {0.0f} };   // peak hold, GUI resets
        std::atomic<float> thd[2]     { {0.0f}, {0.0f} };   // tube harmonics ratio
        std::atomic<float> inPeak     { 0.0f };             // input peak hold, GUI resets
        std::atomic<float> inLevel[2] { {0.0f}, {0.0f} };   // input peak per channel (after INPUT gain), GUI resets
        std::atomic<float> gr[2]      { {0.0f}, {0.0f} };   // compressor gain reduction, dB
        std::atomic<float> grA[2]     { {0.0f}, {0.0f} };   // class-A compressor gain reduction (ch1, ch2), dB
        std::atomic<bool>  clipA      { false };            // class-A soft clip working (GUI resets)
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

    // AUTO MASTER ---------------------------------------------------------
    enum class AutoState { Idle, Listening, Analysing, Verifying, Done, Failed };
    struct AutoPlan
    {
        bool   ok = false;
        int    genre = 0;
        double amount = 0.6;
        ppm::automaster::Correction corr;
        double fitted[ppm::automaster::kOct] {};    // EQ response at the octave centres, dB
        ppm::ChannelSettings eq;
        ppm::CompSettings comp;
        bool   tubeOn = true;
        double drive = 0.0, maxGain = 0.0, ceiling = -1.0;
        double inputGainDb = 0.0;                     // INPUT knob setting chosen (gain staging)
        double inputLufs = -70.0, predictedLufs = -70.0, targetLufs = -10.0, compGrDb = 0.0, limiterGrDb = 0.0;
        juce::String eqText;
    };
    void autoListen();                      // start recording the song (message thread)
    void autoCancel();
    void autoReapply();                     // plan again from the recording with the current genre / AMOUNT
    void autoUndo();                        // back to the settings from before AUTO MASTER
    bool autoCanUndo() const               { return undoValid; }
    bool autoHasRecording() const          { return capLen > 0 && capPos.load() >= capLen; }
    AutoState getAutoState() const         { return autoState.load(); }
    double autoListenProgress() const      { return capLen > 0 ? juce::jlimit (0.0, 1.0, capPos.load() / (double) capLen) : 0.0; }
    double autoListenSecondsDone() const   { return capRate > 0 ? capPos.load() / capRate : 0.0; }
    AutoPlan getAutoPlan() const           { const juce::SpinLock::ScopedLockType sl (planLock); return plan; }
    juce::String getAutoMessage() const    { const juce::SpinLock::ScopedLockType sl (planLock); return autoMessage; }
    double getVerifyLufs() const           { return verifyLufs.load(); }
    void autoMasterTick();                  // runs 10x a second on the message thread (public for tests)
    bool autoBusy() const                  { return solving.load(); }
    double listenSeconds = 30.0;            // length of the recording (tests shorten it)
    double verifySeconds = 8.0;             // how long the loudness check listens after applying
    float takeMaximizerGr()                { return maxGr.exchange (0.0f); }

private:
    void timerCallback() override { autoMasterTick(); }
    void applySettings (const ppm::ChannelSettings&, const ppm::CompSettings&, int order, bool tubeOn, float driveDb);
    void setParamPlain (const juce::String& id, float plain);
    AutoPlan solvePlan (int genre, double amount) const;
    std::vector<float> capL, capR;
    int capLen = 0;
    double capRate = 0.0, capInputDb = 0.0;      // INPUT knob position while recording
    std::atomic<int> capPos { 0 };
    std::atomic<AutoState> autoState { AutoState::Idle };
    std::atomic<bool> solving { false }, solveDone { false }, cancelSolve { false };
    std::atomic<bool> verifyReset { false };
    std::atomic<float> verifyLufs { -200.0f }, maxGr { 0.0f };
    std::atomic<int> verifyCount { 0 };
    int verifyRound = 0;
    AutoPlan plan, pendingPlan;
    juce::String autoMessage;
    juce::SpinLock planLock;
    juce::ValueTree undoState;
    bool undoValid = false;
    juce::ThreadPool pool { 1 };
    ppm::Maximizer maximizer;
    ppm::LoudnessMeter verifyMeter;
    std::atomic<float>* maxOnParam = nullptr;
    std::atomic<float>* maxGainParam = nullptr;
    std::atomic<float>* maxCeilParam = nullptr;

    ppm::params::ChannelParamPtrs chanParams[2];
    ppm::params::CompParamPtrs compParams[2];
    std::atomic<float>* orderParam = nullptr;
    std::atomic<float>* inputParam = nullptr;
    std::atomic<float>* keyOnParam = nullptr;
    std::atomic<float>* keySemisParam = nullptr;
    std::atomic<float>* keyCentsParam = nullptr;
    ppm::KeyShifter keyShifter;
    bool keyActive = false;            // what the audio path is doing right now
    int  keyFadeIn = 0, keyFadeLen = 480, baseLatency = 0;
    void updateLatency();
    float lastInputGain = 1.0f;
    std::atomic<float>* linkParam  = nullptr;
    std::atomic<float>* tubeOnParam = nullptr;
    std::atomic<float>* driveParam  = nullptr;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int osLog2 = 0;
    int maxBlock = 512;
    int currentProgram = 0;
    ppm::ChannelEngine engines[2];
    ppm::StereoVariMu comp;
    ppm::ClassACompressor classA;
    ppm::sontec::SontecEQ paraEq;
    ppm::params::PqParamPtrs pqParams;
    ppm::params::ClassAParamPtrs classAParams;
    ppm::LoudnessMeter loudnessMeter;
    int    activeOrder = 0;        // order currently running
    double orderGain = 1.0, orderStep = 0.001;   // fade used to swap order without a click

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PassiveMasterEQProcessor)
};
