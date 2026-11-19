// Groovebox — OFAD — AGPL-3.0-or-later
// Good Doctor 2 (après la 1.1) : éditeur de tables d'ondes.
//   - des ondes de 2048 points (64 au plus), dessinées au pinceau : libre, ligne, courbes, arc, bruit, lisser, effacer,
//     avec grille horizontale et verticale ;
//   - formule (x de −1 à 1 sur le cycle, t de 0 à 1 d'une onde à l'autre), harmoniques modifiables à la souris ;
//   - traitements : normaliser, inverser, retourner, retirer le continu, lisser, symétrique, fondre (ondes
//     intermédiaires) ;
//   - départ : formes de base, une table calculée, la table enregistrée, ou un WAV importé (format courant :
//     2048 points par onde, à la suite).
// La table est enregistrée comme un WAV dans l'emplacement d'échantillon 2, 3 ou 4 de la piste (« Perso 1 à 3 ») :
// gardée avec le projet, analysée au chargement par le moteur (limitée en bande, hors du fil audio).

import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { actions } from "../../engine/store";
import type { ModuleDesc } from "../../engine/protocol";
import { forgetUserTable, GD2_BUILTIN, GD2_FRAMES, GD2_TABLES, useGd2 } from "./Gd2Panel";

const N = 2048, MAX_FRAMES = 64, HARM = 64;
type Tool = "free" | "line" | "up" | "down" | "arc" | "noise" | "smooth" | "erase";
const TOOLS: { k: Tool; n: string; t: string }[] = [
  { k: "free", n: "✎", t: "Libre" }, { k: "line", n: "╱", t: "Ligne" }, { k: "up", n: "⌒↗", t: "Courbe montante" },
  { k: "down", n: "⌒↘", t: "Courbe descendante" }, { k: "arc", n: "◠", t: "Arc (demi-sinus)" }, { k: "noise", n: "≋", t: "Bruit" },
  { k: "smooth", n: "∿", t: "Lisser" }, { k: "erase", n: "⌫", t: "Effacer (zéro)" },
];

// ---------------------------------------------------------------------------- formes de base
const basic = (kind: string) => {
  const f = new Float32Array(N);
  for (let i = 0; i < N; i++) {
    const p = i / N;
    f[i] = kind === "sin" ? Math.sin(2 * Math.PI * p) : kind === "saw" ? 1 - 2 * p : kind === "sqr" ? (p < 0.5 ? 1 : -1) : 1 - 4 * Math.abs(((p + 0.25) % 1) - 0.5);
  }
  return f;
};

/** Harmoniques (amplitude, phase) d'une onde : les HARM premières. */
function analyse(w: Float32Array) {
  const amp = new Float32Array(HARM), ph = new Float32Array(HARM);
  for (let h = 1; h <= HARM; h++) {
    let re = 0, im = 0;
    for (let i = 0; i < N; i++) { const a = (2 * Math.PI * h * i) / N; re += w[i] * Math.cos(a); im -= w[i] * Math.sin(a); }
    amp[h - 1] = Math.hypot(re, im) / (N / 2);
    ph[h - 1] = Math.atan2(im, re);
  }
  return { amp, ph };
}
function synth(amp: Float32Array, ph: Float32Array) {
  const w = new Float32Array(N);
  for (let h = 1; h <= HARM; h++) {
    const a = amp[h - 1];
    if (a < 1e-5) continue;
    for (let i = 0; i < N; i++) w[i] += a * Math.cos((2 * Math.PI * h * i) / N + ph[h - 1]);
  }
  return w;
}
/** Onde de 64 points (formes montrées par le moteur) → 2048 points par ses harmoniques (pas de marches). */
function fromShape(s: number[]) {
  const M = s.length, amp = new Float32Array(HARM), ph = new Float32Array(HARM);
  for (let h = 1; h < M / 2; h++) {
    let re = 0, im = 0;
    for (let i = 0; i < M; i++) { const a = (2 * Math.PI * h * i) / M; re += s[i] * Math.cos(a); im -= s[i] * Math.sin(a); }
    amp[h - 1] = Math.hypot(re, im) / (M / 2); ph[h - 1] = Math.atan2(im, re);
  }
  return normalize(synth(amp, ph));
}
function normalize(w: Float32Array) {
  let pk = 1e-9;
  for (const x of w) pk = Math.max(pk, Math.abs(x));
  return w.map((x) => x / pk) as Float32Array;
}

// ---------------------------------------------------------------------------- formule (analyseur sûr, sans eval)
type Fn = (x: number, p: number, t: number) => number;
function compile(src: string): Fn | string {
  const toks = src.toLowerCase().match(/\d+\.?\d*(e[+-]?\d+)?|[a-z_]+|\*\*|[-+*/^%(),<>]/g);
  if (!toks || toks.join("").length !== src.replace(/\s+/g, "").toLowerCase().length) return "Caractère non reconnu";
  let i = 0;
  const F: Record<string, (...a: number[]) => number> = {
    sin: Math.sin, cos: Math.cos, tan: Math.tan, abs: Math.abs, sqrt: (a) => Math.sqrt(Math.max(0, a)), exp: Math.exp,
    log: (a) => Math.log(Math.max(1e-12, a)), tanh: Math.tanh, floor: Math.floor, ceil: Math.ceil, round: Math.round, sign: Math.sign,
    min: Math.min, max: Math.max, pow: Math.pow, clamp: (a, lo = -1, hi = 1) => Math.min(hi, Math.max(lo, a)), frac: (a) => a - Math.floor(a),
    tri: (a) => 1 - 4 * Math.abs(((a / (2 * Math.PI) + 0.25) % 1 + 1) % 1 - 0.5), saw: (a) => 1 - 2 * (((a / (2 * Math.PI)) % 1 + 1) % 1),
    sqr: (a) => ((((a / (2 * Math.PI)) % 1) + 1) % 1 < 0.5 ? 1 : -1),
  };
  type Node = (x: number, p: number, t: number) => number;
  const peek = () => toks[i];
  const take = (s?: string) => { const t = toks[i++]; if (s && t !== s) throw new Error(`« ${s} » attendu`); return t; };
  const primary = (): Node => {
    const t = take();
    if (t === undefined) throw new Error("Formule incomplète");
    if (/^\d/.test(t)) { const v = Number(t); return () => v; }
    if (t === "(") { const e = expr(); take(")"); return e; }
    if (t === "-") { const e = unary(); return (x, p, tt) => -e(x, p, tt); }
    if (t === "x") return (x) => x;
    if (t === "p") return (_x, p) => p;
    if (t === "t" || t === "w") return (_x, _p, tt) => tt;
    if (t === "pi") return () => Math.PI;
    if (t === "e") return () => Math.E;
    if (F[t]) {
      take("(");
      const args: Node[] = [expr()];
      while (peek() === ",") { take(","); args.push(expr()); }
      take(")");
      const f = F[t];
      return (x, p, tt) => f(...args.map((a) => a(x, p, tt)));
    }
    throw new Error(`« ${t} » inconnu`);
  };
  const unary = (): Node => primary();
  const power = (): Node => {
    const b = unary();
    if (peek() === "^" || peek() === "**") { take(); const e = power(); return (x, p, t) => Math.pow(b(x, p, t), e(x, p, t)); }
    return b;
  };
  const term = (): Node => {
    let a = power();
    while (peek() === "*" || peek() === "/" || peek() === "%") {
      const op = take(), b = power(), l = a;
      a = op === "*" ? (x, p, t) => l(x, p, t) * b(x, p, t) : op === "/" ? (x, p, t) => l(x, p, t) / (b(x, p, t) || 1e-12) : (x, p, t) => l(x, p, t) % b(x, p, t);
    }
    return a;
  };
  const expr = (): Node => {
    let a = term();
    while (peek() === "+" || peek() === "-" || peek() === "<" || peek() === ">") {
      const op = take(), b = term(), l = a;
      a = op === "+" ? (x, p, t) => l(x, p, t) + b(x, p, t) : op === "-" ? (x, p, t) => l(x, p, t) - b(x, p, t)
        : op === "<" ? (x, p, t) => (l(x, p, t) < b(x, p, t) ? 1 : 0) : (x, p, t) => (l(x, p, t) > b(x, p, t) ? 1 : 0);
    }
    return a;
  };
  try {
    const e = expr();
    if (i < toks.length) return `« ${toks[i]} » en trop`;
    return e;
  } catch (err) { return (err as Error).message; }
}

// ---------------------------------------------------------------------------- WAV
function encodeWav(frames: Float32Array[]) {
  const n = frames.length * N, buf = new ArrayBuffer(44 + n * 4), d = new DataView(buf);
  const w = (o: number, s: string) => { for (let i = 0; i < s.length; i++) d.setUint8(o + i, s.charCodeAt(i)); };
  w(0, "RIFF"); d.setUint32(4, 36 + n * 4, true); w(8, "WAVE"); w(12, "fmt "); d.setUint32(16, 16, true);
  d.setUint16(20, 3, true); d.setUint16(22, 1, true); d.setUint32(24, 48000, true); d.setUint32(28, 48000 * 4, true);
  d.setUint16(32, 4, true); d.setUint16(34, 32, true); w(36, "data"); d.setUint32(40, n * 4, true);
  let o = 44;
  for (const f of frames) for (let i = 0; i < N; i++, o += 4) d.setFloat32(o, Math.max(-1, Math.min(1, f[i])), true);
  return buf;
}
function decodeWav(buf: ArrayBuffer): Float32Array | string {
  const d = new DataView(buf);
  const str = (o: number) => String.fromCharCode(d.getUint8(o), d.getUint8(o + 1), d.getUint8(o + 2), d.getUint8(o + 3));
  if (buf.byteLength < 44 || str(0) !== "RIFF" || str(8) !== "WAVE") return "Ce n'est pas un fichier WAV";
  let o = 12, fmt = 0, ch = 1, bits = 16, data = -1, len = 0;
  while (o + 8 <= buf.byteLength) {
    const id = str(o), size = d.getUint32(o + 4, true);
    if (id === "fmt ") { fmt = d.getUint16(o + 8, true); ch = d.getUint16(o + 10, true); bits = d.getUint16(o + 22, true); if (fmt === 0xfffe) fmt = d.getUint16(o + 32, true); }
    if (id === "data") { data = o + 8; len = Math.min(size, buf.byteLength - data); break; }
    o += 8 + size + (size & 1);
  }
  if (data < 0 || !ch) return "WAV sans données";
  const bps = bits / 8, count = Math.floor(len / (bps * ch)), out = new Float32Array(count);
  for (let i = 0; i < count; i++) {
    const p = data + i * bps * ch;
    out[i] = fmt === 3 ? (bits === 64 ? d.getFloat64(p, true) : d.getFloat32(p, true))
      : bits === 16 ? d.getInt16(p, true) / 32768 : bits === 24 ? ((d.getUint8(p) | (d.getUint8(p + 1) << 8) | (d.getInt8(p + 2) << 16)) / 8388608)
      : bits === 32 ? d.getInt32(p, true) / 2147483648 : (d.getUint8(p) - 128) / 128;
  }
  return out;
}
function splitFrames(x: Float32Array): Float32Array[] {
  if (x.length >= N) {
    const n = Math.min(MAX_FRAMES, Math.floor(x.length / N));
    return Array.from({ length: n }, (_, f) => x.slice(f * N, (f + 1) * N));
  }
  const f = new Float32Array(N);
  for (let i = 0; i < N; i++) { const p = (i / N) * x.length, i0 = Math.floor(p), fr = p - i0; f[i] = x[i0 % x.length] * (1 - fr) + x[(i0 + 1) % x.length] * fr; }
  return [f];
}

// ---------------------------------------------------------------------------- l'éditeur
export function Gd2TableEditor({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const { v, set } = useGd2(channel, m);
  const [slot, setSlot] = useState(1);
  const [frames, setFrames] = useState<Float32Array[]>(() => [basic("saw")]);
  const [cur, setCur] = useState(0);
  const [tool, setTool] = useState<Tool>("free");
  const [gridX, setGridX] = useState(0);
  const [gridY, setGridY] = useState(0);
  const [formula, setFormula] = useState("sin(pi*x) * (1 - t) + saw(pi*x) * t");
  const [msg, setMsg] = useState("");
  const [target, setTarget] = useState("a");
  const undo = useRef<Float32Array[][]>([]);
  const canvas = useRef<HTMLCanvasElement>(null);
  const drag = useRef<{ x0: number; y0: number; lx: number; ly: number; base: Float32Array } | null>(null);
  const fileIn = useRef<HTMLInputElement>(null);

  const frame = frames[Math.min(cur, frames.length - 1)];
  const push = useCallback(() => { undo.current.push(frames.map((f) => f.slice())); if (undo.current.length > 30) undo.current.shift(); }, [frames]);
  const setFrame = (f: Float32Array) => setFrames((fs) => fs.map((x, i) => (i === cur ? f : x)));
  const all = (fn: (f: Float32Array, i: number, n: number) => Float32Array) => { push(); setFrames((fs) => fs.map((f, i) => fn(f, i, fs.length))); };
  const one = (fn: (f: Float32Array) => Float32Array) => { push(); setFrame(fn(frame.slice())); };

  // dessin de l'onde (grille, onde voisine en fond, onde choisie)
  useEffect(() => {
    const c = canvas.current;
    if (!c) return;
    const g = c.getContext("2d");
    if (!g) return;
    const W = c.width, H = c.height;
    g.clearRect(0, 0, W, H);
    g.strokeStyle = "rgba(160,255,190,0.08)"; g.lineWidth = 1;
    const gx = gridX || 16, gy = gridY || 8;
    for (let i = 1; i < gx; i++) { const x = (i / gx) * W; g.beginPath(); g.moveTo(x, 0); g.lineTo(x, H); g.stroke(); }
    for (let i = 1; i < gy; i++) { const y = (i / gy) * H; g.beginPath(); g.moveTo(0, y); g.lineTo(W, y); g.stroke(); }
    g.strokeStyle = "rgba(160,255,190,0.25)"; g.beginPath(); g.moveTo(0, H / 2); g.lineTo(W, H / 2); g.stroke();
    const draw = (f: Float32Array, style: string, width: number) => {
      g.strokeStyle = style; g.lineWidth = width; g.beginPath();
      for (let px = 0; px <= W; px++) { const i = Math.min(N - 1, Math.floor((px / W) * N)); const y = H / 2 - f[i] * (H / 2 - 6); if (px === 0) g.moveTo(px, y); else g.lineTo(px, y); }
      g.stroke();
    };
    if (frames.length > 1) draw(frames[(cur + 1) % frames.length], "rgba(120,200,255,0.18)", 1);
    g.fillStyle = "rgba(130,255,150,0.12)"; g.beginPath(); g.moveTo(0, H / 2);
    for (let px = 0; px <= W; px++) { const i = Math.min(N - 1, Math.floor((px / W) * N)); g.lineTo(px, H / 2 - frame[i] * (H / 2 - 6)); }
    g.lineTo(W, H / 2); g.fill();
    draw(frame, "rgb(150,255,160)", 2);
  }, [frame, frames, cur, gridX, gridY]);

  const toWave = (e: React.PointerEvent) => {
    const r = (e.target as HTMLCanvasElement).getBoundingClientRect();
    let x = Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)), y = Math.max(-1, Math.min(1, -((e.clientY - r.top) / r.height - 0.5) * 2 * 1.03));
    if (gridX) x = Math.round(x * gridX) / gridX;
    if (gridY) y = Math.round(((y + 1) / 2) * gridY) / gridY * 2 - 1;
    return { x: Math.min(N - 1, Math.round(x * N)), y };
  };
  const apply = (f: Float32Array, x0: number, y0: number, x1: number, y1: number, base: Float32Array) => {
    const a = Math.min(x0, x1), b = Math.max(x0, x1);
    const ya = x0 <= x1 ? y0 : y1, yb = x0 <= x1 ? y1 : y0;
    for (let i = a; i <= b; i++) {
      const t = b > a ? (i - a) / (b - a) : 0;
      switch (tool) {
        case "free": case "line": f[i] = ya + (yb - ya) * t; break;
        case "up": f[i] = ya + (yb - ya) * t * t * t; break;
        case "down": f[i] = ya + (yb - ya) * (1 - Math.pow(1 - t, 3)); break;
        case "arc": f[i] = Math.max(-1, Math.min(1, ya + (yb - ya) * t + Math.sin(Math.PI * t) * 0.4 * (ya >= 0 ? 1 : -1))); break;
        case "noise": f[i] = base[i] * 0.2 + (Math.random() * 2 - 1) * Math.abs(yb); break;
        case "erase": f[i] = 0; break;
        case "smooth": { let s = 0, c = 0; for (let k = -24; k <= 24; k++) { s += base[(i + k + N) % N]; c++; } f[i] = s / c; break; }
      }
    }
  };
  const down = (e: React.PointerEvent) => {
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
    push();
    const p = toWave(e);
    drag.current = { x0: p.x, y0: p.y, lx: p.x, ly: p.y, base: frame.slice() };
    if (tool === "free" || tool === "erase" || tool === "noise" || tool === "smooth") { const f = frame.slice(); apply(f, p.x, p.y, p.x, p.y, drag.current.base); setFrame(f); }
  };
  const move = (e: React.PointerEvent) => {
    const d = drag.current;
    if (!d) return;
    const p = toWave(e);
    if (tool === "free" || tool === "erase" || tool === "noise" || tool === "smooth") {
      const f = frame.slice(); apply(f, d.lx, d.ly, p.x, p.y, d.base); setFrame(f); d.lx = p.x; d.ly = p.y;
    } else { const f = d.base.slice(); apply(f, d.x0, d.y0, p.x, p.y, d.base); setFrame(f); }
  };
  const up = () => { drag.current = null; };

  // harmoniques de l'onde choisie, modifiables
  const harm = useMemo(() => analyse(frame), [frame]);
  const harmDrag = useRef(false);
  const setHarm = (e: React.PointerEvent) => {
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    const h = Math.min(HARM - 1, Math.max(0, Math.floor(((e.clientX - r.left) / r.width) * HARM)));
    const a = Math.max(0, Math.min(1, 1 - (e.clientY - r.top) / r.height));
    const amp = harm.amp.slice(); amp[h] = a;
    setFrame(synth(amp, harm.ph));
  };

  const loadFrom = async (k: string) => {
    push();
    if (k === "sin" || k === "saw" || k === "sqr" || k === "tri") { setFrames([basic(k)]); setCur(0); return; }
    if (k === "saved") {
      const r = await actions.tableFrames(channel, slot);
      if (!r) { setMsg(`L'emplacement Perso ${slot} est vide`); return; }
      setFrames(r.frames.map((f) => Float32Array.from(f))); setCur(0); setMsg(`Table Perso ${slot} reprise (${r.frames.length} ondes)`); return;
    }
    const t = Number(k);
    const shapes = m.waveShapes ?? [];
    if (Number.isFinite(t) && t >= 0 && t < GD2_BUILTIN) {
      setFrames(Array.from({ length: GD2_FRAMES }, (_, f) => fromShape(shapes[t * GD2_FRAMES + f] ?? [])));
      setCur(0); setMsg(`Départ : table « ${GD2_TABLES[t]} » (12 ondes)`);
    }
  };
  const importFile = async (file: File) => {
    const r = decodeWav(await file.arrayBuffer());
    if (typeof r === "string") { setMsg(r); return; }
    push();
    const fs = splitFrames(r);
    setFrames(fs); setCur(0); setMsg(`${file.name} : ${fs.length} onde${fs.length > 1 ? "s" : ""} de 2048 points`);
  };
  const save = async () => {
    const wav = encodeWav(frames);
    const now = new Date();
    const stamp = `${now.getHours()}h${String(now.getMinutes()).padStart(2, "0")}m${String(now.getSeconds()).padStart(2, "0")}`;
    const file = new File([wav], `Table perso ${slot} - ${frames.length} ondes ${stamp}.wav`, { type: "audio/wav" });
    if (!(await actions.loadSample(channel, slot, file))) { setMsg(`Perso ${slot} : enregistrement refusé`); return; }
    forgetUserTable(channel, slot);
    if (target !== "-") { set(`${target}_mode`, 0); set(`${target}_tab`, GD2_BUILTIN + slot - 1); set(`${target}_on`, 1); }
    setMsg(`Enregistrée dans Perso ${slot}${target !== "-" ? `, jouée par l'oscillateur ${target.toUpperCase()}` : ""}`);
  };
  const runFormula = (everyFrame: boolean) => {
    const f = compile(formula);
    if (typeof f === "string") { setMsg(`Formule : ${f}`); return; }
    const make = (t: number) => { const w = new Float32Array(N); for (let i = 0; i < N; i++) { const p = i / N; const y = f(p * 2 - 1, p, t); w[i] = Number.isFinite(y) ? Math.max(-1, Math.min(1, y)) : 0; } return w; };
    push();
    if (everyFrame) setFrames((fs) => fs.map((_, i) => make(fs.length > 1 ? i / (fs.length - 1) : 0)));
    else setFrame(make(frames.length > 1 ? cur / (frames.length - 1) : 0));
    setMsg("");
  };

  const smoothW = (f: Float32Array) => { const o = new Float32Array(N); for (let i = 0; i < N; i++) { let s = 0; for (let k = -6; k <= 6; k++) s += f[(i + k + N) % N]; o[i] = s / 13; } return o; };
  const dc = (f: Float32Array) => { let s = 0; for (const x of f) s += x; s /= N; return f.map((x) => x - s) as Float32Array; };
  const morph = () => {
    if (frames.length < 2) { setMsg("Fondre : il faut au moins deux ondes (la première et la dernière)"); return; }
    const a = frames[0], b = frames[frames.length - 1], n = Math.max(frames.length, 8);
    push();
    setFrames(Array.from({ length: n }, (_, k) => { const t = k / (n - 1); return a.map((x, i) => x + (b[i] - x) * t) as Float32Array; }));
  };

  // touches : Ctrl+Z annule
  useEffect(() => {
    const k = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "z" && undo.current.length) { e.preventDefault(); const s = undo.current.pop()!; setFrames(s); setCur((c) => Math.min(c, s.length - 1)); }
    };
    window.addEventListener("keydown", k);
    return () => window.removeEventListener("keydown", k);
  }, []);

  const tabNow = (k: string) => Math.round(v(`${k}_tab`));
  return (
    <div className="gd2-ed">
      <div className="gd2-ed-bar">
        <label>Emplacement
          <select value={slot} onChange={(e) => setSlot(Number(e.target.value))}>
            {[1, 2, 3].map((s) => <option key={s} value={s}>Perso {s} (empl. {s + 1})</option>)}
          </select>
        </label>
        <label>Partir de
          <select value="" onChange={(e) => { if (e.target.value === "file") fileIn.current?.click(); else void loadFrom(e.target.value); }}>
            <option value="" disabled>choisir…</option>
            <option value="sin">Sinus</option><option value="saw">Scie</option><option value="sqr">Carré</option><option value="tri">Triangle</option>
            <option value="saved">La table enregistrée (Perso {slot})</option>
            <option value="file">Importer un WAV…</option>
            {GD2_TABLES.slice(0, GD2_BUILTIN).map((n, i) => <option key={n} value={i}>Table : {n}</option>)}
          </select>
        </label>
        <input ref={fileIn} type="file" accept=".wav,audio/wav" hidden onChange={(e) => { const f = e.target.files?.[0]; if (f) void importFile(f); e.target.value = ""; }} />
        <span className="gd2-ed-tools" role="toolbar" aria-label="Pinceaux">
          {TOOLS.map((t) => <button key={t.k} type="button" className={tool === t.k ? "on" : ""} title={t.t} aria-pressed={tool === t.k} onClick={() => setTool(t.k)}>{t.n}</button>)}
        </span>
        <label>Grille
          <select value={gridX} onChange={(e) => setGridX(Number(e.target.value))}>{[0, 4, 8, 16, 32, 64].map((g) => <option key={g} value={g}>{g ? `${g}` : "libre"}</option>)}</select>
        </label>
        <label>Niveaux
          <select value={gridY} onChange={(e) => setGridY(Number(e.target.value))}>{[0, 4, 8, 16].map((g) => <option key={g} value={g}>{g ? `${g}` : "libres"}</option>)}</select>
        </label>
      </div>
      <canvas ref={canvas} className="gd2-ed-canvas" width={1024} height={300} onPointerDown={down} onPointerMove={move} onPointerUp={up} onPointerCancel={up}
        aria-label={`Onde ${cur + 1} sur ${frames.length} : dessiner avec le pinceau choisi`} />
      <div className="gd2-ed-frames" role="listbox" aria-label="Ondes de la table">
        {frames.map((f, i) => (
          <button key={i} type="button" role="option" aria-selected={i === cur} className={i === cur ? "on" : ""} onClick={() => setCur(i)} title={`Onde ${i + 1}`}>
            <svg viewBox="0 0 64 32" aria-hidden><polyline points={Array.from({ length: 33 }, (_, k) => `${k * 2},${16 - f[Math.min(N - 1, k * 64)] * 13}`).join(" ")} /></svg>
            <span>{i + 1}</span>
          </button>
        ))}
        <button type="button" className="gd2-ed-add" disabled={frames.length >= MAX_FRAMES} onClick={() => { push(); setFrames((fs) => [...fs.slice(0, cur + 1), frame.slice(), ...fs.slice(cur + 1)]); setCur(cur + 1); }} title="Dupliquer l'onde choisie">＋</button>
        <button type="button" className="gd2-ed-add" disabled={frames.length <= 1} onClick={() => { push(); setFrames((fs) => fs.filter((_, i) => i !== cur)); setCur(Math.max(0, cur - 1)); }} title="Retirer l'onde choisie">－</button>
      </div>
      <div className="gd2-ed-row">
        <span className="gd2-ed-proc">
          <button type="button" onClick={() => one(normalize)}>Normaliser</button>
          <button type="button" onClick={() => one((f) => f.map((x) => -x) as Float32Array)}>Inverser</button>
          <button type="button" onClick={() => one((f) => f.reverse())}>Retourner</button>
          <button type="button" onClick={() => one(dc)}>Retirer le continu</button>
          <button type="button" onClick={() => one(smoothW)}>Lisser</button>
          <button type="button" onClick={() => one((f) => { for (let i = 0; i < N / 2; i++) f[N - 1 - i] = -f[i]; return f; })}>Symétrique</button>
          <button type="button" onClick={morph} title="Ondes intermédiaires entre la première et la dernière">Fondre</button>
          <button type="button" onClick={() => all((f) => normalize(f))}>Tout normaliser</button>
        </span>
        <span className="gd2-ed-formula">
          <input value={formula} onChange={(e) => setFormula(e.target.value)} aria-label="Formule : x de −1 à 1, t de 0 à 1 d'une onde à l'autre"
            title="x de −1 à 1 sur le cycle, p de 0 à 1, t de 0 à 1 d'une onde à l'autre ; sin cos tan abs sqrt exp log tanh floor round sign min max pow clamp frac tri saw sqr, pi" />
          <button type="button" onClick={() => runFormula(false)}>Sur l'onde</button>
          <button type="button" onClick={() => runFormula(true)}>Sur toutes</button>
        </span>
      </div>
      <div className="gd2-ed-harm" onPointerDown={(e) => { (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId); push(); harmDrag.current = true; setHarm(e); }}
        onPointerMove={(e) => { if (harmDrag.current) setHarm(e); }} onPointerUp={() => { harmDrag.current = false; }} aria-label="Harmoniques de l'onde : glisser pour les régler">
        {Array.from(harm.amp).map((a, i) => <span key={i} style={{ height: `${Math.min(100, a * 100)}%` }} title={`Harmonique ${i + 1} : ${Math.round(a * 100)} %`} />)}
      </div>
      <div className="gd2-ed-bar">
        <label>Puis jouer par
          <select value={target} onChange={(e) => setTarget(e.target.value)}>
            <option value="a">l'oscillateur A</option><option value="b">l'oscillateur B</option><option value="c">l'oscillateur C</option><option value="-">aucun (seulement enregistrer)</option>
          </select>
        </label>
        <button type="button" className="gd2-ed-save" onClick={() => void save()}>Enregistrer dans Perso {slot}</button>
        <span className="gd2-ed-msg" aria-live="polite">{msg || `${frames.length} onde${frames.length > 1 ? "s" : ""} · onde ${cur + 1} · A joue « ${GD2_TABLES[tabNow("a")] ?? ""} »`}</span>
      </div>
    </div>
  );
}
