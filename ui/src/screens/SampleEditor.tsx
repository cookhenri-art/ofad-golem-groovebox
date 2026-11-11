// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 (lot 2) : ÉDITEUR D'ÉCHANTILLON UNIQUE, le même partout (façades, pads des batteries, écran
// Pistes, prise audio envoyée dans un pad). Forme d'onde et sélection, Inverser, Normaliser, Fondus,
// Rogner, Silence, et « Traiter… » : une chaîne de 4 effets quelconques (Restauration comprise)
// calculée HORS TEMPS RÉEL (aucune latence pendant le jeu), pré-écoutée, puis appliquée à la place
// de l'échantillon : les pads et les motifs qui le jouent ne changent pas. Annuler : 8 versions.

import { useEffect, useRef, useState } from "react";
import { actions, getState, setUi, showMessage, useStore, type ChainSlotUi, type ProcStatus } from "../engine/store";
import { ChainEditor, loadChain, saveChain } from "./ChainEditor";

const OPS: [number, string, string][] = [
  [0, "Inverser", "La sélection jouée à l'envers"], [1, "Normaliser", "Crête de la sélection à -0,3 dB"],
  [2, "Fondu d'entrée", "Sur la sélection (avant elle : silence)"], [3, "Fondu de sortie", "Sur la sélection (après elle : silence)"],
  [4, "Rogner", "Ne garder que la sélection"], [5, "Silence", "La sélection devient silencieuse"],
];
const clock = (s: number) => `${Math.floor(s / 60)}:${(s % 60).toFixed(1).padStart(4, "0")}`;
const readBool = (k: string, d: boolean) => { try { const v = localStorage.getItem(`grv.${k}`); return v === null ? d : v === "1"; } catch { return d; } };
const writeBool = (k: string, v: boolean) => { try { localStorage.setItem(`grv.${k}`, v ? "1" : "0"); } catch { /* stockage indisponible */ } };

export function SampleEditor() {
  const target = useStore((s) => s.ui.sampleEdit);
  if (!target) return null;
  return <SampleEditorBody key={`${target.track}:${target.index}`} track={target.track} index={target.index} />;
}

function SampleEditorBody({ track, index }: { track: number; index: number }) {
  const wave = useStore((s) => s.waves[`${track}:${index}`]);
  const name = useStore((s) => s.channels[track]?.samples?.[index] ?? "");
  const trackName = useStore((s) => s.tracks.tracks[track]?.name ?? `Piste ${track + 1}`);
  const [a, setA] = useState(0);
  const [b, setB] = useState(1);
  const [chain, setChainState] = useState<ChainSlotUi[]>(() => loadChain("proc"));
  const [keepTail, setKeepTail] = useState(() => readBool("procTail", true));
  const [keepOrig, setKeepOrig] = useState(() => readBool("procOrig", false));
  const [st, setSt] = useState<ProcStatus | null>(null);
  const [undo, setUndo] = useState(0);
  const box = useRef<HTMLDivElement>(null);
  const drag = useRef<number | null>(null);
  const wasBusy = useRef(false);

  const setChain = (c: ChainSlotUi[]) => { setChainState(c); saveChain("proc", c); };
  useEffect(() => { if (wave === undefined && name) void actions.wave(track, index); }, [wave, name, track, index]);
  useEffect(() => { void actions.sampleUndoCount(track, index).then(setUndo); }, [track, index, name]);
  const close = () => { void actions.procAudition(false); setUi({ sampleEdit: null }); };
  useEffect(() => {
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape" && !e.defaultPrevented) { e.preventDefault(); close(); } };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  });

  // suivi du calcul : toutes les 0,2 s tant qu'il tourne ou qu'une pré-écoute joue
  const busy = !!st?.busy, playing = !!st?.auditioning;
  useEffect(() => {
    if (!busy && !playing) return;
    let alive = true;
    const id = window.setInterval(async () => { const s = await actions.procStatus(); if (alive) setSt(s); }, 200);
    return () => { alive = false; window.clearInterval(id); };
  }, [busy, playing]);
  useEffect(() => {
    if (wasBusy.current && st && !st.busy) {
      if (st.error) showMessage(`Traitement : ${st.error}`);
      else if (!st.preview) {
        void actions.sampleReplaced(track, index);
        void actions.sampleUndoCount(track, index).then(setUndo);
        showMessage(`Traité et remplacé${st.tailSeconds > 0.05 ? ` (queue gardée : ${st.tailSeconds.toFixed(1).replace(".", ",")} s)` : ""} — « Annuler » rend la version d'avant`);
      }
    }
    wasBusy.current = !!st?.busy;
  }, [st, track, index]);

  const run = async (preview: boolean) => {
    const ok = await actions.procStart(track, index, a, b, chain, keepTail, preview, keepOrig && !preview);
    if (!ok) return;
    setSt(await actions.procStatus());
  };
  const whole = a === 0 && b === 1;
  const at = (e: React.PointerEvent) => { const r = box.current?.getBoundingClientRect(); return r ? Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)) : 0; };
  const peaks = wave?.peaks ?? [];
  const seconds = wave?.seconds ?? 0;
  const nBars = peaks.length / 2;
  const path = nBars > 0 ? peaks.reduce((acc, _, k) => {
    if (k % 2) return acc;
    const x = (k / 2 / nBars) * 1000, lo = 50 - peaks[k] * 48, hi = 50 - peaks[k + 1] * 48;
    return `${acc}M${x.toFixed(1)} ${hi.toFixed(1)}V${lo.toFixed(1)}`;
  }, "") : "";
  const active = chain.filter((c) => c.type && !c.bypass).length;

  return (
    <div className="sedit" role="dialog" aria-label="Éditeur d'échantillon">
      <header className="sedit-head">
        <b className="sedit-title">ÉCHANTILLON</b>
        <span className="sedit-name">{trackName} · {name || "emplacement vide"}{seconds > 0 ? ` · ${clock(seconds)}` : ""}</span>
        <span style={{ flex: 1 }} />
        {OPS.map(([op, label, hint]) => (
          <button key={op} type="button" className="touch touch--small" title={hint} disabled={!name || busy}
            onClick={async () => { await actions.editSample(track, index, op, a, b); void actions.sampleUndoCount(track, index).then(setUndo); if (op === 4) { setA(0); setB(1); } }}>{label}</button>
        ))}
        <button type="button" className="touch touch--small" disabled={undo <= 0 || busy} title="Rendre la version d'avant (8 gardées)"
          onClick={async () => { await actions.sampleUndo(track, index); setUndo(await actions.sampleUndoCount(track, index)); }}>Annuler{undo > 0 ? ` (${undo})` : ""}</button>
        <button type="button" className="touch touch--small" onClick={close}>Fermer</button>
      </header>
      <div ref={box} className="sedit-wave" aria-label={whole ? "Forme d'onde : tout le son" : `Sélection ${clock(a * seconds)} – ${clock(b * seconds)}`}
        onPointerDown={(e) => { (e.target as Element).setPointerCapture?.(e.pointerId); const x = at(e); drag.current = x; setA(x); setB(x); }}
        onPointerMove={(e) => { if (drag.current === null) return; const x = at(e); setA(Math.min(drag.current, x)); setB(Math.max(drag.current, x)); }}
        onPointerUp={() => { if (drag.current !== null && Math.abs(b - a) < 0.003) { setA(0); setB(1); } drag.current = null; }}>
        {name ? (
          <svg viewBox="0 0 1000 100" preserveAspectRatio="none" aria-hidden>
            <path d={path} className="sedit-wave-path" vectorEffect="non-scaling-stroke" />
          </svg>
        ) : <span className="sedit-empty">Emplacement vide : chargez d'abord un son.</span>}
        {!whole && <span className="sedit-sel" style={{ left: `${a * 100}%`, width: `${(b - a) * 100}%` }} />}
        <span className="sedit-hint">{whole ? "Tout le son · glisser pour choisir une zone" : `Zone ${clock(a * seconds)} – ${clock(b * seconds)} (bords fondus sur 5 ms) · toucher sans glisser : tout le son`}</span>
      </div>
      <div className="sedit-body">
        <section className="sedit-box sedit-chain">
          <ChainEditor chain={chain} onChange={setChain} title="Traiter · 4 effets (hors temps réel, sans latence au jeu)" />
        </section>
        <section className="sedit-box sedit-result">
          <span className="chain-title">Résultat</span>
          <div className="sedit-row">
            <button type="button" className="touch" disabled={!name || busy || active === 0} onClick={() => void run(true)}
              title="Calcule le son traité et le fait entendre, sans rien remplacer">▶ Pré-écoute</button>
            <button type="button" className="touch" disabled={!playing} onClick={() => { void actions.procAudition(false); setSt((s) => (s ? { ...s, auditioning: false } : s)); }}>■ Arrêter</button>
            <button type="button" className="touch" disabled={!st?.ready || !st.preview || busy || playing}
              onClick={async () => { await actions.procAudition(true); setSt(await actions.procStatus()); }} title="Réécouter le dernier résultat">Réécouter</button>
          </div>
          <label className="sedit-check"><input type="checkbox" checked={keepTail} onChange={(e) => { setKeepTail(e.target.checked); writeBool("procTail", e.target.checked); }} />
            Garder la queue (réverbération, delay : le son s'allonge)</label>
          <label className="sedit-check"><input type="checkbox" checked={keepOrig} onChange={(e) => { setKeepOrig(e.target.checked); writeBool("procOrig", e.target.checked); }} />
            Garder aussi l'original (copie dans « Son traité / Originaux »)</label>
          {busy && (
            <div className="sedit-progress" role="progressbar" aria-valuenow={Math.round((st?.progress ?? 0) * 100)} aria-valuemin={0} aria-valuemax={100}>
              <span style={{ width: `${Math.round((st?.progress ?? 0) * 100)}%` }} />
            </div>
          )}
          <small className="sedit-note">
            {busy ? `Calcul ${Math.round((st?.progress ?? 0) * 100)} %…`
              : active === 0 ? "Ajoutez au moins un effet à la chaîne."
              : `${active} effet${active > 1 ? "s" : ""} actif${active > 1 ? "s" : ""}. Le résultat prend la place de l'échantillon : les pads et les motifs qui le jouent ne changent pas.`}
            {st?.latencyMs ? ` Latence des effets compensée (${Math.round(st.latencyMs)} ms).` : ""}
          </small>
          <span style={{ flex: 1 }} />
          <div className="sedit-row">
            {busy && <button type="button" className="touch" onClick={() => void actions.procCancel()}>Annuler le calcul</button>}
            <span style={{ flex: 1 }} />
            <button type="button" className="touch touch--on sedit-apply" disabled={!name || busy || active === 0} onClick={() => void run(false)}>Appliquer et remplacer</button>
          </div>
        </section>
      </div>
    </div>
  );
}

/** Ouvre l'éditeur sur l'emplacement (piste, emplacement). */
/** 1.1 : appui long sur une piste (écran Pistes) : l'éditeur s'ouvre sur son premier échantillon chargé. */
export async function openSampleEditorForTrack(track: number) {
  if (!getState().channels[track]) await actions.refreshChannel(track);
  const smp = getState().channels[track]?.samples;
  if (!smp?.length) { showMessage("Cette piste ne joue pas d'échantillons"); return; }
  const i = smp.findIndex((x) => !!x);
  if (i < 0) { showMessage("Aucun échantillon chargé sur cette piste"); return; }
  openSampleEditor(track, i);
}

export function openSampleEditor(track: number, index: number) {
  if (track < 0 || index < 0) return;
  if (!getState().channels[track]?.samples?.[index]) { showMessage("Emplacement vide : chargez d'abord un son"); return; }
  setUi({ sampleEdit: { track, index } });
}
