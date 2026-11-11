// Groovebox — OFAD — AGPL-3.0-or-later
// Habillage réaliste (v0.35.1) : matières, boutons, curseurs, vis, molettes et fonds de pédales
// précalculés (pack « Sillage », édition 05 ; lumière en haut à gauche ; seul le repère des boutons
// tourne). Aucun calcul de rendu en jeu : de simples images posées sous les commandes.
//
// Règles (validées avec Henrico) :
//  - synthés : la couleur d'origine de chaque façade est gardée ; la matière remplace l'aplat (teintée
//    de cette couleur), posée à sa taille réelle en carreaux sans raccord (jamais agrandie) ;
//  - un seul type de bouton par synthé, d'après la vraie machine ; les gros boutons sont les mêmes, en grand ;
//  - laques : pédales et Acid Lover seulement ; bois : machines de principe Moog seulement ;
//  - pédales : de vrais boutons seulement (6 au plus, les sélecteurs en petits boutons) ; sinon : rack ;
//  - racks : unités 19 pouces vissées, gros boutons pour les réglages principaux, vumètre ou afficheur.
//
// Changer une matière, une teinte ou un bouton : une ligne de FACE_SKINS / PEDAL_SKINS / RACK_SKINS.
// Fichiers : ui/public/skin/ (préfixe « sk- » : noms uniques pour le fournisseur de ressources).

import { useSyncExternalStore, type CSSProperties } from "react";

// URL absolue : une URL relative dans une variable CSS serait résolue depuis la feuille de style (assets/)
const abs = (f: string) => { try { return new URL(f, document.baseURI).href; } catch { return f; } };
const img = (n: string) => `url("${abs(`skin/sk-${n}.webp`)}")`;

/** Boutons : image du corps (fixe), repère qui tourne, axe du repère (le dessus du bouton). */
export const KNOBS = {
  fatsub:    { body: "v2-bouton-fatsub", mark: "repere-trait", ax: 49.8, ay: 35.7, ms: 0.78 },
  bignappe:  { body: "v2-bouton-bignappe", mark: "repere-trait", ax: 50, ay: 43.8, ms: 1 },
  acid:      { body: "v2-bouton-acid", mark: "repere-trait", ax: 50, ay: 42.3, ms: 1 },
  ivoire:    { body: "v2-bouton-ivoire", mark: "repere-trait-noir", ax: 50, ay: 37.5, ms: 0.88 },
  noir:      { body: "v2-bouton-noir", mark: "repere-trait", ax: 50, ay: 39.6, ms: 0.8 },
  moog:      { body: "bouton-moog", mark: "repere-trait", ax: 50, ay: 43.8, ms: 0.78 },
  poly:      { body: "bouton-poly", mark: "repere-trait", ax: 50, ay: 43.8, ms: 1 },
  pedale:    { body: "bouton-pedale", mark: "repere-trait", ax: 50, ay: 43.3, ms: 1 },
  creme:     { body: "bouton-creme", mark: "repere-trait-noir", ax: 50, ay: 43.8, ms: 1 },
  echo:      { body: "bouton-echo", mark: "repere-point", ax: 49.7, ay: 44.1, ms: 1 },
  plastique: { body: "bouton-plastique", mark: "repere-trait", ax: 51, ay: 44.3, ms: 1 },
  chrome:    { body: "bouton-chrome", mark: "repere-trait-noir", ax: 49.4, ay: 42.4, ms: 1 },
} as const;
export type KnobName = keyof typeof KNOBS;

/** Matières en carreaux sans raccord (« sk-t-… » : image en miroir 2 × 2) et leur largeur d'affichage
    (taille réelle de l'image, ou plus fine : jamais agrandie). */
const TEXTURES = {
  "alu-brosse": 3200, "alu-sombre": 3200, "peinture-noire": 1024, "v3-metal-noir": 3200, "v3-abs-noir": 720,
  "plastique-gris": 1024, "v3-alu-bleu": 3200, "v3-laque-ambre": 3200, "alu-orange": 3200,
  "v3-bois-noyer": 480,
} as const;
export type Texture = keyof typeof TEXTURES;

export interface FaceSkin {
  tex: Texture;              // matière du panneau
  tint?: string;             // couleur d'origine de la façade, posée sur la matière (multiplication)
  cheek?: Texture;           // joues (noyer : machines de principe Moog)
  knob: KnobName;            // le bouton de la machine (un seul type)
  ink: "light" | "dark";     // graduations claires ou sombres
  fader?: "curseur" | "curseur-gris" | "v2-curseur-bignappe";
}

/** Façades des instruments, par thème de façade (layouts.ts). */
export const FACE_SKINS: Record<string, FaceSkin> = {
  // principe Moog : tôle noire, joues en noyer, bouton noir à jupe chromée
  fatsub:    { tex: "v3-metal-noir", cheek: "v3-bois-noyer", knob: "fatsub", ink: "light" },
  kicker:    { tex: "v3-metal-noir", cheek: "v3-bois-noyer", knob: "moog", ink: "light" },
  // pack Sillage : Acid Lover en métal laqué ambre, Bignappe One en aluminium bleu anodisé
  acid:      { tex: "v3-laque-ambre", knob: "acid", ink: "dark" },
  nappe:     { tex: "v3-alu-bleu", cheek: "v3-metal-noir", knob: "bignappe", ink: "light", fader: "v2-curseur-bignappe" },
  // polyphoniques analogiques
  five:      { tex: "v3-metal-noir", knob: "poly", ink: "light" },
  grace:     { tex: "alu-sombre", knob: "noir", ink: "light" },
  chorus:    { tex: "alu-brosse", tint: "oklch(0.32 0.004 250)", knob: "plastique", ink: "light", fader: "curseur" },
  // monos japonais et semi-modulaire
  hurleur:   { tex: "v3-metal-noir", knob: "plastique", ink: "light" },
  monofirst: { tex: "plastique-gris", tint: "oklch(0.55 0.005 250)", knob: "plastique", ink: "light", fader: "curseur" },
  // numériques et hybrides
  pointe:    { tex: "alu-brosse", tint: "oklch(0.3 0.012 260)", knob: "echo", ink: "light" },
  onde:      { tex: "alu-brosse", tint: "oklch(0.26 0.025 285)", knob: "echo", ink: "light" },
  bretzel:   { tex: "alu-brosse", tint: "oklch(0.64 0.006 250)", knob: "chrome", ink: "light" },
  orbite:    { tex: "alu-brosse", tint: "oklch(0.25 0.02 260)", knob: "noir", ink: "light" },
  // batteries
  machine:   { tex: "alu-sombre", knob: "noir", ink: "light" },
  boum:      { tex: "plastique-gris", tint: "oklch(0.35 0.008 60)", knob: "plastique", ink: "light" },
  club:      { tex: "alu-brosse", tint: "oklch(0.88 0.02 230)", knob: "plastique", ink: "dark" },
  dragon:    { tex: "v3-metal-noir", knob: "noir", ink: "light" },
  cutbeat:   { tex: "alu-brosse", tint: "oklch(0.26 0.016 200)", knob: "echo", ink: "light" },
  // vocodeurs
  voc16:     { tex: "plastique-gris", tint: "#ebe4d4", knob: "noir", ink: "dark" },
  vocstr:    { tex: "alu-brosse", tint: "#5e2832", knob: "poly", ink: "light" },
  vocspec:   { tex: "alu-sombre", knob: "echo", ink: "light" },
  vocsyn:    { tex: "alu-brosse", tint: "#2c3352", knob: "plastique", ink: "light" },
  // échantillonneurs
  sampler:   { tex: "alu-brosse", tint: "oklch(0.28 0.02 60)", knob: "noir", ink: "light" },
  sp:        { tex: "plastique-gris", tint: "oklch(0.3 0.02 150)", knob: "plastique", ink: "light" },
  braise:    { tex: "v3-abs-noir", knob: "plastique", ink: "light" }, // Glutamake (v0.47) : ABS noir, boutons noirs lisses
  docteur:   { tex: "alu-sombre", tint: "oklch(0.2 0.03 245)", knob: "noir", ink: "light" }, // Good Doctor (v0.47 (maj. 5 nov.)) : bleu nuit
  pieuvre:   { tex: "alu-sombre", knob: "noir", ink: "light" },
  // v0.46 : synthés FM et drones
  sauce:     { tex: "v3-metal-noir", knob: "chrome", ink: "light" },
  lyma:      { tex: "v3-metal-noir", cheek: "v3-bois-noyer", knob: "noir", ink: "light" },
  kdragon:   { tex: "alu-sombre", knob: "noir", ink: "light" },
  generic:   { tex: "alu-sombre", knob: "noir", ink: "light" },   // façades sans disposition (Entrée audio)
};

/** Pédales : fond illustré (10-pedales-illustrees), format, un seul style de bouton (parfois un bouton de
    même forme dans l'autre couleur : alt, aux positions altAt). */
export interface PedalSkin { bg: string; tall?: boolean; knob: KnobName; alt?: KnobName; altAt?: number[]; ink: "light" | "dark" }
export const PEDAL_SKINS: Record<string, PedalSkin> = {
  reverb:    { bg: "01-reverberation", knob: "chrome", ink: "light" },
  plate:     { bg: "02-plaque", knob: "pedale", ink: "dark" },
  spring:    { bg: "03-ressort", knob: "creme", ink: "light" },
  chorus:    { bg: "04-chorus", knob: "ivoire", ink: "light" },
  phaser:    { bg: "05-phaser", knob: "pedale", ink: "light" },
  stutter:   { bg: "06-repetition", knob: "chrome", ink: "light" },
  filter:    { bg: "07-filtre", knob: "ivoire", ink: "light" },
  mdelay:    { bg: "08-echos-multiples", tall: true, knob: "chrome", ink: "light" },
  tapeecho:  { bg: "09-echo-bande-tetes", tall: true, knob: "pedale", alt: "creme", altAt: [0], ink: "dark" },
  looper:    { bg: "10-boucleur", knob: "creme", ink: "light" },
  flanger:   { bg: "11-flanger", knob: "chrome", ink: "light" },
  phaser2:   { bg: "12-phaser-double", tall: true, knob: "pedale", ink: "light" },
  dimension: { bg: "13-dimension", knob: "pedale", ink: "dark" },
  tapesat:   { bg: "14-saturation-bande", knob: "creme", ink: "light" },
  tubesat:   { bg: "15-saturation-lampe", knob: "chrome", ink: "light" },
  transient: { bg: "16-sculpteur-transitoires", knob: "pedale", ink: "dark" },
  chorus3:   { bg: "17-chorus-trois-voix", knob: "ivoire", ink: "dark" },
};

/** Racks (unités 19 pouces) : façade, bouton, encre ; vu : vumètre de réduction ; lcd : réglage affiché. */
export interface RackSkin { tex: Texture; tint?: string; knob: KnobName; ink: "light" | "dark"; vu?: boolean; lcd?: string }
const RACK_FAMILY: Record<string, RackSkin> = {
  "Compresseurs et dynamique": { tex: "alu-brosse", knob: "noir", ink: "dark", vu: true },
  "Égaliseurs": { tex: "alu-sombre", knob: "chrome", ink: "light" },
  "Mastering": { tex: "v3-metal-noir", knob: "chrome", ink: "light", vu: true },
  "Échos": { tex: "alu-orange", knob: "noir", ink: "dark" },
  "Réverbérations": { tex: "v3-metal-noir", knob: "chrome", ink: "light" },
  "Saturation": { tex: "v3-metal-noir", knob: "chrome", ink: "light" },
  "Modulation": { tex: "alu-sombre", knob: "noir", ink: "light" },
  "Filtres et performance": { tex: "alu-sombre", knob: "noir", ink: "light" },
  "Restauration": { tex: "alu-brosse", tint: "oklch(0.5 0.035 205)", knob: "noir", ink: "light" }, // 1.1 : outils de nettoyage, acier bleuté
};
export const RACK_SKINS: Record<string, RackSkin> = {
  opto:      { tex: "alu-brosse", knob: "noir", ink: "dark", vu: true },                                  // optique : alu brossé, 2 gros boutons, vumètre
  fet:       { tex: "v3-metal-noir", knob: "chrome", ink: "light", vu: true },                           // FET : métal noir, boutons chromés (v0.36 : plus de grain)
  bus:       { tex: "v3-metal-noir", knob: "chrome", ink: "light", vu: true },
  comp:      { tex: "alu-sombre", knob: "noir", ink: "light", vu: true },
  limiter:   { tex: "alu-sombre", knob: "noir", ink: "light", vu: true },
  gate:      { tex: "alu-brosse", tint: "oklch(0.72 0.01 250)", knob: "noir", ink: "dark", vu: true },
  mband:     { tex: "v3-metal-noir", knob: "chrome", ink: "light", vu: true },
  varimu:    { tex: "alu-brosse", tint: "oklch(0.66 0.015 230)", knob: "noir", ink: "dark", vu: true },    // à lampes : gris bleuté, vumètre
  tubeeq:    { tex: "alu-brosse", tint: "oklch(0.58 0.05 235)", knob: "noir", ink: "light" },             // égaliseur à lampes : bleu-gris, gros boutons
  consoleeq: { tex: "alu-brosse", tint: "oklch(0.42 0.006 250)", knob: "plastique", ink: "light" },
  eq:        { tex: "alu-sombre", knob: "chrome", ink: "light" },
  master:    { tex: "v3-metal-noir", knob: "chrome", ink: "light", vu: true },
  dist:      { tex: "v3-metal-noir", knob: "chrome", ink: "light" },
  delay:     { tex: "alu-orange", knob: "noir", ink: "dark", lcd: "time|division" },                      // l'Écho de ton pack
  atmos:     { tex: "alu-brosse", tint: "oklch(0.3 0.04 285)", knob: "chrome", ink: "light", lcd: "decay" },
  vintverb:  { tex: "v3-metal-noir", knob: "chrome", ink: "light", lcd: "decay" },
  stepfx:    { tex: "alu-sombre", knob: "noir", ink: "light" },
  distcomp:  { tex: "v3-metal-noir", knob: "chrome", ink: "light", vu: true },                        // v0.38 : compresseur à distorsion
  prestige:  { tex: "alu-brosse", tint: "oklch(0.36 0.02 250)", knob: "chrome", ink: "light", lcd: "decay" }, // réverbération de prestige
};
export function rackSkin(type: string, family: string): RackSkin {
  return RACK_SKINS[type] ?? RACK_FAMILY[family] ?? { tex: "alu-sombre", knob: "noir", ink: "light" };
}

const INK = {
  light: { ink: "oklch(0.95 0.012 85)", dim: "oklch(0.95 0.012 85 / 0.72)", line: "oklch(0.95 0.012 85 / 0.3)", grad: "skin/sk-grad-blanche.svg", halo: "oklch(0.1 0.01 60 / 0.9)" },
  dark:  { ink: "oklch(0.17 0.012 60)", dim: "oklch(0.17 0.012 60 / 0.75)", line: "oklch(0.17 0.012 60 / 0.4)", grad: "skin/sk-grad-noire.svg", halo: "oklch(0.97 0.01 85 / 0.85)" },
};

/** Variables CSS d'un bouton. */
export function knobVars(k: KnobName): Record<string, string> {
  const d = KNOBS[k];
  return { "--sk-knob": img(d.body), "--sk-mark": img(d.mark), "--sk-ax": `${d.ax}%`, "--sk-ay": `${d.ay}%`, "--sk-ms": String(d.ms) };
}
function inkVars(ink: "light" | "dark"): Record<string, string> {
  const i = INK[ink];
  return { "--sk-ink": i.ink, "--sk-dim": i.dim, "--sk-line": i.line, "--sk-grad": `url("${abs(i.grad)}")`, "--sk-halo": i.halo };
}
function texVars(t: Texture, tint: string | undefined, prefix: string): Record<string, string> {
  return { [prefix]: img(`t-${t}`), [`${prefix}-size`]: `${TEXTURES[t]}px auto`, [`${prefix}-tint`]: tint ?? "transparent" };
}

/** Variables d'une façade d'instrument (thème de façade ; « generic » sans disposition). */
export function faceSkinStyle(theme: string): { skin: FaceSkin; style: CSSProperties } {
  const s = FACE_SKINS[theme] ?? FACE_SKINS.generic;
  const v: Record<string, string> = {
    ...texVars(s.tex, s.tint, "--sk-panel"), "--sk-vis": img("vis"), "--sk-molette": img("molette"),
    ...knobVars(s.knob), ...inkVars(s.ink), "--sk-fader": img(s.fader ?? "curseur"),
  };
  if (s.cheek) Object.assign(v, texVars(s.cheek, undefined, "--sk-cheek"));
  return { skin: s, style: v as CSSProperties };
}

/** Style du bouton n° i d'une pédale (rien : celui de la pédale). */
export function pedalKnobStyle(type: string, i: number): CSSProperties | undefined {
  const p = PEDAL_SKINS[type];
  if (!p || !p.alt || !(p.altAt ?? []).includes(i)) return undefined;
  return knobVars(p.alt) as CSSProperties;
}

/** Une pédale (fond illustré) : variables et format ; null si l'effet n'a pas de pédale (→ rack). */
export function pedalSkin(type: string): { tall: boolean; style: CSSProperties } | null {
  const p = PEDAL_SKINS[type];
  if (!p) return null;
  return { tall: !!p.tall, style: { "--pd-bg": `url("${abs(`skin/sk-pd-${p.bg}.webp`)}")`, "--sk-foot": img("interrupteur-pied"), "--sk-vis": img("vis"), ...knobVars(p.knob), ...inkVars(p.ink) } as CSSProperties };
}

/** Un rack : variables de la façade et des oreilles. */
export function rackSkinStyle(type: string, family: string): { skin: RackSkin; style: CSSProperties } {
  const r = rackSkin(type, family);
  return { skin: r, style: { ...texVars(r.tex, r.tint, "--sk-panel"), ...texVars("alu-sombre", undefined, "--sk-ear"), "--sk-vis": img("vis"), ...knobVars(r.knob), ...inkVars(r.ink) } as CSSProperties };
}

/** Modules (v0.35.2) : effets à interface dédiée (trop de réglages pour un rack). Bloc des maquettes du
    pack : matière, bouton, couleur d'accent (filet du haut, afficheurs, diodes). */
export interface ModuleSkin { tex: Texture; tint?: string; knob: KnobName; accent: string }
export const MODULE_SKINS: Record<string, ModuleSkin> = {
  dist:   { tex: "alu-sombre", tint: "oklch(0.42 0.012 160)", knob: "chrome", accent: "#ff5a2a" },   // Furnace : acier sombre et chrome
  sculptor: { tex: "alu-sombre", tint: "oklch(0.42 0.012 200)", knob: "noir", accent: "#52d6e8" },  // Sculptor : aluminium anthracite, cyan
  grinder: { tex: "v3-metal-noir", knob: "chrome", accent: "#ff2e63" }, // Grinder : métal noir, accent framboise
  master: { tex: "v3-metal-noir", knob: "noir", accent: "#e9b949" },                                  // Varnish : métal sombre et laiton
  eq:     { tex: "alu-sombre", tint: "oklch(0.4 0.01 210)", knob: "noir", accent: "#22c7dd" },       // Prism : aluminium anthracite
  mband:  { tex: "v3-metal-noir", knob: "chrome", accent: "#f2a33a" },                                // Quartet : métal noir, ambre
  stepfx: { tex: "alu-sombre", tint: "oklch(0.5 0.01 140)", knob: "poly", accent: "#9be15d" },                                 // Hopscotch : aluminium anthracite
  atmos:  { tex: "alu-brosse", tint: "oklch(0.3 0.04 285)", knob: "chrome", accent: "#b39bff" },     // Atmosphere : alu violet profond
  // 1.1 : famille « Golem Titan FX » : métal noir, boutons chromés, filet or (la gamme haut de gamme)
  ...Object.fromEntries(["tvintage", "tspace", "tlimit", "techo", "tdecap", "tsaturn", "tphaser", "tflange", "tfilter"]
    .map((t) => [t, { tex: "v3-metal-noir" as Texture, knob: "chrome" as KnobName, accent: "#d9b45a" }])),
};
export function moduleSkinStyle(type: string): { skin: ModuleSkin; style: CSSProperties } {
  const s = MODULE_SKINS[type] ?? { tex: "alu-sombre" as Texture, knob: "noir" as KnobName, accent: "#e9b949" };
  return { skin: s, style: { ...texVars(s.tex, s.tint, "--sk-panel"), "--sk-vis": img("vis"), ...knobVars(s.knob), ...inkVars("light"), "--mp-acc": s.accent } as CSSProperties };
}

/** 1.0 : FM Player — fonds façon pédales (couleur laquée, matière brossée, emblème d'une onde modulée
    en fréquence), au choix par un petit bouton ; « sobre » : le fond d'origine, sans emblème. */
export const FM1_FONDS = {
  ambre:    { c1: "oklch(0.6 0.15 58)",   c2: "oklch(0.32 0.11 42)",  emb: "creme", swatch: "oklch(0.66 0.16 58)",  label: "Ambre" },
  nuit:     { c1: "oklch(0.38 0.15 285)", c2: "oklch(0.16 0.08 280)", emb: "onde",  swatch: "oklch(0.48 0.17 285)", label: "Nuit" },
  emeraude: { c1: "oklch(0.44 0.09 165)", c2: "oklch(0.18 0.05 170)", emb: "or",    swatch: "oklch(0.52 0.1 165)",  label: "Émeraude" },
  sobre:    { c1: "oklch(0.24 0.01 250)", c2: "oklch(0.13 0.006 250)", emb: "",     swatch: "oklch(0.3 0.01 250)",  label: "Sobre" },
} as const;
export type Fm1Fond = keyof typeof FM1_FONDS;
export function fm1FondStyle(f: Fm1Fond): CSSProperties {
  const d = FM1_FONDS[f] ?? FM1_FONDS.ambre;
  const emb = (pale: boolean) => (d.emb ? `url("${abs(`skin/sk-fm1-embleme-${d.emb}${pale ? "-pale" : ""}.svg`)}")` : "none");
  return { "--fm1-c1": d.c1, "--fm1-c2": d.c2, "--fm1-emb": emb(false), "--fm1-emb-pale": emb(true), "--fm1-tex": img("t-alu-brosse") } as CSSProperties;
}
// 1.1 : un fond PAR PISTE (plusieurs FM Player, chacun sa couleur), gardé sur l'appareil :
// clé locale « grv.fm1Fond.t<piste> » ; une piste jamais réglée prend le dernier fond choisi
// (« grv.fm1Fond », ambre au départ). Partagé par la façade et le mode Grand de la même piste.
const FOND_KEY = "grv.fm1Fond";
const readFond = (k: string): Fm1Fond | null => { try { const v = localStorage.getItem(k); if (v && v in FM1_FONDS) return v as Fm1Fond; } catch { /* stockage absent */ } return null; };
let fondLast: Fm1Fond = readFond(FOND_KEY) ?? "ambre";
const fondByTrack = new Map<number, Fm1Fond>();
const fondListeners = new Set<() => void>();
export function fm1Fond(track: number): Fm1Fond {
  let f = fondByTrack.get(track);
  if (!f) { f = readFond(`${FOND_KEY}.t${track}`) ?? fondLast; fondByTrack.set(track, f); }
  return f;
}
export function setFm1Fond(f: Fm1Fond, track: number) {
  fondByTrack.set(track, f);
  fondLast = f;
  try { localStorage.setItem(`${FOND_KEY}.t${track}`, f); localStorage.setItem(FOND_KEY, f); } catch { /* stockage absent */ }
  fondListeners.forEach((l) => l());
}
export function useFm1Fond(track: number): Fm1Fond {
  return useSyncExternalStore((l) => { fondListeners.add(l); return () => { fondListeners.delete(l); }; }, () => fm1Fond(track));
}
