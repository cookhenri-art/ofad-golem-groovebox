/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Suréchantillonnage ×2 à filtres demi-bande polyphasés à passe-tout (IIR) : pas de latence
    déclarée (déphasage faible), coût réduit. Les coefficients sont calculés selon la méthode
    classique de conception elliptique des demi-bandes polyphasées (Valenzuela & Constantinides ;
    implémentation de référence publique de L. de Soras, licence WTFPL) — réécrite ici.
*/

#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace grv::dsp
{

/** Calcule numCoefs coefficients pour une bande de transition donnée (fraction de fs, ex. 0.04). */
inline std::vector<double> halfbandCoefs (int numCoefs, double transition)
{
    constexpr double pi = 3.14159265358979323846;
    auto k = std::tan ((1.0 - transition * 2.0) * pi / 4.0);
    k *= k;
    const auto kksqrt = std::pow (1.0 - k * k, 0.25);
    const auto e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt);
    const auto e4 = e * e * e * e;
    const auto q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
    const auto order = numCoefs * 2 + 1;

    auto accNum = [&] (int c)
    {
        double acc = 0.0, term;
        int i = 0, j = 1;
        do
        {
            term = std::pow (q, (double) (i * (i + 1))) * std::sin ((double) (i * 2 + 1) * c * pi / order) * j;
            acc += term;
            j = -j; ++i;
        } while (std::abs (term) > 1.0e-100 && i < 200);
        return acc;
    };
    auto accDen = [&] (int c)
    {
        double acc = 0.0, term;
        int i = 1, j = -1;
        do
        {
            term = std::pow (q, (double) (i * i)) * std::cos ((double) (i * 2) * c * pi / order) * j;
            acc += term;
            j = -j; ++i;
        } while (std::abs (term) > 1.0e-100 && i < 200);
        return acc;
    };

    std::vector<double> coefs ((size_t) numCoefs);
    for (int index = 0; index < numCoefs; ++index)
    {
        const auto c = index + 1;
        const auto num = accNum (c) * std::pow (q, 0.25);
        const auto den = accDen (c) + 0.5;
        const auto ww = num / den;
        const auto wwsq = ww * ww;
        const auto x = std::sqrt ((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
        coefs[(size_t) index] = (1.0 - x) / (1.0 + x);
    }
    return coefs;
}

/** Suréchantillonneur ×2 mono : up() produit 2 échantillons, down() en consomme 2. */
class Oversampler2x
{
public:
    static constexpr int numCoefs = 8; // ~ -70 dB d'atténuation hors bande, transition 4 % de fs

    Oversampler2x()
    {
        static const std::vector<double> c = halfbandCoefs (numCoefs, 0.04);
        for (int i = 0; i < numCoefs; ++i) coef[(size_t) i] = c[(size_t) i];
        reset();
    }

    void reset() noexcept { upX.fill (0.0); upY.fill (0.0); dnX.fill (0.0); dnY.fill (0.0); }

    void up (double in, double& out0, double& out1) noexcept
    {
        // chemin pair : coefficients 0, 2, 4… ; chemin impair : 1, 3, 5…
        double a = in, b = in;
        for (int i = 0; i < numCoefs; i += 2) a = stage (a, coef[(size_t) i], upX[(size_t) i], upY[(size_t) i]);
        for (int i = 1; i < numCoefs; i += 2) b = stage (b, coef[(size_t) i], upX[(size_t) i], upY[(size_t) i]);
        out0 = a;
        out1 = b;
    }

    double down (double in0, double in1) noexcept
    {
        double a = in1, b = in0;
        for (int i = 0; i < numCoefs; i += 2) a = stage (a, coef[(size_t) i], dnX[(size_t) i], dnY[(size_t) i]);
        for (int i = 1; i < numCoefs; i += 2) b = stage (b, coef[(size_t) i], dnX[(size_t) i], dnY[(size_t) i]);
        return 0.5 * (a + b);
    }

private:
    /** Passe-tout du premier ordre en z^-2 vu au rythme polyphasé : y = c (x − y1) + x1. */
    static double stage (double x, double c, double& x1, double& y1) noexcept
    {
        const auto y = c * (x - y1) + x1;
        x1 = x;
        y1 = y;
        return y;
    }

    std::array<double, numCoefs> coef {}, upX {}, upY {}, dnX {}, dnY {};
};

/** Suréchantillonneur ×4 (v0.22) : deux étages ×2 en cascade, toujours sans latence déclarée.
    up() produit 4 échantillons, down() en consomme 4. */
class Oversampler4x
{
public:
    void reset() noexcept { outer.reset(); inner.reset(); }

    void up (double in, double* out4) noexcept
    {
        double a, b;
        outer.up (in, a, b);
        inner.up (a, out4[0], out4[1]);
        inner.up (b, out4[2], out4[3]);
    }

    double down (const double* in4) noexcept
    {
        const auto a = inner.down (in4[0], in4[1]);
        const auto b = inner.down (in4[2], in4[3]);
        return outer.down (a, b);
    }

private:
    Oversampler2x outer, inner;
};

} // namespace grv::dsp
