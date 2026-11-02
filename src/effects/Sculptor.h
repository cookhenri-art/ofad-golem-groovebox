/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Sculptor » (Sculpteur) — principe des multi-effets à courbes de modulation (ShaperBox, regardé de
    près) : 9 effets en chaîne, chacun piloté par sa propre courbe (14 formes), sa longueur (1/16 à
    4 mesures, calée sur le morceau, ou relancée à chaque attaque du son), sa quantité, et la bande où il
    agit (tout le son, ou seulement le grave, le médium ou l'aigu : séparations Linkwitz-Riley d'ordre 4,
    somme plate) :
        Temps (le son est relu en retard : ralentis, arrêts de bande, scratches calés) → Saturation →
        Broyage (bits et échantillonnage) → Filtre (passe-bas, passe-haut, passe-bande, résonant) →
        Liquide (flanger ou phaser) → Bruit (souffle ajouté au rythme de la courbe) → Volume (pompe,
        hachures) → Panoramique → Largeur.
    Lissage commun des courbes (pas de clic sur les formes à marches). Aucune latence ajoutée (le
    « Temps » retarde volontairement le son), tout réservé dans prepare(), aucun verrou.
    Les courbes dessinées à la main ne sont pas reprises : 14 formes, longueur, quantité.
*/

#pragma once

#include "Mastering.h"   // LR4
#include "ModFx36.h"     // ModLine

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace sculptor
{
    enum Shaper { sTime = 0, sDrive, sCrush, sFilter, sLiquid, sNoise, sVolume, sPan, sWidth, numShapers };
    constexpr int common = 5;
    enum Common { cOn = 0, cShape, cLength, cAmount, cBand };
    enum Global { trigger = numShapers * common, smoothing, xLow, xHigh, mix, output,
                  filterType, filterReso, timeRange, liquidMode, numSlots };
    inline int ix (int s, int c) noexcept { return s * common + c; }

    inline const std::vector<std::string>& shaperNames()
    {
        static const std::vector<std::string> n { "Temps", "Saturation", "Broyage", "Filtre", "Liquide", "Bruit", "Volume", "Panoramique", "Largeur" };
        return n;
    }
    inline const std::vector<std::string>& shapes()
    {
        static const std::vector<std::string> s { "Sinus", "Triangle", "Scie montante", "Scie descendante", "Carr\xC3\xA9", "Pompe", "Pompe douce",
                                                  "Porte 1/16", "Porte point\xC3\xA9" "e", "Escalier", "Rebond", "Hasard", "Mont\xC3\xA9" "e", "Arr\xC3\xAAt de bande" };
        return s;
    }
    inline const std::vector<std::string>& lengths()
    {
        static const std::vector<std::string> l { "1/16", "1/8", "1/4", "1/2", "1 mes.", "2 mes.", "4 mes." };
        return l;
    }
    inline double lengthQuarters (int i) noexcept { static constexpr double q[7] { 0.25, 0.5, 1, 2, 4, 8, 16 }; return q[std::clamp (i, 0, 6)]; }

    /** Valeur de la forme à la position t (0..1) : 0..1 (1 = effet au repos pour Volume / Filtre / Temps). */
    inline double shapeAt (int s, double t, double rnd) noexcept
    {
        t -= std::floor (t);
        switch (s)
        {
            case 0: return 0.5 - 0.5 * std::cos (kTwoPi * t);
            case 1: return 1.0 - std::abs (2.0 * t - 1.0);
            case 2: return t;
            case 3: return 1.0 - t;
            case 4: return t < 0.5 ? 1.0 : 0.0;
            case 5: return std::min (1.0, std::pow (t / 0.6, 1.6));                            // pompe : creux puis remontée
            case 6: return 0.5 - 0.5 * std::cos (kPi * std::min (1.0, t / 0.85));             // pompe douce
            case 7: { const auto u = t * 4.0 - std::floor (t * 4.0); return u < 0.5 ? 1.0 : 0.0; }   // porte : 4 coups par cycle
            case 8: { const auto u = t * 8.0; return (u < 2.0 || (u >= 3.0 && u < 5.0) || (u >= 6.0 && u < 7.0)) ? 1.0 : 0.0; } // 3 + 3 + 2
            case 9: return std::floor (t * 4.0) / 3.0 > 1.0 ? 1.0 : std::floor (t * 4.0) / 3.0;   // escalier
            case 10: return std::exp (-5.0 * (t * 2.0 - std::floor (t * 2.0)));                 // rebond : deux décroissances
            case 11: return rnd;                                                                  // hasard (marches de 1/16 du cycle)
            case 12: return t * t;                                                                // montée
            default: return 1.0 - t * t;                                                          // arrêt de bande
        }
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "sculptor"; m.name = "Sculptor"; m.role = "Multi-effet \xC3\xA0 courbes"; m.family = "Filtres et performance"; m.form = "module";
            m.principle = "9 effets pilot\xC3\xA9s chacun par sa courbe (14 formes, 1/16 \xC3\xA0 4 mesures, au tempo ou \xC3\xA0 chaque attaque), sur tout le son ou une bande";
            static constexpr int defShape[numShapers] { 13, 0, 11, 1, 0, 11, 5, 0, 1 };
            static constexpr int defLength[numShapers] { 4, 2, 0, 3, 4, 1, 2, 3, 4 };
            for (int s = 0; s < numShapers; ++s)
            {
                const auto pre = "s" + std::to_string (s + 1);
                const auto& nm = shaperNames()[(size_t) s];
                m.params.push_back (C (pre + "on", nm, { "Arr\xC3\xAAt", "Marche" }, s == sVolume ? 1 : 0));
                m.params.push_back (C (pre + "shape", nm + " \xC2\xB7 forme", shapes(), defShape[s]));
                m.params.push_back (C (pre + "len", nm + " \xC2\xB7 longueur", lengths(), defLength[s]));
                m.params.push_back (P (pre + "amt", nm + " \xC2\xB7 quantit\xC3\xA9", "%", 0, 100, s == sVolume ? 80 : 60));
                m.params.push_back (C (pre + "band", nm + " \xC2\xB7 bande", { "Tout", "Grave", "M\xC3\xA9" "dium", "Aigu" }));
            }
            m.params.push_back (C ("trigger", "D\xC3\xA9part des courbes", { "Au tempo", "\xC3\x80 chaque attaque" }));
            m.params.push_back (P ("smooth", "Lissage", "ms", 0, 30, 4));
            m.params.push_back (P ("xlow", "S\xC3\xA9paration grave", "Hz", 40, 1000, 200, 0.4f));
            m.params.push_back (P ("xhigh", "S\xC3\xA9paration aigu", "Hz", 1000, 12000, 3000, 0.4f));
            m.params.push_back (P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100));
            m.params.push_back (P ("out", "Sortie", "dB", -24, 12, 0));
            m.params.push_back (C ("ftype", "Type de filtre", { "Passe-bas", "Passe-haut", "Passe-bande" }));
            m.params.push_back (P ("freso", "R\xC3\xA9sonance", "%", 0, 100, 35));
            m.params.push_back (C ("trange", "Port\xC3\xA9" "e du temps", { "1/16", "1/8", "1/4", "1/2" }, 2));
            m.params.push_back (C ("lmode", "Liquide", { "Flanger", "Phaser" }));
            for (int s = 0; s < numShapers; ++s)
            {
                const auto pre = "s" + std::to_string (s + 1);
                m.sections.push_back ({ "shaper" + std::to_string (s + 1), shaperNames()[(size_t) s], "lfo",
                                        { pre + "on", pre + "shape", pre + "len", pre + "amt", pre + "band" }, "shapers" });
            }
            m.sections.push_back ({ "global", "Commun", "lfo", { "trigger", "smooth", "xlow", "xhigh", "mix", "out" }, "shapers" });
            m.sections.push_back ({ "special", "R\xC3\xA9glages des effets", "lfo", { "ftype", "freso", "trange", "lmode" }, "shapers" });
            auto set = [] (std::initializer_list<std::pair<std::string, float>> l) { return std::vector<std::pair<std::string, float>> (l); };
            m.presets = {
                { "Pompe 1/4", {} },
                { "Pompe + filtre qui respire", set ({ { "s4on", 1 }, { "s4shape", 0 }, { "s4len", 4 }, { "s4amt", 55 } }) },
                { "Arr\xC3\xAAt de bande sur la mesure", set ({ { "s7on", 0 }, { "s1on", 1 }, { "s1shape", 13 }, { "s1len", 4 }, { "s1amt", 100 }, { "trange", 3 } }) },
                { "Demi-vitesse (trap)", set ({ { "s7on", 0 }, { "s1on", 1 }, { "s1shape", 2 }, { "s1len", 3 }, { "s1amt", 100 }, { "trange", 3 } }) },
                { "Hachures 1/16", set ({ { "s7shape", 7 }, { "s7len", 2 }, { "s7amt", 100 } }) },
                { "Broyage al\xC3\xA9" "atoire", set ({ { "s7on", 0 }, { "s3on", 1 }, { "s3shape", 11 }, { "s3len", 4 }, { "s3amt", 80 } }) },
                { "Aigus qui tournent", set ({ { "s7on", 0 }, { "s8on", 1 }, { "s8band", 3 }, { "s8shape", 0 }, { "s8len", 3 }, { "s8amt", 90 } }) },
                { "Grave pomp\xC3\xA9 seul", set ({ { "s7band", 1 }, { "s7shape", 5 }, { "s7len", 2 }, { "s7amt", 90 } }) },
                { "Largeur qui s'ouvre", set ({ { "s7on", 0 }, { "s9on", 1 }, { "s9shape", 12 }, { "s9len", 5 }, { "s9amt", 100 } }) },
                { "Liquide lent", set ({ { "s7on", 0 }, { "s5on", 1 }, { "s5shape", 1 }, { "s5len", 5 }, { "s5amt", 70 } }) },
                { "Souffle rythmique", set ({ { "s6on", 1 }, { "s6shape", 8 }, { "s6len", 4 }, { "s6amt", 50 } }) },
                { "Plucks de filtre (attaque)", set ({ { "trigger", 1 }, { "s7on", 0 }, { "s4on", 1 }, { "s4shape", 10 }, { "s4len", 1 }, { "s4amt", 85 }, { "freso", 55 } }) },
            };
            return m;
        }();
        return d;
    }
}

class Sculptor final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return sculptor::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& c : ch) { c.time.init ((int) (4.2 * sr)); for (auto& l : c.liq) l.init ((int) (0.02 * sr)); }
        wDelay = std::clamp ((int) (0.009 * sr), 1, (int) wBuf.size() - 1);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch)
        {
            c.time.clear(); for (auto& l : c.liq) l.clear();
            for (auto& sp : c.split) for (auto& f : sp.f) f.reset();
            for (auto& sp : c.split) for (auto& a : sp.ap) a.reset();
            c.filt.reset(); c.liqFb = 0.0; for (auto& a : c.ph) a = {}; c.held = 0.0; c.holdN = 0;
        }
        wBuf.fill (0.0f); wPos = 0;
        val.fill (1.0); rnd.fill (0.5); lastStep.fill (-1); phase = 0.0; env = envSlow = 0.0; armed = true; timeCur = 0.0;
    }
    float meter (int i) const noexcept override { return i >= 0 && i < sculptor::numShapers ? (float) val[(size_t) i] : 0.0f; }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace sculptor;
        const auto qps = std::max (20.0, bpm) / 60.0; // noires par seconde
        const bool onAttack = p[trigger] > 0.5f;
        // départ des courbes : position du morceau (au tempo), ou chronomètre relancé à chaque attaque
        if (onAttack)
        {
            float pk = 0.0f;
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
            const auto dt = (double) n / sr;
            env = pk > env ? pk : pk + (env - pk) * std::exp (-dt / 0.03);
            envSlow = pk + (envSlow - pk) * std::exp (-dt / 0.15);
            if (env > 2.2 * envSlow + 2.0e-4 && armed) { phase = 0.0; armed = false; }
            if (env < 1.3 * envSlow) armed = true;
        }
        const auto basePos = onAttack || ! hostPlaying ? phase : hostPpq; // en noires
        phase += (double) n / sr * qps;
        const auto smC = std::exp (-(double) n / (std::max (0.1, (double) p[smoothing]) * 0.001 * sr));
        std::array<double, numShapers> amt {};
        std::array<int, numShapers> band {};
        std::array<bool, numShapers> on {};
        for (int s = 0; s < numShapers; ++s)
        {
            on[(size_t) s] = p[ix (s, cOn)] > 0.5f;
            amt[(size_t) s] = p[ix (s, cAmount)] / 100.0;
            band[(size_t) s] = std::clamp ((int) p[ix (s, cBand)], 0, 3);
            const auto len = lengthQuarters ((int) p[ix (s, cLength)]);
            const auto t = basePos / len;
            const auto step = (int) std::floor (t * 16.0);
            if (step != lastStep[(size_t) s]) { lastStep[(size_t) s] = step; seed = seed * 1664525u + 1013904223u; rnd[(size_t) s] = (double) (seed >> 8) / 16777216.0; }
            const auto target = shapeAt ((int) p[ix (s, cShape)], t, rnd[(size_t) s]);
            val[(size_t) s] = target + (val[(size_t) s] - target) * smC;
        }
        // réglages par bloc
        const auto fl = (double) p[xLow], fh = std::max ((double) p[xHigh], fl * 2.0);
        for (auto& c : ch)
            for (auto& sp : c.split)
            {
                sp.f[0].set (SvfEq::lowpass, fl, sr); sp.f[1].set (SvfEq::highpass, fl, sr);
                sp.f[2].set (SvfEq::lowpass, std::min (fh, 0.45 * sr), sr); sp.f[3].set (SvfEq::highpass, std::min (fh, 0.45 * sr), sr);
                sp.ap[0].set (SvfEq::allpass, std::min (fh, 0.45 * sr), 0.70710678, 0.0, sr);
            }
        // filtre : position de la courbe → fréquence (1 = ouvert pour passe-bas, fermé pour passe-haut)
        const auto ft = std::clamp ((int) p[filterType], 0, 2);
        const auto fv = val[sFilter], fa = amt[sFilter];
        const auto pos = ft == 1 ? fa * (1.0 - fv) : ft == 0 ? 1.0 - fa * (1.0 - fv) : fv;
        const auto fc = std::clamp (20.0 * std::pow (1000.0, pos), 25.0, 0.45 * sr);
        const auto q = 0.6 + 9.0 * std::pow (p[filterReso] / 100.0, 2.0);
        static constexpr SvfEq::Type fType[3] { SvfEq::lowpass, SvfEq::highpass, SvfEq::bandpass };
        for (auto& c : ch) c.filt.set (fType[ft], fc, q, 0.0, sr);
        // temps : retard max = portée (1/16 à 1/2 note) ; lissé (le saut d'une forme à marches devient un glissé)
        static constexpr double rangeQ[4] { 0.25, 0.5, 1.0, 2.0 };
        const auto timeTarget = std::min (4.0 * sr, rangeQ[std::clamp ((int) p[timeRange], 0, 3)] / qps * sr) * amt[sTime] * (1.0 - val[sTime]);
        const bool liqPhaser = p[liquidMode] > 0.5f;
        const auto liqV = val[sLiquid];
        const auto liqDelay = (0.4 + 7.0 * (1.0 - liqV) * (1.0 - liqV)) * 0.001 * sr;
        const auto liqTan = std::tan (kPi * std::clamp (150.0 * std::pow (40.0, liqV), 30.0, 0.45 * sr) / sr), liqA = (liqTan - 1.0) / (liqTan + 1.0);
        const auto driveV = amt[sDrive] * val[sDrive];
        const auto driveG = 1.0 + 24.0 * driveV * driveV;
        const auto crushV = amt[sCrush] * val[sCrush];
        const auto steps = std::pow (2.0, 15.0 - 12.0 * crushV);
        const auto holdLen = 1 + (int) (crushV * crushV * 15.0);
        const auto noiseV = 0.18 * amt[sNoise] * val[sNoise];
        const auto volG = 1.0 - amt[sVolume] * (1.0 - val[sVolume]);
        const auto pan = amt[sPan] * (2.0 * val[sPan] - 1.0);
        const auto panL = std::sqrt (0.5 * (1.0 - pan)) * 1.41421356, panR = std::sqrt (0.5 * (1.0 + pan)) * 1.41421356;
        const auto widthG = 1.0 + amt[sWidth] * (2.0 * val[sWidth] - 1.0);
        const auto wet = p[mix] / 100.0, outG = dbToGain (p[output]);
        const auto timeGlide = 1.0 - std::exp (-1.0 / (0.004 * sr));
        // v0.46 : allumer / éteindre un effet passe par un fondu de 8 ms (plus de saut)
        const auto onStep = 1.0 / (0.008 * sr);

        for (int i = 0; i < n; ++i)
        {
            const double dryL = L[i], dryR = R[i];
            double x[2] { dryL, dryR };
            // chaque effet : sur tout le son, ou sur une seule bande (les autres passent sans toucher)
            for (int s = 0; s < numShapers; ++s)
            {
                auto& g = onG[(size_t) s];
                if (g <= 0.0 && on[(size_t) s]) // rallumé : séparations de bandes reparties de zéro
                    for (auto& c : ch) { for (auto& f : c.split[(size_t) s].f) f.reset(); for (auto& a : c.split[(size_t) s].ap) a.reset(); }
                g = on[(size_t) s] ? std::min (1.0, g + onStep) : std::max (0.0, g - onStep);
            }
            auto stage = [&] (int s, auto&& fn)
            {
                const auto g = onG[(size_t) s];
                if (g <= 0.0) return;
                const auto b = band[(size_t) s];
                for (int c = 0; c < 2; ++c)
                {
                    const auto in = x[c];
                    double y;
                    if (b == 0) y = fn (c, in);
                    else
                    {
                        auto& sp = ch[(size_t) c].split[(size_t) s];
                        const auto lo = sp.f[0].process (in), rest = sp.f[1].process (in);
                        const auto mid = sp.f[2].process (rest), hi = sp.f[3].process (rest);
                        double bands3[3] { sp.ap[0].process (lo), mid, hi };
                        bands3[b - 1] = fn (c, bands3[b - 1]);
                        y = bands3[0] + bands3[1] + bands3[2];
                    }
                    x[c] = g >= 1.0 ? y : in + (y - in) * g;
                }
            };
            if (on[sTime]) timeCur += (timeTarget - timeCur) * timeGlide;
            stage (sTime, [&] (int c, double v) { auto& t = ch[(size_t) c].time; t.push (v); return timeCur < 1.0 ? v : t.read (timeCur); });
            if (onG[sTime] <= 0.0) { ch[0].time.push (x[0]); ch[1].time.push (x[1]); }
            stage (sDrive, [&] (int, double v) { return driveV < 0.001 ? v : fastTanh (v * driveG) / std::sqrt (driveG); });
            stage (sCrush, [&] (int c, double v)
            {
                auto& s = ch[(size_t) c];
                if (++s.holdN >= holdLen) { s.holdN = 0; s.held = std::round (v * steps) / steps; }
                return crushV < 0.001 ? v : s.held;
            });
            stage (sFilter, [&] (int c, double v) { return ch[(size_t) c].filt.process (v); });
            stage (sLiquid, [&] (int c, double v)
            {
                auto& s = ch[(size_t) c];
                if (! liqPhaser)
                {
                    s.liq[0].push (v + 0.6 * s.liqFb);
                    const auto y = s.liq[0].read (liqDelay * (c == 1 ? 1.07 : 1.0));
                    s.liqFb = fastTanh (y);
                    return v * (1.0 - 0.5 * amt[sLiquid]) + y * 0.5 * amt[sLiquid];
                }
                auto y = v + 0.5 * s.liqFb;
                for (auto& a : s.ph) { const auto o = liqA * y + a.x1 - liqA * a.y1; a.x1 = y; a.y1 = o; y = o; }
                s.liqFb = fastTanh (y);
                return v * (1.0 - 0.5 * amt[sLiquid]) + y * 0.5 * amt[sLiquid];
            });
            stage (sNoise, [&] (int, double v) { seed = seed * 1664525u + 1013904223u; return v + noiseV * ((double) (seed >> 9) / 4194304.0 - 1.0); });
            stage (sVolume, [&] (int, double v) { return v * volG; });
            stage (sPan, [&] (int c, double v) { return v * (c == 0 ? panL : panR); });
            {
                // v0.45 : un son mono n'a pas de côtés à élargir : au-delà de 1, un côté est fabriqué
                // avec le milieu retardé de 9 ms (effet de précédence) ; en mono, il s'annule.
                // v0.46 : la ligne est toujours alimentée, l'effet entre et sort en fondu
                const auto m = 0.5 * (x[0] + x[1]);
                wBuf[(size_t) wPos] = (float) m;
                auto rd = wPos - wDelay; if (rd < 0) rd += (int) wBuf.size();
                if (++wPos >= (int) wBuf.size()) wPos = 0;
                if (const auto g = onG[sWidth]; g > 0.0)
                {
                    const auto sd = 0.5 * (x[0] - x[1]) * widthG + std::max (0.0, widthG - 1.0) * 0.75 * (double) wBuf[(size_t) rd];
                    const auto y0 = m + sd, y1 = m - sd;
                    x[0] += (y0 - x[0]) * g; x[1] += (y1 - x[1]) * g;
                }
            }
            L[i] = (float) ((dryL * (1.0 - wet) + x[0] * wet) * outG);
            R[i] = (float) ((dryR * (1.0 - wet) + x[1] * wet) * outG);
        }
    }

private:
    std::array<float, 8192> wBuf {};
    int wPos = 0, wDelay = 432;
    struct Split { std::array<LR4, 4> f; std::array<SvfEq, 1> ap; };
    struct Ap { double x1 = 0.0, y1 = 0.0; };
    struct Chan
    {
        ModLine time; std::array<ModLine, 1> liq; std::array<Split, sculptor::numShapers> split;
        SvfEq filt; double liqFb = 0.0, held = 0.0; int holdN = 0; std::array<Ap, 8> ph {};
    };
    std::array<Chan, 2> ch;
    std::array<double, sculptor::numShapers> val {}, rnd {}, onG {}; // onG : fondu marche / arrêt (v0.46)
    std::array<int, sculptor::numShapers> lastStep {};
    double sr = 48000.0, hostPpq = 0.0, phase = 0.0, env = 0.0, envSlow = 0.0, timeCur = 0.0;
    bool hostPlaying = false, armed = true;
    std::uint32_t seed = 0x1234567u;
};

} // namespace grv::effects
