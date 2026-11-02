/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Filtre analogique multimode des pads de batterie (principe des filtres des batteries
    analogiques modernes : un filtre multimode par voix, avant le VCA).

      - Passe-bas 24 dB et passe-haut 24 dB : échelle à 4 étages (ladder) en topologie TPT
        (Zavalishin, « The Art of VA Filter Design », ch. 5), réinjection saturée (tanh) :
        la résonance se tasse quand on pousse, comme sur le circuit ; passe-haut obtenu par la
        combinaison binomiale des étages (multimode d'échelle) ;
      - Passe-bas 12 dB, passe-bande, passe-haut 12 dB, réjection : filtre à variables d'état
        TPT, entrée légèrement saturée.

    Fréquence de coupure modulable à chaque échantillon (enveloppe du filtre) sans instabilité.
    Classe pure C++, sans allocation.
*/

#pragma once

#include "Common.h"

namespace grv::dsp
{

class AnalogFilter
{
public:
    enum Mode { off = 0, lowPass24, lowPass12, bandPass, highPass12, highPass24, notch, numModes };

    void prepare (double sampleRate) noexcept { sr = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() noexcept { s1 = s2 = s3 = s4 = 0.0; ic1 = ic2 = 0.0; }

    /** cutoffHz : coupure ; reso : 0..1 (auto-oscillation de l'échelle vers 1). */
    double process (double x, int mode, double cutoffHz, double reso) noexcept
    {
        if (mode <= off || mode >= numModes) return x;
        const auto fc = std::clamp (cutoffHz, 16.0, 0.45 * sr);
        // tan(π·fc/sr) par sinus / cosinus rapides (précis sur toute la plage)
        const auto w = 0.5 * fc / sr;
        const auto g = fastSin2Pi (w) / fastSin2Pi (w + 0.25);

        if (mode == lowPass24 || mode == highPass24)
        {
            const auto G = g / (1.0 + g);
            const auto k = 3.9 * std::clamp (reso, 0.0, 1.0);
            const auto inv = 1.0 / (1.0 + g);
            const auto sig = G * G * G * s1 * inv + G * G * s2 * inv + G * s3 * inv + s4 * inv;
            const auto G4 = G * G * G * G;
            auto u = (x - k * sig) / (1.0 + k * G4);
            u = fastTanh (u);                         // réinjection saturée du circuit
            const auto y1 = stage (u, s1, G);
            const auto y2 = stage (y1, s2, G);
            const auto y3 = stage (y2, s3, G);
            const auto y4 = stage (y3, s4, G);
            if (mode == lowPass24) return y4 * (1.0 + 0.5 * k); // compense la perte de la bande passante
            return (u - 4.0 * y1 + 6.0 * y2 - 4.0 * y3 + y4) * (1.0 + 0.25 * k);
        }

        // Variables d'état : Q de 0,5 à ~25
        const auto kq = 2.0 - 1.92 * std::clamp (reso, 0.0, 1.0);
        const auto a1 = 1.0 / (1.0 + g * (g + kq)), a2 = g * a1, a3 = g * a2;
        const auto in = softClip (x * 1.1) / softClip (1.1);
        const auto v3 = in - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        switch (mode)
        {
            case lowPass12:  return v2;
            case bandPass:   return v1 * kq;               // gain 1 au centre
            case highPass12: return in - kq * v1 - v2;
            case notch:      return in - kq * v1;
            default:         return x;
        }
    }

private:
    /** Étage passe-bas du premier ordre en TPT. */
    static double stage (double x, double& s, double G) noexcept
    {
        const auto v = (x - s) * G;
        const auto y = v + s;
        s = y + v;
        return y;
    }

    double sr = 48000.0, s1 = 0.0, s2 = 0.0, s3 = 0.0, s4 = 0.0, ic1 = 0.0, ic2 = 0.0;
};

} // namespace grv::dsp
