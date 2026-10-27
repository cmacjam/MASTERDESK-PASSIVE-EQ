// =============================================================================
//  AutoMaster.h  —  genre-based "auto mastering" assistant
//
//  How it works:
//   1. LISTEN records up to 30 s of the music coming in (silence is skipped).
//   2. The recording's long-term tonal balance is measured in 10 octave bands
//      (31 Hz .. 16 kHz), using only the louder passages.
//   3. That balance is compared with the chosen genre's target curve. The
//      difference, smoothed and limited to +/-6 dB and scaled by AMOUNT, is the
//      EQ correction wanted.
//   4. The four passive-EQ bands are fitted to that correction by searching the
//      real switch positions (frequency / gain / bandwidth / bell-shelf) against
//      the same analog response the GUI uses, preferring few, gentle, broad moves.
//   5. The compressor, tubes, filters and MAXIMIZER are then set from the genre,
//      with threshold and maximizer gain found by rendering the recording
//      (done by the processor, see PluginProcessor.cpp).
//
//  The genre curves are stylistic starting points (typical tonal balance and
//  loudness of each style), not measurements of particular records.
//  Plain C++17; tested in tests/automaster_test.cpp.
// =============================================================================
#pragma once
#include "Model.h"
#include "VariMu.h"
#include <array>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace ppm::automaster
{
constexpr int kOct = 10;
inline constexpr double kOctHz[kOct] = { 31.5, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

constexpr int kNumGenres = 7;
enum Genre { Pop = 0, Jazz, Seventies, Eighties, Nineties, RnB, HipHop };

struct GenreTarget
{
    const char* name;
    const char* shortName;
    double curve[kOct];     // octave-band level, dB relative to the mids (250 Hz - 4 kHz average)
    double lufs;            // integrated loudness target
    double grDb;            // average compressor gain reduction on the loud parts
    int attack, recovery, mix, mode;   // compressor switch positions
    bool scHp;
    bool tubeOn;
    double drive;           // tube drive, dB
    int hp, lp;             // filter positions
    double ceiling;         // maximizer ceiling, dBFS
    const char* character;  // one line for the display
};

inline const GenreTarget& genre (int g)
{
    //                       31    63   125  250  500   1k   2k   4k   8k   16k
    static const GenreTarget t[kNumGenres] = {
        { "POP", "POP",     { 2.0,  6.0, 6.0, 4.0, 2.0, 0.0,-2.0,-3.5,-5.5,-11.0 }, -10.0, 2.0, 5, 2, 10, Compress, true,  true, 0.0, 2, 0, -1.0,
          "bright, forward vocals, tight full low end, loud" },
        { "JAZZ", "JAZZ",   {-4.0,  1.0, 4.0, 4.0, 3.0, 0.0,-2.5,-5.5,-8.5,-15.0 }, -16.0, 0.8, 7, 4, 10, Compress, true,  true, 2.0, 1, 3, -1.0,
          "natural and open, warm mids, full dynamics" },
        { "70'S", "70s",    {-6.0,  0.0, 3.5, 4.5, 3.0, 0.0,-2.0,-5.0,-9.0,-17.0 }, -14.0, 1.5, 6, 4, 10, Compress, true,  true, 6.0, 3, 4, -1.0,
          "warm tape-style tone, rich low mids, soft top" },
        { "80'S", "80s",    {-4.0,  2.0, 4.0, 3.0, 2.0, 0.0,-1.5,-3.0,-5.0,-11.5 }, -12.0, 2.0, 4, 3, 10, Compress, true,  true, 3.0, 3, 0, -1.0,
          "bright and punchy, crisp top, lighter sub bass" },
        { "90'S", "90s",    { 1.0,  5.0, 5.0, 4.0, 2.0, 0.0,-2.0,-4.0,-6.0,-12.5 }, -10.5, 2.5, 4, 2, 10, Compress, true,  true, 2.0, 2, 0, -1.0,
          "big low end, present mids, getting loud" },
        { "R&B", "R&B",     { 4.0,  8.0, 6.0, 4.0, 2.0, 0.0,-2.0,-4.0,-6.0,-12.5 }, -10.0, 2.0, 6, 3, 10, Compress, true,  true, 2.0, 1, 0, -1.0,
          "deep smooth bass, silky top, warm vocals" },
        { "HIP HOP", "HIP HOP", { 6.0, 10.0, 7.0, 3.0, 1.0, 0.0,-2.0,-4.0,-6.0,-12.0 }, -9.0, 2.5, 7, 2, 10, Compress, true, true, 3.0, 1, 0, -1.0,
          "heavy sub and kick, punchy, loud" },
    };
    return t[std::clamp (g, 0, kNumGenres - 1)];
}

// ---------------------------------------------------------------------------
//  Tonal balance in octave bands (mono sum), louder passages only
// ---------------------------------------------------------------------------
struct Spectrum
{
    double db[kOct] {};      // band power, dB (absolute)
    bool   valid[kOct] {};
    bool   ok = false;       // enough signal was analysed
};

inline Spectrum measureSpectrum (const float* l, const float* r, int n, double fs)
{
    struct BQ
    {
        double b0, b1, b2, a1, a2, s1 = 0, s2 = 0;
        double run (double x) { const double y = b0 * x + s1; s1 = b1 * x - a1 * y + s2; s2 = b2 * x - a2 * y; return y; }
    };
    // two cascaded band-pass biquads per band (about one octave wide)
    std::vector<BQ> f;
    Spectrum s;
    for (int b = 0; b < kOct; ++b)
    {
        s.valid[b] = kOctHz[b] < 0.42 * fs;
        const double w0 = 2.0 * kPi * std::min (kOctHz[b], 0.42 * fs) / fs, Q = 1.1;
        const double al = std::sin (w0) / (2 * Q), a0 = 1 + al;
        BQ q { al / a0, 0.0, -al / a0, -2 * std::cos (w0) / a0, (1 - al) / a0 };
        f.push_back (q); f.push_back (q);
    }
    const int blk = std::max (1, (int) (0.4 * fs));
    std::vector<std::array<double, kOct>> blocks;
    std::vector<double> total;
    std::array<double, kOct> acc {}; int cnt = 0;
    for (int i = 0; i < n; ++i)
    {
        const double x = 0.5 * ((double) l[i] + (double) (r != nullptr ? r[i] : l[i]));
        for (int b = 0; b < kOct; ++b)
        {
            const double y = f[(size_t) (2 * b + 1)].run (f[(size_t) (2 * b)].run (x));
            acc[(size_t) b] += y * y;
        }
        if (++cnt == blk)
        {
            double t = 0; for (int b = 0; b < kOct; ++b) { acc[(size_t) b] /= blk; t += acc[(size_t) b]; }
            blocks.push_back (acc); total.push_back (t);
            acc = {}; cnt = 0;
        }
    }
    if (blocks.empty()) return s;
    double mean = 0; for (double t : total) mean += t; mean /= (double) total.size();
    // gate: skip near-silence and anything 10 dB below the average (quiet intros / breaks)
    const double gate = std::max (1e-7, mean * 0.1);
    std::array<double, kOct> sum {}; int used = 0;
    for (size_t k = 0; k < blocks.size(); ++k)
        if (total[k] >= gate) { for (int b = 0; b < kOct; ++b) sum[(size_t) b] += blocks[k][(size_t) b]; ++used; }
    if (used < 3) return s;
    for (int b = 0; b < kOct; ++b) s.db[b] = 10.0 * std::log10 (sum[(size_t) b] / used + 1e-20);
    s.ok = true;
    return s;
}

// Average of the mid bands (250 Hz .. 4 kHz): the reference everything is relative to
inline double midRef (const double* v) { double m = 0; for (int b = 3; b <= 7; ++b) m += v[b]; return m / 5.0; }

// ---------------------------------------------------------------------------
//  The EQ correction wanted (dB per octave band)
// ---------------------------------------------------------------------------
struct Correction
{
    double measuredRel[kOct] {};   // the mix, relative to its mids
    double target[kOct] {};        // the genre curve
    double wanted[kOct] {};        // the correction (after limits and AMOUNT)
};

inline Correction computeCorrection (const Spectrum& s, int g, double amount)
{
    Correction c;
    const auto& t = genre (g);
    const double ref = midRef (s.db);
    double raw[kOct];
    for (int b = 0; b < kOct; ++b)
    {
        c.measuredRel[b] = s.db[b] - ref;
        c.target[b] = t.curve[b] - midRef (t.curve);
        raw[b] = s.valid[b] ? c.target[b] - c.measuredRel[b] : 0.0;
    }
    for (int b = 0; b < kOct; ++b)
    {
        const double a = raw[std::max (0, b - 1)], m = raw[b], z = raw[std::min (kOct - 1, b + 1)];
        double d = 0.25 * a + 0.5 * m + 0.25 * z;                   // smooth: broad moves only
        double hi = 6.0, lo = -6.0;
        if (b == 0) hi = 3.0;                                       // don't pump up rumble
        if (b == kOct - 1) hi = 4.0;
        if (b == 0 && c.measuredRel[0] < -24.0) hi = 0.0;           // nothing down there to boost
        if (! s.valid[b]) hi = lo = 0.0;
        c.wanted[b] = amount * std::clamp (d, lo, hi);
    }
    return c;
}

// Wanted correction at any frequency (interpolated in octaves, flat beyond the ends)
inline double wantedAt (const Correction& c, double hz)
{
    const double x = std::log2 (hz / kOctHz[0]);
    if (x <= 0.0) return c.wanted[0];
    if (x >= kOct - 1) return c.wanted[kOct - 1];
    const int i = (int) x; const double fr = x - i;
    return c.wanted[i] + fr * (c.wanted[i + 1] - c.wanted[i]);
}

// ---------------------------------------------------------------------------
//  Fit the four passive bands to the wanted correction
// ---------------------------------------------------------------------------
inline ChannelSettings fitEq (const Correction& c, int hp, int lp)
{
    constexpr int kPts = 25;
    double hz[kPts], want[kPts], w[kPts];
    for (int i = 0; i < kPts; ++i)
    {
        hz[i] = 28.0 * std::pow (2.0, i * (std::log2 (17000.0 / 28.0) / (kPts - 1)));
        want[i] = wantedAt (c, hz[i]);
        w[i] = (hz[i] < 45.0 || hz[i] > 12000.0) ? 0.5 : 1.0;
    }
    ChannelSettings cs;
    cs.hp = hp; cs.lp = lp; cs.trim = 5; cs.eqIn = true;
    for (auto& b : cs.band) { b.gainStep = 0; b.mode = Out; b.freqIndex = 5; b.bwStep = 4; b.shelf = false; }

    // The filters are set by the genre; the EQ should only fix the tone, so judge it without them
    auto error = [&] (const ChannelSettings& s)
    {
        ChannelSettings eqOnly = s; eqOnly.hp = 0; eqOnly.lp = 0;
        double e = 0.0;
        for (int i = 0; i < kPts; ++i)
        {
            const double d = channelResponseDb (eqOnly, hz[i]) - want[i];
            e += w[i] * d * d;
        }
        double cost = 0.0;                                   // prefer fewer, gentler, broader moves
        for (int b = 0; b < kNumBands; ++b)
            if (s.band[b].mode != Out && s.band[b].gainStep > 0)
                cost += 2.0 + 0.08 * std::abs (bandSignedDb (s.band[b])) + 0.04 * s.band[b].bwStep;
        return e / kPts + cost * 0.15;
    };

    double best = error (cs);
    static const int bwChoices[] = { 0, 2, 4, 6, 8 };
    for (int pass = 0; pass < 3; ++pass)
    {
        bool improved = false;
        for (int b = 0; b < kNumBands; ++b)
        {
            ChannelSettings trial = cs, bestS = cs;
            trial.band[b].mode = Out; trial.band[b].gainStep = 0;
            double e = error (trial);
            if (e < best - 1e-9) { best = e; bestS = trial; }
            const bool canShelf = (b == 0 || b == kNumBands - 1);
            for (int shelf = 0; shelf <= (canShelf ? 1 : 0); ++shelf)
                for (int f = 0; f < kNumFreqSteps; ++f)
                {
                    if (shelf && b == 0 && f > 6) continue;                 // low shelf up to 220 Hz
                    if (shelf && b == 3 && f < 5) continue;                 // high shelf from 3.9 kHz
                    for (int mode : { Boost, Cut })
                        for (int bw : bwChoices)
                            for (int gs = 1; gs <= 10; ++gs)
                            {
                                trial.band[b] = { f, gs, bw, mode, shelf == 1 };
                                e = error (trial);
                                if (e < best - 1e-9) { best = e; bestS = trial; }
                            }
                }
            if (! (bestS.band[b].freqIndex == cs.band[b].freqIndex && bestS.band[b].gainStep == cs.band[b].gainStep
                   && bestS.band[b].bwStep == cs.band[b].bwStep && bestS.band[b].mode == cs.band[b].mode
                   && bestS.band[b].shelf == cs.band[b].shelf))
                improved = true;
            cs = bestS;
        }
        if (! improved) break;
    }
    for (auto& b : cs.band) if (b.mode == Out) { b.gainStep = 0; b.freqIndex = 5; b.bwStep = 7; b.shelf = false; b.mode = Boost; }
    return cs;
}

// Human-readable list of the EQ moves
inline std::string describeEq (const ChannelSettings& cs)
{
    static const char* names[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
    std::string out;
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& s = cs.band[b];
        const double db = bandSignedDb (s);
        if (std::abs (db) < 0.05) continue;
        const double f = kBandFreqs[b][s.freqIndex];
        char fq[16], buf[96];
        if (f >= 1000) std::snprintf (fq, sizeof (fq), f >= 10000 || std::fmod (f, 1000.0) < 0.5 ? "%.0fk" : "%.1fk", f / 1000.0);
        else           std::snprintf (fq, sizeof (fq), "%.0f", f);
        std::snprintf (buf, sizeof (buf), "%s%s %s %s %+.1f dB", out.empty() ? "" : ",  ", names[b], fq,
                       s.shelf ? "shelf" : "bell", db);
        out += buf;
    }
    return out.empty() ? std::string ("EQ flat (already close to the target)") : out;
}
} // namespace ppm::automaster
