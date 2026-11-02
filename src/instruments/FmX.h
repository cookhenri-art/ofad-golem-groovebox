/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Komodo Fuel » — principe des synthés FM modernes à algorithme libre (architecture reprise ;
    ni nom, ni façade, ni son, ni forme d'onde d'origine) :
      - 6 opérateurs ; l'algorithme n'est pas choisi dans une liste : une matrice 6 × 6 dit combien
        chaque opérateur module chacun des autres (diagonale = rétroaction ; les boucles sont
        permises, lues avec un échantillon de retard) ; chaque opérateur a son niveau de sortie ;
      - 12 formes d'onde calculées par opérateur, balayées en continu (sinus, demi-sinus, sinus
        redressé, quarts, sinus doublé, carré doux, scie, triangle, impulsion, octave, marches) ;
      - rapport (0,5 à 32) + fin, ou fréquence fixe ; sensibilité à la vélocité ;
      - une enveloppe à 6 étapes par opérateur (retard, attaque, tenue, déclin, maintien,
        relâchement) avec boucle attaque → déclin tant que la touche est tenue ;
      - filtre multimode résonant par voix (passe-bas / bande / haut) et son enveloppe ;
      - 2 LFO, 1 enveloppe et 16 routages (modulation commune), arpège ; 16 voix.
    Modèle : tables de 2048 points calculées au démarrage, lecture interpolée, FM par la phase ;
    hauteur par bloc, coefficients du filtre tous les 8 échantillons ; aucune latence ajoutée.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

#include <array>
#include <cmath>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace fmx
{
    constexpr int numOps = 6, numWaves = 12, tableSize = 2048;
    enum Global { voices = 0, vMode, glide, tune, level, spread, fType, fCut, fRes, fEnv, fKey, fA, fD, fS, fR, numGlobal };
    enum OpField { oWave = 0, oCoarse, oFine, oFixed, oFixHz, oVel, oDly, oAtt, oHld, oDec, oSus, oRel, oLoop, oOut, opStride };
    constexpr int opBase = numGlobal;
    constexpr int matBase = opBase + numOps * opStride;
    inline constexpr int opParam (int op, int field) noexcept { return opBase + op * opStride + field; }
    /** Modulation de l'opérateur `from` vers l'opérateur `to` (diagonale : rétroaction). */
    inline constexpr int matParam (int from, int to) noexcept { return matBase + from * numOps + to; }

    inline const std::array<double, 18>& coarseRatios()
    {
        static const std::array<double, 18> r { 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 20, 24, 32 };
        return r;
    }

    /** Les 12 formes, calculées (aucune table d'origine). */
    inline const std::array<std::array<float, tableSize + 1>, numWaves>& waves()
    {
        static const auto t = []
        {
            std::array<std::array<float, tableSize + 1>, numWaves> w {};
            for (int i = 0; i <= tableSize; ++i)
            {
                const double x = (double) (i % tableSize) / tableSize, s = std::sin (kTwoPi * x);
                double v[numWaves];
                v[0] = s;                                                         // sinus
                v[1] = x < 0.5 ? s : 0.0;                                         // demi-sinus
                v[2] = std::abs (s) * 2.0 - 1.27;                                 // sinus redressé (centré)
                v[3] = (x < 0.25 || (x >= 0.5 && x < 0.75)) ? std::abs (s) : 0.0;  // quarts
                v[4] = x < 0.5 ? std::sin (kTwoPi * 2.0 * x) : 0.0;              // sinus doublé, une demi-période sur deux
                v[5] = std::tanh (4.0 * s) / std::tanh (4.0);                     // carré doux
                double saw = 0.0; for (int k = 1; k <= 12; ++k) saw += std::sin (kTwoPi * k * x) / k;
                v[6] = saw * 0.6;                                                 // scie (12 partiels)
                v[7] = 1.0 - 4.0 * std::abs (x - 0.5) ;                           // triangle (décalé d'un quart plus bas)
                v[7] = x < 0.25 ? 4.0 * x : x < 0.75 ? 2.0 - 4.0 * x : 4.0 * x - 4.0;
                v[8] = std::tanh (5.0 * (std::sin (kTwoPi * x) - 0.7)) * 0.6 + 0.3; // impulsion étroite douce
                v[9] = (s + 0.6 * std::sin (kTwoPi * 2.0 * x)) / 1.4;             // sinus + octave
                v[10] = (s + 0.5 * std::sin (kTwoPi * 3.0 * x) + 0.3 * std::sin (kTwoPi * 5.0 * x)) / 1.45; // impair
                v[11] = std::round (s * 4.0) / 4.0;                               // marches (9 niveaux)
                for (int k = 0; k < numWaves; ++k) w[(size_t) k][(size_t) i] = (float) v[k];
            }
            return w;
        }();
        return t;
    }
    inline const std::vector<std::string>& waveNames()
    {
        static const std::vector<std::string> n { "Sinus", "Demi-sinus", "Redress\xC3\xA9", "Quarts", "Doubl\xC3\xA9", "Carr\xC3\xA9 doux",
                                                  "Scie", "Triangle", "Impulsion", "Octave", "Impair", "Marches" };
        return n;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "fmx";
            m.name = "Komodo Fuel";
            m.family = "Synth\xC3\xA9s FM";
            m.principle = "FM \xC3\xA0 6 op\xC3\xA9rateurs \xC3\xA0 algorithme libre (matrice 6 \xC3\x97 6), 12 formes d'onde balay\xC3\xA9" "es, enveloppes \xC3\xA0 6 \xC3\xA9tapes en boucle, filtre multimode";
            std::vector<std::string> coarse;
            for (auto r : coarseRatios()) { char b[12]; std::snprintf (b, sizeof b, "%g", r); std::string s (b); for (auto& c : s) if (c == '.') c = ','; coarse.push_back (s); }
            m.params = {
                I ("voices", "Voix", "", 1, 16, 8),
                C ("vmode", "Jeu", { "Poly", "Mono", "Mono legato" }),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("tune", "Transposition", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -30, 6, -18),
                P ("spread", "Largeur st\xC3\xA9r\xC3\xA9o", "%", 0, 100, 25),
                C ("ftype", "Filtre", { "Aucun", "Passe-bas", "Passe-bande", "Passe-haut" }),
                P ("fcut", "Coupure", "Hz", 20, 18000, 3000, 0.3f),
                P ("fres", "R\xC3\xA9sonance", "%", 0, 100, 20),
                P ("fenv", "Qt\xC3\xA9 env.", "%", -100, 100, 0),
                P ("fkey", "Clavier", "%", 0, 100, 50),
                P ("fa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 500, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 30),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 400, 0.3f),
            };
            for (int k = 0; k < numOps; ++k)
            {
                const auto o = "o" + std::to_string (k + 1) + "_";
                const auto n = "Op. " + std::to_string (k + 1) + " \xC2\xB7 ";
                m.params.push_back (P (o + "wave", n + "Forme", "", 0, numWaves - 1, 0));
                m.params.push_back (C (o + "coarse", n + "Rapport", coarse, 1));
                m.params.push_back (P (o + "fine", n + "Fin", "%", -50, 50, 0));
                m.params.push_back (C (o + "fixed", n + "Fr\xC3\xA9quence fixe", onOff));
                m.params.push_back (P (o + "fixhz", n + "Fr\xC3\xA9quence", "Hz", 1, 8000, 220, 0.3f));
                m.params.push_back (P (o + "vel", n + "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 30));
                m.params.push_back (P (o + "dly", n + "Retard", "ms", 0, 4000, 0, 0.3f));
                m.params.push_back (P (o + "att", n + "Attaque", "ms", 0.2f, 10000, 2, 0.3f));
                m.params.push_back (P (o + "hld", n + "Tenue", "ms", 0, 4000, 0, 0.3f));
                m.params.push_back (P (o + "dec", n + "D\xC3\xA9" "clin", "ms", 5, 20000, 1500, 0.3f));
                m.params.push_back (P (o + "sus", n + "Maintien", "%", 0, 100, 70));
                m.params.push_back (P (o + "rel", n + "Rel\xC3\xA2" "chement", "ms", 5, 20000, 400, 0.3f));
                m.params.push_back (C (o + "loop", n + "Boucle", onOff));
                m.params.push_back (P (o + "out", n + "Sortie", "%", 0, 100, k == 0 ? 100.0f : 0.0f));
            }
            for (int j = 0; j < numOps; ++j)
                for (int k = 0; k < numOps; ++k)
                    m.params.push_back (P ("m" + std::to_string (j + 1) + std::to_string (k + 1),
                                           j == k ? "R\xC3\xA9troaction " + std::to_string (j + 1)
                                                  : std::to_string (j + 1) + " \xE2\x86\x92 " + std::to_string (k + 1), "%", 0, 100, (j == 1 && k == 0) ? 40.0f : 0.0f));
            std::vector<std::string> mat;
            for (int j = 0; j < numOps; ++j) for (int k = 0; k < numOps; ++k) mat.push_back ("m" + std::to_string (j + 1) + std::to_string (k + 1));
            m.sections = { { "global", "Global", "global", { "voices", "vmode", "glide", "tune", "spread", "level" }, "global" },
                           { "filter", "Filtre", "filter", { "ftype", "fcut", "fres", "fenv", "fkey" }, "filter" },
                           { "fenvs", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr" }, "env" },
                           { "matrix", "Matrice FM", "osc", mat, "osc" } };
            for (int k = 0; k < numOps; ++k)
            {
                const auto o = "o" + std::to_string (k + 1) + "_";
                std::vector<std::string> ids;
                for (const char* f : { "wave", "coarse", "fine", "fixed", "fixhz", "vel", "dly", "att", "hld", "dec", "sus", "rel", "loop", "out" }) ids.push_back (o + f);
                m.sections.push_back ({ "op" + std::to_string (k + 1), "Op\xC3\xA9rateur " + std::to_string (k + 1), "osc", ids, "osc" });
            }

            using V = std::vector<std::pair<std::string, float>>;
            auto opv = [] (int k, std::initializer_list<std::pair<const char*, float>> l)
            {
                V v; for (const auto& [f, x] : l) v.push_back ({ "o" + std::to_string (k) + "_" + f, x }); return v;
            };
            auto mk = [] (std::initializer_list<V> parts) { V v; for (const auto& p : parts) v.insert (v.end(), p.begin(), p.end()); return v; };
            auto mx = [] (int from, int to, float amt) { return V { { "m" + std::to_string (from) + std::to_string (to), amt } }; };
            const auto clearDefault = V { { "m21", 0 } };
            m.presets = {
                { "Init", V {}, "Init" },
                { "Piano de carburant", mk ({ clearDefault, mx (2, 1, 45), mx (4, 3, 30), mx (6, 5, 25),
                    opv (1, { { "dec", 2500 }, { "sus", 0 }, { "rel", 600 } }), opv (2, { { "coarse", 1 }, { "dec", 900 }, { "sus", 20 }, { "vel", 70 } }),
                    opv (3, { { "coarse", 1 }, { "out", 60 }, { "dec", 3000 }, { "sus", 0 }, { "fine", 0.4f } }), opv (4, { { "coarse", 14 }, { "dec", 300 }, { "sus", 0 } }),
                    opv (5, { { "coarse", 2 }, { "out", 25 }, { "dec", 1500 }, { "sus", 0 } }), opv (6, { { "coarse", 1 }, { "dec", 800 }, { "sus", 0 } }), V { { "level", -18.0f } } }), "Claviers" },
                { "Basse lourde", mk ({ clearDefault, mx (2, 1, 55), mx (2, 2, 25), mx (3, 1, 20),
                    opv (1, { { "coarse", 0 }, { "dec", 1800 }, { "sus", 60 }, { "rel", 150 } }), opv (2, { { "coarse", 0 }, { "wave", 6 }, { "dec", 250 }, { "sus", 25 } }),
                    opv (3, { { "coarse", 1 }, { "dec", 120 }, { "sus", 0 } }), V { { "vmode", 2 }, { "voices", 1 }, { "ftype", 1 }, { "fcut", 1200 }, { "fenv", 40 }, { "fd", 250 }, { "level", -5.0f } } }), "Basses" },
                { "Cloche en boucle", mk ({ clearDefault, mx (2, 1, 50), mx (4, 3, 45),
                    opv (1, { { "dec", 5000 }, { "sus", 0 }, { "rel", 3000 } }), opv (2, { { "coarse", 6 }, { "fine", 2.5f }, { "dec", 1200 }, { "sus", 10 }, { "loop", 1 }, { "att", 30 } }),
                    opv (3, { { "coarse", 1 }, { "out", 50 }, { "dec", 4000 }, { "sus", 0 }, { "rel", 2500 } }), opv (4, { { "coarse", 9 }, { "fine", -1.2f }, { "dec", 2000 }, { "sus", 0 } }),
                    V { { "level", -16.8f } } }), "Effets" },
                { "Nappe \xC3\xA0 formes", mk ({ clearDefault, mx (2, 1, 25), mx (3, 1, 15), mx (5, 4, 30),
                    opv (1, { { "wave", 2.5f }, { "att", 900 }, { "dec", 3000 }, { "sus", 85 }, { "rel", 2500 } }), opv (2, { { "coarse", 1 }, { "wave", 9 }, { "att", 1500 }, { "sus", 70 } }),
                    opv (3, { { "coarse", 2 }, { "att", 2000 }, { "sus", 60 } }), opv (4, { { "coarse", 1 }, { "fine", 0.6f }, { "wave", 7 }, { "out", 70 }, { "att", 1100 }, { "sus", 85 }, { "rel", 2500 } }),
                    opv (5, { { "coarse", 3 }, { "att", 1800 }, { "sus", 50 }, { "loop", 1 }, { "dec", 2500 } }),
                    V { { "ftype", 1 }, { "fcut", 4000 }, { "spread", 70 }, { "level", -17.2f } } }), "Nappes" },
                { "Lead pli\xC3\xA9", mk ({ clearDefault, mx (2, 1, 60), mx (3, 2, 35), mx (1, 1, 20),
                    opv (1, { { "wave", 1 }, { "sus", 90 } }), opv (2, { { "coarse", 1 }, { "wave", 5 }, { "sus", 70 } }), opv (3, { { "coarse", 2 }, { "sus", 50 } }),
                    V { { "vmode", 2 }, { "voices", 1 }, { "glide", 70 }, { "level", -8.7f } } }), "Leads" },
                { "Cuivres libres", mk ({ clearDefault, mx (2, 1, 55), mx (2, 2, 30), mx (4, 3, 40),
                    opv (1, { { "att", 60 }, { "dec", 1500 }, { "sus", 80 }, { "rel", 300 } }), opv (2, { { "coarse", 1 }, { "att", 90 }, { "dec", 900 }, { "sus", 60 } }),
                    opv (3, { { "coarse", 1 }, { "fine", 0.5f }, { "out", 70 }, { "att", 70 }, { "sus", 80 }, { "rel", 300 } }), opv (4, { { "coarse", 1 }, { "att", 100 }, { "sus", 50 } }),
                    V { { "ftype", 1 }, { "fcut", 3500 }, { "fenv", 25 }, { "fa", 80 }, { "level", -18.8f } } }), "Cuivres" },
                { "Pluck en anneau", mk ({ clearDefault, mx (2, 1, 70), mx (3, 2, 40), mx (6, 5, 50),
                    opv (1, { { "dec", 500 }, { "sus", 0 }, { "rel", 300 } }), opv (2, { { "coarse", 3 }, { "dec", 200 }, { "sus", 0 } }), opv (3, { { "coarse", 7 }, { "dec", 120 }, { "sus", 0 } }),
                    opv (5, { { "coarse", 2 }, { "out", 45 }, { "dec", 700 }, { "sus", 0 } }), opv (6, { { "coarse", 5 }, { "dec", 150 }, { "sus", 0 } }), V { { "level", -13.3f } } }), "Plucks" },
                { "Orgue \xC3\xA0 marches", mk ({ clearDefault,
                    opv (1, { { "wave", 11 }, { "sus", 100 }, { "rel", 80 } }), opv (2, { { "coarse", 2 }, { "wave", 0 }, { "out", 60 }, { "sus", 100 }, { "rel", 80 } }),
                    opv (3, { { "coarse", 4 }, { "out", 35 }, { "sus", 100 }, { "rel", 80 } }), V { { "level", -20.6f } } }), "Claviers" },
                { "Drone du r\xC3\xA9servoir", mk ({ clearDefault, mx (2, 1, 35), mx (3, 3, 40), mx (3, 1, 25), mx (1, 2, 15),
                    opv (1, { { "att", 3000 }, { "sus", 100 }, { "rel", 5000 } }), opv (2, { { "coarse", 0 }, { "fine", 0.3f }, { "att", 6000 }, { "dec", 8000 }, { "sus", 60 }, { "loop", 1 } }),
                    opv (3, { { "fixed", 1 }, { "fixhz", 3.3f }, { "att", 4000 }, { "sus", 100 } }),
                    V { { "ftype", 2 }, { "fcut", 900 }, { "fres", 55 }, { "spread", 80 }, { "level", -8.9f } } }), "Nappes" },
                { "Cordes en m\xC3\xA9tal", mk ({ clearDefault, mx (2, 1, 30), mx (2, 2, 35), mx (4, 3, 30), mx (4, 4, 35),
                    opv (1, { { "att", 500 }, { "sus", 90 }, { "rel", 1500 } }), opv (2, { { "coarse", 1 }, { "att", 600 }, { "sus", 80 } }),
                    opv (3, { { "coarse", 1 }, { "fine", 0.7f }, { "out", 90 }, { "att", 550 }, { "sus", 90 }, { "rel", 1500 } }), opv (4, { { "coarse", 1 }, { "fine", 0.7f }, { "att", 600 }, { "sus", 80 } }),
                    V { { "ftype", 1 }, { "fcut", 5000 }, { "spread", 80 }, { "level", -20.1f } } }), "Cordes" },
                { "Arp\xC3\xA8ge carburant", mk ({ clearDefault, mx (2, 1, 55), mx (3, 1, 30),
                    opv (1, { { "dec", 350 }, { "sus", 0 }, { "rel", 200 } }), opv (2, { { "coarse", 2 }, { "wave", 8 }, { "dec", 200 }, { "sus", 0 } }), opv (3, { { "coarse", 4 }, { "dec", 100 }, { "sus", 0 } }),
                    V { { "arp_mode", 1 }, { "arp_oct", 2 }, { "level", -8.7f } } }), "Arp\xC3\xA8ges" },
            };
            HubSpec hub;
            hub.envs = 1; hub.lfos = 2; hub.firstLfo = 1; hub.slots = 16; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // v0.45.2 : banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Enveloppe à 6 étapes : retard, attaque, tenue, déclin, maintien, relâchement ; boucle
    attaque → déclin tant que la touche est tenue. */
class FmxEnv
{
public:
    void reset() noexcept { v = 0.0; st = idle; t = 0.0; }
    void gate (bool on) noexcept
    {
        if (on) { st = delay; t = 0.0; }
        else if (st != idle) st = release;
    }
    bool active() const noexcept { return st != idle; }
    double next (const double* tm, double sus, bool loop, double sr) noexcept
    {
        // tm : retard, attaque, tenue, déclin (s), relâchement (s)
        switch (st)
        {
            case delay: t += 1.0 / sr; if (t >= tm[0]) { st = attack; t = 0.0; } break;
            case attack: v += tm[1] <= 0.0 ? 1.0 : 1.0 / (tm[1] * sr); if (v >= 1.0) { v = 1.0; st = hold; t = 0.0; } break;
            case hold: t += 1.0 / sr; if (t >= tm[2]) st = decay; break;
            case decay:
            {
                const auto c = std::exp (std::log (0.01) / std::max (1.0, tm[3] * sr));
                v = sus + (v - sus) * c;
                if (std::abs (v - sus) < 1.0e-3) { v = sus; st = loop ? attack : sustain; }
                break;
            }
            case sustain: v = sus; break;
            case release:
            {
                const auto c = std::exp (std::log (0.001) / std::max (1.0, tm[4] * sr));
                v *= c;
                if (v < 1.0e-5) { v = 0.0; st = idle; }
                break;
            }
            default: break;
        }
        return v;
    }

private:
    enum Stage { idle, delay, attack, hold, decay, sustain, release };
    double v = 0.0, t = 0.0;
    Stage st = idle;
};

class FmxVoice
{
public:
    void prepare (double s) { sr = s; fenv.prepare (s); reset(); }
    void reset() noexcept
    {
        for (auto& e : env) e.reset();
        fenv.reset(); filt.reset(); dc.reset();
        ph.fill (0.0); out.fill (0.0);
        active = pending = releasePending = false;
    }
    void setIndex (int i) noexcept { static constexpr double spots[8] { 0.1, -0.1, 0.45, -0.45, 0.75, -0.75, 0.3, -0.3 }; pan = spots[i & 7]; }
    void setSpread (int index, int count) noexcept { uni = count > 1 ? 2.0 * index / (count - 1) - 1.0 : 0.0; }
    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; releasePending = false; active = true;
    }
    // v0.45.1 : une voix muette ne garde pas de relâchement en attente (changement de mode, arrêt)
    void release() noexcept { if (active) releasePending = true; }
    bool isActive() const noexcept { return active; }

    static double table (double phase, double wave) noexcept
    {
        const auto& w = fmx::waves();
        auto ph = phase - std::floor (phase);
        const auto pos = ph * fmx::tableSize;
        const auto i = (int) pos;
        const auto f = pos - i;
        wave = std::clamp (wave, 0.0, (double) fmx::numWaves - 1.0);
        const auto a = (int) wave;
        const auto b = std::min (a + 1, fmx::numWaves - 1);
        const auto g = wave - a;
        const auto& ta = w[(size_t) a];
        const auto va = ta[(size_t) i] + f * (ta[(size_t) i + 1] - ta[(size_t) i]);
        if (g <= 1.0e-6) return va;
        const auto& tb = w[(size_t) b];
        const auto vb = tb[(size_t) i] + f * (tb[(size_t) i + 1] - tb[(size_t) i]);
        return va + g * (vb - va);
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace fmx;
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0 && legatoPending) pitch.to ((double) target);
            else if (p[glide] > 0.5f && from >= 0) { pitch.jump ((double) from); pitch.to ((double) target); }
            else pitch.jump ((double) target);
            if (! legatoPending) { for (auto& e : env) e.gate (true); fenv.gate (true); }
        }
        if (releasePending) { releasePending = false; for (auto& e : env) e.gate (false); fenv.gate (false); }
        fenv.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);

        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-(double) n / (p[glide] * 0.001 * sr * 0.35));
        const auto note = pitch.next (glideCoef) + p[tune] + uni * 0.1;
        const auto keyHz = noteHz (note);
        std::array<double, numOps> inc {}, wv {}, outL {}, velG {}, sus {};
        std::array<std::array<double, 5>, numOps> tm {};
        std::array<bool, numOps> loop {};
        std::array<std::array<double, numOps>, numOps> mat {};
        bool anyOut = false;
        for (int k = 0; k < numOps; ++k)
        {
            const auto b = opParam (k, 0);
            const auto hz = p[b + oFixed] > 0.5f ? (double) p[b + oFixHz]
                          : keyHz * coarseRatios()[(size_t) std::clamp ((int) std::lround (p[b + oCoarse]), 0, 17)] * (1.0 + p[b + oFine] / 100.0);
            inc[(size_t) k] = std::clamp (hz / sr, 0.0, 0.49);
            wv[(size_t) k] = p[b + oWave];
            outL[(size_t) k] = p[b + oOut] / 100.0;
            anyOut = anyOut || outL[(size_t) k] > 0.0;
            velG[(size_t) k] = 1.0 - (p[b + oVel] / 100.0) * (1.0 - vel);
            sus[(size_t) k] = p[b + oSus] / 100.0;
            loop[(size_t) k] = p[b + oLoop] > 0.5f;
            tm[(size_t) k] = { p[b + oDly] * 0.001, p[b + oAtt] * 0.001, p[b + oHld] * 0.001, p[b + oDec] * 0.001, p[b + oRel] * 0.001 };
            for (int j = 0; j < numOps; ++j) { const auto a = p[matParam (j, k)] / 100.0; mat[(size_t) j][(size_t) k] = a * a * 4.0; }
        }
        const int ft = std::clamp ((int) p[fType], 0, 3);
        const auto res = p[fRes] / 100.0;
        const auto keyOct = (p[fKey] / 100.0) * (note - 60.0) / 12.0;
        const auto envOct = (p[fEnv] / 100.0) * 7.0;
        const auto gain = dbToGain (p[level]);
        const auto sp = p[spread] / 100.0;
        const auto pv = std::clamp (pan * sp + uni * sp * 0.8, -1.0, 1.0);
        const auto gL = std::sqrt (0.5 * (1.0 - pv)) * 1.4142, gR = std::sqrt (0.5 * (1.0 + pv)) * 1.4142;
        SvfCoefs fc;

        for (int i = 0; i < n; ++i)
        {
            std::array<double, numOps> e {}, now {};
            for (int k = 0; k < numOps; ++k) e[(size_t) k] = env[(size_t) k].next (tm[(size_t) k].data(), sus[(size_t) k], loop[(size_t) k], sr) * velG[(size_t) k];
            // du 6 au 1 : un modulateur de rang plus haut est déjà calculé (même échantillon) ; les
            // autres (boucles, rétroaction) sont lus avec un échantillon de retard
            for (int k = numOps - 1; k >= 0; --k)
            {
                double mod = 0.0;
                for (int j = 0; j < numOps; ++j)
                {
                    const auto a = mat[(size_t) j][(size_t) k];
                    if (a <= 0.0) continue;
                    mod += a * (j > k ? now[(size_t) j] : out[(size_t) j]);
                }
                now[(size_t) k] = table (ph[(size_t) k] + mod, wv[(size_t) k]) * e[(size_t) k];
                ph[(size_t) k] += inc[(size_t) k];
                if (ph[(size_t) k] >= 1.0) ph[(size_t) k] -= 1.0;
            }
            out = now;
            double x = 0.0;
            for (int k = 0; k < numOps; ++k) x += now[(size_t) k] * outL[(size_t) k];
            const auto fe = fenv.next();
            if (ft > 0)
            {
                if ((i & 7) == 0) fc.set (std::clamp ((double) p[fCut] * fastExp2 (envOct * fe + keyOct), 16.0, 0.45 * sr), res, sr);
                const auto o = filt.process (x, fc);
                x = ft == 1 ? o.low : ft == 2 ? o.band : o.high;
            }
            const auto y = dc.process (x) * gain;
            L[i] += (float) (y * gL);
            R[i] += (float) (y * gR);
        }
        bool any = false;
        for (int k = 0; k < numOps; ++k) any = any || (outL[(size_t) k] > 0.0 && env[(size_t) k].active());
        if (! any || ! anyOut) active = false;
    }

private:
    double sr = 48000.0, pan = 0.0, uni = 0.0;
    std::array<double, fmx::numOps> ph {}, out {};
    std::array<FmxEnv, fmx::numOps> env;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool active = false, pending = false, releasePending = false, legatoPending = false;
    Adsr fenv;
    StateFilter filt;
    Glide pitch;
    DcBlocker dc;
};

class FmxSynth final : public SynthBase<FmxVoice, 16>
{
public:
    FmxSynth() { (void) fmx::waves(); for (int i = 0; i < 16; ++i) voices[(size_t) i].setIndex (i); }
    const ModuleDesc& desc() const noexcept override { return fmx::desc(); }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[fmx::voices], 1, 16);
        switch ((int) p[fmx::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (false, true, v); break;
        }
    }
};

} // namespace grv::instruments
