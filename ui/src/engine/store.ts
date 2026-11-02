// Groovebox — OFAD — AGPL-3.0-or-later
// État de l'interface : copie de l'état du moteur (fonctions natives et événement « tick ») et
// état propre à l'interface (écran, piste choisie, page…). Petit magasin sans dépendance.

import { useSyncExternalStore } from "react";
import { getNativeFunction } from "../juce/index.js";
import type {
  AudioRecStatus,
  Catalog, ChannelData, LaneData, LaunchData, SongData, PatternLanesData, MidiData, PatternData, PresetScope, SceneData, StepExtra, Tick, TracksData, UserPresetList, WaveData, MacroData, MontageData, MontageClip,
} from "./protocol";
import { PATTERN_NAMES } from "./protocol";
import { lastChannel, resolve, setBusNameSource, toNorm, type Resolved } from "./params";
import { getLockClip, setLockClip } from "./selection";

export type ScreenMode = "tracks" | "launch" | "sequence" | "sound" | "effects" | "mixer" | "scenes" | "settings";
export type PadMode = "play" | "mute" | "scenes" | "erase" | "steps";

export interface UiState {
  screen: ScreenMode;
  page: number;                 // page de 16 pas
  lane: number;                 // pad / ligne choisie (batteries, zones) ; ligne d'accord (mélodique)
  step: number | null;          // pas choisi (édition du pas)
  stepEdit: boolean;            // les encodeurs écrivent des verrous dans le pas choisi
  editSlot: "A" | "B" | null;   // les encodeurs écrivent dans la scène A ou B
  editBreak: boolean;           // les encodeurs écrivent dans la scène Break
  soundTab: string;
  soundSection: number;         // page dans l'onglet
  fxSlot: number;               // insert choisi (0..3)
  fxTab: string;
  fxSection: number;
  modSlot: number;              // source de modulation affichée (0..5)
  mixerBank: number;            // 0 = les 16 pistes (en défilement), 2 = retours et bus principal, 3 = groupes (1 : ancien 9-16), 4 = spectre du mix
  mixerFirst: number;           // première des 8 pistes visibles dans le défilement (0 à 8) : encodeurs, M / S
  padMode: PadMode;
  octave: number;               // pads mélodiques : décalage d'octave
  scale: number;                // pads mélodiques : gamme (engine/scales.ts)
  root: number;                 // pads mélodiques : tonique (0 = Do)
  padLevels: boolean;           // « 16 niveaux » : le son choisi sur les 16 pads, 16 vélocités
  rollBase: number;             // séquence mélodique : note la plus grave affichée
  shift: boolean;
  browserFamily: string;
  browserIndex: number;
  sceneBank: number;
  message: string | null;
  selection: number[];          // séquenceur : pas sélectionnés (ligne × 1024 + pas)
  selectMode: boolean;          // tactile : toucher sélectionne au lieu de poser
  paint: boolean;               // séquenceur : pinceau (glisser pose ou efface les pas survolés)
  copyDrag: boolean;            // tactile : glisser la sélection la copie (comme Ctrl)
  modAssign: { channel: number; source: number } | null; // affectation : toucher un encodeur le module par cette source
  macroAssign: number | null;   // affectation de macro : toucher un encodeur l'ajoute à cette macro (ou l'en retire)
  face: boolean;                // façade de l'instrument ouverte (vue « vrai synthé »)
  pedals: boolean;              // pédalier ouvert (les inserts du canal en pédales)
  fullscreen: boolean;          // application en plein écran
  launchView: "grid" | "song" | "audio";  // écran Lancement : grille des patterns, morceau ou montage audio
  seqView: "notes" | "auto";    // écran Séquence : notes ou courbes du pattern
  launchAll: boolean;           // grille de lancement : les 16 pistes en défilement (sinon 8 par écran)
  muteTarget: "pads" | "tracks"; // v0.31 : rôle Couper sur une batterie — ses pads (sons) ou les 16 pistes
  faceBig: boolean;            // façades jamais réduites sous leur taille normale (sinon défilement vertical)
  trackClip: number | null;    // v0.41 : piste copiée (Copier / Coller la piste, principe de l'Octatrack)
}

/** Octave affichée par le rouleau de notes, accordée avec l'octave des pads (bouton Page / octave) :
    les pads (premier pad = Do2 à l'octave 0) tombent dans les deux octaves affichées. */
export const ROLL_MIN = 0, ROLL_MAX = 96;
export function rollToOctave(base: number) { return Math.max(-3, Math.min(3, Math.round((base - 36) / 12))); }
export function setRollBase(base: number) {
  const b = Math.max(ROLL_MIN, Math.min(ROLL_MAX, Math.round(base / 12) * 12));
  setUi({ rollBase: b, octave: rollToOctave(b) });
}
export function setPadOctave(octave: number) {
  const o = Math.max(-3, Math.min(3, octave));
  setUi({ octave: o, rollBase: Math.max(ROLL_MIN, Math.min(ROLL_MAX, 36 + o * 12)) });
}

/** Dossiers de travail (Réglages) : chemin affiché seulement ; l'interface n'envoie que des chemins relatifs. */
export type FolderKind = "samples" | "projects" | "presets";
export interface FolderInfo { path: string; custom: boolean }
export interface FoldersData { canChoose: boolean; samples: FolderInfo; projects: FolderInfo; presets: FolderInfo }
/** Contenu d'un sous-dossier : [nom, dossier (1) ou fichier (0), octets]. */
export interface FolderList { ok: boolean; items: [string, number, number][] }

export interface AppState {
  ready: boolean;
  folders: FoldersData | null;           // dossiers de travail (null : pas de moteur)
  song: SongData | null;                 // morceau (arrangeur)
  montage: MontageData | null;           // v0.46 fix8 : montage audio
  patLanes: PatternLanesData | null;     // courbes du pattern édité (écran Séquence, vue Courbes)
  lastParam: number;                     // dernier réglage bougé à l'écran (ajout d'une courbe)
  catalog: Catalog | null;
  tracks: TracksData;
  focus: number;                         // canal choisi (0..15 pistes, 16..19 retours, 20 bus principal)
  channels: Record<number, ChannelData>;
  pattern: PatternData | null;
  extras: StepExtra[];
  scenes: SceneData[];
  tick: Tick;
  eff: Record<number, number>;           // valeurs jouées du canal choisi (emplacement local → normalisé)
  litLanes: number[];                    // allumage des pads de la piste choisie
  litTracks: number[];                   // allumage par piste
  litNotes: number[];                    // notes exactes de la piste choisie (0..127) : tenues ou jouées à l'instant
  waves: Record<string, WaveData | null>; // formes d'onde (piste:index)
  launch: LaunchData | null;             // lancement des patterns
  midi: MidiData;                        // apprentissage MIDI
  macros: MacroData[];                   // macros du logiciel hôte
  ui: UiState;
}

const emptyTick: Tick = {
  t: { playing: false, rec: false, ppq: -1, bpm: 120, fill: false, brk: false },
  g: { xfade: 0, sceneA: 1, sceneB: 9, bpm: 120, swing: 54 },
  hits: [], peaks: new Array(28).fill(0), pos: new Array(16).fill(-1), tpos: new Array(16).fill(-1), eff: [], fxm: [[0, 0, 0], [0, 0, 0], [0, 0, 0], [0, 0, 0]],
  pat: Array.from({ length: 16 }, () => [0, -1] as [number, number]), learn: -1, midi: false, loud: [-100, -100, -100, -100, -100, -100],
  project: false, seq: false, structure: false,
};

let state: AppState = {
  ready: false,
  folders: null,
  song: null,
  montage: null,
  patLanes: null,
  lastParam: -1,
  catalog: null,
  tracks: { tracks: [], inserts: [] },
  focus: 0,
  channels: {},
  pattern: null,
  extras: [],
  scenes: [],
  tick: emptyTick,
  eff: {},
  litLanes: new Array(16).fill(0),
  litTracks: new Array(16).fill(0),
  litNotes: new Array(128).fill(0),
  waves: {},
  launch: null,
  midi: { learn: -1, maps: [] },
  macros: [],
  ui: {
    screen: "sequence", page: 0, lane: 0, step: null, stepEdit: false, editSlot: null, editBreak: false,
    soundTab: "", soundSection: 0, fxSlot: 0, fxTab: "", fxSection: 0, modSlot: 0, mixerBank: 0, mixerFirst: 0,
    padMode: "play", octave: 0, scale: 0, root: 0, padLevels: false, rollBase: 36, shift: false, browserFamily: "Batteries", browserIndex: 0, sceneBank: 0, message: null, trackClip: null,
    selection: [], selectMode: false, paint: false, copyDrag: false, modAssign: null, macroAssign: null, face: false, pedals: false, fullscreen: false, launchView: "grid", seqView: "notes", launchAll: readPref("launchAll", true), muteTarget: readPref("muteTracks", false) ? "tracks" : "pads", faceBig: readPref("faceBig", true),
  },
};

const listeners = new Set<() => void>();
let fullscreenListening = false;
function set(patch: Partial<AppState>) {
  state = { ...state, ...patch };
  listeners.forEach((l) => l());
}
const PAD_MODE_IDS: PadMode[] = ["play", "mute", "scenes", "erase", "steps"];
/** Le contrôleur MIDI suit le rôle des pads, la banque de scènes, la ligne et la page affichées. */
function syncPadMode() {
  if (hasNative("setPadMode")) void call("setPadMode", PAD_MODE_IDS.indexOf(state.ui.padMode), state.ui.sceneBank, state.ui.lane, state.ui.page);
}
/** Préférences d'affichage gardées sur cet ordinateur (stockage du navigateur intégré). */
export function readPref(key: string, def: boolean): boolean {
  try { const v = window.localStorage.getItem(`groovebox.${key}`); return v === null ? def : v === "1"; } catch { return def; }
}
export function writePref(key: string, v: boolean) {
  try { window.localStorage.setItem(`groovebox.${key}`, v ? "1" : "0"); } catch { /* stockage indisponible : réglage gardé pour la séance */ }
}
export function setUi(patch: Partial<UiState>) {
  const before = state.ui;
  set({ ui: { ...state.ui, ...patch } });
  // les pads du contrôleur MIDI suivent le rôle des pads de l'interface
  if (state.ui.padMode !== before.padMode || state.ui.sceneBank !== before.sceneBank || state.ui.lane !== before.lane || state.ui.page !== before.page)
    syncPadMode();
  // v0.42 : Pieuvre 8 — la ligne choisie est la piste interne jouée au clavier et en MIDI
  if (state.ui.lane !== before.lane) syncPlayLane();
}
/** v0.42 : piste interne de Pieuvre 8 jouée au clavier = ligne choisie (pads, séquence, façade). */
function syncPlayLane() {
  const t = state.focus;
  if (t >= (state.catalog?.layout.numTracks ?? 16) || state.tracks.tracks[t]?.instrument !== "octa") return;
  void call("setPlayLane", t, Math.max(0, Math.min(7, state.ui.lane)));
}
export const getState = () => state;
export function useStore<T>(select: (s: AppState) => T): T {
  return useSyncExternalStore((l) => { listeners.add(l); return () => listeners.delete(l); }, () => select(state));
}

// ---------------------------------------------------------------------------
// Fonctions natives

const nativeCache = new Map<string, (...args: unknown[]) => Promise<unknown>>();
function call(name: string, ...args: unknown[]): Promise<unknown> {
  let f = nativeCache.get(name);
  if (!f) { f = getNativeFunction(name) as (...a: unknown[]) => Promise<unknown>; nativeCache.set(name, f); }
  return f(...args);
}
async function callJson<T>(name: string, ...args: unknown[]): Promise<T> {
  const r = await call(name, ...args);
  return (typeof r === "string" ? JSON.parse(r) : r) as T;
}

// ---------------------------------------------------------------------------
// Décodage

function hexToArray(hex: string, signed = false, width = 2): number[] {
  const out: number[] = new Array(Math.floor(hex.length / width));
  for (let i = 0; i < out.length; ++i) {
    const v = parseInt(hex.substr(i * width, width), 16);
    out[i] = signed && v > 127 ? v - 256 : v;
  }
  return out;
}

interface RawLane { dir: number; length?: number; mute: boolean; solo: boolean; vel: string; note: string; len?: string; len3?: string; micro: string }
interface RawPattern extends Omit<PatternData, "lanes" | "flags" | "accent"> { lanes: RawLane[]; flags?: string; accent?: number }
const decodePattern = (p: RawPattern | null): PatternData | null => p && ({
  ...p,
  accent: p.accent ?? 50,
  // une fonction de pas par chiffre hexadécimal
  flags: p.flags ? Array.from(p.flags, (c) => parseInt(c, 16) || 0) : [],
  lanes: p.lanes.map((l): LaneData => ({ dir: l.dir, length: l.length ?? 0, mute: l.mute, solo: l.solo, vel: hexToArray(l.vel), note: hexToArray(l.note), len: l.len3 !== undefined ? hexToArray(l.len3, false, 3) : hexToArray(l.len ?? ""), micro: hexToArray(l.micro, true) })),
});

// ---------------------------------------------------------------------------
// Accès aux réglages

export const layout = () => state.catalog!.layout;
export const instrumentTypes = () => state.tracks.tracks.map((t) => t.instrument);
export function resolveGlobal(global: number): Resolved | null {
  return state.catalog ? resolve(state.catalog, instrumentTypes(), state.tracks.inserts, global,
    (ch, k) => state.channels[ch]?.padfx?.find((f) => f.pad * 2 + f.slot === k)?.type) : null;
}
/** v0.43.4 : verrou d'une ligne de Pieuvre 8 recopié sur une autre ligne : un réglage « tN_… » ou un
    effet de la piste interne N devient celui de la piste d'arrivée ; tout le reste (sortie, mixeur…)
    est gardé tel quel. Autres instruments : inchangé. -1 si le réglage n'existe pas à l'arrivée. */
export function remapLaneLock(global: number, track: number, from: number, to: number): number {
  const L = state.catalog?.layout;
  const type = state.tracks.tracks[track]?.instrument;
  if (!L || type !== "octa" || from === to) return global;
  const m = state.catalog?.instruments.find((i) => i.type === type);
  const ch = Math.floor(global / L.slotsPerChannel), local = global % L.slotsPerChannel;
  if (ch !== track || !m) return global;
  if (local >= L.instrumentBase && local < L.insertBase) {
    const id = m.params[local - L.instrumentBase]?.id ?? "";
    const pre = `t${from + 1}_`;
    if (!id.startsWith(pre)) return global;
    const j = m.params.findIndex((q) => q.id === `t${to + 1}_${id.slice(pre.length)}`);
    return j < 0 ? -1 : ch * L.slotsPerChannel + L.instrumentBase + j;
  }
  const pb = L.padBypassBase ?? -1, pi = L.padInsertBase ?? -1;
  if (pb >= 0 && local >= pb && local < pb + 32) {
    const k = local - pb;
    return Math.floor(k / 2) === from ? ch * L.slotsPerChannel + pb + to * 2 + (k % 2) : global;
  }
  if (pi >= 0 && local >= pi) {
    const k = Math.floor((local - pi) / L.insertSlots), r = (local - pi) % L.insertSlots;
    return Math.floor(k / 2) === from ? ch * L.slotsPerChannel + pi + (to * 2 + (k % 2)) * L.insertSlots + r : global;
  }
  return global;
}

/** Emplacements par canal (adresse globale = canal × slots() + emplacement). */
export const slots = () => state.catalog?.layout.slotsPerChannel ?? 2048;
export const globalOf = (channel: number, local: number) => channel * slots() + local;

/** Valeur de base (normalisée) d'un réglage, d'après la copie locale du canal. */
export function baseValue(global: number): number {
  const L = state.catalog?.layout;
  if (!L) return 0;
  const ch = Math.floor(global / L.slotsPerChannel), local = global % L.slotsPerChannel;
  const c = state.channels[ch];
  if (!c) return 0;
  if (local < L.instrumentBase) return c.mix[local] ?? 0;
  if (local < L.insertBase) return c.inst?.[local - L.instrumentBase] ?? 0;
  if (local < L.modSourceBase) { const s = Math.floor((local - L.insertBase) / L.insertSlots); return c.fx[s]?.values[(local - L.insertBase) % L.insertSlots] ?? 0; }
  if (local < L.modAmountBase) { const s = Math.floor((local - L.modSourceBase) / L.modSourceSlots); return c.mod[s]?.[(local - L.modSourceBase) % L.modSourceSlots] ?? 0; }
  const pb = L.padBypassBase ?? -1, pi = L.padInsertBase ?? -1;
  if (pb >= 0 && local >= pb && local < pb + 32) return c.padfx?.find((f) => f.pad * 2 + f.slot === local - pb)?.bypass ?? 0;
  if (pi >= 0 && local >= pi) { const k = Math.floor((local - pi) / L.insertSlots); return c.padfx?.find((f) => f.pad * 2 + f.slot === k)?.values[(local - pi) % L.insertSlots] ?? 0; }
  const m = c.mods.find((x) => x.amountSlot === local);
  return m ? (m.amount + 1) / 2 : 0.5;
}

function writeLocal(global: number, norm: number) {
  const L = state.catalog!.layout;
  const ch = Math.floor(global / L.slotsPerChannel), local = global % L.slotsPerChannel;
  const c = state.channels[ch];
  if (!c) return;
  const next: ChannelData = { ...c };
  if (local < L.instrumentBase) { next.mix = [...c.mix]; next.mix[local] = norm; }
  else if (local < L.insertBase && c.inst) { next.inst = [...c.inst]; next.inst[local - L.instrumentBase] = norm; }
  else if (local < L.modSourceBase) {
    const s = Math.floor((local - L.insertBase) / L.insertSlots);
    next.fx = c.fx.map((f, i) => (i === s ? { ...f, values: f.values.map((v, k) => (k === (local - L.insertBase) % L.insertSlots ? norm : v)) } : f));
  } else if (local < L.modAmountBase) {
    const s = Math.floor((local - L.modSourceBase) / L.modSourceSlots);
    next.mod = c.mod.map((m, i) => (i === s ? m.map((v, k) => (k === (local - L.modSourceBase) % L.modSourceSlots ? norm : v)) : m));
  } else if (L.padBypassBase !== undefined && local >= L.padBypassBase && local < L.padBypassBase + 32) {
    next.padfx = c.padfx?.map((f) => (f.pad * 2 + f.slot === local - L.padBypassBase! ? { ...f, bypass: norm } : f));
  } else if (L.padInsertBase !== undefined && local >= L.padInsertBase) {
    const k = Math.floor((local - L.padInsertBase) / L.insertSlots), i = (local - L.padInsertBase) % L.insertSlots;
    next.padfx = c.padfx?.map((f) => (f.pad * 2 + f.slot === k ? { ...f, values: f.values.map((v, j) => (j === i ? norm : v)) } : f));
  } else next.mods = c.mods.map((m) => (m.amountSlot === local ? { ...m, amount: norm * 2 - 1 } : m));
  set({ channels: { ...state.channels, [ch]: next } });
}

// Envois groupés : une seule écriture par réglage et par image (les glissements rapides restent fluides)
const pendingParams = new Map<number, number>();
let flushScheduled = false;
function flushParams() {
  flushScheduled = false;
  pendingParams.forEach((v, g) => { void call("setParam", g, v); });
  pendingParams.clear();
}

let messageTimer = 0;
export function showMessage(text: string) {
  window.clearTimeout(messageTimer);
  setUi({ message: text });
  messageTimer = window.setTimeout(() => setUi({ message: null }), 2200);
}

// ---------------------------------------------------------------------------
// Rafraîchissements

setBusNameSource(() => state.tracks.buses); // v0.31 : noms des groupes
async function refreshTracks() { set({ tracks: await callJson<TracksData>("getTracks") }); }
async function refreshChannel(ch: number) { const c = await callJson<ChannelData>("getChannel", ch); set({ channels: { ...state.channels, [ch]: c } }); }
async function refreshAllChannels() {
  const L = state.catalog!.layout;
  const all: Record<number, ChannelData> = {};
  for (let ch = 0; ch <= lastChannel(L); ++ch) {
    const c = await callJson<ChannelData | null>("getChannel", ch);
    if (c) all[ch] = c;
  }
  set({ channels: all });
}
const focusTrack = () => Math.min(state.focus, (state.catalog?.layout.numTracks ?? 16) - 1);
async function refreshPattern() {
  const t = focusTrack();
  const [p, e] = await Promise.all([callJson<RawPattern>("getPattern", t), callJson<StepExtra[]>("getExtras", t)]);
  set({ pattern: decodePattern(p), extras: e });
}
async function refreshScenes() { set({ scenes: await callJson<SceneData[]>("getScenes") }); }
async function refreshLaunch() { set({ launch: await callJson<LaunchData>("getLaunch") }); }
async function refreshMidi() { set({ midi: await callJson<MidiData>("getMidi") }); }
async function refreshMacros() { set({ macros: (await callJson<MacroData[]>("getMacros")) ?? [] }); }
async function refreshEverything() { await refreshTracks(); await refreshAllChannels(); await refreshPattern(); await refreshScenes(); await refreshLaunch(); await refreshMidi(); await refreshMacros(); }

/** Vrai si le moteur propose cette fonction native (le faux moteur du navigateur en propose moins). */
export function hasNative(name: string): boolean {
  const list = (window.__JUCE__?.initialisationData as { __juce__functions?: string[] } | undefined)?.__juce__functions;
  return !!list && list.includes(name);
}

function flash(kind: "lane" | "track", index: number) {
  const key = kind === "lane" ? "litLanes" : "litTracks";
  const lit = [...state[key]];
  lit[index] = (lit[index] ?? 0) + 1;
  set({ [key]: lit } as Partial<AppState>);
  window.setTimeout(() => {
    const next = [...state[key]];
    next[index] = Math.max(0, (next[index] ?? 1) - 1);
    set({ [key]: next } as Partial<AppState>);
  }, 110);
}

// notes allumées : tenues (envoi kn) ou frappées depuis peu (kh, 110 ms)
let heldNotesNow: number[] = [];
const noteFlash = new Array(128).fill(0);
function updateLitNotes() {
  const lit = noteFlash.slice();
  for (const n of heldNotesNow) if (n >= 0 && n < 128) lit[n] += 1;
  const prev = state.litNotes;
  if (lit.some((v, i) => (v > 0) !== ((prev[i] ?? 0) > 0))) set({ litNotes: lit });
}
function noteTick(kn: number[] | undefined, kh: number[] | undefined) {
  heldNotesNow = kn ?? [];
  for (const n of kh ?? []) {
    if (n < 0 || n > 127) continue;
    noteFlash[n] += 1;
    window.setTimeout(() => { noteFlash[n] = Math.max(0, noteFlash[n] - 1); updateLitNotes(); }, 110);
  }
  updateLitNotes();
}

function onTick(payload: unknown) {
  const tick = (typeof payload === "string" ? JSON.parse(payload) : payload) as Tick;
  const eff: Record<number, number> = {};
  tick.eff.forEach(([local, v]) => { eff[local] = v; });
  set({ tick, eff });
  // v0.32.1 : boutons tournés par le contrôleur MIDI ou une macro : l'écran suit (sauf geste en cours)
  tick.pv?.forEach(([g, v]) => { if (!pendingParams.has(g) && Number.isFinite(v)) writeLocal(g, v); });
  const ft = focusTrack();
  tick.hits.forEach(([t, l]) => { flash("track", t); if (t === ft) flash("lane", l); });
  noteTick(tick.kn, tick.kh);
  if (tick.project || tick.structure) void refreshEverything();
  else if (tick.seq) {
    void refreshPattern();
    void refreshTracks(); // coupure / solo changés par le contrôleur MIDI
    if (state.ui.screen === "launch" || state.ui.screen === "sequence") void refreshLaunch();
  }
  if (tick.midi || tick.learn !== state.midi.learn) void refreshMidi();
  // morceau changé (enregistrement en cours, chargement, historique) : relu, au plus 5 fois par seconde
  if (tick.song && state.song && tick.song[2] !== state.song.version && state.ui.screen === "launch") scheduleSongRefresh();
  // courbes du pattern : relues quand le morceau change (enregistrement en cours, annulation…)
  if (tick.song && state.ui.screen === "sequence" && state.ui.seqView === "auto" && tick.song[2] !== lastLaneVersion) { lastLaneVersion = tick.song[2]; schedulePatLanesRefresh(); }
}

let songRefreshTimer = 0;
let lastLaneVersion = -1;
let patLanesTimer = 0;
function schedulePatLanesRefresh() {
  if (patLanesTimer) return;
  patLanesTimer = window.setTimeout(() => { patLanesTimer = 0; void actions.refreshPatLanes(); }, 200);
}
function scheduleSongRefresh() {
  if (songRefreshTimer) return;
  songRefreshTimer = window.setTimeout(() => { songRefreshTimer = 0; void actions.refreshSong(); }, 200);
}

/** Retour en arrière, photo : le moteur renvoie [fait, peut annuler, peut rétablir, photo prise]. */
async function history(name: string, done: string, nothing: string) {
  const r = await callJson<boolean[]>(name);
  if (!Array.isArray(r)) return;
  set({ tick: { ...state.tick, hist: [r[1] ? 1 : 0, r[2] ? 1 : 0, r[3] ? 1 : 0] } });
  if (!r[0]) { if (nothing) showMessage(nothing); return; }
  showMessage(done);
  // v0.43 : les échantillons ont pu revenir en arrière (éditeur) : formes d'onde redemandées
  if (name !== "takePhoto") { set({ waves: {} }); await refreshEverything(); }
}

function onFileDone(payload: unknown) {
  const r = (typeof payload === "string" ? JSON.parse(payload) : payload) as { kind: string; error: string };
  if (r.error) { showMessage(r.error); return; }
  if (r.kind === "open") { showMessage("Projet ouvert"); void refreshEverything(); }
  else if (r.kind === "save") showMessage("Projet enregistré");
  else if (r.kind === "folder") { showMessage("Dossier choisi"); void actions.refreshFolders(); }
  else if (r.kind === "sample") {
    showMessage("Échantillon chargé");
    void (async () => {
      const c = await callJson<ChannelData>("getChannel", state.focus);
      set({ channels: { ...state.channels, [state.focus]: c }, waves: {} });
    })();
  }
}

export async function connect() {
  // Catalogue : lu comme une ressource du moteur (rapide) ; sinon par la fonction native
  let catalog: Catalog | null = null;
  try {
    const r = await fetch("/data/catalog.json", { cache: "no-store" });
    if (r.ok) catalog = (await r.json()) as Catalog;
  } catch { /* passerelle sans accès par adresse : fonction native */ }
  if (!catalog || !catalog.layout) catalog = await callJson<Catalog>("getCatalog");
  set({ catalog });
  await refreshEverything();
  const first = state.tracks.tracks[0];
  const inst = catalog.instruments.find((i) => i.type === first?.instrument);
  set({ ready: true, ui: { ...state.ui, browserFamily: inst?.family ?? "Batteries" } });
  window.__JUCE__.backend.addEventListener("tick", onTick);
  window.__JUCE__.backend.addEventListener("fileDone", onFileDone);
  void actions.refreshFolders();
  void call("setFocus", 0);
  // les pads du contrôleur reprennent le rôle affiché (l'interface a pu être rouverte)
  syncPadMode();
}

// ---------------------------------------------------------------------------
// Actions

export const actions = {
  // v0.32 : enregistrement audio (master, pistes séparées ou les deux ; WAV ou MP3)
  audioRecStart(mode: number, format: number) { return callJson<AudioRecStatus>("audioRecStart", mode, format); },
  audioRecStop() { return callJson<AudioRecStatus>("audioRecStop"); },
  audioRecStatus() { return callJson<AudioRecStatus>("audioRecStatus"); },
  async audioRecReveal() { return !!(await call("audioRecReveal")); },
  /** v0.45.1 : le moteur calcule le spectre du bus principal tant qu'un écran l'affiche. */
  setSpectrum(on: boolean) { void call("setSpectrum", on ? 1 : 0); },

  // Réglages ----------------------------------------------------------------
  setParam(global: number, norm: number) {
    const v = Math.min(1, Math.max(0, norm));
    if (state.lastParam !== global) set({ lastParam: global });
    writeLocal(global, v);
    pendingParams.set(global, v);
    if (!flushScheduled) { flushScheduled = true; requestAnimationFrame(flushParams); }
  },
  resetParam(global: number) {
    const r = resolveGlobal(global);
    if (r) actions.setParam(global, toNorm(r.desc, r.desc.def));
  },

  // Canal choisi ------------------------------------------------------------
  async focus(channel: number) {
    set({ focus: channel, eff: {} });
    setUi({ step: null, stepEdit: false, page: 0, lane: 0, selection: [], modAssign: null });
    syncPlayLane();
    const c = await callJson<ChannelData>("setFocus", channel);
    set({ channels: { ...state.channels, [channel]: c } });
    if (channel < layout().numTracks) {
      await refreshPattern();
      // séquence mélodique : on montre l'octave où sont les notes
      let low = 128;
      state.pattern?.lanes.forEach((ln) => ln.vel.forEach((v, s) => { if (v > 0) low = Math.min(low, ln.note[s]); }));
      if (low < 128) setRollBase(Math.floor(low / 12) * 12);
    }
  },

  // Structure -----------------------------------------------------------------
  async setInstrument(track: number, type: string) {
    const c = await callJson<ChannelData>("setInstrument", track, type);
    set({ channels: { ...state.channels, [track]: c } });
    await refreshTracks();
    if (track === focusTrack()) await refreshPattern();
    setUi({ soundTab: "", soundSection: 0, lane: 0 });
  },
  async setInsert(channel: number, slot: number, type: string) {
    const c = await callJson<ChannelData>("setInsert", channel, slot, type);
    set({ channels: { ...state.channels, [channel]: c } });
    await refreshTracks();
    setUi({ fxTab: "", fxSection: 0 });
  },
  async setPadInsert(track: number, pad: number, slot: number, type: string) {
    const c = await callJson<ChannelData>("setPadInsert", track, pad, slot, type);
    set({ channels: { ...state.channels, [track]: c } });
  },
  async moveInsert(channel: number, from: number, to: number) {
    const c = await callJson<ChannelData>("moveInsert", channel, from, to);
    set({ channels: { ...state.channels, [channel]: c } });
    await refreshTracks();
  },
  async applyPreset(channel: number, slot: number, name: string) {
    const c = await callJson<ChannelData>("applyPreset", channel, slot, name);
    set({ channels: { ...state.channels, [channel]: c } });
    await refreshTracks();
    showMessage(`Préréglage « ${name} »`);
  },
  // Préréglages de l'utilisateur (banques) ------------------------------------------------
  async listPresets(scope: PresetScope, channel: number, slot: number, pad: number): Promise<UserPresetList | null> {
    if (!hasNative("listPresets")) return null;
    return callJson<UserPresetList | null>("listPresets", scope, channel, slot, pad);
  },
  async savePreset(scope: PresetScope, channel: number, slot: number, pad: number, bank: string, name: string, overwrite: boolean): Promise<string> {
    const r = await callJson<string>("savePreset", scope, channel, slot, pad, bank, name, overwrite);
    if (r === "ok") { await refreshChannel(channel); await refreshTracks(); showMessage(`« ${name} » enregistré dans la banque « ${bank} »`); }
    else if (r === "invalid") showMessage("Nom de banque ou de préréglage invalide");
    else if (r === "error") showMessage("Impossible d'écrire le préréglage");
    return r;
  },
  async loadPreset(scope: PresetScope, channel: number, slot: number, pad: number, bank: string, name: string) {
    const c = await callJson<ChannelData | null>("loadPreset", scope, channel, slot, pad, bank, name);
    if (!c) { showMessage("Préréglage illisible"); return; }
    set({ channels: { ...state.channels, [channel]: c } });
    await refreshTracks();
    showMessage(`Préréglage « ${name} »`);
  },
  async deletePreset(scope: PresetScope, channel: number, slot: number, pad: number, bank: string, name: string) {
    const ok = await call("deletePreset", scope, channel, slot, pad, bank, name);
    showMessage(ok ? `« ${name} » mis à la corbeille` : "Suppression impossible");
  },
  revealPresets() { if (hasNative("revealPresets")) void call("revealPresets"); },
  async renameTrack(track: number, name: string) { set({ tracks: await callJson<TracksData>("setTrackName", track, name) }); },
  /** v0.33 : phrase de la voix interne d'un vocodeur (vide = phrase de démonstration). */
  async setSpeech(track: number, text: string) { set({ tracks: await callJson<TracksData>("setTrackSpeech", track, text.slice(0, 164)) }); },
  /** v0.39 : entrée audio coupée par l'application autonome : -1 lire, 0 ouvrir, 1 couper. Réponse : 1 coupée, 0 ouverte, -1 sans objet. */
  async inputMute(op: number): Promise<number> { if (!hasNative("inputMute")) return -1; const r = Number(await call("inputMute", op)); return Number.isFinite(r) ? r : -1; },
  async setTrackColor(track: number, color: number) { set({ tracks: await callJson<TracksData>("setTrackColor", track, color) }); },
  /** v0.24 : sortie de la piste (0 = bus principal, 1 à 7 = groupe, v0.46). */
  async trackOutput(track: number, out: number) { set({ tracks: await callJson<TracksData>("setTrackOutput", track, out) }); },
  // v0.31 : coupure / solo des retours et des groupes, nom d'un groupe
  async busMute(channel: number, on: boolean) { set({ tracks: await callJson<TracksData>("busMute", channel, on) }); },
  async busSolo(channel: number, on: boolean) { set({ tracks: await callJson<TracksData>("busSolo", channel, on) }); },
  async renameGroup(group: number, name: string) { set({ tracks: await callJson<TracksData>("setGroupName", group, name) }); },
  /** Déplacer une piste (v0.23) : tout la suit (son, effets, séquences, scènes, morceau). */
  async moveTrack(from: number, to: number) {
    if (from === to) return;
    const r = await callJson<TracksData | string>("moveTrack", from, to);
    if (typeof r === "string") { showMessage(r); return; }
    set({ tracks: r });
    await refreshEverything();
    await actions.focus(to);
    showMessage(`Piste ${from + 1} déplacée en ${to + 1} (Ctrl+Z pour revenir)`);
  },
  /** v0.41 : copie de piste à piste — la piste « to » devient la copie de « from » (son, effets,
      séquences, verrous, échantillons). Une étape d'historique. */
  async copyTrack(from: number, to: number) {
    if (from === to) return;
    const r = await callJson<TracksData | string>("copyTrack", from, to);
    if (typeof r === "string") { showMessage(r); return; }
    const waves = { ...state.waves };
    for (const k of Object.keys(waves)) if (k.startsWith(`${to}:`)) delete waves[k];
    set({ tracks: r, waves });
    await refreshEverything();
    await actions.focus(to);
    showMessage(`Piste ${from + 1} copiée sur la piste ${to + 1} (Ctrl+Z pour revenir)`);
  },
  /** v0.43 : Pieuvre 8 — écouter une partie (a..b) d'un emplacement ; index -1 : arrêter. */
  previewSample(track: number, index: number, a: number, b: number) { void call("previewSample", track, index, a, b); },
  /** v0.43 : Pieuvre 8 — une piste interne (ou les 8 : -1) remise à ses réglages d'origine. */
  async resetLanes(track: number, lane: number) {
    const r = await callJson<TracksData | string>("resetLanes", track, lane);
    if (typeof r === "string") { showMessage(r); return; }
    set({ tracks: r });
    await refreshEverything();
    // v0.44.1 : les verrous de pas des T remises (pattern édité) partent aussi
    const n = await actions.clearLocks(lane < 0 ? [0, 1, 2, 3, 4, 5, 6, 7] : [lane]);
    showMessage(`${lane < 0 ? "Les 8 pistes internes remises" : `T${lane + 1} remise`} à l'origine${n ? `, ${n} verrou${n > 1 ? "s" : ""} de pas retiré${n > 1 ? "s" : ""}` : ""} (Ctrl+Z pour revenir)`);
  },
  /** v0.42 : Pieuvre 8 — une piste interne copiée sur une autre (réglages, effets, lignes, verrous). */
  async copyLane(track: number, from: number, to: number) {
    if (from === to) return;
    const r = await callJson<TracksData | string>("copyLane", track, from, to);
    if (typeof r === "string") { showMessage(r); return; }
    set({ tracks: r });
    await refreshEverything();
    showMessage(`T${from + 1} copiée sur T${to + 1}, avec ses propres sons (indépendants) — Ctrl+Z pour revenir`);
  },
  /** v0.41 : dupliquer — copie sur la première piste libre après elle (sinon avant). */
  async duplicateTrack(from: number) {
    const tr = state.tracks.tracks;
    let free = tr.findIndex((t, i) => i > from && !t.instrument);
    if (free < 0) free = tr.findIndex((t) => !t.instrument);
    if (free < 0) { showMessage("Les 16 pistes sont prises : libérez-en une pour dupliquer"); return; }
    await actions.copyTrack(from, free);
  },
  /** Insérer une piste libre à cette place (v0.23) : les pistes suivantes glissent d'un cran. */
  async insertTrack(at: number) {
    const r = await callJson<TracksData | string>("insertTrack", at);
    if (typeof r === "string") { showMessage(r); return; }
    set({ tracks: r });
    await refreshEverything();
    await actions.focus(at);
    showMessage(`Piste libre insérée en ${at + 1}`);
  },

  // Modulations ---------------------------------------------------------------
  async addMod(channel: number, source: number, local: number, amount = 0.3) {
    const c = await callJson<ChannelData>("addMod", channel, source, local, amount);
    set({ channels: { ...state.channels, [channel]: c } });
  },
  async removeMod(channel: number, local: number) {
    const c = await callJson<ChannelData>("removeMod", channel, local);
    set({ channels: { ...state.channels, [channel]: c } });
  },

  // Notes ---------------------------------------------------------------------------
  noteOn(track: number, note: number, velocity: number) { void call("noteOn", track, note, velocity); },
  noteOff(track: number, note: number) { void call("noteOff", track, note); },
  /** Expression jouée à l'écran : 0 = pitch bend (-1..1), 1 = molette, 2 = pression, 3 = pédale. */
  expression(track: number, kind: 0 | 1 | 2 | 3, value: number) { void call("expression", track, kind, value); },

  // Séquenceur ---------------------------------------------------------------------
  async seq(name: string, ...args: unknown[]) {
    const p = await callJson<RawPattern>(name, focusTrack(), ...args);
    set({ pattern: decodePattern(p) });
  },
  toggleStep(lane: number, step: number, note?: number) {
    const p = state.pattern;
    if (!p) return;
    const vel = p.lanes[lane]?.vel[step] ?? 0;
    if (vel > 0) {
      void actions.seq("seqSetStep", lane, step, 0);
      if (state.ui.step === step) setUi({ step: null, stepEdit: false });
    } else {
      if (note !== undefined) void actions.seq("seqSetNote", lane, step, note);
      void actions.seq("seqSetStep", lane, step, state.ui.shift ? 127 : 100);
      setUi({ step, lane });
    }
  },
  async trackMute(track: number, mute: boolean) {
    const p = await callJson<RawPattern>("seqTrackMute", track, mute);
    if (track === focusTrack()) set({ pattern: decodePattern(p) });
    await refreshTracks();
  },
  async trackSolo(track: number, solo: boolean) {
    const p = await callJson<RawPattern>("seqTrackSolo", track, solo);
    if (track === focusTrack()) set({ pattern: decodePattern(p) });
    await refreshTracks();
  },
  async extras(name: string, ...args: unknown[]) {
    set({ extras: await callJson<StepExtra[]>(name, focusTrack(), ...args) });
  },
  /** v0.43.4 : copier les verrous d'un pas (piste au premier plan). */
  copyStepLocks(lane: number, step: number) {
    const ex = state.extras.find((e) => e.lane === lane && e.step === step);
    if (!ex || ex.locks.length === 0) { showMessage("Ce pas n'a pas de verrou à copier"); return; }
    setLockClip({ track: focusTrack(), lane, step, locks: ex.locks.map(([g, v]) => [g, v] as [number, number]) });
    showMessage(`${ex.locks.length} verrou${ex.locks.length > 1 ? "s" : ""} du pas ${step + 1} copié${ex.locks.length > 1 ? "s" : ""} : choisissez un autre pas puis « Coller les verrous »`);
  },
  /** v0.43.4 : coller les verrous copiés sur un pas (posé s'il ne l'est pas). Pieuvre 8 : d'une T à
      une autre, les réglages et effets de la T d'origine deviennent ceux de la T d'arrivée. Les verrous
      déjà présents sur le pas restent, ceux des mêmes réglages sont remplacés. */
  async pasteStepLocks(lane: number, step: number) {
    const clip = getLockClip();
    const t = focusTrack();
    if (!clip) { showMessage("Copiez d'abord les verrous d'un pas"); return; }
    if (clip.track !== t) { showMessage("Les verrous se collent dans la même piste (mêmes réglages)"); return; }
    const p = state.pattern;
    if (p && (p.lanes[lane]?.vel[step] ?? 0) <= 0) {
      const src = p.lanes[clip.lane];
      if (src && (src.note[clip.step] ?? 0) > 0) await actions.seq("seqSetNote", lane, step, src.note[clip.step]);
      await actions.seq("seqSetStep", lane, step, src?.vel[clip.step] || 100);
    }
    let n = 0;
    for (const [g, v] of clip.locks) {
      const g2 = remapLaneLock(g, t, clip.lane, lane);
      if (g2 < 0) continue;
      await callJson<StepExtra[]>("stepLock", t, lane, step, g2, v).then((x) => set({ extras: x }));
      ++n;
    }
    showMessage(`${n} verrou${n > 1 ? "s" : ""} collé${n > 1 ? "s" : ""} sur le pas ${step + 1}`);
  },
  /** v0.44.1 : verrous retirés (pattern édité de la piste) — ceux d'un pas, ou de tous les pas des
      lignes données. Rend le nombre de verrous retirés. */
  async clearLocks(lanes: number[], step: number | null = null): Promise<number> {
    const t = focusTrack();
    const todo = state.extras.filter((e) => lanes.includes(e.lane) && (step === null || e.step === step) && e.locks.length > 0)
      .map((e) => ({ lane: e.lane, step: e.step, gs: e.locks.map(([g]) => g) }));
    let n = 0;
    for (const e of todo)
      for (const g of e.gs) { set({ extras: await callJson<StepExtra[]>("stepClearLock", t, e.lane, e.step, g) }); ++n; }
    return n;
  },

  // Scènes -----------------------------------------------------------------------------
  async scene(name: string, ...args: unknown[]) { set({ scenes: await callJson<SceneData[]>(name, ...args) }); },
  launchScene(scene: number) { void call("sceneLaunch", scene, state.ui.shift); },

  // Transport et réglages globaux ---------------------------------------------------
  play() { void call("play"); },
  stop() { void call("stop"); },
  togglePlay() { if (state.tick.t.playing) actions.stop(); else actions.play(); },
  toggleRecord() { void call("record", !state.tick.t.rec); },
  fill(on: boolean) { void call("fill", on); },
  breakKey(on: boolean) { void call("breakKey", on); },
  /** Break temporisé : beats temps (0 : l'arrêter). */
  breakTimed(beats: number) { void call("breakTimed", beats); },
  setGlobal(id: "bpm" | "swing" | "xfade" | "sceneA" | "sceneB", value: number) {
    set({ tick: { ...state.tick, g: { ...state.tick.g, [id]: value } } });
    void call("setGlobal", id, value);
  },
  async newProject(empty: boolean) {
    set({ tracks: await callJson<TracksData>("newProject", empty) });
    await refreshEverything();
  },

  // Échantillons ---------------------------------------------------------------------------
  async loadSample(track: number, index: number, file: File) {
    if (file.size > 64 * 1024 * 1024) { showMessage("Fichier trop lourd (64 Mo au plus)"); return; }
    showMessage(`Chargement de ${file.name}…`);
    const bytes = new Uint8Array(await file.arrayBuffer());
    let binary = "";
    for (let i = 0; i < bytes.length; i += 0x8000) binary += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
    const r = await callJson<ChannelData | string>("loadSample", track, index, file.name, btoa(binary));
    if (typeof r === "string") { showMessage(r); return; }
    set({ channels: { ...state.channels, [track]: r }, waves: { ...state.waves, [`${track}:${index}`]: undefined as unknown as null } });
    showMessage(`${file.name} chargé`);
  },
  /** v0.40 : les données d'une autre piste (réserve commune de Pieuvre 8 : noms et attributs de ses emplacements). */
  async refreshChannel(ch: number) { await refreshChannel(ch); },
  /** v0.40 : éditeur audio d'un emplacement : 0 inverser, 1 normaliser, 2 fondu d'entrée, 3 fondu de sortie, 4 rogner, 5 silence. */
  async editSample(track: number, index: number, op: number, a: number, b: number) {
    const r = await callJson<ChannelData | string>("editSample", track, index, op, a, b);
    if (typeof r === "string") { showMessage(r); return false; }
    set({ channels: { ...state.channels, [track]: r }, waves: { ...state.waves, [`${track}:${index}`]: undefined as unknown as null } });
    showMessage(["Inversé", "Normalisé", "Fondu d'entrée appliqué", "Fondu de sortie appliqué", "Rogné", "Silence posé"][op] + " (Ctrl+Z pour revenir)");
    return true;
  },
  async copySample(fromTrack: number, fromIndex: number, toTrack: number, toIndex: number) {
    const r = await callJson<ChannelData | string>("copySample", fromTrack, fromIndex, toTrack, toIndex);
    if (typeof r === "string") { showMessage(r); return; }
    set({ channels: { ...state.channels, [toTrack]: r }, waves: { ...state.waves, [`${toTrack}:${toIndex}`]: undefined as unknown as null } });
    showMessage(`Emplacement collé (Ctrl+Z pour revenir)`);
  },
  /** v0.41 : une partie (a..b) d'un emplacement copiée dans un autre (n'importe quelle piste Pieuvre). */
  async copySampleRegion(fromTrack: number, fromIndex: number, a: number, b: number, toTrack: number, toIndex: number) {
    const r = await callJson<ChannelData | string>("copySampleRegion", fromTrack, fromIndex, a, b, toTrack, toIndex);
    if (typeof r === "string") { showMessage(r); return false; }
    set({ channels: { ...state.channels, [toTrack]: r }, waves: { ...state.waves, [`${toTrack}:${toIndex}`]: undefined as unknown as null } });
    showMessage(`Sélection collée dans T${Math.floor(toIndex / 16) + 1} · ${(toIndex % 16) + 1} (Ctrl+Z pour revenir)`);
    return true;
  },
  async clearSample(track: number, index: number) {
    const c = await callJson<ChannelData>("clearSample", track, index);
    set({ channels: { ...state.channels, [track]: c }, waves: { ...state.waves, [`${track}:${index}`]: null } });
  },
  async wave(track: number, index: number) {
    const w = await callJson<WaveData | null>("getWave", track, index, 400);
    set({ waves: { ...state.waves, [`${track}:${index}`]: w } });
  },
  async autoSlice(track: number, transients: boolean, count: number) {
    const c = await callJson<ChannelData>("autoSlice", track, transients, count);
    set({ channels: { ...state.channels, [track]: c } });
    showMessage(transients ? "Découpé sur les attaques" : `Découpé en ${count} parts égales`);
  },
  refreshMacros,
  /** Ajoute le réglage à la macro, ou l'en retire s'il y est déjà. Vrai : il y est maintenant. */
  async toggleMacro(macro: number, global: number) {
    const m = await callJson<MacroData[]>("setMacro", macro, global);
    set({ macros: m });
    return !!m?.[macro]?.links.some((l) => l[0] === global);
  },
  async setMacroRange(macro: number, global: number, lo: number, hi: number) { set({ macros: await callJson<MacroData[]>("setMacroRange", macro, global, lo, hi) }); },
  async setMacroMode(macro: number, relative: boolean) { set({ macros: await callJson<MacroData[]>("setMacroMode", macro, relative ? 1 : 0) }); },
  async clearMacro(macro: number) { set({ macros: await callJson<MacroData[]>("clearMacro", macro) }); },
  setMacroValue(macro: number, v: number) { void call("setGlobal", `macro${macro + 1}`, Math.max(0, Math.min(1, v))); },
  /** Plein écran : l'application autonome prend tout l'écran (ou revient en fenêtre) ; dans un navigateur, l'API plein écran. */
  async toggleFullscreen() {
    if (hasNative("fullscreen")) {
      const r = String(await call("fullscreen"));
      if (r === "host") { showMessage("Dans un logiciel hôte, agrandissez la fenêtre du plugin depuis le logiciel"); return; }
      setUi({ fullscreen: r === "on" });
      return;
    }
    try {
      if (!fullscreenListening) {
        // sortie par Échap dans un navigateur : l'état suit
        fullscreenListening = true;
        document.addEventListener("fullscreenchange", () => setUi({ fullscreen: !!document.fullscreenElement }));
      }
      if (document.fullscreenElement) await document.exitFullscreen();
      else await document.documentElement.requestFullscreen();
    } catch { showMessage("Plein écran indisponible ici"); }
  },
  /** Annuler / rétablir (Ctrl+Z / Ctrl+Y) : réglages, séquences, instruments, effets, échantillons. */
  async undo() { await history("undo", "Annulé", "Rien à annuler"); },
  async redo() { await history("redo", "Rétabli", "Rien à rétablir"); },
  /** Photo de tous les réglages, rappelée d'un bouton (le rappel s'annule avec Ctrl+Z). */
  async takePhoto() { await history("takePhoto", "Photo prise : « Rappel » y revient à tout moment", ""); },
  async recallPhoto() { await history("recallPhoto", "Photo rappelée (Ctrl+Z pour revenir avant)", "Aucune photo prise"); },
  /** Enregistreur de la piste (sampler de performance) : 0 = départ / arrêt (ou cycle de la boucle), 1 = arrêter, 2 = effacer. */
  recordAction(track: number, action: 0 | 1 | 2) { void call("recordAction", track, action); },
  async keepRecording(track: number, lane: number, index: number) {
    const r = await callJson<ChannelData | string>("keepRecording", track, lane, index);
    if (typeof r === "string") { showMessage(r); return; }
    set({ channels: { ...state.channels, [track]: r }, waves: { ...state.waves, [`${track}:${index}`]: undefined as unknown as null } });
    showMessage(`Enregistrement gardé dans l'emplacement ${index + 1}`);
  },

  // Patterns et lancement ---------------------------------------------------------------
  async refreshLaunch() { await refreshLaunch(); },
  async launchPattern(track: number, pattern: number) { set({ launch: await callJson<LaunchData>("launchPattern", track, pattern) }); },
  async launchRow(row: number) { set({ launch: await callJson<LaunchData>("launchRow", row) }); },
  async setLaunchQuant(q: number) { set({ launch: await callJson<LaunchData>("setLaunchQuant", q) }); },
  async setRowScene(row: number, scene: number) { set({ launch: await callJson<LaunchData>("setRowScene", row, scene) }); },
  // Morceau (arrangeur) ------------------------------------------------------------------
  async refreshSong() { if (hasNative("getSong")) set({ song: await callJson<SongData>("getSong") }); },
  // v0.46 fix8 : montage audio
  async refreshMontage() { if (hasNative("getMontage")) set({ montage: await callJson<MontageData>("getMontage") }); },
  async montageSetClips(clips: MontageClip[]) { set({ montage: await callJson<MontageData>("montageSetClips", JSON.stringify(clips)) }); },
  async montageSetFlags(on: boolean, loop: boolean, a: number, b: number) { set({ montage: await callJson<MontageData>("montageSetFlags", on, loop, a, b) }); },
  async montagePlayhead(beat: number) { set({ montage: await callJson<MontageData>("montagePlayhead", beat) }); },
  /** Prise du montage : démarre (et lance la lecture) ou s'arrête ; renvoie un message d'erreur ou "". */
  async montageRecord(start: boolean): Promise<string> {
    const r = await callJson<MontageData | string>("montageRecord", start);
    if (typeof r === "string") { showMessage(r); return r; }
    set({ montage: r });
    return "";
  },
  async montageExport(format: number): Promise<string> {
    const r = await callJson<MontageData | string>("montageExport", format);
    if (typeof r === "string") { showMessage(r); return r; }
    set({ montage: r });
    return "";
  },
  async montagePeaks(id: string): Promise<string> { return hasNative("montagePeaks") ? String(await call("montagePeaks", id)) : ""; },
  async setSongFlags(mode: boolean, loop: boolean, read: boolean) { set({ song: await callJson<SongData>("setSongFlags", mode, loop, read) }); },
  /** Rangées bouclées (comprises) ; dernière -1 : jusqu'à la fin du morceau. */
  async setSongLoopRange(from: number, to: number) { set({ song: await callJson<SongData>("setSongLoopRange", from, to) }); },
  /** Nombre de passages de la boucle (0 = sans fin). */
  async setSongLoopCount(n: number) { set({ song: await callJson<SongData>("setSongLoopCount", n) }); },
  async setSongRecord(play: boolean, moves: boolean) { set({ song: await callJson<SongData>("setSongRecord", play, moves) }); },
  async setSongRows(rows: SongData["rows"]) { set({ song: await callJson<SongData>("setSongRows", JSON.stringify(rows.slice(0, 128))) }); },
  async songRowFromPlaying(after: number) { set({ song: await callJson<SongData>("songRowFromPlaying", after) }); },
  /** Courbe : 0 lue / coupée, 1 effacer ; 2 : effacer toutes les courbes. */
  // Courbes du pattern édité de la piste choisie, et courbes du morceau (éditeur)
  async refreshPatLanes() {
    if (!hasNative("getPatternLanes")) return;
    set({ patLanes: await callJson<PatternLanesData | null>("getPatternLanes", focusTrack()) });
  },
  async setPatternLane(global: number, points: number[], on = true) {
    set({ patLanes: await callJson<PatternLanesData | null>("setPatternLane", focusTrack(), global, JSON.stringify(points.map((x) => Math.round(x * 10000) / 10000)), on) });
  },
  async getSongLane(global: number) { return (await callJson<number[]>("getSongLane", global)) ?? []; },
  async setSongLane(global: number, points: number[]) {
    set({ song: await callJson<SongData>("setSongLane", global, JSON.stringify(points.map((x) => Math.round(x * 10000) / 10000))) });
  },
  async songLane(global: number, action: 0 | 1 | 2) { set({ song: await callJson<SongData>("songLane", global, action) }); },
  async setMuteTails(on: boolean) { set({ launch: await callJson<LaunchData>("setMuteTails", on) }); },
  /** Rangée entière (une lettre sur les 16 pistes) : copier vers une autre lettre, effacer. */
  async copyRow(from: number, to: number) {
    set({ launch: await callJson<LaunchData>("copyRow", from, to) });
    await refreshPattern();
    showMessage(`Rangée ${PATTERN_NAMES[from]} copiée vers ${PATTERN_NAMES[to]}`);
  },
  async clearRow(row: number) {
    set({ launch: await callJson<LaunchData>("clearRow", row) });
    await refreshPattern();
    set({ extras: await callJson<StepExtra[]>("getExtras", focusTrack()) });
    showMessage(`Rangée ${PATTERN_NAMES[row]} effacée`);
  },
  async editPattern(track: number, pattern: number) {
    const p = await callJson<RawPattern>("seqEditPattern", track, pattern);
    if (track === focusTrack()) { set({ pattern: decodePattern(p) }); set({ extras: await callJson<StepExtra[]>("getExtras", track) }); setUi({ step: null, stepEdit: false, page: 0, selection: [] }); }
    await refreshLaunch();
  },
  /** Copie vers une autre piste (même motif, verrous gardés quand le réglage existe aussi). */
  async copyPatternTo(fromTrack: number, from: number, track: number, to: number) {
    const p = await callJson<RawPattern>("seqCopyPatternFrom", track, from, to, fromTrack);
    if (track === focusTrack()) { set({ pattern: decodePattern(p) }); set({ extras: await callJson<StepExtra[]>("getExtras", track) }); }
    await refreshLaunch();
  },
  async copyPattern(track: number, from: number, to: number) {
    const p = await callJson<RawPattern>("seqCopyPattern", track, from, to);
    if (track === focusTrack()) set({ pattern: decodePattern(p) });
    await refreshLaunch();
  },
  async clearPattern(track: number, pattern: number) {
    const p = await callJson<RawPattern>("seqClearPattern", track, pattern);
    if (track === focusTrack()) { set({ pattern: decodePattern(p) }); set({ extras: await callJson<StepExtra[]>("getExtras", track) }); setUi({ selection: [] }); }
    await refreshLaunch();
  },
  // Sélection du séquenceur : édition groupée en un appel (notes, conditions, verrous)
  async editCells(json: string) {
    const t = focusTrack();
    const p = await callJson<RawPattern>("seqEditCells", t, json);
    set({ pattern: decodePattern(p) });
    set({ extras: await callJson<StepExtra[]>("getExtras", t) });
  },
  async stepsLock(cells: [number, number][], global: number, value: number, clear: boolean) {
    set({ extras: await callJson<StepExtra[]>("stepsLock", focusTrack(), JSON.stringify(cells), global, value, clear) });
  },

  // Apprentissage MIDI -------------------------------------------------------------------
  async midiLearn(target: number) { set({ midi: await callJson<MidiData>("midiLearn", target) }); },
  async midiResolve(replace: boolean) { set({ midi: await callJson<MidiData>("midiResolve", replace) }); },
  async midiForget(target: number) { set({ midi: await callJson<MidiData>("midiForget", target) }); },
  /** v0.46 : canaux MIDI (0 = tous) et commandes apprises actives ; canal propre à une piste (0 = piste choisie). */
  async setMidiRoute(note: number, ctrl: number, ctrlOn: boolean) { set({ midi: await callJson<MidiData>("setMidiRoute", note, ctrl, ctrlOn) }); },
  async setTrackMidiChannel(track: number, chan: number) { set({ midi: await callJson<MidiData>("setTrackMidiChannel", track, chan) }); },

  // Effets : chaînes, analyseur, mesures --------------------------------------------------
  async applyChain(channel: number, name: string) {
    const c = await callJson<ChannelData>("applyChain", channel, name);
    set({ channels: { ...state.channels, [channel]: c } });
    await refreshTracks();
    setUi({ fxSlot: 0, fxTab: "", fxSection: 0 });
    showMessage(`Chaîne « ${name} »`);
  },
  async spectrum(channel: number, slot: number, bands = 96): Promise<number[]> {
    if (!hasNative("getSpectrum")) return [];
    return await callJson<number[]>("getSpectrum", channel, slot, bands);
  },
  resetLoudness() { void call("resetLoudness"); },

  // Fichiers (sélecteur du système) -------------------------------------------------------
  chooseSample(track: number, index: number) { void call("chooseSample", track, index); },
  // Dossiers de travail -------------------------------------------------------------------
  async refreshFolders() { if (hasNative("getFolders")) set({ folders: await callJson<FoldersData>("getFolders") }); },
  chooseFolder(kind: FolderKind) { if (hasNative("chooseFolder")) void call("chooseFolder", kind); },
  async resetFolder(kind: FolderKind) { set({ folders: await callJson<FoldersData>("resetFolder", kind) }); showMessage("Dossier par défaut"); },
  revealFolder(kind: FolderKind) { if (hasNative("revealFolder")) void call("revealFolder", kind); },
  async listFolder(kind: "samples" | "projects", relative: string): Promise<FolderList> {
    if (!hasNative("listFolder")) return { ok: false, items: [] };
    return (await callJson<FolderList | null>("listFolder", kind, relative)) ?? { ok: false, items: [] };
  },
  loadSampleFile(track: number, index: number, relative: string) { void call("loadSampleFile", track, index, relative); },
  openProjectFile(relative: string) { void call("openProjectFile", relative); },
  /** asTemplate (v0.46) : modèle — réglages, sons et effets gardés ; patterns, verrous et morceau retirés. */
  async saveProjectNamed(name: string, overwrite: boolean, asTemplate = false): Promise<string> {
    const r = await callJson<string>("saveProjectNamed", name, overwrite, asTemplate);
    if (r === "ok") showMessage(asTemplate ? `Modèle « ${name} » enregistré : réglages et sons, sans patterns ni morceau` : `Projet « ${name} » enregistré`);
    else if (r === "invalid") showMessage("Nom de projet invalide");
    else if (r === "error") showMessage("Impossible d'écrire le projet");
    return r;
  },
  saveProject(asTemplate = false) { void call("saveProject", asTemplate); },
  openProject() { void call("openProject"); },
  /** v0.46 fix6 : démarrage — l'application autonome montre l'accueil (choix du projet) une fois par
   *  lancement, et l'écran de lancement natif disparaît quand l'interface est prête. */
  async appStart(): Promise<{ standalone: boolean; welcome: boolean }> {
    if (!hasNative("appStart")) return { standalone: false, welcome: /[?&]accueil\b/.test(window.location.search) };
    return (await callJson<{ standalone: boolean; welcome: boolean } | null>("appStart")) ?? { standalone: false, welcome: false };
  },
};
