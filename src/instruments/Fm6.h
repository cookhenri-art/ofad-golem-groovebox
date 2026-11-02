/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Old Metal » — synthèse FM à 6 opérateurs, principe des claviers FM numériques des années 80
    (architecture reprise ; ni nom, ni panneau, ni son d'usine d'origine — nos sons sont faits ici) :
      - 6 opérateurs sinus par voix, reliés par 32 algorithmes (qui module qui, qui s'entend) ;
        rétroaction 0 à 7 sur un opérateur par algorithme (boucle sur 3 ou 2 opérateurs pour les
        algorithmes 4 et 6) ;
      - par opérateur : rapport de fréquence (grossier 0,5 / 1 à 31 + fin) ou fréquence fixe
        (1, 10, 100, 1000 Hz × fin), désaccord ±7, niveau 0-99, enveloppe à 4 vitesses et 4 niveaux
        (départ au niveau 4, maintien au niveau 3), sensibilité à la vélocité, à la modulation
        d'amplitude, au clavier (vitesses), échelle de niveau au clavier (point de rupture, côtés
        gauche et droit, profondeur, courbe linéaire ou exponentielle, en plus ou en moins) ;
      - enveloppe de hauteur à 4 vitesses / 4 niveaux (50 = au centre, ±4 octaves) ;
      - LFO commun (triangle, scies, carré, sinus, aléatoire échantillonné), retard, profondeurs de
        hauteur et d'amplitude, recalage à la note, calage sur le tempo ; molette et pression vers
        le vibrato et la modulation d'amplitude ;
      - phases des opérateurs recalées à la note (ou libres) ; poly 16 voix, mono, mono legato,
        portamento ; caractère « Années 80 » : convertisseur à virgule flottante à 10 bits de mantisse.

    Modèle (choix documentés) :
      - niveaux en unités logarithmiques (1 unité = 0,75 dB) : niveau de sortie et niveau
        d'enveloppe passent par la même table (0-19 non linéaire, puis linéaire), et s'ajoutent ;
      - vitesses : pente en unités par seconde = 2,13 × 2^(q/4), q = vitesse × 41/64 (+ vitesse au
        clavier) : une descente complète dure ~60 s à 0, ~0,15 s à 50, <1 ms à 99 ; les montées partent
        de −66 dB et accélèrent quand le niveau est bas (montées plus rapides que les descentes) ;
      - un modulateur au niveau 99 dévie la phase de sa cible d'un cycle (2π) ; la rétroaction 7
        vaut un demi-cycle, chaque cran en dessous la moitié ;
      - enveloppes, hauteur et LFO calculés par bloc (64 échantillons au plus), gains interpolés.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "FactoryBanks.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace fm6
{
    enum Global
    {
        algo = 0, feedback, tune, oscSync, level, voices, vMode, glide, character,
        lfoSpeed, lfoSync, lfoDelay, lfoPmd, lfoAmd, lfoKeySync, lfoWave, pms,
        pr1, pr2, pr3, pr4, pl1, pl2, pl3, pl4,
        wheelPm, wheelAm, pressPm, pressAm,
        numGlobal
    };
    // réglages d'un opérateur (6 groupes à la suite, mêmes positions)
    enum OpField
    {
        oMode = 0, oCoarse, oFine, oDetune, oLevel, oR1, oR2, oR3, oR4, oL1, oL2, oL3, oL4,
        oVel, oAms, oRateScale, oBreak, oLDepth, oRDepth, oLCurve, oRCurve,
        opStride
    };
    constexpr int numOps = 6;
    constexpr int opBase = numGlobal;
    constexpr int numSlots = opBase + numOps * opStride;
    constexpr int opParam (int op, int field) noexcept { return opBase + op * opStride + field; }

    /** Algorithme : modulateurs de chaque opérateur (bits : opérateur 1 = bit 0), opérateurs entendus,
        opérateur à rétroaction et source de sa rétroaction (lui-même, ou un autre pour 4 et 6). */
    struct Algorithm { std::uint8_t mods[numOps]; std::uint8_t carriers; std::int8_t fbOp, fbSrc; };

    inline const std::array<Algorithm, 32>& algorithms()
    {
        constexpr std::uint8_t O1 = 1, O2 = 2, O3 = 4, O4 = 8, O5 = 16, O6 = 32;
        static const std::array<Algorithm, 32> a {{
            /*  1 */ { { O2, 0, O4, O5, O6, 0 }, O1 | O3, 5, 5 },
            /*  2 */ { { O2, 0, O4, O5, O6, 0 }, O1 | O3, 1, 1 },
            /*  3 */ { { O2, O3, 0, O5, O6, 0 }, O1 | O4, 5, 5 },
            /*  4 */ { { O2, O3, 0, O5, O6, 0 }, O1 | O4, 5, 3 },
            /*  5 */ { { O2, 0, O4, 0, O6, 0 }, O1 | O3 | O5, 5, 5 },
            /*  6 */ { { O2, 0, O4, 0, O6, 0 }, O1 | O3 | O5, 5, 4 },
            /*  7 */ { { O2, 0, O4 | O5, 0, O6, 0 }, O1 | O3, 5, 5 },
            /*  8 */ { { O2, 0, O4 | O5, 0, O6, 0 }, O1 | O3, 3, 3 },
            /*  9 */ { { O2, 0, O4 | O5, 0, O6, 0 }, O1 | O3, 1, 1 },
            /* 10 */ { { O2, O3, 0, O5 | O6, 0, 0 }, O1 | O4, 2, 2 },
            /* 11 */ { { O2, O3, 0, O5 | O6, 0, 0 }, O1 | O4, 5, 5 },
            /* 12 */ { { O2, 0, O4 | O5 | O6, 0, 0, 0 }, O1 | O3, 1, 1 },
            /* 13 */ { { O2, 0, O4 | O5 | O6, 0, 0, 0 }, O1 | O3, 5, 5 },
            /* 14 */ { { O2, 0, O4, O5 | O6, 0, 0 }, O1 | O3, 5, 5 },
            /* 15 */ { { O2, 0, O4, O5 | O6, 0, 0 }, O1 | O3, 1, 1 },
            /* 16 */ { { O2 | O3 | O5, 0, O4, 0, O6, 0 }, O1, 5, 5 },
            /* 17 */ { { O2 | O3 | O5, 0, O4, 0, O6, 0 }, O1, 1, 1 },
            /* 18 */ { { O2 | O3 | O4, 0, 0, O5, O6, 0 }, O1, 2, 2 },
            /* 19 */ { { O2, O3, 0, O6, O6, 0 }, O1 | O4 | O5, 5, 5 },
            /* 20 */ { { O3, O3, 0, O5 | O6, 0, 0 }, O1 | O2 | O4, 2, 2 },
            /* 21 */ { { O3, O3, 0, O6, O6, 0 }, O1 | O2 | O4 | O5, 2, 2 },
            /* 22 */ { { O2, 0, O6, O6, O6, 0 }, O1 | O3 | O4 | O5, 5, 5 },
            /* 23 */ { { 0, O3, 0, O6, O6, 0 }, O1 | O2 | O4 | O5, 5, 5 },
            /* 24 */ { { 0, 0, O6, O6, O6, 0 }, O1 | O2 | O3 | O4 | O5, 5, 5 },
            /* 25 */ { { 0, 0, 0, O6, O6, 0 }, O1 | O2 | O3 | O4 | O5, 5, 5 },
            /* 26 */ { { 0, O3, 0, O5 | O6, 0, 0 }, O1 | O2 | O4, 5, 5 },
            /* 27 */ { { 0, O3, 0, O5 | O6, 0, 0 }, O1 | O2 | O4, 2, 2 },
            /* 28 */ { { O2, 0, O4, O5, 0, 0 }, O1 | O3 | O6, 4, 4 },
            /* 29 */ { { 0, 0, O4, 0, O6, 0 }, O1 | O2 | O3 | O5, 5, 5 },
            /* 30 */ { { 0, 0, O4, O5, 0, 0 }, O1 | O2 | O3 | O6, 4, 4 },
            /* 31 */ { { 0, 0, 0, 0, O6, 0 }, O1 | O2 | O3 | O4 | O5, 5, 5 },
            /* 32 */ { { 0, 0, 0, 0, 0, 0 }, O1 | O2 | O3 | O4 | O5 | O6, 5, 5 },
        }};
        return a;
    }

    /** Niveau 0-99 → unités logarithmiques 0-127 (0-19 non linéaire, puis linéaire). */
    inline double levelUnits (double v) noexcept
    {
        static constexpr double low[20] { 0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46 };
        v = std::clamp (v, 0.0, 99.0);
        if (v >= 19.0) return 28.0 + v;
        const auto i = (int) v;
        return low[i] + (low[std::min (19, i + 1)] - low[i]) * (v - i);
    }

    inline const std::vector<std::string>& curves()
    {
        static const std::vector<std::string> v { "\xE2\x88\x92Lin", "\xE2\x88\x92" "Exp", "+Exp", "+Lin" };
        return v;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            ModuleDesc m;
            m.type = "fm6";
            m.name = "Old Metal";
            m.family = "Synth\xC3\xA9s FM";
            m.principle = "FM \xC3\xA0 6 op\xC3\xA9rateurs, 32 algorithmes, enveloppes \xC3\xA0 4 vitesses et 4 niveaux, \xC3\xA9" "chelle au clavier";
            m.params = {
                I ("algo", "Algorithme", "", 1, 32, 1),
                I ("feedback", "R\xC3\xA9troaction", "", 0, 7, 0),
                I ("tune", "Transposition", "dt", -24, 24, 0),
                C ("oscsync", "Phases \xC3\xA0 la note", onOff, 1),
                P ("level", "Volume", "dB", -30, 6, -18),
                I ("voices", "Voix", "", 1, 16, 16),
                C ("vmode", "Jeu", { "Poly", "Mono", "Mono legato" }),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                C ("character", "Caract\xC3\xA8re", { "Propre", "Ann\xC3\xA9" "es 80" }),
                I ("lfospeed", "Vitesse LFO", "", 0, 99, 35),
                C ("lfosync", "Synchro LFO", lfoSyncChoices()),
                I ("lfodelay", "Retard LFO", "", 0, 99, 0),
                I ("lfopmd", "Prof. hauteur", "", 0, 99, 0),
                I ("lfoamd", "Prof. amplitude", "", 0, 99, 0),
                C ("lfoksync", "LFO recal\xC3\xA9", onOff, 1),
                C ("lfowave", "Forme LFO", { "Triangle", "Scie desc.", "Scie mont.", "Carr\xC3\xA9", "Sinus", "Al\xC3\xA9" "atoire" }),
                I ("pms", "Sens. hauteur", "", 0, 7, 3),
                I ("pr1", "Vitesse 1", "", 0, 99, 99), I ("pr2", "Vitesse 2", "", 0, 99, 99), I ("pr3", "Vitesse 3", "", 0, 99, 99), I ("pr4", "Vitesse 4", "", 0, 99, 99),
                I ("pl1", "Niveau 1", "", 0, 99, 50), I ("pl2", "Niveau 2", "", 0, 99, 50), I ("pl3", "Niveau 3", "", 0, 99, 50), I ("pl4", "Niveau 4", "", 0, 99, 50),
                P ("wheelpm", "Molette \xE2\x86\x92 vibrato", "%", 0, 100, 50),
                P ("wheelam", "Molette \xE2\x86\x92 amplitude", "%", 0, 100, 0),
                P ("presspm", "Pression \xE2\x86\x92 vibrato", "%", 0, 100, 0),
                P ("pressam", "Pression \xE2\x86\x92 amplitude", "%", 0, 100, 0),
            };
            for (int k = 0; k < numOps; ++k)
            {
                const auto o = "o" + std::to_string (k + 1) + "_";
                const auto n = "Op. " + std::to_string (k + 1) + " \xC2\xB7 ";
                m.params.push_back (C (o + "mode", n + "mode", { "Rapport", "Fixe" }));
                m.params.push_back (I (o + "coarse", n + "grossier", "", 0, 31, 1));
                m.params.push_back (I (o + "fine", n + "fin", "", 0, 99, 0));
                m.params.push_back (I (o + "detune", n + "d\xC3\xA9saccord", "", -7, 7, 0));
                m.params.push_back (I (o + "level", n + "niveau", "", 0, 99, k == 0 ? 99 : 0));
                for (int s = 1; s <= 4; ++s) m.params.push_back (I (o + "r" + std::to_string (s), n + "vitesse " + std::to_string (s), "", 0, 99, 99));
                for (int s = 1; s <= 4; ++s) m.params.push_back (I (o + "l" + std::to_string (s), n + "niveau " + std::to_string (s), "", 0, 99, s == 4 ? 0 : 99));
                m.params.push_back (I (o + "vel", n + "v\xC3\xA9locit\xC3\xA9", "", 0, 7, 0));
                m.params.push_back (I (o + "ams", n + "sens. amplitude", "", 0, 3, 0));
                m.params.push_back (I (o + "rscale", n + "vitesses au clavier", "", 0, 7, 0));
                m.params.push_back (I (o + "break", n + "point de rupture", "", 21, 108, 60));
                m.params.push_back (I (o + "ldepth", n + "profondeur gauche", "", 0, 99, 0));
                m.params.push_back (I (o + "rdepth", n + "profondeur droite", "", 0, 99, 0));
                m.params.push_back (C (o + "lcurve", n + "courbe gauche", curves()));
                m.params.push_back (C (o + "rcurve", n + "courbe droite", curves()));
            }
            // v0.45.1 : ensemble stéréo (chorus à lignes à retard) après les voix ; 0 = son d'origine
            m.params.push_back (P ("ensemble", "Ensemble", "%", 0, 100, 0));
            m.params.push_back (P ("enswidth", "Largeur de l'ensemble", "%", 0, 100, 70));
            m.sections = {
                { "global", "Global", "global", { "algo", "feedback", "tune", "oscsync", "vmode", "voices", "glide", "level" }, "global" },
                { "lfo", "LFO", "lfo", { "lfowave", "lfospeed", "lfosync", "lfodelay", "lfopmd", "lfoamd", "pms", "lfoksync" }, "mod" },
                { "peg", "Enveloppe de hauteur", "env", { "pr1", "pr2", "pr3", "pr4", "pl1", "pl2", "pl3", "pl4" }, "env" },
                { "ctl", "Molette et pression", "mod", { "wheelpm", "wheelam", "presspm", "pressam", "character" }, "mod" },
                { "ens", "Ensemble", "fx", { "ensemble", "enswidth" }, "fx" },
            };
            for (int k = 0; k < numOps; ++k)
            {
                const auto o = "o" + std::to_string (k + 1) + "_";
                const auto t = "Op\xC3\xA9rateur " + std::to_string (k + 1);
                m.sections.push_back ({ "op" + std::to_string (k + 1), t, "osc", { o + "mode", o + "coarse", o + "fine", o + "detune", o + "level", o + "vel", o + "ams", o + "rscale" }, "osc" });
                m.sections.push_back ({ "op" + std::to_string (k + 1) + "eg", t + " \xC2\xB7 enveloppe", "env",
                                        { o + "r1", o + "r2", o + "r3", o + "r4", o + "l1", o + "l2", o + "l3", o + "l4" }, "env" });
                m.sections.push_back ({ "op" + std::to_string (k + 1) + "ks", t + " \xC2\xB7 \xC3\xA9" "chelle", "osc",
                                        { o + "break", o + "ldepth", o + "rdepth", o + "lcurve", o + "rcurve" }, "osc" });
            }

            // Sons d'usine : faits ici (aucun son d'origine). op (k, …) : réglages de l'opérateur k.
            using V = std::vector<std::pair<std::string, float>>;
            auto op = [] (int k, std::initializer_list<std::pair<const char*, float>> kv)
            {
                V v;
                for (const auto& [id, x] : kv) v.push_back ({ "o" + std::to_string (k) + "_" + id, x });
                return v;
            };
            auto eg = [&op] (int k, float r1, float r2, float r3, float r4, float l1, float l2, float l3, float l4)
            { return op (k, { { "r1", r1 }, { "r2", r2 }, { "r3", r3 }, { "r4", r4 }, { "l1", l1 }, { "l2", l2 }, { "l3", l3 }, { "l4", l4 } }); };
            auto mk = [] (std::initializer_list<V> parts) { V v; for (const auto& p : parts) v.insert (v.end(), p.begin(), p.end()); return v; };
            const std::string N = "Nappes", K = "Claviers", B = "Basses", L = "Leads", BR = "Cuivres", S = "Cordes", PL = "Plucks", FX = "Effets";
            m.presets = {
                { "Init", { { "level", -18.1f } }, "Init" },
                { "Piano \xC3\xA9lectrique 81", mk ({
                    V { { "algo", 5 }, { "feedback", 3 }, { "level", -14.9f } },
                    op (1, { { "level", 99 }, { "vel", 2 } }), eg (1, 95, 30, 22, 62, 99, 82, 0, 0),
                    op (2, { { "level", 70 }, { "vel", 6 }, { "rdepth", 30 } }), eg (2, 95, 48, 35, 75, 99, 62, 0, 0),
                    op (3, { { "level", 92 }, { "detune", 3 }, { "vel", 2 } }), eg (3, 95, 28, 20, 60, 99, 85, 0, 0),
                    op (4, { { "coarse", 14 }, { "level", 54 }, { "vel", 7 } }), eg (4, 99, 72, 50, 80, 99, 0, 0, 0),
                    op (5, { { "level", 78 }, { "detune", -3 } }), eg (5, 95, 30, 22, 62, 99, 80, 0, 0),
                    op (6, { { "level", 58 }, { "vel", 4 } }), eg (6, 95, 45, 35, 70, 99, 55, 0, 0) }), K },
                { "Cloches de verre", mk ({
                    V { { "algo", 5 }, { "level", -15.1f } },
                    op (1, { { "level", 99 } }), eg (1, 99, 26, 18, 30, 99, 45, 0, 0),
                    op (2, { { "coarse", 3 }, { "fine", 50 }, { "level", 74 }, { "vel", 4 } }), eg (2, 99, 30, 22, 35, 99, 35, 0, 0),
                    op (3, { { "level", 84 }, { "detune", 5 } }), eg (3, 99, 24, 16, 28, 99, 50, 0, 0),
                    op (4, { { "coarse", 3 }, { "fine", 50 }, { "detune", 5 }, { "level", 70 } }), eg (4, 99, 32, 24, 35, 99, 30, 0, 0),
                    op (5, { { "coarse", 4 }, { "level", 62 } }), eg (5, 99, 40, 30, 40, 99, 20, 0, 0),
                    op (6, { { "coarse", 7 }, { "fine", 6 }, { "level", 55 } }), eg (6, 99, 45, 30, 40, 99, 10, 0, 0) }), K },
                { "Basse FM pinc\xC3\xA9" "e", mk ({
                    V { { "vmode", 1 }, { "algo", 1 }, { "feedback", 5 }, { "level", -1.2f } },
                    op (1, { { "coarse", 0 }, { "level", 99 } }), eg (1, 99, 40, 30, 70, 99, 80, 0, 0),
                    op (2, { { "coarse", 0 }, { "level", 80 }, { "vel", 4 } }), eg (2, 99, 60, 40, 70, 99, 30, 0, 0),
                    op (3, { { "coarse", 0 }, { "level", 72 } }), eg (3, 99, 45, 30, 70, 99, 70, 0, 0),
                    op (4, { { "level", 62 } }), eg (4, 99, 55, 40, 70, 99, 40, 0, 0),
                    op (5, { { "coarse", 3 }, { "level", 42 } }), eg (5, 99, 75, 50, 70, 99, 0, 0, 0),
                    op (6, { { "coarse", 0 }, { "level", 50 } }), eg (6, 99, 70, 50, 70, 99, 20, 0, 0) }), B },
                { "Basse slap", mk ({
                    V { { "vmode", 1 }, { "algo", 16 }, { "feedback", 6 }, { "level", 1.2f } },
                    op (1, { { "level", 99 }, { "vel", 2 } }), eg (1, 99, 45, 30, 75, 99, 72, 0, 0),
                    op (2, { { "level", 74 }, { "vel", 5 } }), eg (2, 99, 55, 35, 75, 99, 25, 0, 0),
                    op (3, { { "coarse", 3 }, { "level", 66 }, { "vel", 6 } }), eg (3, 99, 78, 50, 75, 99, 0, 0, 0),
                    op (4, { { "coarse", 5 }, { "level", 52 } }), eg (4, 99, 80, 50, 75, 99, 0, 0, 0),
                    op (5, { { "level", 68 } }), eg (5, 99, 50, 40, 75, 99, 40, 0, 0),
                    op (6, { { "level", 52 } }), eg (6, 99, 60, 40, 75, 99, 20, 0, 0) }), B },
                { "Basse sub FM", mk ({
                    V { { "vmode", 1 }, { "algo", 5 }, { "level", -2.9f } },
                    op (1, { { "coarse", 0 }, { "level", 99 } }), eg (1, 99, 35, 30, 70, 99, 90, 0, 0),
                    op (2, { { "coarse", 0 }, { "level", 55 } }), eg (2, 99, 50, 30, 70, 99, 40, 0, 0),
                    op (3, { { "level", 78 } }), eg (3, 99, 60, 40, 70, 99, 30, 0, 0),
                    op (4, { { "level", 66 }, { "vel", 5 } }), eg (4, 99, 80, 50, 70, 99, 0, 0, 0),
                    op (5, { { "level", 0 } }), op (6, { { "level", 0 } }) }), B },
                { "Orgue \xC3\xA0 tirettes", mk ({
                    V { { "algo", 32 }, { "feedback", 4 }, { "level", -20.5f }, { "lfopmd", 6 }, { "lfospeed", 34 } },
                    op (1, { { "coarse", 0 }, { "level", 86 } }), eg (1, 99, 99, 99, 80, 99, 99, 99, 0),
                    op (2, { { "level", 99 } }), eg (2, 99, 99, 99, 80, 99, 99, 99, 0),
                    op (3, { { "coarse", 1 }, { "fine", 50 }, { "level", 80 } }), eg (3, 99, 99, 99, 80, 99, 99, 99, 0),
                    op (4, { { "coarse", 2 }, { "level", 86 } }), eg (4, 99, 99, 99, 80, 99, 99, 99, 0),
                    op (5, { { "coarse", 3 }, { "level", 76 } }), eg (5, 99, 99, 99, 80, 99, 99, 99, 0),
                    op (6, { { "coarse", 4 }, { "level", 70 } }), eg (6, 99, 99, 99, 80, 99, 99, 99, 0) }), K },
                { "Cuivres FM", mk ({
                    V { { "algo", 22 }, { "feedback", 5 }, { "level", -16.0f }, { "pr1", 72 }, { "pl4", 46 }, { "pl1", 50 } },
                    op (1, { { "level", 96 } }), eg (1, 62, 40, 35, 62, 99, 95, 92, 0),
                    op (2, { { "level", 76 }, { "vel", 4 } }), eg (2, 52, 38, 32, 60, 99, 92, 86, 0),
                    op (3, { { "level", 90 }, { "detune", 4 } }), eg (3, 64, 40, 35, 62, 99, 95, 92, 0),
                    op (4, { { "level", 88 }, { "detune", -4 } }), eg (4, 64, 40, 35, 62, 99, 95, 92, 0),
                    op (5, { { "level", 84 }, { "coarse", 2 } }), eg (5, 64, 40, 35, 62, 99, 95, 92, 0),
                    op (6, { { "level", 74 }, { "vel", 5 } }), eg (6, 55, 40, 30, 60, 99, 88, 80, 0) }), BR },
                { "Cordes cristal", mk ({
                    V { { "algo", 5 }, { "level", -15.5f }, { "lfopmd", 8 }, { "lfospeed", 30 }, { "lfodelay", 45 } },
                    op (1, { { "level", 96 }, { "detune", 5 } }), eg (1, 42, 30, 25, 45, 99, 95, 90, 0),
                    op (2, { { "level", 58 } }), eg (2, 38, 30, 25, 45, 99, 90, 85, 0),
                    op (3, { { "level", 94 } }), eg (3, 40, 30, 25, 45, 99, 95, 90, 0),
                    op (4, { { "coarse", 2 }, { "level", 50 } }), eg (4, 38, 30, 25, 45, 99, 90, 85, 0),
                    op (5, { { "level", 92 }, { "detune", -5 } }), eg (5, 42, 30, 25, 45, 99, 95, 90, 0),
                    op (6, { { "level", 55 }, { "detune", -2 } }), eg (6, 38, 30, 25, 45, 99, 90, 85, 0) }), S },
                { "Marimba bois\xC3\xA9", mk ({
                    V { { "algo", 5 }, { "level", -6.8f } },
                    op (1, { { "level", 99 }, { "vel", 2 } }), eg (1, 99, 38, 30, 50, 99, 0, 0, 0),
                    op (2, { { "coarse", 4 }, { "level", 66 }, { "vel", 5 } }), eg (2, 99, 62, 40, 60, 99, 0, 0, 0),
                    op (3, { { "coarse", 4 }, { "level", 62 } }), eg (3, 99, 60, 40, 60, 99, 0, 0, 0),
                    op (4, { { "level", 48 } }), eg (4, 99, 70, 50, 60, 99, 0, 0, 0),
                    op (5, { { "level", 0 } }), op (6, { { "level", 0 } }) }), PL },
                { "Kalimba", mk ({
                    V { { "algo", 5 }, { "level", -8.9f } },
                    op (1, { { "level", 99 } }), eg (1, 99, 42, 30, 50, 99, 0, 0, 0),
                    op (2, { { "coarse", 3 }, { "level", 64 }, { "vel", 4 } }), eg (2, 99, 60, 40, 60, 99, 0, 0, 0),
                    op (3, { { "coarse", 3 }, { "detune", 3 }, { "level", 70 } }), eg (3, 99, 55, 40, 60, 99, 0, 0, 0),
                    op (4, { { "coarse", 7 }, { "level", 50 } }), eg (4, 99, 72, 50, 60, 99, 0, 0, 0),
                    op (5, { { "level", 0 } }), op (6, { { "level", 0 } }) }), PL },
                { "Clavecin m\xC3\xA9tal", mk ({
                    V { { "algo", 3 }, { "feedback", 6 }, { "level", -10.4f } },
                    op (1, { { "level", 99 } }), eg (1, 99, 40, 35, 62, 99, 50, 0, 0),
                    op (2, { { "level", 80 }, { "vel", 3 } }), eg (2, 99, 50, 40, 62, 99, 40, 0, 0),
                    op (3, { { "coarse", 5 }, { "level", 68 } }), eg (3, 99, 60, 40, 62, 99, 20, 0, 0),
                    op (4, { { "coarse", 2 }, { "level", 80 } }), eg (4, 99, 42, 35, 62, 99, 45, 0, 0),
                    op (5, { { "level", 70 } }), eg (5, 99, 50, 40, 62, 99, 35, 0, 0),
                    op (6, { { "coarse", 3 }, { "level", 60 } }), eg (6, 99, 60, 40, 62, 99, 20, 0, 0) }), K },
                { "Lead scie FM", mk ({
                    V { { "algo", 1 }, { "feedback", 7 }, { "level", -6.5f }, { "vmode", 2 }, { "glide", 60 }, { "lfopmd", 0 }, { "wheelpm", 70 } },
                    op (1, { { "level", 82 }, { "detune", 5 } }), eg (1, 90, 40, 40, 70, 99, 95, 92, 0),
                    op (2, { { "level", 74 } }), eg (2, 90, 40, 40, 70, 99, 92, 90, 0),
                    op (3, { { "level", 99 } }), eg (3, 90, 40, 40, 70, 99, 95, 92, 0),
                    op (4, { { "level", 84 } }), eg (4, 90, 40, 40, 70, 99, 92, 90, 0),
                    op (5, { { "level", 70 } }), eg (5, 90, 40, 40, 70, 99, 90, 88, 0),
                    op (6, { { "level", 76 } }), eg (6, 90, 40, 40, 70, 99, 90, 88, 0) }), L },
                { "Nappe num\xC3\xA9rique", mk ({
                    V { { "algo", 5 }, { "level", -16.1f }, { "lfoamd", 20 }, { "lfospeed", 22 }, { "lfowave", 4 } },
                    op (1, { { "level", 95 }, { "ams", 1 } }), eg (1, 35, 25, 20, 38, 99, 95, 92, 0),
                    op (2, { { "coarse", 2 }, { "level", 48 } }), eg (2, 30, 25, 20, 38, 99, 85, 80, 0),
                    op (3, { { "level", 92 }, { "detune", 6 }, { "ams", 1 } }), eg (3, 32, 25, 20, 38, 99, 95, 92, 0),
                    op (4, { { "level", 52 } }), eg (4, 30, 25, 20, 38, 99, 85, 80, 0),
                    op (5, { { "coarse", 2 }, { "level", 80 }, { "detune", -6 } }), eg (5, 30, 25, 20, 38, 99, 90, 88, 0),
                    op (6, { { "coarse", 3 }, { "level", 40 } }), eg (6, 28, 25, 20, 38, 99, 80, 75, 0) }), N },
                { "Gong m\xC3\xA9tallique", mk ({
                    V { { "algo", 1 }, { "feedback", 4 }, { "level", -12.8f } },
                    op (1, { { "level", 99 } }), eg (1, 99, 22, 15, 28, 99, 55, 0, 0),
                    op (2, { { "fine", 41 }, { "level", 84 } }), eg (2, 99, 24, 18, 28, 99, 50, 0, 0),
                    op (3, { { "level", 88 }, { "detune", 2 } }), eg (3, 99, 22, 15, 28, 99, 55, 0, 0),
                    op (4, { { "coarse", 2 }, { "fine", 73 }, { "level", 80 } }), eg (4, 99, 25, 18, 28, 99, 45, 0, 0),
                    op (5, { { "coarse", 3 }, { "fine", 17 }, { "level", 70 } }), eg (5, 99, 28, 20, 28, 99, 40, 0, 0),
                    op (6, { { "coarse", 5 }, { "fine", 60 }, { "level", 64 } }), eg (6, 99, 30, 20, 28, 99, 30, 0, 0) }), FX },
                { "Fl\xC3\xBBte de bois", mk ({
                    V { { "algo", 5 }, { "level", -5.3f }, { "lfopmd", 10 }, { "lfospeed", 33 }, { "lfodelay", 40 } },
                    op (1, { { "level", 99 } }), eg (1, 58, 40, 30, 60, 99, 92, 88, 0),
                    op (2, { { "level", 52 }, { "vel", 3 } }), eg (2, 70, 50, 30, 60, 99, 70, 65, 0),
                    op (3, { { "coarse", 2 }, { "level", 60 } }), eg (3, 58, 40, 30, 60, 99, 85, 80, 0),
                    op (4, { { "level", 40 } }), eg (4, 70, 50, 30, 60, 99, 60, 55, 0),
                    op (5, { { "level", 0 } }), op (6, { { "level", 0 } }) }), L },
                // v0.45.1 : basses et nappes plus pleines, ensemble stéréo
                { "Sub FM", mk ({
                    V { { "vmode", 1 }, { "algo", 5 }, { "feedback", 3 }, { "level", -2.8f } },
                    op (1, { { "coarse", 0 }, { "level", 99 } }), eg (1, 99, 50, 40, 70, 99, 94, 90, 0),
                    op (2, { { "coarse", 0 }, { "level", 62 }, { "vel", 4 } }), eg (2, 99, 55, 35, 70, 99, 70, 52, 0),
                    op (3, { { "coarse", 1 }, { "level", 84 } }), eg (3, 99, 50, 40, 70, 99, 90, 82, 0),
                    op (4, { { "coarse", 1 }, { "level", 58 } }), eg (4, 99, 62, 40, 70, 99, 55, 30, 0),
                    op (5, { { "coarse", 0 }, { "level", 76 }, { "detune", 2 } }), eg (5, 99, 50, 40, 70, 99, 92, 88, 0),
                    op (6, { { "coarse", 2 }, { "level", 45 } }), eg (6, 99, 75, 50, 70, 99, 0, 0, 0) }), B },
                { "Basse ronde 80", mk ({
                    V { { "vmode", 1 }, { "algo", 1 }, { "feedback", 4 }, { "level", -3.9f } },
                    op (1, { { "coarse", 0 }, { "level", 99 } }), eg (1, 99, 45, 35, 72, 99, 90, 86, 0),
                    op (2, { { "coarse", 0 }, { "level", 72 }, { "vel", 5 } }), eg (2, 99, 50, 30, 72, 99, 65, 45, 0),
                    op (3, { { "coarse", 0 }, { "level", 92 }, { "detune", 3 } }), eg (3, 99, 45, 35, 72, 99, 88, 84, 0),
                    op (4, { { "coarse", 1 }, { "level", 66 } }), eg (4, 99, 55, 35, 72, 99, 60, 40, 0),
                    op (5, { { "coarse", 1 }, { "level", 50 } }), eg (5, 99, 70, 40, 72, 99, 30, 0, 0),
                    op (6, { { "coarse", 3 }, { "level", 40 } }), eg (6, 99, 80, 50, 72, 99, 0, 0, 0) }), B },
                { "Basse acide FM", mk ({
                    V { { "vmode", 2 }, { "glide", 45 }, { "algo", 1 }, { "feedback", 7 }, { "level", -2.5f } },
                    op (1, { { "coarse", 0 }, { "level", 99 } }), eg (1, 99, 40, 35, 75, 99, 92, 90, 0),
                    op (2, { { "coarse", 0 }, { "level", 84 }, { "vel", 6 } }), eg (2, 99, 45, 30, 75, 99, 55, 35, 0),
                    op (3, { { "coarse", 0 }, { "level", 70 } }), eg (3, 99, 40, 35, 75, 99, 90, 85, 0),
                    op (4, { { "coarse", 1 }, { "level", 70 } }), eg (4, 99, 50, 30, 75, 99, 50, 30, 0),
                    op (5, { { "level", 0 } }), op (6, { { "level", 0 } }) }), B },
                { "Basse qui gronde", mk ({
                    V { { "vmode", 1 }, { "algo", 5 }, { "feedback", 5 }, { "lfowave", 4 }, { "lfospeed", 62 }, { "lfoamd", 70 }, { "level", -4.5f } },
                    op (1, { { "coarse", 0 }, { "level", 99 } }), eg (1, 99, 50, 40, 72, 99, 95, 92, 0),
                    op (2, { { "coarse", 0 }, { "level", 80 }, { "ams", 3 } }), eg (2, 99, 50, 40, 72, 99, 85, 80, 0),
                    op (3, { { "coarse", 1 }, { "level", 70 } }), eg (3, 99, 50, 40, 72, 99, 90, 86, 0),
                    op (4, { { "coarse", 1 }, { "level", 72 }, { "ams", 2 } }), eg (4, 99, 50, 40, 72, 99, 80, 75, 0),
                    op (5, { { "coarse", 0 }, { "level", 80 } }), eg (5, 99, 50, 40, 72, 99, 95, 92, 0),
                    op (6, { { "coarse", 0 }, { "level", 60 } }), eg (6, 99, 50, 40, 72, 99, 70, 60, 0) }), B },
                { "Nappe chaude", mk ({
                    V { { "algo", 5 }, { "feedback", 2 }, { "ensemble", 60 }, { "lfowave", 4 }, { "lfospeed", 30 }, { "lfopmd", 6 }, { "pms", 2 }, { "level", -15.8f } },
                    op (1, { { "level", 96 }, { "detune", -4 } }), eg (1, 28, 30, 25, 30, 99, 95, 92, 0),
                    op (2, { { "level", 52 } }), eg (2, 25, 25, 20, 30, 99, 80, 75, 0),
                    op (3, { { "level", 94 } }), eg (3, 28, 30, 25, 30, 99, 95, 92, 0),
                    op (4, { { "coarse", 2 }, { "level", 40 } }), eg (4, 24, 25, 20, 30, 99, 75, 70, 0),
                    op (5, { { "level", 94 }, { "detune", 4 } }), eg (5, 28, 30, 25, 30, 99, 95, 92, 0),
                    op (6, { { "level", 48 } }), eg (6, 25, 25, 20, 30, 99, 80, 75, 0) }), N },
                { "Nappe de verre", mk ({
                    V { { "algo", 5 }, { "ensemble", 50 }, { "lfowave", 4 }, { "lfospeed", 25 }, { "lfoamd", 18 }, { "level", -11.2f } },
                    op (1, { { "level", 95 }, { "ams", 1 } }), eg (1, 35, 30, 25, 32, 99, 92, 90, 0),
                    op (2, { { "coarse", 3 }, { "level", 46 } }), eg (2, 30, 22, 20, 32, 99, 70, 62, 0),
                    op (3, { { "coarse", 2 }, { "level", 82 }, { "detune", 5 } }), eg (3, 35, 30, 25, 32, 99, 90, 86, 0),
                    op (4, { { "coarse", 5 }, { "level", 38 } }), eg (4, 30, 22, 20, 32, 99, 60, 50, 0),
                    op (5, { { "level", 88 }, { "detune", -5 } }), eg (5, 35, 30, 25, 32, 99, 92, 90, 0),
                    op (6, { { "coarse", 7 }, { "level", 30 } }), eg (6, 30, 22, 20, 32, 99, 55, 45, 0) }), N },
                { "Nappe sombre", mk ({
                    V { { "algo", 32 }, { "feedback", 4 }, { "ensemble", 70 }, { "level", -12.9f } },
                    op (1, { { "coarse", 0 }, { "level", 92 } }), eg (1, 26, 30, 25, 28, 99, 95, 92, 0),
                    op (2, { { "level", 88 }, { "detune", -5 } }), eg (2, 26, 30, 25, 28, 99, 95, 92, 0),
                    op (3, { { "level", 88 }, { "detune", 5 } }), eg (3, 26, 30, 25, 28, 99, 95, 92, 0),
                    op (4, { { "coarse", 2 }, { "level", 66 } }), eg (4, 24, 30, 25, 28, 99, 90, 86, 0),
                    op (5, { { "coarse", 3 }, { "level", 50 } }), eg (5, 22, 30, 25, 28, 99, 85, 80, 0),
                    op (6, { { "level", 62 } }), eg (6, 24, 30, 25, 28, 99, 90, 86, 0) }), N },
                { "Nappe qui respire", mk ({
                    V { { "algo", 5 }, { "feedback", 3 }, { "ensemble", 55 }, { "lfowave", 0 }, { "lfospeed", 18 }, { "lfoamd", 40 }, { "level", -16.1f } },
                    op (1, { { "level", 95 } }), eg (1, 30, 30, 25, 30, 99, 95, 92, 0),
                    op (2, { { "level", 62 }, { "ams", 3 } }), eg (2, 28, 25, 20, 30, 99, 85, 80, 0),
                    op (3, { { "level", 93 }, { "detune", 3 } }), eg (3, 30, 30, 25, 30, 99, 95, 92, 0),
                    op (4, { { "coarse", 2 }, { "level", 50 }, { "ams", 2 } }), eg (4, 28, 25, 20, 30, 99, 80, 75, 0),
                    op (5, { { "level", 93 }, { "detune", -3 } }), eg (5, 30, 30, 25, 30, 99, 95, 92, 0),
                    op (6, { { "coarse", 3 }, { "level", 40 }, { "ams", 3 } }), eg (6, 28, 25, 20, 30, 99, 75, 70, 0) }), N },
                { "Ch\xC5\x93ur num\xC3\xA9rique", mk ({
                    V { { "algo", 5 }, { "ensemble", 65 }, { "lfowave", 4 }, { "lfospeed", 34 }, { "lfopmd", 8 }, { "pms", 3 }, { "level", -13.7f } },
                    op (1, { { "level", 96 } }), eg (1, 40, 30, 25, 35, 99, 95, 92, 0),
                    op (2, { { "level", 60 }, { "vel", 3 } }), eg (2, 32, 30, 25, 35, 99, 88, 84, 0),
                    op (3, { { "coarse", 2 }, { "level", 80 }, { "detune", 4 } }), eg (3, 40, 30, 25, 35, 99, 92, 88, 0),
                    op (4, { { "coarse", 1 }, { "level", 56 } }), eg (4, 32, 30, 25, 35, 99, 85, 80, 0),
                    op (5, { { "level", 90 }, { "detune", -4 } }), eg (5, 40, 30, 25, 35, 99, 95, 92, 0),
                    op (6, { { "coarse", 4 }, { "level", 38 } }), eg (6, 32, 30, 25, 35, 99, 80, 70, 0) }), N },
                { "Piano num\xC3\xA9rique", mk ({
                    V { { "algo", 5 }, { "feedback", 2 }, { "ensemble", 25 }, { "level", -12.0f } },
                    op (1, { { "level", 99 }, { "vel", 3 } }), eg (1, 99, 34, 24, 60, 99, 80, 0, 0),
                    op (2, { { "level", 74 }, { "vel", 6 } }), eg (2, 99, 50, 35, 70, 99, 55, 0, 0),
                    op (3, { { "level", 94 }, { "detune", 2 }, { "vel", 3 } }), eg (3, 99, 30, 22, 58, 99, 82, 0, 0),
                    op (4, { { "coarse", 1 }, { "level", 66 }, { "vel", 6 } }), eg (4, 99, 46, 30, 66, 99, 60, 0, 0),
                    op (5, { { "coarse", 2 }, { "level", 70 }, { "detune", -2 } }), eg (5, 99, 38, 26, 60, 99, 75, 0, 0),
                    op (6, { { "coarse", 5 }, { "level", 48 }, { "vel", 7 } }), eg (6, 99, 70, 45, 70, 99, 0, 0, 0) }), K },
            };
            HubSpec hub;
            hub.envs = 0; hub.lfos = 1; hub.firstLfo = 2; hub.slots = 8; hub.arp = true;
            addModHub (m, hub);
            addFactoryBank (m); // v0.45.2 : banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Valeurs communes aux voix pour un bloc (LFO, molette, pression), écrites par l'instrument. */
struct Fm6Shared { double lfo = 0.0, lfoUni = 0.5, delayGain = 1.0, wheel = 0.0, pressure = 0.0; };

/** Enveloppe à 4 vitesses et 4 niveaux, en unités logarithmiques (0-127). */
class Fm6Eg
{
public:
    void reset() noexcept { cur = 0.0; stage = 4; down = true; }
    void gate (bool on) noexcept
    {
        if (on) { stage = 0; down = false; }
        else { if (stage < 4) stage = 3; down = true; } // relâchement : vers le niveau 4
    }
    /** Avance de dt secondes ; rates : pentes (unités / s) des 4 étapes ; levels : cibles (unités). */
    double advance (double dt, const double* rates, const double* levels) noexcept
    {
        int st = stage;
        if (st >= 4) return cur;               // fini (ou jamais lancé) : reste au niveau 4
        if (st == 2 && ! down) { step (levels[2], rates[2], dt); return cur; } // maintien au niveau 3
        if (step (levels[st], rates[st], dt))
        {
            if (st == 3) stage = 4;
            else if (st < 2) stage = st + 1;
        }
        return cur;
    }
    double level() const noexcept { return cur; }
    void setLevel (double v) noexcept { cur = v; }
    bool finished() const noexcept { return stage >= 4; }
    bool released() const noexcept { return down; }

private:
    // vrai quand la cible est atteinte
    bool step (double target, double rate, double dt) noexcept
    {
        if (cur < target)
        {
            if (cur < 39.0) cur = 39.0; // les montées partent de −66 dB
            const auto acc = 1.0 + (127.0 - cur) / 24.0;
            cur = std::min (target, cur + rate * acc * dt);
        }
        else cur = std::max (target, cur - rate * dt);
        return std::abs (cur - target) < 1.0e-9;
    }
    double cur = 0.0;
    int stage = 4;
    bool down = true;
};

class Fm6Voice
{
public:
    void prepare (double s) { sr = s; reset(); }
    void reset() noexcept
    {
        for (auto& e : eg) e.reset();
        peg.reset(); peg.setLevel (0.0);
        ph.fill (0.0); fb.fill (0.0); last.fill (0.0); gainNow.fill (0.0);
        pegStage = -1; pegValue = 0.0;
        active = false; gate = false; pending = false;
    }
    void setShared (const Fm6Shared* s) noexcept { sh = s; }
    void setSpread (int, int) noexcept {}
    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; releasePending = false; active = true; gate = true;
    }
    // v0.45.1 : une voix muette ne garde pas de relâchement en attente (changement de mode, arrêt)
    void release() noexcept { if (active) { gate = false; releasePending = true; } }
    bool isActive() const noexcept { return active; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace fm6;
        const auto& alg = algorithms()[(size_t) std::clamp ((int) std::lround (p[algo]) - 1, 0, 31)];
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0 && legatoPending) pitch.to ((double) target);
            else if (p[glide] > 0.5f && from >= 0) { pitch.jump ((double) from); pitch.to ((double) target); }
            else { pitch.jump ((double) target); }
            if (! legatoPending)
            {
                for (int k = 0; k < numOps; ++k)
                {
                    eg[(size_t) k].gate (true);
                    if (p[oscSync] > 0.5f) { ph[(size_t) k] = 0.0; fb[(size_t) k] = 0.0; last[(size_t) k] = 0.0; }
                }
                pegOn (true, p);
            }
        }
        if (releasePending)
        {
            releasePending = false;
            for (auto& e : eg) e.gate (false);
            pegOn (false, p);
        }

        const auto dt = (double) n / sr;
        const auto note = (double) target;
        // hauteur : portamento, transposition (+ pitch bend), enveloppe de hauteur, LFO
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-(double) n / (p[glide] * 0.001 * sr * 0.35));
        const auto basePitch = pitch.next (glideCoef) + p[tune];
        const auto pegSemis = pegAdvance (dt, p);
        static constexpr double pmsTable[8] { 0.0, 0.1, 0.2, 0.35, 0.6, 1.0, 2.0, 6.0 };
        const double lfo = sh != nullptr ? sh->lfo : 0.0, lfoUni = sh != nullptr ? sh->lfoUni : 0.5, dg = sh != nullptr ? sh->delayGain : 1.0;
        const double wheel = sh != nullptr ? sh->wheel : 0.0, press = sh != nullptr ? sh->pressure : 0.0;
        const auto pmDepth = std::clamp (p[lfoPmd] / 99.0 * dg + wheel * p[wheelPm] / 100.0 + press * p[pressPm] / 100.0, 0.0, 1.5);
        const auto amDepth = std::clamp (p[lfoAmd] / 99.0 * dg + wheel * p[wheelAm] / 100.0 + press * p[pressAm] / 100.0, 0.0, 1.5);
        const auto vib = pmsTable[std::clamp ((int) p[pms], 0, 7)] * pmDepth * lfo;
        const auto keyHz = noteHz (basePitch + pegSemis + vib);

        // opérateurs : fréquences, gains (enveloppe + niveau + clavier + vélocité + amplitude)
        std::array<double, numOps> inc {}, g0 {}, g1 {};
        static constexpr double amsTable[4] { 0.0, 10.0, 24.0, 64.0 };
        bool anyCarrier = false;
        for (int k = 0; k < numOps; ++k)
        {
            const auto b = opBase + k * opStride;
            const auto coarse = std::clamp ((int) std::lround (p[b + oCoarse]), 0, 31);
            const auto fine = std::clamp ((double) p[b + oFine], 0.0, 99.0);
            double hz;
            if (p[b + oMode] > 0.5f) hz = std::pow (10.0, (double) (coarse & 3)) * std::pow (10.0, fine / 100.0);
            else hz = keyHz * (coarse == 0 ? 0.5 : (double) coarse) * (1.0 + fine / 100.0);
            hz *= fastExp2 (p[b + oDetune] * 1.5 / 1200.0);
            inc[(size_t) k] = std::clamp (hz / sr, 0.0, 0.49);

            // enveloppe : vitesses (avec vitesses au clavier) et niveaux
            const auto rs = std::clamp ((int) p[b + oRateScale], 0, 7);
            const auto qDelta = rs * std::clamp (note / 3.0 - 7.0, 0.0, 31.0) / 8.0;
            double rates[4], levels[4];
            for (int s = 0; s < 4; ++s)
            {
                const auto q = std::min (63.0, p[b + oR1 + s] * 41.0 / 64.0 + qDelta);
                rates[s] = 2.13 * fastExp2 (q / 4.0);
                levels[s] = levelUnits (p[b + oL1 + s]);
            }
            auto& e = eg[(size_t) k];
            const auto after = e.advance (dt, rates, levels);

            // échelle de niveau au clavier (en unités), vélocité
            const auto off = note - p[b + oBreak];
            const bool left = off < 0.0;
            const auto depth = (left ? p[b + oLDepth] : p[b + oRDepth]) / 99.0;
            const auto curve = std::clamp ((int) (left ? p[b + oLCurve] : p[b + oRCurve]), 0, 3);
            const auto oct = std::abs (off) / 12.0;
            const auto amount = depth * (curve == 0 || curve == 3 ? oct * 24.0 : std::min (127.0, 72.0 * (fastExp2 (oct) - 1.0) / 15.0));
            const auto ks = curve >= 2 ? amount : -amount;
            const auto velU = -p[b + oVel] * (1.0 - vel) * 5.5;
            const auto amU = -amsTable[std::clamp ((int) p[b + oAms], 0, 3)] * amDepth * lfoUni;
            const auto outU = levelUnits (p[b + oLevel]);
            auto toGain = [&] (double egU)
            {
                if (egU <= 0.5 || p[b + oLevel] <= 0.0f) return 0.0;
                const auto u = std::min (8.0, egU + outU - 254.0 + ks + velU + amU);
                return u < -127.0 ? 0.0 : std::pow (10.0, u * 0.75 / 20.0);
            };
            g0[(size_t) k] = gainNow[(size_t) k];
            g1[(size_t) k] = toGain (after);
            if ((alg.carriers >> k) & 1) anyCarrier = anyCarrier || ! e.finished() || g1[(size_t) k] > 1.0e-5;
        }

        // calcul échantillon par échantillon : 6 → 1 (un modulateur a toujours un numéro plus grand)
        const auto fbAmt = p[feedback] < 0.5f ? 0.0 : std::ldexp (1.0, (int) std::lround (p[feedback]) - 8);
        const int fbOp = alg.fbOp, fbSrc = alg.fbSrc;
        const bool retro = p[character] > 0.5f;
        const auto out = dbToGain (p[level]);
        const auto inv = 1.0 / (double) n;
        for (int i = 0; i < n; ++i)
        {
            const auto t = (double) (i + 1) * inv;
            std::array<double, numOps> y {};
            double sum = 0.0;
            for (int k = numOps - 1; k >= 0; --k)
            {
                double mod = 0.0;
                const auto m = alg.mods[k];
                for (int j = k + 1; j < numOps; ++j) if ((m >> j) & 1) mod += y[(size_t) j];
                if (k == fbOp && fbAmt > 0.0) mod += fbAmt * (fb[(size_t) fbSrc] + last[(size_t) fbSrc]) * 0.5;
                const auto g = g0[(size_t) k] + (g1[(size_t) k] - g0[(size_t) k]) * t;
                auto phase = ph[(size_t) k] + mod;
                if (retro) phase = std::floor (phase * 4096.0) / 4096.0; // sinus à 4096 points
                const auto v = fastSin2Pi (phase) * g;
                y[(size_t) k] = v;
                ph[(size_t) k] += inc[(size_t) k];
                if (ph[(size_t) k] >= 1.0) ph[(size_t) k] -= 1.0;
                if ((alg.carriers >> k) & 1) sum += v;
            }
            for (int k = 0; k < numOps; ++k) { fb[(size_t) k] = last[(size_t) k]; last[(size_t) k] = y[(size_t) k]; }
            auto s = sum * out;
            if (retro && s != 0.0)
            {
                // convertisseur à virgule flottante : 10 bits de mantisse
                int e2 = 0;
                const auto mant = std::frexp (s, &e2);
                s = std::ldexp (std::round (mant * 1024.0) / 1024.0, e2);
            }
            L[i] += (float) s;
            R[i] += (float) s;
        }
        for (int k = 0; k < numOps; ++k) gainNow[(size_t) k] = g1[(size_t) k];
        if (! gate && ! anyCarrier) active = false;
    }

private:
    void pegOn (bool on, const float* p) noexcept
    {
        // la hauteur part du niveau 4 (comme les enveloppes d'amplitude), puis 1 → 2 → 3 (maintien) ; 4 au relâchement
        if (on) { pegValue = pegSemis (p[fm6::pl4]); pegStage = 0; }
        else pegStage = 3;
    }
    static double pegSemis (double l) noexcept { const auto v = (l - 50.0) / 50.0; return 48.0 * v * std::pow (std::abs (v), 0.8); }
    /** Enveloppe de hauteur : niveaux 0-99 (50 = centre) → ±48 demi-tons ; part du niveau 4. */
    double pegAdvance (double dt, const float* p) noexcept
    {
        using namespace fm6;
        auto semisOf = [] (double l) { return pegSemis (l); };
        if (pegStage < 0) pegValue = semisOf (p[pl4]);
        else if (pegStage <= 3)
        {
            const int st = pegStage;
            const auto tgt = semisOf (p[pl1 + st]);
            const auto speed = 0.3 * fastExp2 (p[pr1 + st] * 0.12);
            if (pegValue < tgt) pegValue = std::min (tgt, pegValue + speed * dt); else pegValue = std::max (tgt, pegValue - speed * dt);
            if (std::abs (pegValue - tgt) < 1.0e-9)
            {
                if (st < 2) pegStage = st + 1;
                else if (st == 3) pegStage = 4;
            }
        }
        return pegValue;
    }

    double sr = 48000.0;
    const Fm6Shared* sh = nullptr;
    std::array<Fm6Eg, fm6::numOps> eg;
    Fm6Eg peg;
    std::array<double, fm6::numOps> ph {}, fb {}, last {}, gainNow {};
    Glide pitch;
    float vel = 1.0f;
    int target = 60, from = -1, pegStage = -1;
    double pegValue = 0.0;
    bool active = false, gate = false, pending = false, releasePending = false, legatoPending = false;
};

class Fm6Synth final : public SynthBase<Fm6Voice, 16>
{
public:
    Fm6Synth() { for (auto& v : voices) v.setShared (&shared); }
    const ModuleDesc& desc() const noexcept override { return fm6::desc(); }
    void prepare (double s, int maxBlock) override { SynthBase::prepare (s, maxBlock); chorus.prepare (s); ensIndex = fm6::desc().indexOf ("ensemble"); }
    void reset() noexcept override { SynthBase::reset(); chorus.reset(); }

    void noteOn (int note, float velocity) noexcept override
    {
        // LFO recalé à la note (ou au premier son après un silence) ; retard relancé
        if (lastP != nullptr && (lastP[fm6::lfoKeySync] > 0.5f || ! anySounding())) { lfoPhase = 0.0; lfoAge = 0.0; }
        else if (! anySounding()) lfoAge = 0.0;
        SynthBase::noteOn (note, velocity);
    }
    void setExpression (float bend, float wheelV, float pressure) noexcept override
    {
        wheelT = wheelV; pressT = pressure;
        SynthBase::setExpression (bend, wheelV, pressure);
    }

    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        blockN = std::min (n, voiceBlock); // le rendu des voix se fait par blocs de 64 au plus
        SynthBase::render (L, R, n, p);
    }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[fm6::voices], 1, 16);
        switch ((int) p[fm6::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (false, true, v); break;
        }
    }
    const float* adjust (const float* p) noexcept override
    {
        using namespace fm6;
        lastP = p;
        // LFO commun, calculé une fois par bloc
        const auto n = (double) blockN;
        const auto speed = std::clamp ((double) p[lfoSpeed], 0.0, 99.0);
        const auto freeHz = 0.062 * fastExp2 (speed * 0.0973);
        const auto hz = lfoSyncHz ((int) p[lfoSync], freeHz, bpm);
        const auto prev = lfoPhase;
        lfoPhase += hz * n / sr;
        if (lfoPhase >= 1.0) lfoPhase -= std::floor (lfoPhase);
        if (lfoPhase < prev) { rng = rng * 1664525u + 1013904223u; held = (double) (rng >> 8) / 8388608.0 - 1.0; }
        double l;
        switch ((int) p[lfoWave])
        {
            case 1: l = 1.0 - 2.0 * lfoPhase; break;
            case 2: l = 2.0 * lfoPhase - 1.0; break;
            case 3: l = lfoPhase < 0.5 ? 1.0 : -1.0; break;
            case 4: l = fastSin2Pi (lfoPhase); break;
            case 5: l = held; break;
            default: l = 1.0 - 4.0 * std::abs (lfoPhase - 0.5); break;
        }
        lfoAge += n / sr;
        const auto delay = std::pow (p[lfoDelay] / 99.0, 2.0) * 6.0;
        shared.lfo = l;
        shared.lfoUni = 0.5 + 0.5 * l;
        shared.delayGain = delay <= 0.0 ? 1.0 : std::clamp ((lfoAge - delay) / (0.5 * delay + 0.05), 0.0, 1.0);
        const auto k = std::min (1.0, n / (0.004 * sr));
        wheel += (wheelT - wheel) * k; press += (pressT - press) * k;
        shared.wheel = wheel; shared.pressure = press;
        return p;
    }

    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        if (ensIndex < 0) return;
        const auto mix = p[ensIndex] / 100.0, width = p[ensIndex + 1] / 100.0;
        if (mix <= 0.001) return;
        // deux chorus décalés (lent et rapide), largeur = écart des retards gauche / droite
        chorus.process (L, R, n, 0.45, 1.2 + 2.2 * width, 7.0, mix * 0.85, 11000.0);
    }

private:
    BbdChorus chorus;
    int ensIndex = -1;
    bool anySounding() const noexcept { for (const auto& v : voices) if (v.isActive()) return true; return false; }
    int blockN = 32;
    Fm6Shared shared;
    const float* lastP = nullptr;
    double lfoPhase = 0.0, lfoAge = 0.0, held = 0.0, wheel = 0.0, press = 0.0, wheelT = 0.0, pressT = 0.0;
    std::uint32_t rng = 0xF6F6u;
};

} // namespace grv::instruments
