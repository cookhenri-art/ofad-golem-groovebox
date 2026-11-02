/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Tables d'ondes à bande limitée générées par synthèse additive (FFT inverse), une table par
    octave pour qu'aucune harmonique ne dépasse la fréquence de Nyquist. Les formes sont
    calculées par le programme (aucune table d'un instrument existant n'est reprise).
*/

#pragma once

#include "SynthCore.h"

#include <complex>
#include <memory>
#include <vector>

namespace grv::synth
{

/** FFT complexe radix 2 en place (n puissance de 2), sens inverse si inverse = vrai. */
inline void fft (std::vector<std::complex<double>>& a, bool inverse)
{
    const auto n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const auto ang = 2.0 * kPi / (double) len * (inverse ? 1.0 : -1.0);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/** Banque de formes d'onde : numWaves formes × numLevels octaves × tableSize échantillons. */
class WaveBank
{
public:
    static constexpr int tableSize = 2048;
    static constexpr int numLevels = 11;     // niveau l : harmoniques jusqu'à 1024 >> l

    /** spectrum(h) renvoie l'amplitude complexe de l'harmonique h (h >= 1) de la forme. */
    template <typename Spectrum>
    void addWave (Spectrum&& spectrum)
    {
        Wave w;
        for (int l = 0; l < numLevels; ++l)
        {
            const int maxH = std::max (1, (tableSize / 2) >> l);
            std::vector<std::complex<double>> bins ((size_t) tableSize);
            for (int h = 1; h < maxH; ++h)
            {
                const auto c = spectrum (h);
                bins[(size_t) h] = c;
                bins[(size_t) (tableSize - h)] = std::conj (c);
            }
            fft (bins, true);
            double peak = 1.0e-9;
            for (auto& b : bins) peak = std::max (peak, std::abs (b.real()));
            if (l == 0) w.norm = 1.0 / peak; // même gain pour toutes les octaves d'une forme
            auto& t = w.levels[(size_t) l];
            for (int i = 0; i < tableSize; ++i) t[(size_t) i] = (float) (bins[(size_t) i].real() * w.norm);
            t[tableSize] = t[0];
        }
        waves.push_back (std::make_unique<Wave> (std::move (w)));
    }

    /** Onde décrite par un cycle dans le temps (shape(x), x dans [0, 1)) : son spectre est
        calculé, puis chaque octave est limitée en bande comme pour addWave. */
    template <typename Shape>
    void addWaveTime (Shape&& shape)
    {
        std::vector<std::complex<double>> bins ((size_t) tableSize);
        for (int i = 0; i < tableSize; ++i) bins[(size_t) i] = shape ((double) i / (double) tableSize);
        fft (bins, false);
        addWave ([&bins] (int h) { return h < tableSize / 2 ? bins[(size_t) h] / (double) tableSize : std::complex<double>(); });
    }

    int size() const noexcept { return (int) waves.size(); }

    /** Niveau d'octave adapté à l'incrément de phase dt (cycles par échantillon). */
    static int levelFor (double dt) noexcept
    {
        // harmoniques permises : 0.45 / dt ; niveau l porte (1024 >> l) harmoniques
        const auto allowed = 0.45 / std::max (1.0e-9, dt);
        int l = 0;
        while (l < numLevels - 1 && (double) ((tableSize / 2) >> l) > allowed) ++l;
        return l;
    }

    /** Niveau d'octave continu (v0.22) : le niveau sûr (aucune harmonique au-delà de Nyquist) et
        le poids du niveau suivant, plus sombre. Le fondu entre les deux, sur le dernier tiers de
        chaque octave seulement (le timbre garde sa brillance), supprime le saut de timbre quand
        la hauteur franchit une octave (portamento, vibrato, enveloppe de hauteur). */
    struct Mip { int level = 0; float upper = 0.0f; };

    static Mip mipFor (double dt) noexcept
    {
        const auto allowed = 0.5 / std::max (1.0e-9, dt);
        const auto x = fastLog2 ((double) (tableSize / 2) / allowed); // niveau continu
        if (x <= -1.0) return {};
        const auto a = (int) std::ceil (x);
        if (a >= numLevels - 1) return { numLevels - 1, 0.0f };
        const auto u = 1.0 - ((double) a - x); // position dans l'octave, 0..1
        return { std::max (0, a), (float) std::clamp ((u - 0.65) / 0.35, 0.0, 1.0) };
    }

    float read (int wave, Mip m, double phase) const noexcept
    {
        const auto y0 = read (wave, m.level, phase);
        if (m.upper <= 0.0f) return y0;
        return y0 + (read (wave, m.level + 1, phase) - y0) * m.upper;
    }

    float read (int wave, int level, double phase) const noexcept
    {
        const auto& t = waves[(size_t) wave]->levels[(size_t) level];
        const auto pos = phase * tableSize;
        const auto i = (int) pos;
        const auto f = (float) (pos - (double) i);
        return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
    }

private:
    struct Wave
    {
        std::array<std::array<float, tableSize + 1>, numLevels> levels {};
        double norm = 1.0;
    };
    std::vector<std::unique_ptr<Wave>> waves;
};

} // namespace grv::synth
