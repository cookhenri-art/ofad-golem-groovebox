/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Poly à tables d'ondes » — principe des synthétiseurs hybrides numériques allemands à trois
    oscillateurs (architecture reprise, ni nom, ni panneau, ni table, ni son d'origine) :
      - oscillateurs 1 et 2 : formes virtuelles-analogiques (impulsion, scie, triangle, sinus) ou
        tables d'ondes parcourues par la « position » (le bouton de largeur sert de position, comme
        sur ces machines) ; octave (64' à 2'), demi-tons, désaccord, FM linéaire (source choisie),
        largeur / position modulée (source choisie) ; l'oscillateur 2 peut se synchroniser sur le 3 ;
      - oscillateur 3 : formes simples, maître de la synchro ;
      - mélangeur : trois oscillateurs, modulation en anneau (1 × 2), bruit coloré ;
      - deux filtres (passe-bas, passe-bande, passe-haut, réjection en 12 ou 24 dB, peigne + / −),
        en série ou en parallèle avec balance, saturation à quatre courbes, suivi du clavier,
        enveloppe et vélocité ;
      - quatre enveloppes à cinq modes (ADSR, ADS1DS2R, un coup, boucle S1-S2, boucle tout),
        trois LFO, matrice de 16 routages (toutes les sources ci-dessus), arpège ;
      - poly 8 voix, mono, unisson. Les douze tables sont calculées par le programme (bibliothèque
        des 64 ondes du wave morph), aucune table d'un instrument existant n'est reprise.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "WaveMorph.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace blo
{
    enum Slot
    {
        vMode = 0, voices, uniVoices, uniDetune, glide, tune, level, velAmp, spreadAmt,
        o1Shape, o1Oct, o1Semi, o1Detune, o1Pos, o1Pwm, o1PwmSrc, o1Fm, o1FmSrc, o1Level,
        o2Shape, o2Oct, o2Semi, o2Detune, o2Pos, o2Pwm, o2PwmSrc, o2Fm, o2FmSrc, o2Level, o2Sync,
        o3Shape, o3Oct, o3Semi, o3Detune, o3Pw, o3Level,
        ringLevel, noiseLevel, noiseColor, vibrato,
        f1Type, f1Cut, f1Reso, f1Drive, f1Curve, f1Key, f1Env, f1Vel,
        f2Type, f2Cut, f2Reso, f2Drive, f2Curve, f2Key, f2Env, f2Vel,
        routing, fBalance,
        envBase, // 4 enveloppes × 7 réglages : mode, A, D, S1, D2, S2, R
        lfoBase = envBase + 28, // 3 LFO × 4 réglages : forme, vitesse, synchro, retard
        numSlots = lfoBase + 12
    };
    constexpr int envStride = 7, lfoStride = 4;

    /** Douze tables de huit ondes, prises dans la bibliothèque du wave morph (calculée). */
    inline const std::array<std::array<int, 8>, 12>& tables()
    {
        static const std::array<std::array<int, 8>, 12> t { {
            { 40, 41, 36, 37, 38, 39, 11, 13 },   // résonances
            { 6, 32, 33, 7, 34, 35, 6, 33 },      // voyelles
            { 30, 31, 27, 28, 29, 5, 24, 25 },    // orgues
            { 3, 22, 21, 20, 19, 4, 12, 10 },     // impulsions
            { 0, 50, 46, 47, 48, 49, 45, 44 },    // plis
            { 2, 51, 52, 53, 61, 63, 18, 62 },    // synchros
            { 56, 54, 55, 14, 15, 57, 58, 60 },   // numérique
            { 0, 42, 43, 8, 11, 12, 26, 9 },      // cloches
            { 0, 1, 17, 16, 2, 18, 41, 40 },      // doux vers vif
            { 59, 60, 57, 58, 44, 45, 62, 63 },   // bosses
            { 35, 9, 32, 34, 7, 6, 31, 30 },      // chœurs
            { 54, 55, 56, 3, 23, 20, 4, 19 },     // escaliers
        } };
        return t;
    }

    inline const std::vector<std::string>& shapeNames()
    {
        static const std::vector<std::string> n {
            "Impulsion", "Scie", "Triangle", "Sinus",
            "Table : r\xC3\xA9sonances", "Table : voyelles", "Table : orgues", "Table : impulsions", "Table : plis", "Table : synchros",
            "Table : num\xC3\xA9rique", "Table : cloches", "Table : doux \xE2\x86\x92 vif", "Table : bosses", "Table : ch\xC5\x93urs", "Table : escaliers" };
        return n;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> octs { "64'", "32'", "16'", "8'", "4'", "2'" };
            const std::vector<std::string> modSrc { "LFO 1", "LFO 2", "LFO 3", "Env. 3", "Env. 4" };
            const std::vector<std::string> fTypes { "Arr\xC3\xAAt", "Passe-bas 24", "Passe-bas 12", "Passe-bande 24", "Passe-bande 12", "Passe-haut 24", "Passe-haut 12",
                                                    "R\xC3\xA9jection 24", "R\xC3\xA9jection 12", "Peigne +", "Peigne \xE2\x88\x92" };
            const std::vector<std::string> curves { "Douce", "Lampe", "Dure", "Repli" };
            ModuleDesc m;
            m.type = "blo";
            m.name = "Ondes Bretzel";
            m.family = "Nappes";
            m.principle = "Hybride num\xC3\xA9rique : 3 oscillateurs (formes analogiques ou tables), 2 filtres s\xC3\xA9rie / parall\xC3\xA8le, 4 enveloppes, 3 LFO, matrice";
            m.params = {
                C ("vmode", "Mode de voix", { "Poly", "Mono", "Unisson" }),
                I ("voices", "Voix", "", 1, 8, 8),
                I ("univoices", "Voix d'unisson", "", 2, 8, 4),
                P ("unidetune", "D\xC3\xA9saccord unisson", "ct", 0, 50, 12),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -24, 6, -8),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 40),
                P ("spread", "\xC3\x89talement", "%", 0, 100, 30),

                C ("o1shape", "Forme 1", shapeNames(), 1), C ("o1oct", "Octave 1", octs, 3), I ("o1semi", "Demi-tons 1", "dt", -12, 12, 0),
                P ("o1detune", "D\xC3\xA9saccord 1", "ct", -50, 50, 0), P ("o1pos", "Largeur / position 1", "%", 0, 100, 0),
                P ("o1pwm", "Modulation 1", "%", 0, 100, 0), C ("o1pwmsrc", "Source 1", modSrc, 1),
                P ("o1fm", "FM 1", "%", 0, 100, 0), C ("o1fmsrc", "Source FM 1", { "Osc. 3", "Osc. 2", "Bruit" }),
                P ("o1level", "Niveau 1", "%", 0, 100, 80),

                C ("o2shape", "Forme 2", shapeNames(), 1), C ("o2oct", "Octave 2", octs, 3), I ("o2semi", "Demi-tons 2", "dt", -12, 12, 0),
                P ("o2detune", "D\xC3\xA9saccord 2", "ct", -50, 50, 8), P ("o2pos", "Largeur / position 2", "%", 0, 100, 0),
                P ("o2pwm", "Modulation 2", "%", 0, 100, 0), C ("o2pwmsrc", "Source 2", modSrc, 1),
                P ("o2fm", "FM 2", "%", 0, 100, 0), C ("o2fmsrc", "Source FM 2", { "Osc. 3", "Osc. 1", "Bruit" }),
                P ("o2level", "Niveau 2", "%", 0, 100, 60),
                C ("o2sync", "Synchro sur 3", { "Non", "Oui" }),

                C ("o3shape", "Forme 3", { "Impulsion", "Scie", "Triangle", "Sinus" }, 1), C ("o3oct", "Octave 3", octs, 2), I ("o3semi", "Demi-tons 3", "dt", -12, 12, 0),
                P ("o3detune", "D\xC3\xA9saccord 3", "ct", -50, 50, 0), P ("o3pw", "Largeur 3", "%", 0, 100, 0),
                P ("o3level", "Niveau 3", "%", 0, 100, 0),

                P ("ring", "Anneau 1 \xC3\x97 2", "%", 0, 100, 0),
                P ("noise", "Bruit", "%", 0, 100, 0),
                P ("noisecol", "Couleur du bruit", "%", -100, 100, 0),
                P ("vibrato", "LFO 1 \xE2\x86\x92 hauteur", "ct", 0, 100, 0),

                C ("f1type", "Filtre 1", fTypes, 1), P ("f1cut", "Coupure 1", "Hz", 20, 20000, 2500, 0.3f), P ("f1reso", "R\xC3\xA9sonance 1", "%", 0, 100, 15),
                P ("f1drive", "Saturation 1", "%", 0, 100, 0), C ("f1curve", "Courbe 1", curves),
                P ("f1key", "Clavier 1", "%", 0, 100, 40), P ("f1env", "Env. 1", "%", -100, 100, 35), P ("f1vel", "V\xC3\xA9locit\xC3\xA9 1", "%", 0, 100, 0),
                C ("f2type", "Filtre 2", fTypes, 0), P ("f2cut", "Coupure 2", "Hz", 20, 20000, 6000, 0.3f), P ("f2reso", "R\xC3\xA9sonance 2", "%", 0, 100, 10),
                P ("f2drive", "Saturation 2", "%", 0, 100, 0), C ("f2curve", "Courbe 2", curves),
                P ("f2key", "Clavier 2", "%", 0, 100, 40), P ("f2env", "Env. 2", "%", -100, 100, 0), P ("f2vel", "V\xC3\xA9locit\xC3\xA9 2", "%", 0, 100, 0),
                C ("routing", "Routage", { "S\xC3\xA9rie", "Parall\xC3\xA8le" }),
                P ("fbalance", "Balance F1 \xE2\x86\x94 F2", "%", 0, 100, 50),
            };
            // quatre enveloppes : 1 filtres, 2 ampli, 3 et 4 libres (matrice, modulation des oscillateurs)
            const std::vector<std::string> envModes { "ADSR", "ADS1DS2R", "Un coup", "Boucle S1-S2", "Boucle tout" };
            static const float defA[4] { 2, 2, 400, 5 }, defD[4] { 600, 900, 1500, 300 }, defS[4] { 30, 80, 50, 0 }, defR[4] { 500, 600, 1000, 300 };
            for (int e = 0; e < 4; ++e)
            {
                const auto k = "e" + std::to_string (e + 1);
                const auto n = e == 0 ? std::string ("Env. filtre") : e == 1 ? std::string ("Env. ampli") : "Env. " + std::to_string (e + 1);
                m.params.push_back (C (k + "mode", n + " mode", envModes));
                m.params.push_back (P (k + "a", n + " attaque", "ms", 0.5f, 10000, defA[e], 0.3f));
                m.params.push_back (P (k + "d", n + " d\xC3\xA9" "clin", "ms", 5, 10000, defD[e], 0.3f));
                m.params.push_back (P (k + "s", n + " niveau 1", "%", 0, 100, defS[e]));
                m.params.push_back (P (k + "d2", n + " d\xC3\xA9" "clin 2", "ms", 5, 10000, 800, 0.3f));
                m.params.push_back (P (k + "s2", n + " niveau 2", "%", 0, 100, defS[e]));
                m.params.push_back (P (k + "r", n + " rel\xC3\xA2" "chement", "ms", 5, 10000, defR[e], 0.3f));
            }
            const std::vector<std::string> lfoShapes { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire" };
            static const float defRate[3] { 5.0f, 0.4f, 2.0f };
            for (int l = 0; l < 3; ++l)
            {
                const auto k = "l" + std::to_string (l + 1);
                const auto n = "LFO " + std::to_string (l + 1);
                m.params.push_back (C (k + "shape", n + " forme", lfoShapes, l == 1 ? 1 : 0));
                m.params.push_back (P (k + "rate", n + " vitesse", "Hz", 0.02f, 30, defRate[l], 0.4f));
                m.params.push_back (C (k + "sync", n + " synchro", lfoSyncChoices()));
                m.params.push_back (C (k + "delay", n + " retard", lfoDelayChoices()));
            }
            m.sections = {
                { "global", "Global", "global", { "vmode", "voices", "univoices", "unidetune", "glide", "tune", "spread", "level" }, "global" },
                { "osc1", "Oscillateur 1", "osc", { "o1shape", "o1oct", "o1semi", "o1detune", "o1pos", "o1pwm", "o1pwmsrc", "o1level" }, "osc" },
                { "osc1fm", "Oscillateur 1 : FM", "osc", { "o1fm", "o1fmsrc" }, "osc" },
                { "osc2", "Oscillateur 2", "osc", { "o2shape", "o2oct", "o2semi", "o2detune", "o2pos", "o2pwm", "o2pwmsrc", "o2level" }, "osc" },
                { "osc2fm", "Oscillateur 2 : FM et synchro", "osc", { "o2fm", "o2fmsrc", "o2sync" }, "osc" },
                { "osc3", "Oscillateur 3", "osc", { "o3shape", "o3oct", "o3semi", "o3detune", "o3pw", "o3level" }, "osc" },
                { "mix", "M\xC3\xA9langeur", "mix", { "o1level", "o2level", "o3level", "ring", "noise", "noisecol", "vibrato" }, "osc" },
                { "filter1", "Filtre 1", "filter", { "f1type", "f1cut", "f1reso", "f1drive", "f1curve", "f1key", "f1env", "f1vel" }, "filter" },
                { "filter2", "Filtre 2", "filter", { "f2type", "f2cut", "f2reso", "f2drive", "f2curve", "f2key", "f2env", "f2vel" }, "filter" },
                { "routing", "Routage des filtres", "filter", { "routing", "fbalance" }, "filter" },
                { "env1", "Enveloppe filtre", "env", { "e1mode", "e1a", "e1d", "e1s", "e1d2", "e1s2", "e1r" }, "env" },
                { "env2", "Enveloppe ampli", "env", { "e2mode", "e2a", "e2d", "e2s", "e2d2", "e2s2", "e2r", "velamp" }, "amp" },
                { "env3", "Enveloppe 3", "env", { "e3mode", "e3a", "e3d", "e3s", "e3d2", "e3s2", "e3r" }, "env" },
                { "env4", "Enveloppe 4", "env", { "e4mode", "e4a", "e4d", "e4s", "e4d2", "e4s2", "e4r" }, "env" },
                { "lfo1", "LFO 1", "lfo", { "l1shape", "l1rate", "l1sync", "l1delay", "vibrato" }, "mod" },
                { "lfo2", "LFO 2", "lfo", { "l2shape", "l2rate", "l2sync", "l2delay" }, "mod" },
                { "lfo3", "LFO 3", "lfo", { "l3shape", "l3rate", "l3sync", "l3delay" }, "mod" },
            };
            const std::string N = "Nappes", K = "Claviers", B = "Basses", L = "Leads", PL = "Plucks", FX = "Effets", A = "Arp\xC3\xA8ges";
            m.presets = {
                { "Init", { { "o2level", 0 }, { "f1cut", 20000 }, { "f1env", 0 }, { "e2s", 100 }, { "e2s2", 100 }, { "level", -10 } }, "Init" },
                { "Nappe de verre", { { "level", -11.1f }, { "o1shape", 5 }, { "o1pos", 30 }, { "o1pwm", 40 }, { "o1pwmsrc", 1 }, { "o2shape", 11 }, { "o2pos", 20 }, { "o2pwm", 30 }, { "o2pwmsrc", 2 },
                                       { "f1cut", 3500 }, { "f1env", 10 }, { "e2a", 900 }, { "e2r", 2500 }, { "e1a", 1200 }, { "e1r", 2000 }, { "l2rate", 0.15f }, { "l3rate", 0.23f }, { "spread", 70 } }, N },
                { "Voix lointaines", { { "level", -12 }, { "o1shape", 5 }, { "o1pos", 50 }, { "o1pwm", 60 }, { "o1pwmsrc", 3 }, { "o2shape", 14 }, { "o2pos", 40 }, { "e3a", 2500 }, { "e3d", 3000 }, { "e3s", 30 },
                                        { "f1type", 2 }, { "f1cut", 2800 }, { "e2a", 1500 }, { "e2r", 3000 }, { "vibrato", 6 }, { "spread", 80 } }, N },
                { "Balayage de tables", { { "level", -8.4f }, { "o1shape", 4 }, { "o1pwm", 80 }, { "o1pwmsrc", 3 }, { "o2shape", 6 }, { "o2pwm", 60 }, { "o2pwmsrc", 3 }, { "e3a", 3000 }, { "e3d", 4000 }, { "e3s", 20 },
                                          { "e3mode", 3 }, { "e3d2", 3500 }, { "e3s2", 80 }, { "f1cut", 4000 }, { "f1reso", 25 }, { "e2a", 1200 }, { "e2r", 2500 } }, N },
                { "Orgue num\xC3\xA9rique", { { "level", -9.7f }, { "o1shape", 6 }, { "o1pos", 40 }, { "o2shape", 6 }, { "o2pos", 70 }, { "o2oct", 4 }, { "f1cut", 9000 }, { "f1env", 0 }, { "e2a", 3 }, { "e2s", 100 }, { "e2r", 120 } }, K },
                { "Cloches tables", { { "level", -5.8f }, { "o1shape", 11 }, { "o1pos", 40 }, { "o2shape", 3 }, { "o2oct", 5 }, { "o2semi", 7 }, { "ring", 40 }, { "o2level", 0 }, { "f1cut", 12000 },
                                      { "e2a", 1 }, { "e2d", 2500 }, { "e2s", 0 }, { "e2r", 2500 } }, K },
                { "Basse synchro", { { "level", -6.4f }, { "vmode", 1 }, { "o1shape", 1 }, { "o1oct", 2 }, { "o2shape", 1 }, { "o2oct", 2 }, { "o2sync", 1 }, { "o3oct", 1 }, { "o2semi", 7 },
                                     { "f1cut", 700 }, { "f1env", 60 }, { "e1d", 300 }, { "e1s", 10 }, { "f1drive", 40 }, { "f1curve", 1 } }, B },
                { "Basse FM", { { "level", 2.6f }, { "vmode", 1 }, { "o1shape", 3 }, { "o1oct", 2 }, { "o1fm", 45 }, { "o1fmsrc", 0 }, { "o3shape", 3 }, { "o3oct", 2 }, { "o2level", 0 },
                                { "f1cut", 1500 }, { "f1env", 40 }, { "e1d", 250 }, { "e1s", 0 }, { "e2d", 600 }, { "e2s", 60 } }, B },
                { "Lead synchro", { { "level", -2 }, { "vmode", 1 }, { "glide", 60 }, { "o2shape", 1 }, { "o2sync", 1 }, { "o2pwm", 0 }, { "o3oct", 3 }, { "o2semi", 12 }, { "o1level", 30 },
                                    { "f1cut", 4000 }, { "f1reso", 30 }, { "vibrato", 10 }, { "l1delay", 4 } }, L },
                { "Lead unisson", { { "level", -12.2f }, { "vmode", 2 }, { "univoices", 6 }, { "unidetune", 16 }, { "o1shape", 1 }, { "o2shape", 1 }, { "o2detune", 12 }, { "f1cut", 3000 }, { "glide", 40 } }, L },
                { "Pluck num\xC3\xA9rique", { { "level", -1.2f }, { "o1shape", 10 }, { "o1pos", 60 }, { "o2shape", 7 }, { "o2pos", 30 }, { "f1cut", 800 }, { "f1env", 70 }, { "e1d", 220 }, { "e1s", 0 },
                                             { "e2d", 500 }, { "e2s", 0 }, { "e2r", 400 } }, PL },
                { "Pluck plis", { { "level", -3.5f }, { "o1shape", 8 }, { "o1pwm", 70 }, { "o1pwmsrc", 3 }, { "e3d", 250 }, { "e3s", 0 }, { "e3a", 0.5f }, { "f1cut", 2000 }, { "f1env", 40 },
                                  { "e1d", 300 }, { "e1s", 0 }, { "e2d", 600 }, { "e2s", 0 }, { "e2r", 500 } }, PL },
                { "Peigne m\xC3\xA9tallique", { { "level", -1.9f }, { "f1type", 9 }, { "f1cut", 300 }, { "f1reso", 75 }, { "noise", 50 }, { "o2level", 0 }, { "e2d", 1500 }, { "e2s", 0 }, { "e2r", 1500 } }, FX },
                { "Boucle d'enveloppe", { { "e1mode", 4 }, { "e1a", 120 }, { "e1d", 180 }, { "e1d2", 160 }, { "e1s2", 0 }, { "f1env", 70 }, { "f1cut", 500 }, { "f1reso", 45 },
                                          { "e2s", 100 }, { "e2r", 800 } }, A },
                { "Arp\xC3\xA8ge tables", { { "level", 1.3f }, { "arp_mode", 1 }, { "arp_rate", 3 }, { "arp_oct", 2 }, { "o1shape", 9 }, { "o1pos", 40 }, { "f1cut", 1500 }, { "f1env", 50 },
                                            { "e1d", 180 }, { "e1s", 0 }, { "e2d", 300 }, { "e2s", 20 } }, A },
                { "Parall\xC3\xA8le voyelles", { { "level", -9.9f }, { "routing", 1 }, { "f1type", 4 }, { "f1cut", 700 }, { "f1reso", 60 }, { "f2type", 4 }, { "f2cut", 2200 }, { "f2reso", 60 },
                                                { "o1shape", 1 }, { "e2a", 600 }, { "e2r", 2000 }, { "l2rate", 0.3f } }, N },
                { "Souffle color\xC3\xA9", { { "level", -2.4f }, { "o1level", 0 }, { "o2level", 0 }, { "noise", 90 }, { "noisecol", -60 }, { "f1type", 4 }, { "f1cut", 1200 }, { "f1reso", 70 },
                                             { "e2a", 1500 }, { "e2r", 3000 }, { "f1env", 20 }, { "e1a", 2000 } }, FX },
            };
            HubSpec hub;
            hub.extEnvs = { "Env. filtre", "Env. ampli", "Env. 3", "Env. 4" };
            hub.extLfos = { "LFO 1", "LFO 2", "LFO 3" };
            hub.envs = 0; hub.lfos = 0; hub.slots = 16; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // v0.47 (maj. 5 nov.) : banque d'usine (au moins 90 sons par synthé)
            return m;
        }();
        return d;
    }
}

/** Enveloppe à cinq modes (ADSR, ADS1DS2R, un coup, boucle S1-S2, boucle tout). */
class ModeEnv
{
public:
    void prepare (double s) noexcept { sr = s; }
    void reset() noexcept { stage = idle; value = 0.0; }
    void set (int m, double aMs, double dMs, double s1, double d2Ms, double s2, double rMs) noexcept
    {
        mode = m; s1Lvl = s1; s2Lvl = s2;
        aInc = 1.0 / std::max (1.0, aMs * 0.001 * sr);
        dCoef = coef (dMs); d2Coef = coef (d2Ms); rCoef = coef (rMs);
    }
    void gate (bool on) noexcept
    {
        if (on) { stage = attack; return; }
        if (stage != idle && mode != 2) stage = release; // un coup : la note va jusqu'au bout
    }
    double next() noexcept
    {
        switch (stage)
        {
            case idle: return 0.0;
            case attack: value += aInc; if (value >= 1.0) { value = 1.0; stage = decay; } break;
            case decay:
                value = s1Lvl + (value - s1Lvl) * dCoef;
                if (std::abs (value - s1Lvl) < 1.0e-3) { value = s1Lvl; stage = mode == 0 ? sustain : decay2; }
                break;
            case decay2:
                value = s2Lvl + (value - s2Lvl) * d2Coef;
                if (std::abs (value - s2Lvl) < 1.0e-3)
                {
                    value = s2Lvl;
                    if (mode == 2) stage = release;          // un coup
                    else if (mode == 3) stage = decay;       // boucle S1-S2
                    else if (mode == 4) stage = attack;      // boucle tout
                    else stage = sustain;
                }
                break;
            case sustain: break;
            case release: value *= rCoef; if (value < 1.0e-4) { value = 0.0; stage = idle; } break;
        }
        return value;
    }
    bool isActive() const noexcept { return stage != idle; }
private:
    enum Stage { idle, attack, decay, decay2, sustain, release };
    double coef (double ms) const noexcept { return std::exp (std::log (0.001) / std::max (1.0, ms * 0.001 * sr)); }
    double sr = 48000.0, value = 0.0, aInc = 1.0, dCoef = 0.9, d2Coef = 0.9, rCoef = 0.9, s1Lvl = 1.0, s2Lvl = 1.0;
    int mode = 0;
    Stage stage = idle;
};

/** Filtre multimode des machines hybrides : échelle, variables d'état (réjection), peigne. */
class BloFilter
{
public:
    void reset() noexcept { ladder.reset(); s1.reset(); s2.reset(); comb.reset(); uPrev = 0.0; fPrev = prim (lastCurve, 0.0); }

    /** Courbes de saturation (0 douce, 1 lampe, 2 dure, 3 repli) et leurs primitives. */
    static double shapeAt (int c, double u) noexcept
    {
        switch (c)
        {
            case 2: return std::clamp (u, -1.0, 1.0);
            case 3: return fastSin2Pi (0.25 * std::clamp (u, -4.0, 4.0));
            default: return std::tanh (u);
        }
    }
    static double prim (int c, double u) noexcept
    {
        switch (c)
        {
            case 2: { const auto a = std::abs (u); return a <= 1.0 ? 0.5 * u * u : a - 0.5; }
            case 3: { const auto w = std::clamp (u, -4.0, 4.0); return -0.6366197723675814 * fastSin2Pi (0.25 * w + 0.25); } // −(2/π)·cos(πu/2)
            default: { u = std::clamp (u, -40.0, 40.0); const auto a = std::abs (u); return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453; }
        }
    }
    double process (double x, int type, double fc, double res, double drive, int curve, double sr) noexcept
    {
        if (type <= 0) return x;
        // saturation avant le filtre : quatre courbes
        if (drive > 0.0)
        {
            const auto g = 1.0 + drive * 9.0;
            const auto v = x * g;
            // v0.22 : les quatre courbes passent par leur antidérivée (ADAA) : moins de repliement
            const auto c = std::clamp (curve, 0, 3);
            const auto u = c == 1 ? v + 0.25 : v;
            if (c != lastCurve) { fPrev = prim (c, uPrev); lastCurve = c; }
            const auto f = prim (c, u);
            const auto du = u - uPrev;
            // part linéaire pondérée par la pente de la courbe (aucune pour le repli) ; borné comme la courbe
            const auto mid = 0.5 * (u + uPrev);
            const auto slope = c == 3 ? 0.0 : c == 2 ? (std::abs (mid) < 1.0 ? 1.0 : 0.0) : 1.0 - std::tanh (mid) * std::tanh (mid);
            double y = std::clamp ((std::abs (du) > 1.0e-3 ? (f - fPrev) / du : shapeAt (c, mid)) + 0.5 * du * slope, -1.0, 1.0);
            uPrev = u; fPrev = f;
            if (c == 1) y -= 0.24491866240370913;                                 // lampe : asymétrique (− tanh 0,25)
            x = y / std::sqrt (g) * 1.4;
        }
        switch (type)
        {
            case 1: return ladder.process (x, fc, res, sr, Ladder::lp24, 1.0);
            case 2: return ladder.process (x, fc, res, sr, Ladder::lp12, 1.0);
            case 3: { const auto a = s1.process (x, fc, res * 0.9, sr).band; return 1.4 * s2.process (a, fc, res * 0.9, sr).band; }
            case 4: return 1.2 * s1.process (x, fc, res, sr).band;
            case 5: return ladder.process (x, fc, res, sr, Ladder::hp24, 1.0);
            case 6: return ladder.process (x, fc, res, sr, Ladder::hp12, 1.0);
            case 7: { const auto o = s1.process (x, fc, res * 0.7, sr); const auto n = o.low + o.high; const auto o2 = s2.process (n, fc, res * 0.7, sr); return o2.low + o2.high; }
            case 8: { const auto o = s1.process (x, fc, res * 0.7, sr); return o.low + o.high; }
            case 9: return comb.process (x, fc, 0.2 + 0.78 * res, sr);
            default: return comb.process (x, fc, -(0.2 + 0.78 * res), sr);
        }
    }
private:
    Ladder ladder;
    StateFilter s1, s2;
    double uPrev = 0.0, fPrev = 0.0;
    int lastCurve = 0;
    CombFilter comb;
};

class BloVoice
{
public:
    void prepare (double s) { sr = s; for (auto& e : env) e.prepare (s); reset(); }
    void reset() noexcept
    {
        oa1.reset(); oa2.reset(); o3.reset(); f1.reset(); f2.reset(); for (auto& e : env) e.reset();
        ph1 = ph2 = 0.0; bl1.reset(); bl2.reset(); dc.reset(); active = false; noiseLp = noiseHp = 0.0;
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
    void release() noexcept { for (auto& e : env) e.gate (false); }
    bool isActive() const noexcept { return active; }

    void modSources (double* e, double* l) const noexcept
    {
        for (int k = 0; k < 4; ++k) e[k] = lastEnv[(size_t) k];
        for (int k = 0; k < 3; ++k) l[k] = lastLfo[(size_t) k];
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace blo;
        const auto& wb = morph::bank();
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! env[1].isActive()) for (auto& e : env) e.gate (true);
        }
        for (int k = 0; k < 4; ++k)
        {
            const auto* q = p + envBase + k * envStride;
            env[(size_t) k].set ((int) q[0], q[1], q[2], q[3] / 100.0, q[4], q[5] / 100.0, q[6]);
        }
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level] + 6.0f) * velocityGain (vel, p[velAmp] / 100.0);
        const auto pn = spread != 0.0 ? spread : pan * p[spreadAmt] / 100.0;
        const auto gl = 1.0 - std::max (0.0, pn) * 0.6, gr = 1.0 + std::min (0.0, pn) * 0.6;
        const auto det = spread * p[uniDetune] / 100.0;
        static constexpr double octOff[6] { -24.0, -12.0, 0.0, 12.0, 24.0, 36.0 }; // 64' … 2' (8' = jeu normal)
        auto offOf = [&] (int oct, int semi, int dt) { return octOff[std::clamp ((int) p[oct], 0, 5)] - 12.0 + std::round (p[semi]) + p[dt] / 100.0 + det; };
        const auto off1 = offOf (o1Oct, o1Semi, o1Detune), off2 = offOf (o2Oct, o2Semi, o2Detune), off3 = offOf (o3Oct, o3Semi, o3Detune);
        const auto sh1 = (int) p[o1Shape], sh2 = (int) p[o2Shape], sh3 = std::clamp ((int) p[o3Shape], 0, 3);
        const auto lv1 = p[o1Level] / 100.0, lv2 = p[o2Level] / 100.0, lv3 = p[o3Level] / 100.0, lvR = p[ringLevel] / 100.0, lvN = p[noiseLevel] / 100.0;
        const auto ncol = p[noiseColor] / 100.0;
        const bool sync2 = p[o2Sync] > 0.5f;
        const auto fm1 = p[o1Fm] / 100.0, fm2 = p[o2Fm] / 100.0;
        const auto fmSrc1 = (int) p[o1FmSrc], fmSrc2 = (int) p[o2FmSrc];
        const auto t1 = (int) p[f1Type], t2 = (int) p[f2Type];
        const bool parallel = p[routing] > 0.5f;
        const auto bal = p[fBalance] / 100.0;
        const auto r1 = p[f1Reso] / 100.0, r2 = p[f2Reso] / 100.0, dr1 = p[f1Drive] / 100.0, dr2 = p[f2Drive] / 100.0;
        const auto cv1 = (int) p[f1Curve], cv2 = (int) p[f2Curve];
        const auto v1 = (p[f1Vel] / 100.0) * 2.0 * (vel - 0.5), v2 = (p[f2Vel] / 100.0) * 2.0 * (vel - 0.5);
        const auto vib = p[vibrato] / 100.0;
        double l[3] {}, e[4] {};

        for (int i = 0; i < n; ++i)
        {
            for (int k = 0; k < 4; ++k) e[k] = env[(size_t) k].next();
            lfoAge += 1.0 / sr;
            for (int k = 0; k < 3; ++k)
            {
                const auto* q = p + lfoBase + k * lfoStride;
                l[k] = lfo[(size_t) k].next ((int) q[0], q[1], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) q[3]));
            }
            auto modOf = [&] (int src) { return src < 3 ? l[src] * 0.5 + 0.5 : e[src - 1]; }; // LFO : 0..1 ; Env. 3 / 4
            const auto note = pitch.next (glideCoef) + p[tune] + vib * l[0];

            // oscillateur 3 (maître de la synchro)
            const auto dt3 = std::clamp (noteHz (note + off3) / sr, 1.0e-7, 0.45);
            double wrap = -1.0;
            static constexpr Osc::Wave vaWave[4] { Osc::pulse, Osc::saw, Osc::triangle, Osc::sine };
            const auto s3 = o3.next (vaWave[sh3], dt3, 0.5 - 0.45 * p[o3Pw] / 100.0, -1.0, &wrap);

            rng = rng * 1664525u + 1013904223u;
            const auto white = (double) (rng >> 8) / 8388608.0 - 1.0;
            // FM linéaire : source choisie (osc. 3, l'autre oscillateur au tour précédent, bruit)
            auto fmIn = [&] (int src, double other) { return src == 0 ? s3 : src == 1 ? other : white; };
            const auto pos1 = std::clamp (p[o1Pos] / 100.0 + p[o1Pwm] / 100.0 * modOf ((int) p[o1PwmSrc]), 0.0, 1.0);
            const auto pos2 = std::clamp (p[o2Pos] / 100.0 + p[o2Pwm] / 100.0 * modOf ((int) p[o2PwmSrc]), 0.0, 1.0);
            const auto s1 = oscOut (wb, sh1, oa1, bl1, ph1, noteHz (note + off1) / sr, pos1, fm1 * fmIn (fmSrc1, last2), -1.0);
            const auto s2 = oscOut (wb, sh2, oa2, bl2, ph2, noteHz (note + off2) / sr, pos2, fm2 * fmIn (fmSrc2, last1), sync2 ? wrap : -1.0);
            last1 = s1; last2 = s2;

            // bruit coloré : vers le grave (−) ou l'aigu (+)
            noiseLp += (white - noiseLp) * 0.08;
            noiseHp = white - noiseLp;
            const auto noise = ncol < 0.0 ? white * (1.0 + ncol) + noiseLp * 2.5 * -ncol : white * (1.0 - ncol) + noiseHp * 1.3 * ncol;

            const auto x = (s1 * lv1 + s2 * lv2 + s3 * lv3 + s1 * s2 * lvR + noise * lvN) * 0.4;
            const auto kt = (note - 60.0) / 12.0;
            const auto fc1 = std::clamp ((double) p[f1Cut] * fastExp2 (p[f1Key] / 100.0 * kt + p[f1Env] / 100.0 * 7.0 * e[0] + v1), 16.0, 0.45 * sr);
            const auto fc2 = std::clamp ((double) p[f2Cut] * fastExp2 (p[f2Key] / 100.0 * kt + p[f2Env] / 100.0 * 7.0 * e[0] + v2), 16.0, 0.45 * sr);
            double y;
            if (parallel) y = f1.process (x, t1, fc1, r1, dr1, cv1, sr) * (1.0 - bal) * 2.0 * 0.6 + f2.process (x, t2, fc2, r2, dr2, cv2, sr) * bal * 2.0 * 0.6;
            else y = f2.process (f1.process (x, t1, fc1, r1, dr1, cv1, sr), t2, fc2, r2, dr2, cv2, sr);
            const auto out = dc.process (y) * e[1] * gain;
            L[i] += (float) (out * gl);
            R[i] += (float) (out * gr);
        }
        for (int k = 0; k < 4; ++k) lastEnv[(size_t) k] = e[k];
        for (int k = 0; k < 3; ++k) lastLfo[(size_t) k] = l[k];
        if (! env[1].isActive()) active = false;
    }

private:
    /** Forme virtuelle-analogique (impulsion à largeur, scie, triangle, sinus) ou table parcourue. */
    double oscOut (const WaveBank& wb, int shape, Osc& va, BlepLine& bl, double& ph, double dt, double pos, double fm, double syncAt) noexcept
    {
        dt = std::clamp (dt * std::max (0.02, 1.0 + 3.0 * fm), 1.0e-7, 0.45);
        if (shape < 4)
        {
            static constexpr Osc::Wave w[4] { Osc::pulse, Osc::saw, Osc::triangle, Osc::sine };
            return va.next (w[shape], dt, 0.5 - 0.45 * pos, syncAt);
        }
        const auto& t = blo::tables()[(size_t) std::clamp (shape - 4, 0, 11)];
        const auto at = pos * 7.0;
        const auto k = std::min (6, (int) at);
        const auto f = at - (double) k;
        const auto lv = WaveBank::mipFor (dt);
        auto rd = [&] (double x) { return wb.read (t[(size_t) k], lv, x) * (1.0 - f) + wb.read (t[(size_t) k + 1], lv, x) * f; };
        const auto y = rd (ph);
        if (syncAt > 0.0)
        {
            // v0.22 : synchro dure corrigée par BLEP (le saut de la table au retour à zéro
            // repliait fortement) ; sortie retardée d'un échantillon, comme les formes analogiques
            auto phAt = ph + syncAt * dt;
            phAt -= std::floor (phAt);
            bl.step (rd (0.0) - rd (phAt), syncAt);
            ph = (1.0 - syncAt) * dt;
        }
        else
        {
            ph += dt;
            ph -= std::floor (ph);
        }
        return bl.push (y);
    }

    double sr = 48000.0, pan = 0.0, spread = 0.0, ph1 = 0.0, ph2 = 0.0, last1 = 0.0, last2 = 0.0, noiseLp = 0.0, noiseHp = 0.0, lfoAge = 1.0e9;
    std::array<double, 4> lastEnv {};
    std::array<double, 3> lastLfo {};
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false;
    std::uint32_t rng = 17;
    Osc oa1, oa2, o3;
    BlepLine bl1, bl2; // tables d'ondes : correction de la synchro
    BloFilter f1, f2;
    std::array<ModeEnv, 4> env;
    std::array<SimpleLfo, 3> lfo;
    Glide pitch;
    DcBlocker dc;
};

class BloSynth final : public SynthBase<BloVoice, 8>
{
public:
    BloSynth()
    {
        (void) morph::bank(); // tables calculées à la création (fil de l'interface), jamais sur le fil audio
        for (int i = 0; i < 8; ++i) voices[(size_t) i].seed (0x5B1Du * (std::uint32_t) (i + 3));
    }
    const ModuleDesc& desc() const noexcept override { return blo::desc(); }
protected:
    const float* adjust (const float* p) noexcept override
    {
        using namespace blo;
        return syncRates (p, numSlots, { { lfoBase + 1, lfoBase + 2 }, { lfoBase + lfoStride + 1, lfoBase + lfoStride + 2 }, { lfoBase + 2 * lfoStride + 1, lfoBase + 2 * lfoStride + 2 } });
    }
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[blo::voices], 1, 8);
        switch ((int) p[blo::vMode])
        {
            case 1: setMode (true, true, v); break;
            case 2: setMode (true, true, v, std::clamp ((int) p[blo::uniVoices], 2, 8)); break;
            default: setMode (v == 1, true, v); break;
        }
    }
};

} // namespace grv::instruments
