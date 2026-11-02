// Groovebox — OFAD — AGPL-3.0-or-later
// Nom lisible d'un réglage à partir de son adresse globale : « P2 · Basse acide · Coupure ».

import { getState, resolveGlobal } from "./store";
import { channelName, lastChannel } from "./params";

/** v0.46 : courbe de coupure / solo d'une piste (adresses juste après les réglages) : [piste, solo], sinon null. */
export function switchLaneOf(global: number): [number, boolean] | null {
  const L = getState().catalog?.layout;
  if (!L) return null;
  const base = (lastChannel(L) + 1) * L.slotsPerChannel;
  const k = global - base;
  if (k < 0 || k >= 2 * L.numTracks) return null;
  return [k % L.numTracks, k >= L.numTracks];
}

/** v0.46 : courbe du crossfader des scènes (après celles de coupure / solo). */
export function isFaderLane(global: number): boolean {
  const L = getState().catalog?.layout;
  return !!L && global === (lastChannel(L) + 1) * L.slotsPerChannel + 2 * L.numTracks;
}

/** v0.46 fix7 : courbe de coupure / solo d'un retour, du principal ou d'un groupe (après le crossfader) : [canal, solo]. */
export function busSwitchLaneOf(global: number): [number, boolean] | null {
  const L = getState().catalog?.layout;
  if (!L) return null;
  const base = (lastChannel(L) + 1) * L.slotsPerChannel + 2 * L.numTracks + 1;
  const n = lastChannel(L) + 1 - L.numTracks;
  const k = global - base;
  if (k < 0 || k >= 2 * n) return null;
  return [L.numTracks + (k % n), k >= n];
}

/** Coupure / solo : piste ou bus (pour l'affichage des courbes en marches). */
export function anySwitchLaneOf(global: number): [number, boolean] | null {
  return switchLaneOf(global) ?? busSwitchLaneOf(global);
}

export function describeGlobal(global: number): string {
  if (isFaderLane(global)) return "Crossfader des scènes (A → B)";
  const bus = busSwitchLaneOf(global);
  if (bus) {
    const st = getState();
    const where = st.catalog ? channelName(st.catalog, bus[0], st.tracks.tracks.map((t) => t.name)) : `Canal ${bus[0] + 1}`;
    return `${where} · ${bus[1] ? "Solo" : "Coupure"}`;
  }
  const sw = switchLaneOf(global);
  const s = getState();
  if (sw) {
    const name = s.tracks.tracks[sw[0]]?.name;
    return `P${sw[0] + 1}${name ? ` · ${name}` : ""} · ${sw[1] ? "Solo" : "Coupure"}`;
  }
  const r = resolveGlobal(global);
  if (!r || !s.catalog) return `Réglage ${global}`;
  const where = channelName(s.catalog, r.channel, s.tracks.tracks.map((t) => t.name));
  // nom porté par plusieurs réglages (le même « Accord » sur chaque pad d'une batterie) : la section le précise
  const m = r.module;
  const twin = m && r.area === "inst" && m.params.some((p, i) => i !== r.index && p.name === r.desc.name);
  const section = twin ? m.sections.find((sec) => sec.params.includes(r.index))?.name : undefined;
  const name = section && section !== r.desc.name ? `${section} · ${r.desc.name}` : r.desc.name;
  const what = m && r.area === "fx" ? `${m.name} · ${name}` : name;
  return `${where} · ${what}`;
}
