/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Réverbe » — principe des réverbérations algorithmiques modernes à réglages « physiques » :
    réseau de 8 lignes à retard réinjectées par une matrice de Hadamard, longueurs modulées
    lentement (pas de coloration métallique), temps de déclin RT60 réglable avec rapport de déclin
    des graves et brillance (amortissement des aigus dans la boucle), diffusion d'entrée,
    premières réflexions, pré-délai, distance (équilibre premières réflexions / queue), largeur,
    égalisation de sortie, gel de la queue.
    Architecture propre.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"
#include "FxBanks.h"

#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace verb
{
    enum Slot { size = 0, decay, predelay, brightness, bass, distance, character, diffusion, width, lowCut, highCut, mix, freeze, numSlots };

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "reverb";
            m.name = "Nave"; m.role = "R\xC3\xA9verb\xC3\xA9ration";
            m.family = "R\xC3\xA9" "verb\xC3\xA9rations"; m.form = "pedal";
            m.principle = "R\xC3\xA9seau de 8 lignes \xC3\xA0 retard modul\xC3\xA9" "es, d\xC3\xA9" "clin RT60, brillance, rapport des graves, premi\xC3\xA8res r\xC3\xA9" "flexions";
            m.params = {
                P ("size", "Espace", "%", 0, 100, 55),
                P ("decay", "Dur\xC3\xA9" "e", "s", 0.1f, 20, 2.2f, 0.4f),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 250, 15, 0.6f),
                P ("brightness", "Brillance", "Hz", 1000, 18000, 6500, 0.4f),
                P ("bass", "Graves (rapport)", "%", 25, 200, 100),
                P ("distance", "Distance", "%", 0, 100, 50),
                P ("character", "Caract\xC3\xA8re", "%", 0, 100, 35),
                P ("diffusion", "Diffusion", "%", 0, 100, 75),
                P ("width", "Largeur", "%", 0, 150, 100),
                P ("lowcut", "Coupe-bas", "Hz", 20, 1000, 120, 0.4f),
                P ("highcut", "Coupe-haut", "Hz", 1500, 20000, 14000, 0.4f),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 25),
                C ("freeze", "Gel", { "Non", "Oui" }),
            };
            m.sections = {
                { "space", "Espace", "reverb", { "size", "decay", "predelay", "distance", "character", "diffusion", "width", "mix" }, "space" },
                { "tone", "Couleur", "eq", { "brightness", "bass", "lowcut", "highcut", "freeze" }, "tone" },
            };
            m.presets = {
                { "Salle", {} },
                { "Pi\xC3\xA8" "ce", { { "size", 25 }, { "decay", 0.7f }, { "predelay", 5 }, { "distance", 30 }, { "brightness", 8000 } } },
                { "Plaque", { { "size", 40 }, { "decay", 1.8f }, { "predelay", 0 }, { "diffusion", 100 }, { "brightness", 11000 }, { "distance", 80 } } },
                { "Cath\xC3\xA9" "drale", { { "size", 100 }, { "decay", 9 }, { "predelay", 40 }, { "brightness", 4500 }, { "bass", 130 }, { "distance", 75 } } },
                { "Voix", { { "size", 45 }, { "decay", 1.6f }, { "predelay", 60 }, { "lowcut", 300 }, { "highcut", 9000 }, { "mix", 18 } } },
                { "Caisse claire", { { "size", 35 }, { "decay", 1.1f }, { "predelay", 10 }, { "brightness", 9000 }, { "diffusion", 90 }, { "mix", 30 } } },
                { "Nappe infinie", { { "size", 90 }, { "decay", 18 }, { "character", 70 }, { "mix", 45 } } },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Reverb final : public Effect
{
public:
    static constexpr int numLines = 8;

    const ModuleDesc& desc() const noexcept override { return verb::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        lineSize = 1;
        while (lineSize < (int) (0.26 * sr)) lineSize <<= 1;
        for (auto& l : lines) l.buf.assign ((size_t) lineSize, 0.0f);
        preSize = 1;
        while (preSize < (int) (0.6 * sr)) preSize <<= 1;
        preL.assign ((size_t) preSize, 0.0f);
        preR.assign ((size_t) preSize, 0.0f);
        static constexpr double apMs[4] { 2.9, 7.7, 2.3, 6.1 }; // gauche : 0-1, droite : 2-3
        for (int k = 0; k < 4; ++k) { diff[(size_t) k].buf.assign (2048, 0.0f); diff[(size_t) k].len = std::clamp ((int) (apMs[k] * 0.001 * sr), 1, 2040); }
        reset();
    }

    void reset() noexcept override
    {
        for (auto& l : lines) { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.lp = 0.0; l.cur = -1.0; }
        wetPrev = -1.0;
        std::fill (preL.begin(), preL.end(), 0.0f);
        std::fill (preR.begin(), preR.end(), 0.0f);
        for (auto& a : diff) { std::fill (a.buf.begin(), a.buf.end(), 0.0f); a.w = 0; }
        for (auto& f : outLc) f.reset();
        for (auto& f : outHc) f.reset();
        w = preW = 0;
        lfo = 0.0;
    }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace verb;
        // Longueurs de base (ms), écartées pour éviter les résonances communes
        static constexpr double baseMs[numLines] { 31.7, 37.1, 41.3, 47.9, 53.3, 59.9, 67.1, 73.7 };
        const auto scale = 0.35 + 1.65 * (p[size] / 100.0) * (p[size] / 100.0);
        const auto rt = std::max (0.1, (double) p[decay]);
        const bool frozen = p[freeze] > 0.5f;
        const auto bassMul = p[bass] / 100.0;
        const auto crossover = std::exp (-kTwoPi * 450.0 / sr);
        const auto damp = std::exp (-kTwoPi * std::min ((double) p[brightness], 0.45 * sr) / sr);
        const auto modDepth = (p[character] / 100.0) * 0.0012 * sr;
        const auto diffAmt = 0.72 * p[diffusion] / 100.0;
        const auto wd = p[width] / 100.0;
        const auto wet = p[mix] / 100.0;
        const auto dist = p[distance] / 100.0;
        const auto early = 0.9 * (1.0 - dist) + 0.1, tail = 0.3 + 0.7 * dist;
        const auto preSamples = std::clamp (p[predelay] * 0.001 * sr, 0.0, (double) preSize - 2.0);
        for (int c = 0; c < 2; ++c)
        {
            outLc[c].set (SvfEq::highpass, p[lowCut], 0.6, 0.0, sr);
            outHc[c].set (SvfEq::lowpass, p[highCut], 0.6, 0.0, sr);
        }
        for (int k = 0; k < numLines; ++k)
        {
            auto& l = lines[(size_t) k];
            l.len = std::min ((double) lineSize - modDepth - 4.0, baseMs[k] * 0.001 * sr * scale);
            // gains de déclin : RT60 aux graves (× rapport) et aux aigus
            const auto secs = l.len / sr;
            l.gHigh = frozen ? 1.0 : std::pow (10.0, -3.0 * secs / rt);
            l.gLow = frozen ? 1.0 : std::pow (10.0, -3.0 * secs / (rt * bassMul));
        }
        const auto lenGlide = 1.0 - std::exp (-1.0 / (0.12 * sr)); // la taille glisse en ~120 ms (v0.22 : plus de saut)
        for (auto& l : lines) if (l.cur < 0.0) l.cur = l.len;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        const auto lfoInc = 0.37 / sr;
        const auto mask = lineSize - 1;
        const auto inGain = frozen ? 0.0 : 0.35;

        for (int i = 0; i < n; ++i)
        {
            const double inL = L[i], inR = R[i];
            // pré-délai
            preL[(size_t) preW] = (float) inL; preR[(size_t) preW] = (float) inR;
            const auto rp = (preW - (int) preSamples + preSize) & (preSize - 1);
            preW = (preW + 1) & (preSize - 1);
            auto xl = (double) preL[(size_t) rp], xr = (double) preR[(size_t) rp];

            // premières réflexions : prises dans le tampon du pré-délai
            double el = 0.0, er = 0.0;
            static constexpr double tapMs[6] { 7.1, 11.9, 17.3, 23.9, 29.1, 37.7 };
            static constexpr double tapG[6] { 0.8, 0.62, 0.5, 0.4, 0.31, 0.24 };
            for (int t = 0; t < 6; ++t)
            {
                const auto d = (int) (tapMs[t] * scale * 0.001 * sr);
                const auto idx = (rp - d + preSize) & (preSize - 1);
                if (t % 2) { el += preR[(size_t) idx] * tapG[t]; er += preL[(size_t) idx] * tapG[t] * 0.9; }
                else { el += preL[(size_t) idx] * tapG[t] * 0.9; er += preR[(size_t) idx] * tapG[t]; }
            }

            // diffusion d'entrée
            for (int k = 0; k < 2; ++k) { xl = allpass (diff[(size_t) k], xl, diffAmt); xr = allpass (diff[(size_t) (k + 2)], xr, diffAmt); }

            // lecture des lignes (modulées), filtre de déclin, matrice de Hadamard
            lfo += lfoInc; if (lfo >= 1.0) lfo -= 1.0;
            double v[numLines], outs[numLines];
            for (int k = 0; k < numLines; ++k)
            {
                auto& l = lines[(size_t) k];
                const auto ph = lfo + (double) k * 0.125;
                const auto m = modDepth * (ph - std::floor (ph) < 0.5 ? 4.0 * (ph - std::floor (ph)) - 1.0 : 3.0 - 4.0 * (ph - std::floor (ph)));
                l.cur += (l.len - l.cur) * lenGlide;
                const auto y = read (l, w - l.cur - m);
                outs[k] = y;
                // plateau du premier ordre : gain gLow sous 450 Hz, gHigh au-dessus, amortissement des aigus
                l.lp = y + (l.lp - y) * crossover;
                auto z = l.gHigh * y + (l.gLow - l.gHigh) * l.lp;
                l.damp = z + (l.damp - z) * damp;
                v[k] = frozen ? z : l.damp;
            }
            hadamard (v);
            for (int k = 0; k < numLines; ++k)
            {
                const auto in = (k < 4 ? xl : xr) * inGain * ((k & 1) ? -1.0 : 1.0);
                lines[(size_t) k].buf[(size_t) (w & mask)] = (float) (v[k] + in);
            }
            w = (w + 1) & mask;

            double tl = 0.0, tr = 0.0;
            for (int k = 0; k < numLines; ++k)
            {
                const auto y = outs[k];
                if (k % 2) tr += y * ((k & 2) ? -1.0 : 1.0); else tl += y * ((k & 4) ? -1.0 : 1.0);
            }
            tl *= 0.45; tr *= 0.45;

            auto ol = early * el * 0.4 + tail * tl, orr = early * er * 0.4 + tail * tr;
            ol = outHc[0].process (outLc[0].process (ol));
            orr = outHc[1].process (outLc[1].process (orr));
            const auto mid = 0.5 * (ol + orr), side = 0.5 * (ol - orr) * wd;
            const auto wt = wet0 + dWet * (double) (i + 1); // mélange en rampe sur le bloc
            L[i] = (float) (inL * (1.0 - wt * 0.5) + (mid + side) * wt);
            R[i] = (float) (inR * (1.0 - wt * 0.5) + (mid - side) * wt);
        }
    }

private:
    struct Line { std::vector<float> buf; double len = 1000.0, cur = -1.0, gLow = 0.5, gHigh = 0.5, lp = 0.0, damp = 0.0; };
    struct Ap { std::vector<float> buf; int w = 0, len = 100; };

    double read (const Line& l, double pos) const noexcept
    {
        const auto mask = lineSize - 1;
        const auto fl = std::floor (pos);
        const auto i0 = (int) fl;
        const auto f = pos - fl;
        // Hermite (v0.22) : lecture modulée sans perte d'aigus ni bruit de modulation
        auto at = [&] (int j) { return (double) l.buf[(size_t) (j & mask)]; };
        return hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
    }

    static double allpass (Ap& a, double x, double g) noexcept
    {
        const auto rd = (a.w - a.len + 2048) & 2047;
        const auto d = (double) a.buf[(size_t) rd];
        const auto y = -g * x + d;
        a.buf[(size_t) a.w] = (float) (x + g * y);
        a.w = (a.w + 1) & 2047;
        return y;
    }

    static void hadamard (double* v) noexcept
    {
        for (int h = 1; h < numLines; h <<= 1)
            for (int i = 0; i < numLines; i += h * 2)
                for (int j = i; j < i + h; ++j) { const auto a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
        const auto norm = 1.0 / std::sqrt ((double) numLines);
        for (int i = 0; i < numLines; ++i) v[i] *= norm;
    }

    double sr = 48000.0, lfo = 0.0, wetPrev = -1.0;
    int lineSize = 1, preSize = 1, w = 0, preW = 0;
    std::array<Line, numLines> lines;
    std::vector<float> preL, preR;
    std::array<Ap, 4> diff;
    std::array<SvfEq, 2> outLc, outHc;
};

} // namespace grv::effects
