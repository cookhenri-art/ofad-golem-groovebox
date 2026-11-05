/*
    Groovebox — OFAD — AGPL-3.0-or-later

    v0.47 (maj. 5 nov.) : réglages d'usine supplémentaires des effets (au moins 20 par effet), construits sur les usages
    courants de chaque famille d'effet ; aucun préréglage d'un autre fabricant n'est recopié.

    FICHIER ÉCRIT PAR tools/presets/fxgen.py : modifier tools/presets/fx_banks.py, pas ici.
*/

#pragma once

#include "../engine/Params.h"

#include <string>
#include <vector>

namespace grv::effects
{

/** Ajoute les réglages d'usine de la banque à la description de l'effet (fin de sa description). */
inline void addFxBank (grv::engine::ModuleDesc& m)
{
    if (m.type == "eq")
    {
        m.presets.push_back ({ "Basse : rondeur et attaque", { { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 35 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b3_on", 1 }, { "b3_type", 0 }, { "b3_freq", 90 }, { "b3_gain", 3 }, { "b3_q", 1.2f }, { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 300 }, { "b4_gain", -3 }, { "b4_q", 1.4f }, { "b6_on", 1 }, { "b6_type", 0 }, { "b6_freq", 1800 }, { "b6_gain", 3 }, { "b6_q", 1.5f } } });
        m.presets.push_back ({ "Synth\xC3\xA9 : enlever la boue", { { "b1_on", 1 }, { "b1_type", 4 }, { "b1_freq", 120 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 350 }, { "b4_gain", -4 }, { "b4_q", 1.2f }, { "b7_on", 1 }, { "b7_type", 2 }, { "b7_freq", 9000 }, { "b7_gain", 2 }, { "b7_q", 0.9f } } });
        m.presets.push_back ({ "Charley : air", { { "b1_on", 1 }, { "b1_type", 4 }, { "b1_freq", 400 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b7_on", 1 }, { "b7_type", 2 }, { "b7_freq", 11000 }, { "b7_gain", 4 }, { "b7_q", 0.9f }, { "b5_on", 1 }, { "b5_type", 0 }, { "b5_freq", 2500 }, { "b5_gain", -2 }, { "b5_q", 2 } } });
        m.presets.push_back ({ "Toms : corps", { { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 50 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b3_on", 1 }, { "b3_type", 0 }, { "b3_freq", 120 }, { "b3_gain", 3 }, { "b3_q", 1.5f }, { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 450 }, { "b4_gain", -4 }, { "b4_q", 2 }, { "b6_on", 1 }, { "b6_type", 0 }, { "b6_freq", 4000 }, { "b6_gain", 3 }, { "b6_q", 1.4f } } });
        m.presets.push_back ({ "Radio AM", { { "b1_on", 1 }, { "b1_type", 4 }, { "b1_freq", 500 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b8_on", 1 }, { "b8_type", 6 }, { "b8_freq", 3500 }, { "b8_gain", 0 }, { "b8_q", 0.9f }, { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 1500 }, { "b4_gain", 6 }, { "b4_q", 1.2f } } });
        m.presets.push_back ({ "M\xC3\xA9gaphone", { { "b1_on", 1 }, { "b1_type", 9 }, { "b1_freq", 700 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b8_on", 1 }, { "b8_type", 11 }, { "b8_freq", 4000 }, { "b8_gain", 0 }, { "b8_q", 0.9f }, { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 1800 }, { "b4_gain", 9 }, { "b4_q", 2 } } });
        m.presets.push_back ({ "Creux du m\xC3\xA9" "dium", { { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 800 }, { "b4_gain", -5 }, { "b4_q", 0.8f } } });
        m.presets.push_back ({ "Sourire (graves et aigus)", { { "b2_on", 1 }, { "b2_type", 1 }, { "b2_freq", 100 }, { "b2_gain", 4 }, { "b2_q", 0.9f }, { "b7_on", 1 }, { "b7_type", 2 }, { "b7_freq", 9000 }, { "b7_gain", 4 }, { "b7_q", 0.9f } } });
        m.presets.push_back ({ "Coupe-bas doux", { { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 80 }, { "b1_gain", 0 }, { "b1_q", 0.9f } } });
        m.presets.push_back ({ "Coupe-bas raide", { { "b1_on", 1 }, { "b1_type", 9 }, { "b1_freq", 100 }, { "b1_gain", 0 }, { "b1_q", 0.9f } } });
        m.presets.push_back ({ "Nappe au fond", { { "b1_on", 1 }, { "b1_type", 4 }, { "b1_freq", 200 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b7_on", 1 }, { "b7_type", 5 }, { "b7_freq", 6000 }, { "b7_gain", 0 }, { "b7_q", 0.9f }, { "b5_on", 1 }, { "b5_type", 0 }, { "b5_freq", 2500 }, { "b5_gain", -3 }, { "b5_q", 1 } } });
        m.presets.push_back ({ "Lead en avant", { { "b1_on", 1 }, { "b1_type", 4 }, { "b1_freq", 150 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b5_on", 1 }, { "b5_type", 0 }, { "b5_freq", 2800 }, { "b5_gain", 4 }, { "b5_q", 1.2f }, { "b7_on", 1 }, { "b7_type", 2 }, { "b7_freq", 10000 }, { "b7_gain", 2 }, { "b7_q", 0.9f } } });
        m.presets.push_back ({ "Grave chaud", { { "b2_on", 1 }, { "b2_type", 1 }, { "b2_freq", 180 }, { "b2_gain", 3 }, { "b2_q", 0.9f }, { "b7_on", 1 }, { "b7_type", 2 }, { "b7_freq", 8000 }, { "b7_gain", -2 }, { "b7_q", 0.9f } } });
        m.presets.push_back ({ "Clavier clair", { { "b1_on", 1 }, { "b1_type", 3 }, { "b1_freq", 90 }, { "b1_gain", 0 }, { "b1_q", 0.9f }, { "b4_on", 1 }, { "b4_type", 0 }, { "b4_freq", 400 }, { "b4_gain", -2 }, { "b4_q", 0.9f }, { "b6_on", 1 }, { "b6_type", 0 }, { "b6_freq", 3500 }, { "b6_gain", 3 }, { "b6_q", 1 } } });
    }
    if (m.type == "opto")
    {
        m.presets.push_back ({ "Voix douce", { { "peak", 30 }, { "gain", 4 } } });
        m.presets.push_back ({ "Voix tenue", { { "peak", 60 }, { "gain", 9 } } });
        m.presets.push_back ({ "Basse ronde", { { "peak", 55 }, { "gain", 8 }, { "emphasis", 0 } } });
        m.presets.push_back ({ "Batterie parall\xC3\xA8le", { { "peak", 75 }, { "gain", 14 }, { "mix", 40 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 liss\xC3\xA9", { { "peak", 40 }, { "gain", 5 } } });
        m.presets.push_back ({ "Piano \xC3\xA9gal", { { "peak", 35 }, { "gain", 5 }, { "emphasis", 30 } } });
        m.presets.push_back ({ "Bus de nappes", { { "peak", 25 }, { "gain", 3 } } });
        m.presets.push_back ({ "Limitation dure", { { "mode", 1 }, { "peak", 70 }, { "gain", 10 } } });
        m.presets.push_back ({ "Aigus calm\xC3\xA9s", { { "peak", 45 }, { "gain", 6 }, { "emphasis", 80 } } });
        m.presets.push_back ({ "Guitare acoustique", { { "peak", 40 }, { "gain", 6 }, { "emphasis", 40 } } });
        m.presets.push_back ({ "\xC3\x89" "crasement doux", { { "peak", 90 }, { "gain", 18 }, { "mix", 50 } } });
        m.presets.push_back ({ "Couleur l\xC3\xA9g\xC3\xA8re", { { "peak", 15 }, { "gain", 2 } } });
        m.presets.push_back ({ "Pompe par la grosse caisse", { { "peak", 70 }, { "gain", 6 }, { "scsrc", 1 } } });
        m.presets.push_back ({ "Lead tenu", { { "peak", 55 }, { "gain", 8 } } });
    }
    if (m.type == "fet")
    {
        m.presets.push_back ({ "Grosse caisse", { { "input", 20 }, { "attack", 400 }, { "release", 150 }, { "ratio", 0 } } });
        m.presets.push_back ({ "Voix pop", { { "input", 22 }, { "attack", 300 }, { "release", 300 }, { "ratio", 0 } } });
        m.presets.push_back ({ "Voix rap serr\xC3\xA9" "e", { { "input", 28 }, { "attack", 600 }, { "release", 500 }, { "ratio", 1 } } });
        m.presets.push_back ({ "Guitare \xC3\xA9lectrique", { { "input", 24 }, { "attack", 200 }, { "release", 350 }, { "ratio", 0 } } });
        m.presets.push_back ({ "Bus batterie", { { "input", 26 }, { "attack", 100 }, { "release", 600 }, { "ratio", 0 }, { "mix", 60 } } });
        m.presets.push_back ({ "Basse funk", { { "input", 24 }, { "attack", 500 }, { "release", 200 }, { "ratio", 1 } } });
        m.presets.push_back ({ "Toms qui ressortent", { { "input", 30 }, { "attack", 80 }, { "release", 700 }, { "ratio", 2 }, { "mix", 50 } } });
        m.presets.push_back ({ "Pi\xC3\xA8" "ce \xC3\xA9" "cras\xC3\xA9" "e", { { "input", 40 }, { "attack", 800 }, { "release", 1100 }, { "ratio", 4 }, { "mix", 35 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 mordant", { { "input", 22 }, { "attack", 300 }, { "release", 250 }, { "ratio", 0 } } });
        m.presets.push_back ({ "Limitation", { { "input", 30 }, { "attack", 700 }, { "release", 200 }, { "ratio", 3 } } });
        m.presets.push_back ({ "Charley dompt\xC3\xA9", { { "input", 20 }, { "attack", 500 }, { "release", 150 }, { "ratio", 1 } } });
        m.presets.push_back ({ "Piano rock", { { "input", 20 }, { "attack", 250 }, { "release", 400 }, { "ratio", 0 } } });
        m.presets.push_back ({ "Tous les boutons", { { "input", 36 }, { "attack", 800 }, { "release", 1000 }, { "ratio", 4 }, { "mix", 70 } } });
        m.presets.push_back ({ "Ambiance de salle", { { "input", 34 }, { "attack", 700 }, { "release", 900 }, { "ratio", 4 }, { "mix", 45 } } });
        m.presets.push_back ({ "Voix discr\xC3\xA8te", { { "input", 14 }, { "attack", 400 }, { "release", 400 }, { "ratio", 0 } } });
    }
    if (m.type == "bus")
    {
        m.presets.push_back ({ "Colle douce", { { "threshold", -8 }, { "ratio", 0 }, { "attack", 4 }, { "release", 4 }, { "makeup", 1 } } });
        m.presets.push_back ({ "Colle serr\xC3\xA9" "e", { { "threshold", -16 }, { "ratio", 1 }, { "attack", 3 }, { "release", 1 }, { "makeup", 4 } } });
        m.presets.push_back ({ "Batterie qui claque", { { "threshold", -18 }, { "ratio", 1 }, { "attack", 4 }, { "release", 0 }, { "makeup", 5 } } });
        m.presets.push_back ({ "Pompe au tempo", { { "threshold", -24 }, { "ratio", 2 }, { "attack", 0 }, { "release", 1 }, { "makeup", 6 } } });
        m.presets.push_back ({ "Bus de synth\xC3\xA9s", { { "threshold", -12 }, { "ratio", 0 }, { "attack", 3 }, { "release", 2 }, { "makeup", 2 } } });
        m.presets.push_back ({ "Bus de basses", { { "threshold", -14 }, { "ratio", 1 }, { "attack", 4 }, { "release", 2 }, { "makeup", 3 }, { "schpf", 20 } } });
        m.presets.push_back ({ "Master qui respire", { { "threshold", -6 }, { "ratio", 0 }, { "attack", 5 }, { "release", 4 }, { "makeup", 1 } } });
        m.presets.push_back ({ "Parall\xC3\xA8le lourd", { { "threshold", -30 }, { "ratio", 2 }, { "attack", 1 }, { "release", 1 }, { "makeup", 10 }, { "mix", 35 } } });
        m.presets.push_back ({ "Grave libre", { { "threshold", -14 }, { "ratio", 1 }, { "attack", 3 }, { "release", 4 }, { "makeup", 3 }, { "schpf", 150 } } });
        m.presets.push_back ({ "Voix en groupe", { { "threshold", -12 }, { "ratio", 1 }, { "attack", 2 }, { "release", 2 }, { "makeup", 3 } } });
        m.presets.push_back ({ "Pompe sur la grosse caisse", { { "threshold", -28 }, { "ratio", 2 }, { "attack", 0 }, { "release", 1 }, { "makeup", 4 }, { "scsrc", 1 } } });
        m.presets.push_back ({ "Nappes liss\xC3\xA9" "es", { { "threshold", -10 }, { "ratio", 0 }, { "attack", 5 }, { "release", 3 }, { "makeup", 2 } } });
        m.presets.push_back ({ "Bus percussions", { { "threshold", -16 }, { "ratio", 1 }, { "attack", 2 }, { "release", 1 }, { "makeup", 4 } } });
        m.presets.push_back ({ "S\xC3\xA9" "curit\xC3\xA9 l\xC3\xA9g\xC3\xA8re", { { "threshold", -4 }, { "ratio", 2 }, { "attack", 0 }, { "release", 0 }, { "makeup", 0 } } });
        m.presets.push_back ({ "Retour d'effets", { { "threshold", -20 }, { "ratio", 1 }, { "attack", 3 }, { "release", 3 }, { "makeup", 4 } } });
    }
    if (m.type == "comp")
    {
        m.presets.push_back ({ "Synth\xC3\xA9 lead", { { "threshold", -16 }, { "ratio", 4 }, { "attack", 8 }, { "release", 100 }, { "style", 0 } } });
        m.presets.push_back ({ "Nappe \xC3\xA9gale", { { "threshold", -20 }, { "ratio", 2.5f }, { "attack", 40 }, { "release", 400 }, { "style", 2 } } });
        m.presets.push_back ({ "Grosse caisse s\xC3\xA8" "che", { { "threshold", -14 }, { "ratio", 5 }, { "attack", 20 }, { "release", 60 }, { "style", 6 } } });
        m.presets.push_back ({ "Caisse claire claquante", { { "threshold", -18 }, { "ratio", 6 }, { "attack", 15 }, { "release", 80 }, { "style", 6 } } });
        m.presets.push_back ({ "Charley calm\xC3\xA9", { { "threshold", -22 }, { "ratio", 3 }, { "attack", 2 }, { "release", 50 }, { "style", 0 } } });
    }
    if (m.type == "limiter")
    {
        m.presets.push_back ({ "Club tr\xC3\xA8s fort", { { "gain", 10 }, { "ceiling", -0.3f }, { "release", 40 }, { "style", 1 }, { "clip", 1 } } });
        m.presets.push_back ({ "Master doux", { { "gain", 3 }, { "ceiling", -1 }, { "release", 200 }, { "style", 0 } } });
        m.presets.push_back ({ "Bus de basses", { { "gain", 4 }, { "ceiling", -3 }, { "release", 60 }, { "style", 0 } } });
        m.presets.push_back ({ "Voix tenue", { { "gain", 4 }, { "ceiling", -6 }, { "release", 100 }, { "style", 0 } } });
        m.presets.push_back ({ "\xC3\x89" "cr\xC3\xAAtage franc", { { "gain", 8 }, { "ceiling", -1 }, { "release", 10 }, { "style", 1 }, { "clip", 1 } } });
        m.presets.push_back ({ "Diffusion radio", { { "gain", 7 }, { "ceiling", -1 }, { "release", 50 }, { "style", 1 }, { "truePeak", 1 } } });
        m.presets.push_back ({ "Grosse caisse en avant", { { "gain", 5 }, { "ceiling", -2 }, { "release", 20 }, { "style", 1 } } });
        m.presets.push_back ({ "Batterie \xC3\xA9" "cras\xC3\xA9" "e", { { "gain", 12 }, { "ceiling", -3 }, { "release", 30 }, { "style", 1 }, { "clip", 1 } } });
        m.presets.push_back ({ "Dynamique gard\xC3\xA9" "e", { { "gain", 1 }, { "ceiling", -0.5f }, { "release", 300 }, { "style", 0 } } });
        m.presets.push_back ({ "Podcast (-16 LUFS)", { { "gain", 4 }, { "ceiling", -1.5f }, { "release", 150 }, { "style", 0 }, { "truePeak", 1 } } });
        m.presets.push_back ({ "Nappes au plafond", { { "gain", 6 }, { "ceiling", -6 }, { "release", 400 }, { "style", 0 } } });
        m.presets.push_back ({ "Synth\xC3\xA9s pouss\xC3\xA9s", { { "gain", 6 }, { "ceiling", -2 }, { "release", 80 }, { "style", 1 } } });
        m.presets.push_back ({ "Pr\xC3\xA9-master", { { "gain", 2 }, { "ceiling", -3 }, { "release", 120 }, { "style", 0 }, { "truePeak", 1 } } });
        m.presets.push_back ({ "Effets tenus", { { "gain", 8 }, { "ceiling", -8 }, { "release", 250 }, { "style", 0 } } });
        m.presets.push_back ({ "Plafond -6 dB", { { "gain", 0 }, { "ceiling", -6 }, { "release", 80 }, { "style", 0 } } });
    }
    if (m.type == "master")
    {
        m.presets.push_back ({ "Techno (-8 LUFS)", { { "drive", 8 }, { "ceiling", -0.5f }, { "character", 60 }, { "clipAmt", 40 }, { "shine", 20 }, { "warmth", 20 } } });
        m.presets.push_back ({ "House chaude", { { "drive", 6 }, { "warmth", 35 }, { "shine", 10 }, { "lowGain", 1 }, { "ceiling", -0.8f } } });
        m.presets.push_back ({ "Hip-hop grave", { { "drive", 6 }, { "lowGain", 2 }, { "warmth", 30 }, { "gainLow", 1 }, { "ceiling", -0.8f } } });
        m.presets.push_back ({ "Ambient ouvert", { { "drive", 1 }, { "shine", 25 }, { "warmth", 10 }, { "widthHigh", 130 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "Pop brillante", { { "drive", 5 }, { "shine", 35 }, { "airGain", 2 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "D\xC3\xA9mo rapide", { { "drive", 4 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "Lo-fi douce", { { "drive", 3 }, { "airGain", -3 }, { "warmth", 40 }, { "shine", 0 }, { "ceiling", -1.5f } } });
        m.presets.push_back ({ "Mono dans le grave", { { "widthLow", 0 }, { "splitLow", 1 }, { "drive", 4 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "Large et a\xC3\xA9r\xC3\xA9", { { "widthMid", 120 }, { "widthHigh", 150 }, { "shine", 25 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "M\xC3\xA9" "dium en avant", { { "midFreq", 2000 }, { "midGain", 2 }, { "drive", 4 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "Dynamique gard\xC3\xA9" "e", { { "dynOn", 0 }, { "drive", 2 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "Drum and bass", { { "drive", 8 }, { "gainLow", 1 }, { "thrLow", -16 }, { "shine", 20 }, { "ceiling", -0.5f } } });
        m.presets.push_back ({ "Dub profond", { { "lowGain", 2 }, { "airGain", -2 }, { "warmth", 30 }, { "drive", 5 }, { "ceiling", -1 } } });
        m.presets.push_back ({ "\xC3\x89lectro dure", { { "drive", 10 }, { "character", 80 }, { "clipAmt", 60 }, { "ceiling", -0.3f } } });
    }
    if (m.type == "gate")
    {
        m.presets.push_back ({ "Grosse caisse s\xC3\xA8" "che", { { "gThresh", -30 }, { "gRange", 60 }, { "gAttack", 0.1f }, { "gHold", 40 }, { "gRelease", 80 } } });
        m.presets.push_back ({ "Toms courts", { { "gThresh", -35 }, { "gRange", 50 }, { "gHold", 80 }, { "gRelease", 200 } } });
        m.presets.push_back ({ "Bruit de fond l\xC3\xA9ger", { { "gThresh", -60 }, { "gRange", 15 }, { "gHold", 50 }, { "gRelease", 300 } } });
        m.presets.push_back ({ "Nappe hach\xC3\xA9" "e", { { "gThresh", -20 }, { "gRange", 80 }, { "gAttack", 0.5f }, { "gHold", 0 }, { "gRelease", 20 } } });
        m.presets.push_back ({ "Porte lente", { { "gThresh", -45 }, { "gRange", 30 }, { "gAttack", 10 }, { "gHold", 100 }, { "gRelease", 800 } } });
        m.presets.push_back ({ "D\xC3\xA9-esseur doux", { { "gRange", 0 }, { "dsThresh", -28 }, { "dsAmount", 6 }, { "dsFreq", 6500 } } });
        m.presets.push_back ({ "D\xC3\xA9-esseur fort", { { "gRange", 0 }, { "dsThresh", -35 }, { "dsAmount", 14 }, { "dsFreq", 7000 } } });
        m.presets.push_back ({ "Cymbales adoucies", { { "gRange", 0 }, { "dsThresh", -30 }, { "dsAmount", 8 }, { "dsFreq", 9000 }, { "dsMode", 1 } } });
        m.presets.push_back ({ "Grave seul (d\xC3\xA9" "clenchement)", { { "gThresh", -30 }, { "gRange", 60 }, { "gHp", 20 }, { "gHold", 30 }, { "gRelease", 100 } } });
        m.presets.push_back ({ "Coupure nette", { { "gThresh", -25 }, { "gRange", 80 }, { "gAttack", 0.05f }, { "gHold", 10 }, { "gRelease", 10 } } });
        m.presets.push_back ({ "Porte et respiration", { { "gThresh", -40 }, { "gRange", 25 }, { "gHold", 150 }, { "gRelease", 600 } } });
        m.presets.push_back ({ "Caisse claire 80", { { "gThresh", -20 }, { "gRange", 80 }, { "gHold", 200 }, { "gRelease", 60 } } });
    }
    if (m.type == "mband")
    {
        m.presets.push_back ({ "Basses : grave serr\xC3\xA9", { { "b1thr", -24 }, { "b1rat", 4 }, { "b1att", 20 }, { "b1rel", 120 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 : m\xC3\xA9" "dium calm\xC3\xA9", { { "b2thr", -22 }, { "b2rat", 3 }, { "b3thr", -22 }, { "b3rat", 3 } } });
        m.presets.push_back ({ "Nappe : aigus doux", { { "b4thr", -26 }, { "b4rat", 3 }, { "b4att", 5 }, { "b4rel", 200 } } });
        m.presets.push_back ({ "Batterie : grave en avant", { { "b1gain", 2 }, { "b1thr", -20 }, { "b1rat", 3 }, { "b4thr", -24 }, { "b4rat", 2 } } });
        m.presets.push_back ({ "Expansion des transitoires", { { "b3mode", 1 }, { "b3thr", -30 }, { "b3rat", 2 }, { "b4mode", 1 }, { "b4thr", -30 }, { "b4rat", 2 } } });
        m.presets.push_back ({ "Master quatre bandes", { { "b1thr", -16 }, { "b1rat", 2 }, { "b2thr", -18 }, { "b2rat", 1.5f }, { "b3thr", -18 }, { "b3rat", 1.5f }, { "b4thr", -20 }, { "b4rat", 2 } } });
        m.presets.push_back ({ "Voix : pr\xC3\xA9sence", { { "b3gain", 2 }, { "b3thr", -24 }, { "b3rat", 2 } } });
        m.presets.push_back ({ "Aigus seuls (\xC3\xA9" "coute)", { { "b4solo", 1 } } });
        m.presets.push_back ({ "M\xC3\xA9" "dium seul (\xC3\xA9" "coute)", { { "b2solo", 1 }, { "b3solo", 1 } } });
        m.presets.push_back ({ "Grave dynamique", { { "f1on", 1 }, { "f1freq", 80 }, { "f1thr", -24 }, { "f1range", -6 } } });
        m.presets.push_back ({ "Chuintantes dynamiques", { { "f2on", 1 }, { "f2freq", 7000 }, { "f2thr", -30 }, { "f2range", -8 } } });
        m.presets.push_back ({ "Boue dynamique", { { "f1on", 1 }, { "f1freq", 300 }, { "f1thr", -26 }, { "f1range", -6 }, { "f1q", 1.2f } } });
        m.presets.push_back ({ "Colle l\xC3\xA9g\xC3\xA8re", { { "b1thr", -10 }, { "b1rat", 1.5f }, { "b2thr", -10 }, { "b2rat", 1.5f }, { "b3thr", -10 }, { "b3rat", 1.5f }, { "b4thr", -10 }, { "b4rat", 1.5f } } });
    }
    if (m.type == "varimu")
    {
        m.presets.push_back ({ "Voix en douceur", { { "input", 4 }, { "threshold", -12 }, { "tc", 1 }, { "tube", 20 } } });
        m.presets.push_back ({ "Nappes coll\xC3\xA9" "es", { { "input", 6 }, { "threshold", -16 }, { "tc", 3 }, { "tube", 30 } } });
        m.presets.push_back ({ "Basse chaude", { { "input", 10 }, { "threshold", -18 }, { "tc", 0 }, { "tube", 50 }, { "output", -5 } } });
        m.presets.push_back ({ "Bus de synth\xC3\xA9s", { { "input", 6 }, { "threshold", -14 }, { "tc", 4 }, { "tube", 35 } } });
        m.presets.push_back ({ "Lampe pouss\xC3\xA9" "e", { { "input", 18 }, { "threshold", -6 }, { "tc", 1 }, { "tube", 80 }, { "output", -10 } } });
        m.presets.push_back ({ "Master gras", { { "input", 8 }, { "threshold", -12 }, { "tc", 4 }, { "tube", 40 }, { "output", -4 } } });
        m.presets.push_back ({ "Parall\xC3\xA8le \xC3\xA9pais", { { "input", 14 }, { "threshold", -24 }, { "tc", 0 }, { "tube", 60 }, { "mix", 40 }, { "output", -6 } } });
        m.presets.push_back ({ "Milieu et c\xC3\xB4t\xC3\xA9s", { { "link", 1 }, { "input", 6 }, { "threshold", -14 }, { "tc", 2 } } });
        m.presets.push_back ({ "Lent et musical", { { "input", 6 }, { "threshold", -16 }, { "tc", 5 }, { "tube", 30 } } });
        m.presets.push_back ({ "Piano rond", { { "input", 4 }, { "threshold", -14 }, { "tc", 1 }, { "tube", 25 } } });
        m.presets.push_back ({ "Batterie dense", { { "input", 12 }, { "threshold", -20 }, { "tc", 0 }, { "tube", 45 }, { "output", -6 } } });
        m.presets.push_back ({ "Pompe par la grosse caisse", { { "input", 6 }, { "threshold", -22 }, { "tc", 0 }, { "scsrc", 1 } } });
        m.presets.push_back ({ "Guitare chaude", { { "input", 8 }, { "threshold", -14 }, { "tc", 1 }, { "tube", 55 } } });
        m.presets.push_back ({ "Effets liss\xC3\xA9s", { { "input", 6 }, { "threshold", -18 }, { "tc", 3 }, { "tube", 20 } } });
    }
    if (m.type == "tubeeq")
    {
        m.presets.push_back ({ "Basse ronde", { { "lowfreq", 2 }, { "lowboost", 5 }, { "lowatten", 2 } } });
        m.presets.push_back ({ "Voix brillante", { { "hifreq", 5 }, { "hiboost", 4 }, { "hibw", 4 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 a\xC3\xA9r\xC3\xA9", { { "hifreq", 6 }, { "hiboost", 5 } } });
        m.presets.push_back ({ "Grave puissant", { { "lowfreq", 1 }, { "lowboost", 6 }, { "lowatten", 3 } } });
        m.presets.push_back ({ "Caisse claire claquante", { { "hifreq", 1 }, { "hiboost", 5 }, { "hibw", 3 } } });
        m.presets.push_back ({ "Nappe adoucie", { { "attfreq", 0 }, { "hiatten", 5 } } });
        m.presets.push_back ({ "Master chaud", { { "lowfreq", 2 }, { "lowboost", 2 }, { "hifreq", 5 }, { "hiboost", 2 }, { "tube", 35 } } });
        m.presets.push_back ({ "Lampe seule pouss\xC3\xA9" "e", { { "tube", 80 } } });
        m.presets.push_back ({ "Piano pr\xC3\xA9sent", { { "hifreq", 2 }, { "hiboost", 3 }, { "hibw", 6 } } });
        m.presets.push_back ({ "Guitare brillante", { { "hifreq", 3 }, { "hiboost", 4 } } });
        m.presets.push_back ({ "Grosse caisse sourde", { { "lowfreq", 2 }, { "lowboost", 6 }, { "attfreq", 1 }, { "hiatten", 6 } } });
        m.presets.push_back ({ "Bus chaleureux", { { "lowfreq", 3 }, { "lowboost", 2 }, { "tube", 40 } } });
        m.presets.push_back ({ "Charley soyeux", { { "hifreq", 6 }, { "hiboost", 3 }, { "attfreq", 2 }, { "hiatten", 3 } } });
        m.presets.push_back ({ "Basse grasse et sombre", { { "lowfreq", 2 }, { "lowboost", 7 }, { "lowatten", 4 }, { "attfreq", 0 }, { "hiatten", 4 }, { "tube", 50 } } });
    }
    if (m.type == "consoleeq")
    {
        m.presets.push_back ({ "Basse qui perce", { { "hpf", 30 }, { "lfgain", 4 }, { "lffreq", 80 }, { "lmfgain", -3 }, { "lmffreq", 300 }, { "hmfgain", 4 }, { "hmffreq", 1200 } } });
        m.presets.push_back ({ "Grosse caisse s\xC3\xA8" "che", { { "hpf", 30 }, { "lfgain", 5 }, { "lffreq", 60 }, { "lmfgain", -6 }, { "lmffreq", 400 }, { "hmfgain", 4 }, { "hmffreq", 3500 } } });
        m.presets.push_back ({ "Toms", { { "hpf", 60 }, { "lfgain", 3 }, { "lffreq", 100 }, { "lmfgain", -4 }, { "lmffreq", 500 }, { "hmfgain", 3 }, { "hmffreq", 4000 } } });
        m.presets.push_back ({ "Charley", { { "hpf", 300 }, { "hfgain", 4 }, { "hffreq", 10000 } } });
        m.presets.push_back ({ "Voix rock", { { "hpf", 100 }, { "lmfgain", -3 }, { "lmffreq", 350 }, { "hmfgain", 5 }, { "hmffreq", 3000 }, { "character", 2 }, { "drive", 30 } } });
        m.presets.push_back ({ "Piano", { { "hpf", 40 }, { "lmfgain", -2 }, { "lmffreq", 250 }, { "hfgain", 3 }, { "hffreq", 10000 } } });
        m.presets.push_back ({ "Guitare rythmique", { { "hpf", 90 }, { "lmfgain", -3 }, { "lmffreq", 300 }, { "hmfgain", 3 }, { "hmffreq", 2500 } } });
        m.presets.push_back ({ "Nappe dans le fond", { { "hpf", 150 }, { "lpf", 9000 }, { "hmfgain", -3 }, { "hmffreq", 2500 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 gras", { { "hpf", 40 }, { "lfgain", 3 }, { "lffreq", 120 }, { "character", 2 }, { "drive", 50 } } });
        m.presets.push_back ({ "Console pouss\xC3\xA9" "e", { { "character", 2 }, { "drive", 70 } } });
        m.presets.push_back ({ "T\xC3\xA9l\xC3\xA9phone", { { "hpf", 350 }, { "lpf", 3500 }, { "hmfgain", 8 }, { "hmffreq", 1500 }, { "hmfq", 1.5f } } });
        m.presets.push_back ({ "Master de console", { { "hpf", 20 }, { "lfgain", 1.5f }, { "lffreq", 60 }, { "hfgain", 1.5f }, { "hffreq", 12000 }, { "character", 0 }, { "drive", 15 } } });
        m.presets.push_back ({ "Lead sans boue", { { "hpf", 120 }, { "lmfgain", -4 }, { "lmffreq", 400 }, { "hmfgain", 2 }, { "hmffreq", 3000 } } });
        m.presets.push_back ({ "Effets assombris", { { "hpf", 200 }, { "lpf", 6000 } } });
    }
    if (m.type == "dist")
    {
        m.presets.push_back ({ "Basse grasse", { { "typeA", 1 }, { "driveA", 55 }, { "toneA", 40 }, { "routing", 4 }, { "xover", 120 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 d\xC3\xA9" "chir\xC3\xA9", { { "typeA", 6 }, { "driveA", 70 }, { "toneA", 60 } } });
        m.presets.push_back ({ "Bits qui bougent", { { "typeA", 7 }, { "driveA", 50 }, { "m1type", 1 }, { "m1div", 8 }, { "m1shape", 0 }, { "m1amt", 60 }, { "m1dest", 0 } } });
        m.presets.push_back ({ "Batterie en bande", { { "typeA", 0 }, { "driveA", 40 }, { "mix", 70 } } });
        m.presets.push_back ({ "Lampe et repli", { { "typeA", 1 }, { "driveA", 40 }, { "typeB", 9 }, { "driveB", 40 }, { "mixB", 50 }, { "routing", 0 } } });
        m.presets.push_back ({ "Fuzz sur le grave", { { "typeA", 6 }, { "driveA", 60 }, { "routing", 4 }, { "xover", 200 }, { "balance", 30 } } });
    }
    if (m.type == "grinder")
    {
        m.presets.push_back ({ "Basse quatre bandes", { { "b1type", 2 }, { "b1drive", 40 }, { "b2type", 7 }, { "b2drive", 50 }, { "b3type", 3 }, { "b3drive", 35 }, { "b4type", 0 } } });
        m.presets.push_back ({ "M\xC3\xA9" "dium seul broy\xC3\xA9", { { "b1type", 0 }, { "b2type", 7 }, { "b2drive", 70 }, { "b3type", 7 }, { "b3drive", 60 }, { "b4type", 0 } } });
        m.presets.push_back ({ "Aigus en bits", { { "b1type", 0 }, { "b2type", 0 }, { "b3type", 0 }, { "b4type", 8 }, { "b4drive", 60 } } });
        m.presets.push_back ({ "Larsen grave", { { "b1fb", 60 }, { "b1fbfreq", 55 }, { "b1type", 2 }, { "b1drive", 50 } } });
        m.presets.push_back ({ "Batterie par bandes", { { "b1type", 1 }, { "b1drive", 40 }, { "b2type", 3 }, { "b2drive", 50 }, { "b3type", 5 }, { "b3drive", 30 }, { "b4type", 12 }, { "b4drive", 30 } } });
        m.presets.push_back ({ "Bandes en st\xC3\xA9r\xC3\xA9o", { { "b1pan", 0 }, { "b2pan", -60 }, { "b3pan", 60 }, { "b4pan", -30 } } });
        m.presets.push_back ({ "Repli au suiveur", { { "m1type", 2 }, { "m1amt", 60 }, { "m1dest", 6 }, { "b2type", 10 } } });
        m.presets.push_back ({ "Douce chaleur", { { "b1type", 1 }, { "b1drive", 15 }, { "b2type", 2 }, { "b2drive", 20 }, { "b3type", 2 }, { "b3drive", 15 }, { "b4type", 1 }, { "b4drive", 10 } } });
        m.presets.push_back ({ "Grave propre, haut sale", { { "b1type", 0 }, { "b2type", 3 }, { "b2drive", 45 }, { "b3type", 7 }, { "b3drive", 55 }, { "b4type", 8 }, { "b4drive", 40 } } });
        m.presets.push_back ({ "Deux bandes", { { "bands", 0 }, { "b1type", 2 }, { "b1drive", 40 }, { "b2type", 7 }, { "b2drive", 55 } } });
        m.presets.push_back ({ "Trois bandes", { { "bands", 1 }, { "b1type", 1 }, { "b1drive", 30 }, { "b2type", 3 }, { "b2drive", 45 }, { "b3type", 9 }, { "b3drive", 35 } } });
    }
    if (m.type == "tapesat")
    {
        m.presets.push_back ({ "Grave de bande", { { "drive", 8 }, { "bump", 5 }, { "speed", 0 } } });
        m.presets.push_back ({ "Aigus doux", { { "drive", 6 }, { "treble", -5 } } });
        m.presets.push_back ({ "Bande pouss\xC3\xA9" "e fort", { { "drive", 18 }, { "bias", 20 }, { "output", -8 } } });
        m.presets.push_back ({ "Souffle et pleurage", { { "drive", 4 }, { "wow", 50 }, { "hiss", 40 } } });
        m.presets.push_back ({ "Master l\xC3\xA9g\xC3\xA8re", { { "drive", 3 }, { "speed", 2 }, { "output", -2 } } });
        m.presets.push_back ({ "Basse satur\xC3\xA9" "e", { { "drive", 14 }, { "bump", 4 }, { "bias", -30 } } });
        m.presets.push_back ({ "Voix chaude", { { "drive", 6 }, { "treble", -2 }, { "wow", 5 } } });
        m.presets.push_back ({ "Cassette fatigu\xC3\xA9" "e", { { "drive", 10 }, { "speed", 0 }, { "treble", -8 }, { "wow", 60 }, { "hiss", 50 } } });
        m.presets.push_back ({ "Bande neuve", { { "drive", 4 }, { "speed", 2 }, { "wow", 0 }, { "hiss", 0 } } });
        m.presets.push_back ({ "Batterie dense", { { "drive", 14 }, { "speed", 1 }, { "bump", 3 }, { "mix", 70 } } });
        m.presets.push_back ({ "Synth\xC3\xA9s coll\xC3\xA9s", { { "drive", 7 }, { "speed", 1 }, { "wow", 8 } } });
        m.presets.push_back ({ "Nappe us\xC3\xA9" "e", { { "drive", 5 }, { "wow", 40 }, { "treble", -6 }, { "hiss", 20 } } });
        m.presets.push_back ({ "Bande en parall\xC3\xA8le", { { "drive", 20 }, { "mix", 40 } } });
        m.presets.push_back ({ "Polarisation basse", { { "drive", 10 }, { "bias", -70 } } });
        m.presets.push_back ({ "Polarisation haute", { { "drive", 10 }, { "bias", 70 } } });
        m.presets.push_back ({ "Bande discr\xC3\xA8te", { { "drive", 2 }, { "wow", 5 }, { "output", 0 } } });
    }
    if (m.type == "tubesat")
    {
        m.presets.push_back ({ "Lampe douce", { { "drive", 20 }, { "bias", 10 }, { "sag", 20 } } });
        m.presets.push_back ({ "Lampe pouss\xC3\xA9" "e", { { "drive", 70 }, { "sag", 50 }, { "output", -6 } } });
        m.presets.push_back ({ "Basse lampe", { { "drive", 50 }, { "tone", 35 }, { "bias", 30 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 chaud", { { "drive", 35 }, { "tone", 55 } } });
        m.presets.push_back ({ "Pr\xC3\xA9" "ampli brillant", { { "drive", 40 }, { "tone", 75 } } });
        m.presets.push_back ({ "Pr\xC3\xA9" "ampli sombre", { { "drive", 40 }, { "tone", 25 } } });
        m.presets.push_back ({ "Ampli qui s'affaisse", { { "drive", 60 }, { "sag", 90 } } });
        m.presets.push_back ({ "Asym\xC3\xA9trique", { { "drive", 50 }, { "bias", 80 } } });
        m.presets.push_back ({ "Parall\xC3\xA8le chaud", { { "drive", 80 }, { "mix", 35 }, { "output", -6 } } });
        m.presets.push_back ({ "Batterie qui sature", { { "drive", 60 }, { "sag", 40 }, { "tone", 60 }, { "mix", 70 } } });
        m.presets.push_back ({ "Voix radio", { { "drive", 55 }, { "tone", 70 }, { "bias", 40 } } });
        m.presets.push_back ({ "Bus color\xC3\xA9", { { "drive", 15 }, { "tone", 50 }, { "sag", 10 } } });
        m.presets.push_back ({ "Grave arrondi", { { "drive", 30 }, { "tone", 30 }, { "sag", 60 } } });
        m.presets.push_back ({ "Lampe \xC3\xA0 fond", { { "drive", 100 }, { "sag", 70 }, { "output", -12 } } });
        m.presets.push_back ({ "Clavier vintage", { { "drive", 45 }, { "tone", 45 }, { "bias", 25 } } });
        m.presets.push_back ({ "Effets satur\xC3\xA9s", { { "drive", 65 }, { "tone", 40 }, { "mix", 50 } } });
    }
    if (m.type == "distcomp")
    {
        m.presets.push_back ({ "Batterie de pi\xC3\xA8" "ce", { { "input", 30 }, { "ratio", 7 }, { "attack", 0 }, { "release", 8 }, { "mix", 50 } } });
        m.presets.push_back ({ "Basse serr\xC3\xA9" "e", { { "input", 22 }, { "ratio", 4 }, { "attack", 4 }, { "release", 5 } } });
        m.presets.push_back ({ "Voix rap", { { "input", 24 }, { "ratio", 5 }, { "attack", 3 }, { "release", 4 }, { "detector", 2 } } });
        m.presets.push_back ({ "Guitare satur\xC3\xA9" "e", { { "input", 26 }, { "ratio", 3 }, { "audio", 1 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 mordant", { { "input", 20 }, { "ratio", 3 }, { "audio", 2 }, { "attack", 5 }, { "release", 3 } } });
        m.presets.push_back ({ "Britannique douce", { { "british", 1 }, { "input", 16 }, { "ratio", 2 } } });
        m.presets.push_back ({ "Grave libre", { { "input", 22 }, { "ratio", 4 }, { "detector", 1 } } });
        m.presets.push_back ({ "Nuke parall\xC3\xA8le", { { "input", 34 }, { "ratio", 7 }, { "mix", 30 } } });
        m.presets.push_back ({ "Couleur distordue", { { "ratio", 0 }, { "audio", 2 }, { "input", 20 } } });
        m.presets.push_back ({ "Caisse claire qui claque", { { "input", 24 }, { "ratio", 4 }, { "attack", 8 }, { "release", 2 } } });
        m.presets.push_back ({ "Nappe tenue", { { "input", 18 }, { "ratio", 2 }, { "attack", 6 }, { "release", 9 } } });
        m.presets.push_back ({ "Bus britannique", { { "british", 1 }, { "input", 20 }, { "ratio", 3 }, { "attack", 5 }, { "release", 6 } } });
        m.presets.push_back ({ "Lead en avant", { { "input", 22 }, { "ratio", 3 }, { "detector", 3 } } });
        m.presets.push_back ({ "Grosse caisse", { { "input", 20 }, { "ratio", 4 }, { "attack", 7 }, { "release", 3 } } });
        m.presets.push_back ({ "Effets \xC3\xA9" "cras\xC3\xA9s", { { "input", 30 }, { "ratio", 6 }, { "mix", 60 } } });
    }
    if (m.type == "transient")
    {
        m.presets.push_back ({ "Grosse caisse qui claque", { { "attack", 70 }, { "sustain", -20 }, { "speed", 0 } } });
        m.presets.push_back ({ "Caisse claire s\xC3\xA8" "che", { { "attack", 50 }, { "sustain", -50 }, { "speed", 0 } } });
        m.presets.push_back ({ "Toms courts", { { "attack", 30 }, { "sustain", -60 }, { "speed", 1 } } });
        m.presets.push_back ({ "Plus de pi\xC3\xA8" "ce", { { "attack", 0 }, { "sustain", 60 }, { "speed", 2 } } });
        m.presets.push_back ({ "Basse percutante", { { "attack", 45 }, { "sustain", 0 }, { "speed", 1 } } });
        m.presets.push_back ({ "Pluck plus court", { { "attack", 20 }, { "sustain", -40 }, { "speed", 0 } } });
        m.presets.push_back ({ "Nappe adoucie", { { "attack", -50 }, { "sustain", 20 }, { "speed", 2 } } });
        m.presets.push_back ({ "Charley serr\xC3\xA9", { { "attack", 40 }, { "sustain", -70 }, { "speed", 0 } } });
        m.presets.push_back ({ "Attaque adoucie", { { "attack", -60 }, { "sustain", 0 }, { "speed", 1 } } });
        m.presets.push_back ({ "Batterie \xC3\xA9" "cr\xC3\xAAt\xC3\xA9" "e", { { "attack", 80 }, { "sustain", 10 }, { "clip", 1 }, { "output", -3 } } });
        m.presets.push_back ({ "Piano pr\xC3\xA9sent", { { "attack", 30 }, { "sustain", 10 }, { "speed", 1 } } });
        m.presets.push_back ({ "Guitare plus nette", { { "attack", 35 }, { "sustain", -15 }, { "speed", 0 } } });
        m.presets.push_back ({ "Queue allong\xC3\xA9" "e", { { "attack", 0 }, { "sustain", 80 }, { "speed", 2 }, { "mix", 70 } } });
        m.presets.push_back ({ "Coup de poing", { { "attack", 100 }, { "sustain", -30 }, { "speed", 0 }, { "clip", 1 } } });
        m.presets.push_back ({ "Transitoires en parall\xC3\xA8le", { { "attack", 90 }, { "sustain", -40 }, { "mix", 50 } } });
        m.presets.push_back ({ "Douceur g\xC3\xA9n\xC3\xA9rale", { { "attack", -30 }, { "sustain", -20 }, { "speed", 2 } } });
    }
    if (m.type == "delay")
    {
        m.presets.push_back ({ "Croche simple", { { "mode", 0 }, { "division", 7 }, { "feedback", 30 }, { "mix", 25 } } });
        m.presets.push_back ({ "Noire point\xC3\xA9" "e", { { "mode", 0 }, { "division", 12 }, { "feedback", 40 }, { "mix", 25 } } });
        m.presets.push_back ({ "Ping-pong croche", { { "mode", 2 }, { "division", 7 }, { "feedback", 45 } } });
        m.presets.push_back ({ "Triolets", { { "mode", 2 }, { "division", 5 }, { "feedback", 40 } } });
        m.presets.push_back ({ "\xC3\x89" "chos diffus", { { "mode", 1 }, { "division", 9 }, { "diffusion", 70 }, { "feedback", 50 } } });
        m.presets.push_back ({ "Seau BBD", { { "echocolour", 4 }, { "mode", 4 }, { "division", 7 }, { "feedback", 50 } } });
        m.presets.push_back ({ "Effacement sur la voix", { { "mode", 2 }, { "division", 9 }, { "feedback", 50 }, { "ducking", 80 } } });
        m.presets.push_back ({ "Gel", { { "freeze", 1 }, { "mix", 40 }, { "feedback", 100 } } });
        m.presets.push_back ({ "Swing", { { "mode", 2 }, { "division", 7 }, { "swing", 60 }, { "feedback", 40 } } });
        m.presets.push_back ({ "\xC3\x89troit", { { "mode", 1 }, { "division", 10 }, { "width", 30 }, { "feedback", 35 } } });
    }
    if (m.type == "tapeecho")
    {
        m.presets.push_back ({ "T\xC3\xAAte 1 courte", { { "heads", 0 }, { "speed", 90 }, { "intensity", 20 }, { "echo", 40 } } });
        m.presets.push_back ({ "T\xC3\xAAte 2 et ressort", { { "heads", 1 }, { "spring", 50 }, { "echo", 55 } } });
        m.presets.push_back ({ "Trois t\xC3\xAAtes", { { "heads", 6 }, { "intensity", 55 }, { "echo", 55 } } });
        m.presets.push_back ({ "Croche au tempo", { { "sync", 1 }, { "division", 5 }, { "heads", 0 }, { "intensity", 40 } } });
        m.presets.push_back ({ "Noire point\xC3\xA9" "e au tempo", { { "sync", 1 }, { "division", 7 }, { "heads", 2 }, { "intensity", 45 } } });
        m.presets.push_back ({ "Dub qui s'emballe", { { "heads", 4 }, { "intensity", 95 }, { "wear", 50 }, { "drive", 40 } } });
        m.presets.push_back ({ "Bande neuve", { { "wear", 0 }, { "drive", 10 } } });
        m.presets.push_back ({ "Ressort et \xC3\xA9" "cho doux", { { "spring", 70 }, { "echo", 35 }, { "intensity", 30 } } });
        m.presets.push_back ({ "Sombre", { { "treble", -10 }, { "bass", 3 }, { "intensity", 50 } } });
        m.presets.push_back ({ "Brillant", { { "treble", 4 }, { "bass", -4 }, { "intensity", 45 } } });
        m.presets.push_back ({ "Bande us\xC3\xA9" "e sale", { { "wear", 90 }, { "drive", 60 }, { "intensity", 55 } } });
        m.presets.push_back ({ "T\xC3\xAAtes 1 et 3", { { "heads", 5 }, { "intensity", 50 } } });
        m.presets.push_back ({ "Slapback rockabilly", { { "heads", 0 }, { "speed", 120 }, { "intensity", 5 }, { "echo", 50 } } });
        m.presets.push_back ({ "Nappe en \xC3\xA9" "cho", { { "heads", 6 }, { "intensity", 70 }, { "echo", 45 }, { "treble", -6 } } });
    }
    if (m.type == "reverb")
    {
        m.presets.push_back ({ "Petite chambre", { { "size", 25 }, { "decay", 0.8f }, { "predelay", 5 } } });
        m.presets.push_back ({ "Studio bois\xC3\xA9", { { "size", 35 }, { "decay", 1.1f }, { "brightness", 5000 }, { "character", 60 } } });
        m.presets.push_back ({ "Grande salle sombre", { { "size", 80 }, { "decay", 3.5f }, { "brightness", 3500 } } });
        m.presets.push_back ({ "Ar\xC3\xA8ne", { { "size", 95 }, { "decay", 5 }, { "predelay", 40 } } });
        m.presets.push_back ({ "Voix lointaine", { { "size", 60 }, { "decay", 2.5f }, { "distance", 85 }, { "predelay", 60 } } });
        m.presets.push_back ({ "Batterie en salle", { { "size", 50 }, { "decay", 1.4f }, { "lowcut", 200 } } });
        m.presets.push_back ({ "Grave d\xC3\xA9gag\xC3\xA9", { { "size", 55 }, { "decay", 2 }, { "lowcut", 400 } } });
        m.presets.push_back ({ "Brillante courte", { { "size", 30 }, { "decay", 0.9f }, { "brightness", 12000 } } });
        m.presets.push_back ({ "Nappe immense", { { "size", 100 }, { "decay", 12 }, { "mix", 40 } } });
        m.presets.push_back ({ "\xC3\x89troite", { { "size", 45 }, { "decay", 1.6f }, { "width", 30 } } });
        m.presets.push_back ({ "Tr\xC3\xA8s large", { { "size", 60 }, { "decay", 2.4f }, { "width", 150 } } });
        m.presets.push_back ({ "Gel", { { "freeze", 1 }, { "mix", 50 } } });
        m.presets.push_back ({ "Caverne", { { "size", 100 }, { "decay", 8 }, { "brightness", 2500 }, { "predelay", 80 } } });
    }
    if (m.type == "vintverb")
    {
        m.presets.push_back ({ "Plaque brillante 80", { { "mode", 1 }, { "color", 1 }, { "decay", 1.8f }, { "damping", 20 } } });
        m.presets.push_back ({ "Chambre 70", { { "mode", 2 }, { "color", 0 }, { "decay", 1.5f } } });
        m.presets.push_back ({ "Concert actuel", { { "mode", 3 }, { "color", 2 }, { "decay", 4 } } });
        m.presets.push_back ({ "Ambiance courte", { { "mode", 4 }, { "decay", 0.7f } } });
        m.presets.push_back ({ "Lumineuse longue", { { "mode", 5 }, { "decay", 8 }, { "modulation", 60 } } });
        m.presets.push_back ({ "Salle sombre 70", { { "mode", 0 }, { "color", 0 }, { "decay", 3 }, { "damping", 70 } } });
        m.presets.push_back ({ "Caisse claire plaque", { { "mode", 1 }, { "decay", 1.2f }, { "predelay", 20 }, { "lowcut", 200 } } });
        m.presets.push_back ({ "Voix 80", { { "mode", 1 }, { "color", 1 }, { "decay", 2.2f }, { "predelay", 40 } } });
        m.presets.push_back ({ "Gel lumineux", { { "mode", 5 }, { "freeze", 1 } } });
        m.presets.push_back ({ "Petite salle", { { "mode", 0 }, { "size", 30 }, { "decay", 1.2f } } });
        m.presets.push_back ({ "Nappe modul\xC3\xA9" "e", { { "mode", 3 }, { "decay", 6 }, { "modulation", 80 } } });
        m.presets.push_back ({ "Grave tenu", { { "mode", 0 }, { "bass", 90 }, { "decay", 3 } } });
        m.presets.push_back ({ "Large 80", { { "mode", 0 }, { "color", 1 }, { "width", 150 }, { "decay", 2.5f } } });
    }
    if (m.type == "plate")
    {
        m.presets.push_back ({ "Plaque moyenne", { { "decay", 1.6f } } });
        m.presets.push_back ({ "Plaque courte", { { "decay", 0.7f }, { "brightness", 70 } } });
        m.presets.push_back ({ "Plaque longue", { { "decay", 4.5f } } });
        m.presets.push_back ({ "Plaque sombre", { { "decay", 2.2f }, { "brightness", 25 } } });
        m.presets.push_back ({ "Plaque tr\xC3\xA8s brillante", { { "decay", 1.8f }, { "brightness", 95 } } });
        m.presets.push_back ({ "Voix avec retard", { { "decay", 2 }, { "predelay", 60 } } });
        m.presets.push_back ({ "Batterie serr\xC3\xA9" "e", { { "decay", 0.9f }, { "lowcut", 300 }, { "mix", 20 } } });
        m.presets.push_back ({ "Grave d\xC3\xA9gag\xC3\xA9", { { "decay", 2 }, { "lowcut", 400 } } });
        m.presets.push_back ({ "\xC3\x89troite", { { "decay", 1.6f }, { "width", 40 } } });
        m.presets.push_back ({ "Tr\xC3\xA8s large", { { "decay", 2 }, { "width", 150 } } });
        m.presets.push_back ({ "Nappe de plaque", { { "decay", 5.5f }, { "brightness", 50 }, { "mix", 40 } } });
        m.presets.push_back ({ "Guitare", { { "decay", 1.4f }, { "predelay", 25 } } });
        m.presets.push_back ({ "Piano", { { "decay", 2.2f }, { "brightness", 55 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 lead", { { "decay", 1.8f }, { "predelay", 40 }, { "mix", 25 } } });
        m.presets.push_back ({ "Caisse claire 80", { { "decay", 2.5f }, { "brightness", 80 }, { "mix", 40 } } });
        m.presets.push_back ({ "Plaque discr\xC3\xA8te", { { "decay", 1.2f }, { "mix", 15 } } });
    }
    if (m.type == "spring")
    {
        m.presets.push_back ({ "Un ressort", { { "springs", 1 }, { "decay", 45 } } });
        m.presets.push_back ({ "Trois ressorts", { { "springs", 3 }, { "decay", 60 } } });
        m.presets.push_back ({ "Ressort court", { { "decay", 25 } } });
        m.presets.push_back ({ "Ressort long", { { "decay", 85 } } });
        m.presets.push_back ({ "Tr\xC3\xA8s gouttelant", { { "drip", 90 } } });
        m.presets.push_back ({ "Sans goutte", { { "drip", 0 } } });
        m.presets.push_back ({ "Tendu", { { "tension", 90 } } });
        m.presets.push_back ({ "D\xC3\xA9tendu", { { "tension", 10 } } });
        m.presets.push_back ({ "Sombre", { { "tone", 20 } } });
        m.presets.push_back ({ "Brillant", { { "tone", 85 } } });
        m.presets.push_back ({ "Guitare surf", { { "springs", 3 }, { "drip", 80 }, { "decay", 60 }, { "mix", 45 } } });
        m.presets.push_back ({ "Orgue", { { "springs", 2 }, { "decay", 50 }, { "tone", 40 }, { "mix", 25 } } });
        m.presets.push_back ({ "Batterie dub", { { "springs", 2 }, { "decay", 75 }, { "drip", 70 }, { "mix", 40 } } });
        m.presets.push_back ({ "Voix r\xC3\xA9tro", { { "springs", 2 }, { "decay", 40 }, { "tone", 60 }, { "mix", 25 } } });
        m.presets.push_back ({ "Synth\xC3\xA9", { { "springs", 3 }, { "decay", 55 }, { "mix", 30 } } });
        m.presets.push_back ({ "Effet de choc", { { "springs", 3 }, { "decay", 90 }, { "drip", 100 }, { "tension", 80 }, { "mix", 60 } } });
    }
    if (m.type == "prestige")
    {
        m.presets.push_back ({ "Salle de concert", { { "mode", 0 }, { "size", 150 }, { "decay", 3 } } });
        m.presets.push_back ({ "Chambre courte", { { "mode", 1 }, { "size", 50 }, { "decay", 0.9f } } });
        m.presets.push_back ({ "Plaque longue", { { "mode", 2 }, { "decay", 3.5f } } });
        m.presets.push_back ({ "Cath\xC3\xA9" "drale", { { "mode", 3 }, { "size", 200 }, { "decay", 8 } } });
        m.presets.push_back ({ "Studio moyen", { { "mode", 4 }, { "size", 80 }, { "decay", 1.2f } } });
        m.presets.push_back ({ "Halo court", { { "mode", 5 }, { "decay", 4 } } });
        m.presets.push_back ({ "Grave allong\xC3\xA9", { { "mode", 0 }, { "lowmult", 1.8f }, { "decay", 2.5f } } });
        m.presets.push_back ({ "Aigus courts", { { "mode", 0 }, { "highmult", 0.3f } } });
        m.presets.push_back ({ "Premi\xC3\xA8res r\xC3\xA9" "flexions", { { "mode", 1 }, { "early", 90 }, { "decay", 0.6f } } });
        m.presets.push_back ({ "Modul\xC3\xA9" "e", { { "mode", 0 }, { "modulation", 80 }, { "decay", 3 } } });
        m.presets.push_back ({ "Retard long", { { "mode", 0 }, { "predelay", 120 }, { "decay", 2.5f } } });
        m.presets.push_back ({ "Gel", { { "mode", 5 }, { "freeze", 1 } } });
        m.presets.push_back ({ "\xC3\x89troite", { { "mode", 4 }, { "width", 40 } } });
        m.presets.push_back ({ "Infinie", { { "mode", 5 }, { "decay", 30 }, { "mix", 40 } } });
    }
    if (m.type == "chorus")
    {
        m.presets.push_back ({ "Trois voix", { { "voices", 1 }, { "rate", 0.5f }, { "depth", 2.5f } } });
        m.presets.push_back ({ "Quatre voix larges", { { "voices", 2 }, { "rate", 0.4f }, { "depth", 3.5f } } });
        m.presets.push_back ({ "Chorus rapide", { { "rate", 2.5f }, { "depth", 1.5f } } });
        m.presets.push_back ({ "Chorus lent", { { "rate", 0.15f }, { "depth", 4 } } });
        m.presets.push_back ({ "Clavier \xC3\xA9lectrique", { { "rate", 0.8f }, { "depth", 2 }, { "mix", 40 } } });
        m.presets.push_back ({ "Basse", { { "rate", 0.6f }, { "depth", 1.5f }, { "tone", 4000 }, { "mix", 35 } } });
        m.presets.push_back ({ "Guitare claire", { { "rate", 0.9f }, { "depth", 2.5f }, { "mix", 45 } } });
        m.presets.push_back ({ "Nappe large", { { "voices", 2 }, { "rate", 0.25f }, { "depth", 4 }, { "mix", 55 } } });
        m.presets.push_back ({ "Flanger doux", { { "delay", 2 }, { "feedback", 40 }, { "depth", 2 }, { "rate", 0.3f } } });
        m.presets.push_back ({ "Cal\xC3\xA9 sur la mesure", { { "sync", 1 }, { "division", 4 }, { "depth", 3 } } });
        m.presets.push_back ({ "Cal\xC3\xA9 \xC3\xA0 la noire", { { "sync", 1 }, { "division", 6 }, { "depth", 2 } } });
        m.presets.push_back ({ "Sombre", { { "tone", 3000 }, { "depth", 3 } } });
        m.presets.push_back ({ "\xC3\x89troit", { { "width", 30 }, { "depth", 2 } } });
        m.presets.push_back ({ "R\xC3\xA9inject\xC3\xA9", { { "feedback", 50 }, { "depth", 2 } } });
        m.presets.push_back ({ "Vibrato rapide", { { "mix", 100 }, { "rate", 6 }, { "depth", 1.5f } } });
        m.presets.push_back ({ "Vibrato lent", { { "mix", 100 }, { "rate", 1.5f }, { "depth", 3 } } });
    }
    if (m.type == "phaser")
    {
        m.presets.push_back ({ "Quatre \xC3\xA9tages", { { "stages", 0 }, { "rate", 0.5f } } });
        m.presets.push_back ({ "Douze \xC3\xA9tages", { { "stages", 3 }, { "rate", 0.3f }, { "depth", 80 } } });
        m.presets.push_back ({ "Rapide", { { "rate", 3 }, { "depth", 60 } } });
        m.presets.push_back ({ "Tr\xC3\xA8s lent", { { "rate", 0.05f }, { "depth", 90 } } });
        m.presets.push_back ({ "Grave", { { "center", 300 } } });
        m.presets.push_back ({ "Aigu", { { "center", 3000 } } });
        m.presets.push_back ({ "R\xC3\xA9injection forte", { { "feedback", 85 } } });
        m.presets.push_back ({ "R\xC3\xA9injection invers\xC3\xA9" "e", { { "feedback", -70 } } });
        m.presets.push_back ({ "Mono", { { "stereo", 0 } } });
        m.presets.push_back ({ "Croche", { { "sync", 1 }, { "division", 7 } } });
        m.presets.push_back ({ "Noire", { { "sync", 1 }, { "division", 6 } } });
        m.presets.push_back ({ "Deux mesures", { { "sync", 1 }, { "division", 3 } } });
        m.presets.push_back ({ "Doux", { { "depth", 40 }, { "mix", 35 }, { "feedback", 10 } } });
        m.presets.push_back ({ "Guitare funk", { { "stages", 0 }, { "rate", 1.2f }, { "depth", 70 }, { "feedback", 30 } } });
        m.presets.push_back ({ "Nappe qui tourne", { { "stages", 2 }, { "rate", 0.12f }, { "depth", 85 }, { "mix", 60 } } });
        m.presets.push_back ({ "Clavier \xC3\xA9lectrique", { { "stages", 0 }, { "rate", 0.7f }, { "feedback", 20 } } });
        m.presets.push_back ({ "Effet de jet", { { "stages", 3 }, { "rate", 0.08f }, { "depth", 100 }, { "feedback", 80 }, { "mix", 60 } } });
    }
    if (m.type == "flanger")
    {
        m.presets.push_back ({ "Rapide", { { "rate", 2 }, { "depth", 50 } } });
        m.presets.push_back ({ "Grave", { { "manual", 6 }, { "feedback", 40 } } });
        m.presets.push_back ({ "Aigu", { { "manual", 0.6f } } });
        m.presets.push_back ({ "R\xC3\xA9injection forte", { { "feedback", 90 } } });
        m.presets.push_back ({ "Invers\xC3\xA9 lent", { { "mode", 1 }, { "rate", 0.1f }, { "feedback", -70 } } });
        m.presets.push_back ({ "Croche", { { "sync", 1 }, { "division", 7 } } });
        m.presets.push_back ({ "Noire", { { "sync", 1 }, { "division", 6 } } });
        m.presets.push_back ({ "Doux", { { "depth", 30 }, { "mix", 30 }, { "feedback", 20 } } });
        m.presets.push_back ({ "Mono", { { "stereo", 0 } } });
        m.presets.push_back ({ "Peigne grave fig\xC3\xA9", { { "mode", 2 }, { "manual", 8 }, { "feedback", 70 } } });
        m.presets.push_back ({ "Batterie", { { "rate", 0.15f }, { "depth", 70 }, { "feedback", 60 }, { "mix", 40 } } });
        m.presets.push_back ({ "Guitare", { { "rate", 0.4f }, { "depth", 60 }, { "feedback", 50 } } });
        m.presets.push_back ({ "Sombre", { { "tone", 3500 } } });
        m.presets.push_back ({ "Nappe", { { "rate", 0.05f }, { "depth", 80 }, { "feedback", 40 }, { "mix", 40 } } });
    }
    if (m.type == "phaser2")
    {
        m.presets.push_back ({ "Deux lents", { { "rateA", 0.1f }, { "rateB", 0.15f } } });
        m.presets.push_back ({ "Deux rapides", { { "rateA", 2 }, { "rateB", 2.7f } } });
        m.presets.push_back ({ "Parall\xC3\xA8le grave et aigu", { { "routing", 1 }, { "centerA", 300 }, { "centerB", 3000 } } });
        m.presets.push_back ({ "S\xC3\xA9rie doux", { { "routing", 0 }, { "depthA", 40 }, { "depthB", 40 }, { "feedbackA", 10 }, { "feedbackB", 10 } } });
        m.presets.push_back ({ "St\xC3\xA9r\xC3\xA9o cal\xC3\xA9", { { "routing", 2 }, { "sync", 1 }, { "divisionA", 4 }, { "divisionB", 5 } } });
        m.presets.push_back ({ "Douze \xC3\xA9tages", { { "stagesA", 3 }, { "stagesB", 3 } } });
        m.presets.push_back ({ "Quatre \xC3\xA9tages", { { "stagesA", 0 }, { "stagesB", 0 } } });
        m.presets.push_back ({ "R\xC3\xA9injection crois\xC3\xA9" "e", { { "feedbackA", 80 }, { "feedbackB", -60 } } });
        m.presets.push_back ({ "Croche et noire", { { "sync", 1 }, { "divisionA", 7 }, { "divisionB", 6 } } });
        m.presets.push_back ({ "Nappe", { { "rateA", 0.08f }, { "rateB", 0.11f }, { "depthA", 90 }, { "depthB", 80 }, { "mix", 60 } } });
        m.presets.push_back ({ "Guitare", { { "rateA", 0.8f }, { "rateB", 1.1f } } });
        m.presets.push_back ({ "Grave seul", { { "centerA", 250 }, { "centerB", 400 } } });
        m.presets.push_back ({ "Aigus seuls", { { "centerA", 2500 }, { "centerB", 5000 } } });
        m.presets.push_back ({ "Effet de jet", { { "stagesA", 3 }, { "rateA", 0.06f }, { "depthA", 100 }, { "feedbackA", 85 } } });
        m.presets.push_back ({ "Discret", { { "depthA", 30 }, { "depthB", 25 }, { "mix", 30 } } });
    }
    if (m.type == "dimension")
    {
        m.presets.push_back ({ "R\xC3\xA9glage 1 sombre", { { "mode", 0 }, { "tone", 5000 } } });
        m.presets.push_back ({ "R\xC3\xA9glage 2 \xC3\xA9troit", { { "mode", 1 }, { "width", 50 } } });
        m.presets.push_back ({ "R\xC3\xA9glage 3 brillant", { { "mode", 2 }, { "tone", 14000 } } });
        m.presets.push_back ({ "R\xC3\xA9glage 4 en parall\xC3\xA8le", { { "mode", 3 }, { "mix", 50 } } });
        m.presets.push_back ({ "Clavier", { { "mode", 1 }, { "mix", 60 } } });
        m.presets.push_back ({ "Basse", { { "mode", 0 }, { "tone", 4000 }, { "mix", 50 } } });
        m.presets.push_back ({ "Voix", { { "mode", 1 }, { "mix", 40 } } });
        m.presets.push_back ({ "Guitare", { { "mode", 2 }, { "mix", 70 } } });
        m.presets.push_back ({ "Synth\xC3\xA9 lead", { { "mode", 2 }, { "mix", 50 } } });
        m.presets.push_back ({ "Nappe tr\xC3\xA8s large", { { "mode", 3 }, { "width", 100 } } });
        m.presets.push_back ({ "Piano", { { "mode", 0 }, { "mix", 60 } } });
        m.presets.push_back ({ "Discret", { { "mode", 0 }, { "mix", 30 } } });
        m.presets.push_back ({ "Mono \xC3\xA9largi", { { "mode", 3 }, { "width", 100 }, { "mix", 80 } } });
        m.presets.push_back ({ "Ch\xC5\x93ur", { { "mode", 2 }, { "tone", 8000 }, { "mix", 70 } } });
        m.presets.push_back ({ "Cordes", { { "mode", 3 }, { "tone", 9000 }, { "mix", 80 } } });
    }
    if (m.type == "chorus3")
    {
        m.presets.push_back ({ "Chorus rapide", { { "rate", 2 }, { "depth", 40 } } });
        m.presets.push_back ({ "Chorus lent", { { "rate", 0.15f }, { "depth", 70 } } });
        m.presets.push_back ({ "Ensemble lent", { { "mode", 2 }, { "rate", 0.3f }, { "depth", 70 } } });
        m.presets.push_back ({ "Ensemble rapide", { { "mode", 2 }, { "rate", 1.5f } } });
        m.presets.push_back ({ "Vibrato profond", { { "mode", 1 }, { "rate", 4 }, { "depth", 80 } } });
        m.presets.push_back ({ "Vibrato l\xC3\xA9ger", { { "mode", 1 }, { "rate", 5 }, { "depth", 25 } } });
        m.presets.push_back ({ "Sombre", { { "tone", 3500 } } });
        m.presets.push_back ({ "\xC3\x89troit", { { "width", 30 } } });
        m.presets.push_back ({ "Cal\xC3\xA9 sur la mesure", { { "sync", 1 }, { "division", 4 } } });
        m.presets.push_back ({ "Cal\xC3\xA9 \xC3\xA0 la noire", { { "sync", 1 }, { "division", 6 } } });
        m.presets.push_back ({ "Nappe", { { "mode", 2 }, { "rate", 0.2f }, { "depth", 80 }, { "mix", 60 } } });
        m.presets.push_back ({ "Clavier", { { "rate", 0.7f }, { "depth", 50 }, { "mix", 40 } } });
        m.presets.push_back ({ "Basse", { { "rate", 0.5f }, { "depth", 30 }, { "tone", 3000 }, { "mix", 35 } } });
        m.presets.push_back ({ "Guitare", { { "rate", 0.9f }, { "depth", 55 }, { "mix", 45 } } });
        m.presets.push_back ({ "Ch\xC5\x93ur de voix", { { "mode", 2 }, { "rate", 0.4f }, { "depth", 60 }, { "mix", 50 } } });
        m.presets.push_back ({ "Discret", { { "mix", 25 }, { "depth", 30 } } });
    }
    if (m.type == "stutter")
    {
        m.presets.push_back ({ "Roulement 1/8", { { "division", 1 }, { "mode", 1 } } });
        m.presets.push_back ({ "Roulement 1/32", { { "division", 5 }, { "mode", 1 } } });
        m.presets.push_back ({ "Roulement 1/64", { { "division", 6 }, { "mode", 1 }, { "length", 0 } } });
        m.presets.push_back ({ "Triolets", { { "division", 2 }, { "mode", 1 } } });
        m.presets.push_back ({ "Hasard 1/16", { { "division", 3 }, { "mode", 2 }, { "chance", 40 } } });
        m.presets.push_back ({ "Hasard rare", { { "division", 4 }, { "mode", 2 }, { "chance", 15 } } });
        m.presets.push_back ({ "Qui descend", { { "division", 3 }, { "mode", 1 }, { "pitch", -12 } } });
        m.presets.push_back ({ "Qui s'\xC3\xA9teint", { { "division", 3 }, { "mode", 1 }, { "decay", 60 } } });
        m.presets.push_back ({ "Hach\xC3\xA9", { { "division", 3 }, { "mode", 1 }, { "gate", 50 } } });
        m.presets.push_back ({ "Hach\xC3\xA9 serr\xC3\xA9", { { "division", 5 }, { "mode", 1 }, { "gate", 30 } } });
        m.presets.push_back ({ "Une mesure", { { "division", 3 }, { "mode", 1 }, { "length", 2 } } });
        m.presets.push_back ({ "Deux mesures", { { "division", 1 }, { "mode", 1 }, { "length", 3 } } });
        m.presets.push_back ({ "Demi-mesure", { { "division", 3 }, { "mode", 1 }, { "length", 1 } } });
        m.presets.push_back ({ "Plong\xC3\xA9" "e", { { "division", 5 }, { "mode", 1 }, { "pitch", -24 }, { "decay", 40 } } });
        m.presets.push_back ({ "En parall\xC3\xA8le", { { "division", 3 }, { "mode", 1 }, { "mix", 50 } } });
        m.presets.push_back ({ "Hasard qui s'\xC3\xA9teint", { { "division", 4 }, { "mode", 2 }, { "chance", 50 }, { "decay", 50 } } });
        m.presets.push_back ({ "Arr\xC3\xAAt", { { "mode", 0 } } });
    }
    if (m.type == "filter")
    {
        m.presets.push_back ({ "Passe-bas lent", { { "type", 0 }, { "cutoff", 800 }, { "lfodepth", 60 }, { "division", 3 } } });
        m.presets.push_back ({ "Passe-haut qui monte", { { "type", 4 }, { "cutoff", 200 }, { "lfodepth", 50 }, { "division", 2 } } });
        m.presets.push_back ({ "Bande qui chante", { { "type", 2 }, { "cutoff", 1000 }, { "reso", 70 }, { "lfodepth", 60 }, { "division", 5 } } });
        m.presets.push_back ({ "Auto-wah rapide", { { "type", 2 }, { "cutoff", 600 }, { "reso", 60 }, { "envdepth", 80 }, { "envspeed", 30 } } });
        m.presets.push_back ({ "Wah lent", { { "type", 1 }, { "cutoff", 500 }, { "reso", 50 }, { "envdepth", 60 }, { "envspeed", 300 } } });
        m.presets.push_back ({ "Croches filtr\xC3\xA9" "es", { { "type", 0 }, { "cutoff", 600 }, { "reso", 40 }, { "lfodepth", 60 }, { "division", 7 } } });
        m.presets.push_back ({ "Doubles croches", { { "type", 0 }, { "cutoff", 900 }, { "reso", 50 }, { "lfodepth", 50 }, { "division", 8 } } });
        m.presets.push_back ({ "Passe-bas ferm\xC3\xA9", { { "type", 0 }, { "cutoff", 400 }, { "reso", 30 } } });
        m.presets.push_back ({ "Passe-haut t\xC3\xA9l\xC3\xA9phone", { { "type", 3 }, { "cutoff", 800 } } });
        m.presets.push_back ({ "R\xC3\xA9sonance satur\xC3\xA9" "e", { { "type", 0 }, { "cutoff", 700 }, { "reso", 85 }, { "drive", 12 } } });
        m.presets.push_back ({ "Passe-bas 12 doux", { { "type", 1 }, { "cutoff", 2000 }, { "reso", 10 } } });
        m.presets.push_back ({ "Balayage 4 mesures", { { "type", 0 }, { "cutoff", 300 }, { "lfodepth", 90 }, { "division", 2 } } });
        m.presets.push_back ({ "Triolets", { { "type", 1 }, { "cutoff", 800 }, { "reso", 40 }, { "lfodepth", 55 }, { "division", 11 } } });
        m.presets.push_back ({ "Libre lent", { { "sync", 0 }, { "rate", 0.2f }, { "type", 0 }, { "cutoff", 900 }, { "lfodepth", 60 } } });
        m.presets.push_back ({ "Bande \xC3\xA9troite", { { "type", 2 }, { "cutoff", 1500 }, { "reso", 85 } } });
        m.presets.push_back ({ "En parall\xC3\xA8le", { { "type", 0 }, { "cutoff", 500 }, { "reso", 60 }, { "mix", 50 } } });
    }
    if (m.type == "mdelay")
    {
        m.presets.push_back ({ "Num\xC3\xA9rique croche", { { "mode", 0 }, { "division", 7 }, { "feedback", 35 } } });
        m.presets.push_back ({ "Bande noire", { { "mode", 1 }, { "division", 10 }, { "feedback", 45 } } });
        m.presets.push_back ({ "Seau croche point\xC3\xA9" "e", { { "mode", 2 }, { "division", 9 }, { "feedback", 50 } } });
        m.presets.push_back ({ "Lo-fi noire", { { "mode", 3 }, { "division", 10 }, { "grit", 60 } } });
        m.presets.push_back ({ "Inverse croche", { { "mode", 4 }, { "division", 7 }, { "feedback", 40 } } });
        m.presets.push_back ({ "Filtre lent", { { "mode", 5 }, { "modspeed", 0.2f }, { "moddepth", 60 } } });
        m.presets.push_back ({ "Double croche", { { "mode", 6 }, { "division", 7 } } });
        m.presets.push_back ({ "Ping-pong triolets", { { "mode", 7 }, { "division", 8 } } });
        m.presets.push_back ({ "Effacement sur la voix", { { "mode", 0 }, { "division", 9 }, { "ducking", 80 } } });
        m.presets.push_back ({ "Modul\xC3\xA9", { { "mode", 1 }, { "moddepth", 60 }, { "modspeed", 1.2f } } });
        m.presets.push_back ({ "Sombre", { { "mode", 2 }, { "tone", 1500 } } });
        m.presets.push_back ({ "Brillant", { { "mode", 0 }, { "tone", 14000 } } });
    }
    if (m.type == "stepfx")
    {
        m.presets.push_back ({ "Roulements de fin", { { "s13", 10 }, { "s14", 10 }, { "s15", 9 }, { "s16", 9 } } });
        m.presets.push_back ({ "Inverse au contretemps", { { "s3", 2 }, { "s7", 2 }, { "s11", 2 }, { "s15", 2 } } });
        m.presets.push_back ({ "Filtres altern\xC3\xA9s", { { "s1", 19 }, { "s5", 20 }, { "s9", 21 }, { "s13", 22 } } });
        m.presets.push_back ({ "\xC3\x89" "crase sur les temps", { { "s1", 25 }, { "s5", 25 }, { "s9", 25 }, { "s13", 25 } } });
        m.presets.push_back ({ "Hachure en 1/32", { { "s9", 32 }, { "s10", 32 }, { "s11", 32 }, { "s12", 32 } } });
        m.presets.push_back ({ "\xC3\x89" "chos de fin", { { "s14", 36 }, { "s16", 37 } } });
        m.presets.push_back ({ "Ralentis", { { "s5", 40 }, { "s13", 41 } } });
        m.presets.push_back ({ "Fondus", { { "s1", 44 }, { "s16", 43 } } });
        m.presets.push_back ({ "Pompe", { { "s1", 46 }, { "s5", 46 }, { "s9", 46 }, { "s13", 46 } } });
        m.presets.push_back ({ "Arr\xC3\xAAt de bande final", { { "s16", 29 } } });
    }
    if (m.type == "sculptor")
    {
        m.presets.push_back ({ "Pompe 1/8", { { "s1on", 1 }, { "s1shape", 5 }, { "s1len", 1 }, { "s1amt", 70 } } });
        m.presets.push_back ({ "Porte 1/16", { { "s1on", 1 }, { "s1shape", 7 }, { "s1len", 4 }, { "s1amt", 80 } } });
        m.presets.push_back ({ "Escalier", { { "s1on", 1 }, { "s1shape", 9 }, { "s1len", 4 }, { "s1amt", 60 } } });
        m.presets.push_back ({ "Rebond", { { "s1on", 1 }, { "s1shape", 10 }, { "s1len", 4 }, { "s1amt", 60 } } });
        m.presets.push_back ({ "Grave qui monte", { { "s1on", 1 }, { "s1shape", 12 }, { "s1len", 5 }, { "s1amt", 70 }, { "s1band", 1 } } });
        m.presets.push_back ({ "Aigus point\xC3\xA9s", { { "s1on", 1 }, { "s1shape", 8 }, { "s1len", 4 }, { "s1amt", 70 }, { "s1band", 3 } } });
        m.presets.push_back ({ "Pompe douce 1/4", { { "s1on", 1 }, { "s1shape", 6 }, { "s1len", 2 }, { "s1amt", 60 } } });
        m.presets.push_back ({ "Hasard sur le m\xC3\xA9" "dium", { { "s1on", 1 }, { "s1shape", 11 }, { "s1len", 0 }, { "s1amt", 60 }, { "s1band", 2 } } });
    }
    if (m.type == "looper")
    {
        m.presets.push_back ({ "Deux mesures cal\xC3\xA9" "es", { { "length", 2 }, { "start", 1 } } });
        m.presets.push_back ({ "Huit mesures cal\xC3\xA9" "es", { { "length", 4 }, { "start", 1 } } });
        m.presets.push_back ({ "Boucle \xC3\xA0 double vitesse", { { "speed", 2 } } });
        m.presets.push_back ({ "Boucle \xC3\xA0 demi-vitesse", { { "speed", 0 } } });
        m.presets.push_back ({ "Boucle \xC3\xA0 l'envers", { { "reverse", 1 } } });
        m.presets.push_back ({ "Surcouches qui durent", { { "keep", 95 } } });
        m.presets.push_back ({ "Surcouches rapides", { { "keep", 60 } } });
        m.presets.push_back ({ "Boucle discr\xC3\xA8te", { { "level", -9 } } });
        m.presets.push_back ({ "Une mesure \xC3\xA0 l'envers", { { "length", 1 }, { "start", 1 }, { "reverse", 1 } } });
        m.presets.push_back ({ "Deux mesures sans direct", { { "length", 2 }, { "start", 1 }, { "dry", 0 } } });
        m.presets.push_back ({ "Demi-vitesse \xC3\xA0 l'envers", { { "speed", 0 }, { "reverse", 1 } } });
        m.presets.push_back ({ "Quatre mesures, surcouches qui s'effacent", { { "length", 3 }, { "start", 1 }, { "keep", 75 } } });
        m.presets.push_back ({ "Libre \xC3\xA0 l'envers", { { "reverse", 1 }, { "start", 0 } } });
        m.presets.push_back ({ "Boucle au premier plan", { { "level", 3 }, { "dry", 60 } } });
        m.presets.push_back ({ "Une mesure double vitesse", { { "length", 1 }, { "start", 1 }, { "speed", 2 } } });
    }
}

} // namespace grv::effects
