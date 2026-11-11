/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Briques DSP communes aux voix : conversions, coefficients d'enveloppe, saturation
    anti-repliée, bruit, filtres. Classes pures C++ (aucune dépendance JUCE), sans allocation.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace grv::dsp
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }

/** Coefficient d'un lissage exponentiel atteignant ~63 % de la cible en timeMs. */
inline double timeConstantCoef (double timeMs, double sampleRate) noexcept
{
    const auto samples = std::max (1.0, timeMs * 0.001 * sampleRate);
    return std::exp (-1.0 / samples);
}

/** Coefficient de déclin atteignant -60 dB en timeMs. */
inline double decay60Coef (double timeMs, double sampleRate) noexcept
{
    const auto samples = std::max (1.0, timeMs * 0.001 * sampleRate);
    return std::exp (std::log (0.001) / samples);
}

//==============================================================================
// Fonctions rapides pour le fil audio (précision largement suffisante pour l'audio)

/** sin(2π·phase), erreur < 4e-6, pour toute phase. */
inline double fastSin2Pi (double phase) noexcept
{
    auto x = phase - std::floor (phase + 0.5);          // [-0.5, 0.5)
    if (x > 0.25) x = 0.5 - x; else if (x < -0.25) x = -0.5 - x;
    const auto t = kTwoPi * x, t2 = t * t;               // [-π/2, π/2]
    return t * (1.0 + t2 * (-1.0 / 6.0 + t2 * (1.0 / 120.0 + t2 * (-1.0 / 5040.0 + t2 * (1.0 / 362880.0)))));
}

/** 2^x, erreur relative < 3e-7 sur [-60, 60]. */
inline double fastExp2 (double x) noexcept
{
    x = std::clamp (x, -60.0, 60.0);
    const auto xi = std::floor (x + 0.5);
    const auto f = x - xi;                               // [-0.5, 0.5]
    // série de 2^f = e^(f·ln 2) jusqu'au degré 6
    const auto p = 1.0 + f * (0.6931471805599453 + f * (0.2402265069591007 + f * (0.0555041086648216
                     + f * (0.0096181291076285 + f * (0.0013333558146428 + f * 0.0001540353039338)))));
    // multiplication par 2^xi en écrivant directement l'exposant (évite ldexp)
    const auto bits = (std::uint64_t) ((std::int64_t) xi + 1023) << 52;
    double scale;
    std::memcpy (&scale, &bits, sizeof scale);
    return p * scale;
}

/** log2(x) pour x > 0, erreur absolue < 1e-4 (exposant lu dans le nombre, mantisse par série). */
inline double fastLog2 (double x) noexcept
{
    x = std::max (x, 1.0e-300);
    std::uint64_t bits;
    std::memcpy (&bits, &x, sizeof bits);
    const auto e = (double) ((int) ((bits >> 52) & 0x7FF) - 1023);
    bits = (bits & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull; // mantisse dans [1, 2)
    double m;
    std::memcpy (&m, &bits, sizeof m);
    // log2(m) = 2·atanh((m-1)/(m+1)) / ln 2, série jusqu'au degré 7
    const auto t = (m - 1.0) / (m + 1.0), t2 = t * t;
    return e + 2.8853900817779268 * t * (1.0 + t2 * (1.0 / 3.0 + t2 * (0.2 + t2 * (1.0 / 7.0))));
}

/** Interpolation d'Hermite à 4 points (x−1, x0, x1, x2), f dans [0, 1). */
inline double hermite4 (double xm1, double x0, double x1, double x2, double f) noexcept
{
    const auto c1 = 0.5 * (x1 - xm1);
    const auto c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
    const auto c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

/** Rampe linéaire par échantillon vers une cible fixée une fois par bloc : supprime les marches
    (« zipper ») des réglages et modulations calculés par bloc. */
class LinearRamp
{
public:
    void reset (double v) noexcept { cur = target = v; step = 0.0; left = 0; }
    /** Nouvelle cible atteinte en n échantillons (n >= 1). */
    void setTarget (double v, int n) noexcept
    {
        if (v == target && left == 0) return;
        target = v;
        left = std::max (1, n);
        step = (target - cur) / (double) left;
    }
    double next() noexcept
    {
        if (left > 0) { cur += step; if (--left == 0) cur = target; }
        return cur;
    }
    double get() const noexcept { return cur; }
private:
    double cur = 0.0, target = 0.0, step = 0.0;
    int left = 0;
};

/** tanh approchée (Padé), précise à 2 % près et bornée à ±1. */
inline double fastTanh (double x) noexcept
{
    x = std::clamp (x, -3.0, 3.0);
    const auto x2 = x * x;
    return x * (27.0 + x2) / (27.0 + 9.0 * x2);
}

/** Écrêtage doux algébrique x / √(1 + x²) : proche de tanh, primitive exacte et peu coûteuse. */
inline double softClip (double x) noexcept { return x / std::sqrt (1.0 + x * x); }

/** Écrêtage doux avec anti-repliement par antidérivée (ADAA, 1er ordre).
    Réf. : Parker, Zavalishin, Le Bivic, « Reducing the aliasing of nonlinear waveshaping
    using continuous-time convolution », DAFx-16. Primitive : √(1 + x²).
    v0.22 : la part linéaire du signal (pondérée par la pente de la courbe) reste directe ; seul
    l'écart à la droite est moyenné (sinon l'ADAA, une moyenne de deux échantillons, ternissait
    les aigus quand la saturation est légère). Dans la partie plate, c'est l'ADAA ordinaire. */
class AdaaSoftClip
{
public:
    void reset() noexcept { xPrev = 0.0; fPrev = 1.0; }

    double process (double x) noexcept
    {
        const auto f = std::sqrt (1.0 + x * x);
        const auto dx = x - xPrev;
        const auto y = std::abs (dx) > 1.0e-3 ? (f - fPrev) / dx : softClip (0.5 * (x + xPrev));
        // la part linéaire (pente de la courbe au milieu) reste directe ; dans la partie plate
        // (pente nulle) c'est l'ADAA ordinaire, sans écart à la courbe
        const auto m = 0.5 * (x + xPrev), q = 1.0 / (1.0 + m * m);
        const auto out = y + 0.5 * dx * q * std::sqrt (q); // pente de x/√(1+x²) : (1+x²)^−3/2
        xPrev = x;
        fPrev = f;
        return std::clamp (out, -1.0, 1.0); // un très grand saut ne dépasse pas la courbe
    }

private:
    double xPrev = 0.0, fPrev = 1.0;
};

/** 1.1 : log(cosh u) et tanh u par table (interpolation d'Hermite cubique, pas de 1/128) : mêmes
    valeurs que la bibliothèque mathématique à 2e-11 près, sans les appels coûteux (log1p, exp, tanh)
    qui faisaient la moitié du coût des saturations (filtres, Varnish, échos, compresseurs). h(a) = log1p(exp(-2a)) pour a >= 0,
    h'(a) = tanh(a) - 1, h''(a) = 1 - tanh²(a). */
struct LogCoshTable
{
    static constexpr int perUnit = 128, size = 19 * perUnit + 4;
    static constexpr double step = 1.0 / perUnit;
    struct Node { double h, d1, d2; };                          // nœuds voisins contigus (cache)
    std::array<Node, size> node {};
    LogCoshTable()
    {
        for (int i = 0; i < size; ++i)
        {
            const auto a = i * step, t = std::tanh (a);
            node[(size_t) i] = { std::log1p (std::exp (-2.0 * a)), t - 1.0, 1.0 - t * t };
        }
    }
    static const LogCoshTable& get() { static const LogCoshTable t; return t; }
    /** Hermite cubique sur (f, f') entre deux nœuds. */
    static double herm (double f0, double df0, double f1, double df1, double x) noexcept
    {
        const auto x2 = x * x, x3 = x2 * x;
        return f0 * (2.0 * x3 - 3.0 * x2 + 1.0) + df0 * step * (x3 - 2.0 * x2 + x)
             + f1 * (-2.0 * x3 + 3.0 * x2) + df1 * step * (x3 - x2);
    }
    /** log(cosh u). */
    double logCosh (double u) const noexcept
    {
        const auto a = std::abs (u);
        if (! (a < 19.0)) return a - 0.6931471805599453;
        const auto pos = a * perUnit;
        const auto i = (int) pos;
        const auto& n0 = node[(size_t) i]; const auto& n1 = node[(size_t) i + 1];
        return a + herm (n0.h, n0.d1, n1.h, n1.d1, pos - (double) i) - 0.6931471805599453;
    }
    /** tanh u. */
    double tanh (double u) const noexcept
    {
        const auto a = std::abs (u);
        if (! (a < 19.0)) return u < 0.0 ? -1.0 : 1.0;
        const auto pos = a * perUnit;
        const auto i = (int) pos;
        const auto& n0 = node[(size_t) i]; const auto& n1 = node[(size_t) i + 1];
        const auto t = 1.0 + herm (n0.d1, n0.d2, n1.d1, n1.d2, pos - (double) i);
        return u < 0.0 ? -t : t;
    }
};

/** tanh avec anti-repliement par antidérivée (ADAA, 1er ordre), v0.22.
    Primitive : ln cosh(x) = |x| + ln(1 + e^(−2|x|)) − ln 2 (forme stable). Pour une courbe
    tanh(k·x), passer u = k·x. Même partage linéaire / écart que AdaaSoftClip. */
class AdaaTanh
{
public:
    AdaaTanh() { (void) LogCoshTable::get(); }                 // table prête hors du fil audio
    void reset() noexcept { xPrev = 0.0; fPrev = 0.0; }

    double process (double x) noexcept
    {
        x = std::clamp (x, -40.0, 40.0);
        const auto& tab = LogCoshTable::get();                 // 1.1 : par table, mêmes valeurs à 1e-12 près
        const auto f = tab.logCosh (x);
        const auto dx = x - xPrev;
        const auto t = tab.tanh (0.5 * (x + xPrev));            // 1.1 : une seule tangente
        const auto y = std::abs (dx) > 1.0e-3 ? (f - fPrev) / dx : t;
        const auto out = y + 0.5 * dx * (1.0 - t * t); // part linéaire pondérée par la pente (1 − tanh²)
        xPrev = x;
        fPrev = f;
        return std::clamp (out, -1.0, 1.0); // un très grand saut ne dépasse pas la courbe
    }

private:
    double xPrev = 0.0, fPrev = 0.0;
};

//==============================================================================
/** Générateur de bruit blanc déterministe (xorshift32), sortie dans [-1, 1]. */
class WhiteNoise
{
public:
    explicit WhiteNoise (std::uint32_t seed = 0x9E3779B9u) noexcept : state (seed != 0 ? seed : 1u) {}

    double next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (double) state * (2.0 / 4294967295.0) - 1.0;
    }

private:
    std::uint32_t state;
};

//==============================================================================
/** Filtre à variables d'état en topologie TPT (Zavalishin, « The Art of VA Filter Design »),
    stable en modulation rapide. Sorties passe-bas, passe-bande et passe-haut simultanées. */
class Svf
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0; }

    void set (double cutoffHz, double q, double sampleRate) noexcept
    {
        const auto fc = std::clamp (cutoffHz, 10.0, 0.49 * sampleRate);
        g = std::tan (kPi * fc / sampleRate);
        k = 1.0 / std::max (0.05, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    struct Out { double low, band, high; };

    Out process (double x) noexcept
    {
        const auto v3 = x - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }

private:
    double g = 0.0, k = 1.0, a1 = 0.0, a2 = 0.0, a3 = 0.0, ic1 = 0.0, ic2 = 0.0;
};

//==============================================================================
/** Oscillateur carré à bande limitée par correction PolyBLEP (Välimäki & Huovilainen). */
class BlepSquare
{
public:
    void reset (double startPhase = 0.0) noexcept { phase = startPhase; }

    double next (double freqHz, double sampleRate) noexcept
    {
        const auto dt = std::clamp (freqHz / sampleRate, 0.0, 0.49);
        auto y = phase < 0.5 ? 1.0 : -1.0;
        y += blep (phase, dt);
        y -= blep (std::fmod (phase + 0.5, 1.0), dt);
        phase += dt;
        if (phase >= 1.0) phase -= 1.0;
        return y;
    }

private:
    static double blep (double t, double dt) noexcept
    {
        if (dt <= 0.0) return 0.0;
        if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
        return 0.0;
    }

    double phase = 0.0;
};

/** Filtre anti-continu du premier ordre. */
class DcBlocker
{
public:
    void prepare (double sampleRate, double cutoffHz = 8.0) noexcept { coef = 1.0 - kTwoPi * cutoffHz / sampleRate; }
    void reset() noexcept { x1 = y1 = 0.0; }
    double process (double x) noexcept { const auto y = x - x1 + coef * y1; x1 = x; y1 = y; return y; }

private:
    double coef = 0.999, x1 = 0.0, y1 = 0.0;
};

} // namespace grv::dsp
