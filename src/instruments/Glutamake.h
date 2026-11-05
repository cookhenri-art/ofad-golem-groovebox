/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Glutamake » — principe des synthés à tables d'ondes avec physique de mouvement (architecture
    reprise, ni nom, ni panneau, ni table, ni son d'origine : tout est calculé par le programme) :
      - deux oscillateurs ; chacun lit deux tables (A et B, mélangées), parcourues par une position
        (12 ondes calculées par table), puis un « morph » en temps réel (étirer, comprimer, refléter,
        miroir, synchro, plier, pulsation, quantifier, bits, FM / AM / anneau par l'autre oscillateur)
        et un modificateur de sortie ; sous-oscillateur et bruit ;
      - un filtre à 10 types : multi (fondu bas → bande → haut), échelle 4 et 2 pôles (bas, haut,
        bande), coupe-bande, « agressif » à réinjection saturée (bas, haut), doux ;
      - enveloppes d'ampli, de filtre et de hauteur, deux LFO, plus deux enveloppes et trois LFO à pas
        de la modulation commune, matrice de 32 routages, 4 boutons de modulation, arpège à pas ;
      - LA MARQUE DE FABRIQUE : une bille qui roule sur une surface (inclinaison, frottement, temps,
        une bosse — creux qui attire ou colline qui repousse —, quatre bords qui absorbent ou
        relancent, ou qu'on traverse), lancée au geste, à chaque note ou par la séquence ; ses sorties
        (X, Y, distance, angle, +X, −X, +Y, −Y, choc) sont des sources de la matrice ;
      - la séquence de mouvement : quatre couloirs de 16 pas indépendants (boucle début / fin, sens
        avant, arrière, aller-retour ou hasard, division ou avance à chaque note, lissage,
        probabilité), eux aussi sources de la matrice.

    Choix de modèle (latence nulle, rien d'alloué dans le rendu) : tables limitées en bande par octave
    (WaveBank), morphs par distorsion de phase avec pas de lecture corrigé (choix de la table moins
    riche quand la phase accélère), filtres TPT existants (échelle, état, Sallen-Key), physique et
    couloirs calculés une fois par bloc (≤ 64 échantillons, 1,3 ms à 48 kHz) pour tout l'instrument.
    Les réglages de la physique et des couloirs sont modulables par la matrice : ils sont lus avec la
    modulation de la voix de la dernière note (bloc précédent), ou tels quels quand rien ne joue.
    Volontairement écarté : effets internes (la piste a les siens), couloirs rythme et hauteur (le
    séquenceur à pas de la piste joue les notes, comme pour les autres synthés).
*/

#pragma once

#include "../synth/SynthBase.h"
#include "../synth/Wavetable.h"
#include "FactoryBanks.h"
#include "MS20.h"

#include <atomic>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace gluta
{
    constexpr int numTables = 8, framesPerTable = 12, laneSteps = 16, numLanes = 4;

    enum Slot
    {
        voices = 0, glide, level, velAmp, tune, spreadAmt, vMode, uniVoices, uniDetune,
        // oscillateur 1 puis 2 (même ordre : o2… = o1… + oscStride)
        o1TabA, o1TabB, o1Pos, o1Ab, o1MorphT, o1Morph, o1Modif, o1Oct, o1Semi, o1Fine, o1Lvl,
        o2TabA, o2TabB, o2Pos, o2Ab, o2MorphT, o2Morph, o2Modif, o2Oct, o2Semi, o2Fine, o2Lvl,
        subLvl, noiseLvl,
        fType, cutoff, reso, fEnv, fKey, fVel, fDrive, fMulti,
        aA, aD, aS, aR, fA, fD, fS, fR, pA, pD, pS, pR, pAmt,
        l1Rate, l1Shape, l1Sync, l1Delay, l2Rate, l2Shape, l2Sync, l2Delay,
        // physique
        phOn, tiltX, tiltY, fric, timeScale, bumpH, bumpX, bumpY, bumpW, bumpShape,
        edgeT, edgeB, edgeL, edgeR, wrapEdges, launchSrc, launchForce, launchAngle, launchRand, startX, startY,
        // séquence de mouvement : marche, départ, puis 4 couloirs de laneStride réglages
        seqOn, seqRetrig,
        laneBase,
        laneStride = laneSteps + 6, // 16 pas, début, fin, sens, division, lissage, probabilité
        numSlots = laneBase + numLanes * laneStride
    };
    constexpr int oscStride = o2TabA - o1TabA;
    // décalages dans un couloir
    enum LaneField { lfStart = laneSteps, lfEnd, lfDir, lfDiv, lfSmooth, lfProb };

    enum Extra { exX = 0, exY, exDist, exAngle, exPosX, exNegX, exPosY, exNegY, exHit, exLaneA, numExtras = exLaneA + numLanes };

    inline const std::vector<std::string>& tableNames()
    {
        static const std::vector<std::string> n { "Analogique", "Voyelles", "Cloches FM", "Plis", "Harmoniques paires",
                                                  "Granuleux", "Cordes frott\xC3\xA9" "es", "Num\xC3\xA9rique 8 bits" };
        return n;
    }
    inline const std::vector<std::string>& morphNames()
    {
        static const std::vector<std::string> n { "Aucun", "\xC3\x89tirer", "Comprimer", "Refl\xC3\xA9ter", "Miroir", "Synchro", "Plier",
                                                  "Pulsation", "Quantifier", "Bits", "FM (autre osc.)", "AM (autre osc.)", "Anneau (autre osc.)" };
        return n;
    }
    inline const std::vector<std::string>& modifNames()
    {
        static const std::vector<std::string> n { "Aucun", "Saturer", "Redresser", "Quantifier", "Ancien 8 bits", "Ancien 12 bits", "Graves renforc\xC3\xA9s" };
        return n;
    }
    inline const std::vector<std::string>& filterNames()
    {
        static const std::vector<std::string> n { "Multi (fondu)", "Passe-bas 4 p\xC3\xB4les", "Passe-bas 2 p\xC3\xB4les", "Passe-haut 4 p\xC3\xB4les",
                                                  "Passe-haut 2 p\xC3\xB4les", "Passe-bande", "Coupe-bande", "Agressif passe-bas",
                                                  "Agressif passe-haut", "Doux passe-bas" };
        return n;
    }
    inline const std::vector<std::string>& extraNames()
    {
        static const std::vector<std::string> n { "Bille X", "Bille Y", "Bille distance", "Bille angle", "Bille +X", "Bille \xE2\x88\x92X",
                                                  "Bille +Y", "Bille \xE2\x88\x92Y", "Bille choc", "S\xC3\xA9q. A", "S\xC3\xA9q. B", "S\xC3\xA9q. C", "S\xC3\xA9q. D" };
        return n;
    }
    /** Divisions d'un couloir, en noires (la dernière : avance à chaque note). */
    inline const std::vector<std::string>& laneDivNames()
    {
        static const std::vector<std::string> n { "1/32", "1/16", "1/8", "1/4", "1/2", "1 mes.", "2 mes.", "\xC3\x80 chaque note" };
        return n;
    }
    inline double laneDivQuarters (int i) noexcept
    {
        static constexpr double q[] { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
        return q[std::clamp (i, 0, 6)];
    }
    constexpr int laneDivNote = 7;

    /** Pas par défaut des couloirs (–100..100) : quatre motifs différents, prêts à router. */
    inline float laneDefault (int lane, int step) noexcept
    {
        static constexpr float a[16] { 100, -40, 20, -80, 60, -20, 0, 80, -100, 40, -60, 20, 80, -40, 0, 60 };
        static constexpr float b[16] { -100, -75, -50, -25, 0, 25, 50, 75, 100, 75, 50, 25, 0, -25, -50, -75 };
        switch (lane)
        {
            case 0: return a[step];
            case 1: return b[step];
            case 2: return (step % 4 == 0) ? 100.0f : (step % 2 == 0 ? 0.0f : -60.0f);
            default: return std::round (90.0f * (float) std::sin (step * 1.7 + 0.4));
        }
    }

    /** Les 8 tables de 12 ondes, calculées (synthèse additive ou forme dans le temps, puis limitées en
        bande par octave). Onde (t, k) : indice t * framesPerTable + k. */
    inline const WaveBank& bank()
    {
        static const WaveBank b = []
        {
            using cd = std::complex<double>;
            auto sine = [] (double a) { return cd (0.0, -0.5 * a); };
            auto frac = [] (double x) { return x - std::floor (x); };
            auto fold = [] (double x) { x = (x + 1.0) * 0.25; x -= std::floor (x); return std::abs (x * 4.0 - 2.0) - 1.0; };
            auto s2p = [] (double x) { return std::sin (kTwoPi * x); };
            auto hash = [] (std::uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; };
            WaveBank w;
            constexpr int F = framesPerTable;
            // 0 analogique : sinus → triangle → scie → carré → impulsion étroite (spectres mélangés)
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1) * 4.0;
                w.addWave ([&, t] (int h)
                {
                    auto key = [&] (int s) -> double
                    {
                        switch (s)
                        {
                            case 0: return h == 1 ? 1.0 : 0.0;
                            case 1: return h % 2 ? (((h / 2) % 2 ? -1.0 : 1.0) * 8.0 / (kPi * kPi * h * h)) : 0.0;
                            case 2: return 2.0 / (kPi * h) * 0.9;
                            case 3: return h % 2 ? 4.0 / (kPi * h) * 0.8 : 0.0;
                            default: return std::sin (kPi * h * 0.12) / (kPi * h) * 3.0;
                        }
                    };
                    const auto s0 = std::min (3, (int) t);
                    const auto f = t - s0;
                    return sine (key (s0) * (1.0 - f) + key (s0 + 1) * f);
                });
            }
            // 1 voyelles : deux formants (en rangs d'harmonique) qui passent A → E → I → O → U
            {
                static constexpr double F1[5] { 7.0, 4.0, 2.7, 4.2, 3.0 }, F2[5] { 10.5, 18.0, 21.0, 7.0, 6.0 };
                for (int k = 0; k < F; ++k)
                {
                    const auto t = (double) k / (F - 1) * 4.0;
                    const auto i0 = std::min (3, (int) t);
                    const auto f = t - i0;
                    const auto c1 = F1[i0] + (F1[i0 + 1] - F1[i0]) * f, c2 = F2[i0] + (F2[i0 + 1] - F2[i0]) * f;
                    w.addWave ([=] (int h)
                    {
                        const auto a = 0.06 + std::exp (-0.5 * std::pow ((h - c1) / 1.1, 2)) + 0.6 * std::exp (-0.5 * std::pow ((h - c2) / 2.2, 2));
                        return h <= 64 ? sine (a / std::sqrt ((double) h)) : cd();
                    });
                }
            }
            // 2 cloches FM : rapport entier qui monte, indice qui grandit
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1);
                const auto ratio = 1.0 + std::floor (t * 3.99) + (k % 2 ? 2.0 : 0.0);
                const auto index = 0.4 + t * 5.0;
                w.addWaveTime ([=] (double x) { return std::sin (kTwoPi * x + index * std::sin (kTwoPi * ratio * x)); });
            }
            // 3 plis : un sinus de plus en plus poussé, replié sur lui-même
            for (int k = 0; k < F; ++k)
            {
                const auto g = 1.0 + (double) k / (F - 1) * 6.5;
                w.addWaveTime ([=] (double x) { return fold (g * s2p (x)); });
            }
            // 4 harmoniques paires : un carré doux auquel s'ajoutent les rangs pairs (orgue → cuivre)
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1);
                w.addWave ([=] (int h)
                {
                    if (h == 1) return sine (1.0);
                    if (h % 2) return sine (0.35 / h);
                    return sine (t * 1.3 / std::pow ((double) h, 0.9 - 0.3 * t));
                });
            }
            // 5 granuleux : spectres au hasard (fixé), de plus en plus denses et aigus
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1);
                const auto top = 6 + (int) (t * 120.0);
                w.addWave ([=] (int h)
                {
                    if (h == 1) return sine (1.0 - 0.6 * t);
                    if (h > top) return cd();
                    const auto r = hash ((std::uint32_t) (h * 131 + k * 7919 + 17));
                    const auto a = (double) (r & 0xFFFF) / 65535.0;
                    if (a < 0.55 - 0.35 * t) return cd();
                    return std::polar (a * (0.5 + t) / std::sqrt ((double) h), (double) (r >> 16) / 65536.0 * kTwoPi);
                });
            }
            // 6 cordes frottées : spectre de la corde frottée (onde de Helmholtz) selon la place de l'archet
            for (int k = 0; k < F; ++k)
            {
                const auto beta = 0.04 + (double) k / (F - 1) * 0.42;
                w.addWave ([=] (int h) { return sine (std::abs (std::sin (kPi * h * beta)) / (h * std::sin (kPi * beta)) * 0.9); });
            }
            // 7 numérique 8 bits : sinus et tierce harmonique en paliers de plus en plus gros
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1);
                const auto q = 2.0 + std::round ((1.0 - t) * 14.0);
                w.addWaveTime ([=] (double x)
                {
                    const auto xs = std::floor (x * 64.0) / 64.0; // 64 échantillons par cycle
                    const auto v = s2p (xs) + 0.5 * s2p (3.0 * xs + t * 0.5);
                    return std::round (v * q) / q;
                });
            }
            (void) frac;
            return w;
        }();
        return b;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> lfoShapes { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9", "Al\xC3\xA9" "atoire" };
            ModuleDesc m;
            m.type = "gluta";
            m.name = "Glutamake";
            m.family = "Polys";
            m.principle = "Tables d'ondes \xC3\xA0 morphs, bille sur une surface (physique de mouvement) et s\xC3\xA9quence \xC3\xA0 couloirs";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Glisse", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Volume", "dB", -24, 6, -5.5f),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 35),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("spread", "Largeur", "%", 0, 100, 40),
                C ("vmode", "Jeu", { "Poly", "Mono", "Unisson" }),
                I ("univoices", "Voix d'unisson", "", 2, 8, 4),
                P ("unidetune", "D\xC3\xA9" "saccord", "ct", 0, 50, 12),
            };
            for (int o = 1; o <= 2; ++o)
            {
                const auto k = "o" + std::to_string (o);
                const auto n = " " + std::to_string (o);
                m.params.push_back (C (k + "taba", "Table A" + n, tableNames(), o == 1 ? 0 : 2));
                m.params.push_back (C (k + "tabb", "Table B" + n, tableNames(), o == 1 ? 1 : 6));
                m.params.push_back (P (k + "pos", "Position" + n, "%", 0, 100, o == 1 ? 30 : 10));
                m.params.push_back (P (k + "ab", "M\xC3\xA9lange A/B" + n, "%", 0, 100, o == 1 ? 0 : 0));
                m.params.push_back (C (k + "mtype", "Morph" + n, morphNames(), 0));
                m.params.push_back (P (k + "morph", "Intensit\xC3\xA9 du morph" + n, "%", 0, 100, 0));
                m.params.push_back (C (k + "modif", "Modificateur" + n, modifNames(), 0));
                m.params.push_back (I (k + "oct", "Octave" + n, "", -3, 3, 0));
                m.params.push_back (I (k + "semi", "Demi-tons" + n, "dt", -12, 12, o == 1 ? 0 : 7));
                m.params.push_back (P (k + "fine", "Fin" + n, "ct", -50, 50, o == 1 ? 0 : 6));
                m.params.push_back (P (k + "lvl", "Niveau" + n, "%", 0, 100, o == 1 ? 80 : 0));
            }
            m.params.push_back (P ("sub", "Sous-oscillateur", "%", 0, 100, 0));
            m.params.push_back (P ("noise", "Bruit", "%", 0, 100, 0));
            m.params.push_back (C ("ftype", "Type de filtre", filterNames(), 1));
            m.params.push_back (P ("cutoff", "Coupure", "Hz", 20, 18000, 2400, 0.3f));
            m.params.push_back (P ("reso", "R\xC3\xA9sonance", "%", 0, 100, 25));
            m.params.push_back (P ("fenv", "Enveloppe", "%", -100, 100, 35));
            m.params.push_back (P ("fkey", "Suivi clavier", "%", 0, 100, 50));
            m.params.push_back (P ("fvel", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 20));
            m.params.push_back (P ("fdrive", "Gain", "dB", 0, 24, 0));
            m.params.push_back (P ("fmulti", "Fondu multi", "%", 0, 100, 0));
            auto adsr = [&] (const std::string& k, float a, float dd, float s, float r)
            {
                m.params.push_back (P (k + "a", "Attaque", "ms", 0.5f, 10000, a, 0.3f));
                m.params.push_back (P (k + "d", "D\xC3\xA9" "clin", "ms", 5, 10000, dd, 0.3f));
                m.params.push_back (P (k + "s", "Maintien", "%", 0, 100, s));
                m.params.push_back (P (k + "r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, r, 0.3f));
            };
            adsr ("a", 3, 400, 80, 450);
            adsr ("f", 3, 600, 30, 500);
            adsr ("p", 0.5f, 120, 0, 100);
            m.params.push_back (P ("pamt", "Hauteur (enveloppe)", "dt", -48, 48, 0));
            for (int l = 1; l <= 2; ++l)
            {
                const auto k = "l" + std::to_string (l);
                m.params.push_back (P (k + "rate", "LFO " + std::to_string (l) + " vitesse", "Hz", 0.02f, 30, l == 1 ? 0.25f : 4.0f, 0.4f));
                m.params.push_back (C (k + "shape", "LFO " + std::to_string (l) + " forme", lfoShapes, l == 1 ? 1 : 0));
                m.params.push_back (C (k + "sync", "LFO " + std::to_string (l) + " synchro", lfoSyncChoices()));
                m.params.push_back (C (k + "delay", "LFO " + std::to_string (l) + " retard", lfoDelayChoices()));
            }
            // physique
            m.params.push_back (C ("phon", "Physique", { "Arr\xC3\xAAt", "Marche" }, 1));
            m.params.push_back (P ("tiltx", "Inclinaison X", "", -100, 100, 0));
            m.params.push_back (P ("tilty", "Inclinaison Y", "", -100, 100, 0));
            m.params.push_back (P ("fric", "Frottement", "%", 0, 100, 18));
            m.params.push_back (P ("ptime", "Temps", "%", 10, 400, 100, 0.5f));
            m.params.push_back (P ("bumph", "Bosse hauteur", "", -100, 100, -60));
            m.params.push_back (P ("bumpx", "Bosse X", "", -100, 100, 0));
            m.params.push_back (P ("bumpy", "Bosse Y", "", -100, 100, 0));
            m.params.push_back (P ("bumpw", "Bosse largeur", "%", 5, 100, 40));
            m.params.push_back (C ("bumpshape", "Forme de la bosse", { "Cloche", "C\xC3\xB4ne", "Plateau", "Anneau" }));
            m.params.push_back (P ("edget", "Bord haut", "", -100, 100, 0));
            m.params.push_back (P ("edgeb", "Bord bas", "", -100, 100, 0));
            m.params.push_back (P ("edgel", "Bord gauche", "", -100, 100, 0));
            m.params.push_back (P ("edger", "Bord droit", "", -100, 100, 0));
            m.params.push_back (C ("wrap", "Traverser les bords", { "Non", "Oui" }));
            m.params.push_back (C ("lsrc", "Lancement", { "Au geste", "\xC3\x80 chaque note", "Note, depuis le d\xC3\xA9part", "S\xC3\xA9quence (couloir A)" }, 1));
            m.params.push_back (P ("lforce", "Force", "%", 0, 100, 60));
            m.params.push_back (P ("langle", "Angle", "\xC2\xB0", 0, 360, 35));
            m.params.push_back (P ("lrand", "Hasard", "%", 0, 100, 20));
            m.params.push_back (P ("startx", "D\xC3\xA9part X", "", -100, 100, 0));
            m.params.push_back (P ("starty", "D\xC3\xA9part Y", "", -100, 100, 0));
            // séquence de mouvement
            m.params.push_back (C ("seqon", "S\xC3\xA9quence", { "Arr\xC3\xAAt", "Marche" }, 1));
            m.params.push_back (C ("seqretrig", "D\xC3\xA9part des couloirs", { "Libre", "\xC3\x80 chaque note" }));
            const char* letters[numLanes] { "a", "b", "c", "d" };
            for (int l = 0; l < numLanes; ++l)
            {
                const auto k = std::string ("sq") + letters[l];
                const auto nm = std::string ("S\xC3\xA9q. ") + (char) ('A' + l);
                for (int s = 0; s < laneSteps; ++s)
                    m.params.push_back (P (k + "s" + std::to_string (s + 1), nm + " pas " + std::to_string (s + 1), "%", -100, 100, laneDefault (l, s)));
                m.params.push_back (I (k + "start", nm + " d\xC3\xA9" "but", "", 1, 16, 1));
                m.params.push_back (I (k + "end", nm + " fin", "", 1, 16, std::array<float, 4> { 16, 12, 7, 14 }[(size_t) l]));
                m.params.push_back (C (k + "dir", nm + " sens", { "Avant", "Arri\xC3\xA8re", "Aller-retour", "Hasard" }, l == 3 ? 2 : 0));
                m.params.push_back (C (k + "div", nm + " division", laneDivNames(), l == 2 ? 2 : 1));
                m.params.push_back (P (k + "smooth", nm + " lissage", "%", 0, 100, l == 1 ? 60 : 0));
                m.params.push_back (P (k + "prob", nm + " probabilit\xC3\xA9", "%", 0, 100, 100));
            }

            m.sections = {
                { "global", "Voix", "global", { "vmode", "voices", "univoices", "unidetune", "glide", "tune", "spread", "velamp", "level" }, "global" },
                { "osc1", "Oscillateur 1", "wave", { "o1taba", "o1tabb", "o1pos", "o1ab", "o1mtype", "o1morph", "o1modif", "o1oct", "o1semi", "o1fine", "o1lvl" }, "osc" },
                { "osc2", "Oscillateur 2", "wave", { "o2taba", "o2tabb", "o2pos", "o2ab", "o2mtype", "o2morph", "o2modif", "o2oct", "o2semi", "o2fine", "o2lvl", "sub", "noise" }, "osc" },
                { "filter", "Filtre", "filter", { "ftype", "cutoff", "reso", "fenv", "fkey", "fvel", "fdrive", "fmulti" }, "filter" },
                { "ampenv", "Enveloppe ampli", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "filtenv", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr", "fenv" }, "env" },
                { "pitchenv", "Enveloppe hauteur", "env", { "pa", "pd", "ps", "pr", "pamt" }, "env" },
                { "lfo1", "LFO 1", "lfo", { "l1sync", "l1rate", "l1shape", "l1delay" }, "mod" },
                { "lfo2", "LFO 2", "lfo", { "l2sync", "l2rate", "l2shape", "l2delay" }, "mod" },
                { "surface", "Surface et bosse", "global", { "phon", "tiltx", "tilty", "fric", "ptime", "bumph", "bumpx", "bumpy", "bumpw", "bumpshape" }, "mod" },
                { "edges", "Bords", "global", { "edget", "edgeb", "edgel", "edger", "wrap" }, "mod" },
                { "launch", "Lancement", "global", { "lsrc", "lforce", "langle", "lrand", "startx", "starty" }, "mod" },
            };
            for (int l = 0; l < numLanes; ++l)
            {
                const auto k = std::string ("sq") + letters[l];
                const auto nm = std::string ("S\xC3\xA9quence ") + (char) ('A' + l);
                m.sections.push_back ({ k, nm, "lfo", { k + "start", k + "end", k + "dir", k + "div", k + "smooth", k + "prob", "seqon", "seqretrig" }, "mod" });
                std::vector<std::string> a, b;
                for (int s = 0; s < 8; ++s) { a.push_back (k + "s" + std::to_string (s + 1)); b.push_back (k + "s" + std::to_string (s + 9)); }
                m.sections.push_back ({ k + "1", nm + " \xC2\xB7 pas 1-8", "lfosteps", a, "mod" });
                m.sections.push_back ({ k + "2", nm + " \xC2\xB7 pas 9-16", "lfosteps", b, "mod" });
            }

            // routages d'origine : la bille parcourt les tables et ouvre le filtre
            // source n° (liste de la matrice) : 0 aucune, 1..5 env., 6..10 LFO, 11..17 jeu, 18..25 macros, 26.. sources propres
            constexpr float sX = 26, sY = 27, sDist = 28, sAng = 29, sHit = 34, sA = 35, sB = 36, sC = 37, sD = 38;
            constexpr float sL1 = 6, sL2 = 7, sEnvF = 2, sMacro1 = 18;
            (void) sAng; (void) sEnvF;
            using PV = std::vector<std::pair<std::string, float>>;
            auto pr = [&] (const std::string& name, const std::string& cat, PV v) { m.presets.push_back ({ name, std::move (v), cat }); };
            pr ("Init", "Init", {});
            pr ("Bille dans le puits", "Nappes", { { "level", -7.6f }, { "o1taba", 1 }, { "o1tabb", 6 }, { "o1ab", 40 }, { "aa", 400 }, { "ar", 1800 }, { "cutoff", 3500 },
                { "mx1_src", sX }, { "mx1_dst@o1pos", 0 }, { "mx1_amt", 45 }, { "mx2_src", sY }, { "mx2_dst@cutoff", 0 }, { "mx2_amt", 25 },
                { "mx3_src", sL1 }, { "mx3_dst@o1ab", 0 }, { "mx3_amt", 30 } });
            pr ("Flipper", "Synth\xC3\xA9s", { { "level", 4.1f }, { "tilty", 35 }, { "fric", 5 }, { "ptime", 130 }, { "bumph", 70 }, { "bumpy", -20 }, { "bumpw", 22 }, { "bumpshape", 1 },
                { "edget", 60 }, { "edgeb", 60 }, { "edgel", 60 }, { "edger", 60 }, { "o1taba", 2 }, { "o1tabb", 3 }, { "o1ab", 50 }, { "ad", 300 }, { "as", 20 }, { "ar", 300 },
                { "mx1_src", sX }, { "mx1_dst@o1pos", 0 }, { "mx1_amt", 60 }, { "mx2_src", sHit }, { "mx2_dst@cutoff", 0 }, { "mx2_amt", 40 } });
            pr ("Plan\xC3\xA8te", "Nappes", { { "level", -8.4f }, { "fric", 0 }, { "bumph", -100 }, { "bumpw", 70 }, { "bumpshape", 1 }, { "edget", -40 }, { "edgeb", -40 }, { "edgel", -40 }, { "edger", -40 },
                { "o1taba", 6 }, { "o1tabb", 1 }, { "o2lvl", 45 }, { "o2taba", 4 }, { "o2tabb", 6 }, { "aa", 900 }, { "ar", 2500 }, { "ftype", 0 }, { "fmulti", 20 },
                { "mx1_src", sDist }, { "mx1_dst@o1ab", 0 }, { "mx1_amt", 70 }, { "mx2_src", sY }, { "mx2_dst@o2pos", 0 }, { "mx2_amt", 50 }, { "mx3_src", sX }, { "mx3_dst@fmulti", 0 }, { "mx3_amt", 35 } });
            pr ("Ast\xC3\xA9ro\xC3\xAF" "des", "Synth\xC3\xA9s", { { "level", -7.7f }, { "tiltx", 25 }, { "tilty", -10 }, { "fric", 2 }, { "ptime", 90 }, { "bumph", 0 }, { "wrap", 1 },
                { "o1taba", 3 }, { "o1mtype", 5 }, { "o1morph", 30 }, { "cutoff", 2800 }, { "reso", 40 },
                { "mx1_src", sX }, { "mx1_dst@o1morph", 0 }, { "mx1_amt", 50 }, { "mx2_src", sY }, { "mx2_dst@o1pos", 0 }, { "mx2_amt", 45 } });
            pr ("Basse \xC3\xA0 plis", "Basses", { { "level", 4.4f }, { "vmode", 1 }, { "o1taba", 3 }, { "o1pos", 15 }, { "o1oct", -1 }, { "sub", 45 }, { "cutoff", 700 }, { "reso", 30 },
                { "fenv", 55 }, { "fd", 260 }, { "fs", 0 }, { "ad", 500 }, { "as", 70 }, { "ar", 120 }, { "lsrc", 2 }, { "lforce", 40 },
                { "mx1_src", sDist }, { "mx1_dst@o1pos", 0 }, { "mx1_amt", 40 } });
            pr ("Basse cloche FM", "Basses", { { "level", 3.6f }, { "vmode", 1 }, { "o1taba", 2 }, { "o1pos", 20 }, { "o1oct", -1 }, { "o1mtype", 10 }, { "o1morph", 20 }, { "o2lvl", 40 }, { "o2taba", 0 },
                { "o2oct", -1 }, { "o2semi", 0 }, { "o2fine", 0 }, { "cutoff", 1200 }, { "fenv", 45 }, { "fd", 300 }, { "fs", 10 }, { "ar", 150 },
                { "mx1_src", sA }, { "mx1_dst@o1morph", 0 }, { "mx1_amt", 35 } });
            pr ("Voyelles qui roulent", "Synth\xC3\xA9s", { { "level", 0.7f }, { "o1taba", 1 }, { "o1tabb", 1 }, { "o1pos", 50 }, { "ftype", 0 }, { "fmulti", 35 }, { "reso", 45 }, { "fric", 10 }, { "bumph", -80 },
                { "bumpshape", 3 }, { "bumpw", 55 }, { "mx1_src", sAng }, { "mx1_dst@o1pos", 0 }, { "mx1_amt", 90 }, { "mx2_src", sB }, { "mx2_dst@cutoff", 0 }, { "mx2_amt", 20 } });
            pr ("Cordes frott\xC3\xA9" "es", "Cordes", { { "level", -12.8f }, { "o1taba", 6 }, { "o1tabb", 6 }, { "o1pos", 40 }, { "o2lvl", 60 }, { "o2taba", 6 }, { "o2tabb", 4 }, { "o2semi", 0 }, { "o2fine", 9 },
                { "aa", 350 }, { "ar", 1400 }, { "cutoff", 5200 }, { "reso", 10 }, { "fenv", 10 }, { "vmode", 0 },
                { "mx1_src", sL1 }, { "mx1_dst@o1pos", 0 }, { "mx1_amt", 25 }, { "mx2_src", sY }, { "mx2_dst@o2pos", 0 }, { "mx2_amt", 35 } });
            pr ("S\xC3\xA9quence de mouvement", "S\xC3\xA9quences", { { "level", 4.2f }, { "o1taba", 0 }, { "o1tabb", 3 }, { "o1ab", 30 }, { "ad", 220 }, { "as", 30 }, { "ar", 250 }, { "cutoff", 1600 }, { "reso", 45 },
                { "mx1_src", sA }, { "mx1_dst@cutoff", 0 }, { "mx1_amt", 35 }, { "mx2_src", sB }, { "mx2_dst@o1pos", 0 }, { "mx2_amt", 45 }, { "mx3_src", sC }, { "mx3_dst@o1ab", 0 }, { "mx3_amt", 40 },
                { "mx4_src", sD }, { "mx4_dst@spread", 0 }, { "mx4_amt", 30 } });
            pr ("Num\xC3\xA9rique granuleux", "Synth\xC3\xA9s", { { "level", -4.9f }, { "o1taba", 7 }, { "o1tabb", 5 }, { "o1mtype", 9 }, { "o1morph", 25 }, { "o1modif", 4 }, { "cutoff", 6000 }, { "ftype", 7 }, { "reso", 35 },
                { "ad", 350 }, { "as", 40 }, { "mx1_src", sX }, { "mx1_dst@o1ab", 0 }, { "mx1_amt", 60 }, { "mx2_src", sHit }, { "mx2_dst@o1morph", 0 }, { "mx2_amt", 50 } });
            pr ("Lead synchro", "Leads", { { "level", -6.7f }, { "vmode", 2 }, { "univoices", 4 }, { "unidetune", 14 }, { "o1taba", 0 }, { "o1pos", 60 }, { "o1mtype", 5 }, { "o1morph", 40 }, { "glide", 40 },
                { "cutoff", 4200 }, { "reso", 20 }, { "mx1_src", sY }, { "mx1_dst@o1morph", 0 }, { "mx1_amt", 35 }, { "mx2_src", sL2 }, { "mx2_dst@tune", 0 }, { "mx2_amt", 1 } });
            pr ("Mod 1 : balayage", "Nappes", { { "level", -3.8f }, { "o1taba", 4 }, { "o1tabb", 6 }, { "aa", 600 }, { "ar", 2000 }, { "macro1", 30 },
                { "mx1_src", sMacro1 }, { "mx1_dst@o1pos", 0 }, { "mx1_amt", 100 }, { "mx2_src", sMacro1 }, { "mx2_dst@o1ab", 0 }, { "mx2_amt", 80 } });

            HubSpec hub;
            hub.extEnvs = { "Env. ampli", "Env. filtre", "Env. hauteur" };
            hub.extLfos = { "LFO 1", "LFO 2" };
            hub.envs = 2; hub.firstEnv = 4; hub.envName = "Env.";
            hub.lfos = 3; hub.firstLfo = 3;
            hub.slots = 32; hub.macros = 4; hub.arp = true; hub.arpSteps = true;
            hub.extraSources = extraNames();
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m);

            // formes montrées par l'interface : 8 tables × 12 ondes, 64 points chacune
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
}

/** Une voix : deux oscillateurs à tables, sous-oscillateur, bruit, filtre, trois enveloppes, deux LFO. */
class GlutaVoice
{
public:
    void prepare (double s) { sr = s; ea.prepare (s); ef.prepare (s); ep.prepare (s); reset(); }
    void reset() noexcept
    {
        ph1 = ph2 = phSub = 0.0; last2 = 0.0; ladder.reset(); svf.reset(); sk.reset(); ea.reset(); ef.reset(); ep.reset(); dc.reset(); active = false;
    }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true; releaseAfterStart = false;
        if (! legatoStart) lfoAge = 0.0;
        rng = rng * 1664525u + 1013904223u;
        pan = (double) (rng >> 8) / 8388608.0 - 1.0;
    }
    // v0.47 (maj. 5 nov.) : relâchée avant d'avoir joué (même bloc) : la note démarre puis se relâche aussitôt
    void release() noexcept { ea.gate (false); ef.gate (false); ep.gate (false); if (pending) releaseAfterStart = true; }
    bool isActive() const noexcept { return active; }

    void modSources (double* env, double* lfo) const noexcept
    {
        env[0] = lastA; env[1] = lastF; env[2] = lastP;
        lfo[0] = lastL1; lfo[1] = lastL2;
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace gluta;
        const auto& wb = bank();
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! ea.isActive()) { ea.gate (true); ef.gate (true); ep.gate (true); }
            if (releaseAfterStart) { releaseAfterStart = false; ea.gate (false); ef.gate (false); ep.gate (false); }
        }
        ea.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        ef.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);
        ep.set (p[pA], p[pD], p[pS] / 100.0, p[pR]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level] + 6.0f) * velocityGain (vel, p[velAmp] / 100.0);
        const auto pn = spread != 0.0 ? spread * std::max (0.6, p[spreadAmt] / 100.0) : pan * p[spreadAmt] / 100.0;
        const auto gl = 1.0 - std::max (0.0, pn) * 0.6, gr = 1.0 + std::min (0.0, pn) * 0.6;
        const auto det = spread * p[uniDetune] / 100.0;
        const Osc os1 = oscOf (p, 0), os2 = oscOf (p, oscStride);
        const auto off1 = 12.0 * std::round (p[o1Oct]) + std::round (p[o1Semi]) + p[o1Fine] / 100.0 + det;
        const auto off2 = 12.0 * std::round (p[o2Oct]) + std::round (p[o2Semi]) + p[o2Fine] / 100.0 - det;
        const auto lv1 = p[o1Lvl] / 100.0, lv2 = p[o2Lvl] / 100.0, lvSub = p[subLvl] / 100.0, lvN = p[noiseLvl] / 100.0;
        // l'autre oscillateur est utile s'il sonne ou s'il module celui qui sonne
        const bool need2 = lv2 > 0.0 || (os1.mtype >= 10 && os1.amt > 0.0);
        const bool need1 = lv1 > 0.0 || lvSub > 0.0 || (os2.mtype >= 10 && os2.amt > 0.0 && lv2 > 0.0);
        const auto ftype = std::clamp ((int) p[fType], 0, 9);
        const auto res = p[reso] / 100.0, drive = dbToGain (p[fDrive]), multi = p[fMulti] / 100.0;
        const auto ktAmt = p[fKey] / 100.0, envOct = p[fEnv] / 100.0 * 7.0, velOct = p[fVel] / 100.0 * (vel - 0.6) * 4.0;
        const auto pAmtSt = (double) p[pAmt];
        double a = 0.0, ff = 0.0, pe = 0.0, l1 = 0.0, l2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            a = ea.next(); ff = ef.next(); pe = ep.next();
            lfoAge += 1.0 / sr;
            l1 = lfo1.next ((int) p[l1Shape], p[l1Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l1Delay]));
            l2 = lfo2.next ((int) p[l2Shape], p[l2Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l2Delay]));
            const auto note = pitch.next (glideCoef) + p[tune] + pe * pAmtSt;
            const auto dt1 = noteHz (note + off1) / sr, dt2 = noteHz (note + off2) / sr;
            double s1 = 0.0, s2 = 0.0;
            if (need1) s1 = osc (wb, os1, ph1, dt1, last2);
            if (need2) { s2 = osc (wb, os2, ph2, dt2, s1); last2 = s2; }
            double sub = 0.0;
            if (lvSub > 0.0) { sub = fastSin2Pi (phSub); phSub += 0.5 * std::min (dt1, 0.45); phSub -= std::floor (phSub); }
            const auto x = s1 * lv1 + s2 * lv2 + sub * lvSub + (lvN > 0.0 ? noise.next() * lvN : 0.0);

            const auto kt = ktAmt * (note - 60.0) / 12.0;
            const auto fc = std::clamp ((double) p[cutoff] * fastExp2 (kt + envOct * ff + velOct), 16.0, 0.45 * sr);
            const auto in = x * 0.35;
            double y;
            switch (ftype)
            {
                case 0: y = svf.processMorph (in, fc, res, sr, multi); break;
                case 1: y = ladder.process (in, fc, res, sr, Ladder::lp24, drive); break;
                case 2: y = ladder.process (in, fc, res, sr, Ladder::lp12, drive); break;
                case 3: y = ladder.process (in, fc, res, sr, Ladder::hp24, drive); break;
                case 4: y = ladder.process (in, fc, res, sr, Ladder::hp12, drive); break;
                case 5: y = ladder.process (in, fc, res, sr, Ladder::bp12, drive); break;
                case 6: { const auto o = svf.process (in, fc, res * 0.9, sr); y = o.low + o.high; break; }
                case 7: y = sk.process (in * drive, false, fc, res * 1.95, 1.0 + 1.5 * res, sr) / std::sqrt (drive); break;
                case 8: y = sk.process (in * drive, true, fc, res * 1.95, 1.0 + 1.5 * res, sr) / std::sqrt (drive); break;
                default: y = svf.process (softClip (in * drive), fc, res * 0.55, sr).low; break;
            }
            if (ftype == 0 || ftype == 6) y = softClip (y * drive) / std::sqrt (drive);
            const auto out = dc.process (y) * a * gain;
            L[i] += (float) (out * gl);
            R[i] += (float) (out * gr);
        }
        lastA = a; lastF = ff; lastP = pe; lastL1 = l1; lastL2 = l2;
        if (! ea.isActive()) active = false;
    }

private:
    struct Osc { int ta, tb, mtype, modif; double pos, ab, amt; };

    static Osc oscOf (const float* p, int off) noexcept
    {
        using namespace gluta;
        return { std::clamp ((int) p[o1TabA + off], 0, numTables - 1), std::clamp ((int) p[o1TabB + off], 0, numTables - 1),
                 std::clamp ((int) p[o1MorphT + off], 0, 12), std::clamp ((int) p[o1Modif + off], 0, 6),
                 std::clamp (p[o1Pos + off] / 100.0, 0.0, 1.0), std::clamp (p[o1Ab + off] / 100.0, 0.0, 1.0), std::clamp (p[o1Morph + off] / 100.0, 0.0, 1.0) };
    }

    /** Lecture d'une table à une position (fondu entre deux ondes voisines). */
    static double readTable (const WaveBank& wb, int table, double pos, WaveBank::Mip m, double ph) noexcept
    {
        const auto fp = pos * (gluta::framesPerTable - 1);
        const auto k0 = std::min ((int) fp, gluta::framesPerTable - 2);
        const auto f = fp - (double) k0;
        const auto base = table * gluta::framesPerTable + k0;
        const auto y0 = (double) wb.read (base, m, ph);
        return f < 1.0e-4 ? y0 : y0 + ((double) wb.read (base + 1, m, ph) - y0) * f;
    }

    /** Un oscillateur : phase (morph de phase), lecture des tables A et B, morph d'amplitude, modificateur. */
    static double osc (const WaveBank& wb, const Osc& o, double& ph, double dt, double other) noexcept
    {
        dt = std::clamp (dt, 1.0e-7, 0.45);
        auto wrap = [] (double x) { return x - std::floor (x); };
        const auto am = o.amt;
        auto rp = ph;
        auto slope = 1.0;
        double window = 1.0;
        if (am > 0.0)
            switch (o.mtype)
            {
                case 1: { const auto e = 1.0 + 3.0 * am; rp = std::pow (ph, e); slope = e; break; }                     // étirer
                case 2: { const auto c = 1.0 + 3.0 * am; rp = std::min (1.0, ph * c); slope = c; break; }              // comprimer
                case 3: { const auto s = 2.0 * (1.0 - 0.5 * am); rp = ph < 0.5 ? ph * s : 1.0 - (ph - 0.5) * s; slope = s; break; } // refléter
                case 4: { const auto mir = ph < 0.5 ? ph : 1.0 - ph; rp = ph + am * (mir - ph); slope = 1.0 + am; break; } // miroir
                case 5: { const auto r = 1.0 + 7.0 * am; rp = wrap (ph * r); slope = r; window = 1.0 - 0.6 * am * ph; break; } // synchro
                case 6: { const auto r = 1.0 + 3.0 * am; rp = std::abs (std::fmod (ph * r, 2.0) - 1.0); slope = r; break; } // plier
                case 7: { const auto w = 1.0 - 0.85 * am; rp = ph < w ? ph / w : 0.0; slope = 1.0 / w; break; }          // pulsation
                case 8: { const auto q = 64.0 - 60.0 * am; rp = std::floor (ph * q) / q; break; }                        // quantifier
                case 10: rp = wrap (ph + 0.5 * am * other); slope = 1.0 + 2.0 * am; break;                               // FM
                default: break;
            }
        const auto mip = WaveBank::mipFor (std::min (0.45, dt * slope));
        double y;
        if (o.ab <= 1.0e-4) y = readTable (wb, o.ta, o.pos, mip, rp);
        else if (o.ab >= 1.0 - 1.0e-4) y = readTable (wb, o.tb, o.pos, mip, rp);
        else y = readTable (wb, o.ta, o.pos, mip, rp) * (1.0 - o.ab) + readTable (wb, o.tb, o.pos, mip, rp) * o.ab;
        if (am > 0.0)
            switch (o.mtype)
            {
                case 9: { const auto q = 16.0 - 14.0 * am; y = std::round (y * q) / q; break; }  // bits
                case 11: y *= (1.0 - am) + am * (0.5 + 0.5 * other); break;                      // AM
                case 12: y = y * (1.0 - am) + y * other * am; break;                             // anneau
                default: break;
            }
        switch (o.modif)
        {
            case 1: y = std::tanh (2.2 * y) * 0.9; break;
            case 2: y = 1.6 * std::abs (y) - 0.7; break;
            case 3: y = std::round (y * 6.0) / 6.0; break;
            case 4: y = std::round (y * 4.0) / 4.0; break;
            case 5: y = std::round (y * 12.0) / 12.0; break;
            case 6: y = std::tanh (1.4 * y) + 0.25 * std::sin (kPi * y); break;
            default: break;
        }
        ph += dt;
        ph -= std::floor (ph);
        return y * window;
    }

    double sr = 48000.0, pan = 0.0, spread = 0.0;
    double ph1 = 0.0, ph2 = 0.0, phSub = 0.0, last2 = 0.0;
    double lastA = 0.0, lastF = 0.0, lastP = 0.0, lastL1 = 0.0, lastL2 = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false, releaseAfterStart = false;
    std::uint32_t rng = 7;
    Ladder ladder;
    StateFilter svf;
    SallenKey12 sk;
    Adsr ea, ef, ep;
    SimpleLfo lfo1, lfo2;
    double lfoAge = 1.0e9;
    Glide pitch;
    WhiteNoise noise { 0xB0B5u };
    DcBlocker dc;
};

class Glutamake final : public SynthBase<GlutaVoice, 8>
{
public:
    Glutamake()
    {
        (void) gluta::bank(); // tables calculées à la création (fil de l'interface), jamais sur le fil audio
        for (int i = 0; i < 8; ++i) voices[(size_t) i].seed (0x51A7u * (std::uint32_t) (i + 1));
        for (auto& l : lanes) l = {};
    }
    const ModuleDesc& desc() const noexcept override { return gluta::desc(); }

    void reset() noexcept override
    {
        SynthBase::reset();
        bx = by = 0.0; bvx = 0.6; bvy = 0.9; hit = 0.0; haveLead = false;
        for (auto& l : lanes) l = {};
    }

    void noteOn (int note, float velocity) noexcept override
    {
        const bool wasSilent = ! SynthBase::isActive();
        SynthBase::noteOn (note, velocity);
        noteCount++;
        if (wasSilent) silentStart = true;
    }

    /** Pavé : 0 tenir la bille en (a, b) ; 1 la lâcher avec la vitesse (a, b) ; 2 la lancer. */
    void touch (int kind, float a, float b) noexcept override
    {
        if (kind == 0) { holdX.store (std::clamp (a, -1.0f, 1.0f)); holdY.store (std::clamp (b, -1.0f, 1.0f)); holding.store (true); }
        else if (kind == 1) { throwVX.store (std::clamp (a, -6.0f, 6.0f)); throwVY.store (std::clamp (b, -6.0f, 6.0f)); holding.store (false); throwCount.fetch_add (1); }
        else if (kind == 2) launchCount.fetch_add (1);
    }

    /** Écran : bille (x, y), pas joué de chaque couloir, chocs, une voix sonne, valeur des couloirs. */
    int status (float* out, int max) const noexcept override
    {
        if (max < 13) return 0;
        out[0] = shownX.load (std::memory_order_relaxed); out[1] = shownY.load (std::memory_order_relaxed);
        for (int l = 0; l < gluta::numLanes; ++l)
        {
            out[2 + l] = (float) shownStep[(size_t) l].load (std::memory_order_relaxed);
            out[9 + l] = shownLane[(size_t) l].load (std::memory_order_relaxed);
        }
        out[6] = (float) (shownHits.load (std::memory_order_relaxed) % 1000);
        out[7] = sounding.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        out[8] = shownHit.load (std::memory_order_relaxed);
        return 13;
    }

protected:
    const float* adjust (const float* p) noexcept override
    { return syncRates (p, gluta::numSlots, { { gluta::l1Rate, gluta::l1Sync }, { gluta::l2Rate, gluta::l2Sync } }); }

    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[gluta::voices], 1, 8);
        switch ((int) p[gluta::vMode])
        {
            case 1: setMode (true, true, v); break;
            case 2: setMode (true, true, v, std::clamp ((int) p[gluta::uniVoices], 2, 8)); break;
            default: setMode (v == 1, true, v); break;
        }
    }

    /** Réglages de la physique et des couloirs vus par la voix de la dernière note (modulés). */
    void voiceDone (int i, const float* vp) noexcept override
    {
        if (i != lastVoice) return;
        std::copy (vp + gluta::phOn, vp + gluta::numSlots, lead.begin() + gluta::phOn);
        haveLead = true;
    }

    void blockStart (const float* common, int n) noexcept override
    {
        using namespace gluta;
        const bool anyActive = SynthBase::isActive();
        // réglages : ceux de la voix de la dernière note (modulés) si une voix joue, sinon ceux de la piste
        const float* q = common;
        if (haveLead && anyActive) { std::copy (common, common + phOn, lead.begin()); q = lead.data(); }
        else haveLead = false;
        const auto dtBlock = (double) n / sr;
        const int notes = noteCount; noteCount = 0;
        const bool retrigAll = silentStart || (notes > 0 && q[seqRetrig] > 0.5f);
        silentStart = false;

        // --- séquence de mouvement
        bool laneAStep = false;
        const bool seqRun = q[seqOn] > 0.5f;
        for (int l = 0; l < numLanes; ++l)
        {
            auto& L = lanes[(size_t) l];
            const auto b = laneBase + l * laneStride;
            const auto s0 = std::clamp ((int) std::lround (q[b + lfStart]) - 1, 0, laneSteps - 1);
            const auto e0 = std::clamp ((int) std::lround (q[b + lfEnd]) - 1, 0, laneSteps - 1);
            const auto lo = std::min (s0, e0), hi = std::max (s0, e0);
            const auto div = std::clamp ((int) q[b + lfDiv], 0, laneDivNote);
            int advances = 0;
            if (retrigAll) { L.pos = -1; L.acc = 0.0; L.dir = 1; }
            if (seqRun)
            {
                if (div == laneDivNote) advances = notes;
                else
                {
                    if (L.pos < 0) advances = 1; // premier pas tout de suite
                    L.acc += dtBlock * std::clamp (bpm, 20.0, 400.0) / 60.0 / laneDivQuarters (div);
                    while (L.acc >= 1.0 && advances < 8) { L.acc -= 1.0; ++advances; }
                    if (L.acc >= 1.0) L.acc -= std::floor (L.acc);
                }
            }
            for (int k = 0; k < advances; ++k)
            {
                const auto dir = std::clamp ((int) q[b + lfDir], 0, 3);
                int next;
                if (L.pos < lo || L.pos > hi) next = dir == 1 ? hi : lo;
                else if (dir == 3) next = lo + (int) (rand01() * (hi - lo + 1)) % (hi - lo + 1);
                else if (dir == 1) next = L.pos - 1 < lo ? hi : L.pos - 1;
                else if (dir == 2)
                {
                    if (hi == lo) next = lo;
                    else { next = L.pos + L.dir; if (next > hi || next < lo) { L.dir = -L.dir; next = L.pos + L.dir; } }
                }
                else next = L.pos + 1 > hi ? lo : L.pos + 1;
                L.pos = std::clamp (next, 0, laneSteps - 1);
                // probabilité : le pas ne joue pas, la valeur précédente reste
                if (rand01() * 100.0 < q[b + lfProb] || L.target == 1.0e9) L.target = std::clamp (q[b + L.pos] / 100.0, -1.0, 1.0);
                if (l == 0) laneAStep = true;
            }
            if (L.target == 1.0e9) L.target = std::clamp (q[b + std::max (0, L.pos)] / 100.0, -1.0, 1.0);
            // lissage : jusqu'à la durée d'un pas
            const auto sm = q[b + lfSmooth] / 100.0;
            if (sm <= 0.0) L.value = L.target;
            else
            {
                const auto stepSec = div == laneDivNote ? 0.25 : 60.0 / std::clamp (bpm, 20.0, 400.0) * laneDivQuarters (div);
                const auto tau = std::max (1.0e-3, sm * stepSec * 0.5);
                L.value += (L.target - L.value) * (1.0 - std::exp (-dtBlock / tau));
            }
            hub.setExtra (exLaneA + l, L.value);
            shownStep[(size_t) l].store (std::max (0, L.pos), std::memory_order_relaxed);
            shownLane[(size_t) l].store ((float) L.value, std::memory_order_relaxed);
        }

        // --- gestes venus de l'écran
        const bool held = holding.load (std::memory_order_relaxed);
        if (held) { bx = holdX.load (std::memory_order_relaxed); by = holdY.load (std::memory_order_relaxed); bvx = bvy = 0.0; }
        const auto tc = throwCount.load (std::memory_order_relaxed);
        if (tc != lastThrow) { lastThrow = tc; if (! held) { bvx = throwVX.load(); bvy = throwVY.load(); } }
        const auto lc = launchCount.load (std::memory_order_relaxed);
        bool doLaunch = false, fromStart = false;
        if (lc != lastLaunch) { lastLaunch = lc; doLaunch = true; }
        const auto src = std::clamp ((int) q[launchSrc], 0, 3);
        if (notes > 0 && (src == 1 || src == 2)) { doLaunch = true; fromStart = src == 2; }
        if (laneAStep && src == 3) doLaunch = true;
        if (doLaunch && ! held)
        {
            if (fromStart) { bx = std::clamp (q[startX] / 100.0, -1.0, 1.0); by = std::clamp (q[startY] / 100.0, -1.0, 1.0); }
            const auto ang = (q[launchAngle] + (rand01() - 0.5) * q[launchRand] * 1.8) * kPi / 180.0;
            const auto sp = 0.4 + q[launchForce] / 100.0 * 2.6;
            bvx = std::cos (ang) * sp; bvy = std::sin (ang) * sp;
        }

        // --- physique de la bille
        if (q[phOn] > 0.5f && ! held)
        {
            const auto t = dtBlock * std::clamp ((double) q[timeScale], 10.0, 400.0) / 100.0;
            constexpr int sub = 2;
            const auto h = t / sub;
            const auto tx = q[tiltX] / 100.0 * 2.2, ty = q[tiltY] / 100.0 * 2.2;
            const auto damp = std::exp (-q[fric] / 100.0 * 3.2 * h);
            const bool wrap = q[wrapEdges] > 0.5f;
            auto rest = [] (double e) { return std::clamp (1.0 + e / 100.0 * 0.9, 0.0, 1.8); };
            for (int s = 0; s < sub; ++s)
            {
                double fx, fy;
                bumpForce (q, bx, by, fx, fy);
                bvx += (tx + fx * 2.4) * h; bvy += (ty + fy * 2.4) * h;
                bvx *= damp; bvy *= damp;
                bx += bvx * h; by += bvy * h;
                if (wrap)
                {
                    if (bx > 1.0) bx -= 2.0; if (bx < -1.0) bx += 2.0;
                    if (by > 1.0) by -= 2.0; if (by < -1.0) by += 2.0;
                }
                else
                {
                    bool hitNow = false;
                    if (bx > 1.0) { bx = 1.0; bvx = -std::abs (bvx) * rest (q[edgeR]); hitNow = true; }
                    if (bx < -1.0) { bx = -1.0; bvx = std::abs (bvx) * rest (q[edgeL]); hitNow = true; }
                    if (by > 1.0) { by = 1.0; bvy = -std::abs (bvy) * rest (q[edgeT]); hitNow = true; }
                    if (by < -1.0) { by = -1.0; bvy = std::abs (bvy) * rest (q[edgeB]); hitNow = true; }
                    if (hitNow && std::hypot (bvx, bvy) > 0.05) { hit = 1.0; ++hits; }
                }
                const auto spd = std::hypot (bvx, bvy);
                if (spd > 6.0) { bvx *= 6.0 / spd; bvy *= 6.0 / spd; }
            }
            if (! std::isfinite (bx) || ! std::isfinite (by) || ! std::isfinite (bvx) || ! std::isfinite (bvy)) { bx = by = bvx = bvy = 0.0; }
        }
        hit *= std::exp (-dtBlock / 0.06);

        const auto dist = std::min (1.0, std::hypot (bx, by) / std::sqrt (2.0));
        auto ang = std::atan2 (by, bx) / kTwoPi; if (ang < 0.0) ang += 1.0;
        hub.setExtra (exX, bx); hub.setExtra (exY, by); hub.setExtra (exDist, dist); hub.setExtra (exAngle, ang);
        hub.setExtra (exPosX, std::max (0.0, bx)); hub.setExtra (exNegX, std::max (0.0, -bx));
        hub.setExtra (exPosY, std::max (0.0, by)); hub.setExtra (exNegY, std::max (0.0, -by));
        hub.setExtra (exHit, hit);
        shownX.store ((float) bx, std::memory_order_relaxed); shownY.store ((float) by, std::memory_order_relaxed);
        shownHits.store (hits, std::memory_order_relaxed); shownHit.store ((float) hit, std::memory_order_relaxed);
        sounding.store (anyActive, std::memory_order_relaxed);
    }

private:
    struct Lane { int pos = -1, dir = 1; double acc = 0.0, value = 0.0, target = 1.0e9; };

    /** Force de la bosse en (x, y) : creux (hauteur < 0) qui attire, colline (> 0) qui repousse. */
    static void bumpForce (const float* q, double x, double y, double& fx, double& fy) noexcept
    {
        using namespace gluta;
        const auto hgt = q[bumpH] / 100.0, cx = q[bumpX] / 100.0, cy = q[bumpY] / 100.0;
        const auto w = std::max (0.05, q[bumpW] / 100.0 * 0.9);
        const auto dx = x - cx, dy = y - cy, r = std::hypot (dx, dy) + 1.0e-6, u = r / w;
        double df = 0.0;
        switch (std::clamp ((int) q[bumpShape], 0, 3))
        {
            case 0: df = -u * std::exp (-0.5 * u * u); break;                                    // cloche
            case 1: df = u < 1.0 ? -1.0 : 0.0; break;                                            // cône
            case 2: { const auto u6 = std::pow (u, 6.0); df = -6.0 * std::pow (u, 5.0) / ((1.0 + u6) * (1.0 + u6)); break; } // plateau
            default: { const auto k = u - 1.0; df = -8.0 * k * std::exp (-4.0 * k * k); break; } // anneau
        }
        const auto g = hgt * df / w;
        fx = -g * dx / r; fy = -g * dy / r;
    }

    double rand01() noexcept { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (double) (rng >> 8) / 16777216.0; }

    std::array<float, grv::engine::instrumentSlots> lead {};
    bool haveLead = false, silentStart = false;
    int noteCount = 0;
    double bx = 0.0, by = 0.0, bvx = 0.6, bvy = 0.9, hit = 0.0;
    int hits = 0;
    std::array<Lane, gluta::numLanes> lanes {};
    std::uint32_t rng = 0xC0FFEEu;
    std::atomic<bool> holding { false }, sounding { false };
    std::atomic<float> holdX { 0.0f }, holdY { 0.0f }, throwVX { 0.0f }, throwVY { 0.0f };
    std::atomic<int> throwCount { 0 }, launchCount { 0 };
    int lastThrow = 0, lastLaunch = 0;
    std::atomic<float> shownX { 0.0f }, shownY { 0.0f }, shownHit { 0.0f };
    std::atomic<int> shownHits { 0 };
    std::array<std::atomic<int>, gluta::numLanes> shownStep {};
    std::array<std::atomic<float>, gluta::numLanes> shownLane {};
};

} // namespace grv::instruments
