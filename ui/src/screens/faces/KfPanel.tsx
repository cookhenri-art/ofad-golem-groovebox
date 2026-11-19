// Groovebox — OFAD — AGPL-3.0-or-later
// Kick Forge (1.2) : sections particulières de la façade.
//  - KfTop : style, tonalité, tempo, hasard, vider, garder les fondus, copier vers construction libre ;
//  - KfDisplay : le grand afficheur — couches, somme, après effets, écrêtage — sur une grille en
//    double-croches ; la couche choisie se règle à la souris : poignées des fondus d'entrée et de
//    sortie (glisser à gauche / à droite : longueur, en haut / en bas : courbe), fin du morceau
//    (longueur), corps de la couche (décalage dans le temps) ;
//  - KfLayers : les couches (jusqu'à 8), comme des pistes : étiquette, morceau ◀ ▶, petite forme
//    d'onde, volume, hauteur, étirement, hasard, verrou, muet, solo, polarité, à l'envers, retirer ;
//    un sample glissé sur une couche la remplace ; « + Ajouter » allume la couche suivante ;
//  - KfBrowser : la bibliothèque (familles, saveurs, morceaux) de la couche choisie ;
//  - KfRack / KfModule : le rack des 9 modules (ordre, marche, ajout) et les réglages du module
//    choisi, avec l'éditeur de courbe du façonneur et celui du mouillé de la convolution.
// Tout ce qui est dessiné vient du VRAI calcul du moteur (getKickView), relu dès qu'il change.

import { useEffect, useMemo, useRef, useState, useSyncExternalStore } from "react";
import { actions, setUi, showMessage, useStore, type AppState, type KickLibrary, type KickView } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";
import { Control } from "./SynthPanel";

const LAYERS = 8;
const FAMILY_COLORS: Record<string, string> = {
  PUNCH: "oklch(0.78 0.17 55)", CRUNCH: "oklch(0.68 0.2 25)", RUMBLE: "oklch(0.7 0.16 300)", TRANSITOIRE: "oklch(0.88 0.15 95)",
  TOP: "oklch(0.82 0.12 200)", RENFORT: "oklch(0.72 0.13 250)", CORPS: "oklch(0.78 0.15 145)", REVERSE: "oklch(0.75 0.17 340)", SAMPLE: "oklch(0.8 0.02 250)",
};
const MODULES: { prefix: string; name: string }[] = [
  { prefix: "eq", name: "Égaliseur" }, { prefix: "ms", name: "Égaliseur M/S" }, { prefix: "ds", name: "Distorsion" },
  { prefix: "ks", name: "Façonneur de kick" }, { prefix: "tr", name: "Transitoires" }, { prefix: "cv", name: "Convolution" },
  { prefix: "fn", name: "Finaliseur" }, { prefix: "mb", name: "Multibande" }, { prefix: "fl", name: "Filtres" },
];
const SAMPLE_FAMILY = 12;

// --- petits magasins partagés entre les sections ----------------------------------------------
function makeStore<T>(init: T) {
  let v = init;
  const subs = new Set<() => void>();
  return {
    get: () => v,
    set: (x: T) => { v = x; subs.forEach((f) => f()); },
    use: () => useSyncExternalStore((f) => { subs.add(f); return () => { subs.delete(f); }; }, () => v),
  };
}
const selModule = makeStore(6); // module du rack choisi (le finaliseur, d'abord)
const keepFades = makeStore(false);
const viewMode = makeStore<"layer" | "sum" | "post" | "clip">("layer");
const browserOpen = makeStore(false);

/** Lecture / écriture des réglages de l'instrument par identifiant (valeurs réelles). */
function useKf(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  return useMemo(() => {
    const idx = (id: string) => m.params.findIndex((p) => p.id === id);
    const get = (id: string) => { const i = idx(id); return i >= 0 ? toPlain(m.params[i], values?.[i] ?? 0) : 0; };
    const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain)); };
    const global = (id: string) => { const i = idx(id); return i >= 0 ? paramAddress(channel, "inst", 0, i) : null; };
    const desc = (id: string) => m.params[idx(id)];
    return { get, set, global, desc };
  }, [channel, m, values]);
}
const lp = (l: number, id: string) => `l${l + 1}_${id}`;

/** Vues du dernier calcul du moteur, relues dès qu'il change (toutes les 90 ms). */
const viewCache = new Map<number, KickView>();
function useKickView(channel: number): KickView | null {
  const [view, setView] = useState<KickView | null>(viewCache.get(channel) ?? null);
  useEffect(() => {
    let live = true, busy = false;
    const tick = async () => {
      if (busy) return;
      busy = true;
      try {
        const known = viewCache.get(channel)?.v ?? -1;
        const v = await actions.kickView(channel, known);
        if (live && v) { viewCache.set(channel, v); setView(v); }
      } finally { busy = false; }
    };
    void tick();
    const id = window.setInterval(tick, 90);
    return () => { live = false; window.clearInterval(id); };
  }, [channel]);
  return view;
}

function useLibrary(): KickLibrary | null {
  const [lib, setLib] = useState<KickLibrary | null>(null);
  useEffect(() => { void actions.kickLibrary().then(setLib); }, []);
  return lib;
}

const bpmOf = (s: AppState) => s.tick?.t?.bpm || s.tick?.g?.bpm || 120;

/** Forme de fondu, même formule que le moteur (x 0..1, courbe -1..1). */
const fadeShape = (x: number, c: number) => {
  x = Math.max(0, Math.min(1, x));
  return c >= 0 ? Math.pow(x, 1 + 4 * c) : 1 - Math.pow(1 - x, 1 - 4 * c);
};

// --- bandeau du haut --------------------------------------------------------------------------
export function KfTop({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const kf = useKf(channel, m);
  const bpm = useStore(bpmOf);
  const keep = keepFades.use();
  const key = Math.round(kf.get("key"));
  const play = () => actions.noteOn(channel, 24 + key, 1, false);
  return (
    <div className="kf-top">
      <Control channel={channel} module={m} ctl={{ id: "style", k: "menu", label: "Style" }} />
      <Control channel={channel} module={m} ctl={{ id: "key", k: "menu", label: "Tonalité" }} />
      <span className="kf-bpm" title="Tempo du projet : rumbles, crunchs hachés et reverse bass suivent le tempo">
        <small>Tempo</small><b className="tabular">{Math.round(bpm)}</b><small>BPM</small>
      </span>
      <div className="kf-top-btns">
        <button type="button" className="kf-btn kf-btn--hot" onPointerDown={(e) => { e.preventDefault(); play(); }} onPointerUp={() => actions.noteOff(channel, 24 + key)}
          title="Écouter le kick">▶ Écouter</button>
        <button type="button" className="kf-btn kf-btn--dice" onClick={async () => { await actions.kickRandomize(channel, 0, -1, bpm, keep); play(); }}
          title={Math.round(kf.get("style")) === 0 ? "Construction libre : les mêmes morceaux, réglés un peu autrement (volumes, hauteurs, étirements, décalages, fondus)" : "Tirer un kick complet dans le style choisi (les couches verrouillées restent)"}>⚄ Hasard</button>
        <button type="button" className={`kf-btn${keep ? " on" : ""}`} aria-pressed={keep} onClick={() => keepFades.set(!keep)}
          title="Le hasard garde les fondus d'entrée et de sortie de chaque couche">Garder les fondus</button>
        <button type="button" className="kf-btn" onClick={() => kf.set("style", 0)} disabled={Math.round(kf.get("style")) === 0}
          title="Garder ce kick et passer en construction libre (le hasard ne fera plus que varier ses réglages)">→ Construction libre</button>
        <button type="button" className="kf-btn kf-btn--warn" onClick={async () => { await actions.kickRandomize(channel, 2, -1, bpm, keep); setUi({ lane: 0 }); }}
          title="Éteindre toutes les couches (sauf les verrouillées) et le rack">Vider</button>
      </div>
    </div>
  );
}

// --- le grand afficheur -----------------------------------------------------------------------
type Drag = { kind: "fi" | "fo" | "end" | "move"; x0: number; y0: number; v0: number; c0: number; len: number; w: number; h: number };

export function KfDisplay({ channel, module: m, top = false }: { channel: number; module: ModuleDesc; top?: boolean }) {
  const view = useKickView(channel);
  const kf = useKf(channel, m);
  const lane = Math.min(LAYERS - 1, useStore((s) => s.ui.lane));
  const mode = viewMode.use();
  const browser = browserOpen.use() && top;
  const svg = useRef<SVGSVGElement>(null);
  const [drag, setDrag] = useState<Drag | null>(null);
  const lockedLen = useRef<number | null>(null);
  const W = 1200, H = 250, mid = H / 2;
  if (!view) return <div className="kf-display kf-display--empty">{top && <KfTop channel={channel} module={m} />}Calcul du kick…</div>;
  const len = lockedLen.current ?? Math.max(1, view.len);
  const sr = view.sr;
  const x = (sample: number) => (sample / len) * W;
  const sel = view.layers[lane];
  const on = kf.get(lp(lane, "on")) > 0.5;
  const fam = Math.round(kf.get(lp(lane, "fam")));
  // forme d'onde (une barre par colonne, signe gardé), étalée sur sa plage d'échantillons
  const wavePath = (w: Float32Array, scale = 1) => {
    let d = "";
    const n = w.length;
    for (let c = 0; c < n; c++) {
      const px = (c / n) * W * (view.len / len);
      d += `M${px.toFixed(1)} ${mid}V${(mid - w[c] * scale * (mid - 8)).toFixed(1)}`;
    }
    return d;
  };
  // grille : double-croches au tempo du calcul
  const s16 = (sr * 60) / Math.max(20, view.bpm) / 4;
  const grid: number[] = [];
  for (let k = 1; k * s16 < len && k < 128; k++) grid.push(k);
  // fondus de la couche choisie
  const start = sel?.start ?? 0, n = sel?.n ?? 0;
  const fi = Math.min(n, kf.get(lp(lane, "filen")) * 0.001 * sr), fo = Math.min(n, kf.get(lp(lane, "folen")) * 0.001 * sr);
  const fic = kf.get(lp(lane, "ficurve")) / 100, foc = kf.get(lp(lane, "focurve")) / 100;
  const envPath = () => {
    if (!sel) return "";
    const pts: string[] = [];
    const top = 18, bot = H - 18;
    const steps = 80;
    for (let k = 0; k <= steps; k++) {
      const t = (k / steps) * n;
      let g = 1;
      if (fi > 0 && t < fi) g *= fadeShape(t / fi, fic);
      if (fo > 0 && t >= n - fo) g *= fadeShape((n - t) / fo, foc);
      pts.push(`${x(start + t).toFixed(1)},${(bot - g * (bot - top)).toFixed(1)}`);
    }
    return pts.join(" ");
  };
  const toLocal = (e: React.PointerEvent) => {
    const r = svg.current!.getBoundingClientRect();
    return { px: ((e.clientX - r.left) / r.width) * W, py: ((e.clientY - r.top) / r.height) * H, w: r.width, h: r.height };
  };
  const begin = (kind: Drag["kind"]) => (e: React.PointerEvent) => {
    e.preventDefault();
    e.stopPropagation();
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    const p = toLocal(e);
    lockedLen.current = len;
    const v0 = kind === "fi" ? kf.get(lp(lane, "filen")) : kind === "fo" ? kf.get(lp(lane, "folen")) : kind === "end" ? Math.min(kf.get(lp(lane, "len")), (n / sr) * 1000) : kf.get(lp(lane, "offset"));
    const c0 = kind === "fi" ? kf.get(lp(lane, "ficurve")) : kind === "fo" ? kf.get(lp(lane, "focurve")) : 0;
    setDrag({ kind, x0: e.clientX, y0: e.clientY, v0, c0, len, w: p.w, h: p.h });
  };
  const moveDrag = (e: React.PointerEvent) => {
    if (!drag) return;
    const msPerPx = ((drag.len / sr) * 1000) / drag.w;
    const dx = (e.clientX - drag.x0) * msPerPx * (e.shiftKey ? 0.2 : 1);
    const dy = ((e.clientY - drag.y0) / drag.h) * 200 * (e.shiftKey ? 0.25 : 1);
    if (drag.kind === "fi") { kf.set(lp(lane, "filen"), Math.max(0, Math.min(1000, drag.v0 + dx))); kf.set(lp(lane, "ficurve"), Math.max(-100, Math.min(100, drag.c0 + dy))); }
    else if (drag.kind === "fo") { kf.set(lp(lane, "folen"), Math.max(0, Math.min(2000, drag.v0 - dx))); kf.set(lp(lane, "focurve"), Math.max(-100, Math.min(100, drag.c0 + dy))); }
    else if (drag.kind === "end") kf.set(lp(lane, "len"), Math.max(5, Math.min(4000, drag.v0 + dx)));
    else {
      // décalage : aimanté à la double-croche avec Ctrl
      let v = Math.max(-20, Math.min(1000, drag.v0 + dx));
      if (e.ctrlKey || e.metaKey) { const q = (s16 / sr) * 1000; v = Math.round(v / q) * q; }
      kf.set(lp(lane, "offset"), v);
    }
  };
  const end = () => { setDrag(null); lockedLen.current = null; };
  const colorOf = (l: number) => {
    const f = Math.round(kf.get(lp(l, "fam")));
    return FAMILY_COLORS[TAGS[f] ?? "SAMPLE"] ?? FAMILY_COLORS.SAMPLE;
  };
  const showLayers = mode === "layer";
  const ms = (smp: number) => `${Math.round((smp / sr) * 1000)} ms`;
  return (
    <div className="kf-display">
      {top && <KfTop channel={channel} module={m} />}
      <div className="kf-views" role="tablist" aria-label="Vue de l'afficheur">
        {([["layer", "Couches"], ["sum", "Somme"], ["post", "Après effets"], ["clip", "Écrêtage"]] as const).map(([id, label]) => (
          <button key={id} type="button" role="tab" aria-selected={mode === id} className={mode === id ? "on" : ""} onClick={() => viewMode.set(id)}>{label}</button>
        ))}
        {top && <button type="button" className={`kf-libbtn${browser ? " on" : ""}`} onClick={() => browserOpen.set(!browser)} aria-expanded={browser}>☰ Bibliothèque · couche {lane + 1}</button>}
        <span className="kf-views-info tabular">
          {ms(view.len)} · tonalité {view.key.toFixed(1)} Hz · crête avant gain {view.peak.toFixed(1)} dB{kf.get("autogain") > 0.5 ? ` · gain auto. ${view.gain >= 1 ? "+" : ""}${(20 * Math.log10(Math.max(1e-6, view.gain))).toFixed(1)} dB` : ""}
        </span>
      </div>
      <div className="kf-screen-wrap">
      {browser && <KfBrowser channel={channel} module={m} />}
      <svg ref={svg} className={`kf-screen${drag ? " is-drag" : ""}`} viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" onPointerMove={moveDrag} onPointerUp={end} onPointerCancel={end}
        aria-label="Forme du kick : fondus, longueur et décalage de la couche choisie se règlent à la souris">
        {grid.map((k) => <line key={k} x1={x(k * s16)} x2={x(k * s16)} y1={0} y2={H} className={k % 4 === 0 ? "kf-grid kf-grid--beat" : "kf-grid"} />)}
        <line x1={0} x2={W} y1={mid} y2={mid} className="kf-axis" />
        {showLayers && view.layers.map((l, i) => l && i !== lane ? <path key={i} d={wavePath(l.w)} className="kf-wave kf-wave--other" style={{ stroke: colorOf(i) }} /> : null)}
        {showLayers && sel && <path d={wavePath(sel.w)} className="kf-wave kf-wave--sel" style={{ stroke: colorOf(lane) }} />}
        {mode === "sum" && <path d={wavePath(view.sum)} className="kf-wave kf-wave--sum" />}
        {(mode === "post" || mode === "clip") && <path d={wavePath(view.post)} className="kf-wave kf-wave--post" />}
        {mode === "clip" && <path d={wavePath(view.gr, -1)} className="kf-wave kf-wave--gr" />}
        {showLayers && sel && on && (
          <g className="kf-edit">
            <rect x={x(start)} y={6} width={Math.max(2, x(start + n) - x(start))} height={H - 12} className="kf-body" onPointerDown={begin("move")} />
            <polyline points={envPath()} className="kf-env" />
            <line x1={x(start + fi)} x2={x(start + fi)} y1={10} y2={H - 10} className="kf-mark" />
            <line x1={x(start + n - fo)} x2={x(start + n - fo)} y1={10} y2={H - 10} className="kf-mark" />
            <circle cx={x(start + fi)} cy={18} r={9} className="kf-handle kf-handle--in" onPointerDown={begin("fi")}><title>Fondu d'entrée : à gauche / à droite = longueur, en haut / en bas = courbe (Maj : fin)</title></circle>
            <circle cx={x(start + n - fo)} cy={18} r={9} className="kf-handle kf-handle--out" onPointerDown={begin("fo")}><title>Fondu de sortie : à gauche / à droite = longueur, en haut / en bas = courbe (Maj : fin)</title></circle>
            <rect x={x(start + n) - 5} y={H / 2 - 22} width={10} height={44} rx={3} className="kf-handle kf-handle--end" onPointerDown={begin("end")}><title>Fin du morceau : longueur de la couche</title></rect>
          </g>
        )}
      </svg>
      </div>
      <div className="kf-display-foot">
        {sel ? (
          <span>
            <b style={{ color: colorOf(lane) }}>Couche {lane + 1}</b> · {sel.name} · départ {ms(sel.start)} · durée {ms(sel.n)}
            {fam === SAMPLE_FAMILY && sel.hz > 20 && <> · tonalité du sample {sel.hz.toFixed(1)} Hz <TakeKey channel={channel} m={m} hz={sel.hz} /></>}
          </span>
        ) : <span>Couche {lane + 1} éteinte : « + Ajouter » ou choisissez un morceau dans la bibliothèque</span>}
        <span className="kf-hint">Glisser le corps : décalage (Ctrl : aimanté à la double-croche) · ronds : fondus · barre de fin : longueur</span>
      </div>
    </div>
  );
}

/** Prendre la tonalité détectée d'un sample comme tonalité du kick. */
function TakeKey({ channel, m, hz }: { channel: number; m: ModuleDesc; hz: number }) {
  const kf = useKf(channel, m);
  const midi = Math.round(69 + 12 * Math.log2(hz / 440));
  let k = midi - 24;
  while (k < 0) k += 12;
  while (k > 23) k -= 12;
  return <button type="button" className="kf-btn kf-btn--mini" onClick={() => { kf.set("key", k); showMessage(`Tonalité du kick : ${kf.desc("key")?.choices?.[k] ?? ""}`); }}>Prendre cette tonalité</button>;
}

// --- les couches ------------------------------------------------------------------------------
const TAGS = ["PUNCH", "CRUNCH", "RUMBLE", "TRANSITOIRE", "TRANSITOIRE", "TOP", "RENFORT", "RUMBLE", "RENFORT", "TOP", "CORPS", "REVERSE", "SAMPLE"];

function MiniWave({ w, color }: { w?: Float32Array; color: string }) {
  if (!w) return <svg className="kf-mini" viewBox="0 0 120 30" />;
  let d = "";
  const step = Math.max(1, Math.floor(w.length / 120));
  let px = 0;
  for (let c = 0; c < w.length; c += step, px++) {
    let mx = 0;
    for (let i = c; i < Math.min(w.length, c + step); i++) mx = Math.max(mx, Math.abs(w[i]));
    d += `M${px} ${15 - mx * 14}V${15 + mx * 14}`;
  }
  return <svg className="kf-mini" viewBox="0 0 120 30" preserveAspectRatio="none"><path d={d} style={{ stroke: color }} /></svg>;
}

function Knob({ global, label }: { global: number | null; label: string }) {
  const b = useParam(global, label);
  return <div className="fc fc--small"><Encoder binding={b} variant="knob" /></div>;
}

export function KfLayers({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const kf = useKf(channel, m);
  const view = useKickView(channel);
  const lib = useLibrary();
  const lane = Math.min(LAYERS - 1, useStore((s) => s.ui.lane));
  const bpm = useStore(bpmOf);
  const keep = keepFades.use();
  const [over, setOver] = useState(-1);
  // couches montrées : les allumées, plus la choisie
  const shown = Array.from({ length: LAYERS }, (_, l) => l).filter((l) => kf.get(lp(l, "on")) > 0.5 || l === lane);
  const nextOff = Array.from({ length: LAYERS }, (_, l) => l).find((l) => kf.get(lp(l, "on")) < 0.5 && l !== lane);
  const playKick = () => actions.noteOn(channel, 24 + Math.round(kf.get("key")), 1, false);
  const stepPart = (l: number, d: number) => {
    const f = Math.round(kf.get(lp(l, "fam")));
    const count = lib?.families[f]?.parts.length ?? 0;
    if (!count) return;
    kf.set(lp(l, "part"), (Math.round(kf.get(lp(l, "part"))) + d + count) % count);
    kf.set(lp(l, "on"), 1);
    window.setTimeout(playKick, 160);
  };
  const drop = async (l: number, file: File) => {
    if (!/\.(wav|aiff?|flac|mp3|ogg)$/i.test(file.name)) { showMessage("Glissez un fichier son (WAV, AIFF, FLAC, MP3, OGG)"); return; }
    if (await actions.loadSample(channel, l, file)) {
      kf.set(lp(l, "fam"), SAMPLE_FAMILY);
      kf.set(lp(l, "on"), 1);
      setUi({ lane: l });
      showMessage(`Couche ${l + 1} : ${file.name}`);
    }
  };
  return (
    <div className="kf-layers">
      {shown.map((l) => {
        const f = Math.round(kf.get(lp(l, "fam")));
        const tag = TAGS[f] ?? "SAMPLE";
        const color = FAMILY_COLORS[tag];
        const isOn = kf.get(lp(l, "on")) > 0.5;
        const v = view?.layers[l];
        const flag = (id: string) => kf.get(lp(l, id)) > 0.5;
        const toggle = (id: string) => kf.set(lp(l, id), flag(id) ? 0 : 1);
        const name = v?.name ?? (f === SAMPLE_FAMILY ? "(glissez un sample)" : lib?.families[f]?.parts[Math.round(kf.get(lp(l, "part")))] ?? "");
        return (
          <div key={l} className={`kf-slot${l === lane ? " is-selected" : ""}${isOn ? "" : " is-off"}${over === l ? " is-drop" : ""}${flag("mute") ? " is-muted" : ""}`} style={{ "--fam": color } as React.CSSProperties}
            onPointerDown={() => { if (l !== lane) setUi({ lane: l }); }}
            onDragOver={(e) => { e.preventDefault(); setOver(l); }} onDragLeave={() => setOver(-1)}
            onDrop={(e) => { e.preventDefault(); setOver(-1); const file = e.dataTransfer.files?.[0]; if (file) void drop(l, file); }}>
            <div className="kf-slot-head">
              <span className="kf-tag">{l + 1} · {tag}</span>
              <button type="button" className="kf-arrow" onClick={() => stepPart(l, -1)} aria-label={`Couche ${l + 1} : morceau précédent`} disabled={f === SAMPLE_FAMILY}>◀</button>
              <button type="button" className="kf-name" onClick={() => { setUi({ lane: l }); browserOpen.set(true); }} title={`${name} — ouvrir la bibliothèque`}>{isOn ? name : "Éteinte"}</button>
              <button type="button" className="kf-arrow" onClick={() => stepPart(l, 1)} aria-label={`Couche ${l + 1} : morceau suivant`} disabled={f === SAMPLE_FAMILY}>▶</button>
            </div>
            <MiniWave w={v?.w} color={color} />
            <div className="kf-slot-knobs">
              <Knob global={kf.global(lp(l, "vol"))} label="Vol." />
              <Knob global={kf.global(lp(l, "pitch"))} label="Transpos." />
              <Knob global={kf.global(lp(l, "stretch"))} label="Étirement" />
            </div>
            <div className="kf-slot-btns">
              <button type="button" className="kf-sb" title="Hasard sur cette couche : autre morceau de la même famille" disabled={flag("lock")}
                onClick={async () => { await actions.kickRandomize(channel, 1, l, bpm, keep); kf.set(lp(l, "on"), 1); window.setTimeout(playKick, 160); }}>⚄</button>
              <button type="button" className={`kf-sb${flag("lock") ? " on" : ""}`} onClick={() => toggle("lock")} title="Verrou : le hasard ne touche pas cette couche">🔒</button>
              <button type="button" className={`kf-sb${flag("mute") ? " on" : ""}`} onClick={() => toggle("mute")} title="Muet">M</button>
              <button type="button" className={`kf-sb${flag("solo") ? " on" : ""}`} onClick={() => toggle("solo")} title="Solo">S</button>
              <button type="button" className={`kf-sb${flag("phase") ? " on" : ""}`} onClick={() => toggle("phase")} title="Polarité inversée">Ø</button>
              <button type="button" className={`kf-sb${flag("rev") ? " on" : ""}`} onClick={() => toggle("rev")} title="Lire le morceau à l'envers">⇆</button>
              <button type="button" className="kf-sb" onClick={() => { kf.set(lp(l, "on"), isOn ? 0 : 1); }} title={isOn ? "Retirer la couche" : "Allumer la couche"}>{isOn ? "✕" : "＋"}</button>
            </div>
          </div>
        );
      })}
      {nextOff !== undefined && (
        <button type="button" className="kf-add" onClick={() => { kf.set(lp(nextOff, "on"), 1); setUi({ lane: nextOff }); browserOpen.set(true); }}
          title="Ajouter une couche (jusqu'à 8) et choisir son morceau">＋ Ajouter<small>une couche</small></button>
      )}
    </div>
  );
}

// --- la bibliothèque --------------------------------------------------------------------------
export function KfBrowser({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const kf = useKf(channel, m);
  const lib = useLibrary();
  const lane = Math.min(LAYERS - 1, useStore((s) => s.ui.lane));
  const open = browserOpen.use();
  const curFam = Math.round(kf.get(lp(lane, "fam")));
  const curPart = Math.round(kf.get(lp(lane, "part")));
  const [fam, setFam] = useState(curFam);
  const [flavor, setFlavor] = useState(-1);
  const file = useRef<HTMLInputElement>(null);
  useEffect(() => { setFam(curFam); setFlavor(-1); }, [lane]); // eslint-disable-line react-hooks/exhaustive-deps
  if (!lib) return <div className="kf-browser">Bibliothèque…</div>;
  const F = lib.families[fam];
  const parts = F ? F.parts.map((name, i) => ({ name, i })).filter((p) => flavor < 0 || Math.floor(p.i / Math.max(1, F.per)) === flavor) : [];
  const pick = (i: number) => {
    kf.set(lp(lane, "fam"), fam);
    kf.set(lp(lane, "part"), i);
    kf.set(lp(lane, "on"), 1);
    window.setTimeout(() => actions.noteOn(channel, 24 + Math.round(kf.get("key")), 1, false), 160);
  };
  return (
    <div className={`kf-browser${open ? " is-open" : ""}`} role="dialog" aria-label={`Bibliothèque de la couche ${lane + 1}`}>
      <button type="button" className="kf-browser-close" onClick={() => browserOpen.set(false)} aria-label="Fermer la bibliothèque">✕</button>
      <div className="kf-fams" role="listbox" aria-label="Familles">
        {lib.families.map((x, i) => (
          <button key={i} type="button" role="option" aria-selected={i === fam} className={`kf-fam${i === fam ? " on" : ""}${i === curFam ? " is-cur" : ""}`}
            style={{ "--fam": FAMILY_COLORS[x.tag] ?? FAMILY_COLORS.SAMPLE } as React.CSSProperties} onClick={() => { setFam(i); setFlavor(-1); }}>
            <span className="kf-fam-dot" />{x.name}<small>{x.parts.length || ""}</small>
          </button>
        ))}
      </div>
      <div className="kf-parts">
        <div className="kf-flavors">
          <button type="button" className={flavor < 0 ? "on" : ""} onClick={() => setFlavor(-1)}>Toutes</button>
          {F?.flavors.map((x, i) => <button key={i} type="button" className={flavor === i ? "on" : ""} onClick={() => setFlavor(i)}>{x}</button>)}
        </div>
        {fam === SAMPLE_FAMILY ? (
          <div className="kf-own">
            <p>Glissez un fichier son sur une couche, ou choisissez-en un pour la couche {lane + 1}. Le sample est accordé sur la tonalité du kick (réglage « sample accordé » de la couche).</p>
            <button type="button" className="kf-btn" onClick={() => file.current?.click()}>Choisir un fichier…</button>
            <input ref={file} type="file" accept="audio/*,.wav,.aif,.aiff,.flac,.mp3,.ogg" hidden onChange={async (e) => {
              const f = e.target.files?.[0];
              e.target.value = "";
              if (f && (await actions.loadSample(channel, lane, f))) { kf.set(lp(lane, "fam"), SAMPLE_FAMILY); kf.set(lp(lane, "on"), 1); showMessage(`Couche ${lane + 1} : ${f.name}`); }
            }} />
          </div>
        ) : (
          <div className="kf-partlist" role="listbox" aria-label={`Morceaux : ${F?.name ?? ""}`}>
            {parts.map((p) => (
              <button key={p.i} type="button" role="option" aria-selected={fam === curFam && p.i === curPart} className={fam === curFam && p.i === curPart ? "on" : ""} onClick={() => pick(p.i)}>{p.name}</button>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

// --- le rack ----------------------------------------------------------------------------------
export function KfRack({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const kf = useKf(channel, m);
  const sel = selModule.use();
  const active = MODULES.map((x, i) => ({ ...x, i, ord: kf.get(`${x.prefix}_ord`) }))
    .filter((x) => kf.get(`${x.prefix}_on`) > 0.5).sort((a, b) => a.ord - b.ord || a.i - b.i);
  const off = MODULES.map((x, i) => ({ ...x, i })).filter((x) => kf.get(`${x.prefix}_on`) < 0.5);
  const [adding, setAdding] = useState(false);
  const renumber = (order: number[]) => order.forEach((mi, k) => kf.set(`${MODULES[mi].prefix}_ord`, k + 1));
  const move = (pos: number, d: number) => {
    const order = active.map((x) => x.i);
    const to = pos + d;
    if (to < 0 || to >= order.length) return;
    [order[pos], order[to]] = [order[to], order[pos]];
    renumber(order);
  };
  return (
    <div className="kf-rack">
      <ol className="kf-chain" aria-label="Ordre des effets du kick">
        <li className="kf-chain-io">Somme des couches</li>
        {active.map((x, pos) => (
          <li key={x.prefix} className={`kf-mod${sel === x.i ? " is-selected" : ""}`}>
            <button type="button" className="kf-mod-name" onClick={() => selModule.set(x.i)}>{pos + 1}. {x.name}</button>
            <span className="kf-mod-btns">
              <button type="button" onClick={() => move(pos, -1)} disabled={pos === 0} aria-label={`${x.name} : plus tôt`}>▲</button>
              <button type="button" onClick={() => move(pos, 1)} disabled={pos === active.length - 1} aria-label={`${x.name} : plus tard`}>▼</button>
              <button type="button" onClick={() => kf.set(`${x.prefix}_on`, 0)} aria-label={`Retirer ${x.name}`}>✕</button>
            </span>
          </li>
        ))}
        <li className="kf-chain-io">Sortie (gain automatique)</li>
      </ol>
      {off.length > 0 && (
        <div className="kf-addmod">
          <button type="button" className="kf-btn" onClick={() => setAdding(!adding)} aria-expanded={adding}>＋ Ajouter un module</button>
          {adding && (
            <div className="kf-addmod-list" role="menu">
              {off.map((x) => (
                <button key={x.prefix} type="button" role="menuitem" onClick={() => {
                  renumber([...active.map((a) => a.i), x.i]);
                  kf.set(`${x.prefix}_on`, 1);
                  selModule.set(x.i);
                  setAdding(false);
                }}>{x.name}</button>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
}

/** Éditeur de points (façonneur : 8 points de gain ; convolution : 3 points du mouillé dans le temps). */
function CurveEditor({ points, onChange, times, color, behind }: { points: number[]; onChange: (i: number, v: number, t?: number) => void; times?: number[]; color: string; behind?: Float32Array }) {
  const W = 600, H = 160;
  const ref = useRef<SVGSVGElement>(null);
  const [drag, setDrag] = useState(-1);
  const xs = times ?? points.map((_, i) => i / (points.length - 1));
  const pos = (i: number) => ({ x: xs[i] * (W - 20) + 10, y: H - 10 - points[i] * (H - 20) });
  const line = points.map((_, i) => `${pos(i).x.toFixed(1)},${pos(i).y.toFixed(1)}`).join(" ");
  const move = (e: React.PointerEvent) => {
    if (drag < 0 || !ref.current) return;
    const r = ref.current.getBoundingClientRect();
    const v = Math.max(0, Math.min(1, 1 - ((e.clientY - r.top) / r.height * H - 10) / (H - 20)));
    const t = Math.max(0.01, Math.min(0.99, ((e.clientX - r.left) / r.width * W - 10) / (W - 20)));
    onChange(drag, v, times && drag > 0 && drag < points.length - 1 ? t : undefined);
  };
  return (
    <svg ref={ref} className="kf-curve" viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" onPointerMove={move} onPointerUp={() => setDrag(-1)} onPointerCancel={() => setDrag(-1)}>
      <rect x={10} y={10} width={W - 20} height={H - 20} className="kf-curve-bg" />
      {behind && <path d={Array.from(behind, (y, c) => `M${(10 + (c / behind.length) * (W - 20)).toFixed(1)} ${H / 2}V${(H / 2 - y * (H / 2 - 12)).toFixed(1)}`).join("")} className="kf-wave kf-wave--sum" opacity={0.35} />}
      <polyline points={`10,${H - 10} ${line} ${W - 10},${H - 10}`} className="kf-curve-fill" style={{ fill: color }} />
      <polyline points={line} className="kf-curve-line" style={{ stroke: color }} />
      {points.map((_, i) => (
        <circle key={i} cx={pos(i).x} cy={pos(i).y} r={8} className="kf-handle" onPointerDown={(e) => { e.preventDefault(); (e.currentTarget as Element).setPointerCapture(e.pointerId); setDrag(i); }}>
          <title>{`Point ${i + 1} : ${Math.round(points[i] * 100)} %`}</title>
        </circle>
      ))}
    </svg>
  );
}

export function KfModule({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const kf = useKf(channel, m);
  const sel = selModule.use();
  const view = useKickView(channel);
  const mod = MODULES[sel];
  const px = mod.prefix;
  const isOn = kf.get(`${px}_on`) > 0.5;
  const ids = m.params.filter((p) => p.id.startsWith(`${px}_`) && p.id !== `${px}_on` && p.id !== `${px}_ord`);
  const custom = new Set(px === "ks" ? ["ks_n1", "ks_n2", "ks_n3", "ks_n4", "ks_n5", "ks_n6", "ks_n7", "ks_n8"] : px === "cv" ? ["cv_c1", "cv_c2t", "cv_c2", "cv_c3"] : []);
  const label = (name: string) => name.replace(`${mod.name} : `, "").replace(/^./, (c) => c.toUpperCase());
  const color = "var(--accent, oklch(0.8 0.15 55))";
  return (
    <div className={`kf-module${isOn ? "" : " is-off"}`}>
      <div className="kf-module-head">
        <h4>{mod.name}</h4>
        <button type="button" className={`kf-btn${isOn ? " on" : ""}`} aria-pressed={isOn} onClick={() => kf.set(`${px}_on`, isOn ? 0 : 1)}>{isOn ? "En marche" : "Éteint"}</button>
      </div>
      {px === "ks" && (
        <CurveEditor color={color} behind={view?.sum} points={Array.from({ length: 8 }, (_, k) => kf.get(`ks_n${k + 1}`) / 100)} onChange={(i, v) => kf.set(`ks_n${i + 1}`, v * 100)} />
      )}
      {px === "cv" && (
        <CurveEditor color={color} points={[kf.get("cv_c1") / 100, kf.get("cv_c2") / 100, kf.get("cv_c3") / 100]} times={[0, kf.get("cv_c2t") / 100, 1]}
          onChange={(i, v, t) => { kf.set(i === 0 ? "cv_c1" : i === 1 ? "cv_c2" : "cv_c3", v * 100); if (t !== undefined) kf.set("cv_c2t", t * 100); }} />
      )}
      <div className="kf-module-ctl">
        {ids.filter((p) => !custom.has(p.id)).map((p) => {
          const k = p.choices && p.choices.length > 4 ? "menu" : undefined;
          return <Control key={p.id} channel={channel} module={m} ctl={{ id: p.id, k, label: label(p.name) }} />;
        })}
      </div>
      {view && (px === "fn" || px === "ds" || px === "mb" || px === "tr") && (
        <svg className="kf-modview" viewBox="0 0 600 80" preserveAspectRatio="none" aria-label="Kick après les effets">
          <path d={Array.from(view.post, (y, c) => `M${(c / view.post.length) * 600} 40V${40 - y * 36}`).join("")} className="kf-wave kf-wave--post" />
          {px === "fn" && <path d={Array.from(view.gr, (y, c) => `M${(c / view.gr.length) * 600} 0V${-y * 40}`).join("")} className="kf-wave kf-wave--gr" />}
        </svg>
      )}
    </div>
  );
}

// --- jeu : une octave de notes depuis la tonalité (le kick suit la note en mode « Note ») -----
const NOTE_FR = ["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"];
export function KfKeys({ track, module: m }: { track: number; module: ModuleDesc }) {
  const kf = useKf(track, m);
  const base = 24 + Math.round(kf.get("key"));
  const follows = kf.get("pitchmode") > 0.5;
  const lit = useStore((s) => s.litLanes);
  return (
    <div className="kf-keys" role="group" aria-label="Jouer le kick">
      <span className="kf-keys-label">{follows ? "Le kick suit la note" : "Hauteur fixe (tonalité) · « Note jouée : Suivie » pour jouer la note"}</span>
      {Array.from({ length: 13 }, (_, k) => {
        const note = base + k;
        return (
          <button key={k} type="button" className={`kf-key${k === 0 || k === 12 ? " is-root" : ""}${(lit[0] ?? 0) > 0 && k === 0 ? " is-lit" : ""}`}
            onPointerDown={(e) => { e.preventDefault(); e.currentTarget.setPointerCapture(e.pointerId); const r = e.currentTarget.getBoundingClientRect(); actions.noteOn(track, note, Math.min(1, Math.max(0.25, (r.bottom - e.clientY) / r.height + 0.25)), false); }}
            onPointerUp={() => actions.noteOff(track, note)} onPointerCancel={() => actions.noteOff(track, note)}>
            {NOTE_FR[note % 12]}<small>{Math.floor(note / 12) - 1}</small>
          </button>
        );
      })}
    </div>
  );
}
