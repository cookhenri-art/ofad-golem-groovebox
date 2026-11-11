// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : rôle de chaque réglage d'une section dans sa courbe (enveloppe, filtre, oscillateur),
// d'après son NOM et son UNITÉ, pour tous les instruments (les identifiants varient d'une machine à
// l'autre : « fa », « e1attack », « me1_att », « p01_aatk »…). Les courbes de l'écran SON se
// dessinent avec les valeurs réelles et chaque point se tire pour régler le réglage qu'il représente.

import type { ModuleDesc, ParamDesc, SectionDesc } from "../engine/protocol";

/** Nom sans accents, en minuscules. */
export const norm = (s: string) => s.normalize("NFD").replace(/[̀-ͯ]/g, "").toLowerCase();

// ---------------------------------------------------------------------------------------------
// Enveloppes

export type EnvRole = "delay" | "attack" | "hold" | "decay" | "sustain" | "decay2" | "sustain2" | "release" | "end";

export interface EnvGroup {
  key: string;            // « » pour l'enveloppe unique, sinon « 1-4 », « a »…
  label: string;
  /** index du réglage dans module.params pour chaque étape présente */
  roles: Partial<Record<EnvRole, number>>;
  /** liste « mode » (ADSR, ADS1DS2R, Un coup, Boucle S1-S2, Boucle tout) et interrupteur « Boucle » */
  mode?: number;
  loop?: number;
}

/** Enveloppe à vitesses et niveaux (4 étapes, principe des synthés FM à 6 opérateurs). */
export interface RateLevelEnv { key: string; rates: number[]; levels: number[]; bipolar: boolean }

const ENV_WORDS: [EnvRole, RegExp][] = [
  ["delay", /\b(retard)\b/],
  ["decay2", /\b(declin 2)\b/],
  ["sustain2", /\b(niveau 2)\b/],
  ["attack", /\b(attaque|crescendo)\b|^a$/],
  ["decay", /\b(declin)\b|^d$/],
  ["release", /\b(relachement)\b|^r$/],
  ["end", /\b(fin)\b/],
];

/** Rôle d'un réglage dans une enveloppe, ou null. */
export function envRoleOf(p: ParamDesc): EnvRole | null {
  if (p.choices && p.choices.length && !/attaque/.test(norm(p.name))) return null;
  const n = norm(p.name);
  if (/[→←]/.test(p.name) || /\b(vel|velocite|qte|quantite|niveau vca|volume|hauteur \(|boucle|mode|synchro|verrou|declench)\b/.test(n)) return null;
  // « Maintien » / « Tenue » : un temps (ms) = tenue au sommet ; un pourcentage = niveau de maintien
  if (/\b(maintien|tenue)\b|^s$/.test(n)) return p.unit === "ms" ? "hold" : (p.unit === "%" || p.unit === "") ? "sustain" : null;
  if ((/\bniveau 1\b/.test(n) || /^niveau$/.test(n)) && p.unit === "%") return "sustain";
  for (const [role, re] of ENV_WORDS) if (re.test(n)) {
    if (role === "sustain2" && p.unit !== "%") return null;
    return role;
  }
  return null;
}

/** Clé de groupe : le nom sans le mot de l'étape (« Attaque 1-4 » → « 1-4 », « Attaque A » → « a »). */
function envKey(p: ParamDesc): string {
  if (p.name.trim().length <= 1) return ""; // « A », « D », « S », « R »
  const n = norm(p.name)
    .replace(/\b(retard|attaque|crescendo|tenue|maintien|declin 2|declin|niveau [12]|relachement|fin|vca|ampli)\b/g, "")
    .replace(/^(env\.?|enveloppe)\s*(filtre|ampli|\d)?/, "")
    .replace(/^op\. \d ·/, "")
    .replace(/[·:]/g, " ").replace(/\s+/g, " ").trim();
  return /^(a|b|1-4|5-8)$/.test(n) ? n : "";
}

/** Enveloppes d'une section : groupes d'étapes (rien si la section n'a pas de temps). */
export function envGroups(m: ModuleDesc, s: SectionDesc): EnvGroup[] {
  const groups = new Map<string, EnvGroup>();
  for (const k of s.params) {
    const p = m.params[k];
    const role = envRoleOf(p);
    if (!role) continue;
    const key = envKey(p);
    const g = groups.get(key) ?? { key, label: key ? key.toUpperCase() : "", roles: {} };
    if (g.roles[role] === undefined) g.roles[role] = k;
    groups.set(key, g);
  }
  // une enveloppe a au moins une durée
  const out = [...groups.values()].filter((g) => g.roles.attack !== undefined || g.roles.decay !== undefined || g.roles.release !== undefined);
  // mode et boucle : dans la section, ou à côté (« fa » → « floop », « e1a » → « e1mode », « me1_att » → « me1_loop »)
  for (const g of out) {
    const first = g.roles.attack ?? g.roles.decay ?? g.roles.release!;
    const prefix = m.params[first].id.replace(/(attack|att|atk|aatk|a|decay|dec|d|release|rel|r)$/, "");
    const find = (suffix: string, re: RegExp) => {
      let i = s.params.find((k) => m.params[k].choices && re.test(norm(m.params[k].name)));
      if (i === undefined && prefix) { const j = m.params.findIndex((p) => p.id === prefix + suffix && !!p.choices && /^(boucle|.*mode)$/.test(norm(p.name))); if (j >= 0) i = j; }
      return i;
    };
    g.loop = find("loop", /\bboucle\b/);
    const mi = find("mode", /\bmode\b/);
    if (mi !== undefined && (m.params[mi].choices ?? []).some((c) => /ADSR|coup|boucle/i.test(c))) g.mode = mi;
  }
  return out;
}

/** Enveloppes vitesses / niveaux d'une section (synthés FM à 6 opérateurs). */
export function rateLevelEnv(m: ModuleDesc, s: SectionDesc): RateLevelEnv | null {
  const rates: number[] = [], levels: number[] = [];
  for (const k of s.params) {
    const n = norm(m.params[k].name);
    const r = /vitesse ([1-4])$/.exec(n), l = /niveau ([1-4])$/.exec(n);
    if (r) rates[Number(r[1]) - 1] = k;
    if (l && !m.params[k].unit) levels[Number(l[1]) - 1] = k;
  }
  if (rates.filter((x) => x !== undefined).length !== 4 || levels.filter((x) => x !== undefined).length !== 4) return null;
  // enveloppe de hauteur : 50 = hauteur jouée (bipolaire)
  const bipolar = !/^op/.test(norm(m.params[rates[0]].name)) && m.params[levels[0]].def === 50;
  return { key: "", rates, levels, bipolar };
}

/** Durée affichée (ms) d'un réglage de temps ; sans unité (0 à 127…) : échelle proportionnelle. */
export function timeMs(p: ParamDesc, plain: number): number {
  if (p.choices && p.choices.length) return plain === 0 ? 3 : 250; // « Rapide / Lente »
  if (p.unit === "ms") return plain;
  if (p.unit === "s") return plain * 1000;
  return ((plain - p.min) / Math.max(1e-9, p.max - p.min)) * 5000;
}
/** Inverse de timeMs : valeur du réglage pour une durée. */
export function plainForMs(p: ParamDesc, ms: number): number {
  let v: number;
  if (p.choices && p.choices.length) v = ms > 40 ? 1 : 0;
  else if (p.unit === "ms") v = ms;
  else if (p.unit === "s") v = ms / 1000;
  else v = p.min + (ms / 5000) * (p.max - p.min);
  return Math.min(p.max, Math.max(p.min, v));
}
/** Niveau 0..1 d'un réglage de niveau (% ou 0..max). */
export function levelOf(p: ParamDesc, plain: number): number {
  return Math.min(1, Math.max(0, (plain - p.min) / Math.max(1e-9, p.max - p.min)));
}
export function plainForLevel(p: ParamDesc, lv: number): number {
  return p.min + Math.min(1, Math.max(0, lv)) * (p.max - p.min);
}

// ---------------------------------------------------------------------------------------------
// Filtres

export type FilterShape = "lp" | "bp" | "hp" | "notch" | "off";
export interface FilterGroup {
  key: string;
  label: string;
  cutoff: number;            // index du réglage de coupure (Hz)
  reso?: number;
  type?: number;             // liste de types (passe-bas 24…)
  morph?: number;            // fondu continu bas → bande → haut (ou bas → réjection → haut)
  morphNotch?: boolean;
  bandpass?: number;         // interrupteur « Passe-bande » (filtre d'état)
  shapeFromName?: FilterShape;
}

const isHz = (p: ParamDesc) => p.unit === "Hz" && !(p.choices && p.choices.length);

function filterKey(p: ParamDesc): string {
  const n = norm(p.name).replace(/^t\d+ · /, "")
    .replace(/\b(coupure|frequence|resonance|filtre|type de filtre|type|du melange|globale)\b/g, "")
    .replace(/\s+/g, " ").trim();
  if (/passe-haut/.test(n)) return "passe-haut";
  if (/passe-bas/.test(n)) return "passe-bas";
  return /^(a|b|1|2)$/.test(n) ? n : n === "" ? "" : n;
}

/** Filtres d'une section (un ou deux), d'après les réglages en Hz. */
export function filterGroups(m: ModuleDesc, s: SectionDesc): FilterGroup[] {
  const out: FilterGroup[] = [];
  const cut = s.params.filter((k) => isHz(m.params[k]) && !/\b(base)\b/.test(norm(m.params[k].name)));
  for (const k of cut) {
    const key = filterKey(m.params[k]);
    const g: FilterGroup = { key, label: key && key.length <= 2 ? key.toUpperCase() : key, cutoff: k };
    if (key === "passe-haut") g.shapeFromName = "hp";
    else if (key === "passe-bas") g.shapeFromName = "lp";
    out.push(g);
  }
  const same = (k: number, g: FilterGroup) => out.length === 1 || filterKey(m.params[k]) === g.key;
  for (const k of s.params) {
    const p = m.params[k], n = norm(p.name);
    for (const g of out) {
      if (!same(k, g)) continue;
      if (g.reso === undefined && /resonance|peak/.test(n) && !/globale/.test(n)) g.reso = k;
      else if (g.type === undefined && p.choices && p.choices.some((c) => /passe-/i.test(c))) g.type = k;
      else if (g.morph === undefined && !p.choices && /(bas|haut) .*→|→ .*(haut|bas)|mode \(bas/.test(n)) { g.morph = k; g.morphNotch = /rejection/.test(n); }
      else if (g.bandpass === undefined && p.choices && /^passe-bande$/.test(n)) g.bandpass = k;
    }
  }
  // un seul filtre et le titre de la section dit sa nature (« Passe-haut »)
  if (out.length === 1 && !out[0].shapeFromName && out[0].type === undefined) {
    const t = norm(s.name);
    if (/passe-haut/.test(t)) out[0].shapeFromName = "hp";
  }
  return out;
}

/** Forme et pente d'un filtre d'après le libellé de son type. */
export function shapeOfChoice(choice: string): { shape: FilterShape; poles: number } {
  const c = norm(choice);
  const shape: FilterShape = /arret|aucun|contourne/.test(c) ? "off" : /haut/.test(c) ? "hp" : /bande/.test(c) ? "bp" : /rejection|coupe-bande/.test(c) ? "notch" : "lp";
  const poles = /24|4 poles/.test(c) ? 4 : /18/.test(c) ? 3 : /12|2 poles/.test(c) ? 2 : /\b6 db/.test(c) ? 1 : shape === "bp" ? 2 : 4;
  return { shape, poles };
}

// ---------------------------------------------------------------------------------------------
// Oscillateurs

export interface OscGroup {
  wave?: number;     // liste de formes (Scie, Carré…)
  shape?: number;    // forme continue (0 sinus → 4 impulsion) ou position de table
  pw?: number;       // largeur d'impulsion
  level?: number;
}

/** Oscillateurs d'une section : le premier et, s'il existe, un second (dessiné en retrait). */
export function oscGroups(m: ModuleDesc, s: SectionDesc): OscGroup[] {
  const groups: OscGroup[] = [];
  const keyOf = (p: ParamDesc) => /^(?:[a-z]*?)(\d|[ab])(?=[a-z_])/.exec(p.id)?.[1] ?? "";
  const byKey = new Map<string, OscGroup>();
  for (const k of s.params) {
    const p = m.params[k], n = norm(p.name), id = p.id;
    const key = keyOf(p);
    const g = byKey.get(key) ?? {};
    if (g.wave === undefined && p.choices && /wave|shape/.test(id) && p.choices.some((c) => /scie|carre|sinus|triangle|impulsion/i.test(norm(c)))) g.wave = k;
    else if (g.shape === undefined && !p.choices && (/shape|pos$/.test(id) || /^forme osc/.test(n)) && !/pwm|smod|samt/.test(id)) g.shape = k;
    else if (g.pw === undefined && !p.choices && (/(^|[^m])pw$|largeur/.test(id) || /largeur|impulsion$/.test(n)) && p.unit === "%") g.pw = k;
    else continue;
    byKey.set(key, g);
  }
  for (const g of byKey.values()) if (g.wave !== undefined || g.shape !== undefined) groups.push(g);
  return groups.slice(0, 2);
}

/** Forme continue d'un libellé de forme (0 sinus, 1 triangle, 2 scie, 3 carré, 4 impulsion). */
export function shapeOfWaveName(name: string): number | null {
  const n = norm(name);
  if (/sinus/.test(n)) return 0;
  if (/triangle/.test(n)) return 1;
  if (/scie/.test(n)) return 2;
  if (/impulsion|pulse/.test(n)) return 4;
  if (/carre/.test(n)) return 3;
  return null;
}
