#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ppm;

PassiveMasterEQProcessor::PassiveMasterEQProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createLayout())
{
    for (int ch = 0; ch < 2; ++ch) { chanParams[ch].init (apvts, ch); compParams[ch].init (apvts, ch); }
    orderParam  = apvts.getRawParameterValue (params::compOrder);
    inputParam  = apvts.getRawParameterValue (params::inputGain);
    keyOnParam    = apvts.getRawParameterValue (params::keyOn);
    keySemisParam = apvts.getRawParameterValue (params::keySemis);
    keyCentsParam = apvts.getRawParameterValue (params::keyCents);
    linkParam   = apvts.getRawParameterValue (params::link);
    tubeOnParam = apvts.getRawParameterValue (params::tubeOn);
    driveParam  = apvts.getRawParameterValue (params::tubeDrive);
    maxOnParam   = apvts.getRawParameterValue (params::maxOn);
    maxGainParam = apvts.getRawParameterValue (params::maxGain);
    maxCeilParam = apvts.getRawParameterValue (params::maxCeiling);
    classAParams.init (apvts);
    pqParams.init (apvts);
    maximizer.prepare (48000.0);
    startTimerHz (10);
}

PassiveMasterEQProcessor::~PassiveMasterEQProcessor()
{
    stopTimer();
    cancelSolve.store (true);
    pool.removeAllJobs (true, 20000);
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
    maxBlock = juce::jmax (1, samplesPerBlock);
    oversampler->initProcessing ((size_t) maxBlock);
    maximizer.prepare (sampleRate);
    maximizer.setParams (maxOnParam->load() > 0.5f, maxGainParam->load(), maxCeilParam->load());
    maximizer.reset();
    verifyMeter.prepare (sampleRate);
    // the maximizer's 1.5 ms look-ahead is always in the path, so latency doesn't change when it's switched
    baseLatency = (osLog2 > 0 ? (int) std::lround (oversampler->getLatencyInSamples()) : 0) + maximizer.latency();
    keyShifter.prepare (sampleRate);
    keyActive = isKeyShiftOn();
    keyFadeIn = 0;
    keyFadeLen = juce::jmax (64, (int) (sampleRate * 0.01));
    updateLatency();

    const double internalRate = sampleRate * (1 << osLog2);
    for (int ch = 0; ch < 2; ++ch)
    {
        engines[ch].prepare (internalRate);
        engines[ch].setSettings (getChannelSettings (ch), tubeOnParam->load() > 0.5f, driveParam->load());
    }
    comp.setSettings (getCompSettings (0), getCompSettings (1), isLinked());
    comp.prepare (internalRate);
    classA.setSettings (classAParams.read());
    classA.prepare (internalRate);
    paraEq.setSettings (pqParams.read (isLinked()));
    paraEq.prepare (internalRate);
    activeOrder = getCompOrder();
    lastInputGain = juce::Decibels::decibelsToGain (inputParam->load());
    scope.sampleRate.store (sampleRate);
    loudnessMeter.prepare (sampleRate);
    orderGain = 1.0;
    orderStep = 1.0 / (0.005 * internalRate);     // 5 ms dip when the order is switched
}

void PassiveMasterEQProcessor::updateLatency()
{
    // the key shifter only adds its delay while it's switched on
    setLatencySamples (baseLatency + (keyActive ? keyShifter.latency() : 0));
}

CompSettings PassiveMasterEQProcessor::getCompSettings (int ch) const
{
    return compParams[(ch == 1 && isLinked()) ? 0 : ch].read();
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
    // INPUT gain: ramped across the block so moving it never clicks
    const float inGain = juce::Decibels::decibelsToGain (inputParam->load());
    for (int ch = 0; ch < numCh; ++ch)
        buffer.applyGainRamp (ch, 0, buffer.getNumSamples(), lastInputGain, inGain);
    lastInputGain = inGain;

    meters.numChannels.store (numCh);
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float pk = buffer.getMagnitude (ch, 0, buffer.getNumSamples());
        holdMax (meters.inPeak, pk);
        holdMax (meters.inLevel[ch], pk);
    }

    // KEY SHIFT: switching fades the old path out over this block and the new one in afterwards,
    // so turning it on/off never clicks (the reported latency changes with it)
    {
        const bool want = isKeyShiftOn();
        const int n = buffer.getNumSamples();
        float* l = buffer.getWritePointer (0);
        float* r = numCh > 1 ? buffer.getWritePointer (1) : l;
        if (keyActive)
        {
            keyShifter.setShift (getKeyShiftSemitones());
            keyShifter.setFormantPreserve (apvts.getRawParameterValue (params::keyFormant)->load() > 0.5f);
            keyShifter.process (l, r, n);
        }
        if (keyFadeIn > 0)
        {
            for (int i = 0; i < n && keyFadeIn > 0; ++i, --keyFadeIn)
            {
                const float g = 1.0f - (float) keyFadeIn / (float) keyFadeLen;
                l[i] *= g; if (r != l) r[i] *= g;
            }
        }
        if (want != keyActive)
        {
            for (int ch = 0; ch < numCh; ++ch) buffer.applyGainRamp (ch, 0, n, 1.0f, 0.0f);   // fade out the old path
            keyActive = want;
            if (keyActive) keyShifter.reset();
            keyFadeIn = keyFadeLen;
            updateLatency();
        }
    }

    // AUTO MASTER: record what goes into the EQ (after INPUT and KEY SHIFT), skipping silence
    if (autoState.load() == AutoState::Listening && capLen > 0)
    {
        const int n = buffer.getNumSamples();
        float pk = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
        const int pos = capPos.load();
        if (pk > 0.003f && pos < capLen)
        {
            const int take = juce::jmin (n, capLen - pos);
            std::copy (buffer.getReadPointer (0), buffer.getReadPointer (0) + take, capL.begin() + pos);
            const float* rs = buffer.getReadPointer (numCh > 1 ? 1 : 0);
            std::copy (rs, rs + take, capR.begin() + pos);
            capPos.store (pos + take);
        }
    }

    const bool   tubeOn = tubeOnParam->load() > 0.5f;
    const double drive  = driveParam->load();
    for (int ch = 0; ch < numCh; ++ch)
        engines[ch].setSettings (getChannelSettings (ch), tubeOn, drive);

    comp.setSettings (getCompSettings (0), getCompSettings (1), isLinked());
    classA.setSettings (classAParams.read());
    paraEq.setSettings (pqParams.read (isLinked()));
    const int wantOrder = getCompOrder();

    // Hosts may send more samples than announced in prepareToPlay: process in pieces
    juce::dsp::AudioBlock<float> whole (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) buffer.getNumSamples());
    for (size_t pos = 0; pos < whole.getNumSamples(); pos += (size_t) maxBlock)
    {
    auto block = whole.getSubBlock (pos, juce::jmin ((size_t) maxBlock, whole.getNumSamples() - pos));
    auto up = oversampler->processSamplesUp (block);

    float* d[2] = { up.getChannelPointer (0), numCh > 1 ? up.getChannelPointer (1) : nullptr };
    for (size_t i = 0; i < up.getNumSamples(); ++i)
    {
        double l = d[0][i];
        double r = d[1] != nullptr ? (double) d[1][i] : l;

        // parametric mastering EQ: first in the chain, in front of the passive EQ
        paraEq.process (l, r);
        if (d[1] == nullptr) r = l;

        if (activeOrder == 0)          // EQ > COMP
        {
            l = engines[0].process (l);
            if (d[1] != nullptr) r = engines[1].process (r); else r = l;
            comp.process (l, r);
        }
        else                           // COMP > EQ
        {
            comp.process (l, r);
            l = engines[0].process (l);
            if (d[1] != nullptr) r = engines[1].process (r);
        }

        // switching the order: fade out, swap, fade back in (avoids a click)
        if (wantOrder != activeOrder || orderGain < 1.0)
        {
            if (wantOrder != activeOrder)
            {
                orderGain -= orderStep;
                if (orderGain <= 0.0) { orderGain = 0.0; activeOrder = wantOrder; }
            }
            else orderGain = std::min (1.0, orderGain + orderStep);
            l *= orderGain; r *= orderGain;
        }

        // M/S CLASS-A compressor + soft clip (after the EQ and vari-mu, before the maximizer)
        if (d[1] != nullptr) classA.process (l, r);
        else { double rr = l; classA.process (l, rr); }

        d[0][i] = (float) l;
        if (d[1] != nullptr) d[1][i] = (float) r;
    }

    oversampler->processSamplesDown (block);
    }

    // MAXIMIZER: final look-ahead brickwall limiter
    maximizer.setParams (maxOnParam->load() > 0.5f, maxGainParam->load(), maxCeilParam->load());
    maximizer.process (buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr, buffer.getNumSamples());
    {
        const float g = (float) maximizer.takeGainReductionDb();
        float prev = maxGr.load();
        while (g > prev && ! maxGr.compare_exchange_weak (prev, g)) {}
    }

    // AUTO MASTER loudness check after the settings were applied
    if (autoState.load() == AutoState::Verifying)
    {
        if (verifyReset.exchange (false)) { verifyMeter.reset(); verifyCount.store (0); }
        float pk = 0.0f;
        for (int ch = 0; ch < numCh; ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        if (pk > 0.003f)
        {
            verifyMeter.process (buffer.getReadPointer (0), numCh > 1 ? buffer.getReadPointer (1) : nullptr, buffer.getNumSamples());
            verifyCount.store (verifyCount.load() + buffer.getNumSamples());
            verifyLufs.store ((float) verifyMeter.readout().integrated);
        }
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        meters.outRms[ch].store (buffer.getRMSLevel (ch, 0, buffer.getNumSamples()));
        holdMax (meters.outPeak[ch], buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
        meters.thd[ch].store ((float) engines[ch].takeDistortionRatio());
        meters.gr[ch].store ((float) comp.takeGainReductionDb (ch));
        meters.grA[ch].store ((float) classA.takeGainReductionDb (ch));
    }
    if (classA.takeClipActive()) meters.clipA.store (true);
    scope.push (buffer.getReadPointer (0), buffer.getReadPointer (numCh > 1 ? 1 : 0), buffer.getNumSamples());

    // loudness of the output
    if (loudness.resetRequested.exchange (false)) loudnessMeter.reset();
    loudnessMeter.process (buffer.getReadPointer (0), numCh > 1 ? buffer.getReadPointer (1) : nullptr, buffer.getNumSamples());
    const auto& lr = loudnessMeter.readout();
    loudness.momentary.store ((float) lr.momentary);       loudness.shortTerm.store ((float) lr.shortTerm);
    loudness.integrated.store ((float) lr.integrated);     loudness.lra.store ((float) lr.lra);
    loudness.momentaryMax.store ((float) lr.momentaryMax); loudness.shortTermMax.store ((float) lr.shortTermMax);
    loudness.truePeakMax.store ((float) lr.truePeakMax);   loudness.truePeak3s.store ((float) lr.truePeak3s);
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
    return juce::isPositiveAndBelow (index, (int) list.size()) ? juce::String::fromUTF8 (list[(size_t) index].name.c_str()) : juce::String();
}

void PassiveMasterEQProcessor::setCurrentProgram (int index)
{
    const auto& list = presets::all();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    currentProgram = index;
    const auto& pr = list[(size_t) index];

    applySettings (pr.ch, pr.comp, pr.order, pr.tubeOn, pr.driveDb);
    if (pr.full) { applyParametric (pr.pq); applyClassA (pr.ca); }    // engineer presets set every unit
    setParamPlain (params::maxOn, 0.0f);          // presets are EQ + compressor only: maximizer off
    setParamPlain (params::maxGain, 0.0f);
}

void PassiveMasterEQProcessor::setParamPlain (const juce::String& id, float plain)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
        prm->endChangeGesture();
    }
}

// Write one set of settings to both channels and link them
void PassiveMasterEQProcessor::applySettings (const ChannelSettings& chs, const CompSettings& c, int order, bool tubeOn, float driveDb)
{
    auto set = [this] (const juce::String& id, float plain) { setParamPlain (id, plain); };
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            const auto& bs = chs.band[b];
            set (params::bandId (ch, b, "freq"),  (float) bs.freqIndex);
            set (params::bandId (ch, b, "gain"),  (float) bs.gainStep);
            set (params::bandId (ch, b, "bw"),    (float) bs.bwStep);
            set (params::bandId (ch, b, "mode"),  (float) bs.mode);
            set (params::bandId (ch, b, "shelf"), bs.shelf ? 1.0f : 0.0f);
        }
        set (params::chanId (ch, "hp"),    (float) chs.hp);
        set (params::chanId (ch, "lp"),    (float) chs.lp);
        set (params::chanId (ch, "trim"),  (float) chs.trim);
        set (params::chanId (ch, "eq_in"), chs.eqIn ? 1.0f : 0.0f);
        set (params::chanId (ch, "c_in"),       c.compIn ? 1.0f : 0.0f);
        set (params::chanId (ch, "c_thresh"),   (float) c.threshold);
        set (params::chanId (ch, "c_attack"),   (float) c.attack);
        set (params::chanId (ch, "c_recovery"), (float) c.recovery);
        set (params::chanId (ch, "c_output"),   (float) c.output);
        set (params::chanId (ch, "c_mix"),      (float) c.mix);
        set (params::chanId (ch, "c_mode"),     (float) c.mode);
        set (params::chanId (ch, "c_schp"),     c.scHp ? 1.0f : 0.0f);
    }
    set (params::compOrder, (float) order);
    set (params::link, 1.0f);
    set (params::tubeOn, tubeOn ? 1.0f : 0.0f);
    set (params::tubeDrive, driveDb);
}

void PassiveMasterEQProcessor::applyEqPreset (int index)
{
    const auto& list = presets::all();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& pr = list[(size_t) index];
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            const auto& bs = pr.ch.band[b];
            setParamPlain (params::bandId (ch, b, "freq"),  (float) bs.freqIndex);
            setParamPlain (params::bandId (ch, b, "gain"),  (float) bs.gainStep);
            setParamPlain (params::bandId (ch, b, "bw"),    (float) bs.bwStep);
            setParamPlain (params::bandId (ch, b, "mode"),  (float) bs.mode);
            setParamPlain (params::bandId (ch, b, "shelf"), bs.shelf ? 1.0f : 0.0f);
        }
        setParamPlain (params::chanId (ch, "hp"),    (float) pr.ch.hp);
        setParamPlain (params::chanId (ch, "lp"),    (float) pr.ch.lp);
        setParamPlain (params::chanId (ch, "trim"),  (float) pr.ch.trim);
        setParamPlain (params::chanId (ch, "eq_in"), 1.0f);
    }
    setParamPlain (params::link, 1.0f);
    if (pr.full) applyParametric (pr.pq);        // engineer EQ presets use both EQs together
}

void PassiveMasterEQProcessor::applyVariMuPreset (int index)
{
    const auto& list = presets::all();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& c = list[(size_t) index].comp;
    for (int ch = 0; ch < 2; ++ch)
    {
        setParamPlain (params::chanId (ch, "c_in"),       c.compIn ? 1.0f : 0.0f);
        setParamPlain (params::chanId (ch, "c_thresh"),   (float) c.threshold);
        setParamPlain (params::chanId (ch, "c_attack"),   (float) c.attack);
        setParamPlain (params::chanId (ch, "c_recovery"), (float) c.recovery);
        setParamPlain (params::chanId (ch, "c_output"),   (float) c.output);
        setParamPlain (params::chanId (ch, "c_mix"),      (float) c.mix);
        setParamPlain (params::chanId (ch, "c_mode"),     (float) c.mode);
        setParamPlain (params::chanId (ch, "c_schp"),     c.scHp ? 1.0f : 0.0f);
    }
    setParamPlain (params::link, 1.0f);
}

void PassiveMasterEQProcessor::applyClassAPreset (int index)
{
    const auto& list = presets::all();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    applyClassA (list[(size_t) index].ca);
}

void PassiveMasterEQProcessor::applyParametric (const ppm::sontec::Settings& st)
{
    setParamPlain (params::pq::in, st.in ? 1.0f : 0.0f);
    for (int c = 0; c < 2; ++c)
    {
        const auto& ch = st.ch[c];
        for (int b = 0; b < 3; ++b)
        {
            setParamPlain (params::pq::band (c, b, "freq"),  (float) ch.band[b].freqIdx);
            setParamPlain (params::pq::band (c, b, "shape"), (float) ch.band[b].shape);
            setParamPlain (params::pq::band (c, b, "level"), (float) ch.band[b].level);
        }
        setParamPlain (params::pq::id (c, "hs"),  (float) ch.hiShelfLevel);
        setParamPlain (params::pq::id (c, "hsf"), ch.hiShelf12k ? 1.0f : 0.0f);
        setParamPlain (params::pq::id (c, "ls"),  (float) ch.loShelfLevel);
        setParamPlain (params::pq::id (c, "lsf"), ch.loShelf100 ? 1.0f : 0.0f);
        setParamPlain (params::pq::id (c, "man"), ch.in ? 1.0f : 0.0f);
    }
}

void PassiveMasterEQProcessor::applyClassA (const ppm::ClassASettings& st)
{
    namespace ca = params::ca;
    setParamPlain (ca::in, st.in ? 1.0f : 0.0f);
    setParamPlain (ca::mode, (float) st.mode);
    setParamPlain (ca::link, st.link ? 1.0f : 0.0f);
    setParamPlain (ca::clipOn, st.softClip ? 1.0f : 0.0f);
    setParamPlain (ca::clip, (float) st.clipDb);
    for (int c = 0; c < 2; ++c)
    {
        const auto& ch = st.ch[c];
        auto set = [&] (const char* what, double v) { setParamPlain (ca::chan (c, what), (float) v); };
        set ("thr", ch.thresholdDb); set ("att", ch.attackMs); set ("rel", ch.releaseMs); set ("ratio", ch.ratioPos);
        set ("gain", ch.gainDb); set ("mix", ch.mix * 100.0);
        set ("ff", ch.feedForward ? 1 : 0); set ("afa", ch.autoFastAttack ? 1 : 0); set ("afr", ch.autoFastRelease ? 1 : 0); set ("warm", ch.warm ? 1 : 0);
        set ("scon", ch.scOn ? 1 : 0); set ("scg", ch.scTilt); set ("scf", ch.scFreq);
        set ("eqon", ch.eqOn ? 1 : 0); set ("eqg", ch.eqTilt); set ("eqf", ch.eqFreq); set ("eqx", ch.eqX10 ? 1 : 0);
    }
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

// =============================================================================
//  AUTO MASTER
// =============================================================================
void PassiveMasterEQProcessor::autoListen()
{
    if (solving.load()) return;
    autoState.store (AutoState::Idle);              // stop the audio thread touching the buffers
    const double fs = juce::jmax (8000.0, getSampleRate() > 0 ? getSampleRate() : 48000.0);
    capRate = fs;
    capInputDb = inputParam->load();
    capLen  = (int) (fs * listenSeconds);
    capL.assign ((size_t) capLen, 0.0f);
    capR.assign ((size_t) capLen, 0.0f);
    capPos.store (0);
    {
        const juce::SpinLock::ScopedLockType sl (planLock);
        autoMessage = "LISTENING - play the song (the loudest part, e.g. a chorus, is best).";
    }
    autoState.store (AutoState::Listening);
}

void PassiveMasterEQProcessor::autoCancel()
{
    cancelSolve.store (true);
    if (autoState.load() == AutoState::Listening) { capLen = 0; capPos.store (0); }
    autoState.store (AutoState::Idle);
    const juce::SpinLock::ScopedLockType sl (planLock);
    autoMessage = "Stopped.";
}

void PassiveMasterEQProcessor::autoReapply()
{
    if (! autoHasRecording() || solving.load()) return;
    autoState.store (AutoState::Analysing);
    {
        const juce::SpinLock::ScopedLockType sl (planLock);
        autoMessage = "ANALYSING...";
    }
    const int genreIdx = (int) std::lround (apvts.getRawParameterValue (params::amGenre)->load());
    const double amount = apvts.getRawParameterValue (params::amAmount)->load() / 100.0;
    cancelSolve.store (false);
    solving.store (true);
    pool.addJob ([this, genreIdx, amount]
    {
        auto p = solvePlan (genreIdx, amount);
        {
            const juce::SpinLock::ScopedLockType sl (planLock);
            pendingPlan = p;
        }
        solveDone.store (! cancelSolve.load());
        solving.store (false);
    });
}

void PassiveMasterEQProcessor::autoUndo()
{
    if (! undoValid) return;
    if (solving.load()) cancelSolve.store (true);
    apvts.replaceState (undoState.createCopy());
    undoValid = false;
    autoState.store (AutoState::Idle);
    const juce::SpinLock::ScopedLockType sl (planLock);
    autoMessage = "UNDONE - your settings from before AUTO MASTER are back.";
}

void PassiveMasterEQProcessor::autoMasterTick()
{
    const auto st = autoState.load();
    // recording finished -> plan it
    if (st == AutoState::Listening && capLen > 0 && capPos.load() >= capLen)
        autoReapply();

    // plan ready -> apply it (message thread)
    if (solveDone.exchange (false))
    {
        AutoPlan p;
        { const juce::SpinLock::ScopedLockType sl (planLock); p = pendingPlan; }
        if (! p.ok)
        {
            autoState.store (AutoState::Failed);
            const juce::SpinLock::ScopedLockType sl (planLock);
            autoMessage = "Not enough music heard - press LISTEN and play the song (louder than -50 dBFS).";
            return;
        }
        if (! undoValid) { undoState = apvts.copyState(); undoValid = true; }
        applySettings (p.eq, p.comp, 0, p.tubeOn, (float) p.drive);
        setParamPlain (params::inputGain, (float) p.inputGainDb);
        setParamPlain (params::maxCeiling, (float) p.ceiling);
        setParamPlain (params::maxGain, (float) p.maxGain);
        setParamPlain (params::maxOn, 1.0f);
        {
            const juce::SpinLock::ScopedLockType sl (planLock);
            plan = p;
            autoMessage = "APPLIED - checking the loudness while the song plays...";
        }
        verifyRound = 0;
        verifyLufs.store (-200.0f);
        verifyCount.store (0);
        verifyReset.store (true);
        autoState.store (AutoState::Verifying);
        return;
    }

    // loudness check: nudge the maximizer so the real playback lands on the target
    if (st == AutoState::Verifying && ! verifyReset.load() && capRate > 0
        && verifyCount.load() >= (int) (verifySeconds * capRate))
    {
        const double target = getAutoPlan().targetLufs;
        const double measured = verifyLufs.load();
        const double err = target - measured;
        if (measured > -70.0 && std::abs (err) > 0.3 && verifyRound < 2)
        {
            const float g = juce::jlimit (-12.0f, 18.0f, maxGainParam->load() + (float) err);
            setParamPlain (params::maxGain, g);
            ++verifyRound;
            verifyReset.store (true);
            return;
        }
        autoState.store (AutoState::Done);
        const juce::SpinLock::ScopedLockType sl (planLock);
        plan.maxGain = maxGainParam->load();
        autoMessage = "DONE: " + juce::String (measured, 1) + " LUFS (target " + juce::String (target, 1) + ")"
                    + (plan.limiterGrDb > 5.0 ? juce::String ("  -  heavy limiting: this mix has big peaks; try less GAIN or a quieter genre")
                                              : juce::String ("  -  fine-tune by ear, or UNDO to compare"));
    }
}

// Runs on the background thread: everything here works on the recording only.
PassiveMasterEQProcessor::AutoPlan PassiveMasterEQProcessor::solvePlan (int genreIdx, double amount) const
{
    namespace am = automaster;
    AutoPlan p;
    p.genre = genreIdx; p.amount = amount;
    const auto& t = am::genre (genreIdx);
    const int n = capLen;
    const double fs = capRate;
    if (n <= 0) return p;

    // 1. tonal balance -> EQ
    const auto spec = am::measureSpectrum (capL.data(), capR.data(), n, fs);
    if (! spec.ok) return p;
    p.corr = am::computeCorrection (spec, genreIdx, amount);
    p.eq = am::fitEq (p.corr, t.hp, t.lp);
    {
        ChannelSettings eqOnly = p.eq; eqOnly.hp = 0; eqOnly.lp = 0;
        for (int b = 0; b < am::kOct; ++b) p.fitted[b] = channelResponseDb (eqOnly, am::kOctHz[b]);
    }
    p.eqText = am::describeEq (p.eq);
    p.tubeOn = t.tubeOn; p.drive = t.drive; p.ceiling = t.ceiling; p.targetLufs = t.lufs;

    auto lufsOf = [fs] (const std::vector<float>& l, const std::vector<float>& r)
    {
        LoudnessMeter m; m.prepare (fs); m.reset();
        m.process (l.data(), r.data(), (int) l.size());
        return m.readout().integrated;
    };
    std::vector<float> a (capL.begin(), capL.begin() + n), b (capR.begin(), capR.begin() + n);
    p.inputLufs = lufsOf (a, b);
    if (cancelSolve.load() || p.inputLufs < -69.0) return p;

    // 2. gain staging: bring the mix to about -16 LUFS with the INPUT knob, so the tubes,
    //    compressor and maximizer always work at a sensible level
    {
        const double pre = juce::jlimit (-24.0 - capInputDb, 24.0 - capInputDb, -16.0 - p.inputLufs);
        p.inputGainDb = std::round ((capInputDb + pre) * 2.0) / 2.0;          // the knob has 0.5 dB steps
        const float g = (float) std::pow (10.0, (p.inputGainDb - capInputDb) / 20.0);
        for (auto& v : a) v *= g;
        for (auto& v : b) v *= g;
    }

    // 3. render the EQ + tubes exactly like the real path (oversampled)
    {
        const int os = fs < 88000.0 ? 2 : (fs < 176000.0 ? 1 : 0);
        const int blk = 1024;
        juce::dsp::Oversampling<float> ov (2, (size_t) os, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        ov.initProcessing ((size_t) blk);
        ChannelEngine eng[2];
        for (auto& e : eng) { e.prepare (fs * (1 << os)); e.setSettings (p.eq, p.tubeOn, p.drive); }
        float* ptr[2] = { a.data(), b.data() };
        for (int pos = 0; pos < n; pos += blk)
        {
            const int len = juce::jmin (blk, n - pos);
            float* chans[2] = { ptr[0] + pos, ptr[1] + pos };
            juce::dsp::AudioBlock<float> block (chans, 2, (size_t) len);
            auto up = ov.processSamplesUp (block);
            for (int c = 0; c < 2; ++c)
            {
                float* d = up.getChannelPointer ((size_t) c);
                for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = (float) eng[c].process (d[i]);
            }
            ov.processSamplesDown (block);
        }
    }
    if (cancelSolve.load()) return p;

    // 4. compressor: find the threshold that gives the genre's amount of gain reduction on the loud parts
    CompSettings cs;
    cs.compIn = true; cs.attack = t.attack; cs.recovery = t.recovery; cs.mix = t.mix; cs.mode = t.mode; cs.scHp = t.scHp; cs.output = 0;
    auto runComp = [&] (const CompSettings& c, std::vector<float>* outL, std::vector<float>* outR)
    {
        StereoVariMu vm;
        vm.setSettings (c, c, true);
        vm.prepare (fs);
        const int blk = juce::jmax (1, (int) (0.1 * fs));
        std::vector<double> grs, pw;
        double gr = 0.0, e = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double l = a[(size_t) i], r = b[(size_t) i];
            e += l * l + r * r;
            vm.process (l, r);
            if (outL != nullptr) { (*outL)[(size_t) i] = (float) l; (*outR)[(size_t) i] = (float) r; }
            if ((i + 1) % blk == 0) { gr = vm.takeGainReductionDb (0); grs.push_back (gr); pw.push_back (e); e = 0.0; }
        }
        double mean = 0; for (double v : pw) mean += v; mean /= juce::jmax ((size_t) 1, pw.size());
        double sum = 0; int cnt = 0;
        for (size_t k = 5; k < grs.size(); ++k) if (pw[k] >= mean * 0.3) { sum += grs[k]; ++cnt; }   // loud parts only
        return cnt > 0 ? sum / cnt : 0.0;
    };
    int lo = 0, hi = kNumThreshSteps - 1;
    while (lo < hi && ! cancelSolve.load())                    // smallest threshold step reaching the target GR
    {
        const int mid = (lo + hi) / 2;
        cs.threshold = mid;
        if (runComp (cs, nullptr, nullptr) >= t.grDb) hi = mid; else lo = mid + 1;
    }
    cs.threshold = lo;
    p.compGrDb = runComp (cs, nullptr, nullptr);
    cs.output = juce::jlimit (0, kNumOutputSteps - 1, (int) std::lround (p.compGrDb / 0.5));   // make-up = average GR
    p.comp = cs;
    std::vector<float> cl ((size_t) n), cr ((size_t) n);
    runComp (cs, &cl, &cr);
    if (cancelSolve.load()) return p;

    // 5. maximizer: gain that lands the integrated loudness on the genre target
    auto limited = [&] (double gainDb, double* grOut)
    {
        Maximizer mx; mx.prepare (fs); mx.setParams (true, gainDb, p.ceiling); mx.reset();
        std::vector<float> l = cl, r = cr;
        const int blk = juce::jmax (1, (int) (0.1 * fs));
        double grSum = 0; int grN = 0;
        for (int pos = 0; pos < n; pos += blk)
        {
            const int len = juce::jmin (blk, n - pos);
            mx.process (l.data() + pos, r.data() + pos, len);
            grSum += mx.takeGainReductionDb(); ++grN;
        }
        if (grOut != nullptr) *grOut = grN > 0 ? grSum / grN : 0.0;
        return lufsOf (l, r);
    };
    double gLo = -12.0, gHi = 18.0;
    if (limited (gHi, nullptr) < p.targetLufs) gLo = gHi;      // can't get there: use the most it allows
    else if (limited (gLo, nullptr) > p.targetLufs) gHi = gLo; // already louder than the target even turned down
    for (int it = 0; it < 14 && gHi - gLo > 0.05 && ! cancelSolve.load(); ++it)
    {
        const double mid = 0.5 * (gLo + gHi);
        if (limited (mid, nullptr) < p.targetLufs) gLo = mid; else gHi = mid;
    }
    p.maxGain = std::round (0.5 * (gLo + gHi) * 10.0) / 10.0;
    p.predictedLufs = limited (p.maxGain, &p.limiterGrDb);
    p.ok = true;
    return p;
}

juce::AudioProcessorEditor* PassiveMasterEQProcessor::createEditor() { return new PassiveMasterEQEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PassiveMasterEQProcessor(); }
