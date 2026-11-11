// Groovebox — OFAD — AGPL-3.0-or-later
// Contrat de données entre le moteur C++ (src/engine/Bridge.cpp, src/PluginEditor.cpp) et l'interface.
// Les fonctions natives renvoient des textes JSON (ASCII) que l'interface décode.

export interface ParamDesc {
  id: string;
  name: string;
  unit: string;
  min: number;
  max: number;
  def: number;
  skew: number;
  integer?: boolean;
  choices?: string[];
}

export interface SectionDesc {
  id: string;
  name: string;
  visual: string;
  tab: string;
  params: number[]; // index des réglages dans ModuleDesc.params
}

export interface ModuleDesc {
  type: string;
  name: string;
  family: string;
  principle: string;
  role?: string;          // v0.35.1, effets : rôle (« Filtre », « Écho à bande »…) quand le nom est un nom propre
  form?: "pedal" | "rack" | "module";             // v0.34, effets : pédalier ou rack
  drumKit: boolean;
  laneNotes?: boolean; // v0.49 : pistes internes qui jouent la note du pas (Pieuvre 8, FM Player)
  params: ParamDesc[];
  sections: SectionDesc[];
  presets: string[];
  presetCats?: string[];               // banque d'usine de chaque préréglage (Basses, Nappes…) ; vide = « Classiques »
  padNames: string[];
  padKinds: string[];
  /** Batterie à machines : chaque machine et les libellés de ses 7 réglages de voix. */
  machines?: { name: string; labels: string[] }[];
  /** Tables d'ondes (wave morph) : allure de chaque onde de la bibliothèque, -1..1. */
  waveShapes?: number[][];
}

export interface Layout {
  numTracks: number;
  numReturns: number;
  masterChannel: number;
  numGroups?: number;       // v0.24 : bus de groupe, après le bus principal
  groupBase?: number;
  slotsPerChannel: number;
  mixerBase: number;
  instrumentBase: number;
  instrumentSlots: number;
  insertBase: number;
  insertSlots: number;
  numInserts: number;
  modSourceBase: number;
  modSourceSlots: number;
  numModSources: number;
  modAmountBase: number;
  trackSoundSlot?: number;  // après la 1.1 : son du pas (numéro de préréglage verrouillé)
  trackSoundMax?: number;
  padBypassBase?: number;   // interrupteurs des inserts de pad (pad × 2 + n)
  padInsertBase?: number;   // réglages des inserts de pad (64 par insert)
  padInserts?: number;
  numScenes: number;
  maxSteps: number;
  numLanes: number;
  numPatterns: number;
}

export interface ChainDesc {
  name: string;
  use: string;
  inserts: [string, string][]; // [type d'effet, préréglage]
}

export interface Catalog {
  instruments: ModuleDesc[];
  effects: ModuleDesc[];
  mixerTrack: ModuleDesc;
  mixerReturn: ModuleDesc;
  mixerMaster: ModuleDesc;
  modSource: ModuleDesc;
  layout: Layout;
  chains: ChainDesc[];
}

export interface TrackInfo {
  name: string;
  color: number;
  instrument: string;
  preset: string;
  mute: boolean;
  solo: boolean;
  out?: number;   // v0.24 : 0 = bus principal, 1 à 7 = bus de groupe (v0.46)
  labels?: string; // après la 1.1 : noms des sections de la piste MIDI, séparés par « | »
  speech?: string; // v0.33 : phrase tapée de la voix interne des vocodeurs (absente = démonstration)
}

export interface TracksData {
  tracks: TrackInfo[];
  inserts: string[][]; // par canal (16 pistes, 4 retours, bus principal, 4 groupes) : type de chaque insert
  insertPresets?: string[][]; // dernier préréglage appliqué à chaque insert
  buses?: { mute: boolean; solo: boolean; name: string }[]; // v0.31 : canaux 16 à 27 (retours, principal, groupes)
}

export interface ModAssign {
  source: number;     // 0..5 sources du canal, 6 = vélocité
  target: number;     // emplacement local du réglage modulé
  amount: number;     // -1..1
  amountSlot: number; // emplacement local de la profondeur
}

export interface ChannelData {
  channel: number;
  mix: number[];
  instrument?: string;
  inst?: number[];
  preset?: string;
  samples?: (string | null)[];
  fx: { type: string; values: number[] }[];
  padfx?: PadFxData[];   // inserts des pads (batteries) : seulement ceux qui ont un effet
  mod: number[][];
  mods: ModAssign[];
}

export interface PadFxData { pad: number; slot: number; type: string; bypass: number; values: number[] }

/** Préréglages de l'utilisateur d'un module ou d'un pad : banques et [banque, nom, famille du pad]. */
export interface UserPresetList {
  folder: string;
  kind: string;
  banks: string[];
  user: [string, string, string][];
  root: string;
}
export type PresetScope = "inst" | "fx" | "pad";

export interface LaneData {
  dir: number;
  length: number;  // longueur propre de la ligne (0 = celle du pattern)
  mute: boolean;
  solo: boolean;
  lockGlide?: boolean; // 1.0 : verrous de la ligne glissés d'un pas à l'autre
  lockMute?: boolean;  // 1.0 : verrous de la ligne coupés (gardés, pas joués)
  vel: number[];
  note: number[];
  len: number[];   // en huitièmes de pas
  micro: number[]; // -32..32 (1/64 de pas)
}

export interface PatternData {
  track: number;
  length: number;
  speed: number;
  mute: boolean;
  solo: boolean;
  division: number;
  pattern: number;   // pattern édité (0..7)
  playing: number;   // pattern joué (-1 = piste arrêtée)
  recQuant: number;  // quantification de l'enregistrement en direct (%)
  accent: number;    // force de l'accent de la piste (%)
  flags: number[];   // fonctions de chaque pas : 1 = accent, 2 = glissé
  /** v0.46 fix27 : valeurs de chaque pas (0 = par défaut) : force de l'accent, durée du glissé (1..100 %). */
  accAmt: number[];
  sldAmt: number[];
  lanes: LaneData[];
}

export interface LaunchTrack {
  playing: number;   // -1 = arrêtée
  queued: number;    // -1 = rien en attente ; 8 = arrêt en attente
  editing: number;
  filled: number[];  // 1 si le pattern contient des notes
  length: number[];  // longueur de chaque pattern (pas)
  division?: number[]; // division de chaque pattern (ordre du moteur)
}

export interface LaunchData {
  quant: number;
  tracks: LaunchTrack[];
  rowScenes: number[]; // scène de réglages rappelée par chaque rangée (-1 = aucune)
  muteTails?: boolean; // coupure / solo : laisser finir les sons et les traînes (faux : coupure immédiate)
}

export interface MidiData {
  learn: number;                       // cible en cours d'apprentissage (-1 = aucune)
  maps: [number, number, number, number][]; // [genre 0 = CC / 1 = note, canal, numéro, cible]
  conflict?: [number, number, number, number, number]; // commande déjà affectée : [genre, canal, numéro, ancienne cible, nouvelle cible]
  /** v0.46 : canaux MIDI — notes et commandes (0 = tous, 1 à 16), commandes actives, canal propre à chaque piste (0 = piste choisie). */
  route?: { note: number; ctrl: number; ctrlOn: boolean; tracks: number[]; lanes?: number[][] }; // lanes : GOLEM 1.0, canal de chaque piste interne T (0 = la T choisie)
  /** v0.46 fix24 : 16 boutons assignables [cible (-1 = libre), position 0..1] et table des valeurs d'écran (JSON). */
  knobs?: [number, number, number?, number?][]; // [cible, position, LFO des boutons 0..4 (v0.46 fix28), profondeur -1..1]
  ctl?: string;
}

export interface StepExtra {
  lane: number;
  step: number;
  cond: number;
  a: number;
  b: number;
  prob: number;
  rat?: number; // répétitions dans le pas (1 à 8)
  rs?: number;  // pente de vélocité des répétitions (-100 à 100 %)
  flam?: number; // flam : écart en ms entre le coup léger et le coup principal (0 = sans)
  arp?: number;  // 1 : note sortie de l'arpégiateur (enregistrée), rejouée sans repasser par lui
  locks: [number, number][]; // [adresse globale, valeur normalisée]
}

export interface SceneData {
  muted: boolean;
  locks: [number, number][];
}

export interface WaveData {
  name: string;
  seconds: number;
  stereo: boolean;
  peaks: number[]; // min, max, min, max…
  onsets?: number[]; // v0.43.3 : attaques détectées (fractions de l'échantillon)
}

/** Morceau (arrangeur) : rangées [mesures, scène (-1 aucune), 16 patterns (-2 garder, -1 arrêt, 0..7)],
    courbes [réglage, lue (1/0), points, dernier temps, 96 valeurs sur la longueur « span » (noires)]. */
export type SongRowData = [number, number, number[]];
export type SongLaneData = [number, number, number, number, number[]];
export interface SongData {
  mode: boolean; loop: boolean; loopFrom?: number; loopTo?: number; loopCount?: number; read: boolean; recPlay: boolean; recMoves: boolean; version: number;
  rows: SongRowData[]; span: number; lanes: SongLaneData[];
}
export const SONG_KEEP = -2, SONG_STOP = -1, SONG_MAX_ROWS = 128, SONG_MAX_BARS = 64;

export interface Tick {
  pv?: [number, number][]; // v0.32.1 : réglages écrits par le contrôleur MIDI ou une macro (adresse, valeur)
  whl?: [number, number]; // v0.32.1 : molettes de la piste choisie (hauteur -1..1, modulation 0..1), y compris du contrôleur MIDI
  t: { playing: boolean; rec: boolean; ppq: number; bpm: number; fill: boolean; brk: boolean; tbrk?: boolean;
    lq?: number; lp?: number;  // v0.43 : boucle la plus longue (durée en noires, avancée 0 à 1)
    mon?: boolean; mp?: number; mrec?: boolean }; // v0.46 fix8 : montage audio (mode, position en noires, prise)
  g: { xfade: number; sceneA: number; sceneB: number; bpm: number; swing: number;
       // v0.46 fix22 : pavé vectoriel (vecOn, vecMode, vecX, vecY, vecOrbit, vecRate, vecDepth, vecPX/vecPY = position jouée, vecSA…vecSD, vecFA…vecFD)
       [key: string]: number };
  hits: [number, number][];
  kn?: number[];   // notes tenues de la piste choisie (clavier, contrôleur, séquence, arpège)
  kh?: number[];   // notes de la piste choisie jouées depuis le dernier envoi
  peaks: number[];
  outlr?: [number, number]; // v0.46 : crêtes gauche / droite de la sortie principale
  och?: number;             // v0.46 : voies de la sortie (1 = mono : le panoramique ne s'entend pas)
  pos: number[];
  tpos: number[];
  eff: [number, number][];
  effg?: [number, number][];         // après la 1.1 : autres canaux, réglages tenus par les scènes A / B (adresse globale)
  fxm: number[][];
  mods?: [number, number][]; // sources de modulation du canal choisi : [valeur, pas en cours]
  mac?: number[];            // valeurs des 8 macros du logiciel hôte (0..1)
  pat: [number, number][];   // par piste : [pattern joué, pattern en attente]
  ph?: number;               // piste choisie : position dans son pattern en pas, avec la fraction (-1 à l'arrêt)
  inst?: number[];           // état de l'instrument de la piste choisie (sampler de performance : enregistreur, position, boucle, machine)
  ke?: number[];             // v0.46 fix28 : valeur jouée du réglage relié à chaque bouton (modulation…), -1 = celle du bouton
  kmods?: [number, number][]; // v0.46 fix28 : sources des boutons (1 à 4 du bus principal) : [valeur, pas]
  ko?: number[];             // v0.46 fix28 : valeur envoyée par chaque bouton avec son LFO (-1 : sans LFO)
  kv?: number[];             // v0.46 fix24 : positions des 16 boutons assignables
  uc?: [number, number][];   // v0.46 fix24 : valeurs d'écran poussées par le MIDI ou un bouton [numéro, valeur 0..1]
  hist?: number[];           // [peut annuler, peut rétablir, photo prise] (1 / 0)
  perf?: number[];           // v0.23 : [calcul audio moyen %, pointe %, mémoire de l'application (Mo), échantillons (Mo), fréquence (Hz), taille des blocs, temps mesuré entre deux blocs (ms) (v0.46 fix16), mémoire de l'appareil (Mo), état Android (-1 ; 100 = indice actif + grands cœurs) (fix17)]
  learn: number;             // apprentissage MIDI en cours
  midi: boolean;             // affectations MIDI changées
  loud: number[];            // [instantanée, courte, intégrée (LUFS), crête vraie, crête vraie max (dBTP), efficace (dB)]
  spec?: number[];           // v0.45.1 : spectre du bus principal, 96 bandes milieu puis 96 côtés (dixièmes de dB), si demandé
  project: boolean;
  seq: boolean;
  structure: boolean;
  song?: [number, number, number, number, number]; // [rangée jouée, position (noires), version, jeu enregistré, mouvements enregistrés]
}

// Divisions, dans l'ordre du moteur (projets enregistrés) ; DIVISION_ORDER : ordre d'affichage
export const DIVISIONS = ["1/8", "1/8T", "1/16", "1/16T", "1/32", "1/4", "1/32T", "1/64"];
export const STEPS_PER_BAR = [8, 12, 16, 24, 32, 4, 48, 64];
export const DIVISION_ORDER = [5, 0, 1, 2, 3, 4, 6, 7];
export const LAUNCH_QUANTS = ["Immédiat", "1 temps", "1 mesure", "2 mesures", "4 mesures"];
// v0.46 fix29 : 16 patterns par piste (A à P), montrés en pages de 8 rangées (A–H, I–P)
export const PATTERN_NAMES = ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P"];
export const STOP_PATTERN = 16;
export const ROW_PAGE = 8;
export const ROW_PAGES = PATTERN_NAMES.length / ROW_PAGE;
/** Nom d'une page de rangées : « A–H », « I–P ». */
export const rowPageName = (page: number) => `${PATTERN_NAMES[page * ROW_PAGE]}–${PATTERN_NAMES[page * ROW_PAGE + ROW_PAGE - 1]}`;
// Commandes MIDI spéciales (cibles négatives, voir PluginProcessor.h)
export const MIDI_XFADE = -2, MIDI_BREAK = -3, MIDI_FILL = -4, MIDI_PLAY = -5;
export const MIDI_START = -6, MIDI_STOP = -7, MIDI_RECORD = -8, MIDI_REC_MOVES = -9, MIDI_SONG_MODE = -10;
/** Coupure et solo de piste pour l'apprentissage MIDI : base - piste (pistes 0 à 15). */
export const MIDI_MUTE_BASE = -100, MIDI_SOLO_BASE = -200;
/** v0.46 : coupure d'un bus (retour, principal, groupe) : -300 - (canal - 16) ; solo d'un groupe : -400 - (canal - 16). */
export const MIDI_BUS_MUTE_BASE = -300, MIDI_BUS_SOLO_BASE = -400;
/** v0.46 : rangées au contrôleur : précédente, suivante, tout arrêter, et lancer A à P (-20 - rangée ; fix29 : I à P). */
export const MIDI_ROW_PREV = -11, MIDI_ROW_NEXT = -12, MIDI_ROW_STOP = -13, MIDI_ROW_BASE = -20;
/** v0.46 : photo des réglages et rappel de la photo au contrôleur. */
export const MIDI_PHOTO = -14, MIDI_RECALL = -15;
/** v0.46 fix24 : bouton assignable k (-500 - k), réglage global h (-600 - h), valeur d'écran n (-700 - n). */
export const MIDI_KNOB_BASE = -500, MIDI_HOST_BASE = -600, MIDI_UI_BASE = -700;
export const NUM_KNOBS = 16, NUM_UI_CTL = 64;
/** Réglages globaux pilotables, dans l'ordre du moteur (ne pas réordonner). */
export const HOST_CTL = ["bpm", "swing", "sceneA", "sceneB", "vecOn", "vecMode", "vecX", "vecY", "vecOrbit", "vecRate", "vecDepth",
  "macro1", "macro2", "macro3", "macro4", "macro5", "macro6", "macro7", "macro8",
  "vecSA", "vecSB", "vecSC", "vecSD", "vecFA", "vecFB", "vecFC", "vecFD", "vecRot", "vecHeight"] as const;
export const HOST_CTL_NAMES: Record<string, string> = {
  bpm: "Tempo", swing: "Swing", sceneA: "Scène A", sceneB: "Scène B", vecOn: "Pavé actif", vecMode: "Pavé : coins", vecX: "Pavé X", vecY: "Pavé Y",
  vecOrbit: "Pavé : mouvement", vecRate: "Pavé : un tour en", vecDepth: "Pavé : taille", vecRot: "Pavé : rotation", vecHeight: "Pavé : hauteur de la forme",
  macro1: "Macro 1", macro2: "Macro 2", macro3: "Macro 3", macro4: "Macro 4", macro5: "Macro 5", macro6: "Macro 6", macro7: "Macro 7", macro8: "Macro 8",
  vecSA: "Pavé : scène A", vecSB: "Pavé : scène B", vecSC: "Pavé : scène C", vecSD: "Pavé : scène D",
  vecFA: "Pavé : effet A", vecFB: "Pavé : effet B", vecFC: "Pavé : effet C", vecFD: "Pavé : effet D",
};
export const hostCode = (id: (typeof HOST_CTL)[number]) => MIDI_HOST_BASE - HOST_CTL.indexOf(id);
export const isKnobCode = (t: number) => t <= MIDI_KNOB_BASE && t > MIDI_KNOB_BASE - NUM_KNOBS;
export const isHostCode = (t: number) => t <= MIDI_HOST_BASE && t > MIDI_HOST_BASE - HOST_CTL.length;
export const isUiCode = (t: number) => t <= MIDI_UI_BASE && t > MIDI_UI_BASE - NUM_UI_CTL;
export const SPEEDS = ["×1/8", "×1/4", "×1/2", "×3/4", "×1", "×3/2", "×2"];
export const DIRECTIONS = ["Avant", "Arrière", "Aller-retour", "Aléatoire"];
export const CONDITIONS = ["Aucune", "A:B", "FILL", "NON FILL", "PRÉ", "NON PRÉ", "VOISIN", "NON VOISIN", "1ER", "NON 1ER"];
/** Conditions et A:B réunis sur un seul encodeur : clé "c" ou "1:a:b". */
export const AB_CHOICES = ["1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4", "1:8", "5:8"];
export const COND_CHOICES = ["0", ...AB_CHOICES.map((ab) => `1:${ab}`), ...CONDITIONS.slice(2).map((_, i) => `${i + 2}`)];
export const condKey = (cond: number, a: number, b: number) => (cond === 1 ? `1:${a}:${b}` : `${cond}`);
/** Liste des conditions, avec la valeur actuelle insérée parmi les A:B si elle n'y est pas (ex. 1:5). */
export const condChoices = (cur: string) => (COND_CHOICES.includes(cur) ? COND_CHOICES
  : [...COND_CHOICES.slice(0, AB_CHOICES.length + 1), cur, ...COND_CHOICES.slice(AB_CHOICES.length + 1)]);
export const parseCond = (k: string) => { const p = k.split(":").map(Number); return p.length === 3 ? { cond: 1, a: p[1], b: p[2] } : { cond: p[0] }; };
export const condText = (k: string) => { const c = parseCond(k); return c.cond === 1 ? `A:B ${c.a}:${c.b}` : CONDITIONS[c.cond]; };
/** Répétitions : nombre et pente (0, décroissante, croissante) sur un seul encodeur, clé "n:pente". */
export const RATCHET_CHOICES = ["1:0", ...[2, 3, 4, 5, 6, 7, 8].flatMap((n) => [`${n}:0`, `${n}:-60`, `${n}:60`])];
export const ratchetKey = (n: number, slope: number) => (n <= 1 ? "1:0" : `${n}:${slope < -20 ? -60 : slope > 20 ? 60 : 0}`);
export const ratchetText = (k: string) => { const [n, s] = k.split(":").map(Number); return n <= 1 ? "Aucune" : `×${n}${s < 0 ? " ↘" : s > 0 ? " ↗" : ""}`; };
export const BREAK_SCENE = 32; // index de la scène Break (0..31 = scènes 1 à 32)

/** Fonctions de pas de la piste. */
export const STEP_ACCENT = 1, STEP_SLIDE = 2, STEP_REC = 4, STEP_TRIGLESS = 8;
/** Flam : écart entre le coup léger et le coup principal (ms). */
export const FLAM_CHOICES = [0, 8, 12, 16, 20, 25, 30, 40, 50, 60];
export const flamText = (ms: number) => (ms === 0 ? "Sans" : `${ms} ms`);

/** Macros du logiciel hôte : valeur (0..1) et réglages pilotés [adresse, de, à] (normalisés). */
export interface MacroData {
  v: number;
  rel?: boolean; // v0.46 : relatif (les réglages partent de leur valeur) ; sinon réel (tous à la position de la macro)
  links: [number, number, number, number?, number?][]; // [adresse, de, à, ancrage du réglage, ancrage de la macro]
}

/** Courbes du pattern édité d'une piste : temps en pas depuis le début du pattern. Points en
    tableau plat [temps, valeur 0..1, temps, valeur…]. */
export interface PatternLanesData {
  track: number;
  pattern: number;
  len: number;                              // longueur du pattern (pas)
  lanes: [number, number, number[]][];      // [réglage, lue (1/0), points]
}

/** v0.32 : état de l'enregistrement audio (fonctions audioRec*). */
export interface AudioRecStatus {
  active: boolean;
  encoding: boolean;      // conversion en MP3 en cours (après l'arrêt)
  progress: number;       // 0..1 de la conversion
  seconds: number;        // durée de la prise
  dropped: number;        // secondes perdues (disque trop lent), 0 normalement
  mode: number;           // 0 master, 1 pistes séparées, 2 les deux
  format: "wav" | "mp3";
  folder: string;         // « Groovebox/Enregistrements/2026-10-30 18h52m03 » (jamais le chemin complet)
  files: string[];
  error: string;
  canReveal: boolean;     // ouvrir le dossier (ordinateur)
  follow?: boolean;       // v0.46.2 : prise calée sur la lecture
  waiting?: boolean;      // v0.46.2 : armée, attend la lecture (ou le début du prochain tour)
  stopping?: boolean;     // v0.46.2 : s'arrêtera pile à la fin du tour de boucle
  startError?: string;
  processing?: boolean;   // 1.1 : traitement de la prise par la chaîne (après l'arrêt)
  treatedFolder?: string; // 1.1 : dossier des fichiers traités (« Son traité/… »)
  treated?: string[];     // 1.1 : fichiers traités
  hasResult?: boolean;    // 1.1 : un fichier peut aller dans un pad
  dest?: number;          // 1.1 : 0 prise, 1 Son traité, 2 pad, 3 remplacer l'original
}

/** 1.1 (lot 3) : réglages d'une prise de l'écran AUDIO. */
export interface AudioTakeOptions {
  master: boolean; masterTreated: boolean;
  tracks: number; sum: boolean; returns: number; input?: boolean;
  chain: { type: string; values: number[]; bypass: boolean }[];
  during: boolean; dest: number; format: number; follow: number; bars: number;
  project?: string;
}


/** v0.46 fix8 : montage audio. Clip : [voie, prise, départ dans le fichier, longueur (échantillons
 *  du fichier), début (noires), volume dB, fondu d'entrée (s), fondu de sortie (s)]. */
export type MontageClip = [number, string, number, number, number, number, number, number];
/** Prise : [identifiant, voie, fréquence, longueur (échantillons), fichier présent (1/0)]. */
export type MontageTake = [string, number, number, number, number];
export interface MontageData {
  on: boolean; bpm: number; loop: boolean; loopA: number; loopB: number; playhead: number; pos: number;
  status: string; takes: MontageTake[]; clips: MontageClip[];
}
