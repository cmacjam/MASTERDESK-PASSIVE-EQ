// Tests for the M/S CLASS-A compressor: checks the behaviours stated in the
// elysia alpha compressor manuals (ratio law, feed forward, attack/release
// definitions, auto fast, sidechain and Niveau filters, M/S, link, soft clip, warm).
#include "dsp/ClassAComp.h"
#include <cstdio>
#include <vector>
#include <string>
#include <functional>

using namespace ppm;
static int failures = 0;
static void check (bool ok, const std::string& what) { std::printf ("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str()); if (! ok) ++failures; }
static const double fs = 192000.0, PI = 3.14159265358979323846;
static std::string f2 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.2f", v); return b; }

static ClassASettings base()
{
    ClassASettings s; s.in = true; s.link = false;
    for (auto& c : s.ch) { c.thresholdDb = -20; c.attackMs = 2; c.releaseMs = 300; c.ratioPos = 6; }
    return s;
}

// steady sine through the unit: output level (dB of peak) and the gain reduction at the end
struct Level { double outDb, grDb; };
static Level steady (const ClassASettings& st, double inDb, double freq = 1000.0, double seconds = 0.6, bool stereoOpposite = false)
{
    ClassACompressor c; c.setSettings (st); c.prepare (fs); c.setSettings (st);
    const double a = std::pow (10.0, inDb / 20.0);
    const int n = (int) (fs * seconds);
    double e = 0; int cnt = 0, start = n - (int) (fs * 0.1);
    for (int i = 0; i < n; ++i)
    {
        const double v = a * std::sin (2 * PI * freq * i / fs);
        double l = v, r = stereoOpposite ? -v : v;
        c.process (l, r);
        if (i == start) c.takeGainReductionDb (0);
        if (i >= start) { e += l * l; ++cnt; }
    }
    return { 20 * std::log10 (std::sqrt (2 * e / cnt) + 1e-12), c.takeGainReductionDb (0) };
}

int main()
{
    std::printf ("== FEEDBACK ratio law (printed ratio holds at 6 dB of reduction, rises above) ==\n");
    {
        auto st = base(); st.ch[0].ratioPos = 11;                 // printed 1:2.5
        // find the input level giving the wanted reduction; ratio = (in - threshold) / (out - threshold) there
        auto localRatio = [&] (double wantGr, double& atGr)
        {
            double lo = -20, hi = 60;
            for (int it = 0; it < 30; ++it) { const double m = 0.5 * (lo + hi); (steady (st, m).grDb < wantGr ? lo : hi) = m; }
            const double in = 0.5 * (lo + hi);
            const auto a = steady (st, in);
            atGr = a.grDb;
            return (in - st.ch[0].thresholdDb) / (a.outDb - st.ch[0].thresholdDb);
        };
        double g6 = 0, g17 = 0;
        const double r6 = localRatio (6.0, g6), r17 = localRatio (17.0, g17);
        check (std::abs (r6 - 2.5) < 0.35, "printed 1:2.5 -> ratio " + f2 (r6) + " at " + f2 (g6) + " dB of reduction (manual: 2.5 at 6 dB)");
        check (std::abs (r17 - 4.5) < 0.6, "                -> ratio " + f2 (r17) + " at " + f2 (g17) + " dB of reduction (manual: ~4.5 at 17 dB)");
        auto lowRatio = base(); lowRatio.ch[0].ratioPos = 0;      // printed 1:1.1
        double gl = 0;
        st = lowRatio;
        const double rl = localRatio (6.0, gl);
        check (std::abs (rl - 1.1) < 0.1, "printed 1:1.1 -> ratio " + f2 (rl) + " at " + f2 (gl) + " dB of reduction");
    }

    std::printf ("== FEED FORWARD (ratios double, top of the scale = limiting into negative ratios) ==\n");
    {
        auto st = base(); st.ch[0].feedForward = true; st.ch[0].ratioPos = 8;      // printed 2.1 -> 4.2 in feed forward
        const auto a = steady (st, 0.0), b = steady (st, 10.0);
        const double r = 10.0 / (b.outDb - a.outDb);
        check (std::abs (r - 4.2) < 0.3, "printed 1:2.1 in feed forward -> ratio " + f2 (r) + " well above threshold (want 4.2)");
        st.ch[0].ratioPos = 11; st.ch[0].thresholdDb = -10;
        const auto c0 = steady (st, 0.0), c10 = steady (st, 10.0);
        check (c10.outDb < c0.outDb - 3.0, "\"X\" in feed forward: +10 dB louder in -> " + f2 (c10.outDb - c0.outDb) + " dB out (negative ratio)");
    }

    std::printf ("== ATTACK = time to build 10 dB of reduction, RELEASE = time back from 6 dB (+ attack) ==\n");
    {
        auto timing = [&] (bool ff, double attMs, double relMs, bool afr, double& attackTime, double& releaseTime, double burstS = 1.0)
        {
            auto st = base(); st.ch[0].feedForward = ff; st.ch[0].attackMs = attMs; st.ch[0].releaseMs = relMs; st.ch[0].ratioPos = 11;
            st.ch[0].thresholdDb = -30; st.ch[0].autoFastRelease = afr;
            ClassACompressor c; c.setSettings (st); c.prepare (fs); c.setSettings (st);
            attackTime = releaseTime = -1;
            const int n = (int) (fs * 6.0);
            double grNow = 0, grAt6 = -1;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / fs;
                const double a = (t >= 0.5 && t < 0.5 + burstS) ? 1.0 : 0.001;     // loud burst from 0.5 s
                double l = a * std::sin (2 * PI * 2000 * t), r = l;
                c.process (l, r);
                grNow = c.takeGainReductionDb (0);
                if (attackTime < 0 && t >= 0.5 && grNow >= 10.0) attackTime = (t - 0.5) * 1000.0;
                if (t >= 0.5 + burstS && grAt6 < 0 && grNow <= 6.0) grAt6 = t;
                if (grAt6 > 0 && releaseTime < 0 && grNow <= 0.05) releaseTime = (t - grAt6) * 1000.0;
            }
        };
        double at = 0, rt = 0;
        timing (false, 24, 300, false, at, rt);
        check (std::abs (at - 24) < 3, "feedback, ATTACK 24 ms -> 10 dB of reduction after " + f2 (at) + " ms");
        check (std::abs (rt - 324) < 25, "RELEASE 300 ms (+ 24 ms attack) -> back from 6 dB in " + f2 (rt) + " ms (linear)");
        timing (true, 24, 300, false, at, rt);
        check (std::abs (at - 48) < 5, "feed forward, ATTACK 24 ms -> " + f2 (at) + " ms (manual: about twice the scale)");
        double at2 = 0, rtFast = 0, rtSlow = 0, rtLong = 0;
        timing (false, 2, 430, false, at2, rtSlow, 0.03);
        timing (false, 2, 430, true, at2, rtFast, 0.03);
        timing (false, 2, 430, true, at2, rtLong, 1.5);
        check (rtFast < rtSlow * 0.5, "AUTO FAST release, 30 ms burst: back from 6 dB in " + f2 (rtSlow) + " ms -> " + f2 (rtFast) + " ms");
        check (rtLong > rtSlow * 0.8, "AUTO FAST release after 1.5 s of compression: stays near the set release (" + f2 (rtLong) + " ms)");
    }

    std::printf ("== AUTO FAST attack ==\n");
    {
        auto overshoot = [&] (bool afa)
        {
            auto st = base(); st.ch[0].attackMs = 110; st.ch[0].ratioPos = 11; st.ch[0].thresholdDb = -35; st.ch[0].autoFastAttack = afa;
            ClassACompressor c; c.setSettings (st); c.prepare (fs); c.setSettings (st);
            double peak = 0;
            for (int i = 0; i < (int) (fs * 1.0); ++i)
            {
                const double t = i / fs, a = t < 0.6 ? 0.1 : 0.9;                    // already compressing, then a loud jump
                double l = a * std::sin (2 * PI * 1000 * t), r = l;
                c.process (l, r);
                if (t > 0.602 && t < 0.62) peak = std::max (peak, std::abs (l));
            }
            return 20 * std::log10 (peak);
        };
        const double slow = overshoot (false), fast = overshoot (true);
        check (fast < slow - 1.8, "loud jump while compressing: " + f2 (slow) + " dBFS -> with AUTO FAST " + f2 (fast) + " dBFS");
    }

    std::printf ("== M/S and LINK ==\n");
    {
        auto st = base(); st.mode = ClassAMidSide; st.ch[0].thresholdDb = 2; st.ch[1].thresholdDb = 2;
        ClassACompressor c; c.setSettings (st); c.prepare (fs); c.setSettings (st);
        double err = 0;
        for (int i = 0; i < 20000; ++i)
        {
            const double l0 = 0.1 * std::sin (i * 0.01), r0 = 0.07 * std::sin (i * 0.017);
            double l = l0, r = r0; c.process (l, r);
            err = std::max (err, std::max (std::abs (l - l0), std::abs (r - r0)));
        }
        check (err < 1e-12, "M/S with nothing compressing: L/R come back exactly");
        st.ch[0].thresholdDb = -30; st.ch[0].ratioPos = 11;       // squeeze the MID only
        const auto side = steady (st, -6.0, 1000.0, 0.6, true), mid = steady (st, -6.0);
        check (std::abs (side.outDb + 6.0) < 0.05 && mid.outDb < -12.0, "MID compressed, SIDE alone: out-of-phase " + f2 (side.outDb) + " dB, mono " + f2 (mid.outDb) + " dB (in -6)");
        auto lk = base(); lk.link = true; lk.ch[0].thresholdDb = -30;
        ClassACompressor d; d.setSettings (lk); d.prepare (fs); d.setSettings (lk);
        for (int i = 0; i < (int) (fs * 0.5); ++i) { double l = 0.5 * std::sin (2 * PI * 1000 * i / fs), r = 0.01 * l; d.process (l, r); }
        const double g0 = d.takeGainReductionDb (0), g1 = d.takeGainReductionDb (1);
        check (std::abs (g0 - g1) < 0.01 && g0 > 5, "LINK: one control voltage - both channels " + f2 (g0) + " / " + f2 (g1) + " dB");
    }

    std::printf ("== SIDECHAIN FILTER (6 dB/oct tilt, up to -12 dB) ==\n");
    {
        auto st = base(); st.ch[0].thresholdDb = -30; st.ch[0].ratioPos = 11;
        const double flat = steady (st, -6.0, 60.0, 1.0).grDb;
        st.ch[0].scOn = true; st.ch[0].scTilt = 1.0; st.ch[0].scFreq = 800;    // HP: lows reduced by 12 dB in the detector
        const double hp = steady (st, -6.0, 60.0, 1.0).grDb;
        check (hp < flat - 2.0, "HP at 800 Hz: a 60 Hz tone triggers less: " + f2 (flat) + " -> " + f2 (hp) + " dB of reduction");
        st.ch[0].scTilt = -1.0;
        const double lpLow = steady (st, -6.0, 60.0, 1.0).grDb;
        check (std::abs (lpLow - flat) < 1.0, "LP: the 60 Hz tone triggers as before (" + f2 (lpLow) + " dB)");
    }

    std::printf ("== NIVEAU FILTER (tilt after the compressor) ==\n");
    {
        auto st = base(); for (auto& c : st.ch) { c.thresholdDb = 2; c.eqOn = true; c.eqFreq = 110; c.eqX10 = true; }   // 1.1 kHz
        st.ch[0].eqTilt = 1.0;
        const double hi = steady (st, -20, 18000).outDb + 20, lo = steady (st, -20, 30).outDb + 20;
        check (std::abs (hi - 3.0) < 0.6 && std::abs (lo + 5.0) < 0.6, "hard right at 1.1 kHz: highs " + f2 (hi) + " dB, lows " + f2 (lo) + " dB (manual: +3 / -5)");
        st.ch[0].eqTilt = -1.0;
        const double hi2 = steady (st, -20, 18000).outDb + 20, lo2 = steady (st, -20, 30).outDb + 20;
        check (std::abs (lo2 - 3.0) < 0.6 && std::abs (hi2 + 4.5) < 0.6, "hard left: lows " + f2 (lo2) + " dB, highs " + f2 (hi2) + " dB (manual: +3 / -4.5)");
    }

    std::printf ("== GAIN / MIX ==\n");
    {
        auto st = base(); st.ch[0].thresholdDb = 2; st.ch[0].gainDb = 12;
        check (std::abs (steady (st, -20).outDb + 8.0) < 0.05, "GAIN +12 dB");
        st.ch[0].mix = 0.0;
        check (std::abs (steady (st, -20).outDb + 20.0) < 0.05, "MIX fully direct: untouched");
    }

    std::printf ("== SOFT CLIP ==\n");
    {
        auto st = base(); st.softClip = true; st.clipDb = -6; st.ch[0].thresholdDb = 2; st.ch[1].thresholdDb = 2;
        ClassACompressor c; c.setSettings (st); c.prepare (fs); c.setSettings (st);
        double peak = 0;
        for (int i = 0; i < 40000; ++i) { double l = 2.0 * std::sin (i * 0.02), r = l; c.process (l, r); peak = std::max (peak, std::abs (l)); }
        check (20 * std::log10 (peak) <= -6.0 + 1e-9, "threshold -6 dB, +6 dBFS in -> peak " + f2 (20 * std::log10 (peak)) + " dBFS (rounded, never above)");
        check (c.takeClipActive(), "soft clip LED lights");
        check (std::abs (steady (st, -13.0).outDb + 13.0) < 1e-6, "below its knee: completely clean");
    }

    std::printf ("== WARM (slew-rate limiter: subtle) ==\n");
    {
        auto st = base(); st.ch[0].thresholdDb = 2; st.ch[0].warm = true;
        const double mid = steady (st, -6, 1000).outDb + 6, top = steady (st, 0, 18000).outDb;
        check (std::abs (mid) < 0.01, "1 kHz at -6 dBFS: unchanged (" + f2 (mid) + " dB)");
        check (top < -0.2 && top > -6.0, "18 kHz at full scale: gently softened (" + f2 (top) + " dB)");
    }

    std::printf (failures == 0 ? "\nALL CLASS-A TESTS PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
