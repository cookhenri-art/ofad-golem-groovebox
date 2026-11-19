// Groovebox — OFAD — AGPL-3.0-or-later
// Good Doctor 2 (après la 1.1) : écrans des oscillateurs (table en relief, nuage de grains, partiels),
// réglages propres au mode choisi, dessin des enveloppes à courbes et des LFO, mélangeur et chaîne
// d'effets. Le moteur est dans src/instruments/GoodDoctor2.h ; l'éditeur de tables dans Gd2TableEditor.tsx.

import { useEffect, useMemo, useRef, useState } from "react";
import { actions, useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import { paramAddress } from "../../controls/bindings";
import type { ModuleDesc } from "../../engine/protocol";
import { Control } from "./SynthPanel";

export const GD2_FRAMES = 12;
export const GD2_BUILTIN = 16;
export const GD2_TABLES = ["Analogique", "Voyelles", "Cloches FM", "Plis", "Harm. paires", "Granuleux", "Cordes frottées", "8 bits",
  "Scie qui gronde", "Synchro balayée", "Carré qui se pince", "Sinus sali", "Kick", "Hoover", "Sinus → carré", "Voix",
  "Perso 1", "Perso 2", "Perso 3"];
const MODES = ["Tables", "Granulaire", "Spectral"];
const DESTS = ["Filtres", "F1", "F2", "Direct"];
const GSRC = ["Échantillon", "Chœur", "Souffle", "Cloches", "Voix", "Cordes", "Grondement"];
const SSRC = ["Table", "Échantillon", "Chœur", "Souffle", "Cloches", "Voix", "Cordes", "Grondement"];
export const GD2_FX = [
  { k: "hy", n: "Élargisseur" }, { k: "ds", n: "Distorsion" }, { k: "fl", n: "Flanger" }, { k: "ph", n: "Phaser" }, { k: "ch", n: "Chorus" },
  { k: "dl", n: "Délai" }, { k: "cp", n: "Compresseur" }, { k: "fi", n: "Filtre" }, { k: "eq", n: "Égaliseur" }, { k: "rv", n: "Réverb." },
];

/** Valeurs réelles des réglages de l'instrument. */
export function useGd2(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  return useMemo(() => {
    const idx = new Map(m.params.map((p, i) => [p.id, i]));
    const v = (id: string) => {
      const i = idx.get(id);
      return i === undefined || !values ? 0 : toPlain(m.params[i], values[i] ?? 0);
    };
    const set = (id: string, plain: number) => {
      const i = idx.get(id);
      if (i === undefined) return;
      actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain));
    };
    return { v, set };
  }, [values, m, channel]);
}

/** Onde d'une table calculée à la position pos (0..1), 64 points interpolés entre deux ondes voisines. */
function tableWave(shapes: number[][], tab: number, pos: number) {
  const fp = pos * (GD2_FRAMES - 1), k0 = Math.min(Math.floor(fp), GD2_FRAMES - 2), f = fp - k0;
  const a = shapes[tab * GD2_FRAMES + k0], b = shapes[tab * GD2_FRAMES + k0 + 1];
  if (!a || !b) return null;
  return a.map((x, i) => x + (b[i] - x) * f);
}

/** Tables perso lues dans le moteur (emplacements 2 à 4), gardées en mémoire par piste et par fichier. */
const userCache = new Map<string, { frames: Float32Array[] } | null>();
export function useUserTable(channel: number, slot: number | null) {
  const samples = useStore((s) => s.channels[channel]?.samples);
  const name = slot !== null ? samples?.[slot] ?? "" : "";
  const key = `${channel}:${slot}:${name}`;
  const [, setTick] = useState(0);
  useEffect(() => {
    if (slot === null || !name || userCache.has(key)) return;
    userCache.set(key, null);
    void actions.tableFrames(channel, slot).then((r) => { userCache.set(key, r); setTick((t) => t + 1); });
  }, [key, slot, name, channel]);
  return slot === null || !name ? null : userCache.get(key) ?? null;
}
/** Après un enregistrement : la table de cet emplacement est relue. */
export function forgetUserTable(channel: number, slot: number) {
  for (const k of [...userCache.keys()]) if (k.startsWith(`${channel}:${slot}:`)) userCache.delete(k);
}

/** Écran d'un oscillateur : la table en relief (tables), le nuage de grains (granulaire), les partiels (spectral). */
export function Gd2Osc({ channel, module: m, osc }: { channel: number; module: ModuleDesc; osc: string }) {
  const { v } = useGd2(channel, m);
  const k = ["a", "b", "c"].includes(osc) ? osc : "a";
  const on = v(`${k}_on`) > 0.5;
  const mode = Math.round(v(`${k}_mode`));
  const tab = Math.round(v(`${k}_tab`)), pos = v(`${k}_pos`) / 100;
  const user = useUserTable(channel, tab >= GD2_BUILTIN ? tab - GD2_BUILTIN + 1 : null);
  const shapes = m.waveShapes ?? [];
  const W = 300, H = 110;
  let body: React.ReactNode = null;
  let title = "";
  if (mode === 0) {
    // ondes voisines en relief, l'onde jouée en avant
    const frames: number[][] = [];
    if (tab < GD2_BUILTIN) for (let f = 0; f < GD2_FRAMES; f++) frames.push(shapes[tab * GD2_FRAMES + f] ?? []);
    else if (user) for (const fr of user.frames) frames.push(Array.from({ length: 128 }, (_, i) => fr[Math.floor((i / 128) * fr.length)]));
    const cur = tab < GD2_BUILTIN ? tableWave(shapes, tab, pos) : frames.length ? frames[Math.min(frames.length - 1, Math.round(pos * (frames.length - 1)))] : null;
    const n = frames.length;
    const ghosts = frames.map((s, fi) => {
      if (!s.length) return "";
      const depth = n > 1 ? fi / (n - 1) : 0;
      const dx = 50 - depth * 50, dy = 34 - depth * 30, sc = 0.62;
      return "M" + s.map((val, i) => `${(dx + (i / (s.length - 1)) * W * sc).toFixed(1)},${(H * 0.62 - dy + 20 - val * 18).toFixed(1)}`).join(" L");
    });
    const curPts = cur ? cur.map((val, i) => `${((i / (cur.length - 1)) * W).toFixed(1)},${(H / 2 - val * 40).toFixed(1)}`).join(" ") : "";
    body = (
      <>
        {ghosts.map((d, i) => d && <path key={i} d={d} className={`gd2-ghost${Math.abs(i / Math.max(1, n - 1) - pos) < 0.5 / Math.max(1, n - 1) ? " cur" : ""}`} />)}
        {curPts && <polyline points={curPts} className="gd2-wave" />}
        {!cur && <text x={W / 2} y={H / 2} className="gd2-empty" textAnchor="middle">Table vide — Éditeur de tables</text>}
      </>
    );
    title = `${GD2_TABLES[tab] ?? ""} · ${Math.round(pos * 100)} %${v(`${k}_uni`) > 1 ? ` · unisson ${Math.round(v(`${k}_uni`))}` : ""}`;
  } else if (mode === 1) {
    // nuage de grains : zone de départ (position ± éparpillement), taille et densité
    const size = v(`${k}_gsize`), dens = v(`${k}_gdens`), spray = v(`${k}_gspray`) / 100;
    const count = Math.min(60, Math.round(dens / 3) + 4);
    const rnd = (i: number) => { const x = Math.sin(i * 12.9898 + tab * 7.1) * 43758.5453; return x - Math.floor(x); };
    const grains = Array.from({ length: count }, (_, i) => {
      const x = (pos + (rnd(i) - 0.5) * spray) % 1;
      return { x: ((x + 1) % 1) * W, y: 18 + rnd(i + 99) * (H - 36), w: Math.max(3, (size / 500) * 60), o: 0.25 + rnd(i + 7) * 0.6 };
    });
    body = (
      <>
        <rect x={Math.max(0, (pos - spray / 2) * W)} y={8} width={Math.max(2, spray * W)} height={H - 16} className="gd2-spray" />
        {grains.map((g, i) => <ellipse key={i} cx={g.x} cy={g.y} rx={g.w / 2} ry={4} className="gd2-grain" style={{ opacity: g.o }} />)}
        <line x1={pos * W} x2={pos * W} y1={4} y2={H - 4} className="gd2-head" />
      </>
    );
    title = `${GSRC[Math.round(v(`${k}_gsrc`))] ?? ""} · ${Math.round(size)} ms · ${Math.round(dens)}/s`;
  } else {
    // partiels : allure de la source (table : ses harmoniques ; sinon une pente), pente, pair/impair, coupure
    const parts = Math.round(v(`${k}_sparts`)), tilt = v(`${k}_stilt`), odd = v(`${k}_sodd`) / 100, cut = v(`${k}_scut`);
    const src = Math.round(v(`${k}_ssrc`));
    const base = src === 0 ? tableWave(shapes, Math.min(tab, GD2_BUILTIN - 1), pos) : null;
    const harm = (h: number) => {
      if (base) {
        let re = 0, im = 0;
        for (let i = 0; i < base.length; i++) { const a = (2 * Math.PI * h * i) / base.length; re += base[i] * Math.cos(a); im -= base[i] * Math.sin(a); }
        return Math.hypot(re, im) / (base.length / 2);
      }
      return 1 / Math.sqrt(h);
    };
    const bars = Array.from({ length: parts }, (_, i) => {
      const h = i + 1;
      let a = h <= 31 || !base ? harm(h) : 0;
      a *= Math.pow(10, (tilt * Math.log2(h)) / 20);
      if (odd > 0 && h % 2 === 0) a *= 1 - odd;
      if (odd < 0 && h % 2 === 1 && h > 1) a *= 1 + odd;
      a /= 1 + Math.pow((h * 130) / cut, 8);
      return a;
    });
    const mx = Math.max(1e-6, ...bars);
    const bw = W / Math.max(1, parts);
    body = <>{bars.map((a, i) => <rect key={i} x={i * bw + 1} width={Math.max(1, bw - 2)} y={H - 20 - (a / mx) * (H - 38)} height={(a / mx) * (H - 38)} className="gd2-bar" />)}</>;
    title = `${SSRC[src] ?? ""} · ${parts} partiels`;
  }
  return (
    <div className={`gd2-osc${on ? "" : " gd2-osc--off"}`}>
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" aria-hidden>
        <line x1={0} x2={W} y1={H / 2} y2={H / 2} className="gd2-axis" />
        {body}
      </svg>
      <span className="gd2-osc-t">{k.toUpperCase()} · {MODES[mode] ?? ""} · {title}</span>
      <span className="gd2-osc-b">{on ? "" : "éteint · "}→ {DESTS[Math.round(v(`${k}_dest`))] ?? ""}{v(`${k}_inv`) > 0.5 ? " · inversé" : ""}{v(`${k}_delay`) > 0.05 ? ` · retard ${v(`${k}_delay`).toFixed(1)} ms` : ""}</span>
    </div>
  );
}

/** Réglages propres au mode de l'oscillateur (tables : déformations et unisson ; granulaire ; spectral). */
export function Gd2ModeKnobs({ channel, module: m, osc }: { channel: number; module: ModuleDesc; osc: string }) {
  const { v } = useGd2(channel, m);
  const k = ["a", "b", "c"].includes(osc) ? osc : "a";
  const mode = Math.round(v(`${k}_mode`));
  const ids: [string, string, string?][] =
    mode === 1 ? [["gsrc", "Source", "menu"], ["gsize", "Taille", "big"], ["gdens", "Densité", "big"], ["gspray", "Éparpil."], ["gscan", "Balayage"],
      ["gpitch", "Hasard haut.", "small"], ["gwin", "Fenêtre", "menu"], ["grev", "À l'envers", "small"], ["uwidth", "Largeur", "small"]]
    : mode === 2 ? [["ssrc", "Source", "menu"], ["sparts", "Partiels", "small"], ["sstretch", "Étirement", "big"], ["sshift", "Décalage"], ["stilt", "Pente"],
      ["sodd", "Pair/impair"], ["sblur", "Flou", "small"], ["scut", "Coupure", "big"], ["sscan", "Balayage", "small"]]
    : [["w1", "Déform. 1", "menu"], ["w1amt", "Intens. 1", "big"], ["w2", "Déform. 2", "menu"], ["w2amt", "Intens. 2"], ["uni", "Unisson", "small"],
      ["udet", "Désacc.", "small"], ["ublend", "Mélange", "small"], ["uwidth", "Largeur", "small"]];
  return (
    <div className="gd2-mode fp-grid" style={{ gridTemplateColumns: `repeat(${mode === 0 ? 4 : 5}, minmax(0, 1fr))` }}>
      {ids.map(([id, label, kind]) => (
        <Control key={id} channel={channel} module={m} ctl={{ id: `${k}_${id}`, label, k: (kind as never) ?? "knob" }} />
      ))}
    </div>
  );
}

/** Dessin d'une enveloppe : attaque, tenue, déclin, maintien, relâchement, avec leurs courbes. */
export function Gd2Env({ channel, module: m, env }: { channel: number; module: ModuleDesc; env: string }) {
  const { v } = useGd2(channel, m);
  const e = `e${["1", "2", "3", "4"].includes(env) ? env : "1"}`;
  const a = v(`${e}a`), h = v(`${e}h`), d = v(`${e}d`), s = v(`${e}s`) / 100, r = v(`${e}r`);
  const ca = v(`${e}ca`) * 0.07, cd = v(`${e}cd`) * 0.07, cr = v(`${e}cr`) * 0.07;
  const curve = (x: number, kk: number) => (Math.abs(kk) < 1e-3 ? x : (Math.exp(kk * x) - 1) / (Math.exp(kk) - 1));
  // échelle de temps « douce » : chaque étape garde une place lisible
  const w = (ms: number) => 8 + Math.sqrt(Math.max(0, ms)) * 1.6;
  const wa = w(a), wh = h > 0 ? w(h) : 0, wd = w(d), ws = 40, wr = w(r);
  const total = wa + wh + wd + ws + wr;
  const W = 300, H = 80, sx = W / total, y = (val: number) => H - 6 - val * (H - 14);
  const pts: string[] = [];
  const seg = (x0: number, wseg: number, f: (t: number) => number) => { for (let i = 0; i <= 24; i++) { const t = i / 24; pts.push(`${((x0 + t * wseg) * sx).toFixed(1)},${y(f(t)).toFixed(1)}`); } };
  seg(0, wa, (t) => curve(t, ca));
  if (wh) seg(wa, wh, () => 1);
  seg(wa + wh, wd, (t) => 1 + (s - 1) * curve(t, cd));
  seg(wa + wh + wd, ws, () => s);
  seg(wa + wh + wd + ws, wr, (t) => s * (1 - curve(t, cr)));
  const names = ["ampli", "filtres", "hauteur", "libre"];
  return (
    <div className="gd2-env">
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" aria-hidden>
        <polygon points={`0,${H - 6} ${pts.join(" ")} ${W},${H - 6}`} className="gd2-env-fill" />
        <polyline points={pts.join(" ")} className="gd2-env-line" />
      </svg>
      <span className="gd2-osc-t">Env. {e.slice(1)} · {names[Number(e.slice(1)) - 1]}{e === "e3" ? ` · ${Math.round(v("pamt"))} dt` : ""}</span>
    </div>
  );
}

/** Dessin d'un LFO (forme, mode). */
export function Gd2Lfo({ channel, module: m, lfo }: { channel: number; module: ModuleDesc; lfo: string }) {
  const { v } = useGd2(channel, m);
  const l = `l${["1", "2", "3"].includes(lfo) ? lfo : "1"}`;
  const shape = Math.round(v(`${l}shape`)), mode = Math.round(v(`${l}mode`)), ph = v(`${l}phase`) / 360;
  const f = (x: number) => {
    switch (shape) {
      case 0: return Math.sin(2 * Math.PI * x);
      case 1: return 1 - 4 * Math.abs(x - 0.5);
      case 2: return 2 * x - 1;
      case 3: return 1 - 2 * x;
      case 4: return x < 0.5 ? 1 : -1;
      case 5: return (2 * (1 - Math.exp(-7 * x))) / (1 - Math.exp(-7)) - 1;
      case 6: return 2 * x * x * x - 1;
      case 7: { const i = Math.floor(x * 8); return Math.sin(i * 91.3) * 0.9; }
      default: { const i = Math.floor(x * 8), t = x * 8 - i, c = 0.5 - 0.5 * Math.cos(Math.PI * t); const a = Math.sin(i * 91.3) * 0.9, b = Math.sin((i + 1) * 91.3) * 0.9; return a + (b - a) * c; }
    }
  };
  const W = 300, H = 70;
  const cycles = mode === 2 ? 1 : 2;
  const pts = Array.from({ length: 241 }, (_, i) => {
    const t = i / 240;
    let x = t * cycles + ph;
    if (mode === 2 && x >= 1 + ph) x = 1 + ph - 1e-6;
    return `${(t * W).toFixed(1)},${(H / 2 - f(x - Math.floor(x)) * (H / 2 - 6)).toFixed(1)}`;
  });
  const modes = ["libre", "redéclenché", "en un coup"];
  return (
    <div className="gd2-env gd2-lfo">
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" aria-hidden>
        <line x1={0} x2={W} y1={H / 2} y2={H / 2} className="gd2-axis" />
        <polyline points={pts.join(" ")} className="gd2-env-line" />
      </svg>
      <span className="gd2-osc-t">LFO {l.slice(1)} · {modes[mode] ?? ""}</span>
    </div>
  );
}

/** Mélangeur : schéma des sorties des cinq sources vers les filtres ; monter un niveau allume sa source. */
export function Gd2Mix({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const { v, set } = useGd2(channel, m);
  const values = useStore((s) => s.channels[channel]?.inst);
  const last = useStore((s) => s.lastParam);
  const prev = useRef<Record<string, number>>({});
  useEffect(() => {
    if (!values) return;
    for (const [lvl, on] of [["a_lvl", "a_on"], ["b_lvl", "b_on"], ["c_lvl", "c_on"], ["sub", "sub_on"], ["noise", "noise_on"]]) {
      const li = m.params.findIndex((p) => p.id === lvl);
      if (li < 0) continue;
      const lv = values[li] ?? 0, before = prev.current[lvl];
      prev.current[lvl] = lv;
      if (before === undefined || lv <= before || last !== paramAddress(channel, "inst", 0, li)) continue;
      if (v(on) <= 0.5) set(on, 1);
    }
  }, [values, last, channel, m, v, set]);
  const src = [
    { n: "A", on: v("a_on") > 0.5, d: Math.round(v("a_dest")) },
    { n: "B", on: v("b_on") > 0.5, d: Math.round(v("b_dest")) },
    { n: "C", on: v("c_on") > 0.5, d: Math.round(v("c_dest")) },
    { n: "Sous-osc.", on: v("sub_on") > 0.5, d: Math.round(v("subdest")) },
    { n: "Bruit", on: v("noise_on") > 0.5, d: Math.round(v("ndest")) },
  ];
  const par = v("route") > 0.5;
  return (
    <div className="gd-route gd2-route" aria-label="Sorties vers les filtres">
      {src.map((s) => <span key={s.n} className={`gd-route-src${s.on ? " on" : ""}`}><b>{s.n}</b> → {DESTS[s.d]}</span>)}
      <span className="gd-route-chain">{par ? "F1 ‖ F2 (parallèle)" : "F1 → F2 (série)"} · {v("f1on") > 0.5 ? "F1" : "F1 éteint"} · {v("f2on") > 0.5 ? "F2" : "F2 éteint"}</span>
    </div>
  );
}

/** Chaîne d'effets : les modules dans leur ordre ; allumer, monter, descendre. */
export function Gd2Chain({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const { v, set } = useGd2(channel, m);
  const order = GD2_FX.map((f, i) => ({ ...f, i, rank: Math.round(v(`${f.k}_ord`)), on: v(`${f.k}_on`) > 0.5 }))
    .sort((a, b) => a.rank - b.rank || a.i - b.i);
  // déplacer : les rangs sont renumérotés 1..10 dans le nouvel ordre
  const move = (pos: number, d: number) => {
    const to = pos + d;
    if (to < 0 || to >= order.length) return;
    const next = [...order];
    [next[pos], next[to]] = [next[to], next[pos]];
    next.forEach((f, r) => { if (Math.round(v(`${f.k}_ord`)) !== r + 1) set(`${f.k}_ord`, r + 1); });
  };
  return (
    <ol className="gd2-chain" aria-label="Ordre des effets">
      {order.map((f, pos) => (
        <li key={f.k} className={f.on ? "on" : ""}>
          <button type="button" className="gd2-chain-on" aria-pressed={f.on} onClick={() => set(`${f.k}_on`, f.on ? 0 : 1)} title={f.on ? "Éteindre" : "Allumer"}>
            <span className="fc-led" aria-hidden />{f.n}
          </button>
          <span className="gd2-chain-mv">
            <button type="button" onClick={() => move(pos, -1)} disabled={pos === 0} aria-label={`${f.n} : plus tôt dans la chaîne`}>▲</button>
            <button type="button" onClick={() => move(pos, 1)} disabled={pos === order.length - 1} aria-label={`${f.n} : plus tard dans la chaîne`}>▼</button>
          </span>
        </li>
      ))}
    </ol>
  );
}
