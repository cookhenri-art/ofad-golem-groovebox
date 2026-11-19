/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Kick Forge (1.2) — bibliothèque de morceaux de kicks CALCULÉS.
    Principe des constructeurs de kicks par couches : un kick se monte en empilant des morceaux
    (punch, clic, crunch, rumble, renforts…). Aucun morceau d'un autre fabricant : chaque morceau est
    fabriqué ici, à la demande, par une recette de synthèse (balayage de hauteur, saturations,
    filtres, bruit, impulsions calées sur le tempo), réglée par un tirage déterministe propre à son
    numéro. Les morceaux tonals sont fabriqués directement dans la tonalité du kick (keyHz) et les
    rumbles, crunchs hachés et reverse bass suivent le tempo (bpm).
    Rien n'est gardé en mémoire : un morceau se recalcule en quelques millisecondes (fil de calcul).
*/

#pragma once

#include "../../dsp/Common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace grv::instruments::kf
{

enum Family
{
    fPunch = 0, fCrunch, fRumble, fClick, fPerc, fPunchTop, fEnhReverb, fEnhRumble, fEnhSchranz, fEnhTop, fBody, fReverse,
    fSample, // le son glissé sur la couche (emplacement d'échantillon de la couche)
    numFamilies
};

struct FamilyInfo
{
    const char* name;      // nom de la famille (bibliothèque)
    const char* tag;       // étiquette courte de la couche
    const char* part;      // début du nom d'un morceau
    std::vector<const char*> flavors;
    int perFlavor;
};

inline const std::array<FamilyInfo, numFamilies>& families()
{
    static const std::array<FamilyInfo, numFamilies> f {{
        { "Principaux \xC2\xB7 Punchs", "PUNCH", "Punch", { "propre", "techno", "dur", "creux", "industriel", "oldschool", "distordu", "zaag" }, 16 },
        { "Principaux \xC2\xB7 Crunchs", "CRUNCH", "Crunch", { "raw", "schranz", "industriel", "bruit\xC3\xA9", "zaag", "reverse" }, 16 },
        { "Principaux \xC2\xB7 Rumbles", "RUMBLE", "Rumble", { "techno", "propre", "industriel", "hypnotique", "psy", "schranz" }, 16 },
        { "Transitoires \xC2\xB7 Clics", "TRANSITOIRE", "Clic", { "impulsion", "bruit", "FM", "tic", "num\xC3\xA9rique" }, 12 },
        { "Transitoires \xC2\xB7 Percs", "TRANSITOIRE", "Perc", { "rim", "bois", "m\xC3\xA9tal", "claqu\xC3\xA9" }, 10 },
        { "Transitoires \xC2\xB7 Tops de punch", "TOP", "Top de punch", { "propre", "dur", "bruit\xC3\xA9" }, 12 },
        { "Renforts \xC2\xB7 R\xC3\xA9verb\xC3\xA9rations de punch", "RENFORT", "R\xC3\xA9verb de punch", { "pi\xC3\xA8" "ce", "salle", "m\xC3\xA9tal" }, 10 },
        { "Renforts \xC2\xB7 Rumbles", "RUMBLE", "Renfort rumble", { "grave", "m\xC3\xA9" "dium", "granuleux" }, 12 },
        { "Renforts \xC2\xB7 Schranz", "RENFORT", "Renfort schranz", { "m\xC3\xA9tal", "grin\xC3\xA7" "ant", "bruit" }, 12 },
        { "Renforts \xC2\xB7 Tops", "TOP", "Renfort top", { "air", "gr\xC3\xA9sil", "craquant" }, 12 },
        { "Corps \xC2\xB7 Sub", "CORPS", "Corps", { "rond", "long", "court", "satur\xC3\xA9", "triangle" }, 10 },
        { "Reverse bass", "REVERSE", "Reverse bass", { "psy", "hard", "doux" }, 12 },
        { "Mes samples", "SAMPLE", "Sample", {}, 0 },
    }};
    return f;
}

inline int partCount (int fam) noexcept
{
    if (fam < 0 || fam >= numFamilies) return 0;
    const auto& f = families()[(size_t) fam];
    return (int) f.flavors.size() * f.perFlavor;
}

inline const char* noteName (int midi) noexcept
{
    static const char* n[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((midi % 12) + 12) % 12];
}

inline std::string partName (int fam, int idx)
{
    const auto n = partCount (fam);
    if (n <= 0) return "";
    idx = std::clamp (idx, 0, n - 1);
    const auto& f = families()[(size_t) fam];
    const auto flavor = idx / f.perFlavor, k = idx % f.perFlavor + 1;
    return std::string (f.part) + " " + f.flavors[(size_t) flavor] + " " + (k < 10 ? "0" : "") + std::to_string (k);
}

//==============================================================================
namespace detail
{
    struct Rng
    {
        std::uint32_t s;
        explicit Rng (std::uint32_t seed) : s (seed * 2654435761u + 0x9E3779B9u) { for (int i = 0; i < 4; ++i) next(); }
        std::uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        double u() noexcept { return (next() >> 8) * (1.0 / 16777216.0); }
        double r (double a, double b) noexcept { return a + (b - a) * u(); }
        double lr (double a, double b) noexcept { return a * std::pow (b / a, u()); } // tirage logarithmique
        double noise() noexcept { return 2.0 * u() - 1.0; }
        int i (int n) noexcept { return (int) (u() * n) % std::max (1, n); }
    };

    inline double soft (double x, double k) noexcept { return k <= 0.01 ? x : std::tanh (k * x) / std::tanh (k); }
    inline double hard (double x, double k) noexcept { return std::clamp (x * k, -1.0, 1.0); }
    inline double fold (double x, double k) noexcept { return std::sin (std::asin (std::clamp (x, -1.0, 1.0)) * k) ; }
    inline double asym (double x, double k) noexcept { return std::tanh (k * x + 0.35) - std::tanh (0.35); }

    /** Filtre d'état hors temps réel (réglé une fois). */
    struct F
    {
        dsp::Svf s;
        int mode = 0; // 0 bas, 1 bande (normalisée), 2 haut
        double k = 1.0;
        F (int m, double hz, double q, double sr) : mode (m), k (1.0 / std::max (0.05, q)) { s.set (hz, q, sr); }
        double operator() (double x) noexcept { const auto o = s.process (x); return mode == 0 ? o.low : mode == 1 ? o.band * k : o.high; }
    };

    inline void normalize (std::vector<float>& v, double peak = 0.9)
    {
        double m = 0.0;
        for (auto x : v) m = std::max (m, (double) std::abs (x));
        if (m < 1.0e-9) return;
        const auto g = peak / m;
        for (auto& x : v) x = (float) (x * g);
    }

    /** Fondu de sortie court (pas de clic en fin de morceau). */
    inline void tailFade (std::vector<float>& v, double sr, double ms = 4.0)
    {
        const auto n = std::min ((int) v.size(), std::max (1, (int) (ms * 0.001 * sr)));
        for (int i = 0; i < n; ++i) v[v.size() - 1 - (size_t) i] *= (float) ((double) i / n);
    }

    inline double env (double t, double atkMs, double decMs) noexcept
    {
        const auto a = atkMs * 0.001;
        if (t < a) return t / a;
        return std::exp (-6.9 * (t - a) / std::max (1.0e-4, decMs * 0.001));
    }
} // namespace detail

//==============================================================================
/** Calcule le morceau idx de la famille fam (mono, fréquence d'échantillonnage sr).
    keyHz : fondamentale de la tonalité du kick (ex. fa 1 = 43,65 Hz) ; bpm : tempo. */
inline std::vector<float> renderPart (int fam, int idx, double keyHz, double bpm, double sr)
{
    using namespace detail;
    std::vector<float> out;
    const auto n = partCount (fam);
    if (n <= 0 || sr <= 0.0) return out;
    idx = std::clamp (idx, 0, n - 1);
    const auto& fi = families()[(size_t) fam];
    const int flavor = idx / fi.perFlavor;
    Rng R ((std::uint32_t) (fam * 7919 + idx * 104729 + 17));
    keyHz = std::clamp (keyHz, 25.0, 120.0);
    bpm = std::clamp (bpm, 40.0, 300.0);
    const auto beat = 60.0 / bpm, s16 = beat / 4.0;
    const auto dt = 1.0 / sr;
    auto alloc = [&] (double seconds) { out.assign ((size_t) std::max (16.0, seconds * sr), 0.0f); return (int) out.size(); };

    switch (fam)
    {
        case fPunch:
        {
            // sinus (ou triangle) balayé du haut vers la tonalité ; saturation propre à chaque saveur
            const auto f0 = keyHz * std::pow (2.0, R.r (2.6, 4.3));
            const auto f1 = keyHz * (flavor == 7 ? R.r (1.9, 2.2) : R.r (0.98, 2.05));
            const auto sweep = flavor == 7 ? R.r (35.0, 70.0) : flavor == 2 ? R.r (6.0, 16.0) : R.r (9.0, 32.0);
            const auto dec = R.r (90.0, 240.0) * (flavor == 5 ? 1.25 : 1.0);
            const auto len = R.r (0.1, 0.26) * (flavor == 5 ? 1.3 : 1.0);
            const auto drive = flavor == 0 ? R.r (0.8, 2.0) : flavor == 1 ? R.r (2.0, 4.0) : flavor == 2 ? R.r (5.0, 9.0) : flavor == 3 ? R.r (3.0, 5.0)
                             : flavor == 4 ? R.r (8.0, 16.0) : flavor == 5 ? R.r (1.2, 2.5) : flavor == 6 ? R.r (2.0, 4.0) : R.r (6.0, 11.0);
            const int N = alloc (len);
            const auto tri = flavor == 5 || R.u() < 0.15;
            F hp (2, flavor == 3 ? R.r (120.0, 260.0) : 20.0, 0.7, sr), bp (1, R.r (700.0, 2200.0), 1.2, sr), lp (0, flavor == 7 ? R.r (2500.0, 5000.0) : 16000.0, 0.7, sr);
            double ph = 0.0;
            const auto slow = R.r (0.0, 0.4); // deuxième balayage lent (oldschool)
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                const auto e1 = std::exp (-t * 1000.0 / sweep), e2 = std::exp (-t * 1000.0 / (sweep * 6.0));
                const auto f = f1 + (f0 - f1) * (e1 * (1.0 - slow) + e2 * slow);
                ph += f * dt; ph -= std::floor (ph);
                double y = tri ? 4.0 * std::abs (ph - 0.5) - 1.0 : std::sin (dsp::kTwoPi * ph);
                y *= env (t, 0.4, dec);
                if (flavor == 4) y = y * 0.85 + 0.25 * bp (R.noise()) * env (t, 0.1, dec * 0.4);
                switch (flavor)
                {
                    case 2: case 7: y = hard (y, drive) * 0.9 + 0.1 * soft (y, drive); break;
                    case 4: y = fold (soft (y, drive * 0.4), 1.0 + drive * 0.12); break;
                    case 6: y = fold (y, 1.0 + drive * 0.4); break;
                    default: y = soft (y, drive); break;
                }
                y = hp (y);
                out[(size_t) i] = (float) lp (y);
            }
            break;
        }
        case fCrunch:
        {
            // médium saturé et tonal (tonalité × 1, 2 ou 3) ; haché en double-croches, glissé (zaag), répété (reverse)
            const auto len = flavor == 5 ? beat - s16 : R.r (0.18, 0.5);
            const int N = alloc (len);
            const auto mult = flavor == 4 ? 2.0 : (double) (1 + R.i (2));
            const auto drive = R.r (8.0, 28.0);
            F hp (2, R.lr (220.0, 700.0), 0.8, sr), lp (0, R.lr (2500.0, 7000.0), 0.8, sr), bp (1, R.lr (600.0, 2500.0), R.r (1.0, 3.0), sr);
            const auto comb = (int) std::max (1.0, sr / (keyHz * R.r (2.5, 5.0)));
            std::vector<double> cb ((size_t) comb + 1, 0.0);
            double ph = 0.0, ph2 = 0.0;
            int wpos = 0;
            const auto atk = R.r (3.0, 18.0), dec = len * 1000.0 * R.r (0.6, 1.2);
            const auto chop = R.r (4.0, 6.0);
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                double f = keyHz * mult * (1.0 + 0.5 * std::exp (-t * 1000.0 / 30.0));
                if (flavor == 4) f = keyHz * (2.0 - 1.0 * std::min (1.0, t / std::max (0.05, len * 0.6))) * (1.0 + 0.01 * std::sin (dsp::kTwoPi * 7.0 * t));
                ph += f * dt; ph -= std::floor (ph);
                ph2 += f * 1.005 * dt; ph2 -= std::floor (ph2);
                double y = 0.6 * std::sin (dsp::kTwoPi * ph) + 0.4 * (2.0 * ph2 - 1.0);
                double a = env (t, atk, dec);
                if (flavor == 1 || flavor == 5)
                {
                    // haché sur les doubles-croches (schranz) ; enveloppe montante dans chaque double-croche (reverse)
                    const auto pos = std::fmod (t, s16) / s16;
                    a *= flavor == 1 ? std::exp (-pos * chop) : std::pow (pos, 1.6);
                    if (flavor == 5 && t < s16 * 0.15) a *= t / (s16 * 0.15);
                }
                if (flavor == 3) y = 0.4 * y + 0.8 * R.noise();
                y *= a;
                switch (flavor)
                {
                    case 2: y = fold (soft (y, 3.0), 3.5); break;
                    case 4: y = hard (y, drive) ; break;
                    default: y = soft (y, drive); break;
                }
                if (flavor == 2)
                {
                    wpos = (wpos + 1) % comb;
                    const auto d = cb[(size_t) wpos];
                    cb[(size_t) wpos] = y + d * 0.6;
                    y = y + d * 0.5;
                }
                y = flavor == 3 ? bp (y) * 1.4 : lp (hp (y));
                out[(size_t) i] = (float) y;
            }
            break;
        }
        case fRumble:
        {
            // queue grave d'un temps : sinus de tonalité + bruit grave, saturés puis filtrés, pulsés en doubles-croches
            const int N = alloc (beat);
            const auto drive = flavor == 1 ? R.r (1.2, 2.5) : flavor == 5 ? R.r (5.0, 10.0) : flavor == 2 ? R.r (4.0, 8.0) : R.r (2.0, 5.0);
            F lp (0, flavor == 5 ? R.r (280.0, 450.0) : R.r (130.0, 260.0), 0.8, sr), nlp (0, R.r (90.0, 180.0), 1.5, sr), lp2 (0, 400.0, 0.7, sr);
            double ph = 0.0, ph2 = 0.0;
            const auto detune = R.r (0.002, 0.015), noiseAmt = flavor == 1 ? R.r (0.05, 0.15) : R.r (0.2, 0.6);
            const auto overall = R.r (0.25, 0.7) * beat; // décroissance d'ensemble
            const auto duck = R.r (0.25, 0.7);
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * dt; ph -= std::floor (ph);
                ph2 += keyHz * (1.0 + detune) * dt; ph2 -= std::floor (ph2);
                double y = 0.55 * std::sin (dsp::kTwoPi * ph) + 0.45 * std::sin (dsp::kTwoPi * ph2) + noiseAmt * nlp (R.noise()) * 4.0;
                const auto pos = std::fmod (t, s16) / s16;
                double a;
                if (flavor == 3) a = 0.75 + 0.25 * std::sin (dsp::kTwoPi * t / s16 * 0.5); // hypnotique : continu, respiration lente
                else if (flavor == 4) a = t < s16 ? 0.0 : std::exp (-pos * 3.2) * (1.0 - std::exp (-pos * 60.0)); // psy : roulement après le kick
                else a = (1.0 - duck * std::exp (-pos * 5.0)) * (1.0 - std::exp (-pos * 40.0)) * (0.6 + 0.4 * std::exp (-pos * 2.0));
                a *= std::exp (-t / overall * 0.9) * std::min (1.0, t / 0.008);
                y = soft (y * a, drive);
                y = lp (y);
                if (flavor == 2) y = 0.8 * y + 0.2 * lp2 (fold (y * 3.0, 2.0));
                out[(size_t) i] = (float) y;
            }
            break;
        }
        case fClick:
        {
            const int N = alloc (R.r (0.004, flavor == 1 ? 0.03 : 0.015));
            const auto f = R.lr (1200.0, 7000.0);
            F hp (2, R.lr (1500.0, 7000.0), 0.7, sr), bp (1, f, R.r (2.0, 8.0), sr);
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                double y = 0.0;
                switch (flavor)
                {
                    case 0: ph += f * dt; y = std::sin (dsp::kTwoPi * ph) * std::exp (-ms / R.r (0.5, 1.5)); break;
                    case 1: y = hp (R.noise()) * std::exp (-ms / R.r (1.0, 6.0)); break;
                    case 2: ph += f * (1.0 + 3.0 * std::exp (-ms / 1.5)) * dt; y = std::sin (dsp::kTwoPi * ph + 2.0 * std::sin (dsp::kTwoPi * ph * 2.7)) * std::exp (-ms / 2.0); break;
                    case 3: y = bp (R.noise()) * std::exp (-ms / R.r (1.0, 3.0)) * 2.0; break;
                    default: ph += f * dt; y = (std::fmod (ph, 1.0) < 0.5 ? 1.0 : -1.0) * (ms < 1000.0 / f * R.r (1.0, 3.0) ? 1.0 : 0.0); break;
                }
                out[(size_t) i] = (float) y;
            }
            break;
        }
        case fPerc:
        {
            const int N = alloc (R.r (0.02, 0.09));
            const auto f = R.lr (flavor == 2 ? 2000.0 : 600.0, flavor == 2 ? 6000.0 : 3000.0);
            F bp (1, f * R.r (1.2, 2.0), R.r (3.0, 12.0), sr), hp (2, 900.0, 0.7, sr);
            double ph = 0.0, ph2 = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                ph += f * dt; ph2 += f * R.r (1.4, 1.42) * dt;
                double y = 0.0;
                switch (flavor)
                {
                    case 0: y = (0.6 * std::sin (dsp::kTwoPi * ph) + 0.6 * bp (R.noise())) * std::exp (-ms / 12.0); break;
                    case 1: y = std::sin (dsp::kTwoPi * ph + 0.5 * std::sin (dsp::kTwoPi * ph2)) * std::exp (-ms / 18.0); break;
                    case 2: y = (std::sin (dsp::kTwoPi * ph) * std::sin (dsp::kTwoPi * ph2 * 1.7)) * std::exp (-ms / 25.0); break;
                    default: y = hp (R.noise()) * (std::exp (-std::fmod (ms, 7.0) / 1.8) * (ms < 21.0 ? 1.0 : std::exp (-(ms - 21.0) / 20.0))); break;
                }
                out[(size_t) i] = (float) y;
            }
            break;
        }
        case fPunchTop:
        {
            // haut d'un punch : balayage aigu vers le médium, filtré en passe-haut, saturé
            const int N = alloc (R.r (0.035, 0.11));
            const auto f0 = R.lr (2500.0, 6000.0), f1 = R.lr (250.0, 600.0);
            F hp (2, R.lr (500.0, 1500.0), 0.8, sr), bp (1, R.lr (2000.0, 6000.0), 1.0, sr);
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                ph += (f1 + (f0 - f1) * std::exp (-ms / R.r (4.0, 12.0))) * dt;
                double y = std::sin (dsp::kTwoPi * ph) * std::exp (-ms / R.r (15.0, 40.0));
                if (flavor == 2) y += 0.5 * bp (R.noise()) * std::exp (-ms / 10.0);
                y = flavor == 1 ? hard (y, R.r (3.0, 8.0)) : soft (y, R.r (1.5, 4.0));
                out[(size_t) i] = (float) hp (y);
            }
            break;
        }
        case fEnhReverb:
        {
            // réverbération courte d'un punch : bruit filtré qui décroît + modes de la pièce sur la tonalité
            const auto len = flavor == 1 ? R.r (0.35, 0.8) : R.r (0.15, 0.4);
            const int N = alloc (len);
            F bp (1, R.lr (300.0, 1800.0), R.r (0.6, 1.5), sr), lp (0, R.lr (2500.0, 8000.0), 0.7, sr);
            std::array<double, 4> modes {}, mph {};
            for (int m = 0; m < 4; ++m) modes[(size_t) m] = keyHz * (2.0 + m) * (flavor == 2 ? R.r (1.3, 2.7) : R.r (0.98, 1.02));
            const auto pre = R.r (0.003, 0.02);
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                double tonal = 0.0;
                for (int m = 0; m < 4; ++m) { mph[(size_t) m] += modes[(size_t) m] * dt; tonal += std::sin (dsp::kTwoPi * mph[(size_t) m]) / (m + 1.0); }
                const auto a = t < pre ? t / pre : std::exp (-6.9 * (t - pre) / len);
                out[(size_t) i] = (float) (lp (bp (R.noise()) * 1.5 + tonal * (flavor == 2 ? 0.6 : 0.25)) * a);
            }
            break;
        }
        case fEnhRumble:
        {
            const int N = alloc (beat);
            F bp (1, flavor == 0 ? R.r (80.0, 140.0) : flavor == 1 ? R.r (150.0, 320.0) : R.r (200.0, 450.0), R.r (1.0, 2.5), sr), lp (0, 700.0, 0.7, sr);
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * (flavor == 0 ? 1.0 : 2.0) * dt;
                const auto pos = std::fmod (t, s16) / s16;
                const auto a = (1.0 - std::exp (-pos * 30.0)) * std::exp (-pos * 3.0) * std::exp (-t / beat * 1.5);
                double y = std::sin (dsp::kTwoPi * ph) + (flavor == 2 ? 1.2 : 0.4) * bp (R.noise()) * 3.0;
                out[(size_t) i] = (float) lp (soft (y * a, flavor == 2 ? 8.0 : 3.0));
            }
            break;
        }
        case fEnhSchranz:
        {
            const int N = alloc (beat * R.r (0.5, 1.0));
            F bp (1, R.lr (900.0, 3200.0), R.r (2.0, 6.0), sr), hp (2, 600.0, 0.7, sr);
            const auto comb = (int) std::max (2.0, sr / R.lr (300.0, 1200.0));
            std::vector<double> cb ((size_t) comb, 0.0);
            int w = 0;
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * 4.0 * dt;
                const auto pos = std::fmod (t, s16) / s16;
                const auto a = std::exp (-pos * 4.0) * (1.0 - std::exp (-pos * 80.0)) * std::exp (-t / beat);
                double x = flavor == 2 ? R.noise() : 0.5 * R.noise() + 0.5 * std::sin (dsp::kTwoPi * ph);
                x = bp (x) * a * 3.0;
                const auto d = cb[(size_t) w];
                cb[(size_t) w] = x + d * (flavor == 0 ? 0.85 : 0.6);
                w = (w + 1) % comb;
                double y = x + d * 0.7;
                y = flavor == 1 ? fold (y, 2.5) : soft (y, 4.0);
                out[(size_t) i] = (float) hp (y);
            }
            break;
        }
        case fEnhTop:
        {
            const int N = alloc (flavor == 0 ? R.r (0.08, 0.3) : R.r (0.03, 0.15));
            F hp (2, flavor == 0 ? R.lr (6000.0, 11000.0) : R.lr (2500.0, 7000.0), 0.8, sr), bp (1, R.lr (3000.0, 9000.0), 2.0, sr);
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                double y = flavor == 1 ? bp (R.noise()) * (R.u() < 0.3 ? 1.0 : 0.2) : R.noise();
                y = flavor == 2 ? hard (y, 3.0) : y;
                out[(size_t) i] = (float) (hp (y) * env (t, flavor == 0 ? 2.0 : 0.2, (double) N / sr * 1000.0 * 0.8) * (ms < 0.0 ? 0.0 : 1.0));
            }
            break;
        }
        case fBody:
        {
            // corps / sub : sinus sur la tonalité, petite chute de hauteur, long déclin
            const auto dec = flavor == 1 ? R.r (700.0, 1500.0) : flavor == 2 ? R.r (120.0, 260.0) : R.r (280.0, 650.0);
            const int N = alloc (dec * 0.0012 + 0.02);
            const auto bend = R.r (1.2, 1.8), bt = R.r (25.0, 90.0);
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * (1.0 + (bend - 1.0) * std::exp (-t * 1000.0 / bt)) * dt; ph -= std::floor (ph);
                double y = flavor == 4 ? 4.0 * std::abs (ph - 0.5) - 1.0 : std::sin (dsp::kTwoPi * ph);
                y *= env (t, 1.0, dec);
                if (flavor == 3) y = soft (y, R.r (3.0, 6.0));
                out[(size_t) i] = (float) y;
            }
            break;
        }
        case fReverse:
        {
            // basse inversée : chaque double-croche monte en puissance (enveloppe montante) puis coupe net
            const auto count = 1 + R.i (3);
            const int N = alloc (s16 * count + 0.003);
            F lp (0, flavor == 2 ? R.r (400.0, 900.0) : R.r (1200.0, 3500.0), 0.8, sr);
            const auto drive = flavor == 1 ? R.r (6.0, 14.0) : flavor == 0 ? R.r (2.5, 5.0) : R.r (1.0, 2.0);
            const auto curve = R.r (1.5, 3.5);
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * dt; ph -= std::floor (ph);
                const auto pos = std::min (1.0, std::fmod (t, s16) / s16);
                const auto a = std::pow (pos, curve) * (t < s16 * count ? 1.0 : 0.0);
                const auto y = std::sin (dsp::kTwoPi * ph) + 0.3 * std::sin (dsp::kTwoPi * ph * 2.0);
                out[(size_t) i] = (float) lp (soft (y * a, drive));
            }
            break;
        }
        default: break;
    }
    normalize (out, 0.9);
    tailFade (out, sr);
    return out;
}

} // namespace grv::instruments::kf
