/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Atmosphère » — principe des grandes réverbérations « pédales d'ambiance » à 12 algorithmes
    (architecture seulement) : une seule boîte, des algorithmes au caractère marqué.
      - Salle, Plaque : réseau de 8 lignes à retard (matrice de Hadamard) aux longueurs modulées ;
      - Chatoiement : un transpositeur dans la boucle (octave, quinte, quarte…) fait monter la
        queue vers l'aigu à chaque passage ;
      - Nuage : lignes longues, diffusion maximale et modulation profonde (nappe sans attaque) ;
      - Floraison : l'entrée traverse de longs diffuseurs avant le réseau, le son « éclot ».
    v0.35, les 7 autres :
      - Pièce : petit réseau dense + premières réflexions ;
      - Ressort : deux ressorts dispersifs (gauche / droite), tension réglable ;
      - Gonflement : à chaque attaque, la réverbération (et, au choix, le direct) repart de zéro et monte en volume ;
      - Chorale : la queue passe dans trois formants de voix (voyelles A E I O U) ;
      - Magnéto : écho à 4 têtes de lecture qui nourrit la réverbération ;
      - Non linéaire : la queue suit une forme à chaque attaque (porte, inversée, montée, descente) ;
      - Réflexions : premières réflexions d'une pièce (12 échos) et un peu de queue.
    « Traîne au contournement » : quand l'insert est contourné, la queue continue sans nouvelle entrée.
    Pré-délai jusqu'à 500 ms, déclin jusqu'à 60 s, gel (queue infinie), coupe-bas / coupe-haut
    de la queue, largeur. Aucune allocation sur le fil audio.
    Architecture propre (aucune reprise de code ni de réglages d'un produit existant).
*/

#pragma once

#include "../dsp/EqFilters.h"
#include "../engine/Module.h"
#include "FxBanks.h"
#include "SpaceFx.h"   // v0.35 : ressorts et ligne de bande

#include <array>
#include <vector>

namespace grv::effects
{

using namespace grv::engine;
using namespace grv::dsp;

namespace atmos
{
    enum Slot { mode = 0, decay, size, predelay, diffusion, modulation, shimmer, pitch, tone, lowCut, highCut, freeze, mix, width,
                rise, swellDry, vowel, formant, tension, spacing, repeats, shape, early, persist, numSlots }; // v0.35 : à partir de rise
    enum Mode { hall = 0, plate, shimmerM, cloud, bloom, room, springM, swell, chorale, magneto, nonlin, reflect, numModes };

    inline double pitchRatio (int i) noexcept
    {
        static constexpr double r[] { 2.0, 1.4983070768766815, 1.3348398541700344, 0.5, 4.0 }; // +12, +7, +5, -12, +24
        return r[std::clamp (i, 0, 4)];
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            ModuleDesc m;
            m.type = "atmos";
            m.name = "Atmosphere"; m.role = "R\xC3\xA9verb\xC3\xA9ration d'ambiance";
            m.family = "R\xC3\xA9" "verb\xC3\xA9rations"; m.form = "module"; // v0.35.1 : 24 réglages, 12 algorithmes : interface dédiée
            m.principle = "R\xC3\xA9verb\xC3\xA9ration d'ambiance \xC3\xA0 12 algorithmes : salle, plaque, chatoiement, nuage, floraison, pi\xC3\xA8" "ce, ressort, gonflement, chorale, magn\xC3\xA9to, non lin\xC3\xA9" "aire, r\xC3\xA9" "flexions ; gel, tra\xC3\xAEne";
            m.params = {
                C ("mode", "Algorithme", { "Salle", "Plaque", "Chatoiement", "Nuage", "Floraison", "Pi\xC3\xA8" "ce", "Ressort", "Gonflement",
                                           "Chorale", "Magn\xC3\xA9to", "Non lin\xC3\xA9" "aire", "R\xC3\xA9" "flexions" }, 0),
                P ("decay", "D\xC3\xA9" "clin", "s", 0.3f, 60, 4, 0.35f),
                P ("size", "Taille", "%", 0, 100, 60),
                P ("predelay", "Pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 500, 20, 0.5f),
                P ("diffusion", "Diffusion", "%", 0, 100, 80),
                P ("modulation", "Modulation", "%", 0, 100, 30),
                P ("shimmer", "Chatoiement", "%", 0, 100, 0),
                C ("pitch", "Transposition", { "+12", "+7", "+5", "-12", "+24" }, 0),
                P ("tone", "Brillance", "Hz", 800, 18000, 7000, 0.4f),
                P ("lowcut", "Coupe-bas", "Hz", 20, 1000, 100, 0.4f),
                P ("highcut", "Coupe-haut", "Hz", 1500, 20000, 16000, 0.4f),
                C ("freeze", "Gel", { "Non", "Oui" }),
                P ("mix", "M\xC3\xA9lange", "%", 0, 100, 35),
                P ("width", "Largeur", "%", 0, 150, 100),
                P ("rise", "Mont\xC3\xA9" "e", "ms", 20, 4000, 600, 0.4f),
                C ("swelldry", "Gonfler le direct", { "Non", "Oui" }),
                C ("vowel", "Voyelle", { "A", "E", "I", "O", "U" }),
                P ("formant", "R\xC3\xA9sonance des voix", "%", 0, 100, 60),
                P ("tension", "Tension des ressorts", "%", 0, 100, 50),
                P ("spacing", "\xC3\x89" "cart des t\xC3\xAA" "tes", "ms", 30, 400, 150, 0.6f),
                P ("repeats", "R\xC3\xA9p\xC3\xA9titions", "%", 0, 95, 40),
                C ("shape", "Forme", { "Porte", "Invers\xC3\xA9" "e", "Mont\xC3\xA9" "e", "Descente" }),
                P ("early", "Premi\xC3\xA8res r\xC3\xA9" "flexions", "%", 0, 100, 50),
                C ("persist", "Tra\xC3\xAEne", { "Non", "Oui" }),
            };
            m.sections = {
                { "space", "Espace", "reverb", { "mode", "decay", "size", "predelay", "diffusion", "modulation", "mix", "width" }, "space" },
                { "algo", "Caract\xC3\xA8re de l'algorithme", "reverb", { "rise", "swelldry", "vowel", "formant", "tension", "spacing", "repeats", "shape", "early" }, "algo" },
                { "shimmer", "Chatoiement et couleur", "eq", { "shimmer", "pitch", "tone", "lowcut", "highcut", "freeze", "persist" }, "tone" },
            };
            m.presets = {
                { "Grande salle", {} },
                { "Plaque brillante", { { "mode", 1 }, { "decay", 2.5f }, { "size", 40 }, { "tone", 11000 }, { "predelay", 10 } } },
                { "Chatoiement d'octave", { { "mode", 2 }, { "decay", 9 }, { "shimmer", 55 }, { "pitch", 0 }, { "mix", 40 }, { "modulation", 45 } } },
                { "Chatoiement en quinte", { { "mode", 2 }, { "decay", 7 }, { "shimmer", 40 }, { "pitch", 1 }, { "mix", 35 } } },
                { "Nuage infini", { { "mode", 3 }, { "decay", 25 }, { "size", 90 }, { "modulation", 70 }, { "tone", 5000 }, { "mix", 45 } } },
                { "Floraison lente", { { "mode", 4 }, { "decay", 12 }, { "size", 80 }, { "diffusion", 95 }, { "mix", 40 } } },
                { "Profondeurs (-12)", { { "mode", 2 }, { "decay", 10 }, { "shimmer", 45 }, { "pitch", 3 }, { "tone", 4000 }, { "mix", 40 } } },
                { "Gel", { { "mode", 3 }, { "decay", 30 }, { "freeze", 1 }, { "mix", 50 } } },
                { "Petite pi\xC3\xA8" "ce", { { "mode", 5 }, { "decay", 0.8f }, { "size", 30 }, { "early", 60 }, { "mix", 25 } }, "Pi\xC3\xA8" "ces" },
                { "Studio de bois", { { "mode", 5 }, { "decay", 1.2f }, { "size", 45 }, { "tone", 5000 }, { "early", 40 } }, "Pi\xC3\xA8" "ces" },
                { "Ressorts d'ampli", { { "mode", 6 }, { "decay", 2.5f }, { "tension", 50 }, { "tone", 4500 }, { "mix", 30 } }, "Ressorts" },
                { "Ressorts tendus", { { "mode", 6 }, { "decay", 4 }, { "tension", 85 }, { "mix", 40 } }, "Ressorts" },
                { "Mont\xC3\xA9" "e de nappe", { { "mode", 7 }, { "decay", 6 }, { "rise", 1500 }, { "mix", 50 } }, "Gonflements" },
                { "Archet (direct gonfl\xC3\xA9)", { { "mode", 7 }, { "decay", 4 }, { "rise", 400 }, { "swelldry", 1 }, { "mix", 40 } }, "Gonflements" },
                { "Ch\xC5\x93ur en A", { { "mode", 8 }, { "decay", 8 }, { "vowel", 0 }, { "formant", 70 }, { "mix", 45 } }, "Chorales" },
                { "Ch\xC5\x93ur en O", { { "mode", 8 }, { "decay", 10 }, { "vowel", 3 }, { "formant", 55 }, { "modulation", 50 }, { "mix", 45 } }, "Chorales" },
                { "Bande et espace", { { "mode", 9 }, { "decay", 3 }, { "spacing", 180 }, { "repeats", 45 }, { "mix", 40 } }, "Magn\xC3\xA9to" },
                { "Dub spatial", { { "mode", 9 }, { "decay", 6 }, { "spacing", 280 }, { "repeats", 70 }, { "tone", 4000 }, { "mix", 45 } }, "Magn\xC3\xA9to" },
                { "Porte des ann\xC3\xA9" "es 80", { { "mode", 10 }, { "decay", 0.35f }, { "shape", 0 }, { "mix", 40 } }, "Non lin\xC3\xA9" "aires" },
                { "Inversion", { { "mode", 10 }, { "decay", 0.8f }, { "shape", 1 }, { "mix", 50 } }, "Non lin\xC3\xA9" "aires" },
                { "Pi\xC3\xA8" "ce r\xC3\xA9" "elle", { { "mode", 11 }, { "size", 40 }, { "early", 80 }, { "mix", 30 } }, "Pi\xC3\xA8" "ces" },
                { "Hangar", { { "mode", 11 }, { "size", 95 }, { "early", 55 }, { "decay", 3 }, { "mix", 35 } }, "Pi\xC3\xA8" "ces" },
            };
            grv::effects::addFxBank (m); // v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires
            return m;
        }();
        return d;
    }
}

class Atmos final : public Effect
{
public:
    const ModuleDesc& desc() const noexcept override { return atmos::desc(); }

    void prepare (double s, int) override
    {
        sr = s;
        lineSize = pow2Above ((int) (0.45 * sr));          // lignes : 45 ms × taille 2,4 × nuage 1,6 (marge comprise)
        for (auto& l : lines) l.assign ((size_t) lineSize, 0.0f);
        preSize = pow2Above ((int) (0.8 * sr));            // pré-délai 500 ms + premières réflexions (≤ 200 ms)
        preL.assign ((size_t) preSize, 0.0f); preR.assign ((size_t) preSize, 0.0f);
        apSize = pow2Above ((int) (0.25 * sr));
        for (auto& a : aps) a.assign ((size_t) apSize, 0.0f);
        shiftSize = pow2Above ((int) (0.2 * sr));
        shiftBuf.assign ((size_t) shiftSize, 0.0f);
        springs[0].prepare (s, 31.0); springs[1].prepare (s, 37.0);
        tape.init ((int) (s * 1.65) + 64);                 // 4 têtes × 400 ms + pleurage
        reset();
    }

    void reset() noexcept override
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        std::fill (preL.begin(), preL.end(), 0.0f); std::fill (preR.begin(), preR.end(), 0.0f);
        for (auto& a : aps) std::fill (a.begin(), a.end(), 0.0f);
        std::fill (shiftBuf.begin(), shiftBuf.end(), 0.0f);
        lw = pw = aw = sw = 0;
        damp.fill (0.0); modPhase.fill (0.0); shiftPhase = 0.0; shimmerFb = 0.0; lenCur.fill (-1.0); wetPrev = -1.0;
        for (auto& f : lc) f.reset();
        for (auto& f : hc) f.reset();
        for (auto& f : vox) f.reset();
        for (int i = 0; i < 8; ++i) modPhase[(size_t) i] = 0.125 * i;
        for (auto& x : springs) x.reset();
        tape.clear(); tapeLp = 0.0; tapePh = 0.0;
        erLpL = erLpR = 0.0; detEnv = 0.0; armed = true; swellG = 1.0; shapeT = 1.0e9; shapeEnv = 0.0;
        tailOnly = false; asleep = false; quietSamples = 0;
    }

    /** Traîne : contourné, l'effet continue de faire sonner sa queue si « Traîne » est sur Oui. */
    bool trails (const float* p) const noexcept override { return p[atmos::persist] > 0.5f; }
    void processTrails (float* L, float* R, int n, const float* p, double bpm) noexcept override
    {
        if (asleep) return;
        tailOnly = true;
        process (L, R, n, p, bpm);
        tailOnly = false;
    }

    void process (float* L, float* R, int n, const float* p, double) noexcept override
    {
        using namespace atmos;
        if (! tailOnly) { asleep = false; quietSamples = 0; }
        const auto md = std::clamp ((int) p[mode], 0, numModes - 1);
        const bool frozen = p[freeze] > 0.5f;
        static constexpr double modeSize[numModes] { 1.0, 0.6, 1.0, 1.6, 1.0, 0.35, 1.0, 1.0, 1.0, 1.0, 1.0, 0.45 };
        const auto sizeK = (0.5 + 1.5 * p[size] / 100.0) * modeSize[md];
        // non linéaire : la forme dessine la queue ; le réseau sonne long en dessous
        const auto shapeLen = std::clamp ((double) p[decay], 0.08, 3.0);
        const auto rt = md == nonlin ? std::max (2.5, shapeLen * 3.0) : (double) p[decay];
        const auto diff = p[diffusion] / 100.0 * (md == cloud || md == bloom || md == room ? 1.0 : 0.85);
        const auto modDepth = (p[modulation] / 100.0) * (md == cloud ? 3.0 : 1.0) * 0.0018 * sr;   // jusqu'à ~1,8 ms (5,4 en nuage)
        const auto shim = md == shimmerM ? std::max (0.25, p[shimmer] / 100.0) : p[shimmer] / 100.0;
        const auto ratio = pitchRatio ((int) p[pitch]);
        const auto wet = p[mix] / 100.0, wd = p[width] / 100.0;
        const auto dampCoef = std::exp (-kTwoPi * std::clamp ((double) p[tone] * (md == atmos::plate ? 1.4 : 1.0), 200.0, 0.45 * sr) / sr);
        const auto preSamples = std::clamp ((double) p[predelay] * 0.001 * sr, 0.0, (double) preSize - 4.0);
        for (int c = 0; c < 2; ++c)
        {
            lc[c].set (SvfEq::highpass, p[lowCut], 0.7, 0.0, sr);
            hc[c].set (SvfEq::lowpass, p[highCut], 0.7, 0.0, sr);
        }
        // longueurs des 8 lignes (ms, nombres premiers entre eux) et gains pour le RT60
        static constexpr double baseMs[8] { 29.7, 34.3, 38.9, 43.1, 47.9, 53.3, 59.1, 63.7 };
        double len[8], g[8];
        for (int k = 0; k < 8; ++k)
        {
            len[k] = std::clamp (baseMs[k] * 0.001 * sr * sizeK, 8.0, (double) lineSize - modDepth - 8.0);
            // gel : perte infime par passage (−0,01 dB/s) — l'erreur de calcul ne peut pas s'accumuler
            g[k] = frozen ? 0.99999 : std::pow (10.0, -3.0 * len[k] / (std::max (0.1, rt) * sr));
            if (lenCur[(size_t) k] < 0.0) lenCur[(size_t) k] = len[k];
        }
        const auto inGain = frozen ? 0.0 : 1.0;
        const auto lenGlide = 1.0 - std::exp (-1.0 / (0.12 * sr)); // la taille glisse en ~120 ms (plus de saut)
        const auto wet0 = wetPrev < 0.0 ? wet : wetPrev, dWet = (wet - wet0) / std::max (1, n);
        wetPrev = wet;
        const auto apG = 0.45 + 0.3 * diff;
        static constexpr double apMs[8] { 4.77, 3.59, 12.73, 9.31, 31.7, 43.9, 67.3, 89.1 }; // 4 courts + 4 longs (floraison)
        const int apCount = md == bloom ? 8 : 4;
        const auto apScale = md == bloom ? 1.0 + p[size] / 100.0 : 1.0;
        const auto mask = lineSize - 1, pmask = preSize - 1, amask = apSize - 1, smask = shiftSize - 1;
        const auto win = std::min (0.08 * sr, (double) shiftSize - 4.0);
        const auto phaseInc = (1.0 - ratio) / win;

        // ressorts : gain de boucle tiré du déclin (boucle ≈ 31 ms + dispersion)
        const auto springFb = std::clamp (std::pow (10.0, -3.0 * 0.045 / std::max (0.1, (double) p[decay])), 0.3, 0.985);
        const auto springTen = p[tension] / 100.0;
        const auto springTone = 1.0 - std::exp (-kTwoPi * std::clamp ((double) p[tone], 1500.0, 6500.0) / sr);
        // gonflement / non linéaire : détection des attaques
        const auto riseStep = 1.0 / (std::max (0.02, p[rise] * 0.001) * sr);
        const auto detRel = std::exp (-1.0 / (0.05 * sr));
        const bool swellDirect = md == swell && p[swellDry] > 0.5f;
        const auto shp = std::clamp ((int) p[shape], 0, 3);
        const auto shapeFade = 1.0 / (0.012 * sr);
        // chorale : 3 formants par voyelle (Hz) et leurs poids
        static constexpr double formants[5][3] { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2250, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        static constexpr double fGain[3] { 1.0, 0.63, 0.35 };
        const auto vw = std::clamp ((int) p[vowel], 0, 4);
        const auto fq = 4.0 + 14.0 * p[formant] / 100.0;
        if (md == chorale)
            for (int c = 0; c < 2; ++c)
                for (int f = 0; f < 3; ++f) vox[(size_t) (c * 3 + f)].set (SvfEq::bandpass, formants[vw][f] * (c == 0 ? 1.0 : 1.015), fq, 0.0, sr);
        // magnéto : 4 têtes
        const auto headLen = std::clamp ((double) p[spacing], 30.0, 400.0) * 0.001 * sr;
        const auto rep = p[repeats] / 100.0;
        const auto tapeLpC = 1.0 - std::exp (-kTwoPi * 5500.0 / sr);
        // premières réflexions (pièce, réflexions) : 12 échos alternés gauche / droite
        static constexpr double erMs[12] { 3.1, 4.7, 7.9, 9.3, 12.7, 15.1, 19.3, 23.9, 29.1, 33.7, 41.3, 47.9 };
        static constexpr double erG[12] { 0.9, 0.85, 0.75, 0.7, 0.62, 0.58, 0.5, 0.45, 0.38, 0.33, 0.27, 0.22 };
        const bool useEr = md == room || md == reflect;
        const auto erScale = (0.5 + 1.5 * p[size] / 100.0) * 0.001 * sr * 2.0;
        const auto erAmt = p[early] / 100.0;
        const auto erLpC = 1.0 - dampCoef;
        double blockPeak = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double inL = L[i], inR = R[i];
            const double feedL = tailOnly ? 0.0 : inL, feedR = tailOnly ? 0.0 : inR;
            // détection d'attaque (gonflement, non linéaire)
            const auto lvl = std::max (std::abs (feedL), std::abs (feedR));
            detEnv = lvl > detEnv ? lvl : detEnv * detRel;
            if (armed && detEnv > 0.01) { armed = false; swellG = 0.0; shapeT = 0.0; }
            else if (! armed && detEnv < 0.003) armed = true;
            if (md == swell) swellG = std::min (1.0, swellG + riseStep);
            // pré-délai
            preL[(size_t) pw] = (float) feedL; preR[(size_t) pw] = (float) feedR;
            const auto prd = (pw - (int) preSamples) & pmask;
            double x = 0.5 * ((double) preL[(size_t) prd] + (double) preR[(size_t) prd]) * inGain;
            // premières réflexions lues dans le pré-délai
            double erL = 0.0, erR = 0.0;
            if (useEr)
            {
                for (int t = 0; t < 12; ++t)
                {
                    const auto d = std::min ((int) (preSamples + erMs[t] * erScale), preSize - 4);
                    const auto j = (size_t) ((pw - d) & pmask);
                    const auto v = erG[t] * 0.5 * ((double) preL[j] + (double) preR[j]);
                    if (t & 1) erR += v; else erL += v;
                }
                erLpL += (erL - erLpL) * erLpC; erLpR += (erR - erLpR) * erLpC;
                erL = erLpL * 0.45; erR = erLpR * 0.45;
                x += 0.4 * (erL + erR); // les réflexions nourrissent aussi la queue (densité)
            }
            pw = (pw + 1) & pmask;
            // magnéto : la bande passe avant la réverbération
            double echoL = 0.0, echoR = 0.0;
            if (md == magneto)
            {
                tapePh += 0.6 / sr; if (tapePh >= 1.0) tapePh -= 1.0;
                const auto wob = 1.0 + 0.002 * fastSin2Pi (tapePh);
                double hs[4];
                for (int h = 0; h < 4; ++h) hs[h] = tape.atFrac (headLen * (h + 1) * wob);
                const auto sum = (hs[0] + hs[1] + hs[2] + hs[3]) * 0.5;
                tapeLp += (sum - tapeLp) * tapeLpC;
                tape.push (std::tanh (x + tapeLp * rep * 0.9));
                echoL = (hs[0] + hs[2]) * 0.5; echoR = (hs[1] + hs[3]) * 0.5;
                x = x * 0.35 + (echoL + echoR) * 0.6;
            }
            // chatoiement : la queue transposée revient à l'entrée (pas pendant le gel : v0.22, la
            // queue gelée s'emballait jusqu'à l'écrêtage)
            if (! frozen) x += shimmerFb * shim * 0.55;

            double yl = 0.0, yr = 0.0;
            if (md == springM)
            {
                // deux ressorts dispersifs : pas de réseau
                const auto xs = x * (frozen ? 0.0 : 0.6);
                const auto fb = frozen ? 0.995 : springFb;
                yl = springs[0].process (xs, springTen, fb, springTone, 1.0);
                yr = springs[1].process (xs, springTen, fb, springTone, 1.0);
            }
            else
            {
                // diffuseurs d'entrée (passe-tout en série)
                for (int a = 0; a < apCount; ++a)
                {
                    const auto dl = std::clamp ((int) (apMs[a] * 0.001 * sr * apScale), 4, apSize - 2);
                    auto& b = aps[(size_t) a];
                    const auto rd = (aw - dl) & amask;
                    const auto dv = (double) b[(size_t) rd];
                    const auto y = -apG * x + dv;
                    b[(size_t) aw] = (float) (x + apG * y);
                    x = y;
                }
                aw = (aw + 1) & amask;

                // lecture des 8 lignes (longueurs modulées), amortissement des aigus
                double o[8];
                for (int k = 0; k < 8; ++k)
                {
                    modPhase[(size_t) k] += (0.07 + 0.031 * k) / sr;
                    if (modPhase[(size_t) k] >= 1.0) modPhase[(size_t) k] -= 1.0;
                    auto& lc_ = lenCur[(size_t) k];
                    lc_ += (len[k] - lc_) * lenGlide;
                    const auto d = lc_ + modDepth * (0.5 + 0.5 * fastSin2Pi (modPhase[(size_t) k]));
                    auto pos = (double) lw - d;
                    if (pos < 0.0) pos += lineSize;
                    const auto i0 = (int) pos;
                    const auto f = pos - (double) i0;
                    const auto& line = lines[(size_t) k];
                    // Hermite (v0.22) : la modulation ne ternit plus la queue
                    auto at = [&] (int j) { return (double) line[(size_t) (j & mask)]; };
                    const auto v = hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
                    damp[(size_t) k] = frozen ? v : v + (damp[(size_t) k] - v) * dampCoef;
                    o[k] = damp[(size_t) k];
                }
                // matrice de Hadamard 8 × 8 (transformée rapide), normalisée
                double h[8];
                for (int k = 0; k < 8; ++k) h[k] = o[k];
                for (int span = 1; span < 8; span <<= 1)
                    for (int a = 0; a < 8; a += span * 2)
                        for (int b = a; b < a + span; ++b) { const auto u = h[b], v = h[b + span]; h[b] = u + v; h[b + span] = u - v; }
                for (int k = 0; k < 8; ++k)
                {
                    auto fbv = h[k] * 0.35355339059327373 * g[k] + x * (k & 1 ? -0.35 : 0.35);
                    if (frozen) fbv = std::clamp (fbv, -4.0, 4.0);
                    lines[(size_t) k][(size_t) lw] = (float) fbv;
                }
                lw = (lw + 1) & mask;

                // sortie stéréo : lignes paires à gauche, impaires à droite
                yl = (o[0] - o[2] + o[4] - o[6]) * 0.5; yr = (o[1] - o[3] + o[5] - o[7]) * 0.5;
            }

            // transpositeur (deux têtes, fenêtres en sinus²) : nourrit le chatoiement
            if (shim > 0.0)
            {
                shiftBuf[(size_t) sw] = (float) (0.5 * (yl + yr));
                shiftPhase += phaseInc;
                shiftPhase -= std::floor (shiftPhase);
                double s = 0.0;
                for (int hd = 0; hd < 2; ++hd)
                {
                    auto ph = shiftPhase + 0.5 * hd; ph -= std::floor (ph);
                    const auto d = 2.0 + ph * win;
                    auto pos = (double) sw - d; if (pos < 0.0) pos += shiftSize;
                    const auto i0 = (int) pos; const auto f = pos - (double) i0;
                    auto at = [&] (int j) { return (double) shiftBuf[(size_t) (j & smask)]; };
                    const auto v = hermite4 (at (i0 - 1), at (i0), at (i0 + 1), at (i0 + 2), f);
                    const auto wv = fastSin2Pi (0.5 * ph); // sin(π·ph)
                    s += v * wv * wv;
                }
                sw = (sw + 1) & smask;
                shimmerFb = std::clamp (s, -2.0, 2.0);
            }
            else shimmerFb = 0.0;

            // couleurs propres aux algorithmes
            if (md == chorale)
            {
                double vl = 0.0, vr = 0.0;
                for (int f = 0; f < 3; ++f) { vl += fGain[f] * vox[(size_t) f].process (yl); vr += fGain[f] * vox[(size_t) (3 + f)].process (yr); }
                const auto gv = 0.9 + 1.4 * p[formant] / 100.0;
                yl = 0.2 * yl + vl * gv; yr = 0.2 * yr + vr * gv;
            }
            else if (md == magneto) { yl = yl * 0.8 + echoL * 0.6; yr = yr * 0.8 + echoR * 0.6; }
            else if (md == room) { yl += erL * erAmt * 1.6; yr += erR * erAmt * 1.6; }
            else if (md == reflect) { yl = erL * (0.6 + 1.6 * erAmt) + yl * (1.0 - erAmt) * 0.7; yr = erR * (0.6 + 1.6 * erAmt) + yr * (1.0 - erAmt) * 0.7; }
            else if (md == nonlin)
            {
                // forme de la queue depuis la dernière attaque
                shapeT += 1.0 / sr;
                const auto u = shapeT / shapeLen;
                double target = 0.0;
                if (u < 1.0) target = shp == 0 ? 1.0 : shp == 1 ? u * u : shp == 2 ? u : 1.0 - u;
                // la porte ferme en 12 ms ; les autres formes suivent leur courbe
                shapeEnv = target > shapeEnv || shp != 0 ? (u < 1.0 ? target : std::max (0.0, shapeEnv - shapeFade)) : std::max (target, shapeEnv - shapeFade);
                yl *= shapeEnv * 1.4; yr *= shapeEnv * 1.4;
            }

            // égalisation de la queue, largeur, mélange
            double ql = hc[0].process (lc[0].process (yl)), qr = hc[1].process (lc[1].process (yr));
            const auto mid = 0.5 * (ql + qr), side = 0.5 * (ql - qr) * wd;
            ql = mid + side; qr = mid - side;
            if (md == swell) { const auto sg = swellG * swellG; ql *= sg; qr *= sg; } // la queue monte en volume à chaque attaque
            const auto wt = wet0 + dWet * (double) (i + 1); // mélange en rampe sur le bloc
            if (tailOnly)
            {
                // contourné : le direct passe entier, la queue s'y ajoute
                L[i] = (float) (inL + ql * wt);
                R[i] = (float) (inR + qr * wt);
                blockPeak = std::max (blockPeak, std::max (std::abs (ql), std::abs (qr)));
            }
            else
            {
                const auto dg = swellDirect ? swellG * swellG : 1.0;
                L[i] = (float) (inL * (1.0 - wt) * dg + ql * wt);
                R[i] = (float) (inR * (1.0 - wt) * dg + qr * wt);
            }
        }
        // traîne finie : on cesse de calculer (le réseau garde son état, silencieux)
        if (tailOnly && ! frozen)
        {
            quietSamples = blockPeak < 1.0e-5 ? quietSamples + n : 0;
            if (quietSamples > (int) (sr * 0.5)) asleep = true;
        }
    }

private:
    static int pow2Above (int n) noexcept { int s = 1; while (s < n) s <<= 1; return s; }

    double sr = 48000.0, shiftPhase = 0.0, shimmerFb = 0.0, wetPrev = -1.0;
    std::array<double, 8> lenCur { -1, -1, -1, -1, -1, -1, -1, -1 };
    int lineSize = 1, preSize = 1, apSize = 1, shiftSize = 1, lw = 0, pw = 0, aw = 0, sw = 0;
    std::array<std::vector<float>, 8> lines;
    std::array<std::vector<float>, 8> aps;
    std::vector<float> preL, preR, shiftBuf;
    std::array<double, 8> damp {}, modPhase {};
    std::array<SvfEq, 2> lc, hc;
    // v0.35
    std::array<Spring, 2> springs;
    std::array<SvfEq, 6> vox;
    DLine tape;
    double tapeLp = 0.0, tapePh = 0.0, erLpL = 0.0, erLpR = 0.0, detEnv = 0.0, swellG = 1.0, shapeT = 1.0e9, shapeEnv = 0.0;
    bool armed = true, tailOnly = false, asleep = false;
    int quietSamples = 0;
};

} // namespace grv::effects
