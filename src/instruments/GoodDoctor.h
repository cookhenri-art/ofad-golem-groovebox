/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « Good Doctor » — principe des grands synthés à tables d'ondes hybrides (architecture reprise,
    ni nom, ni panneau, ni table, ni son d'origine : tout est calculé par le programme) :
      - trois oscillateurs à tables d'ondes (12 tables de 12 ondes, position continue), chacun avec
        DEUX déformations à la suite (« double warp ») : synchro, courbure ±, pulsation, miroir,
        asymétrie (distorsion de phase), quantification, FM / AM / anneau par l'oscillateur suivant,
        FM par le sous-oscillateur, repli, saturation, redressement, bits, passe-bas et passe-haut ;
        octave, demi-tons, fin, niveau, phase de départ et part de hasard (attaque toujours pareille
        pour les basses, ou libre pour les nappes), sortie vers les filtres, un seul, ou en direct ;
      - un sous-oscillateur (sinus, triangle, scie, carré, une ou deux octaves sous la note) et un
        oscillateur de bruit coloré, chacun avec sa sortie (filtres, filtre 1, filtre 2, direct) ;
      - deux filtres (14 types : multi en fondu, échelle 24 et 12 dB bas et haut, bande, réjection,
        « agressifs », doux, peigne + et −, formants en voyelles, diode) en série ou en parallèle,
        avec gain, forme, mélange, suivi du clavier, enveloppe 2 et vélocité ;
      - une distorsion par voix (douce, dure, lampe, repli, redresseur, bits, échantillonnage) ;
      - enveloppes 1 (ampli), 2 (filtres) et 3 (hauteur), deux LFO, plus deux enveloppes et trois LFO à
        pas de la modulation commune, matrice de 32 routages, 8 macros, arpège à pas ;
      - jeu poly, mono ou unisson (jusqu'à 8 voix empilées, désaccord, largeur), glisse.

    Choix de modèle (latence nulle, rien d'alloué dans le rendu) : tables limitées en bande par octave
    (WaveBank, les 8 tables de Glutamake partagées + 4 tables propres aux basses), déformations de
    phase avec pas de lecture corrigé (table moins riche quand la phase accélère), filtres TPT existants
    (échelle, état, Sallen-Key, diode), peigne sur une ligne fixe de 4096 échantillons par filtre.
    Volontairement écarté : oscillateurs à échantillons, granulaires et spectraux (le Groovebox a ses
    échantillonneurs), rack d'effets interne (la piste a ses effets : seule une distorsion par voix
    reste, utile aux basses), tables dessinées à la main.
*/

#pragma once

#include "Glutamake.h"

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace gdoc
{
    constexpr int numOsc = 3, ownTables = 4, sharedTables = gluta::numTables, numTables = sharedTables + ownTables;
    constexpr int frames = gluta::framesPerTable, combSize = 4096;

    enum OscField { ofOn = 0, ofTab, ofPos, ofW1, ofW1Amt, ofW2, ofW2Amt, ofOct, ofSemi, ofFine, ofLvl, ofPhase, ofRand, ofDest, oscStride };
    enum FiltField { ffOn = 0, ffType, ffCut, ffRes, ffDrive, ffShape, ffMix, ffKey, ffEnv, ffVel, fStride };

    enum Slot
    {
        voices = 0, glide, level, velAmp, tune, spreadAmt, vMode, uniVoices, uniDetune,
        oscBase,
        subLvl = oscBase + numOsc * oscStride, subShape, subOct, subDest, noiseLvl, noiseColor, noiseDest,
        route,
        fBase,
        aA = fBase + 2 * fStride, aD, aS, aR, e2A, e2D, e2S, e2R, e3A, e3D, e3S, e3R, pAmt,
        l1Rate, l1Shape, l1Sync, l1Delay, l2Rate, l2Shape, l2Sync, l2Delay,
        distType, distDrive, distMix,
        numSlots
    };

    enum Warp { wNone = 0, wSync, wBendUp, wBendDown, wPulse, wMirror, wAsym, wQuant, wFm, wFmSub,
                wAm, wRing, wFold, wSat, wRect, wBits, wLow, wHigh, numWarps };
    inline bool phaseWarp (int w) noexcept { return w >= wSync && w <= wFmSub; }

    enum Dest { toFilters = 0, toF1, toF2, toDirect };

    inline const std::vector<std::string>& tableNames()
    {
        static const std::vector<std::string> n = []
        {
            auto v = gluta::tableNames();
            for (const char* s : { "Scie qui gronde", "Synchro balay\xC3\xA9" "e", "Carr\xC3\xA9 qui se pince", "Sinus sali" }) v.push_back (s);
            return v;
        }();
        return n;
    }
    inline const std::vector<std::string>& warpNames()
    {
        static const std::vector<std::string> n { "Aucune", "Synchro", "Courbure +", "Courbure \xE2\x88\x92", "Pulsation", "Miroir", "Asym\xC3\xA9trie (phase)",
                                                  "Quantifier", "FM (osc. suivant)", "FM (sous-osc.)", "AM (osc. suivant)", "Anneau (osc. suivant)",
                                                  "Repli", "Saturer", "Redresser", "Bits", "Passe-bas", "Passe-haut" };
        return n;
    }
    inline const std::vector<std::string>& filterNames()
    {
        static const std::vector<std::string> n { "Multi (fondu)", "Passe-bas 24 dB", "Passe-bas 12 dB", "Passe-haut 24 dB", "Passe-haut 12 dB",
                                                  "Passe-bande", "Coupe-bande", "Agressif passe-bas", "Agressif passe-haut", "Doux passe-bas",
                                                  "Peigne +", "Peigne \xE2\x88\x92", "Formants (voyelles)", "Diode passe-bas" };
        return n;
    }
    inline const std::vector<std::string>& destNames()
    {
        static const std::vector<std::string> n { "Filtres", "Filtre 1", "Filtre 2", "Direct" };
        return n;
    }
    inline const std::vector<std::string>& distNames()
    {
        static const std::vector<std::string> n { "Arr\xC3\xAAt", "Douce", "Dure", "Lampe", "Repli", "Redresseur", "Bits", "\xC3\x89" "chantillonnage" };
        return n;
    }

    /** Les 4 tables propres (12 ondes chacune), pensées pour les basses. Onde (t, k) : t * frames + k. */
    inline const WaveBank& ownBank()
    {
        static const WaveBank b = []
        {
            using cd = std::complex<double>;
            auto sine = [] (double a) { return cd (0.0, -0.5 * a); };
            auto s2p = [] (double x) { return std::sin (kTwoPi * x); };
            WaveBank w;
            constexpr int F = frames;
            // 0 scie qui gronde : une scie dont une bosse de formant monte dans les harmoniques
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1);
                const auto c = 2.0 + t * 30.0, wd = 1.0 + t * 3.0;
                w.addWave ([=] (int h) { return sine ((1.0 / h) * (0.35 + 2.2 * std::exp (-0.5 * std::pow ((h - c) / wd, 2)))); });
            }
            // 1 synchro balayée : une scie remise à zéro par une maîtresse, rapport 1 → 8
            for (int k = 0; k < F; ++k)
            {
                const auto r = 1.0 + 7.0 * (double) k / (F - 1);
                w.addWaveTime ([=] (double x) { const auto y = x * r; return 2.0 * (y - std::floor (y)) - 1.0; });
            }
            // 2 carré qui se pince : largeur d'impulsion de 50 % à 4 %
            for (int k = 0; k < F; ++k)
            {
                const auto pw = 0.5 - 0.46 * (double) k / (F - 1);
                w.addWave ([=] (int h) { return sine (2.0 * std::sin (kPi * h * pw) / (kPi * h)); });
            }
            // 3 sinus sali : sinus poussé dans une saturation de plus en plus forte et asymétrique
            for (int k = 0; k < F; ++k)
            {
                const auto t = (double) k / (F - 1);
                const auto g = 1.0 + t * 9.0, as = t * 0.45;
                w.addWaveTime ([=] (double x) { return std::tanh (g * (s2p (x) + as)) - std::tanh (g * as); });
            }
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
            m.type = "gooddoc";
            m.name = "Good Doctor";
            m.family = "Polys";
            m.principle = "Trois oscillateurs \xC3\xA0 tables d'ondes \xC3\xA0 double d\xC3\xA9" "formation, deux filtres en s\xC3\xA9rie ou parall\xC3\xA8le, distorsion, matrice";
            m.params = {
                I ("voices", "Voix", "", 1, 8, 8),
                P ("glide", "Glisse", "ms", 0, 2000, 0, 0.4f),
                P ("level", "Volume", "dB", -30, 6, -5.5f),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 30),
                P ("tune", "Accord", "dt", -24, 24, 0),
                P ("spread", "Largeur", "%", 0, 100, 50),
                C ("vmode", "Jeu", { "Poly", "Mono", "Unisson" }),
                I ("univoices", "Voix d'unisson", "", 2, 8, 5),
                P ("unidetune", "D\xC3\xA9" "saccord", "ct", 0, 100, 18),
            };
            const char* ok[numOsc] { "a", "b", "c" };
            for (int o = 0; o < numOsc; ++o)
            {
                const std::string k = ok[o];
                const std::string n = std::string (" ") + (char) ('A' + o);
                m.params.push_back (C (k + "_on", "Oscillateur" + n, { "Arr\xC3\xAAt", "Marche" }, o == 0 ? 1 : 0));
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
                m.params.push_back (P (k + "_phase", "Phase" + n, "\xC2\xB0", 0, 360, 0));
                m.params.push_back (P (k + "_rand", "Phase au hasard" + n, "%", 0, 100, 100));
                m.params.push_back (C (k + "_dest", "Sortie" + n, destNames(), 0));
            }
            m.params.push_back (P ("sub", "Sous-oscillateur", "%", 0, 100, 0));
            m.params.push_back (C ("subshape", "Forme du sous-osc.", { "Sinus", "Triangle", "Scie", "Carr\xC3\xA9" }));
            m.params.push_back (I ("suboct", "Octave du sous-osc.", "", -2, 0, -1));
            m.params.push_back (C ("subdest", "Sortie du sous-osc.", destNames(), 3));
            m.params.push_back (P ("noise", "Bruit", "%", 0, 100, 0));
            m.params.push_back (P ("ncolor", "Couleur du bruit", "%", 0, 100, 70));
            m.params.push_back (C ("ndest", "Sortie du bruit", destNames(), 0));
            m.params.push_back (C ("route", "Filtres", { "En s\xC3\xA9rie", "En parall\xC3\xA8le" }));
            for (int f = 1; f <= 2; ++f)
            {
                const auto k = "f" + std::to_string (f);
                const auto n = " " + std::to_string (f);
                m.params.push_back (C (k + "on", "Filtre" + n, { "Arr\xC3\xAAt", "Marche" }, f == 1 ? 1 : 0));
                m.params.push_back (C (k + "type", "Type" + n, filterNames(), f == 1 ? 1 : 3));
                m.params.push_back (P (k + "cut", "Coupure" + n, "Hz", 20, 18000, f == 1 ? 3000 : 120, 0.3f));
                m.params.push_back (P (k + "res", "R\xC3\xA9sonance" + n, "%", 0, 100, 20));
                m.params.push_back (P (k + "drive", "Gain" + n, "dB", 0, 24, 0));
                m.params.push_back (P (k + "shape", "Forme" + n, "%", 0, 100, 0));
                m.params.push_back (P (k + "mix", "M\xC3\xA9lange" + n, "%", 0, 100, 100));
                m.params.push_back (P (k + "key", "Clavier" + n, "%", 0, 100, f == 1 ? 50 : 0));
                m.params.push_back (P (k + "env", "Env. 2" + n, "%", -100, 100, f == 1 ? 30 : 0));
                m.params.push_back (P (k + "vel", "V\xC3\xA9locit\xC3\xA9" + n, "%", 0, 100, 0));
            }
            auto adsr = [&] (const std::string& k, float a, float dd, float s, float r)
            {
                m.params.push_back (P (k + "a", "Attaque", "ms", 0.5f, 10000, a, 0.3f));
                m.params.push_back (P (k + "d", "D\xC3\xA9" "clin", "ms", 5, 10000, dd, 0.3f));
                m.params.push_back (P (k + "s", "Maintien", "%", 0, 100, s));
                m.params.push_back (P (k + "r", "Rel\xC3\xA2" "chement", "ms", 5, 10000, r, 0.3f));
            };
            adsr ("a", 1, 400, 85, 250);
            adsr ("e2", 1, 500, 20, 400);
            adsr ("e3", 0.5f, 90, 0, 80);
            m.params.push_back (P ("pamt", "Hauteur (env. 3)", "dt", -48, 48, 0));
            for (int l = 1; l <= 2; ++l)
            {
                const auto k = "l" + std::to_string (l);
                m.params.push_back (P (k + "rate", "LFO " + std::to_string (l) + " vitesse", "Hz", 0.02f, 30, l == 1 ? 0.3f : 5.0f, 0.4f));
                m.params.push_back (C (k + "shape", "LFO " + std::to_string (l) + " forme", lfoShapes, l == 1 ? 1 : 0));
                m.params.push_back (C (k + "sync", "LFO " + std::to_string (l) + " synchro", lfoSyncChoices()));
                m.params.push_back (C (k + "delay", "LFO " + std::to_string (l) + " retard", lfoDelayChoices()));
            }
            m.params.push_back (C ("dtype", "Distorsion", distNames()));
            m.params.push_back (P ("ddrive", "Pouss\xC3\xA9" "e", "dB", 0, 36, 12));
            m.params.push_back (P ("dmix", "M\xC3\xA9lange disto", "%", 0, 100, 100));

            m.sections = {
                { "global", "Voix", "global", { "vmode", "voices", "univoices", "unidetune", "glide", "tune", "spread", "velamp", "level" }, "global" },
            };
            for (int o = 0; o < numOsc; ++o)
            {
                const std::string k = ok[o];
                m.sections.push_back ({ "osc" + k, std::string ("Oscillateur ") + (char) ('A' + o), "wave",
                                        { k + "_on", k + "_tab", k + "_pos", k + "_w1", k + "_w1amt", k + "_w2", k + "_w2amt", k + "_oct", k + "_semi",
                                          k + "_fine", k + "_lvl", k + "_phase", k + "_rand", k + "_dest" }, "osc" });
            }
            m.sections.push_back ({ "subnoise", "Sous-oscillateur et bruit", "osc", { "sub", "subshape", "suboct", "subdest", "noise", "ncolor", "ndest" }, "osc" });
            for (int f = 1; f <= 2; ++f)
            {
                const auto k = "f" + std::to_string (f);
                m.sections.push_back ({ "filter" + std::to_string (f), "Filtre " + std::to_string (f), "filter",
                                        { k + "on", k + "type", k + "cut", k + "res", k + "drive", k + "shape", k + "mix", k + "key", k + "env", k + "vel", "route" }, "filter" });
            }
            m.sections.push_back ({ "ampenv", "Enveloppe 1 (ampli)", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" });
            m.sections.push_back ({ "env2", "Enveloppe 2 (filtres)", "env", { "e2a", "e2d", "e2s", "e2r", "f1env", "f2env" }, "env" });
            m.sections.push_back ({ "env3", "Enveloppe 3 (hauteur)", "env", { "e3a", "e3d", "e3s", "e3r", "pamt" }, "env" });
            m.sections.push_back ({ "lfo1", "LFO 1", "lfo", { "l1sync", "l1rate", "l1shape", "l1delay" }, "mod" });
            m.sections.push_back ({ "lfo2", "LFO 2", "lfo", { "l2sync", "l2rate", "l2shape", "l2delay" }, "mod" });
            m.sections.push_back ({ "dist", "Distorsion", "global", { "dtype", "ddrive", "dmix" }, "fx" });

            m.presets.push_back ({ "Init", {}, "Init" });

            HubSpec hub;
            hub.extEnvs = { "Env. 1 (ampli)", "Env. 2", "Env. 3" };
            hub.extLfos = { "LFO 1", "LFO 2" };
            hub.envs = 2; hub.firstEnv = 4; hub.envName = "Env.";
            hub.lfos = 3; hub.firstLfo = 3;
            hub.slots = 32; hub.macros = 8; hub.arp = true; hub.arpSteps = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m);

            // formes montrées par l'interface : 12 tables × 12 ondes, 64 points chacune
            const auto& shared = gluta::bank();
            const auto& own = ownBank();
            for (int t = 0; t < numTables; ++t)
                for (int k = 0; k < frames; ++k)
                {
                    std::vector<float> shape (64);
                    for (int i = 0; i < 64; ++i)
                        shape[(size_t) i] = t < sharedTables ? shared.read (t * frames + k, 4, (double) i / 64.0)
                                                             : own.read ((t - sharedTables) * frames + k, 4, (double) i / 64.0);
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

/** Un filtre de la voix : les types d'échelle, d'état, Sallen-Key, diode, peigne et formants. */
class GoodDocFilter
{
public:
    void prepare (double s) noexcept { sr = s; diode.prepare (s); reset(); }
    void reset() noexcept
    {
        ladder.reset(); svf.reset(); svf2.reset(); sk.reset(); diode.reset(); comb.fill (0.0f); w = 0; combLp = 0.0;
    }

    /** type 0..13, fc Hz, res 0..1, drive >= 1, shape 0..1. */
    double process (double x, int type, double fc, double res, double drive, double shape) noexcept
    {
        using gdoc::combSize;
        fc = std::clamp (fc, 16.0, 0.45 * sr);
        switch (type)
        {
            case 0: return softClip (svf.processMorph (x * drive, fc, res, sr, shape)) / std::sqrt (drive);
            case 1: return ladder.process (x, fc, res, sr, Ladder::lp24, drive) / std::sqrt (drive);
            case 2: return ladder.process (x, fc, res, sr, Ladder::lp12, drive) / std::sqrt (drive);
            case 3: return ladder.process (x, fc, res, sr, Ladder::hp24, drive) / std::sqrt (drive);
            case 4: return ladder.process (x, fc, res, sr, Ladder::hp12, drive) / std::sqrt (drive);
            case 5: return ladder.process (x, fc, res, sr, Ladder::bp12, drive) / std::sqrt (drive);
            case 6: { const auto o = svf.process (x, fc, res * 0.9, sr); return softClip ((o.low + o.high) * drive) / std::sqrt (drive); }
            case 7: return sk.process (x * drive, false, fc, res * 1.95, 1.0 + 1.5 * res, sr) / std::sqrt (drive);
            case 8: return sk.process (x * drive, true, fc, res * 1.95, 1.0 + 1.5 * res, sr) / std::sqrt (drive);
            case 9: return svf.process (softClip (x * drive), fc, res * 0.55, sr).low;
            case 10: case 11:
            {
                // peigne : retard d'une période de la coupure, réinjection ± (la forme amortit les aigus)
                const auto delay = std::clamp (sr / std::max (fc, sr / (combSize - 4)), 2.0, (double) combSize - 3.0);
                auto rp = (double) w - delay;
                if (rp < 0.0) rp += combSize;
                const auto i0 = (int) rp;
                const auto fr = rp - (double) i0;
                const auto a = (double) comb[(size_t) (i0 & (combSize - 1))], b = (double) comb[(size_t) ((i0 + 1) & (combSize - 1))];
                auto d = a + (b - a) * fr;
                combLp += (d - combLp) * (1.0 - 0.85 * shape);
                d = combLp;
                const auto g = (type == 10 ? 1.0 : -1.0) * std::min (0.97, res * 0.97);
                const auto y = softClip (x * drive + g * d);
                comb[(size_t) w] = (float) y;
                w = (w + 1) & (combSize - 1);
                return y / std::sqrt (drive);
            }
            case 12:
            {
                // formants : deux bandes aux voyelles A E I O U (forme), décalées par la coupure (1 kHz = voix)
                static constexpr double F1[5] { 730, 530, 300, 570, 330 }, F2[5] { 1090, 1840, 2290, 840, 1020 };
                const auto t = std::clamp (shape, 0.0, 1.0) * 4.0;
                const auto i0 = std::min (3, (int) t);
                const auto fr = t - i0;
                const auto sc = fc / 1000.0;
                const auto f1 = std::clamp ((F1[i0] + (F1[i0 + 1] - F1[i0]) * fr) * sc, 30.0, 0.45 * sr);
                const auto f2 = std::clamp ((F2[i0] + (F2[i0 + 1] - F2[i0]) * fr) * sc, 30.0, 0.45 * sr);
                const auto q = 0.55 + 0.4 * res;
                const auto in = softClip (x * drive) / std::sqrt (drive);
                return 1.6 * (svf.process (in, f1, q, sr).band + 0.7 * svf2.process (in, f2, q, sr).band);
            }
            default: return diode.process (x * drive, fc, res) / std::sqrt (drive);
        }
    }

private:
    double sr = 48000.0, combLp = 0.0;
    Ladder ladder;
    StateFilter svf, svf2;
    SallenKey12 sk;
    DiodeLadder diode;
    std::array<float, gdoc::combSize> comb {};
    int w = 0;
};

/** Une voix : trois oscillateurs à tables, sous-oscillateur, bruit, deux filtres, distorsion,
    trois enveloppes, deux LFO. */
class GoodDocVoice
{
public:
    void prepare (double s)
    {
        sr = s; ea.prepare (s); e2.prepare (s); e3.prepare (s); filt[0].prepare (s); filt[1].prepare (s); reset();
    }
    void reset() noexcept
    {
        ph.fill (0.0); last.fill (0.0); wstate.fill (0.0); sub.reset(); filt[0].reset(); filt[1].reset();
        ea.reset(); e2.reset(); e3.reset(); dc.reset(); nlp.reset(); held = 0.0; holdCount = 0.0; active = false;
    }
    // unisson : les voix désaccordées s'additionnent en puissance (1/√n) ; le sous-oscillateur et le bruit,
    // identiques dans chaque voix, s'additionnent en amplitude : ramenés à une seule voix (1/√n de plus)
    void setSpread (int index, int count) noexcept { spread = count > 1 ? (2.0 * index / (count - 1) - 1.0) : 0.0; uniGain = 1.0 / std::sqrt ((double) std::max (1, count)); }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; active = true; releaseAfterStart = false;
        if (! legatoStart) lfoAge = 0.0;
        pan = rand01() * 2.0 - 1.0;
    }
    // v0.47 (maj. 5 nov.) : relâchée avant d'avoir joué (même bloc) : la note démarre puis se relâche aussitôt
    void release() noexcept { ea.gate (false); e2.gate (false); e3.gate (false); if (pending) releaseAfterStart = true; }
    bool isActive() const noexcept { return active; }

    void modSources (double* env, double* lfo) const noexcept
    {
        env[0] = lastA; env[1] = lastE2; env[2] = lastE3;
        lfo[0] = lastL1; lfo[1] = lastL2;
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace gdoc;
        const auto& shared = gluta::bank();
        const auto& own = ownBank();
        if (pending)
        {
            pending = false;
            pitch.jump ((double) (p[glide] > 0.5f && from >= 0 ? from : target));
            pitch.to ((double) target);
            if (! legatoPending || ! ea.isActive())
            {
                ea.gate (true); e2.gate (true); e3.gate (true);
                // phase de départ : fixe (attaque toujours pareille) ou en partie au hasard
                for (int o = 0; o < numOsc; ++o)
                {
                    const auto b = oscBase + o * oscStride;
                    const auto r = std::clamp (p[b + ofRand] / 100.0, 0.0, 1.0);
                    const auto x = p[b + ofPhase] / 360.0 + r * rand01();
                    ph[(size_t) o] = x - std::floor (x);
                }
                sub.reset (0.0);
            }
            if (releaseAfterStart) { releaseAfterStart = false; ea.gate (false); e2.gate (false); e3.gate (false); }
        }
        ea.set (p[aA], p[aD], p[aS] / 100.0, p[aR]);
        e2.set (p[e2A], p[e2D], p[e2S] / 100.0, p[e2R]);
        e3.set (p[e3A], p[e3D], p[e3S] / 100.0, p[e3R]);
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-1.0 / (p[glide] * 0.001 * sr * 0.35));
        const auto gain = dbToGain (p[level] + 6.0f) * velocityGain (vel, p[velAmp] / 100.0) * uniGain;
        const auto pn = spread != 0.0 ? spread * std::max (0.6, p[spreadAmt] / 100.0) : pan * p[spreadAmt] / 100.0 * 0.5;
        const auto gl = 1.0 - std::max (0.0, pn) * 0.6, gr = 1.0 + std::min (0.0, pn) * 0.6;
        const auto det = spread * p[uniDetune] / 100.0;

        // oscillateurs : réglages lus une fois par bloc
        struct O { bool on; int tab, w1, w2, dest; double pos, a1, a2, off, lvl; };
        std::array<O, numOsc> os {};
        for (int o = 0; o < numOsc; ++o)
        {
            const auto b = oscBase + o * oscStride;
            auto& x = os[(size_t) o];
            x.tab = std::clamp ((int) p[b + ofTab], 0, numTables - 1);
            x.w1 = std::clamp ((int) p[b + ofW1], 0, numWarps - 1);
            x.w2 = std::clamp ((int) p[b + ofW2], 0, numWarps - 1);
            x.dest = std::clamp ((int) p[b + ofDest], 0, 3);
            x.pos = std::clamp (p[b + ofPos] / 100.0, 0.0, 1.0);
            x.a1 = std::clamp (p[b + ofW1Amt] / 100.0, 0.0, 1.0);
            x.a2 = std::clamp (p[b + ofW2Amt] / 100.0, 0.0, 1.0);
            x.off = 12.0 * std::round (p[b + ofOct]) + std::round (p[b + ofSemi]) + p[b + ofFine] / 100.0 + (o == 1 ? -det : det) * (o == 2 ? 0.7 : 1.0);
            x.lvl = p[b + ofLvl] / 100.0;
            x.on = p[b + ofOn] > 0.5f && x.lvl > 0.0;
        }
        // un oscillateur éteint qui module un autre (FM, AM, anneau par « l'osc. suivant ») est calculé quand même
        auto modsNext = [] (const O& x) { auto m = [] (int w, double a) { return a > 0.0 && (w == wFm || w == wAm || w == wRing); }; return m (x.w1, x.a1) || m (x.w2, x.a2); };
        std::array<bool, numOsc> need {};
        for (int o = 0; o < numOsc; ++o) need[(size_t) o] = os[(size_t) o].on;
        for (int o = 0; o < numOsc; ++o) if (os[(size_t) o].on && modsNext (os[(size_t) o])) need[(size_t) ((o + 1) % numOsc)] = true;
        const auto lvSub = p[subLvl] / 100.0 * uniGain, lvN = p[noiseLvl] / 100.0 * uniGain;
        const auto subWave = std::array<Osc::Wave, 4> { Osc::sine, Osc::triangle, Osc::saw, Osc::pulse }[(size_t) std::clamp ((int) p[subShape], 0, 3)];
        const auto subOff = 12.0 * std::round (p[subOct]);
        const auto subD = std::clamp ((int) p[subDest], 0, 3), nD = std::clamp ((int) p[noiseDest], 0, 3);
        const bool fmSub = std::any_of (os.begin(), os.end(), [] (const O& x) { return x.on && ((x.w1 == wFmSub && x.a1 > 0.0) || (x.w2 == wFmSub && x.a2 > 0.0)); });
        const auto nCut = 300.0 * std::pow (60.0, p[noiseColor] / 100.0);

        const bool parallel = p[route] > 0.5f;
        struct F { bool on; int type; double cut, res, drive, shape, mix, key, env, vel; };
        std::array<F, 2> fs {};
        for (int f = 0; f < 2; ++f)
        {
            const auto b = fBase + f * fStride;
            auto& x = fs[(size_t) f];
            x.on = p[b + ffOn] > 0.5f;
            x.type = std::clamp ((int) p[b + ffType], 0, 13);
            x.cut = p[b + ffCut]; x.res = std::clamp (p[b + ffRes] / 100.0, 0.0, 1.0); x.drive = dbToGain (p[b + ffDrive]);
            x.shape = std::clamp (p[b + ffShape] / 100.0, 0.0, 1.0); x.mix = std::clamp (p[b + ffMix] / 100.0, 0.0, 1.0);
            x.key = p[b + ffKey] / 100.0; x.env = p[b + ffEnv] / 100.0 * 7.0; x.vel = p[b + ffVel] / 100.0 * (vel - 0.6) * 4.0;
        }
        const auto dType = std::clamp ((int) p[distType], 0, 7);
        const auto dDrive = dbToGain (p[distDrive]), dMix = std::clamp (p[distMix] / 100.0, 0.0, 1.0);
        const auto pAmtSt = (double) p[pAmt];
        double a = 0.0, ev2 = 0.0, ev3 = 0.0, l1 = 0.0, l2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            a = ea.next(); ev2 = e2.next(); ev3 = e3.next();
            lfoAge += 1.0 / sr;
            l1 = lfo1.next ((int) p[l1Shape], p[l1Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l1Delay]));
            l2 = lfo2.next ((int) p[l2Shape], p[l2Rate], sr) * lfoDelayGain (lfoAge, lfoDelaySeconds ((int) p[l2Delay]));
            const auto note = pitch.next (glideCoef) + p[tune] + ev3 * pAmtSt;

            double subOut = 0.0;
            if (lvSub > 0.0 || fmSub) subOut = sub.next (subWave, std::min (0.45, noteHz (note + subOff) / sr), 0.5);

            std::array<double, 4> bus {};
            // C d'abord (il module B), puis B (module A), puis A (module C avec la valeur d'avant)
            for (int o = numOsc - 1; o >= 0; --o)
            {
                if (! need[(size_t) o]) { last[(size_t) o] = 0.0; continue; }
                const auto& x = os[(size_t) o];
                const auto other = last[(size_t) ((o + 1) % numOsc)];
                const auto y = osc (shared, own, x.tab, x.pos, x.w1, x.a1, x.w2, x.a2, ph[(size_t) o], noteHz (note + x.off) / sr, other, subOut, &wstate[(size_t) o * 2]);
                last[(size_t) o] = y;
                if (x.on) bus[(size_t) x.dest] += y * x.lvl;
            }
            if (lvSub > 0.0) bus[(size_t) subD] += subOut * lvSub;
            if (lvN > 0.0) bus[(size_t) nD] += nlp.lowpass (noise.next(), nCut, sr) * lvN * (1.0 + 0.6 * (1.0 - p[noiseColor] / 100.0));

            // filtres en série (1 puis 2) ou en parallèle
            auto run = [&] (int f, double in)
            {
                const auto& x = fs[(size_t) f];
                if (! x.on) return in;
                const auto fc = x.cut * fastExp2 (x.key * (note - 60.0) / 12.0 + x.env * ev2 + x.vel);
                const auto y = filt[(size_t) f].process (in, x.type, fc, x.res, x.drive, x.shape);
                return in + (y - in) * x.mix;
            };
            double y;
            if (! parallel) y = run (1, run (0, (bus[0] + bus[1]) * 0.35) + bus[2] * 0.35) + bus[3] * 0.35;
            else
            {
                const auto y1 = fs[0].on ? run (0, (bus[0] + bus[1]) * 0.35) : (bus[0] + bus[1]) * 0.35;
                const auto y2 = fs[1].on ? run (1, (bus[0] + bus[2]) * 0.35) : bus[2] * 0.35;
                y = (fs[0].on && fs[1].on ? 0.7 : 1.0) * (y1 + y2) + bus[3] * 0.35;
            }
            if (dType > 0) y = y + (distort (y, dType, dDrive) - y) * dMix;
            const auto out = dc.process (y) * a * gain;
            L[i] += (float) (out * gl);
            R[i] += (float) (out * gr);
        }
        lastA = a; lastE2 = ev2; lastE3 = ev3; lastL1 = l1; lastL2 = l2;
        if (! ea.isActive()) active = false;
    }

private:
    static double readTable (const WaveBank& shared, const WaveBank& own, int table, double pos, WaveBank::Mip m, double phs) noexcept
    {
        using namespace gdoc;
        const auto fp = pos * (frames - 1);
        const auto k0 = std::min ((int) fp, frames - 2);
        const auto f = fp - (double) k0;
        const auto& wb = table < sharedTables ? shared : own;
        const auto base = (table < sharedTables ? table : table - sharedTables) * frames + k0;
        const auto y0 = (double) wb.read (base, m, phs);
        return f < 1.0e-4 ? y0 : y0 + ((double) wb.read (base + 1, m, phs) - y0) * f;
    }

    /** Déformation de phase : rend la phase lue et multiplie la pente (choix de la table limitée en bande). */
    static double warpPhase (int w, double a, double phs, double& slope, double& window, double other, double subv) noexcept
    {
        using namespace gdoc;
        if (a <= 0.0) return phs;
        auto wrap = [] (double x) { return x - std::floor (x); };
        switch (w)
        {
            case wSync: { const auto r = 1.0 + 7.0 * a; slope *= r; window *= 1.0 - 0.5 * a * phs; return wrap (phs * r); }
            case wBendUp: { const auto e = 1.0 + 3.0 * a; slope *= e; return std::pow (phs, e); }
            case wBendDown: { const auto e = 1.0 + 3.0 * a; slope *= e; return 1.0 - std::pow (1.0 - phs, e); }
            case wPulse: { const auto pw = 1.0 - 0.9 * a; slope /= pw; return phs < pw ? phs / pw : 0.0; }
            case wMirror: { const auto mir = phs < 0.5 ? phs : 1.0 - phs; slope *= 1.0 + a; return phs + a * (mir - phs); }
            case wAsym:
            {
                // distorsion de phase : la première moitié du cycle se lit plus vite
                const auto k = 0.5 - 0.46 * a;
                slope *= 0.5 / k;
                return phs < k ? 0.5 * phs / k : 0.5 + 0.5 * (phs - k) / (1.0 - k);
            }
            case wQuant: { const auto q = 64.0 - 60.0 * a; return std::floor (phs * q) / q; }
            case wFm: slope *= 1.0 + 3.0 * a; return wrap (phs + 0.6 * a * other);
            case wFmSub: slope *= 1.0 + 2.0 * a; return wrap (phs + 0.5 * a * subv);
            default: return phs;
        }
    }

    /** Déformation d'amplitude (après lecture). st : état du passe-bas / passe-haut de ce rang. */
    double warpAmp (int w, double a, double y, double other, double& st) const noexcept
    {
        using namespace gdoc;
        if (a <= 0.0) return y;
        switch (w)
        {
            case wAm: return y * ((1.0 - a) + a * (0.5 + 0.5 * other));
            case wRing: return y * (1.0 - a) + y * other * a;
            case wFold: { auto x = y * (1.0 + 4.0 * a); x = (x + 1.0) * 0.25; x -= std::floor (x); return std::abs (x * 4.0 - 2.0) - 1.0; }
            case wSat: return std::tanh (y * (1.0 + 6.0 * a)) / std::tanh (1.0 + 6.0 * a * 0.3);
            case wRect: { const auto r = 1.6 * std::abs (y) - 0.7; return y + (r - y) * a; }
            case wBits: { const auto q = 16.0 - 14.5 * a; return std::round (y * q) / q; }
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

    double osc (const WaveBank& shared, const WaveBank& own, int tab, double pos, int w1, double a1, int w2, double a2,
                double& phs, double dt, double other, double subv, double* st) const noexcept
    {
        dt = std::clamp (dt, 1.0e-7, 0.45);
        double slope = 1.0, window = 1.0;
        auto rp = warpPhase (w1, a1, phs, slope, window, other, subv);
        rp = warpPhase (w2, a2, rp, slope, window, other, subv);
        const auto mip = WaveBank::mipFor (std::min (0.45, dt * slope));
        auto y = readTable (shared, own, tab, pos, mip, rp) * window;
        if (! gdoc::phaseWarp (w1)) y = warpAmp (w1, a1, y, other, st[0]);
        if (! gdoc::phaseWarp (w2)) y = warpAmp (w2, a2, y, other, st[1]);
        phs += dt;
        phs -= std::floor (phs);
        return y;
    }

    double distort (double x, int type, double drive) noexcept
    {
        const auto in = x * drive;
        const auto comp = 1.0 / std::sqrt (drive);
        switch (type)
        {
            case 1: return std::tanh (in) * comp * 1.2;
            case 2: return std::clamp (in, -1.0, 1.0) * comp * 1.2;
            case 3: { const auto y = in >= 0.0 ? std::tanh (in) : std::tanh (0.6 * in) / 0.6 * 0.8; return (y - 0.1 * std::tanh (drive * 0.05)) * comp * 1.2; }
            case 4: { auto f = (in * 0.5 + 1.0) * 0.25; f -= std::floor (f); return (std::abs (f * 4.0 - 2.0) - 1.0) * 0.9; }
            case 5: return (std::abs (std::tanh (in)) * 2.0 - 0.8) * comp;
            case 6: { const auto q = std::max (2.0, 64.0 / drive); return std::round (std::tanh (in) * q) / q * comp * 1.2; }
            default:
            {
                // échantillonnage : la valeur est tenue (fréquence qui baisse avec la poussée)
                holdCount += 1.0;
                if (holdCount >= std::max (1.0, drive * 0.5)) { holdCount = 0.0; held = std::tanh (x * 1.5); }
                return held;
            }
        }
    }

    double rand01() noexcept { rng = rng * 1664525u + 1013904223u; return (double) (rng >> 8) / 16777216.0; }

    double sr = 48000.0, pan = 0.0, spread = 0.0, uniGain = 1.0;
    std::array<double, gdoc::numOsc> ph {}, last {};
    std::array<double, gdoc::numOsc * 2> wstate {};
    double lastA = 0.0, lastE2 = 0.0, lastE3 = 0.0, lastL1 = 0.0, lastL2 = 0.0;
    double held = 0.0, holdCount = 0.0;
    float vel = 1.0f;
    int target = 60, from = -1;
    bool pending = false, legatoPending = false, active = false, releaseAfterStart = false;
    std::uint32_t rng = 7;
    Osc sub;
    std::array<GoodDocFilter, 2> filt;
    Adsr ea, e2, e3;
    SimpleLfo lfo1, lfo2;
    double lfoAge = 1.0e9;
    Glide pitch;
    WhiteNoise noise { 0xD0C7u };
    OnePole nlp;
    DcBlocker dc;
};

class GoodDoctor final : public SynthBase<GoodDocVoice, 8>
{
public:
    GoodDoctor()
    {
        (void) gluta::bank(); (void) gdoc::ownBank(); // tables calculées à la création, jamais sur le fil audio
        for (int i = 0; i < 8; ++i) voices[(size_t) i].seed (0xD0C7u * (std::uint32_t) (i + 3));
    }
    const ModuleDesc& desc() const noexcept override { return gdoc::desc(); }

protected:
    const float* adjust (const float* p) noexcept override
    { return syncRates (p, gdoc::numSlots, { { gdoc::l1Rate, gdoc::l1Sync }, { gdoc::l2Rate, gdoc::l2Sync } }); }

    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[gdoc::voices], 1, 8);
        switch ((int) p[gdoc::vMode])
        {
            case 1: setMode (true, true, v); break;
            case 2: setMode (true, true, v, std::clamp ((int) p[gdoc::uniVoices], 2, 8)); break;
            default: setMode (v == 1, true, v); break;
        }
    }
};

} // namespace grv::instruments
