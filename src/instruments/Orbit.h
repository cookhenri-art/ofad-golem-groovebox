/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Orbiteur » — principe de la synthèse vectorielle à orbite des synthétiseurs de bureau
    tchèques récents (architecture reprise, ni nom, ni écran, ni son d'origine) :
      - quatre COINS (A, B, C, D) joués en même temps ; chacun : deux générateurs d'harmoniques
        (motifs A à F d'harmoniques ±1/n — A à C paires, D à F impaires, fondamentale comprise —,
        ou N : bruit rose passé dans un passe-haut résonant accordé sur la note), rapport du
        générateur 2, mélange (à gauche : G1 + G2, à droite : modulation AM, PM, PM+AM, PD+AM,
        PD inversée + AM), filtre propre (passe-bas 12 / 24, bande, haut, formant) ;
      - l'ORBITE : l'orbe bleu suit une trajectoire (manuelle, enveloppe ADSR, trois Lissajous,
        polygone, impulsion) autour d'un centre, avec taille, rotation, déformation X / Y et
        vitesse calée sur le tempo ; l'orbe vert tourne autour de lui (sous-orbite : taille,
        vitesse). La position de l'orbe vert mélange les quatre coins ;
      - filtre du mélange, résonance globale (plafond des résonances) ;
      - effets de la voix et du mélange : vibrato, trémolo, saturation (bande en parallèle d'un
        saturateur), chorus. Écho et réverbération : les inserts de la piste ;
      - poly 8 voix ou pile mono (voix désaccordées et étalées), dérive, dispersion stéréo,
        arpège. Les motifs d'harmoniques sont calculés par le programme.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "../synth/Wavetable.h"
#include "WaveMorph.h"
#include "FactoryBanks.h"

#include <atomic>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace orbit
{
    enum Slot
    {
        vMode = 0, voices, stack, stackDetune, glide, tune, level, velAmp, drift, scatter,
        cornerBase,                       // 4 coins × 8 : g1, g2, rapport, mélange, mode, filtre, coupure, résonance
        globalReso = cornerBase + 32, keytrack, mixType, mixCut, mixReso,
        shape, cx, cy, size, rotate, speed, ngon, pulseW, warpX, warpY, retrig,
        subSize, subRate, subSync,
        orbEnv, oA, oD, oS, oR,
        aA, aD, aS, aR,
        vibDepth, vibRate, tremDepth, tremRate, driveAmt, driveCol, chorusMix, chorusRate, chorusDepth,
        waveBase,                         // v0.20 : onde choisie (position W) de chaque générateur : 4 coins × 2
        phaseBase = waveBase + 8,         // v0.21 : phase de départ de la lecture de chaque générateur : 4 coins × 2
        numSlots = phaseBase + 8
    };
    constexpr int cornerStride = 8;

    /** Ce que joue un générateur : les 6 motifs d'harmoniques (A à F), le bruit, ou W : une onde choisie
        dans la bibliothèque du wave morph (v0.20, réglage « onde » du générateur). */
    constexpr int numHarmonic = 6, noisePattern = 6, wavePattern = 7;
    inline const std::vector<std::string>& patternNames()
    {
        static const std::vector<std::string> n { "A", "B", "C", "D", "E", "F", "N (bruit)", "W (onde choisie)" };
        return n;
    }

    /** Six motifs d'harmoniques ±1/n (fondamentale comprise). A à C : paires ; D à F : impaires. */
    inline const WaveBank& bank()
    {
        static const WaveBank b = []
        {
            using cd = std::complex<double>;
            auto sine = [] (double a) { return cd (0.0, -0.5 * a); };
            // signe selon le rang k de l'harmonique dans sa famille (1, 2, 3…)
            auto pattern = [&] (bool even, int signMode)
            {
                return [=] (int h) -> cd
                {
                    if (h == 1) return sine (1.0);
                    if ((h % 2 == 0) != even) return cd();
                    const int k = even ? h / 2 : (h - 1) / 2;
                    const double s = signMode == 0 ? 1.0 : signMode == 1 ? (k % 2 ? 1.0 : -1.0) : (((k - 1) / 2) % 2 ? -1.0 : 1.0);
                    return sine (s / (double) h);
                };
            };
            WaveBank w;
            w.addWave (pattern (true, 1));   // A : paires, signes alternés
            w.addWave (pattern (true, 2));   // B : paires, + + − −
            w.addWave (pattern (true, 0));   // C : paires, toutes positives
            w.addWave (pattern (false, 0));  // D : impaires, toutes positives (C + D : une scie)
            w.addWave (pattern (false, 1));  // E : impaires, signes alternés
            w.addWave (pattern (false, 2));  // F : impaires, + + − −
            return w;
        }();
        return b;
    }

    inline double speedBeats (int choice) noexcept
    {
        static constexpr double beats[9] { 32.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
        return beats[std::clamp (choice, 0, 8)];
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "orbit";
            m.name = "Orbiteur";
            m.family = "Nappes";
            m.principle = "Synth\xC3\xA8se vectorielle : 4 coins (g\xC3\xA9n\xC3\xA9rateurs d'harmoniques, m\xC3\xA9lange, filtre) m\xC3\xA9lang\xC3\xA9s par un orbe en orbite";
            m.params = {
                C ("vmode", "Mode de voix", { "Poly", "Pile mono" }),
                I ("voices", "Voix", "", 1, 8, 8),
                I ("stack", "Voix de la pile", "", 1, 8, 4),
                P ("stackdet", "D\xC3\xA9saccord de la pile", "ct", 0, 50, 12),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -24, 6, -8),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("drift", "D\xC3\xA9rive", "%", 0, 100, 15),
                P ("scatter", "Dispersion st\xC3\xA9r\xC3\xA9o", "%", 0, 100, 40),
            };
            const std::vector<std::string> ratios { "\xC3\x97 1", "\xC3\x97 2", "\xC3\x97 3", "\xC3\x97 4", "\xC3\x97 5", "\xC3\xB7 2" };
            const std::vector<std::string> modes { "AM", "PM", "PM + AM", "PD + AM", "PD inv. + AM" };
            const std::vector<std::string> fTypes { "Arr\xC3\xAAt", "Passe-bas 12", "Passe-bas 24", "Passe-bande", "Passe-haut", "Formant" };
            static const int g1Def[4] { 3, 2, 0, 4 }, g2Def[4] { 2, 3, 5, 6 }, fDef[4] { 1, 2, 3, 1 };
            static const float cutDef[4] { 3000, 1500, 1200, 5000 };
            for (int c = 0; c < 4; ++c)
            {
                const std::string k (1, (char) ('a' + c));
                const std::string n = std::string ("Coin ") + (char) ('A' + c);
                m.params.push_back (C (k + "g1", n + " g\xC3\xA9n. 1", patternNames(), g1Def[c]));
                m.params.push_back (C (k + "g2", n + " g\xC3\xA9n. 2", patternNames(), g2Def[c]));
                m.params.push_back (C (k + "ratio", n + " rapport", ratios));
                m.params.push_back (P (k + "blend", n + " m\xC3\xA9lange", "%", -100, 100, c == 1 ? 30.0f : c == 2 ? -60.0f : 0.0f));
                m.params.push_back (C (k + "bmode", n + " modulation", modes, 1));
                m.params.push_back (C (k + "ftype", n + " filtre", fTypes, fDef[c]));
                m.params.push_back (P (k + "cut", n + " coupure", "Hz", 30, 18000, cutDef[c], 0.3f));
                m.params.push_back (P (k + "reso", n + " r\xC3\xA9sonance", "%", 0, 100, 20));
            }
            const std::vector<std::string> speeds { "8 mesures", "4 mesures", "2 mesures", "1 mesure", "1/2", "1/4", "1/8", "1/16", "1/32" };
            const std::vector<ParamDesc> rest {
                P ("gres", "R\xC3\xA9sonance globale", "%", 0, 100, 100),
                P ("keytrack", "Suivi clavier des filtres", "%", 0, 100, 60),
                C ("mixtype", "Filtre du m\xC3\xA9lange", fTypes, 0),
                P ("mixcut", "Coupure du m\xC3\xA9lange", "Hz", 30, 18000, 8000, 0.3f),
                P ("mixreso", "R\xC3\xA9sonance du m\xC3\xA9lange", "%", 0, 100, 10),
                C ("shape", "Trajectoire", { "Manuelle", "Enveloppe ADSR", "Lissajous 1", "Lissajous 2", "Lissajous 3", "Polygone", "Impulsion" }, 2),
                P ("cx", "Centre X", "%", 0, 100, 50),
                P ("cy", "Centre Y", "%", 0, 100, 50),
                P ("size", "Taille", "%", 0, 100, 60),
                P ("rotate", "Rotation", "\xC2\xB0", 0, 360, 0),
                C ("speed", "Vitesse", speeds, 2),
                I ("ngon", "C\xC3\xB4t\xC3\xA9s", "", 3, 8, 4),
                P ("pulsew", "Largeur d'impulsion", "%", 5, 95, 50),
                P ("warpx", "D\xC3\xA9" "formation X", "%", -100, 100, 0),
                P ("warpy", "D\xC3\xA9" "formation Y", "%", -100, 100, 0),
                C ("retrig", "Repart \xC3\xA0 chaque note", onOff, 1),
                P ("subsize", "Sous-orbite taille", "%", 0, 100, 15),
                P ("subrate", "Sous-orbite vitesse", "Hz", 0.02f, 20, 0.7f, 0.4f),
                C ("subsync", "Sous-orbite synchro", lfoSyncChoices()),
                C ("orbenv", "Enveloppe de l'orbite", { "= ampli", "Propre" }, 1),
                P ("oa", "Orbite attaque", "ms", 0.5f, 10000, 800, 0.3f),
                P ("od", "Orbite d\xC3\xA9" "clin", "ms", 5, 10000, 1500, 0.3f),
                P ("os", "Orbite maintien", "%", 0, 100, 50),
                P ("or", "Orbite rel\xC3\xA2" "chement", "ms", 5, 10000, 1500, 0.3f),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 20, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 1200, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 80),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 1200, 0.3f),
                P ("vibdepth", "Vibrato", "ct", 0, 100, 0),
                P ("vibrate", "Vibrato vitesse", "Hz", 0.1f, 15, 5, 0.4f),
                P ("tremdepth", "Tr\xC3\xA9molo", "%", 0, 100, 0),
                P ("tremrate", "Tr\xC3\xA9molo vitesse", "Hz", 0.1f, 15, 4, 0.4f),
                P ("drive", "Saturation", "%", 0, 100, 0),
                P ("drivecol", "Couleur saturation", "Hz", 100, 8000, 1200, 0.3f),
                P ("chorus", "Chorus", "%", 0, 100, 25),
                P ("chorusrate", "Chorus vitesse", "Hz", 0.05f, 5, 0.4f, 0.4f),
                P ("chorusdepth", "Chorus profondeur", "ms", 0.5f, 8, 3),
            };
            for (const auto& p : rest) m.params.push_back (p);
            // ondes de la position W (bibliothèque du wave morph) : scie et carré par défaut
            for (int c = 0; c < 4; ++c)
                for (int g = 1; g <= 2; ++g)
                {
                    const std::string k (1, (char) ('a' + c));
                    m.params.push_back (C (k + "w" + std::to_string (g), std::string ("Coin ") + (char) ('A' + c) + " onde " + std::to_string (g), morph::waveNames(), g == 1 ? 2 : 3));
                }
            // phase de chaque générateur : décalage de la lecture de l'onde (modulable : modulation de phase)
            for (int c = 0; c < 4; ++c)
                for (int g = 1; g <= 2; ++g)
                {
                    const std::string k (1, (char) ('a' + c));
                    m.params.push_back (P (k + "ph" + std::to_string (g), std::string ("Coin ") + (char) ('A' + c) + " phase " + std::to_string (g), "\xC2\xB0", 0, 360, 0));
                }
            auto cornerIds = [] (char c)
            {
                const std::string k (1, c);
                return std::vector<std::string> { k + "g1", k + "g2", k + "ratio", k + "blend", k + "bmode", k + "ftype", k + "cut", k + "reso" };
            };
            m.sections = {
                { "global", "Global", "global", { "vmode", "voices", "stack", "stackdet", "glide", "tune", "drift", "level" }, "global" },
                { "cornera", "Coin A", "osc", cornerIds ('a'), "osc" },
                { "cornerb", "Coin B", "osc", cornerIds ('b'), "osc" },
                { "cornerc", "Coin C", "osc", cornerIds ('c'), "osc" },
                { "cornerd", "Coin D", "osc", cornerIds ('d'), "osc" },
                { "orbit", "Orbite", "mod", { "shape", "cx", "cy", "size", "rotate", "speed", "retrig", "ngon" }, "mod" },
                { "warp", "D\xC3\xA9" "formation et sous-orbite", "mod", { "warpx", "warpy", "pulsew", "subsize", "subrate", "subsync" }, "mod" },
                { "oenv", "Enveloppe de l'orbite", "env", { "orbenv", "oa", "od", "os", "or" }, "env" },
                { "filters", "Filtres", "filter", { "gres", "keytrack", "mixtype", "mixcut", "mixreso" }, "filter" },
                { "aenv", "Ampli", "env", { "aa", "ad", "as", "ar", "velamp", "scatter" }, "amp" },
                { "fx", "Effets", "fx", { "vibdepth", "vibrate", "tremdepth", "tremrate", "drive", "drivecol", "chorus", "chorusrate" }, "global" },
            };
            const std::string N = "Nappes", K = "Claviers", B = "Basses", L = "Leads", PL = "Plucks", FX = "Effets", A = "Arp\xC3\xA8ges";
            m.presets = {
                { "Init", { { "shape", 0 }, { "subsize", 0 }, { "chorus", 0 }, { "level", -10.3f } }, "Init" },
                { "Galaxie lente", { { "level", -4.3f }, { "shape", 2 }, { "speed", 0 }, { "size", 80 }, { "subsize", 20 }, { "subrate", 0.2f }, { "aa", 1500 }, { "ar", 3500 }, { "chorus", 40 }, { "scatter", 70 } }, N },
                { "Carr\xC3\xA9 tournant", { { "level", -8.7f }, { "shape", 5 }, { "ngon", 4 }, { "speed", 1 }, { "size", 90 }, { "aa", 800 }, { "ar", 2500 }, { "ablend", 40 }, { "dblend", -50 } }, N },
                { "Huit dans la nuit", { { "level", -5.3f }, { "shape", 3 }, { "speed", 1 }, { "size", 70 }, { "rotate", 45 }, { "aftype", 5 }, { "acut", 900 }, { "areso", 60 }, { "aa", 1200 }, { "ar", 3000 } }, N },
                { "Choeur d'harmoniques", { { "level", -14.6f }, { "ag1", 3 }, { "ag2", 2 }, { "ablend", -100 }, { "bg1", 4 }, { "bg2", 0 }, { "bblend", -70 }, { "bftype", 5 }, { "bcut", 2500 }, { "breso", 70 },
                                             { "shape", 2 }, { "speed", 1 }, { "aa", 900 }, { "ar", 2500 }, { "vibdepth", 6 } }, N },
                { "Enveloppe en voyage", { { "level", -14.3f }, { "shape", 1 }, { "size", 100 }, { "oa", 1500 }, { "od", 2500 }, { "os", 40 }, { "or", 2000 }, { "aa", 600 }, { "ar", 2500 } }, N },
                { "Cloches de phase", { { "level", -6.3f }, { "ablend", 60 }, { "abmode", 1 }, { "aratio", 3 }, { "bblend", 45 }, { "bbmode", 1 }, { "bratio", 4 }, { "shape", 6 }, { "speed", 4 }, { "pulsew", 30 },
                                        { "aa", 1 }, { "ad", 2500 }, { "as", 0 }, { "ar", 2500 } }, K },
                { "Piano de verre", { { "level", -8.4f }, { "ablend", 35 }, { "abmode", 2 }, { "aratio", 1 }, { "shape", 1 }, { "orbenv", 0 }, { "size", 70 }, { "aa", 2 }, { "ad", 1800 }, { "as", 10 }, { "ar", 900 } }, K },
                { "Basse en pile", { { "level", -10.2f }, { "vmode", 1 }, { "stack", 3 }, { "stackdet", 10 }, { "tune", -12 }, { "ag1", 3 }, { "ag2", 2 }, { "ablend", -100 }, { "aftype", 2 }, { "acut", 500 },
                                     { "shape", 0 }, { "cx", 10 }, { "cy", 10 }, { "subsize", 0 }, { "chorus", 0 }, { "ad", 600 }, { "as", 60 } }, B },
                { "Basse PM", { { "level", -0.3f }, { "vmode", 1 }, { "stack", 1 }, { "tune", -12 }, { "ablend", 70 }, { "abmode", 1 }, { "aratio", 1 }, { "shape", 1 }, { "orbenv", 1 }, { "oa", 1 }, { "od", 300 },
                                { "os", 0 }, { "size", 60 }, { "chorus", 0 }, { "ad", 500 }, { "as", 50 } }, B },
                { "Lead orbital", { { "level", -13.1f }, { "vmode", 1 }, { "stack", 5 }, { "stackdet", 15 }, { "glide", 60 }, { "shape", 2 }, { "speed", 4 }, { "size", 30 }, { "vibdepth", 15 }, { "vibrate", 5.5f },
                                    { "drive", 30 } }, L },
                { "Pluck formant", { { "level", -4 }, { "aftype", 5 }, { "acut", 600 }, { "areso", 80 }, { "shape", 1 }, { "orbenv", 1 }, { "oa", 1 }, { "od", 250 }, { "os", 0 }, { "ad", 400 }, { "as", 0 }, { "ar", 300 } }, PL },
                { "Impulsions rythm\xC3\xA9" "es", { { "shape", 6 }, { "speed", 6 }, { "pulsew", 25 }, { "size", 100 }, { "retrig", 1 }, { "as", 90 }, { "ar", 300 } }, A },
                { "Bruit d'\xC3\xA9toiles", { { "level", 1.8f }, { "ag1", 6 }, { "bg1", 6 }, { "cg1", 6 }, { "dg1", 6 }, { "ablend", 0 }, { "bblend", 0 }, { "cblend", 0 }, { "dblend", 0 }, { "shape", 3 }, { "speed", 0 },
                                             { "aa", 2000 }, { "ar", 4000 }, { "chorus", 60 } }, FX },
                { "Tr\xC3\xA9molo spatial", { { "level", -4.1f }, { "tremdepth", 60 }, { "tremrate", 3 }, { "shape", 4 }, { "speed", 2 }, { "aa", 500 }, { "ar", 2000 }, { "scatter", 100 } }, FX },
                // v0.20 : coins pris dans la bibliothèque d'ondes (position W, onde de la bibliothèque du wave morph)
                { "Ch\xC5\x93ur en orbite", { { "level", -4.3f }, { "ag1", 7 }, { "aw1", 6 }, { "bg1", 7 }, { "bw1", 7 }, { "cg1", 7 }, { "cw1", 32 }, { "dg1", 7 }, { "dw1", 35 }, { "ablend", 0 }, { "bblend", 0 }, { "cblend", 0 }, { "dblend", 0 },
                                              { "aftype", 0 }, { "bftype", 0 }, { "cftype", 0 }, { "dftype", 1 }, { "shape", 2 }, { "speed", 0 }, { "size", 85 }, { "chorus", 50 },
                                              { "aa", 1200 }, { "ar", 3000 }, { "scatter", 70 } }, N },
                { "Cloches et m\xC3\xA9tal", { { "level", -7 }, { "ag1", 7 }, { "aw1", 8 }, { "bg1", 7 }, { "bw1", 11 }, { "cg1", 7 }, { "cw1", 42 }, { "dg1", 7 }, { "dw1", 47 }, { "ablend", 0 }, { "bblend", 0 }, { "cblend", 0 }, { "dblend", 0 },
                                             { "shape", 1 }, { "orbenv", 1 }, { "oa", 1 }, { "od", 1200 }, { "os", 0 }, { "or", 1500 }, { "size", 100 },
                                             { "aa", 1 }, { "ad", 2500 }, { "as", 0 }, { "ar", 2000 } }, K },
                { "Orgue vectoriel", { { "level", -12.9f }, { "ag1", 7 }, { "aw1", 27 }, { "bg1", 7 }, { "bw1", 28 }, { "cg1", 7 }, { "cw1", 30 }, { "dg1", 7 }, { "dw1", 31 }, { "ablend", 0 }, { "bblend", 0 }, { "cblend", 0 }, { "dblend", 0 },
                                       { "aftype", 0 }, { "bftype", 0 }, { "cftype", 0 }, { "dftype", 0 }, { "shape", 0 }, { "cx", 30 }, { "cy", 30 }, { "subsize", 10 },
                                       { "vibdepth", 8 }, { "aa", 5 }, { "as", 100 }, { "ar", 120 }, { "chorus", 35 } }, K },
                { "Scies en polygone", { { "level", -9.9f }, { "ag1", 7 }, { "aw1", 2 }, { "bg1", 7 }, { "bw1", 18 }, { "cg1", 7 }, { "cw1", 61 }, { "dg1", 7 }, { "dw1", 52 }, { "ablend", 0 }, { "bblend", 0 }, { "cblend", 0 }, { "dblend", 0 },
                                         { "aftype", 2 }, { "acut", 2500 }, { "bftype", 2 }, { "bcut", 4000 }, { "cftype", 1 }, { "ccut", 3000 }, { "dftype", 2 }, { "dcut", 1800 },
                                         { "shape", 5 }, { "ngon", 5 }, { "speed", 2 }, { "size", 90 }, { "aa", 400 }, { "ar", 1800 }, { "chorus", 40 } }, N },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 0; hub.slots = 8; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            // formes affichées par le choix des ondes (motifs, bruit, bibliothèque)
            auto add = [&m] (const WaveBank& wb, int k)
            {
                std::vector<float> shape (64);
                for (int i = 0; i < 64; ++i) shape[(size_t) i] = wb.read (k, 4, (double) i / 64.0);
                float pk = 1.0e-6f;
                for (auto v : shape) pk = std::max (pk, std::abs (v));
                for (auto& v : shape) v = std::round (v / pk * 1000.0f) / 1000.0f;
                m.waveShapes.push_back (std::move (shape));
            };
            for (int k = 0; k < numHarmonic; ++k) add (bank(), k);
            {
                std::vector<float> noise (64);
                std::uint32_t r = 12345u;
                for (auto& v : noise) { r = r * 1664525u + 1013904223u; v = std::round (((float) (r >> 8) / 8388608.0f - 1.0f) * 1000.0f) / 1000.0f; }
                m.waveShapes.push_back (std::move (noise));
            }
            for (int k = 0; k < morph::bank().size(); ++k) add (morph::bank(), k);
            addFactoryBank (m); // v0.47 (maj. 5 nov.) : banque d'usine (au moins 90 sons par synthé)
            return m;
        }();
        return d;
    }
}

/** Horloge commune de l'instrument (orbites non relancées à chaque note). */
struct OrbitClock { double t = 0.0, bpm = 120.0; };

class OrbitVoice
{
public:
    void prepare (double s) { sr = s; amp.prepare (s); oenv.prepare (s); reset(); }
    void reset() noexcept
    {
        for (auto& c : corners) c = {};
        for (auto& f : cf) { f.a.reset(); f.b.reset(); }
        mixF.a.reset(); mixF.b.reset(); amp.reset(); oenv.reset(); dc.reset(); active = false; age = 0.0;
    }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }
    void seed (std::uint32_t s) noexcept { rng = s; }
    void setClock (const OrbitClock* c) noexcept { clock = c; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true;
        rng = rng * 1664525u + 1013904223u;
        pan = (double) (rng >> 8) / 8388608.0 - 1.0;
        rng = rng * 1664525u + 1013904223u;
        driftSeed = (double) (rng >> 8) / 8388608.0;
    }
    void release() noexcept { amp.gate (false); oenv.gate (false); }
    bool isActive() const noexcept { return active; }
    float blueX = 0.5f, blueY = 0.5f, greenX = 0.5f, greenY = 0.5f; // pour l'écran (dernier bloc)

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace orbit;
        const auto& wb = bank();
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! amp.isActive())
            {
                // départ depuis le silence : chaque générateur repart de sa phase de départ (attaque identique)
                if (! amp.isActive()) for (auto& cr : corners) { cr.ph1 = 0.0; cr.ph2 = 0.0; }
                amp.gate (true); oenv.gate (true);
                if (p[retrig] > 0.5f) { age = 0.0; subPhase = 0.0; }
            }
        }
        amp.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        oenv.set (p[oA], p[oD], p[oS] / 100.0, p[oR]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level] + 6.0f) * velocityGain (vel, p[velAmp] / 100.0);
        const auto pn = spread != 0.0 ? spread : pan * p[scatter] / 100.0;
        const auto gl = 1.0 - std::max (0.0, pn) * 0.7, gr = 1.0 + std::min (0.0, pn) * 0.7;
        const auto det = spread * p[stackDetune] / 100.0;
        const auto gRes = p[globalReso] / 100.0;
        const auto kt = p[keytrack] / 100.0;
        const bool useOwnEnv = p[orbEnv] > 0.5f;
        const auto sus = p[oS] / 100.0, ampSus = p[aS] / 100.0;
        const auto mixT = (int) p[mixType];
        const auto mixRes = std::min (p[mixReso] / 100.0, gRes);
        // tempo de l'orbite : une période de trajectoire, en secondes
        const auto bpm = clock != nullptr ? clock->bpm : 120.0;
        const auto period = speedBeats ((int) p[speed]) * 60.0 / std::max (20.0, bpm);
        const bool retrigOn = p[retrig] > 0.5f;
        const auto shp = (int) p[shape];
        static constexpr double ratioOf[6] { 1.0, 2.0, 3.0, 4.0, 5.0, 0.5 };
        struct CornerSet { int g1, g2, mode, ft; double ratio, blend, cut, res; int w1, w2; double po1, po2; };
        std::array<CornerSet, 4> cs {};
        for (int c = 0; c < 4; ++c)
        {
            const auto* q = p + cornerBase + c * cornerStride;
            cs[(size_t) c] = { std::clamp ((int) q[0], 0, wavePattern), std::clamp ((int) q[1], 0, wavePattern), (int) q[4], (int) q[5], ratioOf[std::clamp ((int) q[2], 0, 5)],
                               q[3] / 100.0, (double) q[6], std::min (q[7] / 100.0, gRes), (int) p[waveBase + c * 2], (int) p[waveBase + c * 2 + 1],
                               p[phaseBase + c * 2] / 360.0, p[phaseBase + c * 2 + 1] / 360.0 };
        }
        double bx = 0.5, by = 0.5, gx = 0.5, gy = 0.5;
        double dt = 0.01, noteKt = 1.0;
        WaveBank::Mip lv;
        std::array<WaveBank::Mip, 4> lv2 {};

        for (int i = 0; i < n; ++i)
        {
            const auto ae = amp.next();
            const auto oe = oenv.next();
            age += 1.0 / sr;
            // trajectoire : toutes les 16 échantillons (mouvement lent), sinon la position du dernier calcul
            if ((i & 15) == 0)
            {
                const auto t = retrigOn || clock == nullptr ? age : clock->t + i / sr;
                const auto ph = t / std::max (1.0e-3, period);
                trajectory (p, shp, ph - std::floor (ph), useOwnEnv ? oe : ae, useOwnEnv ? (int) oenv.getStage() : (int) amp.getStage(), useOwnEnv ? sus : ampSus, bx, by);
                subPhase += 16.0 * p[subRate] / sr;
                subPhase -= std::floor (subPhase);
                const auto r = p[subSize] / 100.0 * 0.5;
                gx = std::clamp (bx + r * fastSin2Pi (subPhase + 0.25), 0.0, 1.0);
                gy = std::clamp (by + r * fastSin2Pi (subPhase), 0.0, 1.0);
                weights = { (1.0 - gx) * (1.0 - gy), gx * (1.0 - gy), (1.0 - gx) * gy, gx * gy };
            }
            const auto glided = pitch.next (glideCoef);
            // hauteur, tables et coefficients des filtres : tous les 16 échantillons (0,3 ms),
            // largement assez pour le vibrato et le portamento, et bien moins de calcul
            if ((i & 15) == 0)
            {
                const auto clockT = clock != nullptr ? clock->t + i / sr : age;
                const auto vib = p[vibDepth] / 100.0 * fastSin2Pi (clockT * p[vibRate]);
                const auto dr = p[drift] / 100.0 * 0.1 * fastSin2Pi (clockT * (0.05 + 0.1 * driftSeed) + driftSeed);
                const auto note = glided + p[tune] + det + vib + dr;
                dt = std::clamp (noteHz (note) / sr, 1.0e-7, 0.45);
                lv = WaveBank::mipFor (dt);
                noteKt = fastExp2 (kt * (note - 60.0) / 12.0);
                for (int c = 0; c < 4; ++c)
                {
                    const auto& k = cs[(size_t) c];
                    lv2[(size_t) c] = WaveBank::mipFor (std::min (0.45, dt * k.ratio));
                    if (weights[(size_t) c] < 1.0e-4) continue;
                    setCoefs (cf[(size_t) c], k.ft, k.cut * noteKt, k.res);
                    if (k.g1 == orbit::noisePattern) corners[(size_t) c].hpC.set (std::clamp (dt * sr, 20.0, 0.45 * sr), 0.75, sr);
                }
                if (mixT > 0) setCoefs (mixF, mixT, (double) p[mixCut] * noteKt, mixRes);
            }

            double mix = 0.0;
            for (int c = 0; c < 4; ++c)
            {
                const auto w = weights[(size_t) c];
                auto& st = corners[(size_t) c];
                const auto& k = cs[(size_t) c];
                const auto dt2 = std::min (0.45, dt * k.ratio);
                // les phases avancent toujours (un coin qui revient n'a pas de clic)
                st.ph1 += dt; st.ph1 -= std::floor (st.ph1);
                st.ph2 += dt2; st.ph2 -= std::floor (st.ph2);
                if (w < 1.0e-4) continue;
                const auto rp1 = st.ph1 + k.po1, rp2 = st.ph2 + k.po2; // phases lues (décalage de départ)
                const auto g2 = gen (wb, k.g2, k.w2, rp2, lv2[(size_t) c], st, false);
                double y;
                const auto b = k.blend;
                if (b <= 0.0) y = gen (wb, k.g1, k.w1, rp1, lv, st, true) + (-b) * g2;
                else
                {
                    switch (k.mode)
                    {
                        case 1: y = gen (wb, k.g1, k.w1, rp1 + b * 0.45 * g2, lv, st, true); break;                                      // PM
                        case 2: { const auto pm = gen (wb, k.g1, k.w1, rp1 + b * 0.45 * g2, lv, st, true); y = pm * (1.0 - 0.5 * b) + b * pm * g2; break; } // PM + AM
                        case 3: case 4:                                                                                                        // PD + AM
                        {
                            const auto e = 1.0 + 4.0 * b;
                            const auto x2 = rp2 - std::floor (rp2);
                            const auto pd = k.mode == 3 ? std::pow (x2, e) : 1.0 - std::pow (1.0 - x2, e);
                            const auto g2d = k.g2 != orbit::noisePattern ? readWave (wb, k.g2, k.w2, lv2[(size_t) c], pd) : g2;
                            const auto g1 = gen (wb, k.g1, k.w1, rp1, lv, st, true);
                            y = g1 * (1.0 - b) + b * g1 * g2d * 2.0;
                            break;
                        }
                        default: { const auto g1 = gen (wb, k.g1, k.w1, rp1, lv, st, true); y = g1 * (1.0 - b) + b * g1 * g2 * 2.0; break; } // AM
                    }
                }
                mix += w * filt (cf[(size_t) c], y, k.ft);
            }
            if (mixT > 0) mix = filt (mixF, mix, mixT);
            const auto out = dc.process (mix) * ae * gain * 0.5;
            L[i] += (float) (out * gl);
            R[i] += (float) (out * gr);
        }
        blueX = (float) bx; blueY = (float) by; greenX = (float) gx; greenY = (float) gy;
        if (! amp.isActive()) active = false;
    }

private:
    struct Corner { double ph1 = 0.0, ph2 = 0.0, pink = 0.0; StateFilter hp; SvfCoefs hpC; };
    /** Filtre d'un coin (ou du mélange) : deux étages et leurs coefficients, calculés hors de la boucle. */
    struct CornerFilters { StateFilter a, b; SvfCoefs ca, cb; };

    /** Générateur : motif d'harmoniques (table) ou bruit rose passé dans un passe-haut accordé. */
    /** Onde d'un générateur (motif, ou onde choisie de la bibliothèque en position W), hors bruit. */
    static double readWave (const WaveBank& wb, int pat, int wave, WaveBank::Mip lv, double phase) noexcept
    {
        phase -= std::floor (phase);
        if (pat < orbit::numHarmonic) return wb.read (pat, lv, phase);
        const auto& lib = morph::bank();
        return lib.read (std::clamp (wave, 0, lib.size() - 1), lv, phase);
    }

    double gen (const WaveBank& wb, int pat, int wave, double phase, WaveBank::Mip lv, Corner& st, bool first) noexcept
    {
        if (pat != orbit::noisePattern) return readWave (wb, pat, wave, lv, phase);
        rng = rng * 1664525u + 1013904223u;
        const auto white = (double) (rng >> 8) / 8388608.0 - 1.0;
        st.pink += (white - st.pink) * 0.12;
        if (! first) return st.pink * 6.0;
        return st.hp.process (st.pink * 8.0, st.hpC).high;
    }

    void setCoefs (CornerFilters& f, int type, double fc, double res) noexcept
    {
        fc = std::clamp (fc, 20.0, 0.45 * sr);
        switch (type)
        {
            case 2: f.ca.set (fc, res * 0.8, sr); break;
            case 5: // formant : deux bandes sur la voyelle choisie par la coupure (U O I E A)
            {
                static constexpr double F1[5] { 325, 450, 300, 400, 800 }, F2[5] { 700, 800, 2300, 1600, 1150 };
                const auto pos = std::clamp (std::log2 (std::max (60.0, fc) / 60.0) / std::log2 (18000.0 / 60.0) * 4.0, 0.0, 4.0);
                const auto k = std::min (3, (int) pos);
                const auto fr = pos - (double) k;
                const auto q = 0.5 + 0.45 * res;
                f.ca.set (std::min (F1[k] + (F1[k + 1] - F1[k]) * fr, 0.45 * sr), q, sr);
                f.cb.set (std::min (F2[k] + (F2[k + 1] - F2[k]) * fr, 0.45 * sr), q, sr);
                break;
            }
            default: f.ca.set (fc, res, sr); break;
        }
    }

    static double filt (CornerFilters& f, double x, int type) noexcept
    {
        switch (type)
        {
            case 1: return f.a.process (x, f.ca).low;
            case 2: return f.b.process (f.a.process (x, f.ca).low, f.ca).low;
            case 3: return 1.3 * f.a.process (x, f.ca).band;
            case 4: return f.a.process (x, f.ca).high;
            case 5: return 1.8 * (f.a.process (x, f.ca).band + 0.7 * f.b.process (x, f.cb).band);
            default: return x;
        }
    }

    /** Position de l'orbe bleu (0..1 × 0..1) sur sa trajectoire. */
    static void trajectory (const float* p, int shape, double ph, double env, int stage, double sus, double& x, double& y) noexcept
    {
        using namespace orbit;
        double u = 0.5, v = 0.5; // dans le carré unité, avant taille, rotation et déformation
        switch (shape)
        {
            case 1: // enveloppe : attaque A → B, déclin B → D, relâchement vers C
            {
                if (stage <= 1) { u = env; v = 0.0; }
                else if (stage == 2 || stage == 3) { u = 1.0; v = std::clamp ((1.0 - env) / std::max (0.02, 1.0 - sus), 0.0, 1.0); }
                else { const auto r = std::clamp (1.0 - env / std::max (0.02, sus), 0.0, 1.0); u = 1.0 - r; v = 1.0; }
                break;
            }
            case 2: u = 0.5 + 0.5 * fastSin2Pi (ph); v = 0.5 + 0.5 * fastSin2Pi (ph + 0.25); break;               // cercle
            case 3: u = 0.5 + 0.5 * fastSin2Pi (ph); v = 0.5 + 0.5 * fastSin2Pi (2.0 * ph + 0.25); break;         // huit
            case 4: u = 0.5 + 0.5 * fastSin2Pi (3.0 * ph); v = 0.5 + 0.5 * fastSin2Pi (2.0 * ph + 0.25); break;   // 3 : 2
            case 5: // polygone parcouru côté par côté
            {
                const int nS = std::clamp ((int) std::lround (p[ngon]), 3, 8);
                const auto pos = ph * nS;
                const auto k = (int) pos;
                const auto f = pos - (double) k;
                const auto a0 = (double) k / nS, a1 = (double) (k + 1) / nS;
                const auto x0 = 0.5 + 0.5 * fastSin2Pi (a0 + 0.25), y0 = 0.5 + 0.5 * fastSin2Pi (a0);
                const auto x1 = 0.5 + 0.5 * fastSin2Pi (a1 + 0.25), y1 = 0.5 + 0.5 * fastSin2Pi (a1);
                u = x0 + (x1 - x0) * f; v = y0 + (y1 - y0) * f;
                break;
            }
            case 6: u = ph < p[pulseW] / 100.0 ? 0.0 : 1.0; v = 0.5; break;                                        // impulsion
            default: break;                                                                                         // manuelle : le centre
        }
        // taille et rotation autour du centre, puis déformation
        const auto s = shape == 0 ? 0.0 : p[size] / 100.0;
        auto dx = (u - 0.5) * s, dy = (v - 0.5) * s;
        const auto rot = p[rotate] / 360.0;
        const auto c = fastSin2Pi (rot + 0.25), sn = fastSin2Pi (rot);
        const auto rx = dx * c - dy * sn, ry = dx * sn + dy * c;
        x = std::clamp (p[cx] / 100.0 + rx, 0.0, 1.0);
        y = std::clamp (p[cy] / 100.0 + ry, 0.0, 1.0);
        auto warp = [] (double z, double w) { return w == 0.0 ? z : std::pow (z, std::pow (4.0, -w)); };
        x = warp (x, p[warpX] / 100.0);
        y = warp (y, p[warpY] / 100.0);
    }

    double sr = 48000.0, pan = 0.0, spread = 0.0, age = 0.0, subPhase = 0.0, driftSeed = 0.0;
    std::array<double, 4> weights { 0.25, 0.25, 0.25, 0.25 };
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false;
    std::uint32_t rng = 23;
    std::array<Corner, 4> corners;
    std::array<CornerFilters, 4> cf;
    CornerFilters mixF;
    Adsr amp, oenv;
    Glide pitch;
    DcBlocker dc;
    const OrbitClock* clock = nullptr;
};

class OrbitSynth final : public SynthBase<OrbitVoice, 8>
{
public:
    OrbitSynth()
    {
        (void) orbit::bank(); // motifs calculés à la création (fil de l'interface)
        for (int i = 0; i < 8; ++i) { voices[(size_t) i].seed (0x0B17u * (std::uint32_t) (i + 5)); voices[(size_t) i].setClock (&clk); }
    }
    const ModuleDesc& desc() const noexcept override { return orbit::desc(); }

    void prepare (double s, int maxBlock) override
    {
        SynthBase::prepare (s, maxBlock);
        satL.reset(); satR.reset();
        chorusL.assign ((size_t) (0.05 * s) + 8, 0.0f);
        chorusR.assign ((size_t) (0.05 * s) + 8, 0.0f);
    }

    /** Écran de l'orbite : orbes bleu et vert de la dernière voix jouée. */
    int status (float* out, int max) const noexcept override
    {
        if (max < 5) return 0;
        out[0] = shown[0].load (std::memory_order_relaxed); out[1] = shown[1].load (std::memory_order_relaxed);
        out[2] = shown[2].load (std::memory_order_relaxed); out[3] = shown[3].load (std::memory_order_relaxed);
        out[4] = sounding.load (std::memory_order_relaxed) ? 1.0f : 0.0f; // une voix joue : les orbes bougent
        return 5;
    }

protected:
    const float* adjust (const float* p) noexcept override
    {
        clk.bpm = bpm;
        return syncRates (p, orbit::numSlots, { { orbit::subRate, orbit::subSync } });
    }
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[orbit::voices], 1, 8);
        if ((int) p[orbit::vMode] == 1) setMode (true, true, v, std::clamp ((int) p[orbit::stack], 1, 8));
        else setMode (v == 1, true, v);
    }
    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace orbit;
        // orbes affichés : la première voix active
        bool any = false;
        for (const auto& v : voices)
            if (v.isActive()) { shown[0].store (v.blueX); shown[1].store (v.blueY); shown[2].store (v.greenX); shown[3].store (v.greenY); any = true; break; }
        sounding.store (any, std::memory_order_relaxed);
        const auto drv = p[driveAmt] / 100.0;
        const auto trem = p[tremDepth] / 100.0;
        const auto ch = p[chorusMix] / 100.0;
        const auto g = 1.0 + drv * 12.0;
        const auto size = (int) chorusL.size();
        for (int i = 0; i < n; ++i)
        {
            const auto t = clk.t + i / sr;
            double l = L[i], r = R[i];
            if (drv > 0.0)
            {
                // saturation : bande en parallèle d'un saturateur
                const auto bl = bpL.process (l, p[driveCol], 0.4, sr).band, br = bpR.process (r, p[driveCol], 0.4, sr).band;
                // v0.22 : saturateur anti-replié (ADAA)
                l = l * (1.0 - drv) + drv * (satL.process (l * g) / std::sqrt (g) * 1.6 + bl * 0.8);
                r = r * (1.0 - drv) + drv * (satR.process (r * g) / std::sqrt (g) * 1.6 + br * 0.8);
            }
            else { satL.reset(); satR.reset(); }
            if (trem > 0.0) { const auto a = 1.0 - trem * (0.5 + 0.5 * fastSin2Pi (t * p[tremRate])); l *= a; r *= a; }
            if (ch > 0.0 && size > 8)
            {
                chorusL[(size_t) cw] = (float) l; chorusR[(size_t) cw] = (float) r;
                const auto lfo = fastSin2Pi (t * p[chorusRate]);
                const auto dl = (2.0 + p[chorusDepth] * (1.0 + lfo) * 0.5) * 0.001 * sr, drr = (2.0 + p[chorusDepth] * (1.0 - lfo) * 0.5) * 0.001 * sr;
                auto tap = [&] (const std::vector<float>& b, double d)
                {
                    auto rp = (double) cw - d; while (rp < 0.0) rp += size;
                    const auto i0 = (int) rp; const auto f = rp - i0;
                    return (double) b[(size_t) i0] * (1.0 - f) + (double) b[(size_t) ((i0 + 1) % size)] * f;
                };
                const auto wl = tap (chorusL, dl), wr = tap (chorusR, drr);
                cw = (cw + 1) % size;
                l = l * (1.0 - 0.5 * ch) + wr * 0.7 * ch;
                r = r * (1.0 - 0.5 * ch) + wl * 0.7 * ch;
            }
            L[i] = (float) l; R[i] = (float) r;
        }
        clk.t += n / sr;
        if (clk.t > 1.0e6) clk.t = 0.0;
    }

private:
    OrbitClock clk;
    std::array<std::atomic<float>, 4> shown { 0.5f, 0.5f, 0.5f, 0.5f };
    std::atomic<bool> sounding { false };
    StateFilter bpL, bpR;
    AdaaTanh satL, satR;
    std::vector<float> chorusL, chorusR;
    int cw = 0;
};

} // namespace grv::instruments
