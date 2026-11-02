// Groovebox — OFAD — AGPL-3.0-or-later
// Wave morph : tables d'ondes des oscillateurs 1 et 2 (principe des synthés à balayage d'ondes
// récents). Chaque oscillateur balaye une liste de 1 à 16 ondes, choisies dans la bibliothèque ;
// la position (modulable) dit où on lit entre deux ondes voisines. L'écran montre l'onde lue.

import { useState } from "react";
import { actions, useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";

const SLOTS = 16;

export function WaveScanPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  return (
    <div className="wavescan">
      {[1, 2].map((o) => <WaveOsc key={o} osc={o} channel={channel} module={m} />)}
    </div>
  );
}

function WaveOsc({ osc, channel, module: m }: { osc: number; channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const [pick, setPick] = useState<number | null>(null); // case de la liste dont on choisit l'onde
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const posIdx = idx(`w${osc}pos`), countIdx = idx(`w${osc}n`);
  const posBinding = useParam(posIdx >= 0 ? paramAddress(channel, "inst", 0, posIdx) : null);
  const countBinding = useParam(countIdx >= 0 ? paramAddress(channel, "inst", 0, countIdx) : null);
  const shapes = m.waveShapes ?? [];
  const slotIdx = Array.from({ length: SLOTS }, (_, k) => idx(`w${osc}s${k + 1}`));
  if (posIdx < 0 || countIdx < 0 || slotIdx.some((k) => k < 0) || shapes.length === 0 || !values) return null;
  const names = m.params[slotIdx[0]].choices ?? [];
  const scan = slotIdx.map((k) => Math.round(toPlain(m.params[k], values[k] ?? 0)));
  const count = Math.round(toPlain(m.params[countIdx], values[countIdx] ?? 0)) + 1;
  // position jouée (modulations comprises) : celle qu'affiche l'encodeur
  const posPlain = toPlain(m.params[posIdx], posBinding?.normalised ?? values[posIdx] ?? 0);
  // la position (0 à 15) parcourt toute la liste, quelle que soit sa longueur
  const pos = Math.min(Math.max(0, posPlain), 15) * (count - 1) / 15;
  const w0 = Math.floor(pos), w1 = Math.min(count - 1, w0 + 1), f = pos - w0;
  const a = shapes[scan[w0]] ?? [], b = shapes[scan[w1]] ?? a;
  const cur = a.map((v, i) => v * (1 - f) + (b[i] ?? v) * f);
  const setSlot = (slot: number, wave: number) => actions.setParam(paramAddress(channel, "inst", 0, slotIdx[slot]), toNorm(m.params[slotIdx[slot]], wave));

  return (
    <section className="wavescan-osc" aria-label={`Table d'ondes de l'oscillateur ${osc}`}>
      <header className="wavescan-head">
        <b>Oscillateur {osc}</b>
        <span className="wavescan-now">{count === 1 ? names[scan[0]] : f < 0.02 ? names[scan[w0]] : `${names[scan[w0]]} → ${names[scan[w1]]}`}</span>
      </header>
      <div className="wavescan-main">
        <WaveView shape={cur} className="wavescan-screen" />
        <div className="wavescan-knobs">
          {posBinding && <Encoder binding={posBinding} variant="knob" />}
          {countBinding && <Encoder binding={countBinding} variant="knob" />}
        </div>
      </div>
      <div className="wavescan-list" role="list" aria-label="Ondes balayées, dans l'ordre">
        {scan.map((w, k) => (
          <button key={k} type="button" role="listitem" disabled={k >= count}
            className={`wavescan-slot${k >= count ? " off" : ""}${k === w0 || (f > 0.02 && k === w1) ? " here" : ""}${pick === k ? " picking" : ""}`}
            title={`${k + 1}. ${names[w]} — toucher pour changer l'onde`} onClick={() => setPick(pick === k ? null : k)}>
            <WaveView shape={shapes[w] ?? []} />
            <span>{k + 1}</span>
          </button>
        ))}
      </div>
      {pick !== null && (
        <div className="wavescan-picker" role="listbox" aria-label={`Onde de la case ${pick + 1}`}>
          <div className="wavescan-picker-head">
            <span>Case {pick + 1} : choisissez une onde</span>
            <button type="button" className="touch touch--small touch--quiet" onClick={() => setPick(null)}>Fermer</button>
          </div>
          <div className="wavescan-picker-grid">
            {names.map((n, w) => (
              <button key={w} type="button" role="option" aria-selected={scan[pick] === w} className={scan[pick] === w ? "on" : ""}
                onClick={() => { setSlot(pick, w); }} title={n}>
                <WaveView shape={shapes[w] ?? []} />
                <span>{n}</span>
              </button>
            ))}
          </div>
        </div>
      )}
    </section>
  );
}

export function WaveView({ shape, className }: { shape: number[]; className?: string }) {
  const w = 120, h = 50;
  const d = shape.length ? "M" + shape.map((v, i) => `${(i / (shape.length - 1)) * w},${h / 2 - v * (h / 2 - 3)}`).join(" L") : "";
  return (
    <svg viewBox={`0 0 ${w} ${h}`} className={`wave-view${className ? ` ${className}` : ""}`} preserveAspectRatio="none" aria-hidden>
      <line x1={0} x2={w} y1={h / 2} y2={h / 2} className="wave-axis" />
      <path d={d} />
    </svg>
  );
}
