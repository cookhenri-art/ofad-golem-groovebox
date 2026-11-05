/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Échos multiples » — principe des grandes pédales de délai multimode : plusieurs machines à
    écho dans une boîte, chacune avec son caractère, et les mêmes commandes partout.
      - Numérique : répétitions nettes ;
      - Bande : pleurage et scintillement, saturation, pertes d'aigus à chaque passage ;
      - Seau (circuits BBD) : sombre, bruit léger, compression douce ;
      - Lo-fi : réduction de bits et d'échantillonnage dans la boucle, bande étroite ;
      - Inverse : chaque tranche jouée à l'envers (deux têtes en fondu, sans clic) ;
      - Filtre : un filtre résonant balayé par un LFO dans la boucle ;
      - Double : deux temps (rapport réglable), un par côté ;
      - Ping-pong : les répétitions passent d'un côté à l'autre.
    Temps libre ou calé sur le tempo, réinjection jusqu'à l'auto-oscillation (bornée), couleur,
    grain, vitesse et profondeur de modulation, effacement sous le signal.
    Architecture propre (aucune reprise de code ni de réglages d'un produit existant).
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"
#include "Delay.h"
#include "FxBanks.h"

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace mdelay
{
    enum Slot { mode = 0, sync, timeMs, division, feedback, mix, tone, grit, modSpeed, modDepth, spread, ducking, numSlots };
    enum Mode { digital = 0, tape, bbd, lofi, reverse, filter, dual, pingpong };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "mdelay";
            m.name = "Comet"; m.role = "\xC3\x89" "chos multiples";
            m.family = "\xC3\x89" "chos"; m.form = "pedal";
            m.principle = "D\xC3\xA9lai multimode : num\xC3\xA9rique, bande, seau BBD, lo-fi, inverse, filtre balay\xC3\xA9, double, ping-pong";
            m.params = {
                C ("mode", "Machine", { "Num\xC3\xA9rique", "Bande", "Seau (BBD)", "Lo-fi", "Inverse", "Filtre", "Double", "Ping-pong" }, 1),
                C ("sync", "Synchro", { "Libre", "Tempo" }, 1),
                P ("time", "Temps", "ms", 20, 3000, 400, 0.4f),
                C ("division", "Division", echo::divisions(), 9),
                P ("feedback", "R\xC3\xA9p\xC3\xA9titions", "%", 0, 110, 45),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 35),
                P ("tone", "Couleur", "Hz", 500, 18000, 6000, 0.4f),
                P ("grit", "Grain", "%", 0, 100, 25),
                P ("modspeed", "Vitesse mod.", "Hz", 0.05f, 8, 0.5f, 0.4f),
                P ("moddepth", "Profondeur mod.", "%", 0, 100, 15),
                P ("spread", "\xC3\x89" "cart (double)", "%", 25, 200, 75),
                P ("ducking", "Effacement", "%", 0, 100, 0),
            };
            m.sections = {
                { "time", "Temps", "delay", { "mode", "sync", "time", "division", "feedback", "mix", "spread", "ducking" }, "time" },
                { "tone", "Couleur et modulation", "eq", { "tone", "grit", "modspeed", "moddepth" }, "tone" },
            };
            m.presets = {
                { "Bande au point\xC3\xA9", {} },
                { "Num\xC3\xA9rique net", { { "mode", 0 }, { "division", 10 }, { "grit", 0 }, { "moddepth", 0 }, { "tone", 14000 } } },
                { "Seau sombre", { { "mode", 2 }, { "division", 7 }, { "feedback", 60 }, { "tone", 2800 } } },
                { "Lo-fi cassette", { { "mode", 3 }, { "division", 9 }, { "grit", 65 }, { "moddepth", 40 }, { "tone", 3500 } } },
                { "\xC3\x89" "chos invers\xC3\xA9s", { { "mode", 4 }, { "division", 13 }, { "feedback", 35 }, { "mix", 45 } } },
                { "Filtre qui respire", { { "mode", 5 }, { "division", 9 }, { "feedback", 70 }, { "modspeed", 0.25f }, { "moddepth", 80 }, { "tone", 1500 }, { "grit", 60 } } },
                { "Double rythmique", { { "mode", 6 }, { "division", 10 }, { "spread", 75 }, { "feedback", 40 } } },
                { "Ping-pong large", { { "mode", 7 }, { "division", 9 }, { "feedback", 50 }, { "mix", 30 } } },
                { "Auto-oscillation", { { "mode", 1 }, { "division", 7 }, { "feedback", 105 }, { "grit", 50 }, { "tone", 3000 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class MultiDelay final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return mdelay::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 6.5)) size <<= 1; // 3 s × écart 200 % + marge
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0; curA = curB = -1.0; lfo = 0.0; flut = 0.0; duck = 0.0; holdL = holdR = 0.0; holdN = 0;
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        for (auto& f : sweep) f.reset();
        rev = {};
        sweepCount = 0;
    }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace mdelay;
        const auto md = std::clamp ((int) p[mode], 0, 7);
        const auto baseMs = p[sync] > 0.5f ? echo::divisionQuarters ((int) p[division]) * 60000.0 / std::max (20.0, bpm) : (double) p[timeMs];
        const auto targetA = std::clamp (baseMs * 0.001 * sr, 16.0, (double) size / 2.2);
        const auto targetB = std::clamp (baseMs * (p[spread] / 100.0) * 0.001 * sr, 16.0, (double) size / 2.2);
        if (curA < 0.0) { curA = targetA; curB = targetB; }
        const auto fb = std::min (1.1, p[feedback] / 100.0);
        const auto wet = p[mix] / 100.0;
        const auto gr = p[grit] / 100.0;
        const auto depth = p[modDepth] / 100.0;
        const auto duckAmt = p[ducking] / 100.0;
        const auto toneHz = md == bbd ? std::min ((double) p[tone], 4000.0) : md == lofi ? std::min ((double) p[tone], 5000.0) : (double) p[tone];
        for (int c = 0; c < 2; ++c)
        {
            lp[c].set (SvfEq::lowpass, toneHz, 0.65, 0.0, sr);
            hp[c].set (SvfEq::highpass, md == lofi ? 300.0 : 60.0, 0.6, 0.0, sr);
        }
        const auto glide = 1.0 - std::exp (-1.0 / (0.1 * sr));
        const auto duckRel = std::exp (-1.0 / (0.25 * sr));
        const auto mask = size - 1;
        const auto lfoInc = (double) p[modSpeed] / sr;
        const auto holdLen = md == lofi ? 1 + (int) (gr * 7.0) : 1;
        const auto quant = md == lofi ? std::ldexp (1.0, 12 - (int) (gr * 7.0)) : 0.0;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n); // mélange en rampe (v0.22)
        wetPrev = wet;

        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            curA += (targetA - curA) * glide;
            curB += (targetB - curB) * glide;
            lfo += lfoInc; if (lfo >= 1.0) lfo -= 1.0;
            const auto sinL = fastSin2Pi (lfo);
            // modulation du temps : pleurage (bande), chorus léger (autres), pas pour le filtre
            double mod = 0.0;
            if (md != filter)
            {
                flut += 6.7 / sr; if (flut >= 1.0) flut -= 1.0;
                mod = depth * 0.003 * sr * sinL + (md == tape ? (0.0004 + gr * 0.0006) * sr * fastSin2Pi (flut) : 0.0);
            }
            const double inL = L[i], inR = R[i];
            double yl, yr;
            if (md == reverse)
            {
                yl = readReverse (bufL, 0, curA);
                yr = readReverse (bufR, 1, curA);
                advanceReverse (curA);
            }
            else
            {
                const auto dA = curA + mod, dB = (md == dual ? curB : curA) + mod;
                yl = read (bufL, dA); yr = read (bufR, md == dual ? dB : dA);
            }

            // boucle de réinjection : couleur, caractère
            double fl = lp[0].process (hp[0].process (yl)), fr = lp[1].process (hp[1].process (yr));
            if (md == filter)
            {
                if (sweepCount-- <= 0)
                {
                    sweepCount = 16;
                    const auto f = toneHz * std::exp2 (depth * 3.0 * sinL);
                    for (auto& s : sweep) s.set (SvfEq::bandpass, std::clamp (f, 80.0, 0.45 * sr), 1.0 + gr * 8.0, 0.0, sr);
                }
                fl = sweep[0].process (fl) * 1.4; fr = sweep[1].process (fr) * 1.4;
            }
            if (md == tape || md == bbd || gr > 0.0)
            {
                const auto drive = 1.0 + gr * (md == tape ? 4.0 : 2.0) + (md == bbd ? 0.6 : 0.0);
                fl = fastTanh (fl * drive) / drive * (1.0 + 0.2 * gr);
                fr = fastTanh (fr * drive) / drive * (1.0 + 0.2 * gr);
            }
            if (md == bbd) { noise = noise * 1664525u + 1013904223u; const auto nz = ((double) (noise >> 9) / 4194304.0 - 1.0) * 1.5e-4; fl += nz; fr += nz; }
            if (md == lofi)
            {
                if (--holdN <= 0) { holdN = holdLen; holdL = fl; holdR = fr; }
                fl = std::round (holdL * quant) / quant; fr = std::round (holdR * quant) / quant;
            }

            double wl, wr;
            if (md == pingpong) { wl = 0.5 * (inL + inR) + fb * fr; wr = fb * fl; }
            else { wl = inL + fb * fl; wr = inR + fb * fr; }
            if (fb > 0.98) { wl = fastTanh (wl); wr = fastTanh (wr); }
            bufL[(size_t) w] = (float) wl;
            bufR[(size_t) w] = (float) wr;
            w = (w + 1) & mask;

            const auto lvl = std::max (std::abs (inL), std::abs (inR));
            duck = std::max (lvl, duck * duckRel);
            const auto dg = 1.0 - duckAmt * std::min (1.0, duck * 3.0);
            L[i] = (float) (inL * (1.0 - wt * 0.5) + yl * wt * dg);
            R[i] = (float) (inR * (1.0 - wt * 0.5) + yr * wt * dg);
        }
    }

private:
    double read (const std::vector<float>& b, double d) const noexcept
    {
        auto pos = (double) w - d;
        if (pos < 0.0) pos += size;
        const auto i0 = (int) pos & (size - 1);
        const auto f = pos - std::floor (pos);
        // Hermite (v0.22) : lecture modulée (pleurage, chorus) sans perte d'aigus
        auto at = [&] (int j) { return (double) b[(size_t) (j & (size - 1))]; };
        return hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
    }

    /** Inverse : deux têtes décalées d'une demi-tranche ; chacune repart du présent et recule sur une
        tranche (le temps du délai, donc jusqu'à deux temps en arrière), avec une fenêtre en sinus² :
        la somme des deux reste constante. */
    struct Rev { std::array<double, 2> count {}; std::array<int, 2> start {}; bool init = false; };
    double readReverse (const std::vector<float>& b, int, double len) noexcept
    {
        if (! rev.init) { rev.init = true; rev.count = { 0.0, 0.5 * len }; rev.start = { w, w }; }
        double s = 0.0;
        for (int h = 0; h < 2; ++h)
        {
            const auto c = rev.count[(size_t) h];
            const auto ph = std::clamp (c / std::max (1.0, len), 0.0, 1.0);
            auto pos = (double) rev.start[(size_t) h] - 1.0 - c; // recule depuis le début de la tranche : la tranche passée, à l'envers
            while (pos < 0.0) pos += size;
            const auto v = (double) b[(size_t) ((int) pos & (size - 1))];
            const auto wv = fastSin2Pi (0.5 * ph);
            s += v * wv * wv;
        }
        return s;
    }
    void advanceReverse (double len) noexcept
    {
        for (int h = 0; h < 2; ++h)
        {
            rev.count[(size_t) h] += 1.0;
            if (rev.count[(size_t) h] >= len) { rev.count[(size_t) h] = 0.0; rev.start[(size_t) h] = w; }
        }
    }

    double sr = 48000.0, curA = -1.0, curB = -1.0, lfo = 0.0, flut = 0.0, duck = 0.0, holdL = 0.0, holdR = 0.0, wetPrev = -1.0;
    int size = 1, w = 0, holdN = 0, sweepCount = 0;
    std::uint32_t noise = 98765u;
    std::vector<float> bufL, bufR;
    std::array<SvfEq, 2> lp, hp, sweep;
    Rev rev;
};

} // namespace grv::effects
