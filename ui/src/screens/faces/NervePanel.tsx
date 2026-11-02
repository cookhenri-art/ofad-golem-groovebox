// Groovebox — OFAD — AGPL-3.0-or-later
// Batterie Nerve (v0.26) : façade disposée comme le logiciel dont elle reprend le principe —
// l'éditeur de motif du pad choisi en haut (vélocité, durée ou décalage de chaque pas, en barres),
// la grille 4 × 4 des pads, le pad choisi (forme d'onde, pages dont le précalcul) et le
// compresseur interne. Inscriptions en grands caractères.

import { useRef, useState } from "react";
import type React from "react";
import { actions, setUi, useStore } from "../../engine/store";
import type { ModuleDesc } from "../../engine/protocol";

type LaneKind = "vel" | "len" | "micro";
const LANES: { id: LaneKind; label: string }[] = [
  { id: "vel", label: "Vélocité" }, { id: "len", label: "Durée" }, { id: "micro", label: "Décalage" },
];

/** Hauteur d'une barre (0..1) et valeur à écrire pour un clic à la hauteur y (0 en bas, 1 en haut). */
const SHAPE: Record<LaneKind, { bar: (v: number) => number; at: (y: number) => number; text: (v: number) => string }> = {
  vel: { bar: (v) => v / 127, at: (y) => Math.max(1, Math.round(y * 127)), text: (v) => (v > 0 ? String(v) : "—") },
  len: { bar: (v) => Math.min(1, v / 64), at: (y) => Math.max(1, Math.round(y * 64)), text: (v) => (v % 8 === 0 ? `${v / 8}` : (v / 8).toFixed(1)) },
  micro: { bar: (v) => (v + 32) / 64, at: (y) => Math.round(y * 64 - 32), text: (v) => (v === 0 ? "0" : `${v > 0 ? "+" : "−"}${Math.abs(v)}`) },
};

export function NervePattern({ module: m }: { channel: number; module: ModuleDesc }) {
  const pattern = useStore((s) => s.pattern);
  const lane = useStore((s) => s.ui.lane);
  const page = useStore((s) => s.ui.page);
  const pos = useStore((s) => s.tick.pos);
  const playing = useStore((s) => s.tick.t.playing);
  const [kind, setKind] = useState<LaneKind>("vel");
  const last = useRef(""); // un glisser n'envoie que les valeurs qui changent
  if (!pattern) return null;
  const ln = pattern.lanes[lane];
  const pages = Math.max(1, Math.ceil(pattern.length / 16));
  const pg = Math.min(page, pages - 1);
  const shape = SHAPE[kind];
  const write = (s: number, y: number, clear: boolean) => {
    const key = `${kind}:${s}:${clear ? "x" : shape.at(y)}`;
    if (key === last.current) return;
    last.current = key;
    if (kind === "vel") void actions.seq("seqSetStep", lane, s, clear ? 0 : shape.at(y));
    else if ((ln?.vel[s] ?? 0) > 0) void actions.seq(kind === "len" ? "seqSetLength8" : "seqSetMicro", lane, s, shape.at(y));
  };
  const onPointer = (s: number) => (e: React.PointerEvent<HTMLButtonElement>) => {
    if (e.type === "pointermove" && e.buttons === 0) return;
    const r = e.currentTarget.getBoundingClientRect();
    const y = Math.min(1, Math.max(0, 1 - (e.clientY - r.top) / r.height));
    write(s, y, kind === "vel" && e.type === "pointerdown" && y < 0.06);
  };
  return (
    <div className="nv-pat">
      <div className="nv-pat-head">
        <b>{String(lane + 1).padStart(2, "0")} · {m.padNames[lane]}</b>
        <span className="nv-pat-hint">{kind === "vel" ? "Toucher ou glisser pour écrire un pas ; tout en bas pour l'effacer" : "Les pas écrits seulement ; glisser pour régler"}</span>
        <span className="nv-tabs" role="tablist" aria-label="Valeur montrée">
          {LANES.map((l) => (
            <button key={l.id} type="button" role="tab" aria-selected={kind === l.id} className={kind === l.id ? "on" : ""} onClick={() => setKind(l.id)}>{l.label}</button>
          ))}
        </span>
        {pages > 1 && (
          <span className="nv-pages">
            <button type="button" disabled={pg <= 0} onClick={() => setUi({ page: pg - 1 })} aria-label="Pas précédents">◀</button>
            <span className="tabular">{pg * 16 + 1}–{Math.min(pattern.length, pg * 16 + 16)}</span>
            <button type="button" disabled={pg >= pages - 1} onClick={() => setUi({ page: pg + 1 })} aria-label="Pas suivants">▶</button>
          </span>
        )}
      </div>
      <div className="nv-bars">
        {Array.from({ length: 16 }, (_, i) => {
          const s = pg * 16 + i;
          const out = s >= pattern.length;
          const on = (ln?.vel[s] ?? 0) > 0;
          const v = kind === "vel" ? ln?.vel[s] ?? 0 : kind === "len" ? ln?.len[s] ?? 8 : ln?.micro[s] ?? 0;
          const h = on || kind === "vel" ? shape.bar(v) : 0;
          const here = playing && (pos[lane] ?? -1) === s;
          return (
            <button key={i} type="button" disabled={out} className={`nv-bar g${Math.floor(i / 4)}${on ? " on" : ""}${here ? " here" : ""}`}
              aria-label={`Pas ${s + 1} : ${on ? shape.text(v) : "vide"}`}
              onPointerDown={onPointer(s)} onPointerMove={onPointer(s)}>
              <span className="nv-bar-fill" style={{ height: `${Math.round(h * 100)}%` }} />
              <span className="nv-bar-num">{s + 1}</span>
              {on && <span className="nv-bar-val tabular">{shape.text(v)}</span>}
            </button>
          );
        })}
      </div>
    </div>
  );
}

/** Grille 4 × 4 des pads : toucher un pad le choisit (les pads du bas de l'écran le jouent). */
export function NervePads({ module: m }: { channel: number; module: ModuleDesc }) {
  const lane = useStore((s) => s.ui.lane);
  const lit = useStore((s) => s.litLanes);
  return (
    <div className="nv-pads">
      {Array.from({ length: 16 }, (_, i) => (
        <button key={i} type="button" className={`nv-pad${i === lane ? " is-selected" : ""}${(lit[i] ?? 0) > 0 ? " is-lit" : ""}`} onClick={() => setUi({ lane: i })}>
          <span className="nv-pad-num">{i + 1}</span>
          <span className="nv-pad-name">{m.padNames[i]}</span>
        </button>
      ))}
    </div>
  );
}
