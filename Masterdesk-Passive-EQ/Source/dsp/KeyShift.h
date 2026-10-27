// =============================================================================
//  KeyShift.h  —  real-time key (pitch) shifter for full stereo mixes
//
//  Peak-locked phase vocoder pitch shifting (Laroche & Dolson, "New phase-vocoder
//  techniques for real-time pitch shifting", 1999):
//    * Each STFT frame is split into regions around spectral peaks.
//    * Each region is moved, as a block, to the bin of the shifted frequency;
//      its phase is rotated by an amount that accumulates frame to frame, so the
//      shifted partial stays continuous. Moving whole regions keeps the shape of
//      each partial, which sounds much cleaner than bin-by-bin shifting.
//    * Peaks, frequencies and phase rotations are measured once from the
//      combined L+R energy and applied identically to both channels, so the
//      phase difference between left and right is preserved exactly: the stereo
//      image and mono compatibility don't change.
//    * Strong onsets reset the phase rotation, which keeps drum hits sharper.
//    * Noise-like regions (no stable frequency from frame to frame) overlap-add
//      partly incoherently and would come out quieter (by 0.2-7 dB depending on
//      the shift); each region's stability is measured and unstable ones get
//      the measured compensating gain, so
//      cymbals, breath and reverb keep their level while steady tones are untouched.
//  The time-domain resolution is the same at every sample rate (frame ~85 ms).
//
//  Latency: frameSize - hop samples (3584 at 44.1/48 kHz). Plain C++17.
// =============================================================================
#pragma once
#include "FFT.h"
#include <algorithm>
#include <array>
#include <memory>

namespace ppm
{
class KeyShifter
{
public:
    void prepare (double sampleRate)
    {
        order = sampleRate > 150000.0 ? 14 : (sampleRate > 60000.0 ? 13 : 12);
        N = 1 << order;  H = N / 8;
        // FFT is 4x longer than the window (zero padding): a finer frequency grid keeps
        // shifted partials at full level (the residual phase error is spread over 1/4 of a cycle)
        M = N * kPad;  half = M / 2;
        fft = std::make_unique<FFT> (order + 2);
        window.resize ((size_t) N);
        for (int i = 0; i < N; ++i) window[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * kPi_ * i / N);
        // Hann analysis x Hann synthesis at 8x overlap sums to 3
        olaScale = 1.0 / 3.0;
        for (auto* v : { &inL, &inR, &outL, &outR }) v->assign ((size_t) N, 0.0);
        for (auto* v : { &accL, &accR }) v->assign ((size_t) (2 * N), 0.0);
        buf.assign ((size_t) M, {});
        XL.assign ((size_t) half + 1, {}); XR.assign ((size_t) half + 1, {});
        YL.assign ((size_t) half + 1, {}); YR.assign ((size_t) half + 1, {});
        mag.assign ((size_t) half + 1, 0.0);
        prevPhL.assign ((size_t) half + 1, 0.0); prevPhR.assign ((size_t) half + 1, 0.0);
        prevTheta.assign ((size_t) half + 1, 0.0); newTheta.assign ((size_t) half + 1, 0.0);
        prevOmega.assign ((size_t) half + 1, 0.0); newOmega.assign ((size_t) half + 1, 0.0);
        prevOwner.assign ((size_t) half + 1, -1); newOwner.assign ((size_t) half + 1, -1);
        peaks.reserve ((size_t) half); bounds.reserve ((size_t) half);
        reset();
    }

    void reset()
    {
        std::fill (inL.begin(), inL.end(), 0.0);  std::fill (inR.begin(), inR.end(), 0.0);
        std::fill (outL.begin(), outL.end(), 0.0); std::fill (outR.begin(), outR.end(), 0.0);
        std::fill (accL.begin(), accL.end(), 0.0); std::fill (accR.begin(), accR.end(), 0.0);
        std::fill (prevPhL.begin(), prevPhL.end(), 0.0); std::fill (prevPhR.begin(), prevPhR.end(), 0.0);
        std::fill (prevTheta.begin(), prevTheta.end(), 0.0);
        std::fill (prevOmega.begin(), prevOmega.end(), 0.0);
        std::fill (prevOwner.begin(), prevOwner.end(), -1);
        rover = N - H;
        prevEnergy = 0.0;
    }

    int latency() const { return N; }                 // samples of delay through the shifter

    // Shift in semitones (fractional allowed, e.g. +0.5 = 50 cents). Can change while running.
    void setShift (double semitones)
    {
        ratio = std::pow (2.0, semitones / 12.0);
        // measured level loss of noise-like content at this shift (white noise, 48 kHz): compensated below
        static const double st[] = { -12, -7, -5, -3, -2, -1, -0.5, -0.1, 0, 0.1, 0.5, 1, 2, 3, 5, 7, 12 };
        static const double db[] = { -2.71, -2.25, -1.96, -1.49, -1.16, -0.79, -0.57, -0.29, 0, -0.32, -0.71, -1.08, -1.75, -2.39, -3.47, -4.41, -6.82 };
        const double x = std::clamp (semitones, -12.0, 12.0);
        int i = 0; while (i < 15 && x > st[i + 1]) ++i;
        const double t = (x - st[i]) / (st[i + 1] - st[i]);
        const double lossDb = db[i] + t * (db[i + 1] - db[i]);
        noisePowerGain = std::pow (10.0, -lossDb / 10.0);
    }

    // In-place stereo processing. For mono pass the same pointer twice.
    void process (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            inL[(size_t) rover] = l[i];
            inR[(size_t) rover] = r[i];
            const float ol = (float) outL[(size_t) (rover - (N - H))];
            const float orr = (float) outR[(size_t) (rover - (N - H))];
            l[i] = ol;
            if (r != l) r[i] = orr;
            if (++rover >= N)
            {
                rover = N - H;
                processFrame();
            }
        }
    }

private:
    static constexpr double kPi_ = 3.14159265358979323846;
    static double princarg (double p) { return p - 2.0 * kPi_ * std::floor ((p + kPi_) / (2.0 * kPi_)); }

    void processFrame()
    {
        using cpx = FFT::cpx;
        // --- analysis: both channels in one complex FFT (L + jR) -------------------------
        for (int i = 0; i < N; ++i) buf[(size_t) i] = cpx (inL[(size_t) i] * window[(size_t) i], inR[(size_t) i] * window[(size_t) i]);
        std::fill (buf.begin() + N, buf.end(), cpx());
        fft->transform (buf.data(), false);
        double energy = 0.0, maxMag = 0.0;
        for (int k = 0; k <= half; ++k)
        {
            const cpx a = buf[(size_t) k], b = std::conj (buf[(size_t) ((M - k) % M)]);
            XL[(size_t) k] = 0.5 * (a + b);
            XR[(size_t) k] = cpx (0.0, -0.5) * (a - b);
            const double m2 = std::norm (XL[(size_t) k]) + std::norm (XR[(size_t) k]);
            mag[(size_t) k] = std::sqrt (m2);
            energy += m2;
            maxMag = std::max (maxMag, mag[(size_t) k]);
        }
        // onset: energy jumps by > 8 dB -> reset phase rotations (keeps transients tight)
        const bool onset = energy > prevEnergy * 6.3 && energy > 1e-6;
        prevEnergy = energy;

        // --- peaks and their regions ----------------------------------------------------------
        peaks.clear();
        const double floorMag = std::max (maxMag * 1e-6, 1e-12);
        for (int k = kPad * 2; k <= half - kPad * 2; ++k)
        {
            const double m = mag[(size_t) k];
            bool isPeak = m > floorMag;
            for (int d = 1; d <= kPad * 2 && isPeak; ++d)          // local maximum over +-2 original bins
                isPeak = m > mag[(size_t) (k - d)] && m >= mag[(size_t) (k + d)];
            if (isPeak) peaks.push_back (k);
        }

        std::fill (YL.begin(), YL.end(), cpx());
        std::fill (YR.begin(), YR.end(), cpx());
        std::fill (newOwner.begin(), newOwner.end(), -1);
        const double binW = 2.0 * kPi_ / M;
        const bool identity = std::abs (ratio - 1.0) < 1e-9;

        // region boundaries: the lowest bin between each pair of neighbouring peaks
        bounds.clear();
        for (size_t pi = 0; pi + 1 < peaks.size(); ++pi)
        {
            int mn = peaks[pi] + 1;
            for (int k = peaks[pi] + 1; k < peaks[pi + 1]; ++k) if (mag[(size_t) k] < mag[(size_t) mn]) mn = k;
            bounds.push_back (mn);
        }

        for (size_t pi = 0; pi < peaks.size(); ++pi)
        {
            const int p = peaks[pi];
            // every bin belongs to exactly one peak's region
            const int lo = pi == 0 ? 0 : bounds[pi - 1];
            const int hi = pi + 1 == peaks.size() ? half : bounds[pi] - 1;

            // instantaneous frequency of the peak, from the louder channel's phase advance
            const bool useL = std::abs (XL[(size_t) p]) >= std::abs (XR[(size_t) p]);
            const double ph = useL ? std::arg (XL[(size_t) p]) : std::arg (XR[(size_t) p]);
            const double prevPh = useL ? prevPhL[(size_t) p] : prevPhR[(size_t) p];
            const double omega = binW * p + princarg (ph - prevPh - binW * p * H) / H;

            int shift = 0;
            double theta = 0.0, gain = 1.0;
            if (! identity)
            {
                // move by the peak's bin position (stable from frame to frame, even for noise);
                // the exact target frequency comes from the phase rotation below
                shift = (int) std::lround (p * (ratio - 1.0));
                const int owner = prevOwner[(size_t) p];
                const double thetaPrev = (onset || owner < 0) ? 0.0 : prevTheta[(size_t) owner];
                theta = onset ? 0.0 : princarg (thetaPrev + (ratio - 1.0) * omega * H);

                // stability: how well this peak's frequency matches the previous frame's peak here
                double stable = 0.0;
                if (owner >= 0 && ! onset)
                {
                    const double tol = 0.25 * 2.0 * kPi_ / N;              // a quarter of a window bin
                    const double d = (omega - prevOmega[(size_t) owner]) / tol;
                    stable = std::exp (-d * d);
                }
                // noise-like regions lose level in the overlap-add: give them back what was measured for this shift
                gain = std::sqrt (stable + (1.0 - stable) * noisePowerGain);
            }
            newTheta[(size_t) p] = theta;
            newOmega[(size_t) p] = omega;
            const cpx rot = std::polar (gain, theta);
            for (int k = lo; k <= hi; ++k)
            {
                newOwner[(size_t) k] = p;
                const int kk = k + shift;
                if (kk < 0 || kk > half) continue;
                YL[(size_t) kk] += XL[(size_t) k] * rot;
                YR[(size_t) kk] += XR[(size_t) k] * rot;
            }
        }
        if (peaks.empty())                         // silence / no tonal content: pass through
            for (int k = 0; k <= half; ++k) { YL[(size_t) k] = XL[(size_t) k]; YR[(size_t) k] = XR[(size_t) k]; }

        for (int k = 0; k <= half; ++k)
        {
            prevPhL[(size_t) k] = std::arg (XL[(size_t) k]);
            prevPhR[(size_t) k] = std::arg (XR[(size_t) k]);
        }
        std::swap (prevTheta, newTheta);
        std::swap (prevOmega, newOmega);
        std::swap (prevOwner, newOwner);

        // --- synthesis: one complex IFFT gives both channels (L real part, R imaginary part) ---
        YL[0] = cpx (YL[0].real(), 0.0); YR[0] = cpx (YR[0].real(), 0.0);
        YL[(size_t) half] = cpx (YL[(size_t) half].real(), 0.0); YR[(size_t) half] = cpx (YR[(size_t) half].real(), 0.0);
        for (int k = 0; k <= half; ++k)
            buf[(size_t) k] = YL[(size_t) k] + cpx (0.0, 1.0) * YR[(size_t) k];
        for (int k = 1; k < half; ++k)
            buf[(size_t) (M - k)] = std::conj (YL[(size_t) k]) + cpx (0.0, 1.0) * std::conj (YR[(size_t) k]);
        fft->transform (buf.data(), true);

        const double sc = olaScale / M;
        for (int i = 0; i < N; ++i)
        {
            accL[(size_t) i] += buf[(size_t) i].real() * window[(size_t) i] * sc;
            accR[(size_t) i] += buf[(size_t) i].imag() * window[(size_t) i] * sc;
        }
        for (int i = 0; i < H; ++i) { outL[(size_t) i] = accL[(size_t) i]; outR[(size_t) i] = accR[(size_t) i]; }
        std::move (accL.begin() + H, accL.begin() + H + N, accL.begin());
        std::move (accR.begin() + H, accR.begin() + H + N, accR.begin());
        std::move (inL.begin() + H, inL.end(), inL.begin());
        std::move (inR.begin() + H, inR.end(), inR.begin());
    }

    static constexpr int kPad = 4;
    int order = 12, N = 4096, M = 16384, H = 512, half = 8192, rover = 0;
    double ratio = 1.0, olaScale = 1.0 / 3.0, prevEnergy = 0.0, noisePowerGain = 1.0;
    std::unique_ptr<FFT> fft;
    std::vector<double> window, inL, inR, outL, outR, accL, accR, mag, prevPhL, prevPhR, prevTheta, newTheta, prevOmega, newOmega;
    std::vector<int> prevOwner, newOwner, peaks, bounds;
    std::vector<FFT::cpx> buf, XL, XR, YL, YR;
};
} // namespace ppm
