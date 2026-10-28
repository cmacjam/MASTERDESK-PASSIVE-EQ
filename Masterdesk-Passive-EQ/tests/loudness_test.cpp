// Loudness meter conformance checks, based on the EBU Tech 3341 / 3342 test cases:
//   g++ -O2 -std=c++17 -ISource tests/loudness_test.cpp -o loudness_test && ./loudness_test
#include "dsp/Loudness.h"
#include <cstdio>
#include <vector>

using ppm::LoudnessMeter;
static int failures = 0;
static void check (bool ok, const char* what, double got, double want, double tol)
{
    std::printf ("  [%s] %-58s %+7.2f (expect %+.1f +/- %.1f)\n", ok ? " ok " : "FAIL", what, got, want, tol);
    if (! ok) ++failures;
}

struct Segment { double seconds, levelDbfs, freq = 1000.0; };

// Stereo sine sequence (both channels), returns the final readout.
static LoudnessMeter::Readout run (double fs, const std::vector<Segment>& segs, bool mono = false)
{
    LoudnessMeter m; m.prepare (fs);
    std::vector<float> buf (512);
    double phase = 0.0;
    for (auto& s : segs)
    {
        const double a = std::pow (10.0, s.levelDbfs / 20.0);
        long n = (long) std::lround (s.seconds * fs);
        while (n > 0)
        {
            const int k = (int) std::min<long> (n, 512);
            for (int i = 0; i < k; ++i) { buf[(size_t) i] = (float) (a * std::sin (phase)); phase += 2 * 3.14159265358979323846 * s.freq / fs; }
            m.process (buf.data(), mono ? nullptr : buf.data(), k);
            n -= k;
        }
    }
    return m.readout();
}

int main()
{
    std::printf ("== EBU Tech 3341 (loudness) ==\n");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto r = run (fs, { { 20.0, -23.0 } });
        char what[96];
        std::snprintf (what, sizeof what, "case 1: 1 kHz -23 dBFS, %.1f kHz  integrated", fs / 1000.0);
        check (std::abs (r.integrated + 23.0) <= 0.1, what, r.integrated, -23.0, 0.1);
        std::snprintf (what, sizeof what, "        momentary / short-term at %.1f kHz", fs / 1000.0);
        check (std::abs (r.momentary + 23.0) <= 0.1 && std::abs (r.shortTerm + 23.0) <= 0.1, what, r.shortTerm, -23.0, 0.1);
    }
    {
        auto r = run (48000.0, { { 20.0, -33.0 } });
        check (std::abs (r.integrated + 33.0) <= 0.1, "case 2: 1 kHz -33 dBFS  integrated", r.integrated, -33.0, 0.1);
        r = run (48000.0, { { 10, -36 }, { 60, -23 }, { 10, -36 } });
        check (std::abs (r.integrated + 23.0) <= 0.1, "case 3: -36 / -23 / -36 (relative gate)", r.integrated, -23.0, 0.1);
        r = run (48000.0, { { 10, -72 }, { 10, -36 }, { 60, -23 }, { 10, -36 }, { 10, -72 } });
        check (std::abs (r.integrated + 23.0) <= 0.1, "case 4: -72/-36/-23/-36/-72 (absolute + relative gate)", r.integrated, -23.0, 0.1);
        r = run (48000.0, { { 20, -26 }, { 20.1, -20 }, { 20, -26 } });
        check (std::abs (r.integrated + 23.0) <= 0.1, "case 5: -26 / -20 / -26", r.integrated, -23.0, 0.1);
        check (std::abs (r.shortTermMax + 20.0) <= 0.1, "        short-term max", r.shortTermMax, -20.0, 0.1);
        check (std::abs (r.momentaryMax + 20.0) <= 0.1, "        momentary max", r.momentaryMax, -20.0, 0.1);
        r = run (48000.0, { { 20.0, -23.0 } }, true);
        check (std::abs (r.integrated + 26.0) <= 0.1, "mono: one channel 1 kHz -23 dBFS", r.integrated, -26.0, 0.1);
    }

    std::printf ("\n== EBU Tech 3342 (loudness range) ==\n");
    {
        auto r = run (48000.0, { { 20, -20 }, { 20, -30 } });
        check (std::abs (r.lra - 10.0) <= 1.0, "case 1: -20 / -30", r.lra, 10.0, 1.0);
        r = run (48000.0, { { 20, -20 }, { 20, -15 } });
        check (std::abs (r.lra - 5.0) <= 1.0, "case 2: -20 / -15", r.lra, 5.0, 1.0);
        r = run (48000.0, { { 20, -40 }, { 20, -20 } });
        check (std::abs (r.lra - 20.0) <= 1.0, "case 3: -40 / -20", r.lra, 20.0, 1.0);
        r = run (48000.0, { { 20, -50 }, { 20, -35 }, { 20, -20 }, { 20, -35 }, { 20, -50 } });
        check (std::abs (r.lra - 15.0) <= 1.0, "case 4: -50/-35/-20/-35/-50", r.lra, 15.0, 1.0);
    }

    std::printf ("\n== True peak ==\n");
    {
        // fs/4 sine with 45 degree phase: every sample lands at 0.707 of the real peak
        const double fs = 48000.0;
        LoudnessMeter m; m.prepare (fs);
        std::vector<float> buf (480);
        long idx = 0; double samplePeak = 0;
        for (int blk = 0; blk < 300; ++blk)
        {
            for (int i = 0; i < 480; ++i, ++idx)
            {
                buf[(size_t) i] = (float) (0.5 * std::sin (2 * 3.14159265358979323846 * 0.25 * (double) idx + 3.14159265358979323846 / 4));
                samplePeak = std::max (samplePeak, (double) std::abs (buf[(size_t) i]));
            }
            m.process (buf.data(), buf.data(), 480);
        }
        std::printf ("  sample peak %.2f dBFS\n", 20 * std::log10 (samplePeak));
        check (std::abs (m.readout().truePeakMax + 6.02) <= 0.3, "fs/4 sine, 45 deg phase (sample peak -9.03)", m.readout().truePeakMax, -6.0, 0.3);
        auto r = run (48000.0, { { 5, -6.0, 997.0 } });
        check (std::abs (r.truePeakMax + 6.0) <= 0.1, "997 Hz sine at -6 dBFS", r.truePeakMax, -6.0, 0.1);
        r = run (48000.0, { { 5, -6.0, 997.0 }, { 5, -20.0, 997.0 } });
        check (std::abs (r.truePeak3s + 20.0) <= 0.1, "3 s true peak follows the signal down", r.truePeak3s, -20.0, 0.1);
        check (std::abs (r.truePeakMax + 6.0) <= 0.1, "true peak max holds the loudest part", r.truePeakMax, -6.0, 0.1);
    }

    std::printf ("\n== Reset ==\n");
    {
        LoudnessMeter m; m.prepare (48000.0);
        std::vector<float> buf (4800, 0.5f);
        for (int i = 0; i < 50; ++i) m.process (buf.data(), buf.data(), 4800);
        m.reset();
        check (m.readout().integrated < -100.0 && m.readout().truePeakMax < -100.0, "reset clears integrated and maxima", m.readout().integrated, -200.0, 1.0);
    }

    std::printf ("\n%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return failures;
}
