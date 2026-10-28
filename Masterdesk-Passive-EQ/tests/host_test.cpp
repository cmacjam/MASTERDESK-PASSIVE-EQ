// Integration test: runs the real AudioProcessor (oversampling, params,
// smoothing) and renders the editor to a PNG. Built only when PPM_BUILD_TESTS=ON.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

static void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plainValue)
{
    auto* p = s.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

// Feeds a sine (left amplitude aL, right aR) for `seconds`; returns the output/input gain in dB per channel.
static std::pair<double, double> measureStereo (PassiveMasterEQProcessor& proc, double sr, double freq,
                                                double aL, double aR, double seconds = 1.0)
{
    const int block = 512, total = (int) (sr * seconds);
    juce::AudioBuffer<float> buf (2, block);
    double acc[2] {}; int count = 0; double phase = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const double v = std::sin (phase); phase += 2 * juce::MathConstants<double>::pi * freq / sr;
            buf.setSample (0, i, (float) (aL * v)); buf.setSample (1, i, (float) (aR * v));
        }
        juce::MidiBuffer m;
        proc.processBlock (buf, m);
        if (pos > total / 2)
        {
            for (int i = 0; i < block; ++i)
                for (int c = 0; c < 2; ++c) acc[c] += buf.getSample (c, i) * buf.getSample (c, i);
            count += block;
        }
    }
    auto g = [&] (int c, double a) { return 20.0 * std::log10 (std::sqrt (acc[c] / count) / (a / std::sqrt (2.0))); };
    return { g (0, aL), g (1, aR) };
}

static double measureDb (PassiveMasterEQProcessor& proc, double sr, double freq)
{
    return measureStereo (proc, sr, freq, 0.1, 0.1).first;
}

// A music-like test signal (kick, bass, chord, hats, hiss), fed through the real
// processor one GUI frame at a time while the editor animates its displays.
static void playMusic (PassiveMasterEQProcessor& proc, PassiveMasterEQEditor& ed, double sr, int frames)
{
    static double t = 0.0;
    static juce::Random rnd (42);
    const int perFrame = (int) (sr / 30.0);
    juce::AudioBuffer<float> buf (2, perFrame);
    for (int f = 0; f < frames; ++f)
    {
        for (int i = 0; i < perFrame; ++i, t += 1.0 / sr)
        {
            const double beat = std::fmod (t, 0.5);
            const double kick = std::exp (-beat * 18.0) * std::sin (2 * juce::MathConstants<double>::pi * (50.0 + 90.0 * std::exp (-beat * 40.0)) * beat);
            const double bass = 0.35 * std::sin (2 * juce::MathConstants<double>::pi * 110.0 * t);
            double chord = 0;
            for (double fr : { 261.6, 329.6, 392.0, 523.3 }) chord += 0.09 * std::sin (2 * juce::MathConstants<double>::pi * fr * t);
            const double hatEnv = std::exp (-std::fmod (t, 0.125) * 60.0);
            const double noise = rnd.nextFloat() * 2.0 - 1.0, noise2 = rnd.nextFloat() * 2.0 - 1.0;
            double chordR = 0;
            for (double fr : { 261.6, 329.6, 392.0, 523.3 }) chordR += 0.09 * std::sin (2 * juce::MathConstants<double>::pi * fr * (t - 0.0007));
            // centre: kick + bass; wide: chord (slightly delayed on R), hats and ambience differ per side
            const double l = 0.45 * (0.8 * kick + bass + chord  + 0.30 * hatEnv * noise  + 0.12 * noise);
            const double r = 0.45 * (0.8 * kick + bass + chordR + 0.30 * hatEnv * noise2 + 0.12 * noise2);
            buf.setSample (0, i, (float) l); buf.setSample (1, i, (float) r);
        }
        juce::MidiBuffer m;
        proc.processBlock (buf, m);
        ed.animate (1);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;

    for (double sr : { 44100.0, 96000.0 })
    {
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        // LOW MID: 270 Hz bell, gain 12, bw 10, boost; HIGH: 12 kHz shelf boost; HP 30 Hz
        setParam (s, "L_b2_freq", 3); setParam (s, "L_b2_gain", 12); setParam (s, "L_b2_bw", 10);
        setParam (s, "L_b4_freq", 8); setParam (s, "L_b4_gain", 9);  setParam (s, "L_b4_shelf", 1);
        setParam (s, "L_b3_freq", 6); setParam (s, "L_b3_gain", 6);  setParam (s, "L_b3_mode", 2); // cut 2.2k
        setParam (s, "L_hp", 4);
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);

        const auto cs = proc.getChannelSettings (0);
        std::printf ("fs %.0f (internal %dx, latency %d):\n", sr, proc.getOversamplingFactor(), proc.getLatencySamples());
        double worst = 0;
        for (double f : { 25.0, 60.0, 150.0, 270.0, 600.0, 2200.0, 5000.0, 12000.0, 18000.0 })
        {
            const double got = measureDb (proc, sr, f), want = ppm::channelResponseDb (cs, f);
            worst = std::max (worst, std::abs (got - want));
            std::printf ("  %6.0f Hz  plugin %+6.2f dB   model %+6.2f dB\n", f, got, want);
        }
        std::printf ("  worst |diff| = %.2f dB\n", worst);
        if (worst > 0.5) ++failures;

        // stress: full-scale noise with tube on, everything maxed — must stay finite
        setParam (s, ppm::params::tubeOn, 1.0f); setParam (s, ppm::params::tubeDrive, 18.0f);
        for (int b = 1; b <= 4; ++b) { setParam (s, "L_b" + juce::String (b) + "_gain", 15); setParam (s, "L_b" + juce::String (b) + "_bw", 15); }
        juce::Random rnd (1);
        juce::AudioBuffer<float> buf (2, 512); float peak = 0; bool finite = true;
        for (int k = 0; k < 400; ++k)
        {
            if (k % 20 == 0) // flip modes/shapes while running
                for (int b = 1; b <= 4; ++b)
                {
                    setParam (s, "L_b" + juce::String (b) + "_mode", (float) rnd.nextInt (3));
                    setParam (s, "L_b" + juce::String (b) + "_shelf", (float) rnd.nextInt (2));
                    setParam (s, "L_b" + juce::String (b) + "_freq", (float) rnd.nextInt (11));
                    setParam (s, "L_lp", (float) rnd.nextInt (6));
                }
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, rnd.nextFloat() * 2 - 1);
            juce::MidiBuffer m; proc.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float v = buf.getSample (c, i); finite &= std::isfinite (v); peak = std::max (peak, std::abs (v)); }
        }
        std::printf ("  stress: finite=%s peak=%.2f\n\n", finite ? "yes" : "NO", peak);
        if (! finite) ++failures;

        if (sr == 44100.0)
        {
            setParam (s, ppm::params::link, 0.0f);
            setParam (s, "R_b1_freq", 2); setParam (s, "R_b1_gain", 10); setParam (s, "R_b1_shelf", 1); setParam (s, "R_b1_bw", 15);
            setParam (s, "R_b4_freq", 9); setParam (s, "R_b4_gain", 8); setParam (s, "R_b4_mode", 0);
            setParam (s, "L_b1_mode", 0); setParam (s, "L_b1_gain", 7); setParam (s, "L_b1_bw", 4); setParam (s, "L_b1_shelf", 0);
            setParam (s, "L_b2_mode", 2); setParam (s, "L_b3_mode", 1); setParam (s, "L_b4_mode", 0); setParam (s, "L_b4_gain", 6); setParam (s, "L_b4_shelf", 1);
            setParam (s, "L_b4_freq", 9); setParam (s, "L_b2_gain", 5); setParam (s, "L_b2_bw", 12); setParam (s, "L_b2_freq", 5); setParam (s, "L_b2_shelf", 0);
            setParam (s, ppm::params::tubeDrive, 3.0f);
            // screenshots go to $PPM_SNAPSHOT_DIR (or the temp folder)
            const auto envDir = juce::SystemStats::getEnvironmentVariable ("PPM_SNAPSHOT_DIR", {});
            const juce::File dir = envDir.isNotEmpty() ? juce::File (envDir) : juce::File::getSpecialLocation (juce::File::tempDirectory);
            dir.createDirectory();
            auto snap = [&] (juce::Component& c, const juce::String& name)
            {
                auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
                auto f = dir.getChildFile (name); f.deleteFile();
                juce::FileOutputStream fos (f);
                juce::PNGImageFormat().writeImageToStream (img, fos);
                std::printf ("snapshot %dx%d -> %s\n", img.getWidth(), img.getHeight(), f.getFullPathName().toRawUTF8());
            };
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
                setParam (s, "L_c_thresh", 10.0f); setParam (s, "R_c_thresh", 10.0f);
                setParam (s, ppm::params::tubeDrive, 3.0f);
                for (int b = 1; b <= 4; ++b) { setParam (s, "L_b" + juce::String (b) + "_gain", 4); setParam (s, "R_b" + juce::String (b) + "_gain", 4); }
                proc.loudness.resetRequested.store (true);          // clear the stress test's readings (like pressing RESET)
                playMusic (proc, *e, sr, 150);
                snap (*ed, "editor.png");
                ed->setSize (PassiveMasterEQEditor::designWidth(), PassiveMasterEQEditor::designHeight());
                snap (*ed, "editor_100.png");
            }
            setParam (s, ppm::params::link, 1.0f);
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
                playMusic (proc, *e, sr, 45);
                snap (*ed, "editor_linked.png");
                e->showHelp();
                snap (*ed, "editor_help.png");
            }
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
                setParam (s, ppm::params::inputGain, 4.5f);
                ed->setSize (1900, 1000);                        // a wide window
                playMusic (proc, *e, sr, 60);
                snap (*ed, "editor_wide.png");
                e->toggleCompact();                             // MINI view
                playMusic (proc, *e, sr, 30);
                snap (*ed, "editor_mini.png");
                e->toggleCompact();
                setParam (s, ppm::params::inputGain, 0.0f);
                // edge handles: pull the right edge out 200 px, then the left edge in 150 px
                // (the plug-in never grows past the screen, so allow for small screens such as CI machines)
                const double aspect = (double) PassiveMasterEQEditor::designWidth() / PassiveMasterEQEditor::designHeight();
                int maxW = 100000;
                if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (ed->getScreenBounds()))
                    maxW = (int) std::floor (std::min ((double) d->userArea.getWidth(), d->userArea.getHeight() * aspect));
                ed->setSize (600, (int) std::lround (600 / aspect));
                const int w0 = ed->getWidth();
                const int want1 = std::min (w0 + 200, maxW);
                e->testEdgeDrag (200, true);
                const int w1 = ed->getWidth(), h1 = ed->getHeight();
                const int want2 = std::max (w1 - 150, (int) std::lround (PassiveMasterEQEditor::designWidth() * 0.45));
                e->testEdgeDrag (150, false);
                const int w2 = ed->getWidth(), h2 = ed->getHeight();
                std::printf ("  edge handles: %d -> pull right +200 -> %dx%d -> push left in 150 -> %dx%d (shape %.3f / %.3f, want %.3f)\n",
                             w0, w1, h1, w2, h2, (double) w1 / h1, (double) w2 / h2, aspect);
                std::printf ("    (screen allows up to %d px wide; expected %d then %d)\n", maxW, want1, want2);
                if (std::abs (w1 - want1) > 2 || std::abs (w2 - want2) > 2
                    || std::abs ((double) w1 / h1 - aspect) > 0.01 || std::abs ((double) w2 / h2 - aspect) > 0.01) ++failures;
                // (help snapshot taken above)
            }
        }
    }
    // ---- presets: every factory preset loads, is finite, and stays within sane gain
    {
        PassiveMasterEQProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::printf ("presets:\n");
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            proc.setCurrentProgram (i);
            const auto cs = proc.getCompSettings (0);
            setParam (proc.apvts, "L_c_in", 0.0f); setParam (proc.apvts, "R_c_in", 0.0f);   // EQ-only check
            const auto c = proc.getChannelSettings (0);
            double lo = 1e9, hi = -1e9;
            for (double f = 20; f <= 20000; f *= 1.05) { const double d = ppm::channelResponseDb (c, f); lo = std::min (lo, d); hi = std::max (hi, d); }
            const double at1k = measureDb (proc, 48000.0, 1000.0) - ppm::channelResponseDb (c, 1000.0);
            std::printf ("  %-32s comp %s thr %+5.1f  ", proc.getProgramName (i).toRawUTF8(), cs.compIn ? "in " : "out", ppm::compThresholdDb (cs.threshold));
            std::printf ("EQ %+6.1f .. %+5.1f dB  (vs model @1k %+.2f)\n",
                         lo, hi, at1k);
            if (std::abs (at1k) > 0.6 || hi > 14.0) ++failures;     // at1k includes tube colour on presets with drive
        }
        // state round-trip keeps the program and parameters
        proc.setCurrentProgram (3);
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        PassiveMasterEQProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = other.getCurrentProgram() == 3
                       && other.getChannelSettings (0).band[1].mode == ppm::Cut
                       && other.getChannelSettings (0).hp == 4;
        std::printf ("  state round-trip: %s\n", same ? "ok" : "MISMATCH");
        if (! same) ++failures;
    }

    // ---- spectrum analyzer reads a known tone correctly -------------------------
    {
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_in", 0.0f); setParam (s, "R_c_in", 0.0f);
        proc.setPlayConfigDetails (2, 2, 48000.0, 1600);
        proc.prepareToPlay (48000.0, 1600);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
        std::printf ("spectrum analyzer (1 kHz sine at -6 dBFS peak):\n");
        juce::AudioBuffer<float> buf (2, 1600);
        double ph = 0;
        for (int f = 0; f < 40; ++f)
        {
            for (int i = 0; i < 1600; ++i, ph += 2 * juce::MathConstants<double>::pi * 1000.0 / 48000.0)
                for (int c = 0; c < 2; ++c) buf.setSample (c, i, (float) (0.5 * std::sin (ph)));
            juce::MidiBuffer m; proc.processBlock (buf, m);
            e->animate (1);
        }
        using SA = ui::SpectrumAnalyzer;
        const float at1k = e->analyzerBandDb (SA::kBand1k), at250 = e->analyzerBandDb (SA::bandForHz (250.0f)), at4k = e->analyzerBandDb (SA::bandForHz (4000.0f));
        std::printf ("  1 kHz band %+6.1f dB (expect -6),  250 Hz band %+6.1f dB,  4 kHz band %+6.1f dB\n", at1k, at250, at4k);
        if (std::abs (at1k + 6.0f) > 1.0f || at250 > -40.0f || at4k > -40.0f) ++failures;
    }

    // ---- loudness meter and vectorscope inside the plugin ------------------------
    {
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_in", 0.0f); setParam (s, "R_c_in", 0.0f);
        proc.setPlayConfigDetails (2, 2, 48000.0, 1600);
        proc.prepareToPlay (48000.0, 1600);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
        std::printf ("loudness + vectorscope (48 kHz, EQ flat, tube and comp off):\n");
        juce::AudioBuffer<float> buf (2, 1600);
        double ph = 0;
        auto play = [&] (double seconds, double ampL, double ampR)
        {
            for (int f = 0; f < (int) (seconds * 30); ++f)
            {
                for (int i = 0; i < 1600; ++i, ph += 2 * juce::MathConstants<double>::pi * 1000.0 / 48000.0)
                { buf.setSample (0, i, (float) (ampL * std::sin (ph))); buf.setSample (1, i, (float) (ampR * std::sin (ph))); }
                juce::MidiBuffer m; proc.processBlock (buf, m);
                e->animate (1);
            }
        };
        const double a23 = std::pow (10.0, -23.0 / 20.0);
        play (10.0, a23, a23);
        const float I = proc.loudness.integrated.load(), S = proc.loudness.shortTerm.load(), tp = proc.loudness.truePeakMax.load();
        std::printf ("  1 kHz -23 dBFS stereo: integrated %+.2f, short-term %+.2f LUFS, true peak %+.2f dBTP\n", I, S, tp);
        if (std::abs (I + 23.0f) > 0.15f || std::abs (S + 23.0f) > 0.15f || std::abs (tp + 23.0f) > 0.2f) ++failures;
        std::printf ("  in-phase: correlation %+.2f, balance %+.2f\n", e->vectorscopeCorrelation(), e->vectorscopeBalance());
        if (e->vectorscopeCorrelation() < 0.95f || std::abs (e->vectorscopeBalance()) > 0.05f) ++failures;
        play (1.5, a23, -a23);
        std::printf ("  out of phase: correlation %+.2f\n", e->vectorscopeCorrelation());
        if (e->vectorscopeCorrelation() > -0.95f) ++failures;
        play (1.5, a23, 0.0);
        std::printf ("  left only: balance %+.2f\n", e->vectorscopeBalance());
        if (e->vectorscopeBalance() > -0.95f) ++failures;
        proc.loudness.resetRequested.store (true);
        play (0.2, a23, a23);
        std::printf ("  after RESET: integrated %s\n", proc.loudness.integrated.load() < -100.0f ? "cleared" : "NOT cleared");
        if (proc.loudness.integrated.load() > -100.0f) ++failures;
    }

    // ---- INPUT gain ------------------------------------------------------------------
    {
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_in", 0.0f); setParam (s, "R_c_in", 0.0f);
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        std::printf ("input gain (EQ flat, tube and comp off):\n");
        for (float g : { -24.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            setParam (s, ppm::params::inputGain, g);
            const double got = measureStereo (proc, 48000.0, 1000.0, 0.05, 0.05, 0.5).first;
            std::printf ("  INPUT %+5.1f dB -> output %+6.2f dB\n", g, got);
            if (std::abs (got - g) > 0.1) ++failures;
        }
    }

    // ---- KEY: shifter inside the plugin, latency, detection through the panel ----------
    {
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_in", 0.0f); setParam (s, "R_c_in", 0.0f);
        proc.setPlayConfigDetails (2, 2, 48000.0, 1600);
        proc.prepareToPlay (48000.0, 1600);
        std::printf ("key shift + detection (48 kHz):\n");
        const int lat0 = proc.getLatencySamples();
        setParam (s, ppm::params::keyOn, 1.0f); setParam (s, ppm::params::keySemis, 2.0f);
        juce::AudioBuffer<float> buf (2, 1600);
        std::vector<float> out;
        double ph = 0;
        for (int b = 0; b < 150; ++b)
        {
            for (int i = 0; i < 1600; ++i, ph += 2 * juce::MathConstants<double>::pi * 440.0 / 48000.0)
                for (int c = 0; c < 2; ++c) buf.setSample (c, i, (float) (0.3 * std::sin (ph)));
            juce::MidiBuffer m; proc.processBlock (buf, m);
            for (int i = 0; i < 1600; ++i) out.push_back (buf.getSample (0, i));
        }
        const int lat1 = proc.getLatencySamples();
        // zero-crossing frequency on the last 2 s
        int zc = 0; for (size_t i = out.size() - 96000; i + 1 < out.size(); ++i) if (out[i] <= 0 && out[i + 1] > 0) ++zc;
        const double f = zc / 2.0;
        std::printf ("  latency off %d / on %d samples; 440 Hz +2 st -> %.1f Hz (want 493.9)\n", lat0, lat1, f);
        if (lat1 - lat0 != 4096 || std::abs (f - 493.9) > 1.5) ++failures;
        setParam (s, ppm::params::keyOn, 0.0f);
        for (int b = 0; b < 3; ++b) { juce::MidiBuffer m; proc.processBlock (buf, m); }
        std::printf ("  switched off: latency %d samples\n", proc.getLatencySamples());
        if (proc.getLatencySamples() != lat0) ++failures;

        // detection: an A minor progression through the plug-in, watched by the panel
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
        auto playChords = [&] (int seconds)
        {
            const double chords[4][4] = { { 45, 64, 69, 72 }, { 38, 62, 65, 69 }, { 40, 64, 68, 71 }, { 45, 64, 69, 72 } };
            static double t = 0;
            for (int f = 0; f < seconds * 30; ++f)
            {
                for (int i = 0; i < 1600; ++i, t += 1.0 / 48000.0)
                {
                    const auto& ch = chords[(int) (t / 1.5) % 4];
                    const double inChord = std::fmod (t, 1.5);
                    const double env = std::min (1.0, inChord / 0.01) * std::exp (-inChord / 2.5);
                    double v = 0;
                    for (int ni = 0; ni < 4; ++ni)
                    {
                        const double f0 = 440.0 * std::pow (2.0, (ch[ni] - 69.0) / 12.0);
                        for (int h = 1; h <= 6; ++h) v += (ni == 0 ? 0.35 : 0.12) / h * std::sin (2 * juce::MathConstants<double>::pi * f0 * h * t);
                    }
                    buf.setSample (0, i, (float) (0.4 * env * v)); buf.setSample (1, i, (float) (0.4 * env * v));
                }
                juce::MidiBuffer m; proc.processBlock (buf, m);
                e->animate (1);
            }
        };
        playChords (24);
        std::printf ("  A minor progression -> panel shows %d (21 = A minor)\n", e->detectedKey());
        if (e->detectedKey() != 21) ++failures;
        setParam (s, ppm::params::keyOn, 1.0f); setParam (s, ppm::params::keySemis, 2.0f);
        playChords (30);
        std::printf ("  with KEY SHIFT +2: song key %d (21 = A minor), now playing in %d (23 = B minor)\n", e->detectedKey(), e->shiftedKey());
        if (e->detectedKey() != 21 || e->shiftedKey() != 23) ++failures;
        const juce::File dir (juce::SystemStats::getEnvironmentVariable ("PPM_SNAPSHOT_DIR", juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName()));
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto file = dir.getChildFile ("editor_key.png"); file.deleteFile();
        { juce::FileOutputStream fos (file); juce::PNGImageFormat().writeImageToStream (img, fos); }
    }

    // ---- compressor inside the plugin --------------------------------------------
    {
        std::printf ("compressor (48 kHz, threshold -18 dBFS, EQ flat, tube off):\n");
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_thresh", 12);   setParam (s, "L_c_attack", 3); setParam (s, "L_c_recovery", 0);
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);

        const double a = std::pow (10.0, -6.0 / 20.0);
        auto g = measureStereo (proc, 48000.0, 1000.0, a, a, 2.0);
        const double want = -ppm::variMuGainReductionDb (-6.0 + 18.0, ppm::Compress);
        const float meter = proc.meters.gr[0].load();
        std::printf ("  -6 dBFS in: gain %+.2f dB (curve %+.2f), GR meter %.2f dB\n", g.first, want, meter);
        if (std::abs (g.first - want) > 0.7 || std::abs (meter + want) > 0.7) ++failures;

        // linked: quiet right side gets the loud left side's gain reduction
        auto gl = measureStereo (proc, 48000.0, 1000.0, a, 0.01, 2.0);
        std::printf ("  linked, loud L / quiet R: L %+.2f dB, R %+.2f dB\n", gl.first, gl.second);
        if (std::abs (gl.first - gl.second) > 0.3) ++failures;
        setParam (s, ppm::params::link, 0.0f);
        setParam (s, "R_c_thresh", 12); setParam (s, "R_c_attack", 3); setParam (s, "R_c_recovery", 0);
        auto gu = measureStereo (proc, 48000.0, 1000.0, a, 0.01, 2.0);
        std::printf ("  unlinked:                 L %+.2f dB, R %+.2f dB\n", gu.first, gu.second);
        if (std::abs (gu.second) > 0.2 || gu.first > -6.0) ++failures;
        setParam (s, ppm::params::link, 1.0f);

        // comp out = unity
        setParam (s, "L_c_in", 0.0f);
        auto go = measureStereo (proc, 48000.0, 1000.0, a, a, 1.0);
        std::printf ("  COMP OUT: gain %+.3f dB\n", go.first);
        if (std::abs (go.first) > 0.05) ++failures;
        setParam (s, "L_c_in", 1.0f);

        // order switch: both orders give finite, sensible output, no blow-up while switching
        bool finite = true;
        juce::AudioBuffer<float> buf (2, 512);
        for (int k = 0; k < 200; ++k)
        {
            if (k % 25 == 0) setParam (s, ppm::params::compOrder, (float) ((k / 25) % 2));
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, 0.7f * (float) std::sin (0.13 * (k * 512 + i)));
            juce::MidiBuffer m; proc.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) finite &= std::isfinite (buf.getSample (c, i)) && std::abs (buf.getSample (c, i)) < 4.0f;
        }
        std::printf ("  order switching EQ>COMP / COMP>EQ while playing: %s\n", finite ? "ok" : "PROBLEM");
        if (! finite) ++failures;
    }

    // ---- MAXIMIZER + AUTO MASTER inside the plugin --------------------------------
    {
        std::printf ("auto master (48 kHz):\n");
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        proc.listenSeconds = 12.0; proc.verifySeconds = 5.0;
        proc.setPlayConfigDetails (2, 2, 48000.0, 1600);
        proc.prepareToPlay (48000.0, 1600);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());

        // a quiet, dull mix: kick, bass, chords, soft hats, about -22 LUFS
        juce::AudioBuffer<float> buf (2, 1600);
        double t = 0, lp = 0, lp2 = 0; juce::Random rnd (9);
        double outPeak = 0;
        auto play = [&] (int frames, bool tick)
        {
            for (int f = 0; f < frames; ++f)
            {
                for (int i = 0; i < 1600; ++i, t += 1.0 / 48000.0)
                {
                    const double tw = 2 * juce::MathConstants<double>::pi;
                    const double beat = std::fmod (t, 0.5);
                    const double kick = std::exp (-beat * 14.0) * std::sin (tw * (48.0 + 80.0 * std::exp (-beat * 35.0)) * beat);
                    const double bass = 0.30 * std::sin (tw * 55.0 * t) * (0.6 + 0.4 * std::sin (tw * 0.5 * t));
                    double chord = 0;
                    for (double fr : { 220.0, 261.6, 329.6, 440.0 }) chord += 0.07 * (std::sin (tw * fr * t) + 0.3 * std::sin (tw * 2 * fr * t));
                    const double n = rnd.nextFloat() * 2.0 - 1.0;
                    lp += 0.25 * (n - lp); lp2 += 0.25 * (lp - lp2);                  // dark hats
                    const double hats = 0.25 * std::exp (-std::fmod (t, 0.125) * 50.0) * lp2;
                    const double v = 0.16 * (0.9 * kick + bass + chord + hats);
                    buf.setSample (0, i, (float) v); buf.setSample (1, i, (float) (v * 0.97));
                }
                juce::MidiBuffer m; proc.processBlock (buf, m);
                outPeak = std::max (outPeak, (double) std::max (buf.getMagnitude (0, 0, 1600), buf.getMagnitude (1, 0, 1600)));
                e->animate (1);
                if (tick && f % 3 == 0) proc.autoMasterTick();
            }
        };
        play (60, false);
        const float lat = (float) proc.getLatencySamples();
        setParam (s, ppm::params::amGenre, (float) ppm::automaster::HipHop);
        proc.autoListen();
        int guard = 0;
        while (! proc.autoHasRecording() && ++guard < 2000) play (1, true);
        proc.autoMasterTick();                                     // starts the analysis
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        while (proc.autoBusy()) juce::Thread::sleep (20);
        std::printf ("  analysis of %.0f s took %.1f s\n", proc.listenSeconds, (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
        proc.autoMasterTick();                                     // applies the plan
        const auto plan = proc.getAutoPlan();
        std::printf ("  HIP HOP plan: %s\n    comp threshold step %d (%.1f dB GR), maximizer %+.1f dB, input %.1f -> predicted %.1f LUFS (target %.1f)\n",
                     plan.eqText.toRawUTF8(), plan.comp.threshold, plan.compGrDb, plan.maxGain, plan.inputLufs, plan.predictedLufs, plan.targetLufs);
        if (! plan.ok || std::abs (plan.predictedLufs - plan.targetLufs) > 0.5) ++failures;
        if (s.getRawParameterValue (ppm::params::maxOn)->load() < 0.5f) ++failures;
        // the mix is dull: the plan should lift the top
        ppm::ChannelSettings eqOnly = plan.eq; eqOnly.hp = 0; eqOnly.lp = 0;
        std::printf ("    EQ at 8 kHz %+.1f dB, 63 Hz %+.1f dB\n", ppm::channelResponseDb (eqOnly, 8000), ppm::channelResponseDb (eqOnly, 63));
        if (ppm::channelResponseDb (eqOnly, 8000) < 1.0) ++failures;

        outPeak = 0;
        guard = 0;
        while (proc.getAutoState() != PassiveMasterEQProcessor::AutoState::Done && ++guard < 2000) play (3, true);
        std::printf ("  after the loudness check: %s\n    measured %.1f LUFS, output peak %.2f dBFS (ceiling %.1f), latency %.0f samples\n",
                     proc.getAutoMessage().toRawUTF8(), proc.getVerifyLufs(), 20 * std::log10 (outPeak), plan.ceiling, lat);
        if (std::abs (proc.getVerifyLufs() - plan.targetLufs) > 1.0) ++failures;
        if (20 * std::log10 (outPeak) > plan.ceiling + 0.01) ++failures;

        const juce::File dir (juce::SystemStats::getEnvironmentVariable ("PPM_SNAPSHOT_DIR", juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName()));
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto file = dir.getChildFile ("editor_auto.png"); file.deleteFile();
        { juce::FileOutputStream fos (file); juce::PNGImageFormat().writeImageToStream (img, fos); }

        // genre change re-plans from the same recording
        setParam (s, ppm::params::amGenre, (float) ppm::automaster::Jazz);
        proc.autoReapply();
        while (proc.autoBusy()) juce::Thread::sleep (20);
        proc.autoMasterTick();
        const auto jp = proc.getAutoPlan();
        std::printf ("  JAZZ re-plan: maximizer %+.1f dB, predicted %.1f LUFS (target %.1f)\n", jp.maxGain, jp.predictedLufs, jp.targetLufs);
        if (! jp.ok || std::abs (jp.predictedLufs - jp.targetLufs) > 0.5) ++failures;

        proc.autoUndo();
        const bool undone = s.getRawParameterValue (ppm::params::maxOn)->load() < 0.5f
                         && std::lround (s.getRawParameterValue ("L_c_thresh")->load()) == 0;
        std::printf ("  UNDO restores the original settings: %s\n", undone ? "yes" : "NO");
        if (! undone) ++failures;
    }

    std::printf ("%s\n", failures ? "FAILED" : "PASSED");
    return failures;
}
