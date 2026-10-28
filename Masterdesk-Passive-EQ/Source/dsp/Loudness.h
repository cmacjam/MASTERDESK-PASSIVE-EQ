// =============================================================================
//  Loudness.h  —  ITU-R BS.1770-4 / EBU R128 loudness meter
//
//   * K-weighting (pre-filter shelf + RLB high-pass), designed for any sample rate
//   * Momentary  (400 ms window)  and  Short-term (3 s window), updated every 100 ms
//   * Integrated: 400 ms blocks with 75 % overlap, absolute gate -70 LUFS,
//     relative gate -10 LU (BS.1770-4)
//   * Loudness Range (EBU Tech 3342): short-term values, abs gate -70, relative
//     gate -20 LU, 10th to 95th percentile
//   * True peak: 4x oversampled (windowed-sinc polyphase FIR), in dBTP
//   * Maxima: momentary max, short-term max, true-peak max, and true-peak over
//     the last 3 s (for PSR)
//
//  Gated statistics use 0.1 LU histograms, so memory is fixed and nothing is
//  allocated on the audio thread. Plain C++17; tested in tests/loudness_test.cpp.
// =============================================================================
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <vector>

namespace ppm
{
class LoudnessMeter
{
public:
    static constexpr double kSilence = -200.0;     // "no reading yet"

    struct Readout
    {
        double momentary = kSilence, shortTerm = kSilence, integrated = kSilence, lra = 0.0;
        double momentaryMax = kSilence, shortTermMax = kSilence;
        double truePeakMax = kSilence;     // dBTP, since reset
        double truePeak3s  = kSilence;     // dBTP, max over the last 3 s
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        designKWeighting();
        designTruePeakFilter();
        subLen = std::max (1, (int) std::lround (fs * 0.1));
        reset();
    }

    void reset()
    {
        for (auto& c : ch) { c.s1a = c.s2a = c.s1b = c.s2b = 0.0; c.hist.fill (0.0); c.histPos = 0; }
        subSum = 0.0; subCount = 0; subPeak = 0.0;
        ring.fill (0.0); peakRing.fill (0.0); ringFill = 0; ringPos = 0;
        histI.fill (0); histS.fill (0); histIE.fill (0.0);
        out = Readout();
    }

    // Feed one block. `right` may be nullptr for mono.
    void process (const float* left, const float* right, int n)
    {
        const int nCh = right != nullptr ? 2 : 1;
        for (int i = 0; i < n; ++i)
        {
            double e = 0.0, pk = 0.0;
            for (int c = 0; c < nCh; ++c)
            {
                const double x = c == 0 ? left[i] : right[i];
                const double y = kWeight (ch[c], x);
                e += y * y;
                pk = std::max (pk, truePeakSample (ch[c], x));
            }
            subSum += e;
            subPeak = std::max (subPeak, pk);
            if (++subCount >= subLen) endSubBlock();
        }
    }

    const Readout& readout() const { return out; }

    static double energyToLufs (double meanSquare) { return meanSquare > 1e-20 ? -0.691 + 10.0 * std::log10 (meanSquare) : kSilence; }

private:
    // ---- histograms: 0.1 LU bins from -70 to +10 LUFS --------------------------
    static constexpr double kHistMin = -70.0, kHistStep = 0.1;
    static constexpr int    kHistBins = 800;
    using Histogram = std::array<unsigned int, kHistBins>;
    static int    binOf (double lufs)   { return std::clamp ((int) std::floor ((lufs - kHistMin) / kHistStep), 0, kHistBins - 1); }
    static double centreOf (int b)      { return kHistMin + kHistStep * (b + 0.5); }
    static double energyOf (double l)   { return std::pow (10.0, (l + 0.691) / 10.0); }

    // ---- per-channel state -------------------------------------------------------
    static constexpr int kTpPhases = 4, kTpTaps = 16;          // 64-tap 4x interpolator
    struct Channel
    {
        double s1a = 0, s2a = 0, s1b = 0, s2b = 0;              // K-weighting biquad states (TDF-II)
        std::array<double, kTpTaps> hist {};                   // true-peak FIR history
        int histPos = 0;
    };
    Channel ch[2];

    // K-weighting coefficients (stage A: shelf, stage B: high-pass)
    double ab0 = 1, ab1 = 0, ab2 = 0, aa1 = 0, aa2 = 0;
    double bb0 = 1, bb1 = -2, bb2 = 1, ba1 = 0, ba2 = 0;

    inline double kWeight (Channel& c, double x) const
    {
        double y = ab0 * x + c.s1a;
        c.s1a = ab1 * x - aa1 * y + c.s2a;
        c.s2a = ab2 * x - aa2 * y;
        const double z = bb0 * y + c.s1b;
        c.s1b = bb1 * y - ba1 * z + c.s2b;
        c.s2b = bb2 * y - ba2 * z;
        return z;
    }

    void designKWeighting()
    {
        // Analog prototypes from BS.1770 (same parameters as libebur128); exact at 48 kHz, valid at any rate
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (3.14159265358979323846 * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            ab0 = (Vh + Vb * K / Q + K * K) / a0;
            ab1 = 2.0 * (K * K - Vh) / a0;
            ab2 = (Vh - Vb * K / Q + K * K) / a0;
            aa1 = 2.0 * (K * K - 1.0) / a0;
            aa2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (3.14159265358979323846 * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            bb0 = 1.0; bb1 = -2.0; bb2 = 1.0;
            ba1 = 2.0 * (K * K - 1.0) / a0;
            ba2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    // ---- true peak -------------------------------------------------------------------
    std::array<std::array<double, kTpTaps>, kTpPhases> tpCoef {};

    void designTruePeakFilter()
    {
        // Kaiser-windowed sinc low-pass at the original Nyquist, split into 4 phases
        const int N = kTpPhases * kTpTaps;
        const double beta = 7.0, centre = (N - 1) / 2.0;
        auto i0 = [] (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        for (int k = 0; k < N; ++k)
        {
            const double t = (k - centre) / kTpPhases;
            const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (3.14159265358979323846 * t) / (3.14159265358979323846 * t);
            const double r = (k - centre) / centre;
            const double w = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
            tpCoef[(size_t) (k % kTpPhases)][(size_t) (k / kTpPhases)] = sinc * w;
        }
        // normalise each phase to unity DC gain
        for (auto& ph : tpCoef)
        {
            double s = 0; for (double v : ph) s += v;
            for (double& v : ph) v /= s;
        }
    }

    inline double truePeakSample (Channel& c, double x) const
    {
        c.hist[(size_t) c.histPos] = x;
        double pk = std::abs (x);
        for (int p = 0; p < kTpPhases; ++p)
        {
            double acc = 0.0;
            int idx = c.histPos;
            for (int t = 0; t < kTpTaps; ++t)
            {
                acc += tpCoef[(size_t) p][(size_t) t] * c.hist[(size_t) idx];
                idx = idx == 0 ? kTpTaps - 1 : idx - 1;
            }
            pk = std::max (pk, std::abs (acc));
        }
        c.histPos = (c.histPos + 1) % kTpTaps;
        return pk;
    }

    // ---- 100 ms sub-blocks ---------------------------------------------------------
    double fs = 48000.0;
    int    subLen = 4800, subCount = 0;
    double subSum = 0.0, subPeak = 0.0;
    std::array<double, 30> ring {};        // mean-square energy of the last 30 sub-blocks (3 s)
    std::array<double, 30> peakRing {};    // true peak of the last 30 sub-blocks
    int ringFill = 0, ringPos = 0;
    Histogram histI {}, histS {};
    std::array<double, kHistBins> histIE {};   // exact energy per integrated bin
    Readout out;

    double meanOfLast (int nBlocks) const
    {
        double s = 0.0;
        for (int k = 0; k < nBlocks; ++k) s += ring[(size_t) ((ringPos - 1 - k + 60) % 30)];
        return s / nBlocks;
    }

    void endSubBlock()
    {
        ring[(size_t) ringPos] = subSum / subLen;
        peakRing[(size_t) ringPos] = subPeak;
        ringPos = (ringPos + 1) % 30;
        ringFill = std::min (ringFill + 1, 30);
        subSum = 0.0; subCount = 0; subPeak = 0.0;

        // true peak
        double pk3 = 0.0;
        for (int k = 0; k < ringFill; ++k) pk3 = std::max (pk3, peakRing[(size_t) ((ringPos - 1 - k + 60) % 30)]);
        out.truePeak3s = pk3 > 1e-10 ? 20.0 * std::log10 (pk3) : kSilence;
        out.truePeakMax = std::max (out.truePeakMax, out.truePeak3s);

        // momentary (400 ms) and integrated gating blocks (400 ms, every 100 ms)
        if (ringFill >= 4)
        {
            out.momentary = energyToLufs (meanOfLast (4));
            out.momentaryMax = std::max (out.momentaryMax, out.momentary);
            if (out.momentary >= -70.0)
            {
                const int b = binOf (out.momentary);
                histI[(size_t) b]++; histIE[(size_t) b] += meanOfLast (4);
                out.integrated = gatedIntegrated();
            }
        }
        // short-term (3 s) and loudness range
        if (ringFill >= 30)
        {
            out.shortTerm = energyToLufs (meanOfLast (30));
            out.shortTermMax = std::max (out.shortTermMax, out.shortTerm);
            if (out.shortTerm >= -70.0) { histS[(size_t) binOf (out.shortTerm)]++; out.lra = loudnessRange(); }
        }
    }

    double gatedIntegrated() const
    {
        double e = 0.0; unsigned long n = 0;
        for (int b = 0; b < kHistBins; ++b) { e += histIE[(size_t) b]; n += histI[(size_t) b]; }
        if (n == 0) return kSilence;
        const double relGate = energyToLufs (e / (double) n) - 10.0;
        e = 0.0; n = 0;
        // bins entirely above the gate (the bin containing the gate is within 0.1 LU of it)
        for (int b = binOf (relGate); b < kHistBins; ++b)
            if (centreOf (b) >= relGate) { e += histIE[(size_t) b]; n += histI[(size_t) b]; }
        return n ? energyToLufs (e / (double) n) : kSilence;
    }

    double loudnessRange() const
    {
        double e = 0.0; unsigned long n = 0;
        for (int b = 0; b < kHistBins; ++b) if (histS[(size_t) b]) { e += histS[(size_t) b] * energyOf (centreOf (b)); n += histS[(size_t) b]; }
        if (n == 0) return 0.0;
        const double relGate = energyToLufs (e / (double) n) - 20.0;
        unsigned long total = 0;
        const int g0 = binOf (relGate) + (centreOf (binOf (relGate)) < relGate ? 1 : 0);
        for (int b = g0; b < kHistBins; ++b) total += histS[(size_t) b];
        if (total == 0) return 0.0;
        auto percentile = [&] (double p)
        {
            const double target = p * (double) (total - 1);
            unsigned long cum = 0;
            for (int b = g0; b < kHistBins; ++b)
            {
                cum += histS[(size_t) b];
                if ((double) cum > target) return centreOf (b);
            }
            return centreOf (kHistBins - 1);
        };
        return std::max (0.0, percentile (0.95) - percentile (0.10));
    }
};
} // namespace ppm
