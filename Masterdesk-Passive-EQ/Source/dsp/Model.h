// =============================================================================
//  Model.h  —  control law + analog model of a parallel passive mastering EQ
//
//  Plain C++17, no JUCE dependency, so it can be unit-tested on its own and is
//  shared by the audio engine (PassiveNetwork.h) and the GUI response curve.
//
//  THE CIRCUIT IDEA
//  ----------------
//  The hardware is a passive *parallel* network followed by tube make-up gain.
//  We model it as a single-node voltage divider:
//
//        Vin ──[ Ys ]──┬── Vo ──► (tube make-up, x K)
//                      │
//                    [ Yp ]
//                      │
//                     GND
//
//    Ys = Gs + Σ (boost branches)      series arm   – more admittance = boost
//    Yp = Gp + Σ (cut branches)        shunt arm    – more admittance = cut
//
//    H(s) = K · Ys / (Ys + Yp),   K = (Gs+Gp)/Gs  (the insertion loss the
//                                                  tube stage makes up)
//
//  Each band is a resonant admittance (series RLC = band-pass for bells,
//  2nd-order low/high-pass shapes for shelves). Because everything lands in
//  one divider, the bands are genuinely parallel and *sub-additive*: two bands
//  boosting the same spot share the same limited headroom (Ys can't exceed
//  "fully open"), exactly the behaviour Manley describes ("if another band is
//  boosting near that frequency, very little will happen"). Resonant shelf
//  shapes put a negative real part into the arm just past the corner, which
//  produces the classic "Pultec" dip/bump when the shelf bandwidth is narrow.
// =============================================================================
#pragma once
#include <cmath>
#include <complex>
#include <algorithm>

namespace ppm
{
constexpr double kPi = 3.14159265358979323846;

constexpr int kNumBands      = 4;
constexpr int kNumFreqSteps  = 11;   // Grayhill frequency switch
constexpr int kNumKnobSteps  = 16;   // detented GAIN and BANDWIDTH knobs
constexpr int kNumFilterPos  = 6;    // OFF + 5 filters
constexpr int kNumTrimSteps  = 11;   // -2.5 .. +2.5 dB in 0.5 dB

// --- Hardware switch positions ----------------------------------------------
inline constexpr double kBandFreqs[kNumBands][kNumFreqSteps] = {
    {   22,   33,   47,   68,  100,  150,  220,   330,   470,   680,  1000 }, // LOW
    {   82,  120,  180,  270,  390,  560,  820,  1200,  1800,  2700,  3900 }, // LOW MID
    {  220,  330,  470,  680, 1000, 1500, 2200,  3300,  4700,  6800, 10000 }, // HIGH MID
    {  560,  820, 1200, 1800, 2700, 3900, 5600,  8200, 12000, 16000, 27000 }, // HIGH
};
inline constexpr const char* kBandNames[kNumBands] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };

// Mastering-version filters (hardware switch order)
inline constexpr double kHpFreqs[kNumFilterPos] = { 0, 12, 16, 23, 30, 39 };            // 18 dB/oct
inline constexpr double kLpFreqs[kNumFilterPos] = { 0, 52000, 40000, 27000, 20000, 15000 };
inline constexpr int    kHpOrder[kNumFilterPos] = { 0, 3, 3, 3, 3, 3 };
inline constexpr int    kLpOrder[kNumFilterPos] = { 0, 5, 3, 3, 3, 3 };                  // 52k = 30 dB/oct

inline double trimDb (int step) { return -2.5 + 0.5 * step; }

enum Mode  { Boost = 0, Out = 1, Cut = 2 };
enum Shape { BandPass = 0, LowPass = 1, HighPass = 2 };

struct BandSettings
{
    int  freqIndex = 5;
    int  gainStep  = 0;      // 0..15, 0 = no change
    int  bwStep    = 7;      // 0..15, 0 = fully CCW (wide), 15 = fully CW (narrow)
    int  mode      = Boost;
    bool shelf     = false;
};

struct ChannelSettings
{
    BandSettings band[kNumBands];
    int  hp    = 0;
    int  lp    = 0;
    int  trim  = 5;          // index -> 0 dB
    bool eqIn  = true;
};

// --- Network constants -------------------------------------------------------
constexpr double kInsertionK = 16.0;           // ~24 dB of passive loss, made up by tubes
constexpr double kGs = 1.0;
constexpr double kGp = kInsertionK - 1.0;      // so (Gs+Gp)/Gs == K

inline bool bandShelfIsLow (int bandIndex) { return bandIndex < 2; } // two left bands = low shelf

// --- Control law: gain/bandwidth interaction ---------------------------------
// Mastering version: max ±11 dB. Widest bell tops out near 6 dB; narrow shelf
// tops out lower than a wide shelf (same proportions as the original unit).
inline double bwNorm (int bwStep) { return std::clamp (bwStep, 0, kNumKnobSteps - 1) / double (kNumKnobSteps - 1); }

inline double bandMaxDb (const BandSettings& b)
{
    const double n = bwNorm (b.bwStep);
    return b.shelf ? 11.0 - 4.4 * n      // wide shelf 11 dB  -> narrow shelf ~6.6 dB
                   :  6.0 + 5.0 * n;     // wide bell   6 dB  -> narrow bell  11 dB
}

// Nominal (isolated) gain of the band in dB, >= 0. Detents are slightly
// finer near zero, which is where mastering moves live.
inline double bandNominalDb (const BandSettings& b)
{
    const double g = std::clamp (b.gainStep, 0, kNumKnobSteps - 1) / double (kNumKnobSteps - 1);
    return bandMaxDb (b) * std::pow (g, 1.25);
}

inline double bandSignedDb (const BandSettings& b)
{
    if (b.mode == Out) return 0.0;
    const double d = bandNominalDb (b);
    return b.mode == Cut ? -d : d;
}

inline double bandQ (const BandSettings& b)
{
    const double n = bwNorm (b.bwStep);
    return b.shelf ? 0.55 * std::pow (2.9, n)    // 0.55 (smooth) .. 1.6 (Pultec dip/bump)
                   : 0.50 * std::pow (6.0, n);   // 0.5 (broad)  .. 3.0 (narrow)
}

// One band resolved to circuit terms.
struct BranchTarget
{
    Shape  shape   = BandPass;
    bool   series  = true;    // true = series arm (boost), false = shunt arm (cut)
    double G       = 0.0;     // peak / asymptotic admittance of the branch
    double freqHz  = 1000.0;
    double Q       = 0.707;
};

inline BranchTarget computeBranch (int bandIndex, const BandSettings& b)
{
    BranchTarget t;
    t.freqHz = kBandFreqs[bandIndex][std::clamp (b.freqIndex, 0, kNumFreqSteps - 1)];
    t.Q      = bandQ (b);
    t.shape  = b.shelf ? (bandShelfIsLow (bandIndex) ? LowPass : HighPass) : BandPass;
    t.series = (b.mode != Cut);

    const double db = (b.mode == Out) ? 0.0 : bandNominalDb (b);
    const double B  = std::pow (10.0, db / 20.0);

    if (db <= 1e-9)        t.G = 0.0;
    else if (t.series)     t.G = kInsertionK * (B - 1.0) / (kInsertionK - B);      // boost B in isolation
    else                   t.G = kInsertionK * (B - 1.0);                          // cut 1/B in isolation
    return t;
}

// Normalised analog shape response N(jw)/D(jw), peak/asymptote = 1.
inline std::complex<double> shapeResponse (Shape shape, double w, double w0, double Q)
{
    const std::complex<double> s (0.0, w);
    const std::complex<double> D = s * s + (w0 / Q) * s + w0 * w0;
    switch (shape)
    {
        case BandPass: return (w0 / Q) * s / D;
        case LowPass:  return (w0 * w0) / D;
        case HighPass: return (s * s) / D;
    }
    return 0.0;
}

inline std::complex<double> butterworthResponse (bool highPass, int order, double fc, double f)
{
    if (order <= 0 || fc <= 0.0) return 1.0;
    std::complex<double> h = 1.0;
    const std::complex<double> s (0.0, f / fc); // normalised
    for (int k = 1; k <= order; ++k)
    {
        const std::complex<double> pk = std::polar (1.0, kPi * (2.0 * k + order - 1.0) / (2.0 * order));
        // LP prototype pole pk; HP via s -> 1/s (poles lie on the unit circle,
        // so 1/pk = conj(pk) and the pole set is unchanged).
        if (highPass) h *= s / (s - pk);
        else          h *= -pk / (s - pk);
    }
    return h;
}

// Analog magnitude of the whole channel (for the GUI curve).
inline double channelResponseDb (const ChannelSettings& c, double f)
{
    if (! c.eqIn) return trimDb (c.trim);
    const double w = 2.0 * kPi * f;
    std::complex<double> Ys = kGs, Yp = kGp;
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto t = computeBranch (b, c.band[b]);
        if (t.G <= 0.0) continue;
        const auto y = t.G * shapeResponse (t.shape, w, 2.0 * kPi * t.freqHz, t.Q);
        (t.series ? Ys : Yp) += y;
    }
    std::complex<double> h = kInsertionK * Ys / (Ys + Yp);
    h *= butterworthResponse (true,  kHpOrder[c.hp], kHpFreqs[c.hp], f);
    h *= butterworthResponse (false, kLpOrder[c.lp], kLpFreqs[c.lp], f);
    return 20.0 * std::log10 (std::max (std::abs (h), 1e-9)) + trimDb (c.trim);
}
} // namespace ppm
