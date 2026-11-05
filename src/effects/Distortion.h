/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Distorsion » — principe des distorsions créatives à deux moteurs : deux étages de
    saturation (14 algorithmes chacun : bande, lampe, transistor, transformateur, redresseur,
    germanium, fuzz, réduction de bits, inversion de bits, repli d'onde, façonneur, diode, repli
    triangle, écrêtage asymétrique) avec asymétrie réglable (harmoniques paires), cinq routages
    (série, parallèle, stéréo, mid/side, par bandes), filtre d'entrée (passe-bas, passe-haut,
    passe-bande, coupe-bande, peigne), correction de sortie graves/aigus, dynamique avant ou après
    (compresseur, multibande, limiteur), réinjection accordable. Les moteurs tournent suréchantillonnés
    ×4 depuis la v0.22 (×2 avant ; deux étages de filtres demi-bande IIR : aucune latence).
    v0.36 (demande d'Henrico, principe Dist COLDFIRE regardé de près) : les filtres n'agissent plus que
    sur le son distordu — filtre d'entrée propre à chaque moteur (avant sa courbe), passe-haut et
    passe-bas de sortie propres à chaque moteur ; la part non distordue (mélange du moteur, mélange
    général) reste intacte. 4 modulateurs internes (LFO, suiveur, fonction, séquenceur 16 pas ; libres
    ou au tempo) branchés sur le drive, les filtres, l'asymétrie, la réinjection… : la distorsion bouge.
    Architecture propre.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../dsp/Oversampler.h"
#include "../engine/Module.h"
#include "FxMod.h"
#include "FxBanks.h"

#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace dist
{
    enum Slot
    {
        driveA = 0, typeA, toneA, mixA, driveB, typeB, toneB, mixB, routing, balance, mix, out, xover,
        preType, preFreq, preReso, postLow, postHigh,
        dynMode, dynPos, dynAmount,
        feedback, fbTime, fbPitch,
        preTypeB, preFreqB, preResoB,
        biasA, biasB, // v0.8 : asymétrie de chaque moteur
        postHpA, postLpA, postHpB, postLpB, // v0.36 : filtres de sortie de chaque moteur
        modBase, // v0.36 : 4 modulateurs × 6 réglages (FxMod.h)
        numSlots = modBase + 4 * 6
    };
    constexpr int numMods = 4;
    /** Destinations des modulateurs (réglages continus) : libellés et réglages atteints. */
    inline const std::vector<std::string>& destNames()
    {
        static const std::vector<std::string> d { "Drive A", "Drive B", "Tonalit\xC3\xA9 A", "Tonalit\xC3\xA9 B", "M\xC3\xA9lange A", "M\xC3\xA9lange B",
            "Asym\xC3\xA9trie A", "Asym\xC3\xA9trie B", "Filtre A fr\xC3\xA9q.", "Filtre A r\xC3\xA9so.", "Filtre B fr\xC3\xA9q.",
            "Filtre B r\xC3\xA9so.", "Passe-haut A", "Passe-bas A", "Passe-haut B", "Passe-bas B", "\xC3\x89quilibre A/B", "R\xC3\xA9injection",
            "Temps r\xC3\xA9inj.", "M\xC3\xA9lange", "Sortie" };
        return d;
    }
    inline const int* destSlots()
    {
        static const int d[] { driveA, driveB, toneA, toneB, mixA, mixB, biasA, biasB, preFreq, preReso, preFreqB, preResoB,
                               postHpA, postLpA, postHpB, postLpB, balance, feedback, fbTime, mix, out };
        return d;
    }

    inline const std::vector<std::string>& algorithms()
    {
        static const std::vector<std::string> a { "Bande", "Lampe", "Transistor", "Transformateur", "Redresseur", "Germanium",
                                                  "Fuzz", "R\xC3\xA9" "duction de bits", "Inversion de bits", "Repli d'onde", "Fa\xC3\xA7onneur",
                                                  "Diode", "Repli triangle", "\xC3\x89" "cr\xC3\xAAtage asym\xC3\xA9trique" };
        return a;
    }

    /** Nombre d'algorithmes avant la v0.8 (projets enregistrés : voir PluginProcessor, version 3). */
    constexpr int algorithmsV1 = 11;

    /** Fonction de transfert d'un algorithme (la même formule sert à la courbe de l'interface).
        bias : décalage avant la courbe (asymétrie, harmoniques paires), retiré à la sortie. */
    inline double shapeRaw (int type, double v, double drive01) noexcept;
    inline double shape (int type, double x, double drive01, double bias = 0.0) noexcept
    {
        const auto g = 1.0 + 63.0 * drive01 * drive01;
        if (bias == 0.0) return shapeRaw (type, x * g, drive01);
        return shapeRaw (type, x * g + bias, drive01) - shapeRaw (type, bias, drive01);
    }
    inline double shapeRaw (int type, double v, double drive01) noexcept
    {
        switch (type)
        {
            case 0: return fastTanh (v * 0.7);                                        // bande : douce
            case 1: { const auto t = fastTanh (v); return fastTanh (v + 0.35 * t * t); }          // lampe : asymétrique, monotone
            case 2: { const auto v2 = v * v; return v / std::sqrt (std::sqrt (1.0 + v2 * v2)); } // transistor : plus dur
            case 3: return fastTanh (v) * 0.9 + 0.1 * fastSin2Pi (v * 0.15915494309189535); // transformateur
            case 4: return std::abs (fastTanh (v)) * 2.0 - 1.0;                       // redresseur
            case 5: return v > 0.0 ? fastTanh (v * 1.2) : fastTanh (v * 0.6);         // germanium
            case 6: return std::clamp (v * 2.0, -1.0, 1.0);                           // fuzz
            case 7: { const auto steps = std::floor (16.5 - drive01 * 14.0); return std::floor (fastTanh (v) * steps + 0.5) / steps; } // bits
            case 8: { const auto y = fastTanh (v); return std::abs (y) > 0.6 ? -y * 0.6 : y; } // inversion de bits
            case 9: return fastSin2Pi (v * 0.9 * 0.15915494309189535);                // repli d'onde
            case 11: return v >= 0.0 ? 1.0 - std::exp (-v * 1.1) : -0.55 * (1.0 - std::exp (v * 1.4)); // diode : conduction d'un seul côté
            case 12: { const auto a = v * 0.25 + 0.25; const auto f = a - std::floor (a); return 1.0 - 4.0 * std::abs (f - 0.5); } // repli triangle
            case 13: return std::clamp (v * 1.5, -0.45, 1.0);                         // écrêtage asymétrique (−0,45 / +1)
            default: { const auto v3 = v * v * v; return fastTanh (v - 0.3 * v3 / (1.0 + std::abs (v3))); } // façonneur
        }
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            std::vector<std::string> pitches { "Libre" };
            for (int i = -12; i <= 12; ++i) pitches.push_back ((i > 0 ? "+" : "") + std::to_string (i) + " dt");
            ModuleDesc m;
            m.type = "dist";
            m.name = "Furnace"; m.role = "Distorsion";
            m.family = "Saturation"; m.form = "module";
            m.principle = "Deux moteurs de saturation, 14 algorithmes, asym\xC3\xA9trie, 5 routages, filtres, dynamique et r\xC3\xA9injection";
            m.params = {
                P ("driveA", "Drive A", "%", 0, 100, 45), C ("typeA", "Type A", algorithms(), 1),
                P ("toneA", "Tonalit\xC3\xA9 A", "%", 0, 100, 50), P ("mixA", "M\xC3\xA9lange A", "%", 0, 100, 100),
                P ("driveB", "Drive B", "%", 0, 100, 30), C ("typeB", "Type B", algorithms(), 9),
                P ("toneB", "Tonalit\xC3\xA9 B", "%", 0, 100, 50), P ("mixB", "M\xC3\xA9lange B", "%", 0, 100, 0),
                C ("routing", "Routage", { "S\xC3\xA9rie", "Parall\xC3\xA8le", "St\xC3\xA9r\xC3\xA9o", "Mid / Side", "Par bandes" }),
                P ("balance", "\xC3\x89quilibre A/B", "%", 0, 100, 50),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100),
                P ("out", "Sortie", "dB", -24, 12, 0),
                P ("xover", "S\xC3\xA9paration", "Hz", 60, 4000, 300, 0.4f),
                C ("preType", "Filtre A", { "Aucun", "Passe-bas", "Passe-haut", "Passe-bande", "Coupe-bande", "Peigne" }),
                P ("preFreq", "Fr\xC3\xA9quence A", "Hz", 40, 16000, 1200, 0.3f),
                P ("preReso", "R\xC3\xA9sonance A", "%", 0, 100, 20),
                P ("postLow", "Sortie \xC2\xB7 graves", "dB", -12, 12, 0),
                P ("postHigh", "Sortie \xC2\xB7 aigus", "dB", -12, 12, 0),
                C ("dynMode", "Mode", { "Compresseur", "Multibande", "Limiteur" }),
                C ("dynPos", "Position", { "Avant saturation", "Apr\xC3\xA8s saturation" }, 1), // v0.46 fix30 : libellés sans ambiguïté (traduction)
                P ("dynAmount", "Quantit\xC3\xA9", "%", 0, 100, 0),
                P ("feedback", "R\xC3\xA9injection", "%", 0, 100, 0),
                P ("fbTime", "Temps", "ms", 0.1f, 40, 4, 0.5f),
                C ("fbPitch", "Accord", pitches),
                C ("preTypeB", "Filtre B", { "Aucun", "Passe-bas", "Passe-haut", "Passe-bande", "Coupe-bande" }),
                P ("preFreqB", "Fr\xC3\xA9quence B", "Hz", 40, 16000, 1200, 0.3f),
                P ("preResoB", "R\xC3\xA9sonance B", "%", 0, 100, 20),
                P ("biasA", "Asym\xC3\xA9trie A", "%", -100, 100, 0),
                P ("biasB", "Asym\xC3\xA9trie B", "%", -100, 100, 0),
                P ("postHpA", "Passe-haut A", "Hz", 20, 2000, 20, 0.3f),
                P ("postLpA", "Passe-bas A", "Hz", 500, 20000, 20000, 0.3f),
                P ("postHpB", "Passe-haut B", "Hz", 20, 2000, 20, 0.3f),
                P ("postLpB", "Passe-bas B", "Hz", 500, 20000, 20000, 0.3f),
            };
            fxmod::addSlots (m, numMods, destNames(), 0);
            m.sections = {
                { "engines", "Moteurs", "dist", { "driveA", "typeA", "toneA", "driveB", "typeB", "toneB", "routing", "balance" }, "engines" },
                { "levels", "Niveaux", "dist", { "mixA", "mixB", "mix", "out", "xover", "biasA", "biasB" }, "engines" },
                { "filters", "Filtres d'entr\xC3\xA9" "e", "distfilter", { "preType", "preFreq", "preReso", "preTypeB", "preFreqB", "preResoB" }, "filters" },
                { "postf", "Filtres de sortie", "distpost", { "postHpA", "postLpA", "postHpB", "postLpB" }, "filters" },
                { "post", "Sortie", "distpost", { "postLow", "postHigh" }, "filters" },
                { "dynamics", "Dynamique", "comp", { "dynMode", "dynPos", "dynAmount" }, "dynamics" },
                { "feedback", "R\xC3\xA9injection", "distfb", { "feedback", "fbTime", "fbPitch" }, "feedback" },
            };
            m.presets = {
                { "Chaleur", { { "driveA", 25 }, { "typeA", 0 }, { "mix", 100 } } },
                { "Lampe satur\xC3\xA9" "e", {} },
                { "D\xC3\xA9" "chirure", { { "driveA", 85 }, { "typeA", 6 }, { "driveB", 60 }, { "typeB", 2 }, { "mixB", 100 }, { "dynAmount", 50 }, { "out", -6 } } },
                { "Repli m\xC3\xA9tallique", { { "driveA", 40 }, { "typeA", 9 }, { "feedback", 35 }, { "fbPitch", 13 }, { "preType", 2 }, { "preFreq", 200 } } },
                { "Basse par bandes", { { "routing", 4 }, { "xover", 180 }, { "driveA", 15 }, { "typeA", 0 }, { "driveB", 70 }, { "typeB", 6 }, { "mixB", 100 } } },
                { "Lo-fi", { { "typeA", 7 }, { "driveA", 55 }, { "toneA", 30 }, { "postHigh", -6 } } },
                { "Batterie \xC3\xA9" "cras\xC3\xA9" "e", { { "typeA", 2 }, { "driveA", 60 }, { "dynMode", 0 }, { "dynPos", 1 }, { "dynAmount", 70 }, { "mix", 60 } } },
                { "Diode chaude", { { "typeA", 11 }, { "driveA", 40 }, { "biasA", 25 }, { "toneA", 40 } } },
                { "Repli vivant", { { "typeA", 12 }, { "driveA", 35 }, { "biasA", -30 }, { "preType", 1 }, { "preFreq", 3000 } } },
                { "Fuzz asym\xC3\xA9trique", { { "typeA", 13 }, { "driveA", 70 }, { "biasA", 40 }, { "out", -4 } } },
                // v0.36 : distorsions qui bougent (modulateurs internes)
                { "Drive qui respire", { { "typeA", 1 }, { "driveA", 45 }, { "m1type", 1 }, { "m1div", 7 }, { "m1shape", 0 }, { "m1amt", 40 }, { "m1dest", 0 } } },
                { "Filtre au suiveur", { { "typeA", 2 }, { "driveA", 55 }, { "preType", 1 }, { "preFreq", 600 }, { "preReso", 55 },
                                         { "m1type", 2 }, { "m1rate", 4 }, { "m1amt", 60 }, { "m1dest", 8 } } },
                { "Hachure en 1/16", { { "typeA", 6 }, { "driveA", 65 }, { "m1type", 4 }, { "m1div", 9 }, { "m1shape", 4 }, { "m1amt", -80 }, { "m1dest", 19 }, { "out", -3 } } },
                { "Repli s\xC3\xA9quenc\xC3\xA9", { { "typeA", 9 }, { "driveA", 30 }, { "m1type", 4 }, { "m1div", 8 }, { "m1shape", 5 }, { "m1amt", 50 }, { "m1dest", 0 },
                                         { "m2type", 1 }, { "m2div", 5 }, { "m2shape", 1 }, { "m2amt", 30 }, { "m2dest", 13 } } },
                { "Attaque mordante", { { "typeA", 1 }, { "driveA", 25 }, { "m1type", 3 }, { "m1rate", 6 }, { "m1shape", 3 }, { "m1amt", 60 }, { "m1dest", 0 } } },
                { "Deux moteurs crois\xC3\xA9s", { { "routing", 1 }, { "typeA", 1 }, { "driveA", 40 }, { "typeB", 7 }, { "driveB", 60 }, { "mixB", 100 },
                                                  { "m1type", 1 }, { "m1div", 6 }, { "m1shape", 1 }, { "m1amt", 90 }, { "m1dest", 16 },
                                                  { "postLpB", 4000 }, { "postHpB", 300 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Distortion final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return dist::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        fbBuf.assign ((size_t) (0.05 * sr) + 8, 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        for (auto& c : ch) c = {};
        mods.reset();
        std::fill (fbBuf.begin(), fbBuf.end(), 0.0f);
        fbW = 0;
        dynEnv = 0.0;
        for (auto& e : bandEnv) e = 0.0;
        inMeter = outMeter = grMeter = 0.0f;
        wetPrev = outPrev = -1.0;
    }

    float meter (int i) const noexcept override
    {
        if (i >= 3 && i < 3 + dist::numMods) return (float) mods.value (i - 3); // v0.36 : modulateurs (écran du module)
        return i == 0 ? inMeter : (i == 1 ? outMeter : grMeter);
    }
    void setPosition (double ppq, bool playing) noexcept override { hostPpq = ppq; hostPlaying = playing; }

    void process (float* L, float* R, int n, const float* pIn, double bpm) noexcept override
    {
        using namespace dist;
        // modulateurs internes : copie modulée des réglages pour ce bloc
        float inPkBlock = 0.0f;
        for (int i = 0; i < n; ++i) inPkBlock = std::max (inPkBlock, std::max (std::abs (L[i]), std::abs (R[i])));
        float pm[numSlots];
        mods.apply (desc(), pIn, pm, numSlots, modBase, destSlots(), (int) destNames().size(), n, sr, bpm, hostPpq, hostPlaying, inPkBlock);
        const float* p = pm;

        const auto tA = std::clamp ((int) p[typeA], 0, 13), tB = std::clamp ((int) p[typeB], 0, 13);
        bA = p[biasA] / 100.0 * 0.6; bB = p[biasB] / 100.0 * 0.6;
        const auto dA = p[driveA] / 100.0, dB = p[driveB] / 100.0;
        const auto route = (int) p[routing];
        const auto bal = p[balance] / 100.0;
        const auto wet = p[mix] / 100.0;
        const auto outGain = dbToGain (p[out]);
        // compensation partielle du gain de drive (le niveau reste exploitable à fond)
        mixA_ = p[mixA] / 100.0; mixB_ = p[mixB] / 100.0;
        compA = 1.0 / (1.0 + 0.5 * dA); compB = 1.0 / (1.0 + 0.5 * dB);
        typeA_ = tA; typeB_ = tB; driveA_ = dA; driveB_ = dB;

        // Filtres : chacun sur l'entrée de SON moteur seulement (v0.36)
        preAType = std::clamp ((int) p[preType], 0, 5);
        static constexpr SvfEq::Type preMap[6] { SvfEq::allpass, SvfEq::lowpass, SvfEq::highpass, SvfEq::bandpass, SvfEq::notch, SvfEq::allpass };
        const auto q = 0.5 + 9.5 * (p[preReso] / 100.0) * (p[preReso] / 100.0);
        preBType = std::clamp ((int) p[preTypeB], 0, 4);
        const auto qB = 0.5 + 9.5 * (p[preResoB] / 100.0) * (p[preResoB] / 100.0);
        const auto tiltA = (p[toneA] - 50.0) / 50.0 * 9.0, tiltB = (p[toneB] - 50.0) / 50.0 * 9.0;
        toneOnA = std::abs (tiltA) > 0.05; toneOnB = std::abs (tiltB) > 0.05;
        hpOnA = p[postHpA] > 21.0f; lpOnA = p[postLpA] < 19900.0f; hpOnB = p[postHpB] > 21.0f; lpOnB = p[postLpB] < 19900.0f;
        const auto nyq = 0.45 * sr;
        for (auto& c : ch)
        {
            if (preAType >= 1 && preAType <= 4) c.pre.set (preMap[preAType], std::min ((double) p[preFreq], nyq), q, 0.0, sr);
            if (preBType >= 1) c.preB.set (preMap[preBType], std::min ((double) p[preFreqB], nyq), qB, 0.0, sr);
            c.postLow.set (SvfEq::lowShelf, 180.0, 0.7, p[postLow], sr);
            c.postHigh.set (SvfEq::highShelf, 4500.0, 0.7, p[postHigh], sr);
            c.toneA_lo.set (SvfEq::lowShelf, 1400.0, 0.6, -tiltA, osFactor * sr);
            c.toneA_hi.set (SvfEq::highShelf, 1400.0, 0.6, tiltA, osFactor * sr);
            c.toneB_lo.set (SvfEq::lowShelf, 1400.0, 0.6, -tiltB, osFactor * sr);
            c.toneB_hi.set (SvfEq::highShelf, 1400.0, 0.6, tiltB, osFactor * sr);
            c.xLow.set (SvfEq::lowpass, p[xover], 0.5, 0.0, sr);
            c.xHigh.set (SvfEq::highpass, p[xover], 0.5, 0.0, sr);
            if (hpOnA) c.hpA.set (SvfEq::highpass, p[postHpA], 0.7071, 0.0, sr);
            if (lpOnA) c.lpA.set (SvfEq::lowpass, std::min ((double) p[postLpA], nyq), 0.7071, 0.0, sr);
            if (hpOnB) c.hpB.set (SvfEq::highpass, p[postHpB], 0.7071, 0.0, sr);
            if (lpOnB) c.lpB.set (SvfEq::lowpass, std::min ((double) p[postLpB], nyq), 0.7071, 0.0, sr);
        }
        // Réinjection : temps libre ou accordé (période de la note, La 220 Hz ± demi-tons)
        const auto pitchIdx = (int) p[fbPitch];
        const auto fbMs = pitchIdx <= 0 ? (double) p[fbTime] : 1000.0 / (220.0 * std::pow (2.0, (pitchIdx - 13) / 12.0));
        // v0.22 : le retard propre du suréchantillonneur (4,6 échantillons) est retiré du temps
        const auto fbDelay = std::clamp (fbMs * 0.001 * sr - osDelaySamples, 1.0, (double) fbBuf.size() / 2.0 - 4.0);
        const auto fbAmt = 0.95 * p[feedback] / 100.0;
        const auto dynMode_ = (int) p[dynMode];
        const auto dynBefore = p[dynPos] < 0.5f;
        const auto dynAmt = p[dynAmount] / 100.0;
        combDelay = std::clamp (sr / std::max (40.0, (double) p[preFreq]), 1.0, (double) combSize - 2.0);

        // v0.22 : mélange et sortie en rampe sur le bloc (un réglage tourné ou modulé ne fait plus de marche)
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        const auto out0 = outPrev < 0.0 ? outGain : outPrev, dOut = (outGain - out0) / std::max (1, n);
        wetPrev = wet; outPrev = outGain;
        const bool post = p[postLow] != 0.0f || p[postHigh] != 0.0f;
        float inPk = 0.0f, outPk = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1), og = out0 + dOut * (double) (i + 1);
            double x[2] { L[i], R[i] };
            // signal direct retardé comme le chemin saturé (même suréchantillonneur, sans courbe)
            double dry[2];
            for (int c = 0; c < 2; ++c) dry[c] = delayAligned (ch[c].dryOs, x[c]);
            inPk = std::max (inPk, (float) std::max (std::abs (x[0]), std::abs (x[1])));

            if (dynAmt > 0.0 && dynBefore) dynamics (x, dynMode_, dynAmt);

            // Réinjection depuis la sortie retardée
            if (fbAmt > 0.0)
            {
                const auto fb = readFb (fbDelay);
                x[0] = x[0] + fbAmt * fb.first;
                x[1] = x[1] + fbAmt * fb.second;
            }

            double y[2];
            route2x (x, y, route, bal);

            // garde-fou de la boucle de réinjection
            pushFb (fastTanh (y[0]), fastTanh (y[1]));

            for (int c = 0; c < 2; ++c)
            {
                y[c] = ch[c].dc.process (y[c]);
                if (post) y[c] = ch[c].postHigh.process (ch[c].postLow.process (y[c]));
            }
            if (dynAmt > 0.0 && ! dynBefore) dynamics (y, dynMode_, dynAmt);

            L[i] = (float) ((dry[0] * (1.0 - wt) + y[0] * wt) * og);
            R[i] = (float) ((dry[1] * (1.0 - wt) + y[1] * wt) * og);
            outPk = std::max (outPk, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        inMeter = std::max (inPk, inMeter * 0.9f);
        outMeter = std::max (outPk, outMeter * 0.9f);
    }

private:
    struct Channel
    {
        SvfEq pre, preB, postLow, postHigh, toneA_lo, toneA_hi, toneB_lo, toneB_hi, xLow, xHigh, hpA, lpA, hpB, lpB;
        Oversampler4x osA, osB;              // moteurs A et B
        Oversampler4x dryOs, dryA, dryB;     // retards alignés : direct général, part non distordue de chaque moteur
        struct Dc { double x1 = 0, y1 = 0; double process (double x) { const auto y = x - x1 + 0.9995 * y1; x1 = x; y1 = y; return y; } } dc;
        std::array<float, 4096> comb {};
        int combW = 0;
    };

    static double delayAligned (Oversampler4x& os, double x) noexcept { double v[4]; os.up (x, v); return os.down (v); }

    /** Moteur A sur le canal c : filtre d'entrée (son moteur seulement) → courbe suréchantillonnée →
        tonalité → filtres de sortie ; la part non distordue (mélange du moteur) reste sans filtre. */
    double engineA (int c, double x) noexcept
    {
        auto& s = ch[(size_t) c];
        const auto clean = delayAligned (s.dryA, x);
        if (mixA_ <= 0.0) return clean;
        double in = x;
        if (preAType >= 1 && preAType <= 4) in = s.pre.process (x);
        else if (preAType == 5)
        {
            // peigne d'entrée (petite ligne propre au canal)
            s.comb[(size_t) s.combW] = (float) x;
            auto pos = (double) s.combW - std::min (combDelay, (double) combSize - 2.0);
            if (pos < 0.0) pos += combSize;
            const auto i0 = (int) pos, i1 = (i0 + 1) % combSize;
            const auto f = pos - (double) i0;
            in = 0.5 * (x + s.comb[(size_t) i0] + (s.comb[(size_t) i1] - s.comb[(size_t) i0]) * f);
            s.combW = (s.combW + 1) % combSize;
        }
        double v[4];
        s.osA.up (in, v);
        for (auto& u : v)
        {
            u = dist::shape (typeA_, u, driveA_, bA);
            if (toneOnA) u = s.toneA_hi.process (s.toneA_lo.process (u));
        }
        auto w = s.osA.down (v);
        if (hpOnA) w = s.hpA.process (w);
        if (lpOnA) w = s.lpA.process (w);
        const auto m = std::min (1.0, mixA_);
        return clean * (1.0 - m) + w * m * compA;
    }
    double engineB (int c, double x) noexcept
    {
        auto& s = ch[(size_t) c];
        const auto clean = delayAligned (s.dryB, x);
        if (mixB_ <= 0.0) return clean;
        const auto in = preBType >= 1 ? s.preB.process (x) : x;
        double v[4];
        s.osB.up (in, v);
        for (auto& u : v)
        {
            u = dist::shape (typeB_, u, driveB_, bB);
            if (toneOnB) u = s.toneB_hi.process (s.toneB_lo.process (u));
        }
        auto w = s.osB.down (v);
        if (hpOnB) w = s.hpB.process (w);
        if (lpOnB) w = s.lpB.process (w);
        const auto m = std::min (1.0, mixB_);
        return clean * (1.0 - m) + w * m * compB;
    }
    bool toneOnA = false, toneOnB = false, hpOnA = false, lpOnA = false, hpOnB = false, lpOnB = false;
    double bA = 0.0, bB = 0.0, mixA_ = 1.0, mixB_ = 0.0, compA = 1.0, compB = 1.0, driveA_ = 0.0, driveB_ = 0.0, combDelay = 1.0;
    int preAType = 0, preBType = 0, typeA_ = 0, typeB_ = 0;

    void route2x (const double* x, double* y, int route, double bal) noexcept
    {
        if (route == 3) // mid / side : A sur le milieu, B sur les côtés
        {
            const double mid = 0.5 * (x[0] + x[1]), side = 0.5 * (x[0] - x[1]);
            const auto m = engineA (0, mid), s = engineB (1, side);
            y[0] = m + s; y[1] = m - s;
            return;
        }
        if (route == 2) { y[0] = engineA (0, x[0]); y[1] = engineB (1, x[1]); return; } // stéréo
        for (int c = 0; c < 2; ++c)
        {
            switch (route)
            {
                case 0: y[c] = engineB (c, engineA (c, x[c])); break;                         // série : A puis B
                case 1: y[c] = (1.0 - bal) * engineA (c, x[c]) + bal * engineB (c, x[c]); break; // parallèle
                default: y[c] = engineA (c, ch[(size_t) c].xLow.process (x[c])) + engineB (c, ch[(size_t) c].xHigh.process (x[c])); break; // par bandes
            }
        }
    }

    static constexpr double osFactor = 4.0;
    static constexpr double osDelaySamples = 4.6; // retard de groupe du ×4 (mesuré, quasi constant jusqu'à 5 kHz)
    FxModBank<dist::numMods> mods;
    double hostPpq = 0.0;
    bool hostPlaying = false;

    void dynamics (double* x, int mode, double amount) noexcept
    {
        const auto peak = std::max (std::abs (x[0]), std::abs (x[1]));
        if (mode == 2) // limiteur : attaque immédiate
        {
            const auto ceiling = dbToGain (-amount * 18.0);
            dynEnv = std::max (peak, dynEnv * relCoef (80.0));
            const auto g = dynEnv > ceiling ? ceiling / dynEnv : 1.0;
            const auto mk = 1.0 / std::sqrt (ceiling);
            x[0] *= g * mk; x[1] *= g * mk;
            grMeter = (float) -gainToDb (g);
            return;
        }
        if (mode == 0) // compresseur 4:1, seuil selon la quantité, compensation automatique
        {
            const auto thrDb = -amount * 30.0;
            const auto a = peak > dynEnv ? attCoef (5.0) : relCoef (120.0);
            dynEnv = peak + (dynEnv - peak) * a;
            const auto lvl = gainToDb (dynEnv);
            const auto over = std::max (0.0, lvl - thrDb);
            const auto grDb = over * (1.0 - 1.0 / 4.0);
            const auto g = dbToGain (-grDb + (-thrDb) * 0.375);
            x[0] *= g; x[1] *= g;
            grMeter = (float) grDb;
            return;
        }
        // multibande : 3 bandes compressées séparément (graves < 200 Hz, aigus > 2,5 kHz)
        for (int c = 0; c < 2; ++c)
        {
            mbLow[c].set (SvfEq::lowpass, 200.0, 0.5, 0.0, sr);
            mbHigh[c].set (SvfEq::highpass, 2500.0, 0.5, 0.0, sr);
        }
        double bands[3][2];
        for (int c = 0; c < 2; ++c)
        {
            bands[0][c] = mbLow[c].process (x[c]);
            bands[2][c] = mbHigh[c].process (x[c]);
            bands[1][c] = x[c] - bands[0][c] - bands[2][c];
        }
        const auto thrDb = -amount * 36.0;
        double maxGr = 0.0;
        for (int b = 0; b < 3; ++b)
        {
            const auto pk = std::max (std::abs (bands[b][0]), std::abs (bands[b][1]));
            const auto a = pk > bandEnv[b] ? attCoef (b == 0 ? 10.0 : 3.0) : relCoef (b == 0 ? 150.0 : 60.0);
            bandEnv[b] = pk + (bandEnv[b] - pk) * a;
            const auto over = std::max (0.0, gainToDb (bandEnv[b]) - thrDb);
            const auto grDb = over * (1.0 - 1.0 / 6.0);
            maxGr = std::max (maxGr, grDb);
            const auto g = dbToGain (-grDb + (-thrDb) * 0.45);
            bands[b][0] *= g; bands[b][1] *= g;
        }
        x[0] = bands[0][0] + bands[1][0] + bands[2][0];
        x[1] = bands[0][1] + bands[1][1] + bands[2][1];
        grMeter = (float) maxGr;
    }

    double attCoef (double ms) const noexcept { return std::exp (-1.0 / (ms * 0.001 * sr)); }
    double relCoef (double ms) const noexcept { return std::exp (-1.0 / (ms * 0.001 * sr)); }

    // Ligne de réinjection stéréo entrelacée (la moitié du tampon par canal)
    void pushFb (double l, double r) noexcept
    {
        const auto half = fbBuf.size() / 2;
        fbBuf[fbW] = (float) l;
        fbBuf[half + fbW] = (float) r;
        fbW = (fbW + 1) % half;
    }
    std::pair<double, double> readFb (double d) const noexcept
    {
        const auto half = (double) (fbBuf.size() / 2);
        auto pos = (double) fbW - d;
        while (pos < 0.0) pos += half;
        const auto i0 = (size_t) pos;
        const auto i1 = (i0 + 1) % (size_t) half;
        const auto f = (float) (pos - (double) i0);
        const auto h = fbBuf.size() / 2;
        return { fbBuf[i0] + (fbBuf[i1] - fbBuf[i0]) * f, fbBuf[h + i0] + (fbBuf[h + i1] - fbBuf[h + i0]) * f };
    }

    static constexpr int combSize = 4096;
    double sr = 48000.0, wetPrev = -1.0, outPrev = -1.0;
    std::array<Channel, 2> ch {};
    std::vector<float> fbBuf;
    size_t fbW = 0;
    std::array<SvfEq, 2> mbLow, mbHigh;
    double dynEnv = 0.0;
    std::array<double, 3> bandEnv {};
    float inMeter = 0.0f, outMeter = 0.0f, grMeter = 0.0f;
};

} // namespace grv::effects
