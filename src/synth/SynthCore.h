/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Briques communes des synthétiseurs : oscillateurs à bande limitée, filtres en échelle et à
    variables d'état (topologie sans délai), enveloppes analogiques, allocation des voix
    (polyphonie avec vol de voix, monophonie avec legato et portamento), chorus à lignes à retard.
    Pur C++, aucune allocation après prepare().
*/

#pragma once

#include "../dsp/Common.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace grv::synth
{

using namespace grv::dsp;

inline double noteHz (double note) noexcept { return 440.0 * fastExp2 ((note - 69.0) / 12.0); }

/** tan(π·x) pour x dans [0, 0.49] (pré-distorsion des filtres). */
inline double prewarp (double x) noexcept
{
    x = std::clamp (x, 1.0e-6, 0.49);
    return fastSin2Pi (0.5 * x) / fastSin2Pi (0.5 * x + 0.25);
}

//==============================================================================
// Oscillateurs (PolyBLEP / PolyBLAMP)

inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

inline double polyBlamp (double t, double dt) noexcept
{
    if (t < dt) { t = t / dt - 1.0; return -1.0 / 3.0 * t * t * t; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt + 1.0; return 1.0 / 3.0 * t * t * t; }
    return 0.0;
}

/** Résidu de BLEP à B-spline cubique (4 échantillons de support) : I(τ + 2) − H(τ), τ dans (−2, 2).
    Repliement nettement plus faible que le PolyBLEP à 2 points. */
inline double bsplineBlepResidual (double tau) noexcept
{
    if (tau <= -2.0 || tau >= 2.0) return 0.0;
    auto integ = [] (double x) noexcept -> double // primitive de la B-spline cubique sur [0, 2]
    {
        if (x <= 0.0) return 0.0;
        if (x < 1.0) return x * x * x * x / 24.0;
        const auto x2 = x * x;
        return 1.0 / 24.0 + (-0.75 * x2 * x2 + 4.0 * x2 * x - 6.0 * x2 + 4.0 * x) / 6.0 - 1.25 / 6.0;
    };
    const auto x = tau + 2.0;                     // [0, 4]
    const auto I = x <= 2.0 ? integ (x) : 1.0 - integ (4.0 - x);
    return I - (tau >= 0.0 ? 1.0 : 0.0);
}

/** Oscillateur analogique : dent de scie, carré à largeur variable, triangle, sinus.
    Discontinuités corrigées par BLEP à B-spline (sortie retardée d'un échantillon, identique
    pour toutes les formes). Synchronisation dure : l'esclave reçoit l'instant de bouclage du maître. */
class Osc
{
public:
    enum Wave { saw = 0, pulse, triangle, sine, numWaves };

    void reset (double startPhase = 0.0) noexcept { phase = startPhase; naive.fill (0.0); corr.fill (0.0); m = 0; }
    double getPhase() const noexcept { return phase; }

    /** Avance d'un échantillon. dt = fréquence / fréquence d'échantillonnage.
        syncAt : position (0, 1] dans l'échantillon où le maître a bouclé, ou < 0.
        wrapAt (sortie) : position du bouclage de cet oscillateur dans l'échantillon, ou -1. */
    double next (Wave w, double dt, double pw, double syncAt = -1.0, double* wrapAt = nullptr) noexcept
    {
        dt = std::clamp (dt, 1.0e-7, 0.45);
        pw = std::clamp (pw, 0.02, 0.98);
        const auto i0 = m & 3;
        corr[(size_t) ((m + 2) & 3)] = 0.0;

        // valeur naïve à l'instant m
        double v;
        switch (w)
        {
            case saw: v = 2.0 * phase - 1.0; break;
            case pulse: v = (phase < pw ? 1.0 : -1.0) + 2.0 * pw - 1.0; break;
            case triangle:
            {
                v = phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
                v += 4.0 * dt * polyBlamp (phase, dt);
                auto t2 = phase - 0.5; if (t2 < 0.0) t2 += 1.0;
                v -= 4.0 * dt * polyBlamp (t2, dt);
                break;
            }
            default: v = fastSin2Pi (phase); break;
        }
        naive[(size_t) i0] = v;

        if (wrapAt) *wrapAt = -1.0;
        if (syncAt > 0.0 && syncAt <= 1.0)
        {
            // phase au moment du bouclage du maître, saut vers 0
            const auto phAt = phase + syncAt * dt;
            const auto ph = phAt - std::floor (phAt);
            if (w == saw || w == pulse) addJumpsBetween (w, phase, ph, dt, pw, 0.0, syncAt);
            // saut de valeur au retour à zéro, corrigé pour toutes les formes (v0.22 : aussi triangle et sinus)
            const auto jump = valueAt (w, 0.0, pw) - valueAt (w, ph, pw);
            addStep (jump, syncAt);
            phase = (1.0 - syncAt) * dt;
            if (w == pulse && phase >= pw) addStep (-2.0, syncAt + pw / dt);
        }
        else
        {
            const auto next = phase + dt;
            if (w == saw)
            {
                if (next >= 1.0) { const auto t = (1.0 - phase) / dt; addStep (-2.0, t); if (wrapAt) *wrapAt = t; }
            }
            else if (w == pulse)
            {
                if (phase < pw && next >= pw) addStep (-2.0, (pw - phase) / dt);
                if (next >= 1.0)
                {
                    const auto t = (1.0 - phase) / dt;
                    addStep (2.0, t);
                    if (next - 1.0 >= pw) addStep (-2.0, (1.0 + pw - phase) / dt);
                    if (wrapAt) *wrapAt = t;
                }
            }
            else if (next >= 1.0 && wrapAt) *wrapAt = (1.0 - phase) / dt;
            phase = next >= 1.0 ? next - 1.0 : next;
        }

        const auto out = (size_t) ((m + 3) & 3); // échantillon m - 1, désormais complet
        ++m;
        return naive[out] + corr[out];
    }

private:
    static double valueAt (Wave w, double ph, double pw) noexcept
    {
        switch (w)
        {
            case saw: return 2.0 * ph - 1.0;
            case pulse: return (ph < pw ? 1.0 : -1.0) + 2.0 * pw - 1.0;
            case triangle: return ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph;
            default: return fastSin2Pi (ph);
        }
    }

    /** Saut de hauteur h à l'instant m + t (t dans (0, 1]) : corrige m - 1 … m + 2. */
    void addStep (double h, double t) noexcept
    {
        for (int k = -1; k <= 2; ++k)
            corr[(size_t) ((m + k) & 3)] += h * bsplineBlepResidual ((double) k - t);
    }

    void addJumpsBetween (Wave w, double from, double to, double, double pw, double, double t) noexcept
    {
        // carré : front descendant franchi avant la synchro
        if (w == pulse && from < pw && to >= pw && to > from) addStep (-2.0, t * (pw - from) / std::max (1.0e-9, to - from));
    }

    double phase = 0.0;
    std::array<double, 4> naive {}, corr {};
    unsigned m = 0;
};

/** Correction BLEP à B-spline pour un oscillateur quelconque (table d'ondes synchronisée…) :
    sortie retardée d'un échantillon, comme Osc. step(h, t) : saut h à l'instant m + t (t dans
    (0, 1]) ; push(v) : valeur naïve de l'instant m, renvoie l'échantillon m − 1 corrigé. */
class BlepLine
{
public:
    void reset() noexcept { naive.fill (0.0); corr.fill (0.0); m = 0; }
    void step (double h, double t) noexcept
    {
        for (int k = -1; k <= 2; ++k)
            corr[(size_t) ((m + (unsigned) (k + 4)) & 3)] += h * bsplineBlepResidual ((double) k - t);
    }
    double push (double v) noexcept
    {
        naive[(size_t) (m & 3)] = v;
        const auto out = (size_t) ((m + 3) & 3);
        const auto y = naive[out] + corr[out];
        ++m;
        corr[(size_t) ((m + 2) & 3)] = 0.0; // case qui recevra les corrections de m + 2
        return y;
    }
private:
    std::array<double, 4> naive {}, corr {};
    unsigned m = 0;
};

/** Forme continue : 0 = sinus, 1 = triangle, 2 = dent de scie, 3 = carré, 4 = impulsion étroite.
    Les deux formes voisines sont fondues ; les sauts sont corrigés par BLEP à B-spline, pondérés
    par leur part dans le mélange (sortie retardée d'un échantillon). Synchronisation dure : syncAt
    = position (0, 1] du bouclage du maître dans l'échantillon ; wrapAt (sortie) = position du
    bouclage de cet oscillateur, -1 sinon. */
class MorphOsc
{
public:
    void reset (double p = 0.0) noexcept { phase = p; naive.fill (0.0); corr.fill (0.0); m = 0; }
    double getPhase() const noexcept { return phase; }

    double next (double shape, double dt, double fm = 0.0, double syncAt = -1.0, double* wrapAt = nullptr) noexcept
    {
        shape = std::clamp (shape, 0.0, 4.0);
        dt = std::clamp (dt, 1.0e-7, 0.45);
        const auto i = std::min (3, (int) std::floor (shape));
        const auto f = shape - (double) i;
        w.fill (0.0);
        w[(size_t) i] = 1.0 - f;
        w[(size_t) i + 1] += f;
        corr[(size_t) ((m + 2) & 3)] = 0.0;
        if (wrapAt) *wrapAt = -1.0;

        auto ph = phase + fm;
        ph -= std::floor (ph);
        double v = mixValue (ph);
        if (w[1] > 0.0)
        {
            auto t2 = ph - 0.5; if (t2 < 0.0) t2 += 1.0;
            v += w[1] * 4.0 * dt * (polyBlamp (ph, dt) - polyBlamp (t2, dt));
        }
        naive[(size_t) (m & 3)] = v;

        if (syncAt > 0.0 && syncAt <= 1.0)
        {
            auto phAt = ph + syncAt * dt;
            if (phAt >= 1.0) { edges (ph, 1.0, 0.0, dt); wrapJump ((1.0 - ph) / dt); phAt -= 1.0; edges (0.0, phAt, (1.0 - ph) / dt, dt); }
            else edges (ph, phAt, 0.0, dt);
            addStep (mixValue (0.0) - mixValue (phAt), syncAt);
            const auto newPh = (1.0 - syncAt) * dt;
            edges (0.0, newPh, syncAt, dt);
            phase = newPh - fm;
            phase -= std::floor (phase);
        }
        else
        {
            const auto next = ph + dt;
            if (next >= 1.0)
            {
                const auto t = (1.0 - ph) / dt;
                edges (ph, 1.0, 0.0, dt);
                wrapJump (t);
                edges (0.0, next - 1.0, t, dt);
                if (wrapAt) *wrapAt = t;
            }
            else edges (ph, next, 0.0, dt);
            phase += dt;
            if (phase >= 1.0) phase -= 1.0;
        }

        const auto out = (size_t) ((m + 3) & 3);
        ++m;
        return naive[out] + corr[out];
    }

private:
    static constexpr double narrow = 0.12;

    double mixValue (double ph) const noexcept
    {
        double v = 0.0;
        if (w[0] > 0.0) v += w[0] * fastSin2Pi (ph);
        if (w[1] > 0.0) v += w[1] * (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph);
        if (w[2] > 0.0) v += w[2] * (2.0 * ph - 1.0);
        if (w[3] > 0.0) v += w[3] * (ph < 0.5 ? 1.0 : -1.0);
        if (w[4] > 0.0) v += w[4] * ((ph < narrow ? 1.0 : -1.0) + 2.0 * narrow - 1.0);
        return v;
    }

    /** Fronts descendants franchis entre les phases a et b (sans bouclage), à partir de l'instant t0. */
    void edges (double a, double b, double t0, double dt) noexcept
    {
        if (w[3] > 0.0 && a < 0.5 && b >= 0.5) addStep (-2.0 * w[3], t0 + (0.5 - a) / dt);
        if (w[4] > 0.0 && a < narrow && b >= narrow) addStep (-2.0 * w[4], t0 + (narrow - a) / dt);
    }

    void wrapJump (double t) noexcept
    {
        const auto h = -2.0 * w[2] + 2.0 * w[3] + 2.0 * w[4];
        if (h != 0.0) addStep (h, t);
    }

    void addStep (double h, double t) noexcept
    {
        for (int k = -1; k <= 2; ++k)
            corr[(size_t) ((m + k) & 3)] += h * bsplineBlepResidual ((double) k - t);
    }

    double phase = 0.0;
    std::array<double, 5> w {};
    std::array<double, 4> naive {}, corr {};
    unsigned m = 0;
};

//==============================================================================
// Filtres

/** Filtre en échelle à 4 pôles (principe des filtres à transistors), topologie TPT résolue
    sans délai (Zavalishin), saturation douce de l'entrée et de la contre-réaction.
    Sorties : passe-bas 6/12/18/24 dB, passe-bande, passe-haut (mélange des étages). */
class Ladder
{
public:
    enum Mode { lp24 = 0, lp18, lp12, lp6, bp12, hp12, hp24, numModes };

    void reset() noexcept { s.fill (0.0); }

    /** fc en Hz, res 0..1 (auto-oscillation vers 1), drive >= 1. */
    double process (double x, double fc, double res, double sr, Mode mode, double drive = 1.0) noexcept
    {
        const auto g = prewarp (fc / sr);
        const auto inv = 1.0 / (1.0 + g);
        const auto G = g * inv;
        const auto k = 4.0 * std::clamp (res, 0.0, 1.05);

        // Estimation de la sortie sans délai : y4 = G^4 u + S, u = x - k y4
        const auto G2 = G * G, G4 = G2 * G2;
        const auto S = inv * (G2 * G * s[0] + G2 * s[1] + G * s[2] + s[3]);
        auto u = (x * drive - k * S) / (1.0 + k * G4);
        u = fastTanh (u); // la saturation de l'étage d'entrée donne le grain et limite la résonance

        double y[4];
        auto in = u;
        for (int i = 0; i < 4; ++i)
        {
            const auto v = (in - s[(size_t) i]) * G;
            y[i] = v + s[(size_t) i];
            s[(size_t) i] = y[i] + v;
            in = y[i];
        }
        // compensation partielle de la perte de grave due à la résonance
        const auto comp = 1.0 + 0.3 * k;
        switch (mode)
        {
            case lp24: return y[3] * comp;
            case lp18: return y[2] * comp;
            case lp12: return y[1] * comp;
            case lp6:  return y[0];
            case bp12: return (y[1] - y[3]) * 2.0 * comp;    // mélange d'étages (principe des filtres multimodes à échelle)
            case hp12: return u - 2.0 * y[0] + y[1];
            default:   return u - 4.0 * y[0] + 6.0 * y[1] - 4.0 * y[2] + y[3];
        }
    }

private:
    std::array<double, 4> s {};
};

/** Filtre à diodes à 4 pôles façon basse acide : pôles inégaux, passe-haut dans la boucle de
    contre-réaction (la résonance reste fine et ne mange pas le grave), saturation dans la boucle. */
class DiodeLadder
{
public:
    void prepare (double sampleRate) noexcept { sr = sampleRate; hpCoef = std::exp (-kTwoPi * 140.0 / sr); reset(); }
    void reset() noexcept { s.fill (0.0); fb = 0.0; hpX = hpY = 0.0; }

    double process (double x, double fc, double res) noexcept
    {
        const auto g = prewarp (fc / sr);
        const auto k = 17.0 * std::clamp (res, 0.0, 1.0);
        // contre-réaction : sortie retardée d'un échantillon, passée dans un passe-haut
        const auto hp = fb - hpX + hpCoef * hpY;
        hpX = fb; hpY = hp;
        auto in = softClip (x - k * hp * 0.25);
        static constexpr double spread[4] { 1.0, 0.84, 0.66, 0.5 }; // pôles inégaux
        for (int i = 0; i < 4; ++i)
        {
            const auto gi = g * spread[i] * 1.6;
            const auto G = gi / (1.0 + gi);
            const auto v = (in - s[(size_t) i]) * G;
            const auto y = v + s[(size_t) i];
            s[(size_t) i] = y + v;
            in = y;
        }
        fb = in;
        return in * (1.0 + 0.25 * k * 0.2);
    }

private:
    double sr = 48000.0, hpCoef = 0.98, fb = 0.0, hpX = 0.0, hpY = 0.0;
    std::array<double, 4> s {};
};

/** Coefficients d'un filtre à variables d'état, partageables entre plusieurs filtres (stéréo, étages). */
struct SvfCoefs
{
    double k = 2.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;

    void set (double fc, double res, double sr) noexcept
    {
        const auto g = prewarp (fc / sr);
        k = 2.0 - 1.98 * std::clamp (res, 0.0, 1.0);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

/** Filtre à variables d'état à 2 pôles, multimode et saturé (principe des filtres d'état).
    Mode continu 0..1 : passe-bas → passe-bande → passe-haut. */
class StateFilter
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0; }

    struct Out { double low, band, high; };

    Out process (double x, const SvfCoefs& c) noexcept
    {
        const auto v3 = x - ic2;
        const auto v1 = c.a1 * ic1 + c.a2 * v3;
        const auto v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * fastTanh (v1) - ic1; // saturation douce de l'intégrateur : stabilité à forte résonance
        ic2 = 2.0 * v2 - ic2;
        return { v2, v1, x - c.k * v1 - v2 };
    }

    Out process (double x, double fc, double res, double sr) noexcept
    {
        SvfCoefs c;
        c.set (fc, res, sr);
        return process (x, c);
    }

    double processMorph (double x, double fc, double res, double sr, double morph) noexcept
    {
        const auto o = process (x, fc, res, sr);
        morph = std::clamp (morph, 0.0, 1.0);
        return morph < 0.5 ? o.low + (o.band - o.low) * (morph * 2.0)
                           : o.band + (o.high - o.band) * ((morph - 0.5) * 2.0);
    }

private:
    double ic1 = 0.0, ic2 = 0.0;
};

/** Passe-haut à un pôle (TPT). */
class OnePole
{
public:
    void reset() noexcept { s = 0.0; }
    double lowpass (double x, double fc, double sr) noexcept
    {
        const auto g = prewarp (fc / sr);
        const auto v = (x - s) * g / (1.0 + g);
        const auto y = v + s;
        s = y + v;
        return y;
    }
    double highpass (double x, double fc, double sr) noexcept { return x - lowpass (x, fc, sr); }
private:
    double s = 0.0;
};

//==============================================================================
// Enveloppes

/** Enveloppe ADSR au comportement analogique (attaque vers une cible au-delà de 1, déclin et
    relâchement exponentiels). Temps en ms, maintien 0..1. */
class Adsr
{
public:
    enum Stage { idle, attack, decay, sustain, release };

    void prepare (double sampleRate) noexcept { sr = sampleRate; }
    void reset() noexcept { stage = idle; value = 0.0; }

    void set (double attackMs, double decayMs, double sustainLevel, double releaseMs) noexcept
    {
        // Les coefficients ne sont recalculés que si les temps changent (coût du exp)
        if (attackMs != a) { a = attackMs; aCoef = coefFor (a, 0.8); }
        if (decayMs != d) { d = decayMs; dCoef = coefFor (d, 0.01); }
        if (releaseMs != r) { r = releaseMs; rCoef = coefFor (r, 0.01); }
        sus = std::clamp (sustainLevel, 0.0, 1.0);
    }

    void gate (bool on, bool retrigger = true) noexcept
    {
        if (on) { if (retrigger || stage == idle || stage == release) stage = attack; }
        else if (stage != idle) stage = release;
    }

    double next() noexcept
    {
        switch (stage)
        {
            case attack:
                value = 1.3 + (value - 1.3) * aCoef; // cible au-delà de 1 : courbe concave
                if (value >= 1.0) { value = 1.0; stage = decay; }
                break;
            case decay:
                value = sus + (value - sus) * dCoef;
                if (value - sus < 1.0e-4) stage = sustain;
                break;
            case sustain: value = sus; break;
            case release:
                value *= rCoef;
                if (value < 1.0e-5) { value = 0.0; stage = idle; }
                break;
            default: break;
        }
        return value;
    }

    bool isActive() const noexcept { return stage != idle; }
    Stage getStage() const noexcept { return stage; }
    double getValue() const noexcept { return value; }

private:
    double coefFor (double ms, double ratio) const noexcept
    {
        // temps pour parcourir l'essentiel du segment
        const auto samples = std::max (1.0, ms * 0.001 * sr);
        return std::exp (std::log (ratio) / samples);
    }

    double sr = 48000.0, value = 0.0, sus = 0.7;
    double a = -1, d = -1, r = -1, aCoef = 0.0, dCoef = 0.0, rCoef = 0.0;
    Stage stage = idle;
};

/** Synchro au tempo des LFO d'instrument : choix 0 = libre (vitesse en Hz), sinon une division
    de la mesure (pointées et ternaires comprises). */
inline const std::vector<std::string>& lfoSyncChoices()
{
    static const std::vector<std::string> v { "Libre", "16 mes.", "8 mes.", "4 mes.", "2 mes.", "1 mes.", "1/2", "1/4", "1/8", "1/16", "1/32",
                                              "1/2T", "1/4T", "1/8T", "1/16T", "1/4 point\xC3\xA9" "e", "1/8 point\xC3\xA9" "e", "1/16 point\xC3\xA9" "e" };
    return v;
}
/** Vitesse d'un LFO d'instrument : celle en Hz en mode libre, sinon une période par division. */
inline double lfoSyncHz (int choice, double freeHz, double bpm) noexcept
{
    static constexpr double quarters[] { 0.0, 64.0, 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                         4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75, 0.375 };
    if (choice <= 0 || choice >= (int) std::size (quarters)) return freeHz;
    return std::clamp (bpm, 20.0, 400.0) / 60.0 / quarters[choice];
}

/** LFO simple d'instrument (sinus, triangle, scie, carré, aléatoire), sortie -1..1. */
/** Retard des LFO d'instrument (v0.13.2) : à crans (ces réglages ne déplacent pas la liste des
    destinations de modulation). Le LFO reste muet pendant le retard, puis entre en fondu sur la
    moitié de ce temps. */
inline const std::vector<std::string>& lfoDelayChoices()
{
    static const std::vector<std::string> v { "Aucun", "50 ms", "100 ms", "200 ms", "300 ms", "500 ms", "750 ms", "1 s", "1,5 s", "2 s", "3 s", "4 s", "6 s", "8 s", "10 s" };
    return v;
}
inline double lfoDelaySeconds (int i) noexcept
{
    static constexpr double s[] { 0.0, 0.05, 0.1, 0.2, 0.3, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 10.0 };
    return s[std::clamp (i, 0, 14)];
}
inline double lfoDelayGain (double age, double delay) noexcept
{
    return delay <= 0.0 ? 1.0 : std::clamp ((age - delay) / (0.5 * delay), 0.0, 1.0);
}

class SimpleLfo
{
public:
    void reset (double p = 0.0) noexcept { phase = p; }
    double next (int shape, double hz, double sr) noexcept
    {
        const auto prev = phase;
        phase += hz / sr;
        if (phase >= 1.0) phase -= std::floor (phase);
        if (phase < prev) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; held = (double) rng / 2147483648.0 - 1.0; }
        switch (shape)
        {
            case 0: return fastSin2Pi (phase);
            case 1: return 1.0 - 4.0 * std::abs (phase - 0.5);
            case 2: return 2.0 * phase - 1.0;
            case 3: return phase < 0.5 ? 1.0 : -1.0;
            default: return held;
        }
    }
private:
    double phase = 0.0, held = 0.0;
    std::uint32_t rng = 0x51ED27u;
};

//==============================================================================
// Allocation des voix

/** Pile de notes tenues (monophonie : priorité à la dernière note). */
class NoteStack
{
public:
    void clear() noexcept { count = 0; }
    void push (int note) noexcept
    {
        remove (note);
        if (count < (int) notes.size()) notes[(size_t) count++] = note;
    }
    void remove (int note) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (notes[(size_t) i] == note)
            {
                for (int j = i; j < count - 1; ++j) notes[(size_t) j] = notes[(size_t) (j + 1)];
                --count;
                return;
            }
    }
    bool empty() const noexcept { return count == 0; }
    int top() const noexcept { return count > 0 ? notes[(size_t) (count - 1)] : -1; }
    int size() const noexcept { return count; }
    int at (int i) const noexcept { return notes[(size_t) i]; }
private:
    std::array<int, 32> notes {};
    int count = 0;
};

/** Allocateur polyphonique : réutilise la voix de la même note, sinon une voix libre, sinon vole
    la voix relâchée la plus ancienne, sinon la plus ancienne. */
template <int N>
class VoiceAllocator
{
public:
    struct Slot { int note = -1; bool held = false; std::uint32_t age = 0; };

    template <typename IsActive>
    int noteOn (int note, int maxVoices, IsActive&& isActive) noexcept
    {
        maxVoices = std::clamp (maxVoices, 1, N);
        ++clock;
        int pick = -1;
        for (int i = 0; i < maxVoices; ++i) if (slots[(size_t) i].note == note && isActive (i)) { pick = i; break; }
        if (pick < 0) for (int i = 0; i < maxVoices; ++i) if (! isActive (i)) { pick = i; break; }
        if (pick < 0)
        {
            std::uint32_t best = 0xFFFFFFFFu;
            for (int i = 0; i < maxVoices; ++i)
                if (! slots[(size_t) i].held && slots[(size_t) i].age < best) { best = slots[(size_t) i].age; pick = i; }
        }
        if (pick < 0)
        {
            std::uint32_t best = 0xFFFFFFFFu;
            for (int i = 0; i < maxVoices; ++i) if (slots[(size_t) i].age < best) { best = slots[(size_t) i].age; pick = i; }
        }
        slots[(size_t) pick] = { note, true, clock };
        return pick;
    }

    /** Renvoie la voix tenue par la note (ou -1). */
    int noteOff (int note) noexcept
    {
        for (int i = 0; i < N; ++i)
            if (slots[(size_t) i].note == note && slots[(size_t) i].held) { slots[(size_t) i].held = false; return i; }
        return -1;
    }

    void clear() noexcept { for (auto& s : slots) s = {}; }
    const Slot& slot (int i) const noexcept { return slots[(size_t) i]; }

private:
    std::array<Slot, N> slots {};
    std::uint32_t clock = 0;
};

/** Portamento : glissement exponentiel de la hauteur (en demi-tons). */
class Glide
{
public:
    void jump (double note) noexcept { current = target = note; }
    void to (double note) noexcept { target = note; }
    double next (double coef) noexcept { current = target + (current - target) * coef; return current; }
    double get() const noexcept { return current; }
private:
    double current = 60.0, target = 60.0;
};

//==============================================================================
/** Chorus stéréo à deux lignes modulées en opposition (principe des chorus à circuits BBD) :
    filtrage d'entrée et de sortie, légère saturation. */
class BbdChorus
{
public:
    static constexpr int maxDelay = 4096;

    void prepare (double sampleRate) noexcept { sr = sampleRate; reset(); }
    void reset() noexcept { buf.fill (0.0f); w = 0; phase = 0.0; lpL = lpR = lpIn = 0.0; }

    /** rateHz, depthMs, delayMs, mix 0..1, en place. */
    void process (float* L, float* R, int n, double rateHz, double depthMs, double delayMs, double mix, double tone = 9000.0) noexcept
    {
        const auto inC = std::exp (-kTwoPi * std::min (tone, 0.45 * sr) / sr);
        for (int i = 0; i < n; ++i)
        {
            const auto mono = 0.5 * ((double) L[i] + (double) R[i]);
            lpIn = mono + (lpIn - mono) * inC;
            buf[(size_t) w] = (float) softClip (lpIn * 0.9);

            phase += rateHz / sr;
            if (phase >= 1.0) phase -= 1.0;
            const auto tri = 1.0 - 4.0 * std::abs (phase - 0.5); // triangle -1..1
            const auto dL = (delayMs + depthMs * tri) * 0.001 * sr;
            const auto dR = (delayMs - depthMs * tri) * 0.001 * sr;
            const auto yL = read (dL), yR = read (dR);
            lpL = yL + (lpL - yL) * inC;
            lpR = yR + (lpR - yR) * inC;
            L[i] = (float) ((double) L[i] * (1.0 - 0.5 * mix) + lpL * mix);
            R[i] = (float) ((double) R[i] * (1.0 - 0.5 * mix) + lpR * mix);
            w = (w + 1) & (maxDelay - 1);
        }
    }

private:
    double read (double delaySamples) const noexcept
    {
        delaySamples = std::clamp (delaySamples, 1.0, (double) maxDelay - 4.0);
        auto pos = (double) w - delaySamples;
        if (pos < 0.0) pos += maxDelay;
        const auto i0 = (int) pos;
        const auto f = pos - (double) i0;
        // Hermite à 4 points (v0.22) : moins de perte d'aigus et de bruit de modulation que le linéaire
        auto at = [this] (int k) { return (double) buf[(size_t) (k & (maxDelay - 1))]; };
        return hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
    }

    double sr = 48000.0, phase = 0.0, lpL = 0.0, lpR = 0.0, lpIn = 0.0;
    std::array<float, maxDelay> buf {};
    int w = 0;
};

/** Anti-clic de redéclenchement (v0.22) : quand une voix qui sonne encore est relancée (vol de
    voix, même note rejouée, monophonie sans legato, pad refrappé), ses oscillateurs repartent à
    leur phase de départ et la sortie fait un saut. Sur les deux premiers échantillons (certaines
    voix ont un échantillon de retard), un pas anormal — plus du double du plus grand pas naturel
    du signal juste avant — est retiré, puis rendu par un décalage qui s'éteint en 2 ms : la forme
    d'onde reste continue, l'attaque est conservée. Un signal déjà brillant (bruit, scie aiguë),
    où un saut ne s'entend pas, n'est pas touché. */
class Declick
{
public:
    void prepare (double sampleRate) noexcept
    {
        coef = std::exp (-1.0 / (0.002 * sampleRate));
        stepFall = std::exp (-1.0 / (0.005 * sampleRate));
        reset();
    }
    void reset() noexcept { offL = offR = 0.0; lastL = lastR = 0.0; rawL = rawR = 0.0; maxStep = 0.0; armed = false; }
    void arm() noexcept { armed = true; }

    void process (float* L, float* R, int n, bool mono = false) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double xl = L[i], xr = mono ? xl : (double) R[i];
            auto yl = xl + offL, yr = xr + offR;
            // pas naturel : entre deux valeurs brutes ; à l'échantillon 0, depuis la dernière sortie
            const auto dl = i == 0 ? yl - lastL : xl - rawL, dr = mono ? 0.0 : (i == 0 ? yr - lastR : xr - rawR);
            const auto step = std::max (std::abs (dl), std::abs (dr));
            if (armed && i < 2 && step > 2.0 * maxStep + 1.0e-4)
            {
                offL -= dl; offR -= dr;
                yl -= dl; yr -= dr;
            }
            else maxStep = std::max (step, maxStep * stepFall);
            if (i == 1 || n == 1) armed = false;
            L[i] = (float) yl;
            if (! mono) R[i] = (float) yr;
            lastL = yl; lastR = yr; rawL = xl; rawR = xr;
            offL *= coef; offR *= coef;
        }
        if (std::abs (offL) + std::abs (offR) < 1.0e-9) offL = offR = 0.0;
    }

    /** Version mono (pads des batteries). */
    void processMono (float* x, int n) noexcept { process (x, x, n, true); }

private:
    double coef = 0.99, stepFall = 0.999, offL = 0.0, offR = 0.0, lastL = 0.0, lastR = 0.0, rawL = 0.0, rawR = 0.0, maxStep = 0.0;
    bool armed = false;
};

/** Gain de vélocité : sensibilité 0..1. */
inline double velocityGain (double velocity, double sensitivity) noexcept
{
    return 1.0 - sensitivity + sensitivity * velocity * velocity;
}

} // namespace grv::synth
