// Offline checks for the DSP model (no JUCE needed):
//   g++ -O2 -std=c++17 -I../Source tests/model_test.cpp -o model_test && ./model_test
#include "dsp/TubeStage.h"
#include <cstdio>
#include <vector>
#include <random>

using namespace ppm;

static double goertzelDb (const std::vector<double>& h, double f, double fs)
{
    std::complex<double> acc = 0;
    for (size_t n = 0; n < h.size(); ++n)
        acc += h[n] * std::polar (1.0, -2.0 * kPi * f * double (n) / fs);
    return 20.0 * std::log10 (std::abs (acc) + 1e-30);
}

static std::vector<double> impulse (const ChannelSettings& c, double fs, size_t len)
{
    ChannelEngine e;
    e.prepare (fs);
    e.setSettings (c, false, 0.0);
    for (int i = 0; i < int (fs * 0.4); ++i) e.process (0.0);   // let smoothing settle
    std::vector<double> h (len);
    for (size_t n = 0; n < len; ++n) h[n] = e.process (n == 0 ? 1.0 : 0.0);
    return h;
}

static double maxDeviation (const ChannelSettings& c, double fs)
{
    auto h = impulse (c, fs, size_t (fs * 1.5));
    double worst = 0;
    for (double f = 20; f <= 20000; f *= 1.06)
        worst = std::max (worst, std::abs (goertzelDb (h, f, fs) - channelResponseDb (c, f)));
    return worst;
}

int main()
{
    const double fs = 176400.0;
    int failures = 0;

    // ---- 1. Control law: isolated band gains ---------------------------------
    std::printf ("== Isolated peak gain (analog model) ==\n");
    for (int shelf = 0; shelf < 2; ++shelf)
        for (int bw : { 0, 7, 15 })
        {
            ChannelSettings c; c.band[1].gainStep = 15; c.band[1].bwStep = bw; c.band[1].shelf = shelf; c.band[1].freqIndex = 5;
            for (int b : { 0, 2, 3 }) c.band[b].mode = Out;
            double peak = -100, f0 = kBandFreqs[1][5];
            for (double f = 20; f < 20000; f *= 1.01) peak = std::max (peak, channelResponseDb (c, f));
            std::printf ("  %s bw=%2d  max boost %+5.2f dB  (nominal %.2f, at %g Hz: %+5.2f, at 30 Hz: %+5.2f)\n",
                         shelf ? "SHELF" : "BELL ", bw, peak, bandNominalDb (c.band[1]), f0,
                         channelResponseDb (c, f0), channelResponseDb (c, 30));
        }

    // ---- 2. Parallel (sub-additive) behaviour --------------------------------
    {
        ChannelSettings c;
        c.band[0] = { 10, 15, 15, Boost, false };   // LOW 1 kHz, narrow, full
        c.band[2] = {  4, 15, 15, Boost, false };   // HIGH MID 1 kHz, narrow, full
        c.band[1].mode = c.band[3].mode = Out;
        std::printf ("\n== Two bands boosting 11 dB at 1 kHz: total %+5.2f dB (series EQ would give +22) ==\n",
                     channelResponseDb (c, 1000));
        c.band[2].mode = Cut;
        std::printf ("== 11 dB boost + 11 dB cut at 1 kHz: total %+5.2f dB ==\n", channelResponseDb (c, 1000));
    }

    // ---- 3. Digital vs analog match ------------------------------------------
    std::printf ("\n== Digital engine vs analog model (20 Hz - 20 kHz, fs = %.0f) ==\n", fs);
    std::mt19937 rng (1234);
    double worstMatch = 0;
    for (int trial = 0; trial < 40; ++trial)
    {
        ChannelSettings c;
        for (auto& b : c.band)
        {
            b.freqIndex = int (rng() % 11); b.gainStep = int (rng() % 16); b.bwStep = int (rng() % 16);
            b.mode = int (rng() % 3); b.shelf = (rng() % 2) == 0;
        }
        c.hp = int (rng() % 6); c.lp = int (rng() % 6); c.trim = int (rng() % 11);
        const double d = maxDeviation (c, fs);
        worstMatch = std::max (worstMatch, d);
    }
    std::printf ("  worst deviation over 40 random settings: %.3f dB\n", worstMatch);
    if (worstMatch > 0.5) { std::printf ("  FAIL\n"); ++failures; }

    // ---- 4. Stability at extremes --------------------------------------------
    std::printf ("\n== Stability (impulse tail energy, all extreme combos) ==\n");
    double worstTail = 0;
    for (int combo = 0; combo < 3 * 3 * 2 * 2 * 4; ++combo)
    {
        int k = combo;
        ChannelSettings c;
        for (int b = 0; b < 4; ++b)
        {
            c.band[b].gainStep = 15;
            c.band[b].mode     = (k + b) % 3;
            c.band[b].shelf    = ((k >> b) & 1) != 0;
            c.band[b].bwStep   = ((k / 3 + b) % 2) ? 15 : 0;
            c.band[b].freqIndex = (k * 7 + b * 3) % 11;
        }
        auto h = impulse (c, fs, size_t (fs * 2.0));
        double tail = 0; for (size_t n = h.size() - 4096; n < h.size(); ++n) tail += h[n] * h[n];
        worstTail = std::max (worstTail, tail);
        if (! std::isfinite (tail)) worstTail = 1e9;
    }
    std::printf ("  worst tail energy: %.3e %s\n", worstTail, worstTail < 1e-12 ? "(stable)" : "FAIL");
    if (! (worstTail < 1e-12)) ++failures;

    // ---- 5. Tube stage harmonics ----------------------------------------------
    std::printf ("\n== Tube stage, 1 kHz sine ==\n");
    for (double drive : { 0.0, 6.0, 12.0 })
        for (double levelDb : { -18.0, -6.0, 0.0 })
        {
            TubeStage t; t.prepare (fs); t.setDrive (drive); t.setEnabled (true);
            const double a = std::pow (10.0, levelDb / 20.0);
            const int N = int (fs);   // 1 s, integer cycles of 1 kHz
            std::vector<double> y ((size_t) N);
            for (int i = 0; i < N; ++i) t.process (a * std::sin (2 * kPi * 1000 * i / fs));  // warm-up
            for (int i = 0; i < N; ++i) y[(size_t) i] = t.process (a * std::sin (2 * kPi * 1000 * i / fs));
            auto amp = [&](double f) { std::complex<double> acc = 0;
                for (int i = 0; i < N; ++i) acc += y[(size_t) i] * std::polar (1.0, -2 * kPi * f * i / fs);
                return 2.0 * std::abs (acc) / N; };
            const double h1 = amp (1000), h2 = amp (2000), h3 = amp (3000);
            std::printf ("  drive %+4.1f dB, in %+5.1f dBFS: out %+6.2f dBFS  H2 %6.1f dBc  H3 %6.1f dBc\n",
                         drive, levelDb, 20 * std::log10 (h1), 20 * std::log10 (h2 / h1), 20 * std::log10 (h3 / h1));
        }

    std::printf ("\n%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return failures;
}
