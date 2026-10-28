// Tests for the MAXIMIZER limiter and the auto-master analysis / EQ planner.
#include "dsp/Maximizer.h"
#include "dsp/AutoMaster.h"
#include <cstdio>
#include <random>
#include <chrono>

using namespace ppm;
static int failures = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what); if (! ok) ++failures; }

// pink-ish noise (Voss-McCartney style, good enough for octave bands)
struct Pink
{
    std::mt19937 rng { 7 };
    std::normal_distribution<double> nd { 0.0, 1.0 };
    double b[7] {};
    double next()
    {
        const double w = nd (rng);
        b[0] = 0.99886 * b[0] + w * 0.0555179; b[1] = 0.99332 * b[1] + w * 0.0750759;
        b[2] = 0.96900 * b[2] + w * 0.1538520; b[3] = 0.86650 * b[3] + w * 0.3104856;
        b[4] = 0.55000 * b[4] + w * 0.5329522; b[5] = -0.7616 * b[5] - w * 0.0168980;
        const double y = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362;
        b[6] = w * 0.115926;
        return y * 0.11;
    }
};

int main()
{
    std::printf ("MAXIMIZER\n");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        Maximizer m; m.prepare (fs);
        m.setParams (true, 12.0, -1.0);
        m.reset();
        Pink p; std::mt19937 rng (3);
        const int n = (int) (fs * 4);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double burst = (i / 4410) % 3 == 0 ? 0.9 * std::sin (i * 0.05) : 0.0;     // sudden loud tones
            l[(size_t) i] = (float) (p.next() * 0.5 + burst);
            r[(size_t) i] = (float) (p.next() * 0.5 - burst * 0.7);
        }
        m.process (l.data(), r.data(), n);
        double peak = 0; for (int i = 0; i < n; ++i) peak = std::max (peak, (double) std::max (std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i])));
        char msg[128];
        std::snprintf (msg, sizeof msg, "fs %.0f: +12 dB into -1 dB ceiling -> peak %.3f dBFS, latency %d (want <= -1.0)", fs, 20 * std::log10 (peak), m.latency());
        check (20 * std::log10 (peak) <= -0.999, msg);
    }
    {
        // off: untouched, just delayed by the look-ahead
        Maximizer m; m.prepare (48000.0); m.setParams (false, 6.0, -1.0); m.reset();
        std::vector<float> x (4800), y;
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) std::sin (i * 0.01) * 0.99f;
        y = x;
        m.process (y.data(), nullptr, (int) y.size());
        double err = 0; const int L = m.latency();
        for (size_t i = (size_t) L; i < y.size(); ++i) err = std::max (err, (double) std::fabs (y[i] - x[i - (size_t) L]));
        check (err < 1e-7, "switched off: output = input delayed by the look-ahead");

        // gain below the ceiling: exact gain, no limiting
        m.setParams (true, -6.0, -1.0); m.reset();
        y = x; for (auto& v : y) v *= 0.3f;
        m.process (y.data(), nullptr, (int) y.size());
        const double g = y[3000] / (x[3000 - (size_t) L] * 0.3);
        std::printf ("    quiet signal, gain -6 dB -> %.3f dB\n", 20 * std::log10 (std::fabs (g)));
        check (std::abs (20 * std::log10 (std::fabs (g)) + 6.0) < 0.05, "below the ceiling the gain is exact");
    }

    std::printf ("ANALYSIS\n");
    {
        const double fs = 48000.0; const int n = (int) (fs * 12);
        Pink p; std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i) { l[(size_t) i] = (float) p.next(); r[(size_t) i] = l[(size_t) i]; }
        auto s = automaster::measureSpectrum (l.data(), r.data(), n, fs);
        double lo = 1e9, hi = -1e9;
        std::printf ("    pink noise octave bands:");
        for (int b = 0; b < automaster::kOct; ++b) { std::printf (" %.1f", s.db[b] - s.db[5]); if (b >= 1 && b <= 8) { lo = std::min (lo, s.db[b]); hi = std::max (hi, s.db[b]); } }
        std::printf ("\n");
        check (s.ok && hi - lo < 2.0, "pink noise measures flat (63 Hz - 8 kHz within 2 dB)");
    }

    std::printf ("EQ PLAN\n");
    for (int g = 0; g < automaster::kNumGenres; ++g)
    {
        const auto& t = automaster::genre (g);
        // a mix that already matches the genre -> (almost) no EQ
        automaster::Spectrum s; s.ok = true;
        for (int b = 0; b < automaster::kOct; ++b) { s.db[b] = t.curve[b] - 20.0; s.valid[b] = true; }
        auto c0 = automaster::computeCorrection (s, g, 1.0);
        auto e0 = automaster::fitEq (c0, t.hp, t.lp);
        double maxDb = 0; for (auto& b : e0.band) maxDb = std::max (maxDb, std::abs (bandSignedDb (b)));

        // a mix that is 4 dB too bright above 4 kHz and 3 dB light in the bass
        for (int b = 0; b < automaster::kOct; ++b) s.db[b] += (b >= 7 ? 4.0 : 0.0) + (b <= 2 ? -3.0 : 0.0);
        const auto t0 = std::chrono::steady_clock::now();
        auto c = automaster::computeCorrection (s, g, 0.75);
        auto e = automaster::fitEq (c, t.hp, t.lp);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        ChannelSettings eqOnly = e; eqOnly.hp = 0; eqOnly.lp = 0;
        double worst = 0;
        for (int b = 1; b < automaster::kOct - 1; ++b)
            worst = std::max (worst, std::abs (channelResponseDb (eqOnly, automaster::kOctHz[b]) - c.wanted[b]));
        std::printf ("    %-8s matched mix: largest band %.2f dB | bright/thin mix: fit error %.2f dB (%.0f ms)\n      %s\n",
                     t.name, maxDb, worst, ms, automaster::describeEq (e).c_str());
        check (maxDb < 0.8, "mix already on target -> EQ stays (nearly) flat");
        check (worst < 1.2, "EQ fits the wanted correction within 1.2 dB (63 Hz - 8 kHz)");
        check (channelResponseDb (eqOnly, 8000) < -1.0 && channelResponseDb (eqOnly, 63) > 0.8, "bright/thin mix -> top comes down, bass comes up");
    }

    std::printf (failures == 0 ? "\nALL AUTO-MASTER TESTS PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
