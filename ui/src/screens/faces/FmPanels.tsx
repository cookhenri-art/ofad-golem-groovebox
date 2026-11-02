// Groovebox — OFAD — AGPL-3.0-or-later
// Old Metal (FM à 6 opérateurs) : écran de l'algorithme — qui module qui (en haut les modulateurs,
// en bas les opérateurs entendus), la boucle de rétroaction ; toucher un opérateur le choisit
// dans les sections « Opérateur », « Enveloppe » et « Échelle » (banque commune « fmop »).

import { useEffect, useReducer } from "react";
import { useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useWriteTarget, writeParam } from "../../controls/bindings";

/** Banques partagées entre sections (même opérateur choisi partout). */
const groupBank = new Map<string, number>();
const groupListeners = new Set<() => void>();
export function setGroupBank(group: string, i: number) { groupBank.set(group, i); groupListeners.forEach((f) => f()); }
export function useGroupBank(group: string | undefined): number | undefined {
  const [, force] = useReducer((x: number) => x + 1, 0);
  useEffect(() => {
    if (!group) return;
    groupListeners.add(force);
    return () => { groupListeners.delete(force); };
  }, [group]);
  return group ? groupBank.get(group) ?? 0 : undefined;
}

/** Même table que le moteur (Fm6.h) : modulateurs de chaque opérateur (bits, opérateur 1 = bit 0),
    opérateurs entendus, opérateur à rétroaction (0 = opérateur 1). */
const O1 = 1, O2 = 2, O3 = 4, O4 = 8, O5 = 16, O6 = 32;
export const FM_ALGOS: { mods: number[]; car: number; fb: number }[] = [
  { mods: [O2, 0, O4, O5, O6, 0], car: O1 | O3, fb: 5 },
  { mods: [O2, 0, O4, O5, O6, 0], car: O1 | O3, fb: 1 },
  { mods: [O2, O3, 0, O5, O6, 0], car: O1 | O4, fb: 5 },
  { mods: [O2, O3, 0, O5, O6, 0], car: O1 | O4, fb: 5 },
  { mods: [O2, 0, O4, 0, O6, 0], car: O1 | O3 | O5, fb: 5 },
  { mods: [O2, 0, O4, 0, O6, 0], car: O1 | O3 | O5, fb: 5 },
  { mods: [O2, 0, O4 | O5, 0, O6, 0], car: O1 | O3, fb: 5 },
  { mods: [O2, 0, O4 | O5, 0, O6, 0], car: O1 | O3, fb: 3 },
  { mods: [O2, 0, O4 | O5, 0, O6, 0], car: O1 | O3, fb: 1 },
  { mods: [O2, O3, 0, O5 | O6, 0, 0], car: O1 | O4, fb: 2 },
  { mods: [O2, O3, 0, O5 | O6, 0, 0], car: O1 | O4, fb: 5 },
  { mods: [O2, 0, O4 | O5 | O6, 0, 0, 0], car: O1 | O3, fb: 1 },
  { mods: [O2, 0, O4 | O5 | O6, 0, 0, 0], car: O1 | O3, fb: 5 },
  { mods: [O2, 0, O4, O5 | O6, 0, 0], car: O1 | O3, fb: 5 },
  { mods: [O2, 0, O4, O5 | O6, 0, 0], car: O1 | O3, fb: 1 },
  { mods: [O2 | O3 | O5, 0, O4, 0, O6, 0], car: O1, fb: 5 },
  { mods: [O2 | O3 | O5, 0, O4, 0, O6, 0], car: O1, fb: 1 },
  { mods: [O2 | O3 | O4, 0, 0, O5, O6, 0], car: O1, fb: 2 },
  { mods: [O2, O3, 0, O6, O6, 0], car: O1 | O4 | O5, fb: 5 },
  { mods: [O3, O3, 0, O5 | O6, 0, 0], car: O1 | O2 | O4, fb: 2 },
  { mods: [O3, O3, 0, O6, O6, 0], car: O1 | O2 | O4 | O5, fb: 2 },
  { mods: [O2, 0, O6, O6, O6, 0], car: O1 | O3 | O4 | O5, fb: 5 },
  { mods: [0, O3, 0, O6, O6, 0], car: O1 | O2 | O4 | O5, fb: 5 },
  { mods: [0, 0, O6, O6, O6, 0], car: O1 | O2 | O3 | O4 | O5, fb: 5 },
  { mods: [0, 0, 0, O6, O6, 0], car: O1 | O2 | O3 | O4 | O5, fb: 5 },
  { mods: [0, O3, 0, O5 | O6, 0, 0], car: O1 | O2 | O4, fb: 5 },
  { mods: [0, O3, 0, O5 | O6, 0, 0], car: O1 | O2 | O4, fb: 2 },
  { mods: [O2, 0, O4, O5, 0, 0], car: O1 | O3 | O6, fb: 4 },
  { mods: [0, 0, O4, 0, O6, 0], car: O1 | O2 | O3 | O5, fb: 5 },
  { mods: [0, 0, O4, O5, 0, 0], car: O1 | O2 | O3 | O6, fb: 4 },
  { mods: [0, 0, 0, 0, O6, 0], car: O1 | O2 | O3 | O4 | O5, fb: 5 },
  { mods: [0, 0, 0, 0, 0, 0], car: O1 | O2 | O3 | O4 | O5 | O6, fb: 5 },
];

/** Positions des opérateurs : entendus en bas, chaque modulateur au-dessus de sa (première) cible. */
function layoutOf(a: { mods: number[]; car: number }) {
  const col: number[] = new Array(6).fill(-1), row: number[] = new Array(6).fill(0);
  let next = 0;
  const place = (k: number, r: number) => {
    if (col[k] >= 0) return;
    row[k] = r;
    const mods = [0, 1, 2, 3, 4, 5].filter((j) => (a.mods[k] >> j) & 1);
    const fresh = mods.filter((j) => col[j] < 0);
    fresh.forEach((j) => place(j, r + 1));
    const cs = mods.map((j) => col[j]).filter((c) => c >= 0);
    col[k] = fresh.length ? cs.reduce((x, y) => x + y, 0) / cs.length : next++;
  };
  for (let k = 0; k < 6; ++k) if ((a.car >> k) & 1) place(k, 0);
  for (let k = 0; k < 6; ++k) place(k, 0);
  return { col, row, cols: Math.max(1, next), rows: Math.max(...row) + 1 };
}

export function FmAlgoPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const target = useWriteTarget(channel);
  const sel = useGroupBank("fmop") ?? 0;
  if (!values) return null;
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const v = (id: string) => { const i = idx(id); return i < 0 ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) writeParam(target, paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain)); };
  const algo = Math.min(32, Math.max(1, Math.round(v("algo"))));
  const fb = Math.round(v("feedback"));
  const a = FM_ALGOS[algo - 1];
  const { col, row, cols, rows } = layoutOf(a);
  const W = 240, H = 150, bw = 30, bh = 20;
  const x = (k: number) => 14 + (cols <= 1 ? (W - 28 - bw) / 2 : (col[k] / (cols - 1)) * (W - 28 - bw));
  const step = rows > 1 ? (H - 56) / (rows - 1) : 0;
  const yy = (k: number) => H - 32 - row[k] * step - bh / 2;
  const links: [number, number][] = [];
  for (let k = 0; k < 6; ++k) for (let j = 0; j < 6; ++j) if ((a.mods[k] >> j) & 1) links.push([j, k]);
  const level = (k: number) => v(`o${k + 1}_level`);
  return (
    <div className="fmalgo">
      <div className="fmalgo-head">
        <button type="button" aria-label="Algorithme précédent" onClick={() => set("algo", algo <= 1 ? 32 : algo - 1)}>◀</button>
        <b className="tabular">ALG {String(algo).padStart(2, "0")}</b>
        <button type="button" aria-label="Algorithme suivant" onClick={() => set("algo", algo >= 32 ? 1 : algo + 1)}>▶</button>
        <span className="fmalgo-fb tabular" title="Rétroaction (0 à 7)">RÉTRO {fb}</span>
      </div>
      <svg viewBox={`0 0 ${W} ${H}`} className="fmalgo-svg" role="img" aria-label={`Algorithme ${algo}`}>
        {links.map(([j, k], i) => {
          const x1 = x(j) + bw / 2, y1 = yy(j) + bh, x2 = x(k) + bw / 2, y2 = yy(k);
          return <path key={i} d={`M${x1} ${y1} C ${x1} ${(y1 + y2) / 2}, ${x2} ${(y1 + y2) / 2}, ${x2} ${y2}`} className="fmalgo-link" />;
        })}
        {[0, 1, 2, 3, 4, 5].filter((k) => (a.car >> k) & 1).map((k) => (
          <line key={`o${k}`} x1={x(k) + bw / 2} y1={yy(k) + bh} x2={x(k) + bw / 2} y2={H - 8} className="fmalgo-out" />
        ))}
        <line x1={10} y1={H - 8} x2={W - 10} y2={H - 8} className="fmalgo-bus" />
        {/* rétroaction : boucle sur l'opérateur */}
        <path d={`M${x(a.fb) + bw} ${yy(a.fb) + bh / 2} h 7 v ${-bh / 2 - 6} h ${-bw / 2 - 7} v 6`} className={`fmalgo-fbk${fb > 0 ? " on" : ""}`} />
        {[0, 1, 2, 3, 4, 5].map((k) => {
          const carrier = ((a.car >> k) & 1) === 1;
          return (
            <g key={k} className={`fmalgo-op${carrier ? " car" : ""}${sel === k ? " sel" : ""}${level(k) <= 0 ? " mute" : ""}`} onClick={() => setGroupBank("fmop", k)} style={{ cursor: "pointer" }}>
              <title>{`Opérateur ${k + 1}${carrier ? " (entendu)" : " (modulateur)"} · niveau ${Math.round(level(k))} — toucher pour le régler`}</title>
              <rect x={x(k)} y={yy(k)} width={bw} height={bh} rx={3} />
              <text x={x(k) + bw / 2} y={yy(k) + bh / 2 + 4} textAnchor="middle">{k + 1}</text>
            </g>
          );
        })}
        <text x={W / 2} y={H - 1} textAnchor="middle" className="fmalgo-outtxt">SORTIE</text>
      </svg>
    </div>
  );
}

/** New Metal (FM à 4 opérateurs) : même table que le moteur (Fm4.h). Opérateurs C, A, B1, B2
    (C = bit 0) ; deux sorties X et Y, fondues par « Mélange X / Y ». */
const A_ = 2, B1_ = 4, B2_ = 8;
export const FM4_ALGOS: { mods: number[]; x: number; y: number; fb: number }[] = [
  { mods: [A_, 0, B2_, 0], x: 0, y: B1_, fb: 1 },
  { mods: [A_, B1_, B2_, 0], x: 0, y: A_, fb: 3 },
  { mods: [A_ | B1_, 0, B2_, 0], x: 0, y: A_, fb: 3 },
  { mods: [A_, B2_, B2_, 0], x: 0, y: B1_, fb: 3 },
  { mods: [A_ | B1_, 0, B2_, 0], x: 0, y: B1_, fb: 1 },
  { mods: [B2_, B2_, B2_, 0], x: 0, y: A_ | B1_, fb: 3 },
  { mods: [B1_, B2_, 0, 0], x: 0, y: A_, fb: 3 },
  { mods: [0, 0, B2_, 0], x: 0, y: A_ | B1_, fb: 3 },
];
const FM4_NAMES = ["C", "A", "B1", "B2"];

export function Fm4AlgoPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const target = useWriteTarget(channel);
  if (!values) return null;
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const v = (id: string) => { const i = idx(id); return i < 0 ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) writeParam(target, paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain)); };
  const algo = Math.min(8, Math.max(1, Math.round(v("algo"))));
  const fb = Math.round(v("feedback"));
  const mix = v("mix");
  const a = FM4_ALGOS[algo - 1];
  // positions : entendus en bas (X à gauche, Y à droite), modulateurs au-dessus de leur cible
  const car = (1 << a.x) | a.y;
  const { col, row, cols, rows } = layoutGeneric(a.mods, car, 4);
  const W = 220, H = 140, bw = 30, bh = 20;
  const x = (k: number) => 14 + (cols <= 1 ? (W - 28 - bw) / 2 : (col[k] / (cols - 1)) * (W - 28 - bw));
  const step = rows > 1 ? (H - 56) / (rows - 1) : 0;
  const yy = (k: number) => H - 32 - row[k] * step - bh / 2;
  const links: [number, number][] = [];
  for (let k = 0; k < 4; ++k) for (let j = 0; j < 4; ++j) if ((a.mods[k] >> j) & 1) links.push([j, k]);
  const level = (k: number) => (k === 0 ? 127 : k === 1 ? v("leva") : v("levb"));
  const tag = (k: number) => (k === a.x ? "X" : (a.y >> k) & 1 ? "Y" : "");
  const xW = Math.round(Math.cos(((mix / 63 + 1) * Math.PI) / 4) * 100), yW = Math.round(Math.sin(((mix / 63 + 1) * Math.PI) / 4) * 100);
  return (
    <div className="fmalgo">
      <div className="fmalgo-head">
        <button type="button" aria-label="Algorithme précédent" onClick={() => set("algo", algo <= 1 ? 8 : algo - 1)}>◀</button>
        <b className="tabular">ALG {algo}</b>
        <button type="button" aria-label="Algorithme suivant" onClick={() => set("algo", algo >= 8 ? 1 : algo + 1)}>▶</button>
        <span className="fmalgo-fb tabular" title="Rétroaction (0 à 127) et part des sorties X et Y">RÉTRO {fb} · X {xW} / Y {yW}</span>
      </div>
      <svg viewBox={`0 0 ${W} ${H}`} className="fmalgo-svg" role="img" aria-label={`Algorithme ${algo}`}>
        {links.map(([j, k], i) => {
          const x1 = x(j) + bw / 2, y1 = yy(j) + bh, x2 = x(k) + bw / 2, y2 = yy(k);
          return <path key={i} d={`M${x1} ${y1} C ${x1} ${(y1 + y2) / 2}, ${x2} ${(y1 + y2) / 2}, ${x2} ${y2}`} className="fmalgo-link" />;
        })}
        {[0, 1, 2, 3].filter((k) => (car >> k) & 1).map((k) => (
          <g key={`o${k}`}>
            <line x1={x(k) + bw / 2} y1={yy(k) + bh} x2={x(k) + bw / 2} y2={H - 8} className="fmalgo-out" style={{ opacity: tag(k) === "X" ? 0.3 + 0.7 * xW / 100 : 0.3 + 0.7 * yW / 100 }} />
            <text x={x(k) + bw / 2 + 4} y={H - 12} className="fmalgo-outtxt">{tag(k)}</text>
          </g>
        ))}
        <line x1={10} y1={H - 8} x2={W - 10} y2={H - 8} className="fmalgo-bus" />
        <path d={`M${x(a.fb) + bw} ${yy(a.fb) + bh / 2} h 7 v ${-bh / 2 - 6} h ${-bw / 2 - 7} v 6`} className={`fmalgo-fbk${fb > 0 ? " on" : ""}`} />
        {[0, 1, 2, 3].map((k) => {
          const carrier = ((car >> k) & 1) === 1;
          return (
            <g key={k} className={`fmalgo-op${carrier ? " car" : ""}${level(k) <= 0 ? " mute" : ""}`}>
              <title>{`Opérateur ${FM4_NAMES[k]}${carrier ? ` (sortie ${tag(k)})` : " (modulateur)"}`}</title>
              <rect x={x(k)} y={yy(k)} width={bw} height={bh} rx={3} />
              <text x={x(k) + bw / 2} y={yy(k) + bh / 2 + 4} textAnchor="middle">{FM4_NAMES[k]}</text>
            </g>
          );
        })}
        <text x={W / 2} y={H - 1} textAnchor="middle" className="fmalgo-outtxt">SORTIE</text>
      </svg>
    </div>
  );
}

/** Même placement que pour 6 opérateurs, pour n opérateurs. */
function layoutGeneric(mods: number[], car: number, n: number) {
  const col: number[] = new Array(n).fill(-1), row: number[] = new Array(n).fill(0);
  let next = 0;
  const place = (k: number, r: number) => {
    if (col[k] >= 0) return;
    row[k] = r;
    const ms = Array.from({ length: n }, (_, j) => j).filter((j) => (mods[k] >> j) & 1);
    const fresh = ms.filter((j) => col[j] < 0);
    fresh.forEach((j) => place(j, r + 1));
    const cs = ms.map((j) => col[j]).filter((c) => c >= 0);
    col[k] = fresh.length ? cs.reduce((x, y) => x + y, 0) / cs.length : next++;
  };
  for (let k = 0; k < n; ++k) if ((car >> k) & 1) place(k, 0);
  for (let k = 0; k < n; ++k) place(k, 0);
  return { col, row, cols: Math.max(1, next), rows: Math.max(...row) + 1 };
}
