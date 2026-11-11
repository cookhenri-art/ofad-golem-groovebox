/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 — famille « Golem Titan FX », lot G3 : les deux saturations haut de gamme.
    Fiche d'architecture : claude/P3/SPEC-GOLEM-TITAN-FX.md (projet).

      - Titan Décapiteur (principe des saturations analogiques à 5 « styles » de circuits) :
          · A : préampli de magnétophone à lampes, très doux, aigus légèrement arrondis ;
          · E : console à transformateurs, grave épais et aigus soyeux ;
          · N : entrée à transistors au germanium, grave lourd, asymétrique ;
          · T : triode, harmoniques paires, chaude ;
          · P : pentode, harmoniques impaires, plus dure.
        AVANT la saturation : coupe-bas, « bosse » (petite bosse de grave qui suit la fréquence du
        coupe-bas, comme la tête d'un magnétophone), tonalité (inclinaison douce) : ils décident de CE qui
        sature. APRÈS : coupe-haut, pente douce (6 dB/oct) ou raide (30 dB/oct, effet baffle).
        Drive à courbe quadratique, « punition » (+20 dB), gain automatique, mélange. ×2 sans latence.

      - Titan Saturne (principe des saturations multibandes) : 1 à 4 bandes, séparations de
        Linkwitz-Riley 24 dB/oct en arbre, bandes basses recalées en phase par des passe-tout (la somme
        sans traitement rend le son d'origine) ; par bande : 16 styles (lampes, bandes, saturation,
        transformateur, 4 amplis, redressement, repli, dégradation, réduction de bits, étalement, fuzz),
        drive, réinjection (résonance courte accordée sur la bande), dynamique (le drive suit le niveau,
        dans un sens ou dans l'autre), tonalité, mélange, niveau. ×2 sans latence par bande.

    Aucune latence. Aucune allocation dans le rendu.
*/

#pragma once

#include "Titan1.h"
#include "../dsp/Oversampler.h"

namespace grv::effects
{

//==============================================================================
// Titan Décapiteur
namespace tdecap
{
    enum Slot { style = 0, drive, punish, autoGain, output, mix, lowCut, thump, tone, highCut, steep, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tdecap"; m.name = "Titan D\xC3\xA9" "capiteur"; m.role = "Saturation analogique"; m.family = titan::family; m.form = "module";
            m.principle = "5 circuits (magn\xC3\xA9tophone \xC3\xA0 lampes, console, germanium, triode, pentode) ; coupe-bas, bosse et tonalit\xC3\xA9 avant, coupe-haut doux ou raide apr\xC3\xA8s ; punition, gain auto ; sans latence";
            m.params = {
                C ("style", "Circuit", { "A \xC2\xB7 Magn\xC3\xA9tophone", "E \xC2\xB7 Console", "N \xC2\xB7 Germanium", "T \xC2\xB7 Triode", "P \xC2\xB7 Pentode" }),
                P ("drive", "Drive", "", 0, 10, 3),
                C ("punish", "Punition", { "Non", "Oui" }),
                C ("autoGain", "Gain auto", { "Non", "Oui" }, 1),
                P ("output", "Sortie", "dB", -24, 12, 0),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                P ("lowCut", "Coupe-bas", "Hz", 10, 600, 10, 0.4f),
                C ("thump", "Bosse", { "Non", "Oui" }),
                P ("tone", "Tonalit\xC3\xA9", "", -10, 10, 0),
                P ("highCut", "Coupe-haut", "Hz", 1000, 30000, 30000, 0.4f),
                C ("steep", "Pente raide", { "Non", "Oui" }),
            };
            m.sections = {
                { "drive", "Saturation", "dist", { "style", "drive", "punish", "autoGain", "output", "mix" }, "global" },
                { "tone", "Avant et apr\xC3\xA8s", "distfilter", { "lowCut", "thump", "tone", "highCut", "steep" }, "global" },
            };
            m.presets = {
                { "Chaleur de console", { { "style", 1 } } },
                { "Bande douce sur la voix", { { "style", 0 }, { "drive", 2.5f }, { "highCut", 14000 } } },
                { "Batterie qui cogne", { { "style", 2 }, { "drive", 5 }, { "lowCut", 70 }, { "thump", 1 }, { "mix", 60 } } },
                { "Basse \xC3\xA9paisse", { { "style", 2 }, { "drive", 4 }, { "tone", -3 } } },
                { "Triode sur synth\xC3\xA9", { { "style", 3 }, { "drive", 5 } } },
                { "Pentode agressive", { { "style", 4 }, { "drive", 7 }, { "highCut", 9000 } } },
                { "Baffle de guitare", { { "style", 4 }, { "drive", 6 }, { "highCut", 4500 }, { "steep", 1 } } },
                { "Radio AM", { { "style", 3 }, { "drive", 6 }, { "lowCut", 450 }, { "highCut", 3000 }, { "steep", 1 } } },
                { "Destruction totale", { { "style", 4 }, { "drive", 10 }, { "punish", 1 }, { "mix", 70 } } },
            };
            return m;
        }();
        return d;
    }

    /** Courbes des 5 circuits (sans état). */
    inline double shape (int st, double x) noexcept
    {
        switch (st)
        {
            case 0: { const auto y = std::tanh (0.85 * x + 0.06 * x * x); return y / 0.85; }          // lampes douces, un soupçon d'asymétrie
            case 1: { const auto u = x + 0.12 * x * x; return u / std::sqrt (1.0 + 0.55 * u * u); }   // transformateur : arrondi progressif
            case 2: { return x >= 0.0 ? (1.0 - std::exp (-1.15 * x)) / 1.15 : -(1.0 - std::exp (1.6 * x)) / 1.6; } // germanium asymétrique
            case 3: { const auto y = std::tanh (x); return y + 0.22 * (x * x) / (1.0 + x * x); }      // triode : paires
            default: { const auto a = std::abs (x); return x / std::pow (1.0 + std::pow (a, 2.6), 1.0 / 2.6); } // pentode : impaires, coude net
        }
    }
}

class TitanDecap final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tdecap::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& d : dc) d.prepare (sr * 2.0, 6.0);
        reset();
    }
    void reset() noexcept override
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& f : pre[c]) f.reset();
            for (auto& f : post[c]) f.reset();
            up[c].reset(); dn[c].reset(); dc[c].reset(); lp1[c] = 0.0; top[c] = 0.0; emph[c].reset(); deemph[c].reset();
        }
        agEnvIn = agEnvOut = 1.0e-4;
    }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tdecap;
        const int st = std::clamp ((int) p[style], 0, 4);
        const double d = std::clamp ((double) p[drive], 0.0, 10.0) / 10.0;
        const double driveDb = 36.0 * d * d + (p[punish] > 0.5f ? 20.0 : 0.0);
        const double g = dsp::dbToGain (driveDb);
        static constexpr double styleIn[5] { 0.55, 0.6, 0.7, 0.65, 0.75 };       // niveau d'entrée du circuit
        const double lc = std::clamp ((double) p[lowCut], 10.0, 0.45 * sr);
        const bool thp = p[thump] > 0.5f;
        const double tn = p[tone] / 10.0;
        const double hc = std::min ((double) p[highCut], 0.45 * sr);
        const bool stp = p[steep] > 0.5f;
        const bool hcOn = p[highCut] < 29500.0f;
        const double out = dsp::dbToGain (p[output]);
        const double wet = p[mix] / 100.0;
        const bool ag = p[autoGain] > 0.5f;
        // pré-accentuation propre aux circuits (E et N épaississent le grave avant de saturer)
        static constexpr double emphDb[5] { 0.0, 2.5, 3.5, 1.0, 0.0 };
        for (int c = 0; c < 2; ++c)
        {
            pre[c][0].set (dsp::SvfEq::highpass, lc, 0.7, 0.0, sr);
            pre[c][1].set (dsp::SvfEq::bell, std::max (lc * 1.1, 20.0), 1.3, thp ? 4.5 : 0.0, sr);
            pre[c][2].set (dsp::SvfEq::lowShelf, 300.0, 0.6, -6.0 * tn, sr);
            pre[c][3].set (dsp::SvfEq::highShelf, 2500.0, 0.6, 6.0 * tn, sr);
            emph[c].set (dsp::SvfEq::lowShelf, 110.0, 0.7, emphDb[st], sr * 2.0);
            deemph[c].set (dsp::SvfEq::lowShelf, 110.0, 0.7, -emphDb[st] * 0.6, sr * 2.0);
            post[c][0].set (dsp::SvfEq::lowpass, hc, 0.54, 0.0, sr);
            post[c][1].set (dsp::SvfEq::lowpass, hc, 1.31, 0.0, sr);
        }
        const double lp1C = std::exp (-kTwoPi * hc / sr);
        static constexpr double topCut[5] { 15000.0, 19000.0, 17000.0, 20000.0, 21000.0 }; // A arrondit les aigus
        const double tc = std::exp (-kTwoPi * std::min (topCut[st], 0.45 * sr) / sr);
        const double agC = std::exp (-1.0 / (0.25 * sr));
        for (int i = 0; i < n; ++i)
        {
            double y[2], x0[2];
            for (int c = 0; c < 2; ++c)
            {
                double x = c == 0 ? L[i] : R[i];
                x0[c] = x;
                for (auto& f : pre[c]) x = f.process (x);
                double a, b;
                up[c].up (x * g * styleIn[st], a, b);
                a = shape (st, emph[c].process (a)); a = deemph[c].process (dc[c].process (a));
                b = shape (st, emph[c].process (b)); b = deemph[c].process (dc[c].process (b));
                auto v = dn[c].down (a, b) / styleIn[st];
                // arrondi naturel des aigus du circuit
                top[c] = v + (top[c] - v) * tc; v = top[c];
                if (hcOn)
                {
                    if (stp) { v = post[c][1].process (post[c][0].process (v)); lp1[c] = v + (lp1[c] - v) * lp1C; v = lp1[c]; } // 30 dB/oct
                    else { lp1[c] = v + (lp1[c] - v) * lp1C; v = lp1[c]; }                                                   // 6 dB/oct
                }
                y[c] = v;
            }
            // gain auto : le niveau de sortie suit celui d'entrée (lentement, sans pomper)
            double agg = 1.0;
            if (ag)
            {
                agEnvIn = std::max (1.0e-5, std::abs (x0[0]) + std::abs (x0[1])) * (1.0 - agC) + agEnvIn * agC;
                agEnvOut = std::max (1.0e-5, std::abs (y[0]) + std::abs (y[1])) * (1.0 - agC) + agEnvOut * agC;
                agg = std::clamp (agEnvIn / agEnvOut, 0.06, 2.0);
            }
            for (int c = 0; c < 2; ++c)
            {
                const auto v = y[c] * agg * out;
                const auto o = x0[c] * (1.0 - wet) + v * wet;
                (c == 0 ? L[i] : R[i]) = (float) std::clamp (o, -8.0, 8.0);
            }
        }
    }
private:
    std::array<std::array<dsp::SvfEq, 4>, 2> pre;
    std::array<std::array<dsp::SvfEq, 2>, 2> post;
    std::array<dsp::SvfEq, 2> emph, deemph;
    std::array<dsp::Oversampler2x, 2> up, dn;
    std::array<dsp::DcBlocker, 2> dc;
    double lp1[2] {}, top[2] {};
    double sr = 48000.0, agEnvIn = 1.0e-4, agEnvOut = 1.0e-4;
};

//==============================================================================
// Titan Saturne
namespace tsat
{
    constexpr int numBands = 4, perBand = 7;
    enum Global { bands = 0, x1, x2, x3, mix, output, numGlobal };
    enum Band { bStyle = 0, bDrive, bFeedback, bDynamics, bTone, bMix, bLevel };
    inline int slot (int band, int what) noexcept { return numGlobal + band * perBand + what; }

    inline const std::vector<std::string>& styles()
    {
        static const std::vector<std::string> v {
            "Lampe douce", "Lampe chaude", "Bande douce", "Bande chaude", "Saturation", "Transformateur",
            "Ampli clair", "Ampli crunch", "Ampli rock", "Ampli plexi", "Redressement", "Repli",
            "D\xC3\xA9gradation", "R\xC3\xA9" "duction de bits", "\xC3\x89talement", "Fuzz" };
        return v;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tsaturn"; m.name = "Titan Saturne"; m.role = "Saturation multibande"; m.family = titan::family; m.form = "module";
            m.principle = "1 \xC3\xA0 4 bandes (Linkwitz-Riley recal\xC3\xA9" "es en phase), 16 styles par bande (lampes, bandes, transformateur, amplis, repli, bits, fuzz\xE2\x80\xA6), r\xC3\xA9injection, dynamique, tonalit\xC3\xA9 ; sans latence";
            m.params = {
                I ("bands", "Bandes", "", 1, 4, 3),
                P ("x1", "S\xC3\xA9paration 1", "Hz", 30, 2000, 180, 0.4f),
                P ("x2", "S\xC3\xA9paration 2", "Hz", 300, 8000, 2200, 0.4f),
                P ("x3", "S\xC3\xA9paration 3", "Hz", 2000, 16000, 8000, 0.4f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                P ("output", "Sortie", "dB", -24, 12, 0),
            };
            static const char* bn[4] { "Grave", "Bas-m\xC3\xA9" "dium", "Haut-m\xC3\xA9" "dium", "Aigu" };
            static const int defStyle[4] { 1, 3, 0, 2 };
            for (int b = 0; b < numBands; ++b)
            {
                const std::string k = std::to_string (b + 1), nm = bn[b];
                m.params.push_back (C ("style" + k, nm + " : style", styles(), defStyle[b]));
                m.params.push_back (P ("drive" + k, nm + " : drive", "%", 0, 100, 25));
                m.params.push_back (P ("fb" + k, nm + " : r\xC3\xA9injection", "%", 0, 100, 0));
                m.params.push_back (P ("dyn" + k, nm + " : dynamique", "%", -100, 100, 0));
                m.params.push_back (P ("tone" + k, nm + " : tonalit\xC3\xA9", "", -10, 10, 0));
                m.params.push_back (P ("mix" + k, nm + " : m\xC3\xA9lange", "%", 0, 100, 100));
                m.params.push_back (P ("level" + k, nm + " : niveau", "dB", -24, 12, 0));
            }
            m.sections = { { "bands", "Bandes", "bands", { "bands", "x1", "x2", "x3", "mix", "output" }, "global" } };
            for (int b = 0; b < numBands; ++b)
            {
                const std::string k = std::to_string (b + 1);
                m.sections.push_back ({ "band" + k, std::string (bn[b]), "dist", { "style" + k, "drive" + k, "fb" + k, "dyn" + k, "tone" + k, "mix" + k, "level" + k }, "global" });
            }
            m.presets = {
                { "Chaleur trois bandes", {} },
                { "Mastering subtil", { { "drive1", 12 }, { "drive2", 10 }, { "drive3", 8 }, { "style1", 0 }, { "style2", 2 }, { "style3", 0 } } },
                { "Basse : grave propre, m\xC3\xA9" "dium sale", { { "bands", 2 }, { "x1", 150 }, { "drive1", 10 }, { "style2", 8 }, { "drive2", 60 } } },
                { "Batterie \xC3\xA9" "cras\xC3\xA9" "e", { { "style1", 5 }, { "drive1", 45 }, { "style2", 15 }, { "drive2", 50 }, { "style3", 7 }, { "drive3", 40 }, { "mix", 60 } } },
                { "Aigus cristallins", { { "bands", 4 }, { "style4", 4 }, { "drive4", 45 }, { "tone4", 3 } } },
                { "Repli m\xC3\xA9tallique", { { "bands", 2 }, { "style2", 11 }, { "drive2", 55 }, { "fb2", 40 } } },
                { "Radio cass\xC3\xA9" "e", { { "bands", 1 }, { "style1", 13 }, { "drive1", 60 }, { "tone1", 4 } } },
                { "Ampli plexi", { { "bands", 1 }, { "style1", 9 }, { "drive1", 65 } } },
                { "Pompe dynamique", { { "dyn1", -70 }, { "dyn2", -50 }, { "drive1", 50 }, { "drive2", 45 } } },
            };
            return m;
        }();
        return d;
    }
}

class TitanSaturn final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tsat::desc(); }
    void prepare (double s, int) override
    {
        sr = s;
        for (auto& c : chan) for (auto& b : c.band) { b.fb.assign ((size_t) 4096, 0.0f); b.dc.prepare (sr * 2.0, 8.0); }
        reset();
    }
    void reset() noexcept override
    {
        for (auto& c : chan)
        {
            for (auto& f : c.lp) for (auto& x : f) x.reset();
            for (auto& f : c.hp) for (auto& x : f) x.reset();
            for (auto& a : c.ap) a.reset();
            for (auto& b : c.band)
            {
                std::fill (b.fb.begin(), b.fb.end(), 0.0f); b.w = 0; b.up.reset(); b.dn.reset(); b.dc.reset(); b.tone.reset();
                b.env = 0.0; b.prev = 0.0; b.held = 0.0; b.hold = 0.0; b.stage.reset(); b.slew = 0.0;
            }
        }
    }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tsat;
        const int nb = std::clamp ((int) std::lround (p[bands]), 1, numBands);
        double xf[3] { p[x1], std::max ((double) p[x2], p[x1] * 1.5), 0.0 };
        xf[2] = std::max ((double) p[x3], xf[1] * 1.5);
        for (auto& f : xf) f = std::min (f, 0.42 * sr);
        static constexpr double q = 0.70710678118654752;
        for (auto& c : chan)
            for (int k = 0; k < 3; ++k)
            {
                for (int s = 0; s < 2; ++s) { c.lp[k][s].set (dsp::SvfEq::lowpass, xf[k], q, 0.0, sr); c.hp[k][s].set (dsp::SvfEq::highpass, xf[k], q, 0.0, sr); }
                c.ap[k].set (dsp::SvfEq::allpass, xf[k], q, 0.0, sr);
                c.ap2[k].set (dsp::SvfEq::allpass, xf[k], q, 0.0, sr);
            }
        BandParams bp[numBands];
        const double center[4] { std::sqrt (30.0 * xf[0]), std::sqrt (xf[0] * xf[1]), std::sqrt (xf[1] * xf[2]), std::min (0.4 * sr, xf[2] * 1.8) };
        for (int b = 0; b < numBands; ++b)
        {
            auto& o = bp[b];
            o.style = std::clamp ((int) p[slot (b, bStyle)], 0, 15);
            const double d = p[slot (b, bDrive)] / 100.0;
            o.drive = dsp::dbToGain (36.0 * d * d);
            o.makeup = 1.0 / std::sqrt (o.drive);
            o.fb = 0.85 * p[slot (b, bFeedback)] / 100.0;
            o.fbLen = std::clamp ((int) (2.0 * sr / std::max (40.0, center[nb == 1 ? 1 : b])), 4, 4000);
            o.dyn = p[slot (b, bDynamics)] / 100.0;
            o.mix = p[slot (b, bMix)] / 100.0;
            o.level = dsp::dbToGain (p[slot (b, bLevel)]);
            o.tone = p[slot (b, bTone)] / 10.0;
            o.drv = d;
            for (auto& c : chan)
            {
                c.band[(size_t) b].tone.set (dsp::SvfEq::highShelf, std::min (center[b] * 1.5, 0.4 * sr), 0.6, 9.0 * o.tone, sr * 2.0);
                c.band[(size_t) b].stage.set (o.style == 9 ? dsp::SvfEq::bell : dsp::SvfEq::highpass, o.style == 9 ? 900.0 : 140.0, 0.7, o.style == 9 ? 5.0 : 0.0, sr * 2.0); // entre deux étages d'ampli
            }
        }
        const double envC = std::exp (-1.0 / (0.03 * sr));
        const double wet = p[mix] / 100.0, out = dsp::dbToGain (p[output]);
        for (int i = 0; i < n; ++i)
            for (int ci = 0; ci < 2; ++ci)
            {
                auto& c = chan[(size_t) ci];
                const double x = ci == 0 ? L[i] : R[i];
                // arbre de séparation : bande 1 = bas de x1 ; reste = haut de x1 ; etc.
                double part[numBands] {};
                double rest = x;
                for (int k = 0; k < nb - 1; ++k)
                {
                    const auto lo = c.lp[k][1].process (c.lp[k][0].process (rest));
                    const auto hi = c.hp[k][1].process (c.hp[k][0].process (rest));
                    part[k] = lo; rest = hi;
                }
                part[nb - 1] = rest;
                // recalage de phase : chaque bande basse passe par les passe-tout des séparations plus hautes
                for (int b = 0; b < nb - 1; ++b)
                    for (int k = b + 1; k < nb - 1; ++k) part[b] = (b == 0 ? c.ap[k] : c.ap2[k]).process (part[b]);
                double sum = 0.0;
                for (int b = 0; b < nb; ++b) sum += processBand (c.band[(size_t) b], bp[b], part[b], envC);
                const auto o = (x * (1.0 - wet) + sum * wet) * out;
                (ci == 0 ? L[i] : R[i]) = (float) std::clamp (o, -8.0, 8.0);
            }
    }

private:
    struct BandParams { int style = 0; double drive = 1.0, makeup = 1.0, fb = 0.0, dyn = 0.0, mix = 1.0, level = 1.0, tone = 0.0, drv = 0.0; int fbLen = 100; };
    struct BandState
    {
        std::vector<float> fb; int w = 0;
        dsp::Oversampler2x up, dn;
        dsp::DcBlocker dc;
        dsp::SvfEq tone, stage;
        double env = 0.0, prev = 0.0, held = 0.0, hold = 0.0, slew = 0.0;
    };
    struct Chan
    {
        std::array<std::array<dsp::SvfEq, 2>, 3> lp, hp;
        std::array<dsp::SvfEq, 3> ap, ap2;
        std::array<BandState, 4> band;
    };

    /** Les 16 styles (au double de la fréquence). */
    double shapeOne (BandState& s, int st, double x, double drv) noexcept
    {
        switch (st)
        {
            case 0:  return std::tanh (x * 0.7) / 0.7 * 0.6 + x / (1.0 + std::abs (x)) * 0.4;
            case 1:  return std::tanh (x + 0.25 * x * x);
            case 2:  return std::atan (x * 0.8) / 0.8 * 0.75 + 0.25 * x / std::sqrt (1.0 + x * x);
            case 3:  { const auto u = x + 0.35 * (x - s.prev); s.prev = x; return std::tanh (u); }   // bande : petite hystérésis
            case 4:  return x / (1.0 + std::abs (x));
            case 5:  { const auto a = std::abs (x); return a < 1.0 ? x - 0.12 * x * x * x : std::copysign (0.88 + 0.12 * std::tanh ((a - 1.0) * 3.0), x); }
            case 6:  return std::tanh (1.2 * x + 0.05) - std::tanh (0.05);
            case 7:  return std::tanh (2.2 * x + 0.3) - std::tanh (0.3);
            case 8:  { auto y = std::tanh (3.0 * x); y = s.stage.process (y); return std::tanh (2.5 * y); }  // deux étages, filtre entre
            case 9:  { auto y = std::tanh (2.5 * x + 0.15); y = s.stage.process (y); return std::tanh (3.0 * y) * 0.9; }
            case 10: return 0.5 * x + 0.8 * std::abs (std::tanh (x)) - 0.3;                         // redressement (continu retiré ensuite)
            case 11: return std::sin (std::clamp (x, -8.0, 8.0) * 1.5707963);                          // repli
            case 12: { const auto a = std::abs (x); const auto th = 0.05 + 0.25 * drv;                  // dégradation : le circuit « s'étouffe »
                       const auto y = a > th ? std::copysign (a - th, x) * (1.0 + th) : x * a / th * 0.3; return std::tanh (y * 1.5); }
            case 13: { const auto bits = 12.0 - 10.0 * drv; const auto qn = std::pow (2.0, bits);         // réduction de bits + d'échantillons
                       s.hold += 1.0 - 0.9 * drv; if (s.hold >= 1.0) { s.hold -= 1.0; s.held = std::round (std::tanh (x) * qn) / qn; } return s.held; }
            case 14: { const auto lim = 0.02 + 0.3 * (1.0 - drv); s.slew += std::clamp (x - s.slew, -lim, lim); return std::tanh (s.slew); } // étalement
            default: { const auto y = std::clamp (x * 3.0 + 0.2, -1.0, 1.0); return y - 0.2 * (1.0 - std::abs (y)); }                     // fuzz
        }
    }

    double processBand (BandState& s, const BandParams& o, double x, double envC) noexcept
    {
        // dynamique : le drive suit le niveau de la bande (+ : plus fort = plus sale ; − : l'inverse)
        const auto a = std::abs (x);
        s.env = a > s.env ? a : a + (s.env - a) * envC;
        const double dynG = o.dyn == 0.0 ? 1.0 : std::pow (2.0, o.dyn * 3.0 * std::clamp ((dsp::gainToDb (s.env) + 30.0) / 30.0, 0.0, 1.0));
        double in = x * o.drive * dynG;
        // réinjection : écho très court accordé sur la bande (résonance colorée)
        if (o.fb > 0.0)
        {
            const auto r = s.fb[(size_t) ((s.w - o.fbLen) & 4095)];
            in += o.fb * r * o.drive;
        }
        double u0, u1;
        s.up.up (in, u0, u1);
        u0 = s.tone.process (s.dc.process (shapeOne (s, o.style, u0, o.drv)));
        u1 = s.tone.process (s.dc.process (shapeOne (s, o.style, u1, o.drv)));
        auto y = s.dn.down (u0, u1) * o.makeup;
        s.fb[(size_t) s.w] = (float) std::clamp (y, -2.0, 2.0); s.w = (s.w + 1) & 4095;
        return (x * (1.0 - o.mix) + y * o.mix) * o.level;
    }

    std::array<Chan, 2> chan;
    double sr = 48000.0;
};

} // namespace grv::effects
