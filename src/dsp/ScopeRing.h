/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Anneau des derniers échantillons de la sortie principale (v0.45.1) : écrit par le fil audio sans
    verrou ni allocation, lu par l'interface (analyseur de spectre, Spectrum.h).
*/

#pragma once

#include <array>
#include <atomic>

namespace grv::dsp
{

/** Derniers échantillons de la sortie : un écrivain (fil audio), un lecteur (interface). */
class ScopeRing
{
public:
    static constexpr int size = 8192; // puissance de deux

    void write (const float* l, const float* r, int n) noexcept
    {
        auto w = pos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) { bufL[(size_t) w] = l[i]; bufR[(size_t) w] = r[i]; w = (w + 1) & (size - 1); }
        pos.store (w, std::memory_order_release);
    }
    /** Copie les n derniers échantillons (n ≤ size). Une lecture pendant une écriture peut mêler deux
        blocs voisins : sans conséquence pour un affichage. */
    void read (float* l, float* r, int n) const noexcept
    {
        const auto w = pos.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) { const auto k = (w - n + i) & (size - 1); l[i] = bufL[(size_t) k]; r[i] = bufR[(size_t) k]; }
    }

private:
    std::array<float, size> bufL {}, bufR {};
    std::atomic<int> pos { 0 };
};

} // namespace grv::dsp
