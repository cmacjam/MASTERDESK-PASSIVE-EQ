// Key shifter + key detector checks (no JUCE):
//   g++ -O2 -std=c++17 -ISource tests/key_test.cpp -o key_test && ./key_test
#include "dsp/KeyShift.h"
#include "dsp/KeyDetect.h"
#include <cstdio>
#include <vector>
#include <random>
#include <string>

using namespace ppm;
static const double PI = 3.14159265358979323846;
static int failures = 0;
static void check (bool ok, const std::string& what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str()); if (! ok) ++failures; }

static std::string keyName (int key)
{
    if (key < 0) return "--";
    return std::string (pitchName (key % 12)) + (key >= 12 ? " minor" : " major");
}

// Peak frequency of a signal segment (Hann + parabolic interpolation on a 65536-point FFT)
static double peakFreq (const std::vector<float>& x, size_t start, double fs, double fmin = 20.0)
{
    const int order = 16, n = 1 << order;
    FFT f (order);
    std::vector<FFT::cpx> b ((size_t) n);
    for (int i = 0; i < n; ++i) b[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / n)) * x[start + (size_t) i];
    f.transform (b.data(), false);
    int kmax = 1; double vmax = 0;
    for (int k = (int) (fmin * n / fs); k < n / 2 - 1; ++k) if (std::abs (b[(size_t) k]) > vmax) { vmax = std::abs (b[(size_t) k]); kmax = k; }
    const double a = std::log (std::abs (b[(size_t) kmax - 1]) + 1e-30), m = std::log (vmax + 1e-30), c = std::log (std::abs (b[(size_t) kmax + 1]) + 1e-30);
    const double d = 0.5 * (a - c) / (a - 2 * m + c);
    return (kmax + d) * fs / n;
}

static double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (b - a));
}

// Run a stereo signal through the shifter
static void shift (std::vector<float>& l, std::vector<float>& r, double fs, double semis)
{
    KeyShifter ks; ks.prepare (fs); ks.setShift (semis);
    for (size_t pos = 0; pos < l.size(); pos += 512)
    {
        const int n = (int) std::min<size_t> (512, l.size() - pos);
        ks.process (l.data() + pos, r.data() + pos, n);
    }
}

// Synthesised chord progression: harmonic-rich chords + bass root, 1.5 s per chord
static std::vector<float> progression (double fs, const std::vector<std::vector<double>>& chords, int repeats)
{
    std::vector<float> y;
    const size_t perChord = (size_t) (fs * 1.5);
    double t = 0;
    for (int rep = 0; rep < repeats; ++rep)
        for (auto& ch : chords)
            for (size_t i = 0; i < perChord; ++i, t += 1.0 / fs)
            {
                const double env = std::min (1.0, (double) i / (fs * 0.01)) * std::exp (-(double) i / (fs * 2.5));
                double v = 0;
                for (size_t ni = 0; ni < ch.size(); ++ni)
                {
                    const double f0 = 440.0 * std::pow (2.0, (ch[ni] - 69.0) / 12.0);
                    for (int h = 1; h <= 6; ++h) v += (ni == 0 ? 0.35 : 0.12) / h * std::sin (2 * PI * f0 * h * t);
                }
                y.push_back ((float) (0.5 * env * v));
            }
    return y;
}

static KeyResult detect (const std::vector<float>& x, double fs)
{
    KeyDetector kd; kd.prepare (fs);
    const int n = kd.frameSize(), hop = (int) (fs / 30.0);          // as the GUI does: ~30 times a second
    for (size_t pos = (size_t) n; pos < x.size(); pos += (size_t) hop)
        kd.addFrame (x.data() + pos - (size_t) n, hop / fs);
    return kd.result();
}

int main()
{
    const double fs = 48000.0;

    std::printf ("== Key shifter: pitch accuracy (sine 440 Hz, 6 s) ==\n");
    for (double semis : { 0.0, 1.0, 2.0, -2.0, 7.0, -5.0, 12.0, -12.0, 0.5, -0.25 })
    {
        const size_t n = (size_t) (fs * 6);
        std::vector<float> l (n), r (n);
        for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float) (0.5 * std::sin (2 * PI * 440.0 * (double) i / fs));
        shift (l, r, fs, semis);
        const double want = 440.0 * std::pow (2.0, semis / 12.0);
        const double got = peakFreq (l, (size_t) (fs * 1.5), fs);
        const double lvl = 20 * std::log10 (rms (l, (size_t) (fs * 1.5), n) / (0.5 / std::sqrt (2.0)));
        const double cents = 1200.0 * std::log2 (got / want);
        char buf[160];
        std::snprintf (buf, sizeof buf, "%+6.2f semitones: %7.2f Hz (want %7.2f, %+5.2f cents), level %+5.2f dB", semis, got, want, cents, lvl);
        check (std::abs (cents) < 3.0 && std::abs (lvl) < 1.0, buf);
    }

    std::printf ("\n== Key shifter: transparency at 0 semitones ==\n");
    {
        const size_t n = (size_t) (fs * 3);
        std::mt19937 rng (3); std::normal_distribution<double> nd (0.0, 0.2);
        std::vector<float> l (n), r (n), l0, r0;
        for (size_t i = 0; i < n; ++i) { l[i] = (float) nd (rng); r[i] = (float) (0.5 * l[i] + nd (rng) * 0.5); }
        l0 = l; r0 = r;
        KeyShifter ks; ks.prepare (fs); ks.setShift (0.0);
        const int lat = ks.latency();
        shift (l, r, fs, 0.0);
        double err = 0, ref = 0;
        for (size_t i = (size_t) lat + 8192; i < n; ++i)
        {
            err += std::pow (l[i] - l0[i - (size_t) lat], 2) + std::pow (r[i] - r0[i - (size_t) lat], 2);
            ref += std::pow (l0[i - (size_t) lat], 2) + std::pow (r0[i - (size_t) lat], 2);
        }
        const double db = 10 * std::log10 (err / ref + 1e-30);
        std::printf ("  latency %d samples (%.1f ms at 48 kHz)\n", lat, 1000.0 * lat / fs);
        check (db < -90.0, "0 semitones reproduces the input exactly (error " + std::to_string ((int) db) + " dB)");
    }

    std::printf ("\n== Key shifter: chord, stereo image, noise ==\n");
    {
        const size_t n = (size_t) (fs * 6);
        // A minor triad, +3 semitones -> C minor triad
        std::vector<float> l (n), r (n);
        const double tri[3] = { 220.0, 261.63, 329.63 };
        for (size_t i = 0; i < n; ++i)
        {
            double v = 0; for (double f : tri) v += 0.2 * std::sin (2 * PI * f * (double) i / fs);
            l[i] = r[i] = (float) v;
        }
        shift (l, r, fs, 3.0);
        // check each partial by measuring with a band-limited search
        const double ratio = std::pow (2.0, 3.0 / 12.0);
        bool allOk = true;
        for (double f : tri)
        {
            const double want = f * ratio;
            std::vector<float> seg (l.begin() + (long) (fs * 1.5), l.end());
            // remove other partials' influence by searching near the expected frequency
            const int order = 16, nn = 1 << order; FFT ff (order);
            std::vector<FFT::cpx> b ((size_t) nn);
            for (int i = 0; i < nn; ++i) b[(size_t) i] = (0.5 - 0.5 * std::cos (2 * PI * i / nn)) * seg[(size_t) i];
            ff.transform (b.data(), false);
            int k0 = (int) (want * 0.97 * nn / fs), k1 = (int) (want * 1.03 * nn / fs), km = k0;
            for (int k = k0; k <= k1; ++k) if (std::abs (b[(size_t) k]) > std::abs (b[(size_t) km])) km = k;
            const double got = km * fs / nn;
            std::printf ("  partial %.2f Hz -> %.2f Hz (want %.2f)\n", f, got, want);
            allOk &= std::abs (1200.0 * std::log2 (got / want)) < 5.0;
        }
        check (allOk, "A minor triad shifted +3 lands on a C minor triad");

        // stereo: out-of-phase stays out of phase, one-sided stays one-sided
        std::vector<float> a (n), bR (n), c (n), d (n);
        for (size_t i = 0; i < n; ++i)
        {
            const float s = (float) (0.4 * std::sin (2 * PI * 330.0 * (double) i / fs) + 0.2 * std::sin (2 * PI * 554.0 * (double) i / fs));
            a[i] = s; bR[i] = -s; c[i] = s; d[i] = 0.0f;
        }
        shift (a, bR, fs, 2.0);
        shift (c, d, fs, -3.0);
        double lr = 0, ll = 0, rr = 0;
        for (size_t i = (size_t) fs; i < n; ++i) { lr += (double) a[i] * bR[i]; ll += (double) a[i] * a[i]; rr += (double) bR[i] * bR[i]; }
        const double corr = lr / std::sqrt (ll * rr);
        const double leak = 20 * std::log10 (rms (d, (size_t) fs, n) / rms (c, (size_t) fs, n) + 1e-12);
        check (corr < -0.99, "out-of-phase stereo keeps its phase (correlation " + std::to_string (corr).substr (0, 6) + ")");
        check (leak < -100.0, "left-only stays left-only (right channel " + std::to_string ((int) leak) + " dB)");

        // noise and parameter changes: stable, sane level
        std::mt19937 rng (9); std::uniform_real_distribution<double> u (-1.0, 1.0);
        std::vector<float> x (n), y (n);
        for (size_t i = 0; i < n; ++i) { x[i] = (float) (0.3 * u (rng)); y[i] = (float) (0.3 * u (rng)); }
        const double inLvl = rms (x, 0, n);
        KeyShifter ks; ks.prepare (fs);
        bool finite = true;
        for (size_t pos = 0; pos < n; pos += 512)
        {
            if (pos % 24000 < 512) ks.setShift ((double) ((int) (pos / 24000) % 9) * 3.0 - 12.0);   // a new setting every 0.5 s
            ks.process (x.data() + pos, y.data() + pos, (int) std::min<size_t> (512, n - pos));
        }
        for (size_t i = 0; i < n; ++i) finite &= std::isfinite (x[i]) && std::isfinite (y[i]) && std::abs (x[i]) < 4.0f;
        const double lvlDb = 20 * std::log10 (rms (x, (size_t) fs, n) / inLvl);
        check (finite && std::abs (lvlDb) < 3.0, "noise with the shift changed every 0.5 s (-12..+12) while playing: stable, level " + std::to_string (lvlDb).substr (0, 5) + " dB");
    }

    std::printf ("\n== Key shifter: level of noise and of a chord progression ==\n");
    for (double semis : { -7.0, -2.0, 2.0, 5.0 })
    {
        const size_t n = (size_t) (fs * 5);
        std::mt19937 rng (11); std::normal_distribution<double> nd (0.0, 0.15);
        std::vector<float> l (n), r (n);
        for (size_t i = 0; i < n; ++i) { l[i] = (float) nd (rng); r[i] = (float) nd (rng); }
        const double inL = rms (l, 0, n);
        shift (l, r, fs, semis);
        const double db = 20 * std::log10 (rms (l, (size_t) fs, n) / inL);
        check (std::abs (db) < 1.5, "white noise " + std::string (semis > 0 ? "+" : "") + std::to_string ((int) semis) + " semitones: level " + std::to_string (db).substr (0, 5) + " dB");
    }
    {
        auto x = progression (fs, { { 45, 64, 69, 72 }, { 38, 62, 65, 69 }, { 40, 64, 68, 71 } }, 2);
        for (double semis : { -3.0, 2.0 })
        {
            std::vector<float> l = x, r = x;
            shift (l, r, fs, semis);
            const double db = 20 * std::log10 (rms (l, (size_t) fs, l.size()) / rms (x, (size_t) fs, x.size()));
            check (std::abs (db) < 1.5, "chord progression " + std::string (semis > 0 ? "+" : "") + std::to_string ((int) semis) + " semitones: level " + std::to_string (db).substr (0, 5) + " dB");
        }
    }

    std::printf ("\n== Key shifter at 44.1 / 96 kHz ==\n");
    for (double rate : { 44100.0, 96000.0 })
    {
        const size_t n = (size_t) (rate * 5);
        std::vector<float> l (n), r (n);
        for (size_t i = 0; i < n; ++i) l[i] = r[i] = (float) (0.5 * std::sin (2 * PI * 440.0 * (double) i / rate));
        shift (l, r, rate, 2.0);
        const double got = peakFreq (l, (size_t) (rate * 1.5), rate);
        const double want = 440.0 * std::pow (2.0, 2.0 / 12.0);
        check (std::abs (1200 * std::log2 (got / want)) < 3.0, std::to_string ((int) rate) + " Hz: +2 semitones -> " + std::to_string (got).substr (0, 6) + " Hz");
    }

    std::printf ("\n== Key detection ==\n");
    // MIDI notes: A2=45, C3=48, ...
    struct Case { const char* name; std::vector<std::vector<double>> chords; int expect; };
    const std::vector<Case> cases = {
        { "C major  (C F G C)",      { { 48, 64, 67, 72 }, { 41, 65, 69, 72 }, { 43, 62, 67, 71 }, { 48, 64, 67, 72 } }, 0 },
        { "A minor  (Am Dm E Am)",   { { 45, 64, 69, 72 }, { 38, 62, 65, 69 }, { 40, 64, 68, 71 }, { 45, 64, 69, 72 } }, 12 + 9 },
        { "G major  (G C D G)",      { { 43, 62, 67, 71 }, { 48, 64, 67, 72 }, { 38, 62, 66, 69 }, { 43, 62, 67, 71 } }, 7 },
        { "E minor  (Em Am B7 Em)",  { { 40, 64, 67, 71 }, { 45, 64, 69, 72 }, { 47, 63, 66, 69 }, { 40, 64, 67, 71 } }, 12 + 4 },
        { "Bb major (Bb Eb F Bb)",   { { 46, 62, 65, 70 }, { 39, 63, 67, 70 }, { 41, 60, 65, 69 }, { 46, 62, 65, 70 } }, 10 },
    };
    for (auto& c : cases)
    {
        auto x = progression (fs, c.chords, 4);
        auto r = detect (x, fs);
        check (r.key == c.expect, std::string (c.name) + " -> " + keyName (r.key) + " (confidence " + std::to_string ((int) (r.confidence * 100)) + " %)");
    }

    std::printf ("\n== Detect after shifting ==\n");
    {
        auto x = progression (fs, cases[1].chords, 4);                 // A minor
        for (double semis : { 2.0, -3.0 })
        {
            std::vector<float> l = x, r = x;
            shift (l, r, fs, semis);
            auto res = detect (l, fs);
            const int expect = 12 + (((9 + (int) semis) % 12) + 12) % 12;
            check (res.key == expect, std::string ("A minor shifted ") + (semis > 0 ? "+" : "") + std::to_string ((int) semis) + " -> " + keyName (res.key)
                                      + " (expect " + keyName (expect) + ")");
        }
    }

    std::printf ("\n== Key info ==\n");
    check (camelotNumber (12 + 9) == 8 && camelotNumber (0) == 8 && camelotNumber (7) == 9 && camelotNumber (12 + 4) == 9, "Camelot: A minor 8A, C major 8B, G major 9B, E minor 9A");
    check (keySignature (9) == 3 && keySignature (5) == -1 && keySignature (12 + 4) == 1 && keySignature (10) == -2, "Key signatures: A major 3#, F major 1b, E minor 1#, Bb major 2b");

    std::printf ("\n%s\n", failures ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED");
    return failures;
}
