// Groovebox — OFAD — AGPL-3.0-or-later
// Éditeur générique d'un module (instrument ou effet) décrit par le moteur : onglets, pages de
// 8 réglages aux encodeurs, visuel adapté à la section, liste des valeurs.

import { ParamMenuZone } from "../controls/Encoder";
import type { Catalog, ModuleDesc, SectionDesc, TracksData } from "../engine/protocol";
import { formatPlain, mixerFor, toNorm, toPlain, MOD_SOURCE_COLORS } from "../engine/params";
import { globalOf, layout, setUi, useStore, actions } from "../engine/store";
import { paramAddress, useParam, useWriteTarget, writeParam } from "../controls/bindings";
import { MOD_SCOPE_SOURCES, MOD_SCOPE_TITLE, type ModScope } from "../engine/modScopes";
import type { EncoderBinding } from "../controls/Encoder";
import { AdsrView, MasterMeters, CompCurve, DeessView, DistCurve, EqCurve, FilterView, GateCurve, ModSourceView, ResponseView, svfDb, SampleView, ShapeView, type EqBand } from "./Visuals";
import { useEffect, useState } from "react";
import { useValueDrag } from "../controls/useValueDrag";
import { DragParam } from "../controls/DragParam";

export const TAB_LABELS: Record<string, string> = {
  global: "Global", osc: "Oscillateur", filter: "Filtre", env: "Enveloppe", amp: "Amplitude", mod: "Modulation",
  engines: "Moteurs", filters: "Filtres", dynamics: "Dynamique", feedback: "Réinjection", time: "Temps", tone: "Couleur",
  space: "Espace", algo: "Algorithme", main: "Réglages", bands: "Bandes", pad: "Pad", zone: "Zone", modsrc: "Modulation",
  padfx: "Effet · envois", padfilter: "Filtre", padamp: "Amplitude", padsmp: "Échantillon", padlfo: "LFO", padpc: "Précalcul", kit: "Kit",
  matrix: "Matrice", arp: "Arpège", modenv: "Env. mod", modlfo: "LFO",
};
/** Onglets dont les sections existent une fois par pad (batteries). */
export const PER_PAD_TABS = ["padfilter", "padamp", "padfx", "padsmp", "padlfo", "padpc"];
const INSTRUMENT_TABS = ["global", "osc", "filter", "env", "amp"];

export type Area = "inst" | "fx";

/** Onglets d'un module : ceux de ses sections, dans l'ordre habituel, puis « Modulation ». */
export function moduleTabs(m: ModuleDesc, area: Area): { id: string; label: string }[] {
  const ids: string[] = [];
  const perPad = m.sections.some((s) => s.visual.startsWith("drumpad") || s.visual === "zone");
  if (perPad) ids.push(m.drumKit && !m.sections.some((s) => s.visual === "zone") ? "pad" : "zone");
  const order = area === "inst" ? INSTRUMENT_TABS : [];
  for (const t of order) if (m.sections.some((s) => s.tab === t && !isPerPad(s))) ids.push(t);
  for (const s of m.sections) if (s.tab && !ids.includes(s.tab) && s.tab !== "mod" && !isPerPad(s)) ids.push(s.tab);
  if (m.sections.some((s) => s.tab === "mod")) ids.push("mod");
  ids.push("modsrc");
  // « mod » (sections de l'instrument) et « modsrc » (sources de la piste) partagent le libellé : on les fusionne
  const merged = ids.filter((id, i) => !(id === "modsrc" && ids.includes("mod")) && ids.indexOf(id) === i);
  return merged.map((id) => ({ id, label: TAB_LABELS[id] ?? id }));
}

const isPerPad = (s: SectionDesc) => s.visual.startsWith("drumpad") || s.visual === "zone";

/** Sections affichées pour un onglet (une par pad pour les batteries et les zones). */
export function sectionsFor(m: ModuleDesc, tab: string, lane: number): SectionDesc[] {
  if (tab === "pad" || tab === "zone") {
    const perPad = m.sections.filter(isPerPad);
    return perPad[lane] ? [perPad[lane]] : perPad.slice(0, 1);
  }
  if (PER_PAD_TABS.includes(tab)) {
    const perPad = m.sections.filter((s) => s.tab === tab);
    return perPad[lane] ? [perPad[lane]] : perPad.slice(0, 1);
  }
  return m.sections.filter((s) => s.tab === tab && !isPerPad(s));
}

/** Valeur jouée (normalisée) d'un réglage d'un module du canal. */
/** Valeurs affichées par les visuels : les mêmes que les encodeurs (valeur jouée en mode normal,
    verrou du pas ou de la scène en édition de verrous, sinon la valeur de base). */
function useValues(channel: number, area: Area, slot: number, m: ModuleDesc | null): number[] {
  const c = useStore((s) => s.channels[channel]);
  const eff = useStore((s) => (s.focus === channel ? s.eff : null));
  const target = useWriteTarget(channel);
  const extras = useStore((s) => s.extras);
  const scenes = useStore((s) => s.scenes);
  if (!m || !c) return [];
  const L = layout();
  const raw = area === "inst" ? (c.inst ?? []) : (c.fx[slot]?.values ?? []);
  const base = area === "inst" ? L.instrumentBase : L.insertBase + slot * L.insertSlots;
  let locks: [number, number][] | undefined;
  if (target.kind === "step") locks = extras.find((e) => e.lane === target.lane && e.step === target.step)?.locks;
  else if (target.kind === "steps") { const [l, st] = target.cells[0] ?? [-1, -1]; locks = extras.find((e) => e.lane === l && e.step === st)?.locks; }
  else if (target.kind === "scene") locks = scenes[target.scene]?.locks;
  if (target.kind !== "base") {
    const lockMap = new Map((locks ?? []).map(([g, v]) => [g, v]));
    return m.params.map((_, i) => lockMap.get(globalOf(channel, base + i)) ?? raw[i] ?? 0);
  }
  return m.params.map((_, i) => eff?.[base + i] ?? raw[i] ?? 0);
}

/** Encodeurs d'une section : 8 liaisons (nombre d'appels de hooks constant). */
export function useSectionEncoders(channel: number, area: Area, slot: number, section: SectionDesc | null): (EncoderBinding | null)[] {
  const ids = Array.from({ length: 8 }, (_, i) => (section && section.params[i] !== undefined ? paramAddress(channel, area, slot, section.params[i]) : null));
  // eslint-disable-next-line react-hooks/rules-of-hooks
  return ids.map((g) => useParam(g));
}

export const MOD_TYPES = ["LFO", "Fonction", "Suiveur", "Séquenceur"];
const modTypeOf = (vals: number[] | undefined) => Math.min(3, Math.max(0, Math.floor((vals?.[0] ?? 0) * 4)));
/** Source affichée : celle choisie si elle est de la famille, sinon la première de la famille. */
export function useScopedModSlot(scope: ModScope) {
  const modSlot = useStore((s) => s.ui.modSlot);
  return MOD_SCOPE_SOURCES[scope].includes(modSlot) ? modSlot : MOD_SCOPE_SOURCES[scope][0];
}

/** Encodeurs de la source de modulation choisie : les réglages de son type (le type se choisit
    sur l'écran, par des boutons, pour ne jamais rester bloqué sur un type sans retour). */
export function useModSourceEncoders(channel: number, scope: ModScope): (EncoderBinding | null)[] {
  const cat = useStore((s) => s.catalog);
  const modSlot = useScopedModSlot(scope);
  const values = useStore((s) => s.channels[channel]?.mod[modSlot]);
  const type = modTypeOf(values);
  const ms = cat?.modSource;
  const sec = ms?.sections[type] ?? null;
  const list = sec ? sec.params.filter((k) => ms?.params[k]?.id !== "type") : [];
  const ids = Array.from({ length: 8 }, (_, i) => (list[i] !== undefined ? paramAddress(channel, "mod", modSlot, list[i]) : null));
  // eslint-disable-next-line react-hooks/rules-of-hooks
  return ids.map((g) => useParam(g));
}

function paramPlain(m: ModuleDesc, values: number[], id: string): number | undefined {
  const i = m.params.findIndex((p) => p.id === id);
  return i < 0 ? undefined : toPlain(m.params[i], values[i] ?? 0);
}

function findPlain(m: ModuleDesc, values: number[], section: SectionDesc, pattern: RegExp): number | undefined {
  for (const i of section.params) if (pattern.test(m.params[i].id)) return toPlain(m.params[i], values[i] ?? 0);
  for (let i = 0; i < m.params.length; ++i) if (pattern.test(m.params[i].id)) return toPlain(m.params[i], values[i] ?? 0);
  return undefined;
}

/** Visuel d'une section, d'après sa nature et les valeurs réelles. */
function paramPlainIn(m: ModuleDesc, values: number[], section: SectionDesc, re: RegExp): number | undefined {
  const k = section.params.find((i) => re.test(m.params[i].id));
  return k === undefined ? undefined : toPlain(m.params[k], values[k] ?? 0);
}

/** Distorsion : les deux moteurs. Toucher une carte la choisit et montre ses 14 algorithmes ;
    « Activé / Coupé » règle son mélange (B est coupé d'origine) ; routage et équilibre.
    Tout s'écrit là où écrivent les encodeurs (son de base, verrou de pas ou scène). */
function DistEngines({ channel, slot, module: m, values }: { channel: number; slot: number; module: ModuleDesc; values: number[] }) {
  const [sel, setSel] = useState<"A" | "B" | null>(null); // moteur dont la liste des algorithmes est ouverte
  const target = useWriteTarget(channel);
  const base = layout().insertBase + slot * layout().insertSlots;
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const plainOf = (id: string) => { const i = idx(id); return i < 0 ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) writeParam(target, globalOf(channel, base + i), toNorm(m.params[i], plain)); };
  const algo = m.params[idx("typeA")]?.choices ?? [];
  const routings = m.params[idx("routing")]?.choices ?? [];
  const routing = plainOf("routing");
  const balance = plainOf("balance") / 100;
  const engines = (["A", "B"] as const).map((e) => ({
    e, type: plainOf(`type${e}`), drive: plainOf(`drive${e}`) / 100, mix: plainOf(`mix${e}`), bias: plainOf(`bias${e}`) / 100 * 0.6,
  }));
  const cur = engines.find((x) => x.e === sel);
  return (
    <div className="dist" onKeyDown={(ev) => { if (ev.key === "Escape" && sel) { ev.preventDefault(); setSel(null); } }}>
      {engines.map(({ e, type, drive, mix, bias }) => (
        <div key={e} role="button" tabIndex={0} aria-expanded={sel === e} aria-haspopup="listbox"
          className={`dist-engine${mix <= 0 ? " is-off" : ""}${sel === e ? " is-selected" : ""}`}
          onClick={() => setSel(sel === e ? null : e)} onKeyDown={(ev) => { if (ev.key === "Enter" || ev.key === " ") { ev.preventDefault(); setSel(sel === e ? null : e); } }}
          title={`Moteur ${e} : toucher pour choisir son algorithme`}>
          <span className="dist-letter">{e}</span>
          <div className="dist-info">
            <span className="dist-type">{algo[type]} <span className="dist-caret" aria-hidden>▾</span></span>
            <span className="dist-sub">Drive {Math.round(drive * 100)} %{mix > 0 && mix < 100 ? ` · ${Math.round(mix)} %` : ""}</span>
          </div>
          <button type="button" className={`dist-toggle${mix > 0 ? " on" : ""}`}
            onClick={(ev) => { ev.stopPropagation(); set(`mix${e}`, mix > 0 ? 0 : 100); }}
            aria-label={`Moteur ${e} ${mix > 0 ? "activé : couper" : "coupé : activer"}`}>{mix > 0 ? "Activé" : "Coupé"}</button>
          <DistCurve type={type} drive={drive} bias={bias} />
        </div>
      ))}
      {cur && (
        <div className="dist-pop" role="listbox" aria-label={`Algorithme du moteur ${cur.e}`}>
          <span className="detail-label">Algorithme du moteur {cur.e}{cur.mix <= 0 ? " — coupé : en choisir un l'active" : ""}</span>
          <div className="dist-algos">
            {algo.map((a, i) => (
              <button key={a} type="button" role="option" aria-selected={i === cur.type} className={i === cur.type ? "on" : ""}
                onClick={() => { set(`type${cur.e}`, i); if (cur.mix <= 0) set(`mix${cur.e}`, 100); setSel(null); }} title={a}>{a}</button>
            ))}
          </div>
        </div>
      )}
      <div className="dist-routing">
        <span className="detail-label">Routage</span>
        <div className="routing-chips">
          {routings.map((r, i) => (
            <button key={r} type="button" className={i === routing ? "on" : ""} onClick={() => set("routing", i)}>{r}</button>
          ))}
        </div>
        <div className="balance" aria-label="Équilibre A/B">
          <span>A</span><span className="balance-bar"><span style={{ left: `${balance * 100}%` }} /></span><span>B</span>
        </div>
      </div>
    </div>
  );
}

/** Égaliseur : courbe des bandes et, pour l'égaliseur, spectre de sa sortie (12 images / s). */
function EqWithAnalyser({ channel, slot, bands, selected, analyse }: { channel: number; slot: number; bands: EqBand[]; selected: number; analyse: boolean }) {
  const [spectrum, setSpectrum] = useState<number[] | undefined>(undefined);
  const playing = useStore((s) => s.tick.t.playing);
  useEffect(() => {
    if (!analyse) return;
    let alive = true;
    const id = window.setInterval(async () => { const sp = await actions.spectrum(channel, slot, 96); if (alive) setSpectrum(sp.length ? sp : undefined); }, 83);
    return () => { alive = false; window.clearInterval(id); };
  }, [analyse, channel, slot, playing]);
  return <EqCurve bands={bands} selected={selected} spectrum={spectrum} />;
}

export function SectionVisual({ channel, area, slot, module: m, section, lane }: { channel: number; area: Area; slot: number; module: ModuleDesc; section: SectionDesc; lane: number }) {
  const values = useValues(channel, area, slot, m);
  const meters = useStore((s) => (s.focus === channel ? s.tick.fxm[slot] : null));
  const wave = useStore((s) => s.waves[`${channel}:${section.visual === "zone" ? 0 : lane}`]);
  const samples = useStore((s) => s.channels[channel]?.samples);
  const hasSample = !!samples?.[section.visual === "zone" ? 0 : lane];
  useEffect(() => {
    if ((section.visual === "zone" || section.visual === "sample" || section.visual === "drumpad:sample") && hasSample && wave === undefined)
      void actions.wave(channel, section.visual === "zone" || section.visual === "sample" ? 0 : lane);
  }, [section.visual, hasSample, wave, channel, lane]);

  const plain = (k: number) => toPlain(m.params[k], values[k] ?? 0);
  const sp = section.params;
  const v = section.visual;

  if (v === "env" || v === "amp") {
    const a = findPlain(m, values, section, /(^|_)(a|aa|fa|ea|m1a|m2a|e[123]a|attack)$/) ?? 2;
    const d = findPlain(m, values, section, /(^|_)(d|ad|fd|ed|m1d|m2d|e[123]d|decay)$/) ?? 300;
    const s = (findPlain(m, values, section, /(^|_)(s|as|fs|es|m1s|m2s|e[123]s|sustain)$/) ?? 70) / 100;
    const r = findPlain(m, values, section, /(^|_)(r|ar|fr|er|m1r|m2r|e[123]r|release|ampdecay)$/) ?? 300;
    return <AdsrView a={a} d={d} s={s} r={r} />;
  }
  if (v === "filter") {
    const hz = findPlain(m, values, section, /cutoff|facutoff|f1cutoff/) ?? 2000;
    const reso = (findPlain(m, values, section, /reso|fareso|f1reso/) ?? 0) / 100;
    const typeIdx = sp.find((k) => /type|mode|slope|ftype|f1mode|fmode/.test(m.params[k].id));
    const choice = typeIdx !== undefined ? (m.params[typeIdx].choices?.[plain(typeIdx)] ?? "") : "";
    const mode = /haut/i.test(choice) ? "hp" : /bande/i.test(choice) ? "bp" : "lp";
    const poles = /12/.test(choice) ? 2 : /18/.test(choice) ? 3 : /6 dB/.test(choice) ? 1 : 4;
    return <FilterView hz={hz} reso={reso} mode={mode} poles={m.type === "acid" ? 4 : poles} />;
  }
  if (v === "osc" || v === "wave") {
    const shapeIdx = sp.find((k) => /shape|pos/.test(m.params[k].id) && !m.params[k].choices);
    const waveIdx = sp.find((k) => /wave/.test(m.params[k].id) && m.params[k].choices);
    if (shapeIdx !== undefined) {
      const p = m.params[shapeIdx];
      const val = plain(shapeIdx);
      return <ShapeView shape={p.max > 4 ? Math.min(4, (val / p.max) * 4) : val} label={formatPlain(p, val)} />;
    }
    if (waveIdx !== undefined) {
      const name = m.params[waveIdx].choices?.[plain(waveIdx)] ?? "";
      const map: Record<string, number> = { Sinus: 0, Triangle: 1, Scie: 2, "Carré": 3, Impulsion: 4 };
      return <ShapeView shape={map[name.split(" ")[0]] ?? 2} label={name} />;
    }
    return <ShapeView shape={2} />;
  }
  if (v === "dist") return <DistEngines channel={channel} slot={slot} module={m} values={values} />;
  if (v === "master") return <MasterMeters meters={meters ?? []} />;
  if (v === "comp") {
    const thr = paramPlain(m, values, "threshold") ?? (m.type === "opto" ? -(paramPlain(m, values, "peak") ?? 40) * 0.42 : m.type === "fet" ? -18 : m.type === "limiter" ? paramPlain(m, values, "ceiling") ?? 0 : -18);
    const ratioPlain = paramPlain(m, values, "ratio");
    const ratio = m.type === "bus" ? [2, 4, 10][ratioPlain ?? 1] : m.type === "fet" ? [4, 8, 12, 20, 30][ratioPlain ?? 0] : m.type === "opto" ? ((paramPlain(m, values, "mode") ?? 0) > 0 ? 12 : 3)
      : m.type === "limiter" ? 50 : (ratioPlain ?? 3);
    const knee = paramPlain(m, values, "knee") ?? (m.type === "opto" ? 8 : 2);
    return <CompCurve threshold={thr} ratio={ratio} knee={knee} gr={meters?.[0] ?? 0} />;
  }
  if (v === "distfilter") {
    const tA = paramPlain(m, values, "preType") ?? 0, fA = paramPlain(m, values, "preFreq") ?? 1200, rA = (paramPlain(m, values, "preReso") ?? 0) / 100;
    const tB = paramPlain(m, values, "preTypeB") ?? 0, fB = paramPlain(m, values, "preFreqB") ?? 1200, rB = (paramPlain(m, values, "preResoB") ?? 0) / 100;
    const names = m.params.find((p) => p.id === "preType")?.choices ?? [];
    const qA = 0.5 + 9.5 * rA * rA, qB = 0.5 + 9.5 * rB * rB;
    const curveA = (f: number) => tA === 0 ? 0 : tA === 5 ? 20 * Math.log10(Math.max(1e-4, Math.abs(Math.cos(Math.PI * f / Math.max(40, fA))))) : svfDb(tA, f, fA, qA);
    return <ResponseView marks={[tA ? fA : 0, tB ? fB : 0].filter((x) => x > 0)} curves={[
      { db: curveA, color: "var(--led-amber)", label: `A : ${names[tA] ?? ""}`, off: tA === 0 },
      { db: (f) => (tB === 0 ? 0 : svfDb(tB, f, fB, qB)), color: "var(--led-blue)", label: `B : ${names[tB] ?? ""}${(paramPlain(m, values, "mixB") ?? 0) <= 0 ? " (moteur B coupé)" : ""}`, off: tB === 0 },
    ]} />;
  }
  if (v === "distpost") {
    const lo = paramPlain(m, values, "postLow") ?? 0, hi = paramPlain(m, values, "postHigh") ?? 0;
    return <ResponseView marks={[180, 4500]} curves={[
      { db: (f) => lo / (1 + Math.pow(f / 180, 2.2)) + hi / (1 + Math.pow(4500 / f, 2.2)), color: "var(--led-amber)", label: `Graves ${lo > 0 ? "+" : ""}${lo.toFixed(1)} dB · aigus ${hi > 0 ? "+" : ""}${hi.toFixed(1)} dB` },
    ]} />;
  }
  if (v === "distfb") {
    const g = 0.95 * (paramPlain(m, values, "feedback") ?? 0) / 100, T = (paramPlain(m, values, "fbTime") ?? 4) / 1000;
    const db = (f: number) => { const w = 2 * Math.PI * f * T; return -20 * Math.log10(Math.max(1e-4, Math.hypot(1 - g * Math.cos(w), g * Math.sin(w)))); };
    return <ResponseView marks={[1 / Math.max(1e-4, T)]} curves={[{ db, color: "var(--led-amber)", label: g <= 0 ? "Réinjection coupée" : `Résonances tous les ${Math.round(1 / T)} Hz` , off: g <= 0 }]} />;
  }
  if (v === "gate") {
    const thr = paramPlain(m, values, "gThresh") ?? -50;
    return <GateCurve threshold={thr} range={paramPlain(m, values, "gRange") ?? 40} hyst={paramPlain(m, values, "gHyst") ?? 0} gr={meters?.[1] ?? 0} off={thr <= -79.9} />;
  }
  if (v === "deess") {
    return <DeessView hz={paramPlain(m, values, "dsFreq") ?? 6500} gr={meters?.[2] ?? 0} max={paramPlain(m, values, "dsAmount") ?? 0}
      wide={(paramPlain(m, values, "dsMode") ?? 0) > 0} listen={(paramPlain(m, values, "dsListen") ?? 0) > 0} />;
  }
  if (v === "eq") {
    const bands: EqBand[] = Array.from({ length: 8 }, (_, b) => ({
      on: (paramPlain(m, values, `b${b + 1}_on`) ?? 0) > 0,
      type: paramPlain(m, values, `b${b + 1}_type`) ?? 0,
      freq: paramPlain(m, values, `b${b + 1}_freq`) ?? 1000,
      gain: (paramPlain(m, values, `b${b + 1}_gain`) ?? 0) + (meters && slot >= 0 ? 0 : 0),
      q: paramPlain(m, values, `b${b + 1}_q`) ?? 1,
    }));
    const sel = Number(/band(\d+)/.exec(section.id)?.[1] ?? 1) - 1;
    return <EqWithAnalyser channel={channel} slot={slot} bands={bands} selected={sel} analyse={m.type === "eq"} />;
  }
  if (v === "zone" || v === "sample" || (v.startsWith("drumpad") && hasSample)) {
    const start = section.params.find((k) => /start$/.test(m.params[k].id));
    const end = section.params.find((k) => /end$/.test(m.params[k].id));
    const zones: [number, number][] = [];
    if (v === "zone") for (let z = 0; z < 16; ++z) {
      const a = paramPlain(m, values, `z${String(z + 1).padStart(2, "0")}_start`), b = paramPlain(m, values, `z${String(z + 1).padStart(2, "0")}_end`);
      if (a !== undefined && b !== undefined && b > a) zones.push([a / 100, b / 100]);
    }
    return <SampleView peaks={wave?.peaks ?? null} start={start !== undefined ? plain(start) / 100 : undefined} end={end !== undefined ? plain(end) / 100 : undefined} zones={zones} />;
  }
  if (v === "padfx") {
    const fx = paramPlainIn(m, values, section, /_fx$/) ?? 0;
    const amt = paramPlainIn(m, values, section, /_fxAmt$/) ?? 0;
    const tone = paramPlainIn(m, values, section, /_fxTone$/) ?? 0;
    const sa = paramPlainIn(m, values, section, /_sendA$/) ?? 0, sb = paramPlainIn(m, values, section, /_sendB$/) ?? 0;
    const names = m.params[section.params[0]]?.choices ?? [];
    return (
      <div className="padfx">
        <div className="padfx-chain">
          <span className="block">Voix du pad</span><span className="flow-arrow" aria-hidden />
          <span className={`block${fx > 0 ? " block--on" : ""}`}>{names[fx] ?? "Aucun"}{fx > 0 ? ` · ${Math.round(amt)} %` : ""}</span><span className="flow-arrow" aria-hidden />
          <span className="block">Niveau · pan</span>
        </div>
        <div className="padfx-sends">
          {([["A", sa], ["B", sb]] as const).map(([k, v2]) => (
            <div key={k} className="padfx-send"><span>Envoi {k}</span><span className="fx-send-bar"><span style={{ width: `${v2}%` }} /></span><b className="tabular">{Math.round(v2)} %</b></div>
          ))}
        </div>
        <p className="sound-hint">Tonalité : {fx === 2 || fx === 3 ? "fréquence du filtre" : fx === 4 ? "attaque" : fx === 5 ? "fréquence d'échantillonnage" : "brillance"} ({Math.round(tone)} %). Les envois vont directement aux retours A et B.</p>
      </div>
    );
  }
  if (v === "padfilter") {
    const type = paramPlainIn(m, values, section, /_ftype$/) ?? 0;
    const names = m.params[section.params[0]]?.choices ?? [];
    const hz = paramPlainIn(m, values, section, /_fcut$/) ?? 4000;
    const reso = (paramPlainIn(m, values, section, /_freso$/) ?? 0) / 100;
    const env = paramPlainIn(m, values, section, /_fenv$/) ?? 0;
    const name = names[type] ?? "";
    if (type === 0) return <div className="padfx"><p className="sound-hint">Filtre désactivé : choisissez un type (encodeur 1) — échelle 24 dB, 12 dB, passe-bande, passe-haut, réjection.</p></div>;
    const mode = /haut/i.test(name) ? "hp" : /bande/i.test(name) ? "bp" : /jection/i.test(name) ? "notch" : "lp";
    return (
      <div className="padfilter-view">
        <FilterView hz={hz} reso={reso} mode={mode} poles={/24/.test(name) ? 4 : 2} />
        <p className="sound-hint">{name}{env !== 0 ? ` · enveloppe ${env > 0 ? "+" : "−"}${Math.round(Math.abs(env) * 0.07 * 10) / 10} oct.` : ""}</p>
      </div>
    );
  }
  if (v === "padamp") {
    const a = paramPlainIn(m, values, section, /_aatk$/) ?? 0;
    const h = paramPlainIn(m, values, section, /_ahold$/) ?? 0;
    const d = paramPlainIn(m, values, section, /_adec$/) ?? 8000;
    const voice = findPlain(m, values, section, new RegExp(`^${section.params.length ? m.params[section.params[0]].id.slice(0, 4) : "p01_"}decay$`)) ?? 300;
    return <AdsrView a={Math.max(1, a)} d={h} s={1} r={d >= 7999 ? voice : Math.min(d, voice)} />;
  }
  if (v.startsWith("drumpad")) {
    const decay = findPlain(m, values, section, /_decay$/) ?? 300;
    return <AdsrView a={1} d={decay * 0.6} s={0} r={decay * 0.4} />;
  }
  if (v === "global" && area === "inst") {
    // Vue d'ensemble : l'architecture de l'instrument en blocs (oscillateur → filtre → enveloppe)
    const pick = (tab: string) => m.sections.find((x) => x.tab === tab && !isPerPad(x));
    const blocks = [pick("osc"), pick("filter"), pick("env") ?? pick("amp")].filter((x): x is SectionDesc => !!x);
    return (
      <div className="overview">
        {blocks.length > 0 && (
          <div className="synth-flow synth-flow--compact" aria-label="Architecture de l'instrument">
            {blocks.map((b, i) => (
              <div key={b.id} className="overview-cell">
                {i > 0 && <span className="flow-arrow" aria-hidden />}
                <button type="button" className="block" onClick={() => setUi({ soundTab: b.tab, soundSection: 0 })}>
                  <span className="block-title">{b.name}</span>
                  <SectionVisual channel={channel} area={area} slot={slot} module={m} section={b} lane={lane} />
                </button>
              </div>
            ))}
          </div>
        )}
        <FlowView module={m} section={section} values={values} channel={channel} area={area} slot={slot} />
      </div>
    );
  }
  if (v === "delay" || v === "reverb" || v === "lfo" || v === "seq" || v === "func" || v === "follow" || v === "mix" || v === "global") {
    return <FlowView module={m} section={section} values={values} channel={channel} area={area} slot={slot} />;
  }
  return <FlowView module={m} section={section} values={values} channel={channel} area={area} slot={slot} />;
}

/** Valeurs d'une section en liste (lisible d'un coup d'œil). Chaque valeur se règle aussi en
    la tirant vers le haut ou le bas (souris ou doigt). */
function FlowView({ module: m, section, values, channel, area, slot }: { module: ModuleDesc; section: SectionDesc; values: number[]; channel: number; area: Area; slot: number }) {
  return (
    <ul className="param-grid">
      {section.params.map((k) => (
        <ParamRow key={k} global={paramAddress(channel, area, slot, k)} name={m.params[k].name} text={formatPlain(m.params[k], toPlain(m.params[k], values[k] ?? 0))} />
      ))}
    </ul>
  );
}

function ParamRow({ global, name, text }: { global: number; name: string; text: string }) {
  const binding = useParam(global);
  const { handlers } = useValueDrag(binding);
  // v0.32.1 : clic droit (appui long au doigt) = menu du réglage, comme sur les boutons
  return (
    <li className="param-row--drag" title={`${name} : glisser vers le haut ou le bas — clic droit : valeur exacte, modulation, MIDI`} {...handlers}>
      <ParamMenuZone binding={binding} className="pm-zone--row"><span>{name}</span><b className="tabular">{text}</b></ParamMenuZone>
    </li>
  );
}

/** Onglet Modulation : les sources de la famille (son, ou effets et mixage), et pour la source
    choisie : son type, ses destinations (profondeur, retrait) et l'affectation guidée
    (« Affecter », puis toucher les encodeurs à moduler). */
export function ModSources({ channel, scope }: { channel: number; scope: ModScope }) {
  const c = useStore((s) => s.channels[channel]);
  const modSlot = useScopedModSlot(scope);
  const assign = useStore((s) => s.ui.modAssign);
  const cat = useStore((s) => s.catalog);
  const tracks = useStore((s) => s.tracks);
  const live = useStore((s) => (s.focus === channel ? s.tick.mods : undefined));
  if (!c || !cat) return null;
  const ms = cat.modSource;
  const sources = MOD_SCOPE_SOURCES[scope];
  const selVals = c.mod[modSlot];
  const selType = modTypeOf(selVals);
  const selName = `${MOD_TYPES[selType]} ${modSlot + 1}`;
  const selColor = MOD_SOURCE_COLORS[modSlot];
  const assigning = !!assign && assign.channel === channel && assign.source === modSlot;
  const dests = c.mods.filter((m) => m.source === modSlot);
  const typeIdx = ms.params.findIndex((p) => p.id === "type");
  const retrigIdx = ms.params.findIndex((p) => p.id === "retrig");
  const loopIdx = ms.params.findIndex((p) => p.id === "loop");
  const retrig = Math.min(2, Math.max(0, Math.floor((selVals?.[retrigIdx] ?? 0) * 3)));
  const loop = (selVals?.[loopIdx] ?? 0) >= 0.5;
  const here = live?.[modSlot]?.[1] ?? -1;
  return (
    <div className={`mod-panel mod-panel--${scope}`}>
      <div className="mod-scope">
        <b>{MOD_SCOPE_TITLE[scope]}</b>
        <span>{scope === "sound" ? "Sources 1 à 4 : réglages de l'instrument (filtre, oscillateurs, enveloppes, pads…)" : "Sources 5 et 6 : effets, volume, panoramique, envois"}</span>
      </div>
      <div className="mod-slots" style={{ gridTemplateColumns: `repeat(${sources.length}, minmax(0, 1fr))`, gridTemplateRows: "1fr" }}>
        {sources.map((i) => {
          const vals = c.mod[i] ?? [];
          const type = modTypeOf(vals);
          const color = MOD_SOURCE_COLORS[i];
          const steps = Array.from({ length: 16 }, (_, k) => toPlain(ms.params[24 + k], vals[24 + k] ?? 0) / 100);
          const levels = [12, 13, 14, 15].map((k) => toPlain(ms.params[k], vals[k] ?? 0) / 100);
          const shape = toPlain(ms.params[4], vals[4] ?? 0);
          const n = c.mods.filter((m) => m.source === i).length;
          return (
            <button key={i} type="button" className={`mod-slot${i === modSlot ? " is-selected" : ""}`} style={{ "--mod": color } as React.CSSProperties}
              onClick={() => setUi({ modSlot: i, modAssign: assign ? { channel, source: i } : null })} aria-pressed={i === modSlot}>
              <span className="mod-slot-head"><span className="mod-dot" style={{ background: color }} />{MOD_TYPES[type]} {i + 1}
                <span className="mod-slot-src">{n === 0 ? "libre" : `${n} dest.`}</span></span>
              <ModSourceView type={type} shape={shape} steps={steps} levels={levels} color={color} live={live?.[i]} />
            </button>
          );
        })}
      </div>
      <div className="mod-dests" style={{ "--mod": selColor } as React.CSSProperties}>
        <div className="mod-dests-head">
          <span className="mod-dot" style={{ background: selColor }} />
          <b>Source {modSlot + 1}</b>
          <span className="mod-types" role="radiogroup" aria-label={`Type de la source ${modSlot + 1}`}>
            {MOD_TYPES.map((t, k) => (
              <button key={t} type="button" role="radio" aria-checked={k === selType} className={k === selType ? "on" : ""}
                onClick={() => {
                  actions.setParam(paramAddress(channel, "mod", modSlot, typeIdx), (k + 0.5) / MOD_TYPES.length);
                  // fonction : repart à chaque note (enveloppe de modulation), le cas le plus courant
                  if (k === 1 && k !== selType) actions.setParam(paramAddress(channel, "mod", modSlot, retrigIdx), 1.5 / 3);
                }}>{t}</button>
            ))}
          </span>
          <button type="button" className={`mod-assign${assigning ? " on" : ""}`}
            onClick={() => setUi({ modAssign: assigning ? null : { channel, source: modSlot } })}>
            {assigning ? "Terminer" : "Affecter à un réglage…"}
          </button>
          <span className="mod-dests-hint">{assigning
            ? (scope === "sound" ? "Touchez les encodeurs de l'écran Son à moduler" : "Touchez les encodeurs des écrans Effets ou Mixeur à moduler")
            : selType === 0 ? "Encodeurs : synchro au tempo (Libre / Tempo), vitesse ou division, forme, phase…" : selType === 3 ? "Tirez les barres pour régler les 16 pas ; encodeurs : division, longueur, lissage, redéclenchement" : selType === 1 ? "Encodeurs : temps et niveaux des 4 segments" : "Les encodeurs règlent la forme de la source"}</span>
        </div>
        {selType === 1 && (
          <div className="mod-func-opts">
            <span className="mod-opt-label">Départ</span>
            <span className="mod-types" role="radiogroup" aria-label="Départ de la fonction">
              {[["Libre (en boucle)", "La fonction tourne seule, en boucle, sans attendre de note"], ["À chaque note", "La fonction repart à chaque note jouée (pads, clavier, séquenceur)"], ["À chaque mesure", "La fonction repart au début de chaque mesure"]].map(([t, hint], k) => (
                <button key={t} type="button" role="radio" aria-checked={k === retrig} className={k === retrig ? "on" : ""} title={hint}
                  onClick={() => actions.setParam(paramAddress(channel, "mod", modSlot, retrigIdx), (k + 0.5) / 3)}>{t}</button>
              ))}
            </span>
            {retrig !== 0 && <>
              <span className="mod-opt-label">Après le 4e segment</span>
              <span className="mod-types" role="radiogroup" aria-label="Boucle de la fonction">
                {["S'arrête", "Recommence"].map((t, k) => (
                  <button key={t} type="button" role="radio" aria-checked={(k === 1) === loop} className={(k === 1) === loop ? "on" : ""}
                    onClick={() => actions.setParam(paramAddress(channel, "mod", modSlot, loopIdx), k === 1 ? 0.75 : 0.25)}>{t}</button>
                ))}
              </span>
            </>}
          </div>
        )}
        {selType === 3 && (
          <div className="mod-steps" aria-label="Pas du séquenceur de modulation">
            {Array.from({ length: 16 }, (_, k) => {
              const idx = ms.params.findIndex((p) => p.id === `step${k + 1}`);
              const v = toPlain(ms.params[idx], selVals?.[idx] ?? 0) / 100;
              return (
                <DragParam key={k} global={paramAddress(channel, "mod", modSlot, idx)} label={`Pas ${k + 1}`} className={`mod-step${k === here ? " here" : ""}`}>
                  <span style={{ height: `${Math.max(4, v * 100)}%` }} />
                </DragParam>
              );
            })}
          </div>
        )}
        {dests.length === 0 ? (
          <p className="mod-dests-empty">Aucune destination. « Affecter à un réglage… », puis touchez un encodeur : il sera modulé par {selName}.</p>
        ) : (
          <ul className="mod-dest-list">
            {dests.map((m) => (
              <li key={m.target}>
                <span className="mod-dest-name">{targetName(cat, tracks, channel, m.target)}</span>
                <DragParam global={globalOf(channel, m.amountSlot)} label={`Profondeur ${selName} → ${targetName(cat, tracks, channel, m.target)}`} className="mod-dest-amount">
                  <span className="mod-dest-bar"><span style={{ left: `${50 + Math.min(0, m.amount) * 50}%`, width: `${Math.abs(m.amount) * 50}%` }} /></span>
                  <b className="tabular">{m.amount >= 0 ? "+" : "−"}{Math.round(Math.abs(m.amount) * 100)} %</b>
                </DragParam>
                <button type="button" className="mod-dest-remove" onClick={() => void actions.removeMod(channel, m.target)}
                  aria-label={`Retirer ${targetName(cat, tracks, channel, m.target)}`} title="Retirer cette destination">×</button>
              </li>
            ))}
          </ul>
        )}
      </div>
    </div>
  );
}

function targetName(cat: Catalog, tracks: TracksData, channel: number, local: number) {
  const L = cat.layout;
  if (local < L.instrumentBase) return mixerFor(cat, channel).params[local]?.name ?? "?";
  if (local < L.insertBase) return cat.instruments.find((i) => i.type === tracks.tracks[channel]?.instrument)?.params[local - L.instrumentBase]?.name ?? "?";
  const slot = Math.floor((local - L.insertBase) / L.insertSlots);
  return cat.effects.find((e) => e.type === tracks.inserts[channel]?.[slot])?.params[(local - L.insertBase) % L.insertSlots]?.name ?? "?";
}

export { globalOf };
