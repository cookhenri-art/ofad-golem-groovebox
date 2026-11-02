/*
    Groovebox — OFAD — AGPL-3.0-or-later

    « New Metal » — principe des synthés FM à 4 opérateurs « simplifiés » des boîtes à
    séquenceur récentes (architecture reprise ; ni nom, ni façade, ni son d'origine) :
      - 4 opérateurs groupés en C (toujours entendu), A (modulateur ou entendu) et B (B1 et B2,
        réglés ensemble) ; 8 algorithmes, chacun avec deux sorties X et Y que « Mélange » fond
        l'une dans l'autre ;
      - rapports de fréquence : C presque entier, A et B plus libres (+ décalage fin du rapport
        pour les sons inharmoniques), désaccord A / B1 ;
      - « Harmoniques » bipolaire : en négatif, C reçoit des partiels en plus ; en positif, A et B1
        (balayage continu d'une série à la suivante, comme une table d'ondes) ;
      - rétroaction sur le modulateur du haut, niveaux de modulation A et B avec chacun son
        enveloppe Attaque / Déclin / Fin (déclenchée) ou Attaque / tenue / Déclin / Fin (à la touche) ;
      - puis la partie soustractive : saturation, filtre multimode (ou passe-bas 24 dB), filtre
        « base / largeur » (passe-haut + passe-bas), enveloppes de filtre et d'ampli ADSR ;
      - 2 LFO, 1 enveloppe de modulation et 8 routages (modulation commune), arpège ; 16 voix,
        mono, legato, portamento.
    Modèle : FM par la phase (phase en cycles), ondes calculées (sinus + partiel ajouté, sans
    table d'origine), hauteur et coefficients des filtres tous les 8 échantillons ; pas
    d'anticipation, aucune latence ajoutée.
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

namespace fm4
{
    enum Slot
    {
        algo = 0, ratC, ratA, ratB1, ratB2, offA, offB, harm, detune, feedback, mix, levA, levB,
        aAtk, aDec, aEnd, aMode, aReset, bAtk, bDec, bEnd, bMode, bReset, velMod,
        drive, fType, fFreq, fRes, fMorph, fEnv, fKey, fVel, fA, fD, fS, fR,
        base, width,
        aa, ad, as, ar, velAmp, spread,
        voices, vMode, glide, tune, level,
        numSlots
    };

    constexpr int opC = 0, opA = 1, opB1 = 2, opB2 = 3;

    /** Algorithme : modulateurs de chaque opérateur (bits ; C = bit 0), sortie X (un opérateur),
        sortie Y (un ou deux opérateurs), opérateur à rétroaction. Les modulations vont toujours
        d'un opérateur de rang plus haut vers un plus bas (B2 → B1 → A → C) : un seul passage. */
    struct Algorithm { int mods[4]; int x; int y; int fb; };
    constexpr int C_ = 1, A_ = 2, B1_ = 4, B2_ = 8;
    inline const std::array<Algorithm, 8>& algorithms()
    {
        static const std::array<Algorithm, 8> a { {
            { { A_, 0, B2_, 0 },        opC, B1_,       opA },   // 1 : A→C · B2→B1 (deux paires)
            { { A_, B1_, B2_, 0 },      opC, A_,        opB2 },  // 2 : B2→B1→A→C (pile)
            { { A_ | B1_, 0, B2_, 0 },  opC, A_,        opB2 },  // 3 : A et B1 → C, B2→B1
            { { A_, B2_, B2_, 0 },      opC, B1_,       opB2 },  // 4 : B2→A et B1, A→C
            { { A_ | B1_, 0, B2_, 0 },  opC, B1_,       opA },   // 5 : A et B1 → C, B2→B1 (Y = B1)
            { { B2_, B2_, B2_, 0 },     opC, A_ | B1_,  opB2 },  // 6 : B2 → C, A, B1
            { { B1_, B2_, 0, 0 },       opC, A_,        opB2 },  // 7 : B1→C, B2→A
            { { 0, 0, B2_, 0 },         opC, A_ | B1_,  opB2 },  // 8 : C et A seuls, B2→B1
        } };
        return a;
    }

    /** Rapports : C (presque entiers), A et B (plus libres). */
    inline const std::array<double, 18>& ratiosC()
    {
        static const std::array<double, 18> r { 0.25, 0.5, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
        return r;
    }
    inline const std::array<double, 34>& ratiosAB()
    {
        static const std::array<double, 34> r = []
        {
            std::array<double, 34> x {};
            x[0] = 0.25; x[1] = 0.5; x[2] = 0.75;
            for (int i = 0; i < 31; ++i) x[(size_t) (3 + i)] = 1.0 + 0.5 * i;
            return x;
        }();
        return r;
    }
    inline std::vector<std::string> ratioNames (bool ab)
    {
        std::vector<std::string> v;
        auto fmt = [] (double r)
        {
            char b[16];
            std::snprintf (b, sizeof b, "%g", r);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        };
        if (ab) for (auto r : ratiosAB()) v.push_back (fmt (r));
        else for (auto r : ratiosC()) v.push_back (fmt (r));
        return v;
    }
    inline int ratioIndexC (double r) { for (size_t i = 0; i < ratiosC().size(); ++i) if (std::abs (ratiosC()[i] - r) < 1e-6) return (int) i; return 2; }
    inline int ratioIndexAB (double r) { for (size_t i = 0; i < ratiosAB().size(); ++i) if (std::abs (ratiosAB()[i] - r) < 1e-6) return (int) i; return 3; }

    /** Temps des enveloppes de modulation (0..127 → 0 à 10 s, courbe exponentielle). */
    inline double modEnvSeconds (double v) noexcept { return v <= 0.0 ? 0.0 : 0.001 * std::pow (10000.0, std::clamp (v, 0.0, 127.0) / 127.0); }

    inline const ModuleDesc& desc()
    {
        static const ModuleDesc d = []
        {
            const std::vector<std::string> onOff { "Non", "Oui" };
            const std::vector<std::string> envModes { "D\xC3\xA9" "clench\xC3\xA9" "e", "\xC3\x80 la touche" };
            ModuleDesc m;
            m.type = "fm4";
            m.name = "New Metal";
            m.family = "Synth\xC3\xA9s FM";
            m.principle = "FM \xC3\xA0 4 op\xC3\xA9rateurs (C, A, B1-B2), 8 algorithmes \xC3\xA0 deux sorties m\xC3\xA9lang\xC3\xA9" "es, harmoniques, puis filtres et enveloppes soustractifs";
            m.params = {
                I ("algo", "Algorithme", "", 1, 8, 1),
                C ("ratc", "Rapport C", ratioNames (false), 2),
                C ("rata", "Rapport A", ratioNames (true), 3),
                C ("ratb1", "Rapport B1", ratioNames (true), 5),
                C ("ratb2", "Rapport B2", ratioNames (true), 3),
                P ("offa", "D\xC3\xA9" "calage A", "", -0.5f, 0.5f, 0),
                P ("offb", "D\xC3\xA9" "calage B", "", -0.5f, 0.5f, 0),
                P ("harm", "Harmoniques", "", -26, 26, 0),
                I ("detune", "D\xC3\xA9" "saccord", "", 0, 127, 0),
                I ("feedback", "R\xC3\xA9troaction", "", 0, 127, 0),
                I ("mix", "M\xC3\xA9lange X / Y", "", -63, 63, 0),
                I ("leva", "Niveau A", "", 0, 127, 60),
                I ("levb", "Niveau B", "", 0, 127, 0),

                I ("aatk", "Attaque A", "", 0, 127, 0),
                I ("adec", "D\xC3\xA9" "clin A", "", 0, 127, 70),
                I ("aend", "Fin A", "", 0, 127, 0),
                C ("amode", "Enveloppe A", envModes),
                C ("areset", "Repart de z\xC3\xA9ro A", onOff, 1),
                I ("batk", "Attaque B", "", 0, 127, 0),
                I ("bdec", "D\xC3\xA9" "clin B", "", 0, 127, 70),
                I ("bend", "Fin B", "", 0, 127, 0),
                C ("bmode", "Enveloppe B", envModes),
                C ("breset", "Repart de z\xC3\xA9ro B", onOff, 1),
                P ("velmod", "V\xC3\xA9locit\xC3\xA9 \xE2\x86\x92 modulation", "%", 0, 100, 30),

                P ("drive", "Saturation", "%", 0, 100, 0),
                C ("ftype", "Type de filtre", { "Multimode", "Passe-bas 24 dB" }),
                P ("ffreq", "Coupure", "Hz", 20, 18000, 18000, 0.3f),
                P ("fres", "R\xC3\xA9sonance", "%", 0, 100, 0),
                P ("fmorph", "Passe-bas \xE2\x86\x92 passe-haut", "%", 0, 100, 0),
                P ("fenv", "Qt\xC3\xA9 env.", "%", -100, 100, 0),
                P ("fkey", "Clavier", "%", 0, 100, 0),
                P ("fvel", "V\xC3\xA9locit\xC3\xA9", "%", 0, 100, 0),
                P ("fa", "Attaque", "ms", 0.5f, 10000, 2, 0.3f),
                P ("fd", "D\xC3\xA9" "clin", "ms", 5, 10000, 400, 0.3f),
                P ("fs", "Maintien", "%", 0, 100, 0),
                P ("fr", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 300, 0.3f),
                P ("base", "Base", "Hz", 20, 8000, 20, 0.3f),
                P ("width", "Largeur", "oct", 0, 10, 10),

                P ("aa", "Attaque", "ms", 0.5f, 10000, 1, 0.3f),
                P ("ad", "D\xC3\xA9" "clin", "ms", 5, 10000, 600, 0.3f),
                P ("as", "Maintien", "%", 0, 100, 80),
                P ("ar", "Rel\xC3\xA2" "chement", "ms", 5, 10000, 250, 0.3f),
                P ("velamp", "V\xC3\xA9l. ampli", "%", 0, 100, 40),
                P ("spread", "Largeur st\xC3\xA9r\xC3\xA9o", "%", 0, 100, 25),

                I ("voices", "Voix", "", 1, 16, 8),
                C ("vmode", "Jeu", { "Poly", "Mono", "Mono legato" }),
                P ("glide", "Portamento", "ms", 0, 2000, 0, 0.4f),
                P ("tune", "Transposition", "dt", -24, 24, 0),
                P ("level", "Volume", "dB", -30, 6, -17),
            };
            m.sections = {
                { "fm", "FM", "osc", { "algo", "ratc", "rata", "ratb1", "ratb2", "harm", "detune", "feedback", "mix", "leva", "levb", "offa", "offb", "velmod" }, "osc" },
                { "envA", "Enveloppe A", "env", { "aatk", "adec", "aend", "amode", "areset" }, "env" },
                { "envB", "Enveloppe B", "env", { "batk", "bdec", "bend", "bmode", "breset" }, "env" },
                { "filter", "Filtres", "filter", { "drive", "ftype", "ffreq", "fres", "fmorph", "fenv", "fkey", "fvel", "base", "width" }, "filter" },
                { "fenvs", "Enveloppe filtre", "env", { "fa", "fd", "fs", "fr", "fenv" }, "env" },
                { "aenv", "Enveloppe ampli", "env", { "aa", "ad", "as", "ar", "velamp", "level" }, "amp" },
                { "global", "Global", "global", { "voices", "vmode", "glide", "tune", "spread", "level" }, "global" },
            };
            using V = std::vector<std::pair<std::string, float>>;
            const auto rc = [] (double r) { return (float) ratioIndexC (r); };
            const auto rab = [] (double r) { return (float) ratioIndexAB (r); };
            m.presets = {
                { "Init", V {}, "Init" },
                { "Basse de fer", V { { "algo", 1 }, { "ratc", rc (1) }, { "rata", rab (1) }, { "leva", 78 }, { "adec", 58 }, { "aend", 12 }, { "feedback", 30 },
                                      { "ffreq", 1400 }, { "fres", 20 }, { "fenv", 45 }, { "fd", 260 }, { "ad", 900 }, { "as", 70 }, { "vmode", 2 }, { "voices", 1 },
                                      { "tune", -12 }, { "drive", 25 }, { "level", -8.2f } }, "Basses" },
                { "Basse ronde", V { { "algo", 2 }, { "ratc", rc (0.5) }, { "rata", rab (0.5) }, { "ratb1", rab (1) }, { "leva", 55 }, { "levb", 20 }, { "adec", 50 }, { "bdec", 40 },
                                     { "ffreq", 900 }, { "fenv", 30 }, { "fd", 200 }, { "vmode", 1 }, { "voices", 1 }, { "level", -8.6f } }, "Basses" },
                { "Piano de verre", V { { "algo", 1 }, { "ratc", rc (1) }, { "rata", rab (14) }, { "ratb1", rab (1) }, { "ratb2", rab (1) }, { "leva", 52 }, { "adec", 55 },
                                        { "levb", 70 }, { "bdec", 75 }, { "bend", 10 }, { "mix", 0 }, { "ad", 2400 }, { "as", 0 }, { "ar", 900 }, { "velmod", 60 }, { "level", -16.1f } }, "Claviers" },
                { "Orgue \xC3\xA0 rapports", V { { "algo", 8 }, { "ratc", rc (1) }, { "rata", rab (2) }, { "ratb1", rab (4) }, { "ratb2", rab (1) }, { "levb", 0 }, { "mix", -10 },
                                                { "harm", -3 }, { "aa", 4 }, { "as", 100 }, { "ar", 120 }, { "level", -17.9f } }, "Claviers" },
                { "Cloche num\xC3\xA9rique", V { { "algo", 1 }, { "ratc", rc (1) }, { "rata", rab (3.5) }, { "offa", 0.07f }, { "leva", 70 }, { "adec", 90 }, { "aend", 6 },
                                                { "ratb1", rab (5) }, { "ratb2", rab (2.5) }, { "levb", 55 }, { "bdec", 85 }, { "mix", 20 },
                                                { "ad", 3500 }, { "as", 0 }, { "ar", 2500 }, { "level", -12.6f } }, "Effets" },
                { "Pluck m\xC3\xA9tal", V { { "algo", 3 }, { "ratc", rc (1) }, { "rata", rab (2) }, { "ratb1", rab (3) }, { "ratb2", rab (1) }, { "leva", 80 }, { "adec", 45 },
                                           { "levb", 60 }, { "bdec", 40 }, { "harm", 6 }, { "ad", 500 }, { "as", 0 }, { "ar", 300 }, { "ffreq", 6000 }, { "fenv", 25 }, { "fd", 300 }, { "level", -9.3f } }, "Plucks" },
                { "Lead vrill\xC3\xA9", V { { "algo", 2 }, { "ratc", rc (1) }, { "rata", rab (1) }, { "ratb1", rab (2) }, { "ratb2", rab (1) }, { "leva", 70 }, { "levb", 45 },
                                           { "adec", 127 }, { "aend", 110 }, { "amode", 1 }, { "bdec", 127 }, { "bend", 100 }, { "bmode", 1 }, { "feedback", 40 },
                                           { "detune", 25 }, { "drive", 30 }, { "vmode", 2 }, { "voices", 1 }, { "glide", 60 }, { "level", -11.5f } }, "Leads" },
                { "Nappe miroir", V { { "algo", 5 }, { "ratc", rc (1) }, { "rata", rab (1) }, { "ratb1", rab (2) }, { "ratb2", rab (1) }, { "leva", 45 }, { "aatk", 90 }, { "adec", 127 }, { "aend", 90 },
                                      { "levb", 40 }, { "batk", 100 }, { "bdec", 127 }, { "bend", 80 }, { "detune", 40 }, { "harm", 4 }, { "mix", 10 },
                                      { "aa", 900 }, { "ad", 3000 }, { "as", 85 }, { "ar", 2500 }, { "ffreq", 5000 }, { "spread", 70 }, { "level", -15.1f } }, "Nappes" },
                { "Cuivres FM", V { { "algo", 2 }, { "ratc", rc (1) }, { "rata", rab (1) }, { "ratb1", rab (1) }, { "ratb2", rab (1) }, { "leva", 72 }, { "aatk", 55 }, { "adec", 80 }, { "aend", 70 },
                                    { "levb", 30 }, { "feedback", 25 }, { "aa", 40 }, { "ad", 1200 }, { "as", 80 }, { "ar", 300 }, { "ffreq", 4000 }, { "fenv", 30 }, { "fa", 60 }, { "fd", 500 }, { "fs", 50 },
                                    { "level", -13.9f } }, "Cuivres" },
                { "Cordes de fil", V { { "algo", 6 }, { "ratc", rc (1) }, { "rata", rab (1) }, { "ratb1", rab (1) }, { "ratb2", rab (1) }, { "levb", 40 }, { "batk", 80 }, { "bdec", 127 }, { "bend", 60 },
                                       { "feedback", 50 }, { "detune", 50 }, { "mix", 20 }, { "aa", 600 }, { "ad", 2000 }, { "as", 90 }, { "ar", 1500 }, { "ffreq", 3500 }, { "spread", 80 }, { "level", -18.9f } }, "Cordes" },
                { "Arp\xC3\xA8ge cristal", V { { "algo", 1 }, { "ratc", rc (2) }, { "rata", rab (4) }, { "leva", 60 }, { "adec", 40 }, { "ratb1", rab (1) }, { "ratb2", rab (7) }, { "levb", 35 }, { "bdec", 50 },
                                              { "ad", 300 }, { "as", 0 }, { "ar", 200 }, { "arp_mode", 1 }, { "arp_oct", 2 }, { "level", -6.2f } }, "Arp\xC3\xA8ges" },
                { "Kick FM", V { { "algo", 1 }, { "ratc", rc (0.5) }, { "rata", rab (1.5) }, { "leva", 90 }, { "adec", 25 }, { "feedback", 20 }, { "ad", 180 }, { "as", 0 }, { "ar", 120 },
                                 { "ffreq", 3000 }, { "vmode", 1 }, { "voices", 1 }, { "tune", -24 }, { "level", 2.2f } }, "Effets" },
                { "Grain de bois", V { { "algo", 7 }, { "ratc", rc (1) }, { "rata", rab (3) }, { "ratb1", rab (5.5) }, { "ratb2", rab (2) }, { "leva", 40 }, { "adec", 20 }, { "levb", 75 }, { "bdec", 18 },
                                       { "ad", 260 }, { "as", 0 }, { "ar", 200 }, { "base", 180 }, { "level", -5.8f } }, "Plucks" },
                { "Voix de verre", V { { "algo", 4 }, { "ratc", rc (1) }, { "rata", rab (2) }, { "ratb1", rab (3) }, { "ratb2", rab (1) }, { "leva", 40 }, { "levb", 30 }, { "harm", -8 },
                                       { "fmorph", 50 }, { "ffreq", 1800 }, { "fres", 45 }, { "aa", 300 }, { "ad", 2000 }, { "as", 80 }, { "ar", 1200 },
                                       { "mx1_src", 6 }, { "mx1_dst@ffreq", 0 }, { "mx1_amt", 25 }, { "lf1_rate", 0.4f }, { "level", -9.5f } }, "Nappes" },
            };
            HubSpec hub;
            hub.envs = 1; hub.lfos = 2; hub.firstLfo = 1; hub.slots = 8; hub.arp = true;
            addModHub (m, hub);
            resolveHubPresetDests (m);
            addFactoryBank (m); // v0.45.2 : banque d'usine, au moins 10 sons par catégorie (FactoryBanks.h)
            return m;
        }();
        return d;
    }
}

/** Enveloppe de modulation d'un groupe (A ou B) : Attaque / Déclin / Fin, déclenchée, ou à la touche
    (Attaque, tenue tant que la touche est enfoncée, puis Déclin vers Fin). La fin est tenue. */
class Fm4ModEnv
{
public:
    void reset() noexcept { v = 0.0; stage = idle; }
    void trigger (bool fromZero) noexcept { if (fromZero) v = 0.0; stage = attack; }
    void gateOff (bool gated) noexcept { if (gated && (stage == attack || stage == hold)) stage = decay; }
    /** Coefficients pour le bloc (temps en secondes). */
    void set (double atkS, double decS, double endLevel, bool gatedMode, double sr) noexcept
    {
        atkInc = atkS <= 0.0 ? 2.0 : 1.0 / (atkS * sr);
        decCoef = decS <= 0.0 ? 0.0 : std::exp (std::log (0.01) / (decS * sr));
        endTarget = std::clamp (endLevel, 0.0, 1.0);
        if (stage == idle) end = endTarget;
        endCoef = 1.0 - std::exp (-1.0 / (0.012 * sr)); // v0.45.1 : la fin suit le bouton en 12 ms (sans craquement)
        gated = gatedMode;
    }
    double next() noexcept
    {
        end += (endTarget - end) * endCoef;
        switch (stage)
        {
            case attack: v += atkInc; if (v >= 1.0) { v = 1.0; stage = gated ? hold : decay; } break;
            case hold: v = 1.0; break;
            case decay: v = end + (v - end) * decCoef; if (std::abs (v - end) < 1.0e-4) { v = end; stage = done; } break;
            case done: v = end; break;
            default: break;
        }
        return v;
    }

private:
    enum Stage { idle, attack, hold, decay, done };
    double v = 0.0, atkInc = 1.0, decCoef = 0.0, end = 0.0, endTarget = 0.0, endCoef = 1.0;
    bool gated = false;
    Stage stage = idle;
};

class Fm4Voice
{
public:
    void prepare (double s) { sr = s; fenv.prepare (s); aenv.prepare (s); reset(); }
    void reset() noexcept
    {
        ph.fill (0.0); fb1 = fb2 = 0.0;
        envA.reset(); envB.reset(); fenv.reset(); aenv.reset();
        f1.reset(); lad.reset(); f2hp.reset(); f2lp.reset(); dc.reset();
        active = gate = pending = releasePending = smoothed = false;
    }
    void setIndex (int i) noexcept
    {
        // place fixe de la voix dans le champ stéréo (alternée gauche / droite)
        static constexpr double spots[8] { 0.15, -0.15, 0.5, -0.5, 0.8, -0.8, 0.35, -0.35 };
        pan = spots[i & 7];
    }
    void setSpread (int index, int count) noexcept { uni = count > 1 ? 2.0 * index / (count - 1) - 1.0 : 0.0; }
    void start (int note, float velocity, bool legatoStart, int prevNote) noexcept
    {
        if (velocity >= 0.0f) vel = velocity;
        target = note; from = prevNote; legatoPending = legatoStart && ! (pending && ! legatoPending); pending = true; releasePending = false; active = true; gate = true;
    }
    // v0.45.1 : une voix muette ne garde pas de relâchement en attente (changement de mode, arrêt)
    void release() noexcept { if (active) { gate = false; releasePending = true; } }
    bool isActive() const noexcept { return active; }

    /** Onde d'un opérateur : sinus, plus un partiel ajouté (série h, balayage continu entre deux séries). */
    static double wave (double phase, double h) noexcept
    {
        const auto s = fastSin2Pi (phase);
        if (h <= 1.0e-3) return s;
        const auto k0 = std::floor (h);
        const auto f = h - k0;
        auto extra = [phase] (double k) { return k < 0.5 ? 0.0 : (0.8 / std::sqrt (k)) * fastSin2Pi (phase * (k + 1.0)); };
        const auto e = (1.0 - f) * extra (k0) + f * extra (k0 + 1.0);
        return (s + e) * (1.0 / (1.0 + 0.45 * std::min (1.0, h)));
    }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using namespace fm4;
        const auto& alg = algorithms()[(size_t) std::clamp ((int) std::lround (p[algo]) - 1, 0, 7)];
        if (pending)
        {
            pending = false;
            if (p[glide] > 0.5f && from >= 0 && legatoPending) pitch.to ((double) target);
            else if (p[glide] > 0.5f && from >= 0) { pitch.jump ((double) from); pitch.to ((double) target); }
            else pitch.jump ((double) target);
            if (! legatoPending)
            {
                envA.trigger (p[aReset] > 0.5f);
                envB.trigger (p[bReset] > 0.5f);
                fenv.gate (true); aenv.gate (true);
            }
        }
        if (releasePending)
        {
            releasePending = false;
            envA.gateOff (p[aMode] > 0.5f);
            envB.gateOff (p[bMode] > 0.5f);
            fenv.gate (false); aenv.gate (false);
        }
        envA.set (modEnvSeconds (p[aAtk]), modEnvSeconds (p[aDec]), p[aEnd] / 127.0, p[aMode] > 0.5f, sr);
        envB.set (modEnvSeconds (p[bAtk]), modEnvSeconds (p[bDec]), p[bEnd] / 127.0, p[bMode] > 0.5f, sr);
        fenv.set (p[fA], p[fD], p[fS] / 100.0, p[fR]);
        aenv.set (p[aa], p[ad], p[as] / 100.0, p[ar]);

        // hauteur (portamento, transposition + pitch bend, désaccord d'unisson) : une fois par bloc
        const auto glideCoef = p[glide] < 0.5f ? 0.0 : std::exp (-(double) n / (p[glide] * 0.001 * sr * 0.35));
        const auto notePitch = pitch.next (glideCoef) + p[tune] + uni * 0.12;
        const auto keyHz = noteHz (notePitch);
        const auto det = p[detune] / 127.0 * 30.0 / 1200.0; // jusqu'à ±30 centièmes
        const auto rC = ratiosC()[(size_t) std::clamp ((int) std::lround (p[ratC]), 0, 17)];
        const auto rA = ratiosAB()[(size_t) std::clamp ((int) std::lround (p[ratA]), 0, 33)] + p[offA];
        const auto rB1 = ratiosAB()[(size_t) std::clamp ((int) std::lround (p[ratB1]), 0, 33)] + p[offB];
        const auto rB2 = ratiosAB()[(size_t) std::clamp ((int) std::lround (p[ratB2]), 0, 33)] + p[offB];
        std::array<double, 4> inc {
            keyHz * rC / sr,
            keyHz * std::max (0.05, rA) * fastExp2 (det) / sr,
            keyHz * std::max (0.05, rB1) * fastExp2 (-det) / sr,
            keyHz * std::max (0.05, rB2) / sr };
        for (auto& x : inc) x = std::clamp (x, 0.0, 0.49);

        // harmoniques : négatif → C, positif → A et B1
        const auto h = (double) p[harm];
        // niveaux de modulation (en cycles de déviation de phase), vélocité
        const auto velK = 1.0 - (p[velMod] / 100.0) * (1.0 - vel);
        const auto la = std::pow (p[levA] / 127.0, 2.0) * 2.5 * velK, lb = std::pow (p[levB] / 127.0, 2.0) * 2.5 * velK;
        const auto fbAmt = std::pow (p[feedback] / 127.0, 1.5) * 1.1;
        // sorties X / Y : fondu à puissance constante
        const auto th = (std::clamp ((double) p[mix], -63.0, 63.0) / 63.0 + 1.0) * 0.25 * kPi;
        const auto gx = std::cos (th), gy = std::sin (th) * (((alg.y & (alg.y - 1)) != 0) ? 0.7 : 1.0);

        const auto od = p[drive] / 100.0;
        const auto drv = 1.0 + od * od * 9.0, drvComp = 1.0 / std::sqrt (drv);
        const bool ladder = p[fType] > 0.5f;
        const auto res = p[fRes] / 100.0;
        const auto morph = p[fMorph] / 100.0;
        const auto keyOct = (p[fKey] / 100.0) * (notePitch - 60.0) / 12.0;
        const auto velOct = (p[fVel] / 100.0) * 3.0 * (vel - 0.6);
        const auto envOct = (p[fEnv] / 100.0) * 7.0;
        // v0.45.1 : la bande Base / Largeur entre et sort en fondu (plus de clic quand la largeur touche son maximum)
        const double bandTarget = (p[base] > 21.0f || p[width] < 9.99f) ? 1.0 : 0.0;
        const bool baseOn = bandTarget > 0.0 || bandMix > 1.0e-4;
        SvfCoefs hpC, lpC;
        if (bandTarget > 0.0 && bandMix <= 1.0e-4) { f2hp.reset(); f2lp.reset(); } // reprise propre de la bande
        if (baseOn)
        {
            const auto b = std::clamp ((double) p[base], 20.0, 8000.0);
            hpC.set (b, 0.0, sr);
            lpC.set (std::clamp (b * fastExp2 (std::clamp ((double) p[width], 0.0, 10.0)), 30.0, 0.45 * sr), 0.0, sr);
        }
        const auto gain = dbToGain (p[level]) * velocityGain (vel, p[velAmp] / 100.0);
        const auto sp = std::clamp ((double) p[spread], 0.0, 100.0) / 100.0;
        const auto pv = std::clamp (pan * sp + uni * sp * 0.8, -1.0, 1.0);
        const auto gL = std::sqrt (0.5 * (1.0 - pv)) * 1.4142, gR = std::sqrt (0.5 * (1.0 + pv)) * 1.4142;

        // v0.45.1 : niveaux A / B, rétroaction et volume en rampe sur le bloc (bouton tourné sans craquement)
        if (! smoothed) { laS = la; lbS = lb; fbS = fbAmt; gS = gain; gxS = gx; gyS = gy; hS = h; mS = morph; bandMix = bandTarget; smoothed = true; }
        const auto inv = 1.0 / std::max (1, n);
        const auto dLa = (la - laS) * inv, dLb = (lb - lbS) * inv, dFb = (fbAmt - fbS) * inv, dG = (gain - gS) * inv;
        const auto dGx = (gx - gxS) * inv, dGy = (gy - gyS) * inv, dH = (h - hS) * inv, dM = (morph - mS) * inv, dBand = (bandTarget - bandMix) * inv;
        SvfCoefs fc;
        double fcHz = 1000.0;
        for (int i = 0; i < n; ++i)
        {
            laS += dLa; lbS += dLb; fbS += dFb; gS += dG; gxS += dGx; gyS += dGy; hS += dH; mS += dM; bandMix += dBand;
            const std::array<double, 4> hw { hS < 0.0 ? -hS : 0.0, hS > 0.0 ? hS : 0.0, hS > 0.0 ? hS : 0.0, 0.0 };
            const auto ea = envA.next() * laS, eb = envB.next() * lbS;
            const std::array<double, 4> idx { 0.0, ea, eb, eb };
            std::array<double, 4> out {};
            for (int k = 3; k >= 0; --k)
            {
                double mod = 0.0;
                for (int j = k + 1; j < 4; ++j) if ((alg.mods[k] >> j) & 1) mod += out[(size_t) j] * idx[(size_t) j];
                if (k == alg.fb) mod += fbS * 0.5 * (fb1 + fb2);
                out[(size_t) k] = wave (ph[(size_t) k] + mod, hw[(size_t) k]);
                if (k == alg.fb) { fb2 = fb1; fb1 = out[(size_t) k]; }
                ph[(size_t) k] += inc[(size_t) k];
                if (ph[(size_t) k] >= 1.0) ph[(size_t) k] -= 1.0;
            }
            double y = 0.0;
            for (int k = 0; k < 4; ++k) if ((alg.y >> k) & 1) y += out[(size_t) k];
            auto x = out[(size_t) alg.x] * gxS + y * gyS;
            if (od > 0.0) x = fastTanh (x * drv) * drvComp * 1.3;

            const auto fe = fenv.next();
            if ((i & 7) == 0)
            {
                fcHz = std::clamp ((double) p[fFreq] * fastExp2 (envOct * fe + keyOct + velOct), 16.0, 0.45 * sr);
                if (! ladder) fc.set (fcHz, res, sr);
            }
            x = ladder ? lad.process (x * 0.5, fcHz, res, sr, Ladder::lp24, 1.0) * 2.0
                       : [&] { const auto o = f1.process (x, fc);
                               return mS < 0.5 ? o.low + (o.band - o.low) * (mS * 2.0) : o.band + (o.high - o.band) * ((mS - 0.5) * 2.0); }();
            if (baseOn) x += (f2lp.process (f2hp.process (x, hpC).high, lpC).low - x) * bandMix;
            const auto v = dc.process (x) * aenv.next() * gS;
            L[i] += (float) (v * gL);
            R[i] += (float) (v * gR);
        }
        laS = la; lbS = lb; fbS = fbAmt; gS = gain; gxS = gx; gyS = gy; hS = h; mS = morph; bandMix = bandTarget;
        if (! aenv.isActive()) { active = false; smoothed = false; }
    }

private:
    double sr = 48000.0, pan = 0.0, uni = 0.0, fb1 = 0.0, fb2 = 0.0, laS = 0.0, lbS = 0.0, fbS = 0.0, gS = 0.0, gxS = 0.0, gyS = 0.0, hS = 0.0, mS = 0.0, bandMix = 0.0;
    bool smoothed = false;
    std::array<double, 4> ph {};
    float vel = 1.0f;
    int target = 60, from = -1;
    bool active = false, gate = false, pending = false, releasePending = false, legatoPending = false;
    Fm4ModEnv envA, envB;
    Adsr fenv, aenv;
    StateFilter f1, f2hp, f2lp;
    Ladder lad;
    Glide pitch;
    DcBlocker dc;
};

class Fm4Synth final : public SynthBase<Fm4Voice, 16>
{
public:
    Fm4Synth() { for (int i = 0; i < 16; ++i) voices[(size_t) i].setIndex (i); }
    const ModuleDesc& desc() const noexcept override { return fm4::desc(); }

protected:
    void configure (const float* p) noexcept override
    {
        const auto v = std::clamp ((int) p[fm4::voices], 1, 16);
        switch ((int) p[fm4::vMode])
        {
            case 1: setMode (true, false, v); break;
            case 2: setMode (true, true, v); break;
            default: setMode (false, true, v); break;
        }
    }
};

} // namespace grv::instruments
