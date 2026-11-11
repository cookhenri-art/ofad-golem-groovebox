/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Effets de studio (v0.34), chacun sur son principe (architecture seulement, ni nom ni façade) :

      - « Multibande » (principe des multibandes à bandes flottantes) : 4 bandes séparées par des
        filtres de Linkwitz-Riley d'ordre 4 (somme plate, compensation de phase par passe-tout),
        compression ou expansion par bande, gain et écoute seule ; plus 2 bandes « flottantes »
        posées n'importe où (cloche dynamique : la bande se creuse, ou se relève, quand son
        contenu dépasse le seuil). Aucune anticipation : latence nulle.
      - « Compresseur à lampes » (principe vari-mu) : le rapport grandit avec le niveau (genou très
        large, de 1,5:1 à 8:1), six constantes de temps dont deux dépendant du programme, étage
        de lampe (harmoniques paires douces), mode stéréo lié ou milieu / côtés.
      - « Égaliseur à lampes » (principe des égaliseurs passifs de studio des années 50) : grave
        accentué ET atténué sur la même fréquence (bosse + creux voisins), aigu en cloche à
        largeur réglable, atténuation des aigus en plateau, étage de lampe de sortie.
      - « Égaliseur de console » (principe des tranches de console anglaises) : passe-haut 18 dB/oct,
        passe-bas 12 dB/oct, 4 bandes (grave et aigu en plateau ou cloche, deux médiums
        paramétriques), trois caractères : Q proportionnel au gain (« noir »), Q constant à
        plateaux plus marqués (« marron »), et caractère G (Q proportionnel + légère saturation).

    Temps réel : aucune allocation ; coefficients recalculés seulement quand un réglage change
    (SvfEq garde les siens) ; les cloches dynamiques sont remises à jour une fois par bloc.
*/

#pragma once

#include "Dynamics.h"
#include "Mastering.h"
#include "FxBanks.h"

#include <array>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

/** Saturation de lampe douce : impaires (tanh) + paires (carré), sans continu (filtré plus loin). */
inline double tubeSat (double x, double drive) noexcept
{
    if (drive <= 1.0e-4) return x;
    const auto k = 1.0 + drive * 3.0;
    const auto y = std::tanh (x * k) / k;
    return y + drive * 0.12 * y * y;
}

//==============================================================================
namespace mband
{
    inline constexpr int numBands = 4, numFloat = 2;
    inline const std::vector<std::string>& modeChoices() { static const std::vector<std::string> v { "Compression", "Expansion" }; return v; }
    inline std::string bid (int b, const char* s) { return "b" + std::to_string (b + 1) + s; }
    inline std::string fid (int f, const char* s) { return "f" + std::to_string (f + 1) + s; }
    // positions figées des réglages (ordre de desc()) : séparations, 4 bandes × 7, 2 flottantes × 7, mélange, sortie
    enum BandSlot { bThr = 0, bRat, bMode, bAtt, bRel, bGain, bSolo, bandSlots };
    enum FloatSlot { fOn = 0, fFreq, fQ, fThr, fRange, fAtt, fRel, floatSlots };
    inline constexpr int xBase = 0, bandBase = 3, floatBase = bandBase + numBands * bandSlots, mixSlot = floatBase + numFloat * floatSlots, outSlot = mixSlot + 1;
    inline constexpr int bandIx (int b, int s) noexcept { return bandBase + b * bandSlots + s; }
    inline constexpr int floatIx (int f, int s) noexcept { return floatBase + f * floatSlots + s; }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "mband"; m.name = "Quartet"; m.role = "Compresseur multibande"; m.family = "Compresseurs et dynamique"; m.form = "module";
            m.principle = "4 bandes (compression ou expansion) + 2 bandes flottantes dynamiques, s\xC3\xA9paration sans latence";
            m.params.push_back (P ("x1", "S\xC3\xA9paration 1", "Hz", 40, 1000, 150, 0.4f));
            m.params.push_back (P ("x2", "S\xC3\xA9paration 2", "Hz", 200, 5000, 1000, 0.4f));
            m.params.push_back (P ("x3", "S\xC3\xA9paration 3", "Hz", 1000, 16000, 5000, 0.4f));
            static const char* names[numBands] { "Grave", "Bas-m\xC3\xA9" "dium", "Haut-m\xC3\xA9" "dium", "Aigu" };
            for (int b = 0; b < numBands; ++b)
            {
                const std::string n = names[b];
                m.params.push_back (P (bid (b, "thr"), n + " \xC2\xB7 seuil", "dB", -60, 0, -18));
                m.params.push_back (P (bid (b, "rat"), n + " \xC2\xB7 rapport", ":1", 1, 20, 1, 0.5f));
                m.params.push_back (C (bid (b, "mode"), n + " \xC2\xB7 mode", modeChoices()));
                m.params.push_back (P (bid (b, "att"), n + " \xC2\xB7 attaque", "ms", 0.1f, 200, 10, 0.4f));
                m.params.push_back (P (bid (b, "rel"), n + " \xC2\xB7 rel\xC3\xA2" "chement", "ms", 10, 2000, 150, 0.4f));
                m.params.push_back (P (bid (b, "gain"), n + " \xC2\xB7 gain", "dB", -12, 12, 0));
                m.params.push_back (C (bid (b, "solo"), n + " \xC2\xB7 \xC3\xA9" "coute", { "Normale", "Seule", "Coup\xC3\xA9" "e" }));
            }
            for (int f = 0; f < numFloat; ++f)
            {
                const std::string n = "Flottante " + std::to_string (f + 1);
                m.params.push_back (C (fid (f, "on"), n, { "Non", "Oui" }));
                m.params.push_back (P (fid (f, "freq"), n + " \xC2\xB7 fr\xC3\xA9quence", "Hz", 30, 16000, f == 0 ? 300.0f : 4500.0f, 0.3f));
                m.params.push_back (P (fid (f, "q"), n + " \xC2\xB7 largeur (Q)", "", 0.3f, 8, 1.5f, 0.5f));
                m.params.push_back (P (fid (f, "thr"), n + " \xC2\xB7 seuil", "dB", -60, 0, -24));
                m.params.push_back (P (fid (f, "range"), n + " \xC2\xB7 plage", "dB", -18, 18, -6));
                m.params.push_back (P (fid (f, "att"), n + " \xC2\xB7 attaque", "ms", 0.1f, 200, 5, 0.4f));
                m.params.push_back (P (fid (f, "rel"), n + " \xC2\xB7 rel\xC3\xA2" "chement", "ms", 10, 2000, 120, 0.4f));
            }
            m.params.push_back (P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100));
            m.params.push_back (P ("out", "Sortie", "dB", -12, 12, 0));

            m.sections.push_back ({ "xover", "S\xC3\xA9parations et sortie", "bands", { "x1", "x2", "x3", "mix", "out" }, "main" });
            for (int b = 0; b < numBands; ++b)
                m.sections.push_back ({ "band" + std::to_string (b + 1), names[b], "comp",
                                        { bid (b, "thr"), bid (b, "rat"), bid (b, "mode"), bid (b, "att"), bid (b, "rel"), bid (b, "gain"), bid (b, "solo") }, "band" + std::to_string (b + 1) });
            for (int f = 0; f < numFloat; ++f)
                m.sections.push_back ({ "float" + std::to_string (f + 1), "Flottante " + std::to_string (f + 1), "eq",
                                        { fid (f, "on"), fid (f, "freq"), fid (f, "q"), fid (f, "thr"), fid (f, "range"), fid (f, "att"), fid (f, "rel") }, "float" + std::to_string (f + 1) });
            m.presets = {
                { "Colle de mixage", { { "b1rat", 1.6f }, { "b2rat", 1.4f }, { "b3rat", 1.4f }, { "b4rat", 1.5f }, { "b1thr", -22 }, { "b2thr", -24 }, { "b3thr", -26 }, { "b4thr", -28 },
                                       { "b1att", 30 }, { "b2att", 20 }, { "b3att", 10 }, { "b4att", 5 }, { "b1rel", 250 }, { "b2rel", 180 }, { "b3rel", 120 }, { "b4rel", 80 } }, "Mixage" },
                { "Basses tenues", { { "b1rat", 4 }, { "b1thr", -24 }, { "b1att", 15 }, { "b1rel", 120 }, { "b1gain", 1.5f } }, "Basse" },
                { "Voix : chuintantes", { { "f2on", 1 }, { "f2freq", 6500 }, { "f2q", 2.5f }, { "f2thr", -30 }, { "f2range", -9 }, { "f2att", 1 }, { "f2rel", 60 } }, "Voix" },
                { "Voix : boue du bas", { { "f1on", 1 }, { "f1freq", 280 }, { "f1q", 1.2f }, { "f1thr", -26 }, { "f1range", -6 } }, "Voix" },
                { "Batterie : claquement", { { "b3rat", 1 }, { "b3mode", 1 }, { "b3thr", -30 }, { "b3gain", 2 }, { "b4mode", 1 }, { "b4rat", 1.6f }, { "b4thr", -34 } }, "Batterie" },
                { "Mastering doux", { { "b1rat", 1.5f }, { "b2rat", 1.3f }, { "b3rat", 1.3f }, { "b4rat", 1.4f }, { "b1thr", -16 }, { "b2thr", -18 }, { "b3thr", -18 }, { "b4thr", -20 },
                                      { "b1att", 40 }, { "b2att", 25 }, { "b3att", 15 }, { "b4att", 8 } }, "Mastering" },
                { "Graves seuls (\xC3\xA9" "coute)", { { "b1solo", 1 } }, "Outils" },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Multiband final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return mband::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        for (auto& c : ch) { for (auto& f : c.xo) f.reset(); for (auto& f : c.ap) f.reset(); for (auto& f : c.bell) f.reset(); }
        for (auto& f : fdet) f.reset();
        env.fill (0.0); gr.fill (0.0); fenv.fill (0.0); fgr.fill (0.0); grMax = 0.0;
    }
    float meter (int i) const noexcept override
    {
        if (i == 0) return (float) grMax;
        if (i >= 3 && i < 3 + mband::numBands) return (float) gr[(size_t) (i - 3)];
        if (i >= 7 && i < 7 + mband::numFloat) return (float) fgr[(size_t) (i - 7)];
        return 0.0f;
    }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace mband;
        // séparations (ordonnées : chacune au moins une octave au-dessus de la précédente)
        const auto f1 = (double) p[xBase];
        const auto f2 = std::max ((double) p[xBase + 1], f1 * 2.0);
        const auto f3 = std::min (std::max ((double) p[xBase + 2], f2 * 2.0), 0.45 * sr);
        const double fx[3] { f1, f2, f3 };
        for (auto& c : ch)
            for (int k = 0; k < 3; ++k) c.xo[(size_t) k].set (fx[k], sr); // 1.1 : étage partagé (identique)
        // passe-tout de compensation : grave par les séparations 2 et 3, bas-médium par la 3
        for (auto& c : ch)
        {
            c.ap[0].set (SvfEq::allpass, f2, 0.70710678, 0.0, sr);
            c.ap[1].set (SvfEq::allpass, f3, 0.70710678, 0.0, sr);
            c.ap[2].set (SvfEq::allpass, f3, 0.70710678, 0.0, sr);
        }
        struct Band { double thr, ratio, att, rel, gain; bool expand; int solo; };
        std::array<Band, numBands> b {};
        bool anySolo = false;
        for (int k = 0; k < numBands; ++k)
        {
            auto& x = b[(size_t) k];
            x.thr = p[bandIx (k, bThr)]; x.ratio = std::max (1.0, (double) p[bandIx (k, bRat)]);
            x.expand = p[bandIx (k, bMode)] > 0.5f;
            x.att = msCoef (p[bandIx (k, bAtt)], sr); x.rel = msCoef (p[bandIx (k, bRel)], sr);
            x.gain = dbToGain (p[bandIx (k, bGain)]);
            x.solo = (int) p[bandIx (k, bSolo)];
            anySolo = anySolo || x.solo == 1;
        }
        // bandes flottantes : cloche dynamique, gain remis à jour une fois par bloc
        struct Float { bool on; double thr, range, att, rel, f, q; };
        std::array<Float, numFloat> fl {};
        for (int k = 0; k < numFloat; ++k)
        {
            auto& x = fl[(size_t) k];
            x.on = p[floatIx (k, fOn)] > 0.5f;
            x.f = std::min ((double) p[floatIx (k, fFreq)], 0.45 * sr); x.q = p[floatIx (k, fQ)];
            x.thr = p[floatIx (k, fThr)]; x.range = p[floatIx (k, fRange)];
            x.att = msCoef (p[floatIx (k, fAtt)], sr); x.rel = msCoef (p[floatIx (k, fRel)], sr);
            fdet[(size_t) k].set (SvfEq::bandpass, x.f, x.q, 0.0, sr);
            if (! x.on) { fgr[(size_t) k] = 0.0; continue; }
            for (auto& c : ch) c.bell[(size_t) k].set (SvfEq::bell, x.f, x.q, fgr[(size_t) k], sr);
        }
        const auto wet = p[mixSlot] / 100.0;
        const auto outG = dbToGain (p[outSlot]);
        double blockMax = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double inL = L[i], inR = R[i];
            double sL = inL, sR = inR;
            // bandes flottantes : détection sur le signal (bande passante), cloche dynamique
            for (int k = 0; k < numFloat; ++k)
            {
                const auto& x = fl[(size_t) k];
                if (! x.on) continue;
                const auto det = std::abs (fdet[(size_t) k].process (0.5 * (sL + sR)));
                auto& e = fenv[(size_t) k];
                e = det > e ? det + (e - det) * x.att : det + (e - det) * x.rel;
                sL = ch[0].bell[(size_t) k].process (sL);
                sR = ch[1].bell[(size_t) k].process (sR);
            }
            // séparation en 4 bandes
            double band[2][numBands];
            const double in2[2] { sL, sR };
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const auto x = in2[c];
                double lo, r1, m1, r2, m2, hi;
                s.xo[0].process (x, lo, r1);
                s.xo[1].process (r1, m1, r2);
                s.xo[2].process (r2, m2, hi);
                band[c][0] = s.ap[1].process (s.ap[0].process (lo));
                band[c][1] = s.ap[2].process (m1);
                band[c][2] = m2;
                band[c][3] = hi;
            }
            double oL = 0.0, oR = 0.0;
            for (int k = 0; k < numBands; ++k)
            {
                const auto& x = b[(size_t) k];
                auto& e = env[(size_t) k];
                const auto lvl = std::max (std::abs (band[0][k]), std::abs (band[1][k]));
                e = lvl > e ? lvl + (e - lvl) * x.att : lvl + (e - lvl) * x.rel;
                double g = 0.0; // réduction (dB, positive) ou relèvement (négative) — lisse via le suiveur
                if (x.ratio > 1.0001)
                {
                    const auto db = fdb (e + 1.0e-9);
                    if (! x.expand) g = db > x.thr ? (db - x.thr) * (1.0 - 1.0 / x.ratio) : 0.0;
                    else g = db < x.thr ? std::min (40.0, (x.thr - db) * (x.ratio - 1.0)) : 0.0; // expansion vers le bas
                }
                gr[(size_t) k] = g;
                blockMax = std::max (blockMax, g);
                const auto gg = fgain (-g) * x.gain;
                const bool heard = anySolo ? x.solo == 1 : x.solo != 2;
                if (! heard) continue;
                oL += band[0][k] * gg; oR += band[1][k] * gg;
            }
            L[i] = (float) ((inL * (1.0 - wet) + oL * wet) * outG);
            R[i] = (float) ((inR * (1.0 - wet) + oR * wet) * outG);
        }
        // gain des cloches flottantes pour le bloc suivant (dépassement du seuil × 0,5, borné par la plage)
        for (int k = 0; k < numFloat; ++k)
        {
            const auto& x = fl[(size_t) k];
            if (! x.on) continue;
            const auto over = std::max (0.0, fdb (fenv[(size_t) k] + 1.0e-9) - x.thr);
            const auto mag = std::min (std::abs (x.range), over * 0.5);
            fgr[(size_t) k] = x.range < 0.0 ? -mag : mag;
            blockMax = std::max (blockMax, x.range < 0.0 ? mag : 0.0);
        }
        grMax = blockMax;
    }

private:
    struct Chan { std::array<LR4Split, 3> xo; std::array<SvfEq, 3> ap; std::array<SvfEq, mband::numFloat> bell; };
    double sr = 48000.0, grMax = 0.0;
    std::array<Chan, 2> ch;
    std::array<SvfEq, mband::numFloat> fdet;
    std::array<double, mband::numBands> env {}, gr {};
    std::array<double, mband::numFloat> fenv {}, fgr {};
};

//==============================================================================
namespace varimu
{
    enum Slot { input = 0, threshold, timeConst, output, tube, mix, link, scSrc, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "varimu"; m.name = "Velvet"; m.role = "Compresseur \xC3\xA0 lampes"; m.family = "Compresseurs et dynamique"; m.form = "rack";
            m.principle = "Vari-mu : le rapport grandit avec le niveau, 6 constantes de temps (2 selon le programme), \xC3\xA9tage de lampe";
            m.params = {
                P ("input", "Entr\xC3\xA9" "e", "dB", -6, 24, 6),
                P ("threshold", "Seuil", "dB", -40, 0, -14),
                C ("tc", "Constante de temps", { "1 \xC2\xB7 0,3 s", "2 \xC2\xB7 0,8 s", "3 \xC2\xB7 2 s", "4 \xC2\xB7 5 s", "5 \xC2\xB7 programme", "6 \xC2\xB7 programme lent" }, 1),
                P ("output", "Sortie", "dB", -24, 12, -3),
                P ("tube", "Lampe", "%", 0, 100, 30),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                C ("link", "St\xC3\xA9r\xC3\xA9o", { "Li\xC3\xA9" "e", "Milieu / c\xC3\xB4t\xC3\xA9s" }),
                C ("scsrc", "Side-chain", sidechainChoices()),
            };
            m.sections = { { "comp", "Compresseur \xC3\xA0 lampes", "comp", { "input", "threshold", "tc", "output", "tube", "mix", "link", "scsrc" }, "main" } };
            m.presets = {
                { "Bus de mixage", { { "input", 4 }, { "threshold", -10 }, { "tc", 1 }, { "output", -2 }, { "tube", 25 } }, "Mixage" },
                { "Voix ronde", { { "input", 8 }, { "threshold", -18 }, { "tc", 0 }, { "output", -4 }, { "tube", 40 } }, "Voix" },
                { "Basse \xC3\xA9paisse", { { "input", 10 }, { "threshold", -20 }, { "tc", 2 }, { "output", -5 }, { "tube", 55 } }, "Basse" },
                { "Batterie qui respire", { { "input", 12 }, { "threshold", -22 }, { "tc", 0 }, { "output", -6 }, { "tube", 45 }, { "mix", 60 } }, "Batterie" },
                { "Mastering vari-mu", { { "input", 3 }, { "threshold", -8 }, { "tc", 4 }, { "output", -1 }, { "tube", 15 }, { "link", 1 } }, "Mastering" },
                { "Couleur seule", { { "input", 14 }, { "threshold", 0 }, { "output", -8 }, { "tube", 80 } }, "Couleur" },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class VariMuCompressor final : public CompressorBase
{
public:
    const ModuleDesc& desc() const noexcept override { return varimu::desc(); }
    void reset() noexcept override { CompressorBase::reset(); env2 = 0.0; gr2 = 0.0; dcL = dcR = 0.0; dcInL = dcInR = 0.0; }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace varimu;
        meters (L, R, n, true);
        const auto inG = dbToGain (p[input]);
        const auto thr = (double) p[threshold];
        const auto outG = dbToGain (p[output]);
        const auto drive = p[tube] / 100.0;
        const auto wet = p[mix] / 100.0;
        const bool ms = p[link] > 0.5f;
        // constantes de temps (attaque ms, relâchement ms ; 5 et 6 : relâchement double selon la durée)
        static constexpr double tAtt[6] { 0.2, 0.2, 0.4, 0.4, 0.4, 0.4 };
        static constexpr double tRel[6] { 300.0, 800.0, 2000.0, 5000.0, 2000.0, 300.0 };
        const auto tc = std::clamp ((int) p[timeConst], 0, 5);
        const auto att = msCoef (tAtt[tc] + 2.0, sr); // la lampe n'est jamais instantanée
        const auto relFast = msCoef (tRel[tc], sr);
        const auto relSlow = msCoef (tc == 4 ? 10000.0 : tc == 5 ? 25000.0 : tRel[tc], sr);
        const auto chargeUp = 1.0 - msCoef (1200.0, sr);
        const auto dcK = 1.0 - 2.0 * kPi * 8.0 / sr;
        const auto detC = msCoef (5.0, sr);
        const float* dL; const float* dR;
        detectInput ((int) p[scSrc], L, R, dL, dR);
        for (int i = 0; i < n; ++i)
        {
            const double l = L[i] * inG, r = R[i] * inG;
            const bool ext = dL != L;
            const double el = ext ? dL[i] * inG : l, er = ext ? dR[i] * inG : r;
            // détection : liée (max) ou milieu / côtés (deux détections)
            const double a = ms ? 0.5 * (el + er) : std::max (std::abs (el), std::abs (er));
            const double bside = ms ? 0.5 * (el - er) : 0.0;
            auto follow = [&] (double x, double& e, double& g)
            {
                x = std::abs (x);
                e = std::max (x, e * detC);
                const auto over = gainToDb (e + 1.0e-9) - thr;
                // genou de 12 dB, rapport qui monte avec le dépassement : 1,5:1 → 8:1
                double target = 0.0;
                if (over > -6.0)
                {
                    const auto o = over + 6.0;
                    const auto ratio = std::min (8.0, 1.5 + 0.18 * o);
                    target = o < 12.0 ? (1.0 - 1.0 / ratio) * o * o / 24.0 : (1.0 - 1.0 / ratio) * (o - 6.0);
                }
                if (target > g) { g = target + (g - target) * att; charge += (1.0 - charge) * chargeUp; }
                else g = target + (g - target) * (tc >= 4 ? (charge > 0.5 ? relSlow : relFast) : relFast);
            };
            follow (a, env, grDb);
            if (ms) follow (bside, env2, gr2);
            charge *= 0.99999;
            double yl, yr;
            if (! ms)
            {
                const auto g = dbToGain (-grDb);
                yl = l * g; yr = r * g;
            }
            else
            {
                const auto mid = 0.5 * (l + r) * dbToGain (-grDb), side = 0.5 * (l - r) * dbToGain (-gr2);
                yl = mid + side; yr = mid - side;
            }
            // étage de lampe (après la cellule), continu retiré
            yl = tubeSat (yl, drive); yr = tubeSat (yr, drive);
            const auto ol = yl - dcInL + dcK * dcL; dcInL = yl; dcL = ol;
            const auto orr = yr - dcInR + dcK * dcR; dcInR = yr; dcR = orr;
            const double x0 = L[i], x1 = R[i];
            L[i] = (float) (x0 * (1.0 - wet) + ol * outG * wet);
            R[i] = (float) (x1 * (1.0 - wet) + orr * outG * wet);
        }
        meters (L, R, n, false);
    }
private:
    double env2 = 0.0, gr2 = 0.0, dcL = 0.0, dcR = 0.0, dcInL = 0.0, dcInR = 0.0;
};

//==============================================================================
namespace tubeeq
{
    enum Slot { lowFreq = 0, lowBoost, lowAtten, hiFreq, hiBw, hiBoost, attFreq, hiAtten, tube, output, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tubeeq"; m.name = "Ember"; m.role = "\xC3\x89galiseur \xC3\xA0 lampes"; m.family = "\xC3\x89galiseurs"; m.form = "rack";
            m.principle = "\xC3\x89galiseur passif de studio : grave accentu\xC3\xA9 et att\xC3\xA9nu\xC3\xA9 sur la m\xC3\xAAme fr\xC3\xA9quence, aigu en cloche, \xC3\xA9tage de lampe";
            m.params = {
                C ("lowfreq", "Grave \xC2\xB7 fr\xC3\xA9quence", { "20 Hz", "30 Hz", "60 Hz", "100 Hz" }, 2),
                P ("lowboost", "Grave \xC2\xB7 accentuer", "", 0, 10, 0),
                P ("lowatten", "Grave \xC2\xB7 att\xC3\xA9nuer", "", 0, 10, 0),
                C ("hifreq", "Aigu \xC2\xB7 fr\xC3\xA9quence", { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" }, 4),
                P ("hibw", "Aigu \xC2\xB7 largeur", "", 0, 10, 5),
                P ("hiboost", "Aigu \xC2\xB7 accentuer", "", 0, 10, 0),
                C ("attfreq", "Aigu \xC2\xB7 att\xC3\xA9nuation \xC3\xA0", { "5 kHz", "10 kHz", "20 kHz" }, 1),
                P ("hiatten", "Aigu \xC2\xB7 att\xC3\xA9nuer", "", 0, 10, 0),
                P ("tube", "Lampe", "%", 0, 100, 20),
                P ("output", "Sortie", "dB", -12, 12, 0),
            };
            m.sections = {
                { "low", "Grave", "eq", { "lowfreq", "lowboost", "lowatten" }, "low" },
                { "high", "Aigu", "eq", { "hifreq", "hibw", "hiboost", "attfreq", "hiatten" }, "high" },
                { "out", "Lampe et sortie", "main", { "tube", "output" }, "out" },
            };
            m.presets = {
                { "Astuce du grave (bosse et creux)", { { "lowfreq", 1 }, { "lowboost", 5 }, { "lowatten", 4 } }, "Basse" },
                { "Grosse caisse ronde", { { "lowfreq", 2 }, { "lowboost", 6 }, { "lowatten", 2 }, { "hifreq", 2 }, { "hiboost", 2 }, { "hibw", 3 } }, "Batterie" },
                { "Air sur la voix", { { "hifreq", 5 }, { "hiboost", 4 }, { "hibw", 8 }, { "tube", 30 } }, "Voix" },
                { "Bus brillant", { { "lowfreq", 1 }, { "lowboost", 2 }, { "hifreq", 6 }, { "hiboost", 3 }, { "hibw", 9 }, { "tube", 25 } }, "Mixage" },
                { "Adoucir les aigus", { { "attfreq", 1 }, { "hiatten", 4 } }, "Mixage" },
                { "Couleur de lampe", { { "tube", 70 }, { "output", -3 } }, "Couleur" },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class TubeEq final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tubeeq::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override { for (auto& c : ch) { c.lb.reset(); c.la.reset(); c.hb.reset(); c.ha.reset(); c.dcIn = c.dc = 0.0; } }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tubeeq;
        static constexpr double lf[4] { 20.0, 30.0, 60.0, 100.0 };
        static constexpr double hf[7] { 3000.0, 4000.0, 5000.0, 8000.0, 10000.0, 12000.0, 16000.0 };
        static constexpr double af[3] { 5000.0, 10000.0, 20000.0 };
        const auto f = lf[std::clamp ((int) p[lowFreq], 0, 3)];
        const auto nyq = 0.45 * sr;
        for (auto& c : ch)
        {
            // accentuation : plateau résonant (Q 1,4) un peu au-dessus de f ; atténuation : plateau doux
            // plus bas (0,55 f). Ensemble : bosse vers f, creux juste au-dessus (mesuré : +7,6 dB à
            // 50 Hz et -1,9 dB vers 120 Hz pour 6 / 6 à 60 Hz)
            c.lb.set (SvfEq::lowShelf, f * 1.2, 1.4, p[lowBoost] * 1.35, sr);
            c.la.set (SvfEq::lowShelf, f * 0.55, 0.6, -p[lowAtten] * 1.4, sr);
            c.hb.set (SvfEq::bell, std::min (hf[std::clamp ((int) p[hiFreq], 0, 6)], nyq), 0.35 + (10.0 - p[hiBw]) * 0.3, p[hiBoost] * 1.8, sr);
            c.ha.set (SvfEq::highShelf, std::min (af[std::clamp ((int) p[attFreq], 0, 2)], nyq), 0.6, -p[hiAtten] * 2.0, sr);
        }
        const auto drive = p[tube] / 100.0 * 0.6;
        const auto outG = dbToGain (p[output]);
        const auto dcK = 1.0 - 2.0 * kPi * 8.0 / sr;
        for (int i = 0; i < n; ++i)
        {
            float* s[2] { L + i, R + i };
            for (int k = 0; k < 2; ++k)
            {
                auto& c = ch[(size_t) k];
                auto y = c.ha.process (c.hb.process (c.la.process (c.lb.process ((double) *s[k]))));
                y = tubeSat (y, drive);
                const auto o = y - c.dcIn + dcK * c.dc; c.dcIn = y; c.dc = o;
                *s[k] = (float) (o * outG);
            }
        }
    }
private:
    struct Chan { SvfEq lb, la, hb, ha; double dcIn = 0.0, dc = 0.0; };
    double sr = 48000.0;
    std::array<Chan, 2> ch;
};

//==============================================================================
namespace consoleeq
{
    enum Slot { hpf = 0, lpf, lfGain, lfFreq, lfBell, lmfGain, lmfFreq, lmfQ, hmfGain, hmfFreq, hmfQ, hfGain, hfFreq, hfBell, character, drive, output, numSlots };
    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "consoleeq"; m.name = "Strip"; m.role = "\xC3\x89galiseur de console"; m.family = "\xC3\x89galiseurs"; m.form = "rack";
            m.principle = "Tranche de console : filtres 18 / 12 dB, 4 bandes, Q proportionnel ou constant, saturation d'entr\xC3\xA9" "e";
            const std::vector<std::string> shelfBell { "Plateau", "Cloche" };
            m.params = {
                P ("hpf", "Passe-haut", "Hz", 16, 350, 16, 0.4f),
                P ("lpf", "Passe-bas", "Hz", 3000, 22000, 22000, 0.4f),
                P ("lfgain", "Grave \xC2\xB7 gain", "dB", -16.5f, 16.5f, 0),
                P ("lffreq", "Grave \xC2\xB7 fr\xC3\xA9quence", "Hz", 40, 600, 100, 0.4f),
                C ("lfbell", "Grave \xC2\xB7 forme", shelfBell),
                P ("lmfgain", "Bas-m\xC3\xA9" "dium \xC2\xB7 gain", "dB", -15, 15, 0),
                P ("lmffreq", "Bas-m\xC3\xA9" "dium \xC2\xB7 fr\xC3\xA9quence", "Hz", 200, 2500, 600, 0.4f),
                P ("lmfq", "Bas-m\xC3\xA9" "dium \xC2\xB7 Q", "", 0.5f, 2.5f, 1),
                P ("hmfgain", "Haut-m\xC3\xA9" "dium \xC2\xB7 gain", "dB", -15, 15, 0),
                P ("hmffreq", "Haut-m\xC3\xA9" "dium \xC2\xB7 fr\xC3\xA9quence", "Hz", 600, 7000, 2500, 0.4f),
                P ("hmfq", "Haut-m\xC3\xA9" "dium \xC2\xB7 Q", "", 0.5f, 2.5f, 1),
                P ("hfgain", "Aigu \xC2\xB7 gain", "dB", -16.5f, 16.5f, 0),
                P ("hffreq", "Aigu \xC2\xB7 fr\xC3\xA9quence", "Hz", 1500, 16000, 8000, 0.4f),
                C ("hfbell", "Aigu \xC2\xB7 forme", shelfBell),
                C ("character", "Caract\xC3\xA8re", { "Noir (Q proportionnel)", "Marron (Q constant)", "G (proportionnel + saturation)" }),
                P ("drive", "Saturation d'entr\xC3\xA9" "e", "%", 0, 100, 0),
                P ("output", "Sortie", "dB", -12, 12, 0),
            };
            m.sections = {
                { "filters", "Filtres", "filter", { "hpf", "lpf", "character", "drive", "output" }, "main" },
                { "lf", "Grave", "eq", { "lfgain", "lffreq", "lfbell" }, "lf" },
                { "lmf", "Bas-m\xC3\xA9" "dium", "eq", { "lmfgain", "lmffreq", "lmfq" }, "lmf" },
                { "hmf", "Haut-m\xC3\xA9" "dium", "eq", { "hmfgain", "hmffreq", "hmfq" }, "hmf" },
                { "hf", "Aigu", "eq", { "hfgain", "hffreq", "hfbell" }, "hf" },
            };
            m.presets = {
                { "Voix : pr\xC3\xA9sence", { { "hpf", 90 }, { "lmfgain", -3 }, { "lmffreq", 350 }, { "hmfgain", 3 }, { "hmffreq", 3500 }, { "hfgain", 2.5f }, { "hffreq", 10000 } }, "Voix" },
                { "Grosse caisse", { { "lfgain", 5 }, { "lffreq", 60 }, { "lfbell", 1 }, { "lmfgain", -6 }, { "lmffreq", 400 }, { "lmfq", 1.5f }, { "hmfgain", 4 }, { "hmffreq", 3500 } }, "Batterie" },
                { "Caisse claire", { { "hpf", 80 }, { "lfgain", 3 }, { "lffreq", 200 }, { "lfbell", 1 }, { "hmfgain", 3 }, { "hmffreq", 5000 }, { "hfgain", 2 } }, "Batterie" },
                { "Basse claire", { { "hpf", 30 }, { "lfgain", 2 }, { "lffreq", 80 }, { "lmfgain", -3 }, { "lmffreq", 250 }, { "hmfgain", 3 }, { "hmffreq", 1200 } }, "Basse" },
                { "Synth\xC3\xA9 qui perce", { { "hpf", 120 }, { "hmfgain", 4 }, { "hmffreq", 2000 }, { "hmfq", 0.8f }, { "hfgain", 3 }, { "character", 2 }, { "drive", 25 } }, "Synth\xC3\xA9" },
                { "Bus de console", { { "lfgain", 1.5f }, { "lffreq", 70 }, { "hfgain", 1.5f }, { "hffreq", 12000 }, { "character", 2 }, { "drive", 35 }, { "output", -1 } }, "Mixage" },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class ConsoleEq final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return consoleeq::desc(); }
    void prepare (double s, int) override { sr = s; reset(); }
    void reset() noexcept override
    {
        for (auto& c : ch) { for (auto& f : c.hp) f.reset(); for (auto& f : c.lp) f.reset(); c.lf.reset(); c.lmf.reset(); c.hmf.reset(); c.hf.reset(); c.dcIn = c.dc = 0.0; c.hp1s = 0.0; }
    }
    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace consoleeq;
        const auto chr = std::clamp ((int) p[character], 0, 2);
        const bool prop = chr != 1;
        const auto nyq = 0.45 * sr;
        // Q proportionnel : étroit quand on pousse fort, large pour les petites corrections
        auto qOf = [prop] (double q, double gain) { return prop ? q * (0.55 + 0.11 * std::abs (gain)) : q; };
        const bool hpOn = p[hpf] > 16.5f, lpOn = p[lpf] < 21500.0f;
        for (auto& c : ch)
        {
            // passe-haut 18 dB/oct : Butterworth d'ordre 3 = second ordre (Q 1) + premier ordre
            c.hp[0].set (SvfEq::highpass, p[hpf], 1.0, 0.0, sr);
            c.hp1g = std::tan (kPi * std::min ((double) p[hpf], nyq) / sr);
            c.lp[0].set (SvfEq::lowpass, std::min ((double) p[lpf], nyq), 0.70710678, 0.0, sr);
            const auto lg = (double) p[lfGain], hg = (double) p[hfGain];
            if (p[lfBell] > 0.5f) c.lf.set (SvfEq::bell, p[lfFreq], qOf (0.9, lg), lg, sr);
            else c.lf.set (SvfEq::lowShelf, p[lfFreq], chr == 1 ? 1.1 : 0.7, lg, sr);  // « marron » : plateau avec léger dépassement
            c.lmf.set (SvfEq::bell, p[lmfFreq], qOf (p[lmfQ], p[lmfGain]), p[lmfGain], sr);
            c.hmf.set (SvfEq::bell, std::min ((double) p[hmfFreq], nyq), qOf (p[hmfQ], p[hmfGain]), p[hmfGain], sr);
            if (p[hfBell] > 0.5f) c.hf.set (SvfEq::bell, std::min ((double) p[hfFreq], nyq), qOf (0.9, hg), hg, sr);
            else c.hf.set (SvfEq::highShelf, std::min ((double) p[hfFreq], nyq), chr == 1 ? 1.1 : 0.7, hg, sr);
        }
        const auto drv = p[drive] / 100.0 * (chr == 2 ? 0.8 : 0.5);
        const auto outG = dbToGain (p[output]);
        const auto dcK = 1.0 - 2.0 * kPi * 8.0 / sr;
        for (int i = 0; i < n; ++i)
        {
            float* s[2] { L + i, R + i };
            for (int k = 0; k < 2; ++k)
            {
                auto& c = ch[(size_t) k];
                auto y = (double) *s[k];
                if (drv > 0.0)
                {
                    y = tubeSat (y, drv);
                    const auto o = y - c.dcIn + dcK * c.dc; c.dcIn = y; c.dc = o; y = o;
                }
                if (hpOn)
                {
                    y = c.hp[0].process (y);
                    // premier ordre TPT (passe-haut)
                    const auto v = (y - c.hp1s) * c.hp1g / (1.0 + c.hp1g);
                    const auto lp = v + c.hp1s; c.hp1s = lp + v;
                    y -= lp;
                }
                y = c.hf.process (c.hmf.process (c.lmf.process (c.lf.process (y))));
                if (lpOn) y = c.lp[0].process (y);
                *s[k] = (float) (y * outG);
            }
        }
    }
private:
    struct Chan { std::array<SvfEq, 1> hp, lp; SvfEq lf, lmf, hmf, hf; double dcIn = 0.0, dc = 0.0, hp1g = 0.0, hp1s = 0.0; };
    double sr = 48000.0;
    std::array<Chan, 2> ch;
};

} // namespace grv::effects
