/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Good Doctor 2 » — principe des grands synthés hybrides de la génération 2025 (architecture
    reprise : ni nom, ni panneau, ni table, ni son d'origine ; tout est calculé par le programme).
    Fiche d'architecture : docs/SPEC-GOOD-DOCTOR-2.md. Good Doctor (le premier) reste tel quel.

      - trois oscillateurs, chacun en mode :
          · tables d'ondes : 16 tables calculées + 3 tables perso (emplacements d'échantillon 2 à 4,
            dessinées dans l'éditeur ou importées au format courant de 2048 points par onde),
            deux déformations à la suite (25 types), unisson jusqu'à 7 voix (désaccord, mélange,
            largeur) ;
          · granulaire : grains tirés de l'échantillon de l'emplacement 1 ou d'une des 6 sources
            calculées (taille, densité, éparpillement, balayage, hasard de hauteur, fenêtre, envers) ;
          · spectral : resynthèse par 64 partiels au plus (étirement, décalage, pente, pair/impair,
            flou, coupure, balayage) d'une table, de l'échantillon ou d'une source calculée ;
        avec octave, demi-tons, fin, niveau, panoramique, phase et hasard, INVERSION de polarité et
        RETARD de départ (basses dures : décaler une couche pour l'effet d'aspiration) ;
      - sous-oscillateur et bruit (5 couleurs), chacun avec sa sortie ;
      - deux filtres stéréo (18 types) en série ou en parallèle ;
      - quatre enveloppes à tenue et à courbes (attaque, déclin, relâchement) : 1 ampli, 2 filtres,
        3 hauteur (la frappe des kicks), 4 libre ; trois LFO (9 formes dont « pompe » et
        « aspiration », libres, redéclenchés ou en un coup, synchronisés au tempo) ; deux LFO à pas,
        une enveloppe, la matrice de 32 routages, 8 macros et l'arpège de la modulation commune ;
      - un rack de 10 effets dans l'ordre choisi (gd2/Gd2Fx.h) ; l'« élargisseur » reprend le principe des
        modules d'unisson et d'espace stéréo de ces synthés.

    Règles temps réel : tables, sources et analyses calculées hors du fil audio ; rien d'alloué dans
    le rendu ; hauteur et réglages lus par bloc de 64 échantillons au plus.
*/

#pragma once

#include "gd2/Gd2Sources.h"
#include "gd2/Gd2Fx.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace gd2
{
    constexpr int numOsc = 3, maxUni = 7, maxGrains = 24;

    enum OscField
    {
        ofOn = 0, ofMode, ofTab, ofPos, ofW1, ofW1Amt, ofW2, ofW2Amt, ofOct, ofSemi, ofFine, ofLvl, ofPan, ofPhase, ofRand,
        ofUni, ofUDet, ofUBlend, ofUWidth, ofInv, ofDelay, ofDest,
        ofGSrc, ofGSize, ofGDens, ofGSpray, ofGScan, ofGPitch, ofGWin, ofGRev,
        ofSSrc, ofSParts, ofSStretch, ofSShift, ofSTilt, ofSOdd, ofSBlur, ofSCut, ofSScan,
        oscStride
    };
    enum FiltField { ffOn = 0, ffType, ffCut, ffRes, ffDrive, ffShape, ffMix, ffKey, ffEnv, ffVel, fStride };
    enum EnvField { eA = 0, eH, eD, eS, eR, eCA, eCD, eCR, envStride };
    enum LfoField { lRate = 0, lShape, lSync, lMode, lDelay, lSmooth, lPhase, lfoStride };
    constexpr int numEnvs = 4, numLfos = 3;

    enum Slot
    {
        voices = 0, glide, level, velAmp, tune, spreadAmt, vMode,
        oscBase,
        subOn = oscBase + numOsc * oscStride, subLvl, subShape, subOct, subPan, subDest, subInv,
        noiseOn, noiseLvl, noiseColor, noiseType, noisePan, noiseDest,
        route,
        fBase,
        envBase = fBase + 2 * fStride,
        pAmt = envBase + numEnvs * envStride,
        lfoBase,
        // rack d'effets
        hyOn = lfoBase + numLfos * lfoStride, hyOrd, hyRate, hyDepth, hyDim, hyMix,
        dsOn, dsOrd, dsType, dsDrive, dsPre, dsFreq, dsMix,
        flOn, flOrd, flRate, flDepth, flFeed, flMix,
        phOn, phOrd, phRate, phDepth, phFreq, phFeed, phMix,
        chOn, chOrd, chRate, chDepth, chDelay, chFeed, chMix,
        dlOn, dlOrd, dlMode, dlSync, dlTime, dlDiv, dlFeed, dlLow, dlHigh, dlMix,
        cpOn, cpOrd, cpMode, cpDepth, cpTime, cpUp, cpDown, cpIn, cpOut, cpLow, cpMid, cpHigh, cpThr, cpRatio,
        fiOn, fiOrd, fiType, fiCut, fiRes, fiDrive, fiMix,
        eqOn, eqOrd, eqLF, eqLG, eqMF, eqMG, eqMQ, eqHF, eqHG,
        rvOn, rvOrd, rvSize, rvPre, rvBright, rvLow, rvWidth, rvMix,
        numSlots
    };
    constexpr int numFx = 10;
    constexpr int fxOn[numFx] { hyOn, dsOn, flOn, phOn, chOn, dlOn, cpOn, fiOn, eqOn, rvOn };

    enum Mode { mTable = 0, mGranular, mSpectral };
    enum Warp
    {
        wNone = 0, wSync, wBendUp, wBendDown, wPulse, wMirror, wAsym, wQuant, wFm, wFmSub,
        wAm, wRing, wFold, wSat, wRect, wBits, wLow, wHigh,
        wWinSync, wPdSoft, wFlip, wSquash, wHalf, wFmNoise, wRingSub, numWarps
    };
    inline bool phaseWarp (int w) noexcept { return (w >= wSync && w <= wFmSub) || w == wWinSync || w == wPdSoft || w == wFlip || w == wFmNoise; }

    inline const std::vector<std::string>& warpNames()
    {
        static const std::vector<std::string> n = []
        {
            auto v = gdoc::warpNames();
            for (const char* s : { "Synchro fen\xC3\xAAtr\xC3\xA9" "e", "Phase douce", "Retourner (moiti\xC3\xA9)", "\xC3\x89" "craser",
                                   "Demi-onde", "FM (bruit)", "Anneau (sous-osc.)" })
                v.push_back (s);
            return v;
        }();
        return n;
    }
    inline const std::vector<std::string>& filterNames()
    {
        static const std::vector<std::string> n = []
        {
            auto v = gdoc::filterNames();
            for (const char* s : { "Passe-bas 18 dB", "Passe-bas 6 dB", "Disperseur", "Phaseur" }) v.push_back (s);
            return v;
        }();
        return n;
    }
    inline const std::vector<std::string>& lfoShapeNames()
    {
        static const std::vector<std::string> n { "Sinus", "Triangle", "Scie \xE2\x86\x91", "Scie \xE2\x86\x93", "Carr\xC3\xA9",
                                                  "Pompe", "Aspiration", "Hasard tenu", "Hasard doux" };
        return n;
    }
    inline const std::vector<std::string>& granSourceNames()
    {
        static const std::vector<std::string> n = []
        {
            std::vector<std::string> v { "\xC3\x89" "chantillon (empl. 1)" };
            for (const auto& s : calcSourceNames()) v.push_back (s);
            return v;
        }();
        return n;
    }
    inline const std::vector<std::string>& specSourceNames()
    {
        static const std::vector<std::string> n = []
        {
            std::vector<std::string> v { "Table de l'oscillateur", "\xC3\x89" "chantillon (empl. 1)" };
            for (const auto& s : calcSourceNames()) v.push_back (s);
            return v;
        }();
        return n;
    }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Arr\xC3\xAAt", "Marche" };
            ModuleDesc m;
            m.type = "gooddoc2";
            m.name = "Good Doctor 2";
            m.family = "Polys";
            m.principle = "Trois oscillateurs hybrides (tables d'ondes, granulaire, spectral) \xC3\xA0 double d\xC3\xA9" "formation et unisson, deux filtres, "
                          "4 enveloppes \xC3\xA0 courbes, LFO, matrice, rack de 10 effets dont compresseur trois bandes";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Glisse", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Volume", "dB", -30, 6, -6),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("spread", "Largeur", "%", 0, 100, 50),
                C ("vmode", "Jeu", { "Poly", "Mono", "Legato" }),
            };
            const char* ok[numOsc] { "a", "b", "c" };
            for (int o = 0; o < numOsc; ++o)
            {
                const std::string k = ok[o];
                const std::string n = std::string (" ") + (char) ('A' + o);
                m.params.push_back (C (k + "_on", "Oscillateur" + n, onOff, o == 0 ? 1 : 0));
                m.params.push_back (C (k + "_mode", "Mode" + n, { "Tables d'ondes", "Granulaire", "Spectral" }));
                m.params.push_back (C (k + "_tab", "Table" + n, tableNames(), o == 0 ? 0 : o == 1 ? 8 : 3));
                m.params.push_back (P (k + "_pos", "Position" + n, "%", 0, 100, o == 0 ? 50 : 0));
                m.params.push_back (C (k + "_w1", "D\xC3\xA9" "formation 1" + n, warpNames(), 0));
                m.params.push_back (P (k + "_w1amt", "Intensit\xC3\xA9 1" + n, "%", 0, 100, 0));
                m.params.push_back (C (k + "_w2", "D\xC3\xA9" "formation 2" + n, warpNames(), 0));
                m.params.push_back (P (k + "_w2amt", "Intensit\xC3\xA9 2" + n, "%", 0, 100, 0));
                m.params.push_back (I (k + "_oct", "Octave" + n, "", -4, 4, 0));
                m.params.push_back (I (k + "_semi", "Demi-tons" + n, "dt", -12, 12, 0));
                m.params.push_back (P (k + "_fine", "Fin" + n, "ct", -100, 100, 0));
                m.params.push_back (P (k + "_lvl", "Niveau" + n, "%", 0, 100, 75));
                m.params.push_back (P (k + "_pan", "Panoramique" + n, "pan", -100, 100, 0));
                m.params.push_back (P (k + "_phase", "Phase" + n, "\xC2\xB0", 0, 360, 0));
                m.params.push_back (P (k + "_rand", "Phase au hasard" + n, "%", 0, 100, 100));
                m.params.push_back (I (k + "_uni", "Unisson" + n, "", 1, maxUni, 1));
                m.params.push_back (P (k + "_udet", "D\xC3\xA9" "saccord" + n, "%", 0, 100, 25));
                m.params.push_back (P (k + "_ublend", "M\xC3\xA9lange unisson" + n, "%", 0, 100, 75));
                m.params.push_back (P (k + "_uwidth", "Largeur unisson" + n, "%", 0, 100, 80));
                m.params.push_back (C (k + "_inv", "Polarit\xC3\xA9" + n, { "Normale", "Invers\xC3\xA9" "e" }));
                m.params.push_back (P (k + "_delay", "Retard" + n, "ms", 0, 100, 0, 0.5f));
                m.params.push_back (C (k + "_dest", "Sortie" + n, gdoc::destNames(), 0));
                m.params.push_back (C (k + "_gsrc", "Source granulaire" + n, granSourceNames(), 1));
                m.params.push_back (P (k + "_gsize", "Taille des grains" + n, "ms", 5, 500, 80, 0.5f));
                m.params.push_back (P (k + "_gdens", "Densit\xC3\xA9" + n, "/s", 1, 200, 30, 0.5f));
                m.params.push_back (P (k + "_gspray", "\xC3\x89" "parpillement" + n, "%", 0, 100, 10));
                m.params.push_back (P (k + "_gscan", "Balayage" + n, "%/s", -100, 100, 8));
                m.params.push_back (P (k + "_gpitch", "Hasard de hauteur" + n, "dt", 0, 12, 0));
                m.params.push_back (C (k + "_gwin", "Fen\xC3\xAAtre" + n, { "Douce", "Triangle", "Plate", "Percussive" }));
                m.params.push_back (P (k + "_grev", "Grains \xC3\xA0 l'envers" + n, "%", 0, 100, 0));
                m.params.push_back (C (k + "_ssrc", "Source spectrale" + n, specSourceNames(), 0));
                m.params.push_back (I (k + "_sparts", "Partiels" + n, "", 4, partials, 32));
                m.params.push_back (P (k + "_sstretch", "\xC3\x89tirement" + n, "%", -100, 100, 0));
                m.params.push_back (P (k + "_sshift", "D\xC3\xA9" "calage" + n, "Hz", -1000, 1000, 0));
                m.params.push_back (P (k + "_stilt", "Pente" + n, "dB", -12, 12, 0));
                m.params.push_back (P (k + "_sodd", "Pair / impair" + n, "%", -100, 100, 0));
                m.params.push_back (P (k + "_sblur", "Flou" + n, "%", 0, 100, 0));
                m.params.push_back (P (k + "_scut", "Coupure spectrale" + n, "Hz", 50, 20000, 20000, 0.3f));
                m.params.push_back (P (k + "_sscan", "Balayage spectral" + n, "%/s", -100, 100, 0));
            }
            m.params.push_back (C ("sub_on", "Sous-oscillateur", onOff, 0));
            m.params.push_back (P ("sub", "Niveau du sous-osc.", "%", 0, 100, 60));
            m.params.push_back (C ("subshape", "Forme du sous-osc.", { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9" }));
            m.params.push_back (I ("suboct", "Octave du sous-osc.", "", -3, 1, -1));
            m.params.push_back (P ("subpan", "Pan du sous-osc.", "pan", -100, 100, 0));
            m.params.push_back (C ("subdest", "Sortie du sous-osc.", gdoc::destNames(), 3));
            m.params.push_back (C ("subinv", "Polarit\xC3\xA9 du sous-osc.", { "Normale", "Invers\xC3\xA9" "e" }));
            m.params.push_back (C ("noise_on", "Bruit", onOff, 0));
            m.params.push_back (P ("noise", "Niveau du bruit", "%", 0, 100, 40));
            m.params.push_back (P ("ncolor", "Couleur du bruit", "%", 0, 100, 70));
            m.params.push_back (C ("ntype", "Type de bruit", { "Blanc", "Rose", "Brun", "Craquements", "Num\xC3\xA9rique" }));
            m.params.push_back (P ("npan", "Pan du bruit", "pan", -100, 100, 0));
            m.params.push_back (C ("ndest", "Sortie du bruit", gdoc::destNames(), 0));
            m.params.push_back (C ("route", "Filtres", { "En s\xC3\xA9rie", "En parall\xC3\xA8le" }));
            for (int f = 1; f <= 2; ++f)
            {
                const auto k = "f" + std::to_string (f);
                const auto n = " " + std::to_string (f);
                m.params.push_back (C (k + "on", "Filtre" + n, onOff, f == 1 ? 1 : 0));
                m.params.push_back (C (k + "type", "Type" + n, filterNames(), f == 1 ? 1 : 3));
                m.params.push_back (P (k + "cut", "Coupure" + n, "Hz", 20, 20000, f == 1 ? 3000 : 120, 0.3f));
                m.params.push_back (P (k + "res", "R\xC3\xA9sonance" + n, "%", 0, 100, 20));
                m.params.push_back (P (k + "drive", "Gain" + n, "dB", 0, 24, 0));
                m.params.push_back (P (k + "shape", "Forme" + n, "%", 0, 100, 0));
                m.params.push_back (P (k + "mix", "M\xC3\xA9lange" + n, "%", 0, 100, 100));
                m.params.push_back (P (k + "key", "Clavier" + n, "%", 0, 100, f == 1 ? 50 : 0));
                m.params.push_back (P (k + "env", "Env. 2" + n, "%", -100, 100, f == 1 ? 30 : 0));
                m.params.push_back (P (k + "vel", "V\xC3\xA9locit\xC3\xA9" + n, "%", 0, 100, 0));
            }
            struct EnvDef { float a, h, d, s, r, ca, cd, cr; };
            const EnvDef ed[numEnvs] { { 1, 0, 400, 85, 250, -20, -50, -50 }, { 1, 0, 500, 20, 400, -20, -50, -50 },
                                       { 0.5f, 0, 90, 0, 80, 0, -60, -50 }, { 300, 0, 300, 50, 300, 0, -40, -40 } };
            for (int e = 0; e < numEnvs; ++e)
            {
                const auto k = "e" + std::to_string (e + 1);
                const auto n = " " + std::to_string (e + 1);
                const auto& x = ed[e];
                m.params.push_back (P (k + "a", "Attaque" + n, "ms", 0, 20000, x.a, 0.25f));
                m.params.push_back (P (k + "h", "Tenue" + n, "ms", 0, 5000, x.h, 0.3f));
                m.params.push_back (P (k + "d", "D\xC3\xA9" "clin" + n, "ms", 1, 20000, x.d, 0.25f));
                m.params.push_back (P (k + "s", "Maintien" + n, "%", 0, 100, x.s));
                m.params.push_back (P (k + "r", "Rel\xC3\xA2" "chement" + n, "ms", 1, 20000, x.r, 0.25f));
                m.params.push_back (P (k + "ca", "Courbe d'attaque" + n, "%", -100, 100, x.ca));
                m.params.push_back (P (k + "cd", "Courbe de d\xC3\xA9" "clin" + n, "%", -100, 100, x.cd));
                m.params.push_back (P (k + "cr", "Courbe de rel\xC3\xA2" "chement" + n, "%", -100, 100, x.cr));
            }
            m.params.push_back (P ("pamt", "Hauteur (env. 3)", "dt", -96, 96, 0));
            for (int l = 1; l <= numLfos; ++l)
            {
                const auto k = "l" + std::to_string (l);
                const auto n = "LFO " + std::to_string (l);
                m.params.push_back (P (k + "rate", n + " vitesse", "Hz", 0.02f, 40, l == 1 ? 0.5f : l == 2 ? 5.0f : 2.0f, 0.4f));
                m.params.push_back (C (k + "shape", n + " forme", lfoShapeNames(), l == 2 ? 0 : 1));
                m.params.push_back (C (k + "sync", n + " synchro", lfoSyncChoices()));
                m.params.push_back (C (k + "mode", n + " mode", { "Libre", "Redéclenché", "En un coup" }, 1));
                m.params.push_back (C (k + "delay", n + " retard", lfoDelayChoices()));
                m.params.push_back (P (k + "smooth", n + " lissage", "%", 0, 100, 0));
                m.params.push_back (P (k + "phase", n + " phase", "\xC2\xB0", 0, 360, 0));
            }
            // rack d'effets
            auto ord = [&] (const std::string& k, int def) { m.params.push_back (I (k, "Rang", "", 1, numFx, def)); };
            m.params.push_back (C ("hy_on", "\xC3\x89largisseur", onOff)); ord ("hy_ord", 1);
            m.params.push_back (P ("hy_rate", "\xC3\x89largisseur : vitesse", "Hz", 0.05f, 8, 0.6f, 0.4f));
            m.params.push_back (P ("hy_depth", "\xC3\x89largisseur : d\xC3\xA9" "saccord", "%", 0, 100, 45));
            m.params.push_back (P ("hy_dim", "\xC3\x89largisseur : espace", "%", 0, 100, 40));
            m.params.push_back (P ("hy_mix", "\xC3\x89largisseur : m\xC3\xA9lange", "%", 0, 100, 50));
            m.params.push_back (C ("ds_on", "Distorsion", onOff)); ord ("ds_ord", 2);
            m.params.push_back (C ("ds_type", "Distorsion : type", Gd2Dist::names()));
            m.params.push_back (P ("ds_drive", "Distorsion : pouss\xC3\xA9" "e", "dB", 0, 48, 18));
            m.params.push_back (C ("ds_pre", "Distorsion : pr\xC3\xA9" "filtre", { "Aucun", "Passe-bas", "Passe-bande", "Passe-haut" }));
            m.params.push_back (P ("ds_freq", "Distorsion : fr\xC3\xA9quence", "Hz", 40, 16000, 800, 0.3f));
            m.params.push_back (P ("ds_mix", "Distorsion : m\xC3\xA9lange", "%", 0, 100, 100));
            m.params.push_back (C ("fl_on", "Flanger", onOff)); ord ("fl_ord", 3);
            m.params.push_back (P ("fl_rate", "Flanger : vitesse", "Hz", 0.02f, 8, 0.25f, 0.4f));
            m.params.push_back (P ("fl_depth", "Flanger : profondeur", "%", 0, 100, 60));
            m.params.push_back (P ("fl_feed", "Flanger : r\xC3\xA9injection", "%", -95, 95, 55));
            m.params.push_back (P ("fl_mix", "Flanger : m\xC3\xA9lange", "%", 0, 100, 50));
            m.params.push_back (C ("ph_on", "Phaser", onOff)); ord ("ph_ord", 4);
            m.params.push_back (P ("ph_rate", "Phaser : vitesse", "Hz", 0.02f, 10, 0.35f, 0.4f));
            m.params.push_back (P ("ph_depth", "Phaser : profondeur", "%", 0, 100, 70));
            m.params.push_back (P ("ph_freq", "Phaser : centre", "Hz", 100, 8000, 900, 0.4f));
            m.params.push_back (P ("ph_feed", "Phaser : r\xC3\xA9injection", "%", -95, 95, 40));
            m.params.push_back (P ("ph_mix", "Phaser : m\xC3\xA9lange", "%", 0, 100, 50));
            m.params.push_back (C ("ch_on", "Chorus", onOff)); ord ("ch_ord", 5);
            m.params.push_back (P ("ch_rate", "Chorus : vitesse", "Hz", 0.03f, 8, 0.6f, 0.4f));
            m.params.push_back (P ("ch_depth", "Chorus : profondeur", "ms", 0, 8, 2.2f));
            m.params.push_back (P ("ch_delay", "Chorus : retard", "ms", 1, 30, 7));
            m.params.push_back (P ("ch_feed", "Chorus : r\xC3\xA9injection", "%", -90, 90, 0));
            m.params.push_back (P ("ch_mix", "Chorus : m\xC3\xA9lange", "%", 0, 100, 50));
            m.params.push_back (C ("dl_on", "D\xC3\xA9lai", onOff)); ord ("dl_ord", 6);
            m.params.push_back (C ("dl_mode", "D\xC3\xA9lai : mode", { "Simple", "Ping-pong", "Bande" }, 1));
            m.params.push_back (C ("dl_sync", "D\xC3\xA9lai : synchro", { "Libre", "Tempo" }, 1));
            m.params.push_back (P ("dl_time", "D\xC3\xA9lai : temps", "ms", 1, 2500, 375, 0.4f));
            m.params.push_back (C ("dl_div", "D\xC3\xA9lai : division", grv::effects::echo::divisions(), 9));
            m.params.push_back (P ("dl_feed", "D\xC3\xA9lai : r\xC3\xA9injection", "%", 0, 110, 40));
            m.params.push_back (P ("dl_low", "D\xC3\xA9lai : coupe-bas", "Hz", 20, 2000, 150, 0.4f));
            m.params.push_back (P ("dl_high", "D\xC3\xA9lai : coupe-haut", "Hz", 1000, 20000, 8000, 0.4f));
            m.params.push_back (P ("dl_mix", "D\xC3\xA9lai : m\xC3\xA9lange", "%", 0, 100, 25));
            m.params.push_back (C ("cp_on", "Compresseur", onOff)); ord ("cp_ord", 7);
            m.params.push_back (C ("cp_mode", "Compresseur : mode", { "Simple", "3 bandes (haut et bas)" }, 1));
            m.params.push_back (P ("cp_depth", "Compresseur : profondeur", "%", 0, 100, 60));
            m.params.push_back (P ("cp_time", "Compresseur : temps", "%", 10, 1000, 100, 0.4f));
            m.params.push_back (P ("cp_up", "Compresseur : vers le haut", "%", 0, 100, 100));
            m.params.push_back (P ("cp_down", "Compresseur : vers le bas", "%", 0, 100, 100));
            m.params.push_back (P ("cp_in", "Compresseur : entr\xC3\xA9" "e", "dB", -24, 24, 0));
            m.params.push_back (P ("cp_out", "Compresseur : sortie", "dB", -24, 24, 0));
            m.params.push_back (P ("cp_low", "Compresseur : grave", "dB", -12, 12, 0));
            m.params.push_back (P ("cp_mid", "Compresseur : m\xC3\xA9" "dium", "dB", -12, 12, 0));
            m.params.push_back (P ("cp_high", "Compresseur : aigu", "dB", -12, 12, 0));
            m.params.push_back (P ("cp_thr", "Compresseur : seuil", "dB", -60, 0, -18));
            m.params.push_back (P ("cp_ratio", "Compresseur : rapport", ":1", 1, 20, 4, 0.5f));
            m.params.push_back (C ("fi_on", "Filtre du rack", onOff)); ord ("fi_ord", 8);
            m.params.push_back (C ("fi_type", "Filtre : type", { "Passe-bas 24", "Passe-bas 12", "Passe-bande", "Passe-haut 12", "Passe-haut 24" }));
            m.params.push_back (P ("fi_cut", "Filtre : coupure", "Hz", 20, 20000, 8000, 0.3f));
            m.params.push_back (P ("fi_res", "Filtre : r\xC3\xA9sonance", "%", 0, 100, 20));
            m.params.push_back (P ("fi_drive", "Filtre : gain", "dB", 0, 24, 0));
            m.params.push_back (P ("fi_mix", "Filtre : m\xC3\xA9lange", "%", 0, 100, 100));
            m.params.push_back (C ("eq_on", "\xC3\x89galiseur", onOff)); ord ("eq_ord", 9);
            m.params.push_back (P ("eq_lf", "\xC3\x89gal. : grave (fr\xC3\xA9q.)", "Hz", 20, 1000, 120, 0.4f));
            m.params.push_back (P ("eq_lg", "\xC3\x89gal. : grave", "dB", -18, 18, 0));
            m.params.push_back (P ("eq_mf", "\xC3\x89gal. : m\xC3\xA9" "dium (fr\xC3\xA9q.)", "Hz", 100, 10000, 1000, 0.3f));
            m.params.push_back (P ("eq_mg", "\xC3\x89gal. : m\xC3\xA9" "dium", "dB", -18, 18, 0));
            m.params.push_back (P ("eq_mq", "\xC3\x89gal. : largeur", "Q", 0.3f, 8, 1, 0.5f));
            m.params.push_back (P ("eq_hf", "\xC3\x89gal. : aigu (fr\xC3\xA9q.)", "Hz", 1000, 18000, 6000, 0.4f));
            m.params.push_back (P ("eq_hg", "\xC3\x89gal. : aigu", "dB", -18, 18, 0));
            m.params.push_back (C ("rv_on", "R\xC3\xA9verb\xC3\xA9ration", onOff)); ord ("rv_ord", 10);
            m.params.push_back (P ("rv_size", "R\xC3\xA9verb. : taille", "%", 0, 100, 55));
            m.params.push_back (P ("rv_pre", "R\xC3\xA9verb. : pr\xC3\xA9-d\xC3\xA9lai", "ms", 0, 250, 15, 0.6f));
            m.params.push_back (P ("rv_bright", "R\xC3\xA9verb. : brillance", "Hz", 1000, 18000, 6500, 0.4f));
            m.params.push_back (P ("rv_low", "R\xC3\xA9verb. : coupe-bas", "Hz", 20, 1000, 150, 0.4f));
            m.params.push_back (P ("rv_width", "R\xC3\xA9verb. : largeur", "%", 0, 150, 100));
            m.params.push_back (P ("rv_mix", "R\xC3\xA9verb. : m\xC3\xA9lange", "%", 0, 100, 25));

            // sections (liste de secours de l'interface ; le panneau propre est dans layouts.ts)
            m.sections = { { "global", "Voix", "global", { "vmode", "voices", "glide", "tune", "spread", "velamp", "level" }, "global" } };
            for (int o = 0; o < numOsc; ++o)
            {
                const std::string k = ok[o];
                std::vector<std::string> ids;
                for (int f = 0; f < oscStride; ++f) ids.push_back (m.params[(size_t) (oscBase + o * oscStride + f)].id);
                m.sections.push_back ({ "osc" + k, std::string ("Oscillateur ") + (char) ('A' + o), "wave", ids, "osc" });
            }
            m.sections.push_back ({ "subnoise", "Sous-oscillateur et bruit", "osc",
                                    { "sub_on", "sub", "subshape", "suboct", "subpan", "subdest", "subinv", "noise_on", "noise", "ncolor", "ntype", "npan", "ndest" }, "osc" });
            m.sections.push_back ({ "sample", "Source (emplacement 1)", "sample", { "a_gsrc", "a_ssrc" }, "osc" });
            for (int f = 1; f <= 2; ++f)
            {
                const auto k = "f" + std::to_string (f);
                m.sections.push_back ({ "filter" + std::to_string (f), "Filtre " + std::to_string (f), "filter",
                                        { k + "on", k + "type", k + "cut", k + "res", k + "drive", k + "shape", k + "mix", k + "key", k + "env", k + "vel", "route" }, "filter" });
            }
            for (int e = 1; e <= numEnvs; ++e)
            {
                const auto k = "e" + std::to_string (e);
                std::vector<std::string> ids { k + "a", k + "h", k + "d", k + "s", k + "r", k + "ca", k + "cd", k + "cr" };
                if (e == 3) ids.push_back ("pamt");
                m.sections.push_back ({ "env" + std::to_string (e), "Enveloppe " + std::to_string (e), "env", ids, e == 1 ? "amp" : "env" });
            }
            for (int l = 1; l <= numLfos; ++l)
            {
                const auto k = "l" + std::to_string (l);
                m.sections.push_back ({ "lfo" + std::to_string (l), "LFO " + std::to_string (l), "lfo",
                                        { k + "sync", k + "rate", k + "shape", k + "mode", k + "delay", k + "smooth", k + "phase" }, "mod" });
            }
            {
                std::vector<std::string> fx;
                for (int i = hyOn; i < numSlots; ++i) fx.push_back (m.params[(size_t) i].id);
                m.sections.push_back ({ "fx", "Effets", "global", fx, "fx" });
            }

            m.presets.push_back ({ "Init", {}, "Init" });

            HubSpec hub;
            hub.extEnvs = { "Env. 1 (ampli)", "Env. 2 (filtres)", "Env. 3 (hauteur)", "Env. 4" };
            hub.extLfos = { "LFO 1", "LFO 2", "LFO 3" };
            hub.envs = 1; hub.firstEnv = 5; hub.envName = "Env.";
            hub.lfos = 2; hub.firstLfo = 4;
            hub.slots = 32; hub.macros = 8; hub.arp = true; hub.arpSteps = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m);

            // formes montrées par l'interface : 16 tables × 12 ondes, 64 points chacune
            for (int t = 0; t < builtinTables; ++t)
                for (int k = 0; k < frames; ++k)
                {
                    std::vector<float> shape (64);
                    for (int i = 0; i < 64; ++i) shape[(size_t) i] = readBuiltin (t, k, WaveBank::Mip { 4, 0.0f }, (double) i / 64.0);
                    float pk = 1.0e-6f;
                    for (auto v : shape) pk = std::max (pk, std::abs (v));
                    for (auto& v : shape) v = std::round (v / pk * 1000.0f) / 1000.0f;
                    m.waveShapes.push_back (std::move (shape));
                }
            m.sampleRuntime = "gooddoc2";
            return m;
        }();
        return d;
    }

    /** Ce que partagent les voix : échantillons de la piste et leurs analyses (fil audio : lecture seule). */
    struct Shared
    {
        const SampleBuffer* sample0 = nullptr;
        const SampleRuntime* rt0 = nullptr;
        std::array<const SampleBuffer*, userSlots> user {};
        std::array<const SampleRuntime*, userSlots> userRt {};
        double baseLevelDb = -6.0; // volume réglé (sans modulation) : il s'applique après le rack d'effets
    };

    /** Enveloppe à tenue et à courbes réglables (attaque, déclin, relâchement). */
    class CurveEnv
    {
    public:
        enum Stage { idle = 0, attack, hold, decay, sustain, release };
        void prepare (double s) noexcept { sr = s; }
        void reset() noexcept { stage = idle; value = 0.0; t = 0.0; }
        void set (double a, double h, double d, double s, double r, double ca, double cd, double cr) noexcept
        {
            aS = std::max (0.0, a) * 0.001 * sr; hS = std::max (0.0, h) * 0.001 * sr;
            dS = std::max (1.0, d * 0.001 * sr); rS = std::max (1.0, r * 0.001 * sr);
            sus = std::clamp (s, 0.0, 1.0);
            if (ca != cA) { cA = ca; kA = kFor (ca); }
            if (cd != cD) { cD = cd; kD = kFor (cd); }
            if (cr != cR) { cR = cr; kR = kFor (cr); }
        }
        void gate (bool on) noexcept
        {
            if (on) { stage = attack; from = value; t = 0.0; }
            else if (stage != idle && stage != release) { stage = release; from = value; t = 0.0; }
        }
        double next() noexcept
        {
            switch (stage)
            {
                case attack:
                    if (aS < 1.0) { value = 1.0; stage = hS >= 1.0 ? hold : decay; t = 0.0; break; }
                    t += 1.0 / aS;
                    if (t >= 1.0) { value = 1.0; stage = hS >= 1.0 ? hold : decay; t = 0.0; break; }
                    value = from + (1.0 - from) * curve (t, kA);
                    break;
                case hold:
                    t += 1.0;
                    if (t >= hS) { stage = decay; t = 0.0; }
                    break;
                case decay:
                    t += 1.0 / dS;
                    if (t >= 1.0) { value = sus; stage = sustain; break; }
                    value = 1.0 + (sus - 1.0) * curve (t, kD);
                    break;
                case sustain: value = sus; break;
                case release:
                    t += 1.0 / rS;
                    if (t >= 1.0) { value = 0.0; stage = idle; break; }
                    value = from * (1.0 - curve (t, kR));
                    break;
                default: break;
            }
            return value;
        }
        bool isActive() const noexcept { return stage != idle; }
        double get() const noexcept { return value; }
    private:
        // courbe 0..1 : 0 = droite ; < 0 = rapide au début (naturel) ; > 0 = lente au début (aspiration)
        static double kFor (double c) noexcept { return std::clamp (c, -100.0, 100.0) * 0.07; }
        static double curve (double x, double k) noexcept
        {
            if (std::abs (k) < 1.0e-3) return x;
            return (std::exp (k * x) - 1.0) / (std::exp (k) - 1.0);
        }
        double sr = 48000.0, value = 0.0, from = 0.0, t = 0.0, aS = 0.0, hS = 0.0, dS = 1.0, rS = 1.0, sus = 1.0;
        double cA = 1.0e9, cD = 1.0e9, cR = 1.0e9, kA = 0.0, kD = 0.0, kR = 0.0;
        Stage stage = idle;
    };

    /** LFO de voix : 9 formes, libre / redéclenché / en un coup, retard, lissage. Sortie -1..1. */
    class VoiceLfo
    {
    public:
        void seed (std::uint32_t s) noexcept { rng = s | 1u; }
        void start (int mode, double phase01) noexcept
        {
            if (mode >= 1) { ph = phase01 - std::floor (phase01); done = false; }
            age = 0.0;
        }
        double next (int shape, int mode, double hz, double sr, double delaySec, double smooth) noexcept
        {
            age += 1.0 / sr;
            double y;
            if (done) y = last;
            else
            {
                const auto prev = ph;
                ph += hz / sr;
                if (ph >= 1.0)
                {
                    if (mode == 2) { ph = 1.0; done = true; }
                    else ph -= std::floor (ph);
                }
                if (ph < prev || age <= 1.0 / sr) { hold0 = hold1; hold1 = rand(); }
                y = shapeAt (shape, ph);
                last = y;
            }
            y *= lfoDelayGain (age, delaySec);
            if (smooth > 0.0) { const auto c = std::exp (-1.0 / std::max (1.0, smooth * 0.05 * sr)); sm = y + (sm - y) * c; return sm; }
            sm = y;
            return y;
        }
    private:
        double rand() noexcept { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (double) rng / 2147483648.0 - 1.0; }
        double shapeAt (int s, double x) const noexcept
        {
            switch (s)
            {
                case 0: return fastSin2Pi (x);
                case 1: return 1.0 - 4.0 * std::abs (x - 0.5);
                case 2: return 2.0 * x - 1.0;
                case 3: return 1.0 - 2.0 * x;
                case 4: return x < 0.5 ? 1.0 : -1.0;
                case 5: return 2.0 * (1.0 - std::exp (-7.0 * x)) / (1.0 - std::exp (-7.0)) - 1.0; // pompe : creux au temps, remonte
                case 6: return 2.0 * x * x * x - 1.0;                                             // aspiration : monte vers la fin
                case 7: return hold1;
                default: { const auto c = 0.5 - 0.5 * std::cos (kPi * x); return hold0 + (hold1 - hold0) * c; }
            }
        }
        double ph = 0.0, age = 0.0, last = 0.0, sm = 0.0, hold0 = 0.0, hold1 = 0.0;
        bool done = false;
        std::uint32_t rng = 0x2545F491u;
    };

    /** Filtre de voix (un canal) : les 14 types de Good Doctor + échelle 18 et 6 dB, disperseur, phaseur. */
    class Filter
    {
    public:
        void prepare (double s) noexcept { sr = s; base.prepare (s); reset(); }
        void reset() noexcept { base.reset(); ladder.reset(); for (auto& a : ap) a.reset(); fb = 0.0; }
        double process (double x, int type, double fc, double res, double drive, double shape) noexcept
        {
            if (type < 14) return base.process (x, type, fc, res, drive, shape);
            fc = std::clamp (fc, 16.0, 0.45 * sr);
            switch (type)
            {
                case 14: return ladder.process (x, fc, res, sr, Ladder::lp18, drive) / std::sqrt (drive);
                case 15: return ladder.process (x, fc, res, sr, Ladder::lp6, drive) / std::sqrt (drive);
                case 16:
                {
                    // disperseur : 8 passe-tout à la coupure (la phase s'étale, le transitoire « siffle »)
                    const auto q = 0.4 + 6.0 * res * res;
                    auto y = softClip (x * drive) / std::sqrt (drive);
                    const auto n = 4 + (int) (shape * 4.0 + 0.5);
                    for (int i = 0; i < n; ++i) { ap[(size_t) i].set (SvfEq::allpass, fc, q, 0.0, sr); y = ap[(size_t) i].process (y); }
                    return y;
                }
                default:
                {
                    // phaseur : 6 passe-tout étalés autour de la coupure, réinjectés, ajoutés au direct
                    auto y = softClip (x * drive + fb * 0.85 * res) / std::sqrt (drive);
                    for (int i = 0; i < 6; ++i)
                    {
                        const auto f = fc * std::pow (2.0, (i - 2.5) * (0.3 + shape * 0.6));
                        ap[(size_t) i].set (SvfEq::allpass, std::clamp (f, 20.0, 0.45 * sr), 0.7, 0.0, sr);
                        y = ap[(size_t) i].process (y);
                    }
                    fb = y;
                    return 0.5 * (x + y);
                }
            }
        }
    private:
        double sr = 48000.0, fb = 0.0;
        GoodDocFilter base;
        Ladder ladder;
        std::array<SvfEq, 8> ap;
    };

    struct Grain
    {
        double pos = 0.0, inc = 0.0, t = 0.0, dt = 0.0, gl = 0.0, gr = 0.0;
        bool on = false;
    };
}

/** Une voix : trois oscillateurs hybrides, sous-oscillateur, bruit, deux filtres stéréo, quatre
    enveloppes à courbes, trois LFO. */
class GoodDoc2Voice
{
public:
    void prepare (double s)
    {
        sr = s;
        for (auto& e : env) e.prepare (s);
        for (auto& f : filt) for (auto& c : f) c.prepare (s);
        reset();
    }
    void reset() noexcept
    {
        for (auto& o : ph) o.fill (0.0);
        for (auto& o : wst) o.fill (0.0);
        last.fill (0.0); age = 0.0;
        sub.reset();
        for (auto& f : filt) for (auto& c : f) c.reset();
        for (auto& e : env) e.reset();
        for (auto& g : grains) for (auto& x : g) x.on = false;
        gTimer.fill (0.0); gScan.fill (0.0); sScan.fill (0.0);
        for (auto& o : sph) o.fill (0.0);
        for (auto& o : samp) o.fill (0.0);
        dcl.reset(); dcr.reset(); nlp.reset(); pinkA = pinkB = pinkC = brown = held = 0.0; holdCount = 0.0;
        active = false;
    }
    void setShared (const gd2::Shared* s) noexcept { shared = s; }
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; uniGain = 1.0 / std::sqrt ((double) std::max (1, count)); }
    void seed (std::uint32_t s) noexcept { rng = s; for (int i = 0; i < gd2::numLfos; ++i) lfo[(size_t) i].seed (s * (std::uint32_t) (i + 7)); }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true; releaseAfterStart = false;
        pan = rand01() * 2.0 - 1.0;
    }
    void release() noexcept { for (auto& e : env) e.gate (false); if (pending) releaseAfterStart = true; }
    bool isActive() const noexcept { return active; }

    void modSources (double* e, double* l) const noexcept
    {
        for (int i = 0; i < gd2::numEnvs; ++i) e[i] = env[(size_t) i].get();
        for (int i = 0; i < gd2::numLfos; ++i) l[i] = lfoOut[(size_t) i];
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace gd2;
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! env[0].isActive())
            {
                for (auto& e : env) e.gate (true);
                age = 0.0;
                for (int o = 0; o < numOsc; ++o)
                {
                    const auto b = oscBase + o * oscStride;
                    const auto r = std::clamp (p[b + ofRand] / 100.0, 0.0, 1.0);
                    for (int u = 0; u < maxUni; ++u)
                    {
                        const auto x = p[b + ofPhase] / 360.0 + r * rand01();
                        ph[(size_t) o][(size_t) u] = x - std::floor (x);
                    }
                    gTimer[(size_t) o] = 1.0e9; // premier grain tout de suite
                    gScan[(size_t) o] = 0.0; sScan[(size_t) o] = 0.0;
                    sph[(size_t) o].fill (0.0);
                }
                for (int l = 0; l < numLfos; ++l)
                {
                    const auto b = lfoBase + l * lfoStride;
                    lfo[(size_t) l].start ((int) p[b + lMode], p[b + lPhase] / 360.0);
                }
                sub.reset (0.0);
            }
            if (releaseAfterStart) { releaseAfterStart = false; for (auto& e : env) e.gate (false); }
        }
        for (int e = 0; e < numEnvs; ++e)
        {
            const auto b = envBase + e * envStride;
            env[(size_t) e].set (p[b + eA], p[b + eH], p[b + eD], p[b + eS] / 100.0, p[b + eR], p[b + eCA], p[b + eCD], p[b + eCR]);
        }
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        // le volume s'applique après le rack (comme le volume général de ces synthés) : la voix ne garde
        // que sa modulation (pompe, aspiration… vers « Volume »), la distorsion reçoit toujours le même niveau
        const auto gain = dbToGain (p[level] - (shared != nullptr ? shared->baseLevelDb : -6.0)) * velocityGain (vel, p[velAmp] / 100.0) * uniGain;
        const auto sp = p[spreadAmt] / 100.0;
        const auto vpan = spread != 0.0 ? spread * std::max (0.6, sp) : pan * sp * 0.5;

        // oscillateurs : réglages lus une fois par bloc
        struct O
        {
            bool on, need; int mode, tab, w1, w2, dest, uni; double pos, a1, a2, off, lvl, panL, panR, udet, gC, gS, uw, inv, delayS;
            const SampleRuntime* urt; const SampleBuffer* ubuf;
        };
        std::array<O, numOsc> os {};
        for (int o = 0; o < numOsc; ++o)
        {
            const auto b = oscBase + o * oscStride;
            auto& x = os[(size_t) o];
            x.mode = std::clamp ((int) p[b + ofMode], 0, 2);
            x.tab = std::clamp ((int) p[b + ofTab], 0, numTableChoices - 1);
            x.w1 = std::clamp ((int) p[b + ofW1], 0, numWarps - 1);
            x.w2 = std::clamp ((int) p[b + ofW2], 0, numWarps - 1);
            x.dest = std::clamp ((int) p[b + ofDest], 0, 3);
            x.pos = std::clamp (p[b + ofPos] / 100.0, 0.0, 1.0);
            x.a1 = std::clamp (p[b + ofW1Amt] / 100.0, 0.0, 1.0);
            x.a2 = std::clamp (p[b + ofW2Amt] / 100.0, 0.0, 1.0);
            x.off = 12.0 * std::round (p[b + ofOct]) + std::round (p[b + ofSemi]) + p[b + ofFine] / 100.0;
            x.lvl = p[b + ofLvl] / 100.0;
            const auto pn = std::clamp (p[b + ofPan] / 100.0 + vpan, -1.0, 1.0);
            x.panL = std::cos ((pn + 1.0) * 0.25 * kPi) * 1.4142; x.panR = std::sin ((pn + 1.0) * 0.25 * kPi) * 1.4142;
            x.uni = x.mode == mTable ? std::clamp ((int) std::round (p[b + ofUni]), 1, maxUni) : 1;
            const auto ud = p[b + ofUDet] / 100.0;
            x.udet = ud * ud;             // demi-tons de part et d'autre à 100 %
            const auto bl = std::clamp (p[b + ofUBlend] / 100.0, 0.0, 1.0);
            x.gC = x.uni > 1 ? std::sqrt (1.0 - 0.75 * bl) : 1.0;
            x.gS = x.uni > 1 ? bl : 0.0;
            if (x.uni > 1)
            {
                const auto sides = (double) (x.uni - (x.uni % 2 == 1 ? 1 : 0));
                const auto centers = (double) (x.uni % 2 == 1 ? 1 : 0);
                const auto norm = 1.0 / std::sqrt (centers * x.gC * x.gC + sides * x.gS * x.gS + 1.0e-9);
                x.gC *= norm; x.gS *= norm;
            }
            x.uw = std::clamp (p[b + ofUWidth] / 100.0 * (0.5 + sp), 0.0, 1.0);
            x.inv = p[b + ofInv] > 0.5f ? -1.0 : 1.0;
            x.delayS = p[b + ofDelay] * 0.001 * sr;
            x.on = p[b + ofOn] > 0.5f && x.lvl > 0.0;
            x.urt = nullptr; x.ubuf = nullptr;
            if (x.tab >= builtinTables && shared != nullptr)
            {
                x.urt = shared->userRt[(size_t) (x.tab - builtinTables)];
                x.ubuf = shared->user[(size_t) (x.tab - builtinTables)];
            }
        }
        auto modsNext = [] (const O& x) { auto m = [] (int w, double a) { return a > 0.0 && (w == wFm || w == wAm || w == wRing); }; return x.mode == mTable && (m (x.w1, x.a1) || m (x.w2, x.a2)); };
        for (int o = 0; o < numOsc; ++o) os[(size_t) o].need = os[(size_t) o].on;
        for (int o = 0; o < numOsc; ++o) if (os[(size_t) o].on && modsNext (os[(size_t) o])) os[(size_t) ((o + 1) % numOsc)].need = true;

        const bool subOnB = p[subOn] > 0.5f && p[subLvl] > 0.0f;
        const auto lvSub = subOnB ? p[subLvl] / 100.0 * uniGain * (p[subInv] > 0.5f ? -1.0 : 1.0) : 0.0;
        const bool fmSub = std::any_of (os.begin(), os.end(), [] (const O& x) { auto u = [] (int w, double a) { return a > 0.0 && (w == wFmSub || w == wRingSub); }; return x.on && x.mode == mTable && (u (x.w1, x.a1) || u (x.w2, x.a2)); });
        const auto subWave = std::array<Osc::Wave, 4> { Osc::sine, Osc::triangle, Osc::saw, Osc::pulse }[(size_t) std::clamp ((int) p[subShape], 0, 3)];
        const auto subOff = 12.0 * std::round (p[subOct]);
        const auto subD = std::clamp ((int) p[subDest], 0, 3), nD = std::clamp ((int) p[noiseDest], 0, 3);
        const auto subP = std::clamp (p[subPan] / 100.0, -1.0, 1.0);
        const auto subL = std::cos ((subP + 1.0) * 0.25 * kPi) * 1.4142, subR = std::sin ((subP + 1.0) * 0.25 * kPi) * 1.4142;
        const bool noiseOnB = p[noiseOn] > 0.5f && p[noiseLvl] > 0.0f;
        const auto lvN = noiseOnB ? p[noiseLvl] / 100.0 * uniGain : 0.0;
        const auto nType = std::clamp ((int) p[noiseType], 0, 4);
        const auto nCut = 300.0 * std::pow (60.0, p[noiseColor] / 100.0);
        const auto nP = std::clamp (p[noisePan] / 100.0, -1.0, 1.0);
        const auto nL = std::cos ((nP + 1.0) * 0.25 * kPi) * 1.4142, nR = std::sin ((nP + 1.0) * 0.25 * kPi) * 1.4142;

        const bool parallel = p[route] > 0.5f;
        struct F { bool on; int type; double cut, res, drive, shape, mix, key, env, vel; };
        std::array<F, 2> fs {};
        for (int f = 0; f < 2; ++f)
        {
            const auto b = fBase + f * fStride;
            auto& x = fs[(size_t) f];
            x.on = p[b + ffOn] > 0.5f;
            x.type = std::clamp ((int) p[b + ffType], 0, 17);
            x.cut = p[b + ffCut]; x.res = std::clamp (p[b + ffRes] / 100.0, 0.0, 1.0); x.drive = dbToGain (p[b + ffDrive]);
            x.shape = std::clamp (p[b + ffShape] / 100.0, 0.0, 1.0); x.mix = std::clamp (p[b + ffMix] / 100.0, 0.0, 1.0);
            x.key = p[b + ffKey] / 100.0; x.env = p[b + ffEnv] / 100.0 * 7.0; x.vel = p[b + ffVel] / 100.0 * (vel - 0.6) * 4.0;
        }
        const auto pAmtSt = (double) p[pAmt];
        struct Lf { int shape, mode; double hz, dly, smooth; };
        std::array<Lf, numLfos> lf {};
        for (int l = 0; l < numLfos; ++l)
        {
            const auto b = lfoBase + l * lfoStride;
            lf[(size_t) l] = { (int) p[b + lShape], (int) p[b + lMode], (double) p[b + lRate], lfoDelaySeconds ((int) p[b + lDelay]), p[b + lSmooth] / 100.0 };
        }

        // granulaire et spectral : sources et réglages du bloc
        prepareGranular (p, os.data());
        prepareSpectral (p, os.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const auto a = env[0].next(); const auto ev2 = env[1].next(); const auto ev3 = env[2].next(); (void) env[3].next();
            for (int l = 0; l < numLfos; ++l)
            {
                const auto& x = lf[(size_t) l];
                lfoOut[(size_t) l] = lfo[(size_t) l].next (x.shape, x.mode, x.hz, sr, x.dly, x.smooth);
            }
            age += 1.0;
            const auto note = pitch.next (glideCoef) + p[tune] + ev3 * pAmtSt;

            double subOut = 0.0;
            if (lvSub != 0.0 || fmSub) subOut = sub.next (subWave, std::min (0.45, noteHz (note + subOff) / sr), 0.5);
            const auto noiseNow = (lvN > 0.0) ? noiseSample (nType, nCut) : 0.0;

            std::array<std::array<double, 2>, 4> bus {};
            for (int o = numOsc - 1; o >= 0; --o)
            {
                auto& x = os[(size_t) o];
                if (! x.need) { last[(size_t) o] = 0.0; continue; }
                if (age <= x.delayS) { last[(size_t) o] = 0.0; continue; } // retard de départ : l'oscillateur attend
                const auto fadeIn = x.delayS > 0.0 ? std::min (1.0, (age - x.delayS) / (0.001 * sr)) : 1.0;
                double yl = 0.0, yr = 0.0;
                const auto other = last[(size_t) ((o + 1) % numOsc)];
                switch (x.mode)
                {
                    case mGranular: granular (o, note + x.off, x, yl, yr); break;
                    case mSpectral: spectral (o, note + x.off, yl, yr); yl *= x.panL; yr *= x.panR; break;
                    default:
                    {
                        const auto base = noteHz (note + x.off) / sr;
                        double mono = 0.0;
                        for (int u = 0; u < x.uni; ++u)
                        {
                            const auto s = x.uni > 1 ? 2.0 * u / (x.uni - 1) - 1.0 : 0.0;
                            const bool center = x.uni % 2 == 1 && u == x.uni / 2;
                            const auto g = x.uni == 1 ? 1.0 : (center ? x.gC : x.gS);
                            const auto dt = x.uni > 1 ? base * fastExp2 (s * x.udet / 12.0) : base;
                            const auto y = tableOsc (x, ph[(size_t) o][(size_t) u], dt, other, subOut, noiseNow, &wst[(size_t) o][(size_t) (u * 2)]) * g;
                            if (u == 0 || center) mono = y;
                            const auto pu = s * x.uw;
                            yl += y * (1.0 - std::max (0.0, pu)); yr += y * (1.0 + std::min (0.0, pu));
                        }
                        last[(size_t) o] = mono;
                        yl *= x.panL; yr *= x.panR;
                        break;
                    }
                }
                if (x.mode != mTable) last[(size_t) o] = 0.5 * (yl + yr);
                if (x.on)
                {
                    const auto k = x.lvl * x.inv * fadeIn;
                    bus[(size_t) x.dest][0] += yl * k; bus[(size_t) x.dest][1] += yr * k;
                }
            }
            if (lvSub != 0.0) { bus[(size_t) subD][0] += subOut * lvSub * subL; bus[(size_t) subD][1] += subOut * lvSub * subR; }
            if (lvN > 0.0) { const auto y = noiseNow * lvN; bus[(size_t) nD][0] += y * nL; bus[(size_t) nD][1] += y * nR; }

            double out[2];
            for (int c = 0; c < 2; ++c)
            {
                auto run = [&] (int f, double in)
                {
                    const auto& x = fs[(size_t) f];
                    if (! x.on) return in;
                    const auto fc = x.cut * fastExp2 (x.key * (note - 60.0) / 12.0 + x.env * ev2 + x.vel);
                    const auto y = filt[(size_t) f][(size_t) c].process (in, x.type, fc, x.res, x.drive, x.shape);
                    return in + (y - in) * x.mix;
                };
                const auto b0 = bus[0][(size_t) c], b1 = bus[1][(size_t) c], b2 = bus[2][(size_t) c], b3 = bus[3][(size_t) c];
                double y;
                if (! parallel) y = run (1, run (0, (b0 + b1) * 0.35) + b2 * 0.35) + b3 * 0.35;
                else
                {
                    const auto y1 = fs[0].on ? run (0, (b0 + b1) * 0.35) : (b0 + b1) * 0.35;
                    const auto y2 = fs[1].on ? run (1, (b0 + b2) * 0.35) : b2 * 0.35;
                    y = (fs[0].on && fs[1].on ? 0.7 : 1.0) * (y1 + y2) + b3 * 0.35;
                }
                out[c] = y;
            }
            L[i] += (float) (dcl.process (out[0]) * a * gain);
            R[i] += (float) (dcr.process (out[1]) * a * gain);
        }
        if (! env[0].isActive()) active = false;
    }

private:
    //--------------------------------------------------------------------------- tables
    double readTable (const gd2::Shared* sh, int table, const gd2::SampleRuntime* urt, const SampleBuffer* ubuf, double pos, WaveBank::Mip m, double phs) const noexcept
    {
        using namespace gd2;
        if (table < builtinTables)
        {
            const auto fp = pos * (frames - 1);
            const auto k0 = std::min ((int) fp, frames - 2);
            const auto f = fp - (double) k0;
            const auto y0 = (double) readBuiltin (table, k0, m, phs);
            return f < 1.0e-4 ? y0 : y0 + ((double) readBuiltin (table, k0 + 1, m, phs) - y0) * f;
        }
        (void) sh;
        if (urt != nullptr && urt->tableFrames > 0)
        {
            const auto nf = urt->tableFrames;
            if (nf == 1) return urt->table.read (0, m, phs);
            const auto fp = pos * (nf - 1);
            const auto k0 = std::min ((int) fp, nf - 2);
            const auto f = fp - (double) k0;
            const auto y0 = (double) urt->table.read (k0, m, phs);
            return f < 1.0e-4 ? y0 : y0 + ((double) urt->table.read (k0 + 1, m, phs) - y0) * f;
        }
        if (ubuf != nullptr && ubuf->data.size() >= 16)
        {
            // table chargée sans analyse (instrument changé après le chargement) : lecture directe
            const auto len = (int) ubuf->data.size();
            const auto fs = len >= userFrameSize ? userFrameSize : len;
            const auto nf = std::max (1, std::min (userMaxFrames, len / fs));
            const auto k = std::min (nf - 1, (int) std::round (pos * (nf - 1)));
            const auto x = phs * fs;
            const auto i0 = (int) x;
            const auto fr = x - i0;
            const auto* d = ubuf->data.data() + (size_t) k * fs;
            return (double) d[i0 % fs] * (1.0 - fr) + (double) d[(i0 + 1) % fs] * fr;
        }
        return 0.0;
    }

    static double warpPhase (int w, double a, double phs, double& slope, double& window, double other, double subv, double noise) noexcept
    {
        using namespace gd2;
        if (a <= 0.0) return phs;
        auto wrap = [] (double x) { return x - std::floor (x); };
        switch (w)
        {
            case wSync: { const auto r = 1.0 + 7.0 * a; slope *= r; window *= 1.0 - 0.5 * a * phs; return wrap (phs * r); }
            case wWinSync: { const auto r = 1.0 + 7.0 * a; slope *= r; window *= 0.5 - 0.5 * std::cos (kTwoPi * phs); return wrap (phs * r); }
            case wBendUp: { const auto e = 1.0 + 3.0 * a; slope *= e; return std::pow (phs, e); }
            case wBendDown: { const auto e = 1.0 + 3.0 * a; slope *= e; return 1.0 - std::pow (1.0 - phs, e); }
            case wPulse: { const auto pw = 1.0 - 0.9 * a; slope /= pw; return phs < pw ? phs / pw : 0.0; }
            case wMirror: { const auto mir = phs < 0.5 ? phs : 1.0 - phs; slope *= 1.0 + a; return phs + a * (mir - phs); }
            case wAsym:
            {
                const auto k = 0.5 - 0.46 * a;
                slope *= 0.5 / k;
                return phs < k ? 0.5 * phs / k : 0.5 + 0.5 * (phs - k) / (1.0 - k);
            }
            case wPdSoft: { slope *= 1.0 + 1.5 * a; return wrap (phs + 0.25 * a * std::sin (kTwoPi * phs)); }
            case wFlip: { slope *= 1.0 + a; return phs < 0.5 ? phs : wrap (phs + 0.5 * a); }
            case wQuant: { const auto q = 64.0 - 60.0 * a; return std::floor (phs * q) / q; }
            case wFm: slope *= 1.0 + 3.0 * a; return wrap (phs + 0.6 * a * other);
            case wFmSub: slope *= 1.0 + 2.0 * a; return wrap (phs + 0.5 * a * subv);
            case wFmNoise: slope *= 1.0 + 2.0 * a; return wrap (phs + 0.15 * a * noise);
            default: return phs;
        }
    }

    double warpAmp (int w, double a, double y, double other, double subv, double& st) const noexcept
    {
        using namespace gd2;
        if (a <= 0.0) return y;
        switch (w)
        {
            case wAm: return y * ((1.0 - a) + a * (0.5 + 0.5 * other));
            case wRing: return y * (1.0 - a) + y * other * a;
            case wRingSub: return y * (1.0 - a) + y * subv * a;
            case wFold: { auto x = y * (1.0 + 4.0 * a); x = (x + 1.0) * 0.25; x -= std::floor (x); return std::abs (x * 4.0 - 2.0) - 1.0; }
            case wSat: return std::tanh (y * (1.0 + 6.0 * a)) / std::tanh (1.0 + 6.0 * a * 0.3);
            case wRect: { const auto r = 1.6 * std::abs (y) - 0.7; return y + (r - y) * a; }
            case wBits: { const auto q = 16.0 - 14.5 * a; return std::round (y * q) / q; }
            case wSquash: { const auto d = 1.0 + 8.0 * a; return y >= 0.0 ? std::tanh (y * d) / std::tanh (d) : y; }
            case wHalf: { const auto h = y > 0.0 ? y * 2.0 - 0.6 : -0.6; return y + (h - y) * a; }
            case wLow:
            {
                const auto g = std::exp (-kTwoPi * (20000.0 * std::pow (0.006, a)) / sr);
                st = y + (st - y) * g;
                return st;
            }
            case wHigh:
            {
                const auto g = std::exp (-kTwoPi * (20.0 * std::pow (300.0, a)) / sr);
                st = y + (st - y) * g;
                return y - st;
            }
            default: return y;
        }
    }

    template <typename X>
    double tableOsc (const X& x, double& phs, double dt, double other, double subv, double noise, double* st) const noexcept
    {
        dt = std::clamp (dt, 1.0e-7, 0.45);
        double slope = 1.0, window = 1.0;
        auto rp = warpPhase (x.w1, x.a1, phs, slope, window, other, subv, noise);
        rp = warpPhase (x.w2, x.a2, rp, slope, window, other, subv, noise);
        rp = std::clamp (rp, 0.0, 0.999999);
        const auto mip = WaveBank::mipFor (std::min (0.45, dt * slope));
        auto y = readTable (shared, x.tab, x.urt, x.ubuf, x.pos, mip, rp) * window;
        if (! gd2::phaseWarp (x.w1)) y = warpAmp (x.w1, x.a1, y, other, subv, st[0]);
        if (! gd2::phaseWarp (x.w2)) y = warpAmp (x.w2, x.a2, y, other, subv, st[1]);
        phs += dt;
        phs -= std::floor (phs);
        return y;
    }

    //--------------------------------------------------------------------------- granulaire
    struct GSrc { const float* data = nullptr; int len = 0; double sr = 48000.0, rootNote = 48.0; };
    std::array<GSrc, gd2::numOsc> gsrc {};
    struct GPar { double size, interval, spray, scan, pitchR, rev; int win; double width, gain; };
    std::array<GPar, gd2::numOsc> gpar {};

    template <typename X>
    void prepareGranular (const float* p, const X* os) noexcept
    {
        using namespace gd2;
        for (int o = 0; o < numOsc; ++o)
        {
            if (os[o].mode != mGranular) continue;
            const auto b = oscBase + o * oscStride;
            const auto src = std::clamp ((int) p[b + ofGSrc], 0, numCalcSources);
            auto& g = gsrc[(size_t) o];
            g = {};
            if (src == 0 && shared != nullptr && shared->sample0 != nullptr && shared->sample0->data.size() > 64)
            {
                g.data = shared->sample0->data.data(); g.len = (int) shared->sample0->data.size(); g.sr = shared->sample0->sampleRate;
                const auto root = shared->rt0 != nullptr ? shared->rt0->spec.rootHz : 261.63;
                g.rootNote = 69.0 + 12.0 * std::log2 (std::max (20.0, root) / 440.0);
            }
            else
            {
                const auto& c = calcSources()[(size_t) std::max (0, src - 1)];
                g.data = c.data.data(); g.len = (int) c.data.size(); g.sr = c.sr; g.rootNote = calcRootNote;
            }
            auto& q = gpar[(size_t) o];
            q.size = std::max (8.0, p[b + ofGSize] * 0.001 * sr);
            q.interval = sr / std::max (0.5f, p[b + ofGDens]);
            q.spray = p[b + ofGSpray] / 100.0;
            q.scan = p[b + ofGScan] / 100.0;
            q.pitchR = p[b + ofGPitch];
            q.rev = p[b + ofGRev] / 100.0;
            q.win = std::clamp ((int) p[b + ofGWin], 0, 3);
            q.width = std::clamp (p[b + ofUWidth] / 100.0, 0.0, 1.0);
            q.gain = 1.0 / std::sqrt (std::max (1.0, q.size / q.interval));
        }
    }

    template <typename X>
    void granular (int o, double note, const X& x, double& yl, double& yr) noexcept
    {
        const auto& g = gsrc[(size_t) o];
        const auto& q = gpar[(size_t) o];
        if (g.data == nullptr || g.len < 64) return;
        auto& pool = grains[(size_t) o];
        // balayage : la position avance (ou recule) d'une part de la source par seconde
        auto& scan = gScan[(size_t) o];
        scan += q.scan / sr;
        scan -= std::floor (scan);
        auto& tm = gTimer[(size_t) o];
        tm += 1.0;
        if (tm >= q.interval)
        {
            tm = 0.0;
            for (auto& gr : pool)
            {
                if (gr.on) continue;
                auto start = x.pos + scan + q.spray * (rand01() - 0.5);
                start -= std::floor (start);
                const auto semis = (note - g.rootNote) + (q.pitchR > 0.0 ? q.pitchR * (rand01() * 2.0 - 1.0) : 0.0);
                auto inc = fastExp2 (semis / 12.0) * g.sr / sr;
                if (rand01() < q.rev) inc = -inc;
                gr.on = true; gr.pos = start * (g.len - 2); gr.inc = inc; gr.t = 0.0; gr.dt = 1.0 / q.size;
                const auto pn = (rand01() * 2.0 - 1.0) * q.width;
                gr.gl = std::cos ((pn + 1.0) * 0.25 * kPi) * 1.4142; gr.gr = std::sin ((pn + 1.0) * 0.25 * kPi) * 1.4142;
                break;
            }
        }
        for (auto& gr : pool)
        {
            if (! gr.on) continue;
            auto p0 = gr.pos;
            if (p0 < 0.0) p0 += g.len - 2; else if (p0 >= g.len - 2) p0 -= g.len - 2;
            const auto i0 = (int) p0;
            const auto fr = p0 - i0;
            const auto s = (double) g.data[(size_t) i0] + ((double) g.data[(size_t) i0 + 1] - g.data[(size_t) i0]) * fr;
            const auto t = gr.t;
            double w;
            switch (q.win)
            {
                case 1: w = 1.0 - std::abs (2.0 * t - 1.0); break;
                case 2: w = std::min (1.0, std::min (t, 1.0 - t) * 8.0); break;
                case 3: w = t < 0.03 ? t / 0.03 : std::exp (-5.0 * (t - 0.03)); break;
                default: w = 0.5 - 0.5 * std::cos (kTwoPi * t); break;
            }
            const auto y = s * w * q.gain * 2.6; // même niveau que les tables (fenêtre et recouvrement compensés)
            yl += y * gr.gl; yr += y * gr.gr;
            gr.pos = p0 + gr.inc;
            gr.t += gr.dt;
            if (gr.t >= 1.0) gr.on = false;
        }
        yl *= x.panL; yr *= x.panR;
    }

    //--------------------------------------------------------------------------- spectral
    struct SPar { int parts = 0; double gain = 0.0; };
    std::array<SPar, gd2::numOsc> spar {};
    std::array<std::array<double, gd2::partials>, gd2::numOsc> samp {}, sstep {};

    template <typename X>
    void prepareSpectral (const float* p, const X* os, int n) noexcept
    {
        using namespace gd2;
        for (int o = 0; o < numOsc; ++o)
        {
            if (os[o].mode != mSpectral) { spar[(size_t) o].parts = 0; continue; }
            const auto b = oscBase + o * oscStride;
            const Spectrum* spec = nullptr;
            const auto src = std::clamp ((int) p[b + ofSSrc], 0, numCalcSources + 1);
            if (src == 0) spec = &tableSpectra()[(size_t) std::min (os[o].tab, builtinTables - 1)];
            else if (src == 1 && shared != nullptr && shared->rt0 != nullptr && shared->rt0->spec.frames > 0) spec = &shared->rt0->spec;
            else spec = &calcSources()[(size_t) std::max (0, src - 2)].spec;
            if (spec == nullptr || spec->frames <= 0) { spar[(size_t) o].parts = 0; continue; }
            // position : réglage + balayage (la position court sur les tranches)
            auto& sc = sScan[(size_t) o];
            sc += p[b + ofSScan] / 100.0 * n / sr;
            sc -= std::floor (sc);
            auto pos = os[o].pos + sc;
            pos -= std::floor (pos);
            if (os[o].pos >= 0.999 && p[b + ofSScan] == 0.0f) pos = 1.0;
            const auto fp = pos * (spec->frames - 1);
            const auto k0 = std::min ((int) fp, std::max (0, spec->frames - 2));
            const auto fr = spec->frames > 1 ? fp - k0 : 0.0;
            const auto k1 = std::min (k0 + 1, spec->frames - 1);

            const auto parts = std::clamp ((int) std::round (p[b + ofSParts]), 4, partials);
            const auto stretch = p[b + ofSStretch] / 100.0;
            const auto shiftHz = (double) p[b + ofSShift];
            const auto tilt = (double) p[b + ofSTilt];
            const auto odd = p[b + ofSOdd] / 100.0;
            const auto blur = std::clamp (p[b + ofSBlur] / 100.0, 0.0, 1.0);
            const auto cut = (double) p[b + ofSCut];
            const auto f0 = noteHz (pitch.get() + p[tune] + (double) p[b + ofOct] * 12.0 + p[b + ofSemi] + p[b + ofFine] / 100.0
                                    + env[2].get() * p[pAmt]);
            const auto smooth = 1.0 - blur * 0.985;
            double sum2 = 0.0;
            for (int k = 0; k < partials; ++k)
            {
                double a = 0.0;
                double hz = 0.0;
                if (k < parts)
                {
                    const auto h = (double) (k + 1);
                    hz = f0 * std::pow (h, 1.0 + 0.15 * stretch) + shiftHz;
                    if (hz > 10.0 && hz < 0.45 * sr)
                    {
                        a = spec->at (k0, k) + (spec->at (k1, k) - spec->at (k0, k)) * fr;
                        if (tilt != 0.0) a *= std::pow (10.0, tilt * std::log2 (h) / 20.0);
                        const bool even = (k + 1) % 2 == 0;
                        if (odd > 0.0 && even) a *= 1.0 - odd;
                        if (odd < 0.0 && ! even && k > 0) a *= 1.0 + odd;
                        a /= 1.0 + std::pow (hz / cut, 8.0);
                    }
                }
                auto& s = samp[(size_t) o][(size_t) k];
                s += (a - s) * smooth;
                sum2 += s * s;
                // pas de phase du partiel pour ce bloc
                const auto w = kTwoPi * std::max (0.0, hz) / sr;
                sstep[(size_t) o][(size_t) k] = w;
            }
            spar[(size_t) o].parts = parts;
            spar[(size_t) o].gain = 0.7 / std::max (0.3, std::sqrt (sum2));
        }
    }

    void spectral (int o, double, double& yl, double& yr) noexcept
    {
        const auto& q = spar[(size_t) o];
        if (q.parts <= 0) return;
        auto& phs = sph[(size_t) o];      // phases (radians) des partiels
        const auto& amps = samp[(size_t) o];
        const auto& stp = sstep[(size_t) o];
        double y = 0.0;
        for (int k = 0; k < q.parts; ++k)
        {
            auto& ph0 = phs[(size_t) k];
            ph0 += stp[(size_t) k];
            if (ph0 > kTwoPi) ph0 -= kTwoPi;
            y += amps[(size_t) k] * fastSin2Pi (ph0 / kTwoPi);
        }
        y *= q.gain;
        yl += y; yr += y;
    }

    //--------------------------------------------------------------------------- bruit
    double noiseSample (int type, double cut) noexcept
    {
        const auto w = noise.next();
        switch (type)
        {
            case 1: { pinkA = 0.99765 * pinkA + w * 0.0990460; pinkB = 0.96300 * pinkB + w * 0.2965164; pinkC = 0.57000 * pinkC + w * 1.0526913; return nlp.lowpass ((pinkA + pinkB + pinkC + w * 0.1848) * 0.25, cut, sr); }
            case 2: { brown = std::clamp (brown + w * 0.04, -1.0, 1.0); brown *= 0.998; return nlp.lowpass (brown * 2.5, cut, sr); }
            case 3: { const auto r = rand01(); return r > 1.0 - 0.002 * (1.0 + cut / 2000.0) ? (rand01() * 2.0 - 1.0) * 2.0 : 0.0; }
            case 4:
            {
                holdCount += cut / sr;
                if (holdCount >= 1.0) { holdCount -= std::floor (holdCount); held = w; }
                return held;
            }
            default: return nlp.lowpass (w, cut, sr) * 1.2;
        }
    }

    double rand01() noexcept { rng = rng * 1664525u + 1013904223u; return (double) (rng >> 8) / 16777216.0; }

    const gd2::Shared* shared = nullptr;
    double sr = 48000.0, pan = 0.0, spread = 0.0, uniGain = 1.0, age = 0.0;
    std::array<std::array<double, gd2::maxUni>, gd2::numOsc> ph {};
    std::array<std::array<double, gd2::maxUni * 2>, gd2::numOsc> wst {};
    std::array<double, gd2::numOsc> last {}, gTimer {}, gScan {}, sScan {};
    std::array<std::array<gd2::Grain, gd2::maxGrains>, gd2::numOsc> grains {};
    std::array<std::array<double, gd2::partials>, gd2::numOsc> sph {};
    std::array<double, gd2::numLfos> lfoOut {};
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false, releaseAfterStart = false;
    std::uint32_t rng = 7;
    Osc sub;
    std::array<std::array<gd2::Filter, 2>, 2> filt;
    std::array<gd2::CurveEnv, gd2::numEnvs> env;
    std::array<gd2::VoiceLfo, gd2::numLfos> lfo;
    Glide pitch;
    WhiteNoise noise { 0xD0C72u };
    OnePole nlp;
    DcBlocker dcl, dcr;
    double pinkA = 0.0, pinkB = 0.0, pinkC = 0.0, brown = 0.0, held = 0.0, holdCount = 0.0;
};

class GoodDoctor2 final : public SynthBase<GoodDoc2Voice, 8>
{
public:
    GoodDoctor2()
        : hyper (std::make_unique<grv::effects::TriChorus>()), dim (std::make_unique<grv::effects::Dimension>()),
          flanger (std::make_unique<grv::effects::Flanger>()), phaser (std::make_unique<grv::effects::Phaser>()),
          chorus (std::make_unique<grv::effects::Chorus>()), delay (std::make_unique<grv::effects::Delay>()),
          filterFx (std::make_unique<grv::effects::FilterFx>()), reverb (std::make_unique<grv::effects::Reverb>())
    {
        // tables, sources et spectres calculés à la création, jamais sur le fil audio
        (void) gluta::bank(); (void) gdoc::ownBank(); (void) gd2::newBank(); (void) gd2::calcSources(); (void) gd2::tableSpectra();
        for (int i = 0; i < 8; ++i) { voices[(size_t) i].seed (0xD0C2u * (std::uint32_t) (i + 3)); voices[(size_t) i].setShared (&shared); }
        ix.hyRate = hyper.index ("rate"); ix.hyDepth = hyper.index ("depth"); ix.hyMix = hyper.index ("mix"); ix.hyMode = hyper.index ("mode"); ix.hyWidth = hyper.index ("width");
        ix.dimMix = dim.index ("mix"); ix.dimMode = dim.index ("mode");
        ix.flRate = flanger.index ("rate"); ix.flDepth = flanger.index ("depth"); ix.flFeed = flanger.index ("feedback"); ix.flMix = flanger.index ("mix");
        ix.phRate = phaser.index ("rate"); ix.phDepth = phaser.index ("depth"); ix.phCenter = phaser.index ("center"); ix.phFeed = phaser.index ("feedback"); ix.phMix = phaser.index ("mix");
        ix.chRate = chorus.index ("rate"); ix.chDepth = chorus.index ("depth"); ix.chDelay = chorus.index ("delay"); ix.chFeed = chorus.index ("feedback"); ix.chMix = chorus.index ("mix");
        ix.dlMode = delay.index ("mode"); ix.dlSync = delay.index ("sync"); ix.dlTime = delay.index ("time"); ix.dlDiv = delay.index ("division"); ix.dlFeed = delay.index ("feedback");
        ix.dlMix = delay.index ("mix"); ix.dlLow = delay.index ("lowcut"); ix.dlHigh = delay.index ("highcut"); ix.dlSat = delay.index ("saturation");
        ix.fiType = filterFx.index ("type"); ix.fiCut = filterFx.index ("cutoff"); ix.fiRes = filterFx.index ("reso"); ix.fiDrive = filterFx.index ("drive"); ix.fiMix = filterFx.index ("mix");
        ix.fiLfo = filterFx.index ("lfodepth"); ix.fiEnv = filterFx.index ("envdepth");
        ix.rvSize = reverb.index ("size"); ix.rvPre = reverb.index ("predelay"); ix.rvBright = reverb.index ("brightness"); ix.rvLow = reverb.index ("lowcut");
        ix.rvWidth = reverb.index ("width"); ix.rvMix = reverb.index ("mix");
    }
    const ModuleDesc& desc() const noexcept override { return gd2::desc(); }

    void prepare (double sampleRate, int maxBlock) override
    {
        SynthBase::prepare (sampleRate, maxBlock);
        const auto b = std::max (64, maxBlock);
        for (auto* f : { &hyper, &dim, &flanger, &phaser, &chorus, &delay, &filterFx, &reverb }) f->prepare (sampleRate, b);
        dist.prepare (sampleRate); comp.prepare (sampleRate); eq.prepare (sampleRate);
        tailOn = false; quiet = 0;
    }
    void reset() noexcept override
    {
        SynthBase::reset();
        for (auto* f : { &hyper, &dim, &flanger, &phaser, &chorus, &delay, &filterFx, &reverb }) f->reset();
        dist.reset(); comp.reset(); eq.reset(); outPrev = -1.0f;
        tailOn = false; quiet = 0;
    }
    void setSample (int index, const SampleBuffer* s) noexcept override
    {
        auto rt = [] (const SampleBuffer* b) { return b != nullptr && ! b->firmware && b->runtime != nullptr ? static_cast<const gd2::SampleRuntime*> (b->runtime.get()) : nullptr; };
        if (index == 0) { shared.sample0 = s; shared.rt0 = rt (s); }
        else if (index >= 1 && index <= gd2::userSlots) { shared.user[(size_t) (index - 1)] = s; shared.userRt[(size_t) (index - 1)] = rt (s); }
    }
    bool isActive() const noexcept override { return SynthBase::isActive() || tailOn; }

protected:
    const float* adjust (const float* p) noexcept override
    {
        using namespace gd2;
        constexpr int b = (int) lfoBase, st = (int) lfoStride, r = (int) lRate, sy = (int) lSync;
        return syncRates (p, numSlots, { { b + r, b + sy }, { b + st + r, b + st + sy }, { b + 2 * st + r, b + 2 * st + sy } });
    }

    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[gd2::voices], 1, 8);
        switch ((int) p[gd2::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (v == 1, true, v); break;
        }
    }

    void blockStart (const float* common, int) noexcept override { shared.baseLevelDb = common[gd2::level]; }

    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        using namespace gd2;
        // après la 1.1 : le volume de sortie glisse sur le bloc (plus de saut quand on tourne le bouton)
        const auto outTarget = (float) dbToGain (p[level] + 6.0f);
        const auto out0 = outPrev < 0.0f ? outTarget : outPrev;
        outPrev = outTarget;
        const auto outStep = (outTarget - out0) / (float) std::max (1, n);
        bool anyOn = false;
        for (int f = 0; f < numFx; ++f) anyOn = anyOn || p[fxOn[f]] > 0.5f;
        if (SynthBase::isActive()) { tailOn = anyOn; quiet = 0; }
        if (! anyOn)
        {
            tailOn = false;
            for (int i = 0; i < n; ++i) { const auto g = out0 + outStep * (float) i; L[i] *= g; R[i] *= g; }
            return;
        }
        std::array<std::pair<int, int>, numFx> order {};
        for (int f = 0; f < numFx; ++f) order[(size_t) f] = { (int) p[fxOn[f] + 1], f };
        std::sort (order.begin(), order.end());
        for (const auto& [rank, f] : order)
        {
            (void) rank;
            if (p[fxOn[f]] <= 0.5f) continue;
            switch (f)
            {
                case 0:
                    hyper.set (ix.hyMode, 2.0f); hyper.set (ix.hyRate, p[hyRate]); hyper.set (ix.hyDepth, p[hyDepth]); hyper.set (ix.hyMix, p[hyMix]); hyper.set (ix.hyWidth, 100.0f);
                    hyper.process (L, R, n, bpm);
                    if (p[hyDim] > 0.0f) { dim.set (ix.dimMix, p[hyDim]); dim.set (ix.dimMode, 2.0f); dim.process (L, R, n, bpm); }
                    break;
                case 1: dist.process (L, R, n, (int) p[dsType], p[dsDrive], (int) p[dsPre], p[dsFreq], p[dsMix] / 100.0); break;
                case 2:
                    flanger.set (ix.flRate, p[flRate]); flanger.set (ix.flDepth, p[flDepth]); flanger.set (ix.flFeed, p[flFeed]); flanger.set (ix.flMix, p[flMix]);
                    flanger.process (L, R, n, bpm); break;
                case 3:
                    phaser.set (ix.phRate, p[phRate]); phaser.set (ix.phDepth, p[phDepth]); phaser.set (ix.phCenter, p[phFreq]); phaser.set (ix.phFeed, p[phFeed]); phaser.set (ix.phMix, p[phMix]);
                    phaser.process (L, R, n, bpm); break;
                case 4:
                    chorus.set (ix.chRate, p[chRate]); chorus.set (ix.chDepth, p[chDepth]); chorus.set (ix.chDelay, p[chDelay]); chorus.set (ix.chFeed, p[chFeed]); chorus.set (ix.chMix, p[chMix]);
                    chorus.process (L, R, n, bpm); break;
                case 5:
                {
                    static constexpr float modes[3] { 0.0f, 2.0f, 3.0f };
                    delay.set (ix.dlMode, modes[std::clamp ((int) p[dlMode], 0, 2)]); delay.set (ix.dlSync, p[dlSync]); delay.set (ix.dlTime, p[dlTime]);
                    delay.set (ix.dlDiv, p[dlDiv]); delay.set (ix.dlFeed, p[dlFeed]); delay.set (ix.dlMix, p[dlMix]); delay.set (ix.dlLow, p[dlLow]);
                    delay.set (ix.dlHigh, p[dlHigh]); delay.set (ix.dlSat, 5.0f);
                    delay.process (L, R, n, bpm); break;
                }
                case 6:
                {
                    const double bands[3] { p[cpLow], p[cpMid], p[cpHigh] };
                    comp.process (L, R, n, p[cpMode] > 0.5f, p[cpDepth] / 100.0, p[cpTime] / 100.0, p[cpUp] / 100.0, p[cpDown] / 100.0,
                                  p[cpIn], p[cpOut], bands, p[cpThr], p[cpRatio]);
                    break;
                }
                case 7:
                    filterFx.set (ix.fiType, p[fiType]); filterFx.set (ix.fiCut, p[fiCut]); filterFx.set (ix.fiRes, p[fiRes]); filterFx.set (ix.fiDrive, p[fiDrive]);
                    filterFx.set (ix.fiMix, p[fiMix]); filterFx.set (ix.fiLfo, 0.0f); filterFx.set (ix.fiEnv, 0.0f);
                    filterFx.process (L, R, n, bpm); break;
                case 8: eq.process (L, R, n, p[eqLF], p[eqLG], p[eqMF], p[eqMG], p[eqMQ], p[eqHF], p[eqHG]); break;
                default:
                    reverb.set (ix.rvSize, p[rvSize]); reverb.set (ix.rvPre, p[rvPre]); reverb.set (ix.rvBright, p[rvBright]); reverb.set (ix.rvLow, p[rvLow]);
                    reverb.set (ix.rvWidth, p[rvWidth]); reverb.set (ix.rvMix, p[rvMix]);
                    reverb.process (L, R, n, bpm); break;
            }
        }
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (L[i])) L[i] = 0.0f;
            if (! std::isfinite (R[i])) R[i] = 0.0f;
            peak = std::max (peak, std::max (std::abs (L[i]), std::abs (R[i])));
            const auto g = out0 + outStep * (float) i;
            L[i] *= g; R[i] *= g;
        }
        // les traînes (délai, réverbération…) jouent jusqu'à se taire (une demi-seconde sous -90 dB)
        if (! SynthBase::isActive() && tailOn)
        {
            quiet = peak > 3.0e-5f ? 0 : quiet + n;
            if (quiet > (int) (sr * 0.5)) tailOn = false;
        }
    }

private:
    float outPrev = -1.0f; // volume de sortie du bloc précédent (-1 : pas encore joué)
    gd2::Shared shared;
    gd2::MappedFx hyper, dim, flanger, phaser, chorus, delay, filterFx, reverb;
    gd2::Gd2Dist dist;
    gd2::Gd2Comp comp;
    gd2::Gd2Eq eq;
    struct
    {
        int hyRate, hyDepth, hyMix, hyMode, hyWidth, dimMix, dimMode, flRate, flDepth, flFeed, flMix, phRate, phDepth, phCenter, phFeed, phMix,
            chRate, chDepth, chDelay, chFeed, chMix, dlMode, dlSync, dlTime, dlDiv, dlFeed, dlMix, dlLow, dlHigh, dlSat,
            fiType, fiCut, fiRes, fiDrive, fiMix, fiLfo, fiEnv, rvSize, rvPre, rvBright, rvLow, rvWidth, rvMix;
    } ix {};
    bool tailOn = false;
    int quiet = 0;
};

} // namespace grv::instruments
