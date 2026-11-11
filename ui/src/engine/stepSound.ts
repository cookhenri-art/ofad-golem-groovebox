// Groovebox — OFAD — AGPL-3.0-or-later
// Après la 1.1 : « Son du pas » — numéro de préréglage verrouillé sur un pas (principe des emplacements de
// la Pieuvre 8). Piste MIDI : c'est le son (programme) de l'appareil ; autres pistes : le préréglage de la
// piste dans l'ordre des numéros (Init = 1, sons d'usine banque par banque, puis ceux de l'utilisateur).

import { useEffect, useState } from "react";
import { actions, useStore } from "./store";
import { toNorm, toPlain } from "./params";

export interface StepSound {
  /** adresse globale du réglage verrouillé (-1 : pas de son du pas pour cette piste) */
  addr: number;
  /** nombre de sons (numéros 1 à count) */
  count: number;
  norm: (n: number) => number;
  num: (norm: number) => number;
  /** nom long (liste) et court (pas) du son n */
  label: (n: number) => string;
  short: (n: number) => string;
}

const namesCache = new Map<string, string[]>();
const NONE: StepSound = { addr: -1, count: 0, norm: () => 0, num: () => 0, label: () => "—", short: () => "" };

export function useStepSound(track: number): StepSound {
  const type = useStore((s) => s.tracks.tracks[track]?.instrument ?? "");
  const cat = useStore((s) => s.catalog);
  const key = `${track}:${type}`;
  const [names, setNames] = useState<string[]>(() => namesCache.get(key) ?? []);
  useEffect(() => {
    if (!type || type === "midiext" || type === "octa") return;
    let alive = true;
    void actions.trackSoundNames(track).then((n: string[]) => { if (alive && Array.isArray(n)) { namesCache.set(key, n); setNames(n); } }).catch(() => {});
    return () => { alive = false; };
  }, [key]); // eslint-disable-line react-hooks/exhaustive-deps
  const L = cat?.layout;
  if (!L || !type || type === "octa") return NONE;
  if (type === "midiext") {
    const m = cat?.instruments.find((i) => i.type === "midiext");
    const i = m?.params.findIndex((p) => p.id === "program") ?? -1;
    if (!m || i < 0) return NONE;
    const d = m.params[i];
    return {
      addr: track * L.slotsPerChannel + L.instrumentBase + i, count: 128,
      norm: (n) => toNorm(d, n), num: (v) => Math.round(toPlain(d, v)),
      label: (n) => (n > 0 ? `Son ${n}` : "—"), short: (n) => (n > 0 ? `S${n}` : ""),
    };
  }
  const slot = L.trackSoundSlot, max = L.trackSoundMax ?? 1023;
  if (slot === undefined) return NONE;
  const count = Math.min(max, names.length);
  return {
    addr: track * L.slotsPerChannel + slot, count,
    norm: (n) => n / max, num: (v) => Math.round(v * max),
    label: (n) => (n > 0 ? `${n} · ${names[n - 1] ?? "?"}` : "—"), short: (n) => (n > 0 ? `S${n}` : ""),
  };
}
