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

static double measureDb (PassiveMasterEQProcessor& proc, double sr, double freq)
{
    const int block = 512, total = (int) sr; // 1 s
    juce::AudioBuffer<float> buf (2, block);
    double acc = 0; int count = 0; double phase = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            const float v = 0.1f * (float) std::sin (phase); phase += 2 * juce::MathConstants<double>::pi * freq / sr;
            buf.setSample (0, i, v); buf.setSample (1, i, v);
        }
        juce::MidiBuffer m;
        proc.processBlock (buf, m);
        if (pos > total / 2)
            for (int i = 0; i < block; ++i) { acc += buf.getSample (0, i) * buf.getSample (0, i); ++count; }
    }
    return 20.0 * std::log10 (std::sqrt (acc / count) / (0.1 / std::sqrt (2.0)));
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
                e->simulateMeters (-3.0f, -1.0f, 0.25f);
                snap (*ed, "editor.png");
            }
            setParam (s, ppm::params::link, 1.0f);
            {
                std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
                auto* e = dynamic_cast<PassiveMasterEQEditor*> (ed.get());
                e->simulateMeters (0.5f, 1.5f, 1.2f);
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
            const auto c = proc.getChannelSettings (0);
            double lo = 1e9, hi = -1e9;
            for (double f = 20; f <= 20000; f *= 1.05) { const double d = ppm::channelResponseDb (c, f); lo = std::min (lo, d); hi = std::max (hi, d); }
            const double at1k = measureDb (proc, 48000.0, 1000.0) - ppm::channelResponseDb (c, 1000.0);
            std::printf ("  %-26s range %+6.1f .. %+5.1f dB   (plugin vs model @1k %+.2f)\n",
                         proc.getProgramName (i).toRawUTF8(), lo, hi, at1k);
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

    std::printf ("%s\n", failures ? "FAILED" : "PASSED");
    return failures;
}
