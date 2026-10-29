// Tests for the parametric mastering EQ (dsp/SontecEQ.h)
#include "dsp/SontecEQ.h"
#include "dsp/FFT.h"
#include <cstdio>
#include <string>
#include <vector>

using namespace ppm::sontec;
static int failures = 0;
static void check (bool ok, const std::string& what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str()); if (! ok) ++failures; }
static std::string f2 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.2f", v); return b; }
static const double PI = 3.14159265358979323846;

// gain of a sine through the EQ at sample rate fs
static double measure (const Settings& st, double fs, double freq, double amp = 0.05)
{
    SontecEQ eq; eq.setSettings (st); eq.prepare (fs); eq.setSettings (st);
    const int n = (int) (fs * 0.5);
    double ei = 0, eo = 0;
    for (int i = 0; i < n; ++i)
    {
        const double v = amp * std::sin (2 * PI * freq * i / fs);
        double l = v, r = v;
        eq.process (l, r);
        if (i > n / 2) { ei += v * v; eo += l * l; }
    }
    return 10 * std::log10 (eo / ei);
}

int main()
{
    std::printf ("== controls ==\n");
    check (levelDb (12) == 0.0 && levelDb (13) == 0.5 && levelDb (18) == 3.0 && levelDb (19) == 4.0 && levelDb (24) == 9.0 && levelDb (0) == -9.0,
           "LEVEL: +/-9 dB, 0.5 dB steps to 3 dB then 1 dB steps");
    check (bandFreqs (0).front() == 11 && bandFreqs (0).back() == 570 && bandFreqs (1).front() == 125 && bandFreqs (1).back() == 6800
           && bandFreqs (2).front() == 3400 && bandFreqs (2).back() == 25000, "FREQ: LO 11-570 Hz, MID 125-6800 Hz, HI 3.4-25 kHz");

    std::printf ("== gain / bandwidth interlock ==\n");
    {
        const double wide = interlockGainDb (12.0, kShapeQ[0]), narrow = interlockGainDb (12.0, kShapeQ[4]);
        check (std::abs (wide - 12.0) < 1e-9 && narrow < 12.0 - 1.2 && narrow > 12.0 - 1.5,
               "12 dB move: widest SHAPE " + f2 (wide) + " dB, narrowest " + f2 (narrow) + " dB (1.2-1.5 dB less)");
    }

    std::printf ("== digital curve = analog curve (at the plug-in's 4x rate) ==\n");
    {
        Settings st; st.in = true;
        auto& c = st.ch[0];
        c.band[0] = { 13, 1, 24 };          // LO 100 Hz, wide, +9
        c.band[1] = { 12, 4, 0 };           // MID 1 kHz, narrow, -9
        c.band[2] = { 22, 2, 22 };          // HI 25 kHz, +7
        c.hiShelfLevel = 18; c.loShelfLevel = 6;
        st.ch[1] = c;
        double worst = 0;
        for (double f : { 30.0, 100.0, 300.0, 1000.0, 3000.0, 8000.0, 15000.0, 20000.0 })
        {
            const double got = measure (st, 192000.0, f), want = channelResponseDb (c, f);
            worst = std::max (worst, std::abs (got - want));
        }
        check (worst < 0.05, "worst difference 30 Hz - 20 kHz: " + f2 (worst) + " dB (25 kHz air bell keeps its analog shape)");
        // what pre-warping alone (no oversampling) would do to the same 25 kHz bell at 48 kHz: shown, not required
        Settings hi; hi.in = true; hi.ch[0].band[2] = { 22, 2, 24 }; hi.ch[1] = hi.ch[0];
        std::printf ("    (for reference: +9 dB at 25 kHz, measured at 18 kHz: analog %s dB, 4x rate %s dB, plain 48 kHz %s dB)\n",
                     f2 (channelResponseDb (hi.ch[0], 18000)).c_str(), f2 (measure (hi, 192000.0, 18000)).c_str(), f2 (measure (hi, 48000.0, 18000)).c_str());
    }

    std::printf ("== op-amp output stage ==\n");
    {
        Settings st; st.in = true;          // EQ flat: only the op-amp stage acts
        auto spectrum = [&] (double amp, double& h2, double& h3, double& maxErr)
        {
            SontecEQ eq; eq.setSettings (st); eq.prepare (48000.0); eq.setSettings (st);
            const int N = 1 << 16; const int k = 683;   // bin-exact frequency
            std::vector<ppm::FFT::cpx> b ((size_t) N);
            maxErr = 0;
            for (int i = 0; i < 2 * N; ++i)
            {
                const double v = amp * std::sin (2 * PI * k * i / N);
                double l = v, r = v; eq.process (l, r);
                if (i >= N) { b[(size_t) (i - N)] = l; maxErr = std::max (maxErr, std::abs (l - v)); }
            }
            ppm::FFT f (16); f.transform (b.data(), false);
            const double a1 = std::abs (b[(size_t) k]);
            h2 = 20 * std::log10 (std::abs (b[(size_t) (2 * k)]) / a1 + 1e-30);
            h3 = 20 * std::log10 (std::abs (b[(size_t) (3 * k)]) / a1 + 1e-30);
        };
        double h2, h3, err;
        spectrum (std::pow (10.0, -12.0 / 20.0), h2, h3, err);
        check (err == 0.0, "-12 dBFS: completely linear (bit-exact)");
        spectrum (0.999, h2, h3, err);
        check (h2 > -88 && h2 < -82 && h3 < -95, "0 dBFS: 2nd harmonic " + f2 (h2) + " dB, 3rd " + f2 (h3) + " dB (soft 2nd at ~-85, minuscule 3rd)");
    }

    std::printf ("== switches ==\n");
    {
        Settings st; st.in = true; st.ch[0].band[1] = { 12, 2, 24 }; st.ch[1] = st.ch[0];
        st.ch[1].in = false;                                  // right channel OFF
        SontecEQ eq; eq.setSettings (st); eq.prepare (48000.0); eq.setSettings (st);
        double errR = 0;
        for (int i = 0; i < 20000; ++i) { const double v = 0.1 * std::sin (i * 0.13); double l = v, r = v; eq.process (l, r); errR = std::max (errR, std::abs (r - v)); }
        check (errR == 0.0, "channel OFF: that channel untouched");
        st.in = false;
        SontecEQ off; off.setSettings (st); off.prepare (48000.0); off.setSettings (st);
        double err = 0;
        for (int i = 0; i < 20000; ++i) { const double v = 0.9 * std::sin (i * 0.13); double l = v, r = v; off.process (l, r); err = std::max (err, std::abs (l - v)); }
        check (err == 0.0, "unit out (PUSH EQ off): untouched");
    }

    std::printf (failures == 0 ? "\nALL PARAMETRIC EQ TESTS PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
