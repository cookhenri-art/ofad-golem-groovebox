/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Copie figée de StepFx.h de la v0.38 (tests seulement) : le son de base de Hopscotch ne doit pas changer.

    « Effets à pas » — principe des multi-effets séquencés en rythme : une grille de 16 pas calée
    sur le tempo ; chaque pas choisit un effet, joué pendant toute sa durée :
      Répète (une tranche courte rejouée en boucle), Inverse (le pas précédent à l'envers),
      Filtre (passe-bas résonant), Écrase (réduction de bits et d'échantillonnage),
      Arrêt bande (la lecture ralentit jusqu'à l'arrêt), Hachure (porte rapide), Écho (envoi
      dans un écho dont la traîne continue après le pas).
    Séquenceur arrêté : le son passe sans effet. Fondus courts à chaque changement (pas de clic).
    Architecture propre (aucune reprise de code ni de réglages d'un produit existant).
*/

#pragma once

#include "dsp/EqFilters.h"
#include "engine/Module.h"
#include "synth/SynthCore.h"

#include <array>
#include <vector>

namespace grv::legacy38
{

using namespace grv::engine;
using namespace grv::dsp;

namespace stepfx
{
    enum Fx { none = 0, repeatFx, reverseFx, filterFx, crushFx, tapeStopFx, chopFx, echoFx, numFx };
    enum Slot { step1 = 0, steps = 16, division, repeatSize, cutoff, resonance, bits, echoTime, echoFeedback, mix, numSlots };

    inline const std::vector<std::string>& fxNames()
    {
        static const std::vector<std::string> v { "\xE2\x80\x94", "R\xC3\xA9p\xC3\xA8te", "Inverse", "Filtre", "\xC3\x89" "crase", "Arr\xC3\xAAt bande", "Hachure", "\xC3\x89" "cho" };
        return v;
    }
    inline double divisionQuarters (int i) noexcept { static constexpr double q[] { 0.125, 0.25, 0.5, 1.0 }; return q[std::clamp (i, 0, 3)]; }
    inline double repeatQuarters (int i) noexcept { static constexpr double q[] { 1.0 / 16, 0.125, 0.25, 0.5 }; return q[std::clamp (i, 0, 3)]; }
    inline double echoQuarters (int i) noexcept { static constexpr double q[] { 0.25, 0.375, 0.5, 0.75 }; return q[std::clamp (i, 0, 3)]; }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "stepfx";
            m.name = "Hopscotch"; m.role = "Effets \xC3\xA0 pas";
            m.family = "Filtres et performance"; m.form = "module";
            m.principle = "Grille de 16 pas cal\xC3\xA9" "e sur le tempo : r\xC3\xA9p\xC3\xA9tition, inverse, filtre, \xC3\xA9" "crasement, arr\xC3\xAAt bande, hachure, \xC3\xA9" "cho, un effet par pas";
            for (int s = 1; s <= 16; ++s) m.params.push_back (C ("s" + std::to_string (s), "Pas " + std::to_string (s), fxNames(), 0));
            m.params.push_back (I ("steps", "Longueur", "pas", 1, 16, 16));
            m.params.push_back (C ("division", "Pas de", { "1/32", "1/16", "1/8", "1/4" }, 1));
            m.params.push_back (C ("repeat", "Tranche r\xC3\xA9p\xC3\xA9t\xC3\xA9" "e", { "1/64", "1/32", "1/16", "1/8" }, 1));
            m.params.push_back (P ("cutoff", "Coupure", "Hz", 80, 12000, 700, 0.3f));
            m.params.push_back (P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 55));
            m.params.push_back (I ("bits", "Bits", "", 3, 12, 6));
            m.params.push_back (C ("echotime", "Temps de l'\xC3\xA9" "cho", { "1/16", "1/16.", "1/8", "1/8." }, 1));
            m.params.push_back (P ("echofb", "R\xC3\xA9p\xC3\xA9titions", "%", 0, 90, 50));
            m.params.push_back (P ("mix", "M\xC3\xA9lange", "%", 0, 100, 100));
            m.sections = {
                { "steps1", "Pas 1 \xC3\xA0 8", "global", { "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8" }, "steps" },
                { "steps2", "Pas 9 \xC3\xA0 16", "global", { "s9", "s10", "s11", "s12", "s13", "s14", "s15", "s16" }, "steps" },
                { "settings", "R\xC3\xA9glages", "global", { "steps", "division", "repeat", "cutoff", "reso", "bits", "echotime", "echofb" }, "settings" },
                { "out", "Sortie", "global", { "mix" }, "settings" },
            };
            auto grid = [] (std::initializer_list<int> v)
            {
                std::vector<std::pair<std::string, float>> out;
                int s = 1;
                for (auto x : v) { if (x != 0) out.push_back ({ "s" + std::to_string (s), (float) x }); ++s; }
                return out;
            };
            m.presets = {
                { "Vide", {} },
                { "B\xC3\xA9gaiement", grid ({ 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1 }) },
                { "Retour en arri\xC3\xA8re", grid ({ 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 2, 0, 2, 2 }) },
                { "Fin de mesure \xC3\xA9" "cras\xC3\xA9" "e", grid ({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 5, 5 }) },
                { "Hach\xC3\xA9 filtr\xC3\xA9", grid ({ 6, 0, 3, 0, 6, 0, 3, 3, 6, 0, 3, 0, 6, 6, 3, 3 }) },
                { "\xC3\x89" "chos sur les contretemps", grid ({ 0, 0, 7, 0, 0, 0, 7, 0, 0, 0, 7, 0, 0, 7, 7, 0 }) },
                { "Tout m\xC3\xA9lang\xC3\xA9", grid ({ 0, 1, 0, 2, 3, 0, 6, 1, 0, 5, 0, 4, 7, 2, 1, 5 }) },
            };
            return m;
        }();
        return d;
    }
}

class StepFx final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return stepfx::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 4.0)) size <<= 1;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        echoSize = 1;
        while (echoSize < (int) (sr * 2.0)) echoSize <<= 1;
        echoL.assign ((size_t) echoSize, 0.0f); echoR.assign ((size_t) echoSize, 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f);
        std::fill (echoL.begin(), echoL.end(), 0.0f); std::fill (echoR.begin(), echoR.end(), 0.0f);
        w = ew = 0; lastStep = -1; k = 0; current = 0; mixNow = 0.0; fade = 1.0; holdL = holdR = 0.0; holdN = 0;
        declick.prepare (sr);
        for (auto& f : flt) f.reset();
    }

    void setPosition (double ppq, bool isPlaying) noexcept override { ppqStart = ppq; playing = isPlaying; }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace stepfx;
        const auto qPerSample = std::max (20.0, bpm) / 60.0 / sr;
        const auto stepQ = divisionQuarters ((int) p[division]);
        const auto stepsN = std::clamp ((int) std::lround (p[steps]), 1, 16);
        const auto stepSamples = stepQ / qPerSample;
        const auto slice = std::max (32.0, repeatQuarters ((int) p[repeatSize]) / qPerSample);
        for (auto& f : flt) f.set (SvfEq::lowpass, p[cutoff], 0.7 + p[resonance] / 100.0 * 9.0, 0.0, sr);
        const auto quant = std::ldexp (1.0, std::clamp ((int) std::lround (p[bits]), 3, 12) - 1);
        const auto holdLen = 2 + (12 - std::clamp ((int) std::lround (p[bits]), 3, 12));
        const auto echoD = std::clamp (echoQuarters ((int) p[echoTime]) / qPerSample, 16.0, (double) echoSize - 4.0);
        const auto echoFb = p[echoFeedback] / 100.0;
        const auto wet = p[mix] / 100.0;
        const auto mixStep = 1.0 / (0.004 * sr), fadeStep = 1.0 / (0.002 * sr);
        const auto chopPeriod = std::max (8.0, 0.0625 / qPerSample);
        const auto mask = size - 1, emask = echoSize - 1;

        for (int i = 0; i < n; ++i)
        {
            const double inL = L[i], inR = R[i];
            bufL[(size_t) w] = (float) inL; bufR[(size_t) w] = (float) inR;
            bool jump = false; // v0.22 : saut de signal (changement de pas ou d'effet) compensé en 2 ms

            int fx = none;
            if (playing)
            {
                const auto q = ppqStart + i * qPerSample;
                const auto idx = (long long) std::floor (q / stepQ + 1.0e-9);
                const auto step = (int) (((idx % stepsN) + stepsN) % stepsN);
                if (idx != lastStep)
                {
                    lastStep = idx; stepStart = w; k = 0; fade = 0.0;
                    jump = current == repeatFx || current == reverseFx || current == tapeStopFx; // lecture de la mémoire relancée
                }
                fx = std::clamp ((int) p[step1 + step], 0, numFx - 1);
            }
            if (fx != current) { current = fx; fade = 0.0; jump = true; }

            double yl = inL, yr = inR;
            switch (current)
            {
                case repeatFx:
                {
                    const auto pos = (stepStart + (int) std::fmod ((double) k, slice)) & mask;
                    yl = bufL[(size_t) pos]; yr = bufR[(size_t) pos];
                    const auto ph = std::fmod ((double) k, slice);
                    const auto edge = std::min (1.0, std::min (ph, slice - ph) / 48.0); // fondu en bord de tranche
                    yl *= edge; yr *= edge;
                    break;
                }
                case reverseFx:
                {
                    const auto pos = (stepStart - 1 - k) & mask;
                    yl = bufL[(size_t) pos]; yr = bufR[(size_t) pos];
                    break;
                }
                case filterFx: yl = flt[0].process (inL); yr = flt[1].process (inR); break;
                case crushFx:
                    if (--holdN <= 0) { holdN = holdLen; holdL = inL; holdR = inR; }
                    yl = std::round (holdL * quant) / quant; yr = std::round (holdR * quant) / quant;
                    break;
                case tapeStopFx:
                {
                    // vitesse 1 → 0 sur le pas : position = k − k²/(2N)
                    const auto kk = std::min ((double) k, stepSamples);
                    const auto off = kk - kk * kk / (2.0 * std::max (1.0, stepSamples));
                    auto pos = (double) stepStart + off;
                    const auto i0 = (int) std::floor (pos); const auto f = pos - (double) i0;
                    yl = bufL[(size_t) (i0 & mask)] * (1.0 - f) + bufL[(size_t) ((i0 + 1) & mask)] * f;
                    yr = bufR[(size_t) (i0 & mask)] * (1.0 - f) + bufR[(size_t) ((i0 + 1) & mask)] * f;
                    const auto g = std::max (0.0, 1.0 - kk / std::max (1.0, stepSamples));
                    yl *= std::sqrt (g); yr *= std::sqrt (g);
                    break;
                }
                case chopFx:
                {
                    const auto ph = std::fmod ((double) k, chopPeriod) / chopPeriod;
                    const auto g = ph < 0.5 ? std::min (1.0, ph * 40.0) : std::max (0.0, 1.0 - (ph - 0.5) * 40.0);
                    yl = inL * g; yr = inR * g;
                    break;
                }
                default: break;
            }

            // écho : alimenté pendant les pas « Écho », sa traîne continue ensuite
            double el, er;
            {
                auto pos = (double) ew - echoD; if (pos < 0.0) pos += echoSize;
                const auto i0 = (int) pos;
                el = echoL[(size_t) (i0 & emask)]; er = echoR[(size_t) (i0 & emask)];
                const auto send = current == echoFx ? 1.0 : 0.0;
                echoL[(size_t) ew] = (float) (inL * send + er * echoFb);   // croisé : de gauche à droite
                echoR[(size_t) ew] = (float) (inR * send + el * echoFb);
                ew = (ew + 1) & emask;
            }

            // mélange avec fondus : entrée et sortie d'un effet, changement d'effet
            fade = std::min (1.0, fade + fadeStep);
            const auto target = current != none && current != echoFx ? 1.0 : 0.0;
            mixNow += std::clamp (target - mixNow, -mixStep, mixStep);
            const auto m = mixNow * wet;
            // les effets qui lisent la mémoire repartent en fondu à chaque pas (pas de saut)
            const auto fd = current == repeatFx || current == reverseFx || current == tapeStopFx ? fade : 1.0;
            const auto fl = yl * fd + inL * (1.0 - fd), fr = yr * fd + inR * (1.0 - fd);
            L[i] = (float) (inL * (1.0 - m) + fl * m + el * wet);
            R[i] = (float) (inR * (1.0 - m) + fr * m + er * wet);
            if (jump) declick.arm();
            declick.process (L + i, R + i, 1);
            w = (w + 1) & mask;
            ++k;
        }
        ppqStart += n * qPerSample;
    }

private:
    double sr = 48000.0, ppqStart = 0.0, mixNow = 0.0, fade = 1.0, holdL = 0.0, holdR = 0.0;
    synth::Declick declick; // sauts aux changements de pas et d'effet (v0.22)
    bool playing = false;
    int size = 1, echoSize = 1, w = 0, ew = 0, stepStart = 0, k = 0, current = 0, holdN = 0;
    long long lastStep = -1;
    std::vector<float> bufL, bufR, echoL, echoR;
    std::array<SvfEq, 2> flt;
};

} // namespace grv::legacy38
