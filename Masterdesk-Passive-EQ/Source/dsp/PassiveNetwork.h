// =============================================================================
//  PassiveNetwork.h  —  real-time discretisation of the parallel passive network
//
//  Every branch admittance is discretised with the bilinear transform
//  (trapezoidal rule, pre-warped at the branch frequency), the same thing a
//  SPICE companion model does for each L and C. Each discretised branch has an
//  instantaneous part (b0) and a state part, so the single node voltage can be
//  solved exactly every sample — no unit delay in the loop, no series biquads.
//
//  Runs in double precision at the oversampled rate (>= 176.4 kHz) so the
//  27 kHz shelf and 52 kHz low-pass behave like the analog curves.
// =============================================================================
#pragma once
#include "Model.h"
#include <array>

namespace ppm
{
// ---------------------------------------------------------------------------
struct Biquad   // Transposed Direct Form II, double precision
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double s1 = 0, s2 = 0;

    void reset() { s1 = s2 = 0; }
    inline double process (double x)
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
};

// Bilinear transform of  (n2 s^2 + n1 s + n0) / (s^2 + d1 s + d0),
// pre-warped so that analog frequency w0 lands exactly at w0.
inline void bilinear2 (Biquad& q, double n2, double n1, double n0, double d1, double d0,
                       double w0, double fs)
{
    const double T  = 1.0 / fs;
    const double wd = std::min (w0, 0.98 * kPi * fs);           // keep tan() finite
    const double c  = wd / std::tan (0.5 * wd * T);
    const double c2 = c * c;
    const double a0 = c2 + d1 * c + d0;
    q.b0 = (n2 * c2 + n1 * c + n0) / a0;
    q.b1 = 2.0 * (n0 - n2 * c2)    / a0;
    q.b2 = (n2 * c2 - n1 * c + n0) / a0;
    q.a1 = 2.0 * (d0 - c2)         / a0;
    q.a2 = (c2 - d1 * c + d0)      / a0;
}

inline void bilinear1 (Biquad& q, double n1, double n0, double wc, double fs)   // (n1 s + n0)/(s + wc)
{
    const double c  = wc / std::tan (0.5 * wc / fs);
    const double a0 = c + wc;
    q.b0 = (n1 * c + n0) / a0;
    q.b1 = (n0 - n1 * c) / a0;
    q.b2 = 0.0;
    q.a1 = (wc - c) / a0;
    q.a2 = 0.0;
}

// ---------------------------------------------------------------------------
// Butterworth HP/LP of order 1..6 as a biquad cascade.
struct ButterworthCascade
{
    std::array<Biquad, 3> sec;
    int numSec = 0;

    void design (bool highPass, int order, double fc, double fs)
    {
        numSec = 0;
        if (order <= 0 || fc <= 0.0) return;
        fc = std::min (fc, 0.45 * fs);
        const double wc = 2.0 * kPi * fc;
        for (int k = 1; k <= order; ++k)
        {
            const double ang = kPi * (2.0 * k + order - 1.0) / (2.0 * order);
            const double re  = std::cos (ang);
            const double im  = std::sin (ang);
            if (im > 1e-9)                                       // one of each conjugate pair
            {
                const double Q = -1.0 / (2.0 * re);
                auto& q = sec[(size_t) numSec++];
                if (highPass) bilinear2 (q, 1, 0, 0,       wc / Q, wc * wc, wc, fs);
                else          bilinear2 (q, 0, 0, wc * wc, wc / Q, wc * wc, wc, fs);
            }
            else if (std::abs (im) <= 1e-9)                      // real pole (odd order)
            {
                auto& q = sec[(size_t) numSec++];
                if (highPass) bilinear1 (q, 1, 0,  wc, fs);
                else          bilinear1 (q, 0, wc, wc, fs);
            }
        }
    }
    void reset() { for (auto& s : sec) s.reset(); }
    inline double process (double x)
    {
        for (int i = 0; i < numSec; ++i) x = sec[(size_t) i].process (x);
        return x;
    }
};

// Click-free switched filter: crossfades between two cascades on change.
class SwitchedFilter
{
public:
    void prepare (bool isHighPass, double sampleRate)
    {
        hp = isHighPass; fs = sampleRate;
        fadeStep = 1.0 / (0.030 * fs);          // 30 ms crossfade
        for (auto& f : filt) { f.design (hp, 0, 0, fs); f.reset(); }
        pos[0] = pos[1] = pending = 0; active = 0; fade = 1.0;
    }

    void setPosition (int p)
    {
        pending = p;
        if (fade >= 1.0 && p != pos[active]) startFade();
    }

    inline double process (double x)
    {
        if (fade >= 1.0) return filt[active].process (x);
        const double a = filt[active].process (x);
        const double b = filt[1 - active].process (x);
        fade += fadeStep;
        const double g = std::min (fade, 1.0);
        const double y = a * (1.0 - g) + b * g;
        if (fade >= 1.0)
        {
            active = 1 - active;
            if (pending != pos[active]) startFade();
        }
        return y;
    }

private:
    void startFade()
    {
        const size_t other = 1 - active;
        pos[other] = pending;
        const auto& freqs = hp ? kHpFreqs : kLpFreqs;
        const auto& ords  = hp ? kHpOrder : kLpOrder;
        filt[other].design (hp, ords[pending], freqs[pending], fs);
        filt[other].reset();
        fade = 0.0;
    }

    std::array<ButterworthCascade, 2> filt;
    int pos[2] {}, pending = 0;
    size_t active = 0;
    double fade = 1.0, fadeStep = 0.001, fs = 48000.0;
    bool hp = true;
};

// ---------------------------------------------------------------------------
// One resonant admittance branch with smoothed parameters.
class AdmittanceBranch
{
public:
    void prepare (double sampleRate, int updateInterval)
    {
        fs = sampleRate;
        // one-pole smoothing evaluated once per update tick, ~20 ms time constant
        smooth = 1.0 - std::exp (-double (updateInterval) / (0.020 * fs));
        curG = tgtG = 0.0; q.reset(); dirty = true;
    }

    void setTarget (Shape shape, double G, double freqHz, double Q)
    {
        tgtShape = shape; tgtG = G; tgtLogW = std::log (2.0 * kPi * freqHz); tgtLogQ = std::log (Q);
        if (! initialised) { curShape = shape; curLogW = tgtLogW; curLogQ = tgtLogQ; initialised = true; dirty = true; }
    }

    // Called every `updateInterval` samples.
    void tick()
    {
        // Shape changes: fade the branch out, swap shape, fade back in.
        const double goalG = (tgtShape != curShape) ? 0.0 : tgtG;
        if (tgtShape != curShape && curG < 1e-5)
        {
            curShape = tgtShape; curG = 0.0; q.reset(); dirty = true;
        }

        const double dG = goalG - curG, dW = tgtLogW - curLogW, dQ = tgtLogQ - curLogQ;
        if (std::abs (dG) > 1e-7 || std::abs (dW) > 1e-6 || std::abs (dQ) > 1e-6)
        {
            curG    += dG * smooth;
            curLogW += dW * smooth;
            curLogQ += dQ * smooth;
            if (std::abs (goalG - curG) < 1e-7) curG = goalG;
            dirty = true;
        }
        if (dirty) { recalc(); dirty = false; }
    }

    bool isActive() const { return curG > 0.0 || tgtG > 0.0; }

    double b0() const { return q.b0; }
    double state() const { return q.s1; }

    // Advance state with the branch voltage v (after the node is solved).
    inline void commit (double v)
    {
        const double i = q.b0 * v + q.s1;
        q.s1 = q.b1 * v - q.a1 * i + q.s2;
        q.s2 = q.b2 * v - q.a2 * i;
    }

    void resetIfSilent() { if (curG <= 0.0 && tgtG <= 0.0) q.reset(); }

private:
    void recalc()
    {
        const double w0 = std::exp (curLogW), Q = std::exp (curLogQ), G = curG;
        switch (curShape)
        {
            case BandPass: bilinear2 (q, 0, G * w0 / Q, 0,          w0 / Q, w0 * w0, w0, fs); break;
            case LowPass:  bilinear2 (q, 0, 0,          G * w0 * w0, w0 / Q, w0 * w0, w0, fs); break;
            case HighPass: bilinear2 (q, G, 0,          0,          w0 / Q, w0 * w0, w0, fs); break;
        }
    }

    Biquad q;
    double fs = 192000.0, smooth = 0.01;
    double curG = 0, tgtG = 0, curLogW = 0, tgtLogW = 0, curLogQ = 0, tgtLogQ = 0;
    Shape  curShape = BandPass, tgtShape = BandPass;
    bool   dirty = true, initialised = false;
};

// ---------------------------------------------------------------------------
// The four-band parallel network. Each band owns a series-arm (boost) and a
// shunt-arm (cut) branch so Boost <-> Cut moves glide instead of clicking.
class PassiveNetwork
{
public:
    static constexpr int kUpdateInterval = 16;

    void prepare (double sampleRate)
    {
        for (auto& b : series) b.prepare (sampleRate, kUpdateInterval);
        for (auto& b : shunt)  b.prepare (sampleRate, kUpdateInterval);
        counter = 0;
    }

    void setTargets (const ChannelSettings& c)
    {
        for (int i = 0; i < kNumBands; ++i)
        {
            const auto t = computeBranch (i, c.band[i]);
            series[(size_t) i].setTarget (t.shape, t.series ? t.G : 0.0, t.freqHz, t.Q);
            shunt [(size_t) i].setTarget (t.shape, t.series ? 0.0 : t.G, t.freqHz, t.Q);
        }
    }

    inline double process (double vin)
    {
        if (counter-- <= 0)
        {
            counter = kUpdateInterval - 1;
            for (auto& b : series) { b.tick(); b.resetIfSilent(); }
            for (auto& b : shunt)  { b.tick(); b.resetIfSilent(); }
        }

        // Solve KCL at the single output node:
        // (Gs + Σb0s)(Vin - Vo) + Σs_s  =  (Gp + Σb0p) Vo + Σs_p
        double gS = kGs, gP = kGp, hS = 0.0, hP = 0.0;
        for (auto& b : series) if (b.isActive()) { gS += b.b0(); hS += b.state(); }
        for (auto& b : shunt)  if (b.isActive()) { gP += b.b0(); hP += b.state(); }

        const double vo = (vin * gS + hS - hP) / (gS + gP);

        for (auto& b : series) if (b.isActive()) b.commit (vin - vo);
        for (auto& b : shunt)  if (b.isActive()) b.commit (vo);

        return vo * kInsertionK;     // tube make-up gain restores unity at DC
    }

private:
    std::array<AdmittanceBranch, kNumBands> series, shunt;
    int counter = 0;
};
} // namespace ppm
