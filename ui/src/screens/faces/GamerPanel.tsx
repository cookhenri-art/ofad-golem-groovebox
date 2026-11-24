/** 1.3 : Gamer Lover — le téléviseur à tube de la façade. Il suit la puce choisie : onde 8 bits (largeur,
    triangle en escalier, bruit), table 4 bits du portable (même calcul que le moteur, gbWaveValue),
    schéma des 4 opérateurs FM (mêmes algorithmes que GamerLover.h), échantillon 16 bits, grille des
    16 pads de la batterie. Sous l'écran, les cinq cartouches choisissent la puce. */
import { useState, type ReactNode } from "react";
import { actions, useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam, useWriteTarget, writeParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";

const CHIPS = ["8 bits", "Portable", "16 bits FM", "16 bits éch.", "Batterie"];
// couleur du phosphore par puce (vert de console de salon, vert olive du portable, bleu FM, ambre, rouge)
const PHOS = ["oklch(0.86 0.2 145)", "oklch(0.84 0.14 120)", "oklch(0.8 0.15 235)", "oklch(0.85 0.15 75)", "oklch(0.75 0.2 25)"];
const DUTIES = [0.125, 0.25, 0.5, 0.75];

/** Même calcul que le moteur (GamerLover.h, gbWaveValue) : 32 pas sur 16 niveaux. */
function gbWave(w: number, step: number) {
  const x = (step + 0.5) / 32, tw = 2 * Math.PI;
  let v: number;
  switch (w) {
    case 0: v = Math.sin(tw * x); break;
    case 1: v = 2 * x - 1; break;
    case 2: v = Math.tanh(3 * Math.sin(tw * x)); break;
    case 3: v = (Math.sin(tw * x) + 0.6 * Math.sin(2 * tw * x) + 0.4 * Math.sin(3 * tw * x)) / 1.6; break;
    case 4: v = (Math.sin(tw * x) + 0.8 * Math.sin(3 * tw * x) * Math.cos(tw * x)) / 1.4; break;
    case 5: v = x < 0.5 ? Math.sin(tw * x) : 0.3 * Math.sin(2 * tw * x); break;
    case 6: v = (Math.sin(tw * x) + 0.7 * Math.sin(5 * tw * x)) / 1.5; break;
    default: v = (Math.sin(tw * x) + Math.sin(7 * tw * x + 1) * 0.8) / 1.6;
  }
  const q = Math.min(15, Math.max(0, Math.round((v + 1) * 7.5)));
  return q / 7.5 - 1;
}

/** Algorithmes FM (GamerLover.h) : qui module qui ([source, cible]) et qui s'entend. */
const FM_ALGOS: { links: [number, number][]; out: number[] }[] = [
  { links: [[0, 1], [1, 2], [2, 3]], out: [3] },
  { links: [[0, 2], [1, 2], [2, 3]], out: [3] },
  { links: [[1, 2], [0, 3], [2, 3]], out: [3] },
  { links: [[0, 1], [1, 3], [2, 3]], out: [3] },
  { links: [[0, 1], [2, 3]], out: [1, 3] },
  { links: [[0, 1], [0, 2], [0, 3]], out: [1, 2, 3] },
  { links: [[0, 1]], out: [1, 2, 3] },
  { links: [], out: [0, 1, 2, 3] },
];

function useGamer(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const target = useWriteTarget(channel);
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const v = (id: string) => { const i = idx(id); return i < 0 || !values ? 0 : toPlain(m.params[i], values[i] ?? 0); };
  const set = (id: string, plain: number) => { const i = idx(id); if (i >= 0) writeParam(target, paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain)); };
  return { v, set, ok: !!values };
}

/** Tracé d'une période en escalier (points x 0..1, y -1..1) dans la zone de l'écran. */
function stepPath(ys: number[], W: number, H: number) {
  const n = ys.length, top = H * 0.18, h = H * 0.56;
  const Y = (y: number) => top + (1 - (y + 1) / 2) * h;
  let d = `M 0 ${Y(ys[0]).toFixed(1)}`;
  for (let k = 0; k < n * 2; k++) {
    const y = ys[k % n];
    const x0 = (k / n) * (W / 2), x1 = ((k + 1) / n) * (W / 2);
    d += ` L ${x0.toFixed(1)} ${Y(y).toFixed(1)} L ${x1.toFixed(1)} ${Y(y).toFixed(1)}`;
  }
  return d;
}

function Screen({ v, chip, W = 400, H = 150, channel }: { v: (id: string) => number; chip: number; W?: number; H?: number; channel?: number }) {
  const wave8 = Math.round(v("wave8"));
  let body: ReactNode = null;
  let caption = "";
  if (chip <= 1) {
    let ys: number[];
    if (wave8 === 0) {
      const du = DUTIES[Math.round(v("duty"))] ?? 0.5;
      ys = Array.from({ length: 32 }, (_, k) => ((k + 0.5) / 32 < du ? 1 : -1));
      caption = `Carré ${Math.round(du * 1000) / 10} %`.replace(".", ",");
      const mv = Math.round(v("dutymove"));
      if (mv > 0) caption += ` · ${["", "montée", "alternance", "balayage"][mv]}`;
    } else if (wave8 === 1 && chip === 0) {
      ys = Array.from({ length: 32 }, (_, k) => { const lv = k < 16 ? 15 - k : k - 16; return lv / 7.5 - 1; });
      caption = "Triangle en escalier · 16 niveaux";
    } else if (wave8 === 1) {
      const w = Math.round(v("gbwave"));
      ys = Array.from({ length: 32 }, (_, k) => gbWave(w, k));
      caption = `Table 4 bits · ${["Sinus", "Scie", "Carré doux", "Orgue", "Voix", "Basse", "Cloche", "Métal"][w] ?? ""}`;
    } else {
      // bruit : suite fixe (registre à décalage), plus serrée en mode court
      const short = Math.round(v("noisemode")) === 1;
      let r = 0x4001;
      ys = Array.from({ length: short ? 16 : 32 }, () => { const b = (r ^ (r >> (short ? 6 : 1))) & 1; r = (r >> 1) | (b << 14); return r & 1 ? 1 : -1; });
      caption = short ? "Bruit court · métallique" : "Bruit long";
    }
    body = <path d={stepPath(ys, W, H)} className="gl-trace" />;
  } else if (chip === 2) {
    const a = FM_ALGOS[Math.min(8, Math.max(1, Math.round(v("fmalgo")))) - 1];
    // opérateurs : modulateurs au-dessus de leur cible, entendus sur la ligne du bas
    const depth = [0, 0, 0, 0];
    for (let pass = 0; pass < 4; pass++) for (const [s, t] of a.links) depth[s] = Math.max(depth[s], depth[t] + 1);
    const maxD = Math.max(...depth);
    const rows: number[][] = Array.from({ length: maxD + 1 }, () => []);
    depth.forEach((d, k) => rows[d].push(k));
    const pos = [0, 0, 0, 0].map(() => ({ x: 0, y: 0 }));
    rows.forEach((row, d) => row.forEach((k, j) => {
      pos[k] = { x: ((j + 1) / (row.length + 1)) * W, y: H * 0.62 - (maxD ? (d / maxD) * H * 0.34 : 0) };
    }));
    const fb = Math.round(v("fmfb"));
    body = (
      <g>
        {a.links.map(([s, t], i) => <line key={i} x1={pos[s].x} y1={pos[s].y} x2={pos[t].x} y2={pos[t].y} className="gl-link" />)}
        {a.out.map((k) => <line key={`o${k}`} x1={pos[k].x} y1={pos[k].y} x2={pos[k].x} y2={H * 0.78} className="gl-link gl-link--out" />)}
        <line x1={W * 0.12} y1={H * 0.78} x2={W * 0.88} y2={H * 0.78} className="gl-link gl-link--out" />
        {fb > 0 && <path d={`M ${pos[0].x + 16} ${pos[0].y - 6} q 22 -18 0 -26 q -22 8 -16 20`} className="gl-link" />}
        {pos.map((p, k) => (
          <g key={k}>
            <rect x={p.x - 16} y={p.y - 12} width={32} height={24} rx={2} className={a.out.includes(k) ? "gl-op gl-op--car" : "gl-op"} />
            <text x={p.x} y={p.y + 5} className="gl-optext">{k + 1}</text>
          </g>
        ))}
      </g>
    );
    caption = `Algorithme ${Math.round(v("fmalgo"))} · rétroaction ${fb}`;
  } else if (chip === 3) {
    const s = Math.round(v("smpl"));
    // dessin évocateur : salve qui décroît, quantifiée en blocs (compression par blocs de 16)
    const ys = Array.from({ length: 64 }, (_, k) => {
      const env = Math.exp(-k / (18 + s * 3)) * 0.9 + 0.1;
      const q = Math.round(Math.sin(k * (0.7 + s * 0.23)) * env * 8) / 8;
      return q;
    });
    body = <path d={stepPath(ys, W, H)} className="gl-trace" />;
    caption = `Échantillon · ${["Cordes", "Chœur", "Piano élec.", "Cuivres", "Flûte", "Basse", "Guitare", "Orgue"][s] ?? ""} · ${Math.round(v("smplsmooth")) ? "dure" : "douce"}`;
  } else {
    const names = ["GC 8 b", "GC FM", "CC 8 b", "CC port.", "Clap", "Charl. f.", "Charl. o.", "Cymbale", "Tom gr.", "Tom méd.", "Tom aigu", "Tom FM", "Cloche", "Pièce", "Saut", "Explos."];
    body = (
      <g>
        {names.map((n, k) => {
          // la grille laisse la place du nom de la puce en haut à droite
          const c = k % 4, r = 3 - Math.floor(k / 4), cw = (W - 70) / 4, rh = (H - 20) / 4;
          const x = 10 + c * cw, y = 10 + r * rh;
          return (
            <g key={k} className={channel !== undefined ? "gl-padhit" : undefined}
              onPointerDown={channel !== undefined ? (e) => { e.preventDefault(); actions.noteOn(channel, 36 + k, 0.9, false); } : undefined}
              onPointerUp={channel !== undefined ? () => actions.noteOff(channel, 36 + k) : undefined}
              onPointerLeave={channel !== undefined ? (e) => { if (e.buttons) actions.noteOff(channel, 36 + k); } : undefined}>
              <rect x={x} y={y} width={cw - 6} height={rh - 4} rx={3} className="gl-pad" />
              <text x={x + (cw - 6) / 2} y={y + rh * 0.36} className="gl-padnote" style={{ fontSize: Math.min(9, cw * 0.16) }}>{36 + k}</text>
              <text x={x + (cw - 6) / 2} y={y + rh * 0.76} className="gl-padname" style={{ fontSize: Math.min(9.5, cw * 0.17) }}>{n}</text>
            </g>
          );
        })}
      </g>
    );
    caption = ""; // les pads s'écoutent au toucher
  }
  return (
    <svg viewBox={`0 0 ${W} ${H}`} className="gl-svg" aria-hidden="true">
      {body}
      <text x={10} y={H - 10} className="gl-caption">{caption}</text>
      <text x={W - 10} y={18} className="gl-chiptag">{CHIPS[chip]}</text>
    </svg>
  );
}

export function GamerTv({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const { v, set, ok } = useGamer(channel, m);
  if (!ok) return null;
  const chip = Math.min(4, Math.max(0, Math.round(v("chip"))));
  return (
    <div className="gl-tv" style={{ ["--gl-phos" as string]: PHOS[chip] }}>
      <div className="gl-cabinet">
        <div className="gl-glass">
          <Screen v={v} chip={chip} channel={channel} />
          <span className="gl-scan" />
        </div>
        <div className="gl-grille" aria-hidden="true"><i /><i /><i /><i /><i /></div>
      </div>
      <div className="gl-carts" role="radiogroup" aria-label="Puce">
        {CHIPS.map((n, k) => (
          <button key={k} type="button" role="radio" aria-checked={chip === k} className={`gl-cart${chip === k ? " on" : ""}`}
            style={{ ["--gl-c" as string]: PHOS[k] }} onClick={() => set("chip", k)}>
            <span>{n}</span>
          </button>
        ))}
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------------------------------------
   1.3 (essai) : écran du téléphone « comme sur les machines à pads » — 8 menus à gauche, le téléviseur
   et les pastilles de la puce au centre, une rangée de 8 boutons qui suit le menu choisi (mêmes menus
   que le greffon MPC / Force). Si l'essai tient, d'autres instruments pourront passer à ce modèle. */
const AK_NAV: { n: string; ks: [string, string][] }[] = [
  { n: "Jeu", ks: [["level", "Niveau"], ["tune", "Accord"], ["glide", "Glisse"], ["voices", "Voix"], ["vibdepth", "Vibrato"], ["sweep", "Chute"], ["echomix", "Écho"], ["bits", "Bits"]] },
  { n: "Ondes", ks: [["wave8", "Onde"], ["duty", "Largeur"], ["dutymove", "Animée"], ["gbwave", "Table"], ["noisemode", "Bruit"], ["smpl", "Échant."], ["smplsmooth", "Lecture"], ["tick", "Cadence"]] },
  { n: "FM", ks: [["fmalgo", "Algo."], ["fmfb", "Rétro."], ["fmmul1", "Op. 1"], ["fmmul2", "Op. 2"], ["fmmul3", "Op. 3"], ["fmmul4", "Op. 4"], ["fmdec", "Déclin"], ["fmgrain", "Grain"]] },
  { n: "Niveaux FM", ks: [["fmlev1", "Op. 1"], ["fmlev2", "Op. 2"], ["fmlev3", "Op. 3"], ["fmlev4", "Op. 4"]] },
  { n: "Tables", ks: [["arp", "Arpège"], ["arpspeed", "Vitesse"], ["sweep", "Chute"], ["sweeptime", "Durée"], ["vibdepth", "Vibrato"], ["vibrate", "Vit. vib."], ["vibdelay", "Retard"], ["dutymove", "Animée"]] },
  { n: "Volume", ks: [["vatk", "Attaque"], ["vdec", "Déclin"], ["vsus", "Maintien"], ["vrel", "Relâch."], ["level", "Niveau"]] },
  { n: "Batterie", ks: [["drtune", "Accord"], ["drdecay", "Déclin"], ["drtone", "Timbre"], ["level", "Niveau"]] },
  { n: "Console", ks: [["bits", "Bits"], ["rate", "Fréq."], ["tone", "Filtre"], ["width", "Stéréo"], ["echomix", "Écho"], ["echotime", "Temps"], ["echofb", "Réinj."], ["echofilter", "Filtre écho"]] },
];

function AkKnob({ channel, m, id, label }: { channel: number; m: ModuleDesc; id: string; label: string }) {
  const i = m.params.findIndex((p) => p.id === id);
  const b = useParam(i < 0 ? null : paramAddress(channel, "inst", 0, i), label);
  return <div className="gk-knob"><Encoder binding={b} variant="knob" /></div>;
}

function Pills({ label, items, value, onPick }: { label: string; items: string[]; value: number; onPick: (i: number) => void }) {
  return (
    <div className="gk-pills" role="radiogroup" aria-label={label}>
      <span className="gk-pills-k">{label}</span>
      {items.map((n, i) => (
        <button key={i} type="button" role="radio" aria-checked={value === i} className={`gk-pill${value === i ? " on" : ""}`} onClick={() => onPick(i)}>{n}</button>
      ))}
    </div>
  );
}

export function GamerAkai({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const { v, set, ok } = useGamer(channel, m);
  const [nav, setNav] = useState(0);
  if (!ok) return null;
  const chip = Math.min(4, Math.max(0, Math.round(v("chip"))));
  const choices = (id: string) => m.params.find((p) => p.id === id)?.choices ?? [];
  const pick = (id: string) => ({ items: choices(id), value: Math.round(v(id)), onPick: (i: number) => set(id, i) });
  const ks = AK_NAV[nav].ks;
  return (
    <div className="gk" style={{ ["--gl-phos" as string]: PHOS[chip] }}>
      <nav className="gk-nav" aria-label="Menus">
        {AK_NAV.map((n, i) => (
          <button key={n.n} type="button" className={`gk-navbtn${nav === i ? " on" : ""}`} aria-pressed={nav === i} onClick={() => setNav(i)}>{n.n}</button>
        ))}
      </nav>
      <div className="gk-main">
        <div className="gk-top">
          <div className="gk-tv">
            <div className="gl-glass"><Screen v={v} chip={chip} W={240} H={124} channel={channel} /><span className="gl-scan" /></div>
          </div>
          <div className="gk-info">
            <Pills label="Puce" items={CHIPS} value={chip} onPick={(i) => set("chip", i)} />
            {chip <= 1 && <Pills label="Onde" {...pick("wave8")} items={chip === 0 ? ["Carré", "Triangle", "Bruit"] : ["Carré", "Table 4 bits", "Bruit"]} />}
            {chip <= 1 && Math.round(v("wave8")) === 0 && <Pills label="Largeur" {...pick("duty")} />}
            {chip === 1 && Math.round(v("wave8")) === 1 && <Pills label="Table" {...pick("gbwave")} />}
            {chip === 2 && <Pills label="Algorithme" items={["1", "2", "3", "4", "5", "6", "7", "8"]} value={Math.round(v("fmalgo")) - 1} onPick={(i) => set("fmalgo", i + 1)} />}
            {chip === 3 && <Pills label="Échantillon" {...pick("smpl")} />}
            {chip === 4 && <Pills label="Sortie" {...pick("tone")} />}
          </div>
        </div>
        <div className="gk-knobs" key={nav}>
          {ks.map(([id, label]) => <AkKnob key={id} channel={channel} m={m} id={id} label={label} />)}
          {Array.from({ length: 8 - ks.length }, (_, i) => <div key={`e${i}`} className="gk-knob gk-knob--empty" aria-hidden="true" />)}
        </div>
      </div>
    </div>
  );
}
