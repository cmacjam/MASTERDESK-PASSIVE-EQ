// =============================================================================
//  Maximizer.h  —  final look-ahead brickwall limiter ("MAXIMIZER")
//
//  GAIN pushes the level up, CEILING is the highest sample peak allowed out.
//   * Look-ahead 1.5 ms: the gain starts coming down before a peak arrives.
//   * The needed gain is held (sliding minimum over the look-ahead window), then
//     released smoothly (program-dependent: fast after short peaks, slower when
//     limiting is continuous), then averaged over the look-ahead window. The
//     average of values that are all <= the needed gain is itself <= it, so a
//     peak can never get through: the output never exceeds CEILING.
//   * Both channels get the same gain (no image shift).
//   * Off = the signal passes untouched, but delayed by the same 1.5 ms, so
//     the plug-in's latency doesn't change when you switch it.
//  Plain C++17; tested in tests/automaster_test.cpp.
// =============================================================================
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace ppm
{
class Maximizer
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        L = std::max (1, (int) std::lround (0.0015 * fs));
        delay[0].assign ((size_t) L + 1, 0.0f);
        delay[1].assign ((size_t) L + 1, 0.0f);
        box.assign ((size_t) L, 1.0);
        dqVal.assign ((size_t) L + 2, 1.0);
        dqIdx.assign ((size_t) L + 2, 0);
        relFast = 1.0 - std::exp (-1.0 / (0.040 * fs));
        relSlow = 1.0 - std::exp (-1.0 / (0.400 * fs));
        avgCoef = 1.0 - std::exp (-1.0 / (0.300 * fs));
        rampStep = 1.0 / (0.020 * fs);
        reset();
    }

    void reset()
    {
        std::fill (delay[0].begin(), delay[0].end(), 0.0f);
        std::fill (delay[1].begin(), delay[1].end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0);
        boxSum = (double) L; dqHead = dqTail = 0; pos = 0; n = 0;
        env = 1.0; grAvg = 0.0; peakGr = 0.0;
        cur = tgt;
    }

    int latency() const { return L; }

    void setParams (bool on, double gainDb, double ceilingDb)
    {
        tgt.on   = on ? 1.0 : 0.0;
        tgt.gain = std::pow (10.0, gainDb / 20.0);
        tgt.ceil = std::pow (10.0, std::min (0.0, ceilingDb) / 20.0);
    }

    // In place. right may be nullptr (mono).
    void process (float* left, float* right, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            // smooth parameter changes (20 ms)
            cur.gain += (tgt.gain - cur.gain) * 0.002;
            cur.ceil += (tgt.ceil - cur.ceil) * 0.002;
            if (cur.on < tgt.on) cur.on = std::min (tgt.on, cur.on + rampStep);
            else if (cur.on > tgt.on) cur.on = std::max (tgt.on, cur.on - rampStep);

            const float xl = left[i], xr = right != nullptr ? right[i] : xl;
            const double g = 1.0 + cur.on * (cur.gain - 1.0);
            const double pk = std::max (std::fabs (xl), std::fabs (xr)) * g;
            const double need = pk > cur.ceil ? cur.ceil / pk : 1.0;

            // sliding minimum of `need` over the last L+1 samples (monotonic queue)
            const int cap = (int) dqVal.size();
            while (dqTail != dqHead && dqVal[(size_t) ((dqTail - 1 + cap) % cap)] >= need) dqTail = (dqTail - 1 + cap) % cap;
            dqVal[(size_t) dqTail] = need; dqIdx[(size_t) dqTail] = n; dqTail = (dqTail + 1) % cap;
            while (dqIdx[(size_t) dqHead] < n - L) dqHead = (dqHead + 1) % cap;
            const double held = dqVal[(size_t) dqHead];

            // release: fast for short peaks, slower while limiting is sustained
            if (held < env) env = held;
            else
            {
                const double rel = relSlow + (relFast - relSlow) * std::clamp (1.0 - grAvg / 3.0, 0.0, 1.0);
                env += (held - env) * rel;
            }
            grAvg += avgCoef * ((1.0 - env) * 20.0 - grAvg);    // rough dB-ish measure of how busy it is

            // average over the look-ahead window -> smooth gain that is always <= need
            boxSum += env - box[(size_t) (n % L)];
            box[(size_t) (n % L)] = env;
            const double gain = std::min (1.0, boxSum / L);

            // delay the audio by L samples
            const int sz = L + 1;
            delay[0][(size_t) pos] = xl; delay[1][(size_t) pos] = xr;
            const int rd = (pos + 1) % sz;                     // oldest = written L samples ago
            pos = rd;
            const double dl = delay[0][(size_t) rd], dr = delay[1][(size_t) rd];

            const double c = cur.ceil;
            double yl = dl * g * gain, yr = dr * g * gain;
            yl = std::clamp (yl, -c, c); yr = std::clamp (yr, -c, c);   // only matters while CEILING is moving
            left[i] = (float) (dl + cur.on * (yl - dl));
            if (right != nullptr) right[i] = (float) (dr + cur.on * (yr - dr));

            peakGr = std::max (peakGr, cur.on * -20.0 * std::log10 (std::max (gain, 1e-6)));
            if (++n >= (1 << 30)) rebase();
        }
    }

    // Largest gain reduction since the last call, dB.
    double takeGainReductionDb() { const double v = peakGr; peakGr = 0.0; return v; }

private:
    void rebase()
    {
        // keep indices small; the queue and box keep their relative positions
        const int shift = n - (n % L);
        const int cap = (int) dqVal.size();
        for (int k = dqHead; k != dqTail; k = (k + 1) % cap) dqIdx[(size_t) k] -= shift;
        n -= shift;
        boxSum = 0.0; for (double v : box) boxSum += v;      // also clears float drift
    }

    struct P { double on = 0.0, gain = 1.0, ceil = 0.891; };
    P cur, tgt;
    double fs = 48000.0;
    int L = 72;
    std::vector<float> delay[2];
    std::vector<double> box, dqVal;
    std::vector<int> dqIdx;
    int dqHead = 0, dqTail = 0, pos = 0, n = 0;
    double boxSum = 72.0, env = 1.0, grAvg = 0.0, peakGr = 0.0;
    double relFast = 0.001, relSlow = 0.0001, avgCoef = 0.0001, rampStep = 0.001;
};
} // namespace ppm
