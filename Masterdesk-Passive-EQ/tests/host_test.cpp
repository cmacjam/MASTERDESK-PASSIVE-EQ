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
        const float at1k = e->analyzerBandDb (17), at250 = e->analyzerBandDb (11), at4k = e->analyzerBandDb (23);
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

    std::printf ("%s\n", failures ? "FAILED" : "PASSED");
    return failures;
}
