#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ppm;

PassiveMasterEQProcessor::PassiveMasterEQProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createLayout())
{
    for (int ch = 0; ch < 2; ++ch) chanParams[ch].init (apvts, ch);
    linkParam   = apvts.getRawParameterValue (params::link);
    tubeOnParam = apvts.getRawParameterValue (params::tubeOn);
    driveParam  = apvts.getRawParameterValue (params::tubeDrive);
}

bool PassiveMasterEQProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainInputChannelSet() == out;
}

void PassiveMasterEQProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Run the passive network and tube stage at >= 176.4 kHz so the 27 kHz
    // shelf / 52 kHz filter are correct and tube harmonics don't alias.
    osLog2 = sampleRate < 88000.0 ? 2 : (sampleRate < 176000.0 ? 1 : 0);

    const int numCh = juce::jmax (1, getTotalNumOutputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numCh, (size_t) osLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples (osLog2 > 0 ? (int) std::lround (oversampler->getLatencyInSamples()) : 0);

    const double internalRate = sampleRate * (1 << osLog2);
    for (int ch = 0; ch < 2; ++ch)
    {
        engines[ch].prepare (internalRate);
        engines[ch].setSettings (getChannelSettings (ch), tubeOnParam->load() > 0.5f, driveParam->load());
    }
}

ChannelSettings PassiveMasterEQProcessor::getChannelSettings (int ch) const
{
    return chanParams[(ch == 1 && isLinked()) ? 0 : ch].read();
}

void PassiveMasterEQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), 2, getTotalNumOutputChannels());
    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0 || oversampler == nullptr) return;

    auto holdMax = [] (std::atomic<float>& a, float v)
    {
        float prev = a.load();
        while (v > prev && ! a.compare_exchange_weak (prev, v)) {}
    };
    meters.numChannels.store (numCh);
    for (int ch = 0; ch < numCh; ++ch)
        holdMax (meters.inPeak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

    const bool   tubeOn = tubeOnParam->load() > 0.5f;
    const double drive  = driveParam->load();
    for (int ch = 0; ch < numCh; ++ch)
        engines[ch].setSettings (getChannelSettings (ch), tubeOn, drive);

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) buffer.getNumSamples());
    auto up = oversampler->processSamplesUp (block);

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = up.getChannelPointer ((size_t) ch);
        auto& e = engines[ch];
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            d[i] = (float) e.process ((double) d[i]);
    }

    oversampler->processSamplesDown (block);

    for (int ch = 0; ch < numCh; ++ch)
    {
        meters.outRms[ch].store (buffer.getRMSLevel (ch, 0, buffer.getNumSamples()));
        holdMax (meters.outPeak[ch], buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        meters.thd[ch].store ((float) engines[ch].takeDistortionRatio());
    }
}

void PassiveMasterEQProcessor::copyChannel (int fromCh, int toCh)
{
    for (auto& suffix : params::channelParamSuffixes())
    {
        auto* src = apvts.getParameter (params::chanPrefix (fromCh) + suffix);
        auto* dst = apvts.getParameter (params::chanPrefix (toCh)   + suffix);
        if (src == nullptr || dst == nullptr) continue;
        dst->beginChangeGesture();
        dst->setValueNotifyingHost (src->getValue());
        dst->endChangeGesture();
    }
}

const juce::String PassiveMasterEQProcessor::getProgramName (int index)
{
    const auto& list = presets::all();
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String (list[(size_t) index].name) : juce::String();
}

void PassiveMasterEQProcessor::setCurrentProgram (int index)
{
    const auto& list = presets::all();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentProgram = index;
    const auto& pr = list[(size_t) index];

    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
            prm->endChangeGesture();
        }
    };

    // presets are stereo: write the same settings to both channels and link them
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            const auto& bs = pr.ch.band[b];
            set (params::bandId (ch, b, "freq"),  (float) bs.freqIndex);
            set (params::bandId (ch, b, "gain"),  (float) bs.gainStep);
            set (params::bandId (ch, b, "bw"),    (float) bs.bwStep);
            set (params::bandId (ch, b, "mode"),  (float) bs.mode);
            set (params::bandId (ch, b, "shelf"), bs.shelf ? 1.0f : 0.0f);
        }
        set (params::chanId (ch, "hp"),    (float) pr.ch.hp);
        set (params::chanId (ch, "lp"),    (float) pr.ch.lp);
        set (params::chanId (ch, "trim"),  (float) pr.ch.trim);
        set (params::chanId (ch, "eq_in"), pr.ch.eqIn ? 1.0f : 0.0f);
    }
    set (params::link, 1.0f);
    set (params::tubeOn, pr.tubeOn ? 1.0f : 0.0f);
    set (params::tubeDrive, pr.driveDb);
}

void PassiveMasterEQProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PassiveMasterEQProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentProgram = (int) apvts.state.getProperty ("program", 0);
        }
}

juce::AudioProcessorEditor* PassiveMasterEQProcessor::createEditor() { return new PassiveMasterEQEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PassiveMasterEQProcessor(); }
