// Groovebox — OFAD — AGPL-3.0-or-later
// Modules (v0.35.2) : les effets qui ont trop de réglages pour une pédale ou un rack (distorsion à deux
// moteurs, mastering, égaliseur 8 bandes, multibande, effets à pas, réverbération à 12 algorithmes)
// s'ouvrent dans une interface dédiée, sur le modèle des blocs des maquettes du pack : bandeau (nom,
// afficheur, diodes), grand écran, sous-panneaux encastrés avec TOUS les réglages visibles, boutons
// réels, sélecteurs ◀ ▶ sur afficheur, boutons à diode. Dessin original (aucune façade reprise).
// Échap ou ✕ : refermer.

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import type { CSSProperties, ReactNode } from "react";
import { actions, useStore } from "../engine/store";
import type { ModuleDesc, ParamDesc } from "../engine/protocol";
import { toPlain } from "../engine/params";
import { paramAddress, useParam } from "../controls/bindings";
import { Encoder, ParamMenuZone } from "../controls/Encoder";
import { useBypass } from "../controls/useBypass";
import { PresetPanel } from "../controls/PresetPanel";
import { moduleSkinStyle } from "../skin/skins";
import { DistCurve, eqBandDb } from "./Visuals";
import type { EqBand } from "./Visuals";

type Area = "fx" | "pfx";
type Size = "xl" | "lg" | "md" | "sm";

/** Commande : identifiant du réglage, libellé (sinon celui du réglage, sans le préfixe « Bande · »),
    taille du bouton, libellés courts d'un sélecteur à diodes. */
interface Ctl { id: string; label?: string; size?: Size; short?: string[]; as?: "lcd" | "seg" | "led" | "solo" }
type C = string | Ctl;
/** Sous-panneau : titre, diode de mise en marche, commandes, colonnes de boutons, têtes de colonnes,
    écran intégré, largeur. relevant : réglages utiles seulement pour certaines valeurs d'un sélecteur. */
interface Group {
  title: string; on?: C[]; head?: C[]; ctl: C[]; cols?: number; heads?: string[]; size?: Size; w?: number; grow?: boolean;
  screen?: ScreenKind; relevant?: { by: string; map: Record<string, number[]> }; band?: number; selCols?: number;
  custom?: "palette" | "perso" | "steptools";
}
type ScreenKind = "dist" | "eq" | "mband" | "steps" | "atmos" | "mastereq" | "mastermeters" | "grinder" | "sculptor";
interface Layout {
  lcd?: C;                       // sélecteur sur l'afficheur du bandeau
  lcdText?: (v: Vals) => string; // ou texte de l'afficheur (mesure, préréglage)
  leds?: C[];                    // diodes du bandeau
  screen?: ScreenKind; screenH?: number; firstBand?: number; // bloc choisi à l'ouverture
  side?: Group[]; sideW?: number;
  rows: Group[][];
}
interface Vals { plain: (id: string) => number; meters: number[]; preset?: string }

const ATMOS_REL = { by: "mode", map: { rise: [7], swelldry: [7], vowel: [8], formant: [8], tension: [6], spacing: [9], repeats: [9], shape: [10], early: [5, 11] } };

const bandStrip = (b: number): Group => ({
  title: `Bande ${b}`, band: b - 1, on: [{ id: `b${b}_on`, label: "" }], w: 165,
  ctl: [{ id: `b${b}_type`, as: "lcd" }, { id: `b${b}_freq`, label: "Fréq." }, `b${b}_gain`, { id: `b${b}_q`, label: "Q" },
    { id: `b${b}_range`, label: "Dynam.", size: "sm" }, { id: `b${b}_thr`, size: "sm" }],
});
const mbandStrip = (b: number, name: string): Group => ({
  title: name, w: 224, head: [{ id: `b${b}solo`, as: "solo" }],
  ctl: [{ id: `b${b}mode`, as: "seg", short: ["Compr.", "Expans."] }, `b${b}thr`, `b${b}rat`, `b${b}gain`, { id: `b${b}att`, size: "sm", label: "Attaque" }, { id: `b${b}rel`, size: "sm", label: "Relâch." }],
});
const floatStrip = (f: number): Group => ({
  title: `Flottante ${f}`, w: 224, on: [{ id: `f${f}on`, label: "" }],
  ctl: [{ id: `f${f}freq`, label: "Fréq." }, { id: `f${f}q`, label: "Q" }, `f${f}range`, { id: `f${f}thr`, size: "sm" }, { id: `f${f}att`, size: "sm", label: "Attaque" }, { id: `f${f}rel`, size: "sm", label: "Relâch." }],
});

/** Modulateur interne (v0.36) : type, destination, forme, division ; vitesse et quantité. */
const modStrip = (k: number, w?: number): Group => ({
  title: `Modulation ${k}`, grow: !w, w, selCols: 2,
  ctl: [{ id: `m${k}type`, as: "lcd", label: "Type" }, { id: `m${k}dest`, as: "lcd", label: "Destination" }, { id: `m${k}shape`, as: "lcd", label: "Forme" },
    { id: `m${k}div`, as: "lcd", label: "Division" }, { id: `m${k}rate`, label: "Vitesse" }, { id: `m${k}amt`, label: "Quantité" }], cols: 2,
});
const grinderBand = (b: number, name: string): Group => ({
  title: name, grow: true, relevant: { by: "bands", map: b === 3 ? Object.fromEntries(["type", "drive", "bias", "gate", "dyn", "fb", "fbfreq", "level", "pan"].map((k) => [`b3${k}`, [1, 2]]))
    : b === 4 ? Object.fromEntries(["type", "drive", "bias", "gate", "dyn", "fb", "fbfreq", "level", "pan"].map((k) => [`b4${k}`, [2]])) : {} },
  ctl: [{ id: `b${b}type`, as: "lcd", label: "Algorithme" }, { id: `b${b}drive`, label: "Drive" }, { id: `b${b}bias`, label: "Asymétrie" }, { id: `b${b}level`, label: "Niveau" },
    { id: `b${b}gate`, label: "Porte", size: "sm" }, { id: `b${b}dyn`, label: "Dynamique", size: "sm" }, { id: `b${b}pan`, label: "Pan", size: "sm" },
    { id: `b${b}fb`, label: "Réinjection", size: "sm" }, { id: `b${b}fbfreq`, label: "Accord", size: "sm" }], cols: 3,
});

/** Sculptor (v0.37) : un effet à courbe (forme, longueur, bande, quantité) et ses réglages propres. */
const SHAPERS = ["Temps", "Saturation", "Broyage", "Filtre", "Liquide", "Bruit", "Volume", "Panoramique", "Largeur"];
const shaperStrip = (k: number): Group => {
  const extra: C[] = k === 1 ? [{ id: "trange", as: "lcd", label: "Portée" }] : k === 4 ? [{ id: "ftype", as: "lcd", label: "Type" }, { id: "freso", label: "Résonance", size: "sm" }]
    : k === 5 ? [{ id: "lmode", as: "seg" }] : [];
  return {
    title: SHAPERS[k - 1], band: k - 1, grow: true, on: [{ id: `s${k}on`, label: "" }], selCols: 2,
    ctl: [{ id: `s${k}shape`, as: "lcd", label: "Forme" }, { id: `s${k}len`, as: "lcd", label: "Longueur" }, { id: `s${k}band`, as: "lcd", label: "Bande" },
      ...extra, { id: `s${k}amt`, label: "Quantité", size: "lg" }], cols: 2,
  };
};

const LAYOUTS: Record<string, Layout> = {
  // Furnace (principe Dist COLDFIRE) : deux moteurs, filtres propres à chaque moteur (sur le son distordu
  // seulement), dynamique, réinjection, 4 modulateurs internes (v0.36)
  dist: {
    lcd: { id: "routing", label: "Routage" }, screen: "dist", screenH: 240, sideW: 360,
    side: [
      { title: "Entrée / sortie", ctl: ["mix", "out", { id: "balance", label: "Équilibre A/B" }] },
      { title: "Réinjection", ctl: [{ id: "fbPitch", as: "lcd" }, { id: "feedback", label: "Quantité" }, { id: "fbTime", label: "Temps" }], cols: 2 },
    ],
    rows: [[
      { title: "Moteur A", ctl: [{ id: "typeA", as: "lcd" }, { id: "driveA", label: "Drive", size: "lg" }, { id: "toneA", label: "Tonalité" }, { id: "biasA", label: "Asymétrie" }, { id: "mixA", label: "Mélange" }], cols: 2, w: 236 },
      { title: "Filtres A · son distordu", ctl: [{ id: "preType", as: "lcd", label: "Entrée" }, { id: "preFreq", label: "Fréquence" }, { id: "preReso", label: "Résonance" },
        { id: "postHpA", label: "Passe-haut" }, { id: "postLpA", label: "Passe-bas" }], cols: 2, w: 236 },
      { title: "Moteur B", ctl: [{ id: "typeB", as: "lcd" }, { id: "driveB", label: "Drive", size: "lg" }, { id: "toneB", label: "Tonalité" }, { id: "biasB", label: "Asymétrie" }, { id: "mixB", label: "Mélange" }], cols: 2, w: 236 },
      { title: "Filtres B · son distordu", ctl: [{ id: "preTypeB", as: "lcd", label: "Entrée" }, { id: "preFreqB", label: "Fréquence" }, { id: "preResoB", label: "Résonance" },
        { id: "postHpB", label: "Passe-haut" }, { id: "postLpB", label: "Passe-bas" }], cols: 2, w: 236 },
      { title: "Dynamique", ctl: [{ id: "dynMode", as: "lcd" }, { id: "dynPos", short: ["Pré", "Post"] }, { id: "dynAmount", label: "Quantité" }], cols: 1, w: 190 },
      { title: "Sortie", ctl: [{ id: "postLow", label: "Graves" }, { id: "postHigh", label: "Aigus" }, { id: "xover", label: "Séparation" }], cols: 1, grow: true },
    ], [modStrip(1), modStrip(2), modStrip(3), modStrip(4)]],
  },
  // Sculptor (principe ShaperBox) : 9 effets pilotés chacun par sa courbe, sur tout le son ou une bande (v0.37)
  sculptor: {
    lcd: { id: "trigger", label: "Départ" }, firstBand: 6, screen: "sculptor", screenH: 240, sideW: 300,
    side: [{ title: "Commun", ctl: ["mix", "out", "smooth", { id: "xlow", label: "Sép. grave" }, { id: "xhigh", label: "Sép. aigu" }] }],
    rows: [[1, 2, 3, 4, 5].map(shaperStrip), [6, 7, 8, 9].map(shaperStrip)],
  },
  // Grinder (principe Ohmicide) : 2 à 4 bandes, chacune porte, dynamique, réinjection, distorsion (v0.36)
  grinder: {
    lcd: { id: "bands", label: "Bandes" }, screen: "grinder", screenH: 230, sideW: 330,
    side: [
      { title: "Entrée / sortie", ctl: ["input", "output", "mix"] },
      { title: "Séparations", ctl: [{ id: "x1", label: "Sép. 1" }, { id: "x2", label: "Sép. 2" }, { id: "x3", label: "Sép. 3" }] },
    ],
    rows: [[grinderBand(1, "Grave"), grinderBand(2, "Bas-médium"), grinderBand(3, "Haut-médium"), grinderBand(4, "Aigu")],
      [modStrip(1), modStrip(2), modStrip(3)]],
  },
  // Varnish : chaîne de mastering (égaliseur, dynamique 3 bandes, image, excitation, maximiseur)
  master: {
    lcdText: (v) => { const l = v.meters[6] ?? -100; return `${v.preset ? v.preset + " · " : ""}Sonie ${l > -70 ? l.toFixed(1).replace(".", ",") : "—"} LUFS · plafond ${v.plain("ceiling").toFixed(1).replace(".", ",")} dB`; },
    rows: [[
      { title: "Égaliseur", on: [{ id: "eqOn", label: "" }], screen: "mastereq", w: 330,
        ctl: [{ id: "lowCut", short: ["Non", "20", "30", "40"] }, "lowGain", "midFreq", "midGain", "airGain"], cols: 2, size: "lg" },
      { title: "Dynamique 3 bandes · image", on: [{ id: "dynOn", label: "" }], head: [{ id: "imgOn", label: "Image" }], w: 580, cols: 4, size: "lg",
        heads: ["Grave", "Médium", "Aigu", "Commun"],
        ctl: [{ id: "thrLow", label: "Seuil" }, { id: "thrMid", label: "Seuil" }, { id: "thrHigh", label: "Seuil" }, { id: "xLow", label: "Sép. grave" },
          { id: "ratLow", label: "Rapport" }, { id: "ratMid", label: "Rapport" }, { id: "ratHigh", label: "Rapport" }, { id: "xHigh", label: "Sép. aigu" },
          { id: "gainLow", label: "Niveau" }, { id: "gainMid", label: "Niveau" }, { id: "gainHigh", label: "Niveau" }, { id: "attack", label: "Attaque" },
          { id: "widthLow", label: "Largeur" }, { id: "widthMid", label: "Largeur" }, { id: "widthHigh", label: "Largeur" }, { id: "release", label: "Relâch." }] },
      { title: "Excitation", on: [{ id: "excOn", label: "" }], ctl: [{ id: "shine", label: "Éclat" }, { id: "warmth", label: "Chaleur" }], cols: 1, w: 160, size: "lg" },
      { title: "Maximiseur", on: [{ id: "maxOn", label: "" }], screen: "mastermeters", grow: true, cols: 2,
        ctl: [{ id: "drive", label: "Gain", size: "xl" }, "ceiling", "character", { id: "clipAmt", label: "Écrêtage" }, { id: "truePeak", as: "led" }, { id: "splitLow", as: "led" }] },
    ]],
  },
  // Prism : égaliseur 8 bandes (dynamique par bande)
  eq: {
    lcdText: (v) => {
      const on = [1, 2, 3, 4, 5, 6, 7, 8].filter((b) => v.plain(`b${b}_on`) > 0).length;
      const dyn = [1, 2, 3, 4, 5, 6, 7, 8].filter((b) => v.plain(`b${b}_on`) > 0 && Math.abs(v.plain(`b${b}_range`)) > 0.05).length;
      return `${v.preset ? v.preset + " · " : ""}${on} bande${on > 1 ? "s" : ""}${dyn ? ` · ${dyn} dynamique${dyn > 1 ? "s" : ""}${v.plain("spectral") > 0 ? " spectrale" + (dyn > 1 ? "s" : "") : ""}` : ""} · ${["stéréo", "mid", "side", "mid / side"][v.plain("channels")] ?? ""}`;
    },
    screen: "eq", screenH: 250, sideW: 250,
    side: [{ title: "Sortie", ctl: [{ id: "channels", as: "lcd" }, { id: "output", size: "lg" }], cols: 1 },
      { title: "Dynamique des bandes", ctl: [{ id: "spectral", as: "seg" }, { id: "dynatt", label: "Attaque" }, { id: "dynrel", label: "Relâch." }], cols: 2 }],
    rows: [[1, 2, 3, 4, 5, 6, 7, 8].map(bandStrip)],
  },
  // Quartet : compresseur 4 bandes + 2 bandes flottantes
  mband: {
    lcdText: (v) => `Réduction ${(v.meters[0] ?? 0) > 0.05 ? "−" + (v.meters[0] ?? 0).toFixed(1) : "0"} dB`,
    screen: "mband", screenH: 230, sideW: 330,
    side: [
      { title: "Séparations", ctl: [{ id: "x1", label: "Sép. 1" }, { id: "x2", label: "Sép. 2" }, { id: "x3", label: "Sép. 3" }] },
      { title: "Sortie", ctl: [{ id: "mix", label: "Mélange" }, { id: "out", label: "Sortie" }] },
    ],
    rows: [[mbandStrip(1, "Grave"), mbandStrip(2, "Bas-médium"), mbandStrip(3, "Haut-médium"), mbandStrip(4, "Aigu"), floatStrip(1), floatStrip(2)]],
  },
  // Hopscotch : 16 à 64 pas, un effet et son réglage par pas (v0.39)
  stepfx: {
    lcd: { id: "division", label: "Pas de" }, screen: "steps", screenH: 300, sideW: 300,
    side: [{ title: "Séquence", ctl: [{ id: "steps", size: "lg" }], cols: 1, custom: "steptools" }],
    rows: [[
      { title: "Effets des pas", ctl: [], grow: true, custom: "palette" },
      { title: "Réglages perso", ctl: [], w: 420, custom: "perso" },
    ], [
      { title: "Commun · Répète", ctl: [{ id: "repeat", as: "lcd" }], w: 190 },
      { title: "Filtre", ctl: ["cutoff", "reso"], size: "lg", w: 240 },
      { title: "Écrase", ctl: ["bits"], size: "lg", w: 130 },
      { title: "Écho", ctl: [{ id: "echotime", as: "lcd" }, "echofb"], size: "lg", w: 230 },
      { title: "Sortie", ctl: ["mix"], size: "lg", w: 130 },
    ]],
  },
  // Atmosphere : réverbération d'ambiance à 12 algorithmes
  atmos: {
    lcd: { id: "mode", label: "Algorithme" }, leds: [{ id: "freeze", label: "Gel" }, { id: "persist", label: "Traîne" }],
    screen: "atmos", screenH: 230, sideW: 400,
    side: [{ title: "Espace", ctl: [{ id: "decay", size: "lg" }, { id: "size", size: "lg" }, { id: "mix", size: "lg" }, "predelay", "diffusion", "width"] }],
    rows: [[
      { title: "Caractère de l'algorithme", relevant: ATMOS_REL, cols: 6, w: 640,
        ctl: [{ id: "vowel", as: "lcd" }, { id: "shape", as: "lcd" }, { id: "swelldry", as: "led" }, "rise", { id: "formant", label: "Voix" }, { id: "tension", label: "Tension" },
          { id: "spacing", label: "Écart" }, "repeats", { id: "early", label: "Réflexions" }] },
      { title: "Modulation · chatoiement", ctl: [{ id: "pitch", as: "lcd" }, "modulation", "shimmer"], cols: 2, w: 250 },
      { title: "Couleur", ctl: ["tone", "lowcut", "highcut"], grow: true },
    ]],
  },
};

export function hasModulePanel(type: string): boolean { return type in LAYOUTS; }

const norm = (c: C): Ctl => (typeof c === "string" ? { id: c } : c);
const shortLabel = (s: string) => s.split(" · ").pop() ?? s;
const isOnOff = (p: ParamDesc) => p.choices?.length === 2 && /^(non|arrêt)$/i.test(p.choices[0]);

/** Bouton (corps de la matière du module, repère qui tourne, valeur au survol). */
function MKnob({ g, label, size, dim }: { g: number; label?: string; size: Size; dim?: boolean }) {
  const b = useParam(g >= 0 ? g : null, label);
  return (
    <div className={`pd-knob mp-knob mp-k--${size}${dim ? " mp-dim" : ""}`}>
      <Encoder binding={b} variant="knob" />
      <span className="pd-lab">{b ? shortLabel(b.label) : ""}</span>
    </div>
  );
}

/** Sélecteur sur afficheur : ◀ valeur ▶ (listes longues : algorithmes, types de filtre). */
function MLcd({ g, label, wide, dim, prefix }: { g: number; label?: string; wide?: boolean; dim?: boolean; prefix?: string }) {
  const b = useParam(g >= 0 ? g : null);
  const n = b?.entry?.choices?.length ?? 0, i = Math.round(b?.entry?.plain ?? 0);
  const step = (d: number) => b?.entry?.pick(((i + d) % n + n) % n);
  return (
    <div className={`mp-lcd${wide ? " mp-lcd--wide" : ""}${dim ? " mp-dim" : ""}`}>
      {label !== "" && <span className="mp-lcd-lab">{label ?? (b ? shortLabel(b.label) : "")}</span>}
      <div className="mp-lcd-box">
        <button type="button" aria-label="Précédent" onClick={() => step(-1)}>◀</button>
        <span title={b?.text}>{prefix ? <small>{prefix} · </small> : null}{b?.text ?? ""}</span>
        <button type="button" aria-label="Suivant" onClick={() => step(1)}>▶</button>
      </div>
    </div>
  );
}

/** Boutons à diode : un par choix (listes courtes : coupe-bas 20/30/40, compression/expansion). */
function MSeg({ g, label, short, dim }: { g: number; label?: string; short?: string[]; dim?: boolean }) {
  const b = useParam(g >= 0 ? g : null);
  const ch = b?.entry?.choices ?? [], i = Math.round(b?.entry?.plain ?? 0);
  return (
    <div className={`mp-seg${dim ? " mp-dim" : ""}`} role="radiogroup" aria-label={b?.label}>
      {label && <span className="mp-seg-lab">{label}</span>}
      {ch.map((c, k) => (
        <button key={c} type="button" role="radio" aria-checked={k === i} className={`mp-lb${k === i ? " on" : ""}`} title={c} onClick={() => b?.entry?.pick(k)}>
          <i />{short?.[k] ?? c}</button>
      ))}
    </div>
  );
}

/** Bouton à diode marche / arrêt. */
function MLed({ g, label, dim }: { g: number; label?: string; dim?: boolean }) {
  const b = useParam(g >= 0 ? g : null);
  const on = (b?.entry?.plain ?? 0) > 0.5;
  const text = label ?? (b ? shortLabel(b.label) : "");
  return (
    <button type="button" className={`mp-lb${on ? " on" : ""}${text ? "" : " mp-lb--dot"}${dim ? " mp-dim" : ""}`} aria-pressed={on}
      title={`${b?.label ?? ""} : ${on ? "en marche" : "arrêt"}`} onClick={() => b?.entry?.pick(on ? 0 : 1)}><i />{text}</button>
  );
}

/** Écoute d'une bande (normale / seule / coupée) : deux boutons à diode S et M, comme sur une console. */
function MSolo({ g }: { g: number }) {
  const b = useParam(g >= 0 ? g : null);
  const v = Math.round(b?.entry?.plain ?? 0);
  return (
    <span className="mp-seg" role="group" aria-label={b?.label}>
      <button type="button" className={`mp-lb${v === 1 ? " on" : ""}`} aria-pressed={v === 1} title="Écouter la bande seule" onClick={() => b?.entry?.pick(v === 1 ? 0 : 1)}><i />S</button>
      <button type="button" className={`mp-lb${v === 2 ? " on" : ""}`} aria-pressed={v === 2} title="Couper la bande" onClick={() => b?.entry?.pick(v === 2 ? 0 : 2)}><i />M</button>
    </span>
  );
}

interface Ctx { m: ModuleDesc; addr: (id: string) => number; vals: Vals; channel: number; area: Area; slot: number }

/** v0.46 : boutons à diode, sélecteurs et afficheurs : clic droit (appui long) = menu du réglage
    (contrôleur MIDI, macros, modulation, valeur exacte), comme les boutons rotatifs. */
function MenuWrap({ g, children }: { g: number; children: ReactNode }) {
  const b = useParam(g >= 0 ? g : null);
  return <ParamMenuZone binding={b} className="pm-zone--mp">{children}</ParamMenuZone>;
}

function Control({ c, ctx, size, dim }: { c: Ctl; ctx: Ctx; size: Size; dim?: boolean }) {
  const p = ctx.m.params.find((x) => x.id === c.id);
  const g = ctx.addr(c.id);
  if (!p) return null;
  const n = p.choices?.length ?? 0;
  if (n > 1) {
    const kind = c.as ?? (isOnOff(p) ? "led" : n <= 4 && p.choices!.every((x) => x.length <= 9) ? "seg" : "lcd");
    if (kind === "solo") return <MenuWrap g={g}><MSolo g={g} /></MenuWrap>;
    if (kind === "led") return <MenuWrap g={g}><MLed g={g} label={c.label} dim={dim} /></MenuWrap>;
    if (kind === "seg") return <MenuWrap g={g}><MSeg g={g} label={c.label} short={c.short} dim={dim} /></MenuWrap>;
    return <MenuWrap g={g}><MLcd g={g} label={c.label} dim={dim} /></MenuWrap>;
  }
  return <MKnob g={g} label={c.label} size={c.size ?? size} dim={dim} />;
}

function GroupView({ gr, ctx, band, setBand }: { gr: Group; ctx: Ctx; band: number; setBand: (b: number) => void }) {
  const items = gr.ctl.map(norm);
  const kindOf = (c: Ctl) => { const p = ctx.m.params.find((x) => x.id === c.id); return !p ? "none" : (p.choices?.length ?? 0) > 1 ? (c.as === "led" || (!c.as && isOnOff(p)) ? "led" : "sel") : "knob"; };
  const dim = (c: Ctl) => !!gr.relevant && !!gr.relevant.map[c.id] && !gr.relevant.map[c.id].includes(Math.round(ctx.vals.plain(gr.relevant.by)));
  const size = gr.size ?? "md";
  const sels = items.filter((c) => kindOf(c) === "sel"), knobs = items.filter((c) => kindOf(c) === "knob"), leds = items.filter((c) => kindOf(c) === "led");
  const sel = gr.band !== undefined && gr.band === band;
  // v0.45 : un groupe à interrupteur éteint est grisé ; toucher un de ses réglages l'allume
  const onId = gr.on && gr.on.length === 1 ? norm(gr.on[0]).id : null;
  const onB = useParam(onId && ctx.addr(onId) >= 0 ? ctx.addr(onId) : null);
  const off = !!onB?.entry && Math.round(onB.entry.plain) === 0;
  return (
    <section className={`mp-sub${sel ? " sel" : ""}${gr.grow ? " mp-sub--grow" : ""}${off ? " is-off" : ""}`} style={gr.w ? { width: gr.w } : undefined}
      title={off ? `${gr.title} éteint : touchez un réglage (ou la lampe) pour l'allumer` : undefined}
      onPointerDownCapture={off ? (e) => { if (!(e.target as HTMLElement).closest("h4")) onB?.entry?.pick(1); } : undefined}
      onPointerDown={gr.band !== undefined ? () => setBand(gr.band!) : undefined}>
      <h4>
        {gr.on?.map((c) => <Control key={norm(c).id} c={norm(c)} ctx={ctx} size="sm" />)}
        <span className="mp-sub-title">{gr.title}</span>
        {gr.head && <span className="mp-sub-head">{gr.head.map((c) => <Control key={norm(c).id} c={norm(c)} ctx={ctx} size="sm" />)}</span>}
      </h4>
      {gr.screen && <Screen kind={gr.screen} ctx={ctx} band={band} setBand={setBand} />}
      {sels.length > 0 && <div className="mp-sels" style={gr.selCols ? { gridTemplateColumns: `repeat(${gr.selCols}, minmax(0, 1fr))` } : undefined}>{sels.map((c) => <Control key={c.id} c={c} ctx={ctx} size={size} dim={dim(c)} />)}</div>}
      {knobs.length > 0 && (
        <div className="mp-grid" style={{ "--cols": Math.min(gr.cols ?? 3, knobs.length) } as CSSProperties}>
          {gr.heads?.map((h) => <span key={h} className="mp-colhead">{h}</span>)}
          {knobs.map((c) => <Control key={c.id} c={c} ctx={ctx} size={size} dim={dim(c)} />)}
        </div>
      )}
      {leds.length > 0 && <div className="mp-leds">{leds.map((c) => <Control key={c.id} c={c} ctx={ctx} size={size} dim={dim(c)} />)}</div>}
      {gr.custom === "palette" && <StepPalette ctx={ctx} />}
      {gr.custom === "perso" && <StepPerso ctx={ctx} />}
      {gr.custom === "steptools" && <StepTools ctx={ctx} />}
    </section>
  );
}

// ——— écrans ———

const W = 1000, H = 250;
const fx = (f: number) => (Math.log10(Math.max(20, Math.min(20000, f)) / 20) / 3) * W;
const fOfX = (x: number) => 20 * Math.pow(1000, Math.max(0, Math.min(1, x / W)));

function Grid({ h = H, db = false, text = true }: { h?: number; db?: boolean; text?: boolean }) {
  return (
    <g className="mp-gridlines">
      {[50, 100, 200, 500, 1000, 2000, 5000, 10000].map((f) => <line key={f} x1={fx(f)} x2={fx(f)} y1={0} y2={h} />)}
      {db && [-12, 0, 12].map((d) => <line key={d} x1={0} x2={W} y1={h / 2 - (d / 24) * (h / 2 - 8)} y2={h / 2 - (d / 24) * (h / 2 - 8)} className={d === 0 ? "zero" : ""} />)}
      {text && [100, 1000, 10000].map((f) => <text key={f} x={fx(f) + 4} y={h - 6}>{f >= 1000 ? `${f / 1000} k` : f}</text>)}
    </g>
  );
}

/** Égaliseur : courbe, spectre de la sortie, points des bandes à faire glisser (fréquence, gain). */
function EqScreen({ ctx, band, setBand }: { ctx: Ctx; band: number; setBand: (b: number) => void }) {
  const { vals, addr, channel: ch, slot, area } = ctx;
  const freqB = [1, 2, 3, 4, 5, 6, 7, 8].map((b) => useParam(addr(`b${b}_freq`))); // eslint-disable-line react-hooks/rules-of-hooks
  const gainB = [1, 2, 3, 4, 5, 6, 7, 8].map((b) => useParam(addr(`b${b}_gain`))); // eslint-disable-line react-hooks/rules-of-hooks
  const bands: EqBand[] = Array.from({ length: 8 }, (_, b) => ({ on: vals.plain(`b${b + 1}_on`) > 0, type: vals.plain(`b${b + 1}_type`), freq: vals.plain(`b${b + 1}_freq`), gain: vals.plain(`b${b + 1}_gain`), q: vals.plain(`b${b + 1}_q`) }));
  const yOf = (db: number) => H / 2 - (Math.max(-24, Math.min(24, db)) / 24) * (H / 2 - 8);
  const pts: string[] = [];
  for (let i = 0; i <= 200; ++i) { const f = 20 * Math.pow(1000, i / 200); pts.push(`${(i / 200) * W},${yOf(bands.reduce((a, b) => a + eqBandDb(b, f), 0))}`); }
  const [spectrum, setSpectrum] = useState<number[] | undefined>(undefined);
  useEffect(() => {
    if (area !== "fx" || slot < 0) return;
    let alive = true;
    const id = window.setInterval(async () => { const sp = await actions.spectrum(ch, slot, 128); if (alive) setSpectrum(sp.length ? sp : undefined); }, 83);
    return () => { alive = false; window.clearInterval(id); };
  }, [area, ch, slot]);
  const svg = useRef<SVGSVGElement>(null);
  const drag = (b: number) => (e: React.PointerEvent) => {
    e.preventDefault(); e.stopPropagation(); setBand(b);
    const el = svg.current; if (!el) return;
    (e.target as Element).setPointerCapture(e.pointerId);
    const move = (ev: PointerEvent) => {
      const r = el.getBoundingClientRect();
      const x = ((ev.clientX - r.left) / r.width) * W, y = ((ev.clientY - r.top) / r.height) * H;
      freqB[b]?.entry?.set(Math.round(fOfX(x)));
      if (bands[b].type <= 2) gainB[b]?.entry?.set(Math.round(((H / 2 - y) / (H / 2 - 8)) * 24 * 10) / 10);
    };
    const up = () => { window.removeEventListener("pointermove", move); window.removeEventListener("pointerup", up); };
    window.addEventListener("pointermove", move); window.addEventListener("pointerup", up);
  };
  return (
    <svg ref={svg} viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" className="mp-svg">
      {spectrum && spectrum.length > 1 && <path className="mp-spectrum" d={`M0 ${H} ` + spectrum.map((db, i) => `L${((i + 0.5) / spectrum.length) * W} ${H - Math.max(0, Math.min(1, (db + 90) / 90)) * H}`).join(" ") + ` L${W} ${H} Z`} />}
      <Grid db />
      <path className="mp-fill" d={`M0 ${H / 2} L${pts.join(" L")} L${W} ${H / 2} Z`} />
      <polyline className="mp-curve" points={pts.join(" ")} />
      {bands.map((b, i) => {
        const rg = vals.plain(`b${i + 1}_range`);
        if (!b.on || b.type > 2 || Math.abs(rg) < 0.05) return null;
        const x = Math.min(W - 14, Math.max(14, fx(b.freq))), live = vals.meters[i];
        return <g key={`d${i}`} className="mp-dyn">
          <line x1={x} x2={x} y1={yOf(b.gain)} y2={yOf(b.gain + rg)} />
          <line x1={x - 9} x2={x + 9} y1={yOf(b.gain + rg)} y2={yOf(b.gain + rg)} />
          {live !== undefined && <circle cx={x} cy={yOf(live)} r={4} />}
        </g>;
      })}
      {bands.map((b, i) => (
        <g key={i} className={`mp-node${i === band ? " on" : ""}${b.on ? "" : " off"}`} onPointerDown={drag(i)} style={{ cursor: "grab" }}>
          <circle cx={Math.min(W - 14, Math.max(14, fx(b.freq)))} cy={yOf(b.type <= 2 ? b.gain : 0)} r={i === band ? 13 : 11} />
          <text x={Math.min(W - 14, Math.max(14, fx(b.freq)))} y={yOf(b.type <= 2 ? b.gain : 0) + 4.5}>{i + 1}</text>
        </g>
      ))}
    </svg>
  );
}

/** Multibande : les 4 bandes entre leurs séparations, réduction de gain de chacune, bandes flottantes. */
function MbandScreen({ ctx }: { ctx: Ctx }) {
  const { vals } = ctx;
  const xs = [vals.plain("x1"), vals.plain("x2"), vals.plain("x3")];
  const edges = [20, ...xs, 20000];
  const names = ["Grave", "Bas-médium", "Haut-médium", "Aigu"];
  const fmt = (f: number) => { const r = Math.round(f / 10) * 10; return r >= 1000 ? `${(r / 1000).toFixed(r >= 10000 ? 0 : 1).replace(".", ",")} k` : `${r}`; };
  return (
    <div className="mp-screen-in">
      <svg viewBox={`0 0 ${W} ${H - 30}`} preserveAspectRatio="none" className="mp-svg">
        <Grid h={H - 30} text={false} />
        {names.map((n, b) => {
          const gr = Math.max(0, vals.meters[3 + b] ?? 0), solo = vals.plain(`b${b + 1}solo`);
          const x0 = fx(edges[b]), x1 = fx(edges[b + 1]);
          return <g key={n} opacity={solo === 2 ? 0.3 : 1}>
            <rect x={x0 + 3} y={0} width={Math.max(0, x1 - x0 - 6)} height={Math.min(H - 30, (gr / 24) * (H - 30))} className="mp-gr" />
            <text x={(x0 + x1) / 2} y={(H - 30) / 2} className="mp-big-t">{gr > 0.05 ? `−${gr.toFixed(1)}` : "0,0"}</text>
            <text x={(x0 + x1) / 2} y={(H - 30) / 2 + 22} className="mp-small-t">dB · {vals.plain(`b${b + 1}mode`) > 0 ? "expansion" : "compression"}{solo === 1 ? " · seule" : ""}</text>
          </g>;
        })}
        {xs.map((f, i) => <g key={i}><line x1={fx(f)} x2={fx(f)} y1={14} y2={H - 30} className="mp-xover" /><circle cx={fx(f)} cy={10} r={6} className="mp-xdot" /></g>)}
        {[1, 2].map((k) => vals.plain(`f${k}on`) > 0 && (
          <g key={k} className="mp-float">
            <line x1={fx(vals.plain(`f${k}freq`))} x2={fx(vals.plain(`f${k}freq`))} y1={30} y2={H - 30} />
            <text x={fx(vals.plain(`f${k}freq`)) + 6} y={44}>F{k} · −{Math.max(0, vals.meters[6 + k] ?? 0).toFixed(1)} dB</text>
          </g>
        ))}
      </svg>
      <div className="mp-bands">{names.map((n, b) => <span key={n}>{n} · {fmt(edges[b])}–{fmt(edges[b + 1])}</span>)}</div>
    </div>
  );
}

/** Distorsion : courbes de transfert des deux moteurs et le routage. */
function DistScreen({ ctx }: { ctx: Ctx }) {
  const { vals, m } = ctx;
  const algo = m.params.find((p) => p.id === "typeA")?.choices ?? [];
  const routing = m.params.find((p) => p.id === "routing")?.choices?.[vals.plain("routing")] ?? "";
  return (
    <div className="mp-dist">
      {(["A", "B"] as const).map((e) => {
        const mix = vals.plain(`mix${e}`);
        return (
          <div key={e} className={`mp-dist-eng${mix <= 0 ? " off" : ""}`}>
            <span className="mp-dist-t"><b>{e}</b> {algo[vals.plain(`type${e}`)] ?? ""}<small>drive {Math.round(vals.plain(`drive${e}`))} % · {mix <= 0 ? "coupé" : `${Math.round(mix)} %`}</small></span>
            <DistCurve type={vals.plain(`type${e}`)} drive={vals.plain(`drive${e}`) / 100} bias={vals.plain(`bias${e}`) / 100 * 0.6} />
          </div>
        );
      })}
      <div className="mp-dist-route"><small>Routage</small><b>{routing}</b>
        <span className="mp-dist-bal"><i style={{ left: `${vals.plain("balance")}%` }} /></span><small>A · équilibre · B</small>
        <ModBars ctx={ctx} count={4} first={3} /></div>
    </div>
  );
}

/** Modulateurs internes en direct : une barre par emplacement (valeur et destination). */
function ModBars({ ctx, count, first }: { ctx: Ctx; count: number; first: number }) {
  const { vals, m } = ctx;
  const dests = m.params.find((p) => p.id === "m1dest")?.choices ?? [];
  return (
    <div className="mp-modbars">
      {Array.from({ length: count }, (_, k) => {
        const on = vals.plain(`m${k + 1}type`) > 0, v = vals.meters[first + k] ?? 0;
        return <span key={k} className={on ? "on" : ""} title={on ? dests[vals.plain(`m${k + 1}dest`)] : "Modulation à l'arrêt"}>
          <b>M{k + 1}</b><i><em style={{ left: `${50 + Math.max(-1, Math.min(1, v)) * 50}%` }} /></i><small>{on ? dests[vals.plain(`m${k + 1}dest`)] : "—"}</small>
        </span>;
      })}
    </div>
  );
}

/** Distorsion multibande : bandes entre leurs séparations, algorithme et niveau de chaque bande. */
function GrinderScreen({ ctx }: { ctx: Ctx }) {
  const { vals, m } = ctx;
  const nb = 2 + vals.plain("bands");
  const xs = [vals.plain("x1"), vals.plain("x2"), vals.plain("x3")].slice(0, nb - 1);
  const edges = [20, ...xs, 20000];
  const algos = m.params.find((p) => p.id === "b1type")?.choices ?? [];
  const names = ["Grave", "Bas-médium", "Haut-médium", "Aigu"];
  const fmt = (f: number) => { const r = Math.round(f / 10) * 10; return r >= 1000 ? `${(r / 1000).toFixed(r >= 10000 ? 0 : 1).replace(".", ",")} k` : `${r}`; };
  return (
    <div className="mp-screen-in">
      <svg viewBox={`0 0 ${W} ${H - 30}`} preserveAspectRatio="none" className="mp-svg">
        <Grid h={H - 30} text={false} />
        {Array.from({ length: nb }, (_, b) => {
          const x0 = fx(edges[b]), x1 = fx(edges[b + 1]);
          const lvl = Math.min(1, (vals.meters[b] ?? 0) * 1.6);
          const drive = vals.plain(`b${b + 1}drive`) / 100;
          return <g key={b}>
            <rect x={x0 + 3} y={(H - 30) * (1 - lvl)} width={Math.max(0, x1 - x0 - 6)} height={(H - 30) * lvl} className="mp-gr" />
            <text x={(x0 + x1) / 2} y={(H - 30) / 2} className="mp-big-t">{algos[vals.plain(`b${b + 1}type`)] ?? ""}</text>
            <text x={(x0 + x1) / 2} y={(H - 30) / 2 + 22} className="mp-small-t">drive {Math.round(drive * 100)} %{vals.plain(`b${b + 1}fb`) > 0 ? ` · réinj. ${Math.round(vals.plain(`b${b + 1}fbfreq`))} Hz` : ""}{vals.plain(`b${b + 1}gate`) > -79.5 ? " · porte" : ""}</text>
          </g>;
        })}
        {xs.map((f, i) => <g key={i}><line x1={fx(f)} x2={fx(f)} y1={14} y2={H - 30} className="mp-xover" /><circle cx={fx(f)} cy={10} r={6} className="mp-xdot" /></g>)}
      </svg>
      <div className="mp-bands">{Array.from({ length: nb }, (_, b) => <span key={b}>{names[b]} · {fmt(edges[b])}–{fmt(edges[b + 1])}</span>)}</div>
      <div className="mp-modbars-float"><ModBars ctx={ctx} count={3} first={4} /></div>
    </div>
  );
}

/** Formes du Sculptor (mêmes formules que le moteur, Sculptor.h). */
function sculptShape(s: number, t: number, k: number): number {
  t -= Math.floor(t);
  switch (s) {
    case 0: return 0.5 - 0.5 * Math.cos(2 * Math.PI * t);
    case 1: return 1 - Math.abs(2 * t - 1);
    case 2: return t;
    case 3: return 1 - t;
    case 4: return t < 0.5 ? 1 : 0;
    case 5: return Math.min(1, Math.pow(t / 0.6, 1.6));
    case 6: return 0.5 - 0.5 * Math.cos(Math.PI * Math.min(1, t / 0.85));
    case 7: { const u = t * 4 - Math.floor(t * 4); return u < 0.5 ? 1 : 0; }
    case 8: { const u = t * 8; return u < 2 || (u >= 3 && u < 5) || (u >= 6 && u < 7) ? 1 : 0; }
    case 9: return Math.min(1, Math.floor(t * 4) / 3);
    case 10: return Math.exp(-5 * (t * 2 - Math.floor(t * 2)));
    case 11: { const st = Math.floor(t * 16); return ((Math.sin((st + 1) * 12.9898 + k * 78.233) * 43758.5453) % 1 + 1) % 1; }
    case 12: return t * t;
    default: return 1 - t * t;
  }
}
const SHAPER_COLORS = ["#ffb35a", "#ff6b4a", "#c98bff", "#52d6e8", "#7fd6a6", "#d9d2c3", "#f2e05c", "#6fa8ff", "#ff7fbf"];

/** Sculptor : les courbes des effets en marche sur une mesure ; l'effet choisi en gras, sa valeur en direct. */
function SculptorScreen({ ctx, band }: { ctx: Ctx; band: number }) {
  const { vals } = ctx;
  const lens = [0.25, 0.5, 1, 2, 4, 8, 16];
  const h = H - 20;
  return (
    <div className="mp-screen-in">
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" className="mp-svg">
        <g className="mp-gridlines">{Array.from({ length: 17 }, (_, i) => <line key={i} x1={(i / 16) * W} x2={(i / 16) * W} y1={0} y2={h} className={i % 4 === 0 ? "zero" : ""} />)}
          {[1, 2, 3, 4].map((b) => <text key={b} x={((b - 1) / 4) * W + 6} y={h + 15}>temps {b}</text>)}</g>
        {SHAPERS.map((_, k) => {
          const on = vals.plain(`s${k + 1}on`) > 0;
          if (!on && k !== band) return null;
          const shape = vals.plain(`s${k + 1}shape`), len = lens[vals.plain(`s${k + 1}len`)] ?? 1, amt = vals.plain(`s${k + 1}amt`) / 100;
          const pts: string[] = [];
          for (let i = 0; i <= 400; ++i) { const q = (i / 400) * 4; const v = sculptShape(shape, q / len, k); const y = 1 - amt * (1 - v); pts.push(`${(i / 400) * W},${8 + (1 - y) * (h - 16)}`); }
          return <polyline key={k} points={pts.join(" ")} fill="none" stroke={SHAPER_COLORS[k]} strokeWidth={k === band ? 3 : 1.4} vectorEffect="non-scaling-stroke" opacity={on ? (k === band ? 1 : 0.55) : 0.25} strokeDasharray={on ? undefined : "5 5"} />;
        })}
      </svg>
      <div className="mp-atmos-tag"><b style={{ color: SHAPER_COLORS[band] }}>{SHAPERS[band] ?? ""}</b>{vals.plain(`s${band + 1}on`) > 0 ? "" : " · à l'arrêt"} · courbe sur une mesure (1 = repos)</div>
      <div className="mp-shaper-live">{SHAPERS.map((n, k) => vals.plain(`s${k + 1}on`) > 0 && (
        <span key={n} style={{ "--c": SHAPER_COLORS[k] } as CSSProperties}><i><b style={{ height: `${Math.round((vals.meters[k] ?? 0) * 100)}%` }} /></i><small>{n.slice(0, 3)}</small></span>))}</div>
    </div>
  );
}

/** Réverbération : enveloppe de la queue (pré-délai, premières réflexions, déclin), selon l'algorithme. */
function AtmosScreen({ ctx }: { ctx: Ctx }) {
  const { vals, m } = ctx;
  const mode = vals.plain("mode"), decay = vals.plain("decay"), pre = vals.plain("predelay") / 1000;
  const name = m.params.find((p) => p.id === "mode")?.choices?.[mode] ?? "";
  const frozen = vals.plain("freeze") > 0;
  const T = Math.max(1.2, Math.min(12, pre + decay * 1.15)), h = H - 20;
  const tx = (t: number) => 20 + (t / T) * (W - 40);
  const pts: string[] = [];
  for (let i = 0; i <= 200; ++i) {
    const t = (i / 200) * T, u = t - pre;
    let a = u < 0 ? 0 : frozen ? 1 : Math.pow(10, (-3 * u) / Math.max(0.05, decay));
    if (u >= 0 && mode === 7) a *= Math.min(1, u / Math.max(0.02, vals.plain("rise") / 1000));             // gonflement
    if (u >= 0 && mode === 10) { const L = Math.min(3, Math.max(0.08, decay)), s = vals.plain("shape"); a = u > L ? 0 : s === 1 ? u / L : s === 2 ? Math.min(1, u / (L * 0.8)) : s === 3 ? 1 - u / L : 1; }
    if (u >= 0 && mode === 9) a *= 0.75 + 0.25 * Math.cos((u * 1000 / Math.max(30, vals.plain("spacing"))) * 2 * Math.PI);
    pts.push(`${tx(t)},${h - a * (h - 24)}`);
  }
  const er = mode === 5 || mode === 11 ? [3.1, 4.7, 7.9, 9.3, 12.7, 15.1, 19.3, 23.9, 29.1, 33.7, 41.3, 47.9].map((ms, i) => ({ t: pre + (ms * (0.5 + 1.5 * vals.plain("size") / 100) * 2) / 1000, g: [0.9, 0.85, 0.75, 0.7, 0.62, 0.58, 0.5, 0.45, 0.38, 0.33, 0.27, 0.22][i] * vals.plain("early") / 100 })) : [];
  return (
    <div className="mp-screen-in">
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" className="mp-svg">
        <g className="mp-gridlines">{Array.from({ length: Math.floor(T) + 1 }, (_, s) => <g key={s}><line x1={tx(s)} x2={tx(s)} y1={0} y2={h} /><text x={tx(s) + 4} y={h + 14}>{s} s</text></g>)}</g>
        {pre > 0 && <rect x={tx(0)} y={0} width={tx(pre) - tx(0)} height={h} className="mp-pre" />}
        <path className="mp-fill" d={`M${tx(0)} ${h} L${pts.join(" L")} L${tx(T)} ${h} Z`} />
        <polyline className="mp-curve" points={pts.join(" ")} />
        {er.map((e, i) => <line key={i} x1={tx(e.t)} x2={tx(e.t)} y1={h} y2={h - e.g * (h - 30)} className="mp-er" />)}
      </svg>
      <div className="mp-atmos-tag"><b>{name}</b>{frozen ? " · gel" : ""}{vals.plain("persist") > 0 ? " · traîne" : ""}{mode === 8 ? ` · voyelle ${m.params.find((p) => p.id === "vowel")?.choices?.[vals.plain("vowel")] ?? ""}` : ""}</div>
      <div className="mp-seg7"><span>{frozen ? "INF" : decay < 10 ? decay.toFixed(1) : Math.round(decay)}</span><small>déclin · s</small></div>
    </div>
  );
}

/** Égaliseur du mastering : coupe-bas, plateau grave, cloche des médiums, air. */
function MasterEqScreen({ ctx }: { ctx: Ctx }) {
  const { vals } = ctx;
  const lc = [0, 20, 30, 40][vals.plain("lowCut")] ?? 0, on = vals.plain("eqOn") > 0;
  const db = (f: number) => {
    if (!on) return 0;
    let d = (lc ? -10 * Math.log10(1 + Math.pow(lc / f, 4)) : 0) + vals.plain("lowGain") / (1 + Math.pow(f / 110, 2.2)) + vals.plain("airGain") / (1 + Math.pow(11000 / f, 2.2));
    d += vals.plain("midGain") * Math.exp(-Math.pow(Math.log2(f / Math.max(20, vals.plain("midFreq"))) / 1.1, 2));
    return d;
  };
  const h = 120, y = (d: number) => h / 2 - (Math.max(-12, Math.min(12, d)) / 12) * (h / 2 - 6);
  const pts: string[] = [];
  for (let i = 0; i <= 120; ++i) { const f = 20 * Math.pow(1000, i / 120); pts.push(`${(i / 120) * W},${y(db(f))}`); }
  return (
    <svg viewBox={`0 0 ${W} ${h}`} preserveAspectRatio="none" className="mp-svg mp-svg--mini">
      <g className="mp-gridlines">{[100, 1000, 10000].map((f) => <line key={f} x1={fx(f)} x2={fx(f)} y1={0} y2={h} />)}<line x1={0} x2={W} y1={h / 2} y2={h / 2} className="zero" /></g>
      <path className="mp-fill" d={`M0 ${h / 2} L${pts.join(" L")} L${W} ${h / 2} Z`} /><polyline className="mp-curve" points={pts.join(" ")} />
    </svg>
  );
}

/** Mesures du mastering : sonie (afficheur 7 segments), réductions de gain par bande, maximiseur, écrêtage. */
function MasterMetersScreen({ ctx }: { ctx: Ctx }) {
  const mt = ctx.vals.meters;
  const lufs = mt[6] ?? -100;
  const bars: [string, number][] = [["G", mt[3] ?? 0], ["M", mt[4] ?? 0], ["A", mt[5] ?? 0], ["Max", mt[0] ?? 0], ["Écr.", mt[7] ?? 0]];
  return (
    <div className="mp-mmeters">
      <div className="mp-seg7 mp-seg7--in"><small>sonie</small><span>{lufs > -70 ? lufs.toFixed(1) : "--.-"}</span><small>LUFS · plafond {ctx.vals.plain("ceiling").toFixed(1)}</small></div>
      <div className="mp-vbars">{bars.map(([n, d]) => (
        <div key={n} className="mp-vbar"><span className="mp-vbar-t"><i style={{ height: `${Math.min(100, (Math.max(0, d) / 12) * 100)}%` }} /></span><small>{n}</small></div>
      ))}</div>
      <small className="mp-vbars-cap">réduction de gain (0 à 12 dB)</small>
    </div>
  );
}

// ——— Hopscotch (v0.39) : 64 pas, un effet et son réglage par pas (principe Looperator) ———
/** Couleurs par genre d'effet (0 = rien … 9 = volume ; 10 = réglage perso sans genre). */
export const STEP_COLORS = ["transparent", "oklch(0.75 0.16 30)", "oklch(0.72 0.14 280)", "oklch(0.78 0.14 85)", "oklch(0.72 0.16 145)",
  "oklch(0.7 0.12 230)", "oklch(0.75 0.17 0)", "oklch(0.78 0.12 190)", "oklch(0.74 0.13 320)", "oklch(0.8 0.1 110)", "oklch(0.8 0.02 250)"];
const STEP_TYPES = ["—", "Répète", "Inverse", "Filtre", "Écrase", "Arrêt bande", "Hachure", "Écho", "Ralenti", "Volume"];
const STEP_ABBR = ["", "Rép", "Inv", "Flt", "Écr", "Arr", "Hach", "Écho", "Ral", "Vol"];
/** Réglages A à D de chaque genre (mêmes formules que le moteur, StepFx.h : persoPlain). */
const STEP_SETTINGS: { n: string; f: (u: number) => string }[][] = [
  [],
  [{ n: "Tranche", f: (u) => ["1/64", "1/32", "1/16", "1/8", "1/4"][Math.round(u * 4)] }, { n: "Hauteur", f: (u) => `${Math.round(-12 + 24 * u) > 0 ? "+" : ""}${Math.round(-12 + 24 * u)} dt` },
    { n: "Déclin", f: (u) => `${Math.round(100 * u)} %` }, { n: "Porte", f: (u) => `${Math.round(25 + 75 * u)} %` }],
  [{ n: "Vitesse", f: (u) => `×${(0.5 * Math.pow(4, u)).toFixed(2).replace(".", ",")}` }, { n: "Déclin", f: (u) => `${Math.round(100 * u)} %` }],
  [{ n: "Coupure", f: (u) => { const hz = 80 * Math.pow(150, u); return hz >= 1000 ? `${(hz / 1000).toFixed(1).replace(".", ",")} kHz` : `${Math.round(hz)} Hz`; } },
    { n: "Résonance", f: (u) => `${Math.round(100 * u)} %` }, { n: "Type", f: (u) => ["Passe-bas", "Passe-haut", "Passe-bande"][Math.round(u * 2)] },
    { n: "Balayage", f: (u) => `${(-4 + 8 * u) > 0 ? "+" : ""}${(-4 + 8 * u).toFixed(1).replace(".", ",")} oct.` }],
  [{ n: "Bits", f: (u) => `${3 + Math.round(9 * u)} bits` }, { n: "Échantillonnage", f: (u) => `÷${1 + Math.round(31 * u)}` }, { n: "Saturation", f: (u) => `${Math.round(24 * u)} dB` }],
  [{ n: "Durée", f: (u) => `${Math.round((0.25 + 0.75 * u) * 100)} % du pas` }, { n: "Courbe", f: (u) => (u < 0.34 ? "douce" : u < 0.67 ? "moyenne" : "sèche") }, { n: "Sens", f: (u) => (u < 0.5 ? "Arrêt" : "Démarrage") }],
  [{ n: "Vitesse", f: (u) => ["1/64", "1/32", "1/16", "1/8"][Math.round(u * 3)] }, { n: "Ouverture", f: (u) => `${Math.round(10 + 80 * u)} %` },
    { n: "Profondeur", f: (u) => `${Math.round(100 * u)} %` }, { n: "Douceur", f: (u) => `${Math.round(100 * u)} %` }],
  [{ n: "Temps", f: (u) => ["1/16", "1/16.", "1/8", "1/8.", "1/4", "1/4."][Math.round(u * 5)] }, { n: "Répétitions", f: (u) => `${Math.round(90 * u)} %` },
    { n: "Couleur", f: (u) => { const hz = 800 * Math.pow(20, u); return hz >= 15999 ? "ouverte" : `${(hz / 1000).toFixed(1).replace(".", ",")} kHz`; } }],
  [{ n: "Vitesse", f: (u) => `×${(0.25 + 0.5 * u).toFixed(2).replace(".", ",")}` }, { n: "Déclin", f: (u) => `${Math.round(100 * u)} %` }],
  [{ n: "Forme", f: (u) => ["Fondu sortant", "Fondu entrant", "Silence", "Pompe"][Math.round(u * 3)] }, { n: "Profondeur", f: (u) => `${Math.round(100 * u)} %` }],
];

/** Genre d'un choix de pas, d'après son nom (« Répète 1/64 » → Répète) ; « Perso k » : genre réglé (-k-1 si inconnu). */
function stepTypeOf(name: string): number {
  if (name.startsWith("Perso")) return -1;
  for (let t = STEP_TYPES.length - 1; t > 0; --t) if (name.startsWith(STEP_TYPES[t])) return t;
  return 0;
}
function persoIndex(name: string) { return name.startsWith("Perso") ? Number(name.slice(6)) - 1 : -1; }

let brushListeners: ((b: number | null) => void)[] = [];
let brush: number | null = null;
function useBrush(): [number | null, (b: number | null) => void] {
  const [b, setB] = useState(brush);
  useEffect(() => { brushListeners.push(setB); return () => { brushListeners = brushListeners.filter((f) => f !== setB); }; }, []);
  return [b, (v) => { brush = v; brushListeners.forEach((f) => f(v)); }];
}
let painting: number | null = null; // glisser sur les pas : pose le même choix
if (typeof window !== "undefined") window.addEventListener("pointerup", () => { painting = null; });

/** Genre et libellés d'un choix (le genre d'un « Perso k » est lu dans ses réglages). */
function stepInfo(choices: string[], v: number, persoType?: (k: number) => number) {
  const name = choices[v] ?? "";
  const pk = persoIndex(name);
  const t = pk >= 0 ? (persoType?.(pk) ?? 10) : stepTypeOf(name);
  const top = pk >= 0 ? `P${pk + 1}` : STEP_ABBR[t] ?? "";
  const rest = pk >= 0 ? (STEP_ABBR[t] ?? "") : name.slice(STEP_TYPES[t]?.length ?? 0).replace(/^\s*:?\s*/, "");
  return { name, t, top, rest, color: STEP_COLORS[t] ?? STEP_COLORS[10] };
}

/** Grille de pas : un clic pose le choix de la palette (ou avance), clic droit recule ; glisser pose sur plusieurs pas. */
export function StepCell({ global, index, m, big = false, active = true, playing = false, persoType }:
  { global: number; index: number; m: ModuleDesc; big?: boolean; active?: boolean; playing?: boolean; persoType?: (k: number) => number }) {
  const b = useParam(global >= 0 ? global : null);
  const [br] = useBrush();
  const choices = m.params.find((p) => p.id === `s${index + 1}`)?.choices ?? STEP_TYPES;
  const v = Math.min(choices.length - 1, Math.floor((b?.normalised ?? 0) * choices.length));
  const info = stepInfo(choices, v, persoType);
  const set = (k: number) => b?.entry?.pick(((k % choices.length) + choices.length) % choices.length);
  const paintValue = () => (br === v ? 0 : br!);
  return (
    <button type="button" className={`pd-step${big ? " mp-step" : ""}${active ? "" : " mp-step--out"}${playing ? " mp-step--play" : ""}${index % 4 === 0 ? " mp-step--beat" : ""}`}
      style={{ "--sc": info.color } as CSSProperties}
      title={`Pas ${index + 1} : ${info.name} (clic : ${big && br !== null ? `poser « ${choices[br]} »` : "suivant"}, clic droit : précédent${big ? ", glisser : plusieurs pas" : ""})`}
      onPointerDown={(e) => { if (!big || br === null || e.button !== 0) return; painting = paintValue(); set(painting); }}
      onPointerEnter={() => { if (big && painting !== null) set(painting); }}
      onClick={() => { if (!(big && br !== null)) set(v + 1); }}
      onContextMenu={(e) => { e.preventDefault(); set(v - 1); }}>
      <span>{index + 1}</span>
      {big ? <b>{v > 0 ? <>{info.top}<br />{info.rest}</> : ""}</b> : <b>{v > 0 ? info.top : ""}</b>}
    </button>
  );
}

/** Palette : choisir un effet, puis un de ses préréglages (ou un réglage perso), puis toucher les pas. */
function StepPalette({ ctx }: { ctx: Ctx }) {
  const [br, setBr] = useBrush();
  const choices = ctx.m.params.find((p) => p.id === "s1")?.choices ?? [];
  const persoType = (k: number) => Math.round(ctx.vals.plain(`p${k + 1}type`));
  const brType = br === null ? null : br === 0 ? 0 : persoIndex(choices[br] ?? "") >= 0 ? -1 : stepTypeOf(choices[br] ?? "");
  const [type, setType] = useState<number>(brType ?? 1);
  const shown = type === -1 ? choices.map((n, i) => [n, i] as const).filter(([n]) => persoIndex(n) >= 0)
    : choices.map((n, i) => [n, i] as const).filter(([n, i]) => i > 0 && persoIndex(n) < 0 && stepTypeOf(n) === type);
  return (
    <div className="mp-legend mp-palette">
      <div className="mp-pal-types">
        <button type="button" className={`mp-lb${br === 0 ? " on" : ""}`} onClick={() => setBr(br === 0 ? null : 0)} title="Effacer les pas touchés"><i />Effacer</button>
        {STEP_TYPES.map((n, t) => t === 0 ? null : (
          <button key={n} type="button" className={`mp-lb${type === t ? " on" : ""}`} onClick={() => { setType(t); setBr(choices.indexOf(n) >= 0 ? choices.indexOf(n) : null); }}>
            <i style={{ background: STEP_COLORS[t] }} />{n}</button>
        ))}
        <button type="button" className={`mp-lb${type === -1 ? " on" : ""}`} onClick={() => { setType(-1); setBr(choices.findIndex((n) => persoIndex(n) === 0)); }}>
          <i style={{ background: STEP_COLORS[10] }} />Perso</button>
      </div>
      <div className="mp-pal-presets">
        {shown.map(([n, i]) => {
          const pk = persoIndex(n);
          const label = pk >= 0 ? `${n} · ${STEP_TYPES[persoType(pk)] ?? "—"}` : n === STEP_TYPES[type] ? `${n} (réglages communs)`
            : n.slice(STEP_TYPES[type].length).replace(/^\s*:?\s*/, "");
          return <button key={i} type="button" className={`mp-chip${br === i ? " on" : ""}`} onClick={() => setBr(br === i ? null : i)}
            style={{ "--sc": STEP_COLORS[pk >= 0 ? persoType(pk) : type] } as CSSProperties}>{label}</button>;
        })}
      </div>
      <small>{br === null ? "Choisir un effet, puis un réglage, puis toucher ou balayer les pas (sinon : un clic avance le pas)"
        : br === 0 ? "Toucher ou balayer les pas pour les effacer" : `Toucher ou balayer les pas pour poser « ${choices[br]} »`}</small>
    </div>
  );
}

/** Réglages perso : 4 emplacements, chacun un effet et ses réglages, à poser sur les pas comme un préréglage. */
function StepPerso({ ctx }: { ctx: Ctx }) {
  const [k, setK] = useState(0);
  const t = Math.round(ctx.vals.plain(`p${k + 1}type`));
  const defs = STEP_SETTINGS[t] ?? [];
  return (
    <div className="mp-perso">
      <div className="mp-seg mp-perso-tabs">{[0, 1, 2, 3].map((i) => {
        const ti = Math.round(ctx.vals.plain(`p${i + 1}type`));
        return <button key={i} type="button" className={`mp-lb${k === i ? " on" : ""}`} onClick={() => setK(i)}><i style={{ background: STEP_COLORS[ti] }} />P{i + 1}</button>;
      })}</div>
      <MLcd g={ctx.addr(`p${k + 1}type`)} label="Effet" />
      <div className="mp-grid" style={{ "--cols": 4 } as CSSProperties}>
        {["a", "b", "c", "d"].map((x, j) => {
          const d = defs[j];
          const u = ctx.vals.plain(`p${k + 1}${x}`) / 100;
          return (
            <div key={x} className={`mp-perso-k${d ? "" : " mp-dim"}`}>
              <MKnob g={ctx.addr(`p${k + 1}${x}`)} label={d?.n ?? "—"} size="md" dim={!d} />
              <small>{d ? d.f(u) : ""}</small>
            </div>
          );
        })}
      </div>
    </div>
  );
}

/** Écran : jusqu'à 64 pas en 4 rangées de 16 (une mesure en 1/16 par rangée), tête de lecture. */
function StepsScreen({ ctx }: { ctx: Ctx }) {
  const len = Math.round(ctx.vals.plain("steps")) || 16;
  const now = Math.round(ctx.vals.meters[0] ?? 0) - 1;
  const persoType = (k: number) => Math.round(ctx.vals.plain(`p${k + 1}type`));
  const rows = Math.max(1, Math.ceil(len / 16));
  return (
    <div className="mp-steps mp-steps64" style={{ gridTemplateRows: `repeat(${rows}, minmax(0, 1fr))` }}>
      {Array.from({ length: rows * 16 }, (_, i) => <StepCell key={i} global={ctx.addr(`s${i + 1}`)} index={i} m={ctx.m} big active={i < len} playing={i === now} persoType={persoType} />)}
    </div>
  );
}

/** Outils de la séquence : longueur en mesures, dupliquer, effacer. */
function StepTools({ ctx }: { ctx: Ctx }) {
  const len = Math.round(ctx.vals.plain("steps")) || 16;
  const lenB = useParam(ctx.addr("steps"));
  const cells = Array.from({ length: 64 }, (_, i) => ctx.addr(`s${i + 1}`));
  const choicesN = ctx.m.params.find((p) => p.id === "s1")?.choices?.length ?? 1;
  const normOf = (i: number) => (Math.round(ctx.vals.plain(`s${i + 1}`)) + 0.5) / choicesN;
  const dup = () => {
    if (len >= 64) return;
    const n = Math.min(len, 64 - len);
    for (let i = 0; i < n; ++i) actions.setParam(cells[len + i], normOf(i));
    lenB?.entry?.pick(len + n);
  };
  const clear = () => { for (const g of cells) actions.setParam(g, 0); };
  return (
    <div className="mp-steptools">
      <div className="mp-seg">{[16, 32, 48, 64].map((n) => (
        <button key={n} type="button" className={`mp-lb${len === n ? " on" : ""}`} onClick={() => lenB?.entry?.pick(n)}><i />{`${n / 16} mes.`}</button>
      ))}</div>
      <div className="mp-seg">
        <button type="button" className="mp-lb" disabled={len >= 64} onClick={dup} title="Recopier les pas à la suite et doubler la longueur (jusqu'à 64 pas)"><i />Dupliquer ×2</button>
        <button type="button" className="mp-lb" onClick={clear} title="Effacer les 64 pas (Ctrl+Z pour revenir)"><i />Tout effacer</button>
      </div>
    </div>
  );
}

function Screen({ kind, ctx, band, setBand }: { kind: ScreenKind; ctx: Ctx; band: number; setBand: (b: number) => void }) {
  switch (kind) {
    case "eq": return <EqScreen ctx={ctx} band={band} setBand={setBand} />;
    case "mband": return <MbandScreen ctx={ctx} />;
    case "dist": return <DistScreen ctx={ctx} />;
    case "atmos": return <AtmosScreen ctx={ctx} />;
    case "steps": return <StepsScreen ctx={ctx} />;
    case "mastereq": return <div className="mp-screen mp-screen--mini"><MasterEqScreen ctx={ctx} /></div>;
    case "mastermeters": return <MasterMetersScreen ctx={ctx} />;
    case "grinder": return <GrinderScreen ctx={ctx} />;
    case "sculptor": return <SculptorScreen ctx={ctx} band={band} />;
  }
}

/** Le module ouvert : en surimpression plein écran, mis à l'échelle pour tenir dans la fenêtre. */
export function ModulePanel({ channel, area = "fx", slot, m, onClose }: { channel: number; area?: Area; slot: number; m: ModuleDesc; onClose: () => void }) {
  const lay = LAYOUTS[m.type];
  const pad = area === "pfx" ? { pad: Math.floor(slot / 2), k: slot % 2 } : null;
  const raw = useStore((s) => (pad ? s.channels[channel]?.padfx?.find((f) => f.pad === pad.pad && f.slot === pad.k)?.values : s.channels[channel]?.fx[slot]?.values));
  // v0.47 : interrupteur Actif / Contourné assignable (MIDI, bouton assignable, scènes) : voir useBypass
  const byp = useBypass(channel, pad ? "pfx" : "fx", slot, m.name);
  const bypassed = byp.bypassed;
  const meters = useStore((s) => (!pad && s.focus === channel ? s.tick.fxm[slot] : undefined)) ?? [];
  const preset = useStore((s) => (pad ? undefined : s.tracks.insertPresets?.[channel]?.[slot])) || undefined;
  const [band, setBand] = useState(lay?.firstBand ?? 0);
  const [presetsOpen, setPresetsOpen] = useState(false); // v0.47 : préréglages de l'effet (inserts du canal)
  const root = useRef<HTMLDivElement>(null);
  useEffect(() => {
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); onClose(); } };
    window.addEventListener("keydown", esc, true);
    return () => window.removeEventListener("keydown", esc, true);
  }, [onClose]);
  useLayoutEffect(() => {
    const el = root.current;
    if (!el) return;
    const fit = () => {
      el.style.zoom = "1";
      const w = el.offsetWidth, h = el.offsetHeight;
      const ov = el.parentElement; // surimpression : sa taille en pixels CSS tient compte de l'échelle de l'appareil
      const aw = ov?.clientWidth ?? window.innerWidth, ah = ov?.clientHeight ?? window.innerHeight;
      const z = Math.max(0.45, Math.min(1, (aw - 24) / w, (ah - 24) / h));
      el.style.zoom = String(z);
    };
    fit();
    window.addEventListener("resize", fit);
    void document.fonts?.ready.then(fit);
    return () => window.removeEventListener("resize", fit);
  }, [m.type]);
  if (!lay) return null;
  const values = raw ?? [];
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const plain = (id: string) => { const i = idx(id); return i < 0 ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const addr = (id: string) => { const i = idx(id); return i < 0 ? -1 : paramAddress(channel, area, slot, i); };
  const ctx: Ctx = { m, addr, vals: { plain, meters, preset }, channel, slot, area };
  const { style } = moduleSkinStyle(m.type);
  const toggle = byp.toggle;
  const lcd = lay.lcd ? norm(lay.lcd) : null;
  const body: ReactNode = (
    <>
      {(lay.screen || lay.side) && (
        <div className="mp-row">
          {lay.screen && <div className="mp-screen" style={{ minHeight: lay.screenH ?? 230 }}><Screen kind={lay.screen} ctx={ctx} band={band} setBand={setBand} /></div>}
          {lay.side && <div className="mp-side" style={{ width: lay.sideW ?? 320 }}>{lay.side.map((g) => <GroupView key={g.title} gr={g} ctx={ctx} band={band} setBand={setBand} />)}</div>}
        </div>
      )}
      {lay.rows.map((row, r) => <div key={r} className="mp-row">{row.map((g) => <GroupView key={g.title} gr={g} ctx={ctx} band={band} setBand={setBand} />)}</div>)}
    </>
  );
  return (
    <div className="mp-overlay" role="dialog" aria-label={`Module ${m.name}`} onPointerDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div ref={root} className={`mp sk-knobs mp--${m.type}${bypassed ? " mp--off" : ""}`} style={style}>
        <span className="pd-screw pd-screw--tl" aria-hidden /><span className="pd-screw pd-screw--tr" aria-hidden />
        <span className="pd-screw pd-screw--bl" aria-hidden /><span className="pd-screw pd-screw--br" aria-hidden />
        <header className="mp-head">
          <span className="mp-logo">{m.name}</span>
          {lcd ? <MLcd g={addr(lcd.id)} label="" wide prefix={lcd.label} /> : <div className="mp-lcd mp-lcd--wide"><div className="mp-lcd-box mp-lcd-box--text"><span>{lay.lcdText?.(ctx.vals) ?? preset ?? ""}</span></div></div>}
          {lay.leds && <span className="mp-tools">{lay.leds.map((c) => <MLed key={norm(c).id} g={addr(norm(c).id)} label={norm(c).label} />)}</span>}
          <span className="mp-role">{m.role ?? m.family}</span>
          {!pad && <button type="button" className={`mp-lb mp-presets-btn${presetsOpen ? " on" : ""}`} onClick={() => setPresetsOpen(!presetsOpen)}
            title="Préréglages de l'effet : ceux d'usine et les vôtres (enregistrer, rappeler)">★ Préréglages</button>}
          <ParamMenuZone binding={byp.binding} className="pm-zone--byp">
            <button type="button" className={`mp-lb mp-power${bypassed ? "" : " on"}`} aria-pressed={!bypassed} onClick={toggle}
              title={bypassed ? "Effet contourné : appuyer pour l'activer (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)" : "Effet actif : appuyer pour le contourner (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)"}><i />{bypassed ? "Contourné" : "Actif"}</button>
          </ParamMenuZone>
          <button type="button" className="mp-close" aria-label="Fermer le module" onClick={onClose}>✕</button>
        </header>
        {body}
      </div>
      {presetsOpen && !pad && (
        <div className="face-presets mp-presets">
          <PresetPanel key={`${channel}:${slot}:${m.type}`} scope="fx" channel={channel} slot={slot} pad={-1} title={`${slot + 1} · ${m.name}`}
            current={preset} factory={m.presets}
            onPickFactory={(n) => void actions.applyPreset(channel, slot, n)} onClose={() => setPresetsOpen(false)} />
        </div>
      )}
    </div>
  );
}
