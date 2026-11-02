// Groovebox — OFAD — AGPL-3.0-or-later
// Sélection du séquenceur : déplacer, copier, coller, dupliquer, effacer, régler plusieurs pas.
// Fonctions pures : elles préparent une édition groupée que le moteur applique en une fois
// (seqEditCells), conditions et verrous compris.

import type { PatternData, StepExtra } from "./protocol";

/** Un pas : ligne (pad ou ligne de note) et position. */
export const cellKey = (lane: number, step: number) => lane * 1024 + step;
export const laneOf = (k: number) => Math.floor(k / 1024);
export const stepOf = (k: number) => k % 1024;

export interface CellData {
  l: number; s: number;
  vel: number; note: number; len: number; micro: number;
  ex?: { cond: number; a: number; b: number; prob: number; rat?: number; rs?: number; flam?: number; arp?: number; locks: [number, number][] };
  /** v0.31 : fonctions du pas d'origine (accent total, glissé, enregistrement, verrous seuls) */
  fl?: number;
}

export interface Clip {
  track: number;
  instrument: string;
  drum: boolean;
  cells: CellData[];
}

let clipboard: Clip | null = null;

/** v0.43.4 : verrous d'un pas copiés (piste, ligne d'origine), à coller sur un autre pas de la même piste. */
export interface LockClip { track: number; lane: number; step: number; locks: [number, number][] }
let lockClip: LockClip | null = null;
export const getLockClip = () => lockClip;
export const setLockClip = (c: LockClip | null) => { lockClip = c; };
export const getClipboard = () => clipboard;
export const setClipboard = (c: Clip | null) => { clipboard = c; };

export function snapshot(p: PatternData, extras: StepExtra[], keys: number[]): CellData[] {
  const out: CellData[] = [];
  for (const k of keys) {
    const l = laneOf(k), s = stepOf(k), ln = p.lanes[l];
    if (!ln || (ln.vel[s] ?? 0) <= 0) continue;
    const e = extras.find((x) => x.lane === l && x.step === s);
    out.push({
      l, s, vel: ln.vel[s], note: ln.note[s], len: ln.len[s], micro: ln.micro[s], fl: p.flags?.[s] ?? 0,
      ex: e ? { cond: e.cond, a: e.a, b: e.b, prob: e.prob, rat: e.rat ?? 1, rs: e.rs ?? 0, flam: e.flam ?? 0, arp: e.arp ?? 0, locks: e.locks.map(([g, v]) => [g, v] as [number, number]) } : undefined,
    });
  }
  return out;
}

/** Tous les pas posés du pattern (Ctrl+A). */
export function allKeys(p: PatternData): number[] {
  const out: number[] = [];
  p.lanes.forEach((ln, l) => { for (let s = 0; s < p.length; ++s) if ((ln.vel[s] ?? 0) > 0) out.push(cellKey(l, s)); });
  return out;
}

export interface Plan { clear: [number, number][]; writes: CellData[]; keys: number[] }

/** Place des pas à un décalage : batterie, dRow = lignes (pads) ; mélodique, dRow = demi-tons
    (la ligne de destination est une ligne libre à ce pas). remove : les pas d'origine sont vidés
    (déplacer) ; sinon ils restent (copier, coller, dupliquer). Les pas hors du pattern sont
    abandonnés ; un pas déjà occupé à destination est remplacé.
    wrap (déplacer) : comme une rotation Elektron, un pas qui sort d'un côté de la boucle revient
    de l'autre (boucle du pattern, ou du pad s'il a sa propre longueur) ; aucun pas n'est perdu. */
export function place(p: PatternData, cells: CellData[], dRow: number, dStep: number, drum: boolean, remove: [number, number][], wrap = false): Plan {
  const lanes = p.lanes.length;
  // occupation après le retrait des pas d'origine
  const busy = new Set<number>();
  p.lanes.forEach((ln, l) => { for (let s = 0; s < p.length; ++s) if ((ln.vel[s] ?? 0) > 0) busy.add(cellKey(l, s)); });
  for (const [l, s] of remove) busy.delete(cellKey(l, s));
  const writesAt = new Map<number, CellData>();
  const noteAt = (s: number, n: number) => {
    for (let l = 0; l < lanes; ++l) if (busy.has(cellKey(l, s)) && writesAt.get(cellKey(l, s))?.note === n) return l;
    for (let l = 0; l < lanes; ++l) if (busy.has(cellKey(l, s)) && !writesAt.has(cellKey(l, s)) && p.lanes[l].note[s] === n) return l;
    return -1;
  };
  const loopOf = (lane: number, from: number) => {
    const own = drum ? p.lanes[lane]?.length ?? 0 : 0;
    return own > 0 && own < p.length && from < own ? own : p.length;
  };
  for (const c of cells) {
    let s = c.s + dStep;
    if (wrap && p.length > 0) { const n = loopOf(drum ? c.l + dRow : c.l, c.s); s = ((s % n) + n) % n; }
    if (s < 0 || s >= p.length) continue;
    let l: number, note = c.note;
    if (drum) {
      l = c.l + dRow;
      if (l < 0 || l >= lanes) continue;
    } else {
      note = c.note + dRow;
      if (note < 0 || note > 127) continue;
      l = noteAt(s, note);                                  // même note déjà là : on la remplace
      if (l < 0 && !busy.has(cellKey(c.l, s))) l = c.l;     // sinon la même ligne si elle est libre
      if (l < 0) for (let k = 0; k < lanes; ++k) if (!busy.has(cellKey(k, s))) { l = k; break; }
      if (l < 0) continue;                                  // accord plein : pas de place
    }
    const w: CellData = { ...c, l, s, note };
    busy.add(cellKey(l, s));
    writesAt.set(cellKey(l, s), w);
  }
  const writes = [...writesAt.values()];
  return { clear: remove, writes, keys: writes.map((w) => cellKey(w.l, w.s)) };
}

/** Recopie les verrous d'une piste vers une autre : adresses de la piste d'origine ramenées sur
    la nouvelle. Ceux de l'instrument sont abandonnés si l'instrument n'est pas le même, ceux
    des inserts et des sources de modulation si les inserts ne sont pas les mêmes (ils
    piloteraient un autre réglage). */
export function retarget(cells: CellData[], from: number, to: number, sameInstrument: boolean, sameInserts: boolean,
  L: { instrumentBase: number; insertBase: number; modAmountBase: number; slotsPerChannel: number }): CellData[] {
  if (from === to) return cells;
  return cells.map((c) => !c.ex ? c : {
    ...c,
    ex: {
      ...c.ex,
      locks: c.ex.locks.flatMap(([g, v]) => {
        const S = L.slotsPerChannel;
        const ch = Math.floor(g / S), local = g % S;
        if (ch !== from) return [[g, v] as [number, number]];
        if (!sameInstrument && local >= L.instrumentBase && local < L.insertBase) return [];
        if (!sameInserts && local >= L.insertBase && local < L.modAmountBase) return [];
        return [[to * S + local, v] as [number, number]];
      }),
    },
  });
}

/** Texte envoyé au moteur. */
export function payload(plan: { clear: [number, number][]; writes: CellData[] }): string {
  return JSON.stringify({
    clear: plan.clear,
    set: plan.writes.map((w) => [w.l, w.s, w.vel, w.note, w.len, w.micro]),
    ex: plan.writes.filter((w) => w.ex).map((w) => [w.l, w.s, w.ex!.cond, w.ex!.a, w.ex!.b, w.ex!.prob, w.ex!.locks, w.ex!.rat ?? 1, w.ex!.rs ?? 0, w.ex!.flam ?? 0, w.ex!.arp ?? 0]),
  });
}

/** Coin haut-gauche d'un ensemble de pas : plus petite position, plus petite ligne (ou plus
    haute note en mélodique). */
export function origin(cells: CellData[], drum: boolean): { row: number; step: number } {
  let step = Infinity, row = drum ? Infinity : -Infinity;
  for (const c of cells) {
    step = Math.min(step, c.s);
    row = drum ? Math.min(row, c.l) : Math.max(row, c.note);
  }
  return { row: Number.isFinite(row) ? row : 0, step: Number.isFinite(step) ? step : 0 };
}

/** v0.31 : fonctions des pas (accent total, glissé, enregistrement, verrous seuls) qui suivent
    une copie, un collage, une duplication ou un déplacement. Les fonctions sont par pas (toutes
    les lignes) : elles sont posées sur les pas de destination ; en déplacement, elles sont
    retirées d'un pas d'origine qui n'a plus aucune note. Retour : [bit, pas, poser] à envoyer
    au moteur (seqStepsFlag). */
export function flagMoves(p: PatternData, writes: CellData[], removed: [number, number][]): [number, number[], boolean][] {
  const out: [number, number[], boolean][] = [];
  const dst = new Set(writes.map((w) => w.s));
  const still = new Set<number>();
  // pas d'origine qui gardent au moins une note après le déplacement
  const removedKeys = new Set(removed.map(([l, s]) => cellKey(l, s)));
  p.lanes.forEach((ln, l) => { for (let s = 0; s < p.length; ++s) if ((ln.vel[s] ?? 0) > 0 && !removedKeys.has(cellKey(l, s))) still.add(s); });
  for (const bit of [1, 2, 4, 8]) {
    const on = [...new Set(writes.filter((w) => ((w.fl ?? 0) & bit) !== 0).map((w) => w.s))];
    if (on.length) out.push([bit, on, true]);
    const off = [...new Set(removed.map(([, s]) => s))].filter((s) => ((p.flags?.[s] ?? 0) & bit) !== 0 && !still.has(s) && !dst.has(s));
    if (off.length) out.push([bit, off, false]);
  }
  return out;
}
