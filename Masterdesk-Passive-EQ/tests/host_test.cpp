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

// every failed check names its line, so a CI run shows which check failed without the full log
static void failCheck (int& failures, int line)
{
    ++failures;
    std::printf ("CHECK FAILED at host_test.cpp line %d\n", line);
    std::fflush (stdout);
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
        if (worst > 0.5) failCheck (failures, __LINE__);

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
        if (! finite) failCheck (failures, __LINE__);

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
                // pop-out units: build each one offscreen and snapshot it
                for (int u = 0; u < ui::Faceplate::kNumUnits; ++u)
                {
                    auto host = e->createUnitHost (u);
                    host->setLookAndFeel (&ed->getLookAndFeel());
                    host->setSize (host->getWidth() * 7 / 10, host->getHeight() * 7 / 10);
                    host->tick();
                    snap (*host, "unit_" + std::to_string (u) + ".png");
                }
                e->toggleCompact();                             // MINI view
                playMusic (proc, *e, sr, 30);
                snap (*ed, "editor_mini.png");
                e->toggleCompact();
                setParam (s, ppm::params::inputGain, 0.0f);
                // side handles: sideways = wider (panel spreads out, height stays), down = bigger, narrower = smaller
                const double aspect = (double) PassiveMasterEQEditor::designWidth() / PassiveMasterEQEditor::designHeight();
                int maxW = 100000, maxH = 100000;
                if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (ed->getScreenBounds()))
                    { maxW = d->userArea.getWidth(); maxH = d->userArea.getHeight() - 80; }
                // CI machines have small virtual screens (often shorter than the plug-in's smallest size, 30 %):
                // the editor never goes below that size, so the room it really has is the larger of the two
                const int minW = (int) std::ceil (PassiveMasterEQEditor::designWidth() * 0.30);
                const int minH = (int) std::lround (minW / aspect);
                maxH = std::max (maxH, minH);
                maxW = std::max (maxW, minW);
                // start inside that room
                const int startW = std::max (minW, std::min (600, (int) ((maxH - 120) * aspect)));
                ed->setSize (startW, (int) std::lround (startW / aspect));
                const int w0 = ed->getWidth(), h0 = ed->getHeight();
                e->testEdgeDrag (300, 0, true);                       // pull the right side out
                const int w1 = ed->getWidth(), h1 = ed->getHeight(), p1 = e->panelWidth();
                e->testEdgeDrag (-200, 0, false);                     // pull the left side out (leftwards = wider)
                const int w2 = ed->getWidth(), h2 = ed->getHeight();
                e->testEdgeDrag (0, 60, true);                        // pull down: bigger
                const int w3 = ed->getWidth(), h3 = ed->getHeight();
                e->testEdgeDrag (-2000, 0, true);                     // push far in: never narrower than the panel's shape
                const int w4 = ed->getWidth(), h4 = ed->getHeight();
                std::printf ("  side handles: %dx%d -> right +300 -> %dx%d (panel %d wide, standard %d) -> left +200 -> %dx%d -> down +60 -> %dx%d -> pushed in -> %dx%d (shape %.3f, min %.3f)\n",
                             w0, h0, w1, h1, p1, PassiveMasterEQEditor::designWidth(), w2, h2, w3, h3, w4, h4, (double) w4 / h4, aspect);
                if (std::abs (w1 - std::min (w0 + 300, maxW)) > 2 || h1 != h0 || p1 <= PassiveMasterEQEditor::designWidth()
                    || std::abs (w2 - std::min (w1 + 200, maxW)) > 2 || h2 != h0
                    || std::abs (h3 - std::min (h2 + 60, maxH)) > 2 || w3 < w2
                    || (double) w4 / h4 < aspect - 0.01 || h4 > maxH + 1) failCheck (failures, __LINE__);
                // SCROLL mode: the panel fills the width; hovering near the top / bottom edge glides it up / down (no scroll bar)
                {
                    e->toggleScroll();
                    ed->setSize (840, 520);                               // the editor may fit this to a small screen:
                    const int sw0 = ed->getWidth(), sh0 = ed->getHeight();   // use the size it really has
                    const double maxPos = e->scrollMax();
                    e->hoverScrollStep (sh0 / 2, 1.0);                        // mouse in the middle: nothing moves
                    const double still = e->scrollPosition();
                    snap (*ed, "editor_scroll_top.png");
                    e->hoverScrollStep (sh0 - 6, 0.5);                    // near the bottom edge for half a second
                    const double down1 = e->scrollPosition();
                    for (int i = 0; i < 400; ++i) e->hoverScrollStep (sh0 - 2, 1.0 / 60.0);   // keep hovering: reaches the end
                    const double bottom = e->scrollPosition();
                    snap (*ed, "editor_scroll_bottom.png");
                    e->hoverScrollStep (40, 0.5);                         // just inside the top zone: slow
                    const double slowUp = bottom - e->scrollPosition();
                    e->hoverScrollStep (2, 0.5);                          // right at the top edge: fast
                    const double fastUp = bottom - slowUp - e->scrollPosition();
                    for (int i = 0; i < 400; ++i) e->hoverScrollStep (1, 1.0 / 60.0);
                    const double top = e->scrollPosition();
                    const int sw = ed->getWidth(), sh = ed->getHeight();
                    e->toggleScroll();                                    // back to the normal fit-to-window view
                    const int nw = ed->getWidth(), nh = ed->getHeight();
                    std::printf ("  hover scroll: %dx%d, range 0..%.0f of %d panel px; middle %.0f; bottom edge 0.5 s -> %.0f, held -> %.0f; "
                                 "top zone slow %.0f / edge fast %.0f px per 0.5 s; held -> %.0f; off -> %dx%d\n",
                                 sw, sh, maxPos, PassiveMasterEQEditor::designHeight(), still, down1, bottom, slowUp, fastUp, top, nw, nh);
                    // at 840 wide the zoom is 840/1440 = 0.583, so 520 px shows 891 of the 2380 panel px
                    if (std::abs (maxPos - (PassiveMasterEQEditor::designHeight() - sh0 * 1440.0 / (double) (sw0 - PassiveMasterEQEditor::kScrollBarW))) > 2.0 || still != 0.0
                        || down1 <= 100.0 || std::abs (bottom - maxPos) > 0.01 || slowUp <= 0.0 || fastUp < 3.0 * slowUp
                        || top != 0.0 || (double) nw / nh < aspect - 0.01) failCheck (failures, __LINE__);
                }
                // CLICK & ZOOM: zoom in at a point (it stays under the mouse), look around, zoom back out
                {
                    ed->setSize (600, 992);                               // may be fitted to a small screen: use the real size
                    const float zw = (float) ed->getWidth(), zh = (float) ed->getHeight();
                    const juce::Point<float> at (zw * 0.75f, zh * 0.3f);
                    const auto before = e->visiblePanelArea();
                    const auto pBefore = ed->getLocalPoint (ed.get(), at);
                    const juce::Point<float> panelAt (before.getX() + at.x / zw * before.getWidth(), before.getY() + at.y / zh * before.getHeight());
                    e->toggleZoomAt (at);
                    const auto in = e->visiblePanelArea();
                    const juce::Point<float> panelAfter (in.getX() + at.x / zw * in.getWidth(), in.getY() + at.y / zh * in.getHeight());
                    snap (*ed, "editor_zoom.png");
                    const float z = e->zoomFactor();
                    e->hoverPanStep (2, (int) (zh * 0.5f), 0.5);                        // left edge: look left
                    const float leftMove = in.getX() - e->visiblePanelArea().getX();
                    e->hoverPanStep ((int) (zw * 0.5f), (int) zh - 2, 0.5);                      // bottom edge: look down
                    const float downMove = e->visiblePanelArea().getY() - in.getY();
                    for (int i = 0; i < 600; ++i) e->hoverPanStep (2, 2, 1.0 / 60.0);   // top-left corner: stops at the panel's edge
                    const auto corner = e->visiblePanelArea();
                    e->toggleZoomAt (at);                                 // double-click again: back out
                    const auto out = e->visiblePanelArea();
                    std::printf ("  click & zoom: %.1fx, view %.0fx%.0f -> %.0fx%.0f panel px, point under mouse (%.0f,%.0f) -> (%.0f,%.0f); "
                                 "hover left %.0f, down %.0f; corner (%.0f,%.0f); out %.1fx, view %.0fx%.0f\n",
                                 z, before.getWidth(), before.getHeight(), in.getWidth(), in.getHeight(), panelAt.x, panelAt.y, panelAfter.x, panelAfter.y,
                                 leftMove, downMove, corner.getX(), corner.getY(), e->zoomFactor(), out.getWidth(), out.getHeight());
                    juce::ignoreUnused (pBefore);
                    if (std::abs (z - 2.0f) > 0.01f || std::abs (in.getWidth() * 2.0f - before.getWidth()) > 2.0f
                        || panelAt.getDistanceFrom (panelAfter) > 2.0f || leftMove <= 10.0f || downMove <= 10.0f
                        || corner.getX() > 0.5f || corner.getY() > 0.5f || e->zoomFactor() != 1.0f
                        || std::abs (out.getWidth() - before.getWidth()) > 2.0f) failCheck (failures, __LINE__);
                }
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
            setParam (proc.apvts, ppm::params::pq::in, 0.0f); setParam (proc.apvts, ppm::params::ca::in, 0.0f);   // passive EQ alone
            const auto c = proc.getChannelSettings (0);
            double lo = 1e9, hi = -1e9;
            for (double f = 20; f <= 20000; f *= 1.05) { const double d = ppm::channelResponseDb (c, f); lo = std::min (lo, d); hi = std::max (hi, d); }
            const double at1k = measureDb (proc, 48000.0, 1000.0) - ppm::channelResponseDb (c, 1000.0);
            std::printf ("  %-32s comp %s thr %+5.1f  ", proc.getProgramName (i).toRawUTF8(), cs.compIn ? "in " : "out", ppm::compThresholdDb (cs.threshold));
            std::printf ("EQ %+6.1f .. %+5.1f dB  (vs model @1k %+.2f)\n",
                         lo, hi, at1k);
            if (std::abs (at1k) > 0.6 || hi > 14.0) failCheck (failures, __LINE__);     // at1k includes tube colour on presets with drive
        }
        // engineer presets: every EQ preset measured through the plug-in matches the units' models
        // (parametric + passive), and the compressor settings land on the Class-A / Vari-Mu
        {
            int nEng = 0, nEq = 0; double worstAll = 0;
            const auto& list = ppm::presets::all();
            for (int i = 0; i < proc.getNumPrograms(); ++i)
            {
                const auto& pr = list[(size_t) i];
                if (! pr.full) continue;
                ++nEng;
                proc.setCurrentProgram (i);
                const auto ca = proc.getClassASettings();
                const auto cs = proc.getCompSettings (0);
                if (ca.in != pr.ca.in || cs.compIn != pr.comp.compIn
                    || (pr.ca.in && (std::abs (ca.ch[0].ratioPos - pr.ca.ch[0].ratioPos) > 0.011 || std::abs (ca.ch[0].thresholdDb - pr.ca.ch[0].thresholdDb) > 0.06
                                     || std::abs (ca.ch[0].attackMs - pr.ca.ch[0].attackMs) > 0.011 + 0.01 * pr.ca.ch[0].attackMs
                                     || ca.ch[0].feedForward != pr.ca.ch[0].feedForward || std::abs (ca.ch[0].mix - pr.ca.ch[0].mix) > 0.006)))
                { std::printf ("  ENGINEER PRESET %s: compressor settings did not land\n", pr.name.c_str()); failCheck (failures, __LINE__); }
                const bool hasEq = pr.pq.in || std::any_of (std::begin (pr.ch.band), std::end (pr.ch.band), [] (const ppm::BandSettings& b) { return b.gainStep > 0; }) || pr.ch.hp || pr.ch.lp;
                if (! hasEq) continue;
                ++nEq;
                // EQ only: compressors and tubes out
                setParam (proc.apvts, "L_c_in", 0.0f); setParam (proc.apvts, "R_c_in", 0.0f);
                setParam (proc.apvts, ppm::params::ca::in, 0.0f); setParam (proc.apvts, ppm::params::tubeOn, 0.0f);
                const auto pqs = proc.getParametricSettings();
                const auto pas = proc.getChannelSettings (0);
                double worst = 0;
                for (double f : { 40.0, 100.0, 250.0, 700.0, 1500.0, 3500.0, 8000.0, 14000.0 })
                {
                    const double model = (pqs.in ? ppm::sontec::channelResponseDb (pqs.ch[0], f) : 0.0) + ppm::channelResponseDb (pas, f);
                    worst = std::max (worst, std::abs (measureDb (proc, 48000.0, f) - model));
                }
                worstAll = std::max (worstAll, worst);
                if (worst > 0.35) { std::printf ("  ENGINEER PRESET %s: plug-in %.2f dB off its model\n", pr.name.c_str(), worst); failCheck (failures, __LINE__); }
            }
            std::printf ("  engineer presets: %d (%d with EQ), plug-in vs model worst %.2f dB, compressor settings land: checked\n", nEng, nEq, worstAll);
            if (nEng < 35) failCheck (failures, __LINE__);
        }
        // PRESETS buttons on the units: each lists its own presets and changes only its own unit
        {
            using namespace ppm::presets;
            const auto eq = unitPresets (UnitKind::Eq), vm = unitPresets (UnitKind::VariMu), ca = unitPresets (UnitKind::ClassA);
            const auto& list = all();
            auto find = [&] (const char* name) { for (int i = 0; i < (int) list.size(); ++i) if (list[(size_t) i].name == name) return i; return -1; };
            proc.setCurrentProgram (0);
            const int cla = find ("Chris Lord-Alge - Lead Vocals (1176 Fast Stage)"), bou = find ("Eric Boulanger - Surgical Mastering EQ"),
                      schmitt = find ("Al Schmitt - Silky Tube Vocal Leveler");
            proc.applyEqPreset (bou);                                    // EQ menu: both EQs, compressors untouched
            const bool eqOk = proc.getParametricSettings().in && ! proc.getClassASettings().in && proc.getChannelSettings (0).hp == list[(size_t) bou].ch.hp;
            proc.applyClassAPreset (cla);                                // Class-A menu: EQs stay as they were
            const auto cas = proc.getClassASettings();
            const bool caOk = cas.in && std::abs (cas.ch[0].ratioPos - list[(size_t) cla].ca.ch[0].ratioPos) < 0.011 && proc.getParametricSettings().in;
            proc.applyVariMuPreset (schmitt);                            // Vari-Mu menu
            const auto vms = proc.getCompSettings (0);
            const bool vmOk = vms.compIn && vms.threshold == list[(size_t) schmitt].comp.threshold && proc.getClassASettings().in && proc.getParametricSettings().in;
            std::printf ("  unit PRESETS buttons: EQ %d, Vari-Mu %d, Class-A %d presets; EQ only %s, Class-A only %s, Vari-Mu only %s\n",
                         (int) eq.size(), (int) vm.size(), (int) ca.size(), eqOk ? "ok" : "WRONG", caOk ? "ok" : "WRONG", vmOk ? "ok" : "WRONG");
            bool masterOnly = true;
            for (int i : eq) masterOnly = masterOnly && (list[(size_t) i].category == "Factory" || list[(size_t) i].category == "Mix & Master Bus"
                                                         || list[(size_t) i].category == kManualCategory || isTrackCategory (list[(size_t) i].category));
            for (const auto& p : list) if (p.full && p.category != "Mix & Master Bus" && ! isTrackCategory (p.category) && presetHasEq (p) && p.name != "Init (Flat)") masterOnly = false;
            std::printf ("  EQ presets are stereo-master only: %s\n", masterOnly ? "yes" : "NO");
            // no two presets in a unit's menu have the same settings
            auto eqKey = [&] (const Preset& p)
            {
                juce::String k;
                for (const auto& b : p.ch.band) k << (b.gainStep > 0 && b.mode != ppm::Out ? juce::String (b.freqIndex) + "," + juce::String (b.gainStep) + "," + juce::String (b.bwStep) + "," + juce::String (b.mode) + "," + juce::String ((int) b.shelf) : juce::String ("-")) << ";";
                k << p.ch.hp << "/" << p.ch.lp << "/" << p.ch.trim;
                if (p.full && p.pq.in) for (const auto& b : p.pq.ch[0].band) k << "|" << b.freqIdx << "," << b.shape << "," << b.level;
                if (p.full && p.pq.in) k << "|" << p.pq.ch[0].hiShelfLevel << (int) p.pq.ch[0].hiShelf12k << p.pq.ch[0].loShelfLevel << (int) p.pq.ch[0].loShelf100;
                return k;
            };
            auto vmKey = [] (const Preset& p) { const auto& c = p.comp; return juce::String (c.threshold) + "," + juce::String (c.attack) + "," + juce::String (c.recovery) + "," + juce::String (c.output) + "," + juce::String (c.mix) + "," + juce::String (c.mode); };
            auto caKey = [] (const Preset& p) { const auto& h = p.ca.ch[0]; return juce::String (h.thresholdDb) + "," + juce::String (h.attackMs) + "," + juce::String (h.releaseMs) + "," + juce::String (h.ratioPos) + "," + juce::String ((int) h.feedForward) + "," + juce::String (h.mix); };
            int dups = 0;
            auto check = [&] (const std::vector<int>& ids, std::function<juce::String (const Preset&)> key)
            {
                for (size_t a = 0; a < ids.size(); ++a)
                    for (size_t b = a + 1; b < ids.size(); ++b)
                        if (key (list[(size_t) ids[a]]) == key (list[(size_t) ids[b]]))
                        { std::printf ("  SAME SETTINGS: %s / %s\n", list[(size_t) ids[a]].name.c_str(), list[(size_t) ids[b]].name.c_str()); ++dups; }
            };
            check (eq, eqKey); check (vm, vmKey); check (ca, caKey);
            std::printf ("  duplicate presets: %d\n", dups);
            if (dups) failCheck (failures, __LINE__);

            // EQ preset + matching compressor: every EQ preset finds its compressor, and choosing one
            // leaves exactly one compressor running (or none for flat / track presets without one)
            int nLinked = 0, nVm = 0, nCa = 0, bad = 0, nTrack = 0;
            for (int i : eq)
            {
                const auto lc = linkedCompressor (i);
                const auto& p = list[(size_t) i];
                if (isTrackCategory (p.category)) ++nTrack;
                if (lc.kind != UnitKind::Eq && lc.index < 0) { std::printf ("  NO MATCHING COMPRESSOR FOUND for %s\n", p.name.c_str()); ++bad; continue; }
                proc.applyEqPreset (i, true);
                const bool vmOn = proc.getCompSettings (0).compIn, caOn = proc.getClassASettings().in;
                const bool ok = lc.kind == UnitKind::VariMu ? (vmOn && ! caOn) : lc.kind == UnitKind::ClassA ? (caOn && ! vmOn) : (! vmOn && ! caOn);
                if (! ok) { std::printf ("  %s: compressors after linking wrong (vari-mu %d, class-A %d)\n", p.name.c_str(), (int) vmOn, (int) caOn); ++bad; }
                if (lc.kind == UnitKind::VariMu) ++nVm; else if (lc.kind == UnitKind::ClassA) ++nCa;
                if (lc.kind != UnitKind::Eq) ++nLinked;
            }
            const bool vmBefore = proc.getCompSettings (0).compIn, caBefore = proc.getClassASettings().in;
            const float thrBefore = proc.getClassASettings().ch[0].thresholdDb;
            proc.applyEqPreset (find ("Eric Boulanger - Surgical Mastering EQ"), false);   // link off: compressors untouched
            const bool untouched = proc.getCompSettings (0).compIn == vmBefore && proc.getClassASettings().in == caBefore
                                && proc.getClassASettings().ch[0].thresholdDb == thrBefore;
            std::printf ("  EQ + matching compressor: %d of %d EQ presets link one (%d Vari-Mu, %d Class-A); %d track presets; link off leaves the compressors: %s\n",
                         nLinked, (int) eq.size(), nVm, nCa, nTrack, untouched ? "yes" : "NO");
            if (bad || nTrack < 16 || ! untouched) failCheck (failures, __LINE__);
            if (! masterOnly) failCheck (failures, __LINE__);
            if (eq.size() < 25 || vm.size() < 8 || ca.size() < 25 || ! eqOk || ! caOk || ! vmOk) failCheck (failures, __LINE__);
        }
        // state round-trip keeps the program and parameters
        proc.setCurrentProgram (3);
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        PassiveMasterEQProcessor other; other.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool same = other.getCurrentProgram() == 3
                       && other.getChannelSettings (0).band[1].mode == ppm::Cut
                       && other.getChannelSettings (0).hp == 4;
        std::printf ("  state round-trip: %s\n", same ? "ok" : "MISMATCH");
        if (! same) failCheck (failures, __LINE__);
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
        if (std::abs (at1k + 6.0f) > 1.0f || at250 > -40.0f || at4k > -40.0f) failCheck (failures, __LINE__);
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
        if (std::abs (I + 23.0f) > 0.15f || std::abs (S + 23.0f) > 0.15f || std::abs (tp + 23.0f) > 0.2f) failCheck (failures, __LINE__);
        std::printf ("  in-phase: correlation %+.2f, balance %+.2f\n", e->vectorscopeCorrelation(), e->vectorscopeBalance());
        if (e->vectorscopeCorrelation() < 0.95f || std::abs (e->vectorscopeBalance()) > 0.05f) failCheck (failures, __LINE__);
        play (1.5, a23, -a23);
        std::printf ("  out of phase: correlation %+.2f\n", e->vectorscopeCorrelation());
        if (e->vectorscopeCorrelation() > -0.95f) failCheck (failures, __LINE__);
        play (1.5, a23, 0.0);
        std::printf ("  left only: balance %+.2f\n", e->vectorscopeBalance());
        if (e->vectorscopeBalance() > -0.95f) failCheck (failures, __LINE__);
        proc.loudness.resetRequested.store (true);
        play (0.2, a23, a23);
        std::printf ("  after RESET: integrated %s\n", proc.loudness.integrated.load() < -100.0f ? "cleared" : "NOT cleared");
        if (proc.loudness.integrated.load() > -100.0f) failCheck (failures, __LINE__);
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
            if (std::abs (got - g) > 0.1) failCheck (failures, __LINE__);
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
        if (lat1 - lat0 != 4096 || std::abs (f - 493.9) > 1.5) failCheck (failures, __LINE__);
        setParam (s, ppm::params::keyOn, 0.0f);
        for (int b = 0; b < 3; ++b) { juce::MidiBuffer m; proc.processBlock (buf, m); }
        std::printf ("  switched off: latency %d samples\n", proc.getLatencySamples());
        if (proc.getLatencySamples() != lat0) failCheck (failures, __LINE__);

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
        if (e->detectedKey() != 21) failCheck (failures, __LINE__);
        setParam (s, ppm::params::keyOn, 1.0f); setParam (s, ppm::params::keySemis, 2.0f);
        playChords (30);
        std::printf ("  with KEY SHIFT +2: song key %d (21 = A minor), now playing in %d (23 = B minor)\n", e->detectedKey(), e->shiftedKey());
        if (e->detectedKey() != 21 || e->shiftedKey() != 23) failCheck (failures, __LINE__);
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
        if (std::abs (g.first - want) > 0.7 || std::abs (meter + want) > 0.7) failCheck (failures, __LINE__);

        // linked: quiet right side gets the loud left side's gain reduction
        auto gl = measureStereo (proc, 48000.0, 1000.0, a, 0.01, 2.0);
        std::printf ("  linked, loud L / quiet R: L %+.2f dB, R %+.2f dB\n", gl.first, gl.second);
        if (std::abs (gl.first - gl.second) > 0.3) failCheck (failures, __LINE__);
        setParam (s, ppm::params::link, 0.0f);
        setParam (s, "R_c_thresh", 12); setParam (s, "R_c_attack", 3); setParam (s, "R_c_recovery", 0);
        auto gu = measureStereo (proc, 48000.0, 1000.0, a, 0.01, 2.0);
        std::printf ("  unlinked:                 L %+.2f dB, R %+.2f dB\n", gu.first, gu.second);
        if (std::abs (gu.second) > 0.2 || gu.first > -6.0) failCheck (failures, __LINE__);
        setParam (s, ppm::params::link, 1.0f);

        // comp out = unity
        setParam (s, "L_c_in", 0.0f);
        auto go = measureStereo (proc, 48000.0, 1000.0, a, a, 1.0);
        std::printf ("  COMP OUT: gain %+.3f dB\n", go.first);
        if (std::abs (go.first) > 0.05) failCheck (failures, __LINE__);
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
        if (! finite) failCheck (failures, __LINE__);
    }

    // ---- parametric mastering EQ inside the plugin ------------------------------
    {
        std::printf ("parametric mastering EQ (44.1 kHz, passive EQ flat, tube and comp off):\n");
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        namespace pq = ppm::params::pq;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_in", 0.0f); setParam (s, "R_c_in", 0.0f);
        setParam (s, pq::in, 1.0f);
        setParam (s, pq::band (0, 0, "freq"), 13.0f); setParam (s, pq::band (0, 0, "level"), 22.0f);   // LO 100 Hz +7
        setParam (s, pq::band (0, 1, "freq"), 14.0f); setParam (s, pq::band (0, 1, "shape"), 4.0f);
        setParam (s, pq::band (0, 1, "level"), 4.0f);                                                   // MID 1.4 kHz narrow -5
        setParam (s, pq::band (0, 2, "freq"), 21.0f); setParam (s, pq::band (0, 2, "level"), 20.0f);   // HI 23 kHz +5
        setParam (s, pq::id (0, "hs"), 15.0f);                                                          // hi shelf +1.5
        proc.setPlayConfigDetails (2, 2, 44100.0, 512);
        proc.prepareToPlay (44100.0, 512);
        const auto cs = proc.getParametricSettings().ch[0];
        double worst = 0;
        for (double f : { 40.0, 100.0, 400.0, 1400.0, 5000.0, 12000.0, 18000.0 })
        {
            const double got = measureDb (proc, 44100.0, f), want = ppm::sontec::channelResponseDb (cs, f);
            worst = std::max (worst, std::abs (got - want));
            std::printf ("    %6.0f Hz  plugin %+6.2f dB   model %+6.2f dB\n", f, got, want);
        }
        std::printf ("  worst |diff| = %.2f dB (linked: right follows left)\n", worst);
        if (worst > 0.15) failCheck (failures, __LINE__);
        setParam (s, pq::in, 0.0f);
        const double off = measureDb (proc, 44100.0, 100.0);
        std::printf ("  PUSH EQ off: %+.3f dB at 100 Hz\n", off);
        if (std::abs (off) > 0.01) failCheck (failures, __LINE__);
    }

    // ---- M/S CLASS-A compressor inside the plugin --------------------------------
    {
        std::printf ("M/S class-A compressor (48 kHz, EQ flat, tube and vari-mu off):\n");
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        setParam (s, ppm::params::tubeOn, 0.0f);
        setParam (s, "L_c_in", 0.0f); setParam (s, "R_c_in", 0.0f);
        namespace ca = ppm::params::ca;
        setParam (s, ca::in, 1.0f); setParam (s, ca::mode, (float) ppm::ClassAMidSide); setParam (s, ca::link, 0.0f);
        setParam (s, ca::chan (0, "thr"), -30.0f); setParam (s, ca::chan (0, "ratio"), 11.0f);  // squeeze the MID (1:2.5 feedback)
        setParam (s, ca::chan (0, "att"), 2.0f);
        setParam (s, ca::chan (1, "thr"), 2.0f);                                                 // leave the SIDE alone
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        const double a = std::pow (10.0, -10.0 / 20.0);
        auto mid = measureStereo (proc, 48000.0, 1000.0, a, a, 1.0);
        auto side = measureStereo (proc, 48000.0, 1000.0, a, -a, 1.0);
        // expected: the feedback law, 20 dB over threshold: g = (R(g) - 1) / R(g) * 20  (solved by iteration)
        double g = 0;
        for (int it = 0; it < 50; ++it) { const double R = ppm::classAFeedbackRatio (11.0, g); g = (R - 1.0) / R * 20.0; }
        const double want = -g;
        std::printf ("  M/S, MID threshold -30 / 1:2.5 feedback, SIDE off: mono -10 dBFS -> %+.2f dB (curve %+.2f), out-of-phase -> %+.2f dB, MID meter %.1f dB\n",
                     mid.first, want, side.first, proc.meters.grA[0].load());
        if (std::abs (mid.first - want) > 1.0 || std::abs (side.first) > 0.1) failCheck (failures, __LINE__);
        // soft clip at the plug-in's output: +9 dBFS in, never above 0 dBFS out
        setParam (s, ca::chan (0, "thr"), 2.0f);
        setParam (s, ca::clipOn, 1.0f);
        juce::AudioBuffer<float> buf (2, 512); double peak = 0, ph = 0;
        for (int b = 0; b < 100; ++b)
        {
            for (int i = 0; i < 512; ++i, ph += 2 * juce::MathConstants<double>::pi * 1000.0 / 48000.0)
                for (int c = 0; c < 2; ++c) buf.setSample (c, i, (float) (2.8 * std::sin (ph)));
            juce::MidiBuffer m; proc.processBlock (buf, m);
            if (b > 10) for (int c = 0; c < 2; ++c) peak = std::max (peak, (double) buf.getMagnitude (c, 0, 512));
        }
        std::printf ("  SOFT CLIP: +9 dBFS in -> output peak %.3f (%.2f dBFS)\n", peak, 20 * std::log10 (peak));
        if (peak > 1.02) failCheck (failures, __LINE__);                    // (the oversampling filter may add a hair above the clipper's 1.0)
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
        if (! plan.ok || std::abs (plan.predictedLufs - plan.targetLufs) > 0.5) failCheck (failures, __LINE__);
        if (s.getRawParameterValue (ppm::params::maxOn)->load() < 0.5f) failCheck (failures, __LINE__);
        // the mix is dull: the plan should lift the top
        ppm::ChannelSettings eqOnly = plan.eq; eqOnly.hp = 0; eqOnly.lp = 0;
        std::printf ("    EQ at 8 kHz %+.1f dB, 63 Hz %+.1f dB\n", ppm::channelResponseDb (eqOnly, 8000), ppm::channelResponseDb (eqOnly, 63));
        if (ppm::channelResponseDb (eqOnly, 8000) < 1.0) failCheck (failures, __LINE__);

        outPeak = 0;
        guard = 0;
        while (proc.getAutoState() != PassiveMasterEQProcessor::AutoState::Done && ++guard < 2000) play (3, true);
        std::printf ("  after the loudness check: %s\n    measured %.1f LUFS, output peak %.2f dBFS (ceiling %.1f), latency %.0f samples\n",
                     proc.getAutoMessage().toRawUTF8(), proc.getVerifyLufs(), 20 * std::log10 (outPeak), plan.ceiling, lat);
        if (std::abs (proc.getVerifyLufs() - plan.targetLufs) > 1.0) failCheck (failures, __LINE__);
        if (20 * std::log10 (outPeak) > plan.ceiling + 0.01) failCheck (failures, __LINE__);

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
        if (! jp.ok || std::abs (jp.predictedLufs - jp.targetLufs) > 0.5) failCheck (failures, __LINE__);

        proc.autoUndo();
        const bool undone = s.getRawParameterValue (ppm::params::maxOn)->load() < 0.5f
                         && std::lround (s.getRawParameterValue ("L_c_thresh")->load()) == 0;
        std::printf ("  UNDO restores the original settings: %s\n", undone ? "yes" : "NO");
        if (! undone) failCheck (failures, __LINE__);
    }

    // ---- everything together: AUTO MASTER plans around the Parametric EQ and the Class-A the user has on,
    //      and its loudness prediction matches what really plays (no loudness-check nudging)
    {
        std::printf ("all units together (48 kHz):\n");
        PassiveMasterEQProcessor proc;
        auto& s = proc.apvts;
        proc.listenSeconds = 12.0; proc.verifySeconds = 1000.0;          // keep measuring, never nudge
        proc.setPlayConfigDetails (2, 2, 48000.0, 1600);
        proc.prepareToPlay (48000.0, 1600);
        // Parametric EQ: +6 dB at 100 Hz (LO band); Class-A: 4:1 feed-forward, -20 dB threshold, +3 dB gain
        ppm::sontec::Settings pq; pq.in = true;
        for (auto& c : pq.ch) { c.in = true; c.band[0] = { 13, 1, 24 }; }
        proc.applyParametric (pq);
        ppm::ClassASettings ca; ca.in = true; ca.link = true;
        for (auto& c : ca.ch) { c.thresholdDb = -20; c.ratioPos = 7.5; c.feedForward = true; c.attackMs = 10; c.releaseMs = 200; c.gainDb = 3; c.mix = 1; }
        proc.applyClassA (ca);
        setParam (s, ppm::params::amGenre, (float) ppm::automaster::Pop);

        juce::AudioBuffer<float> buf (2, 1600);
        double t = 0; juce::Random rnd (3); double outPeak = 0;
        auto play = [&] (int frames, bool tick)
        {
            for (int f = 0; f < frames; ++f)
            {
                for (int i = 0; i < 1600; ++i, t += 1.0 / 48000.0)
                {
                    const double tw = 2 * juce::MathConstants<double>::pi, beat = std::fmod (t, 0.5);
                    const double kick = std::exp (-beat * 14.0) * std::sin (tw * (48.0 + 80.0 * std::exp (-beat * 35.0)) * beat);
                    double chord = 0;
                    for (double fr : { 196.0, 246.9, 293.7, 392.0 }) chord += 0.07 * std::sin (tw * fr * t);
                    const double v = 0.2 * (0.8 * kick + 0.25 * std::sin (tw * 49.0 * t) + chord + 0.05 * (rnd.nextFloat() * 2 - 1));
                    buf.setSample (0, i, (float) v); buf.setSample (1, i, (float) (v * 0.95));
                }
                juce::MidiBuffer m; proc.processBlock (buf, m);
                outPeak = std::max (outPeak, (double) std::max (buf.getMagnitude (0, 0, 1600), buf.getMagnitude (1, 0, 1600)));
                if (tick && f % 3 == 0) proc.autoMasterTick();
            }
        };
        play (30, false);
        proc.autoListen();
        int guard = 0;
        while (! proc.autoHasRecording() && ++guard < 2000) play (1, true);
        proc.autoMasterTick();
        while (proc.autoBusy()) juce::Thread::sleep (20);
        proc.autoMasterTick();
        const auto plan = proc.getAutoPlan();
        const bool kept = proc.getParametricSettings().in && proc.getClassASettings().in;
        play (30, true);                                             // let the meters settle after applying
        outPeak = 0;
        play (300, true);                                            // 10 s of playback, measured
        const double measured = proc.getVerifyLufs();
        std::printf ("  POP plan with Parametric %s, Class-A %s: predicted %.1f LUFS, measured %.1f (target %.1f); output peak %.2f dBFS (ceiling %.1f); units kept on: %s\n  %s\n",
                     plan.withParametric ? "on" : "OFF", plan.withClassA ? "on" : "OFF", plan.predictedLufs, measured, plan.targetLufs,
                     20 * std::log10 (outPeak), plan.ceiling, kept ? "yes" : "NO", proc.getAutoMessage().toRawUTF8());
        if (! plan.ok || ! plan.withParametric || ! plan.withClassA || ! kept
            || std::abs (measured - plan.predictedLufs) > 0.8 || std::abs (plan.predictedLufs - plan.targetLufs) > 0.5
            || 20 * std::log10 (outPeak) > plan.ceiling + 0.01) failCheck (failures, __LINE__);

        // a whole factory preset (footer menu) switches the Parametric EQ and Class-A out
        proc.setCurrentProgram (0);
        const bool reset = ! proc.getParametricSettings().in && ! proc.getClassASettings().in;
        std::printf ("  factory preset switches Parametric EQ and Class-A out: %s\n", reset ? "yes" : "NO");
        if (! reset) failCheck (failures, __LINE__);
    }

    std::printf ("%s\n", failures ? "FAILED" : "PASSED");
    return failures;
}
