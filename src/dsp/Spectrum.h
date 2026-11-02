/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Analyseur de spectre du bus principal (v0.45.1, principe des analyseurs de mixage du type
    « courbe lissée à crêtes tenues ») :
      - fil audio : la sortie principale est recopiée dans un anneau sans verrou (ScopeRing) ;
        aucun calcul, aucune latence ajoutée au son ;
      - fil de l'interface : FFT de 4096 points (fenêtre de Hann) sur les derniers échantillons,
        milieu (G+D)/2 et côtés (G−D)/2, regroupés en bandes logarithmiques de 20 Hz à 20 kHz
        (le maximum des raies de la bande ; entre deux raies dans le grave, valeur interpolée).
      - les valeurs sont en dB « crête de sinus » : un sinus à pleine échelle donne 0 dB.
    La pente d'affichage, le lissage dans le temps et les crêtes tenues sont faits par l'interface.
*/

#pragma once

#include "ScopeRing.h"

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace grv::dsp
{

class SpectrumAnalyzer
{
public:
    static constexpr int order = 12, fftSize = 1 << order, numBands = 96;
    static constexpr double fLo = 20.0, fHi = 20000.0;

    SpectrumAnalyzer() : fft (order)
    {
        window.resize (fftSize);
        for (int i = 0; i < fftSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);
        work.resize (2 * fftSize);
        inL.resize (fftSize); inR.resize (fftSize);
    }

    /** Fréquence centrale de la bande b. */
    static double bandHz (int b) noexcept { return fLo * std::pow (fHi / fLo, (b + 0.5) / numBands); }

    /** Calcule milieu et côtés (dB, plancher -120) depuis l'anneau. */
    void compute (const ScopeRing& ring, double sampleRate, std::array<float, numBands>& mid, std::array<float, numBands>& side)
    {
        ring.read (inL.data(), inR.data(), fftSize);
        analyse (true, sampleRate, mid);
        analyse (false, sampleRate, side);
    }

    /** Variante sur un tampon fourni (essais). */
    void computeFrom (const float* l, const float* r, double sampleRate, std::array<float, numBands>& mid, std::array<float, numBands>& side)
    {
        std::copy (l, l + fftSize, inL.begin()); std::copy (r, r + fftSize, inR.begin());
        analyse (true, sampleRate, mid);
        analyse (false, sampleRate, side);
    }

private:
    void analyse (bool isMid, double sr, std::array<float, numBands>& out)
    {
        std::fill (work.begin(), work.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            work[(size_t) i] = (isMid ? 0.5f * (inL[(size_t) i] + inR[(size_t) i]) : 0.5f * (inL[(size_t) i] - inR[(size_t) i])) * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (work.data(), true);
        const auto binHz = sr / fftSize;
        const auto norm = 4.0f / (float) fftSize; // sinus à pleine échelle → 1
        auto magAt = [&] (double hz)
        {
            const auto x = hz / binHz;
            const auto i = (int) x;
            if (i >= fftSize / 2 - 1) return 0.0f;
            const auto f = (float) (x - i);
            return (work[(size_t) i] + f * (work[(size_t) i + 1] - work[(size_t) i])) * norm;
        };
        for (int b = 0; b < numBands; ++b)
        {
            const auto lo = fLo * std::pow (fHi / fLo, (double) b / numBands), hi = fLo * std::pow (fHi / fLo, (double) (b + 1) / numBands);
            const auto i0 = (int) std::ceil (lo / binHz), i1 = (int) std::floor (hi / binHz);
            float m = 0.0f;
            if (i1 >= i0) for (int i = i0; i <= i1 && i < fftSize / 2; ++i) m = std::max (m, work[(size_t) i] * norm);
            else m = magAt (bandHz (b));
            out[(size_t) b] = (float) std::max (-120.0, 20.0 * std::log10 (std::max (1.0e-7f, m)));
        }
    }

    juce::dsp::FFT fft;
    std::vector<float> window, work, inL, inR;
};

} // namespace grv::dsp
