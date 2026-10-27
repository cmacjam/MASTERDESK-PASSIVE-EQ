// =============================================================================
//  FFT.h  —  small radix-2 complex FFT (no dependencies, no allocation per call)
// =============================================================================
#pragma once
#include <complex>
#include <vector>
#include <cmath>

namespace ppm
{
class FFT
{
public:
    using cpx = std::complex<double>;

    explicit FFT (int order) : n (1 << order), rev ((size_t) n), tw ((size_t) n / 2)
    {
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        for (int k = 0; k < n / 2; ++k) tw[(size_t) k] = std::polar (1.0, -2.0 * 3.14159265358979323846 * k / n);
    }

    int size() const { return n; }

    // In-place transform of n complex values. inverse = true -> unscaled inverse (divide by n yourself).
    void transform (cpx* x, bool inverse) const
    {
        for (int i = 0; i < n; ++i) if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    cpx w = tw[(size_t) (j * step)];
                    if (inverse) w = std::conj (w);
                    const cpx u = x[i + j], v = x[i + j + half] * w;
                    x[i + j] = u + v;
                    x[i + j + half] = u - v;
                }
        }
    }

private:
    int n;
    std::vector<int> rev;
    std::vector<cpx> tw;
};
} // namespace ppm
