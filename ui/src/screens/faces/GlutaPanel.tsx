// Groovebox — OFAD — AGPL-3.0-or-later
// Glutamake (v0.47) : le pavé de la physique (la surface, la bosse, les bords, la bille lue dans
// l'état de l'instrument ; tenir la bille au doigt, la lâcher d'un geste, la lancer), l'écran de
// chaque oscillateur (l'onde lue : tables A et B, position, morph, modificateur) et les couloirs de
// la séquence de mouvement (pas, boucle, pas joué).

import { useEffect, useRef } from "react";
import { actions, useStore } from "../../engine/store";
import { toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress } from "../../controls/bindings";
import { StepBar } from "./MatrixPanel";

const FRAMES = 12;
const TABLES = ["Analogique", "Voyelles", "Cloches FM", "Plis", "Harm. paires", "Granuleux", "Cordes frottées", "8 bits"];
const MORPHS = ["—", "Étirer", "Comprimer", "Refléter", "Miroir", "Synchro", "Plier", "Pulsation", "Quantifier", "Bits", "FM", "AM", "Anneau"];

/** Lecture des réglages de la piste (valeurs réelles). */
function usePlain(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  return (id: string) => {
    const i = m.params.findIndex((p) => p.id === id);
    return i < 0 || !values ? 0 : toPlain(m.params[i], values[i] ?? 0);
  };
}

/** Même calcul que le moteur (Glutamake.h, bumpForce) : hauteur de la bosse en (x, y). */
function bumpHeight(v: (id: string) => number, x: number, y: number) {
  const h = v("bumph") / 100, cx = v("bumpx") / 100, cy = v("bumpy") / 100, w = Math.max(0.05, (v("bumpw") / 100) * 0.9);
  const u = Math.hypot(x - cx, y - cy) / w;
  switch (Math.round(v("bumpshape"))) {
    case 1: return h * Math.max(0, 1 - u);
    case 2: return h / (1 + Math.pow(u, 6));
    case 3: { const k = u - 1; return h * Math.exp(-4 * k * k); }
    default: return h * Math.exp(-0.5 * u * u);
  }
}

export function GlutaPad({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const v = usePlain(channel, m);
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const cv = useRef<HTMLCanvasElement>(null);
  const field = useRef<{ key: string; img: HTMLCanvasElement | null }>({ key: "", img: null });
  const trail = useRef<[number, number][]>([]);
  const drag = useRef<{ t: number; x: number; y: number; vx: number; vy: number; last: number } | null>(null);
  const bx = status.length >= 2 ? status[0] : 0, by = status.length >= 2 ? status[1] : 0;
  const hit = status[8] ?? 0, hits = status[6] ?? 0;
  const on = v("phon") > 0.5, wrap = v("wrap") > 0.5;

  // dessin : fond (la bosse), lueur de l'inclinaison, bords, trace, bille
  useEffect(() => {
    const c = cv.current;
    const g = c?.getContext("2d");
    if (!c || !g) return;
    const W = c.width;
    const key = ["bumph", "bumpx", "bumpy", "bumpw", "bumpshape"].map((k) => Math.round(v(k))).join(",");
    if (field.current.key !== key || !field.current.img) {
      const N = 96, off = document.createElement("canvas");
      off.width = N; off.height = N;
      const og = off.getContext("2d");
      if (og) {
        const img = og.createImageData(N, N);
        for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
          const x = (i / (N - 1)) * 2 - 1, y = 1 - (j / (N - 1)) * 2, U = bumpHeight(v, x, y);
          const k = (j * N + i) * 4, vig = 1 - (0.25 * (x * x + y * y)) / 2;
          const r = U > 0 ? 66 + U * 170 : 66 * (1 + U * 0.85), gg = U > 0 ? 24 + U * 112 : 24 * (1 + U * 0.85), b = U > 0 ? 14 + U * 70 : 14 * (1 + U * 0.85);
          img.data[k] = Math.max(0, r * vig); img.data[k + 1] = Math.max(0, gg * vig); img.data[k + 2] = Math.max(0, b * vig); img.data[k + 3] = 255;
        }
        og.putImageData(img, 0, 0);
      }
      field.current = { key, img: off };
    }
    g.imageSmoothingEnabled = true;
    if (field.current.img) g.drawImage(field.current.img, 0, 0, W, W);
    const tx = v("tiltx") / 100, ty = v("tilty") / 100;
    if (Math.abs(tx) + Math.abs(ty) > 0.02) {
      const gr = g.createLinearGradient(W / 2 - (tx * W) / 2, W / 2 + (ty * W) / 2, W / 2 + (tx * W) / 2, W / 2 - (ty * W) / 2);
      gr.addColorStop(0, "rgba(255,90,46,0)"); gr.addColorStop(1, `rgba(255,90,46,${Math.min(0.35, Math.hypot(tx, ty) * 0.3)})`);
      g.fillStyle = gr; g.fillRect(0, 0, W, W);
    }
    g.strokeStyle = "rgba(255,170,120,.08)"; g.lineWidth = 1;
    for (let i = 1; i < 8; i++) { const p = (i * W) / 8; g.beginPath(); g.moveTo(p, 0); g.lineTo(p, W); g.moveTo(0, p); g.lineTo(W, p); g.stroke(); }
    const bxp = ((v("bumpx") / 100 + 1) / 2) * W, byp = (1 - (v("bumpy") / 100 + 1) / 2) * W, bw = ((v("bumpw") / 100) * 0.9 * W) / 2;
    g.strokeStyle = v("bumph") >= 0 ? "rgba(255,200,150,.3)" : "rgba(0,0,0,.45)";
    for (let r = 1; r <= 3; r++) { g.beginPath(); g.arc(bxp, byp, bw * r * 0.6, 0, Math.PI * 2); g.stroke(); }
    const edge = (e: number, x0: number, y0: number, x1: number, y1: number) => {
      g.setLineDash(wrap ? [W / 40, W / 40] : []);
      g.strokeStyle = wrap ? "rgba(255,255,255,.25)" : e >= 0 ? `rgba(255,${160 + e},${90 + e},${0.25 + e / 160})` : `rgba(40,20,20,${0.4 - e / 250})`;
      g.lineWidth = W / 60; g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke(); g.setLineDash([]);
    };
    const d = W / 140;
    edge(v("edget"), 0, d, W, d); edge(v("edgeb"), 0, W - d, W, W - d); edge(v("edgel"), d, 0, d, W); edge(v("edger"), W - d, 0, W - d, W);
    const px = ((bx + 1) / 2) * W, py = (1 - (by + 1) / 2) * W;
    const tr = trail.current;
    tr.push([px, py]); if (tr.length > 30) tr.shift();
    for (let i = 1; i < tr.length; i++) {
      const [x0, y0] = tr[i - 1], [x1, y1] = tr[i];
      if (Math.hypot(x1 - x0, y1 - y0) > W / 2) continue;
      g.strokeStyle = `rgba(255,140,70,${(i / tr.length) * 0.6})`; g.lineWidth = (W / 160) * (1 + i / 8);
      g.beginPath(); g.moveTo(x0, y0); g.lineTo(x1, y1); g.stroke();
    }
    const R = W / 24;
    const rg = g.createRadialGradient(px - R / 3, py - R / 3, R / 8, px, py, R * 1.2);
    rg.addColorStop(0, "#fff7ea"); rg.addColorStop(0.4, on ? "#ffb04a" : "#8a7a6a"); rg.addColorStop(1, on ? "#c8331a" : "#4a3a30");
    g.shadowColor = `rgba(255,90,46,${0.5 + hit * 0.5})`; g.shadowBlur = R * (1.2 + hit * 2);
    g.fillStyle = rg; g.beginPath(); g.arc(px, py, R, 0, Math.PI * 2); g.fill(); g.shadowBlur = 0;
  });

  const at = (e: React.PointerEvent) => {
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    return [Math.max(-1, Math.min(1, ((e.clientX - r.left) / r.width) * 2 - 1)), Math.max(-1, Math.min(1, 1 - ((e.clientY - r.top) / r.height) * 2))];
  };
  const down = (e: React.PointerEvent) => {
    e.preventDefault(); e.stopPropagation();
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    const [x, y] = at(e), t = performance.now();
    drag.current = { t, x, y, vx: 0, vy: 0, last: t };
    actions.instTouch(channel, 0, x, y);
  };
  const move = (e: React.PointerEvent) => {
    const d = drag.current;
    if (!d) return;
    const [x, y] = at(e), t = performance.now(), dt = Math.max(1, t - d.t) / 1000;
    d.vx = d.vx * 0.5 + ((x - d.x) / dt) * 0.5; d.vy = d.vy * 0.5 + ((y - d.y) / dt) * 0.5;
    d.t = t; d.x = x; d.y = y;
    if (t - d.last > 16) { d.last = t; actions.instTouch(channel, 0, x, y); } // un geste par image au plus
  };
  const up = () => {
    const d = drag.current;
    if (!d) return;
    drag.current = null;
    actions.instTouch(channel, 0, d.x, d.y);
    actions.instTouch(channel, 1, Math.max(-6, Math.min(6, d.vx * 0.6)), Math.max(-6, Math.min(6, d.vy * 0.6)));
  };

  const dist = Math.min(1, Math.hypot(bx, by) / Math.SQRT2);
  const ang = ((Math.atan2(by, bx) / (2 * Math.PI)) + 1) % 1;
  const outs: [string, number, boolean][] = [["X", bx, true], ["Y", by, true], ["Dist.", dist, false], ["Angle", ang, false]];
  return (
    <div className="gl-pad">
      <canvas ref={cv} width={480} height={480} className="gl-pad-cv" onPointerDown={down} onPointerMove={move} onPointerUp={up} onPointerCancel={up}
        title="Tenir : la bille suit le doigt · lâcher d'un geste : elle part avec sa vitesse" />
      <div className="gl-pad-outs">
        {outs.map(([n, val, bip]) => (
          <div key={n} className="gl-out"><span>{n}</span><b>{val.toFixed(2)}</b>
            <i><em style={bip ? { left: `${val < 0 ? 50 + val * 50 : 50}%`, width: `${Math.abs(val) * 50}%` } : { left: 0, width: `${val * 100}%` }} /></i></div>
        ))}
      </div>
      <div className="gl-pad-row">
        <button type="button" className="fc-toggle" onClick={() => actions.instTouch(channel, 2, 0, 0)}>Lancer</button>
        <span className="gl-pad-info">{on ? "Physique en marche" : "Physique à l'arrêt"} · {Math.round(hits)} chocs</span>
      </div>
    </div>
  );
}

/** Écran d'un oscillateur : l'onde lue (tables A et B, position, morph, modificateur). */
export function GlutaOsc({ channel, module: m, osc }: { channel: number; module: ModuleDesc; osc: string }) {
  const v = usePlain(channel, m);
  const k = `o${osc === "2" ? 2 : 1}`;
  const ta = Math.round(v(`${k}taba`)), tb = Math.round(v(`${k}tabb`)), pos = v(`${k}pos`) / 100, ab = v(`${k}ab`) / 100;
  const mt = Math.round(v(`${k}mtype`)), mo = v(`${k}morph`) / 100, md = Math.round(v(`${k}modif`));
  const shapes = m.waveShapes ?? [];
  const read = (t: number, ph: number) => {
    const fp = pos * (FRAMES - 1), k0 = Math.min(Math.floor(fp), FRAMES - 2), f = fp - k0;
    const s0 = shapes[t * FRAMES + k0], s1 = shapes[t * FRAMES + k0 + 1];
    if (!s0 || !s1) return 0;
    const x = (((ph % 1) + 1) % 1) * s0.length, i = Math.floor(x) % s0.length, j = (i + 1) % s0.length, fr = x - Math.floor(x);
    const a = s0[i] + (s0[j] - s0[i]) * fr, b = s1[i] + (s1[j] - s1[i]) * fr;
    return a + (b - a) * f;
  };
  const N = 160;
  const pts: string[] = [];
  for (let i = 0; i <= N; i++) {
    const ph = i / N;
    let rp = ph;
    if (mo > 0) switch (mt) {
      case 1: rp = Math.pow(ph, 1 + 3 * mo); break;
      case 2: rp = Math.min(1, ph * (1 + 3 * mo)); break;
      case 3: { const s = 2 * (1 - 0.5 * mo); rp = ph < 0.5 ? ph * s : 1 - (ph - 0.5) * s; break; }
      case 4: { const mir = ph < 0.5 ? ph : 1 - ph; rp = ph + mo * (mir - ph); break; }
      case 5: rp = (ph * (1 + 7 * mo)) % 1; break;
      case 6: rp = Math.abs(((ph * (1 + 3 * mo)) % 2) - 1); break;
      case 7: { const w = 1 - 0.85 * mo; rp = ph < w ? ph / w : 0; break; }
      case 8: { const q = 64 - 60 * mo; rp = Math.floor(ph * q) / q; break; }
      case 10: rp = ph + 0.5 * mo * Math.sin(2 * Math.PI * ph * 2); break;
      default: break;
    }
    let y = ab <= 0 ? read(ta, rp) : ab >= 1 ? read(tb, rp) : read(ta, rp) * (1 - ab) + read(tb, rp) * ab;
    if (mo > 0 && mt === 9) { const q = 16 - 14 * mo; y = Math.round(y * q) / q; }
    if (mo > 0 && mt === 11) y *= 1 - mo * 0.5 * (1 + Math.sin(ph * 2 * Math.PI * 3));
    if (mo > 0 && mt === 12) y *= (1 - mo) + mo * Math.sin(ph * 2 * Math.PI * 5);
    switch (md) {
      case 1: y = Math.tanh(2.2 * y) * 0.9; break;
      case 2: y = 1.6 * Math.abs(y) - 0.7; break;
      case 3: y = Math.round(y * 6) / 6; break;
      case 4: y = Math.round(y * 4) / 4; break;
      case 5: y = Math.round(y * 12) / 12; break;
      case 6: y = Math.tanh(1.4 * y) + 0.25 * Math.sin(Math.PI * y); break;
      default: break;
    }
    pts.push(`${(ph * 300).toFixed(1)},${(45 - Math.max(-1.2, Math.min(1.2, y)) * 36).toFixed(1)}`);
  }
  // ondes voisines de la table A, en ombre
  const ghosts = [-2, -1, 1, 2].map((dk) => {
    const fp = Math.max(0, Math.min(FRAMES - 1, Math.round(pos * (FRAMES - 1)) + dk));
    const s = shapes[ta * FRAMES + fp];
    return s ? "M" + s.map((val, i) => `${((i / (s.length - 1)) * 300).toFixed(1)},${(45 - val * 30 + dk * 5).toFixed(1)}`).join(" L") : "";
  });
  return (
    <div className="gl-osc">
      <svg viewBox="0 0 300 90" preserveAspectRatio="none" aria-hidden>
        <line x1={0} x2={300} y1={45} y2={45} className="gl-osc-axis" />
        {ghosts.map((d, i) => d && <path key={i} d={d} className="gl-osc-ghost" />)}
        <polyline points={pts.join(" ")} className="gl-osc-wave" />
      </svg>
      <span className="gl-osc-t">{TABLES[ta] ?? ""}{ab > 0 ? ` ⇄ ${TABLES[tb] ?? ""}` : ""}</span>
      <span className="gl-osc-b">pos {Math.round(pos * 100)} % · {MORPHS[mt] ?? ""}{mt ? ` ${Math.round(mo * 100)} %` : ""}</span>
    </div>
  );
}

/** Un couloir de la séquence de mouvement : 16 pas, la boucle (début → fin) et le pas joué. */
export function GlutaLane({ channel, module: m, lane }: { channel: number; module: ModuleDesc; lane: string }) {
  const v = usePlain(channel, m);
  const li = Math.max(0, "abcd".indexOf(lane));
  const playing = useStore((s) => (s.focus === channel ? s.tick.inst?.[2 + li] : undefined));
  const value = useStore((s) => (s.focus === channel ? s.tick.inst?.[9 + li] : undefined));
  const at = (id: string) => { const i = m.params.findIndex((p) => p.id === id); return i < 0 ? -1 : paramAddress(channel, "inst", 0, i); };
  const s0 = Math.round(v(`sq${lane}start`)) - 1, e0 = Math.round(v(`sq${lane}end`)) - 1;
  const lo = Math.min(s0, e0), hi = Math.max(s0, e0);
  return (
    <div className="steps-ed gl-lane">
      <div className="steps-ed-bars">
        {Array.from({ length: 16 }, (_, i) => (
          <div key={i} className={`${i < lo || i > hi ? "steps-ed-out" : ""}${playing === i ? " gl-lane-now" : ""}`}>
            <StepBar global={at(`sq${lane}s${i + 1}`)} index={i} />
          </div>
        ))}
      </div>
      {value !== undefined && <div className="gl-lane-val"><em style={{ left: `${value < 0 ? 50 + value * 50 : 50}%`, width: `${Math.abs(value) * 50}%` }} /></div>}
    </div>
  );
}
