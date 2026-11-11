// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : télécommande — envoi MIDI du téléphone. Deux usages :
//  - piloter GOLEM sur le PC : messages exclusifs propres à GOLEM (src/engine/RemoteProtocol.h),
//    « régler cible = valeur » en valeur absolue (la télécommande reste en phase avec le PC) ;
//  - piloter un appareil MIDI branché (FM-1…) : notes, CC, programme, fichiers .syx.
// Les réglages bougés vite (fader, pavé) sont regroupés : un seul message toutes les 12 ms.

import { actions } from "../engine/store";

/** Cibles de GOLEM (mêmes numéros que l'apprentissage MIDI, PluginProcessor.h). */
export const T = {
  xfade: -2, breakHold: -3, fill: -4, play: -5, start: -6, stop: -7, record: -8,
  rowPrev: -11, rowNext: -12, rowStop: -13,
  row: (r: number) => -20 - r,          // lancer la rangée A à P
  mute: (t: number) => -100 - t,
  solo: (t: number) => -200 - t,
  knob: (k: number) => -500 - k,        // bouton assignable 1 à 16 (k = 0..15)
  host: (h: number) => -600 - h,        // réglages globaux (ordre de hostCtlId)
};
/** Réglages globaux : index dans hostCtlId (PluginProcessor.h). */
export const HOST = { bpm: 0, swing: 1, vecOn: 4, vecMode: 5, vecX: 6, vecY: 7, vecOrbit: 8, vecRate: 9, vecDepth: 10, macro1: 11, vecSA: 19, vecRot: 27, vecHeight: 28 };
/** 1.1 : retour seulement (RemoteProtocol.h) : position jouée du pavé de GOLEM, avec le mouvement. */
export const VEC_SHOWN_X = -690, VEC_SHOWN_Y = -691;
export const NUM_SCENES = 32;

const BIAS = 1 << 20;
const pending = new Map<number, number>();
let timer = 0;
let linked = false;

export function setLinked(on: boolean) { linked = on; if (!on) pending.clear(); }
export function isLinked() { return linked; }

function encodeSet(entries: [number, number][]): number[] {
  const b = [0xf0, 0x7d, 0x47, 0x4c, 0x01, 0x01];
  for (const [target, v] of entries) {
    const raw = Math.max(0, Math.min((1 << 21) - 1, target + BIAS));
    const v14 = Math.round(Math.max(0, Math.min(1, v)) * 16383);
    b.push((raw >> 14) & 0x7f, (raw >> 7) & 0x7f, raw & 0x7f, (v14 >> 7) & 0x7f, v14 & 0x7f);
  }
  b.push(0xf7);
  return b;
}

function flush() {
  timer = 0;
  if (!linked || pending.size === 0) { pending.clear(); return; }
  const all = [...pending.entries()];
  pending.clear();
  const msgs: number[][] = [];
  for (let i = 0; i < all.length; i += 48) msgs.push(encodeSet(all.slice(i, i + 48)));
  actions.midiSend(msgs.length === 1 ? msgs[0] : msgs);
}

/** Régler une cible de GOLEM (valeur 0..1). `now` : envoi immédiat (boutons). */
export function setTarget(target: number, v: number, now = false) {
  touched.set(target, performance.now());
  pending.set(target, v);
  if (now) { if (timer) { window.clearTimeout(timer); } flush(); return; }
  if (!timer) timer = window.setTimeout(flush, 12);
}

/** Bouton momentané de GOLEM (lecture, rangée…) : appui puis relâché. */
export function pulse(target: number) { setTarget(target, 1, true); setTarget(target, 0, true); }

/** « Bonjour » au PC ; full : il renvoie tout son état. */
export function hello(full = false) { if (linked) actions.midiSend([0xf0, 0x7d, 0x47, 0x4c, 0x01, 0x02, full ? 1 : 0, 0xf7]); }

// --- retour du PC (1.1) ------------------------------------------------------------------------
/** Dernier geste sur une cible : le retour du PC pour cette cible est ignoré 400 ms (pas de tremblement). */
const touched = new Map<number, number>();
export function recentlyTouched(target: number) { const t = touched.get(target); return t !== undefined && performance.now() - t < 400; }

export type RemoteMsg = { kind: "state"; values: [number, number][] }
  | { kind: "name"; track: number; color: number; name: string }
  | { kind: "knob"; knob: number; color: number; label: string; sub: string }
  | { kind: "levels"; levels: number[] }
  | { kind: "learn"; type: "cc" | "note"; ch: number; num: number }
  | { kind: "preset"; track: number; ch: number; focus: boolean; noteCh: number; ctrlOn: boolean; ctrlCh: number; slots: { prog: number; total: number; name: string; type: string }[] };
const textOf = (d: number[], from: number, to: number) => {
  const bytes: number[] = [];
  for (let i = from; i + 1 < to; i += 2) bytes.push(((d[i] & 15) << 4) | (d[i + 1] & 15));
  try { return new TextDecoder().decode(new Uint8Array(bytes)).trim(); } catch { return ""; }
};
/** Message reçu du PC (octets sans F0 ni F7). */
export function parseRemote(d: number[]): RemoteMsg | null {
  if (d.length < 5 || d[0] !== 0x7d || d[1] !== 0x47 || d[2] !== 0x4c || d[3] !== 0x01) return null;
  if (d[4] === 0x03) {
    const values: [number, number][] = [];
    for (let i = 5; i + 5 <= d.length; i += 5) {
      const raw = ((d[i] & 0x7f) << 14) | ((d[i + 1] & 0x7f) << 7) | (d[i + 2] & 0x7f);
      values.push([raw - BIAS, (((d[i + 3] & 0x7f) << 7) | (d[i + 4] & 0x7f)) / 16383]);
    }
    return { kind: "state", values };
  }
  if (d[4] === 0x04 && d.length >= 7) return { kind: "name", track: d[5], color: d[6], name: textOf(d, 7, d.length) };
  if (d[4] === 0x06 && d.length >= 7) {
    const sep = d.indexOf(0x7f, 7);
    const end = sep < 0 ? d.length : sep;
    return { kind: "knob", knob: d[5], color: d[6], label: textOf(d, 7, end), sub: sep < 0 ? "" : textOf(d, sep + 1, d.length) };
  }
  if (d[4] === 0x07) return { kind: "levels", levels: d.slice(5).map((v) => v / 127) };
  // 1.1 : « Apprendre » : CC ou note reçu de l'appareil branché (message local, StandaloneInput.h)
  if (d[4] === 0x0a && d.length >= 8) return { kind: "learn", type: d[5] === 1 ? "note" : "cc", ch: (d[6] & 15) + 1, num: d[7] & 0x7f };
  // 1.1 : préréglages d'une piste (instrument puis 4 inserts) : n° sur 14 bits (16383 = aucun), nombre de sons, nom, type
  if (d[4] === 0x09 && d.length >= 11) {
    const slots: { prog: number; total: number; name: string; type: string }[] = [];
    let i = 11;
    const seg = () => { const sep = d.indexOf(0x7f, i); const end = sep < 0 ? d.length : sep; const t = textOf(d, i, end); i = end + 1; return t; };
    while (i + 4 <= d.length && slots.length < 5) {
      const p14 = (d[i] << 7) | d[i + 1], total = (d[i + 2] << 7) | d[i + 3];
      i += 4;
      const name = seg();
      slots.push({ prog: p14 === 16383 ? -1 : p14, total, name, type: seg() });
    }
    return { kind: "preset", track: d[5], ch: d[6], focus: d[7] === 1, noteCh: d[8], ctrlOn: d[9] === 1, ctrlCh: d[10], slots };
  }
  return null;
}

// --- appareil MIDI branché ------------------------------------------------------------------
const ccLast = new Map<number, number>();
let ccTimer = 0;
const ccPending = new Map<number, number>();
function flushCc() {
  ccTimer = 0;
  if (!linked || ccPending.size === 0) { ccPending.clear(); return; }
  const msgs: number[][] = [];
  for (const [key, v] of ccPending) {
    if (ccLast.get(key) === v) continue;
    ccLast.set(key, v);
    msgs.push([0xb0 | (key >> 7), key & 0x7f, v]);
  }
  ccPending.clear();
  if (msgs.length) actions.midiSend(msgs.length === 1 ? msgs[0] : msgs);
}
/** Dernière valeur envoyée d'un CC (0..1), pour le pavé Chaos. */
export function ccValue(channel: number, num: number): number { const v = ccLast.get(Math.max(0, Math.min(15, channel - 1)) * 128 + (num & 0x7f)); return v === undefined ? 0.5 : v / 127; }
/** CC (canal 1..16, numéro 0..127, valeur 0..1). */
export function cc(channel: number, num: number, v: number) {
  const ch = Math.max(0, Math.min(15, channel - 1));
  ccPending.set(ch * 128 + (num & 0x7f), Math.round(Math.max(0, Math.min(1, v)) * 127));
  if (!ccTimer) ccTimer = window.setTimeout(flushCc, 12);
}
/** CC envoyé tout de suite (pads : l'appui et le relâché ne doivent pas se fondre en un seul message). */
export function ccNow(channel: number, num: number, v: number) {
  if (!linked) return;
  const ch = Math.max(0, Math.min(15, channel - 1)), n = num & 0x7f, val = Math.round(Math.max(0, Math.min(1, v)) * 127);
  ccPending.delete(ch * 128 + n);
  ccLast.set(ch * 128 + n, val);
  actions.midiSend([0xb0 | ch, n, val]);
}
export function noteOn(channel: number, note: number, vel = 100) { if (linked) actions.midiSend([0x90 | ((channel - 1) & 15), note & 0x7f, vel & 0x7f]); }
export function noteOff(channel: number, note: number) { if (linked) actions.midiSend([0x80 | ((channel - 1) & 15), note & 0x7f, 0]); }
export function program(channel: number, n: number) { if (linked) actions.midiSend([0xc0 | ((channel - 1) & 15), n & 0x7f]); }
/** Hauteur (-1..1). */
export function bend(channel: number, v: number) {
  if (!linked) return;
  const x = Math.round((Math.max(-1, Math.min(1, v)) + 1) * 8191.5);
  const c = Math.min(16383, x);
  actions.midiSend([0xe0 | ((channel - 1) & 15), c & 0x7f, (c >> 7) & 0x7f]);
}
/** Fichier .syx : un ou plusieurs messages F0 … F7. Renvoie le nombre de messages envoyés. */
export function sendSyx(data: Uint8Array): number {
  if (!linked) return 0;
  const msgs: number[][] = [];
  let cur: number[] | null = null;
  for (const b of data) {
    if (b === 0xf0) cur = [0xf0];
    else if (cur) { cur.push(b); if (b === 0xf7) { msgs.push(cur); cur = null; } }
  }
  for (const m of msgs) actions.midiSend(m);
  return msgs.length;
}
/** Tout éteindre sur un canal (notes bloquées). */
export function allNotesOff(channel: number) { if (linked) actions.midiSend([[0xb0 | ((channel - 1) & 15), 123, 0], [0xb0 | ((channel - 1) & 15), 64, 0]]); }

// --- 1.1 : « Apprendre » : le prochain CC (ou note) reçu de l'appareil se pose sur le réglage choisi ---
export type LearnEvent = { type: "cc" | "note"; ch: number; num: number };
/** Renvoie vrai si l'événement est pris (sinon on attend le suivant : une note quand on attend un CC…). */
export type LearnFn = (e: LearnEvent) => boolean;
let learnFn: LearnFn | null = null;
const learnSubs = new Set<() => void>();
export function learnArm(fn: LearnFn | null) {
  learnFn = fn;
  actions.remoteLearn(fn !== null);
  learnSubs.forEach((f) => f());
}
export function learnArmed(): LearnFn | null { return learnFn; }
export function onLearnChange(f: () => void): () => void { learnSubs.add(f); return () => { learnSubs.delete(f); }; }
export function feedLearn(e: LearnEvent) {
  const f = learnFn;
  if (f && f(e)) learnArm(null);
}
