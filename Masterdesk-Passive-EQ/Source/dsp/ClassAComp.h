// =============================================================================
//  ClassAComp.h  —  M/S CLASS-A compressor (ash-blue unit)
//
//  Built to follow the published behaviour of the elysia alpha compressor
//  (elysia "alpha compressor" manual v1.1 and the Plugin Alliance manual; this
//  is an independent model, not affiliated with elysia):
//
//  Signal flow (as in the manual):
//    [M/S encode] -> sidechain filter -> compressor -> Niveau filter -> gain
//    -> mix (direct / compressed) -> WARM -> [M/S decode] -> soft clip -> out
//
//  Per channel (left = L or MID, right = R or SIDE):
//   * THRESHOLD over a 33 dB range (+2 .. -31 dBFS).
//   * FEEDBACK (default) or FEED FORWARD topology. Feedback: the detector hears
//     the compressed output; soft knee; the printed ratio (1:1.1 .. 1:2.5) is
//     the ratio at 6 dB of gain reduction and it rises with more reduction
//     (1:2.5 at 6 dB -> ~1:4.5 at 17 dB). Feed forward: detector hears the
//     input; printed ratios double, the top of the scale ("X") passes through
//     limiting (inf:1) into negative ratios (louder in -> quieter out:
//     ~0 dB -> -8 dB, +10 dB -> -15 dB around a 0 dB threshold); attack times
//     are about twice the printed value.
//   * ATTACK 0.01 .. 150 ms = time to build up 10 dB of gain reduction.
//   * RELEASE 60 ms .. 1.8 s = time to return from 6 dB of gain reduction;
//     linear (constant dB per second); the attack time adds to it.
//   * AUTO FAST attack: on fast, loud peaks (once there is ~3 dB of reduction)
//     the attack is shortened, then returns to the set value.
//   * AUTO FAST release: right after a burst the release drops to ~20 ms and
//     then glides back to the set value (over ~250 ms).
//   * SIDECHAIN FILTER: 6 dB/oct shelving tilt of the detector only, sweeping
//     from LP (highs reduced, up to -12 dB) through flat to HP (lows reduced,
//     up to -12 dB), at 30 Hz .. 3.3 kHz.
//   * NIVEAU FILTER (audio EQ, after the compressor): one knob tilts around a
//     frequency (20 Hz .. 2 kHz, x10 = 200 Hz .. 20 kHz). Right: highs up to
//     +3 dB and lows down to -5 dB. Left: lows up to +3 dB, highs down to -4.5 dB.
//   * GAIN 0 .. +12 dB, MIX direct .. compressed.
//   * WARM: a slew-rate limiter (subtle: changes transients and upper harmonics).
//  Global:
//   * M/S (left controls = MID, right = SIDE), LINK (one control voltage from
//     the left controls for both channels; filter, mix, gain, warm stay separate).
//   * SOFT CLIP with THRESHOLD: saturation-style peak rounding after the M/S
//     decoder; never exceeds its ceiling; LED shows when it works.
//  Runs at the plug-in's internal oversampled rate (4x below 50 kHz, 2x below
//  100 kHz - the same as the original plug-in version).
//  Plain C++17; tested in tests/classa_test.cpp.
// =============================================================================
#pragma once
#include "PassiveNetwork.h"
#include <cmath>
#include <algorithm>

namespace ppm
{
enum ClassAMode { ClassAStereo = 0, ClassAMidSide = 1 };

// printed RATIO scale (12 positions, the last is "X")
inline constexpr double kClassARatioScale[12] = { 1.1, 1.2, 1.3, 1.4, 1.6, 1.7, 1.8, 1.9, 2.1, 2.2, 2.4, 2.5 };
constexpr double kClassARatioTop = 11.0;

struct ClassAChannel
{
    double thresholdDb = -10.0;    // +2 .. -31 dBFS
    double attackMs    = 13.0;     // 0.01 .. 150
    double releaseMs   = 300.0;    // 60 .. 1800
    double ratioPos    = 6.0;      // 0 .. 11 (printed scale position)
    bool   feedForward = false;
    bool   autoFastAttack = false, autoFastRelease = false;
    double gainDb      = 0.0;      // 0 .. 12
    double mix         = 1.0;      // 0 = direct .. 1 = compressed
    bool   scOn = false; double scTilt = 0.0; double scFreq = 280.0;     // tilt -1 (LP) .. +1 (HP)
    bool   eqOn = false; double eqTilt = 0.0; double eqFreq = 500.0; bool eqX10 = false;   // tilt -1 (darker) .. +1 (brighter)
    bool   warm = false;
};

struct ClassASettings
{
    bool   in = false;
    int    mode = ClassAStereo;
    bool   link = true;
    ClassAChannel ch[2];
    bool   softClip = false;
    double clipDb = 0.0;           // soft clip ceiling, -12 .. 0 dBFS
};

// printed ratio at a (fractional) scale position
inline double classAPrintedRatio (double pos)
{
    const double p = std::clamp (pos, 0.0, kClassARatioTop);
    const int i = std::min ((int) p, 10);
    return kClassARatioScale[i] + (p - i) * (kClassARatioScale[i + 1] - kClassARatioScale[i]);
}
// Feed forward slope (dB of reduction per dB over threshold): ratios doubled; past position 10
// the curve goes through limiting (slope 1) into negative ratios (slope ~1.67 at "X")
inline double classAFeedForwardSlope (double pos)
{
    if (pos <= 10.0) return 1.0 - 1.0 / (2.0 * classAPrintedRatio (pos));
    const double s10 = 1.0 - 1.0 / (2.0 * kClassARatioScale[10]);
    return s10 + (std::min (pos, kClassARatioTop) - 10.0) * (1.67 - s10);
}
// Feedback: ratio at a given amount of reduction (the printed value holds at 6 dB, rises above)
inline double classAFeedbackRatio (double pos, double grDb)
{
    const double rp = classAPrintedRatio (pos);
    return 1.0 + (rp - 1.0) * std::max (0.25, 1.0 + 0.1212 * (grDb - 6.0));
}
// 6 dB soft knee (quadratic): exactly zero more than 3 dB below threshold, so the unit is bit-transparent there
inline double classAKnee (double overDb)
{
    if (overDb <= -3.0) return 0.0;
    if (overDb < 3.0) return (overDb + 3.0) * (overDb + 3.0) / 12.0;
    return overDb;
}

// Soft clip, normalised to a ceiling of 1.0: identity below 0.5 (-6 dB), saturates smoothly towards 1.0
inline double classASoftClip (double x)
{
    constexpr double t = 0.5;
    const double ax = std::abs (x);
    if (ax <= t) return x;
    return std::copysign (t + (1.0 - t) * std::tanh ((ax - t) / (1.0 - t)), x);
}

// first-order shelving section (bilinear), gain in dB at the shelf side
struct Shelf1
{
    double b0 = 1, b1 = 0, a1 = 0, z = 0;
    void set (bool high, double freq, double gainDb, double fs)
    {
        const double A = std::pow (10.0, gainDb / 20.0);
        const double wc = std::tan (kPi * std::clamp (freq, 10.0, fs * 0.45) / fs);
        // low shelf: H(s) = (s + A wc) / (s + wc);  high shelf: H(s) = (A s + wc) / (s + wc)
        double n1, n0;
        if (high) { n1 = A; n0 = wc; } else { n1 = 1.0; n0 = A * wc; }
        const double d = 1.0 + wc;
        b0 = (n1 + n0) / d; b1 = (n0 - n1) / d; a1 = (wc - 1.0) / d;
    }
    double process (double x) { const double y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    void reset() { z = 0; }
};

class ClassACompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; dt = 1.0 / fs;
        smooth = 1.0 - std::exp (-1.0 / (0.020 * fs));
        bypassStep = 1.0 / (0.020 * fs);
        slewMax = 2.0 * kPi * 12000.0 / fs;          // WARM: full-scale slew of a 12 kHz sine
        designFilters();
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            gr[c] = 0.0; peakGr[c] = 0.0; burstAge[c] = 0.0; demand[c] = 0.0; warmPrev[c] = 0.0;
            scLo[c].reset(); scHi[c].reset(); eqLo[c].reset(); eqHi[c].reset();
            curGain[c] = std::pow (10.0, s.ch[c].gainDb / 20.0); curMix[c] = s.ch[c].mix;
        }
        curOn = s.in ? 1.0 : 0.0; clipActive = false;
    }

    void setSettings (const ClassASettings& n)
    {
        const bool filtersChanged = [&] {
            for (int c = 0; c < 2; ++c)
            {
                const auto &a = n.ch[c], &b = s.ch[c];
                if (a.scOn != b.scOn || std::abs (a.scTilt - b.scTilt) > 1e-6 || std::abs (a.scFreq - b.scFreq) > 1e-3
                    || a.eqOn != b.eqOn || std::abs (a.eqTilt - b.eqTilt) > 1e-6 || std::abs (a.eqFreq - b.eqFreq) > 1e-3 || a.eqX10 != b.eqX10)
                    return true;
            }
            return false; } ();
        s = n;
        if (s.link)     // one control voltage: the left compressor controls drive both channels
        {
            auto& r = s.ch[1]; const auto& l = s.ch[0];
            r.thresholdDb = l.thresholdDb; r.attackMs = l.attackMs; r.releaseMs = l.releaseMs; r.ratioPos = l.ratioPos;
            r.feedForward = l.feedForward; r.autoFastAttack = l.autoFastAttack; r.autoFastRelease = l.autoFastRelease;
        }
        for (int c = 0; c < 2; ++c) tgtGain[c] = std::pow (10.0, s.ch[c].gainDb / 20.0);
        clipLin = std::pow (10.0, std::clamp (s.clipDb, -12.0, 0.0) / 20.0);
        if (filtersChanged) designFilters();
    }

    inline void process (double& l, double& r)
    {
        const double inL = l, inR = r;
        const bool ms = s.mode == ClassAMidSide;
        double x[2] = { ms ? 0.5 * (l + r) : l, ms ? 0.5 * (l - r) : r };

        const double onTarget = s.in ? 1.0 : 0.0;
        if (curOn < onTarget) curOn = std::min (onTarget, curOn + bypassStep);
        else if (curOn > onTarget) curOn = std::max (onTarget, curOn - bypassStep);

        // ---- sidechain: detector level (filtered), peak-sensing
        double det[2];
        for (int c = 0; c < 2; ++c)
        {
            double v = x[c];
            const int f = s.link ? 0 : c;                         // linked: the left (or MID) filter for both
            if (s.ch[f].scOn) v = scHi[c].process (scLo[c].process (v));
            det[c] = 20.0 * std::log10 (std::abs (v) + 1e-12);   // peak-sensing
        }
        if (s.link) det[0] = det[1] = std::max (det[0], det[1]);

        // ---- gain computer + ballistics (linear attack / release in dB)
        for (int c = 0; c < 2; ++c)
        {
            const auto& ch = s.ch[c];
            const double inOver = det[c] - ch.thresholdDb;
            double target;
            if (ch.feedForward)
                target = classAFeedForwardSlope (ch.ratioPos) * classAKnee (inOver);
            else
            {   // feedback: the detector hears the output (input - reduction). Solve g = (R(g) - 1) * knee (inOver - g)
                // directly (a few Newton steps from the current value): the true feedback curve, always stable.
                double g = std::max (0.0, gr[c]);
                for (int it = 0; it < 3; ++it)
                {
                    const double R = classAFeedbackRatio (ch.ratioPos, g);
                    g = std::max (0.0, g + ((R - 1.0) * classAKnee (inOver - g) - g) / R);
                }
                target = g;
            }
            double attS = std::max (1e-5, ch.attackMs * 0.001) * (ch.feedForward ? 2.0 : 1.0);
            double& g = gr[c];
            if (target > g)
            {
                if (ch.autoFastAttack && g > 3.0 && target - g > 3.0) attS = std::min (attS, std::max (1e-5, attS / 20.0));
                g = std::min (target, g + 10.0 / attS * dt);                       // 10 dB in ATTACK
            }
            else
            {
                double relS = std::max (0.01, ch.releaseMs * 0.001) + attS;      // the attack time adds to the release
                // AUTO FAST release: after a short burst, recover in ~20 ms; the longer the compression lasted,
                // the closer the release stays to its set value
                if (ch.autoFastRelease) relS = 0.020 + (relS - 0.020) * (1.0 - std::exp (-burstAge[c] / 0.25));
                g = std::max (target, g - 6.0 / relS * dt);                        // back from 6 dB in RELEASE
            }
            // how long the current compression event has lasted (reset once it has let go)
            // (the detector is peak-sensing: hold its demand briefly to bridge the waveform's zero crossings)
            demand[c] = std::max (target, demand[c] - 100.0 * dt);
            if (g < 0.5 && target > g + 0.5) burstAge[c] = 0.0;                    // a new event starts
            else if (g > 1.0 && demand[c] >= g - 1.0) burstAge[c] += dt;          // the music is still pushing it
        }
        if (s.link) gr[0] = gr[1] = std::max (gr[0], gr[1]);

        // ---- audio path per channel
        for (int c = 0; c < 2; ++c)
        {
            const auto& ch = s.ch[c];
            const double dry = x[c];
            double y = dry * std::pow (10.0, -gr[c] / 20.0);
            if (ch.eqOn) y = eqHi[c].process (eqLo[c].process (y));               // Niveau filter
            curGain[c] += smooth * (tgtGain[c] - curGain[c]);
            curMix[c]  += smooth * (ch.mix - curMix[c]);
            y *= curGain[c];
            y = dry + curMix[c] * (y - dry);                                       // direct / compressed
            if (ch.warm)                                                          // slew-rate limiting
            {
                const double d = std::clamp (y - warmPrev[c], -slewMax, slewMax);
                y = warmPrev[c] + d;
            }
            warmPrev[c] = y;
            x[c] = y;
            peakGr[c] = std::max (peakGr[c], gr[c] * curOn);
        }

        double ol = ms ? x[0] + x[1] : x[0];
        double orr = ms ? x[0] - x[1] : x[1];
        if (s.softClip)
        {
            if (std::max (std::abs (ol), std::abs (orr)) > 0.5 * clipLin) clipActive = true;
            ol = clipLin * classASoftClip (ol / clipLin);
            orr = clipLin * classASoftClip (orr / clipLin);
        }
        l = inL + curOn * (ol - inL);
        r = inR + curOn * (orr - inR);
    }

    double takeGainReductionDb (int c) { const double v = peakGr[c]; peakGr[c] = 0.0; return v; }
    bool   takeClipActive() { const bool v = clipActive; clipActive = false; return v; }

private:
    void designFilters()
    {
        for (int c = 0; c < 2; ++c)
        {
            const auto& ch = s.ch[c];
            // sidechain: HP side = lows reduced (low shelf cut), LP side = highs reduced (high shelf cut)
            static const double depth[6] = { 0.0, 0.3, 1.0, 2.0, 5.0, 12.0 };     // printed SC GAIN scale
            const double a = std::clamp (std::abs (ch.scTilt), 0.0, 1.0) * 5.0;
            const int i = std::min ((int) a, 4);
            const double cut = depth[i] + (a - i) * (depth[i + 1] - depth[i]);
            scLo[c].set (false, ch.scFreq, ch.scTilt > 0 ? -cut : 0.0, fs);
            scHi[c].set (true,  ch.scFreq, ch.scTilt < 0 ? -cut : 0.0, fs);
            // Niveau filter: tilt around EQ FREQ
            const double f = ch.eqFreq * (ch.eqX10 ? 10.0 : 1.0), t = std::clamp (ch.eqTilt, -1.0, 1.0);
            const double hiDb = t >= 0 ? 3.0 * t : 4.5 * t, loDb = t >= 0 ? -5.0 * t : -3.0 * t;
            eqLo[c].set (false, f, loDb, fs);
            eqHi[c].set (true,  f, hiDb, fs);
        }
    }

    ClassASettings s;
    Shelf1 scLo[2], scHi[2], eqLo[2], eqHi[2];
    double fs = 192000.0, dt = 1.0 / 192000.0, smooth = 0.001, bypassStep = 0.001, slewMax = 0.4, clipLin = 1.0;
    double gr[2] {}, peakGr[2] {}, burstAge[2] { 0.0, 0.0 }, demand[2] {}, warmPrev[2] {};
    double curGain[2] { 1.0, 1.0 }, curMix[2] { 1.0, 1.0 }, tgtGain[2] { 1.0, 1.0 }, curOn = 0.0;
    bool clipActive = false;
};
} // namespace ppm
