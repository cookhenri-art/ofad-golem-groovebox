// Groovebox — OFAD — AGPL-3.0-or-later
// Conversions et affichage des réglages, identiques au moteur (src/engine/Params.h).

import type { Catalog, ModuleDesc, ParamDesc } from "./protocol";

export function toPlain(p: ParamDesc, norm: number): number {
  const n = Math.min(1, Math.max(0, norm));
  if (p.choices && p.choices.length) return Math.min(p.choices.length - 1, Math.floor(n * p.choices.length));
  let v = p.min + (p.max - p.min) * (p.skew === 1 ? n : Math.pow(n, 1 / p.skew));
  if (p.integer) v = Math.round(v);
  return v;
}

export function toNorm(p: ParamDesc, plain: number): number {
  if (p.choices && p.choices.length) {
    const c = p.choices.length;
    const i = Math.min(c - 1, Math.max(0, Math.round(plain)));
    return c <= 1 ? 0 : (i + 0.5) / c;
  }
  if (p.max <= p.min) return 0;
  const x = Math.min(1, Math.max(0, (plain - p.min) / (p.max - p.min)));
  return p.skew === 1 ? x : Math.pow(x, p.skew);
}

export const isStepped = (p: ParamDesc) => !!(p.choices && p.choices.length) || !!p.integer;
export const stepCount = (p: ParamDesc) => (p.choices && p.choices.length ? p.choices.length : Math.round(p.max - p.min) + 1);
export const isBipolar = (p: ParamDesc) => p.min < 0 && p.max > 0;

const minus = "−";
const signed = (v: number, digits = 1) => `${v > 0 ? "+" : v < 0 ? minus : ""}${Math.abs(v).toFixed(digits)}`;

export function formatPlain(p: ParamDesc, v: number): string {
  if (p.choices && p.choices.length) return p.choices[Math.min(p.choices.length - 1, Math.max(0, Math.round(v)))] ?? "";
  // déclin du VCA d'un pad au maximum : pas de déclin (la voix seule décide)
  if (p.id.endsWith("_adec") && v >= p.max - 1) return "∞";
  switch (p.unit) {
    case "dB": return v <= p.min + 0.05 && p.min <= -47 ? "−∞" : `${signed(v, Math.abs(v) < 10 ? 1 : 0)} dB`;
    case "Hz": return v >= 1000 ? `${(v / 1000).toFixed(v >= 10000 ? 1 : 2)} kHz` : v < 10 ? `${v.toFixed(2)} Hz` : v < 100 ? `${v.toFixed(1)} Hz` : `${Math.round(v)} Hz`;
    case "ms": return v >= 1000 ? `${(v / 1000).toFixed(2)} s` : v < 10 ? `${v.toFixed(1)} ms` : `${Math.round(v)} ms`;
    case "µs": return `${Math.round(v)} µs`;
    case "s": return `${v.toFixed(v < 10 ? 2 : 1)} s`;
    case "%": return isBipolar(p) ? `${signed(v, 0)} %` : `${Math.round(v)} %`;
    case "dt": return p.integer ? `${signed(v, 0)} dt` : `${signed(v, 1)} dt`;
    case "ct": return `${signed(v, 0)} ct`;
    case "pan": { const r = Math.round(v); return r === 0 ? "Centre" : r < 0 ? `G ${-r}` : `D ${r}`; }
    case "oct": return `${signed(v, 0)} oct.`;
    case "note": return noteName(Math.round(v));
    case "°": return `${Math.round(v)}°`;
    case ":1": return `${v.toFixed(1)}:1`;
    case "pas": return `${Math.round(v)} pas`;
    case "": return p.integer ? `${Math.round(v)}` : v.toFixed(2);
    default: return `${p.integer ? Math.round(v) : v.toFixed(1)} ${p.unit}`;
  }
}

export const formatNorm = (p: ParamDesc, norm: number) => formatPlain(p, toPlain(p, norm));

const NOTES = ["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"];
// convention française : le do central (MIDI 60) est Do3
export const noteName = (n: number) => `${NOTES[((n % 12) + 12) % 12]}${Math.floor(n / 12) - 2}`;

// ---------------------------------------------------------------------------
// Adresses : canal × emplacements par canal (layout.slotsPerChannel) + emplacement (voir src/engine/Params.h)

export interface Resolved {
  desc: ParamDesc;
  module: ModuleDesc | null;   // instrument ou effet (null pour mixage / modulation)
  area: "mix" | "inst" | "fx" | "mod" | "amount" | "pfx" | "padbyp";
  slot: number;                // insert (0..3) ou source (0..5)
  index: number;               // index dans le module
  channel: number;
  local: number;
}

export function resolve(cat: Catalog, instruments: string[], inserts: string[][], global: number,
  padType?: (channel: number, k: number) => string | undefined): Resolved | null {
  const L = cat.layout;
  const channel = Math.floor(global / L.slotsPerChannel);
  const local = global % L.slotsPerChannel;
  if (channel < 0 || channel > lastChannel(L)) return null;
  const mixer = mixerFor(cat, channel);
  if (local < L.instrumentBase) {
    const d = mixer.params[local - L.mixerBase];
    return d ? { desc: d, module: null, area: "mix", slot: 0, index: local, channel, local } : null;
  }
  if (local < L.insertBase) {
    const m = channel < L.numTracks ? cat.instruments.find((x) => x.type === instruments[channel]) : undefined;
    const d = m?.params[local - L.instrumentBase];
    return d && m ? { desc: d, module: m, area: "inst", slot: 0, index: local - L.instrumentBase, channel, local } : null;
  }
  if (local < L.modSourceBase) {
    const slot = Math.floor((local - L.insertBase) / L.insertSlots);
    const m = cat.effects.find((x) => x.type === inserts[channel]?.[slot]);
    const d = m?.params[(local - L.insertBase) % L.insertSlots];
    return d && m ? { desc: d, module: m, area: "fx", slot, index: (local - L.insertBase) % L.insertSlots, channel, local } : null;
  }
  if (local < L.modSourceBase + L.numModSources * L.modSourceSlots) {
    const slot = Math.floor((local - L.modSourceBase) / L.modSourceSlots);
    const index = (local - L.modSourceBase) % L.modSourceSlots;
    const d = cat.modSource.params[index];
    return d ? { desc: d, module: null, area: "mod", slot, index, channel, local } : null;
  }
  // profondeur d'une modulation (-100 à +100 %), comme dans le moteur
  if (local >= L.modAmountBase && local < L.modAmountBase + 64) return { desc: AMOUNT_DESC, module: null, area: "amount", slot: 0, index: local - L.modAmountBase, channel, local };
  // inserts des pads (batteries) : interrupteurs, puis 64 réglages par insert (pad × 2 + n)
  const pb = L.padBypassBase ?? -1, pi = L.padInsertBase ?? -1, n = (L.padInserts ?? 2) * 16;
  if (pb >= 0 && local >= pb && local < pb + n) return padType?.(channel, local - pb) ? { desc: BYPASS_DESC, module: null, area: "padbyp", slot: local - pb, index: 0, channel, local } : null;
  if (pi >= 0 && local >= pi && local < pi + n * L.insertSlots) {
    const k = Math.floor((local - pi) / L.insertSlots), index = (local - pi) % L.insertSlots;
    const m = cat.effects.find((x) => x.type === padType?.(channel, k));
    const d = m?.params[index];
    return d && m ? { desc: d, module: m, area: "pfx", slot: k, index, channel, local } : null;
  }
  return null;
}
const BYPASS_DESC: ParamDesc = { id: "bypass", name: "Contourné", unit: "", min: 0, max: 1, def: 0, skew: 1, choices: ["Non", "Oui"] };
const AMOUNT_DESC: ParamDesc = { id: "amount", name: "Profondeur", unit: "%", min: -100, max: 100, def: 0, skew: 1 };

// --- Canaux : 16 pistes, 4 retours, bus principal, puis les bus de groupe (v0.24) -------------
type LayoutLike = Catalog["layout"];
export const groupBase = (L: LayoutLike) => L.groupBase ?? L.masterChannel + 1;
export const numGroups = (L: LayoutLike) => L.numGroups ?? 0;
/** Dernier canal (le bus principal sans groupes, le dernier groupe sinon). */
export const lastChannel = (L: LayoutLike) => (numGroups(L) > 0 ? groupBase(L) + numGroups(L) - 1 : L.masterChannel);
export const isGroup = (L: LayoutLike, ch: number) => numGroups(L) > 0 && ch >= groupBase(L) && ch < groupBase(L) + numGroups(L);
export const groupChannels = (L: LayoutLike) => Array.from({ length: numGroups(L) }, (_, i) => groupBase(L) + i);
/** Réglages de mixage d'un canal : un groupe a ceux d'une piste (envois vers les retours). */
export const mixerFor = (cat: Catalog, ch: number) =>
  ch === cat.layout.masterChannel ? cat.mixerMaster : isGroup(cat.layout, ch) || ch < cat.layout.numTracks ? cat.mixerTrack : cat.mixerReturn;
/** v0.31 : noms donnés aux groupes (lus dans l'état des pistes, branché par le magasin). */
let busNameSource: () => ({ name?: string } | undefined)[] | undefined = () => undefined;
export function setBusNameSource(f: () => ({ name?: string } | undefined)[] | undefined) { busNameSource = f; }
/** Nom court d'un canal hors pistes : « Principal », « Retour A », « Groupe 1 » (ou le nom donné au groupe). */
export const busName = (L: LayoutLike, ch: number) => {
  if (ch === L.masterChannel) return "Principal";
  if (isGroup(L, ch)) {
    const given = busNameSource()?.[ch - L.numTracks]?.name;
    return given ? given : `Groupe ${ch - groupBase(L) + 1}`;
  }
  return `Retour ${String.fromCharCode(65 + ch - L.numTracks)}`;
};
/** Repère d'une tranche : 1..16, A..D, M, G1..G7. */
export const busTag = (L: LayoutLike, ch: number) =>
  ch < L.numTracks ? String(ch + 1) : ch === L.masterChannel ? "M" : isGroup(L, ch) ? `G${ch - groupBase(L) + 1}` : String.fromCharCode(65 + ch - L.numTracks);

export const channelName = (cat: Catalog, channel: number, trackNames: string[]) =>
  channel < cat.layout.numTracks ? `P${channel + 1} · ${trackNames[channel] ?? ""}`
    : channel === cat.layout.masterChannel ? "Bus principal" : busName(cat.layout, channel);

export const MOD_SOURCE_COLORS = [
  "oklch(0.78 0.16 200)", "oklch(0.76 0.17 160)", "oklch(0.8 0.15 95)",
  "oklch(0.74 0.18 330)", "oklch(0.75 0.17 290)", "oklch(0.78 0.16 55)", "oklch(0.82 0.1 250)",
];
export const modSourceName = (i: number) => (i >= 6 ? "Vélocité" : `Modulation ${i + 1}`);
export const modSourceShort = (i: number) => (i >= 6 ? "VÉL" : `M${i + 1}`);

export const TRACK_COLORS = [
  "var(--k-kick)", "var(--k-tom)", "var(--k-hat)", "var(--k-snare)", "var(--k-clap)", "var(--k-cowbell)", "var(--k-rim)", "var(--k-sample)",
];
export const trackColor = (c: number) => TRACK_COLORS[((c % TRACK_COLORS.length) + TRACK_COLORS.length) % TRACK_COLORS.length];
