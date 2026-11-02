/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Écho » — principe des échos à modélisation de caractères (v0.36 : couleurs d'écho de machines
    — studio 80, bande usée, cassette, seau BBD, radio —, lo-fi, groove en avance / en retard, gel) : simple, double, ping-pong,
    bande (pleurage et scintillement, saturation, pertes d'aigus dans la boucle) et analogique
    à circuits BBD (plus sombre, bruit léger, saturation douce). Temps libre ou calé sur le tempo
    (divisions droites, pointées, ternaires), décalage « swing » de la seconde tête, filtres
    coupe-bas et coupe-haut dans la réinjection, diffusion, largeur, effacement sous le signal
    (ducking). Changement de temps sans clic (glissement façon bande).
    Architecture propre.
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"

#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace echo
{
    enum Slot { mode = 0, sync, timeMs, division, ratio, feedback, mix, lowCut, highCut, saturation, wobble,
                diffusion, width, ducking, swing,
                echoColour, lofi, groove, freeze, numSlots }; // v0.36 : à partir de echoColour (anciens projets : Neutre)

    inline const std::vector<std::string>& divisions()
    {
        static const std::vector<std::string> d { "1/64", "1/32", "1/16T", "1/32.", "1/16", "1/8T", "1/16.", "1/8",
                                                  "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/2.", "1 mes." };
        return d;
    }
    inline double divisionQuarters (int i) noexcept
    {
        static constexpr double q[16] { 1.0 / 16, 1.0 / 8, 1.0 / 6, 3.0 / 16, 0.25, 1.0 / 3, 0.375, 0.5,
                                        2.0 / 3, 0.75, 1.0, 4.0 / 3, 1.5, 2.0, 3.0, 4.0 };
        return q[std::clamp (i, 0, 15)];
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "delay";
            m.name = "Ricochet"; m.role = "\xC3\x89" "cho st\xC3\xA9r\xC3\xA9o";
            m.family = "\xC3\x89" "chos"; m.form = "rack";
            m.principle = "\xC3\x89" "cho \xC3\xA0 caract\xC3\xA8res : simple, double, ping-pong, bande, analogique";
            m.params = {
                C ("mode", "Caract\xC3\xA8re", { "Simple", "Double", "Ping-pong", "Bande", "Analogique" }, 2),
                C ("sync", "Synchro", { "Libre", "Tempo" }, 1),
                P ("time", "Temps", "ms", 1, 2500, 375, 0.4f),
                C ("division", "Division", divisions(), 9),
                P ("ratio", "Seconde t\xC3\xAA" "te", "%", 10, 200, 100),
                P ("feedback", "R\xC3\xA9injection", "%", 0, 110, 40),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 30),
                P ("lowcut", "Coupe-bas", "Hz", 20, 2000, 120, 0.4f),
                P ("highcut", "Coupe-haut", "Hz", 1000, 20000, 9000, 0.4f),
                P ("saturation", "Saturation", "%", 0, 100, 15),
                P ("wobble", "Pleurage", "%", 0, 100, 0),
                P ("diffusion", "Diffusion", "%", 0, 100, 0),
                P ("width", "Largeur", "%", 0, 100, 100),
                P ("ducking", "Effacement", "%", 0, 100, 0),
                P ("swing", "Swing", "%", 0, 100, 0),
                C ("echocolour", "Machine", { "Neutre", "Studio 80", "Bande us\xC3\xA9" "e", "Cassette", "Seau BBD", "Radio" }),
                P ("lofi", "Lo-fi", "%", 0, 100, 0),
                P ("groove", "Groove", "%", -50, 50, 0),
                C ("freeze", "Gel", { "Non", "Oui" }),
            };
            m.sections = {
                { "time", "Temps", "delay", { "mode", "sync", "time", "division", "ratio", "swing", "feedback", "mix" }, "time" },
                { "tone", "Couleur", "eq", { "lowcut", "highcut", "saturation", "wobble", "diffusion" }, "tone" },
                { "space", "Espace", "delay", { "width", "ducking", "mix" }, "space" },
                { "machine", "Couleur d'\xC3\xA9" "cho", "delay", { "echocolour", "lofi", "groove", "freeze" }, "tone" },
            };
            m.presets = {
                { "Ping-pong point\xC3\xA9", {} },
                { "Slapback", { { "mode", 0 }, { "sync", 0 }, { "time", 110 }, { "feedback", 5 }, { "mix", 25 } } },
                { "Bande chaude", { { "mode", 3 }, { "division", 10 }, { "feedback", 55 }, { "saturation", 45 }, { "wobble", 35 }, { "highcut", 5000 } } },
                { "Analogique sombre", { { "mode", 4 }, { "division", 7 }, { "feedback", 60 }, { "highcut", 3000 }, { "saturation", 30 } } },
                { "Voix discr\xC3\xA8te", { { "mode", 1 }, { "division", 10 }, { "ratio", 150 }, { "feedback", 25 }, { "mix", 18 }, { "ducking", 60 }, { "lowcut", 250 } } },
                { "Dub infini", { { "mode", 3 }, { "division", 9 }, { "feedback", 98 }, { "saturation", 55 }, { "highcut", 3500 }, { "mix", 40 } } },
                { "Studio 80", { { "echocolour", 1 }, { "mode", 1 }, { "division", 10 }, { "feedback", 35 }, { "lofi", 20 } } },
                { "Bande us\xC3\xA9" "e", { { "echocolour", 2 }, { "mode", 3 }, { "division", 9 }, { "feedback", 55 } } },
                { "Cassette", { { "echocolour", 3 }, { "division", 7 }, { "feedback", 40 }, { "mix", 28 } } },
                { "Radio lointaine", { { "echocolour", 5 }, { "division", 10 }, { "feedback", 45 }, { "lofi", 40 } } },
                { "Groove en retard", { { "groove", 30 }, { "mode", 0 }, { "division", 7 }, { "feedback", 30 } } },
            };
            return m;
        }();
        return d;
    }
}

class Delay final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return echo::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        size = 1;
        while (size < (int) (sr * 5.5)) size <<= 1; // 5,5 s au plus (2,5 s × rapport 200 %)
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset() noexcept override
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0;
        curA = curB = -1.0; wetPrev = -1.0;
        for (auto& f : lc) f.reset();
        for (auto& f : hc) f.reset();
        for (auto& a : ap) a = {};
        duck = 0.0; wobPhase = 0.0; flutPhase = 0.0;
    }

    void process (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        using namespace echo;
        const auto m = (int) p[echo::mode];
        const auto grooveMs = p[groove] / 100.0 * 0.0625 * 60000.0 / std::max (20.0, bpm); // ± 1/64 : en avance ou en retard
        const auto baseMs = std::max (1.0, (p[sync] > 0.5f ? divisionQuarters ((int) p[division]) * 60000.0 / std::max (20.0, bpm) : (double) p[timeMs]) + grooveMs);
        // couleur d'écho (v0.36) : bande passante, usure, bruit selon la machine choisie
        const auto colr = std::clamp ((int) p[echoColour], 0, 5);
        static constexpr double colHc[6] { 20000, 12000, 4500, 7000, 3000, 3200 }, colLc[6] { 0, 0, 0, 80, 0, 400 };
        static constexpr double colWob[6] { 0, 0, 0.5, 0.3, 0.08, 0 }, colSat[6] { 0, 0.05, 0.3, 0.15, 0.2, 0.25 }, colNoise[6] { 0, 0, 1.0e-4, 3.0e-4, 2.0e-4, 1.5e-4 };
        const auto lofiAmt = std::max ((double) p[lofi] / 100.0, colr == 1 ? 0.15 : 0.0);
        const bool frozen = p[freeze] > 0.5f;
        const auto ratioB = p[ratio] / 100.0;
        const auto swingAmt = p[swing] / 100.0;
        const auto targetA = std::clamp (baseMs * 0.001 * sr, 1.0, (double) size - 8.0);
        const auto targetB = std::clamp (baseMs * ratioB * (1.0 + 0.33 * swingAmt) * 0.001 * sr, 1.0, (double) size - 8.0);
        if (curA < 0.0) { curA = targetA; curB = targetB; }
        const auto fb = std::min (1.1, p[feedback] / 100.0);
        const auto wet = p[mix] / 100.0;
        const auto sat = std::min (1.0, p[saturation] / 100.0 + colSat[colr]);
        const auto wob = std::min (1.0, p[wobble] / 100.0 + colWob[colr]);
        const auto diff = p[diffusion] / 100.0;
        const auto wd = p[width] / 100.0;
        const auto duckAmt = p[ducking] / 100.0;
        const bool tape = m == 3, bbd = m == 4;
        const auto hcFreq = std::min (bbd ? std::min ((double) p[highCut], 4500.0) : (double) p[highCut], colHc[colr]);
        const auto lcFreq = std::max ((double) p[lowCut], colLc[colr]);
        // lo-fi : échantillonnage ramené (jusqu'à 1/8) et résolution réduite (jusqu'à 6 bits) dans la boucle
        const auto holdN = 1 + (int) (lofiAmt * lofiAmt * 7.0);
        const auto steps = std::pow (2.0, 16.0 - 10.0 * lofiAmt);
        for (int c = 0; c < 2; ++c)
        {
            lc[c].set (SvfEq::highpass, lcFreq, 0.6, 0.0, sr);
            hc[c].set (SvfEq::lowpass, hcFreq, tape ? 0.6 : 0.707, 0.0, sr);
        }
        const auto glide = 1.0 - std::exp (-1.0 / (0.08 * sr)); // les changements de temps glissent en 80 ms
        const auto duckRel = std::exp (-1.0 / (0.25 * sr));
        const auto mask = size - 1;
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n); // mélange en rampe (v0.22)
        wetPrev = wet;

        for (int i = 0; i < n; ++i)
        {
            const auto wt = wet0 + dWet * (double) (i + 1);
            curA += (targetA - curA) * glide;
            curB += (targetB - curB) * glide;
            // pleurage (lent) et scintillement (rapide) de la bande
            double mod = 0.0;
            if (wob > 0.0 || tape)
            {
                wobPhase += 0.6 / sr; if (wobPhase >= 1.0) wobPhase -= 1.0;
                flutPhase += 7.3 / sr; if (flutPhase >= 1.0) flutPhase -= 1.0;
                const auto depth = (tape ? 0.15 : 0.0) + wob;
                mod = depth * (0.004 * sr * std::sin (kTwoPi * wobPhase) * 0.25 + 0.0006 * sr * std::sin (kTwoPi * flutPhase) * 0.25);
            }
            const auto dA = curA + mod, dB = curB + mod;

            const double inL = L[i], inR = R[i];
            double yl, yr;
            switch (m)
            {
                case 1: yl = read (bufL, dA); yr = read (bufR, dB); break;           // double : deux temps
                case 2: yl = read (bufL, dA); yr = read (bufR, dA); break;           // ping-pong
                default: yl = read (bufL, dA); yr = read (bufR, m == 0 ? dA : dB); break;
            }

            // Boucle de réinjection : filtres, saturation, diffusion
            double fl = hc[0].process (lc[0].process (yl)), fr = hc[1].process (lc[1].process (yr));
            if (sat > 0.0 || tape || bbd)
            {
                const auto drive = 1.0 + sat * 4.0 + (bbd ? 0.5 : 0.0);
                fl = std::tanh (fl * drive) / drive * (1.0 + 0.25 * sat);
                fr = std::tanh (fr * drive) / drive * (1.0 + 0.25 * sat);
            }
            if (bbd || colNoise[colr] > 0.0) { noise = noise * 1664525u + 1013904223u; const auto nz = ((double) (noise >> 9) / 4194304.0 - 1.0) * std::max (bbd ? 2.0e-4 : 0.0, colNoise[colr]); fl += nz; fr += nz; }
            if (lofiAmt > 0.001)
            {
                if (++holdCount >= holdN) { holdCount = 0; holdL = std::round (fl * steps) / steps; holdR = std::round (fr * steps) / steps; }
                fl = holdL; fr = holdR;
            }
            if (diff > 0.0) { fl = allpass (0, fl, diff); fr = allpass (1, fr, diff); }

            double wl, wr;
            if (frozen) { wl = m == 2 ? yr : yl; wr = m == 2 ? yl : yr; }     // gel : la boucle tourne telle quelle, plus d'entrée
            else if (m == 2) { wl = 0.5 * (inL + inR) + fb * fr; wr = fb * fl; } // ping-pong : entrée mono à gauche, croisement
            else { wl = inL + fb * fl; wr = inR + fb * fr; }
            // au-delà de 100 % la boucle reste bornée par la saturation
            if (fb > 0.98 && ! frozen) { wl = std::tanh (wl); wr = std::tanh (wr); }
            bufL[(size_t) w] = (float) wl;
            bufR[(size_t) w] = (float) wr;
            w = (w + 1) & mask;

            // Effacement sous le signal d'entrée
            const auto lvl = std::max (std::abs (inL), std::abs (inR));
            duck = std::max (lvl, duck * duckRel);
            const auto duckGain = 1.0 - duckAmt * std::min (1.0, duck * 3.0);

            // Largeur : mélange vers le mono
            const auto mid = 0.5 * (yl + yr), side = 0.5 * (yl - yr) * wd;
            const auto ol = (mid + side) * duckGain, orr = (mid - side) * duckGain;
            L[i] = (float) (inL * (1.0 - wt * 0.5) + ol * wt);
            R[i] = (float) (inR * (1.0 - wt * 0.5) + orr * wt);
        }
    }

private:
    double read (const std::vector<float>& b, double d) const noexcept
    {
        auto pos = (double) w - d;
        if (pos < 0.0) pos += size;
        const auto i0 = (int) pos & (size - 1);
        const auto f = pos - std::floor (pos);
        const auto xm1 = (double) b[(size_t) ((i0 - 1) & (size - 1))], x0 = (double) b[(size_t) i0];
        const auto x1 = (double) b[(size_t) ((i0 + 1) & (size - 1))], x2 = (double) b[(size_t) ((i0 + 2) & (size - 1))];
        const auto c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2.0 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    struct Ap { std::array<float, 4096> buf {}; int w = 0; };
    double allpass (int c, double x, double amount) noexcept
    {
        // deux passe-tout en série, longueurs premières
        static constexpr int len[2][2] { { 347, 1129 }, { 389, 1201 } };
        const auto g = 0.6 * amount;
        for (int k = 0; k < 2; ++k)
        {
            auto& a = ap[(size_t) (c * 2 + k)];
            const auto L = len[c][k];
            const auto rd = (a.w - L + 4096) & 4095;
            const auto d = (double) a.buf[(size_t) rd];
            const auto y = -g * x + d;
            a.buf[(size_t) a.w] = (float) (x + g * y);
            a.w = (a.w + 1) & 4095;
            x = y;
        }
        return x;
    }

    double sr = 48000.0, curA = -1.0, curB = -1.0, duck = 0.0, wobPhase = 0.0, flutPhase = 0.0, wetPrev = -1.0, holdL = 0.0, holdR = 0.0;
    int size = 1, w = 0, holdCount = 0;
    std::uint32_t noise = 12345u;
    std::vector<float> bufL, bufR;
    std::array<SvfEq, 2> lc, hc;
    std::array<Ap, 4> ap {};
};

} // namespace grv::effects
