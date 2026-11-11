/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Famille « Vocodeurs » (v0.25) : quatre instruments qui reprennent le PRINCIPE de quatre
    vocodeurs connus (architecture seulement : ni nom, ni façade, ni son d'usine d'origine).

      - « Vocodeur 16 bandes » (principe du vocodeur analogique 16 canaux de 1978) : 16 bandes fixes
        de 50 Hz à 5 080 Hz en 24 dB/oct, suiveurs très rapides, souffle (sibilantes) et bourdon
        (plosives) ajoutés à la porteuse, passage direct des aigus au-dessus de 5 080 Hz, maintien
        (échantillonneur-bloqueur des enveloppes), décalage des bandes (croisement du tableau de
        connexion : la bande k de la porteuse s'ouvre avec la bande k ± n de la voix).
      - « Vocodeur à cordes » (principe des claviers cordes + vocodeur) : la porteuse est une
        machine à cordes (scie 8' + 4', registre qui passe du violon à l'orgue, ensemble à trois
        phases, crescendo et relâchement, instabilité « jitter »), 16 à 40 bandes, déplacement des
        formants, gel, sibilantes, cordes seules mélangeables.
      - « Vocodeur spectral » (principe des vocodeurs logiciels à algorithmes multiples) : 8
        algorithmes (analogique 12 / 20 bandes, numérique 32, haute définition 40, voyelles,
        inversé, cristal, velours), formants, expansion, traînes réverbérantes, stéréo par bandes,
        synthèse des consonnes non voisées, aigus directs, porteuse à unisson et FM linéaire, gel.
      - « Voix de synthèse » (principe des processeurs vocaux à modules parallèles) : vocodeur 12
        bandes, voix d'ordinateur (vocodeur 6 bandes réduit en bits et en fréquence), boîte parlante
        (trois formants suivis en temps réel, sans latence), voix modélisée (voyelle, nasalité,
        formants) et épaississement (doublage stéréo) mélangés par niveaux.

    Modulateur (la voix) : voix interne (phrase de démonstration ou phrase tapée, v0.33), entrée
    audio, piste précédente (qui peut être retirée du mixage), n'importe quelle piste (lue après ses
    inserts, avant son volume), ou prise de son (v0.33) : un appui arme l'enregistrement, il démarre
    dès que la voix arrive à l'entrée audio (avec 80 ms d'avance gardées en mémoire pour ne pas
    couper la première consonne), s'arrête seul après 0,9 s de silence (ou au second appui), puis la
    phrase tourne en boucle, libre ou calée sur 1, 2 ou 4 mesures du séquenceur. Elle est gardée
    avec le projet comme un échantillon (emplacement 1 de la piste).
    Porteuse : 8 voix jouées au clavier, au séquenceur ou par l'arpège.
*/

#pragma once

#include "../synth/SynthBase.h"
#include "../synth/Vocoder.h"
#include "FactoryBanks.h"

#include <atomic>

namespace grv::instruments
{

using namespace grv::engine;
using namespace grv::synth;

namespace voc
{
    enum Model { m16 = 0, strings, spectral, vsynth, numModels };

    /** Sources du modulateur. */
    inline const std::vector<std::string>& sourceChoices()
    {
        static const std::vector<std::string> v = []
        {
            std::vector<std::string> c { "Voix interne", "Entr\xC3\xA9" "e audio", "Piste pr\xC3\xA9" "c\xC3\xA9" "dente" };
            for (int t = 1; t <= numTracks; ++t) c.push_back ("Piste " + std::to_string (t));
            c.push_back ("Prise de son"); // v0.33 : ajouté à la fin (projets convertis : version 14)
            return c;
        }();
        return v;
    }
    enum Source { srcInternal = 0, srcInput, srcPrevious, srcTrack1 };
    inline constexpr int srcCapture = srcTrack1 + numTracks; // v0.33 : prise de son (phrase enregistrée)
    inline constexpr double captureSeconds = 16.0;            // durée maximale d'une prise

    inline ParamDesc keep (ParamDesc d) { d.keepOnPreset = true; return d; }

    /** Réglages du modulateur, communs aux quatre modèles. */
    inline void addModulator (ModuleDesc& m)
    {
        m.params.push_back (keep (C ("src", "Modulateur", sourceChoices())));
        m.params.push_back (keep (P ("srcgain", "Gain du modulateur", "dB", -24, 24, 0)));
        m.params.push_back (keep (C ("srcmute", "Piste pr\xC3\xA9" "c\xC3\xA9" "dente", { "Entendue", "Retir\xC3\xA9" "e du mixage" }, 1)));
        m.params.push_back (keep (P ("gate", "Porte de bruit", "dB", -90, -20, -60)));
        m.params.push_back (P ("vrate", "D\xC3\xA9" "bit (voix interne)", "Hz", 0.5f, 8.0f, 2.5f, 0.6f));
        m.params.push_back (P ("vpitch", "Hauteur (voix interne)", "Hz", 70, 300, 125, 0.6f));
        m.params.push_back (P ("dry", "Voix directe", "%", 0, 100, 0));
        // v0.39 : ampleur de la mélodie de la phrase tapée (0 = recto tono, 100 = normale, 200 = chantante)
        m.params.push_back (P ("vexpr", "Intonation (voix interne)", "%", 0, 200, 100));
    }

    inline const std::vector<std::string>& waveChoices()
    {
        static const std::vector<std::string> v { "Scie", "Impulsion", "Scie + sous-octave", "Cordes", "Ch\xC5\x93ur", "Bruit" };
        return v;
    }

    /** Réglages de la porteuse ; extra : ceux propres au modèle (registre, unisson, FM…). */
    inline void addCarrier (ModuleDesc& m, int wave, bool strings, bool spectral)
    {
        m.params.push_back (I ("voices", "Voix", "", 1, 8, 8));
        m.params.push_back (C ("cwave", "Forme", waveChoices(), wave));
        m.params.push_back (P ("cpw", "Largeur", "%", 5, 95, 50));
        m.params.push_back (C ("coct", "Registre", { "16'", "8'", "4'" }, 1));
        m.params.push_back (P ("tune", "Accord", "dt", -24, 24, 0));
        m.params.push_back (P ("cdetune", "D\xC3\xA9saccord", "ct", 0, 50, strings ? 8.0f : 6.0f));
        m.params.push_back (I ("cuni", "Unisson", "", 1, 4, spectral ? 2 : 1));
        if (spectral) m.params.push_back (P ("cfm", "FM lin\xC3\xA9" "aire", "%", 0, 100, 0));
        if (strings)
        {
            m.params.push_back (P ("creg", "Registre (violon \xE2\x86\x92 orgue)", "%", 0, 100, 35));
            m.params.push_back (P ("cjitter", "Instabilit\xC3\xA9", "%", 0, 100, 20));
        }
        m.params.push_back (P ("cbright", "Brillance", "Hz", 500, 18000, 12000, 0.3f));
        m.params.push_back (P ("cnoise", "Bruit", "%", 0, 100, 0));
        m.params.push_back (P ("ca", strings ? "Crescendo" : "Attaque", "ms", 0.5f, 4000, strings ? 120.0f : 5.0f, 0.3f));
        m.params.push_back (P ("cr", "Rel\xC3\xA2" "chement", "ms", 5, 8000, strings ? 900.0f : 250.0f, 0.3f));
    }

    struct Algo { const char* name; int bands; double fLo, fHi; int order; double q; double attMul, relMul; bool invert; double trimDb; };
    inline constexpr Algo algos[8] {
        { "Analogique 12", 12, 100.0, 6000.0, 2, 1.0, 1.0, 1.0, false, 1.5 },
        { "Analogique 20", 20, 80.0, 8000.0, 2, 1.0, 1.0, 1.0, false, 0.5 },
        { "Num\xC3\xA9rique 32", 32, 60.0, 10000.0, 2, 1.2, 0.7, 0.8, false, 0.5 },
        { "Haute d\xC3\xA9" "finition 40", 40, 50.0, 12000.0, 2, 1.5, 0.6, 0.8, false, -1.5 },
        { "Voyelles", 16, 200.0, 4000.0, 1, 2.5, 1.0, 1.2, false, 2.5 },
        { "Invers\xC3\xA9", 20, 80.0, 8000.0, 2, 1.0, 1.0, 1.0, true, 3.5 },
        { "Cristal", 24, 150.0, 9000.0, 2, 3.2, 1.0, 1.5, false, -5.5 },
        { "Velours", 10, 100.0, 5000.0, 1, 0.6, 2.5, 2.0, false, -1.0 },
    };

    inline const ModuleDesc& desc (int model)
    {
        static const std::array<ModuleDesc, numModels> all = []
        {
            std::array<ModuleDesc, numModels> d;
            HubSpec hub;
            hub.envs = 0; hub.lfos = 1; hub.slots = 4; hub.firstLfo = 1; hub.arp = true;
            const std::vector<std::string> onOff { "Non", "Oui" };

            // --- Vocodeur 16 bandes ---------------------------------------------------------------
            {
                auto& m = d[m16];
                m.type = "voc16";
                m.name = "Robot 78";
                m.family = "Vocodeurs";
                m.principle = "Vocodeur analogique 16 canaux : bandes fixes de 50 Hz \xC3\xA0 5 080 Hz, souffle et bourdon, aigus directs, maintien, d\xC3\xA9" "calage des bandes";
                addModulator (m);
                addCarrier (m, 0, false, false);
                m.params.push_back (P ("resp", "R\xC3\xA9" "ponse", "ms", 0.3f, 30, 1.5f, 0.4f));
                m.params.push_back (P ("rel", "Rel\xC3\xA2" "chement", "ms", 5, 600, 35, 0.4f));
                m.params.push_back (I ("shift", "D\xC3\xA9" "calage des bandes", "", -8, 8, 0));
                m.params.push_back (P ("hiss", "Souffle", "%", 0, 100, 45));
                m.params.push_back (P ("buzz", "Bourdon", "%", 0, 100, 0));
                m.params.push_back (C ("direct", "Aigus directs", onOff, 1));
                m.params.push_back (P ("directlvl", "Niveau des aigus", "%", 0, 100, 40));
                m.params.push_back (C ("hold", "Maintien", onOff));
                m.params.push_back (P ("level", "Volume", "dB", -30, 6, -6));
                m.sections = {
                    { "mod", "Modulateur", "global", { "src", "srcgain", "gate", "srcmute", "vrate", "vpitch", "dry", "ploop" }, "global" },
                    { "car", "Porteuse", "osc", { "cwave", "cpw", "coct", "cdetune", "cuni", "cbright", "cnoise", "tune" }, "osc" },
                    { "voc", "Vocodeur", "filter", { "resp", "rel", "shift", "hiss", "buzz", "direct", "directlvl", "hold" }, "filter" },
                    { "amp", "Ampli", "env", { "ca", "cr", "voices", "level" }, "env" },
                };
                m.presets = {
                    { "Robot classique", { { "cwave", 0 }, { "hiss", 45 }, { "level", -3 } }, "Voix" },
                    { "Ch\xC5\x93ur 1978", { { "cwave", 4 }, { "cuni", 3 }, { "cdetune", 14 }, { "ca", 60 }, { "cr", 900 }, { "rel", 60 }, { "level", -3.5f } }, "Voix" },
                    { "Voix grave", { { "cwave", 2 }, { "coct", 0 }, { "cbright", 6000 }, { "hiss", 30 }, { "level", -2 } }, "Voix" },
                    { "Impulsion parlante", { { "cwave", 1 }, { "cpw", 25 }, { "resp", 0.6f }, { "rel", 20 }, { "level", -6 } }, "Voix" },
                    { "Bandes d\xC3\xA9" "cal\xC3\xA9" "es +3", { { "shift", 3 }, { "cwave", 0 }, { "level", 0.5f } }, "Effets" },
                    { "Bandes d\xC3\xA9" "cal\xC3\xA9" "es -4", { { "shift", -4 }, { "cwave", 2 }, { "level", 1 } }, "Effets" },
                    { "Tenue longue", { { "rel", 400 }, { "cwave", 4 }, { "cr", 1500 }, { "level", -6 } }, "Nappes" },
                    { "Murmure", { { "cwave", 5 }, { "hiss", 80 }, { "directlvl", 70 }, { "level", -1.5f } }, "Effets" },
                    { "Bourdon et souffle", { { "buzz", 60 }, { "hiss", 70 }, { "cwave", 1 }, { "cpw", 40 }, { "level", -9 } }, "Voix" },
                    { "Lent et doux", { { "resp", 12 }, { "rel", 250 }, { "cbright", 4000 }, { "cwave", 4 }, { "ca", 200 }, { "cr", 1500 }, { "level", -5.5f } }, "Nappes" },
                };
            }

            // --- Vocodeur à cordes ------------------------------------------------------------------
            {
                auto& m = d[strings];
                m.type = "vocstr";
                m.name = "Robot Choral";
                m.family = "Vocodeurs";
                m.principle = "Clavier cordes et vocodeur : porteuse \xC3\xA0 cordes (8' + 4', registre, ensemble), 16 \xC3\xA0 40 bandes, formants, gel, sibilantes";
                addModulator (m);
                addCarrier (m, 3, true, false);
                m.params.push_back (C ("bands", "Bandes", { "16", "24", "32", "40" }, 2));
                m.params.push_back (P ("formant", "Formants", "dt", -12, 12, 0));
                m.params.push_back (P ("resp", "R\xC3\xA9" "ponse", "ms", 0.3f, 30, 2.0f, 0.4f));
                m.params.push_back (P ("rel", "Rel\xC3\xA2" "chement", "ms", 5, 1500, 60, 0.4f));
                m.params.push_back (C ("freeze", "Gel", onOff));
                m.params.push_back (P ("sib", "Sibilantes", "%", 0, 100, 50));
                m.params.push_back (P ("ensemble", "Ensemble", "%", 0, 100, 60));
                m.params.push_back (P ("strings", "Cordes seules", "%", 0, 100, 0));
                m.params.push_back (P ("level", "Volume", "dB", -30, 6, -6));
                m.sections = {
                    { "mod", "Modulateur", "global", { "src", "srcgain", "gate", "srcmute", "vrate", "vpitch", "dry", "ploop" }, "global" },
                    { "car", "Cordes", "osc", { "cwave", "creg", "coct", "cdetune", "cjitter", "cbright", "cnoise", "tune" }, "osc" },
                    { "voc", "Vocodeur", "filter", { "bands", "formant", "resp", "rel", "freeze", "sib", "strings" }, "filter" },
                    { "amp", "Ensemble et ampli", "env", { "ca", "cr", "ensemble", "voices", "level" }, "env" },
                };
                m.presets = {
                    { "Cordes qui chantent", { { "level", -3.5f } }, "Voix" },
                    { "Ch\xC5\x93ur d'orgue", { { "creg", 85 }, { "ensemble", 40 }, { "bands", 3 }, { "level", -2 } }, "Voix" },
                    { "Violons parlants", { { "creg", 5 }, { "cbright", 15000 }, { "ensemble", 75 }, { "level", -4 } }, "Voix" },
                    { "Formants graves", { { "formant", -5 }, { "coct", 0 }, { "level", 0 } }, "Voix" },
                    { "Formants aigus", { { "formant", 6 }, { "coct", 2 }, { "level", 2 } }, "Effets" },
                    { "Nappe lente", { { "rel", 600 }, { "ca", 800 }, { "cr", 3000 }, { "level", 0 } }, "Nappes" },
                    { "Cordes et voix", { { "strings", 35 }, { "ensemble", 80 }, { "level", -8.5f } }, "Nappes" },
                    { "16 bandes vintage", { { "bands", 0 }, { "sib", 70 }, { "cjitter", 45 }, { "level", -5 } }, "Voix" },
                    { "Ch\xC5\x93ur lent", { { "ca", 1500 }, { "cr", 4000 }, { "rel", 300 }, { "ensemble", 90 }, { "level", 1 } }, "Nappes" },
                    { "Impulsions chorales", { { "cwave", 4 }, { "ensemble", 70 }, { "bands", 1 }, { "level", -5.5f } }, "Voix" },
                };
            }

            // --- Vocodeur spectral ------------------------------------------------------------------
            {
                auto& m = d[spectral];
                m.type = "vocspec";
                m.name = "Spectral Vocal";
                m.family = "Vocodeurs";
                m.principle = "Vocodeur logiciel \xC3\xA0 algorithmes : 8 bancs (analogique, num\xC3\xA9rique, voyelles, invers\xC3\xA9, cristal\xE2\x80\xA6), formants, expansion, tra\xC3\xAEnes, st\xC3\xA9r\xC3\xA9o, consonnes de synth\xC3\xA8se";
                addModulator (m);
                addCarrier (m, 0, false, true);
                std::vector<std::string> names;
                for (const auto& a : algos) names.push_back (a.name);
                m.params.push_back (C ("algo", "Algorithme", names, 1));
                m.params.push_back (P ("formant", "Formants", "dt", -12, 12, 0));
                m.params.push_back (P ("expand", "Expansion", "%", -50, 100, 0));
                m.params.push_back (P ("traces", "Tra\xC3\xAEnes", "%", 0, 100, 0));
                m.params.push_back (P ("stereo", "St\xC3\xA9r\xC3\xA9o", "%", 0, 100, 40));
                m.params.push_back (P ("unvoiced", "Consonnes", "%", 0, 100, 50));
                m.params.push_back (P ("hfthru", "Aigus directs", "%", 0, 100, 30));
                m.params.push_back (P ("resp", "R\xC3\xA9" "ponse", "ms", 0.3f, 30, 2.0f, 0.4f));
                m.params.push_back (P ("rel", "Rel\xC3\xA2" "chement", "ms", 5, 1500, 50, 0.4f));
                m.params.push_back (C ("freeze", "Gel", onOff));
                m.params.push_back (P ("level", "Volume", "dB", -30, 6, -6));
                m.sections = {
                    { "mod", "Modulateur", "global", { "src", "srcgain", "gate", "srcmute", "vrate", "vpitch", "dry", "ploop" }, "global" },
                    { "car", "Synth\xC3\xA9tiseur", "osc", { "cwave", "cpw", "coct", "cuni", "cdetune", "cfm", "cbright", "cnoise" }, "osc" },
                    { "voc", "Vocodeur", "filter", { "algo", "formant", "expand", "traces", "stereo", "unvoiced", "hfthru", "freeze" }, "filter" },
                    { "amp", "R\xC3\xA9" "ponse et ampli", "env", { "resp", "rel", "ca", "cr", "voices", "tune", "level" }, "env" },
                };
                m.presets = {
                    { "Analogique propre", { { "level", -4.5f } }, "Voix" },
                    { "Haute d\xC3\xA9" "finition", { { "algo", 3 }, { "hfthru", 45 }, { "level", -5 } }, "Voix" },
                    { "Num\xC3\xA9rique 32", { { "algo", 2 }, { "cuni", 3 }, { "cdetune", 12 }, { "level", -3.5f } }, "Voix" },
                    { "Voyelles r\xC3\xA9sonantes", { { "algo", 4 }, { "cwave", 1 }, { "cpw", 30 }, { "level", -5 } }, "Voix" },
                    { "Monde invers\xC3\xA9", { { "algo", 5 }, { "stereo", 70 }, { "level", 3 } }, "Effets" },
                    { "Cristal", { { "algo", 6 }, { "traces", 40 }, { "stereo", 80 }, { "level", 0.5f } }, "Effets" },
                    { "Velours", { { "algo", 7 }, { "cwave", 4 }, { "ca", 80 }, { "cr", 1200 }, { "level", -1.5f } }, "Nappes" },
                    { "Tra\xC3\xAEnes infinies", { { "traces", 90 }, { "algo", 1 }, { "cr", 2500 }, { "level", -8.5f } }, "Nappes" },
                    { "FM qui parle", { { "cfm", 45 }, { "cuni", 1 }, { "algo", 2 }, { "level", -7.5f } }, "Voix" },
                    { "Expansion forte", { { "expand", 80 }, { "algo", 3 }, { "level", -4.5f } }, "Voix" },
                    { "Formants descendus", { { "formant", -7 }, { "coct", 0 }, { "level", -1.5f } }, "Voix" },
                    { "Large et doux", { { "expand", -30 }, { "stereo", 100 }, { "cuni", 4 }, { "cdetune", 20 }, { "level", -8 } }, "Nappes" },
                };
            }

            // --- Voix de synthèse --------------------------------------------------------------------
            {
                auto& m = d[vsynth];
                m.type = "vocsyn";
                m.name = "Speak Synth";
                m.family = "Vocodeurs";
                m.principle = "Processeur vocal \xC3\xA0 modules parall\xC3\xA8les : vocodeur, voix d'ordinateur, bo\xC3\xAEte parlante, voix mod\xC3\xA9lis\xC3\xA9" "e, \xC3\xA9paississement";
                addModulator (m);
                addCarrier (m, 0, false, false);
                m.params.push_back (P ("mvoc", "Vocodeur", "%", 0, 100, 80));
                m.params.push_back (P ("mcomp", "Ordinateur", "%", 0, 100, 0));
                m.params.push_back (P ("mtalk", "Bo\xC3\xAEte parlante", "%", 0, 100, 0));
                m.params.push_back (P ("mbio", "Voix mod\xC3\xA9lis\xC3\xA9" "e", "%", 0, 100, 0));
                m.params.push_back (P ("mpoly", "\xC3\x89paississement", "%", 0, 100, 25));
                m.params.push_back (P ("vformant", "Formants du vocodeur", "dt", -12, 12, 0));
                m.params.push_back (I ("crush", "Bits (ordinateur)", "", 2, 12, 5));
                m.params.push_back (I ("crate", "\xC3\x89" "chantillonnage (ordinateur)", "", 1, 24, 6));
                m.params.push_back (P ("talkshift", "Formants (bo\xC3\xAEte)", "dt", -12, 12, 0));
                m.params.push_back (P ("vowel", "Voyelle (A \xE2\x86\x92 OU)", "%", 0, 100, 25));
                m.params.push_back (P ("nasal", "Nasalit\xC3\xA9", "%", 0, 100, 20));
                m.params.push_back (P ("bioshift", "Formants (voix)", "dt", -12, 12, 0));
                m.params.push_back (P ("polywidth", "Largeur (\xC3\xA9paississement)", "%", 0, 100, 70));
                m.params.push_back (P ("resp", "R\xC3\xA9" "ponse", "ms", 0.3f, 30, 2.0f, 0.4f));
                m.params.push_back (P ("rel", "Rel\xC3\xA2" "chement", "ms", 5, 1000, 40, 0.4f));
                m.params.push_back (P ("level", "Volume", "dB", -30, 6, -6));
                m.sections = {
                    { "mod", "Modulateur", "global", { "src", "srcgain", "gate", "srcmute", "vrate", "vpitch", "dry", "ploop" }, "global" },
                    { "mix", "Modules", "filter", { "mvoc", "mcomp", "mtalk", "mbio", "mpoly", "level" }, "filter" },
                    { "detail", "R\xC3\xA9glages des modules", "filter", { "vformant", "crush", "crate", "talkshift", "vowel", "nasal", "bioshift", "polywidth" }, "filter" },
                    { "car", "Porteuse", "osc", { "cwave", "cpw", "coct", "cuni", "cdetune", "cbright", "tune" }, "osc" },
                    { "amp", "Ampli", "env", { "resp", "rel", "ca", "cr", "voices" }, "env" },
                };
                m.presets = {
                    { "Vocodeur moderne", { { "level", 0 } }, "Voix" },
                    { "Voix d'ordinateur", { { "mvoc", 20 }, { "mcomp", 90 }, { "mpoly", 0 }, { "level", -0.5f } }, "Effets" },
                    { "Bo\xC3\xAEte parlante funk", { { "mvoc", 0 }, { "mtalk", 100 }, { "mpoly", 10 }, { "cwave", 1 }, { "cpw", 35 }, { "level", -3 } }, "Voix" },
                    { "Voix mod\xC3\xA9lis\xC3\xA9" "e", { { "mvoc", 0 }, { "mbio", 100 }, { "vowel", 40 }, { "level", 4.5f } }, "Voix" },
                    { "Ch\xC5\x93ur \xC3\xA9pais", { { "mvoc", 70 }, { "mpoly", 90 }, { "cuni", 3 }, { "cdetune", 14 }, { "level", 0 } }, "Voix" },
                    { "Robot et bo\xC3\xAEte", { { "mvoc", 50 }, { "mtalk", 60 }, { "mcomp", 30 }, { "level", -1 } }, "Voix" },
                    { "Bits cass\xC3\xA9s", { { "mvoc", 0 }, { "mcomp", 100 }, { "crush", 3 }, { "crate", 12 }, { "level", -1.5f } }, "Effets" },
                    { "Nasale", { { "mvoc", 30 }, { "mbio", 80 }, { "nasal", 85 }, { "vowel", 70 }, { "level", 5 } }, "Effets" },
                    { "Tout ensemble", { { "mvoc", 60 }, { "mcomp", 25 }, { "mtalk", 40 }, { "mbio", 30 }, { "mpoly", 60 }, { "level", -1 } }, "Voix" },
                    { "Voix basse", { { "vformant", -6 }, { "talkshift", -5 }, { "coct", 0 }, { "mtalk", 40 }, { "level", 1 } }, "Voix" },
                };
            }

            for (auto& m : d)
            {
                addModHub (m, hub);
                // v0.33 : boucle de la prise de son (ajouté à la fin : positions des réglages inchangées)
                m.params.push_back (keep (C ("ploop", "Boucle de la prise", { "Longueur de la phrase", "1 mesure", "2 mesures", "4 mesures" }, 0)));
                addFactoryBank (m);
            }
            return d;
        }();
        return all[(size_t) std::clamp (model, 0, (int) numModels - 1)];
    }

    /** Positions des réglages d'un modèle (-1 : absent). */
    struct Idx
    {
        int src = -1, srcgain = -1, srcmute = -1, gate = -1, vrate = -1, vpitch = -1, vexpr = -1, dry = -1, ploop = -1;
        int voices = -1, cwave = -1, cpw = -1, coct = -1, tune = -1, cdetune = -1, cuni = -1, cfm = -1, creg = -1, cjitter = -1, cbright = -1, cnoise = -1, ca = -1, cr = -1;
        int resp = -1, rel = -1, shift = -1, hiss = -1, buzz = -1, direct = -1, directlvl = -1, hold = -1, level = -1;
        int bands = -1, formant = -1, freeze = -1, sib = -1, ensemble = -1, strings = -1;
        int algo = -1, expand = -1, traces = -1, stereo = -1, unvoiced = -1, hfthru = -1;
        int mvoc = -1, mcomp = -1, mtalk = -1, mbio = -1, mpoly = -1, vformant = -1, crush = -1, crate = -1, talkshift = -1, vowel = -1, nasal = -1, bioshift = -1, polywidth = -1;

        void bind (const ModuleDesc& d)
        {
            const std::pair<int*, const char*> all[] {
                { &src, "src" }, { &srcgain, "srcgain" }, { &srcmute, "srcmute" }, { &gate, "gate" }, { &vrate, "vrate" }, { &vpitch, "vpitch" }, { &vexpr, "vexpr" }, { &dry, "dry" }, { &ploop, "ploop" },
                { &voices, "voices" }, { &cwave, "cwave" }, { &cpw, "cpw" }, { &coct, "coct" }, { &tune, "tune" }, { &cdetune, "cdetune" }, { &cuni, "cuni" }, { &cfm, "cfm" },
                { &creg, "creg" }, { &cjitter, "cjitter" }, { &cbright, "cbright" }, { &cnoise, "cnoise" }, { &ca, "ca" }, { &cr, "cr" },
                { &resp, "resp" }, { &rel, "rel" }, { &shift, "shift" }, { &hiss, "hiss" }, { &buzz, "buzz" }, { &direct, "direct" }, { &directlvl, "directlvl" }, { &hold, "hold" }, { &level, "level" },
                { &bands, "bands" }, { &formant, "formant" }, { &freeze, "freeze" }, { &sib, "sib" }, { &ensemble, "ensemble" }, { &strings, "strings" },
                { &algo, "algo" }, { &expand, "expand" }, { &traces, "traces" }, { &stereo, "stereo" }, { &unvoiced, "unvoiced" }, { &hfthru, "hfthru" },
                { &mvoc, "mvoc" }, { &mcomp, "mcomp" }, { &mtalk, "mtalk" }, { &mbio, "mbio" }, { &mpoly, "mpoly" }, { &vformant, "vformant" }, { &crush, "crush" }, { &crate, "crate" },
                { &talkshift, "talkshift" }, { &vowel, "vowel" }, { &nasal, "nasal" }, { &bioshift, "bioshift" }, { &polywidth, "polywidth" },
            };
            for (const auto& [ptr, id] : all) *ptr = d.indexOf (id);
        }
    };

    inline float at (const float* p, int i, float def) noexcept { return i >= 0 ? p[i] : def; }
}

/** Voix de la porteuse : 1 à 4 oscillateurs à l'unisson, formes classiques, cordes (8' + 4' et
    registre), chœur, bruit ; FM linéaire ; instabilité ; brillance ; enveloppe attaque / relâchement. */
class VocCarrierVoice
{
public:
    void bindIdx (const voc::Idx* i) noexcept { ix = i; }
    void prepare (double s) { sr = s; env.prepare (s); reset(); }
    void reset() noexcept
    {
        for (auto& o : osc) o.reset();
        for (auto& o : osc4) o.reset();
        sub.reset(); fmOsc.reset(); lp.reset(); env.reset(); active = false; jit = 0.0; jitT = 0.0;
    }
    void start (int note, float velocity, bool legato, int) noexcept
    {
        current = note;
        if (velocity >= 0.0f) vel = velocity;
        if (! legato || ! active) env.gate (true);
        active = true;
        rng = rng * 1664525u + 1013904223u;
        // phases de départ différentes par oscillateur (unisson vivant, pas de phase commune)
        for (int i = 0; i < 4; ++i) { osc[(size_t) i].reset (0.23 * i + (double) (rng >> 24) / 512.0); osc4[(size_t) i].reset (0.37 * i); }
    }
    void setSpread (int, int) noexcept {}
    void release() noexcept { env.gate (false); }
    bool isActive() const noexcept { return active; }
    void seed (std::uint32_t s) noexcept { rng = s; }

    void render (float* L, float* R, int n, const float* p) noexcept
    {
        using voc::at;
        const auto& x = *ix;
        env.set (at (p, x.ca, 5.0f), 1.0, 1.0, at (p, x.cr, 250.0f));
        const auto wave = (int) at (p, x.cwave, 0.0f);
        const auto pw = at (p, x.cpw, 50.0f) / 100.0;
        const auto oct = 12.0 * (std::round (at (p, x.coct, 1.0f)) - 1.0);
        const auto uni = std::clamp ((int) at (p, x.cuni, 1.0f), 1, 4);
        const auto det = at (p, x.cdetune, 6.0f);
        const auto fm = at (p, x.cfm, 0.0f) / 100.0;
        const auto reg = at (p, x.creg, 35.0f) / 100.0;
        const auto jitAmt = at (p, x.cjitter, 0.0f) / 100.0;
        const auto bright = std::clamp ((double) at (p, x.cbright, 12000.0f), 200.0, 0.45 * sr);
        const auto noiseAmt = at (p, x.cnoise, 0.0f) / 100.0;
        // instabilité : marche lente au hasard (jusqu'à ±25 cents), nouvelle cible toutes les 60 ms
        jitClock += n;
        if (jitClock > (int) (0.06 * sr)) { jitClock = 0; rng = rng * 1664525u + 1013904223u; jitT = ((double) (rng >> 8) / 8388608.0 - 1.0) * 0.25 * jitAmt; }
        jit += (jitT - jit) * std::min (1.0, (double) n / (0.05 * sr));
        const auto base = (double) current + at (p, x.tune, 0.0f) + oct + jit;
        const auto gain = 0.55 / std::sqrt ((double) uni) * (0.6 + 0.4 * vel);

        for (int i = 0; i < n; ++i)
        {
            const auto e = env.next();
            double s = 0.0;
            const auto fmv = fm > 0.0 ? fmOsc.next (Osc::sine, noteHz (base) / sr, 0.5) * fm * 2.0 : 0.0;
            for (int u = 0; u < uni; ++u)
            {
                const auto spread = uni == 1 ? 0.0 : ((double) u / (uni - 1) - 0.5) * 2.0;
                const auto dt = noteHz (base + spread * det / 100.0) / sr * (1.0 + fmv);
                auto& o = osc[(size_t) u];
                switch (wave)
                {
                    case 1: s += o.next (Osc::pulse, dt, pw); break;
                    case 2: s += o.next (Osc::saw, dt, 0.5) + (u == 0 ? 0.6 * sub.next (Osc::pulse, dt * 0.5, 0.5) : 0.0); break;
                    case 3:
                    {
                        // cordes : scie 8' + scie 4' ; le registre passe de « violon » (8' seul,
                        // brillant) à « orgue » (4' fort, impulsion large plus ronde)
                        const auto a = o.next (Osc::saw, dt, 0.5);
                        const auto b = osc4[(size_t) u].next (reg > 0.6 ? Osc::pulse : Osc::saw, dt * 2.0, 0.5);
                        s += a * (1.0 - 0.5 * reg) + b * (0.25 + 0.6 * reg);
                        break;
                    }
                    case 4: s += 0.6 * o.next (Osc::pulse, dt, 0.25) + 0.45 * osc4[(size_t) u].next (Osc::saw, dt * 1.003, 0.5); break;
                    case 5: s += noise.next() * 0.8; break;
                    default: s += o.next (Osc::saw, dt, 0.5); break;
                }
            }
            s = s * gain + noise.next() * noiseAmt * 0.4;
            s = lp.lowpass (s, bright, sr);
            const auto out = (float) (s * e);
            L[i] += out;
            R[i] += out;
        }
        if (! env.isActive()) active = false;
    }

private:
    const voc::Idx* ix = nullptr;
    double sr = 48000.0, jit = 0.0, jitT = 0.0;
    int jitClock = 0, current = 60;
    float vel = 1.0f;
    bool active = false;
    std::uint32_t rng = 1;
    std::array<Osc, 4> osc, osc4;
    Osc sub, fmOsc;
    OnePole lp;
    Adsr env;
    WhiteNoise noise { 0x3C0FFEEu };
};

/** Les quatre vocodeurs : la porteuse (voix ci-dessus) est traitée en fin de bloc par le banc du modèle. */
class VocoderSynth final : public SynthBase<VocCarrierVoice, 8>
{
public:
    explicit VocoderSynth (int m) : model (std::clamp (m, 0, (int) voc::numModels - 1))
    {
        ix.bind (voc::desc (model));
        for (int i = 0; i < 8; ++i) { voices[(size_t) i].bindIdx (&ix); voices[(size_t) i].seed (0xB0C0u * (std::uint32_t) (i + 7)); }
    }

    const ModuleDesc& desc() const noexcept override { return voc::desc (model); }

    void prepare (double s, int block) override
    {
        SynthBase::prepare (s, block);
        bank.prepare (s); compBank.prepare (s); talkBank.prepare (s);
        uv.prepare (s); gate.prepare (s); demo.prepare (s);
        // prise de son : tampons réservés ici (jamais dans le rendu) ; une prise non gardée est perdue
        capBuf.assign ((size_t) (voc::captureSeconds * s) + 1, 0.0f);
        preBuf.assign ((size_t) (0.08 * s) + 1, 0.0f);
        preW = 0; capPos = 0; capLen.store (0); capState.store (0); playPos = 0.0;
        capEnvAtt = 1.0 - std::exp (-1.0 / (0.003 * s));
        capEnvRel = std::exp (-1.0 / (0.120 * s));
        chorus.prepare (s); dbl.prepare (s);
        dc.prepare (s); dcR.prepare (s);
        noteAtt = std::exp (-1.0 / (0.002 * s));
        consAtt = 1.0 - std::exp (-1.0 / (0.004 * s)); // 1.1 : ouverture des consonnes (~10 ms à 90 %)
        noteRel = std::exp (-1.0 / (0.080 * s));
        talkRel = std::exp (-1.0 / (0.040 * s));
        resetFx();
    }
    void reset() noexcept override { SynthBase::reset(); playPos = 0.0; bank.reset(); compBank.reset(); talkBank.reset(); uv.reset(); gate.reset(); demo.reset(); chorus.reset(); dbl.reset(); resetFx(); }

    void noteOn (int note, float velocity) noexcept override { lastNote = note; SynthBase::noteOn (note, velocity); }

    // --- v0.33 : phrase tapée, prise de son -------------------------------------------------------
    void setSpeech (const SpeechScript* sc) noexcept override { demo.setScript (sc); }
    void setPosition (double ppq, bool playing) noexcept override { blockPpq = ppq; transportOn = playing; }
    void transportStarted() noexcept override { SynthBase::transportStarted(); playPos = 0.0; demo.restartScript(); }
    void setSample (int index, const SampleBuffer* smp) noexcept override
    {
        if (index != 0) return;
        phrase = smp != nullptr && smp->data.size() > 16 ? smp : nullptr;
        // la prise vient d'être gardée : la lecture passe du tampon à l'échantillon (même position)
        if (phrase != nullptr && capState.load() == capSaving) capState.store (capNone);
    }
    /** 0 = armer / arrêter, 1 = arrêter, 2 = effacer la prise en cours (fil audio). */
    void recordAction (int a) noexcept override
    {
        const auto st = capState.load();
        if (st == capCopying) return;
        if (a == 2) { capState.store (capNone); capLen.store (0); return; }
        if (st == capRecording) { finishCapture(); return; }
        if (st == capArmed) { capState.store (capNone); return; } // appui annulé avant la voix
        if (a == 1) return;
        capPos = 0; capLen.store (0); capVoice = 0; preW = 0; capEnv = 0.0;
        std::fill (preBuf.begin(), preBuf.end(), 0.0f);
        capState.store (capArmed);
    }

    /** Prise terminée et pas encore gardée avec le projet (fil de l'interface). */
    bool captureToKeep() const noexcept { return capState.load() == capReady; }
    /** Copie de la prise (fil de l'interface) ; le fil audio ne réécrit pas le tampon pendant ce temps. */
    bool copyCapture (std::vector<float>& out, double& rate) const
    {
        int expected = capReady;
        if (! capState.compare_exchange_strong (expected, capCopying)) return false;
        const auto len = (size_t) capLen.load();
        const bool ok = len > 16 && len <= capBuf.size();
        if (ok) { out.assign (capBuf.begin(), capBuf.begin() + (long) len); rate = sr; }
        capState.store (ok ? capSaving : capNone);
        return ok;
    }
    /** La prise n'a pas pu être gardée : elle continue de tourner, sans nouvel essai. */
    void captureKeepFailed() const noexcept { int e = capSaving; capState.compare_exchange_strong (e, capUnsaved); }

    /** Blocs de plus de 64 : la base les découpe ; le modulateur est lu au même endroit. */
    void render (float* L, float* R, int n, const float* p) noexcept override
    {
        for (int o = 0; o < n; o += voiceBlock)
        {
            modOffset = o;
            SynthBase::render (L + o, R + o, std::min (voiceBlock, n - o), p);
        }
        modOffset = 0;
    }

    void setInput (const float* l, const float* r) noexcept override { inL = l; inR = r; }
    void setNeighbor (const float* l, const float* r) noexcept override { nbL = l; nbR = r; }
    void setSidechainSources (const float* const* l, const float* const* r) noexcept override { scL = l; scR = r; }
    bool absorbsNeighbor (const float* p) const noexcept override
    {
        return (int) voc::at (p, ix.src, 0.0f) == voc::srcPrevious && voc::at (p, ix.srcmute, 1.0f) > 0.5f;
    }

    // la voix interne, le gel et les traînes sonnent sans note : l'instrument reste actif
    bool isActive() const noexcept override { return true; }

    int status (float* out, int max) const noexcept override
    {
        // [bandes, part non voisée, niveau de chaque bande, puis (v0.33) état de la prise, secondes
        //  enregistrées, avancement de la lecture, phrase présente, mot prononcé de la phrase tapée,
        //  crête de l'entrée pendant la prise]
        const auto nb = std::min (shownBands.load (std::memory_order_relaxed), max - 8);
        if (nb <= 0) return 0;
        out[0] = (float) nb;
        out[1] = uvShown.load (std::memory_order_relaxed);
        for (int k = 0; k < nb; ++k) out[2 + k] = shownLevel[(size_t) k].load (std::memory_order_relaxed);
        out[2 + nb] = (float) capState.load (std::memory_order_relaxed);
        out[3 + nb] = (float) ((double) capLen.load (std::memory_order_relaxed) / std::max (1.0, sr));
        out[4 + nb] = playShown.load (std::memory_order_relaxed);
        out[5 + nb] = hasPhraseShown.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        out[6 + nb] = (float) wordShown.load (std::memory_order_relaxed);
        out[7 + nb] = inPeakShown.load (std::memory_order_relaxed); // crête de l'entrée pendant la prise (0 : rien n'arrive)
        return nb + 8;
    }

protected:
    void configure (const float* p) noexcept override
    {
        setMode (false, true, std::clamp ((int) voc::at (p, ix.voices, 8.0f), 1, 8));
    }

    void postProcess (float* L, float* R, int n, const float* p) noexcept override
    {
        using voc::at;
        // --- le modulateur -----------------------------------------------------------------
        float mod[voiceBlock];
        readModulator (mod, n, p);
        const auto dryAmt = at (p, ix.dry, 0.0f) / 100.0;
        const auto outGain = dbToGain (at (p, ix.level, -6.0f));

        // --- banc du modèle ------------------------------------------------------------------
        VocoderBank::Config c;
        c.attackMs = at (p, ix.resp, 2.0f);
        c.releaseMs = at (p, ix.rel, 40.0f);
        double hiss = 0.0, buzz = 0.0, hf = 0.0, unv = 0.0, strDry = 0.0;
        switch (model)
        {
            case voc::m16:
                c.bands = 16; c.fLo = 50.0; c.fHi = 5080.0; c.order = 2;
                c.bandShift = (int) at (p, ix.shift, 0.0f);
                c.freeze = at (p, ix.hold, 0.0f) > 0.5f;
                hiss = at (p, ix.hiss, 45.0f) / 100.0;
                buzz = at (p, ix.buzz, 0.0f) / 100.0;
                hf = at (p, ix.direct, 1.0f) > 0.5f ? at (p, ix.directlvl, 40.0f) / 100.0 : 0.0;
                break;
            case voc::strings:
            {
                static constexpr int counts[4] { 16, 24, 32, 40 };
                c.bands = counts[std::clamp ((int) at (p, ix.bands, 2.0f), 0, 3)];
                c.fLo = 70.0; c.fHi = 9500.0; c.order = 2; c.trimDb = 2.0;
                c.formant = at (p, ix.formant, 0.0f);
                c.freeze = at (p, ix.freeze, 0.0f) > 0.5f;
                unv = at (p, ix.sib, 50.0f) / 100.0;
                hf = unv * 0.5;
                strDry = at (p, ix.strings, 0.0f) / 100.0;
                break;
            }
            case voc::spectral:
            {
                const auto& a = voc::algos[std::clamp ((int) at (p, ix.algo, 1.0f), 0, 7)];
                c.bands = a.bands; c.fLo = a.fLo; c.fHi = a.fHi; c.order = a.order; c.qScale = a.q; c.invert = a.invert; c.trimDb = a.trimDb;
                c.attackMs *= a.attMul; c.releaseMs *= a.relMul;
                c.formant = at (p, ix.formant, 0.0f);
                c.expansion = at (p, ix.expand, 0.0f) / 50.0;
                c.traces = at (p, ix.traces, 0.0f) / 100.0;
                c.stereo = at (p, ix.stereo, 40.0f) / 100.0;
                c.freeze = at (p, ix.freeze, 0.0f) > 0.5f;
                unv = at (p, ix.unvoiced, 50.0f) / 100.0;
                hf = at (p, ix.hfthru, 30.0f) / 100.0;
                break;
            }
            default:
                c.bands = 12; c.fLo = 100.0; c.fHi = 7000.0; c.order = 2; c.trimDb = 2.0;
                c.formant = at (p, ix.vformant, 0.0f);
                c.stereo = 0.3;
                hf = 0.2; unv = 0.4;
                break;
        }
        bank.setConfig (c);
        if (model == voc::vsynth) configureVsynth (p, c);

        // --- échantillon par échantillon ----------------------------------------------------
        const auto buzzDt = noteHz ((double) (lastNote >= 0 ? lastNote : 45)) / sr;
        const auto mixVoc = model == voc::vsynth ? at (p, ix.mvoc, 80.0f) / 100.0 : 1.0;
        // les aigus directs, le souffle et la voix interne entendue en direct suivent le niveau de
        // la porteuse : sans note (ou en fin de relâchement), la piste se tait — la voix interne,
        // elle, parle en continu
        const auto srcNow = (int) at (p, ix.src, 0.0f);
        const bool internal = srcNow == voc::srcInternal || (srcNow == voc::srcCapture && capState.load (std::memory_order_relaxed) != capArmed
                                                             && capState.load (std::memory_order_relaxed) != capRecording);
        // porteuse muette depuis un moment (aucune note) : seule l'analyse tourne (barres de
        // l'écran, enveloppes prêtes), la synthèse et les modules sont sautés (moitié du coût)
        double carMax = 0.0;
        for (int i = 0; i < n; ++i) carMax = std::max (carMax, std::abs ((double) L[i] + (double) R[i]));
        if (carMax < 1.0e-7 && carEnv < 1.0e-6)
        {
            if (! idle) { idle = true; bank.resetSynth(); bank.settle(); resetModules(); }
            for (int i = 0; i < n; ++i)
            {
                carEnv = 0.0; noteEnv = 0.0; consEnv = 0.0;
                const double m = mod[i];
                uv.process (m);
                bank.analyse (m);
                if ((shapeTick++ & 3) == 0) bank.shape();
                const auto dm = internal ? 0.0 : m * dryAmt;
                L[i] = (float) dm; R[i] = (float) dm;
            }
        }
        else idle = false;
        for (int i = 0; i < n && ! idle; ++i)
        {
            const auto ca = 0.5 * std::abs ((double) L[i] + (double) R[i]);
            carEnv = ca > carEnv ? ca + (carEnv - ca) * noteAtt : carEnv * noteRel;
            noteEnv = std::min (1.0, carEnv * 6.0);
            // 1.1 : souffle, consonnes et aigus directs s'ouvrent en ~10 ms au début d'une note (une note
            // qui part pendant une consonne de la voix ne claque plus) et se referment avec la note
            consEnv = noteEnv > consEnv ? consEnv + (noteEnv - consEnv) * consAtt : noteEnv;
            const double m = mod[i];
            const auto u = uv.process (m);
            bank.analyse (m);
            if ((shapeTick++ & 3) == 0) bank.shape(); // enveloppes mises en forme tous les 4 échantillons (0,08 ms)
            // porteuse : carrier mono, + souffle (consonnes) et bourdon (voyelles) du 16 bandes,
            // + consonnes synthétisées (bruit) des autres modèles
            double car = 0.5 * ((double) L[i] + (double) R[i]);
            const auto voiced = std::min (1.0, gate.level() * 8.0);
            // (souffle, bourdon et consonnes ne sonnent que pendant le jeu, comme la porteuse)
            if (hiss > 0.0) car += noise.next() * hiss * u * 0.4 * consEnv; // 1.1 : -6 dB (consonnes moins saillantes)
            if (buzz > 0.0) car += buzzOsc.next (Osc::pulse, buzzDt, 0.2) * buzz * (1.0 - u) * voiced * 0.5 * noteEnv;
            if (unv > 0.0) car += noise.next() * unv * u * 0.35 * consEnv; // 1.1 : -6 dB
            double l = 0.0, r = 0.0;
            if (mixVoc > 0.0) { bank.synth (car, l, r); vocOn = true; }
            else if (vocOn) { bank.resetSynth(); vocOn = false; }
            l *= mixVoc; r *= mixVoc;
            if (model == voc::vsynth) vsynthModules (m, car, l, r);
            // aigus directs : la voix au-dessus de la dernière bande (« s », « t » intelligibles)
            if (hf <= 0.0) { if (hfOn) { hfA.reset(); hfB.reset(); hfOn = false; } }
            else
            {
                hfOn = true;
                // 1.1 : au niveau propre de la voix (le gain du modulateur règle la sensibilité de
                // l'analyse, pas le volume des consonnes laissées passer) et ouvert en douceur au
                // début d'une note (consEnv : ~10 ms), sans coup sec
                const auto h = hfB.highpass (hfA.highpass (m, bank.config().fHi, sr), bank.config().fHi, sr) * hf * 0.7 * consEnv / std::max (1.0, modGainNow); // 1.1 : -6 dB
                l += h; r += h;
            }
            if (strDry > 0.0) { l += car * strDry * 0.8; r += car * strDry * 0.8; }
            const auto dm = m * dryAmt * (internal ? noteEnv : 1.0);
            l += dm; r += dm;
            L[i] = (float) l; R[i] = (float) r;
        }

        // --- effets propres au modèle ---------------------------------------------------------
        if (model == voc::strings)
        {
            const auto ens = at (p, ix.ensemble, 60.0f) / 100.0;
            if (ens <= 0.001) { if (ensOn) { chorus.reset(); ensOn = false; } } // éteint : la mémoire des lignes est vidée
            else { ensOn = true; chorus.process (L, R, n, 0.55 + 5.5 * ens * 0.1, 2.2 * ens + 0.3, 6.0, std::min (1.0, ens * 1.2), 9000.0); }
        }
        if (model == voc::vsynth)
        {
            const auto thick = at (p, ix.mpoly, 25.0f) / 100.0;
            if (thick <= 0.001) { if (dblOn) { dbl.reset(); dblOn = false; } }
            else { dblOn = true; dbl.process (L, R, n, 0.35, 1.8 + 3.0 * at (p, ix.polywidth, 70.0f) / 100.0, 14.0, thick, 12000.0); }
        }
        for (int i = 0; i < n; ++i)
        {
            L[i] = (float) dc.process ((double) L[i] * outGain);
            R[i] = (float) dcR.process ((double) R[i] * outGain);
        }

        // --- affichage ------------------------------------------------------------------------
        bank.updateShown();
        const auto nb = bank.bands();
        for (int k = 0; k < nb; ++k) shownLevel[(size_t) k].store (bank.shownLevel (k), std::memory_order_relaxed);
        shownBands.store (nb, std::memory_order_relaxed);
        uvShown.store ((float) uv.value(), std::memory_order_relaxed);
    }

private:
    /** Porteuse muette : modules et aigus directs repartent de zéro au prochain son. */
    void resetModules() noexcept
    {
        compBank.reset(); talkBank.reset();
        for (auto& f : talk) f.reset();
        for (auto& f : bio) f.reset();
        nasalNotch.reset(); hfA.reset(); hfB.reset();
        crushHold = 0.0; talkEnv = bioEnv = 0.0;
    }

    void resetFx() noexcept
    {
        for (auto& f : talk) f.reset();
        for (auto& f : bio) f.reset();
        hfA.reset(); hfB.reset(); dc.reset(); dcR.reset(); nasalNotch.reset();
        crushHold = 0.0; crushCount = 0; carEnv = noteEnv = consEnv = 0.0; talkF.fill (0.0); talkG.fill (0.0); bioEnv = 0.0; talkEnv = 0.0;
        buzzOsc.reset();
    }

    /** Le modulateur du bloc : voix interne, entrée, piste précédente, une piste ou la prise de son ;
        gain et porte. */
    void readModulator (float* mod, int n, const float* p) noexcept
    {
        using voc::at;
        const auto src = (int) at (p, ix.src, 0.0f);
        const float* a = nullptr;
        const float* b = nullptr;
        if (src == voc::srcInput || src == voc::srcCapture) { a = inL; b = inR; }
        else if (src == voc::srcPrevious) { a = nbL; b = nbR; }
        else if (src >= voc::srcTrack1 && src < voc::srcCapture && scL != nullptr && scR != nullptr)
        {
            const auto t = src - voc::srcTrack1;
            if (t >= 0 && t < numTracks) { a = scL[t]; b = scR[t]; }
        }
        const auto g = dbToGain (at (p, ix.srcgain, 0.0f));
        modGainNow = g; // 1.1 : les aigus directs gardent le niveau propre de la voix
        const auto gdb = at (p, ix.gate, -60.0f);
        const auto thr = gdb <= -89.9f ? 0.0 : dbToGain (gdb);
        const auto rate = at (p, ix.vrate, 2.5f), pitch = at (p, ix.vpitch, 125.0f), expr = at (p, ix.vexpr, 100.0f) / 100.0f;
        if (src == voc::srcCapture) { readCapture (mod, n, p, a, b, g, thr); return; }
        playShown.store (0.0f, std::memory_order_relaxed);
        hasPhraseShown.store (false, std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            double x;
            if (src == voc::srcInternal) x = demo.next (rate, pitch, expr);
            else if (a != nullptr) x = 0.5 * ((double) a[modOffset + i] + (double) (b != nullptr ? b[modOffset + i] : a[modOffset + i]));
            else x = 0.0;
            // 1.1 : gain du modulateur puis écrêtage DOUX au-delà de 2 (borné à 4) ; l'ancien écrêtage dur
            // (gain fort sur la voix interne) fabriquait des aigus parasites qui ouvraient les bandes hautes
            // et le souffle : des « tchak » sur certaines syllabes
            x *= g;
            if (const auto ax = std::abs (x); ax > 2.0) x = std::copysign (2.0 + 2.0 * std::tanh ((ax - 2.0) * 0.5), x);
            if (! std::isfinite (x)) x = 0.0;
            mod[i] = (float) gate.process (x, src == voc::srcInternal ? 0.0 : thr);
        }
        wordShown.store (src == voc::srcInternal ? demo.currentWord() : -1, std::memory_order_relaxed);
    }

    // --- Prise de son (v0.33) -----------------------------------------------------------------------
    enum CapState : int { capNone = 0, capArmed, capRecording, capReady, capCopying, capSaving, capUnsaved };

    /** Armée ou en cours : la voix du micro passe telle quelle (le vocodeur répond pendant la prise).
        Sinon : la phrase tourne (tampon de la prise tant qu'elle n'est pas gardée, puis l'échantillon). */
    void readCapture (float* mod, int n, const float* p, const float* a, const float* b, double g, double thr) noexcept
    {
        using voc::at;
        const auto st = capState.load (std::memory_order_relaxed);
        if (st == capArmed || st == capRecording)
        {
            // départ à partir de -45 dB (ou de la porte de bruit si elle est plus haute)
            const auto startThr = std::max (thr, 0.0056);
            auto peak = (double) inPeakShown.load (std::memory_order_relaxed) * 0.97;
            for (int i = 0; i < n; ++i)
            {
                double x = a != nullptr ? 0.5 * ((double) a[modOffset + i] + (double) (b != nullptr ? b[modOffset + i] : a[modOffset + i])) : 0.0;
                if (! std::isfinite (x)) x = 0.0;
                x = std::clamp (x, -4.0, 4.0);
                peak = std::max (peak, std::abs (x));
                captureSample (x, startThr);
                mod[i] = (float) gate.process (std::clamp (x * g, -4.0, 4.0), thr);
            }
            inPeakShown.store ((float) std::min (4.0, peak), std::memory_order_relaxed);
            playShown.store (0.0f, std::memory_order_relaxed);
            hasPhraseShown.store (false, std::memory_order_relaxed);
            wordShown.store (-1, std::memory_order_relaxed);
            return;
        }

        // lecture : tampon de la prise (pas encore gardée) ou échantillon gardé avec le projet
        const float* dataL = nullptr;
        const float* dataR = nullptr;
        int len = 0;
        double rate = sr;
        if (st == capReady || st == capCopying || st == capSaving || st == capUnsaved) { dataL = capBuf.data(); len = capLen.load (std::memory_order_relaxed); }
        else if (phrase != nullptr)
        {
            dataL = phrase->data.data();
            dataR = phrase->right.size() == phrase->data.size() ? phrase->right.data() : nullptr;
            len = (int) phrase->data.size();
            rate = phrase->sampleRate;
        }
        hasPhraseShown.store (dataL != nullptr && len > 16, std::memory_order_relaxed);
        wordShown.store (-1, std::memory_order_relaxed);
        if (dataL == nullptr || len <= 16)
        {
            for (int i = 0; i < n; ++i) mod[i] = (float) gate.process (0.0, 0.0);
            playShown.store (0.0f, std::memory_order_relaxed);
            return;
        }
        const auto step = rate / sr;                         // échantillons de la phrase par échantillon rendu
        const auto phraseOut = (double) len / step;          // durée de la phrase (échantillons rendus)
        static constexpr int barsFor[4] { 0, 1, 2, 4 };
        const auto bars = barsFor[std::clamp ((int) at (p, ix.ploop, 0.0f), 0, 3)];
        const auto beatSamples = 60.0 / std::max (20.0, bpm) * sr;
        // libre : la phrase et un quart de seconde de silence ; mesures : 4 temps par mesure
        const auto loopLen = bars == 0 ? phraseOut + 0.25 * sr : (double) bars * 4.0 * beatSamples;
        const bool synced = bars > 0 && transportOn;
        double pos = 0.0;
        for (int i = 0; i < n; ++i)
        {
            if (synced)
            {
                // calée sur le séquenceur : la phrase repart au début de chaque groupe de mesures
                const auto beats = blockPpq + (double) (modOffset + i) / beatSamples;
                const auto span = (double) bars * 4.0;
                pos = std::fmod (std::max (0.0, beats), span) * beatSamples;
            }
            else
            {
                pos = playPos;
                playPos += 1.0;
                if (playPos >= loopLen) playPos -= loopLen;
            }
            double x = 0.0;
            const auto fi = pos * step;
            const auto i0 = (int) fi;
            if (i0 >= 0 && i0 + 1 < len)
            {
                const auto fr = fi - (double) i0;
                auto s0 = (double) dataL[i0], s1 = (double) dataL[i0 + 1];
                if (dataR != nullptr) { s0 = 0.5 * (s0 + (double) dataR[i0]); s1 = 0.5 * (s1 + (double) dataR[i0 + 1]); }
                x = s0 + (s1 - s0) * fr;
            }
            x = std::clamp (x * g, -4.0, 4.0);
            if (! std::isfinite (x)) x = 0.0;
            mod[i] = (float) gate.process (x, thr);
        }
        if (synced) playPos = pos; // reprise libre au même endroit si le séquenceur s'arrête
        playShown.store ((float) std::clamp (pos / std::max (1.0, phraseOut), 0.0, 1.0), std::memory_order_relaxed);
    }

    /** Un échantillon du micro pendant la prise : attente de la voix, enregistrement, fin seule. */
    void captureSample (double x, double startThr) noexcept
    {
        const auto ax = std::abs (x);
        capEnv = ax > capEnv ? capEnv + (ax - capEnv) * capEnvAtt : capEnv * capEnvRel;
        const auto st = capState.load (std::memory_order_relaxed);
        if (st == capArmed)
        {
            if (preBuf.empty()) return;
            preBuf[(size_t) preW] = (float) x;
            if (++preW >= (int) preBuf.size()) preW = 0;
            if (capEnv < startThr) return;
            // la voix arrive : les 80 ms gardées en mémoire ouvrent la prise (première consonne entière)
            const auto pn = (int) preBuf.size();
            for (int k = 0; k < pn && k < (int) capBuf.size(); ++k) capBuf[(size_t) k] = preBuf[(size_t) ((preW + k) % pn)];
            capPos = std::min (pn, (int) capBuf.size());
            capVoice = capPos;
            capLen.store (capPos, std::memory_order_relaxed);
            capState.store (capRecording);
            return;
        }
        if (st != capRecording) return; // prise finie au milieu du bloc : la suite n'est plus enregistrée
        if (capPos >= (int) capBuf.size()) { finishCapture(); return; }
        capBuf[(size_t) capPos++] = (float) x;
        if (ax >= startThr) capVoice = capPos; // dernier instant où la voix dépasse le seuil (fin précise)
        capLen.store (capPos, std::memory_order_relaxed);
        // 0,9 s de silence après au moins 0,4 s de prise : la phrase est dite
        if (capPos > (int) (0.4 * sr) && capPos - capVoice > (int) (0.9 * sr)) finishCapture();
    }

    /** Fin de la prise : silence final retiré (120 ms gardées), fondus de 5 et 15 ms, lecture au début. */
    void finishCapture() noexcept
    {
        if (capState.load() != capRecording) return;
        auto len = std::min (capPos, capVoice + (int) (0.12 * sr));
        if (len < (int) (0.15 * sr)) { capState.store (capNone); capLen.store (0); return; } // rien de dit
        const auto fadeIn = std::min (len / 4, (int) (0.005 * sr));
        const auto fadeOut = std::min (len / 4, (int) (0.015 * sr));
        for (int k = 0; k < fadeIn; ++k) capBuf[(size_t) k] *= (float) k / (float) fadeIn;
        for (int k = 0; k < fadeOut; ++k) capBuf[(size_t) (len - 1 - k)] *= (float) k / (float) fadeOut;
        capLen.store (len);
        playPos = 0.0;
        capState.store (capReady);
    }

    // --- Voix de synthèse : modules ---------------------------------------------------------------
    void configureVsynth (const float* p, const VocoderBank::Config& base) noexcept
    {
        using voc::at;
        // ordinateur : 6 bandes larges
        VocoderBank::Config cc = base;
        cc.bands = 6; cc.fLo = 200.0; cc.fHi = 4000.0; cc.order = 1; cc.qScale = 0.8; cc.formant = 0.0; cc.stereo = 0.0; cc.trimDb = -0.7;
        compBank.setConfig (cc);
        crushBits = std::clamp ((int) at (p, ix.crush, 5.0f), 2, 12);
        crushRate = std::clamp ((int) at (p, ix.crate, 6.0f), 1, 24);
        // boîte parlante : banc d'analyse de 24 bandes (300 Hz à 4 kHz) pour suivre trois formants
        VocoderBank::Config tc = base;
        tc.bands = 24; tc.fLo = 200.0; tc.fHi = 4200.0; tc.order = 1; tc.qScale = 1.6; tc.formant = 0.0; tc.attackMs = 4.0; tc.releaseMs = 40.0;
        talkBank.setConfig (tc);
        mComp = at (p, ix.mcomp, 0.0f) / 100.0;
        mTalk = at (p, ix.mtalk, 0.0f) / 100.0;
        mBio = at (p, ix.mbio, 0.0f) / 100.0;
        talkShift = std::exp2 (at (p, ix.talkshift, 0.0f) / 12.0);
        // voix modélisée : trois formants de la voyelle choisie (A, È, I, O, OU), décalés
        static constexpr double vw[5][3] { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
        const auto pos = std::clamp ((double) at (p, ix.vowel, 25.0f) / 100.0, 0.0, 1.0) * 4.0;
        const auto i0 = std::min (3, (int) pos);
        const auto f = pos - i0;
        const auto bs = std::exp2 (at (p, ix.bioshift, 0.0f) / 12.0);
        for (int k = 0; k < 3; ++k)
            bio[(size_t) k].set ((vw[i0][k] + (vw[i0 + 1][k] - vw[i0][k]) * f) * bs, 7.0 + 3.0 * k, sr);
        nasal = at (p, ix.nasal, 20.0f) / 100.0;
        nasalNotch.set (1000.0 * bs, 1.2, sr);
        // suivi des formants de la boîte : toutes les 32 échantillons (0,7 ms)
    }

    void vsynthModules (double m, double car, double& l, double& r) noexcept
    {
        // ordinateur : vocodeur 6 bandes sur la porteuse, puis réduction de bits et de fréquence
        // un module éteint n'est pas calculé : ses filtres repartent à zéro (sinon une résonance
        // restée en mémoire ressortirait quand on le rallume)
        if (mComp <= 0.0) { if (compOn) { compBank.reset(); crushHold = 0.0; compOn = false; } }
        else
        {
            compOn = true;
            compBank.analyse (m);
            if ((shapeTick & 3) == 1) compBank.shape();
            double cl = 0.0, cr = 0.0;
            const auto sq = car > 1.0e-6 ? 0.6 : (car < -1.0e-6 ? -0.6 : 0.0);
            compBank.synth (sq * noteEnv, cl, cr); // porteuse rendue carrée : timbre d'ordinateur
            if (++crushCount >= crushRate) { crushCount = 0; const auto q = std::exp2 ((double) crushBits - 1.0); crushHold = std::round (cl * q) / q; }
            l += crushHold * mComp; r += crushHold * mComp;
        }
        // boîte parlante : trois résonances qui suivent les formants de la voix
        if (mTalk <= 0.0) { if (talkOn) { talkBank.reset(); for (auto& f : talk) f.reset(); talkEnv = 0.0; talkOn = false; } }
        else
        {
            talkOn = true;
            talkBank.analyse (m);
            talkEnv = std::max (std::abs (m), talkEnv * talkRel);
            if ((talkTick++ & 31) == 0) trackFormants();
            double y = 0.0;
            for (int k = 0; k < 3; ++k) y += talk[(size_t) k].process (car).band / 9.0 * talkG[(size_t) k];
            y *= std::min (1.0, talkEnv * 6.0) * 0.53; // niveau mesuré : comme le vocodeur
            l += y * mTalk; r += y * mTalk;
        }
        // voix modélisée : voyelle fixe sur la porteuse, nasalité, ouverte par le niveau de la voix
        if (mBio <= 0.0) { if (bioOn) { for (auto& f : bio) f.reset(); nasalNotch.reset(); bioEnv = 0.0; bioOn = false; } }
        else
        {
            bioOn = true;
            bioEnv = std::max (std::abs (m), bioEnv * talkRel);
            auto y = bio[0].process (car).band / 7.0 + bio[1].process (car).band * 0.6 / 10.0 + bio[2].process (car).band * 0.35 / 13.0;
            const auto o = nasalNotch.process (y);
            y = y * (1.0 - nasal) + (o.low + o.high) * nasal + o.low * nasal * 0.5; // creux vers 1 kHz, grave renforcé
            y *= std::min (1.0, bioEnv * 6.0) * 0.5;
            l += y * mBio; r += y * mBio;
        }
    }

    /** Trois maxima locaux du spectre de la voix → fréquences et gains des résonances (lissés). */
    void trackFormants() noexcept
    {
        const auto nb = talkBank.bands();
        std::array<int, 3> top { -1, -1, -1 };
        std::array<double, 3> lvl { 0.0, 0.0, 0.0 };
        for (int k = 1; k < nb - 1; ++k)
        {
            const auto e = talkBank.envelope (k);
            if (e < talkBank.envelope (k - 1) || e < talkBank.envelope (k + 1)) continue;
            for (int s = 0; s < 3; ++s)
                if (e > lvl[(size_t) s])
                {
                    for (int t = 2; t > s; --t) { lvl[(size_t) t] = lvl[(size_t) (t - 1)]; top[(size_t) t] = top[(size_t) (t - 1)]; }
                    lvl[(size_t) s] = e; top[(size_t) s] = k;
                    break;
                }
        }
        // rangés du plus grave au plus aigu : F1, F2, F3
        std::array<std::pair<double, double>, 3> fs {};
        static constexpr double fallback[3] { 600.0, 1400.0, 2500.0 };
        for (int s = 0; s < 3; ++s) fs[(size_t) s] = top[(size_t) s] >= 0 ? std::make_pair (talkBank.bandFreq (top[(size_t) s]), lvl[(size_t) s]) : std::make_pair (fallback[s], 0.0);
        std::sort (fs.begin(), fs.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        const auto norm = 1.0 / (std::max ({ fs[0].second, fs[1].second, fs[2].second }) + 1.0e-6);
        for (int s = 0; s < 3; ++s)
        {
            const auto target = std::log2 (fs[(size_t) s].first * talkShift);
            auto& f = talkF[(size_t) s];
            f = f <= 0.0 ? target : f + (target - f) * 0.25;
            talkG[(size_t) s] += (fs[(size_t) s].second * norm - talkG[(size_t) s]) * 0.25;
            talk[(size_t) s].set (std::exp2 (f), 9.0, sr);
        }
    }

    int model;
    voc::Idx ix;
    VocoderBank bank, compBank, talkBank;
    UnvoicedDetector uv;
    ModGate gate;
    DemoVoice demo;
    BbdChorus chorus, dbl;
    OnePole hfA, hfB;
    DcBlocker dc, dcR;
    WhiteNoise noise { 0xACE1u };
    Osc buzzOsc;
    std::array<grv::dsp::Svf, 3> talk, bio;
    grv::dsp::Svf nasalNotch;
    std::array<double, 3> talkF {}, talkG {};
    double mComp = 0.0, mTalk = 0.0, mBio = 0.0, talkShift = 1.0, nasal = 0.0, crushHold = 0.0, bioEnv = 0.0, talkEnv = 0.0;
    double talkRel = 0.9995, noteEnv = 0.0, carEnv = 0.0, noteAtt = 0.99, noteRel = 0.999;
    double consEnv = 0.0, consAtt = 0.005, modGainNow = 1.0; // 1.1
    bool compOn = false, talkOn = false, bioOn = false, vocOn = false, ensOn = false, dblOn = false, idle = false, hfOn = false;
    int modOffset = 0;
    unsigned shapeTick = 0;
    int crushBits = 5, crushRate = 6, crushCount = 0, talkTick = 0, lastNote = -1;
    const float* inL = nullptr;
    const float* inR = nullptr;
    const float* nbL = nullptr;
    const float* nbR = nullptr;
    const float* const* scL = nullptr;
    const float* const* scR = nullptr;
    // v0.33 : prise de son et phrase gardée (emplacement 1 de la piste)
    std::vector<float> capBuf, preBuf;
    mutable std::atomic<int> capState { 0 };
    std::atomic<int> capLen { 0 };
    int capPos = 0, capVoice = 0, preW = 0;
    double capEnv = 0.0, capEnvAtt = 0.01, capEnvRel = 0.999, playPos = 0.0, blockPpq = 0.0;
    bool transportOn = false;
    const SampleBuffer* phrase = nullptr;
    std::atomic<float> playShown { 0.0f };
    std::atomic<bool> hasPhraseShown { false };
    std::atomic<int> wordShown { -1 };
    std::atomic<float> inPeakShown { 0.0f };
    std::array<std::atomic<float>, VocoderBank::maxBands> shownLevel {};
    std::atomic<int> shownBands { 0 };
    std::atomic<float> uvShown { 0.0f };
};

} // namespace grv::instruments
