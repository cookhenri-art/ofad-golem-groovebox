/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Grinder » (Broyeur) — principe des distorsions multibandes de studio (Ohmicide, regardé de près) :
    le son est séparé en 2 à 4 bandes (Linkwitz-Riley d'ordre 4, phases recalées par passe-tout : la
    somme est plate) et CHAQUE bande a sa propre chaîne :
        porte de bruit → dynamique (compression ou expansion « montante », un seul bouton) →
        générateur de réinjection (la sortie de la bande revient à son entrée par une ligne accordée :
        la bande se met à « larsen » à la fréquence choisie, écarté en stéréo) → distorsion (16
        algorithmes dont « aucune » et « impaire »), drive, asymétrie → niveau et panoramique.
    Plus : gain d'entrée, sortie, mélange, et 3 modulateurs internes (LFO, suiveur, fonction,
    séquenceur ; FxMod.h) sur n'importe quel réglage continu d'une bande.

    Aucune latence (courbes sans suréchantillonnage à phase linéaire : ×2 IIR par bande), tout réservé
    dans prepare(), aucun verrou. Architecture propre : aucune reprise de code ni de réglages d'usine.
*/

#pragma once

#include "Distortion.h"
#include "Mastering.h"   // LR4, fdb, fgain
#include "FxMod.h"
#include "FxBanks.h"

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace grinder
{
    constexpr int numBands = 4;
    constexpr int bandParams = 9;
    enum Global { bands = 0, x1, x2, x3, input, output, mix, bandBase };
    enum BandSlot { bGate = 0, bDyn, bType, bDrive, bBias, bFb, bFbFreq, bLevel, bPan };
    constexpr int modBase = bandBase + numBands * bandParams;
    constexpr int numMods = 3;
    constexpr int numSlots = modBase + numMods * fxmod::slotParams;
    inline int bandIx (int b, int s) noexcept { return bandBase + b * bandParams + s; }

    /** 16 algorithmes : « Aucune » (dynamique et réinjection seules), les 14 de Furnace, « Impaire ». */
    inline const std::vector<std::string>& algorithms()
    {
        static const std::vector<std::string> a = []
        {
            std::vector<std::string> v { "Aucune" };
            for (const auto& s : dist::algorithms()) v.push_back (s);
            v.push_back ("Impaire");
            return v;
        }();
        return a;
    }
    inline double curve (int type, double x, double drive01, double bias) noexcept
    {
        if (type <= 0) return x;
        if (type == 15)
        {
            // impaire : harmoniques impaires seulement (polynôme de Tchebychev d'ordre 3 sur une saturation douce)
            const auto g = 1.0 + 31.0 * drive01 * drive01;
            const auto t = fastTanh (x * g + bias) - fastTanh (bias);
            return 0.55 * t + 0.45 * (4.0 * t * t * t - 3.0 * t);
        }
        return dist::shape (type - 1, x, drive01, bias);
    }

    inline const std::vector<std::string>& bandNames() { static const std::vector<std::string> n { "Grave", "Bas-m\xC3\xA9" "dium", "Haut-m\xC3\xA9" "dium", "Aigu" }; return n; }

    inline const std::vector<std::string>& destNames()
    {
        static const std::vector<std::string> d = []
        {
            std::vector<std::string> v;
            for (const char* b : { "B1", "B2", "B3", "B4" })
                for (const char* k : { "drive", "asym.", "r\xC3\xA9inj.", "accord", "niveau", "dynam." }) v.push_back (std::string (b) + " " + k);
            v.push_back ("Entr\xC3\xA9" "e"); v.push_back ("M\xC3\xA9lange"); v.push_back ("S\xC3\xA9p. 1"); v.push_back ("S\xC3\xA9p. 2"); v.push_back ("S\xC3\xA9p. 3");
            return v;
        }();
        return d;
    }
    inline const int* destSlots()
    {
        static const std::vector<int> d = []
        {
            std::vector<int> v;
            for (int b = 0; b < numBands; ++b)
                for (int s : { (int) bDrive, (int) bBias, (int) bFb, (int) bFbFreq, (int) bLevel, (int) bDyn }) v.push_back (bandIx (b, s));
            for (int s : { (int) input, (int) mix, (int) x1, (int) x2, (int) x3 }) v.push_back (s);
            return v;
        }();
        return d.data();
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "grinder"; m.name = "Grinder"; m.role = "Distorsion multibande";
            m.family = "Saturation"; m.form = "module";
            m.principle = "Distorsion multibande : 2 \xC3\xA0 4 bandes, chacune avec porte, dynamique, r\xC3\xA9injection accord\xC3\xA9" "e, 16 algorithmes, asym\xC3\xA9trie ; 3 modulateurs";
            m.params = {
                C ("bands", "Bandes", { "2", "3", "4" }, 2),
                P ("x1", "S\xC3\xA9paration 1", "Hz", 40, 1000, 180, 0.4f),
                P ("x2", "S\xC3\xA9paration 2", "Hz", 200, 5000, 900, 0.4f),
                P ("x3", "S\xC3\xA9paration 3", "Hz", 1000, 16000, 4500, 0.4f),
                P ("input", "Entr\xC3\xA9" "e", "dB", -24, 24, 0),
                P ("output", "Sortie", "dB", -24, 12, 0),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
            };
            static constexpr int defType[numBands] { 1, 2, 3, 8 };
            static constexpr float defDrive[numBands] { 25, 45, 40, 30 };
            for (int b = 0; b < numBands; ++b)
            {
                const auto pre = "b" + std::to_string (b + 1);
                const auto& nm = bandNames()[(size_t) b];
                m.params.push_back (P (pre + "gate", nm + " \xC2\xB7 porte", "dB", -80, 0, -80));
                m.params.push_back (P (pre + "dyn", nm + " \xC2\xB7 dynamique", "%", -100, 100, 0));
                m.params.push_back (C (pre + "type", nm + " \xC2\xB7 algorithme", algorithms(), defType[b]));
                m.params.push_back (P (pre + "drive", nm + " \xC2\xB7 drive", "%", 0, 100, defDrive[b]));
                m.params.push_back (P (pre + "bias", nm + " \xC2\xB7 asym\xC3\xA9trie", "%", -100, 100, 0));
                m.params.push_back (P (pre + "fb", nm + " \xC2\xB7 r\xC3\xA9injection", "%", 0, 100, 0));
                m.params.push_back (P (pre + "fbfreq", nm + " \xC2\xB7 accord", "Hz", 30, 4000, 110.0f * (float) (1 << (b * 2)), 0.3f));
                m.params.push_back (P (pre + "level", nm + " \xC2\xB7 niveau", "dB", -48, 12, 0));
                m.params.push_back (P (pre + "pan", nm + " \xC2\xB7 panoramique", "pan", -100, 100, 0));
                m.sections.push_back ({ "band" + std::to_string (b + 1), nm, "dist",
                                        { pre + "type", pre + "drive", pre + "bias", pre + "gate", pre + "dyn", pre + "fb", pre + "fbfreq", pre + "level" }, "bands" });
            }
            m.sections.insert (m.sections.begin(), { "global", "Bandes et sortie", "bands", { "bands", "x1", "x2", "x3", "input", "output", "mix" }, "bands" });
            fxmod::addSlots (m, numMods, destNames(), 0, 6); // par défaut : drive des bandes 1, 2, 3
            auto set = [] (std::initializer_list<std::pair<std::string, float>> l) { return std::vector<std::pair<std::string, float>> (l); };
            m.presets = {
                { "Quatre couleurs", {} },
                { "Basse qui mord", set ({ { "bands", 1 }, { "x1", 120 }, { "x2", 1200 }, { "b1type", 0 }, { "b1dyn", -40 }, { "b2type", 5 }, { "b2drive", 70 },
                                          { "b3type", 3 }, { "b3drive", 55 }, { "b3level", -3 } }) },
                { "Batterie broy\xC3\xA9" "e", set ({ { "b1type", 7 }, { "b1drive", 40 }, { "b2type", 3 }, { "b2drive", 60 }, { "b2dyn", -50 },
                                                     { "b3type", 8 }, { "b3drive", 55 }, { "b4type", 9 }, { "b4drive", 50 }, { "b4gate", -40 }, { "output", -3 } }) },
                { "Larsen accord\xC3\xA9", set ({ { "b2fb", 70 }, { "b2fbfreq", 440 }, { "b2type", 2 }, { "b2drive", 50 }, { "b3fb", 50 }, { "b3fbfreq", 1320 } }) },
                { "Aigus seulement", set ({ { "b1type", 0 }, { "b1drive", 0 }, { "b2type", 0 }, { "b2drive", 0 }, { "b3type", 12 }, { "b3drive", 50 }, { "b4type", 16 }, { "b4drive", 60 } }) },
                { "Voix radio sale", set ({ { "bands", 1 }, { "b1level", -24 }, { "b2type", 3 }, { "b2drive", 65 }, { "b2gate", -45 }, { "b3level", -12 }, { "mix", 85 } }) },
                { "Drive au suiveur", set ({ { "b2drive", 30 }, { "b3drive", 30 }, { "m1type", 2 }, { "m1rate", 4 }, { "m1amt", 60 }, { "m1dest", 6 },
                                            { "m2type", 2 }, { "m2rate", 4 }, { "m2amt", 60 }, { "m2dest", 12 } }) },
                { "Bandes qui dansent", set ({ { "m1type", 1 }, { "m1div", 7 }, { "m1shape", 1 }, { "m1amt", 50 }, { "m1dest", 6 },
                                              { "m2type", 4 }, { "m2div", 9 }, { "m2shape", 5 }, { "m2amt", 45 }, { "m2dest", 22 },
                                              { "m3type", 1 }, { "m3div", 5 }, { "m3shape", 0 }, { "m3amt", 30 }, { "m3dest", 26 } }) },
                { "Impaire et sombre", set ({ { "b2type", 15 }, { "b2drive", 55 }, { "b3type", 15 }, { "b3drive", 45 }, { "b4level", -10 } }) },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Grinder final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return grinder::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& c : ch) for (auto& b : c.fb) b.init ((int) (sr / 25.0) + 8);
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : ch)
        {
            for (auto& f : c.xo) f.reset();
            for (auto& f : c.ap) f.reset();
            for (auto& b : c.fb) b.clear();
            for (auto& o : c.os) o.reset();
            for (auto& v : c.fbY) v = 0.0;
            for (auto& d : c.dc) d = {};
            c.dryOs.reset();
        }
        gateEnv.fill (0.0); gateGain.fill (0.0); dynEnv.fill (0.0); levelPrev.fill (-1.0);
        mods.reset();
        for (auto& m : bandMeter) m = 0.0f;
    }
    float meter (int i) const noexcept override
    {
        if (i >= 0 && i < grinder::numBands) return bandMeter[(size_t) i];         // niveau de chaque bande (écran)
        if (i >= 4 && i < 4 + grinder::numMods) return (float) mods.value (i - 4);  // modulateurs
        return 0.0f;
    }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* pIn, double bpm) noexcept override
    {
        using namespace grinder;
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        float pm[numSlots];
        mods.apply (desc(), pIn, pm, numSlots, modBase, destSlots(), (int) destNames().size(), n, sr, bpm, hostPpq, hostPlaying, pk);
        const float* p = pm;

        const auto nb = 2 + std::clamp ((int) p[bands], 0, 2);
        // séparations ordonnées (au moins une demi-octave d'écart)
        const auto f1 = (double) p[x1];
        const auto f2 = std::max ((double) p[x2], f1 * 1.5);
        const auto f3 = std::min (std::max ((double) p[x3], f2 * 1.5), 0.45 * sr);
        // 2 bandes : f1 seule ; 3 bandes : f1, f2 ; 4 : f1, f2, f3
        const double fx[3] { f1, f2, f3 };
        for (auto& c : ch)
        {
            for (int k = 0; k < 3; ++k) c.xo[(size_t) k].set (fx[k], sr); // 1.1 : séparations à étage partagé (identique)
            c.ap[0].set (SvfEq::allpass, f2, 0.70710678, 0.0, sr);
            c.ap[1].set (SvfEq::allpass, f3, 0.70710678, 0.0, sr);
            c.ap[2].set (SvfEq::allpass, f3, 0.70710678, 0.0, sr);
        }
        struct Band { int type; double drive, bias, gateThr, dyn, fb, fbDelay[2], level, panL, panR; };
        std::array<Band, numBands> bd {};
        for (int b = 0; b < numBands; ++b)
        {
            auto& x = bd[(size_t) b];
            x.type = std::clamp ((int) p[bandIx (b, bType)], 0, 15);
            x.drive = p[bandIx (b, bDrive)] / 100.0;
            x.bias = p[bandIx (b, bBias)] / 100.0 * 0.6;
            x.gateThr = p[bandIx (b, bGate)];
            x.dyn = p[bandIx (b, bDyn)] / 100.0;
            x.fb = 0.97 * p[bandIx (b, bFb)] / 100.0;
            const auto hz = std::clamp ((double) p[bandIx (b, bFbFreq)], 30.0, 4000.0);
            x.fbDelay[0] = std::clamp (sr / hz, 2.0, sr / 25.0);
            x.fbDelay[1] = std::clamp (sr / (hz * 1.012), 2.0, sr / 25.0); // écart stéréo du larsen
            x.level = dbToGain (p[bandIx (b, bLevel)]);
            const auto pan = p[bandIx (b, bPan)] / 100.0;
            x.panL = std::sqrt (0.5 * (1.0 - pan)) * 1.41421356; x.panR = std::sqrt (0.5 * (1.0 + pan)) * 1.41421356;
        }
        const auto inG = dbToGain (p[input]), outG = dbToGain (p[output]);
        const auto wet = p[mix] / 100.0;
        const auto gAtt = std::exp (-1.0 / (0.0008 * sr)), gRel = std::exp (-1.0 / (0.06 * sr));
        const auto dAtt = std::exp (-1.0 / (0.005 * sr)), dRel = std::exp (-1.0 / (0.12 * sr));
        const auto gateSm = std::exp (-1.0 / (0.002 * sr));
        std::array<float, numBands> bm {};

        for (int i = 0; i < n; ++i)
        {
            const double in[2] { L[i] * inG, R[i] * inG };
            // direct retardé comme les bandes (même suréchantillonneur) : pas de peigne au mélange
            double dry[2];
            { double a, b; ch[0].dryOs.up (L[i], a, b); dry[0] = ch[0].dryOs.down (a, b); }
            { double a, b; ch[1].dryOs.up (R[i], a, b); dry[1] = ch[1].dryOs.down (a, b); }
            // séparation
            double band[2][numBands];
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const auto x = in[c];
                double lo, r1, m1, r2, m2, hi;
                s.xo[0].process (x, lo, r1);
                if (nb == 2) { band[c][0] = lo; band[c][1] = r1; band[c][2] = band[c][3] = 0.0; continue; }
                s.xo[1].process (r1, m1, r2);
                if (nb == 3) { band[c][0] = s.ap[0].process (lo); band[c][1] = m1; band[c][2] = r2; band[c][3] = 0.0; continue; }
                s.xo[2].process (r2, m2, hi);
                band[c][0] = s.ap[1].process (s.ap[0].process (lo));
                band[c][1] = s.ap[2].process (m1);
                band[c][2] = m2;
                band[c][3] = hi;
            }
            double out[2] { 0.0, 0.0 };
            for (int b = 0; b < nb; ++b)
            {
                const auto& x = bd[(size_t) b];
                const auto lvl = std::max (std::abs (band[0][b]), std::abs (band[1][b]));
                // porte de bruit (rapide, sans claquement : gain lissé)
                auto& ge = gateEnv[(size_t) b];
                ge = lvl > ge ? lvl + (ge - lvl) * gAtt : lvl + (ge - lvl) * gRel;
                const auto open = x.gateThr <= -79.5 || fdb (ge + 1.0e-9) > x.gateThr;
                auto& gg = gateGain[(size_t) b];
                gg = (open ? 1.0 : 0.0) + (gg - (open ? 1.0 : 0.0)) * gateSm;
                // dynamique « montante » : < 0 resserre vers -18 dB (les faibles montent, les forts baissent) ;
                // > 0 expanse (les écarts grandissent)
                auto& de = dynEnv[(size_t) b];
                de = lvl > de ? lvl + (de - lvl) * dAtt : lvl + (de - lvl) * dRel;
                double dg = 1.0;
                if (std::abs (x.dyn) > 0.005 && de > 1.0e-5)
                {
                    const auto dist = fdb (de) + 18.0;
                    const auto gDb = std::clamp (x.dyn < 0.0 ? dist * x.dyn * 0.85 : dist * x.dyn * 0.7, -30.0, 18.0);
                    dg = fgain (gDb);
                }
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[(size_t) c];
                    // générateur de réinjection : la sortie de la bande revient à son entrée, accordée
                    double v = band[c][b] * gg * dg;
                    if (x.fb > 0.0) v += x.fb * s.fb[(size_t) b].read (x.fbDelay[c]);
                    double u0, u1;
                    s.os[(size_t) b].up (v, u0, u1);
                    u0 = grinder::curve (x.type, u0, x.drive, x.bias); u1 = grinder::curve (x.type, u1, x.drive, x.bias);
                    auto y = s.os[(size_t) b].down (u0, u1);
                    y = s.dc[(size_t) b].process (y);
                    s.fb[(size_t) b].push (fastTanh (y));
                    // compensation douce du drive pour garder un niveau exploitable
                    const auto comp = x.type == 0 ? 1.0 : 1.0 / (1.0 + 0.6 * x.drive);
                    const auto o = y * comp * x.level * (c == 0 ? x.panL : x.panR);
                    out[c] += o;
                    bm[(size_t) b] = std::max (bm[(size_t) b], (float) std::abs (o));
                }
            }
            L[i] = (float) ((dry[0] * (1.0 - wet) + out[0] * wet) * outG);
            R[i] = (float) ((dry[1] * (1.0 - wet) + out[1] * wet) * outG);
        }
        for (int b = 0; b < numBands; ++b) bandMeter[(size_t) b] = std::max (bm[(size_t) b], bandMeter[(size_t) b] * 0.85f);
    }

private:
    struct FbLine
    {
        std::vector<float> buf; int mask = 0, w = 0;
        void init (int len) { int n = 1; while (n < len + 4) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
        void clear() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (double x) noexcept { buf[(size_t) w] = (float) x; w = (w + 1) & mask; }
        double read (double d) const noexcept
        {
            const auto pos = (double) w - d;
            const auto fl = std::floor (pos);
            const auto i0 = (int) fl; const auto f = pos - fl;
            const auto a = (double) buf[(size_t) (i0 & mask)], b = (double) buf[(size_t) ((i0 + 1) & mask)];
            return a + (b - a) * f;
        }
    };
    struct Dc { double x1 = 0, y1 = 0; double process (double x) { const auto y = x - x1 + 0.9995 * y1; x1 = x; y1 = y; return y; } };
    struct Chan
    {
        std::array<LR4Split, 3> xo; std::array<SvfEq, 3> ap;
        std::array<FbLine, grinder::numBands> fb;
        std::array<Oversampler2x, grinder::numBands> os;
        std::array<double, grinder::numBands> fbY {};
        std::array<Dc, grinder::numBands> dc {};
        Oversampler2x dryOs;
    };
    std::array<Chan, 2> ch;
    std::array<double, grinder::numBands> gateEnv {}, gateGain {}, dynEnv {}, levelPrev {};
    std::array<float, grinder::numBands> bandMeter {};
    FxModBank<grinder::numMods> mods;
    double sr = 48000.0, hostPpq = 0.0;
    bool hostPlaying = false;
};

} // namespace grv::effects
