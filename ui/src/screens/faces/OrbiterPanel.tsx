// Groovebox — OFAD — AGPL-3.0-or-later
// Orbiteur : l'écran de l'orbite. Les quatre coins A à D (leur son est réglé dans la section
// « Coin »), la trajectoire de l'orbe bleu calculée depuis les réglages (forme, taille, rotation,
// déformation), l'orbe bleu et l'orbe vert (sous-orbite) de la voix qui joue, lus dans l'état de
// l'instrument. Glisser dans le carré déplace le centre de l'orbite.

import { useRef, useState } from "react";
import { createPortal } from "react-dom";
import { actions, useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress } from "../../controls/bindings";
import { WaveView } from "./WaveScanPanel";

const N = 160; // points du tracé
const SIZE = 220, PAD = 22;

/** Même calcul que le moteur (Orbit.h, trajectory) : position 0..1 × 0..1 pour une phase 0..1. */
function trajectory(v: (id: string) => number, ph: number): [number, number] {
  const shape = Math.round(v("shape"));
  const s2p = (x: number) => Math.sin(2 * Math.PI * x);
  let u = 0.5, w = 0.5;
  switch (shape) {
    case 1: { // enveloppe : A → B (attaque), B → D (déclin), D → C (relâchement)
      const t = ph * 3;
      if (t < 1) { u = t; w = 0; } else if (t < 2) { u = 1; w = t - 1; } else { u = 1 - (t - 2); w = 1; }
      break;
    }
    case 2: u = 0.5 + 0.5 * s2p(ph); w = 0.5 + 0.5 * s2p(ph + 0.25); break;
    case 3: u = 0.5 + 0.5 * s2p(ph); w = 0.5 + 0.5 * s2p(2 * ph + 0.25); break;
    case 4: u = 0.5 + 0.5 * s2p(3 * ph); w = 0.5 + 0.5 * s2p(2 * ph + 0.25); break;
    case 5: {
      const n = Math.min(8, Math.max(3, Math.round(v("ngon"))));
      const pos = ph * n, k = Math.floor(pos), f = pos - k;
      const a0 = k / n, a1 = (k + 1) / n;
      const x0 = 0.5 + 0.5 * s2p(a0 + 0.25), y0 = 0.5 + 0.5 * s2p(a0), x1 = 0.5 + 0.5 * s2p(a1 + 0.25), y1 = 0.5 + 0.5 * s2p(a1);
      u = x0 + (x1 - x0) * f; w = y0 + (y1 - y0) * f;
      break;
    }
    case 6: u = ph < v("pulsew") / 100 ? 0 : 1; w = 0.5; break;
    default: break;
  }
  const size = shape === 0 ? 0 : v("size") / 100;
  const dx = (u - 0.5) * size, dy = (w - 0.5) * size;
  const rot = v("rotate") / 360, c = s2p(rot + 0.25), sn = s2p(rot);
  const clamp = (z: number) => Math.min(1, Math.max(0, z));
  let x = clamp(v("cx") / 100 + dx * c - dy * sn), y = clamp(v("cy") / 100 + dx * sn + dy * c);
  const warp = (z: number, k: number) => (k === 0 ? z : Math.pow(z, Math.pow(4, -k)));
  x = warp(x, v("warpx") / 100); y = warp(y, v("warpy") / 100);
  return [x, y];
}

export function OrbiterPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const svg = useRef<SVGSVGElement>(null);
  if (!values) return null;
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const v = (id: string) => { const i = idx(id); return i < 0 ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain)); };
  const choice = (id: string) => { const i = idx(id); return i < 0 ? "" : m.params[i].choices?.[Math.round(v(id))] ?? ""; };

  // générateur sur W : le nom de l'onde choisie
  const gen = (c: string, g: number) => {
    const i = idx(`${c}g${g}`);
    const isW = i >= 0 && Math.round(v(`${c}g${g}`)) === (m.params[i].choices?.length ?? 8) - 1;
    return isW ? choice(`${c}w${g}`) : choice(`${c}g${g}`);
  };
  const px = (x: number) => PAD + x * SIZE;
  const count = Math.round(v("shape")) === 6 ? 2 : N + 1;
  const path = Array.from({ length: count }, (_, k) => trajectory(v, k / (count - 1)))
    .map(([x, y], k) => `${k ? "L" : "M"}${px(x).toFixed(1)},${px(y).toFixed(1)}`).join(" ");
  const sounding = (status[4] ?? 0) > 0.5;
  const blue: [number, number] = sounding ? [status[0] ?? 0.5, status[1] ?? 0.5] : trajectory(v, 0);
  const green: [number, number] = sounding ? [status[2] ?? 0.5, status[3] ?? 0.5] : blue;
  const sub = v("subsize") / 100 * 0.5;
  // mélange des coins à la position de l'orbe vert
  const [gx, gy] = green;
  const w = [(1 - gx) * (1 - gy), gx * (1 - gy), (1 - gx) * gy, gx * gy];

  const moveCentre = (e: React.PointerEvent) => {
    const r = svg.current?.getBoundingClientRect();
    if (!r) return;
    const scale = (SIZE + 2 * PAD) / r.width;
    const x = ((e.clientX - r.left) * scale - PAD) / SIZE, y = ((e.clientY - r.top) * scale - PAD) / SIZE;
    set("cx", Math.round(Math.min(1, Math.max(0, x)) * 100));
    set("cy", Math.round(Math.min(1, Math.max(0, y)) * 100));
  };
  const corners = ["a", "b", "c", "d"] as const;
  const cornerXY = [[0, 0], [1, 0], [0, 1], [1, 1]] as const;

  return (
    <div className="orbiter">
      <svg ref={svg} className="orbiter-screen" viewBox={`0 0 ${SIZE + 2 * PAD} ${SIZE + 2 * PAD}`} role="img"
        aria-label={`Orbite ${choice("shape")}, centre ${Math.round(v("cx"))} % / ${Math.round(v("cy"))} %`}
        onPointerDown={(e) => { (e.target as Element).setPointerCapture?.(e.pointerId); moveCentre(e); }}
        onPointerMove={(e) => { if (e.buttons & 1) moveCentre(e); }}>
        <rect x={PAD} y={PAD} width={SIZE} height={SIZE} rx={6} className="orbiter-field" />
        {[0.25, 0.5, 0.75].map((g) => (
          <g key={g} className="orbiter-grid">
            <line x1={px(g)} y1={PAD} x2={px(g)} y2={PAD + SIZE} /><line x1={PAD} y1={px(g)} x2={PAD + SIZE} y2={px(g)} />
          </g>
        ))}
        {corners.map((c, k) => (
          <g key={c} className="orbiter-corner" style={{ opacity: 0.35 + 0.65 * w[k] }}>
            <circle cx={px(cornerXY[k][0])} cy={px(cornerXY[k][1])} r={6 + 10 * w[k]} />
            <text x={px(cornerXY[k][0]) + (cornerXY[k][0] ? -14 : 14)} y={px(cornerXY[k][1]) + (cornerXY[k][1] ? -12 : 18)} textAnchor="middle">{c.toUpperCase()}</text>
          </g>
        ))}
        <path d={path} className="orbiter-path" />
        <g className="orbiter-centre"><line x1={px(v("cx") / 100) - 6} y1={px(v("cy") / 100)} x2={px(v("cx") / 100) + 6} y2={px(v("cy") / 100)} />
          <line x1={px(v("cx") / 100)} y1={px(v("cy") / 100) - 6} x2={px(v("cx") / 100)} y2={px(v("cy") / 100) + 6} /></g>
        {sub > 0 && <circle cx={px(blue[0])} cy={px(blue[1])} r={sub * SIZE} className="orbiter-sub" />}
        <circle cx={px(blue[0])} cy={px(blue[1])} r={7} className="orbiter-blue" />
        <circle cx={px(green[0])} cy={px(green[1])} r={5} className="orbiter-green" />
      </svg>
      <ul className="orbiter-legend" aria-label="Les quatre coins">
        {corners.map((c, k) => (
          <li key={c}>
            <b>{c.toUpperCase()}</b>
            <span>{gen(c, 1)} + {gen(c, 2)} {choice(`${c}ratio`)}</span>
            <span className="orbiter-mix">{Math.round(v(`${c}blend`)) <= 0 ? "somme" : choice(`${c}bmode`)} · {choice(`${c}ftype`)}</span>
            <i style={{ width: `${Math.round(w[k] * 100)}%` }} aria-label={`${Math.round(w[k] * 100)} %`} />
          </li>
        ))}
        <li className="orbiter-hint">Glisser dans le carré : centre de l'orbite. Bleu : l'orbe ; vert : la sous-orbite, qui mélange les coins.</li>
      </ul>
    </div>
  );
}

/** Ondes W d'un coin : pour chacun de ses deux générateurs, l'onde jouée quand le générateur est sur
    W (« onde choisie »), prise dans les 64 ondes de la bibliothèque du wave morph. Choisir une onde met
    aussi le générateur sur W. A à F et N restent les motifs d'origine. */
export function OrbWavesPanel({ channel, module: m, corner }: { channel: number; module: ModuleDesc; corner: string }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const [pick, setPick] = useState<1 | 2 | null>(null);
  const shapes = m.waveShapes ?? [];
  if (!values || !/^[abcd]$/.test(corner)) return null;
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const gens = ([1, 2] as const).map((g) => {
    const gi = idx(`${corner}g${g}`), wi = idx(`${corner}w${g}`);
    const gd = gi >= 0 ? m.params[gi] : undefined, wd = wi >= 0 ? m.params[wi] : undefined;
    return { g, gi, wi, gd, wd, pat: gd ? Math.round(toPlain(gd, values[gi] ?? 0)) : 0, wave: wd ? Math.round(toPlain(wd, values[wi] ?? 0)) : 0 };
  });
  if (gens.some((x) => !x.gd || !x.wd)) return null;
  const W = (gens[0].gd!.choices?.length ?? 8) - 1;   // position W : la dernière
  const names = gens[0].wd!.choices ?? [];
  const libShape = (w: number) => shapes[W + w] ?? [];   // formes : 6 motifs, bruit, puis la bibliothèque
  const choose = (g: 1 | 2, w: number) => {
    const x = gens[g - 1];
    actions.setParam(paramAddress(channel, "inst", 0, x.wi), toNorm(x.wd!, w));
    if (x.pat !== W) actions.setParam(paramAddress(channel, "inst", 0, x.gi), toNorm(x.gd!, W));
  };
  const C = corner.toUpperCase();
  return (
    <div className="orbwaves">
      {gens.map(({ g, pat, wave }) => (
        <button key={g} type="button" className={`orbwaves-gen${pick === g ? " picking" : ""}${pat === W ? " live" : ""}`} onClick={() => setPick(pick === g ? null : g)}
          title={`Coin ${C}, générateur ${g}, onde W : ${names[wave] ?? ""}${pat === W ? "" : " (jouée quand le générateur est sur W)"} — toucher pour changer`}>
          <span className="orbwaves-lab">W {g}</span>
          <WaveView shape={libShape(wave)} />
          <span className="orbwaves-name">{names[wave] ?? ""}</span>
        </button>
      ))}
      {pick !== null && createPortal(
        <div className="orbwaves-veil" onPointerDown={(e) => { if (e.target === e.currentTarget) setPick(null); }}>
          <div className="orbwaves-picker" role="dialog" aria-label={`Coin ${C}, générateur ${pick} : onde W`}>
            <div className="orbwaves-picker-head">
              <b>Coin {C} · générateur {pick} · onde W</b>
              <button type="button" className="touch touch--small touch--quiet" onClick={() => setPick(null)}>Fermer</button>
            </div>
            <p className="orbwaves-hint">Bibliothèque d'ondes (celles du wave morph). Choisir une onde met le générateur sur W.</p>
            <div className="orbwaves-grid" role="listbox">
              {names.map((n, w) => {
                const on = gens[pick - 1].wave === w;
                return (
                  <button key={w} type="button" role="option" aria-selected={on} className={on ? "on" : ""} title={n} onClick={() => choose(pick, w)}>
                    <WaveView shape={libShape(w)} />
                    <span>{n}</span>
                  </button>
                );
              })}
            </div>
          </div>
        </div>, document.body)}
    </div>
  );
}
