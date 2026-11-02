// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 : écrans de la famille Drones.
//  - Komodo Dragon : les quatre masques de 8 bits (toucher un bit l'inverse) et la forme d'onde
//    obtenue pour le masque A et le masque B de l'oscillateur choisi (Osc. 1 ou 2), et le résultat
//    au réglage « Delta » (passage de A à B) — même calcul que le moteur.
import { useState } from "react";
import { useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useWriteTarget, writeParam } from "../../controls/bindings";

const SEGS = [8, 16, 32, 64, 128, 256];

/** Forme d'onde : masque A et masque B mélangés par « delta » (0 = A seul, 1 = B seul). */
function wavePath(maskA: number, maskB: number, delta: number, nseg: number, mode: number, W: number, H: number) {
  const N = 256;
  let d = "";
  for (let i = 0; i <= N; ++i) {
    const ph = i / N;
    const seg = Math.min(nseg - 1, Math.floor(ph * nseg));
    const g = ((maskA >> (seg & 7)) & 1) * (1 - delta) + ((maskB >> (seg & 7)) & 1) * delta;
    const s = Math.sin(2 * Math.PI * ph);
    const y = mode === 1 ? s * (2 * g - 1) : mode === 2 ? s * g + Math.abs(s) * (1 - g) : s * g;
    d += `${i === 0 ? "M" : "L"}${((ph * W)).toFixed(1)} ${(H / 2 - y * (H / 2 - 4)).toFixed(1)}`;
  }
  return d;
}

export function MaskPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const target = useWriteTarget(channel);
  const [osc, setOsc] = useState<1 | 2>(1); // oscillateur dont les courbes sont montrées
  if (!values) return null;
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const v = (id: string) => { const i = idx(id); return i < 0 ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) writeParam(target, paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain)); };
  const nseg = SEGS[Math.max(0, Math.min(5, Math.round(v("res"))))];
  const mode = Math.round(v("mmode"));
  const masks: [string, string][] = [["o1a", "Osc. 1 · A"], ["o1b", "Osc. 1 · B"], ["o2a", "Osc. 2 · A"], ["o2b", "Osc. 2 · B"]];
  const W = 240, H = 70;
  return (
    <div className="maskp">
      <div className="maskp-osc mp-seg" role="radiogroup" aria-label="Courbes de l'oscillateur">
        {([1, 2] as const).map((o) => (
          <button key={o} type="button" role="radio" aria-checked={osc === o} className={`mp-lb${osc === o ? " on" : ""}`}
            title={`Montrer les courbes de l'oscillateur ${o}`} onClick={() => setOsc(o)}><i />Osc. {o}</button>
        ))}
      </div>
      <div className="maskp-waves">
        {(() => {
          const a = Math.round(v(`o${osc}a`)), b = Math.round(v(`o${osc}b`)), delta = Math.max(0, Math.min(1, v("damt") / 100));
          const views: [string, number][] = [[`A · ${nseg} segments`, 0], [`B · ${nseg} segments`, 1], [`Résultat · delta ${Math.round(delta * 100)} %`, delta]];
          return views.map(([txt, dl], k) => (
            <svg key={k} viewBox={`0 0 ${W} ${H}`} className={`maskp-svg${k === 2 ? " maskp-svg--mix" : ""}`} role="img" aria-label={`Osc. ${osc} · ${txt}`}>
              <line x1={0} x2={W} y1={H / 2} y2={H / 2} className="maskp-axis" />
              <path d={wavePath(a, b, dl, nseg, mode, W, H)} className="maskp-wave" />
              <text x={6} y={14} className="maskp-txt">Osc. {osc} · {txt}</text>
            </svg>
          ));
        })()}
      </div>
      {masks.map(([id, label]) => {
        const mask = Math.round(v(id));
        return (
          <div key={id} className={`maskp-row${id.startsWith(`o${osc}`) ? " is-shown" : ""}`}>
            <small onClick={() => setOsc(id.startsWith("o1") ? 1 : 2)} style={{ cursor: "pointer" }}>{label}</small>
            {Array.from({ length: 8 }, (_, b) => {
              const on = ((mask >> b) & 1) === 1;
              return <button key={b} type="button" className={`maskp-bit${on ? " on" : ""}`} aria-pressed={on}
                title={`Bit ${b + 1} : ${on ? "garde le sinus" : "coupe / inverse / redresse"} (segments ${b + 1}, ${b + 9}…)`}
                onClick={() => set(id, mask ^ (1 << b))} />;
            })}
            <b className="tabular">{mask}</b>
          </div>
        );
      })}
    </div>
  );
}
