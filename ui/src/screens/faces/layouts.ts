// Groovebox — OFAD — AGPL-3.0-or-later
// Panneaux des instruments, disposés comme sur la famille de machines dont chacun reprend
// l'architecture (sections, commandes « marque de fabrique »). Voir SynthPanel.tsx.

import type { Ctl, FaceLayout, Panel } from "./SynthPanel";

const BUS_SOURCES = ["Tri.", "Carré", "Scie", "Rampe", "É-B", "Env. F", "Env. A", "Sinus", "Bruit", "Cste"];

/** Basse mono : grand monophonique à deux oscillateurs (bus de modulation à gauche, oscillateurs,
    mélangeur, filtre ; en dessous glisse, deux enveloppes DAHDSR, arpège, sortie). */
const mono: FaceLayout = {
  theme: "fatsub",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Modulation", banks: [{ label: "Bus 1", value: "1" }, { label: "Bus 2", value: "2" }], cols: 4, grow: 1.25,
            ctl: [
              { id: "mb{b}_rate", k: "big", label: "Vitesse" },
              { id: "mb{b}_pitch", label: "Hauteur" },
              { id: "mb{b}_filt", label: "Filtre" },
              { id: "mb{b}_src", k: "vswitch", label: "Source", short: BUS_SOURCES },
              { id: "mb{b}_sync", k: "small", label: "Synchro" },
              { id: "mb{b}_pgm", label: "Programm." },
              { id: "mb{b}_gdst", k: "small", label: "Destination" },
              { stack: [
                { id: "mb{b}_pdst", k: "switch", label: "Hauteur de", short: ["1+2", "1", "2"] },
                { id: "mb{b}_ctrl", k: "switch", label: "Commande", short: ["Molette", "Press.", "—"] },
                { id: "mb{b}_reset", label: "Redém. clavier" },
              ] },
            ] },
          { title: "Oscillateurs", cols: 5, grow: 1.6,
            ctl: [
              { id: "o1oct", k: "switch", label: "Octave 1", short: ["32'", "16'", "8'", "4'", "2'"] },
              { id: "o1shape", k: "big", label: "Forme 1" },
              { id: "o2oct", k: "switch", label: "Octave 2", short: ["32'", "16'", "8'", "4'", "2'"] },
              { id: "o2shape", k: "big", label: "Forme 2" },
              { stack: [
                { id: "sync", label: "Synchro dure" },
                { id: "mode", label: "Duo" },
                { id: "o2kb", label: "Clavier osc. 2", on: 0 },
                { id: "oscreset", label: "Phase redém." },
              ] },
              { id: "o2semi", label: "Fréquence 2" },
              { id: "o2fine", k: "small", label: "Fin 2" },
              { id: "o2beat", k: "small", label: "Battement" },
              { id: "subshape", label: "Sous-osc. sinus" },
              { id: "drift", k: "small", label: "Dérive" },
            ] },
          { title: "Mélangeur", cols: 2, grow: 0.7,
            ctl: ["mix1", "mixsub", "mix2", "mixnoise", { id: "mixfb", label: "Réinjection" }] },
          { title: "Filtre", cols: 3, grow: 1.1,
            ctl: [
              { id: "cutoff", k: "big", label: "Coupure" },
              { id: "reso", label: "Résonance" },
              { id: "drive", label: "Multidrive" },
              { id: "slope", k: "switch", label: "Pente", short: ["24", "18", "12", "6"] },
              { id: "envamt", label: "Qté env." },
              { id: "keytrack", label: "Clavier" },
            ] },
        ],
        [
          { title: "Glisse", cols: 2, grow: 0.9,
            ctl: [
              { id: "glide", k: "big", label: "Temps" },
              { stack: [
                { id: "glidemode", label: "Lié seulement", on: 1 },
                { id: "glidetype", k: "switch", label: "Type", short: ["Temps", "Vitesse", "Expo"] },
              ] },
              { id: "glideosc", k: "switch", label: "Sur", short: ["1+2", "1", "2"] },
              { id: "priority", k: "switch", label: "Priorité", short: ["Basse", "Haute", "Dern."] },
            ] },
          { title: "Enveloppe filtre", cols: 5, grow: 1.5, visual: "fenv",
            ctl: [
              { id: "fa", label: "Attaque" }, { id: "fd", label: "Déclin" }, { id: "fs", label: "Niveau" }, { id: "fr", label: "Relâch." },
              { stack: [{ id: "floop", label: "Boucle" }, { id: "fsync", label: "Synchro" }, { id: "retrig", label: "Multi" }] },
              { id: "fdly", k: "small", label: "Retard" }, { id: "fhld", k: "small", label: "Tenue" },
              { id: "velfilter", k: "small", label: "Vélocité" }, { id: "fkb", k: "small", label: "Clavier" },
            ] },
          { title: "Enveloppe ampli", cols: 5, grow: 1.5, visual: "aenv",
            ctl: [
              { id: "aa", label: "Attaque" }, { id: "ad", label: "Déclin" }, { id: "as", label: "Niveau" }, { id: "ar", label: "Relâch." },
              { stack: [{ id: "aloop", label: "Boucle" }, { id: "async", label: "Synchro" }, { id: "alatch", label: "Verrou" }] },
              { id: "adly", k: "small", label: "Retard" }, { id: "ahld", k: "small", label: "Tenue" },
              { id: "velamp", k: "small", label: "Vélocité" }, { id: "akb", k: "small", label: "Clavier" },
            ] },
          { title: "Arpège", cols: 2, grow: 0.8,
            ctl: [
              { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" },
              { id: "arp_oct", k: "small", label: "Octaves" }, { id: "arp_gate", k: "small", label: "Durée" },
              { id: "arp_hold", label: "Maintien" }, { id: "arp_swing", k: "small", label: "Swing" },
            ] },
          { title: "Sortie", cols: 1, grow: 0.5,
            ctl: [{ id: "level", k: "big", label: "Volume" }, { id: "tune", k: "small", label: "Accord" }, { id: "pb_range", k: "small", label: "Pitch bend" }] },
        ],
      ],
    },
  ],
};

/** Basse acide : la rangée de boutons de la petite basse à séquenceur (forme, accord, coupure,
    résonance, modulation d'enveloppe, déclin, accent), puis les ajouts, puis ses pas. */
const acid: FaceLayout = {
  theme: "acid",
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateur", cols: 2, grow: 0.6, ctl: [{ id: "wave", k: "switch", label: "Forme", short: ["Scie", "Carré"] }, { id: "tune", label: "Accord" }] },
          { title: "Filtre et enveloppe", cols: 5, grow: 2.4, ctl: [
            { id: "cutoff", k: "big", label: "Coupure" }, { id: "reso", k: "big", label: "Résonance" }, { id: "envmod", k: "big", label: "Mod. env." },
            { id: "decay", k: "big", label: "Déclin" }, { id: "accent", k: "big", label: "Accent" },
          ] },
          { title: "Volume", cols: 1, grow: 0.5, ctl: [{ id: "level", k: "big", label: "Volume" }] },
        ],
        [
          { title: "En plus", cols: 8, grow: 1, ctl: [
            { id: "glide", label: "Glissé" }, { id: "accthr", label: "Seuil accent" }, { id: "fine", k: "small", label: "Fin" }, { id: "keytrack", label: "Suivi clavier" },
            { id: "drive", label: "Saturation" }, { id: "dist", label: "Distorsion" }, { id: "ampdecay", label: "Déclin ampli" },
          ] },
        ],
        [
          { title: "Pas de la séquence · note, octave, vélocité, durée, accent, glissé", cols: 1, grow: 1, ctl: [], custom: "steps303" },
        ],
      ],
    },
  ],
};

/** Batterie analogique à machines : le pad choisi par pages (écran + 8 boutons), le kit, les pas. */
const drumA: FaceLayout = {
  theme: "machine",
  pages: [{
    id: "sound", label: "Son",
    rows: [
      [{ title: "Pad choisi", ctl: [], custom: "elektron" }],
      [{ title: "Le kit", ctl: [], custom: "kitcols:3" }],
      [{ title: "Accent", ctl: [], custom: "accent", grow: 0 }, { title: "Pas du pad choisi", ctl: [], custom: "stepkeysR" }],
    ],
  }],
};

/** Batterie 80 : une colonne de boutons par instrument, puis les 16 touches de pas colorées. */
const drum80: FaceLayout = {
  theme: "boum",
  pages: [{
    id: "sound", label: "Son",
    rows: [
      [{ title: "Instruments", ctl: [], custom: "kitcols" }],
      [{ title: "Accent", ctl: [], custom: "accent", grow: 0 }, { title: "Pas de l'instrument choisi", ctl: [], custom: "stepkeys8" }],
    ],
  }],
};

/** Batterie 90 : même principe, façade grise. */
const drum90: FaceLayout = {
  theme: "club",
  pages: [{
    id: "sound", label: "Son",
    rows: [
      [{ title: "Instruments", ctl: [], custom: "kitcols" }],
      [{ title: "Accent", ctl: [], custom: "accent", grow: 0 }, { title: "Pas de l'instrument choisi", ctl: [], custom: "stepkeys9" }],
    ],
  }],
};


/** Poly analogique : la rangée unique des polyphoniques 5 voix de la fin des années 70 — Poly-Mod,
    LFO, Wheel-Mod, oscillateurs A et B, mélangeur, filtre, deux enveloppes, ampli —, ici sur deux
    rangées ; interrupteurs crème à LED pour les formes cumulables et les destinations. */
const poly5: FaceLayout = {
  theme: "five",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Poly-Mod", cols: 2, grow: 0.9, ctl: [
            { id: "pmenv", label: "Env. filtre" }, { id: "pmoscb", label: "Osc. B" },
            { stack: [{ id: "pmfreqa", label: "Fréq. A" }, { id: "pmpwa", label: "Largeur A" }, { id: "pmfilter", label: "Filtre" }] },
          ] },
          { title: "LFO", cols: 2, grow: 0.7, ctl: [
            { id: "lforate", label: "Fréquence" },
            { stack: [{ id: "lsaw", label: "Scie" }, { id: "ltri", label: "Triangle" }, { id: "lsquare", label: "Carré" }] },
            { id: "lfosync", k: "small", label: "Synchro" },
          ] },
          { title: "Wheel-Mod", cols: 3, grow: 1.1, ctl: [
            { id: "wmmix", label: "LFO ↔ bruit" }, { id: "wminit", label: "Qté initiale" },
            { stack: [{ id: "wmfreqa", label: "Fréq. A" }, { id: "wmfreqb", label: "Fréq. B" }, { id: "wmpwa", label: "Largeur A" }] },
            { id: "atwheel", k: "small", label: "Pression" }, { gap: true },
            { stack: [{ id: "wmpwb", label: "Largeur B" }, { id: "wmfilter", label: "Filtre" }] },
          ] },
          { title: "Oscillateur A", cols: 3, grow: 1, ctl: [
            { id: "asemi", k: "big", label: "Fréquence" },
            { stack: [{ id: "asaw", label: "Scie" }, { id: "apulse", label: "Impulsion" }, { id: "async", label: "Synchro" }] },
            { id: "apw", label: "Largeur" },
          ] },
          { title: "Oscillateur B", cols: 4, grow: 1.3, ctl: [
            { id: "bsemi", k: "big", label: "Fréquence" }, { id: "bfine", label: "Fin" },
            { stack: [{ id: "bsaw", label: "Scie" }, { id: "btri", label: "Triangle" }, { id: "bpulse", label: "Impulsion" }] },
            { stack: [{ id: "blow", label: "Basse fréq." }, { id: "bkey", label: "Clavier" }] },
            { id: "bpw", label: "Largeur" },
          ] },
        ],
        [
          { title: "Mélangeur", cols: 3, grow: 0.9, ctl: [{ id: "mixa", label: "Osc. A" }, { id: "mixb", label: "Osc. B" }, { id: "mixnoise", label: "Bruit" }] },
          { title: "Filtre", cols: 4, grow: 1.3, ctl: [
            { id: "cutoff", k: "big", label: "Coupure" }, { id: "reso", label: "Résonance" }, { id: "envamt", label: "Qté env." }, { id: "keytrack", label: "Clavier" },
            { id: "filterrev", k: "switch", label: "Révision", short: ["Anc.", "Réc."] }, { id: "velfilter", k: "small", label: "Vélocité" }, { id: "atfilter", k: "small", label: "Pression" },
          ] },
          { title: "Enveloppe filtre", cols: 4, grow: 1.1, visual: "fenv", ctl: [
            { id: "fa", label: "Attaque" }, { id: "fd", label: "Déclin" }, { id: "fs", label: "Maintien" }, { id: "fr", label: "Relâch." },
          ] },
          { title: "Ampli", cols: 4, grow: 1.1, visual: "aenv", ctl: [
            { id: "aa", label: "Attaque" }, { id: "ad", label: "Déclin" }, { id: "as", label: "Maintien" }, { id: "ar", label: "Relâch." },
          ] },
          { title: "Jeu", cols: 3, grow: 1, ctl: [
            { id: "glide", label: "Glide" }, { id: "level", k: "big", label: "Volume" }, { id: "tune", k: "small", label: "Accord" },
            { stack: [{ id: "unison", label: "Unisson" }, { id: "release", label: "Relâch." }] },
            { id: "unidetune", k: "small", label: "Écart" }, { id: "vintage", k: "small", label: "Vintage" },
            { id: "voices", k: "small", label: "Voix" }, { id: "velamp", k: "small", label: "Vél. ampli" }, { id: "pb_range", k: "small", label: "Pitch bend" },
          ] },
        ],
      ],
    },
    {
      id: "arp", label: "Arpège",
      rows: [
        [
          { title: "Arpège", cols: 6, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", k: "big", label: "Mode" }] }, { id: "arp_oct", label: "Octaves" }, { id: "arp_rate", label: "Division" },
            { id: "arp_gate", label: "Durée" }, { id: "arp_swing", label: "Swing" }, { id: "arp_hold", label: "Maintien" },
          ] },
        ],
      ],
    },
  ],
};

/** Poly chorus : panneau à curseurs du polyphonique à oscillateurs numériquement contrôlés —
    arpège, LFO, oscillateur, passe-haut, filtre, ampli, enveloppe, chorus I / II. */
const polyj: FaceLayout = {
  theme: "chorus",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Arpège", cols: 4, grow: 1.2, tone: "oklch(0.7 0.16 45)", ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] },
            { stack: [{ id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }] },
            { id: "arp_rate", k: "fader", label: "Division" }, { id: "arp_gate", k: "fader", label: "Durée" },
          ] },
          { title: "LFO", cols: 3, grow: 0.8, tone: "oklch(0.68 0.17 30)", ctl: [
            { id: "lforate", k: "fader", label: "Vitesse" }, { id: "lfodelay", k: "fader", label: "Montée" },
            { stack: [{ id: "lfotrig", k: "switch", label: "Déclench.", short: ["Auto", "Manuel"] }, { id: "lfosync", k: "small", label: "Synchro" }] },
          ] },
          { title: "Oscillateur", cols: 8, grow: 2.2, tone: "oklch(0.75 0.15 70)", ctl: [
            { id: "range", k: "vswitch", label: "Registre", short: ["16'", "8'", "4'"] },
            { id: "lfopitch", k: "fader", label: "LFO" }, { id: "pwmdepth", k: "fader", label: "PWM" },
            { stack: [{ id: "pwmsrc", k: "vswitch", label: "Source", short: ["Man.", "LFO", "Env."] }, { id: "pw", k: "small", label: "Largeur" }] },
            { stack: [{ id: "pulse", label: "Impulsion" }, { id: "saw", label: "Scie" }] },
            { id: "sub", k: "fader", label: "Sub" }, { id: "noise", k: "fader", label: "Bruit" },
          ] },
          { title: "Passe-haut", cols: 1, grow: 0.4, tone: "oklch(0.82 0.13 95)", ctl: [{ id: "hpf", k: "vswitch", label: "Fréq.", short: ["0", "1", "2", "3"] }] },
        ],
        [
          { title: "Filtre", cols: 7, grow: 2, tone: "oklch(0.7 0.12 230)", ctl: [
            { id: "cutoff", k: "fader", label: "Fréq." }, { id: "reso", k: "fader", label: "Rés." },
            { id: "envpol", k: "vswitch", label: "Env.", short: ["+", "−"] },
            { id: "envamt", k: "fader", label: "Env." }, { id: "lfofilter", k: "fader", label: "LFO" }, { id: "keytrack", k: "fader", label: "Clavier" },
            { id: "velfilter", k: "small", label: "Vélocité" },
          ] },
          { title: "Ampli", cols: 2, grow: 0.6, tone: "oklch(0.72 0.1 160)", ctl: [
            { id: "vca", k: "vswitch", label: "Mode", short: ["Env.", "Porte"] }, { id: "level", k: "fader", label: "Niveau" },
          ] },
          { title: "Enveloppe", cols: 4, grow: 1.1, tone: "oklch(0.7 0.14 300)", ctl: [
            { id: "ea", k: "fader", label: "A" }, { id: "ed", k: "fader", label: "D" }, { id: "es", k: "fader", label: "S" }, { id: "er", k: "fader", label: "R" },
          ] },
          { title: "Chorus", cols: 1, grow: 0.5, ctl: [
            { stack: [{ id: "chorus", label: "I", bit: 1 }, { id: "chorus", label: "II", bit: 2 }] },
          ] },
          { title: "Levier et jeu", cols: 3, grow: 0.9, ctl: [
            { id: "pb_range", label: "Oscillateur" }, { id: "bendvcf", label: "Filtre" }, { id: "tune", k: "small", label: "Accord" },
            { id: "voices", k: "small", label: "Voix" }, { id: "drift", k: "small", label: "Dérive" }, { id: "velamp", k: "small", label: "Vél. ampli" },
          ] },
        ],
      ],
    },
  ],
};

/** Poly hybride : panneau gris foncé des polyphoniques hybrides récents — oscillateurs 1/2/3 sur
    un même jeu de boutons (sélection de l'oscillateur), « Shape » et son effet, mélangeur,
    filtre à gros bouton, enveloppes à curseurs, LFO 1/2, matrice de 16 routages en 2e page. */
const hybrid: FaceLayout = {
  theme: "pointe",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Voix", cols: 2, grow: 0.8, ctl: [
            { id: "vmode", k: "vswitch", label: "Mode", short: ["Poly", "Mono", "Mono leg.", "Mono 2"] },
            { stack: [{ id: "glide", k: "small", label: "Glide" }, { id: "unidetune", k: "small", label: "Écart" }] },
            { id: "voices", k: "small", label: "Voix" }, { id: "spread", k: "small", label: "Étalement" },
          ] },
          { title: "Oscillateur", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }, { label: "3", value: "3" }], cols: 4, grow: 1.7, ctl: [
            { id: "o{b}shape", k: "big", label: "Forme" },
            { id: "o{b}samt", k: "big", label: "Shape" },
            { stack: [
              { id: "o{b}tbl", k: "switch", label: "Source", short: ["Analog.", "Table"] },
              { id: "o{b}smod", k: "switch", label: "Effet Shape", short: ["Synchro", "Densité"] },
            ] },
            { stack: [{ id: "o{b}semi", label: "Grossier" }, { id: "o{b}fine", k: "small", label: "Fin" }] },
          ] },
          { title: "Mélangeur", cols: 4, grow: 1.4, ctl: [
            { id: "mix1", label: "Osc. 1" }, { id: "mix2", label: "Osc. 2" }, { id: "mix3", label: "Osc. 3" }, { id: "mixring", label: "Anneau" },
            { id: "mixnoise", label: "Bruit" }, { id: "fm31", k: "small", label: "FM 3→1" }, { id: "fm12", k: "small", label: "FM 1→2" }, { id: "fm23", k: "small", label: "FM 2→3" },
          ] },
          { title: "Filtre", cols: 4, grow: 1.6, ctl: [
            { id: "cutoff", k: "big", label: "Fréquence" }, { id: "reso", label: "Résonance" }, { id: "predrive", label: "Overdrive" }, { id: "postdist", label: "Distorsion" },
            { id: "fmode", k: "vswitch", label: "Type", short: ["LP 24", "LP 12", "BP", "HP 12", "HP 24"] },
            { id: "envamt", label: "Env. 2" }, { id: "keytrack", k: "small", label: "Clavier" }, { id: "velfilter", k: "small", label: "Vélocité" },
          ] },
        ],
        [
          { title: "LFO", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 3, grow: 1, ctl: [
            { id: "l{b}rate", k: "big", label: "Vitesse" },
            { id: "l{b}shape", k: "vswitch", label: "Forme", short: ["Sin.", "Tri.", "Scie", "Carré", "A&M"] },
            { stack: [{ id: "l{b}sync", k: "small", label: "Synchro" }, { id: "l{b}delay", k: "small", label: "Retard" }] },
          ] },
          { title: "LFO 1 vers", cols: 1, grow: 0.5, ctl: [{ id: "l1pitch", label: "Hauteur" }, { id: "l1shapes", label: "Forme" }] },
          { title: "LFO 2 vers", cols: 1, grow: 0.5, ctl: [{ id: "l2cutoff", label: "Filtre" }, { id: "l2amp", label: "Ampli" }] },
          { title: "Env.", banks: [{ label: "Ampli", value: "a" }, { label: "Mod 1", value: "m1" }, { label: "Mod 2", value: "m2" }], cols: 4, grow: 1.4, tone: "oklch(0.68 0.2 25)", ctl: [
            { id: "{b}a", k: "fader", label: "A" }, { id: "{b}d", k: "fader", label: "D" }, { id: "{b}s", k: "fader", label: "S" }, { id: "{b}r", k: "fader", label: "R" },
          ] },
          { title: "Env. 3", cols: 1, grow: 0.5, ctl: [{ id: "m2pitch", label: "→ hauteur" }, { id: "m2shape", label: "→ forme" }] },
          { title: "Arpège", cols: 3, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small", label: "Durée" },
            { stack: [{ id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }] }, { id: "arp_swing", k: "small", label: "Swing" },
          ] },
          { title: "Sortie", cols: 1, grow: 0.5, ctl: [{ id: "level", k: "big", label: "Volume" }, { id: "tune", k: "small", label: "Accord" }, { id: "pb_range", k: "small", label: "Pitch bend" }] },
        ],
      ],
    },
    {
      id: "matrix", label: "Matrice",
      rows: [
        [
          { title: "Matrice de modulation · 16 routages", ctl: [], custom: "matrix", grow: 2 },
          { title: "LFO", banks: [{ label: "LFO 3", value: "3" }, { label: "LFO 4", value: "4" }], cols: 3, grow: 1, ctl: [
            { id: "lf{b}_rate", k: "big", label: "Vitesse" }, { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_sync", k: "small", label: "Synchro" },
            { id: "lf{b}_mode", k: "vswitch", label: "Mode", short: ["Voix", "Global", "Libre"] }, { stack: [{ id: "lf{b}_delay", k: "small", label: "Retard" }, { id: "lf{b}_fade", k: "small", label: "Fondu" }] }, { id: "lf{b}_phase", k: "small", label: "Phase" },
          ] },
        ],
      ],
    },
  ],
};

/** Wave morph : panneau noir à LED violettes des synthés à tables d'ondes récents — oscillateurs
    1/2 et leurs mutants (A et B), oscillateur 3, mélangeur, deux filtres, cinq enveloppes, cinq LFO
    (dont trois à pas), macros ; matrice de 32 routages et arpège à pas en 2e page. */
const morph: FaceLayout = {
  theme: "onde",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateur", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 3, grow: 1.1, ctl: [
            { id: "w{b}pos", k: "big", label: "Position" },
            { stack: [{ id: "w{b}semi", label: "Demi-tons" }, { id: "w{b}fine", k: "small", label: "Fin" }] },
            { id: "w{b}ratio", k: "small", label: "Rapport FM" },
          ] },
          { title: "Mutant", banks: [{ label: "1 A", value: "1mut" }, { label: "1 B", value: "1mut2" }, { label: "2 A", value: "2mut" }, { label: "2 B", value: "2mut2" }], cols: 2, grow: 1, ctl: [
            { id: "w{b}", k: "vswitch", label: "Type", short: ["Aucun", "FM", "Synchro", "Largeur", "Repli", "Pile", "Phase", "Harmon."] },
            { id: "w{b}amt", k: "big", label: "Intensité" },
          ] },
          { title: "Osc. 3", cols: 2, grow: 0.6, ctl: [{ id: "w3pos", label: "Forme" }, { id: "w3semi", label: "Demi-tons" }, { id: "w3fine", k: "small", label: "Fin" }] },
          { title: "Mélangeur", cols: 3, grow: 1, ctl: [
            { id: "mix1", label: "Osc. 1" }, { id: "mix2", label: "Osc. 2" }, { id: "mix3", label: "Osc. 3" }, { id: "mixring", label: "Anneau" }, { id: "mixnoise", label: "Bruit" },
            { id: "routing", k: "switch", label: "Filtres", short: ["Série", "Para."] },
          ] },
          { title: "Filtre 1", cols: 3, grow: 1.3, ctl: [
            { id: "f1cutoff", k: "big", label: "Coupure" },
            { id: "f1mode", k: "vswitch", label: "Type", short: ["LP 24", "LP 12", "BP", "HP 12", "LP 18", "LP 6", "HP 24", "Voyelle", "Peigne+", "Peigne−"] },
            { stack: [{ id: "f1reso", label: "Résonance" }, { id: "f1drive", k: "small", label: "Saturation" }] },
            { id: "env1f1", k: "small", label: "Env. 1" }, { id: "keytrack", k: "small", label: "Clavier" },
          ] },
          { title: "Filtre 2", cols: 2, grow: 0.9, ctl: [
            { id: "f2cutoff", k: "big", label: "Coupure" }, { id: "f2morph", label: "Bas › haut" }, { id: "f2reso", label: "Résonance" }, { id: "env1f2", k: "small", label: "Env. 1" },
          ] },
        ],
        [
          { title: "Env.", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }, { label: "3", value: "3" }], cols: 4, grow: 1.1, tone: "oklch(0.7 0.19 300)", ctl: [
            { id: "e{b}a", k: "fader", label: "A" }, { id: "e{b}d", k: "fader", label: "D" }, { id: "e{b}s", k: "fader", label: "S" }, { id: "e{b}r", k: "fader", label: "R" },
          ] },
          { title: "Env.", banks: [{ label: "4", value: "1" }, { label: "5", value: "2" }], cols: 4, grow: 1.2, ctl: [
            { id: "me{b}_dly", k: "small", label: "Retard" }, { id: "me{b}_att", label: "Attaque" }, { id: "me{b}_hld", k: "small", label: "Tenue" }, { id: "me{b}_dec", label: "Déclin" },
            { id: "me{b}_sus", label: "Niveau" }, { id: "me{b}_rel", label: "Relâch." },
            { stack: [{ id: "me{b}_loop", label: "Boucle" }, { id: "me{b}_sync", label: "Tempo" }] }, { id: "me{b}_vel", k: "small", label: "Vélocité" },
          ] },
          { title: "LFO", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 2, grow: 0.8, ctl: [
            { id: "l{b}rate", k: "big", label: "Vitesse" }, { id: "l{b}shape", k: "vswitch", label: "Forme", short: ["Sin.", "Tri.", "Scie", "Carré", "A&M"] },
            { id: "l{b}sync", k: "small", label: "Synchro" }, { id: "l{b}delay", k: "small", label: "Retard" },
          ] },
          { title: "LFO à pas", banks: [{ label: "3", value: "3" }, { label: "4", value: "4" }, { label: "5", value: "5" }], cols: 4, grow: 1.6, custom: "steps:lf{b}_s:lf{b}_steps", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", label: "Vitesse" }, { id: "lf{b}_steps", k: "small", label: "Pas" }, { id: "lf{b}_sync", k: "small", label: "Synchro" },
            { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
        ],
        [
          { title: "Macros", cols: 8, grow: 2, ctl: ["macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8"] },
          { title: "Voix", cols: 4, grow: 1.2, ctl: [
            { id: "vmode", k: "vswitch", label: "Mode", short: ["Poly", "Mono", "Unisson"] },
            { id: "univoices", k: "small", label: "Unisson" }, { id: "unidetune", k: "small", label: "Désacc." }, { id: "glide", k: "small", label: "Glide" },
          ] },
          { title: "Sortie", cols: 3, grow: 0.8, ctl: [{ id: "level", k: "big", label: "Volume" }, { id: "spread", k: "small", label: "Étalement" }, { id: "pb_range", k: "small", label: "Pitch bend" }] },
        ],
      ],
    },
    {
      id: "waves", label: "Ondes",
      rows: [[{ title: "Tables d'ondes · les ondes balayées par les oscillateurs 1 et 2, et l'onde lue", ctl: [], custom: "wavescan", grow: 2 }]],
    },
    {
      id: "matrix", label: "Matrice et arpège",
      rows: [
        [{ title: "Matrice de modulation · 32 routages", ctl: [], custom: "matrix", grow: 2 }],
        [
          { title: "Arpège", cols: 4, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small", label: "Durée" }, { id: "arp_swing", k: "small", label: "Swing" },
            { id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }, { id: "arp_steps", k: "small", label: "Pas" },
          ] },
          { title: "Pas de l'arpège", banks: [{ label: "Transposition", value: "t" }, { label: "Vélocité", value: "v" }], ctl: [], custom: "steps:arp_{b}:arp_steps", grow: 1.4 },
        ],
      ],
    },
  ],
};

/** Nappes : façade bleu ardoise des virtuels-analogiques à parties A/B — oscillateurs 1 et 2 de la
    partie choisie, filtre de partie, commande globale des filtres, amplis A/B, deux enveloppes de
    modulation, deux LFO ; valeurs sur petits écrans ; matrice et arpège à pas en 2e page. */
const PART = [{ label: "Partie A", value: "a" }, { label: "Partie B", value: "b" }];
const WAVES = ["Sin.", "Tri.", "Scie", "Carré", "Imp.", "Bruit", "Imp. ét.", "Tri-scie"];
const pads: FaceLayout = {
  theme: "nappe",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateur 1", banks: PART, cols: 5, grow: 1.6, ctl: [
            { id: "{b}wave", k: "vswitch", label: "Forme", short: WAVES },
            { stack: [{ id: "{b}oct", k: "small", label: "Octave" }, { id: "{b}semi", k: "small", label: "Note" }, { id: "{b}fine", k: "small", label: "Fin" }] },
            { stack: [{ id: "{b}unison", label: "Voix" }, { id: "{b}detune", label: "Désaccord" }] },
            { stack: [{ id: "{b}phase", k: "small", label: "Phase" }, { id: "{b}retrig", k: "switch", label: "Phase", short: ["Libre", "Fixe"] }, { id: "{b}inv", label: "Inverser" }] },
            { stack: [{ id: "{b}level", label: "Volume" }, { id: "{b}pan", k: "small", label: "Pan." }, { id: "{b}spread", k: "small", label: "Stéréo" }] },
          ] },
          { title: "Oscillateur 2", banks: PART, cols: 5, grow: 1.6, ctl: [
            { id: "{b}2wave", k: "vswitch", label: "Forme", short: WAVES },
            { stack: [{ id: "{b}2oct", k: "small", label: "Octave" }, { id: "{b}2semi", k: "small", label: "Note" }, { id: "{b}2fine", k: "small", label: "Fin" }] },
            { stack: [{ id: "{b}2unison", label: "Voix" }, { id: "{b}2detune", label: "Désaccord" }] },
            { stack: [{ id: "{b}2phase", k: "small", label: "Phase" }, { id: "{b}2retrig", k: "switch", label: "Phase", short: ["Libre", "Fixe"] }, { id: "{b}2inv", label: "Inverser" }] },
            { stack: [{ id: "{b}2level", label: "Volume" }, { id: "{b}2pan", k: "small", label: "Pan." }, { id: "{b}2spread", k: "small", label: "Stéréo" }] },
          ] },
          { title: "Filtre", banks: PART, cols: 3, grow: 1, ctl: [
            { id: "f{b}type", k: "vswitch", label: "Type", short: ["LP 12", "LP 24", "BP", "HP", "Aucun"] },
            { id: "f{b}cutoff", k: "big", label: "Coupure" },
            { stack: [{ id: "f{b}reso", label: "Résonance" }, { id: "f{b}drive", k: "small", label: "Saturation" }] },
          ] },
          { title: "Commande des filtres", cols: 2, grow: 0.8, ctl: [
            { id: "fccutoff", k: "big", label: "Coupure" }, { id: "fcreso", label: "Résonance" }, { id: "keytrack", k: "small", label: "Clavier" }, { id: "warmdrive", label: "Warm drive" },
          ] },
        ],
        [
          { title: "Ampli", banks: PART, cols: 4, grow: 1.2, ctl: [
            { id: "{b}a", label: "Attaque" }, { id: "{b}d", label: "Déclin" }, { id: "{b}s", label: "Maintien" }, { id: "{b}r", label: "Relâch." },
            { id: "{b}envown", k: "switch", label: "Ampli B", short: ["= A", "Propre"] },
          ] },
          { title: "Env. mod", banks: [{ label: "1", value: "m" }, { label: "2", value: "m2" }], cols: 4, grow: 1.1, ctl: [
            { id: "{b}a", label: "Attaque" }, { id: "{b}d", label: "Déclin" }, { id: "{b}s", label: "Maintien" }, { id: "{b}r", label: "Relâch." },
          ] },
          { title: "Env. mod 1 vers", cols: 3, grow: 0.8, ctl: [{ id: "modcutoffa", label: "Filtre A" }, { id: "modcutoffb", label: "Filtre B" }, { id: "modpitch", label: "Hauteur" }] },
          { title: "LFO", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 3, grow: 0.9, ctl: [
            { id: "l{b}rate", label: "Vitesse" }, { id: "l{b}shape", k: "vswitch", label: "Forme", short: ["Sin.", "Tri.", "Scie", "Carré", "A&M"] },
            { stack: [{ id: "l{b}sync", k: "small", label: "Synchro" }, { id: "l{b}delay", k: "small", label: "Retard" }] },
          ] },
          { title: "LFO vers", cols: 3, grow: 0.8, ctl: [{ id: "l1pitch", label: "1 → vibrato" }, { id: "l1pan", label: "1 → pan." }, { id: "l2cutoff", label: "2 → filtres" }] },
          { title: "Divers", cols: 3, grow: 1.1, ctl: [
            { id: "vmode", k: "vswitch", label: "Mode", short: ["Poly", "Mono", "Legato"] }, { id: "voices", k: "small", label: "Polyphonie" }, { id: "glide", k: "small", label: "Portamento" },
            { id: "chorusmix", label: "Chorus" }, { id: "width", label: "Largeur" }, { id: "level", k: "big", label: "Volume" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "Matrice et arpège",
      rows: [
        [{ title: "Matrice de modulation · 16 routages", ctl: [], custom: "matrix", grow: 2 }],
        [
          { title: "Arpège", cols: 4, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small", label: "Durée" }, { id: "arp_swing", k: "small", label: "Swing" },
            { id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }, { id: "arp_steps", k: "small", label: "Pas" }, { id: "pb_range", k: "small", label: "Pitch bend" },
          ] },
          { title: "Pas de l'arpège", banks: [{ label: "Transposition", value: "t" }, { label: "Vélocité", value: "v" }], ctl: [], custom: "steps:arp_{b}:arp_steps", grow: 1.4 },
        ],
      ],
    },
  ],
};

/** Sampler de performance : machine, écran, six boutons A à F par page, enregistreur.
    v0.46 fix16 : page « Routages » retirée (vide : les LFO se règlent dans la page de la piste). */
const octa: FaceLayout = {
  theme: "pieuvre",
  wheels: true,
  pages: [
    { id: "sound", label: "Son", rows: [[{ title: "Piste", ctl: [], custom: "octa" }]] },
  ],
};


/** Mono semi-modulaire : panneau noir, deux oscillateurs, les deux filtres en grand (coupure et
    « peak »), générateur de modulation, deux enveloppes ; les liens par défaut en boutons. */
const ms20: FaceLayout = {
  theme: "hurleur",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateur 1", cols: 2, grow: 0.9, ctl: [
            { id: "v1wave", k: "vswitch", label: "Forme", short: ["Tri.", "Scie", "Imp.", "Bruit"] }, { id: "v1scale", k: "vswitch", label: "Taille", short: ["32'", "16'", "8'", "4'"] },
            { id: "v1pw", label: "Largeur" }, { id: "v1level", k: "big", label: "Niveau" },
          ] },
          { title: "Oscillateur 2", cols: 2, grow: 0.9, ctl: [
            { id: "v2wave", k: "vswitch", label: "Forme", short: ["Scie", "Carré", "Étroite", "Anneau"] }, { id: "v2scale", k: "vswitch", label: "Taille", short: ["32'", "16'", "8'", "4'"] },
            { id: "v2pitch", label: "Écart" }, { id: "v2level", k: "big", label: "Niveau" },
          ] },
          { title: "Hauteur", cols: 2, grow: 0.8, ctl: [
            { id: "tune", label: "Accord" }, { id: "porta", label: "Portamento" }, { id: "pitchmg", label: "← Générateur" }, { id: "pitcheg1", label: "← Env. 1" },
          ] },
          { title: "Passe-haut", cols: 2, grow: 0.9, tone: "oklch(0.7 0.1 230)", ctl: [
            { id: "hpcut", k: "big", label: "Coupure" }, { id: "hppeak", k: "big", label: "Peak" }, { id: "hpmg", label: "← Générateur" }, { id: "hpeg2", label: "← Env. 2" },
          ] },
          { title: "Passe-bas", cols: 3, grow: 1.2, tone: "oklch(0.72 0.14 30)", ctl: [
            { id: "lpcut", k: "big", label: "Coupure" }, { id: "lppeak", k: "big", label: "Peak" }, { id: "drive", label: "Saturation" },
            { id: "lpmg", label: "← Générateur" }, { id: "lpeg2", label: "← Env. 2" },
          ] },
        ],
        [
          { title: "Générateur de modulation", cols: 5, grow: 1.3, ctl: [
            { id: "mgshape", k: "big", label: "Forme" }, { id: "mgrate", k: "big", label: "Vitesse" }, { id: "mgsync", k: "small", label: "Synchro" },
            { id: "mgdelay", label: "Retard" }, { id: "modsrc", k: "vswitch", label: "Sortie", short: ["Rampe", "Carré", "É-B"] },
          ] },
          { title: "Enveloppe 1", cols: 3, grow: 0.8, ctl: [{ id: "e1delay", label: "Retard" }, { id: "e1attack", label: "Attaque" }, { id: "e1release", label: "Relâch." }] },
          { title: "Enveloppe 2", cols: 5, grow: 1.3, ctl: [
            { id: "e2hold", label: "Maintien" }, { id: "e2attack", label: "Attaque" }, { id: "e2decay", label: "Déclin" }, { id: "e2sustain", label: "Tenue" }, { id: "e2release", label: "Relâch." },
          ] },
          { title: "Sortie", cols: 2, grow: 0.5, ctl: [{ id: "velamp", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" }] },
        ],
      ],
    },
  ],
};

/** Mono à mélangeur : panneau gris clair à curseurs, une rangée de sections de gauche à droite
    (modulateur, oscillateur, mélangeur, filtre, ampli, enveloppe) comme sur ces petits synthés. */
const sh101: FaceLayout = {
  theme: "monofirst",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Modulateur", cols: 4, grow: 1, ctl: [
            { id: "lforate", k: "fader", label: "Vitesse" }, { id: "lfodelay", k: "fader", label: "Retard" },
            { id: "lfowave", k: "vswitch", label: "Forme", short: ["Tri.", "Carré", "Aléa.", "Bruit"] }, { id: "lfosync", k: "small", label: "Synchro" },
          ] },
          { title: "Oscillateur", cols: 4, grow: 1.2, ctl: [
            { id: "vcolfo", k: "fader", label: "Mod" }, { id: "range", k: "vswitch", label: "Registre", short: ["16'", "8'", "4'", "2'"] },
            { id: "pw", k: "fader", label: "Largeur" }, { id: "pwmsrc", k: "vswitch", label: "Pilote", short: ["Man.", "LFO", "Env."] },
          ] },
          { title: "Mélangeur", cols: 5, grow: 1.4, ctl: [
            { id: "pulse", k: "fader", label: "Impulsion" }, { id: "saw", k: "fader", label: "Scie" }, { id: "sub", k: "fader", label: "Sous-osc." },
            { id: "subtype", k: "vswitch", label: "Sous", short: ["-1", "-2", "-2 imp."] }, { id: "noise", k: "fader", label: "Bruit" },
          ] },
          { title: "Filtre", cols: 5, grow: 1.5, tone: "oklch(0.72 0.14 30)", ctl: [
            { id: "cutoff", k: "fader", label: "Coupure" }, { id: "reso", k: "fader", label: "Résonance" }, { id: "envamt", k: "fader", label: "Env." },
            { id: "lfoamt", k: "fader", label: "Mod" }, { id: "kbd", k: "fader", label: "Clavier" },
          ] },
          { title: "Ampli", cols: 1, grow: 0.45, ctl: [{ id: "vcamode", k: "vswitch", label: "Mode", short: ["Env.", "Porte"] }] },
          { title: "Enveloppe", cols: 5, grow: 1.4, ctl: [
            { id: "envtrig", k: "vswitch", label: "Départ", short: ["Note", "Porte", "LFO"] },
            { id: "attack", k: "fader", label: "A" }, { id: "decay", k: "fader", label: "D" }, { id: "sustain", k: "fader", label: "S" }, { id: "release", k: "fader", label: "R" },
          ] },
        ],
        [
          { title: "Jeu", cols: 4, grow: 1, ctl: [
            { id: "portamode", k: "switch", label: "Portamento", short: ["Arrêt", "Marche", "Auto"] }, { id: "porta", label: "Temps" }, { id: "tune", label: "Accord" }, { id: "level", k: "big", label: "Volume" },
          ] },
          { title: "Arpège", cols: 5, grow: 1.6, ctl: [
            { id: "arp_mode", k: "small", label: "Arpège" }, { id: "arp_oct", label: "Octaves" }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", label: "Durée" }, { id: "arp_hold", k: "switch", label: "Maintien", short: ["Non", "Oui"] },
          ] },
        ],
      ],
    },
  ],
};

const ARP_PANEL = { title: "Arpège", cols: 4, grow: 1, ctl: [
  { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small" as const, label: "Durée" }, { id: "arp_swing", k: "small" as const, label: "Swing" },
  { id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch" as const, label: "Octaves" }, { id: "pb_range", k: "small" as const, label: "Pitch bend" },
] };
const SHAPES_BLO = ["Imp.", "Scie", "Tri.", "Sin.", "Rés.", "Voy.", "Org.", "Imp. t", "Plis", "Sync", "Num.", "Clo.", "D→V", "Bos.", "Ch.", "Esc."];
const BLO_OSC = [{ label: "1", value: "1" }, { label: "2", value: "2" }];
const BLO_ENV = [{ label: "Filtre", value: "1" }, { label: "Ampli", value: "2" }, { label: "3", value: "3" }, { label: "4", value: "4" }];
const BLO_LFO = [{ label: "1", value: "1" }, { label: "2", value: "2" }, { label: "3", value: "3" }];

/** Poly à tables d'ondes : boîtier de bureau gris métal, écran, trois oscillateurs (deux à tables),
    deux filtres en série ou parallèle, quatre enveloppes à cinq modes, trois LFO, matrice. */
const blo: FaceLayout = {
  theme: "bretzel",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateurs 1 et 2", banks: BLO_OSC, bankAll: true, cols: 5, grow: 1.7, ctl: [
            { id: "o{b}shape", k: "vswitch", label: "Forme / table", short: SHAPES_BLO, vcols: 2 },
            { stack: [{ id: "o{b}oct", k: "small", label: "Octave" }, { id: "o{b}semi", k: "small", label: "Demi-tons" }, { id: "o{b}detune", k: "small", label: "Désaccord" }] },
            { id: "o{b}pos", k: "big", label: "Largeur / pos." },
            { stack: [{ id: "o{b}pwm", label: "Modulation" }, { id: "o{b}pwmsrc", k: "small", label: "par" }] },
            { stack: [{ id: "o{b}fm", label: "FM" }, { id: "o{b}fmsrc", k: "small", label: "depuis" }, { id: "o{b}level", label: "Niveau" }] },
          ] },
          { title: "Oscillateur 3", cols: 3, grow: 0.9, ctl: [
            { id: "o3shape", k: "vswitch", label: "Forme", short: ["Imp.", "Scie", "Tri.", "Sin."] },
            { stack: [{ id: "o3oct", k: "small", label: "Octave" }, { id: "o3semi", k: "small", label: "Demi-tons" }, { id: "o3detune", k: "small", label: "Désaccord" }] },
            { stack: [{ id: "o3pw", label: "Largeur" }, { id: "o3level", label: "Niveau" }, { id: "o2sync", k: "switch", label: "Synchro 2", short: ["Non", "Oui"] }] },
          ] },
          { title: "Mélangeur", cols: 3, grow: 0.9, ctl: [
            { id: "ring", label: "Anneau 1×2" }, { id: "noise", label: "Bruit" }, { id: "noisecol", label: "Couleur" }, { id: "vibrato", label: "Vibrato" },
          ] },
        ],
        [
          { title: "Filtres", banks: BLO_OSC, bankAll: true, cols: 4, grow: 1.6, tone: "oklch(0.75 0.13 60)", ctl: [
            { id: "f{b}type", k: "vswitch", label: "Type", short: ["Arrêt", "LP24", "LP12", "BP24", "BP12", "HP24", "HP12", "Réj24", "Réj12", "Pgn+", "Pgn−"], vcols: 2 },
            { id: "f{b}cut", k: "big", label: "Coupure" },
            { stack: [{ id: "f{b}reso", label: "Résonance" }, { id: "f{b}drive", label: "Saturation" }, { id: "f{b}curve", k: "small", label: "Courbe" }] },
            { stack: [{ id: "f{b}env", label: "Env." }, { id: "f{b}key", k: "small", label: "Clavier" }, { id: "f{b}vel", k: "small", label: "Vélocité" }] },
          ] },
          { title: "Routage", cols: 1, grow: 0.5, ctl: [{ id: "routing", k: "switch", label: "Filtres", short: ["Série", "Parallèle"] }, { id: "fbalance", label: "F1 ↔ F2" }] },
          { title: "Enveloppes", banks: BLO_ENV, cols: 4, grow: 1.6, ctl: [
            { id: "e{b}mode", k: "vswitch", label: "Mode", short: ["ADSR", "ADS1DS2R", "Un coup", "Boucle S", "Boucle"] },
            { stack: [{ id: "e{b}a", label: "Attaque" }, { id: "e{b}d", label: "Déclin" }] },
            { stack: [{ id: "e{b}s", label: "Niveau 1" }, { id: "e{b}d2", label: "Déclin 2" }] },
            { stack: [{ id: "e{b}s2", label: "Niveau 2" }, { id: "e{b}r", label: "Relâch." }] },
          ] },
          { title: "LFO", banks: BLO_LFO, cols: 3, grow: 0.9, ctl: [
            { id: "l{b}shape", k: "vswitch", label: "Forme", short: ["Sin.", "Tri.", "Scie", "Carré", "Aléa."] },
            { id: "l{b}rate", label: "Vitesse" },
            { stack: [{ id: "l{b}sync", k: "small", label: "Synchro" }, { id: "l{b}delay", k: "small", label: "Retard" }] },
          ] },
          { title: "Jeu", cols: 3, flowCols: 9, grow: 1, ctl: [
            { id: "vmode", k: "vswitch", label: "Mode", short: ["Poly", "Mono", "Unisson"] }, { id: "voices", k: "small", label: "Voix" }, { id: "glide", k: "small", label: "Portamento" },
            { id: "univoices", k: "small", label: "Unisson" }, { id: "unidetune", k: "small", label: "Désaccord" }, { id: "spread", k: "small", label: "Étalement" },
            { id: "tune", k: "small", label: "Accord" }, { id: "velamp", k: "small", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "Matrice et arpège",
      rows: [[{ title: "Matrice de modulation · 16 routages", ctl: [], custom: "matrix", grow: 2 }], [ARP_PANEL]],
    },
  ],
};

/** Poly à filtre d'état : panneau noir, joues en bois, deux oscillateurs, filtre à mode continu
    (passe-bas → réjection → passe-haut, bouton passe-bande), X-Mod, LFO partagé. */
const ob6: FaceLayout = {
  theme: "grace",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateur 1", cols: 3, grow: 1, ctl: [
            { id: "afreq", k: "big", label: "Fréquence" }, { id: "ashape", label: "Forme" }, { id: "apw", label: "Largeur" },
            { id: "async", k: "switch", label: "Synchro", short: ["Non", "Oui"] },
          ] },
          { title: "Oscillateur 2", cols: 4, grow: 1.3, ctl: [
            { id: "bfreq", k: "big", label: "Fréquence" }, { id: "bfine", label: "Fin" }, { id: "bshape", label: "Forme" }, { id: "bpw", label: "Largeur" },
            { id: "blow", k: "switch", label: "Basse fréq.", short: ["Non", "Oui"] }, { id: "bkey", k: "switch", label: "Clavier", short: ["Non", "Oui"] },
          ] },
          { title: "Mélangeur", cols: 2, grow: 0.8, ctl: [{ id: "mixa", label: "Osc. 1" }, { id: "mixsub", label: "Sous-osc." }, { id: "mixb", label: "Osc. 2" }, { id: "mixnoise", label: "Bruit" }] },
          { title: "Filtre d'état", cols: 4, grow: 1.6, tone: "oklch(0.72 0.15 25)", ctl: [
            { id: "cutoff", k: "big", label: "Coupure" }, { id: "reso", label: "Résonance" }, { id: "fmode", k: "big", label: "Mode" },
            { stack: [{ id: "envamt", label: "Env." }, { id: "keytrack", k: "switch", label: "Clavier", short: ["0", "½", "1"] }, { id: "bandpass", k: "switch", label: "Bande", short: ["Non", "Oui"] }] },
            { id: "velenv", k: "small", label: "Vél. env." }, { id: "atfilter", k: "small", label: "Pression" },
          ] },
        ],
        [
          { title: "X-Mod", cols: 4, grow: 1.3, ctl: [
            { id: "xenv", label: "Env. filtre" }, { id: "xoscb", label: "Osc. 2" },
            { stack: [{ id: "xfreqa", k: "toggle", label: "Fréq. 1" }, { id: "xshapea", k: "toggle", label: "Forme 1" }, { id: "xpwa", k: "toggle", label: "Largeur 1" }] },
            { stack: [{ id: "xcut", k: "toggle", label: "Coupure" }, { id: "xmode", k: "toggle", label: "Mode" }] },
          ] },
          { title: "LFO", cols: 4, grow: 1.5, ctl: [
            { id: "lforate", label: "Vitesse" }, { id: "lfosync", k: "small", label: "Synchro" }, { id: "lfoshape", k: "vswitch", label: "Forme", short: ["Sin.", "Scie", "Rampe", "Carré", "Aléa."] },
            { stack: [{ id: "lfoamt", label: "Quantité" }, { id: "atlfo", k: "small", label: "Pression" }] },
          ] },
          { title: "LFO vers", cols: 3, grow: 0.9, ctl: [
            { id: "lfofreqa", k: "toggle", label: "Fréq. 1" }, { id: "lfofreqb", k: "toggle", label: "Fréq. 2" }, { id: "lfopw", k: "toggle", label: "Largeur" },
            { id: "lfocut", k: "toggle", label: "Coupure" }, { id: "lfomode", k: "toggle", label: "Mode" }, { id: "lfoamp", k: "toggle", label: "Volume" },
          ] },
          { title: "Env. filtre", cols: 4, grow: 1.1, ctl: [{ id: "fa", k: "fader", label: "A" }, { id: "fd", k: "fader", label: "D" }, { id: "fs", k: "fader", label: "S" }, { id: "fr", k: "fader", label: "R" }] },
          { title: "Env. ampli", cols: 4, grow: 1.1, ctl: [{ id: "aa", k: "fader", label: "A" }, { id: "ad", k: "fader", label: "D" }, { id: "as", k: "fader", label: "S" }, { id: "ar", k: "fader", label: "R" }] },
        ],
        [
          { title: "Jeu", cols: 8, grow: 2, ctl: [
            { id: "voices", k: "small", label: "Voix" }, { id: "unison", k: "switch", label: "Unisson", short: ["Non", "Oui"] }, { id: "univoices", k: "small", label: "Voix unisson" }, { id: "unidetune", k: "small", label: "Désaccord" },
            { id: "glide", k: "small", label: "Portamento" }, { id: "vintage", label: "Vintage" }, { id: "tune", k: "small", label: "Accord" }, { id: "velamp", k: "small", label: "Vélocité" },
          ] },
          { title: "Sortie", cols: 1, grow: 0.4, ctl: [{ id: "level", k: "big", label: "Volume" }] },
          ARP_PANEL,
        ],
      ],
    },
  ],
};

const ORB_PAT = ["A", "B", "C", "D", "E", "F", "N", "W"];
const ORB_CORNERS = [{ label: "A", value: "a" }, { label: "B", value: "b" }, { label: "C", value: "c" }, { label: "D", value: "d" }];

/** Orbiteur (principe des synthés vectoriels à orbite) : l'écran de l'orbite au centre, les quatre
    coins en banques, la trajectoire, la sous-orbite, les effets. */
const orbit: FaceLayout = {
  theme: "orbite",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Orbite · les coins A à D mélangés par l'orbe vert", ctl: [], custom: "orbiter", grow: 1.8 },
          { title: "Coin", banks: ORB_CORNERS, cols: 5, grow: 2.1, custom: "orbwaves:{b}", copyExtra: ["{b}w1", "{b}w2"], ctl: [
            { id: "{b}g1", k: "vswitch", label: "Gén. 1", short: ORB_PAT }, { id: "{b}g2", k: "vswitch", label: "Gén. 2", short: ORB_PAT },
            { stack: [{ id: "{b}ratio", k: "small", label: "Rapport" }, { id: "{b}blend", k: "big", label: "Mélange" }, { id: "{b}bmode", k: "small", label: "Modulation" }] },
            { stack: [{ id: "{b}ftype", k: "small", label: "Filtre" }, { id: "{b}cut", label: "Coupure" }, { id: "{b}reso", label: "Résonance" }] },
            { stack: [{ id: "{b}ph1", label: "Phase 1" }, { id: "{b}ph2", label: "Phase 2" }] },
          ] },
        ],
        [
          { title: "Trajectoire", cols: 5, grow: 1.6, tone: "oklch(0.72 0.14 240)", ctl: [
            { id: "shape", k: "vswitch", label: "Forme", short: ["Man.", "ADSR", "Liss. 1", "Liss. 2", "Liss. 3", "Poly.", "Imp."] },
            { stack: [{ id: "speed", k: "small", label: "Vitesse" }, { id: "retrig", k: "switch", label: "Départ", short: ["Libre", "Note"] }] },
            { stack: [{ id: "size", label: "Taille" }, { id: "rotate", label: "Rotation" }] },
            { stack: [{ id: "cx", label: "Centre X" }, { id: "cy", label: "Centre Y" }] },
            { stack: [{ id: "ngon", k: "small", label: "Côtés" }, { id: "pulsew", k: "small", label: "Impulsion" }] },
          ] },
          { title: "Déformation et sous-orbite", cols: 3, grow: 1.1, tone: "oklch(0.75 0.15 150)", ctl: [
            { stack: [{ id: "warpx", label: "Déf. X" }, { id: "warpy", label: "Déf. Y" }] },
            { stack: [{ id: "subsize", label: "Taille" }, { id: "subrate", label: "Vitesse" }] },
            { id: "subsync", k: "small", label: "Synchro" },
          ] },
          { title: "Env. de l'orbite", cols: 5, grow: 1.2, ctl: [
            { id: "orbenv", k: "vswitch", label: "Env.", short: ["= ampli", "Propre"] },
            { id: "oa", k: "fader", label: "A" }, { id: "od", k: "fader", label: "D" }, { id: "os", k: "fader", label: "S" }, { id: "or", k: "fader", label: "R" },
          ] },
          { title: "Ampli", cols: 4, grow: 1, ctl: [{ id: "aa", k: "fader", label: "A" }, { id: "ad", k: "fader", label: "D" }, { id: "as", k: "fader", label: "S" }, { id: "ar", k: "fader", label: "R" }] },
        ],
        [
          // v0.43.6 : filtre du mélange en haut (coupure, résonance), le reste dessous — noms entiers
          { title: "Filtres", cols: 3, grow: 1.4, wideLabels: true, ctl: [
            { id: "mixcut", label: "Coupure" }, { id: "mixreso", label: "Résonance" }, { gap: true },
            { id: "gres", label: "Résonance globale" }, { id: "keytrack", label: "Clavier" }, { id: "mixtype", k: "small", label: "Filtre du mélange" },
          ] },
          { title: "Effets", cols: 9, grow: 2.2, ctl: [
            { id: "vibdepth", label: "Vibrato" }, { id: "vibrate", k: "small", label: "Vitesse" }, { id: "tremdepth", label: "Trémolo" }, { id: "tremrate", k: "small", label: "Vitesse" },
            { id: "drive", label: "Saturation" }, { id: "drivecol", k: "small", label: "Couleur" }, { id: "chorus", label: "Chorus" }, { id: "chorusrate", k: "small", label: "Vitesse" }, { id: "chorusdepth", k: "small", label: "Profondeur" },
          ] },
          { title: "Jeu", cols: 6, grow: 1.6, ctl: [
            { id: "vmode", k: "vswitch", label: "Mode", short: ["Poly", "Pile"] }, { id: "voices", k: "small", label: "Voix" }, { id: "stack", k: "small", label: "Pile" },
            { id: "stackdet", k: "small", label: "Désaccord" }, { id: "glide", k: "small", label: "Portamento" }, { id: "tune", k: "small", label: "Accord" },
            { id: "drift", k: "small", label: "Dérive" }, { id: "scatter", k: "small", label: "Stéréo" }, { id: "velamp", k: "small", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "Modulations et arpège",
      rows: [[{ title: "Matrice de modulation · 8 routages", ctl: [], custom: "matrix", grow: 2 }], [ARP_PANEL]],
    },
  ],
};

/** Old Metal (FM à 6 opérateurs) : l'algorithme en écran (toucher un opérateur le choisit), puis
    l'opérateur choisi (fréquence, niveau, sensibilités), son enveloppe à 4 vitesses / 4 niveaux et son
    échelle au clavier ; en dessous le global, le LFO, l'enveloppe de hauteur et les contrôleurs. */
const FM_OPS = [1, 2, 3, 4, 5, 6].map((k) => ({ label: `OP ${k}`, value: String(k) }));
const FM_CURVES = ["−Lin", "−Exp", "+Exp", "+Lin"];
const fm6: FaceLayout = {
  theme: "metal",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Algorithme · toucher un opérateur pour le régler", ctl: [], custom: "fmalgo", grow: 1.15 },
          { title: "Opérateur", banks: FM_OPS, bankGroup: "fmop", cols: 6, grow: 2.2, wideLabels: true, ctl: [
            { id: "o{b}_mode", k: "vswitch", label: "Mode", short: ["Rapport", "Fixe"] },
            { id: "o{b}_coarse", label: "Grossier" }, { id: "o{b}_fine", label: "Fin" }, { id: "o{b}_detune", label: "Désaccord" },
            { id: "o{b}_level", k: "big", label: "Niveau" },
            { stack: [{ id: "o{b}_vel", k: "small", label: "Vélocité" }, { id: "o{b}_ams", k: "small", label: "Sens. ampl." }, { id: "o{b}_rscale", k: "small", label: "Vit. clavier" }] },
          ] },
        ],
        [
          { title: "Enveloppe de l'opérateur", banks: FM_OPS, bankGroup: "fmop", cols: 8, grow: 1.9, tone: "oklch(0.78 0.14 180)", ctl: [
            { id: "o{b}_r1", k: "fader", label: "V1" }, { id: "o{b}_r2", k: "fader", label: "V2" }, { id: "o{b}_r3", k: "fader", label: "V3" }, { id: "o{b}_r4", k: "fader", label: "V4" },
            { id: "o{b}_l1", k: "fader", label: "N1" }, { id: "o{b}_l2", k: "fader", label: "N2" }, { id: "o{b}_l3", k: "fader", label: "N3" }, { id: "o{b}_l4", k: "fader", label: "N4" },
          ] },
          { title: "Échelle au clavier", banks: FM_OPS, bankGroup: "fmop", cols: 3, grow: 1.1, wideLabels: true, ctl: [
            { id: "o{b}_ldepth", label: "Prof. gauche" }, { id: "o{b}_break", label: "Rupture" }, { id: "o{b}_rdepth", label: "Prof. droite" },
            { id: "o{b}_lcurve", k: "switch", label: "Courbe gauche", short: FM_CURVES }, { gap: true }, { id: "o{b}_rcurve", k: "switch", label: "Courbe droite", short: FM_CURVES },
          ] },
        ],
        [
          { title: "Global", cols: 6, grow: 1.5, wideLabels: true, ctl: [
            { id: "algo", k: "big", label: "Algorithme" }, { id: "feedback", label: "Rétroaction" }, { id: "tune", label: "Transposition" },
            { stack: [{ id: "oscsync", label: "Phases à la note" }, { id: "character", k: "switch", label: "Caractère", short: ["Propre", "80"] }] },
            { stack: [{ id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }] },
            { stack: [{ id: "voices", k: "small", label: "Voix" }, { id: "glide", k: "small", label: "Portamento" }, { id: "level", k: "small", label: "Volume" }] },
          ] },
          { title: "LFO", cols: 6, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.15 330)", ctl: [
            { id: "lfowave", k: "vswitch", label: "Forme", short: ["Tri.", "Scie ↓", "Scie ↑", "Carré", "Sinus", "Aléa."] },
            { stack: [{ id: "lfospeed", label: "Vitesse" }, { id: "lfosync", k: "small", label: "Synchro" }] },
            { stack: [{ id: "lfodelay", label: "Retard" }, { id: "lfoksync", label: "Recalé" }] },
            { stack: [{ id: "lfopmd", label: "Prof. hauteur" }, { id: "pms", k: "small", label: "Sens. hauteur" }] },
            { id: "lfoamd", label: "Prof. amplitude" },
            { stack: [{ id: "wheelpm", k: "small", label: "Molette → vibrato" }, { id: "wheelam", k: "small", label: "Molette → ampl." }] },
          ] },
        ],
        [
          { title: "Enveloppe de hauteur", cols: 8, grow: 1.5, tone: "oklch(0.8 0.14 85)", ctl: [
            { id: "pr1", k: "fader", label: "V1" }, { id: "pr2", k: "fader", label: "V2" }, { id: "pr3", k: "fader", label: "V3" }, { id: "pr4", k: "fader", label: "V4" },
            { id: "pl1", k: "fader", label: "N1" }, { id: "pl2", k: "fader", label: "N2" }, { id: "pl3", k: "fader", label: "N3" }, { id: "pl4", k: "fader", label: "N4" },
          ] },
          { title: "Pression", cols: 2, grow: 0.6, wideLabels: true, ctl: [{ id: "presspm", label: "→ vibrato" }, { id: "pressam", label: "→ amplitude" }] },
        ],
      ],
    },
    {
      id: "matrix", label: "Modulations et arpège",
      rows: [[{ title: "Matrice de modulation · 8 routages", ctl: [], custom: "matrix", grow: 2 }], [ARP_PANEL]],
    },
  ],
};

/** New Metal (FM à 4 opérateurs, v0.45) : l'algorithme en écran (sorties X et Y), les rapports,
    harmoniques et niveaux de modulation ; les deux enveloppes de modulation A et B ; puis la
    partie soustractive (saturation, filtre multimode, base / largeur, enveloppes) ; LFO à part. */
const LFO12 = [{ label: "LFO 1", value: "1" }, { label: "LFO 2", value: "2" }];
const fm4: FaceLayout = {
  theme: "newmetal",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Algorithme", ctl: [], custom: "fm4algo", grow: 1 },
          { title: "FM", cols: 7, grow: 2.6, wideLabels: true, ctl: [
            { id: "ratc", label: "Rapport C" }, { id: "rata", label: "Rapport A" }, { id: "ratb1", label: "Rapport B1" }, { id: "ratb2", label: "Rapport B2" },
            { id: "harm", k: "big", label: "Harmoniques" }, { id: "leva", k: "big", label: "Niveau A" }, { id: "levb", k: "big", label: "Niveau B" },
            { id: "offa", k: "small", label: "Décalage A" }, { id: "offb", k: "small", label: "Décalage B" }, { id: "detune", k: "small", label: "Désaccord" },
            { id: "feedback", label: "Rétroaction" }, { id: "mix", k: "big", label: "Mélange X / Y" }, { id: "algo", k: "small", label: "Algorithme" }, { id: "velmod", k: "small", label: "Vélocité" },
          ] },
        ],
        [
          { title: "Enveloppe A", cols: 5, grow: 1, tone: "oklch(0.8 0.14 25)", ctl: [
            { id: "aatk", k: "fader", label: "Attaque" }, { id: "adec", k: "fader", label: "Déclin" }, { id: "aend", k: "fader", label: "Fin" },
            { id: "amode", k: "vswitch", label: "Mode", short: ["Décl.", "Touche"] }, { id: "areset", k: "switch", label: "Depuis 0", short: ["Non", "Oui"] },
          ] },
          { title: "Enveloppe B", cols: 5, grow: 1, tone: "oklch(0.8 0.14 250)", ctl: [
            { id: "batk", k: "fader", label: "Attaque" }, { id: "bdec", k: "fader", label: "Déclin" }, { id: "bend", k: "fader", label: "Fin" },
            { id: "bmode", k: "vswitch", label: "Mode", short: ["Décl.", "Touche"] }, { id: "breset", k: "switch", label: "Depuis 0", short: ["Non", "Oui"] },
          ] },
        ],
        [
          { title: "Filtres", cols: 5, grow: 1.7, wideLabels: true, tone: "oklch(0.78 0.14 60)", ctl: [
            { id: "drive", label: "Saturation" }, { id: "ftype", k: "vswitch", label: "Type", short: ["Multi", "24 dB"] }, { id: "ffreq", k: "big", label: "Coupure" },
            { id: "fres", label: "Résonance" }, { id: "fmorph", label: "Bas → haut" },
            { id: "fenv", label: "Qté env." }, { id: "fkey", k: "small", label: "Clavier" }, { id: "fvel", k: "small", label: "Vélocité" },
            { id: "base", label: "Base" }, { id: "width", label: "Largeur" },
          ] },
          { title: "Enveloppe filtre", cols: 4, grow: 1, ctl: [
            { id: "fa", k: "fader", label: "A" }, { id: "fd", k: "fader", label: "D" }, { id: "fs", k: "fader", label: "S" }, { id: "fr", k: "fader", label: "R" },
          ] },
        ],
        [
          { title: "Ampli", cols: 6, grow: 1.4, ctl: [
            { id: "aa", k: "fader", label: "A" }, { id: "ad", k: "fader", label: "D" }, { id: "as", k: "fader", label: "S" }, { id: "ar", k: "fader", label: "R" },
            { id: "velamp", k: "small", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" },
          ] },
          { title: "Jeu", cols: 5, grow: 1.2, wideLabels: true, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }, { id: "voices", k: "small", label: "Voix" },
            { id: "glide", k: "small", label: "Portamento" }, { id: "tune", label: "Transposition" }, { id: "spread", label: "Stéréo" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "LFO, modulations et arpège",
      rows: [
        [
          { title: "LFO", banks: LFO12, cols: 6, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.15 330)", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", k: "big", label: "Vitesse" }, { id: "lf{b}_sync", label: "Synchro" },
            { id: "lf{b}_mode", label: "Mode" }, { id: "lf{b}_fade", k: "small", label: "Fondu" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Enveloppe de modulation", cols: 6, grow: 1.4, ctl: [
            { id: "me1_dly", k: "fader", label: "Retard" }, { id: "me1_att", k: "fader", label: "A" }, { id: "me1_hld", k: "fader", label: "Tenue" },
            { id: "me1_dec", k: "fader", label: "D" }, { id: "me1_sus", k: "fader", label: "S" }, { id: "me1_rel", k: "fader", label: "R" },
          ] },
        ],
        [{ title: "Matrice de modulation · 8 routages (sources : LFO 1 et 2, enveloppe, vélocité…)", ctl: [], custom: "matrix", grow: 2 }],
        [ARP_PANEL],
      ],
    },
  ],
};

/** Komodo Fuel (FM à 6 opérateurs à matrice libre, v0.45.1) : la matrice 6 × 6 (ligne = opérateur qui
    module, colonne = opérateur modulé, diagonale = rétroaction), l'opérateur choisi (forme, rapport,
    sortie) et son enveloppe à 6 étapes ; filtre multimode, jeu ; LFO et matrice de modulation à part. */
const FMX_OPS = [1, 2, 3, 4, 5, 6].map((k) => ({ label: `OP ${k}`, value: String(k) }));
const fmx: FaceLayout = {
  theme: "fuel",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Ce que module l'opérateur choisi · sur lui-même = rétroaction", banks: FMX_OPS, bankGroup: "fmxop", cols: 6, grow: 1.3, ctl: [
            { id: "m{b}1", label: "{b} → 1" }, { id: "m{b}2", label: "{b} → 2" }, { id: "m{b}3", label: "{b} → 3" }, { id: "m{b}4", label: "{b} → 4" }, { id: "m{b}5", label: "{b} → 5" }, { id: "m{b}6", label: "{b} → 6" },
          ] },
          { title: "Opérateur", banks: FMX_OPS, bankGroup: "fmxop", cols: 4, grow: 1.3, wideLabels: true, ctl: [
            { id: "o{b}_wave", k: "big", label: "Forme" }, { id: "o{b}_coarse", label: "Rapport" }, { id: "o{b}_fine", label: "Fin" }, { id: "o{b}_out", k: "big", label: "Sortie" },
            { id: "o{b}_fixed", k: "switch", label: "Fixe", short: ["Non", "Oui"] }, { id: "o{b}_fixhz", label: "Fréquence" }, { id: "o{b}_vel", k: "small", label: "Vélocité" },
            { id: "o{b}_loop", k: "switch", label: "Boucle", short: ["Non", "Oui"] },
          ] },
        ],
        [
          { title: "Enveloppe de l'opérateur", banks: FMX_OPS, bankGroup: "fmxop", cols: 6, grow: 1.4, tone: "oklch(0.8 0.15 55)", ctl: [
            { id: "o{b}_dly", k: "fader", label: "Retard" }, { id: "o{b}_att", k: "fader", label: "Attaque" }, { id: "o{b}_hld", k: "fader", label: "Tenue" },
            { id: "o{b}_dec", k: "fader", label: "Déclin" }, { id: "o{b}_sus", k: "fader", label: "Maintien" }, { id: "o{b}_rel", k: "fader", label: "Relâch." },
          ] },
          { title: "Filtre", cols: 5, grow: 1.2, wideLabels: true, tone: "oklch(0.78 0.14 140)", ctl: [
            { id: "ftype", k: "vswitch", label: "Type", short: ["Aucun", "Bas", "Bande", "Haut"] }, { id: "fcut", k: "big", label: "Coupure" },
            { id: "fres", label: "Résonance" }, { id: "fenv", label: "Qté env." }, { id: "fkey", k: "small", label: "Clavier" },
          ] },
        ],
        [
          { title: "Enveloppe filtre", cols: 4, grow: 1, ctl: [
            { id: "fa", k: "fader", label: "A" }, { id: "fd", k: "fader", label: "D" }, { id: "fs", k: "fader", label: "S" }, { id: "fr", k: "fader", label: "R" },
          ] },
          { title: "Jeu", cols: 6, grow: 1.5, wideLabels: true, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }, { id: "voices", k: "small", label: "Voix" },
            { id: "glide", k: "small", label: "Portamento" }, { id: "tune", label: "Transposition" }, { id: "spread", label: "Stéréo" }, { id: "level", k: "big", label: "Volume" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "LFO, modulations et arpège",
      rows: [
        [
          { title: "LFO", banks: LFO12, cols: 6, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.15 330)", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", k: "big", label: "Vitesse" }, { id: "lf{b}_sync", label: "Synchro" },
            { id: "lf{b}_mode", label: "Mode" }, { id: "lf{b}_fade", k: "small", label: "Fondu" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Enveloppe de modulation", cols: 6, grow: 1.4, ctl: [
            { id: "me1_dly", k: "fader", label: "Retard" }, { id: "me1_att", k: "fader", label: "A" }, { id: "me1_hld", k: "fader", label: "Tenue" },
            { id: "me1_dec", k: "fader", label: "D" }, { id: "me1_sus", k: "fader", label: "S" }, { id: "me1_rel", k: "fader", label: "R" },
          ] },
        ],
        [{ title: "Matrice de modulation · 16 routages (sources : LFO 1 et 2, enveloppe, vélocité…)", ctl: [], custom: "matrix", grow: 2 }],
        [ARP_PANEL],
      ],
    },
  ],
};

/** v0.46 — famille Drones. Hot Sauce (hybride à deux moteurs), Octo Lyma (8 voix en paires,
    bourdons), Komodo Dragon (masques de bits). */
const ENG_TYPES = ["Analog.", "Table", "Harm.", "Modal"];
const ENV12 = [{ label: "ENV 1", value: "1" }, { label: "ENV 2", value: "2" }];
const LFO123 = [{ label: "LFO 1", value: "1" }, { label: "LFO 2", value: "2" }, { label: "LFO 3", value: "3" }];
const engine = (e: "a" | "b"): Ctl[] => [
  { id: `${e}type`, k: "vswitch", label: "Type", short: ENG_TYPES },
  { id: `${e}p1`, k: "big", label: "Réglage 1" }, { id: `${e}p2`, label: "Réglage 2" }, { id: `${e}lvl`, k: "big", label: "Niveau" },
  { id: `${e}oct`, k: "small", label: "Octave" }, { id: `${e}semi`, k: "small", label: "Demi-tons" }, { id: `${e}fine`, k: "small", label: "Fin" },
];
const hots: FaceLayout = {
  theme: "sauce",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Moteur A · réglage 1 : forme / position / pente / matière · 2 : désaccord / pli / parité / durée", cols: 4, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.17 35)", ctl: engine("a") },
          { title: "Moteur B", cols: 4, grow: 1.6, wideLabels: true, tone: "oklch(0.8 0.15 80)", ctl: [...engine("b"), { id: "fmab", label: "FM A → B" }] },
          { title: "Utilitaire", cols: 1, grow: 0.5, wideLabels: true, ctl: [{ id: "sub", label: "Sous-osc." }, { id: "noise", label: "Bruit" }, { id: "ncolor", k: "small", label: "Couleur" }] },
        ],
        [
          { title: "Filtres · 1 : d'état (bas → haut) · 2 : échelle 24 dB", cols: 5, grow: 1.8, wideLabels: true, tone: "oklch(0.78 0.14 60)", ctl: [
            { id: "f1cut", k: "big", label: "Coupure 1" }, { id: "f1res", label: "Résonance 1" }, { id: "f1morph", label: "Bas → haut" },
            { id: "f2cut", k: "big", label: "Coupure 2" }, { id: "f2res", label: "Résonance 2" },
            { id: "f2drive", label: "Saturation 2" }, { id: "route", label: "Série → parallèle" }, { id: "fenv", label: "Qté env." }, { id: "fkey", k: "small", label: "Clavier" },
          ] },
          { title: "Enveloppe filtre", cols: 4, grow: 0.9, ctl: [
            { id: "fa", k: "fader", label: "A" }, { id: "fd", k: "fader", label: "D" }, { id: "fs", k: "fader", label: "S" }, { id: "fr", k: "fader", label: "R" },
          ] },
        ],
        [
          { title: "Ampli", cols: 6, grow: 1.4, ctl: [
            { id: "aa", k: "fader", label: "A" }, { id: "ad", k: "fader", label: "D" }, { id: "as", k: "fader", label: "S" }, { id: "ar", k: "fader", label: "R" },
            { id: "velamp", k: "small", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" },
          ] },
          { title: "Jeu", cols: 5, grow: 1.2, wideLabels: true, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }, { id: "voices", k: "small", label: "Voix" },
            { id: "glide", k: "small", label: "Portamento" }, { id: "tune", label: "Transposition" }, { id: "spread", label: "Stéréo" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "LFO, modulations et arpège",
      rows: [
        [
          { title: "LFO", banks: LFO123, cols: 6, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.15 330)", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", k: "big", label: "Vitesse" }, { id: "lf{b}_sync", label: "Synchro" },
            { id: "lf{b}_mode", label: "Mode" }, { id: "lf{b}_fade", k: "small", label: "Fondu" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Enveloppe de modulation", banks: ENV12, cols: 6, grow: 1.4, ctl: [
            { id: "me{b}_dly", k: "fader", label: "Retard" }, { id: "me{b}_att", k: "fader", label: "A" }, { id: "me{b}_hld", k: "fader", label: "Tenue" },
            { id: "me{b}_dec", k: "fader", label: "D" }, { id: "me{b}_sus", k: "fader", label: "S" }, { id: "me{b}_rel", k: "fader", label: "R" },
          ] },
        ],
        [{ title: "Matrice de modulation · 16 routages (sources : LFO 1 à 3, enveloppes, vélocité…)", ctl: [], custom: "matrix", grow: 2 }],
        [ARP_PANEL],
      ],
    },
  ],
};

const PAIR_SRC = ["Voisine", "LFO", "Retour"];
const lyra: FaceLayout = {
  theme: "lyma",
  wheels: false,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Voix · accord de chaque voix (jeu Plaques : la note joue la voix « note modulo 8 »)", cols: 8, grow: 2.4, wideLabels: true, tone: "oklch(0.55 0.15 40)", ctl:
            [1, 2, 3, 4, 5, 6, 7, 8].map((v) => ({ id: `t${v}`, k: "big" as const, label: `Voix ${v}` })) },
        ],
        [
          { title: "Paires · forme d'onde (triangle → carré, seule forme de la machine), modulation et sa source", cols: 6, grow: 2.2, wideLabels: true, ctl: [1, 2, 3, 4].flatMap((k) => {
            const n = ["1-2", "3-4", "5-6", "7-8"][k - 1];
            return [{ id: `p${k}sharp`, label: `Forme ${n}` }, { id: `p${k}mod`, label: `Modulation ${n}` }, { id: `p${k}src`, k: "vswitch" as const, label: `Source ${n}`, short: PAIR_SRC }];
          }).slice(0, 12) },
          { title: "Groupes 1-4 et 5-8 · tenue = bourdon tant qu'une note du groupe est tenue", cols: 4, grow: 1.5, wideLabels: true, tone: "oklch(0.5 0.12 250)", ctl: [
            { id: "q1pitch", label: "Hauteur 1-4" }, { id: "q1hold", k: "big", label: "Tenue 1-4" }, { id: "q1att", label: "Attaque 1-4" }, { id: "q1dec", label: "Déclin 1-4" },
            { id: "q2pitch", label: "Hauteur 5-8" }, { id: "q2hold", k: "big", label: "Tenue 5-8" }, { id: "q2att", label: "Attaque 5-8" }, { id: "q2dec", label: "Déclin 5-8" },
          ] },
        ],
        [
          { title: "LFO hyper · Libre : vitesses en Hz · Tempo : divisions", cols: 4, grow: 1.2, wideLabels: true, tone: "oklch(0.55 0.18 330)", ctl: [
            { id: "lfoa", k: "big", label: "LFO A" }, { id: "lfob", k: "big", label: "LFO B" },
            { id: "lfomode", k: "switch", label: "Combinaison", short: ["ET", "OU"] }, { id: "lfolink", k: "switch", label: "Lier A → B", short: ["Non", "Oui"] },
            { id: "lfoadiv", k: "menu", label: "A au tempo" }, { id: "lfobdiv", k: "menu", label: "B au tempo" },
            { id: "lfosync", k: "switch", label: "Vitesse", short: ["Libre", "Tempo"] },
          ] },
          // v0.46 : filtre (ajout : la machine d'origine n'en a pas) ; ouvert par défaut
          { title: "Filtre", cols: 4, grow: 1, wideLabels: true, tone: "oklch(0.55 0.14 90)", ctl: [
            { id: "fcut", k: "big", label: "Coupure" }, { id: "freso", k: "big", label: "Résonance" },
            { id: "fmode", k: "vswitch", label: "Type", short: ["P-bas", "P-bande", "P-haut"] }, { id: "flfo", label: "LFO → coupure" },
          ] },
          { title: "Retard modulé · deux lignes croisées · Libre : ms · Tempo : divisions", cols: 7, grow: 1.7, wideLabels: true, tone: "oklch(0.5 0.12 160)", ctl: [
            { id: "dtime1", k: "big", label: "Temps 1" }, { id: "dtime2", k: "big", label: "Temps 2" }, { id: "dmod1", label: "Mod. 1" }, { id: "dmod2", label: "Mod. 2" },
            { id: "dmodsrc", k: "switch", label: "Source", short: ["LFO", "Soi"] }, { id: "dfb", label: "Réinjection" }, { id: "dmix", k: "big", label: "Retard" },
            { id: "ddiv1", k: "menu", label: "Temps 1 au tempo" }, { id: "ddiv2", k: "menu", label: "Temps 2 au tempo" },
            { id: "dsync", k: "switch", label: "Temps", short: ["Libre", "Tempo"] },
          ] },
          { title: "Global", cols: 5, grow: 1.2, wideLabels: true, ctl: [
            { id: "play", k: "vswitch", label: "Jeu", short: ["Plaques", "Clavier"] }, { id: "base", label: "Note de base" },
            { id: "drive", label: "Distorsion" }, { id: "velamp", k: "small", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" },
          ] },
        ],
      ],
    },
  ],
};

const bitm: FaceLayout = {
  theme: "kdragon",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Masques · toucher un bit l'inverse", ctl: [], custom: "bitmask", grow: 1.3 },
          { title: "Oscillateurs", cols: 4, grow: 1.6, wideLabels: true, ctl: [
            { id: "res", k: "vswitch", label: "Segments", short: ["8", "16", "32", "64", "128", "256"] }, { id: "mmode", k: "vswitch", label: "Bit à zéro", short: ["Coupe", "Inverse", "Redresse"] },
            { id: "o1a", label: "Masque 1 A" }, { id: "o1b", label: "Masque 1 B" }, { id: "o2a", label: "Masque 2 A" }, { id: "o2b", label: "Masque 2 B" },
            { id: "o2semi", label: "Osc. 2 demi-tons" }, { id: "o2fine", k: "small", label: "Osc. 2 fin" },
          ] },
          { title: "Mélangeur", cols: 2, grow: 0.7, wideLabels: true, ctl: [
            { id: "mix1", k: "big", label: "Osc. 1" }, { id: "mix2", k: "big", label: "Osc. 2" }, { id: "noise", label: "Bruit" }, { id: "noisehz", k: "small", label: "Échantillonnage" },
          ] },
        ],
        [
          { title: "Delta · passage du masque A au masque B", cols: 5, grow: 1.3, tone: "oklch(0.82 0.15 140)", wideLabels: true, ctl: [
            { id: "damt", k: "big", label: "Delta" }, { id: "denv", label: "Enveloppe" }, { id: "da", k: "fader", label: "Attaque" }, { id: "dd", k: "fader", label: "Déclin" },
            { id: "dloop", k: "switch", label: "Boucle", short: ["Non", "Oui"] },
          ] },
          { title: "Hauteur", cols: 2, grow: 0.5, wideLabels: true, ctl: [{ id: "pamt", label: "Enveloppe" }, { id: "pdec", label: "Déclin" }] },
          { title: "Filtre d'état 12 dB", cols: 5, grow: 1.2, wideLabels: true, tone: "oklch(0.78 0.14 60)", ctl: [
            { id: "ftype", k: "vswitch", label: "Type", short: ["Bas", "Bande", "Haut", "Réjec."] }, { id: "fcut", k: "big", label: "Coupure" },
            { id: "fres", label: "Résonance" }, { id: "fenv", label: "Qté env." }, { id: "fkey", k: "small", label: "Clavier" },
          ] },
        ],
        [
          { title: "Enveloppe filtre", cols: 4, grow: 0.9, ctl: [
            { id: "fa", k: "fader", label: "A" }, { id: "fd", k: "fader", label: "D" }, { id: "fs", k: "fader", label: "S" }, { id: "fr", k: "fader", label: "R" },
          ] },
          { title: "Ampli", cols: 6, grow: 1.3, ctl: [
            { id: "aa", k: "fader", label: "A" }, { id: "ad", k: "fader", label: "D" }, { id: "as", k: "fader", label: "S" }, { id: "ar", k: "fader", label: "R" },
            { id: "velamp", k: "small", label: "Vélocité" }, { id: "level", k: "big", label: "Volume" },
          ] },
          { title: "Jeu", cols: 5, grow: 1.2, wideLabels: true, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }, { id: "voices", k: "small", label: "Voix" },
            { id: "glide", k: "small", label: "Portamento" }, { id: "tune", label: "Transposition" }, { id: "spread", label: "Stéréo" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "LFO, modulations et arpège",
      rows: [
        [
          { title: "LFO", banks: LFO12, cols: 6, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.15 330)", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", k: "big", label: "Vitesse" }, { id: "lf{b}_sync", label: "Synchro" },
            { id: "lf{b}_mode", label: "Mode" }, { id: "lf{b}_fade", k: "small", label: "Fondu" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Enveloppe de modulation", cols: 6, grow: 1.4, ctl: [
            { id: "me1_dly", k: "fader", label: "Retard" }, { id: "me1_att", k: "fader", label: "A" }, { id: "me1_hld", k: "fader", label: "Tenue" },
            { id: "me1_dec", k: "fader", label: "D" }, { id: "me1_sus", k: "fader", label: "S" }, { id: "me1_rel", k: "fader", label: "R" },
          ] },
        ],
        [{ title: "Matrice de modulation · 8 routages", ctl: [], custom: "matrix", grow: 2 }],
        [ARP_PANEL],
      ],
    },
  ],
};

/** World Tour (poly analogique 8 voix, v0.45) : oscillateurs, mélangeur qui sature, oscillateur
    de modulation, deux filtres en échelle (série, parallèle, stéréo), enveloppes ; retard diffusant. */
const FEET = ["32'", "16'", "8'", "4'", "2'"];
const muse: FaceLayout = {
  theme: "tour",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateurs", cols: 5, grow: 2, wideLabels: true, ctl: [
            { id: "o1oct", k: "switch", label: "Pied 1", short: FEET }, { id: "o1wave", k: "big", label: "Forme 1" }, { id: "o1fm", label: "FM 2 → 1" },
            { id: "o2sync", label: "Synchro 2 → 1" }, { id: "o2key", label: "Clavier 2" },
            { id: "o2oct", k: "switch", label: "Pied 2", short: FEET }, { id: "o2wave", k: "big", label: "Forme 2" }, { id: "o2semi", label: "Demi-tons 2" }, { id: "o2fine", label: "Fin 2" },
          ] },
          { title: "Mélangeur", cols: 6, grow: 1.6, ctl: [
            { id: "mxo1", k: "fader", label: "Osc. 1" }, { id: "mxo2", k: "fader", label: "Osc. 2" }, { id: "mxmod", k: "fader", label: "Modul." },
            { id: "mxnoise", k: "fader", label: "Bruit" }, { id: "mxring", k: "fader", label: "Anneau" }, { id: "overload", k: "fader", label: "Surcharge" },
          ] },
        ],
        [
          { title: "Oscillateur de modulation", cols: 7, grow: 2.2, wideLabels: true, tone: "oklch(0.8 0.13 140)", ctl: [
            { id: "mowave", k: "vswitch", label: "Forme", short: ["Sinus", "Scie", "Scie inv.", "Carré", "Bruit"] }, { id: "morate", k: "big", label: "Vitesse" },
            { stack: [{ id: "moaudio", k: "switch", label: "Plage", short: ["Lente", "Audio"] }, { id: "mokey", label: "Clavier" }] },
            { stack: [{ id: "moreset", label: "Recalé" }, { id: "mouni", label: "Unipolaire" }] },
            { stack: [{ id: "mopitch", label: "→ hauteur" }, { id: "modest", k: "switch", label: "De", short: ["1 et 2", "1", "2"] }] },
            { stack: [{ id: "mofilter", label: "→ filtres" }, { id: "mowaveamt", k: "small", label: "→ formes" }] },
            { stack: [{ id: "movca", label: "→ ampli" }, { id: "mopan", label: "→ pano" }] },
          ] },
          { title: "Filtres en échelle", cols: 5, grow: 2, wideLabels: true, tone: "oklch(0.8 0.14 60)", ctl: [
            { id: "f1mode", k: "vswitch", label: "Filtre 1", short: ["Bas", "Haut"] }, { id: "f1cut", k: "big", label: "Coupure 1" }, { id: "f1res", label: "Résonance 1" },
            { id: "route", k: "vswitch", label: "Routage", short: ["Série", "Parall.", "Stéréo"] }, { id: "flink", label: "Liées" },
            { id: "fenv", label: "Qté env." }, { id: "f2cut", k: "big", label: "Coupure 2" }, { id: "f2res", label: "Résonance 2" },
            { id: "fkey", k: "small", label: "Clavier" }, { id: "fvel", k: "small", label: "Vélocité" },
          ] },
        ],
        [
          { title: "Enveloppe filtre", cols: 5, grow: 1.2, ctl: [
            { id: "e1a", k: "fader", label: "A" }, { id: "e1d", k: "fader", label: "D" }, { id: "e1s", k: "fader", label: "S" }, { id: "e1r", k: "fader", label: "R" }, { id: "e1loop", label: "Boucle" },
          ] },
          { title: "Enveloppe ampli", cols: 5, grow: 1.2, ctl: [
            { id: "e2a", k: "fader", label: "A" }, { id: "e2d", k: "fader", label: "D" }, { id: "e2s", k: "fader", label: "S" }, { id: "e2r", k: "fader", label: "R" }, { id: "velamp", k: "small", label: "Vélocité" },
          ] },
          { title: "Retard diffusant", cols: 5, grow: 1.3, wideLabels: true, tone: "oklch(0.78 0.12 220)", ctl: [
            { id: "dlymix", k: "big", label: "Retard" }, { id: "dlytime", label: "Temps" }, { id: "dlysync", label: "Synchro" }, { id: "dlyfb", label: "Réinjection" }, { id: "dlydiff", label: "Diffusion" },
          ] },
        ],
        [
          { title: "Vibrato", cols: 3, grow: 0.9, wideLabels: true, ctl: [
            { id: "vibrate", label: "Vitesse" }, { id: "vibamt", label: "Vibrato" }, { id: "vibwheel", label: "Molette" },
          ] },
          { title: "Jeu", cols: 4, grow: 1.4, wideLabels: true, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }, { id: "voices", k: "small", label: "Voix" }, { id: "glide", label: "Portamento" },
            { id: "spread", label: "Désaccord" }, { id: "drift", label: "Dérive" }, { id: "tune", label: "Transposition" }, { id: "level", k: "big", label: "Volume" },
          ] },
        ],
      ],
    },
    {
      id: "matrix", label: "LFO, modulations et arpège",
      rows: [
        [
          { title: "LFO", banks: LFO12, cols: 6, grow: 1.6, wideLabels: true, tone: "oklch(0.78 0.15 330)", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", k: "big", label: "Vitesse" }, { id: "lf{b}_sync", label: "Synchro" },
            { id: "lf{b}_mode", label: "Mode" }, { id: "lf{b}_fade", k: "small", label: "Fondu" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Enveloppe de modulation", cols: 6, grow: 1.4, ctl: [
            { id: "me1_dly", k: "fader", label: "Retard" }, { id: "me1_att", k: "fader", label: "A" }, { id: "me1_hld", k: "fader", label: "Tenue" },
            { id: "me1_dec", k: "fader", label: "D" }, { id: "me1_sus", k: "fader", label: "S" }, { id: "me1_rel", k: "fader", label: "R" },
          ] },
        ],
        [{ title: "Matrice de modulation · 16 routages", ctl: [], custom: "matrix", grow: 2 }],
        [ARP_PANEL],
      ],
    },
  ],
};

/** Boîte « Impact » (v0.26) : disposée comme la boîte compacte — une bande par instrument avec son
    bouton COULEUR et son NIVEAU, la sortie (distorsion, accent) à droite, les pas en bas ; le pad
    choisi se règle en entier dans la page « Pad choisi ». */
const drumImp: FaceLayout = {
  theme: "dragon",
  pages: [
    {
      id: "sound", label: "Instruments",
      rows: [
        [{ title: "Instruments · toucher un nom pour choisir l'instrument", ctl: [], custom: "impstrips", grow: 6 },
         { title: "Sortie", ctl: [], custom: "impout", grow: 0.75, tone: "oklch(0.72 0.19 40)" }],
        [{ title: "Pas de l'instrument choisi", ctl: [], custom: "stepkeysI" }],
      ],
    },
    {
      id: "pad", label: "Pad choisi",
      rows: [
        [{ title: "Pad choisi · la couleur est le dernier réglage", ctl: [], custom: "elektron", grow: 2.2 }],
        [{ title: "Tous les instruments", ctl: [], custom: "kitcols:4" }],
      ],
    },
  ],
};

/** Batterie « Nerve » (v0.26) : disposée comme le logiciel — en haut la grille 4 × 4 des pads et les
    réglages du son du pad choisi (1.1, à la place de l'éditeur de motif), le pad choisi (forme d'onde et pages, dont le précalcul) et le
    compresseur interne ; le kit en colonnes dans la page « Le kit ». */
const drumNrv: FaceLayout = {
  theme: "cutbeat",
  pages: [
    {
      id: "sound", label: "Pads et son",
      rows: [
        [{ title: "Pads", ctl: [], custom: "nervepads", grow: 1 }, { title: "Son du pad choisi", ctl: [], custom: "padsound", grow: 2.3 }],
        [
          { title: "Pad choisi · déposez un échantillon sur un pad pour remplacer son son", ctl: [], custom: "elektron", grow: 2.6 },
          { title: "Compresseur interne", cols: 3, grow: 0.9, tone: "oklch(0.78 0.15 170)", ctl: [
            { id: "duckpad", k: "small", label: "Pad déclencheur" }, { id: "duckamt", k: "big", label: "Profondeur" }, { id: "duckrel", label: "Relâchement" },
          ] },
        ],
      ],
    },
    {
      id: "kit", label: "Le kit",
      rows: [
        [{ title: "Le kit", ctl: [], custom: "kitcols:3" }],
        [{ title: "Accent", ctl: [], custom: "accent", grow: 0 }, { title: "Pas du pad choisi", ctl: [], custom: "stepkeysR" }],
      ],
    },
  ],
};

/** Percussion à séquenceur (v0.26) : panneau noir à joues de bois — rangée des oscillateurs et du
    mélangeur, rangée filtre et ampli, puis séquenceur 8 pas et réglages de séquence.
    v0.46 fix16 : bloc Transport (Déclencher, Marche / arrêt) retiré ; le petit transport de la
    façade (●, ▶ / ❚❚, ■) le remplace, comme sur tous les instruments. */
const dfam: FaceLayout = {
  theme: "kicker",
  pages: [{
    id: "sound", label: "Son",
    rows: [
      [
        { title: "Oscillateurs", cols: 10, grow: 3.2, ctl: [
          { id: "vcodecay", label: "Déclin VCO" }, { id: "prange", label: "Séq. → hauteur" }, { id: "vco1eg", label: "VCO 1 env." },
          { id: "vco1freq", k: "big", label: "VCO 1" }, { id: "vco1wave", k: "vswitch", label: "Forme 1", short: ["Triangle", "Carré"] },
          { id: "fm", label: "FM 1 → 2" }, { id: "vco2eg", label: "VCO 2 env." }, { id: "vco2freq", k: "big", label: "VCO 2" },
          { id: "vco2wave", k: "vswitch", label: "Forme 2", short: ["Triangle", "Carré"] }, { id: "sync", k: "vswitch", label: "Synchro", short: ["Non", "Oui"] },
        ] },
        { title: "Mélangeur", cols: 1, grow: 0.42, ctl: [{ id: "mix1", k: "small", label: "VCO 1" }, { id: "mixnoise", k: "small", label: "Bruit" }, { id: "mix2", k: "small", label: "VCO 2" }] },
      ],
      [
        { title: "Filtre et ampli", cols: 9, grow: 1, ctl: [
          { id: "vcfdecay", label: "Déclin VCF" }, { id: "vcfeg", label: "VCF env." }, { id: "vcfmode", k: "vswitch", label: "Mode", short: ["Passe-bas", "Passe-haut"] },
          { id: "cutoff", k: "big", label: "Coupure" }, { id: "reso", label: "Résonance" }, { id: "noisevcf", label: "Bruit → VCF" },
          { id: "vcaattack", k: "vswitch", label: "Attaque VCA", short: ["Rapide", "Lente"] }, { id: "vcadecay", label: "Déclin VCA" }, { id: "volume", k: "big", label: "Volume" },
        ] },
      ],
      [
        { title: "Séquenceur 8 pas", ctl: [], custom: "dfamseq", grow: 3 },
        { title: "Séquence", cols: 2, grow: 0.85, ctl: [
          { id: "seqmode", k: "switch", label: "Mode", short: ["8 pas", "Note"] }, { id: "seqlen", label: "Longueur" },
          { id: "pitchto", k: "vswitch", label: "Hauteurs vers", short: ["VCO 1 et 2", "Arrêt", "VCO 2"] },
          { stack: [{ id: "velsens", k: "small", label: "Sensibilité" }, { id: "drift", k: "small", label: "Dérive" }] },
        ] },
      ],
    ],
  }],
};

// --- Vocodeurs (v0.25) -------------------------------------------------------------------------
const VOC_WAVES = ["Scie", "Imp.", "Sous", "Cordes", "Chœur", "Bruit"];
const VOC_SOURCE: Panel = { title: "La voix (modulateur)", ctl: [], custom: "vocsrc", grow: 1 };
const VOC_VOICE: Panel = { title: "Voix", cols: 6, grow: 1.1, ctl: [
  { id: "srcgain", label: "Gain" }, { id: "gate", label: "Porte" }, { id: "dry", label: "Voix directe" },
  { id: "vrate", k: "small", label: "Débit (interne)" }, { id: "vpitch", k: "small", label: "Hauteur (interne)" }, { id: "vexpr", k: "small", label: "Intonation" },
] };
const VOC_BANDS: Panel = { title: "Bandes analysées", ctl: [], custom: "vocbands", grow: 1.4 };

/** Vocodeur 16 bandes : façade de laboratoire (rack sombre, rangée de 16 voyants), porteuse,
    réponse des suiveurs, souffle et bourdon, aigus directs, maintien, décalage des bandes. */
const voc16: FaceLayout = {
  theme: "voc16",
  bigText: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [VOC_SOURCE, VOC_VOICE, VOC_BANDS],
        [
          { title: "Porteuse", cols: 8, grow: 2.2, ctl: [
            { id: "cwave", k: "menu", label: "Forme", short: VOC_WAVES }, { id: "cpw", label: "Largeur" },
            { id: "coct", k: "switch", label: "Registre", short: ["16'", "8'", "4'"] }, { id: "cuni", k: "small", label: "Unisson" },
            { id: "cdetune", k: "small", label: "Désaccord" }, { id: "cbright", label: "Brillance" }, { id: "cnoise", k: "small", label: "Bruit" },
            { id: "tune", k: "small", label: "Accord" },
          ] },
          { title: "Analyse", cols: 3, grow: 1, ctl: [
            { id: "resp", k: "big", label: "Réponse" }, { id: "rel", label: "Relâchement" }, { id: "shift", label: "Décalage" },
            { id: "hiss", label: "Souffle" }, { id: "buzz", label: "Bourdon" }, { id: "hold", k: "toggle", label: "Maintien" },
          ] },
        ],
      ],
    },
    {
      id: "out", label: "Sortie",
      rows: [[
        { title: "Sortie", cols: 6, grow: 1, ctl: [
          { id: "direct", k: "toggle", label: "Aigus directs" }, { id: "directlvl", label: "Niveau aigus" },
          { id: "ca", k: "fader", label: "Attaque" }, { id: "cr", k: "fader", label: "Relâch." }, { id: "voices", k: "small", label: "Voix" }, { id: "level", k: "big", label: "Volume" },
        ] },
      ]],
    },
  ],
};

/** Vocodeur à cordes : clavier cordes (registre, ensemble, crescendo) et vocodeur. */
const vocstr: FaceLayout = {
  theme: "vocstr",
  bigText: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [VOC_SOURCE, VOC_VOICE, VOC_BANDS],
        [
          { title: "Cordes", cols: 11, grow: 1, ctl: [
            { id: "cwave", k: "menu", label: "Forme", short: VOC_WAVES }, { id: "creg", k: "big", label: "Registre" },
            { id: "coct", k: "switch", label: "Octave", short: ["16'", "8'", "4'"] }, { id: "cdetune", label: "Désaccord" },
            { id: "cjitter", label: "Instabilité" }, { id: "cbright", label: "Brillance" }, { id: "cnoise", k: "small", label: "Bruit" }, { id: "tune", k: "small", label: "Accord" },
            { id: "ca", k: "fader", label: "Crescendo" }, { id: "cr", k: "fader", label: "Relâch." },
          ] },
        ],
      ],
    },
    {
      id: "voc", label: "Vocodeur et ensemble",
      rows: [[
        { title: "Vocodeur", cols: 7, grow: 2, ctl: [
          { id: "bands", k: "switch", label: "Bandes", short: ["16", "24", "32", "40"] }, { id: "formant", k: "big", label: "Formants" },
          { id: "resp", label: "Réponse" }, { id: "rel", label: "Relâchement" }, { id: "sib", label: "Sibilantes" }, { id: "freeze", k: "toggle", label: "Gel" },
          { id: "strings", label: "Cordes seules" },
        ] },
        { title: "Ensemble", cols: 3, grow: 1, ctl: [{ id: "ensemble", k: "big", label: "Ensemble" }, { id: "voices", k: "small", label: "Voix" }, { id: "level", k: "big", label: "Volume" }] },
      ]],
    },
  ],
};

/** Vocodeur spectral : algorithme, formants, expansion, traînes, stéréo ; synthé à unisson et FM. */
const vocspec: FaceLayout = {
  theme: "vocspec",
  bigText: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [VOC_SOURCE, VOC_VOICE, VOC_BANDS],
        [
          { title: "Vocodeur", cols: 10, grow: 1, ctl: [
            { id: "algo", k: "vswitch", label: "Algorithme" }, { id: "formant", k: "big", label: "Formants" }, { id: "expand", label: "Expansion" },
            { id: "traces", label: "Traînes" }, { id: "stereo", label: "Stéréo" }, { id: "unvoiced", label: "Consonnes" }, { id: "hfthru", label: "Aigus directs" },
            { id: "resp", k: "small", label: "Réponse" }, { id: "rel", k: "small", label: "Relâch." }, { id: "freeze", k: "toggle", label: "Gel" },
          ] },
        ],
      ],
    },
    {
      id: "synth", label: "Synthétiseur",
      // v0.34.1 : deux rangées (11 réglages sur une seule ne tenaient pas en grandes inscriptions)
      rows: [
        [
          { title: "Synthétiseur", cols: 8, grow: 1, ctl: [
            { id: "cwave", k: "menu", label: "Forme", short: VOC_WAVES }, { id: "cpw", label: "Largeur" }, { id: "cuni", k: "small", label: "Unisson" },
            { id: "cdetune", label: "Désaccord" }, { id: "cfm", label: "FM" }, { id: "coct", k: "switch", label: "Registre", short: ["16'", "8'", "4'"] },
            { id: "cbright", label: "Brillance" }, { stack: [{ id: "cnoise", k: "small", label: "Bruit" }, { id: "tune", k: "small", label: "Accord" }] },
          ] },
        ],
        [
          { title: "Ampli", cols: 3, grow: 1, ctl: [{ id: "ca", k: "fader", label: "Attaque" }, { id: "cr", k: "fader", label: "Relâch." }, { id: "level", k: "big", label: "Volume" }] },
        ],
      ],
    },
  ],
};

/** Voix de synthèse : cinq modules mélangés par curseurs, leurs réglages, la porteuse. */
const vocsyn: FaceLayout = {
  theme: "vocsyn",
  bigText: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [VOC_SOURCE, VOC_VOICE, VOC_BANDS],
        [
          { title: "Modules", cols: 5, grow: 1, ctl: [
            { id: "mvoc", k: "fader", label: "Vocodeur" }, { id: "mcomp", k: "fader", label: "Ordinateur" }, { id: "mtalk", k: "fader", label: "Boîte" },
            { id: "mbio", k: "fader", label: "Voix modél." }, { id: "mpoly", k: "fader", label: "Épaisseur" },
          ] },
          { title: "Réglages des modules", cols: 4, grow: 1.1, ctl: [
            { id: "vformant", label: "Formants voc." }, { id: "crush", k: "small", label: "Bits" }, { id: "crate", k: "small", label: "Échant." }, { id: "talkshift", label: "Formants boîte" },
            { id: "vowel", k: "big", label: "Voyelle" }, { id: "nasal", label: "Nasalité" }, { id: "bioshift", label: "Formants voix" }, { id: "polywidth", label: "Largeur" },
          ] },
        ],
      ],
    },
    {
      id: "carrier", label: "Porteuse",
      // v0.34.1 : deux rangées (12 réglages sur une seule ne tenaient pas en grandes inscriptions)
      rows: [
        [
          { title: "Porteuse", cols: 7, grow: 1, ctl: [
            { id: "cwave", k: "menu", label: "Forme", short: VOC_WAVES }, { id: "cpw", label: "Largeur" }, { id: "cuni", k: "small", label: "Unisson" }, { id: "cdetune", k: "small", label: "Désaccord" },
            { id: "coct", k: "switch", label: "Registre", short: ["16'", "8'", "4'"] }, { id: "cbright", label: "Brillance" }, { id: "tune", k: "small", label: "Accord" },
          ] },
        ],
        [
          { title: "Analyse", cols: 2, grow: 1, ctl: [{ id: "resp", label: "Réponse" }, { id: "rel", label: "Relâchement" }] },
          { title: "Ampli", cols: 3, grow: 1.4, ctl: [{ id: "ca", k: "fader", label: "Attaque" }, { id: "cr", k: "fader", label: "Relâch." }, { id: "level", k: "big", label: "Volume" }] },
        ],
      ],
    },
  ],
};


/** Sampler à découpes (v0.28) : forme d'onde en grand avec les 16 zones, grille des zones, puis
    tous les réglages de la zone choisie, les découpes et le kit. */
const ZONE_CTL = (bus: boolean) => [
  { id: "z{z}_start", label: "Début" }, { id: "z{z}_end", label: "Fin" }, { id: "z{z}_tune", k: "big" as const, label: "Accord" },
  { id: "z{z}_level", label: "Niveau" }, { id: "z{z}_pan", label: "Panoramique" },
  { id: "z{z}_attack", label: "Attaque" }, { id: "z{z}_release", label: "Relâchement" },
  { id: "z{z}_cutoff", k: "big" as const, label: "Coupure" }, { id: "z{z}_reso", label: "Résonance" },
  { id: "z{z}_mode", k: "vswitch" as const, label: "Lecture" },
  { stack: [{ id: "z{z}_reverse", label: "Inversé" }, { id: "z{z}_sync", k: "small" as const, label: "Calage tempo" }, ...(bus ? [{ id: "z{z}_bus", k: "small" as const, label: "Bus d'effets" }] : [])] },
];
const sampler: FaceLayout = {
  theme: "sampler",
  pages: [
    {
      id: "sound", label: "Échantillon",
      rows: [
        [
          { title: "Échantillon · zones et découpes", ctl: [], custom: "bigwave:zone{n}", grow: 3 },
          { title: "Zones", ctl: [], custom: "zonepads", grow: 1 },
        ],
        [
          { title: "Zone choisie", cols: 11, grow: 3.2, ctl: ZONE_CTL(false) },
          { title: "Découpes", cols: 2, grow: 0.8, ctl: [{ id: "slicecnt", k: "small" }, { id: "slicepage", k: "small" }, { id: "retrig", k: "small" }, { id: "rtim", k: "small" }] },
          { title: "Kit", cols: 2, grow: 0.8, ctl: [{ id: "level", k: "big" }, "velamp", { id: "ftype", k: "vswitch" }, "choke"] },
        ],
      ],
    },
  ],
};

/** Sampler chromatique (v0.28) : l'échantillon et sa boucle en grand, la lecture, le filtre et
    ses deux enveloppes à curseurs. */
const keys: FaceLayout = {
  theme: "sampler",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Échantillon et boucle", cols: 7, grow: 2.6, custom: "bigwave:sample", ctl: [
            "start", "end", { id: "loop", k: "vswitch" }, "loopstart", "loopend", { stack: ["reverse", "trigmode"] },
          ] },
          { title: "Lecture", cols: 3, grow: 1, ctl: [{ id: "tune", k: "big" }, "fine", "root", "voices", "glide", "pan"] },
        ],
        [
          { title: "Filtre", cols: 4, grow: 1.3, ctl: [{ id: "ftype", k: "vswitch" }, { id: "cutoff", k: "big" }, "reso", "envamt", "keytrack", "velfilter"] },
          { title: "Enveloppe filtre", cols: 4, grow: 1, ctl: [{ id: "fa", k: "fader", label: "A" }, { id: "fd", k: "fader", label: "D" }, { id: "fs", k: "fader", label: "S" }, { id: "fr", k: "fader", label: "R" }] },
          { title: "Enveloppe ampli", cols: 6, grow: 1.5, ctl: [
            { id: "aa", k: "fader", label: "A" }, { id: "ad", k: "fader", label: "D" }, { id: "as", k: "fader", label: "S" }, { id: "ar", k: "fader", label: "R" },
            "velamp", { id: "level", k: "big" },
          ] },
        ],
      ],
    },
  ],
};

/** Sampler à pads (v0.28) : 16 gros pads, le pad choisi (onde et réglages), les deux bus
    d'effets à trois commandes et le kit. */
const sp: FaceLayout = {
  theme: "sp",
  pages: [
    {
      id: "sound", label: "Pads",
      rows: [
        [
          { title: "Pads", ctl: [], custom: "zonepads:big", grow: 1.2 },
          { title: "Pad choisi", cols: 6, grow: 2.6, custom: "bigwave:zone{n}", ctl: ZONE_CTL(true) },
        ],
        [
          { title: "Bus d'effets 1", cols: 4, grow: 1, ctl: [{ id: "b1type", k: "big", label: "Effet" }, { id: "b1c1", label: "Commande 1" }, { id: "b1c2", label: "Commande 2" }, { id: "b1c3", label: "Commande 3" }] },
          { title: "Bus d'effets 2", cols: 4, grow: 1, ctl: [{ id: "b2type", k: "big", label: "Effet" }, { id: "b2c1", label: "Commande 1" }, { id: "b2c2", label: "Commande 2" }, { id: "b2c3", label: "Commande 3" }] },
          { title: "Routage et kit", cols: 6, grow: 1.3, ctl: [{ id: "busroute", label: "Bus" }, { id: "level", k: "big" }, "velamp", { id: "ftype", k: "vswitch" }, "choke", { id: "slices", k: "vswitch" }] },
        ],
      ],
    },
  ],
};

/** Glutamake (v0.47, principe des synthés à tables d'ondes avec physique de mouvement) : ABS noir,
    boutons noirs lisses, arcs braise ; la bille et les oscillateurs en tête, puis filtre, enveloppes,
    LFO et boutons de modulation ; la physique, la séquence de mouvement et la matrice en pages. */
const GL_ENVS = [{ label: "Ampli", value: "a" }, { label: "Filtre", value: "f" }, { label: "Hauteur", value: "p" }];
const GL_LANES = [{ label: "A", value: "a" }, { label: "B", value: "b" }, { label: "C", value: "c" }, { label: "D", value: "d" }];
const GL_PHYS: Panel[] = [
  { title: "Surface et bosse", cols: 5, grow: 1.6, ctl: [
    { id: "phon", label: "Physique", on: 1 }, { id: "tiltx", label: "Inclin. X" }, { id: "tilty", label: "Inclin. Y" }, { id: "fric", label: "Frottement" }, { id: "ptime", label: "Temps" },
    { id: "bumph", k: "big", label: "Bosse" }, { id: "bumpx", label: "Bosse X" }, { id: "bumpy", label: "Bosse Y" }, { id: "bumpw", label: "Largeur" },
    { id: "bumpshape", k: "vswitch", label: "Forme", short: ["Cloche", "Cône", "Plateau", "Anneau"] },
  ] },
  { title: "Bords · − absorbe, + relance", cols: 3, grow: 1, ctl: [
    { id: "edget", label: "Haut" }, { id: "edgeb", label: "Bas" }, { id: "wrap", label: "Traverser", on: 1 },
    { id: "edgel", label: "Gauche" }, { id: "edger", label: "Droite" },
  ] },
  { title: "Lancement", cols: 3, grow: 1.2, ctl: [
    { id: "lsrc", k: "vswitch", label: "Lancer", short: ["Geste", "Note", "Départ", "Séq. A"] },
    { id: "lforce", k: "big", label: "Force" }, { id: "langle", label: "Angle" }, { id: "lrand", k: "small", label: "Hasard" },
    { id: "startx", k: "small", label: "Départ X" }, { id: "starty", k: "small", label: "Départ Y" },
  ] },
];
const gluta: FaceLayout = {
  theme: "braise",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Physique · la bille", ctl: [], custom: "glutapad", grow: 0.85 },
          { title: "Oscillateur", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 7, grow: 2.4, custom: "glutaosc:{b}", ctl: [
            { id: "o{b}taba", k: "menu", label: "Table A" }, { id: "o{b}tabb", k: "menu", label: "Table B" },
            { id: "o{b}mtype", k: "menu", label: "Morph" }, { id: "o{b}modif", k: "menu", label: "Modificateur" },
            { id: "o{b}oct", k: "small", label: "Octave" }, { id: "o{b}semi", k: "small", label: "Demi-tons" }, { id: "o{b}fine", k: "small", label: "Fin" },
            { id: "o{b}pos", k: "big", label: "Position" }, { id: "o{b}ab", label: "Mélange A/B" }, { id: "o{b}morph", k: "big", label: "Morph" },
            { id: "o{b}lvl", label: "Niveau" }, { id: "sub", k: "small", label: "Sous-osc." }, { id: "noise", k: "small", label: "Bruit" },
          ] },
          { title: "Filtre", cols: 4, grow: 1.3, ctl: [
            { id: "ftype", k: "menu", label: "Type" }, { id: "cutoff", k: "big", label: "Coupure" }, { id: "reso", label: "Résonance" }, { id: "fenv", label: "Enveloppe" },
            { id: "fkey", k: "small", label: "Clavier" }, { id: "fvel", k: "small", label: "Vélocité" }, { id: "fdrive", k: "small", label: "Gain" }, { id: "fmulti", k: "small", label: "Fondu multi" },
          ] },
        ],
        [
          { title: "Enveloppe", banks: GL_ENVS, cols: 5, grow: 1.3, tone: "oklch(0.72 0.17 45)", ctl: [
            { id: "{b}a", k: "fader", label: "A" }, { id: "{b}d", k: "fader", label: "D" }, { id: "{b}s", k: "fader", label: "S" }, { id: "{b}r", k: "fader", label: "R" },
            { id: "pamt", k: "small", label: "Hauteur" },
          ] },
          { title: "Env.", banks: [{ label: "4", value: "1" }, { label: "5", value: "2" }], cols: 4, grow: 1.1, ctl: [
            { id: "me{b}_att", label: "Attaque" }, { id: "me{b}_dec", label: "Déclin" }, { id: "me{b}_sus", label: "Niveau" }, { id: "me{b}_rel", label: "Relâch." },
            { id: "me{b}_dly", k: "small", label: "Retard" }, { id: "me{b}_hld", k: "small", label: "Tenue" },
            { stack: [{ id: "me{b}_loop", label: "Boucle" }, { id: "me{b}_sync", label: "Tempo" }] }, { id: "me{b}_vel", k: "small", label: "Vélocité" },
          ] },
          { title: "LFO", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 2, grow: 0.7, ctl: [
            { id: "l{b}rate", k: "big", label: "Vitesse" }, { id: "l{b}shape", k: "vswitch", label: "Forme", short: ["Sin.", "Tri.", "Scie", "Carré", "A&M"] },
            { id: "l{b}sync", k: "small", label: "Synchro" }, { id: "l{b}delay", k: "small", label: "Retard" },
          ] },
          { title: "Mod", cols: 2, grow: 0.6, ctl: [{ id: "macro1", label: "Mod 1" }, { id: "macro2", label: "Mod 2" }, { id: "macro3", label: "Mod 3" }, { id: "macro4", label: "Mod 4" }] },
          { title: "Voix et sortie", cols: 3, grow: 1, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Unisson"] }, { id: "level", k: "big", label: "Volume" }, { id: "glide", k: "small", label: "Glisse" },
            { id: "voices", k: "small", label: "Voix" }, { id: "univoices", k: "small", label: "Unisson" }, { id: "unidetune", k: "small", label: "Désacc." },
            { id: "spread", k: "small", label: "Largeur" }, { id: "tune", k: "small", label: "Accord" }, { id: "velamp", k: "small", label: "Vél. ampli" },
          ] },
        ],
      ],
    },
    {
      id: "physics", label: "Physique",
      rows: [
        [{ title: "La bille · tenir au doigt, lâcher d'un geste", ctl: [], custom: "glutapad", grow: 1.1 }, GL_PHYS[0]],
        [GL_PHYS[1], GL_PHYS[2]],
      ],
    },
    {
      id: "motion", label: "Séquence de mouvement",
      rows: [
        [
          { title: "Couloir · sources Séq. A à D de la matrice", banks: GL_LANES, cols: 3, grow: 2.6, custom: "glutalane:{b}", ctl: [
            { id: "sq{b}start", label: "Début" }, { id: "sq{b}end", label: "Fin" },
            { id: "sq{b}dir", k: "vswitch", label: "Sens", short: ["Avant", "Arrière", "A-R", "Hasard"] },
            { id: "sq{b}div", k: "menu", label: "Division" }, { id: "sq{b}smooth", label: "Lissage" }, { id: "sq{b}prob", label: "Probabilité" },
          ] },
          { title: "Séquence", cols: 1, grow: 0.6, ctl: [{ id: "seqon", label: "En marche", on: 1 }, { id: "seqretrig", k: "vswitch", label: "Départ", short: ["Libre", "Note"] }] },
        ],
      ],
    },
    {
      id: "matrix", label: "Matrice et arpège",
      rows: [
        [{ title: "Matrice de modulation · 32 routages (la bille et les couloirs sont des sources)", ctl: [], custom: "matrix", grow: 2 }],
        [
          { title: "LFO à pas", banks: [{ label: "3", value: "3" }, { label: "4", value: "4" }, { label: "5", value: "5" }], cols: 4, grow: 1.6, custom: "steps:lf{b}_s:lf{b}_steps", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", label: "Vitesse" }, { id: "lf{b}_steps", k: "small", label: "Pas" }, { id: "lf{b}_sync", k: "small", label: "Synchro" },
            { id: "lf{b}_mode", k: "small", label: "Mode" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Arpège", cols: 4, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small", label: "Durée" }, { id: "arp_swing", k: "small", label: "Swing" },
            { id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }, { id: "arp_steps", k: "small", label: "Pas" },
          ] },
          { title: "Pas de l'arpège", banks: [{ label: "Transposition", value: "t" }, { label: "Vélocité", value: "v" }], ctl: [], custom: "steps:arp_{b}:arp_steps", grow: 1.4 },
        ],
      ],
    },
  ],
};

/** Good Doctor (v0.47 (maj. 5 nov.), principe des grands synthés à tables d'ondes hybrides) : bleu nuit, boutons
    noirs à arcs turquoise ; les trois oscillateurs (onglets A, B, C), sous-oscillateur et bruit, les
    deux filtres en tête ; enveloppes, LFO, macros, voix et distorsion dessous ; modulation et matrice en pages. */
const GD_ENVS = [{ label: "1 Ampli", value: "a" }, { label: "2 Filtres", value: "e2" }, { label: "3 Hauteur", value: "e3" }];
const gooddoc: FaceLayout = {
  theme: "docteur",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Oscillateur", banks: [{ label: "A", value: "a" }, { label: "B", value: "b" }, { label: "C", value: "c" }], cols: 7, grow: 2.3, custom: "gdosc:{b}", ctl: [
            { id: "{b}_on", label: "Marche", on: 1 }, { id: "{b}_tab", k: "menu", label: "Table" }, { id: "{b}_w1", k: "menu", label: "Déform. 1" }, { id: "{b}_w2", k: "menu", label: "Déform. 2" },
            { id: "{b}_dest", k: "menu", label: "Sortie" }, { id: "{b}_oct", k: "small", label: "Octave" }, { id: "{b}_semi", k: "small", label: "Demi-tons" },
            { id: "{b}_pos", k: "big", label: "Position" }, { id: "{b}_w1amt", k: "big", label: "Intens. 1" }, { id: "{b}_w2amt", label: "Intens. 2" },
            { id: "{b}_lvl", k: "big", label: "Niveau" }, { id: "{b}_fine", k: "small", label: "Fin" }, { id: "{b}_phase", k: "small", label: "Phase" }, { id: "{b}_rand", k: "small", label: "Hasard" },
          ] },
          // 18 nov. : mélangeur — les cinq sources côte à côte (marche, niveau, sortie), comme sur les grands
          // synthés à tables d'ondes ; monter le niveau d'un oscillateur éteint l'allume (GoodDocMix)
          { title: "Mélangeur", cols: 5, grow: 1.5, custom: "gdmix", ctl: [
            { id: "a_on", label: "A", on: 1 }, { id: "b_on", label: "B", on: 1 }, { id: "c_on", label: "C", on: 1 }, { id: "subshape", k: "menu", label: "Forme sous-osc." }, { id: "ncolor", k: "small", label: "Couleur" },
            { id: "a_lvl", k: "fader", label: "A" }, { id: "b_lvl", k: "fader", label: "B" }, { id: "c_lvl", k: "fader", label: "C" }, { id: "sub", k: "fader", label: "Sous-osc." }, { id: "noise", k: "fader", label: "Bruit" },
            { id: "a_dest", k: "menu", label: "Sortie A" }, { id: "b_dest", k: "menu", label: "Sortie B" }, { id: "c_dest", k: "menu", label: "Sortie C" }, { id: "subdest", k: "menu", label: "Sortie sous-osc." }, { id: "ndest", k: "menu", label: "Sortie bruit" },
            { gap: true }, { gap: true }, { gap: true }, { id: "suboct", k: "small", label: "Octave" }, { gap: true },
          ] },
          { title: "Filtre", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 4, grow: 1.4, ctl: [
            { id: "f{b}on", label: "Marche", on: 1 }, { id: "f{b}type", k: "menu", label: "Type" }, { id: "route", k: "vswitch", label: "Filtres", short: ["Série", "Parallèle"] }, { id: "f{b}mix", k: "small", label: "Mélange" },
            { id: "f{b}cut", k: "big", label: "Coupure" }, { id: "f{b}res", label: "Résonance" }, { id: "f{b}drive", k: "small", label: "Gain" }, { id: "f{b}shape", k: "small", label: "Forme" },
            { id: "f{b}env", label: "Env. 2" }, { id: "f{b}key", k: "small", label: "Clavier" }, { id: "f{b}vel", k: "small", label: "Vélocité" },
          ] },
        ],
        [
          { title: "Enveloppe", banks: GD_ENVS, cols: 5, grow: 1.3, tone: "oklch(0.78 0.13 195)", ctl: [
            { id: "{b}a", k: "fader", label: "A" }, { id: "{b}d", k: "fader", label: "D" }, { id: "{b}s", k: "fader", label: "S" }, { id: "{b}r", k: "fader", label: "R" },
            { id: "pamt", k: "small", label: "Hauteur" },
          ] },
          { title: "LFO", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 2, grow: 0.7, ctl: [
            { id: "l{b}rate", k: "big", label: "Vitesse" }, { id: "l{b}shape", k: "vswitch", label: "Forme", short: ["Sin.", "Tri.", "Scie", "Carré", "A&M"] },
            { id: "l{b}sync", k: "small", label: "Synchro" }, { id: "l{b}delay", k: "small", label: "Retard" },
          ] },
          { title: "Macros", cols: 4, grow: 1, ctl: [
            { id: "macro1", label: "1" }, { id: "macro2", label: "2" }, { id: "macro3", label: "3" }, { id: "macro4", label: "4" },
            { id: "macro5", label: "5" }, { id: "macro6", label: "6" }, { id: "macro7", label: "7" }, { id: "macro8", label: "8" },
          ] },
          { title: "Distorsion", cols: 1, grow: 0.5, ctl: [{ id: "dtype", k: "menu", label: "Type" }, { id: "ddrive", label: "Poussée" }, { id: "dmix", k: "small", label: "Mélange" }] },
          { title: "Voix et sortie", cols: 3, grow: 1, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Unisson"] }, { id: "level", k: "big", label: "Volume" }, { id: "glide", k: "small", label: "Glisse" },
            { id: "voices", k: "small", label: "Voix" }, { id: "univoices", k: "small", label: "Unisson" }, { id: "unidetune", k: "small", label: "Désacc." },
            { id: "spread", k: "small", label: "Largeur" }, { id: "tune", k: "small", label: "Accord" }, { id: "velamp", k: "small", label: "Vél. ampli" },
          ] },
        ],
      ],
    },
    {
      id: "mod", label: "Modulation",
      rows: [
        [
          { title: "Env.", banks: [{ label: "4", value: "1" }, { label: "5", value: "2" }], cols: 4, grow: 1.1, ctl: [
            { id: "me{b}_att", label: "Attaque" }, { id: "me{b}_dec", label: "Déclin" }, { id: "me{b}_sus", label: "Niveau" }, { id: "me{b}_rel", label: "Relâch." },
            { id: "me{b}_dly", k: "small", label: "Retard" }, { id: "me{b}_hld", k: "small", label: "Tenue" },
            { stack: [{ id: "me{b}_loop", label: "Boucle" }, { id: "me{b}_sync", label: "Tempo" }] }, { id: "me{b}_vel", k: "small", label: "Vélocité" },
          ] },
          { title: "LFO à pas", banks: [{ label: "3", value: "3" }, { label: "4", value: "4" }, { label: "5", value: "5" }], cols: 4, grow: 1.6, custom: "steps:lf{b}_s:lf{b}_steps", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", label: "Vitesse" }, { id: "lf{b}_steps", k: "small", label: "Pas" }, { id: "lf{b}_sync", k: "small", label: "Synchro" },
            { id: "lf{b}_mode", k: "small", label: "Mode" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
        ],
        [
          { title: "Arpège", cols: 4, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small", label: "Durée" }, { id: "arp_swing", k: "small", label: "Swing" },
            { id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }, { id: "arp_steps", k: "small", label: "Pas" },
          ] },
          { title: "Pas de l'arpège", banks: [{ label: "Transposition", value: "t" }, { label: "Vélocité", value: "v" }], ctl: [], custom: "steps:arp_{b}:arp_steps", grow: 1.4 },
        ],
      ],
    },
    {
      id: "matrix", label: "Matrice",
      rows: [[{ title: "Matrice de modulation · 32 routages (enveloppes, LFO, macros, jeu → n'importe quel réglage)", ctl: [], custom: "matrix", grow: 2 }]],
    },
  ],
};

/** Après la 1.1 : Good Doctor 2 — principe des grands synthés hybrides de 2025 : pages Oscillateurs, Mélangeur,
    Effets, Modulation, Matrice et Éditeur de tables ; écrans verts sur fond graphite (dessin propre à GOLEM). */
const GD2_OSC = [{ label: "A", value: "a" }, { label: "B", value: "b" }, { label: "C", value: "c" }];
const gooddoc2: FaceLayout = {
  theme: "docteur2",
  wheels: true,
  pages: [
    {
      id: "osc", label: "Oscillateurs",
      rows: [
        [
          { title: "Oscillateur", banks: GD2_OSC, bankGroup: "gd2osc", cols: 7, grow: 2.3, custom: "gd2osc:{b}", ctl: [
            { id: "{b}_on", label: "Marche", on: 1 }, { id: "{b}_mode", k: "vswitch", label: "Mode", short: ["Tables", "Grains", "Spectral"] },
            { id: "{b}_tab", k: "menu", label: "Table" }, { id: "{b}_dest", k: "menu", label: "Sortie" },
            { id: "{b}_oct", k: "small", label: "Octave" }, { id: "{b}_semi", k: "small", label: "Demi-tons" }, { id: "{b}_fine", k: "small", label: "Fin" },
            { id: "{b}_pos", k: "big", label: "Position" }, { id: "{b}_lvl", k: "big", label: "Niveau" }, { id: "{b}_pan", label: "Pan" },
            { id: "{b}_phase", k: "small", label: "Phase" }, { id: "{b}_rand", k: "small", label: "Hasard" },
            { id: "{b}_inv", label: "Inversé", on: 1 }, { id: "{b}_delay", k: "small", label: "Retard" },
          ] },
          { title: "Déformations · grains · partiels", banks: GD2_OSC, bankGroup: "gd2osc", ctl: [], custom: "gd2modes:{b}", grow: 1.5 },
          { title: "Filtre", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }], cols: 4, grow: 1.4, ctl: [
            { id: "f{b}on", label: "Marche", on: 1 }, { id: "f{b}type", k: "menu", label: "Type" }, { id: "route", k: "vswitch", label: "Filtres", short: ["Série", "Parallèle"] }, { id: "f{b}mix", k: "small", label: "Mélange" },
            { id: "f{b}cut", k: "big", label: "Coupure" }, { id: "f{b}res", label: "Résonance" }, { id: "f{b}drive", k: "small", label: "Gain" }, { id: "f{b}shape", k: "small", label: "Forme" },
            { id: "f{b}env", label: "Env. 2" }, { id: "f{b}key", k: "small", label: "Clavier" }, { id: "f{b}vel", k: "small", label: "Vélocité" },
          ] },
        ],
        [
          { title: "Sous-osc. et bruit", cols: 3, grow: 0.9, ctl: [
            { id: "sub_on", label: "Sous-osc.", on: 1 }, { id: "subshape", k: "menu", label: "Forme" }, { id: "suboct", k: "small", label: "Octave" },
            { id: "noise_on", label: "Bruit", on: 1 }, { id: "ntype", k: "menu", label: "Type" }, { id: "ncolor", k: "small", label: "Couleur" },
          ] },
          { title: "Enveloppe", banks: [{ label: "1 ampli", value: "1" }, { label: "2 filtres", value: "2" }, { label: "3 hauteur", value: "3" }, { label: "4", value: "4" }],
            cols: 8, grow: 1.8, custom: "gd2env:{b}", tone: "oklch(0.84 0.17 140)", ctl: [
            { id: "e{b}a", k: "fader", label: "A" }, { id: "e{b}h", k: "fader", label: "T" }, { id: "e{b}d", k: "fader", label: "D" }, { id: "e{b}s", k: "fader", label: "S" },
            { id: "e{b}r", k: "fader", label: "R" }, { id: "e{b}ca", k: "small", label: "Courbe A" }, { id: "e{b}cd", k: "small", label: "Courbe D" }, { id: "e{b}cr", k: "small", label: "Courbe R" },
            { id: "pamt", k: "small", label: "Hauteur (3)" },
          ] },
          { title: "LFO", banks: [{ label: "1", value: "1" }, { label: "2", value: "2" }, { label: "3", value: "3" }], cols: 4, grow: 1.2, custom: "gd2lfo:{b}", ctl: [
            { id: "l{b}rate", k: "big", label: "Vitesse" }, { id: "l{b}shape", k: "menu", label: "Forme" }, { id: "l{b}sync", k: "small", label: "Synchro" },
            { id: "l{b}mode", k: "vswitch", label: "Mode", short: ["Libre", "Redécl.", "1 coup"] },
            { id: "l{b}delay", k: "small", label: "Retard" }, { id: "l{b}smooth", k: "small", label: "Lissage" }, { id: "l{b}phase", k: "small", label: "Phase" },
          ] },
          { title: "Macros", cols: 4, grow: 0.9, ctl: [
            { id: "macro1", label: "1" }, { id: "macro2", label: "2" }, { id: "macro3", label: "3" }, { id: "macro4", label: "4" },
            { id: "macro5", label: "5" }, { id: "macro6", label: "6" }, { id: "macro7", label: "7" }, { id: "macro8", label: "8" },
          ] },
          { title: "Voix et sortie", cols: 3, grow: 0.9, ctl: [
            { id: "vmode", k: "vswitch", label: "Jeu", short: ["Poly", "Mono", "Legato"] }, { id: "level", k: "big", label: "Volume" }, { id: "glide", k: "small", label: "Glisse" },
            { id: "voices", k: "small", label: "Voix" }, { id: "tune", k: "small", label: "Accord" }, { id: "spread", k: "small", label: "Largeur" },
            { id: "velamp", k: "small", label: "Vél. ampli" },
          ] },
        ],
      ],
    },
    {
      id: "mix", label: "Mélangeur",
      rows: [
        [
          { title: "Mélangeur · A, B, C, sous-oscillateur, bruit", cols: 5, grow: 2.2, custom: "gd2mix", ctl: [
            { id: "a_on", label: "A", on: 1 }, { id: "b_on", label: "B", on: 1 }, { id: "c_on", label: "C", on: 1 }, { id: "sub_on", label: "Sous-osc.", on: 1 }, { id: "noise_on", label: "Bruit", on: 1 },
            { id: "a_lvl", k: "fader", label: "A" }, { id: "b_lvl", k: "fader", label: "B" }, { id: "c_lvl", k: "fader", label: "C" }, { id: "sub", k: "fader", label: "Sous-osc." }, { id: "noise", k: "fader", label: "Bruit" },
            { id: "a_pan", label: "Pan A" }, { id: "b_pan", label: "Pan B" }, { id: "c_pan", label: "Pan C" }, { id: "subpan", label: "Pan sous-osc." }, { id: "npan", label: "Pan bruit" },
            { id: "a_dest", k: "menu", label: "Sortie A" }, { id: "b_dest", k: "menu", label: "Sortie B" }, { id: "c_dest", k: "menu", label: "Sortie C" }, { id: "subdest", k: "menu", label: "Sortie sous-osc." }, { id: "ndest", k: "menu", label: "Sortie bruit" },
            { id: "a_inv", label: "Inv. A", on: 1 }, { id: "b_inv", label: "Inv. B", on: 1 }, { id: "c_inv", label: "Inv. C", on: 1 }, { id: "subinv", label: "Inv. sous-osc.", on: 1 }, { gap: true },
            { id: "a_delay", k: "small", label: "Retard A" }, { id: "b_delay", k: "small", label: "Retard B" }, { id: "c_delay", k: "small", label: "Retard C" }, { id: "suboct", k: "small", label: "Octave" }, { id: "ncolor", k: "small", label: "Couleur" },
          ] },
          { title: "Filtres", cols: 2, grow: 0.7, ctl: [
            { id: "route", k: "vswitch", label: "Filtres", short: ["Série", "Parallèle"] }, { id: "level", k: "big", label: "Volume" },
            { id: "f1on", label: "F1", on: 1 }, { id: "f2on", label: "F2", on: 1 }, { id: "f1cut", label: "Coupure 1" }, { id: "f2cut", label: "Coupure 2" },
          ] },
        ],
      ],
    },
    {
      id: "fx", label: "Effets",
      rows: [
        [
          { title: "Chaîne · ordre et marche", ctl: [], custom: "gd2chain", grow: 1.1 },
          { title: "Élargisseur", cols: 2, grow: 0.8, ctl: [
            { id: "hy_on", label: "Marche", on: 1 }, { id: "hy_mix", k: "big", label: "Mélange" }, { id: "hy_rate", label: "Vitesse" }, { id: "hy_depth", label: "Désaccord" }, { id: "hy_dim", label: "Espace" },
          ] },
          { title: "Distorsion", cols: 3, grow: 1, ctl: [
            { id: "ds_on", label: "Marche", on: 1 }, { id: "ds_type", k: "menu", label: "Type" }, { id: "ds_pre", k: "menu", label: "Préfiltre" },
            { id: "ds_drive", k: "big", label: "Poussée" }, { id: "ds_freq", label: "Fréquence" }, { id: "ds_mix", label: "Mélange" },
          ] },
          { title: "Compresseur · 3 bandes", cols: 5, grow: 1.6, ctl: [
            { id: "cp_on", label: "Marche", on: 1 }, { id: "cp_mode", k: "vswitch", label: "Mode", short: ["Simple", "3 bandes"] }, { id: "cp_depth", k: "big", label: "Profondeur" },
            { id: "cp_time", label: "Temps" }, { id: "cp_in", k: "small", label: "Entrée" },
            { id: "cp_up", label: "Vers le haut" }, { id: "cp_down", label: "Vers le bas" }, { id: "cp_out", k: "small", label: "Sortie" },
            { id: "cp_thr", k: "small", label: "Seuil (simple)" }, { id: "cp_ratio", k: "small", label: "Rapport" },
            { id: "cp_low", k: "small", label: "Grave" }, { id: "cp_mid", k: "small", label: "Médium" }, { id: "cp_high", k: "small", label: "Aigu" },
          ] },
        ],
        [
          { title: "Flanger", cols: 3, grow: 0.8, ctl: [{ id: "fl_on", label: "Marche", on: 1 }, { id: "fl_rate", label: "Vitesse" }, { id: "fl_depth", label: "Profondeur" }, { id: "fl_feed", label: "Réinjection" }, { id: "fl_mix", label: "Mélange" }] },
          { title: "Phaser", cols: 3, grow: 0.8, ctl: [{ id: "ph_on", label: "Marche", on: 1 }, { id: "ph_rate", label: "Vitesse" }, { id: "ph_depth", label: "Profondeur" }, { id: "ph_freq", label: "Centre" }, { id: "ph_feed", label: "Réinjection" }, { id: "ph_mix", label: "Mélange" }] },
          { title: "Chorus", cols: 3, grow: 0.8, ctl: [{ id: "ch_on", label: "Marche", on: 1 }, { id: "ch_rate", label: "Vitesse" }, { id: "ch_depth", label: "Profondeur" }, { id: "ch_delay", label: "Retard" }, { id: "ch_feed", label: "Réinjection" }, { id: "ch_mix", label: "Mélange" }] },
          { title: "Délai", cols: 4, grow: 1.1, ctl: [
            { id: "dl_on", label: "Marche", on: 1 }, { id: "dl_mode", k: "vswitch", label: "Mode", short: ["Simple", "Ping-pong", "Bande"] }, { id: "dl_sync", label: "Tempo", on: 1 }, { id: "dl_div", k: "menu", label: "Division" },
            { id: "dl_time", label: "Temps" }, { id: "dl_feed", label: "Réinjection" }, { id: "dl_mix", k: "big", label: "Mélange" }, { id: "dl_low", k: "small", label: "Coupe-bas" }, { id: "dl_high", k: "small", label: "Coupe-haut" },
          ] },
        ],
        [
          { title: "Filtre", cols: 3, grow: 0.8, ctl: [{ id: "fi_on", label: "Marche", on: 1 }, { id: "fi_type", k: "menu", label: "Type" }, { id: "fi_cut", k: "big", label: "Coupure" }, { id: "fi_res", label: "Résonance" }, { id: "fi_drive", label: "Gain" }, { id: "fi_mix", label: "Mélange" }] },
          { title: "Égaliseur", cols: 4, grow: 1, ctl: [
            { id: "eq_on", label: "Marche", on: 1 }, { id: "eq_lf", k: "small", label: "Grave (Hz)" }, { id: "eq_mf", k: "small", label: "Médium (Hz)" }, { id: "eq_hf", k: "small", label: "Aigu (Hz)" },
            { id: "eq_mq", k: "small", label: "Largeur" }, { id: "eq_lg", label: "Grave" }, { id: "eq_mg", label: "Médium" }, { id: "eq_hg", label: "Aigu" },
          ] },
          { title: "Réverbération", cols: 4, grow: 1, ctl: [
            { id: "rv_on", label: "Marche", on: 1 }, { id: "rv_size", k: "big", label: "Taille" }, { id: "rv_mix", k: "big", label: "Mélange" }, { id: "rv_pre", k: "small", label: "Pré-délai" },
            { id: "rv_bright", label: "Brillance" }, { id: "rv_low", k: "small", label: "Coupe-bas" }, { id: "rv_width", label: "Largeur" },
          ] },
        ],
      ],
    },
    {
      id: "mod", label: "Modulation",
      rows: [
        [
          { title: "Env. 5", cols: 4, grow: 1.1, ctl: [
            { id: "me1_att", label: "Attaque" }, { id: "me1_dec", label: "Déclin" }, { id: "me1_sus", label: "Niveau" }, { id: "me1_rel", label: "Relâch." },
            { id: "me1_dly", k: "small", label: "Retard" }, { id: "me1_hld", k: "small", label: "Tenue" },
            { stack: [{ id: "me1_loop", label: "Boucle" }, { id: "me1_sync", label: "Tempo" }] }, { id: "me1_vel", k: "small", label: "Vélocité" },
          ] },
          { title: "LFO à pas (dessinables)", banks: [{ label: "4", value: "4" }, { label: "5", value: "5" }], cols: 4, grow: 1.6, custom: "steps:lf{b}_s:lf{b}_steps", ctl: [
            { id: "lf{b}_shape", label: "Forme" }, { id: "lf{b}_rate", label: "Vitesse" }, { id: "lf{b}_steps", k: "small", label: "Pas" }, { id: "lf{b}_sync", k: "small", label: "Synchro" },
            { id: "lf{b}_mode", k: "small", label: "Mode" }, { id: "lf{b}_delay", k: "small", label: "Retard" },
          ] },
        ],
        [
          { title: "Arpège", cols: 4, grow: 1, ctl: [
            { stack: [{ id: "arp_on", label: "En marche" }, { id: "arp_mode", label: "Mode" }] }, { id: "arp_rate", label: "Division" }, { id: "arp_gate", k: "small", label: "Durée" }, { id: "arp_swing", k: "small", label: "Swing" },
            { id: "arp_hold", label: "Maintien" }, { id: "arp_oct", k: "switch", label: "Octaves" }, { id: "arp_steps", k: "small", label: "Pas" },
          ] },
          { title: "Pas de l'arpège", banks: [{ label: "Transposition", value: "t" }, { label: "Vélocité", value: "v" }], ctl: [], custom: "steps:arp_{b}:arp_steps", grow: 1.4 },
        ],
      ],
    },
    {
      id: "matrix", label: "Matrice",
      rows: [[{ title: "Matrice de modulation · 32 routages (4 enveloppes, 5 LFO, macros, jeu → n'importe quel réglage, effets compris)", ctl: [], custom: "matrix", grow: 2 }]],
    },
    {
      id: "editor", label: "Éditeur de tables",
      rows: [[{ title: "Éditeur de tables d'ondes · Perso 1 à 3 (emplacements d'échantillon 2 à 4)", ctl: [], custom: "gd2editor", grow: 2 }]],
    },
  ],
};

/** v0.49 : FM Player — la machine émulée en grand (écran au milieu, ses commandes de part et d'autre, son
    clavier dessous), puis le firmware et la sortie. Dessin propre à GOLEM. */
const fmplayer: FaceLayout = {
  theme: "fmplayer",
  wheels: true,
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "La machine", ctl: [], custom: "fm1", grow: 3.4 },
          { title: "Firmware", cols: 3, grow: 1, custom: "fm1fw", ctl: [
            { id: "level", k: "big", label: "Volume" }, { id: "transpose", k: "small", label: "Transpos." }, { id: "lead", k: "small", label: "Avance" },
          ] },
        ],
      ],
    },
  ],
};

/** Micro Tonnerre (après la 1.1) : générateur de rythmes à synthèse universelle — à gauche les
    8 canaux (jouer, choisir), l'allure du canal choisi et son mélange ; en dessous son
    oscillateur, son bruit et sa vélocité (« {z} » = canal choisi) ; les pas du canal en bas. Le kit
    en colonnes dans la page « Le kit ». Le morphing entre deux kits : scènes et crossfader. */
const microton: FaceLayout = {
  theme: "tonnerre",
  pages: [
    {
      id: "sound", label: "Canaux",
      rows: [
        [
          { title: "Canaux · numéro : jouer · nom : régler", ctl: [], custom: "mtchans", grow: 1.05 },
          { title: "Allure du canal", ctl: [], custom: "mtshape", grow: 1.25, tone: "oklch(0.88 0.17 95)" },
          { title: "Mélange", cols: 4, grow: 1.5, ctl: [
            { id: "p{z}_mix", k: "big", label: "Osc. / bruit" }, { id: "p{z}_eqf", label: "Fréq. EQ" }, { id: "p{z}_eqg", label: "Gain EQ" }, { id: "p{z}_dist", k: "big", label: "Distorsion" },
            { id: "p{z}_level", k: "big", label: "Niveau" }, { id: "p{z}_pan", label: "Pan" }, { id: "p{z}_choke", k: "vswitch", label: "Étouffement", short: ["—", "1", "2", "3", "4"] },
          ] },
        ],
        [
          { title: "Oscillateur", cols: 5, grow: 1.9, tone: "oklch(0.8 0.15 240)", ctl: [
            { id: "p{z}_wave", k: "vswitch", label: "Forme", short: ["Sinus", "Triangle", "Scie"] }, { id: "p{z}_freq", k: "big", label: "Fréquence" },
            { id: "p{z}_mmode", k: "vswitch", label: "Modulation", short: ["Déclin", "Sinus", "Aléa."] }, { id: "p{z}_mamt", k: "big", label: "Quantité" },
            { stack: [{ id: "p{z}_mtime", k: "small", label: "Durée" }, { id: "p{z}_mrate", k: "small", label: "Vitesse" }] },
            { id: "p{z}_oatk", k: "fader", label: "Attaque" }, { id: "p{z}_odec", k: "fader", label: "Déclin" },
          ] },
          { title: "Bruit", cols: 5, grow: 1.8, tone: "oklch(0.8 0.13 30)", ctl: [
            { id: "p{z}_nfilt", k: "vswitch", label: "Filtre", short: ["P.-bas", "P.-bande", "P.-haut"] }, { id: "p{z}_nfreq", k: "big", label: "Fréquence" },
            { id: "p{z}_nq", label: "Résonance" }, { id: "p{z}_nenv", k: "vswitch", label: "Enveloppe", short: ["Expo.", "Lin.", "Claquée"] },
            { id: "p{z}_nstereo", label: "Stéréo", on: 1 },
            { id: "p{z}_natk", k: "fader", label: "Attaque" }, { id: "p{z}_ndec", k: "fader", label: "Déclin" },
          ] },
          { title: "Vélocité", cols: 1, grow: 0.5, ctl: [
            { id: "p{z}_vosc", k: "small", label: "Osc." }, { id: "p{z}_vnoise", k: "small", label: "Bruit" }, { id: "p{z}_vmod", k: "small", label: "Modul." },
          ] },
        ],
        [
          { title: "Envois · kit", cols: 3, grow: 0.8, ctl: [
            { id: "p{z}_sendA", label: "Envoi A" }, { id: "p{z}_sendB", label: "Envoi B" }, { id: "volume", k: "big", label: "Volume du kit" },
          ] },
          { title: "Accent", ctl: [], custom: "accent", grow: 0 },
          { title: "Pas du canal choisi", ctl: [], custom: "stepkeysR", grow: 3 },
        ],
      ],
    },
    {
      id: "kit", label: "Le kit",
      rows: [
        [{ title: "Le kit · toucher un nom pour choisir le canal", ctl: [], custom: "mtkit" }],
        [{ title: "Accent", ctl: [], custom: "accent", grow: 0 }, { title: "Pas du canal choisi", ctl: [], custom: "stepkeysR" }],
      ],
    },
  ],
};

/** Kick Forge (1.2) : constructeur de kicks par couches. Page « Kick » : bandeau (style, tonalité,
    hasard), grand afficheur réglable à la souris, couches comme des pistes, réglages de la couche
    choisie (« {n} » = couche choisie, 1 à 8) ; page « Effets du kick » : rack de 9 modules et
    réglages du module choisi ; page « Bibliothèque » : familles et morceaux. */
const kickforge: FaceLayout = {
  theme: "forge",
  pages: [
    {
      id: "sound", label: "Kick",
      rows: [
        [{ title: "Kick · cliquer sur le nom d'une couche pour ouvrir la bibliothèque", ctl: [], custom: "kfdisplay:top", grow: 1 }],
        [{ title: "Couches · cliquer pour choisir · glisser un sample sur une couche", ctl: [], custom: "kflayers", grow: 1 }],
        [
          { title: "Couche {n} · source", cols: 7, grow: 2.1, tone: "oklch(0.78 0.17 55)", ctl: [
            { id: "l{n}_vol", k: "big", label: "Volume" }, { id: "l{n}_pitch", label: "Transpos." }, { id: "l{n}_fine", k: "small", label: "Accord fin" }, { id: "l{n}_stretch", label: "Étirement" },
            { id: "l{n}_offset", label: "Décalage" }, { id: "l{n}_len", label: "Longueur" }, { id: "l{n}_keytrk", label: "Sample accordé" },
          ] },
          { title: "Fondus", cols: 4, grow: 1.1, tone: "oklch(0.82 0.12 200)", ctl: [
            { id: "l{n}_filen", label: "Entrée" }, { id: "l{n}_ficurve", k: "small", label: "Courbe" }, { id: "l{n}_folen", label: "Sortie" }, { id: "l{n}_focurve", k: "small", label: "Courbe" },
          ] },
          { title: "Tranche · filtres et égaliseur", cols: 6, grow: 1.7, tone: "oklch(0.78 0.15 145)", ctl: [
            { id: "l{n}_hp", label: "Passe-haut" }, { id: "l{n}_lp", label: "Passe-bas" }, { id: "l{n}_eqlo", k: "small", label: "Grave" },
            { id: "l{n}_eqmidf", k: "small", label: "Fréq. médium" }, { id: "l{n}_eqmidg", k: "small", label: "Médium" }, { id: "l{n}_eqhi", k: "small", label: "Aigu" },
          ] },
          { title: "Tranche · attaque et distorsion", cols: 5, grow: 1.6, tone: "oklch(0.68 0.2 25)", ctl: [
            { id: "l{n}_tratk", label: "Attaque" }, { id: "l{n}_trsus", label: "Maintien" }, { id: "l{n}_drvtype", k: "menu", label: "Distorsion" },
            { id: "l{n}_drv", k: "big", label: "Poussée" }, { id: "l{n}_drvmix", k: "small", label: "Mélange" },
          ] },
          { title: "Jeu", cols: 5, grow: 1.3, ctl: [
            { id: "volume", k: "big", label: "Volume" }, { id: "velsens", k: "small", label: "Vélocité" },
            { id: "pitchmode", k: "vswitch", label: "Note jouée", short: ["Fixe", "Suivie"] }, { id: "autogain", label: "Gain auto." },
            { id: "phasealign", label: "Phase auto." },
          ] },
        ],
      ],
    },
    {
      id: "fx", label: "Effets du kick",
      rows: [
        [
          { title: "Rack · ordre des effets", ctl: [], custom: "kfrack", grow: 0.8 },
          { title: "Module choisi", ctl: [], custom: "kfmodule", grow: 2.2 },
        ],
        [{ title: "Kick", ctl: [], custom: "kfdisplay", grow: 1 }],
      ],
    },
  ],
};

/** 1.3 : Gamer Lover (puces de consoles 8 et 16 bits) : coque violet nuit, sérigraphie jaune cartouche ;
    le téléviseur à tube suit la puce, les cartouches la choisissent ; ondes, FM, échantillons et volume
    sur la page Son, les tables (arpège, hauteur), la batterie et la sortie d'époque sur la seconde page. */
const GM_OPS = [{ label: "1", value: "1" }, { label: "2", value: "2" }, { label: "3", value: "3" }, { label: "4", value: "4" }];
const gamer: FaceLayout = {
  theme: "cartouche",
  // téléphone (essai) : un seul écran façon machine à pads — 8 menus, le téléviseur, 8 boutons
  phonePages: [{ id: "screen", label: "Écran", rows: [[{ title: "", ctl: [], custom: "gamerakai", grow: 1 }]] }],
  pages: [
    {
      id: "sound", label: "Son",
      rows: [
        [
          { title: "Écran · la puce choisie", ctl: [], custom: "gamertv", grow: 1.5 },
          { title: "Puce", cols: 3, grow: 1.2, ctl: [
            { id: "level", k: "big", label: "Niveau" }, { id: "voices", label: "Voix" }, { id: "tune", label: "Accord" },
            { id: "glide", label: "Glisse" }, { id: "tick", k: "vswitch", label: "Cadence", short: ["60 Hz", "50 Hz"] },
          ] },
          { title: "Volume · table d'enveloppe", cols: 2, grow: 0.9, ctl: [
            { id: "vatk", label: "Attaque" }, { id: "vdec", label: "Déclin" }, { id: "vsus", label: "Maintien" }, { id: "vrel", label: "Relâchement" },
          ] },
        ],
        [
          { title: "Ondes 8 bits et portable", cols: 5, grow: 1.6, tone: "oklch(0.86 0.2 145)", ctl: [
            { id: "wave8", k: "vswitch", label: "Onde", short: ["Carré", "Tri./table", "Bruit"] },
            { id: "duty", k: "vswitch", label: "Largeur", short: ["12,5", "25", "50", "75"] },
            { id: "dutymove", k: "menu", label: "Largeur animée" }, { id: "gbwave", k: "menu", label: "Table 4 bits" },
            { id: "noisemode", k: "toggle", label: "Bruit court", on: 1 },
          ] },
          { title: "FM 16 bits · opérateurs", banks: GM_OPS, cols: 6, grow: 2, tone: "oklch(0.8 0.15 235)", ctl: [
            { id: "fmalgo", k: "big", label: "Algorithme" }, { id: "fmfb", label: "Rétroaction" },
            { id: "fmmul{b}", label: "Rapport" }, { id: "fmlev{b}", label: "Niveau" },
            { id: "fmdec", label: "Déclin mod." }, { id: "fmgrain", k: "small", label: "Grain" },
          ] },
          { title: "Échantillons 16 bits", cols: 2, grow: 0.9, tone: "oklch(0.85 0.15 75)", ctl: [
            { id: "smpl", k: "menu", label: "Échantillon" },
            { id: "smplsmooth", k: "vswitch", label: "Lecture", short: ["Douce", "Dure"] },
          ] },
        ],
      ],
    },
    {
      id: "tables", label: "Tables et sortie",
      rows: [
        [
          { title: "Écran · la puce choisie", ctl: [], custom: "gamertv", grow: 1.3 },
          { title: "Tables · arpège", cols: 2, grow: 0.8, ctl: [
            { id: "arp", k: "menu", label: "Arpège" }, { id: "arpspeed", k: "big", label: "Vitesse" },
          ] },
          { title: "Tables · hauteur", cols: 3, grow: 1.3, ctl: [
            { id: "sweep", k: "big", label: "Chute" }, { id: "sweeptime", label: "Durée" },
            { id: "vibdepth", label: "Vibrato" }, { id: "vibrate", label: "Vitesse" }, { id: "vibdelay", label: "Retard" },
          ] },
        ],
        [
          { title: "Batterie · 16 pads", cols: 3, grow: 1, tone: "oklch(0.75 0.2 25)", ctl: [
            { id: "drtune", label: "Accord" }, { id: "drdecay", label: "Déclin" }, { id: "drtone", label: "Timbre" },
          ] },
          { title: "Sortie console", cols: 4, grow: 1.5, ctl: [
            { id: "bits", k: "big", label: "Résolution" },
            { id: "rate", k: "vswitch", label: "Fréquence", short: ["Pleine", "32 k", "22 k", "16 k", "11 k"] },
            { id: "tone", k: "vswitch", label: "Filtre", short: ["Ouvert", "Salon", "Portable", "Télé"] },
            { id: "width", label: "Stéréo" },
          ] },
          { title: "Écho à 8 prises", cols: 4, grow: 1.4, ctl: [
            { id: "echomix", k: "big", label: "Écho" }, { id: "echotime", label: "Temps" }, { id: "echofb", label: "Réinjection" },
            { id: "echofilter", k: "vswitch", label: "Filtre", short: ["Clair", "Doux", "Sombre", "Grotte"] },
          ] },
        ],
      ],
    },
  ],
};

export const FACE_LAYOUTS: Record<string, FaceLayout> = { mono, acid, drumA, drum80, drum90, poly5, polyj, hybrid, morph, pads, octa, ms20, sh101, blo, ob6, orbit, fm6, fm4, fmx, muse, hots, lyra, bitm, drumImp, drumNrv, dfam, voc16, vocstr, vocspec, vocsyn, sampler, keys, sp, gluta, gooddoc, gooddoc2, fmplayer, microton, kickforge, gamer };
