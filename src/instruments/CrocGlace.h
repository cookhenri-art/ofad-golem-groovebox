/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Croc Glacé » — synthé psy à tables d'ondes. Il reprend le principe des synthés psychédéliques
    « soustractif + FM + distorsion créative + séquenceur de LFO + ORB à quatre sons » (architecture
    reprise, ni nom, ni façade, ni table, ni son d'origine : tout est calculé par le programme), et y
    mêle deux instruments GOLEM : la sous-orbite de l'Orbiteur et la physique de Glutamake.
      - PORTEUSE et MODULATEUR : oscillateurs à tables d'ondes (9 tables calculées de 8 ondes, plus une
        table perso importée d'un WAV : emplacement 1 = porteuse, emplacement 2 = modulateur), position,
        octave, demi-ton, fin ; PWM par distorsion de phase sur la porteuse ; onde B, Crush, Shape, Fold
        sur le modulateur ; mélange porteuse ↔ modulateur, sous-oscillateur ;
      - « BITE » : FM (le modulateur pousse la phase de la porteuse), Fold, Crush, Soft ;
      - FILTRE : passe-bas 24 / 12, passe-bande, passe-haut, coupe-bande, peigne + / −, formant ; deux
        étages, drive, enveloppe ADSR, suivi clavier, vélocité, LFO du filtre au tempo ;
      - SÉQUENCEUR DE LFO : 8 LFO (formes dont hasard et chaos, tempo, marches, phase, départ à la note) ;
        4 couloirs de 32 pas : chaque pas joue l'un des 8 LFO à son niveau ; par couloir : longueur,
        division, sens (avant, arrière, aller-retour, hasard), mode (grille du morceau, à chaque note,
        libre), swing, modificateurs de sortie (lissage, quantification, courbe, pli, gigue) et une
        destination directe ; les couloirs sont aussi des sources de la matrice ;
      - L'ORB : quatre coins, chacun un son (son d'usine chargé, ou son en cours capturé ; un coin vide
        joue le son en cours). Le point X / Y fait muter le son de l'un à l'autre (réglages continus
        fondus, réglages à crans pris au coin le plus proche). Le point se tient au doigt ou suit une
        orbite qui tourne autour de lui (formes, attracteurs de Lorenz et de Rössler, chaos ; sens avant,
        arrière, aller-retour ; tempo ou libre jusqu'à 80 Hz ; taille, rotation, chaos) ; ajout GOLEM :
        une sous-orbite en sens opposé (principe de l'Orbiteur), un orbe 3 qui tourne à l'envers autour
        du point, et une physique (principe de Glutamake) : rigide, élastique (le point suit l'orbite par
        un ressort) ou gravité (l'orbe 3 est attiré par le point et rebondit sur les bords). ORB X / Y et
        l'orbe 3 sont des sources : six routes directes, calculées dans la voix tous les 16 échantillons
        (modulations rapides sans escalier : sons qui tournent, « squelch » de la psytrance), et sources de
        la matrice ;
      - matrice de modulation (16 routages), arpégiateur, poly / mono / legato / unisson, glissé ;
      - effets internes : flanger, écho, réverbération (ceux du Groovebox), réglables jusqu'à l'excès
        (écho de 1 ms qui s'auto-entretient, flanger à forte rétroaction, réverbération peu diffuse) pour
        les sons métalliques et les effets spéciaux.

    Choix de modèle (latence nulle, rien d'alloué dans le rendu) : tables limitées en bande par octave
    (WaveBank), FM par la phase avec une table moins riche quand l'indice monte ; filtres TPT existants
    (échelle, état) et peigne à ligne fractionnaire ; commandes (hauteur, coupure, routes, couloirs) tous
    les 16 échantillons (0,3 ms), gain et panoramique interpolés à l'échantillon ; LFO et couloirs
    calculés pour tout l'instrument ; morph une fois par bloc (64 échantillons au plus).
    Volontairement écarté de cette version : suréchantillonnage, variations A à D des couloirs, outils de
    génération (hasard, euclidien, transformations : à faire dans l'interface), orbite dessinée.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "../synth/Wavetable.h"
#include "FactoryBanks.h"
#include "../dsp/Common.h"

// les effets internes de Good Doctor 2 (enveloppe des effets du Groovebox) lisent les outils de grv::dsp
namespace grv::instruments::gd2 { using namespace grv::dsp; }
#include "gd2/Gd2Fx.h"
#include "gd2/Gd2Sources.h" // tables calculées de Glutamake, Good Doctor et Good Doctor 2
#include "../effects/ModFx36.h"
#include "../effects/Delay.h"
#include "../effects/Reverb.h"

#include <atomic>
#include <limits>
#include <memory>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace croc
{
    // tables : 0-10 calculées ici (8 ondes), 11 bruit, 12-27 tables de Glutamake, Good Doctor et Good Doctor 2
    // (12 ondes, lues dans leurs banques), 28 perso (WAV)
    constexpr int numTables = 11, frames = 8, noiseTable = numTables, golemFirst = noiseTable + 1, golemTables = 16,
                  userTable = golemFirst + golemTables;
    constexpr int numLfos = 8, numLanes = 4, laneSteps = 32, numRoutes = 6, numOrbOuts = 8, numCorners = 4;
    constexpr int lfoStride = 7, laneHead = 15, laneStride = laneHead + laneSteps * 2, routeStride = 3;

    enum Slot
    {
        vMode = 0, voices, uniVoices, uniDetune, glide, tune, level, velAmp, spreadAmt,
        cTab, cPos, cOct, cSemi, cFine, cPwm, cPwmRate, cPd,
        mTabA, mTabB, mPos, mAb, mOct, mSemi, mFine, mCrush, mShape, mFold, mPd,
        oscMix, subLvl, noiseLvl, noiseCol, biteFm, biteFold, biteCrush, biteSoft,
        modA, modD, modS, modR, modFm, modPitch,
        fType, cutoff, reso, f2Stage, fDrive, fEnv, fKey, fVel, flSync, flRate, flAmt, flShape,
        aA, aD, aS, aR, fA, fD, fS, fR,
        distMode, distDrive, distMix,
        flgTime, flgRate, flgDepth, flgFb, flgMix,
        dlMode, dlSync, dlTime, dlDiv, dlFb, dlGlide, dlTone, dlMix,
        rvSize, rvWidth, rvDamp, rvDiff, rvMix,
        lfoBase,                                   // 8 × : forme, synchro, vitesse, quantité, marches, départ, phase
        laneBase = lfoBase + numLfos * lfoStride,  // 4 × : en-tête (13), puis 32 pas × (LFO, niveau)
        orbOn = laneBase + numLanes * laneStride, o1Shape, o1Dir, o1Sync, o1Rate, o1Size, o1Rot, o1Chaos,
        o2Shape, o2Size, o2Ratio, o2Dir, o2Phase,
        o3Shape, o3Size, o3Ratio, o3Dir, o3Phase,
        orbPhys, orbSpring, orbDamp, orbBounce, orbRetrig,
        routeBase,                                 // 6 × : source, destination, quantité
        morphOn = routeBase + numRoutes * routeStride, morphAmt, morphX, morphY,
        numSlots
        // puis la modulation commune (matrice, arpège), puis les 4 coins de l'ORB (voir cornerStride())
    };
    enum LfoField { lShape = 0, lSync, lRate, lAmt, lSteps, lRetrig, lPhase };
    enum LaneField { lnOn = 0, lnLen, lnDiv, lnDir, lnMode, lnSwing, lnSmooth, lnQuant, lnCurve, lnFold, lnJitter, lnDst, lnAmt, lnSrc, lnMotif };

    /** Destinations directes (couloirs et routes de l'ORB) et leur échelle à 100 %. */
    enum Dest { dNone = 0, dPitch, dPitchC, dPitchM, dCut, dRes, dFm, dFold, dCrush, dPosC, dPosM, dMix, dVol, dPan, dPwm, dDrive, dMorphX, dMorphY,
                dDecF, dDecA, dNoise, dPdC, dPdM, dShapeM, dDist, dFlgDepth, dFlgFb, dDlyTime, dDlyFb, dDlyMix, dRvSize,
                // 27 nov. 2026 : destinations d'effets ajoutées à la fin (lasers, métaux qui bougent)
                dFlgTime, dRvDiff, dDlyTone, dPhFreq, dPhFb, dStWidth, dCombPitch, numDests };
    /** Destinations qui agissent sur les effets (communs à toutes les voix). */
    inline bool fxDest (int d) noexcept { return (d >= dDist && d <= dRvSize) || (d >= dFlgTime && d < numDests); }
    /** Formes de l'orbite : 0 à 8 suivent une phase ; Lorenz, Rössler et Chaos ne se répètent jamais. */
    enum OrbShape { osCircle = 0, osEllipse, osEight, osLissajous, osStar, osSpiral, osRose, osTriangle, osSquare, osLorenz, osRossler, osChaos, numOrbShapes };

    /** Réglages du son que l'ORB mélange entre ses 4 coins (les modulations et l'ORB lui-même n'en font pas
        partie : le mouvement ne se mélange pas avec lui-même). Les réglages à crans (tables, octaves, type de
        filtre…) prennent la valeur du coin le plus proche du point. */
    inline const std::vector<int>& morphSlots()
    {
        static const std::vector<int> v { cTab, cPos, cOct, cSemi, cFine, cPwm, cPwmRate, cPd, mTabA, mTabB, mPos, mAb, mOct, mSemi, mFine, mCrush, mShape, mFold, mPd,
                                          oscMix, subLvl, noiseLvl, noiseCol, biteFm, biteFold, biteCrush, biteSoft, modA, modD, modS, modR, modFm, modPitch,
                                          fType, cutoff, reso, f2Stage, fDrive, fEnv, fKey, fVel, flAmt, flShape,
                                          aA, aD, aS, aR, fA, fD, fS, fR, level, glide, uniDetune, distMode, distDrive, distMix,
                                          flgTime, flgRate, flgDepth, flgFb, flgMix, dlMode, dlSync, dlTime, dlDiv, dlFb, dlGlide, dlTone, dlMix,
                                          rvSize, rvWidth, rvDamp, rvDiff, rvMix };
        return v;
    }
    /** Disposition d'un coin : chargé (Non / Oui), son d'origine (n° du préréglage d'usine + 1, 0 : son capturé),
        puis les réglages de morphSlots(), dans cet ordre. */
    inline int cornerStride() { return 2 + (int) morphSlots().size(); }

    inline const std::vector<std::string>& tableNames()
    {
        static const std::vector<std::string> n = []
        {
            std::vector<std::string> v { "Classiques", "Scies sync", "Voyelles", "Harmoniques", "Plis", "Num\xC3\xA9rique",
                                         "M\xC3\xA9tal FM", "Acide", "Cristal", "R\xC3\xA9sonance (scie)", "R\xC3\xA9sonance (trap\xC3\xA8ze)", "Bruit (blanc \xE2\x86\x92 brun)" };
            // les tables des autres synthés de GOLEM, calculées par eux (rien à charger)
            const auto& g = gd2::tableNames();
            for (int t = 0; t < golemTables; ++t)
                v.push_back ((t < gluta::numTables ? "Glutamake : " : t < gdoc::numTables ? "Good Doctor : " : "Good Doctor 2 : ") + g[(size_t) t]);
            v.push_back ("Perso (WAV)");
            return v;
        }();
        return n;
    }
    inline const std::vector<std::string>& filterNames()
    {
        static const std::vector<std::string> n { "Passe-bas 24", "Passe-bas 12", "Passe-bande", "Passe-haut", "Coupe-bande",
                                                  "Peigne +", "Peigne \xE2\x88\x92", "Formant" };
        return n;
    }
    inline const std::vector<std::string>& destNames()
    {
        static const std::vector<std::string> n { "Aucune", "Hauteur", "Hauteur porteuse", "Hauteur modulateur", "Coupure", "R\xC3\xA9sonance",
                                                  "FM", "Fold", "Crush", "Position porteuse", "Position modulateur", "M\xC3\xA9lange osc.",
                                                  "Volume", "Panoramique", "PWM", "Drive", "ORB X", "ORB Y",
                                                  "D\xC3\xA9" "clin du filtre", "D\xC3\xA9" "clin de l'ampli", "Bruit", "Distorsion de phase porteuse", "Distorsion de phase modulateur",
                                                  "Shape modulateur", "Distorsion (effet)", "Flanger profondeur", "Flanger r\xC3\xA9troaction", "\xC3\x89" "cho temps",
                                                  "\xC3\x89" "cho r\xC3\xA9troaction", "\xC3\x89" "cho m\xC3\xA9lange", "R\xC3\xA9verb taille",
                                                  "Flanger temps", "R\xC3\xA9verb diffusion", "\xC3\x89" "cho couleur", "Phaser fr\xC3\xA9quence",
                                                  "Phaser r\xC3\xA9troaction", "St\xC3\xA9r\xC3\xA9o largeur", "Peigne hauteur" };
        return n;
    }
    inline const std::vector<std::string>& lfoShapeNames()
    {
        static const std::vector<std::string> n { "Sinus", "Triangle", "Scie", "Rampe", "Carr\xC3\xA9", "Hasard (A&M)", "Hasard liss\xC3\xA9",
                                                  "Chaos (sauts)", "Chaos liss\xC3\xA9", "Bruit blanc", "Bruit rose" };
        return n;
    }
    inline const std::vector<std::string>& divNames()
    {
        static const std::vector<std::string> n { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/8 point\xC3\xA9" "e", "1/16 point\xC3\xA9" "e" };
        return n;
    }
    inline double divQuarters (int i) noexcept
    {
        static constexpr double q[] { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };
        return q[std::clamp (i, 0, 10)];
    }
    inline const std::vector<std::string>& lfoPick()
    {
        static const std::vector<std::string> n { "L1", "L2", "L3", "L4", "L5", "L6", "L7", "L8", "Fixe" }; // Fixe : le niveau seul (portes, escaliers)
        return n;
    }
    /** 32 motifs classiques pour les couloirs : 16 pas (LFO 0-7, 8 = fixe ; niveau en %), répétés jusqu'à 32.
        Choisir un motif remplace les pas du couloir ; « Perso » joue les pas réglés à la main. */
    struct Motif { const char* name; std::uint8_t src[16]; std::uint8_t lvl[16]; };
    inline const std::vector<Motif>& motifs()
    {
        static const std::vector<Motif> m = []
        {
            std::vector<Motif> v;
            auto add = [&v] (const char* n, std::initializer_list<int> src, std::initializer_list<int> lvl)
            {
                Motif x { n, {}, {} };
                for (int i = 0; i < 16; ++i)
                {
                    x.src[i] = (std::uint8_t) (src.size() == 1 ? *src.begin() : *(src.begin() + (i % (int) src.size())));
                    x.lvl[i] = (std::uint8_t) (lvl.size() == 1 ? *lvl.begin() : *(lvl.begin() + (i % (int) lvl.size())));
                }
                v.push_back (x);
            };
            auto euclid = [] (int hits, int steps) { std::vector<int> r ((size_t) steps, 0); for (int i = 0; i < steps; ++i) r[(size_t) i] = ((i * hits) % steps) < hits ? 100 : 0; return r; };
            auto addE = [&add, &euclid] (const char* n, int h, int st)
            {
                const auto e = euclid (h, st);
                std::vector<int> l16 (16);
                for (int i = 0; i < 16; ++i) l16[(size_t) i] = e[(size_t) (i % st)];
                add (n, { 8 }, { l16[0], l16[1], l16[2], l16[3], l16[4], l16[5], l16[6], l16[7], l16[8], l16[9], l16[10], l16[11], l16[12], l16[13], l16[14], l16[15] });
            };
            const int F = 8; // fixe
            // portes et rythmes (pas fixes : le niveau seul)
            add ("Plein", { F }, { 100 });
            add ("Roulement psy", { F }, { 0, 100, 100, 100 });
            add ("Croches", { F }, { 100, 0 });
            add ("Contretemps", { F }, { 0, 0, 100, 0 });
            add ("Galop", { F }, { 100, 0, 100, 100 });
            add ("Galop invers\xC3\xA9", { F }, { 100, 100, 0, 100 });
            add ("Porte de trance", { F }, { 100, 0, 100, 100, 0, 100, 100, 0, 100, 0, 100, 100, 0, 100, 100, 0 });
            addE ("Euclide 3/8", 3, 8);
            addE ("Euclide 5/8", 5, 8);
            addE ("Euclide 5/16", 5, 16);
            addE ("Euclide 7/16", 7, 16);
            addE ("Euclide 9/16", 9, 16);
            // escaliers et accents
            add ("Mont\xC3\xA9" "e", { F }, { 6, 12, 18, 25, 31, 37, 43, 50, 56, 62, 68, 75, 81, 87, 93, 100 });
            add ("Descente", { F }, { 100, 93, 87, 81, 75, 68, 62, 56, 50, 43, 37, 31, 25, 18, 12, 6 });
            add ("Aller-retour", { F }, { 12, 25, 37, 50, 62, 75, 87, 100, 100, 87, 75, 62, 50, 37, 25, 12 });
            add ("Escalier", { F }, { 25, 50, 75, 100 });
            add ("Accent", { F }, { 100, 30, 30, 30 });
            add ("Accents psy", { F }, { 100, 40, 70, 40 });
            add ("Hasard fig\xC3\xA9 A", { F }, { 70, 15, 95, 40, 5, 85, 30, 60, 100, 20, 50, 80, 10, 65, 35, 90 });
            add ("Hasard fig\xC3\xA9 B", { F }, { 30, 90, 10, 55, 75, 20, 100, 45, 15, 70, 40, 95, 60, 5, 85, 25 });
            // mouvements de LFO (chaque pas joue un LFO)
            add ("Huit LFO", { 0, 1, 2, 3, 4, 5, 6, 7 }, { 100 });
            add ("LFO 1 et 2", { 0, 1 }, { 100 });
            add ("Vagues", { 4 }, { 100 });
            add ("Lent puis vif", { 0, 0, 0, 0, 4, 4, 4, 4 }, { 100 });
            add ("Hasard pas \xC3\xA0 pas", { 6, 7 }, { 100 });
            add ("Chaos glitch", { 6, 7, 4, 6, 7, 6, 4, 7 }, { 100, 100, 60, 100, 80, 100, 50, 100 });
            add ("Squelch", { 4, 3, 4, 1 }, { 100, 60, 100, 40 });
            add ("Sir\xC3\xA8ne", { 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2 }, { 100 });
            add ("Rampes", { 0, 2 }, { 100 });
            add ("B\xC3\xA9gaiement", { 4, F, 4, F, 5, 5, F, 4 }, { 100, 0, 100, 0, 100, 100, 0, 100 });
            add ("Hitech", { 6, 7, 6, 4 }, { 100, 80, 100, 100 });
            add ("Respiration", { 0 }, { 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100 });
            return v;
        }();
        return m;
    }
    inline std::vector<std::string> motifNames()
    {
        std::vector<std::string> v { "Perso (pas \xC3\xA0 la main)" };
        for (const auto& x : motifs()) v.push_back (x.name);
        return v;
    }

    inline const std::vector<std::string>& orbShapeNames()
    {
        static const std::vector<std::string> n { "Cercle", "Ellipse", "Huit", "Lissajous 3:2", "\xC3\x89toile", "Spirale", "Rose", "Triangle", "Carr\xC3\xA9",
                                                  "Lorenz", "R\xC3\xB6ssler", "Chaos" };
        return n;
    }
    inline const std::vector<std::string>& orbOutNames()
    {
        static const std::vector<std::string> n { "ORB X", "ORB Y", "Orbite X", "Orbite Y", "Orbe 3 X", "Orbe 3 Y", "Distance point-3", "Angle point-3" };
        return n;
    }
    /** Sources propres offertes à la matrice (après les macros), dans cet ordre. */
    enum Extra { exLfo1 = 0, exLane1 = exLfo1 + numLfos, exOrbX = exLane1 + numLanes, exOrbY, exO3X, exO3Y, numExtras };
    inline std::vector<std::string> extraNames()
    {
        std::vector<std::string> v;
        for (int k = 0; k < numLfos; ++k) v.push_back ("LFO " + std::to_string (k + 1));
        for (int k = 0; k < numLanes; ++k) v.push_back ("Couloir " + std::to_string (k + 1));
        for (const char* s : { "ORB X", "ORB Y", "Orbe 3 X", "Orbe 3 Y" }) v.push_back (s);
        return v;
    }

    inline double hash01 (std::uint64_t n) noexcept
    {
        n ^= n >> 33; n *= 0xff51afd7ed558ccdULL; n ^= n >> 33; n *= 0xc4ceb9fe1a85ec53ULL; n ^= n >> 33;
        return (double) (n >> 11) * (1.0 / 9007199254740992.0);
    }
    /** Bruit lisse (-1..1) : valeurs au hasard aux entiers, fondu en cosinus entre elles ; ne se répète jamais. */
    inline double smoothNoise (double x, std::uint64_t seed) noexcept
    {
        const auto i0 = (std::int64_t) std::floor (x);
        const auto f = x - (double) i0;
        const auto a = 2.0 * hash01 ((std::uint64_t) i0 * 0x9E37u + seed) - 1.0, b = 2.0 * hash01 ((std::uint64_t) (i0 + 1) * 0x9E37u + seed) - 1.0;
        const auto s = 0.5 - 0.5 * fastSin2Pi (f * 0.5 + 0.25);
        return a + (b - a) * s;
    }

    inline const WaveBank& bank()
    {
        static const WaveBank b = []
        {
            using cd = std::complex<double>;
            WaveBank w;
            auto sine = [] (double a) { return cd (0.0, -0.5 * a); };
            auto pulse = [] (double width) { return [width] (double x) { return x < width ? 1.0 : -1.0; }; };
            // 0 · Classiques : scie, double scie, carré, impulsions, triangle, sinus + quinte, sinus
            w.addWaveTime ([] (double x) { return 2.0 * x - 1.0; });
            w.addWaveTime ([] (double x) { return (2.0 * x - 1.0) * 0.6 + (2.0 * (x * 2.0 - std::floor (x * 2.0)) - 1.0) * 0.4; });
            w.addWaveTime (pulse (0.5));
            w.addWaveTime (pulse (0.3));
            w.addWaveTime (pulse (0.12));
            w.addWaveTime ([] (double x) { return 1.0 - 4.0 * std::abs (x - 0.5); });
            w.addWaveTime ([] (double x) { return std::sin (kTwoPi * x) * 0.75 + std::sin (kTwoPi * 3.0 * x) * 0.25; });
            w.addWaveTime ([] (double x) { return std::sin (kTwoPi * x); });
            // 1 · Scies sync : scie relancée à chaque cycle (synchro dure), rapport croissant
            for (double r : { 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 7.0 })
                w.addWaveTime ([r] (double x) { const auto y = x * r; return (2.0 * (y - std::floor (y)) - 1.0) * (1.0 - 0.35 * x); });
            // 2 · Voyelles : harmoniques d'une fondamentale de 110 Hz passées par trois formants
            {
                static constexpr double F[8][3] { { 325, 700, 2700 }, { 450, 800, 2830 }, { 800, 1150, 2900 }, { 660, 1700, 2400 },
                                                  { 400, 1600, 2700 }, { 300, 2300, 3000 }, { 250, 1750, 2150 }, { 350, 1350, 2250 } };
                for (const auto& f : F)
                    w.addWave ([&f, sine] (int h) -> cd
                    {
                        if (h > 90) return cd();
                        const auto hz = 110.0 * h;
                        double a = 0.0;
                        for (int k = 0; k < 3; ++k) { const auto d = (hz - f[k]) / (60.0 + 40.0 * k); a += (k == 0 ? 1.0 : k == 1 ? 0.7 : 0.35) * std::exp (-d * d); }
                        return sine ((a + 0.02) / std::sqrt ((double) h) * (h % 2 ? 1.0 : -1.0));
                    });
            }
            // 3 · Harmoniques : de plus en plus d'harmoniques (orgue qui s'ouvre)
            for (int top : { 2, 4, 6, 9, 13, 18, 26, 40 })
                w.addWave ([top, sine] (int h) -> cd { return h <= top ? sine (1.0 / std::pow ((double) h, 0.7)) : cd(); });
            // 4 · Plis : sinus replié de plus en plus fort
            for (double g : { 1.0, 1.8, 2.6, 3.5, 4.5, 6.0, 8.0, 11.0 })
                w.addWaveTime ([g] (double x) { return std::sin (0.5 * kPi * g * std::sin (kTwoPi * x)); });
            // 5 · Numérique : scie en marches, puis motifs de bits
            for (int q : { 16, 8, 4, 3 })
                w.addWaveTime ([q] (double x) { return std::floor (x * q) / (q - 1) * 2.0 - 1.0; });
            for (int k : { 1, 2, 3, 5 })
                w.addWaveTime ([k] (double x)
                {
                    const auto i = (int) (x * 256.0) & 255;
                    return (double) ((i ^ (i >> k)) & 255) / 127.5 - 1.0;
                });
            // 6 · Métal FM : sinus modulé en phase (rapport entier, indice croissant)
            {
                static constexpr double RI[8][2] { { 2, 0.5 }, { 2, 1.5 }, { 3, 1.2 }, { 3, 3.0 }, { 4, 2.0 }, { 5, 2.5 }, { 7, 3.0 }, { 9, 4.0 } };
                for (const auto& ri : RI)
                    w.addWaveTime ([&ri] (double x) { return std::sin (kTwoPi * x + ri[1] * std::sin (kTwoPi * ri[0] * x)); });
            }
            // 7 · Acide : scie avec une bosse de résonance qui monte dans le spectre
            for (double c : { 3.0, 5.0, 7.0, 10.0, 14.0, 19.0, 25.0, 32.0 })
                w.addWave ([c, sine] (int h) -> cd
                {
                    const auto d = ((double) h - c) / (1.2 + 0.08 * c);
                    return sine ((1.0 + 5.0 * std::exp (-d * d)) / (double) h);
                });
            // 8 · Cristal : partiels épars (nombres triangulaires) dont le poids glisse vers l'aigu
            {
                static constexpr int part[7] { 1, 3, 6, 10, 15, 21, 28 };
                for (int f = 0; f < frames; ++f)
                    w.addWave ([f, sine] (int h) -> cd
                    {
                        for (int k = 0; k < 7; ++k)
                            if (part[k] == h) { const auto d = (double) k - f * 0.85; return sine (std::exp (-0.6 * d * d) + (k == 0 ? 0.25 : 0.0)); }
                        return cd();
                    });
            }
            // 9 et 10 · Résonances (principe des ondes résonantes à distorsion de phase) : un sinus de plus en plus aigu
            // dans une fenêtre en scie (9) ou en trapèze (10), relancé à chaque cycle
            for (double r : { 1.5, 2.0, 3.0, 4.0, 5.5, 7.0, 9.0, 12.0 })
                w.addWaveTime ([r] (double x) { return (1.0 - x) * std::sin (kTwoPi * r * x) * 1.2; });
            for (double r : { 1.5, 2.0, 3.0, 4.0, 5.5, 7.0, 9.0, 12.0 })
                w.addWaveTime ([r] (double x)
                {
                    const auto win = x < 0.5 ? 1.0 : std::max (0.0, 1.0 - (x - 0.5) * 2.0);
                    return win * std::sin (kTwoPi * r * x);
                });
            return w;
        }();
        return b;
    }

    /** Table perso d'un WAV (emplacement 1 : porteuse, 2 : modulateur), calculée au chargement. */
    struct SampleRuntime
    {
        int frames = 0;
        WaveBank table;
    };
    constexpr int userFrameSize = 2048, userMaxFrames = 64;

    /** Construit la table perso d'un échantillon (hors du fil audio). Format courant des tables :
        des ondes de 2048 points à la suite ; plus court : un seul cycle étiré. */
    inline std::shared_ptr<void> buildRuntime (int index, const SampleBuffer& s)
    {
        if (index < 0 || index > 1 || s.data.size() < 16) return {};
        auto r = std::make_shared<SampleRuntime>();
        const auto n = (int) s.data.size();
        if (n >= userFrameSize)
        {
            const auto count = std::min (userMaxFrames, n / userFrameSize);
            for (int f = 0; f < count; ++f)
            {
                const float* wv = s.data.data() + (size_t) f * userFrameSize;
                r->table.addWaveTime ([wv] (double x) { return (double) wv[(size_t) std::clamp ((int) (x * userFrameSize), 0, userFrameSize - 1)]; });
            }
            r->frames = count;
        }
        else
        {
            const float* wv = s.data.data();
            r->table.addWaveTime ([wv, n] (double x)
            {
                const auto p = x * n;
                const auto i = (int) p;
                const auto f = p - i;
                return (double) wv[(size_t) (i % n)] * (1.0 - f) + (double) wv[(size_t) ((i + 1) % n)] * f;
            });
            r->frames = 1;
        }
        return r;
    }


    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "croc";
            m.name = "Croc Glac\xC3\xA9";
            m.family = "Nappes";
            m.principle = "Synth\xC3\xA9 psy : tables d'ondes porteuse et modulateur, FM et distorsion cr\xC3\xA9" "ative, 8 LFO jou\xC3\xA9s en s\xC3\xA9quence par 4 couloirs, et l'ORB : quatre sons dans les coins, m\xC3\xA9lang\xC3\xA9s par un point en orbite";
            m.sampleRuntime = "croc";
            std::vector<std::string> tabB { "Aucune" };
            for (const auto& t : tableNames()) tabB.push_back (t);
            m.params = {
                C ("vmode", "Jeu", { "Poly", "Mono", "Legato", "Unisson" }, 0),
                I ("voices", "Voix", "", 1, 8, 8),
                I ("univoices", "Voix d'unisson", "", 2, 8, 4),
                P ("unidetune", "D\xC3\xA9saccord d'unisson", "ct", 0, 50, 12),
                P ("glide", "Gliss\xC3\xA9", "ms", 0, 2000, 0, 0.4f),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -40, 6, -22),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 40),
                P ("spread", "Largeur", "%", 0, 100, 40),
                C ("ctab", "Table", tableNames(), 0),
                P ("cpos", "Position", "%", 0, 100, 100), // Init : sinus (dernière onde des Classiques)
                I ("coct", "Octave", "", -3, 3, 0),
                I ("csemi", "Demi-ton", "dt", -12, 12, 0),
                P ("cfine", "Fin", "ct", -100, 100, 0),
                P ("cpwm", "PWM", "%", 0, 100, 0),
                P ("cpwmrate", "Vitesse PWM", "Hz", 0.05f, 20, 0.8f, 0.4f),
                P ("cpd", "Distorsion de phase", "%", 0, 100, 0),
                C ("mtaba", "Onde A", tableNames(), 0),
                C ("mtabb", "Onde B", tabB, 0),
                P ("mpos", "Position", "%", 0, 100, 0), // Init : scie
                P ("mab", "M\xC3\xA9lange A/B", "%", 0, 100, 50),
                I ("moct", "Octave", "", -3, 3, 0),
                I ("msemi", "Demi-ton", "dt", -12, 12, 0),
                P ("mfine", "Fin", "ct", -100, 100, 0),
                P ("mcrush", "Crush", "%", 0, 100, 0),
                P ("mshape", "Shape", "%", 0, 100, 0),
                P ("mfold", "Fold", "%", 0, 100, 0),
                P ("mpd", "Distorsion de phase", "%", 0, 100, 0),
                P ("oscmix", "Porteuse \xE2\x86\x94 modulateur", "%", 0, 100, 0),
                P ("sub", "Sous-oscillateur", "%", 0, 100, 0),
                P ("noise", "Bruit", "%", 0, 100, 0),
                P ("noisecol", "Couleur du bruit", "%", 0, 100, 0), // 0 : blanc, 50 : rose, 100 : brun
                P ("fm", "FM", "%", 0, 100, 0),
                P ("fold", "Fold", "%", 0, 100, 0),
                P ("crush", "Crush", "%", 0, 100, 0),
                P ("soft", "Soft", "%", 0, 100, 0),
                // enveloppe de modulation : le « zap » de l'attaque (FM et hauteur)
                P ("moda", "Attaque", "ms", 0.5f, 10000, 0.5f, 0.3f),
                P ("modd", "D\xC3\xA9" "clin", "ms", 1, 10000, 60, 0.3f),
                P ("mods", "Maintien", "%", 0, 100, 0),
                P ("modr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 100, 0.3f),
                P ("modfm", "Env. \xE2\x86\x92 FM", "%", -100, 100, 0),
                P ("modpitch", "Env. \xE2\x86\x92 hauteur", "dt", -48, 48, 0),
                C ("ftype", "Type", filterNames(), 0),
                P ("cutoff", "Coupure", "Hz", 20, 20000, 2500, 0.25f),
                P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 20),
                C ("f2stage", "Deux \xC3\xA9tages", onOff, 0),
                P ("fdrive", "Drive", "%", 0, 100, 0),
                P ("fenv", "Enveloppe", "%", -100, 100, 30),
                P ("fkey", "Suivi clavier", "%", 0, 100, 40),
                P ("fvel", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 0),
                C ("flsync", "LFO synchro", lfoSyncChoices(), 8),
                P ("flrate", "LFO vitesse", "Hz", 0.02f, 40, 2, 0.35f),
                P ("flamt", "LFO quantit\xC3\xA9", "%", 0, 100, 0),
                C ("flshape", "LFO forme", { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Hasard" }, 1),
                P ("aa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 600, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 80),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 250, 0.3f),
                P ("fa", "Attaque", "ms", 0.5f, 10000, 1, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 350, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 20),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 300, 0.3f),
                C ("distmode", "Distorsion type", { "Lampe", "\xC3\x89" "cr\xC3\xAA" "tage", "Bitcrush", "Repli" }, 0),
                P ("distdrive", "Distorsion drive", "%", 0, 100, 40),
                P ("distmix", "Distorsion m\xC3\xA9lange", "%", 0, 100, 0),
                P ("flgtime", "Flanger temps", "ms", 0.05f, 10, 2, 0.4f), // très court : laser, métal
                P ("flgrate", "Flanger vitesse", "Hz", 0.02f, 20, 0.3f, 0.4f),
                P ("flgdepth", "Flanger profondeur", "%", 0, 100, 60),
                P ("flgfb", "Flanger r\xC3\xA9troaction", "%", -99, 99, 50),
                P ("flgmix", "Flanger m\xC3\xA9lange", "%", 0, 100, 0),
                C ("dlmode", "\xC3\x89" "cho mode", { "Simple", "Ping-pong" }, 1),
                C ("dlsync", "\xC3\x89" "cho synchro", { "Libre", "Tempo" }, 1),
                P ("dltime", "\xC3\x89" "cho temps", "ms", 0.05f, 2000, 375, 0.3f), // jusqu'à 0,05 ms : peigne métallique
                C ("dldiv", "\xC3\x89" "cho division", grv::effects::echo::divisions(), 9),
                P ("dlfb", "\xC3\x89" "cho r\xC3\xA9troaction", "%", 0, 120, 35), // au-delà de 100 : l'écho s'emballe (saturé)
                P ("dlglide", "\xC3\x89" "cho gliss\xC3\xA9", "ms", 0, 500, 40, 0.5f), // 0 : le temps saute net (glitch)
                P ("dltone", "\xC3\x89" "cho couleur", "%", 0, 100, 30),
                P ("dlmix", "\xC3\x89" "cho m\xC3\xA9lange", "%", 0, 100, 0),
                P ("rvsize", "R\xC3\xA9verb taille", "%", 0, 100, 50),
                P ("rvwidth", "R\xC3\xA9verb largeur", "%", 0, 100, 100),
                P ("rvdamp", "R\xC3\xA9verb amortissement", "%", 0, 100, 40),
                P ("rvdiff", "R\xC3\xA9verb diffusion", "%", 0, 100, 75), // faible : réverbération métallique
                P ("rvmix", "R\xC3\xA9verb m\xC3\xA9lange", "%", 0, 100, 0),
            };
            // les 8 LFO (au tempo par défaut)
            static const int lSyncDef[numLfos] { 6, 7, 7, 8, 9, 7, 9, 8 }, lShapeDef[numLfos] { 3, 2, 3, 2, 1, 1, 5, 5 };
            for (int k = 0; k < numLfos; ++k)
            {
                const auto id = "l" + std::to_string (k + 1);
                const auto nm = "LFO " + std::to_string (k + 1) + " ";
                m.params.push_back (C (id + "shape", nm + "forme", lfoShapeNames(), lShapeDef[k]));
                m.params.push_back (C (id + "sync", nm + "synchro", lfoSyncChoices(), lSyncDef[k]));
                m.params.push_back (P (id + "rate", nm + "vitesse", "Hz", 0.02f, 1000, 1, 0.2f)); // libre : jusqu'aux fréquences audio (grain, glitch)
                m.params.push_back (P (id + "amt", nm + "quantit\xC3\xA9", "%", 0, 100, 100));
                m.params.push_back (I (id + "steps", nm + "marches (lisse \xE2\x86\x92 num\xC3\xA9rique)", "", 0, 32, 0));
                m.params.push_back (C (id + "retrig", nm + "repart \xC3\xA0 la note", onOff, 0));
                m.params.push_back (P (id + "phase", nm + "phase", "\xC2\xB0", 0, 360, 0));
            }
            // les 4 couloirs : chaque pas joue l'un des 8 LFO, à son niveau
            static const int lnDstDef[numLanes] { dCut, dFm, dPosC, dPitch };
            for (int l = 0; l < numLanes; ++l)
            {
                const auto id = "c" + std::to_string (l + 1);
                const auto nm = "Couloir " + std::to_string (l + 1) + " ";
                m.params.push_back (C (id + "on", nm + "en marche", onOff, 0));
                m.params.push_back (I (id + "len", nm + "longueur", "", 1, laneSteps, 16));
                m.params.push_back (C (id + "div", nm + "division", divNames(), 4));
                m.params.push_back (C (id + "dir", nm + "sens", { "Avant", "Arri\xC3\xA8re", "Aller-retour", "Hasard" }, 0));
                m.params.push_back (C (id + "mode", nm + "mode", { "Grille du morceau", "\xC3\x80 chaque note", "Libre" }, 0));
                m.params.push_back (P (id + "swing", nm + "swing", "%", 50, 75, 50));
                m.params.push_back (P (id + "smooth", nm + "lissage", "%", 0, 100, 0));
                m.params.push_back (I (id + "quant", nm + "quantification", "", 0, 32, 0));
                m.params.push_back (P (id + "curve", nm + "courbe", "%", -100, 100, 0));
                m.params.push_back (P (id + "fold", nm + "repli", "%", 0, 100, 0));
                m.params.push_back (P (id + "jit", nm + "hasard par pas", "%", 0, 100, 0));
                m.params.push_back (C (id + "dst", nm + "destination", destNames(), lnDstDef[l]));
                m.params.push_back (P (id + "amt", nm + "quantit\xC3\xA9", "%", -100, 100, 0));
                {
                    std::vector<std::string> srcN { "S\xC3\xA9quence (les pas)" };
                    for (int k = 0; k < numLfos; ++k) srcN.push_back ("LFO " + std::to_string (k + 1) + " seul");
                    m.params.push_back (C (id + "src", nm + "source", srcN, 0));
                    m.params.push_back (C (id + "motif", nm + "motif", motifNames(), 0));
                }
                for (int s = 0; s < laneSteps; ++s)
                {
                    const auto n = std::to_string (s + 1);
                    m.params.push_back (C (id + "l" + n, nm + "pas " + n + " LFO", lfoPick(), s % numLfos));
                    m.params.push_back (P (id + "v" + n, nm + "pas " + n + " niveau", "%", 0, 100, 100));
                }
            }
            const std::vector<std::string> rel { "Oppos\xC3\xA9", "M\xC3\xAAme sens" };
            const std::vector<ParamDesc> orb {
                C ("orbon", "Orbite en marche", onOff, 0),
                C ("o1shape", "Orbite forme", orbShapeNames(), 0),
                C ("o1dir", "Orbite sens", { "Avant", "Arri\xC3\xA8re", "Aller-retour" }, 0),
                C ("o1sync", "Orbite synchro", lfoSyncChoices(), 4),
                P ("o1rate", "Orbite vitesse", "Hz", 0.02f, 80, 0.25f, 0.3f),
                P ("o1size", "Orbite taille", "%", 0, 100, 70),
                P ("o1rot", "Orbite rotation", "\xC2\xB0", 0, 360, 0),
                P ("o1chaos", "Orbite chaos", "%", 0, 100, 0),
                C ("o2shape", "Sous-orbite forme", orbShapeNames(), osCircle),
                P ("o2size", "Sous-orbite taille", "%", 0, 100, 0),
                P ("o2ratio", "Sous-orbite vitesse", "\xC3\x97", 0.125f, 16, 3, 0.35f),
                C ("o2dir", "Sous-orbite sens", rel, 0),
                P ("o2phase", "Sous-orbite phase", "\xC2\xB0", 0, 360, 0),
                C ("o3shape", "Orbe 3 forme", orbShapeNames(), osEllipse),
                P ("o3size", "Orbe 3 taille", "%", 0, 100, 60),
                P ("o3ratio", "Orbe 3 vitesse", "\xC3\x97", 0.125f, 16, 0.5f, 0.35f),
                C ("o3dir", "Orbe 3 sens", rel, 0),
                P ("o3phase", "Orbe 3 phase", "\xC2\xB0", 0, 360, 90),
                C ("orbphys", "Physique", { "Rigide", "\xC3\x89lastique", "Gravit\xC3\xA9" }, 0),
                P ("orbspring", "\xC3\x89lasticit\xC3\xA9", "%", 0, 100, 40),
                P ("orbdamp", "Amortissement", "%", 0, 100, 30),
                P ("orbbounce", "Rebond", "%", 0, 100, 60),
                C ("orbretrig", "D\xC3\xA9part de l'orbite", { "Libre", "\xC3\x80 chaque note" }, 0),
            };
            for (const auto& p : orb) m.params.push_back (p);
            static const int rSrc[numRoutes] { 0, 1, 6, 4, 5, 7 }, rDst[numRoutes] { dCut, dRes, dFm, dPitch, dPosC, dFold };
            for (int r = 0; r < numRoutes; ++r)
            {
                const auto n = std::to_string (r + 1);
                m.params.push_back (C ("r" + n + "src", "Route " + n + " source", orbOutNames(), rSrc[r]));
                m.params.push_back (C ("r" + n + "dst", "Route " + n + " destination", destNames(), rDst[r]));
                m.params.push_back (P ("r" + n + "amt", "Route " + n + " quantit\xC3\xA9", "%", -100, 100, 0));
            }
            m.params.push_back (C ("morphon", "ORB (morph des 4 coins)", { "Arr\xC3\xAAt", "Marche" }, 0));
            m.params.push_back (P ("morphamt", "Profondeur du morph", "%", 0, 100, 100));
            m.params.push_back (P ("morphx", "Point X", "%", -100, 100, 0));
            m.params.push_back (P ("morphy", "Point Y", "%", -100, 100, 0));

            m.sections = {
                { "voice", "Voix et sortie", "global", { "vmode", "voices", "univoices", "unidetune", "glide", "tune", "spread", "level" }, "global" },
                { "carrier", "Porteuse", "osc", { "ctab", "cpos", "coct", "csemi", "cfine", "cpwm", "cpwmrate", "cpd" }, "osc" },
                { "modulator", "Modulateur", "osc", { "mtaba", "mtabb", "mpos", "mab", "moct", "msemi", "mfine" }, "osc" },
                { "modshape", "Modulateur : forme", "osc", { "mcrush", "mshape", "mfold", "mpd", "oscmix", "sub" }, "osc" },
                { "noise", "Bruit", "osc", { "noise", "noisecol" }, "osc" },
                { "bite", "Bite!", "dist", { "fm", "fold", "crush", "soft" }, "osc" },
                { "modenv", "Enveloppe de modulation (zap)", "env", { "moda", "modd", "mods", "modr", "modfm", "modpitch" }, "env" },
                { "filter", "Filtre", "filter", { "ftype", "cutoff", "reso", "f2stage", "fdrive", "fenv", "fkey", "fvel" }, "filter" },
                { "flfo", "LFO du filtre", "lfo", { "flsync", "flrate", "flamt", "flshape" }, "filter" },
                { "fenvs", "Enveloppe du filtre", "env", { "fa", "fd", "fs", "fr" }, "env" },
                { "aenv", "Ampli", "env", { "aa", "ad", "as", "ar", "velamp" }, "amp" },
                { "dist", "Distorsion", "fx", { "distmode", "distdrive", "distmix" }, "global" },
                { "flanger", "Flanger", "fx", { "flgtime", "flgrate", "flgdepth", "flgfb", "flgmix" }, "global" },
                { "echo", "\xC3\x89" "cho", "fx", { "dlmode", "dlsync", "dltime", "dldiv", "dlfb", "dlglide", "dltone", "dlmix" }, "global" },
                { "reverb", "R\xC3\xA9verb\xC3\xA9ration", "fx", { "rvsize", "rvwidth", "rvdamp", "rvdiff", "rvmix" }, "global" },
            };
            for (int k = 0; k < numLfos; ++k)
            {
                const auto id = "l" + std::to_string (k + 1);
                m.sections.push_back ({ "lfo" + std::to_string (k + 1), "LFO " + std::to_string (k + 1), "lfo",
                                        { id + "shape", id + "sync", id + "rate", id + "amt", id + "steps", id + "retrig", id + "phase" }, "mod" });
            }
            for (int l = 0; l < numLanes; ++l)
            {
                const auto id = "c" + std::to_string (l + 1);
                const auto nm = "Couloir " + std::to_string (l + 1);
                m.sections.push_back ({ "lane" + std::to_string (l + 1), nm, "lfo", { id + "on", id + "len", id + "div", id + "dir", id + "mode", id + "swing", id + "dst", id + "amt" }, "mod" });
                m.sections.push_back ({ "lanemod" + std::to_string (l + 1), nm + " : source et sortie", "lfo", { id + "src", id + "motif", id + "smooth", id + "quant", id + "curve", id + "fold", id + "jit" }, "mod" });
            }
            m.sections.push_back ({ "orbmorph", "ORB : morph des 4 coins", "mod", { "morphon", "morphamt", "morphx", "morphy" }, "mod" });
            m.sections.push_back ({ "orbit", "ORB : orbite", "mod", { "orbon", "o1shape", "o1dir", "o1sync", "o1rate", "o1size", "o1rot", "o1chaos" }, "mod" });
            m.sections.push_back ({ "orb2", "ORB : sous-orbite (sens oppos\xC3\xA9)", "mod", { "o2shape", "o2size", "o2ratio", "o2dir", "o2phase" }, "mod" });
            m.sections.push_back ({ "orb3", "ORB : orbe 3", "mod", { "o3shape", "o3size", "o3ratio", "o3dir", "o3phase" }, "mod" });
            m.sections.push_back ({ "orbphys", "ORB : physique", "mod", { "orbphys", "orbspring", "orbdamp", "orbbounce", "orbretrig" }, "mod" });
            for (int r = 0; r < numRoutes; r += 2)
            {
                const auto a = std::to_string (r + 1), b = std::to_string (r + 2);
                m.sections.push_back ({ "routes" + a, "ORB : routes " + a + " et " + b, "mod",
                                        { "r" + a + "src", "r" + a + "dst", "r" + a + "amt", "r" + b + "src", "r" + b + "dst", "r" + b + "amt" }, "mod" });
            }

            // sons d'usine (calculés par GOLEM) : psytrance d'abord — basses roulantes, squelch qui tourne
            using PV = std::vector<std::pair<std::string, float>>;
            auto pr = [&m] (const std::string& name, const std::string& cat, PV v) { m.presets.push_back ({ name, std::move (v), cat }); };
            auto with = [] (PV base, PV more) { for (auto& x : more) base.push_back (x); return base; };
            // couloir l (1..4) : pas (LFO 1..8, niveau) à la suite
            auto lane = [] (PV v, int l, PV head, std::initializer_list<std::array<float, 2>> st)
            {
                const auto id = "c" + std::to_string (l);
                v.push_back ({ id + "on", 1 });
                for (const auto& e : head) v.push_back ({ id + e.first, e.second });
                int s = 1;
                for (const auto& a : st) { const auto n = std::to_string (s++); v.push_back ({ id + "l" + n, a[0] - 1.0f }); v.push_back ({ id + "v" + n, a[1] }); }
                return v;
            };
            const PV bass { { "vmode", 1 }, { "sub", 25 }, { "ftype", 0 }, { "cutoff", 260 }, { "reso", 30 }, { "fenv", 55 }, { "fa", 0.5f }, { "fd", 130 }, { "fs", 0 }, { "fr", 60 },
                            { "aa", 0.5f }, { "ad", 170 }, { "as", 0 }, { "ar", 50 }, { "fkey", 50 }, { "velamp", 30 } };
            const std::string B = "Basses", S = "S\xC3\xA9quences", PL = "Plucks", N = "Nappes", FX = "Effets", A = "Arp\xC3\xA8ges";
            (void) lane; // pas réglés à la main dans un son d'usine (les motifs servent plus souvent)
            // un couloir sur un motif d'usine (n° de motifs(), 1-32)
            auto motif = [] (PV v, int l, int mo, PV head)
            {
                const auto id = "c" + std::to_string (l);
                v.push_back ({ id + "on", 1 }); v.push_back ({ id + "motif", (float) mo });
                for (const auto& e : head) v.push_back ({ id + e.first, e.second });
                return v;
            };
            const PV drone { { "aa", 1500 }, { "ad", 2000 }, { "as", 100 }, { "ar", 2000 }, { "fenv", 0 }, { "rvmix", 35 }, { "rvsize", 75 } };
            const std::string Dr = "Drones", Tx = "Textures", Gl = "Glitch", Me = "M\xC3\xA9tal";
            pr ("Init", "Init", {});
            // --- drones psychés qui tournent
            pr ("Drone en orbite", Dr, with (drone, { { "ctab", 2 }, { "cpos", 30 }, { "orbon", 1 }, { "o1sync", 3 }, { "o1size", 80 }, { "o2size", 30 }, { "o2ratio", 3 },
                { "r5src", 0 }, { "r5amt", 60 }, { "r1amt", 30 }, { "cutoff", 1800 }, { "reso", 35 } }));
            pr ("Drone FM tournant", Dr, with (drone, { { "fm", 30 }, { "moct", 1 }, { "msemi", 7 }, { "mpos", 100 }, { "orbon", 1 }, { "o1sync", 0 }, { "o1rate", 0.15f },
                { "r3src", 0 }, { "r3amt", 40 }, { "sub", 30 }, { "cutoff", 3000 } }));
            pr ("Drone de Lorenz", Dr, with (drone, { { "ctab", 4 }, { "cpos", 40 }, { "orbon", 1 }, { "o1shape", osLorenz }, { "o1sync", 0 }, { "o1rate", 0.3f },
                { "r1amt", 45 }, { "r6amt", 40 }, { "reso", 50 }, { "cutoff", 1200 } }));
            pr ("Gouffre", Dr, with (drone, { { "ctab", 20 }, { "cpos", 50 }, { "coct", -1 }, { "sub", 40 }, { "cutoff", 600 }, { "reso", 40 }, { "flamt", 30 }, { "flsync", 3 },
                { "orbon", 1 }, { "o1sync", 2 }, { "r1amt", 25 }, { "rvmix", 45 } }));
            pr ("Respiration alien", Dr, motif (with (drone, { { "ctab", 13 }, { "cpos", 50 }, { "cutoff", 2200 }, { "reso", 30 } }), 1, 32,
                { { "dst", dPosC }, { "amt", 60 }, { "div", 2 }, { "smooth", 60 } }));
            pr ("Cath\xC3\xA9" "drale de glace", Dr, with (drone, { { "ctab", 8 }, { "cpos", 40 }, { "vmode", 3 }, { "univoices", 4 }, { "unidetune", 18 }, { "flgmix", 25 },
                { "flgtime", 4 }, { "rvmix", 50 }, { "rvsize", 90 }, { "orbon", 1 }, { "o1sync", 3 }, { "r5src", 1 }, { "r5amt", 40 } }));
            // --- nappes qui mutent
            pr ("Aurore", N, with (drone, { { "ctab", 3 }, { "cpos", 60 }, { "cpwm", 50 }, { "orbon", 1 }, { "o1sync", 4 }, { "o1shape", osRose }, { "r5src", 0 }, { "r5amt", 50 },
                { "r1src", 1 }, { "r1amt", 25 }, { "cutoff", 3500 } }));
            pr ("Banquise lente", N, with (drone, { { "ctab", 1 }, { "cpos", 20 }, { "vmode", 3 }, { "univoices", 6 }, { "unidetune", 20 }, { "cutoff", 2000 }, { "reso", 25 },
                { "flamt", 25 }, { "flsync", 2 }, { "rvmix", 45 } }));
            pr ("Voile de voyelles", N, motif (motif (with (drone, { { "ctab", 27 }, { "cpos", 50 }, { "cutoff", 2600 } }), 1, 23, { { "dst", dPosC }, { "amt", 40 } }),
                2, 21, { { "dst", dCut }, { "amt", 30 }, { "div", 3 }, { "smooth", 60 } }));
            // --- textures alien
            pr ("Insectes", Tx, { { "ctab", 11 }, { "cpos", 0 }, { "ftype", 2 }, { "cutoff", 3000 }, { "reso", 80 }, { "fenv", 0 }, { "aa", 200 }, { "as", 100 }, { "ar", 800 },
                { "c1on", 1 }, { "c1src", 7 }, { "c1dst", dCut }, { "c1amt", 70 } });
            pr ("Bulles m\xC3\xA9talliques", Tx, { { "ctab", 11 }, { "cpos", 10 }, { "ftype", 2 }, { "cutoff", 1500 }, { "reso", 90 }, { "fenv", 0 }, { "aa", 50 }, { "as", 100 }, { "ar", 1200 },
                { "dlsync", 0 }, { "dltime", 6 }, { "dlfb", 92 }, { "dlmix", 50 }, { "dlglide", 15 }, { "l5sync", 0 }, { "l5rate", 3 },
                { "c1on", 1 }, { "c1src", 5 }, { "c1dst", dDlyTime }, { "c1amt", 25 } });
            pr ("Radio alien", Tx, motif ({ { "ctab", 5 }, { "cpos", 60 }, { "crush", 30 }, { "ftype", 2 }, { "cutoff", 1800 }, { "reso", 60 }, { "fenv", 0 }, { "as", 100 },
                { "distmode", 2 }, { "distmix", 40 }, { "distdrive", 50 } }, 1, 26, { { "dst", dPosC }, { "amt", 80 } }));
            pr ("Respirateur", Tx, { { "noise", 50 }, { "noisecol", 50 }, { "ctab", 2 }, { "ftype", 7 }, { "cutoff", 800 }, { "fenv", 0 }, { "aa", 800 }, { "as", 100 }, { "ar", 2000 },
                { "c1on", 1 }, { "c1src", 1 }, { "c1dst", dCut }, { "c1amt", 70 } });
            pr ("Cr\xC3\xA9" "ature", Tx, { { "ctab", 6 }, { "cpos", 40 }, { "fm", 20 }, { "mpd", 40 }, { "orbon", 1 }, { "orbphys", 2 }, { "o1sync", 0 }, { "o1rate", 2 },
                { "r3src", 6 }, { "r3amt", 60 }, { "r4src", 4 }, { "r4amt", 15 }, { "cutoff", 1500 }, { "reso", 55 }, { "as", 100 }, { "ar", 900 } });
            // --- glitch hitech (principe du tuto : sinus aigu, Chaos en sauts sur la hauteur)
            pr ("Glitch hitech", Gl, { { "vmode", 1 }, { "coct", 3 }, { "c1on", 1 }, { "c1src", 7 }, { "c1dst", dPitch }, { "c1amt", 60 }, { "l7shape", 7 },
                { "ad", 200 }, { "as", 60 }, { "ar", 80 }, { "cutoff", 6000 }, { "fenv", 0 } });
            pr ("Charleys glitch", Gl, motif ({ { "vmode", 1 }, { "coct", 2 }, { "c1on", 1 }, { "c1src", 8 }, { "c1dst", dPitch }, { "c1amt", 100 }, { "l8shape", 9 },
                { "ftype", 3 }, { "cutoff", 4000 }, { "fenv", 0 }, { "as", 100 }, { "ar", 40 } }, 2, 3, { { "dst", dVol }, { "amt", -100 } }));
            pr ("B\xC3\xA9gaiement num\xC3\xA9rique", Gl, motif (motif (motif ({ { "ctab", 5 }, { "cpos", 40 }, { "fenv", 0 }, { "as", 100 }, { "dlsync", 1 }, { "dldiv", 4 },
                { "dlfb", 50 }, { "dlmix", 30 }, { "dlglide", 0 } }, 1, 30, { { "dst", dVol }, { "amt", -100 }, { "div", 5 } }),
                2, 26, { { "dst", dPitch }, { "amt", 30 } }), 3, 19, { { "dst", dDlyTime }, { "amt", 60 }, { "div", 3 } }));
            pr ("Rafale FM", Gl, motif ({ { "vmode", 1 }, { "fm", 50 }, { "moct", 2 }, { "mpos", 100 }, { "modfm", 80 }, { "modpitch", 24 }, { "modd", 40 }, { "ad", 150 },
                { "as", 30 }, { "cutoff", 5000 }, { "fenv", 0 } }, 1, 31, { { "dst", dFm }, { "amt", 50 } }));
            pr ("Machine qui bugue", Gl, motif ({ { "ctab", 24 }, { "cpos", 30 }, { "c2on", 1 }, { "c2src", 7 }, { "c2dst", dCrush }, { "c2amt", 60 }, { "distmode", 2 },
                { "distmix", 50 }, { "distdrive", 60 }, { "fenv", 0 }, { "as", 100 } }, 1, 25, { { "dst", dPosC }, { "amt", 90 } }));
            // --- peignes métalliques (bruit, passe-bande résonant, écho très court qui s'emballe)
            pr ("Peigne m\xC3\xA9tallique", Me, { { "noise", 70 }, { "ftype", 5 }, { "cutoff", 400 }, { "reso", 90 }, { "fenv", 0 }, { "dlsync", 0 }, { "dltime", 3 },
                { "dlfb", 95 }, { "dlmix", 40 }, { "aa", 5 }, { "ad", 900 }, { "as", 20 }, { "ar", 600 } });
            pr ("Tuyau hurlant", Me, { { "ctab", 11 }, { "cpos", 50 }, { "ftype", 2 }, { "cutoff", 900 }, { "reso", 92 }, { "fenv", 0 }, { "flgtime", 0.3f }, { "flgfb", 95 },
                { "flgdepth", 30 }, { "flgrate", 0.2f }, { "flgmix", 60 }, { "as", 100 }, { "c1on", 1 }, { "c1src", 6 }, { "c1dst", dCut }, { "c1amt", 30 }, { "c1smooth", 50 } });
            pr ("Cloche de glace", Me, { { "ctab", 8 }, { "fm", 25 }, { "moct", 3 }, { "mpos", 100 }, { "ftype", 6 }, { "cutoff", 700 }, { "reso", 85 }, { "fenv", 0 },
                { "ad", 1500 }, { "as", 0 }, { "ar", 1500 }, { "dlsync", 0 }, { "dltime", 11 }, { "dlfb", 80 }, { "dlmix", 30 }, { "rvmix", 30 } });
            pr ("Sons m\xC3\xA9talliques", Me, { { "fm", 60 }, { "moct", 3 }, { "msemi", 5 }, { "mpd", 50 }, { "ftype", 2 }, { "cutoff", 2500 }, { "reso", 70 }, { "fenv", 0 },
                { "dlsync", 0 }, { "dltime", 2 }, { "dlfb", 92 }, { "dlmix", 35 }, { "rvdiff", 5 }, { "rvmix", 30 }, { "as", 100 } });
            // --- effets : lasers, balayages, montées, zaps
            pr ("Laser", FX, { { "vmode", 1 }, { "coct", 2 }, { "modpitch", -48 }, { "modd", 300 }, { "modfm", 60 }, { "fm", 30 }, { "flgtime", 0.5f }, { "flgfb", 90 },
                { "flgmix", 40 }, { "ad", 400 }, { "as", 0 }, { "ar", 300 }, { "cutoff", 8000 }, { "fenv", 0 } });
            pr ("Balayage cosmique", FX, { { "noise", 80 }, { "noisecol", 30 }, { "ftype", 2 }, { "cutoff", 400 }, { "reso", 70 }, { "fenv", 0 }, { "l1sync", 4 },
                { "c1on", 1 }, { "c1src", 1 }, { "c1dst", dCut }, { "c1amt", 90 }, { "flgmix", 40 }, { "rvmix", 40 }, { "aa", 500 }, { "as", 100 }, { "ar", 2000 } });
            pr ("Mont\xC3\xA9" "e", FX, motif (motif ({ { "noise", 40 }, { "ftype", 1 }, { "cutoff", 800 }, { "fenv", 0 }, { "aa", 100 }, { "as", 100 }, { "ar", 1000 } },
                1, 13, { { "dst", dPitch }, { "amt", 25 }, { "div", 2 }, { "smooth", 80 } }), 2, 13, { { "dst", dCut }, { "amt", 80 }, { "div", 2 }, { "smooth", 80 } }));
            pr ("Zap", FX, { { "vmode", 1 }, { "modpitch", 36 }, { "modd", 25 }, { "modfm", 100 }, { "fm", 0 }, { "mpos", 100 }, { "moct", 2 }, { "ad", 120 }, { "as", 0 },
                { "ar", 80 }, { "cutoff", 8000 }, { "fenv", 0 } });
            pr ("M\xC3\xA9t\xC3\xA9ore", FX, { { "orbon", 1 }, { "o1shape", osSpiral }, { "o1sync", 5 }, { "r4src", 3 }, { "r4amt", 50 }, { "fold", 30 }, { "cutoff", 2000 },
                { "aa", 50 }, { "as", 80 }, { "ar", 1200 }, { "dlmix", 40 }, { "dlfb", 55 }, { "rvmix", 30 } });
            // --- 27 nov. 2026 : les nouveaux effets (peigne qui suit la note, phaser, stéréo, gel, ordre)
            pr ("Cloches de glace", Me, { { "ctab", 8 }, { "cpos", 30 }, { "noise", 45 }, { "noisecol", 20 }, { "cutoff", 7000 }, { "fenv", 0 },
                { "aa", 0.5f }, { "ad", 70 }, { "as", 0 }, { "ar", 60 }, { "dlcomb", 1 }, { "dlmix", 70 }, { "dlfb", 96 }, { "dlglide", 0 }, { "dltone", 12 },
                { "rvmix", 30 }, { "rvsize", 70 }, { "rvdiff", 40 } });
            pr ("Laser balay\xC3\xA9", FX, { { "noise", 30 }, { "cpos", 70 }, { "fm", 20 }, { "cutoff", 9000 }, { "fenv", 0 }, { "aa", 5 }, { "as", 100 }, { "ar", 600 },
                { "flgtime", 1.2f }, { "flgfb", 93 }, { "flgdepth", 20 }, { "flgmix", 65 }, { "l1sync", 5 }, { "l1shape", 2 },
                { "c1on", 1 }, { "c1src", 1 }, { "c1dst", dFlgTime }, { "c1amt", 90 }, { "stwidth", 150 } });
            pr ("Tourbillon phas\xC3\xA9", N, with (drone, { { "ctab", 3 }, { "cpos", 45 }, { "vmode", 3 }, { "univoices", 3 }, { "unidetune", 12 }, { "cutoff", 4000 },
                { "phmix", 85 }, { "phstages", 2 }, { "phfb", 80 }, { "phrate", 0.12f }, { "phdepth", 85 }, { "phfreq", 600 }, { "stwidth", 165 }, { "rvmix", 40 } }));
            pr ("Glitch gel\xC3\xA9", Gl, motif ({ { "noise", 60 }, { "cpos", 80 }, { "cutoff", 9000 }, { "fenv", 0 }, { "aa", 0.5f }, { "ad", 140 }, { "as", 30 }, { "ar", 200 },
                { "dlcomb", 1 }, { "dlmix", 65 }, { "dlfb", 90 }, { "dlglide", 0 }, { "dltone", 25 }, { "fxorder", 1 }, { "rvmix", 35 }, { "rvsize", 55 }, { "rvdiff", 30 },
                { "distmode", 2 }, { "distdrive", 35 }, { "distmix", 40 }, { "sthaas", 9 } }, 1, 27, { { "dst", dCombPitch }, { "amt", 50 }, { "quant", 13 } }));
            // --- squelch
            pr ("Squelch qui tourne", S, with (bass, { { "cpos", 0 }, { "cutoff", 520 }, { "reso", 80 }, { "fdrive", 45 }, { "fenv", 35 }, { "fd", 160 }, { "ad", 220 },
                { "orbon", 1 }, { "o1sync", 7 }, { "o1size", 70 }, { "o2size", 35 }, { "o2ratio", 4 }, { "r1amt", 70 }, { "r2amt", 30 } }));
            pr ("Squelch s\xC3\xA9quenc\xC3\xA9", S, motif (motif (with (bass, { { "cpos", 0 }, { "reso", 82 }, { "fdrive", 40 }, { "cutoff", 400 } }),
                1, 27, { { "dst", dCut }, { "amt", 60 } }), 2, 18, { { "dst", dDecF }, { "amt", 60 } }));
            pr ("Gargouille orbitale", S, with (bass, { { "cpos", 0 }, { "ftype", 5 }, { "cutoff", 300 }, { "reso", 85 }, { "orbon", 1 }, { "orbphys", 1 }, { "orbspring", 70 },
                { "orbdamp", 15 }, { "o1sync", 7 }, { "o2size", 30 }, { "o2ratio", 3 }, { "r1amt", 60 }, { "fenv", 30 } }));
            // --- quelques basses
            pr ("Basse roulante", B, with (bass, { { "cpos", 0 } }));
            pr ("Basse glac\xC3\xA9" "e FM", B, with (bass, { { "cpos", 0 }, { "fm", 38 }, { "moct", 1 }, { "mpos", 100 }, { "cutoff", 700 }, { "fenv", 40 }, { "fd", 200 }, { "sub", 15 } }));
            pr ("Basse en orbite", B, with (bass, { { "cpos", 0 }, { "cutoff", 380 }, { "reso", 45 }, { "orbon", 1 }, { "o1sync", 9 }, { "o1size", 60 }, { "r1amt", 35 }, { "r2amt", 20 } }));
            pr ("Pluck cristal", PL, { { "ctab", 8 }, { "cpos", 30 }, { "cutoff", 4000 }, { "fenv", 40 }, { "fd", 220 }, { "fs", 0 }, { "ad", 380 }, { "as", 0 }, { "ar", 300 },
                { "dlmix", 22 }, { "rvmix", 25 } });
            pr ("Arp\xC3\xA8ge polaire", A, { { "arp_on", 1 }, { "arp_mode", 1 }, { "arp_rate", 3 }, { "arp_oct", 2 }, { "ctab", 8 }, { "cpos", 20 }, { "cutoff", 1800 }, { "reso", 45 },
                { "fenv", 35 }, { "fd", 180 }, { "fs", 0 }, { "ad", 250 }, { "as", 0 }, { "ar", 200 }, { "orbon", 1 }, { "o1sync", 5 }, { "r1amt", 40 }, { "dlmix", 25 }, { "rvmix", 20 } });

            // niveaux mesurés (intensité perçue, tools/PresetCheck) : -16 LUFS, -18 pour les nappes et les effets
            {
                static const std::vector<std::pair<std::string, float>> lv {
                    { "Init", -22.0f },
                    { "Drone en orbite", -14.4f },
                    { "Drone FM tournant", -17.6f },
                    { "Drone de Lorenz", -18.5f },
                    { "Gouffre", -9.4f },
                    { "Respiration alien", -12.3f },
                    { "Cath\xC3\xA9" "drale de glace", -10.7f },
                    { "Aurore", -13.6f },
                    { "Banquise lente", -15.3f },
                    { "Voile de voyelles", -8.6f },
                    { "Insectes", -38.5f },
                    { "Bulles m\xC3\xA9talliques", -29.9f },
                    { "Radio alien", -31.1f },
                    { "Respirateur", -27.1f },
                    { "Cr\xC3\xA9" "ature", -26.5f },
                    { "Glitch hitech", -18.3f },
                    { "Charleys glitch", -19.2f },
                    { "B\xC3\xA9gaiement num\xC3\xA9rique", -21.7f },
                    { "Rafale FM", -11.0f },
                    { "Machine qui bugue", -26.3f },
                    { "Peigne m\xC3\xA9tallique", -15.7f },
                    { "Tuyau hurlant", -24.0f },
                    { "Cloche de glace", 0.4f },
                    { "Sons m\xC3\xA9talliques", -33.6f },
                    { "Laser", -9.8f },
                    { "Balayage cosmique", -25.4f },
                    { "Mont\xC3\xA9" "e", -20.3f },
                    { "Zap", -7.5f },
                    { "M\xC3\xA9t\xC3\xA9ore", -15.2f },
                    { "Squelch qui tourne", -7.5f },
                    { "Squelch s\xC3\xA9quenc\xC3\xA9", -9.6f },
                    { "Gargouille orbitale", -9.4f },
                    { "Basse roulante", -8.1f },
                    { "Basse glac\xC3\xA9" "e FM", -7.0f },
                    { "Basse en orbite", -8.2f },
                    { "Pluck cristal", -11.6f },
                    { "Arp\xC3\xA8ge polaire", -7.5f },
                    { "Cloches de glace", -6.3f },
                    { "Laser balay\xC3\xA9", -23.1f },
                    { "Tourbillon phas\xC3\xA9", -13.7f },
                    { "Glitch gel\xC3\xA9", -10.4f } };
                for (auto& pz : m.presets)
                    for (const auto& e : lv)
                        if (pz.name == e.first) pz.values.push_back ({ "level", e.second });
            }
            HubSpec hub;
            hub.extEnvs = { "Env. ampli", "Env. filtre" };
            hub.envs = 1; hub.firstEnv = 3; hub.envName = "Env.";
            hub.lfos = 0;
            hub.slots = 16; hub.macros = 4; hub.arp = true; hub.arpSteps = true;
            hub.extraSources = extraNames();
            addModHub (m, hub);
            resolveHubPresetDests (m);

            // les 4 coins de l'ORB, après la modulation commune (hors des destinations de la matrice) :
            // copies des réglages du son, mêmes plages ; l'interface y charge un son d'usine ou capture le son en cours
            for (int c = 0; c < numCorners; ++c)
            {
                const std::string k = std::string ("k") + (char) ('a' + c) + "_";
                const std::string nm = std::string ("Coin ") + (char) ('A' + c) + " \xC2\xB7 ";
                m.params.push_back (C (k + "on", nm + "charg\xC3\xA9", onOff, 0));
                m.params.push_back (I (k + "src", nm + "son d'origine", "", 0, 999, 0));
                for (const auto sl : morphSlots())
                {
                    auto q = m.params[(size_t) sl];
                    q.id = k + q.id;
                    q.name = nm + q.name;
                    m.params.push_back (std::move (q));
                }
            }
            // sons d'usine de l'ORB : quatre sons d'usine dans les coins, le point porté par l'orbite
            auto morphPreset = [&m] (const std::string& name, const std::string& cat, std::array<const char*, numCorners> corners, PV extra, float levelOffset = 0.0f)
            {
                auto find = [&m] (const char* n) { for (size_t i = 0; i < m.presets.size(); ++i) if (m.presets[i].name == n) return (int) i; return -1; };
                PV v;
                if (const auto a = find (corners[0]); a >= 0) v = m.presets[(size_t) a].values; // le son du panneau : celui du coin A
                v.push_back ({ "morphon", 1 }); v.push_back ({ "orbon", 1 });
                for (int c = 0; c < numCorners; ++c)
                {
                    const auto idx = find (corners[(size_t) c]);
                    if (idx < 0) continue;
                    const std::string k = std::string ("k") + (char) ('a' + c) + "_";
                    v.push_back ({ k + "on", 1 }); v.push_back ({ k + "src", (float) (idx + 1) });
                    for (const auto sl : morphSlots())
                    {
                        const auto& pd = m.params[(size_t) sl];
                        float val = pd.def;
                        for (const auto& e : m.presets[(size_t) idx].values) if (e.first == pd.id) val = e.second;
                        v.push_back ({ k + pd.id, val });
                    }
                }
                for (auto& x : extra) v.push_back (x);
                // niveau mesuré du mélange : décalage commun au panneau et aux quatre coins
                if (levelOffset != 0.0f)
                    for (auto& e : v) // (pas de liaison structurée ici : MSVC refuse de la lire dans une lambda)
                        if (e.first == "level" || (e.first.size() == 8 && e.first[0] == 'k' && e.first.substr (2) == "_level")) e.second += levelOffset;
                m.presets.push_back ({ name, std::move (v), cat });
            };
            morphPreset ("Nappe qui mute", "Nappes", { "Drone en orbite", "Cath\xC3\xA9" "drale de glace", "Respiration alien", "Drone FM tournant" },
                         { { "o1sync", 3 }, { "o1size", 90 }, { "o2size", 20 }, { "o2ratio", 0.5f } }, -1.4f);
            morphPreset ("Glitch mutant", "Glitch", { "Glitch hitech", "Charleys glitch", "B\xC3\xA9gaiement num\xC3\xA9rique", "Machine qui bugue" },
                         { { "o1sync", 5 }, { "o1size", 85 }, { "o1shape", osChaos } }, 1.6f); // 27 nov. : effets du panneau en morph
            morphPreset ("M\xC3\xA9tal en fusion", "M\xC3\xA9tal", { "Peigne m\xC3\xA9tallique", "Tuyau hurlant", "Cloche de glace", "Sons m\xC3\xA9talliques" },
                         { { "o1shape", osRossler }, { "o1sync", 0 }, { "o1rate", 0.5f }, { "orbphys", 1 } }, -5.5f); // 27 nov. : effets du panneau en morph
            morphPreset ("Squelch mutant", "S\xC3\xA9quences", { "Squelch qui tourne", "Squelch s\xC3\xA9quenc\xC3\xA9", "Gargouille orbitale", "Basse en orbite" },
                         { { "o1sync", 4 }, { "o1size", 85 }, { "o1shape", osRose } }, 5.1f);

            // 27 nov. 2026 (Henrico : effets extrêmes façon Alaskan Bites trop durs à trouver) — réglages ajoutés
            // EN FIN de liste : peigne qui suit la note, phaser, stéréo, gel de la réverbération, ordre des effets
            m.params.push_back (C ("dlcomb", "\xC3\x89" "cho peigne (suit la note)", { "Arr\xC3\xAAt", "Marche" }, 0));
            m.params.push_back (I ("dlcombst", "Peigne hauteur", "dt", -36, 36, 0));
            m.params.push_back (P ("dlcombfine", "Peigne fin", "ct", -100, 100, 0));
            m.params.push_back (C ("phstages", "Phaser \xC3\xA9tages", { "4", "8", "12" }, 1));
            m.params.push_back (P ("phfreq", "Phaser fr\xC3\xA9quence", "Hz", 60, 8000, 700, 0.3f));
            m.params.push_back (P ("phrate", "Phaser vitesse", "Hz", 0.02f, 40, 0.3f, 0.3f));
            m.params.push_back (P ("phdepth", "Phaser profondeur", "%", 0, 100, 70));
            m.params.push_back (P ("phfb", "Phaser r\xC3\xA9troaction", "%", -95, 95, 50));
            m.params.push_back (P ("phmix", "Phaser m\xC3\xA9lange", "%", 0, 100, 0));
            m.params.push_back (P ("stwidth", "St\xC3\xA9r\xC3\xA9o largeur", "%", 0, 200, 100));
            m.params.push_back (P ("sthaas", "St\xC3\xA9r\xC3\xA9o d\xC3\xA9" "calage", "ms", 0, 30, 0));
            m.params.push_back (C ("rvfreeze", "R\xC3\xA9verb gel", { "Non", "Oui" }, 0));
            m.params.push_back (C ("fxorder", "Ordre des effets", { "\xC3\x89" "cho puis r\xC3\xA9verb", "R\xC3\xA9verb puis \xC3\xA9" "cho" }, 0));
            m.sections.push_back ({ "phaser", "Phaser", "fx", { "phstages", "phfreq", "phrate", "phdepth", "phfb", "phmix" }, "global" });
            m.sections.push_back ({ "stereo", "St\xC3\xA9r\xC3\xA9o", "fx", { "stwidth", "sthaas" }, "global" });
            for (auto& sc : m.sections)
            {
                if (sc.id == "echo") { sc.params.push_back ("dlcomb"); sc.params.push_back ("dlcombst"); sc.params.push_back ("dlcombfine"); }
                if (sc.id == "reverb") { sc.params.push_back ("rvfreeze"); sc.params.push_back ("fxorder"); }
            }

            // formes montrées par l'interface : 9 tables × 8 ondes, 64 points chacune
            const auto& wb = bank();
            for (int k = 0; k < wb.size(); ++k)
            {
                std::vector<float> shape (64);
                for (int i = 0; i < 64; ++i) shape[(size_t) i] = wb.read (k, 4, (double) i / 64.0);
                float pk = 1.0e-6f;
                for (auto v : shape) pk = std::max (pk, std::abs (v));
                for (auto& v : shape) v = std::round (v / pk * 1000.0f) / 1000.0f;
                m.waveShapes.push_back (std::move (shape));
            }
            return m;
        }();
        return d;
    }

    /** Bruit blanc → rose → brun (couleur 0..1) : rose par le filtre de Paul Kellet, brun par intégration. */
    struct Noise
    {
        std::uint32_t r = 22222u;
        double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, brown = 0;
        double next (double colour) noexcept
        {
            r = r * 1664525u + 1013904223u;
            const auto w = (double) (r >> 8) / 8388608.0 - 1.0;
            if (colour <= 0.001) return w;
            b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
            b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
            const auto pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11;
            b6 = w * 0.115926;
            if (colour <= 0.5) return w + (pink - w) * colour * 2.0;
            brown = std::clamp (brown * 0.995 + w * 0.06, -1.0, 1.0);
            return pink + (brown * 1.6 - pink) * (colour - 0.5) * 2.0;
        }
    };

    /** État commun à toutes les voix pour un bloc (calculé par l'instrument). */
    struct Shared
    {
        double sr = 48000.0, bpm = 120.0;
        double lane[numLanes][5] {};           // valeur de chaque couloir aux points de commande du bloc (tous les 16 échantillons)
        bool laneOn[numLanes] {};
        double flfo[5] {};                     // LFO du filtre aux points de commande
        double orbCycles = 0.0, orbPh2 = 0.0, orbPh3 = 0.0; // horloge commune de l'orbite (départ « libre » des voix)
        const SampleRuntime* user[2] { nullptr, nullptr };
    };

    /** Position (u, v) dans [-1, 1]² d'une forme d'orbite qui suit une phase (cycles). */
    inline void orbShape (int shape, double ph, double& u, double& v) noexcept
    {
        ph -= std::floor (ph);
        auto poly = [&] (int sides, double inner)
        {
            // polygone (ou étoile si inner < 1) parcouru sommet par sommet
            const int nv = inner < 1.0 ? sides * 2 : sides;
            const auto pos = ph * nv;
            const auto k = (int) pos;
            const auto f = pos - (double) k;
            auto vert = [&] (int i, double& x, double& y)
            {
                const auto r = (inner < 1.0 && (i % 2)) ? inner : 1.0;
                const auto a = (double) i / nv;
                x = r * fastSin2Pi (a + 0.25); y = r * fastSin2Pi (a);
            };
            double x0, y0, x1, y1;
            vert (k, x0, y0); vert (k + 1, x1, y1);
            u = x0 + (x1 - x0) * f; v = y0 + (y1 - y0) * f;
        };
        switch (shape)
        {
            case osEllipse: u = fastSin2Pi (ph + 0.25); v = 0.45 * fastSin2Pi (ph); break;
            case osEight: u = fastSin2Pi (ph); v = fastSin2Pi (2.0 * ph); break;
            case osLissajous: u = fastSin2Pi (3.0 * ph); v = fastSin2Pi (2.0 * ph + 0.25); break;
            case osStar: poly (5, 0.4); break;
            case osSpiral: { const auto r = 0.15 + 0.85 * ph; u = r * fastSin2Pi (3.0 * ph + 0.25); v = r * fastSin2Pi (3.0 * ph); break; }
            case osRose: { const auto r = fastSin2Pi (3.0 * ph * 0.5 + 0.25); u = r * fastSin2Pi (ph * 0.5 + 0.25); v = r * fastSin2Pi (ph * 0.5); break; } // rose à 3 pétales
            case osTriangle: poly (3, 1.0); break;
            case osSquare: poly (4, 1.0); break;
            default: u = fastSin2Pi (ph + 0.25); v = fastSin2Pi (ph); break;
        }
    }

    /** L'ORB : orbite autour du point X / Y (forme, sens, taille, rotation, chaos), sous-orbite en sens opposé,
        orbe 3, physique. Un état par voix (et un pour l'instrument quand rien ne joue). */
    struct OrbState
    {
        double cyc = 0.0, ph2 = 0.0, ph3 = 0.0;           // phases (cyc : sans repli, pour les formes qui ne se répètent pas)
        double ax = 0.1, ay = 0.0, az = 20.0;             // attracteurs de Lorenz et Rössler
        double px = 0.0, py = 0.0, vx = 0.0, vy = 0.0;    // point (élastique)
        double qx = 0.0, qy = 0.0, wx = 0.0, wy = 0.0;    // orbe 3 (gravité)
        bool init = false;
        std::uint64_t seed = 1;

        void start (bool fromNote, const float* p, const Shared* sh) noexcept
        {
            cyc = fromNote || sh == nullptr ? 0.0 : sh->orbCycles;
            ph2 = fromNote || sh == nullptr ? p[o2Phase] / 360.0 : sh->orbPh2;
            ph3 = fromNote || sh == nullptr ? p[o3Phase] / 360.0 : sh->orbPh3;
            if (fromNote) { ax = 0.1; ay = 0.0; az = 20.0; }
        }

        /** Avance de h secondes ; out : ORB X / Y (le point), orbite X / Y, orbe 3 X / Y, distance et angle point-3. */
        void step (const float* p, double h, std::array<double, numOrbOuts>& out) noexcept
        {
            const bool on = p[orbOn] > 0.5f;
            const auto cx = std::clamp (p[morphX] / 100.0, -1.0, 1.0), cy = std::clamp (p[morphY] / 100.0, -1.0, 1.0);
            double ox = 0.0, oy = 0.0, tx = cx, ty = cy, t3x = cx, t3y = cy;
            const auto shape = std::clamp ((int) p[o1Shape], 0, numOrbShapes - 1);
            if (on)
            {
                const auto dir = (int) p[o1Dir];
                const auto hz = std::clamp ((double) p[o1Rate], 0.0, 200.0);
                const auto adv = hz * h * (dir == 1 ? -1.0 : 1.0);
                cyc += adv;
                if (std::abs (cyc) > 1.0e7) cyc = 0.0;
                double u = 0.0, v = 0.0;
                if (shape == osLorenz || shape == osRossler)
                {
                    // attracteurs : intégrés au pas de la vitesse (un « tour » ≈ une période)
                    const auto dtA = std::min (0.02, std::abs (adv) * (shape == osLorenz ? 0.8 : 6.0));
                    for (int k = 0; k < 2; ++k)
                    {
                        const auto e = dtA * 0.5;
                        double dx, dy, dz;
                        if (shape == osLorenz) { dx = 10.0 * (ay - ax); dy = ax * (28.0 - az) - ay; dz = ax * ay - 8.0 / 3.0 * az; }
                        else { dx = -ay - az; dy = ax + 0.2 * ay; dz = 0.2 + az * (ax - 5.7); }
                        ax += dx * e; ay += dy * e; az += dz * e;
                    }
                    if (! std::isfinite (ax + ay + az) || std::abs (ax) > 1.0e3) { ax = 0.1; ay = 0.0; az = 20.0; }
                    if (shape == osLorenz) { u = ax / 20.0; v = (az - 25.0) / 22.0; }
                    else { u = ax / 11.0; v = ay / 11.0; }
                }
                else if (shape == osChaos) { u = smoothNoise (cyc * 3.0, seed * 31 + 7); v = smoothNoise (cyc * 3.0, seed * 31 + 19); }
                else
                {
                    auto ph = cyc;
                    if (dir == 2) { const auto pp = 2.0 * (ph * 0.5 - std::floor (ph * 0.5)); ph = pp < 1.0 ? pp : 2.0 - pp; } // aller-retour
                    orbShape (shape, ph, u, v);
                }
                const auto ch = p[o1Chaos] / 100.0;
                if (ch > 0.0) { u += ch * 0.6 * smoothNoise (cyc * 5.0, seed * 13 + 3); v += ch * 0.6 * smoothNoise (cyc * 5.0, seed * 13 + 11); }
                const auto s1 = p[o1Size] / 100.0, rot = p[o1Rot] / 360.0;
                const auto cr = fastSin2Pi (rot + 0.25), sn = fastSin2Pi (rot);
                ox = s1 * (u * cr - v * sn); oy = s1 * (u * sn + v * cr);
                // sous-orbite (autour de l'orbite, sens opposé par défaut) et orbe 3 (autour du point, à l'envers)
                ph2 += adv * std::clamp ((double) p[o2Ratio], 0.0, 32.0) * (p[o2Dir] > 0.5f ? 1.0 : -1.0); ph2 -= std::floor (ph2);
                ph3 += adv * std::clamp ((double) p[o3Ratio], 0.0, 32.0) * (p[o3Dir] > 0.5f ? 1.0 : -1.0); ph3 -= std::floor (ph3);
                orbShape (std::min ((int) p[o2Shape], (int) osSquare), ph2, u, v);
                tx = cx + ox + p[o2Size] / 100.0 * u; ty = cy + oy + p[o2Size] / 100.0 * v;
                orbShape (std::min ((int) p[o3Shape], (int) osSquare), ph3, u, v);
                t3x = cx + p[o3Size] / 100.0 * u; t3y = cy + p[o3Size] / 100.0 * v;
            }
            const auto phys = on ? std::clamp ((int) p[orbPhys], 0, 2) : 0;
            if (phys == 0 || ! init) { px = tx; py = ty; qx = t3x; qy = t3y; vx = vy = wx = wy = 0.0; init = true; }
            else
            {
                // ressort amorti vers l'orbite (élastique) ; l'orbe 3 attiré par le point, rebonds sur les bords (gravité)
                const auto sp = p[orbSpring] / 100.0;
                const auto w0 = kTwoPi * (60.0 * (1.0 - 0.95 * sp));
                const auto k = w0 * w0, c = 2.0 * (0.05 + 0.9 * p[orbDamp] / 100.0) * w0;
                vx += (k * (tx - px) - c * vx) * h; vy += (k * (ty - py) - c * vy) * h;
                px += vx * h; py += vy * h;
                if (phys == 2)
                {
                    const auto dx = px - qx, dy = py - qy, d2 = dx * dx + dy * dy + 0.02;
                    const auto g = (30.0 + 600.0 * sp) / (d2 * std::sqrt (d2));
                    const auto k3 = k * 0.15;
                    wx += (g * dx + k3 * (t3x - qx) - c * 0.5 * wx) * h; wy += (g * dy + k3 * (t3y - qy) - c * 0.5 * wy) * h;
                    const auto vm = std::hypot (wx, wy);
                    if (vm > 400.0) { wx *= 400.0 / vm; wy *= 400.0 / vm; }
                    qx += wx * h; qy += wy * h;
                    const auto rest = p[orbBounce] / 100.0;
                    if (qx > 1.0) { qx = 1.0; wx = -std::abs (wx) * rest; } if (qx < -1.0) { qx = -1.0; wx = std::abs (wx) * rest; }
                    if (qy > 1.0) { qy = 1.0; wy = -std::abs (wy) * rest; } if (qy < -1.0) { qy = -1.0; wy = std::abs (wy) * rest; }
                }
                else { qx = t3x; qy = t3y; }
                if (! std::isfinite (px + py + qx + qy + vx + vy + wx + wy)) { px = tx; py = ty; qx = t3x; qy = t3y; vx = vy = wx = wy = 0.0; }
            }
            auto cl = [] (double x) { return std::clamp (x, -1.0, 1.0); };
            out = { cl (px), cl (py), cl (cx + ox), cl (cy + oy), cl (qx), cl (qy),
                    std::clamp (std::hypot (px - qx, py - qy) * 0.5, 0.0, 1.0), std::atan2 (qy - py, qx - px) / kPi };
        }
    };

    /** Effets propres à Croc Glacé (ceux de GOLEM sont trop sages pour le glitch) : écho de 0,05 ms à 2 s, au glissé
        réglable (0 : le temps saute net), rétroaction saturée qui peut s'emballer ; flanger très court à forte
        rétroaction (laser, métal) ; distorsion lampe / écrêtage / bitcrush / repli. Mémoire allouée dans prepare(). */
    struct Echo
    {
        std::vector<float> bl, br;
        int w = 0, size = 0;
        double cur = 1000.0, lpL = 0.0, lpR = 0.0;
        void prepare (double sr) { size = (int) (sr * 2.2) + 8; bl.assign ((size_t) size, 0.0f); br.assign ((size_t) size, 0.0f); w = 0; }
        void reset() noexcept { std::fill (bl.begin(), bl.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f); lpL = lpR = 0.0; }
        double read (const std::vector<float>& b, double d) const noexcept
        {
            auto rp = (double) w - d;
            while (rp < 0.0) rp += size;
            const auto i0 = (int) rp;
            const auto f = rp - i0;
            const auto i1 = i0 + 1 >= size ? 0 : i0 + 1;
            return (double) b[(size_t) i0] * (1.0 - f) + (double) b[(size_t) i1] * f;
        }
        void process (float* L, float* R, int n, double sr, double timeMs, bool ping, double fb, double glideMs, double tone, double mix) noexcept
        {
            if (size < 16) return;
            const auto target = std::clamp (timeMs * 0.001 * sr, 1.0, (double) size - 4.0);
            const auto g = glideMs < 0.05 ? 1.0 : 1.0 - std::exp (-1.0 / (glideMs * 0.001 * sr));
            const auto tc = 1.0 - 0.93 * std::clamp (tone, 0.0, 1.0);
            for (int i = 0; i < n; ++i)
            {
                cur += (target - cur) * g;
                const auto dl = read (bl, cur), dr = read (br, cur);
                lpL += (dl - lpL) * tc; lpR += (dr - lpR) * tc;
                const auto inL = (double) L[i], inR = (double) R[i];
                double wl, wr;
                if (ping) { wl = 0.5 * (inL + inR) + fb * lpR; wr = fb * lpL; }
                else { wl = inL + fb * lpL; wr = inR + fb * lpR; }
                bl[(size_t) w] = (float) std::tanh (wl); // saturée : au-delà de 100 % elle s'emballe sans exploser
                br[(size_t) w] = (float) std::tanh (wr);
                if (++w >= size) w = 0;
                L[i] = (float) (inL + (lpL - inL) * mix * (ping ? 1.0 : 1.0));
                R[i] = (float) (inR + (lpR - inR) * mix);
            }
        }
    };
    struct Flanger
    {
        std::vector<float> bl, br;
        int w = 0, size = 0;
        double ph = 0.0, fl = 0.0, fr = 0.0;
        void prepare (double sr) { size = (int) (sr * 0.03) + 8; bl.assign ((size_t) size, 0.0f); br.assign ((size_t) size, 0.0f); w = 0; }
        void reset() noexcept { std::fill (bl.begin(), bl.end(), 0.0f); std::fill (br.begin(), br.end(), 0.0f); fl = fr = 0.0; }
        double read (const std::vector<float>& b, double d) const noexcept
        {
            auto rp = (double) w - d;
            while (rp < 0.0) rp += size;
            const auto i0 = (int) rp;
            const auto f = rp - i0;
            const auto i1 = i0 + 1 >= size ? 0 : i0 + 1;
            return (double) b[(size_t) i0] * (1.0 - f) + (double) b[(size_t) i1] * f;
        }
        void process (float* L, float* R, int n, double sr, double timeMs, double rate, double depth, double fb, double mix) noexcept
        {
            if (size < 16) return;
            const auto base = std::clamp (timeMs * 0.001 * sr, 1.0, (double) size * 0.45);
            for (int i = 0; i < n; ++i)
            {
                ph += rate / sr; if (ph >= 1.0) ph -= 1.0;
                const auto m = fastSin2Pi (ph);
                const auto dL = std::clamp (base * (1.0 + 0.95 * depth * m), 1.0, (double) size - 4.0);
                const auto dR = std::clamp (base * (1.0 - 0.95 * depth * m), 1.0, (double) size - 4.0);
                fl = read (bl, dL); fr = read (br, dR);
                const auto inL = (double) L[i], inR = (double) R[i];
                bl[(size_t) w] = (float) std::tanh (inL + fb * fl);
                br[(size_t) w] = (float) std::tanh (inR + fb * fr);
                if (++w >= size) w = 0;
                L[i] = (float) (inL * (1.0 - 0.5 * mix) + fl * mix);
                R[i] = (float) (inR * (1.0 - 0.5 * mix) + fr * mix);
            }
        }
    };
    struct Distortion
    {
        double hold[2] { 0.0, 0.0 };
        int count[2] { 0, 0 };
        void process (float* L, float* R, int n, int mode, double drive, double mix) noexcept
        {
            drive = std::clamp (drive, 0.0, 1.0);
            const auto g = 1.0 + 24.0 * drive * drive;
            const auto comp = 1.0 - 0.45 * drive;
            const auto levels = std::pow (2.0, 12.0 - 10.0 * drive);
            const auto holdN = 1 + (int) (drive * drive * 20.0);
            float* ch[2] { L, R };
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const auto x = (double) ch[c][i];
                    double y;
                    switch (mode)
                    {
                        case 1: y = std::clamp (x * g, -1.0, 1.0) * 0.85; break;                          // écrêtage dur
                        case 2:                                                                             // bitcrush : bits et échantillonnage
                            if (--count[c] <= 0) { count[c] = holdN; hold[c] = std::round (x * levels) / levels; }
                            y = hold[c]; break;
                        case 3: y = fastSin2Pi (x * g * 0.25); break;                                     // repli
                        default: y = std::tanh (x * g + 0.2) - std::tanh (0.2); break;                     // lampe (asymétrique)
                    }
                    ch[c][i] = (float) (x + (y * (mode == 2 ? 1.0 : comp) - x) * mix);
                }
        }
    };

    /** Phaser (27 nov. 2026) : 4, 8 ou 12 cellules passe-tout du premier ordre, fréquence balayée par un LFO
        (±3 octaves à 100 % de profondeur, droite décalée d'un quart de tour), rétroaction saturée jusqu'à ±95 %.
        Coefficients recalculés tous les 16 échantillons. Aucune allocation. */
    struct Phaser
    {
        static constexpr int maxStages = 12;
        double z[2][maxStages] {}, fbs[2] { 0.0, 0.0 }, a[2] { 0.0, 0.0 }, ph = 0.0;
        int cnt = 0;
        void reset() noexcept { for (auto& c : z) for (auto& v : c) v = 0.0; fbs[0] = fbs[1] = 0.0; cnt = 0; }
        void process (float* L, float* R, int n, double sr, int stages, double freq, double rate, double depth, double fb, double mix) noexcept
        {
            stages = std::clamp (stages, 1, maxStages);
            float* ch[2] { L, R };
            for (int i = 0; i < n; ++i)
            {
                if (cnt-- <= 0)
                {
                    cnt = 15;
                    for (int c = 0; c < 2; ++c)
                    {
                        const auto m = fastSin2Pi (ph + 0.25 * c);
                        const auto f = std::clamp (freq * fastExp2 (3.0 * depth * m), 20.0, 0.45 * sr);
                        const auto t = std::tan (kPi * f / sr);
                        a[c] = (t - 1.0) / (t + 1.0);
                    }
                }
                ph += rate / sr; if (ph >= 1.0) ph -= 1.0;
                for (int c = 0; c < 2; ++c)
                {
                    const auto x = (double) ch[c][i];
                    auto y = x + fb * fbs[c];
                    for (int k = 0; k < stages; ++k)
                    {
                        const auto o = a[c] * y + z[c][k];
                        z[c][k] = y - a[c] * o;
                        y = o;
                    }
                    fbs[c] = std::tanh (y);
                    ch[c][i] = (float) (x * (1.0 - 0.5 * mix) + y * 0.5 * mix);
                }
            }
        }
    };

    /** Stéréo (27 nov. 2026) : largeur milieu / côtés de 0 (mono) à 200 %, et décalage de Haas (la droite retardée
        de 0 à 30 ms) pour écarter un son mono. Mémoire allouée dans prepare(). */
    struct Stereo
    {
        std::vector<float> buf;
        int w = 0, size = 0;
        void prepare (double sr) { size = (int) (sr * 0.032) + 4; buf.assign ((size_t) size, 0.0f); w = 0; }
        void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); }
        void process (float* L, float* R, int n, double sr, double width, double haasMs) noexcept
        {
            const auto d = std::clamp ((int) (haasMs * 0.001 * sr), 0, size - 2);
            for (int i = 0; i < n; ++i)
            {
                double l = L[i], r = R[i];
                if (d > 0 && size > 0)
                {
                    buf[(size_t) w] = (float) r;
                    auto rp = w - d; if (rp < 0) rp += size;
                    r = buf[(size_t) rp];
                    if (++w >= size) w = 0;
                }
                const auto mid = 0.5 * (l + r), side = 0.5 * (l - r) * width;
                L[i] = (float) (mid + side);
                R[i] = (float) (mid - side);
            }
        }
    };

    /** Échelle des destinations directes (valeur ajoutée pour une modulation de 1, soit 100 %). */
    inline double destScale (int d) noexcept
    {
        switch (d)
        {
            case dPitch: case dPitchC: case dPitchM: return 48.0; // demi-tons : ±4 octaves (sauts du glitch)
            case dCut: return 4.0;                    // octaves
            case dRes: return 0.6;
            default: return 1.0;
        }
    }
}

/** Une voix : porteuse et modulateur à tables, Bite, filtre à deux étages, deux enveloppes, son ORB. */
class CrocVoice
{
public:
    void prepare (double s)
    {
        sr = s; ea.prepare (s); ef.prepare (s); em.prepare (s);
        const auto len = (size_t) (s / 15.0) + 8;
        for (auto& c : comb) c.buf.assign (len, 0.0f);
        reset();
    }
    void reset() noexcept
    {
        pc = pm = psub = pwmPh = 0.0; held = 0.0; holdCount = 0;
        for (auto& l : ladder) l.reset();
        for (auto& f : svf) f.reset();
        for (auto& f : svfB) f.reset();
        for (auto& c : comb) { std::fill (c.buf.begin(), c.buf.end(), 0.0f); c.w = 0; c.lp = 0.0; }
        ea.reset(); ef.reset(); em.reset(); dc.reset(); active = false; pending = false;
        const auto sd = orb.seed; orb = {}; orb.seed = sd;
        gPrev = 0.0; glPrev = grPrev = 1.0;
    }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }
    void seed (std::uint32_t s) noexcept { rng = s; orb.seed = s; nC.r = s * 3u + 1u; nM.r = s * 7u + 5u; nX.r = s * 11u + 9u; }
    void setShared (const croc::Shared* s) noexcept { sh = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true; releaseAfterStart = false;
        rng = rng * 1664525u + 1013904223u;
        pan = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    void release() noexcept { ea.gate (false); ef.gate (false); em.gate (false); if (pending) releaseAfterStart = true; }
    bool isActive() const noexcept { return active; }
    void modSources (double* env, double* /*lfo*/) const noexcept { env[0] = lastA; env[1] = lastF; }

    /** Sorties de l'ORB de cette voix (dernier point de commande). */
    std::array<double, croc::numOrbOuts> orbOut {};

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace croc;
        const auto& wb = bank();
        if (pending)
        {
            pending = false;
            const bool lg = legatoPending && ea.isActive();
            const bool glideOn = p[glide] > 0.5f && from >= 0 && ((int) p[vMode] != 2 || lg);
            pitch.jump ((double) (glideOn ? from : target));
            pitch.to ((double) target);
            if (! lg)
            {
                if (! ea.isActive()) orb.init = false; // départ depuis le silence : la physique repart de l'orbite
                ea.gate (true); ef.gate (true); em.gate (true);
                orb.start (p[orbRetrig] > 0.5f, p, sh); // à chaque note : orbite au départ ; libre : sur l'horloge commune
            }
            if (releaseAfterStart) { releaseAfterStart = false; ea.gate (false); ef.gate (false); em.gate (false); }
        }
        em.set (p[modA], p[modD], p[modS] / 100.0, p[modR]);
        const auto modFmV = p[modFm] / 100.0, modPitchV = (double) p[modPitch];
        const auto pdC0 = p[cPd] / 100.0, pdM0 = p[mPd] / 100.0;
        const auto noise0 = p[noiseLvl] / 100.0, noiseColV = std::clamp (p[noiseCol] / 100.0, 0.0, 1.0);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-16.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto baseGain = dbToGain (p[level] + 18.0f) * velocityGain (vel, p[velAmp] / 100.0);
        const auto basePan = spread != 0.0 ? spread * std::max (0.6, p[spreadAmt] / 100.0) : pan * p[spreadAmt] / 100.0;
        const auto det = spread * p[uniDetune] / 100.0;
        const auto offC = 12.0 * std::round (p[cOct]) + std::round (p[cSemi]) + p[cFine] / 100.0 + det;
        const auto offM = 12.0 * std::round (p[mOct]) + std::round (p[mSemi]) + p[mFine] / 100.0 - det;
        const auto ctab = std::clamp ((int) p[cTab], 0, userTable), mta = std::clamp ((int) p[mTabA], 0, userTable);
        const auto mtb = std::clamp ((int) p[mTabB], 0, userTable + 1) - 1; // -1 : aucune
        const auto ftype = std::clamp ((int) p[fType], 0, 7);
        const bool twoStage = p[f2Stage] > 0.5f;
        const auto ktAmt = p[fKey] / 100.0, envOct = p[fEnv] / 100.0 * 7.0, velOct = p[fVel] / 100.0 * (vel - 0.6) * 4.0;
        const auto flAmtOct = p[flAmt] / 100.0 * 3.0;
        const auto mAbV = std::clamp (p[mAb] / 100.0, 0.0, 1.0);
        const auto shapeV = p[mShape] / 100.0, mFoldV = p[mFold] / 100.0, mCrushV = p[mCrush] / 100.0;
        const auto subV = p[subLvl] / 100.0, softV = p[biteSoft] / 100.0;
        const auto softG = 1.0 + 9.0 * softV * softV, softN = softV > 0.0 ? (1.0 - 0.45 * softV) / std::tanh (softG) : 1.0;
        struct Route { int src, dst; double amt; };
        std::array<Route, numRoutes> routes {};
        bool anyRoute = false;
        for (int r = 0; r < numRoutes; ++r)
        {
            const auto* q = p + routeBase + r * routeStride;
            routes[(size_t) r] = { std::clamp ((int) q[0], 0, numOrbOuts - 1), std::clamp ((int) q[1], 0, numDests - 1), q[2] / 100.0 };
            if (routes[(size_t) r].dst != dNone && routes[(size_t) r].amt != 0.0) anyRoute = true;
        }
        std::array<int, numLanes> lDst {};
        std::array<double, numLanes> lAmt {};
        for (int l = 0; l < numLanes; ++l)
        {
            const auto* q = p + laneBase + l * laneStride;
            lDst[(size_t) l] = sh != nullptr && sh->laneOn[l] ? std::clamp ((int) q[lnDst], 0, numDests - 1) : dNone;
            lAmt[(size_t) l] = q[lnAmt] / 100.0;
        }
        const SampleRuntime* uC = sh != nullptr ? sh->user[0] : nullptr;
        const SampleRuntime* uM = sh != nullptr ? sh->user[1] : nullptr;

        double a = lastA, ff = lastF;
        int i = 0;
        while (i < n)
        {
            const int len = std::min (16, n - i);
            const int sub = std::min (4, i >> 4);
            const auto h = (double) len / sr;

            // --- l'ORB de la voix, puis les modulations directes : routes et couloirs
            orb.step (p, h, orbOut);
            std::array<double, numDests> md {};
            if (anyRoute)
                for (const auto& r : routes) if (r.dst != dNone) md[(size_t) r.dst] += r.amt * orbOut[(size_t) r.src];
            if (sh != nullptr)
                for (int l = 0; l < numLanes; ++l) if (lDst[(size_t) l] != dNone) md[(size_t) lDst[(size_t) l]] += lAmt[(size_t) l] * sh->lane[l][sub];
            md[dNone] = 0.0;
            if (i == 0)
            {
                // déclins modulés (couloirs, routes) : ±3 octaves de durée, posés une fois par bloc
                ea.set (p[aA], p[aD] * fastExp2 (std::clamp (md[dDecA], -1.0, 1.0) * 3.0), p[aS] / 100.0, p[aR]);
                ef.set (p[fA], p[fD] * fastExp2 (std::clamp (md[dDecF], -1.0, 1.0) * 3.0), p[fS] / 100.0, p[fR]);
            }
            const auto emV = emLast;

            // --- hauteurs, tables, filtre (une fois par point de commande)
            const auto note = pitch.next (glideCoef) + p[tune] + md[dPitch] * destScale (dPitch) + modPitchV * emV;
            const auto dtc = std::clamp (noteHz (note + offC + md[dPitchC] * destScale (dPitchC)) / sr, 1.0e-7, 0.45);
            const auto dtm = std::clamp (noteHz (note + offM + md[dPitchM] * destScale (dPitchM)) / sr, 1.0e-7, 0.45);
            const auto fmIdx = std::clamp (p[biteFm] / 100.0 + md[dFm] + modFmV * emV, 0.0, 1.5);
            const auto fmAmt = fmIdx * fmIdx * 2.0;
            pwmPh += p[cPwmRate] * h; pwmPh -= std::floor (pwmPh);
            const auto pwmV = std::clamp (p[cPwm] / 100.0 + md[dPwm], 0.0, 1.0);
            // PWM et distorsion de phase : le point de bascule de la phase se déplace (la sinusoïde se tord vers la scie)
            const auto pdC = std::clamp (pdC0 + md[dPdC], 0.0, 1.0), pdM = std::clamp (pdM0 + md[dPdM], 0.0, 1.0);
            const auto width = std::max (0.02, (0.5 - 0.45 * pwmV * (0.5 + 0.5 * fastSin2Pi (pwmPh))) * (1.0 - 0.96 * pdC));
            const auto widthM = std::max (0.02, 0.5 * (1.0 - 0.96 * pdM));
            const auto noiseV = std::clamp (noise0 + md[dNoise], 0.0, 1.0);
            const auto shapeVm = std::clamp (shapeV + md[dShapeM], 0.0, 1.0);
            const auto slopeC = 0.5 / std::min (width, 1.0 - width) * (1.0 + 6.0 * fmAmt * std::max (1.0, dtm / dtc));
            // Le dernier niveau d'octave de la banque ne porte aucune harmonique : avec une FM forte et
            // un rapport élevé, la pente estimée y menait et la porteuse se taisait. On garde au moins
            // le fondamental (niveau numLevels − 2, sans fondu vers le niveau vide).
            const auto safeMip = [] (double dt) noexcept
            {
                auto m = WaveBank::mipFor (std::min (0.45, dt));
                if (m.level >= WaveBank::numLevels - 2) m = { WaveBank::numLevels - 2, 0.0f };
                return m;
            };
            const auto mipC = safeMip (dtc * slopeC);
            const auto mipM = safeMip (dtm * (1.0 + 2.0 * shapeVm) * 0.5 / std::min (widthM, 1.0 - widthM));
            const auto mipS = WaveBank::mipFor (0.5 * dtc);
            const auto posC = std::clamp (p[cPos] / 100.0 + md[dPosC], 0.0, 1.0), posM = std::clamp (p[mPos] / 100.0 + md[dPosM], 0.0, 1.0);
            const auto mixV = std::clamp (p[oscMix] / 100.0 + md[dMix], 0.0, 1.0);
            const auto foldV = std::clamp (p[biteFold] / 100.0 + md[dFold], 0.0, 1.0);
            const auto crushV = std::clamp (p[biteCrush] / 100.0 + md[dCrush], 0.0, 1.0);
            const auto foldG = 1.0 + 7.0 * foldV;
            const auto bitsQ = crushV > 0.0 ? std::pow (2.0, 15.0 - 13.0 * crushV) : 0.0;
            const auto holdN = crushV > 0.0 ? 1 + (int) (crushV * crushV * 24.0) : 1;
            const auto mBitsQ = mCrushV > 0.0 ? std::pow (2.0, 12.0 - 10.0 * mCrushV) : 0.0;
            const auto mFoldG = 1.0 + 6.0 * mFoldV;
            const auto kt = ktAmt * (note - 60.0) / 12.0;
            const auto flV = sh != nullptr ? sh->flfo[sub] : 0.0;
            const auto fc = std::clamp ((double) p[cutoff] * fastExp2 (kt + envOct * ff + velOct + flAmtOct * flV + md[dCut] * destScale (dCut)), 20.0, 0.45 * sr);
            const auto res = std::clamp (p[reso] / 100.0 + md[dRes] * destScale (dRes), 0.0, 1.0);
            const auto drv = std::clamp (p[fDrive] / 100.0 + md[dDrive], 0.0, 1.0);
            const auto drive = 1.0 + 7.0 * drv * drv;
            setFilter (ftype, fc, res);
            // gain et panoramique : cible à la fin du point de commande, interpolés à l'échantillon
            const auto gTarget = baseGain * std::clamp (1.0 + md[dVol], 0.0, 1.0);
            const auto pn = std::clamp (basePan + md[dPan], -1.0, 1.0);
            const auto glT = 1.0 - std::max (0.0, pn) * 0.7, grT = 1.0 + std::min (0.0, pn) * 0.7;
            const auto dg = (gTarget - gPrev) / len, dgl = (glT - glPrev) / len, dgr = (grT - grPrev) / len;

            for (int k = 0; k < len; ++k, ++i)
            {
                a = ea.next(); ff = ef.next(); emLast = em.next();
                // modulateur : distorsion de phase, Shape (courbure de phase), onde A (+ onde B), Fold, Crush
                auto rpm = pdM > 0.0 ? (pm < widthM ? pm * 0.5 / widthM : 0.5 + (pm - widthM) * 0.5 / (1.0 - widthM)) : pm;
                if (shapeVm > 0.0) rpm = rpm + shapeVm * (rpm * rpm * rpm - rpm);
                double ym = mta == noiseTable ? nM.next (posM) : readTable (wb, uM, mta, posM, mipM, rpm);
                if (mtb >= 0) ym += ((mtb == noiseTable ? nM.next (posM) : readTable (wb, uM, mtb, posM, mipM, rpm)) - ym) * mAbV;
                if (mFoldV > 0.0) ym = fastSin2Pi (ym * mFoldG * 0.25);
                if (mCrushV > 0.0) ym = std::round (ym * mBitsQ) / mBitsQ;
                pm += dtm; if (pm >= 1.0) pm -= 1.0;
                // porteuse : PWM par la phase, FM par le modulateur
                auto rp = pc < width ? pc * 0.5 / width : 0.5 + (pc - width) * 0.5 / (1.0 - width);
                if (fmAmt > 0.0) { rp += fmAmt * ym; rp -= std::floor (rp); if (rp >= 1.0) rp = 0.0; } // (−1e-17 replié donne 1,0)
                const auto yc = ctab == noiseTable ? nC.next (posC) : readTable (wb, uC, ctab, posC, mipC, rp);
                pc += dtc; if (pc >= 1.0) pc -= 1.0;
                double x = yc + (ym - yc) * mixV;
                if (subV > 0.0) { x += subV * (double) wb.read (7, mipS, psub); psub += 0.5 * dtc; if (psub >= 1.0) psub -= 1.0; } // sinus une octave plus bas
                if (noiseV > 0.0) x += noiseV * nX.next (noiseColV); // générateur de bruit (charleys, souffles, peignes)
                // Bite : Fold, Crush (bits et échantillonnage), Soft
                if (foldV > 0.0) x = fastSin2Pi (x * foldG * 0.25);
                if (crushV > 0.0)
                {
                    if (--holdCount <= 0) { holdCount = holdN; held = std::round (x * bitsQ) / bitsQ; }
                    x = held;
                }
                if (softV > 0.0) x = std::tanh (x * softG) * softN;
                // filtre (un ou deux étages)
                double y = filt (0, ftype, x * 0.42, fc, res, drive);
                if (twoStage) y = filt (1, ftype, y, fc, res * 0.7, 1.0 + 0.5 * (drive - 1.0));
                gPrev += dg; glPrev += dgl; grPrev += dgr;
                const auto out = dc.process (y) * a * gPrev;
                L[i] += (float) (out * glPrev);
                R[i] += (float) (out * grPrev);
            }
            gPrev = gTarget; glPrev = glT; grPrev = grT;
        }
        lastA = a; lastF = ff;
        if (! ea.isActive()) active = false;
    }

private:
    struct Comb { std::vector<float> buf; int w = 0; double lp = 0.0; };

    /** Lecture d'une table à une position (fondu entre deux ondes voisines) ; table perso si choisie. */
    static double readTable (const WaveBank& wb, const croc::SampleRuntime* user, int table, double pos, WaveBank::Mip m, double ph) noexcept
    {
        if (table == croc::userTable)
        {
            if (user == nullptr || user->frames <= 0) table = 0; // pas de WAV chargé : la première table
            else
            {
                if (user->frames == 1) return (double) user->table.read (0, m, ph);
                const auto fp = pos * (user->frames - 1);
                const auto k0 = std::min ((int) fp, user->frames - 2);
                const auto f = fp - (double) k0;
                const auto y0 = (double) user->table.read (k0, m, ph);
                return f < 1.0e-4 ? y0 : y0 + ((double) user->table.read (k0 + 1, m, ph) - y0) * f;
            }
        }
        if (table >= croc::golemFirst && table < croc::golemFirst + croc::golemTables)
        {
            // tables de Glutamake, Good Doctor et Good Doctor 2 (12 ondes), lues dans leurs banques
            const auto t = table - croc::golemFirst;
            const auto fp = pos * (gd2::frames - 1);
            const auto k0 = std::min ((int) fp, gd2::frames - 2);
            const auto f = fp - (double) k0;
            const auto y0 = (double) gd2::readBuiltin (t, k0, m, ph);
            return f < 1.0e-4 ? y0 : y0 + ((double) gd2::readBuiltin (t, k0 + 1, m, ph) - y0) * f;
        }
        if (table >= croc::noiseTable) table = 0;
        const auto fp = pos * (croc::frames - 1);
        const auto k0 = std::min ((int) fp, croc::frames - 2);
        const auto f = fp - (double) k0;
        const auto base = table * croc::frames + k0;
        const auto y0 = (double) wb.read (base, m, ph);
        return f < 1.0e-4 ? y0 : y0 + ((double) wb.read (base + 1, m, ph) - y0) * f;
    }

    /** Coefficients des filtres d'état (coupe-bande, formant), une fois par point de commande. */
    void setFilter (int type, double fc, double res) noexcept
    {
        if (type == 4) cA.set (fc, res * 0.85, sr);
        else if (type == 7)
        {
            static constexpr double F1[5] { 325, 450, 300, 400, 800 }, F2[5] { 700, 800, 2300, 1600, 1150 };
            const auto pos = std::clamp (std::log2 (std::max (60.0, fc) / 60.0) / std::log2 (18000.0 / 60.0) * 4.0, 0.0, 4.0);
            const auto k = std::min (3, (int) pos);
            const auto fr = pos - (double) k;
            const auto q = 0.5 + 0.45 * res;
            cA.set (std::min (F1[k] + (F1[k + 1] - F1[k]) * fr, 0.45 * sr), q, sr);
            cB.set (std::min (F2[k] + (F2[k + 1] - F2[k]) * fr, 0.45 * sr), q, sr);
        }
    }

    double combProcess (Comb& c, double x, double fc, double g) noexcept
    {
        const auto size = (int) c.buf.size();
        if (size < 8) return x;
        const auto D = std::clamp (sr / fc, 2.0, (double) size - 3.0);
        auto rp = (double) c.w - D;
        if (rp < 0.0) rp += size;
        const auto i0 = (int) rp;
        const auto f = rp - i0;
        const auto i1 = i0 + 1 >= size ? 0 : i0 + 1;
        const auto dl = (double) c.buf[(size_t) i0] * (1.0 - f) + (double) c.buf[(size_t) i1] * f;
        c.lp += (dl - c.lp) * 0.6; // amortissement dans la boucle (le peigne ne siffle pas)
        const auto y = x + g * c.lp;
        c.buf[(size_t) c.w] = (float) softClip (y);
        if (++c.w >= size) c.w = 0;
        return y * (1.0 - 0.55 * std::abs (g));
    }

    double filt (int st, int type, double x, double fc, double res, double drive) noexcept
    {
        switch (type)
        {
            // échelle : le drive pousse l'étage d'entrée saturé (grain, squelch) ; niveau ramené d'autant
            case 0: return ladder[(size_t) st].process (x, fc, res, sr, Ladder::lp24, drive) / std::sqrt (drive);
            case 1: return ladder[(size_t) st].process (x, fc, res, sr, Ladder::lp12, drive) / std::sqrt (drive);
            case 2: return ladder[(size_t) st].process (x, fc, res, sr, Ladder::bp12, drive) / std::sqrt (drive);
            case 3: return ladder[(size_t) st].process (x, fc, res, sr, Ladder::hp24, drive) / std::sqrt (drive);
            case 4: { const auto o = svf[(size_t) st].process (softClip (x * drive) / std::sqrt (drive), cA); return o.low + o.high; }
            case 5: return combProcess (comb[(size_t) st], softClip (x * drive) / std::sqrt (drive), fc, 0.5 + 0.48 * res);
            case 6: return combProcess (comb[(size_t) st], softClip (x * drive) / std::sqrt (drive), fc, -(0.5 + 0.48 * res));
            default:
            {
                const auto in = softClip (x * drive) / std::sqrt (drive);
                return 1.8 * (svf[(size_t) st].process (in, cA).band + 0.7 * svfB[(size_t) st].process (in, cB).band);
            }
        }
    }

    double sr = 48000.0, pan = 0.0, spread = 0.0;
    double pc = 0.0, pm = 0.0, psub = 0.0, pwmPh = 0.0, held = 0.0;
    int holdCount = 0;
    croc::OrbState orb;
    double gPrev = 0.0, glPrev = 1.0, grPrev = 1.0;
    double lastA = 0.0, lastF = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false, releaseAfterStart = false;
    std::uint32_t rng = 7;
    std::array<Ladder, 2> ladder;
    std::array<StateFilter, 2> svf, svfB;
    std::array<Comb, 2> comb;
    SvfCoefs cA, cB;
    Adsr ea, ef, em;
    croc::Noise nC, nM, nX;
    double emLast = 0.0;
    Glide pitch;
    DcBlocker dc;
    const croc::Shared* sh = nullptr;
};


class CrocGlace final : public SynthBase<CrocVoice, 8>
{
public:
    CrocGlace()
        : reverb (std::make_unique<grv::effects::Reverb>())
    {
        (void) croc::bank(); // tables calculées à la création (fil de l'interface), jamais sur le fil audio
        for (int i = 0; i < 8; ++i) { voices[(size_t) i].seed (0xC20Cu * (std::uint32_t) (i + 3)); voices[(size_t) i].setShared (&sh); }
        ix.rvSize = reverb.index ("size"); ix.rvDecay = reverb.index ("decay"); ix.rvBright = reverb.index ("brightness");
        ix.rvWidth = reverb.index ("width"); ix.rvMix = reverb.index ("mix"); ix.rvDiff = reverb.index ("diffusion");
        cornerAt = croc::desc().indexOf ("ka_on");
        {
            const auto& d = croc::desc();
            xi = { d.indexOf ("dlcomb"), d.indexOf ("dlcombst"), d.indexOf ("dlcombfine"), d.indexOf ("phstages"), d.indexOf ("phfreq"),
                   d.indexOf ("phrate"), d.indexOf ("phdepth"), d.indexOf ("phfb"), d.indexOf ("phmix"), d.indexOf ("stwidth"),
                   d.indexOf ("sthaas"), d.indexOf ("rvfreeze"), d.indexOf ("fxorder") };
        }
        ix.rvFreeze = reverb.index ("freeze");
        globalOrb.seed = 0xC20Cu;
    }
    const ModuleDesc& desc() const noexcept override { return croc::desc(); }

    void prepare (double s, int maxBlock) override
    {
        SynthBase::prepare (s, maxBlock);
        sh.sr = s;
        reverb.prepare (s, std::max (64, maxBlock));
        echo.prepare (s); flg.prepare (s); stereo.prepare (s); phaser.reset();
    }

    void reset() noexcept override
    {
        SynthBase::reset();
        reverb.reset(); echo.reset(); flg.reset(); dist = {}; phaser.reset(); stereo.reset();
        for (auto& a : lfoAcc) a = 0.0;
        for (auto& c : chaosX) c = 0.37;
        for (auto& c : chaosLast) c = std::numeric_limits<std::int64_t>::min();
        for (auto& L : lanes) L = {};
        flAcc = 0.0; globalOrb = {}; globalOrb.seed = 0xC20Cu;
        tailOn = false; quiet = 0; fxWas = { false, false, false, false };
    }

    void noteOn (int note, float velocity) noexcept override
    {
        SynthBase::noteOn (note, velocity);
        ++noteCount;
        combNote = note; // le peigne de l'écho suit la dernière note jouée
    }

    void setPosition (double ppq, bool playing) noexcept override { blockPpq = ppq; transport = playing; }

    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        if (index < 0 || index > 1) return;
        sh.user[(size_t) index] = s != nullptr && ! s->firmware && s->runtime != nullptr ? static_cast<const croc::SampleRuntime*> (s->runtime.get()) : nullptr;
    }

    bool isActive() const noexcept override { return SynthBase::isActive() || tailOn; }

    /** Écran : ORB X / Y (le point), orbite X / Y, orbe 3 X / Y ; pas joué de chaque couloir (−1 : arrêt) ;
        une voix sonne ; valeur de chaque couloir ; valeurs des 8 LFO ; morph en marche, poids des coins A à D. */
    int status (float* out, int max) const noexcept override
    {
        if (max < 32) return 0;
        for (int k = 0; k < 6; ++k) out[k] = shownOrb[(size_t) k].load (std::memory_order_relaxed);
        for (int l = 0; l < 4; ++l) { out[6 + l] = (float) shownStep[(size_t) l].load (std::memory_order_relaxed); out[11 + l] = shownLane[(size_t) l].load (std::memory_order_relaxed); }
        out[10] = sounding.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        for (int k = 0; k < 8; ++k) out[15 + k] = shownLfo[(size_t) k].load (std::memory_order_relaxed);
        out[23] = shownMorphOn.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        for (int c = 0; c < 4; ++c) out[24 + c] = shownW[(size_t) c].load (std::memory_order_relaxed);
        return 28;
    }

protected:
    const float* adjust (const float* p) noexcept override
    {
        using namespace croc;
        constexpr int b = (int) lfoBase, st = lfoStride;
        const auto* q = syncRates (p, numSlots, {
            { b + lRate, b + lSync }, { b + st + lRate, b + st + lSync }, { b + 2 * st + lRate, b + 2 * st + lSync }, { b + 3 * st + lRate, b + 3 * st + lSync },
            { b + 4 * st + lRate, b + 4 * st + lSync }, { b + 5 * st + lRate, b + 5 * st + lSync }, { b + 6 * st + lRate, b + 6 * st + lSync },
            { b + 7 * st + lRate, b + 7 * st + lSync }, { flRate, flSync }, { o1Rate, o1Sync } });
        // le point de l'ORB promené par les couloirs (destinations ORB X / Y, bloc précédent)
        scratch[(size_t) morphX] = std::clamp (scratch[(size_t) morphX] + (float) (laneMorph[0] * 100.0), -100.0f, 100.0f);
        scratch[(size_t) morphY] = std::clamp (scratch[(size_t) morphY] + (float) (laneMorph[1] * 100.0), -100.0f, 100.0f);
        if (p[morphOn] > 0.5f) morph (scratch.data());
        else shownMorphOn.store (false, std::memory_order_relaxed);
        return q;
    }

    /** L'ORB à 4 coins : le point (ORB X / Y, au doigt ou en orbite) mélange les sons des coins A (haut gauche),
        B (haut droite), C (bas gauche), D (bas droite). Un coin vide joue le son en cours. */
    void morph (float* q) noexcept
    {
        using namespace croc;
        if (cornerAt < 0) return;
        const auto stride = cornerStride();
        std::array<bool, numCorners> loaded {};
        bool any = false;
        for (int c = 0; c < numCorners; ++c) { loaded[(size_t) c] = q[cornerAt + c * stride] > 0.5f; any = any || loaded[(size_t) c]; }
        const auto x = orbNow[0], y = orbNow[1];
        const auto gx = 0.5 * (x + 1.0), gy = 0.5 * (1.0 - y); // A en haut à gauche
        const std::array<double, numCorners> w { (1.0 - gx) * (1.0 - gy), gx * (1.0 - gy), (1.0 - gx) * gy, gx * gy };
        shownMorphOn.store (true, std::memory_order_relaxed);
        for (int c = 0; c < numCorners; ++c) shownW[(size_t) c].store ((float) w[(size_t) c], std::memory_order_relaxed);
        if (! any) return; // quatre coins vides : le son en cours partout
        int dom = 0;
        for (int c = 1; c < numCorners; ++c) if (w[(size_t) c] > w[(size_t) dom]) dom = c;
        const auto amt = std::clamp (q[morphAmt] / 100.0, 0.0, 1.0);
        const auto& d = desc();
        const auto& slots = morphSlots();
        for (size_t j = 0; j < slots.size(); ++j)
        {
            const auto sl = slots[j];
            // 27 nov. 2026 (Henrico) : les effets (distorsion, flanger, écho, réverbération) suivent toujours le
            // panneau, même en morph. Mélangés entre les coins, ils tombaient à zéro vers un coin sans effet et
            // l'écho / la réverbération se vidaient à chaque passage. Les coins gardent leurs valeurs (format inchangé).
            if (sl >= distMode && sl <= rvMix) continue;
            const auto& pd = d.params[(size_t) sl];
            const auto at = cornerAt + 2 + (int) j;
            auto val = [&] (int c) { return loaded[(size_t) c] ? q[at + c * stride] : q[sl]; }; // coin vide : le son en cours
            if (! pd.choices.empty() || pd.integer)
            {
                if (amt >= 0.5) q[sl] = val (dom);
                continue;
            }
            const bool logScale = pd.skew != 1.0f && pd.min > 0.0f;
            double v = 0.0;
            if (logScale)
            {
                for (int c = 0; c < numCorners; ++c) v += w[(size_t) c] * std::log (std::max ((double) pd.min, (double) val (c)));
                const auto cur = std::log (std::max ((double) pd.min, (double) q[sl]));
                q[sl] = (float) std::exp (cur + amt * (v - cur));
            }
            else
            {
                for (int c = 0; c < numCorners; ++c) v += w[(size_t) c] * val (c);
                q[sl] = (float) (q[sl] + amt * (v - q[sl]));
            }
            q[sl] = std::clamp (q[sl], pd.min, pd.max);
        }
    }

    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[croc::voices], 1, 8);
        switch ((int) p[croc::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            case 3: setMode (true, true, v, std::clamp ((int) p[croc::uniVoices], 2, 8)); break;
            default: setMode (v == 1, true, v); break;
        }
    }

    void voiceDone (int i, const float* /*vp*/) noexcept override
    {
        if (i != lastVoice) return;
        leadOrb = voices[(size_t) i].orbOut;
        haveLead = true;
    }

    /** Les 8 LFO, les 4 couloirs et le LFO du filtre pour tout l'instrument ; horloge commune de l'orbite ;
        sources de la matrice. */
    void blockStart (const float* q, int n) noexcept override
    {
        using namespace croc;
        sh.bpm = bpm;
        const auto dtB = (double) n / sr;
        const int notes = noteCount; noteCount = 0;
        const bool anyActive = SynthBase::isActive();
        if (notes > 0) noteSec = 0.0;
        const auto qps = std::clamp (bpm, 20.0, 400.0) / 60.0; // noires par seconde
        const int pts = std::max (1, (n + 15) / 16);
        const auto ppqSec = blockPpq / qps;

        // --- les 8 LFO (au début du bloc pour la matrice, aux points de commande pour les couloirs)
        std::array<std::array<double, 5>, numLfos> lv {};
        for (int k = 0; k < numLfos; ++k)
        {
            const auto* L = q + lfoBase + k * lfoStride;
            const auto hz = std::max (0.0, (double) L[lRate]);
            const auto ph0 = L[lPhase] / 360.0;
            const bool retrig = L[lRetrig] > 0.5f, synced = L[lSync] > 0.5f;
            if (! retrig && ! (synced && transport)) lfoAcc[(size_t) k] += hz * dtB;
            if (lfoAcc[(size_t) k] > 1.0e7) lfoAcc[(size_t) k] = 0.0;
            for (int j = 0; j < pts; ++j)
            {
                const auto t = j * 16.0 / sr;
                double cyc;
                if (retrig) cyc = (noteSec + t) * hz;
                else if (synced && transport) cyc = (ppqSec + t) * hz;
                else cyc = lfoAcc[(size_t) k] - hz * dtB + t * hz;
                lv[(size_t) k][(size_t) j] = lfoValue ((int) L[lShape], cyc + ph0, (int) std::lround (L[lSteps]), k) * L[lAmt] / 100.0;
            }
            for (int j = pts; j < 5; ++j) lv[(size_t) k][(size_t) j] = lv[(size_t) k][(size_t) (pts - 1)];
            hub.setExtra (exLfo1 + k, lv[(size_t) k][0]);
            shownLfo[(size_t) k].store ((float) lv[(size_t) k][0], std::memory_order_relaxed);
        }

        // --- les 4 couloirs : chaque pas joue l'un des 8 LFO, à son niveau
        laneMorph = { 0.0, 0.0 };
        fxMod.fill (0.0);
        for (int l = 0; l < numLanes; ++l)
        {
            const auto* C = q + laneBase + l * laneStride;
            auto& st = lanes[(size_t) l];
            sh.laneOn[l] = C[lnOn] > 0.5f;
            if (! sh.laneOn[l])
            {
                for (auto& v : sh.lane[l]) v = 0.0;
                st.value = 0.0;
                hub.setExtra (exLane1 + l, 0.0);
                shownStep[(size_t) l].store (-1, std::memory_order_relaxed);
                shownLane[(size_t) l].store (0.0f, std::memory_order_relaxed);
                continue;
            }
            const auto len = std::clamp ((int) std::lround (C[lnLen]), 1, laneSteps);
            const auto dq = divQuarters ((int) C[lnDiv]);
            const auto mode = std::clamp ((int) C[lnMode], 0, 2);
            if (mode == 1 && notes > 0) st.acc = 0.0;
            const auto swing = std::clamp (C[lnSwing] / 100.0, 0.5, 0.75);
            const auto smooth = C[lnSmooth] / 100.0, curve = C[lnCurve] / 100.0, fold = C[lnFold] / 100.0, jit = C[lnJitter] / 100.0;
            const auto quant = (int) std::lround (C[lnQuant]);
            const auto dir = std::clamp ((int) C[lnDir], 0, 3);
            const auto single = std::clamp ((int) C[lnSrc], 0, numLfos);           // 0 : la séquence ; 1-8 : un seul LFO (le « LFO Switch »)
            const auto motif = std::clamp ((int) C[lnMotif], 0, (int) motifs().size()); // 0 : les pas réglés à la main
            const Motif* mt = motif > 0 ? &motifs()[(size_t) (motif - 1)] : nullptr;
            int shownS = 0;
            for (int j = 0; j < pts; ++j)
            {
                const auto t = j * 16.0 / sr;
                const auto posQ = mode == 0 && transport ? blockPpq + t * qps : st.acc + t * qps;
                // swing : la 1re croche de chaque paire dure plus longtemps
                const auto pairPos = posQ / (2.0 * dq);
                const auto pairN = (std::int64_t) std::floor (pairPos);
                const auto f = pairPos - (double) pairN;
                const auto second = f >= swing;
                const auto stepN = pairN * 2 + (second ? 1 : 0);
                int idx;
                const auto m = (int) (((stepN % len) + len) % len);
                switch (dir)
                {
                    case 1: idx = len - 1 - m; break;
                    case 2: { const auto per = std::max<std::int64_t> (1, 2 * len - 2); const auto k = (int) (((stepN % per) + per) % per); idx = len <= 1 ? 0 : (k < len ? k : (int) per - k); break; }
                    case 3: idx = std::min (len - 1, (int) (hash01 ((std::uint64_t) stepN * 977u + (std::uint64_t) l * 31u) * len)); break;
                    default: idx = m; break;
                }
                if (j == 0) shownS = idx;
                double x;
                if (single > 0) x = lv[(size_t) (single - 1)][(size_t) j];
                else
                {
                    // chaque pas joue l'un des 8 LFO (ou « Fixe » : le niveau seul), à son niveau ; un motif remplace les pas
                    const auto lfo = mt != nullptr ? (int) mt->src[idx % 16] : std::clamp ((int) C[laneHead + idx * 2], 0, numLfos);
                    const auto lvl = mt != nullptr ? mt->lvl[idx % 16] / 100.0 : C[laneHead + idx * 2 + 1] / 100.0;
                    x = (lfo >= numLfos ? 1.0 : lv[(size_t) lfo][(size_t) j]) * lvl;
                }
                // hasard par pas : chaque pas part ailleurs, jusqu'à ±100 %
                if (jit > 0.0) x += jit * (hash01 ((std::uint64_t) stepN * 131u + (std::uint64_t) l * 7u + 5u) * 2.0 - 1.0);
                if (curve != 0.0) { const auto e = curve > 0.0 ? 1.0 + 3.0 * curve : 1.0 / (1.0 - 3.0 * curve); x = (x < 0.0 ? -1.0 : 1.0) * std::pow (std::min (1.0, std::abs (x)), e); }
                if (fold > 0.0) x = fastSin2Pi (x * (1.0 + 4.0 * fold) * 0.25);
                if (quant >= 2) x = std::round ((std::clamp (x, -1.0, 1.0) + 1.0) * 0.5 * (quant - 1)) / (quant - 1) * 2.0 - 1.0;
                x = std::clamp (x, -1.0, 1.0);
                if (smooth > 0.0) st.value += (x - st.value) * (1.0 - std::exp (-16.0 / sr / std::max (1.0e-4, smooth * 0.7 * dq / qps)));
                else st.value = x;
                sh.lane[l][j] = st.value;
            }
            for (int j = pts; j < 5; ++j) sh.lane[l][j] = st.value;
            st.acc += dtB * qps;
            if (st.acc > 1.0e7) st.acc = 0.0;
            hub.setExtra (exLane1 + l, sh.lane[l][0]);
            const auto dst = std::clamp ((int) C[lnDst], 0, numDests - 1);
            if (dst == dMorphX || dst == dMorphY) laneMorph[(size_t) (dst - dMorphX)] += C[lnAmt] / 100.0 * sh.lane[l][0];
            if (fxDest (dst)) fxMod[(size_t) dst] += C[lnAmt] / 100.0 * sh.lane[l][0]; // effets : communs, modulés par le couloir
            shownStep[(size_t) l].store (shownS, std::memory_order_relaxed);
            shownLane[(size_t) l].store ((float) sh.lane[l][0], std::memory_order_relaxed);
        }

        // --- LFO du filtre (commun : calé sur la grille quand il est au tempo)
        {
            const auto hz = std::max (0.0, (double) q[flRate]);
            const bool synced = q[flSync] > 0.5f;
            for (int j = 0; j < pts; ++j)
            {
                const auto t = j * 16.0 / sr;
                const auto cyc = synced && transport ? (ppqSec + t) * hz : flAcc + t * hz;
                const auto f = cyc - std::floor (cyc);
                double y;
                switch ((int) q[flShape])
                {
                    case 0: y = fastSin2Pi (cyc); break;
                    case 2: y = 2.0 * f - 1.0; break;
                    case 3: y = f < 0.5 ? 1.0 : -1.0; break;
                    case 4: y = 2.0 * hash01 ((std::uint64_t) (std::int64_t) std::floor (cyc) + 4242u) - 1.0; break;
                    default: y = 1.0 - 4.0 * std::abs (f - 0.5); break;
                }
                sh.flfo[(size_t) j] = y;
            }
            for (int j = pts; j < 5; ++j) sh.flfo[(size_t) j] = sh.flfo[(size_t) (pts - 1)];
            if (! (synced && transport)) flAcc += hz * dtB;
            if (flAcc > 1.0e7) flAcc = 0.0;
        }

        // --- horloge commune de l'orbite (départ « libre » des voix, écran et morph quand rien ne joue)
        {
            std::array<double, numOrbOuts> g {};
            if (q[o1Sync] > 0.5f && transport && q[orbRetrig] < 0.5f && (int) q[o1Shape] < croc::osLorenz)
            {
                // au tempo et en lecture : l'orbite est calée sur la grille du morceau
                const auto hz = std::max (0.0, (double) q[o1Rate]) * ((int) q[o1Dir] == 1 ? -1.0 : 1.0);
                const auto target = ppqSec * hz;
                globalOrb.ph2 += (target - globalOrb.cyc) * q[o2Ratio] * (q[o2Dir] > 0.5f ? 1.0 : -1.0);
                globalOrb.ph3 += (target - globalOrb.cyc) * q[o3Ratio] * (q[o3Dir] > 0.5f ? 1.0 : -1.0);
                globalOrb.ph2 -= std::floor (globalOrb.ph2); globalOrb.ph3 -= std::floor (globalOrb.ph3);
                globalOrb.cyc = target;
                globalOrb.step (q, 0.0, g);
            }
            else globalOrb.step (q, dtB, g);
            sh.orbCycles = globalOrb.cyc; sh.orbPh2 = globalOrb.ph2; sh.orbPh3 = globalOrb.ph3;
            // ORB : celui de la voix de la dernière note, sinon l'horloge commune
            if (haveLead && anyActive) orbNow = leadOrb;
            else { haveLead = false; orbNow = g; }
        }
        // routes de l'ORB vers les effets (l'ORB de la voix de la dernière note, ou l'horloge commune)
        for (int r = 0; r < numRoutes; ++r)
        {
            const auto* R = q + routeBase + r * routeStride;
            const auto dst = std::clamp ((int) R[1], 0, numDests - 1);
            if (fxDest (dst)) fxMod[(size_t) dst] += R[2] / 100.0 * orbNow[(size_t) std::clamp ((int) R[0], 0, numOrbOuts - 1)];
        }
        hub.setExtra (exOrbX, orbNow[0]); hub.setExtra (exOrbY, orbNow[1]);
        hub.setExtra (exO3X, orbNow[4]); hub.setExtra (exO3Y, orbNow[5]);
        for (int k = 0; k < 6; ++k) shownOrb[(size_t) k].store ((float) orbNow[(size_t) k], std::memory_order_relaxed);
        sounding.store (anyActive, std::memory_order_relaxed);
        noteSec += dtB;
    }

    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace croc;
        // effets modulés par les couloirs et les routes de l'ORB (destinations « effets »)
        const auto& fm = fxMod;
        const auto distMixV = std::clamp (p[distMix] / 100.0, 0.0, 1.0);
        const auto flgMixV = std::clamp (p[flgMix] / 100.0, 0.0, 1.0);
        const auto dlMixV = std::clamp (p[dlMix] / 100.0 + fm[dDlyMix], 0.0, 1.0);
        const auto rvMixV = p[rvMix];
        const bool frozen = xp (p, xi.rvFreeze, 0.0f) > 0.5f;
        const auto phMixV = std::clamp (xp (p, xi.phMix, 0.0f) / 100.0, 0.0, 1.0);
        const bool on[4] { distMixV > 0.005, flgMixV > 0.005, dlMixV > 0.005, rvMixV > 0.5f };
        const bool phOn = phMixV > 0.005;
        if (on[0]) dist.process (L, R, n, std::clamp ((int) p[distMode], 0, 3), p[distDrive] / 100.0 + fm[dDist], distMixV);
        if (on[1])
            flg.process (L, R, n, sr, std::clamp ((double) p[flgTime] * fastExp2 (std::clamp (fm[dFlgTime], -1.0, 1.0) * 3.0), 0.05, 10.0), p[flgRate],
                         std::clamp (p[flgDepth] / 100.0 + fm[dFlgDepth], 0.0, 1.0), std::clamp (p[flgFb] / 100.0 + fm[dFlgFb], -0.99, 0.99), flgMixV);
        // phaser (27 nov. 2026)
        if (phOn)
        {
            static constexpr int stagesOf[3] { 4, 8, 12 };
            phaser.process (L, R, n, sr, stagesOf[std::clamp ((int) xp (p, xi.phStages, 1.0f), 0, 2)],
                            xp (p, xi.phFreq, 700.0f) * fastExp2 (std::clamp (fm[dPhFreq], -1.0, 1.0) * 4.0), xp (p, xi.phRate, 0.3f),
                            std::clamp (xp (p, xi.phDepth, 70.0f) / 100.0, 0.0, 1.0),
                            std::clamp (xp (p, xi.phFb, 50.0f) / 100.0 + fm[dPhFb], -0.95, 0.95), phMixV);
        }
        else if (phWas) phaser.reset();
        phWas = phOn;
        auto doEcho = [&]
        {
            if (! on[2]) return;
            double baseMs;
            if (xp (p, xi.comb, 0.0f) > 0.5f)
            {
                // peigne : le temps de l'écho est la période de la note jouée (± demi-tons) — métal accordé
                const auto st = (double) combNote - 69.0 + xp (p, xi.combSt, 0.0f) + xp (p, xi.combFine, 0.0f) / 100.0 + fm[dCombPitch] * 24.0;
                baseMs = 1000.0 / (440.0 * fastExp2 (std::clamp (st, -60.0, 60.0) / 12.0));
            }
            else baseMs = p[dlSync] > 0.5f ? grv::effects::echo::divisionQuarters ((int) p[dlDiv]) * 60000.0 / std::clamp (bpm, 20.0, 400.0) : (double) p[dlTime];
            echo.process (L, R, n, sr, std::clamp (baseMs * fastExp2 (std::clamp (fm[dDlyTime], -1.0, 1.0) * 3.0), 0.05, 2000.0), p[dlMode] > 0.5f,
                          std::clamp (p[dlFb] / 100.0 + fm[dDlyFb], 0.0, 1.2), p[dlGlide], std::clamp (p[dlTone] / 100.0 + fm[dDlyTone], 0.0, 1.0), dlMixV);
        };
        auto doReverb = [&]
        {
            if (! on[3]) return;
            const auto sz = std::clamp (p[rvSize] + (float) (fm[dRvSize] * 100.0), 0.0f, 100.0f);
            reverb.set (ix.rvSize, sz); reverb.set (ix.rvDecay, 0.5f + 8.0f * (sz / 100.0f) * (sz / 100.0f));
            reverb.set (ix.rvBright, (float) (18000.0 * std::pow (0.08, p[rvDamp] / 100.0)));
            reverb.set (ix.rvWidth, p[rvWidth]); reverb.set (ix.rvMix, rvMixV);
            reverb.set (ix.rvDiff, std::clamp (p[rvDiff] + (float) (fm[dRvDiff] * 100.0), 0.0f, 100.0f));
            if (ix.rvFreeze >= 0) reverb.set (ix.rvFreeze, frozen ? 1.0f : 0.0f);
            reverb.process (L, R, n, bpm);
        };
        if (xp (p, xi.order, 0.0f) > 0.5f) { doReverb(); doEcho(); } // réverbération puis écho (queues répétées)
        else { doEcho(); doReverb(); }
        // stéréo en bout de chaîne
        {
            const auto width = std::clamp (xp (p, xi.stWidth, 100.0f) / 100.0 + fm[dStWidth], 0.0, 2.0);
            const auto haas = (double) xp (p, xi.stHaas, 0.0f);
            if (std::abs (width - 1.0) > 0.005 || haas > 0.05) stereo.process (L, R, n, sr, width, haas);
        }
        // un effet coupé repart vide la fois suivante (pas de reste d'écho ancien)
        if (fxWas[1] && ! on[1]) flg.reset();
        if (fxWas[2] && ! on[2]) echo.reset();
        if (fxWas[3] && ! on[3]) reverb.reset();
        for (int k = 0; k < 4; ++k) fxWas[(size_t) k] = on[k];

        // queue des effets : l'instrument reste actif tant que la sortie n'est pas retombée
        if (SynthBase::isActive()) { tailOn = on[1] || on[2] || on[3] || phOn; quiet = 0; tailLen = 0; }
        else if (tailOn && frozen && on[3]) { quiet = 0; tailLen = 0; } // réverbération gelée : la queue tient tant que le gel est mis
        else if (tailOn)
        {
            float pk = 0.0f;
            for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
            quiet = pk < 1.0e-5f ? quiet + n : 0;
            tailLen += n;
            if (quiet > (int) (0.4 * sr) || tailLen > (int) (30.0 * sr) || ! (on[1] || on[2] || on[3] || phOn)) { tailOn = false; quiet = 0; tailLen = 0; }
        }
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (L[i])) L[i] = 0.0f;
            if (! std::isfinite (R[i])) R[i] = 0.0f;
        }
        blockPpq += n / sr * std::clamp (bpm, 20.0, 400.0) / 60.0; // render découpé en blocs de 64 : position de la suite
    }

private:
    struct Lane { double acc = 0.0, value = 0.0; };

    /** Valeur d'un LFO (-1..1) au nombre de cycles cyc ; marches : quantification du lisse au numérique. */
    double lfoValue (int shape, double cyc, int steps, int k) noexcept
    {
        const auto f = cyc - std::floor (cyc);
        const auto i0 = (std::int64_t) std::floor (cyc);
        double y;
        switch (shape)
        {
            case 1: y = 1.0 - 4.0 * std::abs (f - 0.5); break;
            case 2: y = 1.0 - 2.0 * f; break;                 // scie (descendante)
            case 3: y = 2.0 * f - 1.0; break;                 // rampe (montante)
            case 4: y = f < 0.5 ? 1.0 : -1.0; break;
            case 5: y = 2.0 * croc::hash01 ((std::uint64_t) i0 * 7u + (std::uint64_t) k * 1013u) - 1.0; break;
            case 6: y = croc::smoothNoise (cyc, (std::uint64_t) k * 1013u + 1u); break;
            case 7: case 8:
            {
                // chaos : suite logistique, une valeur par cycle ; « sauts » : nette (glitch), « lissé » : fondue jusqu'à la suivante
                auto& x = chaosX[(size_t) k];
                if (chaosLast[(size_t) k] != i0)
                {
                    chaosPrev[(size_t) k] = chaosCur[(size_t) k];
                    x = 3.99 * x * (1.0 - x);
                    if (x < 1.0e-6 || x > 1.0 - 1.0e-6) x = 0.2 + 0.6 * croc::hash01 ((std::uint64_t) i0 + 77u * (std::uint64_t) (k + 1));
                    chaosCur[(size_t) k] = 2.0 * x - 1.0;
                    chaosLast[(size_t) k] = i0;
                }
                if (shape == 7) { y = chaosCur[(size_t) k]; break; }
                const auto s = 0.5 - 0.5 * fastSin2Pi (f * 0.5 + 0.25);
                y = chaosPrev[(size_t) k] + (chaosCur[(size_t) k] - chaosPrev[(size_t) k]) * s;
                break;
            }
            case 9: y = lfoNoise[(size_t) k].next (0.0); break;                                   // bruit blanc (indépendant de la vitesse)
            case 10: y = std::clamp (lfoNoise[(size_t) k].next (0.5) * 2.2, -1.0, 1.0); break;      // bruit rose
            default: y = fastSin2Pi (cyc); break;
        }
        if (steps >= 2) y = std::round ((y + 1.0) * 0.5 * (steps - 1)) / (steps - 1) * 2.0 - 1.0;
        return y;
    }

    croc::Shared sh;
    gd2::MappedFx reverb;
    croc::Echo echo;
    croc::Flanger flg;
    croc::Distortion dist;
    struct { int rvSize = -1, rvDecay = -1, rvBright = -1, rvWidth = -1, rvMix = -1, rvDiff = -1, rvFreeze = -1; } ix;
    struct { int comb = -1, combSt = -1, combFine = -1, phStages = -1, phFreq = -1, phRate = -1, phDepth = -1, phFb = -1, phMix = -1,
             stWidth = -1, stHaas = -1, rvFreeze = -1, order = -1; } xi;
    croc::Phaser phaser;
    croc::Stereo stereo;
    int combNote = 60;
    float xp (const float* p, int i, float def) const noexcept { return i >= 0 ? p[i] : def; }
    int cornerAt = -1;
    std::array<double, croc::numLfos> lfoAcc {}, chaosX {}, chaosPrev {}, chaosCur {};
    std::array<std::int64_t, croc::numLfos> chaosLast {};
    std::array<croc::Noise, croc::numLfos> lfoNoise {};
    std::array<Lane, croc::numLanes> lanes {};
    croc::OrbState globalOrb;
    std::array<double, croc::numOrbOuts> leadOrb {}, orbNow {};
    std::array<double, 2> laneMorph {};
    std::array<double, croc::numDests> fxMod {};
    std::array<bool, 4> fxWas { false, false, false, false };
    bool phWas = false;
    bool haveLead = false, transport = false, tailOn = false;
    double flAcc = 0.0, blockPpq = 0.0, noteSec = 1.0e3;
    int noteCount = 0, quiet = 0, tailLen = 0;
    std::array<std::atomic<float>, 6> shownOrb {};
    std::array<std::atomic<float>, croc::numLfos> shownLfo {};
    std::array<std::atomic<float>, croc::numLanes> shownLane {};
    std::array<std::atomic<int>, croc::numLanes> shownStep {};
    std::array<std::atomic<float>, croc::numCorners> shownW {};
    std::atomic<bool> shownMorphOn { false }, sounding { false };
};

} // namespace grv::instruments
