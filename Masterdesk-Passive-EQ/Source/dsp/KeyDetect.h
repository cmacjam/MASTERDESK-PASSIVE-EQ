// =============================================================================
//  KeyDetect.h  —  musical key detection from audio
//
//   1. FFT of the newest ~0.34 s (16384 samples at 48 kHz, scaled with rate).
//   2. Pitch salience for every semitone from E1 to C7 (A = 440 Hz): the sum of
//      that note's first 8 harmonics, weighted down for higher harmonics. This
//      credits the fundamental rather than its overtones (e.g. the 5th harmonic
//      of a bass E is a G#, which would otherwise make E minor look like E major).
//      Salience is folded onto the 12 pitch classes (chromagram).
//   3. A slowly-moving average of the chroma (about 20 s memory), so the result
//      follows the song but doesn't flicker on single chords.
//   4. Correlation with the Temperley-Kostka-Payne major and minor key profiles for
//      all 24 keys; the best match is the key, and the gap to the runner-up is
//      the confidence.
//  Plain C++17; tested in tests/key_test.cpp.
// =============================================================================
#pragma once
#include "FFT.h"
#include <array>
#include <algorithm>
#include <memory>

namespace ppm
{
struct KeyResult
{
    int    key = -1;          // 0..11 = C..B major, 12..23 = C..B minor, -1 = not enough signal yet
    double confidence = 0.0;  // 0..1
};

inline const char* pitchName (int pc)
{
    static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[((pc % 12) + 12) % 12];
}

// Camelot wheel code (used by DJs for harmonic mixing), e.g. A minor = 8A, C major = 8B
inline int camelotNumber (int key)
{
    const int pc = key % 12;
    const bool minor = key >= 12;
    const int relMajor = minor ? (pc + 3) % 12 : pc;           // minor keys share the number of their relative major
    static const int majNum[12] = { 8, 3, 10, 5, 12, 7, 2, 9, 4, 11, 6, 1 };   // C, C#/Db, D, Eb, E, F, F#, G, Ab, A, Bb, B
    return majNum[relMajor];
}

// Key signature: positive = number of sharps, negative = number of flats
inline int keySignature (int key)
{
    const int pc = key % 12;
    const int relMajor = key >= 12 ? (pc + 3) % 12 : pc;
    static const int sig[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };    // C Db D Eb E F F# G Ab A Bb B
    return sig[relMajor];
}

class KeyDetector
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        order = fs > 150000.0 ? 16 : (fs > 60000.0 ? 15 : 14);     // ~0.34 s window: resolves semitones down to ~100 Hz
        N = 1 << order;
        fft = std::make_unique<FFT> (order);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / N);
        buf.assign ((size_t) N, {});
        // semitone candidates E1 (28) .. C7 (96); salience is read from the spectrum at each harmonic.
        // Harmonics below the frequency where one FFT bin is narrower than half a semitone are skipped:
        // there neighbouring notes can't be told apart, so low notes are recognised by their upper harmonics.
        const double binHz = fs / (1 << order);
        const double minResolved = binHz / (std::pow (2.0, 1.0 / 24.0) - 1.0);
        notes.clear();
        for (int midi = 28; midi <= 96; ++midi)
        {
            Note nt; nt.pc = midi % 12;
            const double f0 = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            for (int h = 1; h <= kHarm; ++h)
            {
                const double f = f0 * h;
                const int lo = (int) std::floor (f * std::pow (2.0, -0.4 / 12.0) * N / fs);
                const int hi = (int) std::ceil  (f * std::pow (2.0,  0.4 / 12.0) * N / fs);
                nt.lo[h - 1] = std::clamp (lo, 1, N / 2 - 1);
                nt.hi[h - 1] = std::clamp (std::max (hi, lo), 1, N / 2 - 1);
                nt.ok[h - 1] = f >= minResolved && f < fs * 0.45 && f < 6000.0;
            }
            notes.push_back (nt);
        }
        mag.assign ((size_t) N / 2, 0.0);
        reset();
    }

    void reset() { chroma.fill (0.0); frames = 0; }
    int frameSize() const { return N; }

    // Analyse the newest frameSize() samples (mono). `seconds` = time since the last call.
    void addFrame (const float* mono, double seconds)
    {
        double energy = 0.0;
        for (int i = 0; i < N; ++i) { buf[(size_t) i] = FFT::cpx (mono[i] * win[(size_t) i], 0.0); energy += (double) mono[i] * mono[i]; }
        if (energy / N < 1e-7) return;                                    // below about -70 dBFS: keep the last result
        fft->transform (buf.data(), false);

        for (int k = 0; k < N / 2; ++k) mag[(size_t) k] = std::abs (buf[(size_t) k]);
        std::array<double, 12> c {};
        for (auto& nt : notes)
        {
            double sal = 0.0, w = 1.0;
            for (int h = 0; h < kHarm; ++h, w *= 0.8)
            {
                if (! nt.ok[h]) continue;
                double m = 0.0;
                for (int k = nt.lo[h]; k <= nt.hi[h]; ++k) m = std::max (m, mag[(size_t) k]);
                sal += w * m;
            }
            c[(size_t) nt.pc] += sal;
        }
        double sum = 0.0; for (double v : c) sum += v;
        if (sum <= 0.0) return;

        const double a = 1.0 - std::exp (-seconds / 20.0);               // ~20 s memory
        for (int i = 0; i < 12; ++i) chroma[(size_t) i] += a * (c[(size_t) i] / sum - chroma[(size_t) i]);
        ++frames;
    }

    KeyResult result() const
    {
        KeyResult r;
        if (frames < 8) return r;
        // Temperley-Kostka-Payne key profiles: weight the third (major vs minor) strongly
        static const double major[12] = { 0.748, 0.060, 0.488, 0.082, 0.670, 0.460, 0.096, 0.715, 0.104, 0.366, 0.057, 0.400 };
        static const double minor[12] = { 0.712, 0.084, 0.474, 0.618, 0.049, 0.460, 0.105, 0.747, 0.404, 0.067, 0.133, 0.330 };
        double best = -2.0, second = -2.0;
        for (int key = 0; key < 24; ++key)
        {
            const double* prof = key < 12 ? major : minor;
            const int tonic = key % 12;
            double mx = 0, my = 0;
            for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) i]; my += prof[(i - tonic + 12) % 12]; }
            mx /= 12; my /= 12;
            double sxy = 0, sxx = 0, syy = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double x = chroma[(size_t) i] - mx, y = prof[(i - tonic + 12) % 12] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            const double corr = (sxx > 0 && syy > 0) ? sxy / std::sqrt (sxx * syy) : 0.0;
            if (corr > best) { second = best; best = corr; r.key = key; }
            else if (corr > second) second = corr;
        }
        r.confidence = std::clamp ((best - second) * 8.0 + std::max (0.0, best - 0.5), 0.0, 1.0);
        return r;
    }

    const std::array<double, 12>& getChroma() const { return chroma; }

private:
    double fs = 48000.0;
    int order = 13, N = 8192, frames = 0;
    std::unique_ptr<FFT> fft;
    static constexpr int kHarm = 8;
    struct Note { int pc = 0; int lo[kHarm] {}, hi[kHarm] {}; bool ok[kHarm] {}; };
    std::vector<Note> notes;
    std::vector<double> win, mag;
    std::vector<FFT::cpx> buf;
    std::array<double, 12> chroma {};
};
} // namespace ppm
