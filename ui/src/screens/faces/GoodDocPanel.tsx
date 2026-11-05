// Groovebox — OFAD — AGPL-3.0-or-later
// Good Doctor (v0.47 (maj. 5 nov.)) : l'écran de chaque oscillateur (l'onde lue à sa position, avec ses deux
// déformations, comme le moteur les applique — GoodDoctor.h) et le schéma des sorties vers les filtres.

import { useStore } from "../../engine/store";
import { toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";

const FRAMES = 12;
const TABLES = ["Analogique", "Voyelles", "Cloches FM", "Plis", "Harm. paires", "Granuleux", "Cordes frottées", "8 bits",
  "Scie qui gronde", "Synchro balayée", "Carré qui se pince", "Sinus sali"];
const WARPS = ["—", "Synchro", "Courbure +", "Courbure −", "Pulsation", "Miroir", "Asymétrie", "Quantifier", "FM suiv.", "FM sous-osc.",
  "AM suiv.", "Anneau suiv.", "Repli", "Saturer", "Redresser", "Bits", "Passe-bas", "Passe-haut"];
const DESTS = ["Filtres", "Filtre 1", "Filtre 2", "Direct"];

function usePlain(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  return (id: string) => {
    const i = m.params.findIndex((p) => p.id === id);
    return i < 0 || !values ? 0 : toPlain(m.params[i], values[i] ?? 0);
  };
}

/** Déformation de phase (mêmes formules que warpPhase). L'autre oscillateur est figuré par un sinus. */
function warpPhase(w: number, a: number, ph: number): [number, number] {
  if (a <= 0) return [ph, 1];
  const wrap = (x: number) => x - Math.floor(x);
  switch (w) {
    case 1: { const r = 1 + 7 * a; return [wrap(ph * r), 1 - 0.5 * a * ph]; }
    case 2: return [Math.pow(ph, 1 + 3 * a), 1];
    case 3: return [1 - Math.pow(1 - ph, 1 + 3 * a), 1];
    case 4: { const pw = 1 - 0.9 * a; return [ph < pw ? ph / pw : 0, 1]; }
    case 5: { const mir = ph < 0.5 ? ph : 1 - ph; return [ph + a * (mir - ph), 1]; }
    case 6: { const k = 0.5 - 0.46 * a; return [ph < k ? (0.5 * ph) / k : 0.5 + (0.5 * (ph - k)) / (1 - k), 1]; }
    case 7: { const q = 64 - 60 * a; return [Math.floor(ph * q) / q, 1]; }
    case 8: return [wrap(ph + 0.6 * a * Math.sin(2 * Math.PI * ph * 2)), 1];
    case 9: return [wrap(ph + 0.5 * a * Math.sin(Math.PI * ph)), 1];
    default: return [ph, 1];
  }
}
function warpAmp(w: number, a: number, y: number, ph: number) {
  if (a <= 0) return y;
  switch (w) {
    case 10: return y * ((1 - a) + a * (0.5 + 0.5 * Math.sin(2 * Math.PI * ph * 2)));
    case 11: return y * (1 - a) + y * Math.sin(2 * Math.PI * ph * 2) * a;
    case 12: { let x = y * (1 + 4 * a); x = (x + 1) * 0.25; x -= Math.floor(x); return Math.abs(x * 4 - 2) - 1; }
    case 13: return Math.tanh(y * (1 + 6 * a)) / Math.tanh(1 + 6 * a * 0.3);
    case 14: { const r = 1.6 * Math.abs(y) - 0.7; return y + (r - y) * a; }
    case 15: { const q = 16 - 14.5 * a; return Math.round(y * q) / q; }
    default: return y;
  }
}

export function GoodDocOsc({ channel, module: m, osc }: { channel: number; module: ModuleDesc; osc: string }) {
  const v = usePlain(channel, m);
  const k = ["a", "b", "c"].includes(osc) ? osc : "a";
  const on = v(`${k}_on`) > 0.5;
  const tab = Math.round(v(`${k}_tab`)), pos = v(`${k}_pos`) / 100;
  const w1 = Math.round(v(`${k}_w1`)), a1 = v(`${k}_w1amt`) / 100, w2 = Math.round(v(`${k}_w2`)), a2 = v(`${k}_w2amt`) / 100;
  const shapes = m.waveShapes ?? [];
  const read = (ph: number) => {
    const fp = pos * (FRAMES - 1), k0 = Math.min(Math.floor(fp), FRAMES - 2), f = fp - k0;
    const s0 = shapes[tab * FRAMES + k0], s1 = shapes[tab * FRAMES + k0 + 1];
    if (!s0 || !s1) return 0;
    const x = (((ph % 1) + 1) % 1) * s0.length, i = Math.floor(x) % s0.length, j = (i + 1) % s0.length, fr = x - Math.floor(x);
    const a = s0[i] + (s0[j] - s0[i]) * fr, b = s1[i] + (s1[j] - s1[i]) * fr;
    return a + (b - a) * f;
  };
  const N = 180;
  const pts: string[] = [];
  for (let i = 0; i <= N; i++) {
    const ph = i / N;
    const [r1, g1] = warpPhase(w1, a1, ph);
    const [r2, g2] = warpPhase(w2, a2, r1);
    let y = read(r2) * g1 * g2;
    y = warpAmp(w1, a1, y, ph);
    y = warpAmp(w2, a2, y, ph);
    pts.push(`${(ph * 300).toFixed(1)},${(45 - Math.max(-1.2, Math.min(1.2, y)) * 36).toFixed(1)}`);
  }
  // les ondes voisines de la table, en relief (lecture « 3D » de la position)
  const ghosts = [-3, -2, -1, 1, 2, 3].map((dk) => {
    const fp = Math.max(0, Math.min(FRAMES - 1, Math.round(pos * (FRAMES - 1)) + dk));
    const s = shapes[tab * FRAMES + fp];
    return s ? "M" + s.map((val, i) => `${((i / (s.length - 1)) * 280 + 10 + dk * 3).toFixed(1)},${(45 - val * 26 - dk * 4).toFixed(1)}`).join(" L") : "";
  });
  const warps = [w1 && a1 > 0 ? `${WARPS[w1]} ${Math.round(a1 * 100)} %` : "", w2 && a2 > 0 ? `${WARPS[w2]} ${Math.round(a2 * 100)} %` : ""].filter(Boolean).join(" + ");
  return (
    <div className={`gd-osc${on ? "" : " gd-osc--off"}`}>
      <svg viewBox="0 0 300 90" preserveAspectRatio="none" aria-hidden>
        <line x1={0} x2={300} y1={45} y2={45} className="gd-osc-axis" />
        {ghosts.map((d, i) => d && <path key={i} d={d} className="gd-osc-ghost" />)}
        <polyline points={pts.join(" ")} className="gd-osc-wave" />
      </svg>
      <span className="gd-osc-t">{k.toUpperCase()} · {TABLES[tab] ?? ""} · {Math.round(pos * 100)} %</span>
      <span className="gd-osc-b">{on ? (warps || "sans déformation") : "éteint"} → {DESTS[Math.round(v(`${k}_dest`))] ?? ""}</span>
    </div>
  );
}

/** Schéma des sorties : oscillateurs, sous-oscillateur et bruit vers les filtres (série ou parallèle). */
export function GoodDocRoute({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const v = usePlain(channel, m);
  const par = v("route") > 0.5;
  const src = [
    { n: "A", on: v("a_on") > 0.5 && v("a_lvl") > 0, d: Math.round(v("a_dest")) },
    { n: "B", on: v("b_on") > 0.5 && v("b_lvl") > 0, d: Math.round(v("b_dest")) },
    { n: "C", on: v("c_on") > 0.5 && v("c_lvl") > 0, d: Math.round(v("c_dest")) },
    { n: "Sub", on: v("sub") > 0, d: Math.round(v("subdest")) },
    { n: "Bruit", on: v("noise") > 0, d: Math.round(v("ndest")) },
  ];
  const f1 = v("f1on") > 0.5, f2 = v("f2on") > 0.5;
  return (
    <div className="gd-route" aria-label="Sorties vers les filtres">
      {src.map((s) => (
        <span key={s.n} className={`gd-route-src${s.on ? " on" : ""}`}><b>{s.n}</b> → {DESTS[s.d]}</span>
      ))}
      <span className="gd-route-chain">{par ? "F1 ‖ F2 (parallèle)" : "F1 → F2 (série)"} · {f1 ? "F1" : "F1 éteint"} · {f2 ? "F2" : "F2 éteint"}</span>
    </div>
  );
}
