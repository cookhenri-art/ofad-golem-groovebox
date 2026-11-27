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
#include "../../dsp/EqFilters.h"

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
        { "Principaux \xC2\xB7 Punchs", "PUNCH", "Punch", { "propre", "techno", "dur", "creux", "industriel", "oldschool", "distordu", "zaag", "psy (zap)" }, 16 },
        { "Principaux \xC2\xB7 Crunchs", "CRUNCH", "Crunch", { "raw", "schranz", "industriel", "bruit\xC3\xA9", "zaag", "reverse" }, 16 },
        { "Principaux \xC2\xB7 Rumbles", "RUMBLE", "Rumble", { "techno", "propre", "industriel", "hypnotique", "psy", "schranz" }, 16 },
        { "Transitoires \xC2\xB7 Clics", "TRANSITOIRE", "Clic", { "impulsion", "bruit", "FM", "tic", "num\xC3\xA9rique" }, 12 },
        { "Transitoires \xC2\xB7 Percs", "TRANSITOIRE", "Perc", { "rim", "bois", "m\xC3\xA9tal", "claqu\xC3\xA9" }, 10 },
        { "Transitoires \xC2\xB7 Tops de punch", "TOP", "Top de punch", { "propre", "dur", "bruit\xC3\xA9" }, 12 },
        { "Renforts \xC2\xB7 R\xC3\xA9verb\xC3\xA9rations de punch", "RENFORT", "R\xC3\xA9verb de punch", { "pi\xC3\xA8" "ce", "salle", "m\xC3\xA9tal" }, 10 },
        { "Renforts \xC2\xB7 Rumbles", "RUMBLE", "Renfort rumble", { "grave", "m\xC3\xA9" "dium", "granuleux" }, 12 },
        { "Renforts \xC2\xB7 Schranz", "RENFORT", "Renfort schranz", { "m\xC3\xA9tal", "grin\xC3\xA7" "ant", "bruit" }, 12 },
        { "Renforts \xC2\xB7 Tops", "TOP", "Renfort top", { "air", "gr\xC3\xA9sil", "craquant" }, 12 },
        { "Corps \xC2\xB7 Sub", "CORPS", "Corps", { "rond", "long", "court", "satur\xC3\xA9", "triangle", "basse psy", "basse psy FM" }, 10 },
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
namespace detail
{
    /** Valeur répartie du morceau k (0..n-1) sur l'axe d (suite à faible écart : les morceaux d'une
        saveur couvrent toute la fourchette, au lieu de se ressembler par hasard). */
    inline double spread (int k, int d) noexcept
    {
        static const double alpha[8] { 0.6180339887, 0.7548776662, 0.5698402910, 0.8191725134, 0.4142135624, 0.3247179572, 0.2360679775, 0.4655712319 };
        const auto v = 0.5 + (k + 1) * alpha[d & 7] + 0.137 * (d >> 3);
        return v - std::floor (v);
    }
    inline double lerp (double a, double b, double t) noexcept { return a + (b - a) * t; }
    inline double lerpLog (double a, double b, double t) noexcept { return a * std::pow (b / a, t); }

    /** Décimation d'un signal calculé à os × sr (sinus cardinal fenêtré de Blackman, coupure à
        0,45 × sr) : les saturations sont calculées suréchantillonnées, sans repliement audible. */
    inline std::vector<float> decimate (const std::vector<double>& x, int os)
    {
        if (os <= 1) return std::vector<float> (x.begin(), x.end());
        const int half = 12 * os, taps = 2 * half + 1;
        std::vector<double> h ((size_t) taps);
        const auto fc = 0.45 / os;
        double sum = 0.0;
        for (int j = 0; j < taps; ++j)
        {
            const auto t = j - half;
            const auto sinc = t == 0 ? 2.0 * fc : std::sin (dsp::kTwoPi * fc * t) / (dsp::kPi * t);
            const auto w = 0.42 - 0.5 * std::cos (dsp::kTwoPi * j / (taps - 1)) + 0.08 * std::cos (2.0 * dsp::kTwoPi * j / (taps - 1));
            h[(size_t) j] = sinc * w; sum += h[(size_t) j];
        }
        for (auto& v : h) v /= sum;
        const auto n = (int) x.size(), m = n / os;
        std::vector<float> y ((size_t) std::max (1, m));
        for (int i = 0; i < m; ++i)
        {
            double acc = 0.0;
            const auto c = i * os;
            for (int j = 0; j < taps; ++j)
            {
                const auto at = c + j - half;
                if (at >= 0 && at < n) acc += h[(size_t) j] * x[(size_t) at];
            }
            y[(size_t) i] = (float) acc;
        }
        return y;
    }

    /** Coupe le continu (les saturations asymétriques en créent) : passe-haut 12 Hz. */
    inline void dcBlock (std::vector<float>& v, double sr)
    {
        const auto a = std::exp (-dsp::kTwoPi * 12.0 / sr);
        double xm = 0.0, ym = 0.0;
        for (auto& s : v) { const double y = s - xm + a * ym; xm = s; ym = y; s = (float) y; }
    }
} // namespace detail

/** Calcule le morceau idx de la famille fam (mono, fréquence d'échantillonnage sr).
    keyHz : fondamentale de la tonalité du kick (ex. fa 1 = 43,65 Hz) ; bpm : tempo.
    1.2 (son) : toutes les parties tonales finissent JUSTE sur la tonalité (ou son octave), pour que
    les couches s'additionnent sans battre ; les saturations sont calculées à 4 × sr ; les
    morceaux d'une saveur se répartissent sur toute la fourchette de chaque réglage. */
inline std::vector<float> renderPart (int fam, int idx, double keyHz, double bpm, double sr)
{
    using namespace detail;
    std::vector<float> out;
    const auto n = partCount (fam);
    if (n <= 0 || sr <= 0.0) return out;
    idx = std::clamp (idx, 0, n - 1);
    const auto& fi = families()[(size_t) fam];
    const int flavor = idx / fi.perFlavor, k = idx % fi.perFlavor;
    Rng R ((std::uint32_t) (fam * 7919 + idx * 104729 + 17));
    keyHz = std::clamp (keyHz, 25.0, 120.0);
    bpm = std::clamp (bpm, 40.0, 300.0);
    const auto beat = 60.0 / bpm, s16 = beat / 4.0;
    auto S = [&] (int d) { return std::clamp (spread (k, d) + R.r (-0.03, 0.03), 0.0, 1.0); };

    // calcul suréchantillonné (familles saturées) : srx = os × sr, puis décimation
    int os = 1;
    switch (fam) { case fPunch: case fCrunch: case fRumble: case fPunchTop: case fEnhRumble: case fEnhSchranz: case fBody: case fReverse: os = 4; break; default: break; }
    const auto srx = sr * os, dt = 1.0 / srx;
    std::vector<double> buf;
    auto alloc = [&] (double seconds) { buf.assign ((size_t) std::max (16.0 * os, seconds * srx), 0.0); return (int) buf.size(); };

    switch (fam)
    {
        case fPunch:
        {
            if (flavor == 8)
            {
                // 26 nov. 2026 : kick psytrance. Le « zap » : la hauteur part très haut (2 à 8 kHz), tombe sur
                // la tonalité en 15 à 35 ms (courbe exponentielle très raide qui ARRIVE sur la tonalité au
                // bout du temps de chute), puis reste parfaitement plate : sinus propre, à peine saturé.
                // Corps court (le kick doit finir avant la première note de basse).
                const auto f1 = keyHz, f0 = std::min (srx * 0.2, lerpLog (2000.0, 8000.0, S (0)));
                const auto T = lerp (15.0, 35.0, S (1)) * 0.001, tau = T / lerp (3.5, 6.0, S (3));
                const auto eT = std::exp (-T / tau);
                const auto hold = lerp (0.0, 25.0, S (2)) * 0.001, dec = lerp (70.0, 150.0, S (6)) * 0.001;
                const int N = alloc (std::min (0.4, hold + dec * 1.4 + 0.01));
                const auto drive = lerp (1.0, 1.8, S (7));
                dsp::SvfEq hp;
                hp.set (dsp::SvfEq::highpass, 18.0, 0.7, 0.0, srx);
                double ph = 0.0;
                for (int i = 0; i < N; ++i)
                {
                    const auto t = i * dt;
                    const auto e = t >= T ? 0.0 : (std::exp (-t / tau) - eT) / (1.0 - eT);
                    ph += (f1 + (f0 - f1) * e) * dt; ph -= std::floor (ph);
                    const auto a = t < 0.0004 ? t / 0.0004 : t < hold ? 1.0 : std::exp (-6.9 * (t - hold) / dec);
                    buf[(size_t) i] = hp.process (soft (std::sin (dsp::kTwoPi * ph) * a, drive));
                }
                break;
            }
            // sinus balayé du haut vers la tonalité (fin JUSTE sur la tonalité ou son octave), balayage
            // en deux temps, « tok » de médium (résonance avant saturation), saturation par saveur
            struct Fl { double oLo, oHi, t1Lo, t1Hi, decLo, decHi, holdMax, knock; };
            static const Fl fl[8] {
                { 2.4, 3.8,  6.0, 22.0, 110, 260,  8, 0.0 },  // propre
                { 2.6, 4.2,  5.0, 18.0, 120, 300, 12, 0.15 }, // techno
                { 3.0, 4.8,  3.0, 10.0, 140, 320, 25, 0.9 },  // dur (hardstyle : tok)
                { 2.2, 3.6,  6.0, 25.0,  90, 220,  5, 0.0 },  // creux
                { 3.0, 5.0,  2.5, 12.0, 120, 300, 18, 0.6 },  // industriel
                { 1.9, 3.2, 10.0, 45.0, 160, 420, 10, 0.0 },  // oldschool (balayage lent, triangle)
                { 3.0, 4.8,  3.0, 14.0, 140, 340, 22, 0.7 },  // distordu
                { 1.6, 2.6,  8.0, 30.0, 150, 360, 30, 0.5 },  // zaag (fin à l'octave, FM)
            };
            const auto& c = fl[flavor];
            const auto endMul = flavor == 7 ? 2.0 : (flavor == 2 || flavor == 6) && S (5) > 0.6 ? 2.0 : 1.0;
            const auto f1 = keyHz * endMul, f0 = keyHz * std::pow (2.0, lerp (c.oLo, c.oHi, S (0)));
            const auto t1 = lerpLog (c.t1Lo, c.t1Hi, S (1)) * 0.001, t2 = t1 * lerp (4.0, 10.0, S (3)), mix2 = lerp (0.08, 0.4, S (4));
            const auto hold = c.holdMax * S (2) * 0.001, dec = lerp (c.decLo, c.decHi, S (6)) * 0.001;
            const auto len = std::min (0.6, hold + dec * 1.6 + 0.01);
            const int N = alloc (len);
            const auto drive = flavor == 0 ? lerp (1.0, 2.2, S (7)) : flavor == 1 ? lerp (1.6, 3.6, S (7)) : flavor == 2 ? lerp (4.0, 9.0, S (7))
                             : flavor == 3 ? lerp (2.0, 4.0, S (7)) : flavor == 4 ? lerp (5.0, 12.0, S (7)) : flavor == 5 ? lerp (1.2, 2.2, S (7))
                             : flavor == 6 ? lerp (5.0, 12.0, S (7)) : lerp (6.0, 14.0, S (7));
            dsp::SvfEq knock, notch, tone, hp;
            const auto fk = lerp (170.0, 420.0, S (8));
            knock.set (dsp::SvfEq::bandpass, fk, lerp (2.5, 6.0, S (9)), 0.0, srx);
            notch.set (dsp::SvfEq::bell, lerp (280.0, 700.0, S (8)), 1.2, -lerp (6.0, 14.0, S (9)), srx);
            tone.set (dsp::SvfEq::lowpass, flavor == 7 ? lerp (3000.0, 6500.0, S (10)) : flavor == 6 || flavor == 2 ? lerp (6000.0, 11000.0, S (10)) : 16000.0, 0.7, 0.0, srx);
            hp.set (dsp::SvfEq::highpass, flavor == 3 ? lerp (45.0, 90.0, S (11)) : 18.0, 0.7, 0.0, srx);
            F noiseBp (1, lerp (1200.0, 3500.0, S (11)), 1.5, srx), clickHp (2, 2500.0, 0.7, srx);
            const auto fm = flavor == 7 ? lerp (0.4, 1.8, S (12)) : 0.0, h2 = flavor == 1 ? lerp (0.05, 0.2, S (12)) : 0.0;
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                const auto e = (1.0 - mix2) * std::exp (-t / t1) + mix2 * std::exp (-t / t2);
                const auto f = f1 + (f0 - f1) * e;
                ph += f * dt; ph -= std::floor (ph);
                double y = flavor == 5 ? 0.7 * std::sin (dsp::kTwoPi * ph) + 0.3 * (4.0 * std::abs (ph - 0.5) - 1.0)
                         : std::sin (dsp::kTwoPi * ph + fm * std::sin (dsp::kTwoPi * 2.0 * ph)) + h2 * std::sin (dsp::kTwoPi * 2.0 * ph);
                const auto a = t < 0.0006 ? t / 0.0006 : t < hold ? 1.0 : std::exp (-6.9 * (t - hold) / dec);
                y *= a;
                if (c.knock > 0.0) y += c.knock * 2.5 * knock.process (y);
                if (flavor == 4) y += 0.35 * noiseBp (R.noise()) * a * std::exp (-t / 0.015);
                if (flavor == 5) y += 0.25 * clickHp (R.noise()) * std::exp (-t / 0.0025);
                switch (flavor)
                {
                    case 0: case 5: y = soft (y, drive); break;
                    case 1: y = (asym (y, drive) ) / std::tanh (drive); break;
                    case 2: y = soft (y, drive) * 0.85 + 0.15 * hard (y, drive); break;
                    case 3: y = notch.process (soft (y, drive)); break;
                    case 4: y = fold (soft (y, drive * 0.35), 1.0 + drive * 0.12); break;
                    case 6: y = hard (asym (y, drive * 0.5) * 1.4, 1.0); break;
                    default: y = hard (y, drive) * 0.8 + 0.2 * soft (y, drive); break;
                }
                buf[(size_t) i] = hp.process (tone.process (y));
            }
            break;
        }
        case fCrunch:
        {
            // médium saturé et tonal (tonalité × 1, 2 ou 4) ; haché en double-croches (schranz),
            // glissé (zaag), répété en montant (reverse)
            const auto len = flavor == 5 ? beat - s16 : lerp (0.16, 0.5, S (0));
            const int N = alloc (len);
            const double mults[3] { 1.0, 2.0, 4.0 };
            const auto mult = flavor == 4 ? 2.0 : mults[(int) (S (1) * 2.999)];
            const auto drive = lerp (6.0, 26.0, S (2));
            F hp (2, lerpLog (180.0, 700.0, S (3)), 0.8, srx), lp (0, lerpLog (2500.0, 8000.0, S (4)), 0.8, srx), bp (1, lerpLog (600.0, 2600.0, S (5)), lerp (1.0, 3.0, S (6)), srx);
            const auto comb = (int) std::max (1.0, srx / (keyHz * lerp (2.5, 5.0, S (7))));
            std::vector<double> cb ((size_t) comb + 1, 0.0);
            double ph = 0.0, ph2 = 0.0;
            int wpos = 0;
            const auto atk = lerp (2.0, 16.0, S (8)), dec = len * 1000.0 * lerp (0.6, 1.2, S (9));
            const auto chop = lerp (3.0, 7.0, S (10)), det = lerp (1.002, 1.012, S (11));
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                double f = keyHz * mult * (1.0 + 0.5 * std::exp (-t * 1000.0 / 30.0));
                if (flavor == 4) f = keyHz * (2.0 - 1.0 * std::min (1.0, t / std::max (0.05, len * 0.6))) * (1.0 + 0.01 * std::sin (dsp::kTwoPi * 7.0 * t));
                ph += f * dt; ph -= std::floor (ph);
                ph2 += f * det * dt; ph2 -= std::floor (ph2);
                double y = 0.6 * std::sin (dsp::kTwoPi * ph) + 0.4 * (2.0 * ph2 - 1.0);
                double a = env (t, atk, dec);
                if (flavor == 1 || flavor == 5)
                {
                    const auto pos = std::fmod (t, s16) / s16;
                    a *= flavor == 1 ? std::exp (-pos * chop) : std::pow (pos, 1.6);
                    if (flavor == 5 && t < s16 * 0.15) a *= t / (s16 * 0.15);
                }
                if (flavor == 3) y = 0.4 * y + 0.8 * R.noise();
                y *= a;
                switch (flavor)
                {
                    case 2: y = fold (soft (y, 3.0), 3.5); break;
                    case 4: y = hard (y, drive); break;
                    default: y = soft (y, drive); break;
                }
                if (flavor == 2)
                {
                    wpos = (wpos + 1) % comb;
                    const auto d = cb[(size_t) wpos];
                    cb[(size_t) wpos] = y + d * 0.6;
                    y = y + d * 0.5;
                }
                buf[(size_t) i] = flavor == 3 ? bp (y) * 1.4 : lp (hp (y));
            }
            break;
        }
        case fRumble:
        {
            // queue grave d'un temps : sinus de tonalité + bruit grave, saturés puis filtrés, pulsés en doubles-croches
            const int N = alloc (beat);
            const auto drive = flavor == 1 ? lerp (1.2, 2.5, S (0)) : flavor == 5 ? lerp (5.0, 10.0, S (0)) : flavor == 2 ? lerp (4.0, 8.0, S (0)) : lerp (2.0, 5.0, S (0));
            F lp (0, flavor == 5 ? lerp (280.0, 450.0, S (1)) : lerp (130.0, 260.0, S (1)), 0.8, srx), nlp (0, lerp (90.0, 180.0, S (2)), 1.5, srx), lp2 (0, 400.0, 0.7, srx);
            double ph = 0.0, ph2 = 0.0;
            const auto detune = lerp (0.002, 0.015, S (3)), noiseAmt = flavor == 1 ? lerp (0.05, 0.15, S (4)) : lerp (0.2, 0.6, S (4));
            const auto overall = lerp (0.25, 0.7, S (5)) * beat, duck = lerp (0.25, 0.7, S (6));
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * dt; ph -= std::floor (ph);
                ph2 += keyHz * (1.0 + detune) * dt; ph2 -= std::floor (ph2);
                double y = 0.55 * std::sin (dsp::kTwoPi * ph) + 0.45 * std::sin (dsp::kTwoPi * ph2) + noiseAmt * nlp (R.noise()) * 4.0;
                const auto pos = std::fmod (t, s16) / s16;
                double a;
                if (flavor == 3) a = 0.75 + 0.25 * std::sin (dsp::kTwoPi * t / s16 * 0.5);
                else if (flavor == 4) a = t < s16 ? 0.0 : std::exp (-pos * 3.2) * (1.0 - std::exp (-pos * 60.0));
                else a = (1.0 - duck * std::exp (-pos * 5.0)) * (1.0 - std::exp (-pos * 40.0)) * (0.6 + 0.4 * std::exp (-pos * 2.0));
                a *= std::exp (-t / overall * 0.9) * std::min (1.0, t / 0.008);
                y = soft (y * a, drive);
                y = lp (y);
                if (flavor == 2) y = 0.8 * y + 0.2 * lp2 (fold (y * 3.0, 2.0));
                buf[(size_t) i] = y;
            }
            break;
        }
        case fClick:
        {
            const int N = alloc (lerp (0.004, flavor == 1 ? 0.03 : 0.015, S (0)));
            const auto f = lerpLog (1200.0, 7000.0, S (1));
            F hp (2, lerpLog (1500.0, 7000.0, S (2)), 0.7, srx), bp (1, f, lerp (2.0, 8.0, S (3)), srx);
            const auto d1 = lerp (0.4, 1.6, S (4)), d2 = lerp (1.0, 6.0, S (4)), cyc = lerp (1.0, 3.0, S (5));
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                double y = 0.0;
                switch (flavor)
                {
                    case 0: ph += f * dt; y = std::sin (dsp::kTwoPi * ph) * std::exp (-ms / d1); break;
                    case 1: y = hp (R.noise()) * std::exp (-ms / d2); break;
                    case 2: ph += f * (1.0 + 3.0 * std::exp (-ms / 1.5)) * dt; y = std::sin (dsp::kTwoPi * ph + 2.0 * std::sin (dsp::kTwoPi * ph * 2.7)) * std::exp (-ms / (d1 * 1.5)); break;
                    case 3: y = bp (R.noise()) * std::exp (-ms / (d1 * 2.0)) * 2.0; break;
                    default: ph += f * dt; y = (std::fmod (ph, 1.0) < 0.5 ? 1.0 : -1.0) * (ms < 1000.0 / f * cyc ? 1.0 : 0.0); break;
                }
                buf[(size_t) i] = y;
            }
            break;
        }
        case fPerc:
        {
            const int N = alloc (lerp (0.02, 0.09, S (0)));
            const auto f = lerpLog (flavor == 2 ? 2000.0 : 600.0, flavor == 2 ? 6000.0 : 3000.0, S (1));
            F bp (1, f * lerp (1.2, 2.0, S (2)), lerp (3.0, 12.0, S (3)), srx), hp (2, 900.0, 0.7, srx);
            const auto ratio = lerp (1.38, 1.62, S (4));
            double ph = 0.0, ph2 = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                ph += f * dt; ph2 += f * ratio * dt;
                double y = 0.0;
                switch (flavor)
                {
                    case 0: y = (0.6 * std::sin (dsp::kTwoPi * ph) + 0.6 * bp (R.noise())) * std::exp (-ms / 12.0); break;
                    case 1: y = std::sin (dsp::kTwoPi * ph + 0.5 * std::sin (dsp::kTwoPi * ph2)) * std::exp (-ms / 18.0); break;
                    case 2: y = (std::sin (dsp::kTwoPi * ph) * std::sin (dsp::kTwoPi * ph2 * 1.7)) * std::exp (-ms / 25.0); break;
                    default: y = hp (R.noise()) * (std::exp (-std::fmod (ms, 7.0) / 1.8) * (ms < 21.0 ? 1.0 : std::exp (-(ms - 21.0) / 20.0))); break;
                }
                buf[(size_t) i] = y;
            }
            break;
        }
        case fPunchTop:
        {
            // haut d'un punch : balayage aigu vers le médium, filtré en passe-haut, saturé
            const int N = alloc (lerp (0.035, 0.11, S (0)));
            const auto f0 = lerpLog (2500.0, 6000.0, S (1)), f1 = lerpLog (250.0, 600.0, S (2));
            F hp (2, lerpLog (500.0, 1500.0, S (3)), 0.8, srx), bp (1, lerpLog (2000.0, 6000.0, S (4)), 1.0, srx);
            const auto ts = lerp (4.0, 12.0, S (5)), td = lerp (15.0, 40.0, S (6)), dr = flavor == 1 ? lerp (3.0, 8.0, S (7)) : lerp (1.5, 4.0, S (7));
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt, ms = t * 1000.0;
                ph += (f1 + (f0 - f1) * std::exp (-ms / ts)) * dt;
                double y = std::sin (dsp::kTwoPi * ph) * std::exp (-ms / td);
                if (flavor == 2) y += 0.5 * bp (R.noise()) * std::exp (-ms / 10.0);
                y = flavor == 1 ? hard (y, dr) : soft (y, dr);
                buf[(size_t) i] = hp (y);
            }
            break;
        }
        case fEnhReverb:
        {
            // réverbération courte d'un punch : bruit filtré qui décroît + modes de la pièce sur la tonalité
            const auto len = flavor == 1 ? lerp (0.35, 0.8, S (0)) : lerp (0.15, 0.4, S (0));
            const int N = alloc (len);
            F bp (1, lerpLog (300.0, 1800.0, S (1)), lerp (0.6, 1.5, S (2)), srx), lp (0, lerpLog (2500.0, 8000.0, S (3)), 0.7, srx);
            std::array<double, 4> modes {}, mph {};
            for (int m = 0; m < 4; ++m) modes[(size_t) m] = keyHz * (2.0 + m) * (flavor == 2 ? R.r (1.3, 2.7) : R.r (0.99, 1.01));
            const auto pre = lerp (0.003, 0.02, S (4));
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                double tonal = 0.0;
                for (int m = 0; m < 4; ++m) { mph[(size_t) m] += modes[(size_t) m] * dt; tonal += std::sin (dsp::kTwoPi * mph[(size_t) m]) / (m + 1.0); }
                const auto a = t < pre ? t / pre : std::exp (-6.9 * (t - pre) / len);
                buf[(size_t) i] = lp (bp (R.noise()) * 1.5 + tonal * (flavor == 2 ? 0.6 : 0.25)) * a;
            }
            break;
        }
        case fEnhRumble:
        {
            const int N = alloc (beat);
            F bp (1, flavor == 0 ? lerp (80.0, 140.0, S (0)) : flavor == 1 ? lerp (150.0, 320.0, S (0)) : lerp (200.0, 450.0, S (0)), lerp (1.0, 2.5, S (1)), srx), lp (0, 700.0, 0.7, srx);
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * (flavor == 0 ? 1.0 : 2.0) * dt;
                const auto pos = std::fmod (t, s16) / s16;
                const auto a = (1.0 - std::exp (-pos * 30.0)) * std::exp (-pos * 3.0) * std::exp (-t / beat * 1.5);
                double y = std::sin (dsp::kTwoPi * ph) + (flavor == 2 ? 1.2 : 0.4) * bp (R.noise()) * 3.0;
                buf[(size_t) i] = lp (soft (y * a, flavor == 2 ? 8.0 : 3.0));
            }
            break;
        }
        case fEnhSchranz:
        {
            const int N = alloc (beat * lerp (0.5, 1.0, S (0)));
            F bp (1, lerpLog (900.0, 3200.0, S (1)), lerp (2.0, 6.0, S (2)), srx), hp (2, 600.0, 0.7, srx);
            const auto comb = (int) std::max (2.0, srx / lerpLog (300.0, 1200.0, S (3)));
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
                buf[(size_t) i] = hp (y);
            }
            break;
        }
        case fEnhTop:
        {
            const int N = alloc (flavor == 0 ? lerp (0.08, 0.3, S (0)) : lerp (0.03, 0.15, S (0)));
            F hp (2, flavor == 0 ? lerpLog (6000.0, 11000.0, S (1)) : lerpLog (2500.0, 7000.0, S (1)), 0.8, srx), bp (1, lerpLog (3000.0, 9000.0, S (2)), 2.0, srx);
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                double y = flavor == 1 ? bp (R.noise()) * (R.u() < 0.3 ? 1.0 : 0.2) : R.noise();
                y = flavor == 2 ? hard (y, 3.0) : y;
                buf[(size_t) i] = hp (y) * env (t, flavor == 0 ? 2.0 : 0.2, (double) N / srx * 1000.0 * 0.8);
            }
            break;
        }
        case fBody:
        {
            if (flavor >= 5)
            {
                // 26 nov. 2026 : basse psytrance, une note par double-croche (à répéter au tempo : « Répétition
                // x4, sans le 1er » = kick - basse - basse - basse). Note courte sur la tonalité, sans chute
                // de hauteur. « basse psy » : scie (somme d'harmoniques, sans repliement) dans un passe-bas
                // qui se referme vite (le « pluck ») ; « basse psy FM » : sinus modulé par un sinus dont
                // l'indice retombe (le « squelch » rond).
                const auto len = lerp (0.08, 0.14, S (0));
                const int N = alloc (len + 0.004);
                const auto dec = lerp (45.0, 95.0, S (1)) * 0.001;
                const auto fHi = lerpLog (900.0, 3200.0, S (2)), fLo = lerpLog (110.0, 260.0, S (3)), ft = lerp (18.0, 60.0, S (4)) * 0.001;
                const auto res = lerp (0.9, 2.2, S (5));
                const auto idx0 = lerp (1.5, 4.5, S (2)), it = lerp (15.0, 45.0, S (4)) * 0.001, ratio = S (6) > 0.5 ? 2.0 : 1.0;
                const int H = std::max (1, std::min (40, (int) (srx * 0.45 / keyHz)));
                dsp::SvfEq lp, hp;
                hp.set (dsp::SvfEq::highpass, keyHz * 0.6, 0.7, 0.0, srx);
                double ph = 0.0, pm = 0.0;
                for (int i = 0; i < N; ++i)
                {
                    const auto t = i * dt;
                    ph += keyHz * dt; ph -= std::floor (ph);
                    double y;
                    if (flavor == 5)
                    {
                        y = 0.0;
                        for (int h = 1; h <= H; ++h) y += std::sin (dsp::kTwoPi * ph * h) / h;
                        y *= 0.55;
                        if ((i & 15) == 0) lp.set (dsp::SvfEq::lowpass, fLo + (fHi - fLo) * std::exp (-t / ft), res, 0.0, srx);
                        y = lp.process (y);
                    }
                    else
                    {
                        pm += keyHz * ratio * dt; pm -= std::floor (pm);
                        y = std::sin (dsp::kTwoPi * ph + idx0 * std::exp (-t / it) * std::sin (dsp::kTwoPi * pm));
                    }
                    const auto a = (t < 0.0015 ? t / 0.0015 : std::exp (-t / dec)) * (t < len ? 1.0 : std::max (0.0, 1.0 - (t - len) / 0.004));
                    buf[(size_t) i] = hp.process (soft (y * a, 1.3));
                }
                break;
            }
            // corps / sub : sinus sur la tonalité (fin JUSTE sur la tonalité), petite chute de hauteur,
            // long déclin ; rond (un peu d'harmonique 2), long (maintien), court, saturé (pair), triangle
            struct Fl { double decLo, decHi, holdMax; };
            static const Fl fl[5] { { 280, 650, 40 }, { 700, 1600, 220 }, { 110, 260, 10 }, { 280, 750, 60 }, { 280, 650, 40 } };
            const auto bend = lerp (1.05, 1.9, S (0)), bt = lerpLog (15.0, 110.0, S (1)) * 0.001;
            const auto dec = lerp (fl[flavor].decLo, fl[flavor].decHi, S (2)) * 0.001, hold = fl[flavor].holdMax * S (3) * 0.001;
            const auto h2 = flavor == 0 ? lerp (0.0, 0.12, S (4)) : 0.0, drive = lerp (2.0, 7.0, S (4));
            dsp::SvfEq tone; tone.set (dsp::SvfEq::lowpass, flavor == 3 ? lerp (2500.0, 6000.0, S (5)) : flavor == 4 ? lerp (900.0, 3000.0, S (5)) : 18000.0, 0.7, 0.0, srx);
            const int N = alloc (std::min (3.0, hold + dec * 1.3 + 0.02));
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * (1.0 + (bend - 1.0) * std::exp (-t / bt)) * dt; ph -= std::floor (ph);
                double y = flavor == 4 ? 4.0 * std::abs (ph - 0.5) - 1.0 : std::sin (dsp::kTwoPi * ph) + h2 * std::sin (dsp::kTwoPi * 2.0 * ph);
                const auto a = t < 0.0012 ? t / 0.0012 : t < hold ? 1.0 : std::exp (-6.9 * (t - hold) / dec);
                y *= a;
                if (flavor == 3) y = asym (y, drive) / std::tanh (drive);
                buf[(size_t) i] = tone.process (y);
            }
            break;
        }
        case fReverse:
        {
            // basse inversée : chaque double-croche monte en puissance (enveloppe montante) puis coupe net
            const auto count = 1 + (int) (S (0) * 2.999);
            const int N = alloc (s16 * count + 0.003);
            F lp (0, flavor == 2 ? lerp (400.0, 900.0, S (1)) : lerp (1200.0, 3500.0, S (1)), 0.8, srx);
            const auto drive = flavor == 1 ? lerp (6.0, 14.0, S (2)) : flavor == 0 ? lerp (2.5, 5.0, S (2)) : lerp (1.0, 2.0, S (2));
            const auto curve = lerp (1.5, 3.5, S (3));
            double ph = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const auto t = i * dt;
                ph += keyHz * dt; ph -= std::floor (ph);
                const auto pos = std::min (1.0, std::fmod (t, s16) / s16);
                const auto a = std::pow (pos, curve) * (t < s16 * count ? 1.0 : 0.0);
                const auto y = std::sin (dsp::kTwoPi * ph) + 0.3 * std::sin (dsp::kTwoPi * ph * 2.0);
                buf[(size_t) i] = lp (soft (y * a, drive));
            }
            break;
        }
        default: break;
    }
    out = decimate (buf, os);
    if (os > 1 || fam == fPunch || fam == fBody) dcBlock (out, sr);
    normalize (out, 0.9);
    tailFade (out, sr);
    return out;
}

} // namespace grv::instruments::kf
