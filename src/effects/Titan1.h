/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 — famille « Golem Titan FX », lot G1 : les deux réverbérations haut de gamme.
    Fiche d'architecture : claude/P3/SPEC-GOLEM-TITAN-FX.md (projet).

      - Titan Vintage (principe des réverbérations numériques des années 70–80, à la manière des
        « réverbérations postmodernes » du marché) : 12 modes (salles, plaques, pièces, chambre, espaces
        aléatoire et choral, ambiance, lisses, chaotique, non linéaire) et 3 couleurs d'époque :
          · 1970 : le réservoir tourne à mi-fréquence (bande ≈ 10 kHz), interpolation linéaire avec bruit
            dans la modulation ;
          · 1980 : pleine bande, mais la même modulation bruitée ;
          · Maintenant : interpolation d'Hermite, modulation propre.
        Moteur : 4 diffuseurs passe-tout, réseau de 8 lignes (matrice de Hadamard), un passe-tout
        modulé dans chaque ligne (diffusion tardive), déclin à deux bandes, amortissement dans la boucle.
        Les modes chaotiques mettent une saturation douce de bande dans la boucle ; le mode non linéaire
        ferme la queue d'une porte (réverbération « coupée » des années 80).

      - Titan Espace (principe des réverbérations de mixage à « égaliseur de la durée ») : un bouton
        Espace (petite pièce → grande salle, durée naturelle comprise), durée de 25 à 400 %, Distance,
        Brillance, Caractère, et la marque de fabrique : la durée réglée PAR BANDE (grave, médium, aigu)
        dans la boucle, puis un égaliseur de sortie. Styles Moderne, Vintage, Plaque ; ducking, gel.

    Aucune latence. Aucune allocation dans le rendu.
*/

#pragma once

#include "SpaceFx.h"
#include "../dsp/EqFilters.h"

namespace grv::effects
{

namespace titan
{
    inline const std::string family = "Golem Titan FX";

    /** Hadamard 8 × 8 normalisée, en place. */
    inline void hadamard8 (double* h) noexcept
    {
        for (int s = 1; s < 8; s <<= 1)
            for (int k = 0; k < 8; k += 2 * s)
                for (int j = k; j < k + s; ++j) { const auto a = h[j], b = h[j + s]; h[j] = a + b; h[j + s] = a - b; }
        for (int k = 0; k < 8; ++k) h[k] *= 0.35355339059327373;
    }

    /** Petit générateur pseudo-aléatoire (fil audio). */
    struct Rand
    {
        std::uint32_t s = 0x9E3779B9u;
        double next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) s * (1.0 / 4294967296.0) * 2.0 - 1.0; }
    };

    /** Lecture d'Hermite sur une ligne (d ≥ 2). */
    inline double atHermite (const DLine& l, double d) noexcept
    {
        const auto i = (int) d; const auto f = d - i;
        return dsp::hermite4 (l.at (i - 1 < 1 ? 1 : i - 1), l.at (i), l.at (i + 1), l.at (i + 2), f);
    }

    /** Pré-délai synchronisé : libre, puis en fractions de noire. */
    inline const std::vector<std::string>& syncChoices()
    {
        static const std::vector<std::string> v { "Libre", "1/64", "1/32", "1/16", "1/16 point\xC3\xA9" "e", "1/8", "1/8 point\xC3\xA9" "e", "1/4" };
        return v;
    }
    inline double syncQuarters (int i) noexcept
    {
        static constexpr double q[8] { 0.0, 1.0 / 16, 1.0 / 8, 1.0 / 4, 3.0 / 8, 1.0 / 2, 3.0 / 4, 1.0 };
        return q[std::clamp (i, 0, 7)];
    }
}

//==============================================================================
// Titan Vintage
namespace tvint
{
    enum Slot { mode = 0, color, mix, predelay, decay, size, attack, width,
                earlyDiff, lateDiff, modRate, modDepth, freeze, highCut, damping, bassMult, bassXover, numSlots };

    inline const std::vector<std::string>& modes()
    {
        static const std::vector<std::string> v {
            "Grande salle", "Salle brillante", "Plaque", "Pi\xC3\xA8" "ce", "Chambre", "Espace al\xC3\xA9" "atoire",
            "Espace choral", "Ambiance", "Plaque lisse", "Pi\xC3\xA8" "ce lisse", "Salle chaotique", "Non lin\xC3\xA9" "aire" };
        return v;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tvintage"; m.name = "Titan Vintage"; m.role = "R\xC3\xA9verb\xC3\xA9ration d'\xC3\xA9poque"; m.family = titan::family; m.form = "module";
            m.principle = "12 r\xC3\xA9verb\xC3\xA9rations num\xC3\xA9riques des ann\xC3\xA9" "es 70\xE2\x80\x93" "80 et 3 couleurs d'\xC3\xA9poque (1970 \xC3\xA0 mi-fr\xC3\xA9quence, 1980 bruit\xC3\xA9" "e, Maintenant propre) ; sans latence";
            m.params = {
                C ("mode", "Mode", modes()),
                C ("color", "Couleur", { "1970", "1980", "Maintenant" }, 1),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 500, 10, 0.5f),
                P ("decay", "Dur\xC3\xA9" "e", "s", 0.2f, 30, 2.5f, 0.35f),
                P ("size", "Taille", "%", 0, 100, 60),
                P ("attack", "Attaque", "%", 0, 100, 40),
                P ("width", "Largeur", "%", 0, 150, 100),
                P ("earlyDiff", "Diffusion initiale", "%", 0, 100, 70),
                P ("lateDiff", "Diffusion tardive", "%", 0, 100, 60),
                P ("modRate", "Vitesse de modulation", "Hz", 0.05f, 6, 0.6f, 0.5f),
                P ("modDepth", "Profondeur de modulation", "%", 0, 100, 40),
                C ("freeze", "Gel", { "Non", "Oui" }),
                P ("highCut", "Coupe-haut", "Hz", 1000, 20000, 9000, 0.4f),
                P ("damping", "Amortissement", "Hz", 800, 20000, 6000, 0.4f),
                P ("bassMult", "Graves (rapport)", "\xC3\x97", 0.25f, 4, 1.2f, 0.5f),
                P ("bassXover", "Fr\xC3\xA9quence des graves", "Hz", 80, 1500, 400, 0.4f),
            };
            m.sections = {
                { "space", "Espace", "reverb", { "mode", "color", "mix", "predelay", "decay", "size", "attack", "width" }, "global" },
                { "motion", "Diffusion et modulation", "reverb", { "earlyDiff", "lateDiff", "modRate", "modDepth", "freeze" }, "global" },
                { "tone", "Couleur", "global", { "highCut", "damping", "bassMult", "bassXover" }, "global" },
            };
            m.presets = {
                { "Grande salle 1980", {} },
                { "Plaque de voix 1970", { { "mode", 2 }, { "color", 0 }, { "decay", 1.8f }, { "predelay", 25 }, { "size", 45 }, { "highCut", 7000 }, { "mix", 22 } } },
                { "Pi\xC3\xA8" "ce de batterie", { { "mode", 3 }, { "color", 2 }, { "decay", 0.8f }, { "size", 30 }, { "attack", 20 }, { "mix", 25 } } },
                { "Chambre chaude", { { "mode", 4 }, { "color", 1 }, { "decay", 1.4f }, { "size", 40 }, { "damping", 4500 }, { "bassMult", 1.5f } } },
                { "Choral infini", { { "mode", 6 }, { "color", 2 }, { "decay", 14 }, { "size", 90 }, { "modDepth", 75 }, { "modRate", 0.3f }, { "mix", 45 } } },
                { "Ambiance serr\xC3\xA9" "e", { { "mode", 7 }, { "color", 2 }, { "decay", 0.5f }, { "size", 20 }, { "mix", 20 } } },
                { "Plaque lisse brillante", { { "mode", 8 }, { "color", 2 }, { "decay", 2.2f }, { "highCut", 14000 }, { "damping", 10000 } } },
                { "Synth\xC3\xA9 chaotique", { { "mode", 10 }, { "color", 1 }, { "decay", 8 }, { "size", 85 }, { "modDepth", 60 }, { "mix", 40 } } },
                { "Caisse claire coup\xC3\xA9" "e", { { "mode", 11 }, { "color", 1 }, { "decay", 0.45f }, { "size", 55 }, { "mix", 35 } } },
                { "Cath\xC3\xA9" "drale 1970", { { "mode", 0 }, { "color", 0 }, { "decay", 9 }, { "size", 100 }, { "predelay", 45 }, { "highCut", 6000 } } },
            };
            return m;
        }();
        return d;
    }

    /** Caractère de chaque mode : taille, densité d'entrée, modulation, réflexions, diffusion, aigus. */
    struct ModeDef { double size, inDiff, mod, early, lateDiff, bright, random; bool chaos, gate, plate; };
    inline const ModeDef& modeDef (int i)
    {
        static const ModeDef d[12] {
            { 1.45, 1.00, 1.0, 0.25, 1.00, 0.90, 0.0, false, false, false },  // grande salle
            { 1.30, 0.90, 0.9, 0.25, 0.90, 1.25, 0.0, false, false, false },  // salle brillante
            { 0.80, 1.40, 0.8, 0.00, 1.20, 1.20, 0.0, false, false, true  },  // plaque
            { 0.55, 0.80, 0.6, 0.55, 0.70, 0.85, 0.0, false, false, false },  // pièce
            { 0.70, 0.95, 0.5, 0.45, 0.85, 0.80, 0.0, false, false, false },  // chambre
            { 1.10, 1.00, 1.1, 0.30, 0.95, 1.00, 1.0, false, false, false },  // espace aléatoire
            { 1.20, 1.00, 2.6, 0.20, 0.95, 1.00, 0.2, false, false, false },  // espace choral
            { 0.35, 0.60, 0.4, 0.80, 0.50, 0.95, 0.0, false, false, false },  // ambiance
            { 0.85, 1.30, 0.4, 0.00, 1.10, 1.05, 0.0, false, false, true  },  // plaque lisse
            { 0.60, 0.85, 0.3, 0.50, 0.75, 0.95, 0.0, false, false, false },  // pièce lisse
            { 1.40, 1.00, 1.8, 0.20, 1.00, 0.95, 1.4, true,  false, false },  // salle chaotique
            { 1.00, 1.20, 0.6, 0.35, 1.10, 1.00, 0.0, false, true,  false },  // non linéaire
        };
        return d[std::clamp (i, 0, 11)];
    }
}

class TitanVintage final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tvint::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : lines) l.init ((int) (0.25 * sr) + 64);
        for (auto& a : late) a.init ((int) (0.03 * sr) + 64);
        for (auto& a : diffL) a.init ((int) (0.04 * sr) + 64);
        for (auto& a : diffR) a.init ((int) (0.04 * sr) + 64);
        preL.init ((int) (0.52 * sr)); preR.init ((int) (0.52 * sr));
        er.init ((int) (0.12 * sr));
        dcL.prepare (sr, 6.0); dcR.prepare (sr, 6.0);
        reset();
    }

    void reset() noexcept override
    {
        for (auto& l : lines) l.clear();
        for (auto& a : late) a.d.clear();
        for (auto& a : diffL) a.d.clear();
        for (auto& a : diffR) a.d.clear();
        preL.clear(); preR.clear(); er.clear();
        lo.fill (0.0); damp.fill (0.0); walk.fill (0.0);
        for (auto& f : outHc) f.reset();
        for (auto& f : inHc) f.reset();
        dcL.reset(); dcR.reset();
        phase = 0.0; half = false; holdL = holdR = 0.0; smL = smR = 0.0; inAccL = inAccR = 0.0;
        gate = 0.0; gateHold = 0; envIn = 0.0;
    }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace tvint;
        const auto& md = modeDef ((int) p[mode]);
        const int col = std::clamp ((int) p[color], 0, 2);
        const bool vintage = col < 2;
        const bool halfRate = col == 0;
        const double tr = halfRate ? sr * 0.5 : sr;                   // fréquence du réservoir
        const bool frz = p[freeze] > 0.5f;
        const double sz = md.size * (0.3 + 1.4 * p[size] / 100.0);
        static constexpr double lenMs[8] { 31.3, 37.9, 43.1, 47.7, 53.9, 61.3, 67.7, 73.1 };
        const double dec = std::max (0.1, (double) p[decay]) * (md.gate ? 2.5 : 1.0);
        const double bassK = p[bassMult];
        for (int k = 0; k < 8; ++k)
        {
            len[k] = std::clamp (lenMs[k] * sz * 0.001 * tr, 16.0, (double) lines[0].buf.size() / (halfRate ? 1.0 : 1.0) - 64.0);
            const auto t = len[k] / tr;
            gHi[k] = frz ? 1.0 : std::pow (10.0, -3.0 * t / dec);
            gLo[k] = frz ? 1.0 : std::pow (10.0, -3.0 * t / (dec * bassK));
            gLo[k] = std::min (gLo[k], 0.9995);
        }
        xc = std::exp (-kTwoPi * std::min ((double) p[bassXover], 0.45 * tr) / tr);
        dampC = frz ? 0.0 : std::exp (-kTwoPi * std::min ((double) p[damping] * md.bright, 0.45 * tr) / tr);
        // diffusion d'entrée : l'attaque allonge les diffuseurs (montée de la densité plus lente)
        static constexpr double difMs[4] { 4.3, 3.1, 11.9, 8.7 };
        const double atk = 0.4 + 2.2 * p[attack] / 100.0;
        for (int k = 0; k < 4; ++k)
        {
            diffL[(size_t) k].len = std::clamp ((int) (difMs[k] * atk * md.inDiff * 0.001 * tr), 1, (int) diffL[0].d.buf.size() - 8);
            diffR[(size_t) k].len = std::clamp ((int) (difMs[k] * 1.13 * atk * md.inDiff * 0.001 * tr), 1, (int) diffR[0].d.buf.size() - 8);
        }
        const double difG = 0.25 + 0.5 * p[earlyDiff] / 100.0;
        const double lateG = std::clamp (0.6 * p[lateDiff] / 100.0 * md.lateDiff, 0.0, 0.7);
        static constexpr double lateMs[8] { 5.3, 7.1, 6.1, 8.9, 4.7, 9.7, 5.9, 7.7 };
        for (int k = 0; k < 8; ++k) late[(size_t) k].len = std::clamp ((int) (lateMs[k] * std::min (1.6, sz) * 0.001 * tr), 2, (int) late[0].d.buf.size() - 8);
        const double modD = p[modDepth] / 100.0 * md.mod * 0.0012 * tr;
        const double rate = p[modRate];
        const double noiseAmt = vintage ? 0.35 : 0.0;
        const int preN = std::clamp ((int) (p[predelay] * 0.001 * sr), 1, (int) preL.buf.size() - 4);
        const double hcF = std::min ((double) p[highCut], halfRate ? 0.42 * tr : 0.45 * sr);
        for (int c = 0; c < 2; ++c)
        {
            outHc[(size_t) c].set (dsp::SvfEq::lowpass, hcF, 0.6, 0.0, sr);
            inHc[(size_t) c].set (dsp::SvfEq::lowpass, halfRate ? 0.4 * tr : std::min (19000.0, 0.45 * sr), 0.6, 0.0, sr);
        }
        const double wet = p[mix] / 100.0, wd = p[width] / 100.0;
        const double earlyA = md.early * 0.4;
        // porte du mode non linéaire : tenue proportionnelle à la durée, fermeture rapide
        const int holdN = (int) (std::max (0.05, (double) p[decay]) * 0.6 * sr);
        const double gateRel = std::exp (-1.0 / (0.012 * sr)), gateAtt = std::exp (-1.0 / (0.002 * sr));
        const double envRel = std::exp (-1.0 / (0.05 * sr));
        static constexpr double erMs[8] { 6.7, 10.9, 15.3, 21.1, 27.7, 33.1, 41.9, 49.3 };
        static constexpr double erG[8] { 0.85, -0.75, 0.66, -0.58, 0.5, -0.44, 0.37, -0.3 };
        int erN[8]; for (int k = 0; k < 8; ++k) erN[k] = std::clamp ((int) (erMs[k] * std::min (1.8, sz) * 0.001 * sr), 1, (int) er.buf.size() - 4);
        const double lfoInc = rate / tr;

        for (int i = 0; i < n; ++i)
        {
            const double xl = frz ? 0.0 : (double) L[i], xr = frz ? 0.0 : (double) R[i];
            preL.push (xl); preR.push (xr);
            auto il = inHc[0].process (preL.at (preN)), ir = inHc[1].process (preR.at (preN));
            // réflexions premières (au rythme plein)
            er.push (0.5 * (il + ir));
            double eL = 0.0, eR = 0.0;
            for (int k = 0; k < 8; ++k) { const auto v = er.at (erN[k]) * erG[k]; if (k & 1) eR += v; else eL += v; }
            // porte : suiveur de l'entrée
            if (md.gate)
            {
                const auto a = std::max (std::abs (xl), std::abs (xr));
                envIn = a > envIn ? a : a + (envIn - a) * envRel;
                if (envIn > 0.003) gateHold = holdN;
                const double target = gateHold > 0 ? 1.0 : 0.0;
                if (gateHold > 0) --gateHold;
                gate = target + (gate - target) * (target > gate ? gateAtt : gateRel);
            }
            // 1970 : le réservoir ne tourne qu'un échantillon sur deux (entrée moyennée, sortie tenue)
            double yl, yr;
            if (halfRate)
            {
                inAccL += il; inAccR += ir;
                half = ! half;
                if (half) { tank (inAccL * 0.5, inAccR * 0.5, difG, lateG, modD, lfoInc, noiseAmt, md, true); inAccL = inAccR = 0.0; }
                yl = holdL; yr = holdR;
            }
            else
            {
                tank (il, ir, difG, lateG, modD, lfoInc, noiseAmt, md, vintage);
                yl = holdL; yr = holdR;
            }
            // lissage de la sortie tenue (mi-fréquence) : passe-bas d'ordre 1 de plus
            smL += (yl - smL) * (halfRate ? 0.55 : 1.0);
            smR += (yr - smR) * (halfRate ? 0.55 : 1.0);
            auto tl = smL + eL * earlyA, trr = smR + eR * earlyA;
            if (md.gate) { tl *= gate; trr *= gate; }
            const auto mid = 0.5 * (tl + trr), side = 0.5 * (tl - trr) * wd;
            auto ol = outHc[0].process (mid + side), orr = outHc[1].process (mid - side);
            ol = dcL.process (ol); orr = dcR.process (orr);
            L[i] = (float) (L[i] * (1.0 - wet) + ol * wet);
            R[i] = (float) (R[i] * (1.0 - wet) + orr * wet);
        }
    }

private:
    void tank (double il, double ir, double difG, double lateG, double modD, double lfoInc, double noiseAmt, const tvint::ModeDef& md, bool lossy) noexcept
    {
        for (auto& a : diffL) il = a.process (il, difG);
        for (auto& a : diffR) ir = a.process (ir, -difG);
        phase += lfoInc; if (phase >= 1.0) phase -= 1.0;
        double y[8];
        for (int k = 0; k < 8; ++k)
        {
            // modulation : sinus décalés par ligne, + marche aléatoire (espaces aléatoire / chaotique),
            // + bruit d'interpolation des machines d'époque
            double m = modD * (0.5 + 0.5 * dsp::fastSin2Pi (phase + k * 0.137));
            if (md.random > 0.0)
            {
                walk[(size_t) k] += (rnd.next() * 0.0025 - walk[(size_t) k] * 0.0004);
                walk[(size_t) k] = std::clamp (walk[(size_t) k], -1.0, 1.0);
                m += modD * md.random * (0.5 + 0.5 * walk[(size_t) k]);
            }
            if (noiseAmt > 0.0) m += noiseAmt * rnd.next() * (0.2 + modD * 0.02);
            const auto d = len[k] + m + 2.0;
            const auto raw = lossy ? lines[(size_t) k].atFrac (d) : titan::atHermite (lines[(size_t) k], d);
            // déclin à deux bandes + amortissement dans la boucle
            lo[(size_t) k] = raw + (lo[(size_t) k] - raw) * xc;
            auto v = lo[(size_t) k] * gLo[k] + (raw - lo[(size_t) k]) * gHi[k];
            damp[(size_t) k] = v + (damp[(size_t) k] - v) * dampC;
            v = damp[(size_t) k];
            if (md.chaos) v = std::tanh (v * 1.6) * 0.625;           // saturation douce de bande
            y[k] = late[(size_t) k].process (v, (k & 1) ? lateG : -lateG);
        }
        double h[8];
        for (int k = 0; k < 8; ++k) h[k] = y[k];
        titan::hadamard8 (h);
        for (int k = 0; k < 8; ++k)
        {
            const auto in = (k & 1) ? ir : il;
            lines[(size_t) k].push (std::clamp (h[k] + in * 0.45, -8.0, 8.0));
        }
        holdL = 0.5 * (y[0] - y[2] + y[4] - y[6]) + (md.plate ? 0.25 * y[1] : 0.0);
        holdR = 0.5 * (y[1] - y[3] + y[5] - y[7]) + (md.plate ? 0.25 * y[0] : 0.0);
    }

    std::array<DLine, 8> lines;
    std::array<APLine, 8> late;
    std::array<APLine, 4> diffL, diffR;
    DLine preL, preR, er;
    std::array<double, 8> lo {}, damp {}, walk {};
    double len[8] {}, gLo[8] {}, gHi[8] {};
    double xc = 0.0, dampC = 0.0;
    std::array<dsp::SvfEq, 2> outHc, inHc;
    dsp::DcBlocker dcL, dcR;
    titan::Rand rnd;
    double sr = 48000.0, phase = 0.0, holdL = 0.0, holdR = 0.0, smL = 0.0, smR = 0.0, inAccL = 0.0, inAccR = 0.0;
    bool half = false;
    double gate = 0.0, envIn = 0.0;
    int gateHold = 0;
};

//==============================================================================
// Titan Espace
namespace tspace
{
    enum Slot { style = 0, space, decayPct, distance, brightness, character, width, mix,
                predelay, sync, ducking, freeze,
                dLowFreq, dLowMult, dMidFreq, dMidMult, dHighFreq, dHighMult,
                lowCut, highCut, lowShelf, highShelf, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "tspace"; m.name = "Titan Espace"; m.role = "R\xC3\xA9verb\xC3\xA9ration de mixage"; m.family = titan::family; m.form = "module";
            m.principle = "Un bouton Espace (pi\xC3\xA8" "ce \xE2\x86\x92 grande salle), dur\xC3\xA9" "e r\xC3\xA9gl\xC3\xA9" "e par bande dans la boucle (grave, m\xC3\xA9" "dium, aigu), \xC3\xA9galiseur de sortie, ducking ; sans latence";
            m.params = {
                C ("style", "Style", { "Moderne", "Vintage", "Plaque" }),
                P ("space", "Espace", "%", 0, 100, 50),
                P ("decayPct", "Dur\xC3\xA9" "e", "%", 25, 400, 100, 0.5f),
                P ("distance", "Distance", "%", 0, 100, 50),
                P ("brightness", "Brillance", "%", 0, 100, 50),
                P ("character", "Caract\xC3\xA8re", "%", 0, 100, 30),
                P ("width", "Largeur", "%", 0, 150, 100),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 25),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 500, 0, 0.5f),
                C ("sync", "Pr\xC3\xA9-d\xC3\xA9lai au tempo", titan::syncChoices()),
                P ("ducking", "Ducking", "%", 0, 100, 0),
                C ("freeze", "Gel", { "Non", "Oui" }),
                P ("dLowFreq", "Grave : fr\xC3\xA9quence", "Hz", 40, 1000, 200, 0.4f),
                P ("dLowMult", "Grave : dur\xC3\xA9" "e", "%", 25, 400, 110, 0.5f),
                P ("dMidFreq", "M\xC3\xA9" "dium : fr\xC3\xA9quence", "Hz", 250, 6000, 1200, 0.4f),
                P ("dMidMult", "M\xC3\xA9" "dium : dur\xC3\xA9" "e", "%", 25, 400, 100, 0.5f),
                P ("dHighFreq", "Aigu : fr\xC3\xA9quence", "Hz", 1500, 16000, 6000, 0.4f),
                P ("dHighMult", "Aigu : dur\xC3\xA9" "e", "%", 25, 400, 70, 0.5f),
                P ("lowCut", "Coupe-bas", "Hz", 20, 1000, 60, 0.4f),
                P ("highCut", "Coupe-haut", "Hz", 1000, 20000, 18000, 0.4f),
                P ("lowShelf", "Plateau grave", "dB", -12, 12, 0),
                P ("highShelf", "Plateau aigu", "dB", -12, 12, 0),
            };
            m.sections = {
                { "space", "Espace", "reverb", { "style", "space", "decayPct", "distance", "brightness", "character", "width", "mix" }, "global" },
                { "time", "Temps", "reverb", { "predelay", "sync", "ducking", "freeze" }, "global" },
                { "decayeq", "Dur\xC3\xA9" "e par bande", "global", { "dLowFreq", "dLowMult", "dMidFreq", "dMidMult", "dHighFreq", "dHighMult" }, "global" },
                { "posteq", "\xC3\x89galiseur de sortie", "global", { "lowCut", "highCut", "lowShelf", "highShelf" }, "global" },
            };
            m.presets = {
                { "Salle moyenne", {} },
                { "Petite pi\xC3\xA8" "ce de voix", { { "space", 22 }, { "distance", 35 }, { "brightness", 60 }, { "predelay", 20 }, { "lowCut", 180 }, { "mix", 18 } } },
                { "Grande salle sombre", { { "space", 85 }, { "decayPct", 140 }, { "brightness", 30 }, { "dHighMult", 50 }, { "dLowMult", 140 }, { "mix", 30 } } },
                { "Plaque de caisse claire", { { "style", 2 }, { "space", 40 }, { "brightness", 70 }, { "distance", 70 }, { "mix", 30 } } },
                { "Vintage des ann\xC3\xA9" "es 80", { { "style", 1 }, { "space", 60 }, { "character", 55 }, { "brightness", 55 } } },
                { "Voix avec ducking", { { "space", 55 }, { "predelay", 40 }, { "ducking", 60 }, { "lowCut", 250 }, { "mix", 28 } } },
                { "Cath\xC3\xA9" "drale", { { "space", 100 }, { "decayPct", 250 }, { "distance", 85 }, { "dHighMult", 45 }, { "mix", 35 } } },
                { "Nappe gel\xC3\xA9" "e", { { "space", 90 }, { "decayPct", 400 }, { "character", 70 }, { "mix", 50 } } },
            };
            return m;
        }();
        return d;
    }
}

class TitanSpace final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return tspace::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        for (auto& l : lines) l.init ((int) (0.3 * sr) + 64);
        for (auto& a : diff) a.init ((int) (0.04 * sr) + 64);
        for (auto& a : late) a.init ((int) (0.03 * sr) + 64);
        pre.init ((int) (2.2 * sr));                 // 500 ms libre, ou une noire à 30 BPM
        er.init ((int) (0.16 * sr));
        duck.set (2.0, 250.0, sr);
        dcL.prepare (sr, 6.0); dcR.prepare (sr, 6.0);
        reset();
    }

    void reset() noexcept override
    {
        for (auto& l : lines) l.clear();
        for (auto& a : diff) a.d.clear();
        for (auto& a : late) a.d.clear();
        pre.clear(); er.clear();
        for (auto& b : bands) for (auto& f : b) f.reset();
        for (auto& f : post) f.reset();
        duck.reset();
        phase = 0.0; dcL.reset(); dcR.reset();
    }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace tspace;
        const int st = std::clamp ((int) p[style], 0, 2);
        const bool frz = p[freeze] > 0.5f;
        const double sp = p[space] / 100.0;
        // Espace : taille et durée naturelle (petite pièce 0,3 s → grande salle 6 s)
        const double sz = (st == 2 ? 0.55 : 1.0) * (0.25 + 1.95 * sp * sp + 0.15 * sp);
        const double natural = 0.3 * std::pow (20.0, sp);
        const double baseDecay = std::max (0.08, natural * p[decayPct] / 100.0);
        const double bright = p[brightness] / 100.0;
        // brillance : agit aussi sur la durée des aigus
        const double hiMul = p[dHighMult] / 100.0 * (0.45 + 1.1 * bright);
        static constexpr double lenMs[8] { 29.3, 34.7, 39.1, 44.9, 50.3, 56.9, 62.3, 68.9 };
        for (int k = 0; k < 8; ++k)
        {
            len[k] = std::clamp (lenMs[k] * sz * 0.001 * sr, 12.0, (double) lines[0].buf.size() - 64.0);
            const auto t = len[k] / sr;
            const auto g = [&] (double mult) { return frz ? 0.0 : -60.0 * t / (baseDecay * mult); }; // dB par passage
            const auto gMid = g (p[dMidMult] / 100.0);
            const auto gBase = g (1.0);
            const auto gLo = g (p[dLowMult] / 100.0);
            const auto gHi = g (hiMul);
            base[k] = frz ? 1.0 : std::pow (10.0, gBase / 20.0);
            auto& b = bands[(size_t) k];
            b[0].set (dsp::SvfEq::lowShelf, p[dLowFreq], 0.7, std::clamp (gLo - gBase, -24.0, 12.0), sr);
            b[1].set (dsp::SvfEq::bell, p[dMidFreq], 0.8, std::clamp (gMid - gBase, -24.0, 12.0), sr);
            b[2].set (dsp::SvfEq::highShelf, std::min ((double) p[dHighFreq], 0.45 * sr), 0.7, std::clamp (gHi - gBase, -24.0, 12.0), sr);
        }
        const double ch = p[character] / 100.0;
        const double modD = (st == 1 ? 1.6 : 1.0) * (ch < 0.6 ? ch * 0.9 : 0.54 + (ch - 0.6) * 4.0) * 0.0006 * sr;
        const double rate = 0.25 + ch * 1.4;
        static constexpr double difMs[4] { 3.7, 2.9, 9.1, 6.7 };
        for (int k = 0; k < 4; ++k) diff[(size_t) k].len = std::clamp ((int) (difMs[k] * std::min (1.6, 0.6 + sz) * 0.001 * sr), 1, (int) diff[0].d.buf.size() - 8);
        const double difG = st == 2 ? 0.72 : st == 1 ? 0.5 : 0.62;
        static constexpr double lateMs[8] { 4.9, 6.7, 5.7, 8.3, 4.3, 9.1, 5.3, 7.3 };
        for (int k = 0; k < 8; ++k) late[(size_t) k].len = std::clamp ((int) (lateMs[k] * std::min (1.5, 0.5 + sz) * 0.001 * sr), 2, (int) late[0].d.buf.size() - 8);
        const double lateG = st == 2 ? 0.55 : st == 1 ? 0.3 : 0.45;
        // pré-délai (au tempo si demandé) + pré-délai implicite de la distance (proche : plus long)
        const double dist = p[distance] / 100.0;
        double preMs = p[predelay];
        if (const auto q = titan::syncQuarters ((int) p[sync]); q > 0.0 && bpm > 1.0) preMs = q * 60000.0 / bpm;
        preMs += (1.0 - dist) * 12.0;
        const int preN = std::clamp ((int) (preMs * 0.001 * sr), 1, (int) pre.buf.size() - 4);
        const double earlyA = (st == 2 ? 0.0 : 1.0) * (0.15 + 0.55 * (1.0 - dist));
        const double lateA = 0.45 + 0.55 * dist;
        // égaliseur de sortie (+ la brillance règle aussi le niveau des aigus, + style vintage : bande réduite)
        const double hc = std::min ((double) p[highCut] * (st == 1 ? 0.65 : 1.0), 0.45 * sr);
        for (int c = 0; c < 2; ++c)
        {
            post[(size_t) (c * 4 + 0)].set (dsp::SvfEq::highpass, p[lowCut], 0.7, 0.0, sr);
            post[(size_t) (c * 4 + 1)].set (dsp::SvfEq::lowpass, hc, 0.7, 0.0, sr);
            post[(size_t) (c * 4 + 2)].set (dsp::SvfEq::lowShelf, 200.0, 0.7, p[lowShelf], sr);
            post[(size_t) (c * 4 + 3)].set (dsp::SvfEq::highShelf, std::min (6000.0, 0.4 * sr), 0.7, p[highShelf] + (bright - 0.5) * 8.0, sr);
        }
        const double wet = p[mix] / 100.0, wd = p[width] / 100.0, dk = p[ducking] / 100.0;
        static constexpr double erMs[10] { 4.3, 7.9, 11.3, 16.1, 19.7, 25.3, 31.1, 38.9, 47.3, 57.1 };
        static constexpr double erG[10] { 0.9, -0.8, 0.72, -0.64, 0.56, -0.49, 0.42, -0.36, 0.3, -0.25 };
        int erN[10]; for (int k = 0; k < 10; ++k) erN[k] = std::clamp ((int) (erMs[k] * (0.5 + sz) * 0.001 * sr), 1, (int) er.buf.size() - 4);
        const double lfoInc = rate / sr;
        for (int i = 0; i < n; ++i)
        {
            const double xl = L[i], xr = R[i];
            const auto mono = frz ? 0.0 : 0.5 * (xl + xr);
            const auto side = frz ? 0.0 : 0.5 * (xl - xr);
            pre.push (mono);
            auto v = pre.at (preN);
            er.push (v);
            double eL = 0.0, eR = 0.0;
            for (int k = 0; k < 10; ++k) { const auto t = er.at (erN[k]) * erG[k]; if (k & 1) eR += t; else eL += t; }
            for (auto& a : diff) v = a.process (v, difG);
            phase += lfoInc; if (phase >= 1.0) phase -= 1.0;
            double y[8];
            for (int k = 0; k < 8; ++k)
            {
                const double m = modD * (0.5 + 0.5 * dsp::fastSin2Pi (phase * (1.0 + 0.07 * k) + k * 0.19));
                auto raw = st == 1 ? lines[(size_t) k].atFrac (len[k] + m + 2.0) : titan::atHermite (lines[(size_t) k], len[k] + m + 2.0);
                raw *= base[k];
                auto& b = bands[(size_t) k];
                raw = b[2].process (b[1].process (b[0].process (raw)));
                y[k] = late[(size_t) k].process (raw, (k & 1) ? lateG : -lateG);
            }
            double h[8];
            for (int k = 0; k < 8; ++k) h[k] = y[k];
            titan::hadamard8 (h);
            for (int k = 0; k < 8; ++k) lines[(size_t) k].push (std::clamp (h[k] + (k < 4 ? v : -v) * 0.4 + ((k & 1) ? side : -side) * 0.2, -8.0, 8.0));
            auto tl = 0.5 * (y[0] - y[2] + y[4] - y[6]) * lateA + eL * earlyA * 0.4;
            auto trr = 0.5 * (y[1] - y[3] + y[5] - y[7]) * lateA + eR * earlyA * 0.4;
            const auto mid = 0.5 * (tl + trr), sd = 0.5 * (tl - trr) * wd;
            double ol = mid + sd, orr = mid - sd;
            for (int f = 0; f < 4; ++f) { ol = post[(size_t) f].process (ol); orr = post[(size_t) (4 + f)].process (orr); }
            ol = dcL.process (ol); orr = dcR.process (orr);
            // ducking : la queue se retire sous le son direct
            const auto env = duck.process (std::max (std::abs (xl), std::abs (xr)));
            const double dg = dk > 0.0 ? 1.0 - dk * std::clamp ((dsp::gainToDb (env) + 40.0) / 34.0, 0.0, 1.0) : 1.0;
            L[i] = (float) (xl * (1.0 - wet) + ol * wet * dg);
            R[i] = (float) (xr * (1.0 - wet) + orr * wet * dg);
        }
    }

private:
    std::array<DLine, 8> lines;
    std::array<APLine, 4> diff;
    std::array<APLine, 8> late;
    DLine pre, er;
    std::array<std::array<dsp::SvfEq, 3>, 8> bands;
    std::array<dsp::SvfEq, 8> post;
    dsp::EnvelopeFollower duck;
    dsp::DcBlocker dcL, dcR;
    double len[8] {}, base[8] {};
    double sr = 48000.0, phase = 0.0;
};

} // namespace grv::effects
