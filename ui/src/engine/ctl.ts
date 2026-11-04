// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix24 : cibles pilotables (commandes MIDI apprises, 16 boutons assignables).
// Une cible est un nombre, comme pour l'apprentissage MIDI :
//   >= 0            : réglage du moteur (adresse globale)
//   -2 … -415       : commandes spéciales (crossfader, lecture, coupures…)
//   -600 - h        : réglage global (tempo, swing, scènes, pavé, macros), voir HOST_CTL
//   -700 - n        : valeur d'un écran (Séquence › Longueur…) appliquée par l'interface
// Les valeurs d'écran ont un identifiant stable (« Séquence.Longueur ») rangé dans une table de 64
// numéros, gardée dans le projet ; le moteur renvoie leurs valeurs (MIDI, bouton) dans le tick.

import type { EncoderBinding } from "../controls/Encoder";
import { MIDI_UI_BASE, NUM_UI_CTL, isUiCode, isKnobCode, MIDI_KNOB_BASE } from "./protocol";
import { actions, getState, globalOf, showMessage, tickHooks } from "./store";

interface CtlTable { slots: (string | null)[]; labels: Record<string, string> }

function parseTable(text: string | undefined): CtlTable {
  try {
    const o = JSON.parse(text || "{}") as Partial<CtlTable>;
    const slots = Array.from({ length: NUM_UI_CTL }, (_, i) => (typeof o.slots?.[i] === "string" ? o.slots[i] : null));
    return { slots, labels: typeof o.labels === "object" && o.labels ? o.labels : {} };
  } catch {
    return { slots: Array.from({ length: NUM_UI_CTL }, () => null), labels: {} };
  }
}

const table = () => parseTable(getState().midi.ctl);

/** Valeurs d'écran affichées (dernière liaison de chaque identifiant) : c'est elle qui s'applique. */
const live = new Map<string, EncoderBinding>();
export function registerCtl(id: string, b: EncoderBinding) { live.set(id, b); }

/** Cible d'une liaison : réglage du moteur, commande ou réglage global (ctl), sinon valeur d'écran. */
export function bindingCode(b: EncoderBinding | null | undefined): number | null {
  if (!b) return null;
  if (b.ctl !== undefined) return b.ctl;
  if (b.mod) return globalOf(b.mod.channel, b.mod.local);
  return null;
}

/** Numéro déjà donné à une valeur d'écran (sans en réserver). */
export function peekUiCode(id: string): number | null {
  const n = table().slots.indexOf(id);
  return n >= 0 ? MIDI_UI_BASE - n : null;
}

/** Numéro d'une valeur d'écran : réservé au besoin (la table est gardée dans le projet). */
export async function uiCode(id: string, label: string): Promise<number | null> {
  const t = table();
  let n = t.slots.indexOf(id);
  if (n < 0) {
    // une place libre, sinon une place qu'aucune commande MIDI ni aucun bouton n'utilise
    const used = new Set<number>();
    const s = getState();
    s.midi.maps.forEach((m) => { if (isUiCode(m[3])) used.add(MIDI_UI_BASE - m[3]); });
    s.midi.knobs?.forEach(([target]) => { if (isUiCode(target)) used.add(MIDI_UI_BASE - target); });
    n = t.slots.indexOf(null);
    if (n < 0) n = t.slots.findIndex((_, i) => !used.has(i));
    if (n < 0) { showMessage("Trop de valeurs d'écran pilotées (64) : libérez-en une (bouton ou commande MIDI)"); return null; }
    if (t.slots[n]) delete t.labels[t.slots[n] as string];
    t.slots[n] = id;
  }
  t.labels[id] = label;
  await actions.setCtlTable(JSON.stringify(t));
  return MIDI_UI_BASE - n;
}

/** Cible d'une liaison, valeur d'écran comprise (réservée au besoin). */
export async function resolveCode(b: EncoderBinding | null | undefined, uiId?: string): Promise<number | null> {
  const c = bindingCode(b);
  if (c !== null) return c;
  if (uiId && b) return uiCode(uiId, b.label);
  return null;
}

/** Nom d'une valeur d'écran (table). */
export function uiCtlName(code: number): string {
  const t = table();
  const id = t.slots[MIDI_UI_BASE - code];
  if (!id) return `Valeur ${MIDI_UI_BASE - code + 1}`;
  const label = t.labels[id];
  const screen = id.split(".")[0];
  return label ? `${screen} · ${label}` : id;
}

export const knobOf = (code: number) => (isKnobCode(code) ? MIDI_KNOB_BASE - code : -1);

/** Applique une valeur 0..1 à une valeur d'écran (liaison affichée en dernier). */
function applyUi(n: number, v: number) {
  const id = table().slots[n];
  if (!id) return;
  const b = live.get(id);
  if (!b) { showMessage(`Ouvrez une fois l'écran « ${id.split(".")[0]} » pour que « ${table().labels[id] ?? id} » se pilote`); return; }
  const x = Math.min(1, Math.max(0, v));
  if (b.kind === "continuous") b.onChange(x);
  else if (b.setNorm) b.setNorm(x);
  else if (b.entry?.choices) b.entry.pick(Math.round(x * (b.entry.choices.length - 1)));
  else if (b.entry) b.entry.set(b.entry.min + x * (b.entry.max - b.entry.min));
}

tickHooks.uc = (list) => { for (const [n, v] of list) applyUi(n, v); };
