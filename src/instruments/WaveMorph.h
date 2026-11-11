/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Wave morph » — principe des synthétiseurs à balayage de tables d'ondes récents (architecture
    reprise, ni nom, ni panneau, ni son d'origine) :
      - trois oscillateurs : 1 et 2 parcourent en continu 16 formes calculées par synthèse
        additive, 3 lit une forme choisie ;
      - deux « mutants » par oscillateur 1 et 2, en chaîne : FM interne, synchro, compression de
        phase (largeur), repli, pile d'ondes (copies désaccordées), phase décalée (différence de
        deux lectures), harmonique (lecture à un multiple de la fréquence, en fondu) ;
      - modulation en anneau, bruit ;
      - deux filtres en série ou en parallèle : le 1er en échelle (24, 18, 12, 6 dB, bande, haut)
        ou voyelle (formants) ou peigne (±), le 2e à variables d'état morphable bas → bande → haut ;
      - cinq enveloppes, cinq LFO (dont trois à pas), matrice de 32 routages à deux sources,
        huit macros, arpège à pas ; poly, mono ou unisson jusqu'à 8 voix. Les 16 formes sont
        générées par le programme.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"
#include "../synth/Wavetable.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace morph
{
    enum Slot
    {
        voices = 0, glide, level, velAmp, tune, spreadAmt,
        w1Pos, w1Semi, w1Fine, w1Mut, w1MutAmt, w1Ratio,
        w2Pos, w2Semi, w2Fine, w2Mut, w2MutAmt, w2Ratio,
        mix1, mix2, mixRing, mixNoise,
        f1Mode, f1Cutoff, f1Reso, f1Drive, f2Cutoff, f2Reso, f2Morph, routing, keytrack, env1F1, env1F2,
        e1A, e1D, e1S, e1R, e2A, e2D, e2S, e2R, e3A, e3D, e3S, e3R, env3Pos,
        l1Rate, l1Shape, l1Pos, l2Rate, l2Shape, l2Pitch, l2Cutoff,
        l1Sync, l2Sync, // v0.9 : synchro au tempo
        // v0.10 : oscillateur 3, deuxième mutant, modes de voix
        w3Pos, w3Semi, w3Fine, mix3, w1Mut2, w1Mut2Amt, w2Mut2, w2Mut2Amt, vMode, uniVoices, uniDetune,
        // v0.12.5 : tables d'ondes choisies (1 à 16 ondes balayées par oscillateur)
        w1Count, w2Count, w1Scan, w2Scan = w1Scan + 16,
        l1Delay = w2Scan + 16, l2Delay, // v0.13.2 : retard des LFO à chaque note
        numSlots
    };

    inline const std::vector<std::string>& waveNames()
    {
        static const std::vector<std::string> n {
            "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Impulsion", "Orgue", "Voix A", "Voix O",
            "Cloche", "Cordes", "Creux", "M\xC3\xA9tal", "Premiers", "Bande", "Grain", "Doux",
            // v0.12.5 : bibliothèque élargie (les 16 premières gardent leur numéro)
            "Scie douce", "Scie ronde", "Scie vive", "Impulsion 10 %", "Impulsion 25 %", "Impulsion 33 %", "Impulsion 40 %", "Carr\xC3\xA9 doux",
            "Paires", "Octaves", "Quintes", "Orgue jazz", "Orgue rock", "Orgue plein", "Fl\xC3\xBBte", "Bourdon",
            "Voix E", "Voix I", "Voix U", "Ch\xC5\x93ur", "R\xC3\xA9sonance 4", "R\xC3\xA9sonance 8", "R\xC3\xA9sonance 16", "R\xC3\xA9sonance 32",
            "Filtr\xC3\xA9" "e 4", "Filtr\xC3\xA9" "e 16", "Sinus + 3", "Sinus + 5", "Demi-sinus", "Sinus redress\xC3\xA9", "Repli 1", "Repli 2",
            "Repli 3", "Triangle pli\xC3\xA9", "Sinus satur\xC3\xA9", "Synchro 2", "Synchro 4", "Synchro 7", "Escalier 4", "Escalier 8",
            "Sinus 3 bits", "Exponentielle", "Parabole", "Bosse", "Double bosse", "Pile de scies", "Rampe et plateau", "Ondul\xC3\xA9" };
        return n;
    }

    inline const WaveBank& bank()
    {
        static const WaveBank b = []
        {
            using cd = std::complex<double>;
            auto sine = [] (double a) { return cd (0.0, -0.5 * a); };
            WaveBank w;
            w.addWave ([&] (int h) { return h == 1 ? sine (1.0) : cd(); });
            w.addWave ([&] (int h) { return h % 2 ? sine (((h / 2) % 2 ? -1.0 : 1.0) / (double) (h * h)) : cd(); });
            w.addWave ([&] (int h) { return sine (1.0 / h); });
            w.addWave ([&] (int h) { return h % 2 ? sine (1.0 / h) : cd(); });
            w.addWave ([&] (int h) { return sine (std::sin (kPi * h * 0.22) / h); });
            w.addWave ([&] (int h) { static const double bars[9] { 0, 1.0, 0.8, 0.6, 0.5, 0, 0.35, 0, 0.3 }; return h <= 8 ? sine (bars[h]) : cd(); });
            w.addWave ([&] (int h) { const auto f = std::exp (-0.5 * std::pow ((h - 3.0) / 1.2, 2)) + 0.6 * std::exp (-0.5 * std::pow ((h - 7.0) / 1.8, 2)); return sine ((0.25 + f) / h); });
            w.addWave ([&] (int h) { const auto f = std::exp (-0.5 * std::pow ((h - 2.0) / 0.8, 2)) + 0.5 * std::exp (-0.5 * std::pow ((h - 5.0) / 1.2, 2)); return sine ((0.15 + f) / h); });
            w.addWave ([&] (int h) { static const int hs[6] { 1, 3, 7, 11, 17, 23 }; for (int k = 0; k < 6; ++k) if (h == hs[k]) return sine (std::pow (0.7, k)); return cd(); });
            w.addWave ([&] (int h) { return sine (std::pow ((double) h, -0.85) * (1.0 + 0.6 * std::exp (-0.5 * std::pow ((h - 9.0) / 4.0, 2)))); });
            w.addWave ([&] (int h) { return (h % 2 && h >= 5) ? sine (1.0 / h) : cd(); });
            w.addWave ([&] (int h) { std::uint32_t r = 0x9E3779B9u * (std::uint32_t) h; r ^= r >> 15; const auto ph = (double) (r & 0xFFFF) / 65536.0 * kTwoPi;
                                     const auto a = h <= 48 ? 0.5 : 0.5 * 48.0 / h; return std::polar (a / std::sqrt ((double) h), ph); });
            w.addWave ([&] (int h) { auto prime = [] (int x) { if (x < 2) return x == 1; for (int d = 2; d * d <= x; ++d) if (x % d == 0) return false; return true; };
                                     return prime (h) ? sine (1.0 / h) : cd(); });
            w.addWave ([&] (int h) { return (h >= 6 && h <= 18) ? sine (std::sin (kPi * (h - 6) / 12.0) * 0.5) : (h == 1 ? sine (0.4) : cd()); });
            w.addWave ([&] (int h) { std::uint32_t r = 0x85EBCA6Bu * (std::uint32_t) (h + 7); r ^= r >> 13; r *= 0xC2B2AE35u; r ^= r >> 16;
                                     return std::polar (((double) (r & 0xFF) / 255.0) / std::sqrt ((double) h), (double) (r >> 16) / 65536.0 * kTwoPi); });
            w.addWave ([&] (int h) { return h == 1 ? sine (1.0) : (h == 2 ? sine (0.3) : (h == 3 ? sine (0.1) : cd())); });

            // --- v0.12.5 : 48 ondes de plus ---------------------------------------------------
            auto pulse = [&] (double d) { return [&, d] (int h) { return sine (std::sin (kPi * h * d) / h); }; };
            auto peaks = [] (double h, std::initializer_list<std::pair<double, double>> fs, double floor) // formants : (harmonique, largeur)
            { double a = floor; for (const auto& [c, wd] : fs) a += std::exp (-0.5 * std::pow ((h - c) / wd, 2)); return a; };
            auto drawbars = [&] (std::initializer_list<double> amps) // tirettes : 16', 5 1/3', 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1' (16' = harmonique 1)
            {
                static const int hs[9] { 1, 3, 2, 4, 6, 8, 10, 12, 16 };
                std::array<double, 17> a {};
                int k = 0;
                for (auto v : amps) { if (k < 9) a[(size_t) hs[k]] += v; ++k; }
                return [a, &sine] (int h) { return h <= 16 && a[(size_t) h] > 0.0 ? sine (a[(size_t) h]) : std::complex<double>(); };
            };
            auto frac = [] (double x) { return x - std::floor (x); };
            auto fold = [] (double x) { // repli : la tension qui dépasse ±1 revient en arrière
                x = (x + 1.0) * 0.25; x -= std::floor (x); return std::abs (x * 4.0 - 2.0) - 1.0; };
            auto s2p = [] (double x) { return std::sin (kTwoPi * x); };

            w.addWave ([&] (int h) { return sine (std::pow ((double) h, -1.5)); });                       // scie douce
            w.addWave ([&] (int h) { return sine (1.0 / ((double) h * h)); });                              // scie ronde
            w.addWave ([&] (int h) { return h <= 96 ? sine (std::pow ((double) h, -0.75)) : cd(); });       // scie vive
            w.addWave (pulse (0.10)); w.addWave (pulse (0.25)); w.addWave (pulse (1.0 / 3.0)); w.addWave (pulse (0.40));
            w.addWave ([&] (int h) { return h % 2 ? sine (1.0 / ((double) h * std::sqrt ((double) h))) : cd(); }); // carré doux
            w.addWave ([&] (int h) { return h == 1 ? sine (1.0) : h % 2 == 0 ? sine (1.0 / h) : cd(); });          // paires
            w.addWave ([&] (int h) { return (h & (h - 1)) == 0 && h <= 64 ? sine (1.0 / std::sqrt ((double) h)) : cd(); }); // octaves
            w.addWave ([&] (int h) { static const int q[7] { 1, 2, 3, 4, 6, 8, 12 }; for (int k = 0; k < 7; ++k) if (h == q[k]) return sine (std::pow (0.75, k)); return cd(); }); // quintes
            w.addWave (drawbars ({ 1, 1, 1 }));                                   // orgue jazz (888000000)
            w.addWave (drawbars ({ 1, 1, 1, 1, 0, 0, 0, 0, 0.6 }));               // orgue rock
            w.addWave (drawbars ({ 1, 1, 1, 1, 1, 1, 1, 1, 1 }));                 // orgue plein
            w.addWave ([&] (int h) { return h == 1 ? sine (1.0) : h == 2 ? sine (0.18) : h == 3 ? sine (0.08) : h <= 6 ? sine (0.02) : cd(); }); // flûte
            w.addWave ([&] (int h) { return h <= 12 ? sine (1.0 / std::pow ((double) h, 0.6)) : cd(); });                                   // bourdon
            w.addWave ([&] (int h) { return sine (peaks (h, { { 2.0, 0.8 }, { 10.0, 2.0 } }, 0.08) / h); });   // voix E
            w.addWave ([&] (int h) { return sine (peaks (h, { { 1.5, 0.7 }, { 13.0, 2.5 } }, 0.06) / h); });   // voix I
            w.addWave ([&] (int h) { return sine (peaks (h, { { 1.2, 0.6 }, { 3.5, 1.0 } }, 0.04) / h); });    // voix U
            w.addWave ([&] (int h) { return std::polar (peaks (h, { { 3.0, 1.5 }, { 6.0, 2.0 }, { 12.0, 3.0 } }, 0.15) / std::sqrt ((double) h), 1.7 * h * h); }); // chœur
            for (double c : { 4.0, 8.0, 16.0, 32.0 })                             // résonance : scie et une bosse de résonance (filtre poussé)
                w.addWave ([&, c] (int h) { return sine ((1.0 + 5.0 * std::exp (-0.5 * std::pow ((h - c) / (0.12 * c + 0.6), 2))) / h); });
            for (double c : { 4.0, 16.0 })                                        // scie filtrée (passe-bas à 24 dB)
                w.addWave ([&, c] (int h) { return sine (1.0 / (h * (1.0 + std::pow (h / c, 4.0)))); });
            w.addWave ([&] (int h) { return h == 1 ? sine (1.0) : h == 3 ? sine (0.5) : cd(); });            // sinus + 3
            w.addWave ([&] (int h) { return h == 1 ? sine (1.0) : h == 5 ? sine (0.4) : cd(); });            // sinus + 5
            w.addWaveTime ([&] (double x) { return std::max (0.0, s2p (x)); });                             // demi-sinus
            w.addWaveTime ([&] (double x) { return std::abs (s2p (x)); });                                  // sinus redressé
            for (double g : { 2.0, 3.5, 6.0 }) w.addWaveTime ([&, g] (double x) { return fold (g * s2p (x)); }); // repli 1 à 3
            w.addWaveTime ([&] (double x) { return fold (3.0 * (1.0 - 4.0 * std::abs (x - 0.5))); });      // triangle plié
            w.addWaveTime ([&] (double x) { return std::tanh (3.0 * s2p (x)); });                           // sinus saturé
            for (double r : { 2.5, 4.3, 7.6 }) w.addWaveTime ([&, r] (double x) { return 2.0 * frac (x * r) - 1.0; }); // synchro
            for (double st : { 4.0, 8.0 }) w.addWaveTime ([&, st] (double x) { return 2.0 * std::floor (x * st) / (st - 1.0) - 1.0; }); // escalier
            w.addWaveTime ([&] (double x) { return std::round (s2p (x) * 3.0) / 3.0; });                    // sinus 3 bits
            w.addWaveTime ([&] (double x) { return 2.0 * std::exp (-4.0 * x) - 1.0; });                     // exponentielle
            w.addWaveTime ([&] (double x) { return 1.0 - 8.0 * (x - 0.5) * (x - 0.5); });                   // parabole
            w.addWaveTime ([&] (double x) { return std::exp (-std::pow ((x - 0.5) / 0.08, 2)); });          // bosse
            w.addWaveTime ([&] (double x) { return std::exp (-std::pow ((x - 0.3) / 0.06, 2)) - 0.8 * std::exp (-std::pow ((x - 0.7) / 0.06, 2)); }); // double bosse
            w.addWaveTime ([&] (double x) { return frac (x) + 0.6 * frac (2.0 * x + 0.3) + 0.4 * frac (3.0 * x + 0.6); }); // pile de scies
            w.addWaveTime ([&] (double x) { return x < 0.5 ? 4.0 * x - 1.0 : 1.0 - 0.4 * (x - 0.5); });    // rampe et plateau
            w.addWaveTime ([&] (double x) { return std::sin (kTwoPi * x + 2.0 * std::sin (3.0 * kTwoPi * x)); }); // ondulé
            return w;
        }();
        return b;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> lfoShapes { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire" };
            const std::vector<std::string> mutants { "Aucun", "FM", "Synchro", "Compression", "Repli", "Pile d'ondes", "Phase d\xC3\xA9" "cal\xC3\xA9" "e", "Harmonique" };
            ModuleDesc m;
            m.type = "morph";
            m.name = "Synthonde";
            m.family = "Polys";
            m.principle = "Balayage de tables d'ondes \xC3\xA0 2 oscillateurs mutants, 2 filtres en s\xC3\xA9rie ou parall\xC3\xA8le";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Niveau", "dB", -24, 6, -10),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("spread", "\xC3\x89" "talement", "%", 0, 100, 40),

                P ("w1pos", "Position 1", "", 0, 15, 2), I ("w1semi", "Demi-tons 1", "dt", -24, 24, 0), P ("w1fine", "Fin 1", "ct", -50, 50, 0),
                C ("w1mut", "Mutant 1", mutants), P ("w1mutamt", "Intensit\xC3\xA9 1", "%", 0, 100, 0), P ("w1ratio", "Rapport 1", "", 0.5f, 8, 2, 0.6f),
                P ("w2pos", "Position 2", "", 0, 15, 9), I ("w2semi", "Demi-tons 2", "dt", -24, 24, 0), P ("w2fine", "Fin 2", "ct", -50, 50, 7),
                C ("w2mut", "Mutant 2", mutants), P ("w2mutamt", "Intensit\xC3\xA9 2", "%", 0, 100, 0), P ("w2ratio", "Rapport 2", "", 0.5f, 8, 3, 0.6f),
                P ("mix1", "Niveau 1", "%", 0, 100, 70),
                P ("mix2", "Niveau 2", "%", 0, 100, 50),
                P ("mixring", "Anneau", "%", 0, 100, 0),
                P ("mixnoise", "Bruit", "%", 0, 100, 0),

                C ("f1mode", "Filtre 1", { "Passe-bas 24", "Passe-bas 12", "Passe-bande", "Passe-haut 12",
                                           "Passe-bas 18", "Passe-bas 6", "Passe-haut 24", "Voyelle", "Peigne +", "Peigne \xE2\x88\x92" }),
                P ("f1cutoff", "Coupure 1", "Hz", 20, 18000, 3000, 0.3f),
                P ("f1reso", "R\xC3\xA9sonance 1", "%", 0, 100, 15),
                P ("f1drive", "Saturation 1", "dB", 0, 24, 0),
                P ("f2cutoff", "Coupure 2", "Hz", 20, 18000, 9000, 0.3f),
                P ("f2reso", "R\xC3\xA9sonance 2", "%", 0, 100, 10),
                P ("f2morph", "Bas \xE2\x86\x92 bande \xE2\x86\x92 haut", "%", 0, 100, 0),
                C ("routing", "Routage", { "S\xC3\xA9rie", "Parall\xC3\xA8le" }),
                P ("keytrack", "Suivi clavier", "%", 0, 100, 40),
                P ("env1f1", "Env. 1 \xE2\x86\x92 filtre 1", "%", -100, 100, 35),
                P ("env1f2", "Env. 1 \xE2\x86\x92 filtre 2", "%", -100, 100, 0),

                P ("e1a", "Attaque", "ms", 0.5f, 10000, 3, 0.3f), P ("e1d", "D\xC3\xA9" "clin", "ms", 5, 10000, 700, 0.3f),
                P ("e1s", "Maintien", "%", 0, 100, 30), P ("e1r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 600, 0.3f),
                P ("e2a", "Attaque", "ms", 0.5f, 10000, 3, 0.3f), P ("e2d", "D\xC3\xA9" "clin", "ms", 5, 10000, 900, 0.3f),
                P ("e2s", "Maintien", "%", 0, 100, 80), P ("e2r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 700, 0.3f),
                P ("e3a", "Attaque", "ms", 0.5f, 10000, 600, 0.3f), P ("e3d", "D\xC3\xA9" "clin", "ms", 5, 10000, 2000, 0.3f),
                P ("e3s", "Maintien", "%", 0, 100, 50), P ("e3r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 1000, 0.3f),
                P ("env3pos", "Env. 3 \xE2\x86\x92 positions", "", -8, 8, 0),

                P ("l1rate", "LFO 1 vitesse", "Hz", 0.02f, 30, 0.3f, 0.4f), C ("l1shape", "LFO 1 forme", lfoShapes, 1),
                P ("l1pos", "LFO 1 \xE2\x86\x92 positions", "", 0, 8, 0),
                P ("l2rate", "LFO 2 vitesse", "Hz", 0.02f, 30, 5, 0.4f), C ("l2shape", "LFO 2 forme", lfoShapes),
                P ("l2pitch", "LFO 2 \xE2\x86\x92 hauteur", "ct", 0, 100, 0),
                P ("l2cutoff", "LFO 2 \xE2\x86\x92 coupure", "%", 0, 100, 0),
                C ("l1sync", "LFO 1 synchro", lfoSyncChoices()), C ("l2sync", "LFO 2 synchro", lfoSyncChoices()),

                C ("w3pos", "Forme 3", waveNames()), I ("w3semi", "Demi-tons 3", "dt", -24, 24, -12), P ("w3fine", "Fin 3", "ct", -50, 50, 0),
                P ("mix3", "Niveau 3", "%", 0, 100, 0),
                C ("w1mut2", "Mutant 1 B", mutants), P ("w1mut2amt", "Intensit\xC3\xA9 1 B", "%", 0, 100, 0),
                C ("w2mut2", "Mutant 2 B", mutants), P ("w2mut2amt", "Intensit\xC3\xA9 2 B", "%", 0, 100, 0),
                C ("vmode", "Mode de voix", { "Poly", "Mono", "Unisson" }),
                I ("univoices", "Voix d'unisson", "", 2, 8, 4),
                P ("unidetune", "D\xC3\xA9" "saccord", "ct", 0, 50, 12),
            };
            // tables d'ondes : nombre d'ondes balayées (choix, pour ne pas devenir une destination de
            // la matrice) et les 16 cases de chaque oscillateur ; d'origine, les 16 premières ondes
            {
                std::vector<std::string> counts;
                for (int k = 1; k <= 16; ++k) counts.push_back (std::to_string (k) + (k == 1 ? " onde" : " ondes"));
                m.params.push_back (C ("w1n", "Ondes 1", counts, 15));
                m.params.push_back (C ("w2n", "Ondes 2", counts, 15));
                for (int o = 1; o <= 2; ++o)
                    for (int k = 0; k < 16; ++k)
                        m.params.push_back (C ("w" + std::to_string (o) + "s" + std::to_string (k + 1), "Onde " + std::to_string (o) + "." + std::to_string (k + 1), waveNames(), k));
            }
            m.params.push_back (C ("l1delay", "LFO 1 retard", lfoDelayChoices()));
            m.params.push_back (C ("l2delay", "LFO 2 retard", lfoDelayChoices()));
            m.sections = {
                { "global", "Global", "global", { "vmode", "voices", "univoices", "unidetune", "glide", "tune", "spread", "velamp", "level" }, "global" },
                { "wave1", "Oscillateur 1", "wave", { "w1pos", "w1semi", "w1fine", "w1ratio", "mix1" }, "osc" },
                { "mut1", "Mutants de l'oscillateur 1", "osc", { "w1mut", "w1mutamt", "w1mut2", "w1mut2amt", "w1ratio" }, "osc" },
                { "wave2", "Oscillateur 2", "wave", { "w2pos", "w2semi", "w2fine", "w2ratio", "mix2" }, "osc" },
                { "mut2", "Mutants de l'oscillateur 2", "osc", { "w2mut", "w2mutamt", "w2mut2", "w2mut2amt", "w2ratio" }, "osc" },
                { "wave3", "Oscillateur 3", "osc", { "w3pos", "w3semi", "w3fine", "mix3" }, "osc" },
                { "scan1", "Ondes de l'oscillateur 1", "osc", { "w1n", "w1s1", "w1s2", "w1s3", "w1s4", "w1s5", "w1s6", "w1s7" }, "osc" },
                { "scan2", "Ondes de l'oscillateur 2", "osc", { "w2n", "w2s1", "w2s2", "w2s3", "w2s4", "w2s5", "w2s6", "w2s7" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "mix1", "mix2", "mix3", "mixring", "mixnoise" }, "osc" },
                { "filter1", "Filtre 1", "filter", { "f1mode", "f1cutoff", "f1reso", "f1drive", "keytrack", "env1f1" }, "filter" },
                { "filter2", "Filtre 2", "filter", { "f2cutoff", "f2reso", "f2morph", "routing", "env1f2" }, "filter" },
                { "env1", "Enveloppe 1 (filtres)", "env", { "e1a", "e1d", "e1s", "e1r", "env1f1", "env1f2" }, "env" },
                { "env2", "Enveloppe 2 (ampli)", "env", { "e2a", "e2d", "e2s", "e2r", "velamp", "level" }, "amp" },
                { "env3", "Enveloppe 3 (positions)", "env", { "e3a", "e3d", "e3s", "e3r", "env3pos" }, "env" },
                { "lfo1", "LFO 1", "lfo", { "l1sync", "l1rate", "l1shape", "l1pos", "l1delay" }, "mod" },
                { "lfo2", "LFO 2", "lfo", { "l2sync", "l2rate", "l2shape", "l2pitch", "l2cutoff", "l2delay" }, "mod" },
            };
            m.presets = {
                { "Balayage", { { "env3pos", 6 }, { "w1pos", 1 }, { "w2pos", 6 } } },
                { "Voix de verre", { { "w1pos", 6 }, { "w2pos", 7 }, { "l1pos", 2 }, { "e2a", 400 }, { "e2r", 2000 }, { "f1cutoff", 6000 } } },
                { "Basse mutante", { { "w1pos", 3 }, { "w1mut", 1 }, { "w1mutamt", 45 }, { "w1ratio", 2 }, { "mix2", 0 }, { "f1cutoff", 900 }, { "e1d", 250 }, { "e1s", 0 }, { "env1f1", 60 } } },
                { "Cloches m\xC3\xA9talliques", { { "w1pos", 8 }, { "w2pos", 11 }, { "mixring", 35 }, { "e2s", 0 }, { "e2d", 2000 }, { "e2r", 2000 }, { "routing", 1 } } },
                { "Synchro balay\xC3\xA9" "e", { { "w1mut", 2 }, { "w1mutamt", 60 }, { "l1pos", 3 }, { "l1rate", 0.15f }, { "f1reso", 30 } } },
                { "Pile chorale", { { "w1mut", 5 }, { "w1mutamt", 70 }, { "w2mut", 5 }, { "w2mutamt", 50 }, { "w1pos", 9 }, { "w2pos", 6 }, { "e2a", 300 }, { "e2r", 1800 } } },
                { "Voyelles", { { "f1mode", 7 }, { "f1reso", 55 }, { "w1pos", 2 }, { "mix2", 0 }, { "mx1_src", 7 }, { "mx1_dst@f1cutoff", 0 }, { "mx1_amt", 35 }, { "l2rate", 0.6f } } },
                { "Peigne m\xC3\xA9tallique", { { "f1mode", 8 }, { "f1reso", 80 }, { "f1cutoff", 400 }, { "mixnoise", 40 }, { "e2s", 0 }, { "e2d", 900 }, { "e2r", 900 } } },
                { "Lead unisson", { { "vmode", 2 }, { "univoices", 6 }, { "unidetune", 18 }, { "w1pos", 2 }, { "w2pos", 3 }, { "glide", 40 }, { "w1mut2", 7 }, { "w1mut2amt", 30 } } },
                { "Basse sous-oscillateur", { { "mix3", 70 }, { "w3pos", 0 }, { "w3semi", -12 }, { "mix2", 0 }, { "f1cutoff", 900 }, { "env1f1", 50 }, { "e1d", 250 }, { "e1s", 0 } } },
            };
            HubSpec hub;
            hub.extEnvs = { "Env. 1 (filtres)", "Env. 2 (ampli)", "Env. 3" };
            hub.extLfos = { "LFO 1", "LFO 2" };
            hub.envs = 2; hub.firstEnv = 4; hub.envName = "Env.";
            hub.lfos = 3; hub.firstLfo = 3;
            hub.slots = 32; hub.macros = 8; hub.arp = true; hub.arpSteps = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // banque d'usine : 50 sons par catégories (FactoryBanks.h)
            const auto& wb = bank();
            for (int k = 0; k < wb.size(); ++k)
            {
                std::vector<float> shape (64);
                for (int i = 0; i < 64; ++i) shape[(size_t) i] = wb.read (k, 4, (double) i / 64.0); // 64 harmoniques : lisible
                float pk = 1.0e-6f;
                for (auto v : shape) pk = std::max (pk, std::abs (v));
                for (auto& v : shape) v = std::round (v / pk * 1000.0f) / 1000.0f;
                m.waveShapes.push_back (std::move (shape));
            }
            return m;
        }();
        return d;
    }
}

/** État d'un oscillateur à table d'ondes et ses deux mutants. */
struct MorphOscState
{
    double ph = 0.0, pm = 0.0, st1 = 0.31, st2 = 0.67;
    void reset (double p) noexcept { ph = p; pm = 0.0; st1 = p + 0.31; st2 = p + 0.67; }
};

/** Filtre en peigne (retard de 1/fc, réinjection ±) pour une voix. */
class CombFilter
{
public:
    void reset() noexcept { buf.fill (0.0f); w = 0; }
    double process (double x, double fc, double fb, double sr) noexcept
    {
        const auto d = std::clamp (sr / std::max (20.0, fc), 2.0, (double) (size - 2));
        auto rp = (double) w - d;
        if (rp < 0.0) rp += size;
        const auto i0 = (int) rp;
        const auto f = rp - (double) i0;
        const auto y0 = buf[(size_t) i0], y1 = buf[(size_t) ((i0 + 1) & (size - 1))];
        const auto delayed = (double) y0 + ((double) y1 - (double) y0) * f;
        const auto y = x + fb * delayed;
        buf[(size_t) w] = (float) softClip (y);
        w = (w + 1) & (size - 1);
        return y * (1.0 - 0.5 * std::abs (fb));
    }
private:
    static constexpr int size = 4096;
    std::array<float, size> buf {};
    int w = 0;
};

class MorphVoice
{
public:
    void prepare (double s) { sr = s; e1.prepare (s); e2.prepare (s); e3.prepare (s); reset(); }
    void reset() noexcept
    {
        o1.reset (0.0); o2.reset (0.0); o3.reset (0.0); f1.reset(); f2.reset(); fv1.reset(); fv2.reset(); comb.reset();
        e1.reset(); e2.reset(); e3.reset(); dc.reset(); active = false; declick = false; lastOut = offset = 0.0;
    }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true;
        if (! legatoStart) lfoAge = 0.0;
        rng = rng * 1664525u + 1013904223u;
        pan = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { e1.gate (false); e2.gate (false); e3.gate (false); }
    bool isActive() const noexcept { return active; }

    void modSources (double* env, double* lfo) const noexcept
    {
        env[0] = lastE1; env[1] = lastE2; env[2] = lastE3;
        lfo[0] = lastL1; lfo[1] = lastL2;
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace morph;
        const auto& wb = bank();
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            // 1.1 : toute note non liée (hors legato) relance ses oscillateurs au même point :
            // attaque et rondeur identiques d'une note à l'autre (avant, les oscillateurs désaccordés
            // tournaient librement et chaque note grave tombait ailleurs dans leur battement)
            // (une voix qui sonne encore — relâchement en cours — repart aussi au même point ; la
            // sortie rejoint son ancienne valeur en ~2 ms, sans clic)
            if (! (legatoPending && e2.isActive())) { o1.reset (0.0); o2.reset (0.0); o3.reset (0.0); declick = e2.isActive(); }
            if (! legatoPending || ! e2.isActive()) { e1.gate (true); e2.gate (true); e3.gate (true); }
        }
        e1.set (p[e1A], p[e1D], p[e1S] / 100.0, p[e1R]);
        e2.set (p[e2A], p[e2D], p[e2S] / 100.0, p[e2R]);
        e3.set (p[e3A], p[e3D], p[e3S] / 100.0, p[e3R]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level] + 6.0f) * velocityGain (vel, p[velAmp] / 100.0); // +6 dB de réserve (projets v5)
        const auto pn = spread != 0.0 ? spread * std::max (0.6, p[spreadAmt] / 100.0) : pan * p[spreadAmt] / 100.0;
        const auto gl = 1.0 - std::max (0.0, pn) * 0.6, gr = 1.0 + std::min (0.0, pn) * 0.6;
        const auto mode1 = std::clamp ((int) p[f1Mode], 0, 9);
        const auto drv = dbToGain (p[f1Drive]);
        const bool parallel = p[routing] > 0.5f;
        const auto det = spread * p[uniDetune] / 100.0;
        const auto off1 = std::round (p[w1Semi]) + p[w1Fine] / 100.0 + det, off2 = std::round (p[w2Semi]) + p[w2Fine] / 100.0 + det;
        const auto off3 = std::round (p[w3Semi]) + p[w3Fine] / 100.0 + det;
        const auto mx1 = p[mix1] / 100.0, mx2 = p[mix2] / 100.0, mx3 = p[mix3] / 100.0, mxR = p[mixRing] / 100.0, mxN = p[mixNoise] / 100.0;
        const auto ktAmt = p[keytrack] / 100.0, l2Oct = p[l2Cutoff] / 100.0 * 3.0;
        const auto env1Oct1 = (p[env1F1] / 100.0) * 7.0, env1Oct2 = (p[env1F2] / 100.0) * 7.0;
        const auto res1 = p[f1Reso] / 100.0, res2 = p[f2Reso] / 100.0, mrph = p[f2Morph] / 100.0;
        static constexpr Ladder::Mode modes[7] { Ladder::lp24, Ladder::lp12, Ladder::bp12, Ladder::hp12, Ladder::lp18, Ladder::lp6, Ladder::hp24 };
        const Mut m1 { (int) p[w1Mut], p[w1MutAmt] / 100.0, (int) p[w1Mut2], p[w1Mut2Amt] / 100.0, p[w1Ratio] };
        const Mut m2 { (int) p[w2Mut], p[w2MutAmt] / 100.0, (int) p[w2Mut2], p[w2Mut2Amt] / 100.0, p[w2Ratio] };
        const auto w3 = std::clamp ((int) p[w3Pos], 0, wb.size() - 1);
        // listes balayées : la position (0 à 15) parcourt toute la liste, quelle que soit sa longueur
        Scan sc1, sc2;
        sc1.count = std::clamp ((int) p[w1Count] + 1, 1, 16); sc2.count = std::clamp ((int) p[w2Count] + 1, 1, 16);
        for (int k = 0; k < 16; ++k)
        {
            sc1.w[(size_t) k] = std::clamp ((int) p[w1Scan + k], 0, wb.size() - 1);
            sc2.w[(size_t) k] = std::clamp ((int) p[w2Scan + k], 0, wb.size() - 1);
        }
        sc1.scale = (sc1.count - 1) / 15.0; sc2.scale = (sc2.count - 1) / 15.0;
        double a1 = 0.0, a2 = 0.0, a3 = 0.0, l1 = 0.0, l2 = 0.0;

        const auto offsetCoef = std::exp (-1.0 / (0.0005 * sr)); // 1.1 : fondu de raccord (~2 ms)
        for (int i = 0; i < n; ++i)
        {
            a1 = e1.next(); a2 = e2.next(); a3 = e3.next();
            lfoAge += 1.0 / sr;
            l1 = lfo1.next ((int) p[l1Shape], p[l1Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l1Delay]));
            l2 = lfo2.next ((int) p[l2Shape], p[l2Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l2Delay]));
            const auto note = pitch.next (glideCoef) + p[tune] + l2 * p[l2Pitch] / 100.0;
            const auto posMod = a3 * p[env3Pos] + l1 * p[l1Pos];

            const auto s1 = osc (wb, sc1, o1, noteHz (note + off1) / sr, (p[w1Pos] + posMod) * sc1.scale, m1);
            const auto s2 = (mx2 > 0.0 || mxR > 0.0) ? osc (wb, sc2, o2, noteHz (note + off2) / sr, (p[w2Pos] + posMod) * sc2.scale, m2) : 0.0;
            double s3 = 0.0;
            if (mx3 > 0.0)
            {
                const auto dt3 = std::clamp (noteHz (note + off3) / sr, 1.0e-7, 0.45);
                s3 = wb.read (w3, WaveBank::mipFor (dt3), o3.ph);
                o3.ph += dt3; o3.ph -= std::floor (o3.ph);
            }

            const auto x = s1 * mx1 + s2 * mx2 + s3 * mx3 + s1 * s2 * mxR + (mxN > 0.0 ? noise.next() * mxN : 0.0);
            const auto kt = ktAmt * (note - 60.0) / 12.0 + l2 * l2Oct;
            const auto fc1 = std::clamp ((double) p[f1Cutoff] * fastExp2 (kt + env1Oct1 * a1), 16.0, 0.45 * sr);
            auto filter1 = [&] (double in) -> double
            {
                if (mode1 < 7) return f1.process (in, fc1, res1, sr, modes[mode1], drv);
                if (mode1 == 7) return vowel (softClip (in * drv), fc1, res1);
                return comb.process (softClip (in * drv), fc1, (mode1 == 8 ? 1.0 : -1.0) * (0.2 + 0.78 * res1), sr);
            };
            double y;
            if (parallel)
            {
                const auto fc2 = std::clamp ((double) p[f2Cutoff] * fastExp2 (kt + env1Oct2 * a1), 16.0, 0.45 * sr);
                y = 0.6 * (filter1 (x * 0.35) + f2.processMorph (x * 0.35, fc2, res2, sr, mrph));
            }
            else
            {
                y = filter1 (x * 0.35);
                // Filtre 2 ouvert et sans résonance : inutile de le calculer
                if (p[f2Cutoff] < 17500.0f || res2 > 0.0 || mrph > 0.0 || env1Oct2 != 0.0)
                {
                    const auto fc2 = std::clamp ((double) p[f2Cutoff] * fastExp2 (kt + env1Oct2 * a1), 16.0, 0.45 * sr);
                    y = f2.processMorph (y, fc2, res2, sr, mrph);
                }
            }
            auto out = dc.process (y) * a2 * gain;
            if (declick) { offset = lastOut - out; declick = false; }
            out += offset; offset *= offsetCoef; lastOut = out;
            L[i] += (float) (out * gl);
            R[i] += (float) (out * gr);
        }
        lastE1 = a1; lastE2 = a2; lastE3 = a3; lastL1 = l1; lastL2 = l2;
        if (! e2.isActive()) active = false;
    }

private:
    struct Mut { int a; double amtA; int b; double amtB; double ratio; };
    struct Scan { std::array<int, 16> w {}; int count = 16; double scale = 1.0; };

    /** Filtre voyelle : deux formants qui glissent de A à U selon la coupure. */
    double vowel (double x, double fc, double res) noexcept
    {
        static constexpr double F1[5] { 800, 400, 300, 450, 325 }, F2[5] { 1150, 1600, 2300, 800, 700 }; // A E I O U
        const auto pos = std::clamp (std::log2 (std::max (60.0, fc) / 60.0) / std::log2 (18000.0 / 60.0) * 4.0, 0.0, 4.0);
        const auto k = std::min (3, (int) pos);
        const auto f = pos - (double) k;
        const auto fa = F1[k] + (F1[k + 1] - F1[k]) * f, fb = F2[k] + (F2[k + 1] - F2[k]) * f;
        const auto q = 0.55 + 0.4 * res;
        return 1.6 * (fv1.process (x, std::min (fa, 0.45 * sr), q, sr).band + 0.7 * fv2.process (x, std::min (fb, 0.45 * sr), q, sr).band);
    }

    /** Un oscillateur : position dans la banque (fondu entre deux formes), deux mutants en chaîne. */
    static double osc (const WaveBank& wb, const Scan& sc, MorphOscState& st, double dt, double pos, const Mut& mu) noexcept
    {
        dt = std::clamp (dt, 1.0e-7, 0.45);
        pos = std::clamp (pos, 0.0, (double) (sc.count - 1));
        const auto k0 = (int) pos;
        const auto w0 = sc.w[(size_t) k0];
        const auto w1 = sc.w[(size_t) std::min (k0 + 1, sc.count - 1)];
        const auto f = pos - (double) k0;
        auto rd = [&] (double ph, double d) { const auto l = WaveBank::mipFor (d); return wb.read (w0, l, ph) * (1.0 - f) + wb.read (w1, l, ph) * f; };
        auto wrap = [] (double x) { return x - std::floor (x); };

        auto readPhase = st.ph;
        auto effDt = dt;
        double window = 1.0;
        bool fmDone = false;
        // mutants de phase (dans l'ordre A puis B)
        for (int k = 0; k < 2; ++k)
        {
            const auto mut = k == 0 ? mu.a : mu.b;
            const auto amt = k == 0 ? mu.amtA : mu.amtB;
            if (amt <= 0.0) continue;
            switch (mut)
            {
                case 1: // FM : modulateur sinusoïdal interne au rapport donné
                    if (! fmDone) { st.pm += dt * mu.ratio; st.pm -= std::floor (st.pm); fmDone = true; }
                    readPhase = wrap (readPhase + amt * 0.35 * fastSin2Pi (st.pm));
                    effDt *= 1.0 + amt * mu.ratio;
                    break;
                case 2: // synchro : lecture accélérée, remise à zéro à chaque cycle
                {
                    const auto r = 1.0 + amt * 7.0;
                    readPhase = wrap (readPhase * r);
                    effDt *= r;
                    window *= 1.0 - 0.6 * amt * st.ph;
                    break;
                }
                case 3: // compression de phase (largeur variable)
                {
                    const auto c = 0.5 - 0.45 * amt;
                    readPhase = readPhase < c ? readPhase * 0.5 / c : 0.5 + (readPhase - c) * 0.5 / (1.0 - c);
                    effDt /= std::max (0.1, c * 2.0);
                    break;
                }
                default: break;
            }
        }
        effDt = std::min (effDt, 0.45);
        double y = rd (readPhase, effDt);
        // mutants de lecture et de sortie
        for (int k = 0; k < 2; ++k)
        {
            const auto mut = k == 0 ? mu.a : mu.b;
            const auto amt = k == 0 ? mu.amtA : mu.amtB;
            if (amt <= 0.0) continue;
            switch (mut)
            {
                case 4: y = fastSin2Pi (0.25 * y * (1.0 + amt * 5.0)); break; // repli
                case 5: // pile d'ondes : deux copies désaccordées (jusqu'à ±25 centièmes)
                {
                    const auto c = amt * 25.0 / 1200.0;
                    const auto up = fastExp2 (c), down = fastExp2 (-c);
                    y = (y + amt * (rd (st.st1, effDt * up) + rd (st.st2, effDt * down))) / (1.0 + 1.3 * amt);
                    st.st1 = wrap (st.st1 + dt * up); st.st2 = wrap (st.st2 + dt * down);
                    break;
                }
                case 6: y = 0.5 * (y - rd (wrap (readPhase + 0.5 * amt), effDt)) * (1.0 + amt); break; // phase décalée
                case 7: // harmonique : lecture à h fois la fréquence, h glissant de 1 à 8
                {
                    const auto h = 1.0 + amt * 7.0;
                    const auto hi = std::floor (h);
                    const auto fr = h - hi;
                    const auto d1 = std::min (0.45, effDt * hi), d2 = std::min (0.45, effDt * (hi + 1.0));
                    y = rd (wrap (readPhase * hi), d1) * (1.0 - fr) + rd (wrap (readPhase * (hi + 1.0)), d2) * fr;
                    break;
                }
                default: break;
            }
        }
        st.ph += dt;
        st.ph -= std::floor (st.ph);
        return y * window;
    }

    double sr = 48000.0, pan = 0.0, spread = 0.0;
    double lastE1 = 0.0, lastE2 = 0.0, lastE3 = 0.0, lastL1 = 0.0, lastL2 = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false, declick = false;
    double lastOut = 0.0, offset = 0.0; // 1.1 : raccord sans clic quand les oscillateurs sont relancés
    std::uint32_t rng = 11;
    MorphOscState o1, o2, o3;
    Ladder f1;
    StateFilter f2, fv1, fv2;
    CombFilter comb;
    Adsr e1, e2, e3;
    SimpleLfo lfo1, lfo2;
    double lfoAge = 1.0e9;
    Glide pitch;
    WhiteNoise noise { 0xFEEDu };
    DcBlocker dc;
};

class WaveMorph final : public SynthBase<MorphVoice, 8>
{
public:
    WaveMorph()
    {
        (void) morph::bank(); // tables calculées à la création (fil de l'interface), jamais sur le fil audio
        for (int i = 0; i < 8; ++i) voices[(size_t) i].seed (0x3C6Eu * (std::uint32_t) (i + 1));
    }
    const ModuleDesc& desc() const noexcept override { return morph::desc(); }
protected:
    const float* adjust (const float* p) noexcept override
    { return syncRates (p, morph::numSlots, { { morph::l1Rate, morph::l1Sync }, { morph::l2Rate, morph::l2Sync } }); }
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[morph::voices], 1, 8);
        switch ((int) p[morph::vMode])
        {
            case 1: setMode (true, true, v); break;
            case 2: setMode (true, true, v, std::clamp ((int) p[morph::uniVoices], 2, 8)); break;
            default: setMode (v == 1, true, v); break;
        }
    }
};

} // namespace grv::instruments
