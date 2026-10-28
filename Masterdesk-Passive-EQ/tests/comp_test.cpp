// Offline checks for the variable-mu compressor (no JUCE needed):
//   g++ -O2 -std=c++17 -ISource tests/comp_test.cpp -o comp_test && ./comp_test
#include "dsp/VariMu.h"
#include <cstdio>
#include <vector>
#include <random>

using namespace ppm;

static const double fs = 176400.0;
static int failures = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what); if (! ok) ++failures; }

static double rmsDb (const std::vector<double>& v, size_t from, size_t to)
{
    double a = 0; for (size_t i = from; i < to; ++i) a += v[i] * v[i];
    return 10.0 * std::log10 (a / double (to - from) + 1e-30);
}

// Run a sine of `levelDb` (peak dBFS) through the compressor; returns the gain change in dB after settling.
static double staticGain (CompSettings s, double levelDb, double freq = 1000.0)
{
    StereoVariMu c; c.setSettings (s, s, true); c.prepare (fs);
    const double a = std::pow (10.0, levelDb / 20.0);
    const size_t n = size_t (fs * 3.0);
    std::vector<double> in (n), out (n);
    for (size_t i = 0; i < n; ++i)
    {
        double l = a * std::sin (2 * kPi * freq * double (i) / fs), r = l;
        in[i] = l; c.process (l, r); out[i] = l;
    }
    return rmsDb (out, n - size_t (fs * 0.5), n) - rmsDb (in, n - size_t (fs * 0.5), n);
}

int main()
{
    std::printf ("== Static curve (threshold -18 dBFS, 1 kHz sine, SC HP on) ==\n");
    CompSettings s; s.threshold = 12; s.attack = 3; s.recovery = 0; s.output = 0; s.mix = 10; s.scHp = true;
    double worst = 0;
    for (int mode : { Compress, Limit })
    {
        s.mode = mode;
        for (double lvl : { -36.0, -24.0, -18.0, -12.0, -6.0, 0.0 })
        {
            const double g = staticGain (s, lvl);
            const double want = -variMuGainReductionDb (lvl - compThresholdDb (s.threshold), mode);
            worst = std::max (worst, std::abs (g - want));
            std::printf ("  %-8s in %+5.0f dBFS  gain %+6.2f dB  (curve %+6.2f)  out %+6.2f dBFS\n",
                         mode == Compress ? "COMPRESS" : "LIMIT", lvl, g, want, lvl + g);
        }
    }
    check (worst < 0.6, "measured gain reduction follows the static curve (< 0.6 dB)");
    check (std::abs (staticGain (s = CompSettings(), -20.0)) < 0.05, "default settings are transparent on normal material");

    {
        CompSettings c; c.threshold = 12;
        c.mode = Compress; const double gc = staticGain (c, -6.0);
        c.mode = Limit;    const double gl = staticGain (c, -6.0);
        check (gl < gc - 2.0, "LIMIT compresses harder than COMPRESS at the same level");
        // effective ratio of compress mode, 12 dB over threshold
        c.mode = Compress;
        const double g6 = staticGain (c, -6.0), g0 = staticGain (c, 0.0);
        const double ratio = 6.0 / (6.0 + (g0 - g6));
        std::printf ("  compress-mode slope between -6 and 0 dBFS: %.2f:1\n", ratio);
        check (ratio > 2.0 && ratio < 5.0, "compress mode ratio rises to ~2-4:1 when driven");
    }

    std::printf ("\n== Mix / bypass ==\n");
    {
        CompSettings c; c.threshold = 20; c.mix = 0; c.output = 12;
        StereoVariMu cm; cm.setSettings (c, c, true); cm.prepare (fs);
        double maxDiff = 0;
        for (int i = 0; i < int (fs); ++i)
        {
            const double x = 0.8 * std::sin (2 * kPi * 440 * i / fs);
            double l = x, r = x; cm.process (l, r); maxDiff = std::max (maxDiff, std::abs (l - x));
        }
        check (maxDiff < 1e-12, "MIX 0 % passes the signal untouched");
        c.mix = 10; c.compIn = false;
        StereoVariMu cb; cb.setSettings (c, c, true); cb.prepare (fs);
        maxDiff = 0;
        for (int i = 0; i < int (fs); ++i)
        {
            const double x = 0.8 * std::sin (2 * kPi * 440 * i / fs);
            double l = x, r = x; cb.process (l, r); maxDiff = std::max (maxDiff, std::abs (l - x));
        }
        check (maxDiff < 1e-12, "COMP OUT passes the signal untouched");
        c.compIn = true; c.mix = 5; c.output = 0;
        const double half = staticGain (c, -6.0);
        c.mix = 10;
        const double full = staticGain (c, -6.0);
        std::printf ("  -6 dBFS, threshold -30: mix 50 %% %+5.2f dB, mix 100 %% %+5.2f dB\n", half, full);
        check (half > full + 1.0, "MIX 50 % keeps more of the dry level (parallel compression)");
    }

    std::printf ("\n== Attack / recovery timing (threshold -18, step -40 -> -6 dBFS) ==\n");
    for (int att : { 1, 5, 9 })
        for (int rec : { 0, 3, 7 })
        {
            CompSettings c; c.threshold = 12; c.attack = att; c.recovery = rec; c.scHp = false;
            StereoVariMu cm; cm.setSettings (c, c, true); cm.prepare (fs);
            const double quiet = 0.01, loud = std::pow (10.0, -6.0 / 20.0);
            const double recSec = kRecoverySec[rec];
            const size_t nLoud = size_t (fs * 1.0), nQuiet = size_t (fs * (recSec * 2.0 + 0.5));
            double t63 = -1, grLoud = 0;
            std::vector<double> grTrace;
            size_t i = 0;
            for (; i < size_t (fs * 0.5); ++i) { double l = quiet * std::sin (2 * kPi * 1000 * double (i) / fs), r = l; cm.process (l, r); }
            cm.takeGainReductionDb (0);
            const double target = variMuGainReductionDb (-6.0 + 18.0, Compress);
            for (size_t k = 0; k < nLoud; ++k, ++i)
            {
                double l = loud * std::sin (2 * kPi * 1000 * double (i) / fs), r = l; cm.process (l, r);
                if (k % 64 == 0)
                {
                    const double g = cm.takeGainReductionDb (0);
                    if (t63 < 0 && g >= 0.632 * target) t63 = double (k) / fs;
                    grLoud = g;
                }
            }
            double t95 = -1;
            for (size_t k = 0; k < nQuiet; ++k, ++i)
            {
                double l = quiet * std::sin (2 * kPi * 1000 * double (i) / fs), r = l; cm.process (l, r);
                if (k % 64 == 63)
                {
                    const double g = cm.takeGainReductionDb (0);
                    if (t95 < 0 && g <= 0.05 * grLoud) t95 = double (k) / fs;
                }
            }
            const double attMs = kAttackMs[att];
            std::printf ("  attack %2.0f ms -> 63%% in %5.1f ms   recovery %.1f s -> 95%% back in %.2f s\n",
                         attMs, t63 * 1000.0, recSec, t95);
            if (! (t63 > 0 && t63 * 1000.0 < attMs * 2.0 + 6.0 && t63 * 1000.0 > attMs * 0.5)) ++failures, std::printf ("  [FAIL] attack timing\n");
            if (! (t95 > recSec * 0.6 && t95 < recSec * 1.5 + 0.02)) ++failures, std::printf ("  [FAIL] recovery timing\n");
        }

    std::printf ("\n== Stereo link and sidechain filter ==\n");
    for (bool link : { true, false })
    {
        CompSettings c; c.threshold = 12;
        StereoVariMu cm; cm.setSettings (c, c, link); cm.prepare (fs);
        for (int i = 0; i < int (fs * 2); ++i)
        {
            double l = 0.5 * std::sin (2 * kPi * 1000 * i / fs), r = 0.01 * std::sin (2 * kPi * 1000 * i / fs);
            cm.process (l, r);
        }
        const double gl = cm.takeGainReductionDb (0), gr = cm.takeGainReductionDb (1);
        std::printf ("  %s: loud left GR %.2f dB, quiet right GR %.2f dB\n", link ? "LINKED  " : "UNLINKED", gl, gr);
        check (link ? std::abs (gl - gr) < 0.01 : gr < 0.05, link ? "linked: both sides get the same gain reduction"
                                                                 : "unlinked: the quiet side is left alone");
    }
    {
        CompSettings c; c.threshold = 12; c.scHp = true;
        const double withHp = staticGain (c, -6.0, 40.0);
        c.scHp = false;
        const double noHp = staticGain (c, -6.0, 40.0);
        std::printf ("  40 Hz bass at -6 dBFS: SC HP on %+5.2f dB, off %+5.2f dB\n", withHp, noHp);
        check (withHp > noHp + 3.0, "SC HP stops bass from driving the compressor");
    }

    std::printf ("\n== Tube colour and stability ==\n");
    {
        auto h2 = [] (int thr)
        {
            CompSettings c; c.threshold = thr; c.scHp = false;
            StereoVariMu cm; cm.setSettings (c, c, true); cm.prepare (fs);
            const int N = int (fs);
            std::vector<double> y ((size_t) N);
            for (int i = 0; i < N; ++i) { double l = 0.5 * std::sin (2 * kPi * 1000 * i / fs), r = l; cm.process (l, r); }
            for (int i = 0; i < N; ++i) { double l = 0.5 * std::sin (2 * kPi * 1000 * i / fs), r = l; cm.process (l, r); y[(size_t) i] = l; }
            auto amp = [&] (double f) { std::complex<double> a = 0; for (int i = 0; i < N; ++i) a += y[(size_t) i] * std::polar (1.0, -2 * kPi * f * i / fs); return std::abs (a); };
            return 20.0 * std::log10 (amp (2000) / amp (1000));
        };
        const double clean = h2 (0), squeezed = h2 (16);
        std::printf ("  H2 at 0 dB GR: %6.1f dBc   at heavy GR: %6.1f dBc\n", clean, squeezed);
        check (squeezed > clean + 10.0 && squeezed < -30.0, "2nd harmonic grows with gain reduction but stays subtle");

        std::mt19937 rng (7);
        std::uniform_real_distribution<double> u (-1.0, 1.0);
        bool finite = true; double peak = 0;
        for (int trial = 0; trial < 20; ++trial)
        {
            CompSettings c; c.threshold = int (rng() % 21); c.attack = int (rng() % 10); c.recovery = int (rng() % 8);
            c.output = int (rng() % 25); c.mix = int (rng() % 11); c.mode = int (rng() % 2); c.scHp = rng() % 2;
            StereoVariMu cm; cm.setSettings (c, c, rng() % 2); cm.prepare (fs);
            for (int i = 0; i < int (fs * 0.3); ++i)
            {
                double l = u (rng) * 2.0, r = u (rng) * 2.0;   // +6 dBFS noise
                cm.process (l, r);
                finite &= std::isfinite (l) && std::isfinite (r);
                peak = std::max (peak, std::abs (l));
            }
        }
        std::printf ("  random settings, +6 dBFS noise: finite=%s, peak %.2f\n", finite ? "yes" : "NO", peak);
        check (finite, "stable with any settings");
    }

    std::printf ("\n%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return failures;
}
