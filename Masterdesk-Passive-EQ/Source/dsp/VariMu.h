// =============================================================================
//  VariMu.h  —  stereo variable-mu tube compressor (mastering style)
//
//  How a variable-mu compressor behaves, and how it's modelled here:
//   * The gain element is a remote-cutoff tube whose gain ("mu") falls as its
//     grid bias rises. That gives a very soft knee and a ratio that keeps
//     increasing the harder you push: gentle ~1.5:1 at the knee, ~3-4:1 deep
//     in compression. LIMIT mode tightens the knee and raises the ratio.
//   * Gain changes are smoothed by ATTACK (time constant) and RECOVERY
//     (release; the hardware's stepped 0.2 - 8 s positions).
//   * The detector can ignore lows (SC HP 100 Hz) so the bass doesn't pump
//     the whole mix.
//   * LINK drives both channels from the louder side so the stereo image
//     never shifts.
//   * The tube adds a little 2nd harmonic that grows with gain reduction.
//   * MIX blends the compressed signal with the uncompressed one (parallel
//     compression); OUTPUT is make-up gain on the compressed path.
//
//  Plain C++17, no JUCE: unit-tested in tests/model_test.cpp.
// =============================================================================
#pragma once
#include "PassiveNetwork.h"

namespace ppm
{
// --- stepped control positions -------------------------------------------------
constexpr int kNumThreshSteps = 21;   // 0 .. -30 dB in 1.5 dB steps
constexpr int kNumAttackSteps = 10;
constexpr int kNumRecoverySteps = 8;
constexpr int kNumOutputSteps = 25;   // 0 .. +12 dB in 0.5 dB steps
constexpr int kNumMixSteps    = 11;   // 0 .. 100 % in 10 % steps

inline constexpr double kAttackMs[kNumAttackSteps]     = { 2, 5, 10, 15, 20, 25, 35, 45, 55, 70 };
inline constexpr double kRecoverySec[kNumRecoverySteps] = { 0.2, 0.4, 0.6, 0.8, 1.2, 2.0, 4.0, 8.0 };

inline double compThresholdDb (int step) { return step <= 0 ? 0.0 : -1.5 * (double) std::min (step, kNumThreshSteps - 1); }
inline double compOutputDb    (int step) { return  0.5 * (double) std::clamp (step, 0, kNumOutputSteps - 1); }
inline double compMix         (int step) { return  0.1 * (double) std::clamp (step, 0, kNumMixSteps - 1); }

enum CompMode { Compress = 0, Limit = 1 };

struct CompSettings
{
    bool compIn    = true;
    int  threshold = 0;        // index: 0 = 0 dB (no compression on normal material)
    int  attack    = 5;        // 25 ms
    int  recovery  = 3;        // 0.8 s
    int  output    = 0;        // 0 dB make-up
    int  mix       = 10;       // 100 %
    int  mode      = Compress;
    bool scHp      = true;
};

// Static curve: dB of gain reduction for a detector level `overDb` above threshold.
inline double variMuGainReductionDb (double overDb, int mode)
{
    const double w  = mode == Limit ? 2.0 : 4.0;                       // knee softness
    const double x  = overDb / w;
    const double ov = x > 30.0 ? overDb : w * std::log1p (std::exp (x)); // soft-plus: smooth knee
    const double r  = mode == Limit ? 4.0 + 16.0 * (1.0 - std::exp (-ov / 8.0))
                                    : 1.5 +  2.5 * (1.0 - std::exp (-ov / 12.0));
    return ov * (1.0 - 1.0 / r);
}

// -----------------------------------------------------------------------------
class StereoVariMu
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rmsCoef    = 1.0 - std::exp (-1.0 / (0.003 * fs));    // 3 ms RMS window
        paramCoef  = 1.0 - std::exp (-1.0 / (0.020 * fs));    // smooth stepped knobs
        dcCoef     = 1.0 - std::exp (-2.0 * kPi * 5.0 / fs);
        bypassStep = 1.0 / (0.020 * fs);
        for (int c = 0; c < 2; ++c)
        {
            bilinear2 (scHp[c], 1, 0, 0, 2.0 * kPi * 100.0 / 0.7071, std::pow (2.0 * kPi * 100.0, 2.0), 2.0 * kPi * 100.0, fs);
            scHp[c].reset();
            ms[c] = 0.0; gr[c] = 0.0; dc[c] = 0.0; peakGr[c] = 0.0;
            st[c].thr = tg[c].thr; st[c].out = tg[c].out; st[c].mix = tg[c].mix; st[c].on = tg[c].on;
        }
        updateTimeConstants();
    }

    void setSettings (const CompSettings& left, const CompSettings& right, bool stereoLink)
    {
        const CompSettings* s[2] = { &left, &right };
        for (int c = 0; c < 2; ++c)
        {
            tg[c].thr  = compThresholdDb (s[c]->threshold);
            tg[c].out  = std::pow (10.0, compOutputDb (s[c]->output) / 20.0);
            tg[c].mix  = compMix (s[c]->mix);
            tg[c].on   = s[c]->compIn ? 1.0 : 0.0;
            mode[c]    = s[c]->mode;
            useHp[c]   = s[c]->scHp;
            if (attackIdx[c] != s[c]->attack || recoveryIdx[c] != s[c]->recovery)
            {
                attackIdx[c] = s[c]->attack; recoveryIdx[c] = s[c]->recovery;
                timesDirty = true;
            }
        }
        linked = stereoLink;
        if (timesDirty) updateTimeConstants();
    }

    // Process one stereo sample in place. For mono, pass the same value twice.
    inline void process (double& l, double& r)
    {
        double* x[2] = { &l, &r };
        double level[2];
        for (int c = 0; c < 2; ++c)
        {
            auto& s = st[c]; const auto& t = tg[c];
            s.thr += paramCoef * (t.thr - s.thr);
            s.out += paramCoef * (t.out - s.out);
            s.mix += paramCoef * (t.mix - s.mix);
            if (s.on < t.on) s.on = std::min (t.on, s.on + bypassStep);
            else if (s.on > t.on) s.on = std::max (t.on, s.on - bypassStep);

            const double sc = useHp[c] ? scHp[c].process (*x[c]) : *x[c];
            ms[c] += rmsCoef * (sc * sc - ms[c]);
            // sqrt(2*ms) so a sine reads its peak level: threshold is in dBFS peak
            level[c] = 10.0 * std::log10 (2.0 * ms[c] + 1e-20);
        }
        if (linked) level[0] = level[1] = std::max (level[0], level[1]);

        for (int c = 0; c < 2; ++c)
        {
            auto& s = st[c];
            const double target = variMuGainReductionDb (level[c] - s.thr, mode[c]);
            gr[c] += (target > gr[c] ? attCoef[c] : relCoef[c]) * (target - gr[c]);
            if (linked && c == 1) gr[1] = gr[0];          // identical gain on both sides

            const double dry = *x[c];
            double wet = dry * std::pow (10.0, -gr[c] / 20.0);

            // variable-mu tube colour: small 2nd harmonic, grows with gain reduction
            const double k = 0.0035 * std::min (gr[c], 20.0);
            double h = k * wet * wet;
            dc[c] += dcCoef * (h - dc[c]);
            wet += h - dc[c];

            wet *= s.out;
            const double comp = dry + s.mix * (wet - dry);
            *x[c] = dry + s.on * (comp - dry);
            peakGr[c] = std::max (peakGr[c], gr[c] * s.on);
        }
    }

    // Largest gain reduction since the last call, in dB (for the meters).
    double takeGainReductionDb (int c) { const double v = peakGr[c]; peakGr[c] = 0.0; return v; }

private:
    void updateTimeConstants()
    {
        for (int c = 0; c < 2; ++c)
        {
            const double att = kAttackMs[std::clamp (attackIdx[c], 0, kNumAttackSteps - 1)] * 0.001;
            // RECOVERY = time to get back ~95 % of the way (3 time constants)
            const double rel = kRecoverySec[std::clamp (recoveryIdx[c], 0, kNumRecoverySteps - 1)] / 3.0;
            attCoef[c] = 1.0 - std::exp (-1.0 / (att * fs));
            relCoef[c] = 1.0 - std::exp (-1.0 / (rel * fs));
        }
        timesDirty = false;
    }

    struct Smoothed { double thr = 0.0, out = 1.0, mix = 1.0, on = 1.0; };
    Smoothed st[2], tg[2];
    Biquad scHp[2];
    double ms[2] {}, gr[2] {}, dc[2] {}, peakGr[2] {};
    double attCoef[2] { 0.001, 0.001 }, relCoef[2] { 0.0001, 0.0001 };
    int    attackIdx[2] { 5, 5 }, recoveryIdx[2] { 3, 3 }, mode[2] { Compress, Compress };
    bool   useHp[2] { true, true }, linked = true, timesDirty = true;
    double fs = 192000.0, rmsCoef = 0.01, paramCoef = 0.001, dcCoef = 0.0001, bypassStep = 0.001;
};
} // namespace ppm
