// Groovebox — OFAD — AGPL-3.0-or-later
// Liaisons encodeur ↔ réglage du moteur.
//  - mode normal : la valeur de base du réglage ;
//  - « Éditer A / B / Break » : l'encodeur écrit dans la scène éditée ;
//  - pas choisi en édition (principe Elektron) : l'encodeur écrit un verrou dans ce pas ;
//  - choix à crans : un cran = une valeur.

import { useEffect, useState } from "react";
import { actions, getState, globalOf, layout, resolveGlobal, slots, useStore } from "../engine/store";
import { formatNorm, isBipolar, isStepped, stepCount, toNorm, toPlain } from "../engine/params";
import { BREAK_SCENE } from "../engine/protocol";
import type { EncoderBinding, ValueEntry } from "./Encoder";

/** Réglages à crans jamais modulés : nombre et mode des voix (les changer en continu couperait les notes). */
const NOT_MODULATED = new Set(["voices", "vmode", "univoices", "mode"]);

/** Scène qui remplace la valeur d'un réglage au crossfader (côté A ou B tout au bout, ou le côté
    qui pèse le plus). null si aucune scène active ne verrouille ce réglage. */
function sceneOverride(global: number): { side: "A" | "B"; scene: number } | null {
  const s = getState();
  const g = s.tick.g;
  const has = (sc: number) => {
    const d = s.scenes[sc - 1];
    return !!d && !d.muted && d.locks.some(([slot]) => slot === global);
  };
  const a = has(Math.round(g.sceneA)), b = has(Math.round(g.sceneB));
  if (!a && !b) return null;
  if (g.xfade < 0.5) return a ? { side: "A", scene: Math.round(g.sceneA) } : g.xfade > 0.001 ? { side: "B", scene: Math.round(g.sceneB) } : null;
  return b ? { side: "B", scene: Math.round(g.sceneB) } : g.xfade < 0.999 ? { side: "A", scene: Math.round(g.sceneA) } : null;
}

export type WriteTarget =
  | { kind: "base" }
  | { kind: "scene"; scene: number; slot: "A" | "B" | "Break" }
  | { kind: "step"; lane: number; step: number }
  | { kind: "steps"; cells: [number, number][] };

/** Où écrivent les encodeurs pour un réglage du canal donné. */
export function useWriteTarget(channel: number): WriteTarget {
  const editSlot = useStore((s) => s.ui.editSlot);
  const editBreak = useStore((s) => s.ui.editBreak);
  const g = useStore((s) => s.tick.g);
  const stepEdit = useStore((s) => s.ui.stepEdit);
  const step = useStore((s) => s.ui.step);
  const lane = useStore((s) => s.ui.lane);
  const focus = useStore((s) => s.focus);
  const selection = useStore((s) => s.ui.selection);
  if (editBreak) return { kind: "scene", scene: BREAK_SCENE, slot: "Break" };
  if (editSlot) return { kind: "scene", scene: Math.round(editSlot === "A" ? g.sceneA : g.sceneB) - 1, slot: editSlot };
  // plusieurs pas sélectionnés : le verrou est écrit dans chacun
  if (stepEdit && selection.length > 1 && channel === focus && focus < 16)
    return { kind: "steps", cells: selection.map((k) => [Math.floor(k / 1024), k % 1024] as [number, number]) };
  if (stepEdit && step !== null && channel === focus && focus < 16) return { kind: "step", lane, step };
  return { kind: "base" };
}

function lockedValue(target: WriteTarget, global: number): number | undefined {
  const s = getState();
  if (target.kind === "scene") return s.scenes[target.scene]?.locks.find(([g]) => g === global)?.[1];
  if (target.kind === "step") return s.extras.find((e) => e.lane === target.lane && e.step === target.step)?.locks.find(([g]) => g === global)?.[1];
  if (target.kind === "steps") {
    for (const [l, st] of target.cells) {
      const v = s.extras.find((e) => e.lane === l && e.step === st)?.locks.find(([g]) => g === global)?.[1];
      if (v !== undefined) return v;
    }
  }
  return undefined;
}

/** Écrit une valeur là où écrivent les encodeurs (base, scène, pas) : pour les visuels cliquables. */
export function writeParam(target: WriteTarget, global: number, norm: number) { write(target, global, norm); }

/** Réglages tout juste tournés : pendant le geste, l'encodeur montre la valeur écrite et non la
    valeur jouée (sinon il sautait entre les deux quand une scène ou un verrou la remplace). */
const recentEdits = new Map<number, number>();
const EDIT_HOLD_MS = 700;

function write(target: WriteTarget, global: number, norm: number) {
  const v = Math.min(1, Math.max(0, norm));
  if (target.kind === "base") recentEdits.set(global, performance.now());
  if (target.kind === "scene") void actions.scene("sceneSetLock", target.scene, global, v);
  else if (target.kind === "step") void actions.extras("stepLock", target.lane, target.step, global, v);
  else if (target.kind === "steps") void actions.stepsLock(target.cells, global, v, false);
  else actions.setParam(global, v);
}

function clearLock(target: WriteTarget, global: number) {
  if (target.kind === "scene") void actions.scene("sceneClearLock", target.scene, global);
  else if (target.kind === "step") void actions.extras("stepClearLock", target.lane, target.step, global);
  else if (target.kind === "steps") void actions.stepsLock(target.cells, global, 0, true);
  else actions.resetParam(global);
}

const MACHINE_PARAMS = ["tune", "decay", "tone", "snap", "x1", "x2", "x3"];
/** Libellé d'un réglage de voix d'un pad d'une batterie à machines : celui de la machine choisie. */
/** Sampler à pads : les trois commandes d'un bus prennent le nom de ce qu'elles règlent dans l'effet choisi. */
const SP_BUS_LABELS: string[][] = [
  ["Commande 1", "Commande 2", "Commande 3"],
  ["Craquements", "Usure", "Pleurage"],
  ["Bits", "Échantillonnage", "Coupe-haut"],
  ["Pleurage", "Saturation", "Souffle"],
  ["Filtre ◀ PB · PH ▶", "Résonance", "Saturation"],
  ["Graves", "Médiums", "Aigus"],
  ["Longueur", "Vitesse", "Boucle"],
  ["Temps", "Réinjection", "Mélange"],
  ["Taille", "Brillance", "Mélange"],
];

export function machineLabel(global: number): string | undefined {
  const s = getState();
  const L = s.catalog?.layout;
  if (!L) return undefined;
  const ch = Math.floor(global / L.slotsPerChannel), local = global % L.slotsPerChannel;
  if (ch >= L.numTracks || local < L.instrumentBase || local >= L.insertBase) return undefined;
  const m = s.catalog?.instruments.find((i) => i.type === s.tracks.tracks[ch]?.instrument);
  if (m?.type === "sp") {
    const bus = /^b([12])c([123])$/.exec(m.params[local - L.instrumentBase]?.id ?? "");
    if (!bus) return undefined;
    const ti = m.params.findIndex((p) => p.id === `b${bus[1]}type`);
    const norm = s.channels[ch]?.inst?.[ti];
    if (ti < 0 || norm === undefined) return undefined;
    const pd = m.params[ti];
    const type = Math.min(SP_BUS_LABELS.length - 1, Math.max(0, Math.round(toPlain(pd, norm))));
    return SP_BUS_LABELS[type]?.[Number(bus[2]) - 1];
  }
  if (!m?.machines) return undefined;
  const id = m.params[local - L.instrumentBase]?.id ?? "";
  const hit = /^(p\d\d)_(\w+)$/.exec(id);
  if (!hit) return undefined;
  const k = MACHINE_PARAMS.indexOf(hit[2]);
  if (k < 0) return undefined;
  const mi = m.params.findIndex((p) => p.id === `${hit[1]}_mach`);
  const norm = s.channels[ch]?.inst?.[mi];
  if (mi < 0 || norm === undefined) return undefined;
  const machine = Math.min(m.machines.length - 1, Math.floor(norm * m.machines.length));
  return m.machines[machine]?.labels[k];
}

/** Liaison d'un réglage du moteur par son adresse globale. */
export function useParam(global: number | null, label?: string): EncoderBinding | null {
  const S = slots();
  const ch = global !== null && global >= 0 ? Math.floor(global / S) : -1;
  const target = useWriteTarget(ch);
  // abonnements : valeur de base, valeur jouée, verrous
  useStore((s) => (global !== null && global >= 0 ? s.channels[ch] : null));
  useStore((s) => (ch === s.focus && global !== null ? s.eff[global % S] : undefined));
  useStore((s) => (target.kind === "scene" ? s.scenes[target.scene] : target.kind === "step" || target.kind === "steps" ? s.extras : null));
  useStore((s) => s.scenes);
  useStore((s) => (Math.round(s.tick.g.xfade * 100) << 12) + s.tick.g.sceneA * 64 + s.tick.g.sceneB);
  // fin du geste : un nouveau rendu pour revenir à la valeur jouée
  const [, setHoldEnd] = useState(0);
  const editedAt = global !== null && global >= 0 ? recentEdits.get(global) ?? 0 : 0;
  useEffect(() => {
    const left = editedAt + EDIT_HOLD_MS - performance.now();
    if (left <= 0) return;
    const id = window.setTimeout(() => setHoldEnd((n) => n + 1), left + 20);
    return () => window.clearTimeout(id);
  }, [editedAt]);
  if (global === null || global < 0) return null;
  const r = resolveGlobal(global);
  if (!r) return null;
  label = label ?? machineLabel(global);
  const s = getState();
  const base = (() => {
    const c = s.channels[ch];
    if (!c) return 0;
    return baseValueOf(global);
  })();
  const lock = lockedValue(target, global);
  const eff = ch === s.focus ? s.eff[global % S] : undefined;
  const editing = target.kind === "base" && performance.now() - editedAt < EDIT_HOLD_MS;
  const shown = target.kind === "base" ? (editing ? base : eff ?? base) : (lock ?? base);
  const d = r.desc;
  // en mode normal : réglage remplacé par une scène au crossfader (sa valeur ne suit plus l'encodeur)
  const sceneOver = target.kind === "base" ? sceneOverride(global) : null;
  const lockedFlag = target.kind === "scene" && lock !== undefined ? (target.slot === "B" ? "B" : "A")
    : (target.kind === "step" || target.kind === "steps") && lock !== undefined ? "A"
    : sceneOver ? sceneOver.side : null;
  const hint = sceneOver ? `Verrouillé par la scène ${sceneOver.scene} (crossfader côté ${sceneOver.side}) : le son garde la valeur de la scène. `
    + `Pour le régler ici, effacez ce verrou dans l'écran Scènes ou passez le crossfader de l'autre côté.` : undefined;
  const modTarget = r.area !== "amount" && r.area !== "mod" ? { channel: ch, local: global % S } : undefined;
  // réglages à crans modulables aussi (le moteur arrondit au cran) ; pas ceux qui réorganisent les voix
  const steppedMod = modTarget && !NOT_MODULATED.has(d.id) ? modTarget : undefined;
  // saisie exacte (menu de l'encodeur) : valeur en unités du réglage, ou choix dans la liste
  const entry: ValueEntry = {
    plain: toPlain(d, shown), min: d.min, max: d.max, unit: d.unit, integer: !!d.integer, choices: d.choices,
    set: (plain) => { if (Number.isFinite(plain)) write(target, global, toNorm(d, Math.min(d.max, Math.max(d.min, plain)))); },
    pick: (i) => write(target, global, toNorm(d, i)),
  };

  if (isStepped(d)) {
    const n = stepCount(d);
    const plain = toPlain(d, shown);
    const index = d.choices ? plain : plain - d.min;
    return {
      kind: "stepped",
      label: label ?? d.name,
      text: formatNorm(d, shown),
      normalised: n > 1 ? index / (n - 1) : 0,
      locked: lockedFlag,
      hint,
      mod: steppedMod,
      entry,
      onStep: (dir) => {
        const nextIndex = Math.min(n - 1, Math.max(0, index + dir));
        if (nextIndex !== index) write(target, global, toNorm(d, d.choices ? nextIndex : d.min + nextIndex));
      },
      reset: () => clearLock(target, global),
    };
  }
  return {
    kind: "continuous",
    label: label ?? d.name,
    text: formatNorm(d, shown),
    normalised: shown,
    bipolar: isBipolar(d) || d.unit === "pan",
    locked: lockedFlag,
    hint,
    mod: modTarget,
    entry,
    onChange: (v) => write(target, global, v),
    reset: () => clearLock(target, global),
  };
}

function baseValueOf(global: number) {
  // évite une dépendance circulaire : la valeur de base est lue dans le magasin
  const L = layout();
  const s = getState();
  const ch = Math.floor(global / L.slotsPerChannel), local = global % L.slotsPerChannel;
  const c = s.channels[ch];
  if (!c) return 0;
  if (local < L.instrumentBase) return c.mix[local] ?? 0;
  if (local < L.insertBase) return c.inst?.[local - L.instrumentBase] ?? 0;
  if (local < L.modSourceBase) { const k = Math.floor((local - L.insertBase) / L.insertSlots); return c.fx[k]?.values[(local - L.insertBase) % L.insertSlots] ?? 0; }
  if (local < L.modAmountBase) { const k = Math.floor((local - L.modSourceBase) / L.modSourceSlots); return c.mod[k]?.[(local - L.modSourceBase) % L.modSourceSlots] ?? 0; }
  // inserts des pads : interrupteurs puis réglages (pad × 2 + n)
  const pb = L.padBypassBase ?? -1, pi = L.padInsertBase ?? -1;
  if (pb >= 0 && local >= pb && local < pb + 32) return c.padfx?.find((f) => f.pad * 2 + f.slot === local - pb)?.bypass ?? 0;
  if (pi >= 0 && local >= pi) { const k = Math.floor((local - pi) / L.insertSlots); return c.padfx?.find((f) => f.pad * 2 + f.slot === k)?.values[(local - pi) % L.insertSlots] ?? 0; }
  return 0.5;
}

/** Adresse globale d'un réglage d'un module du canal. */
export function paramAddress(channel: number, area: "mix" | "inst" | "fx" | "mod" | "pfx" | "padbyp", slot: number, index: number): number {
  const L = layout();
  // pfx / padbyp : slot = pad × 2 + n (inserts des pads de batterie)
  const base = area === "mix" ? L.mixerBase : area === "inst" ? L.instrumentBase
    : area === "fx" ? L.insertBase + slot * L.insertSlots
    : area === "pfx" ? (L.padInsertBase ?? 2048) + slot * L.insertSlots
    : area === "padbyp" ? (L.padBypassBase ?? 1600) + slot
    : L.modSourceBase + slot * L.modSourceSlots;
  return globalOf(channel, base + index);
}

/** Liaison à crans sur une liste de valeurs (réglages de l'interface ou du séquenceur). */
export function steppedBinding<T>(label: string, options: readonly T[], current: T, onPick: (v: T) => void, show: (v: T) => string = String): EncoderBinding {
  const index = Math.max(0, options.indexOf(current));
  return {
    kind: "stepped",
    label,
    text: show(current),
    normalised: options.length > 1 ? index / (options.length - 1) : 0,
    onStep: (dir) => {
      const next = Math.min(options.length - 1, Math.max(0, index + dir));
      if (next !== index) onPick(options[next]);
    },
  };
}

/** Liaison continue sur une valeur de l'interface. */
export function valueBinding(label: string, normalised: number, text: string, onChange: (v: number) => void, bipolar = false): EncoderBinding {
  return { kind: "continuous", label, text, normalised, bipolar, onChange: (v) => onChange(Math.min(1, Math.max(0, v))) };
}
