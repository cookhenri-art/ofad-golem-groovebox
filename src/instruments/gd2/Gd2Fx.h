/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Good Doctor 2 — rack d'effets interne (après toutes les voix) :
      Hyper/Dimension, Distorsion, Flanger, Phaser, Chorus, Délai, Compresseur (simple ou « OTT » :
      3 bandes, compression vers le bas ET vers le haut), Filtre, Égaliseur, Réverbération.
    Chaque module a sa marche et son rang dans la chaîne (1 à 10, ex aequo : ordre de la liste).

    Choix de modèle : les effets de modulation, le délai, la réverbération et le filtre sont les
    effets éprouvés du Groovebox (mêmes classes que dans les inserts), pilotés par un tableau de
    réglages tenu ici ; la distorsion (10 courbes, préfiltre, suréchantillonnage ×2), le
    compresseur trois bandes et l'égaliseur sont propres à l'instrument. Rien n'est alloué dans le
    rendu : tous les effets sont créés et préparés avant.
*/

#pragma once

#include "../../effects/ModFx.h"
#include "../../effects/ModFx36.h"
#include "../../effects/Fx38.h"
#include "../../effects/Delay.h"
#include "../../effects/Reverb.h"
#include "../../dsp/EqFilters.h"
#include "../../dsp/Oversampler.h"

#include <array>
#include <memory>
#include <vector>

namespace grv::instruments::gd2
{

/** Un effet du Groovebox piloté par ses identifiants de réglages. */
class MappedFx
{
public:
    explicit MappedFx (std::unique_ptr<grv::engine::Effect> fx) : e (std::move (fx))
    {
        const auto& d = e->desc();
        p.resize (d.params.size());
        for (size_t i = 0; i < d.params.size(); ++i) p[i] = d.params[i].def;
    }
    int index (const char* id) const { return e->desc().indexOf (id); }
    void set (int i, float v) noexcept { if (i >= 0 && i < (int) p.size()) p[(size_t) i] = v; }
    void prepare (double sr, int block) { e->prepare (sr, block); }
    void reset() noexcept { e->reset(); }
    void process (float* L, float* R, int n, double bpm) noexcept { e->process (L, R, n, p.data(), bpm); }

private:
    std::unique_ptr<grv::engine::Effect> e;
    std::vector<float> p;
};

//==============================================================================
/** Distorsion : préfiltre, 10 courbes, suréchantillonnage ×2, mélange. */
class Gd2Dist
{
public:
    enum Pre { preOff = 0, preLow, preBand, preHigh };
    void prepare (double s) noexcept { sr = s; reset(); }
    void reset() noexcept { for (auto& o : os) o.reset(); for (auto& f : pf) f.reset(); held.fill (0.0); count = 0.0; primed = false; }

    void process (float* L, float* R, int n, int type, double driveDb, int pre, double preHz, double mix) noexcept
    {
        // après la 1.1 : poussée et mélange lissés à chaque échantillon (~10 ms) — tourner le bouton ne craque plus
        // (avant : un saut de gain tous les 32 échantillons)
        if (! primed) { sDrive = driveDb; sMix = mix; primed = true; }
        const auto k = smoothK();
        if (pre != preOff)
            for (auto& f : pf) f.set (pre == preLow ? SvfEq::lowpass : pre == preBand ? SvfEq::bandpass : SvfEq::highpass, preHz, 0.8, 0.0, sr);
        for (int i = 0; i < n; ++i)
        {
            sDrive += (driveDb - sDrive) * k;
            sMix += (mix - sMix) * k;
            const auto g = dbToGain (sDrive), comp = dbToGain (-0.42 * sDrive); // = 1 / g^0,42
            if (type == 8) { count += 1.0; if (count >= std::max (1.0, std::sqrt (g) * 1.5)) count = 0.0; }
            float* io[2] { L + i, R + i };
            for (int c = 0; c < 2; ++c)
            {
                const double dry = *io[c];
                double x = pre != preOff ? pf[(size_t) c].process (dry) : dry;
                double a, b;
                os[(size_t) c].up (x * g, a, b);
                a = shape (a, type, g, c); b = shape (b, type, g, c);
                auto y = os[(size_t) c].down (a, b) * comp;
                if (pre != preOff) y += dry - x; // la part hors du préfiltre passe telle quelle
                *io[c] = (float) (dry + (y - dry) * sMix);
            }
        }
    }

    static const std::vector<std::string>& names()
    {
        static const std::vector<std::string> n { "Lampe", "Douce", "Dure", "Diode", "Repli lin\xC3\xA9" "aire", "Repli sinus",
                                                  "Z\xC3\xA9ro-carr\xC3\xA9", "Asym\xC3\xA9trique", "\xC3\x89" "chantillonnage", "Bits" };
        return n;
    }

private:
    double shape (double x, int type, double g, int c) noexcept
    {
        switch (type)
        {
            case 0: return x >= 0.0 ? std::tanh (x) : std::tanh (0.65 * x) / 0.65 * 0.85;
            case 1: return x / std::sqrt (1.0 + x * x);
            case 2: return std::clamp (x, -1.0, 1.0);
            case 3: return (x >= 0.0 ? 1.0 - std::exp (-x) : -(1.0 - std::exp (0.6 * x)) / 0.6 * 0.7);
            case 4: { auto f = (x * 0.25 + 0.25); f -= std::floor (f); return std::abs (f * 4.0 - 2.0) - 1.0; }
            case 5: return std::sin (x * 0.5 * kPi);
            case 6: { const auto y = x * std::abs (x); return std::clamp (y, -1.0, 1.0); }
            case 7: return std::tanh (x + 0.45) - std::tanh (0.45);
            case 8:
            {
                // échantillonnage : la valeur est tenue, de plus en plus longtemps avec la poussée (après la 1.1 :
                // les deux côtés sont tenus aux mêmes instants ; le compteur avance une fois par échantillon)
                if (count == 0.0) held[(size_t) c] = std::tanh (x / std::sqrt (g) * 1.5);
                return held[(size_t) c];
            }
            default:
            {
                const auto q = std::max (2.0, 128.0 / g);
                return std::round (std::tanh (x / std::sqrt (g)) * q) / q * std::sqrt (g);
            }
        }
    }

    double smoothK() const noexcept { return 1.0 - std::exp (-1.0 / (0.01 * sr)); }
    double sr = 48000.0, count = 0.0, sDrive = 0.0, sMix = 1.0;
    bool primed = false;
    std::array<Oversampler2x, 2> os;
    std::array<SvfEq, 2> pf;
    std::array<double, 2> held {};
};

//==============================================================================
/** Compresseur : simple (une bande) ou trois bandes vers le bas et vers le haut (« OTT »). */
class Gd2Comp
{
public:
    void prepare (double s) noexcept
    {
        sr = s;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k)
            {
                lp[(size_t) c][(size_t) k].set (SvfEq::lowpass, 88.3, 0.7071, 0.0, sr);
                hp[(size_t) c][(size_t) k].set (SvfEq::highpass, 88.3, 0.7071, 0.0, sr);
                lp2[(size_t) c][(size_t) k].set (SvfEq::lowpass, 2500.0, 0.7071, 0.0, sr);
                hp2[(size_t) c][(size_t) k].set (SvfEq::highpass, 2500.0, 0.7071, 0.0, sr);
            }
        for (auto& a : ap) a.set (SvfEq::allpass, 2500.0, 0.7071, 0.0, sr);
        reset();
    }
    void reset() noexcept
    {
        for (auto& c : lp) for (auto& f : c) f.reset();
        for (auto& c : hp) for (auto& f : c) f.reset();
        for (auto& c : lp2) for (auto& f : c) f.reset();
        for (auto& c : hp2) for (auto& f : c) f.reset();
        for (auto& a : ap) a.reset();
        env.fill (-30.0); gain.fill (0.0); // départ neutre : pas de remontée de +18 dB sur la première note
        primed = false;
    }

    /** depth 0..1 (part du signal traité), time 0.1..10 (multiplie attaques et relâchements),
        up, down 0..1, inDb, outDb, bandDb[3] (sortie de chaque bande), ott : 3 bandes. */
    void process (float* L, float* R, int n, bool ott, double depth, double time, double up, double down,
                  double inDb, double outDb, const double* bandDb, double thrDb, double ratio) noexcept
    {
        // après la 1.1 : entrée, sortie, profondeur, haut / bas, bandes, seuil et rapport lissés à chaque échantillon
        // (~10 ms) — tourner un bouton ne craque plus (avant : un saut tous les 32 échantillons)
        const double target[10] { inDb, outDb, depth, up, down, bandDb[0], bandDb[1], bandDb[2], thrDb, std::max (1.0, ratio) };
        if (! primed) { for (int q = 0; q < 10; ++q) sm[(size_t) q] = target[q]; primed = true; }
        const auto k = 1.0 - std::exp (-1.0 / (0.01 * sr));
        auto step = [&]
        {
            for (int q = 0; q < 10; ++q) sm[(size_t) q] += (target[q] - sm[(size_t) q]) * k;
        };
        if (! ott)
        {
            const auto at = coef (10.0 * time), rl = coef (120.0 * time);
            for (int i = 0; i < n; ++i)
            {
                step();
                const auto gin = dbToGain (sm[0]), gout = dbToGain (sm[1]);
                const auto l = L[i] * gin, r = R[i] * gin;
                const auto lev = gainToDb (std::max (std::abs (l), std::abs (r)));
                env[0] = lev > env[0] ? lev + (env[0] - lev) * at : lev + (env[0] - lev) * rl;
                const auto over = env[0] - sm[8];
                const auto gr = over > 0.0 ? -over * (1.0 - 1.0 / sm[9]) : 0.0;
                const auto g = dbToGain (gr * sm[2]) * gout;
                L[i] = (float) (l * g); R[i] = (float) (r * g);
            }
            return;
        }
        // seuils et temps de la célèbre réglure à trois bandes : grave, médium, aigu
        static constexpr double thDown[3] { -33.8, -30.2, -35.5 }, thUp[3] { -40.8, -41.8, -40.8 };
        static constexpr double attMs[3] { 47.8, 22.4, 13.5 }, relMs[3] { 282.0, 282.0, 132.0 };
        static constexpr double makeup[3] { 5.0, 3.0, 5.0 };
        std::array<double, 3> at {}, rl {};
        for (int b = 0; b < 3; ++b) { at[(size_t) b] = coef (attMs[b] * time); rl[(size_t) b] = coef (relMs[b] * time); }
        for (int i = 0; i < n; ++i)
        {
            step();
            const auto gin = dbToGain (sm[0]), gout = dbToGain (sm[1]);
            const auto depthS = sm[2], upS = sm[3], downS = sm[4];
            std::array<std::array<double, 3>, 2> band {};
            for (int c = 0; c < 2; ++c)
            {
                const auto x = (c == 0 ? L[i] : R[i]) * gin;
                // séparation sans latence : Linkwitz-Riley 4e ordre à 88 Hz et 2,5 kHz, grave recalé en phase
                const auto lo = lp[(size_t) c][1].process (lp[(size_t) c][0].process (x));
                const auto rest = hp[(size_t) c][1].process (hp[(size_t) c][0].process (x));
                band[(size_t) c][0] = ap[(size_t) c].process (lo);
                band[(size_t) c][1] = lp2[(size_t) c][1].process (lp2[(size_t) c][0].process (rest));
                band[(size_t) c][2] = hp2[(size_t) c][1].process (hp2[(size_t) c][0].process (rest));
            }
            double ol = 0.0, orr = 0.0;
            for (int b = 0; b < 3; ++b)
            {
                const auto lev = gainToDb (std::max (std::abs (band[0][(size_t) b]), std::abs (band[1][(size_t) b])));
                auto& e = env[(size_t) b];
                e = lev > e ? lev + (e - lev) * at[(size_t) b] : lev + (e - lev) * rl[(size_t) b];
                double gDb = 0.0;
                if (e > thDown[b]) gDb -= (e - thDown[b]) * (1.0 - 1.0 / 66.7) * downS;               // vers le bas
                else if (e < thUp[b] && e > -62.0) gDb += std::min (18.0, (thUp[b] - e) * (1.0 - 1.0 / 4.17)) * upS; // vers le haut (pas le bruit de fond)
                const auto g = dbToGain (gDb + makeup[b] + sm[5 + (size_t) b]);
                ol += band[0][(size_t) b] * g;
                orr += band[1][(size_t) b] * g;
            }
            const auto dl = L[i] * gin, dr = R[i] * gin;
            // garde-fou : une remontée encore en cours quand une note attaque ne dépasse pas ~ +3 dBFS
            auto safe = [] (double x) { return x / std::sqrt (1.0 + x * x * 0.35); };
            L[i] = (float) safe ((dl + (ol - dl) * depthS) * gout);
            R[i] = (float) safe ((dr + (orr - dr) * depthS) * gout);
        }
    }

private:
    double coef (double ms) const noexcept { return std::exp (-1.0 / std::max (1.0, ms * 0.001 * sr)); }
    double sr = 48000.0;
    std::array<std::array<SvfEq, 2>, 2> lp, hp, lp2, hp2;
    std::array<SvfEq, 2> ap;
    std::array<double, 3> env {}, gain {};
    std::array<double, 10> sm {};
    bool primed = false;
};

//==============================================================================
/** Égaliseur : plateau grave, cloche, plateau aigu. */
class Gd2Eq
{
public:
    void prepare (double s) noexcept { sr = s; reset(); }
    void reset() noexcept { lo.reset(); mid.reset(); hi.reset(); }
    void process (float* L, float* R, int n, double lf, double lg, double mf, double mg, double mq, double hf, double hg) noexcept
    {
        lo.set (SvfEq::lowShelf, lf, 0.7, lg, sr);
        mid.set (SvfEq::bell, mf, mq, mg, sr);
        hi.set (SvfEq::highShelf, hf, 0.7, hg, sr);
        for (int i = 0; i < n; ++i)
        {
            L[i] = (float) hi.l.process (mid.l.process (lo.l.process (L[i])));
            R[i] = (float) hi.r.process (mid.r.process (lo.r.process (R[i])));
        }
    }
private:
    double sr = 48000.0;
    SvfEqStereo lo, mid, hi;
};

} // namespace grv::instruments::gd2
