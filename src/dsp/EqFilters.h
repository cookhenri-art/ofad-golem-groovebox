/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Filtres d'égalisation à variables d'état en topologie trapézoïdale (principe publié par
    A. Simper, Cytomic) : cloche, plateaux, passe-haut/bas, coupe-bande, passe-bande. Stables
    quand on les module, coefficients recalculés seulement quand les réglages changent.
*/

#pragma once

#include "Common.h"

#include <cmath>

namespace grv::dsp
{

class SvfEq
{
public:
    enum Type { lowpass = 0, highpass, bandpass, notch, bell, lowShelf, highShelf, allpass };

    void reset() noexcept { ic1 = ic2 = 0.0; }

    void set (Type type, double fc, double q, double gainDb, double sr) noexcept
    {
        if (type == cType && fc == cFc && q == cQ && gainDb == cGain && sr == cSr) return;
        cType = type; cFc = fc; cQ = q; cGain = gainDb; cSr = sr;
        fc = std::clamp (fc, 10.0, 0.48 * sr);
        q = std::max (0.05, q);
        auto g = std::tan (kPi * fc / sr);
        auto k = 1.0 / q;
        const auto A = std::pow (10.0, gainDb / 40.0);
        switch (type)
        {
            case lowpass:   m0 = 0; m1 = 0; m2 = 1; break;
            case highpass:  m0 = 1; m1 = -k; m2 = -1; break;
            case bandpass:  m0 = 0; m1 = k; m2 = 0; break; // gain unité au centre
            case notch:     m0 = 1; m1 = -k; m2 = 0; break;
            case allpass:   m0 = 1; m1 = -2.0 * k; m2 = 0; break;
            case bell:      k = 1.0 / (q * A); m0 = 1; m1 = k * (A * A - 1.0); m2 = 0; break;
            case lowShelf:  g /= std::sqrt (A); m0 = 1; m1 = k * (A - 1.0); m2 = A * A - 1.0; break;
            case highShelf: g *= std::sqrt (A); m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A; break;
        }
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double process (double v0) noexcept
    {
        const auto v3 = v0 - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

private:
    double a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0, ic1 = 0, ic2 = 0;
    Type cType = allpass;
    double cFc = -1, cQ = -1, cGain = -999, cSr = -1;
};

/** Paire stéréo d'un même filtre. */
struct SvfEqStereo
{
    SvfEq l, r;
    void reset() noexcept { l.reset(); r.reset(); }
    void set (SvfEq::Type t, double fc, double q, double gainDb, double sr) noexcept { l.set (t, fc, q, gainDb, sr); r.set (t, fc, q, gainDb, sr); }
};

/** Détecteur de niveau crête à attaque et relâchement (dB). */
class EnvelopeFollower
{
public:
    void set (double attackMs, double releaseMs, double sr) noexcept
    {
        att = std::exp (-1.0 / std::max (1.0, attackMs * 0.001 * sr));
        rel = std::exp (-1.0 / std::max (1.0, releaseMs * 0.001 * sr));
    }
    void reset() noexcept { env = 0.0; }
    double process (double x) noexcept
    {
        x = std::abs (x);
        env = x > env ? x + (env - x) * att : x + (env - x) * rel;
        return env;
    }
    double value() const noexcept { return env; }
private:
    double att = 0.9, rel = 0.999, env = 0.0;
};

inline double gainToDb (double g) noexcept { return 20.0 * std::log10 (std::max (1.0e-9, g)); }

} // namespace grv::dsp
