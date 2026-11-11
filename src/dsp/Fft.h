/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 : transformée de Fourier rapide (radix 2, sur place), sans dépendance : le moteur l'emploie
    hors de JUCE (restauration, traitement hors temps réel). Facteurs et ordre des bits calculés dans
    prepare() ; forward / inverse sans allocation. L'inverse est normalisée (divisée par N).
*/

#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace grv::dsp
{

class Fft
{
public:
    void prepare (int order)
    {
        n = 1 << order;
        tw.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k) tw[(size_t) k] = std::polar (1.0f, (float) (-2.0 * 3.14159265358979323846 * k / n));
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
    }
    int size() const noexcept { return n; }
    void forward (std::complex<float>* x) const noexcept { run (x, false); }
    void inverse (std::complex<float>* x) const noexcept
    {
        run (x, true);
        const float s = 1.0f / (float) n;
        for (int i = 0; i < n; ++i) x[i] *= s;
    }

private:
    void run (std::complex<float>* x, bool inv) const noexcept
    {
        for (int i = 0; i < n; ++i) if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const auto u = x[i + j], v = x[i + j + half] * w;
                    x[i + j] = u + v;
                    x[i + j + half] = u - v;
                }
        }
    }
    int n = 0;
    std::vector<std::complex<float>> tw;
    std::vector<int> rev;
};

} // namespace grv::dsp
