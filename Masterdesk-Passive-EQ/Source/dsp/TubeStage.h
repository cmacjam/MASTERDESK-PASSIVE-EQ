// =============================================================================
//  TubeStage.h  —  make-up amplifier colour: triode curvature + output iron
//
//  Calibrated so the stage is unity-gain for small signals (the network's
//  insertion loss is already made up in PassiveNetwork). Harmonics rise with
//  level like the real thing: at the default drive a 0 dBFS sine carries a few
//  tenths of a percent, mostly 2nd harmonic; low-level detail stays clean.
//
//    1. Output-transformer core: low frequencies (flux ~ ∫V) saturate first,
//       adding weight below ~80 Hz.
//    2. Triode transfer: biased tanh -> asymmetric -> even + odd harmonics,
//       normalised to unity small-signal slope.
//    3. DC blocker (the asymmetry produces a small DC term).
//
//  Runs at the oversampled rate, so the harmonics don't alias.
// =============================================================================
#pragma once
#include "PassiveNetwork.h"

namespace ppm
{
class TubeStage
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        lpCoef = 1.0 - std::exp (-2.0 * kPi * 80.0 / fs);      // transformer "flux" band
        dcCoef = 1.0 - std::exp (-2.0 * kPi * 4.0  / fs);      // 4 Hz DC blocker
        mixStep = 1.0 / (0.020 * fs);
        reset();
        setDrive (driveDb);
    }

    void reset() { lp = 0.0; dcX = dcY = 0.0; }

    // driveDb: how hard the signal hits the stage (output is level-compensated)
    void setDrive (double dB)
    {
        driveDb = dB;
        drive   = std::pow (10.0, dB / 20.0);
    }
    void setEnabled (bool e) { targetMix = e ? 1.0 : 0.0; }

    inline double process (double x)
    {
        if (targetMix <= 0.0 && mix <= 0.0) return x;

        // --- 1. output iron: low band saturates gently -----------------------
        lp += lpCoef * (x - lp);
        const double lowDrive = lp * drive * kIronDrive;
        const double ironSat  = (std::tanh (lowDrive) - lowDrive) / (kIronDrive * drive);
        double y = x + ironSat;

        // --- 2. triode curvature ---------------------------------------------
        const double u = y * drive;
        const double t = (std::tanh (kCurve * (u + kBias)) - kTanhBias) * kNorm;
        y = t / drive;

        // --- 3. DC-block only the added harmonics, so the clean signal path
        //        stays exactly linear (no low-frequency phase shift) ---------
        double dist = y - x;
        dcY += dcCoef * (dist - dcY);
        dist -= dcY;

        if (std::abs (mix - targetMix) > 0.0)
            mix = targetMix > mix ? std::min (targetMix, mix + mixStep) : std::max (targetMix, mix - mixStep);

        dist *= mix;
        distEnergy += dist * dist;     // for the HARMONICS meter
        inEnergy   += x * x;
        return x + dist;
    }

    // Harmonic content added since the last call, as a ratio (0.01 = 1 %).
    double takeDistortionRatio()
    {
        const double r = inEnergy > 1e-12 ? std::sqrt (distEnergy / inEnergy) : 0.0;
        distEnergy = inEnergy = 0.0;
        return r;
    }

private:
    // Calibration (tuned in tests/model_test.cpp): at 0 dB drive and a 0 dBFS
    // 1 kHz sine: H2 ≈ -54 dBc, H3 ≈ -66 dBc (≈0.2 % THD). +12 dB drive: ≈ -43 / -42 dBc.
    static constexpr double kCurve    = 0.08;
    static constexpr double kBias     = 0.60;
    static constexpr double kIronDrive = 0.35;
    inline static const double kTanhBias = std::tanh (kCurve * kBias);
    inline static const double kNorm     = 1.0 / (kCurve * (1.0 - kTanhBias * kTanhBias));

    double fs = 192000.0, lpCoef = 0, dcCoef = 0, lp = 0, dcX = 0, dcY = 0;
    double drive = 1.0, driveDb = 0.0;
    double mix = 1.0, targetMix = 1.0, mixStep = 0.001;
    double distEnergy = 0.0, inEnergy = 0.0;
};

// ---------------------------------------------------------------------------
// One full channel strip at the oversampled rate:
//   HP filter -> passive parallel network (+make-up) -> LP filter -> tube -> trim
class ChannelEngine
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hp.prepare (true, fs);
        lp.prepare (false, fs);
        net.prepare (fs);
        tube.prepare (fs);
        fadeStep = 1.0 / (0.020 * fs);
        trimCoef = 1.0 - std::exp (-1.0 / (0.015 * fs));
        eqMix = eqTarget; trimCur = trimTgt;
    }

    void setSettings (const ChannelSettings& c, bool tubeOn, double driveDb)
    {
        hp.setPosition (c.hp);
        lp.setPosition (c.lp);
        net.setTargets (c);
        eqTarget = c.eqIn ? 1.0 : 0.0;
        trimTgt  = std::pow (10.0, trimDb (c.trim) / 20.0);
        tube.setEnabled (tubeOn);
        tube.setDrive (driveDb);
    }

    inline double process (double x)
    {
        double eq = x;
        if (eqMix > 0.0 || eqTarget > 0.0)
        {
            eq = hp.process (x);
            eq = net.process (eq);
            eq = lp.process (eq);
        }
        if (std::abs (eqMix - eqTarget) > 0.0)
            eqMix = eqTarget > eqMix ? std::min (1.0, eqMix + fadeStep) : std::max (0.0, eqMix - fadeStep);
        double y = x + eqMix * (eq - x);

        y = tube.process (y);
        trimCur += trimCoef * (trimTgt - trimCur);
        return y * trimCur;
    }

    double takeDistortionRatio() { return tube.takeDistortionRatio(); }

private:
    SwitchedFilter hp, lp;
    PassiveNetwork net;
    TubeStage tube;
    double fs = 192000.0, eqMix = 1.0, eqTarget = 1.0, fadeStep = 0.001;
    double trimCur = 1.0, trimTgt = 1.0, trimCoef = 0.001;
};
} // namespace ppm
