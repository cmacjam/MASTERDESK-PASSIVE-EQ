// =============================================================================
//  SontecEQ.h  —  parametric mastering EQ (the black "PARAMETRIC MASTERING
//                 EQUALIZER" unit, in front of the passive EQ)
//
//  Laid out and scaled after the classic MES-432C/9 mastering equalizer
//  (an independent model, not affiliated with Sontec):
//   * Per channel: SHELVING (HI shelf, LO shelf with 50 / 100 Hz switch) and
//     three parametric bands LO 11-570 Hz, MID 125-6800 Hz, HI 3.4-25 kHz,
//     each with SHAPE (5 / 6 / 9 / 11 / 15 dB/oct, wide -> narrow), FREQ
//     (stepped detents) and LEVEL (+/-9 dB: 0.5 dB steps to 3 dB, then 1 dB).
//   * Gain / bandwidth interlock (as specified for this model): the narrower
//     the SHAPE, the less peak gain - full gain at the widest setting, about
//     1.35 dB less (for a 12 dB move; in proportion for smaller ones) at the
//     narrowest.
//   * Output stage modelled as a high-headroom discrete op-amp: completely
//     linear below -10 dBFS; above that an asymmetric polynomial adds soft
//     2nd harmonic (about -85 dB at 0 dBFS) and a minuscule 3rd.
//   * Filters are bilinear with frequency and bandwidth pre-warping and run at the plug-in's
//     oversampled rate (4x at 44.1 / 48 kHz), so a 25 kHz bell or the high
//     shelf keeps its true analog shape instead of being squashed at Nyquist.
//  Plain C++17; tested in tests/sontec_test.cpp.
// =============================================================================
#pragma once
#include <cmath>
#include <complex>
#include <algorithm>
#include <vector>

namespace ppm::sontec
{
constexpr double kPiS = 3.14159265358979323846;
constexpr int kBands = 3;

// stepped FREQ detents (read from the front panel scales)
inline const std::vector<double>& bandFreqs (int band)
{
    static const std::vector<double> f[kBands] = {
        { 11, 13, 16, 19, 23, 27, 31, 36, 42, 50, 60, 72, 85, 100, 120, 140, 170, 200, 240, 290, 340, 400, 480, 570 },
        { 125, 150, 180, 215, 250, 300, 360, 420, 500, 600, 700, 840, 1000, 1200, 1400, 1700, 2000, 2400, 2800, 3400, 4000, 4800, 5600, 6800 },
        { 3400, 3700, 4000, 4400, 4800, 5200, 5700, 6200, 6800, 7400, 8000, 8800, 9600, 10500, 11500, 12500, 13500, 15000, 17000, 19000, 21000, 23000, 25000 },
    };
    return f[std::clamp (band, 0, kBands - 1)];
}
inline const char* kBandNames[kBands] = { "LO", "MID", "HI" };

constexpr int kNumShapes = 5;
inline constexpr double kShapeDbOct[kNumShapes] = { 5, 6, 9, 11, 15 };     // printed SHAPE scale
inline constexpr double kShapeQ[kNumShapes]     = { 0.45, 0.6, 1.2, 1.9, 4.0 };

constexpr int kNumLevels = 25;      // index 12 = 0 dB
inline double levelDb (int idx)
{
    static const double v[13] = { 0, 0.5, 1, 1.5, 2, 2.5, 3, 4, 5, 6, 7, 8, 9 };
    const int k = std::clamp (idx, 0, kNumLevels - 1) - 12;
    return k >= 0 ? v[k] : -v[-k];
}

// narrower SHAPE -> a little less peak gain (about 1.35 dB less for a 12 dB move at the narrowest setting)
inline double interlockGainDb (double gainDb, double Q)
{
    const double t = std::clamp (std::log (Q / kShapeQ[0]) / std::log (kShapeQ[kNumShapes - 1] / kShapeQ[0]), 0.0, 1.0);
    return gainDb * (1.0 - 0.1125 * t);
}

constexpr double kShelfQ = 0.6;     // gentle, musical shelves
inline double hiShelfHz (bool alt) { return alt ? 12000.0 : 8000.0; }
inline double loShelfHz (bool hundred) { return hundred ? 100.0 : 50.0; }

struct BandSettings { int freqIdx = 11; int shape = 2; int level = 12; };
struct ChannelSettings
{
    BandSettings band[kBands];
    int  hiShelfLevel = 12, loShelfLevel = 12;
    bool hiShelf12k = false, loShelf100 = true;
    bool in = true;                  // MAN (in) / OFF for this channel
};
struct Settings
{
    bool in = false;                 // PUSH EQ: the whole unit in / out
    ChannelSettings ch[2];
};

// ---- analog prototypes (magnitude, for the GUI and tests) ---------------------------
inline std::complex<double> bellH (double f, double f0, double Q, double gDb)
{
    const double A = std::pow (10.0, gDb / 40.0);
    const std::complex<double> s (0.0, f / f0);
    return (s * s + s * (A / Q) + 1.0) / (s * s + s / (A * Q) + 1.0);
}
inline std::complex<double> shelfH (double f, double f0, double Q, double gDb, bool high)
{
    const double A = std::pow (10.0, gDb / 40.0), sq = std::sqrt (A);
    const std::complex<double> s (0.0, f / f0);
    if (high) return A * (A * s * s + sq / Q * s + 1.0) / (s * s + sq / Q * s + A);
    return A * (s * s + sq / Q * s + A) / (A * s * s + sq / Q * s + 1.0);
}
inline double channelResponseDb (const ChannelSettings& c, double f)
{
    if (! c.in) return 0.0;
    std::complex<double> h = 1.0;
    for (int b = 0; b < kBands; ++b)
    {
        const auto& bs = c.band[b];
        const double Q = kShapeQ[std::clamp (bs.shape, 0, kNumShapes - 1)];
        const double g = interlockGainDb (levelDb (bs.level), Q);
        if (std::abs (g) > 1e-9) h *= bellH (f, bandFreqs (b)[(size_t) std::clamp (bs.freqIdx, 0, (int) bandFreqs (b).size() - 1)], Q, g);
    }
    if (c.hiShelfLevel != 12) h *= shelfH (f, hiShelfHz (c.hiShelf12k), kShelfQ, levelDb (c.hiShelfLevel), true);
    if (c.loShelfLevel != 12) h *= shelfH (f, loShelfHz (c.loShelf100), kShelfQ, levelDb (c.loShelfLevel), false);
    return 20.0 * std::log10 (std::abs (h));
}

// ---- digital: pre-warped bilinear biquads ------------------------------------------
struct SBiquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
    bool active = false;
    inline double process (double x) { if (! active) return x; const double y = b0 * x + s1; s1 = b1 * x - a1 * y + s2; s2 = b2 * x - a2 * y; return y; }
    void reset() { s1 = s2 = 0; }
    // analog H(s) = (n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0) with s normalised to w0; pre-warped at w0
    void setAnalog (double n2, double n1, double n0, double d2, double d1, double d0, double f0, double fs)
    {
        const double K = 1.0 / std::tan (kPiS * std::min (f0, fs * 0.49) / fs);
        const double a0 = d2 * K * K + d1 * K + d0;
        b0 = (n2 * K * K + n1 * K + n0) / a0;
        b1 = 2.0 * (n0 - n2 * K * K) / a0;
        b2 = (n2 * K * K - n1 * K + n0) / a0;
        a1 = 2.0 * (d0 - d2 * K * K) / a0;
        a2 = (d2 * K * K - d1 * K + d0) / a0;
        active = true;
    }
    void setBell (double f0, double Q, double gDb, double fs)
    {
        if (std::abs (gDb) < 1e-9) { active = false; return; }
        const double A = std::pow (10.0, gDb / 40.0);
        // the bilinear transform squeezes bandwidth near Nyquist: widen the prototype to match the analog curve
        const double w0 = 2.0 * kPiS * std::min (f0, fs * 0.49) / fs;
        const double Qd = Q * std::pow (std::sin (w0) / w0, 0.7);
        setAnalog (1.0, A / Qd, 1.0, 1.0, 1.0 / (A * Qd), 1.0, f0, fs);
    }
    void setShelf (double f0, double Q, double gDb, bool high, double fs)
    {
        if (std::abs (gDb) < 1e-9) { active = false; return; }
        const double A = std::pow (10.0, gDb / 40.0), sq = std::sqrt (A);
        if (high) setAnalog (A * A, A * sq / Q, A, 1.0, sq / Q, A, f0, fs);
        else      setAnalog (A, A * sq / Q, A * A, A, sq / Q, 1.0, f0, fs);
    }
};

// High-headroom discrete op-amp: linear below -10 dBFS, soft asymmetric rounding above
struct OpAmpStage
{
    static constexpr double kT = 0.31622776601683794;     // -10 dBFS
    static constexpr double kC2 = 2.3e-4, kC3 = 0.3 * 2.3e-4;
    double dcX = 0, dcY = 0, dcCoef = 0.9999;
    void prepare (double fs) { dcCoef = std::exp (-2.0 * kPiS * 3.0 / fs); dcX = dcY = 0; }
    inline double process (double x)
    {
        const double d = std::abs (x) - kT;
        if (d <= 0.0 && std::abs (dcY) < 1e-15) { dcX = 0.0; return x; }                // completely linear
        const double h = d > 0.0 ? kC2 * d * d - std::copysign (kC3 * d * d * d, x) : 0.0;  // 2nd (asymmetric) + 3rd
        // the even term carries a little DC: remove it (from the added harmonics only)
        const double y = h - dcX + dcCoef * dcY;
        dcX = h; dcY = y;
        return x + y;
    }
};

class SontecEQ
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rampStep = 1.0 / (0.020 * fs);
        for (auto& o : opamp) o.prepare (fs);
        dirty = true; design();
        for (auto& ch : f) for (auto& b : ch) b.reset();
        onNow = s.in ? 1.0 : 0.0;
        for (int c = 0; c < 2; ++c) chOn[c] = s.ch[c].in ? 1.0 : 0.0;
    }

    void setSettings (const Settings& n)
    {
        for (int c = 0; c < 2; ++c)
        {
            const auto &a = n.ch[c], &b = s.ch[c];
            bool same = a.hiShelfLevel == b.hiShelfLevel && a.loShelfLevel == b.loShelfLevel && a.hiShelf12k == b.hiShelf12k && a.loShelf100 == b.loShelf100;
            for (int k = 0; k < kBands; ++k)
                same = same && a.band[k].freqIdx == b.band[k].freqIdx && a.band[k].shape == b.band[k].shape && a.band[k].level == b.band[k].level;
            if (! same) dirty = true;
        }
        s = n;
        if (dirty) design();
    }

    inline void process (double& l, double& r)
    {
        const double target = s.in ? 1.0 : 0.0;
        if (onNow < target) onNow = std::min (target, onNow + rampStep);
        else if (onNow > target) onNow = std::max (target, onNow - rampStep);
        if (onNow <= 0.0) { for (auto& ch : f) for (auto& b : ch) b.reset(); return; }
        double* x[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const double t = s.ch[c].in ? 1.0 : 0.0;
            if (chOn[c] < t) chOn[c] = std::min (t, chOn[c] + rampStep);
            else if (chOn[c] > t) chOn[c] = std::max (t, chOn[c] - rampStep);
            const double dry = *x[c];
            double y = dry;
            for (auto& b : f[c]) y = b.process (y);
            y = opamp[c].process (y);
            const double w = onNow * chOn[c];
            *x[c] = dry + w * (y - dry);
        }
    }

private:
    void design()
    {
        for (int c = 0; c < 2; ++c)
        {
            const auto& cs = s.ch[c];
            for (int b = 0; b < kBands; ++b)
            {
                const auto& bs = cs.band[b];
                const auto& fr = bandFreqs (b);
                const double Q = kShapeQ[std::clamp (bs.shape, 0, kNumShapes - 1)];
                f[c][b].setBell (fr[(size_t) std::clamp (bs.freqIdx, 0, (int) fr.size() - 1)], Q, interlockGainDb (levelDb (bs.level), Q), fs);
            }
            f[c][3].setShelf (hiShelfHz (cs.hiShelf12k), kShelfQ, levelDb (cs.hiShelfLevel), true, fs);
            f[c][4].setShelf (loShelfHz (cs.loShelf100), kShelfQ, levelDb (cs.loShelfLevel), false, fs);
        }
        dirty = false;
    }

    Settings s;
    SBiquad f[2][5];
    OpAmpStage opamp[2];
    double fs = 192000.0, rampStep = 0.001, onNow = 0.0, chOn[2] { 1.0, 1.0 };
    bool dirty = true;
};
} // namespace ppm::sontec
