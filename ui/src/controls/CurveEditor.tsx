// Groovebox — OFAD — AGPL-3.0-or-later
// Éditeur de courbe d'automatisation (pattern ou morceau), principe repris de Sillage.
// v0.44.1 :
//  - trois outils : Crayon (main levée), Ligne (droite d'un appui au relâché) et Gomme ;
//    à la souris, Maj + glisser trace une ligne et le clic droit glissé gomme, quel que soit l'outil ;
//  - grille d'écriture au choix (Libre, 1/4, 1/8, 1/16, 1/32) : chaque case touchée prend UNE valeur
//    tenue jusqu'à la case suivante (marches nettes, comme un verrou par pas) ;
//  - la gomme laisse un « trou » : le réglage n'y est plus piloté et reprend sa valeur normale
//    (point de valeur -1, compris par le moteur) ; tout gommer retire la courbe ;
//  - « Effacer la courbe » la retire d'un coup.
// Corrige : le tracé ne lisait pas le point précédent au bon moment (mise à jour différée de React),
// d'où des traits fins isolés au lieu d'un tracé continu et une gomme qui n'effaçait presque rien.
// La courbe est relue en ligne droite entre ses points, comme dans le moteur.

import { useEffect, useMemo, useRef, useState } from "react";

export interface CurveEditorProps {
  points: number[];                 // [x, valeur, x, valeur…] triés ; valeur -1 : trou
  span: number;                     // longueur affichée (pas ou noires)
  grid: number;                     // trait fin tous les `grid` (un temps)
  bar: number;                      // trait marqué tous les `bar` (une mesure)
  res: number;                      // écart entre deux points dessinés à main levée
  quarter?: number;                 // unités par noire (pattern : pas par temps ; morceau : 1)
  playX?: number;                   // tête de lecture (-1 : aucune)
  format?: (v: number) => string;   // valeur affichée sous le pointeur
  unitLabel?: (x: number) => string;
  onCommit: (points: number[]) => void;
}

type P = { x: number; y: number };
type Tool = "draw" | "line" | "erase";
const GAP = -1;
const isGap = (y: number) => y < 0;
const toPairs = (flat: number[]): P[] => { const o: P[] = []; for (let i = 0; i + 1 < flat.length; i += 2) o.push({ x: flat[i], y: flat[i + 1] }); return o; };
const toFlat = (p: P[]) => p.flatMap((q) => [Math.round(q.x * 10000) / 10000, isGap(q.y) ? GAP : Math.round(q.y * 10000) / 10000]);
const ZOOMS = [0.25, 0.5, 0.75, 1, 1.5, 2, 3, 4, 6, 8];
/** Grilles d'écriture : divisions de la noire (0 : libre). */
const SNAPS: { div: number; label: string }[] = [
  { div: 0, label: "Libre" }, { div: 1, label: "1/4" }, { div: 2, label: "1/8" }, { div: 4, label: "1/16" }, { div: 8, label: "1/32" },
];
let lastSnap = 4;                   // la grille choisie est gardée d'une courbe à l'autre
let lastTool: Tool = "draw";

/** Valeur de la courbe en x (mêmes règles que le moteur) ; -1 dans un trou. */
export function curveValueAt(pts: P[], x: number): number {
  if (!pts.length) return GAP;
  if (x <= pts[0].x) return pts[0].y;
  const last = pts[pts.length - 1];
  if (x >= last.x) return last.y;
  let i = 1;
  while (i < pts.length && pts[i].x <= x) ++i;
  const a = pts[i - 1], b = pts[i];
  if (isGap(a.y)) return GAP;
  if (isGap(b.y)) return a.y;
  const d = b.x - a.x;
  return d <= 1e-6 ? b.y : a.y + (b.y - a.y) * ((x - a.x) / d);
}

const sortPts = (p: P[]) => p.sort((a, b) => a.x - b.x);

/** Remplace la courbe sur [lo, hi] par `add` ; au-delà de hi, la courbe d'avant reprend telle quelle. */
function writeRange(pts: P[], lo: number, hi: number, add: P[], span: number): P[] {
  const after = curveValueAt(pts, hi + 1e-4);
  const keep = pts.filter((q) => q.x < lo - 1e-6 || q.x > hi + 1e-6);
  const out = [...keep, ...add];
  // reprise de la valeur d'origine juste après la zone écrite (sinon tout ce qui suit glisserait)
  if (pts.length && hi < span - 1e-4 && !keep.some((q) => Math.abs(q.x - (hi + 1e-4)) < 1e-6)) out.push({ x: hi + 1e-4, y: after });
  return sortPts(out);
}

/** Allège sans changer la relecture : doublons, points au milieu d'un palier, trous répétés. */
function tidy(pts: P[]): P[] {
  const s = sortPts(pts.map((q) => ({ ...q })));
  const a: P[] = [];
  for (const q of s) {
    const l = a[a.length - 1];
    if (l && Math.abs(q.x - l.x) < 1e-5) { a[a.length - 1] = q; continue; }
    a.push(q);
  }
  const b: P[] = [];
  for (let i = 0; i < a.length; ++i) {
    const p = b[b.length - 1], q = a[i], n = a[i + 1];
    if (p && n && isGap(p.y) && isGap(q.y)) continue;                                   // trou dans un trou
    if (p && n && !isGap(p.y) && !isGap(q.y) && !isGap(n.y) && Math.abs(p.y - q.y) < 1e-4 && Math.abs(q.y - n.y) < 1e-4) continue; // palier
    b.push(q);
  }
  // trous en fin de courbe sans rien après : inutiles s'il ne reste que des trous
  return b.some((q) => !isGap(q.y)) ? b : [];
}

export function CurveEditor({ points, span, grid, bar, res, quarter = 1, playX = -1, format, unitLabel, onCommit }: CurveEditorProps) {
  const [draft, setDraft] = useState<P[] | null>(null);
  const [hover, setHover] = useState<P | null>(null);
  const [tool, setToolState] = useState<Tool>(lastTool);
  const [snapDiv, setSnapState] = useState(lastSnap);
  const [zoom, setZoom] = useState(1);                       // 1 = toute la longueur dans la largeur
  const box = useRef<HTMLDivElement>(null);
  const drag = useRef<{ base: P[]; work: P[]; start: P; last: P; mode: Tool; id: number } | null>(null);
  const committed = useMemo(() => toPairs(points), [points]);
  const shown = draft ?? committed;
  const setTool = (t: Tool) => { lastTool = t; setToolState(t); };
  const setSnap = (d: number) => { lastSnap = d; setSnapState(d); };
  const cell = snapDiv > 0 ? quarter / snapDiv : 0;          // largeur d'une case de la grille d'écriture

  // Ctrl + molette : zoom autour de la largeur affichée
  useEffect(() => {
    const el = box.current;
    if (!el) return;
    const wheel = (e: WheelEvent) => {
      if (!e.ctrlKey) return;
      e.preventDefault();
      setZoom((z) => { const i = ZOOMS.indexOf(z); return ZOOMS[Math.max(0, Math.min(ZOOMS.length - 1, (i < 0 ? 3 : i) + (e.deltaY < 0 ? 1 : -1)))]; });
    };
    el.addEventListener("wheel", wheel, { passive: false });
    return () => el.removeEventListener("wheel", wheel);
  }, []);

  const at = (e: React.PointerEvent<SVGSVGElement>): P => {
    const r = e.currentTarget.getBoundingClientRect();
    const x = Math.max(0, Math.min(span, ((e.clientX - r.left) / Math.max(1, r.width)) * span));
    const y = Math.max(0, Math.min(1, 1 - (e.clientY - r.top) / Math.max(1, r.height)));
    return { x, y };
  };
  const cellOf = (x: number) => Math.min(Math.floor(x / cell), Math.max(0, Math.ceil(span / cell) - 1));
  /** Cases de la grille de a à b, chacune à la valeur de la droite a→b en son milieu. */
  const cellsWrite = (pts: P[], a: P, b: P): P[] => {
    const c0 = cellOf(Math.min(a.x, b.x)), c1 = cellOf(Math.max(a.x, b.x));
    let out = pts;
    for (let c = c0; c <= c1; ++c) {
      const lo = c * cell, hi = Math.min(span, (c + 1) * cell);
      const mid = (lo + hi) / 2;
      const f = Math.abs(b.x - a.x) < 1e-9 ? 1 : Math.max(0, Math.min(1, (mid - a.x) / (b.x - a.x)));
      const y = c0 === c1 ? b.y : a.y + (b.y - a.y) * f;
      const edge = Math.min(0.001, (hi - lo) * 0.01);
      out = writeRange(out, lo, hi - 1e-5, [{ x: lo, y }, { x: hi - edge, y }], span);   // reprise juste après la fin de case
    }
    return out;
  };
  const segment = (a: P, b: P): P[] => {
    const out: P[] = [];
    const n = Math.max(1, Math.ceil(Math.abs(b.x - a.x) / res));
    for (let k = 0; k <= n; ++k) { const f = k / n; out.push({ x: a.x + (b.x - a.x) * f, y: a.y + (b.y - a.y) * f }); }
    return out;
  };
  const freeWrite = (pts: P[], a: P, b: P): P[] => writeRange(pts, Math.min(a.x, b.x), Math.max(a.x, b.x), segment(a, b), span);
  const eraseWrite = (pts: P[], a: P, b: P): P[] => {
    let lo = Math.min(a.x, b.x), hi = Math.max(a.x, b.x);
    if (cell > 0) { lo = cellOf(lo) * cell; hi = Math.min(span, (cellOf(hi) + 1) * cell) - 1e-4; }
    else { lo = Math.max(0, lo - res / 2); hi = Math.min(span, hi + res / 2); }
    return writeRange(pts, lo, hi, [{ x: lo, y: GAP }], span);
  };
  const stroke = (mode: Tool, pts: P[], a: P, b: P) =>
    mode === "erase" ? eraseWrite(pts, a, b) : cell > 0 ? cellsWrite(pts, a, b) : freeWrite(pts, a, b);

  const down = (e: React.PointerEvent<SVGSVGElement>) => {
    if (e.button !== 0 && e.button !== 2) return;
    e.preventDefault();
    e.currentTarget.setPointerCapture(e.pointerId);
    const p = at(e);
    const base = toPairs(points);
    const mode: Tool = e.button === 2 ? "erase" : e.shiftKey ? "line" : tool;
    const work = mode === "line" ? stroke("draw", base, p, p) : stroke(mode, base, p, p);
    drag.current = { base, work, start: p, last: p, mode, id: e.pointerId };
    setDraft(work);
  };
  const move = (e: React.PointerEvent<SVGSVGElement>) => {
    const p = at(e);
    setHover(p);
    const d = drag.current;
    if (!d || d.id !== e.pointerId) return;
    // le point précédent est lu ICI, avant d'être remplacé (et non dans une mise à jour différée)
    const from = d.last;
    d.work = d.mode === "line" ? stroke("draw", d.base, d.start, p) : stroke(d.mode, d.work, from, p);
    d.last = p;
    setDraft(d.work);
  };
  const up = (e: React.PointerEvent<SVGSVGElement>) => {
    const d = drag.current;
    if (!d || d.id !== e.pointerId) return;
    drag.current = null;
    setDraft(null);
    onCommit(toFlat(tidy(d.work).slice(0, 4096)));
  };

  const width = `${zoom * 100}%`;
  const lines = useMemo(() => {
    const out: { x: number; strong: boolean; fine: boolean }[] = [];
    const step = cell > 0 && cell < grid ? cell : grid;
    if (step > 0 && span / step <= 1024) for (let k = 0, x = 0; x <= span + 1e-6; ++k, x = k * step) {
      const strong = bar > 0 && Math.abs(x / bar - Math.round(x / bar)) < 1e-6;
      const beat = grid > 0 && Math.abs(x / grid - Math.round(x / grid)) < 1e-6;
      out.push({ x, strong, fine: !beat });
    }
    return out;
  }, [span, grid, bar, cell]);

  // morceaux tracés (les trous ne sont pas dessinés)
  const runs = useMemo(() => {
    const out: string[] = [];
    if (!shown.length) return out;
    let cur = "";
    const close = () => { if (cur) out.push(cur); cur = ""; };
    const lineTo = (x: number, y: number) => { cur += cur ? ` L ${x} ${1 - y}` : `M ${x} ${1 - y}`; };
    if (!isGap(shown[0].y)) lineTo(0, shown[0].y);
    for (let i = 0; i < shown.length; ++i) {
      const q = shown[i];
      if (isGap(q.y)) {
        const prev = shown[i - 1];
        if (prev && !isGap(prev.y)) lineTo(q.x, prev.y);
        close();
      } else lineTo(q.x, q.y);
    }
    const last = shown[shown.length - 1];
    if (!isGap(last.y)) lineTo(span, last.y);
    close();
    return out;
  }, [shown, span]);
  const gaps = useMemo(() => {
    const out: { a: number; b: number }[] = [];
    if (!shown.length) return [{ a: 0, b: span }];
    for (let i = 0; i < shown.length; ++i) if (isGap(shown[i].y)) out.push({ a: shown[i].x, b: shown[i + 1]?.x ?? span });
    if (isGap(shown[0].y) && shown[0].x > 0) out.push({ a: 0, b: shown[0].x });
    return out;
  }, [shown, span]);

  const hoverValue = hover ? curveValueAt(shown, hover.x) : GAP;
  const toolBtn = (t: Tool, label: string, title: string) => (
    <button type="button" className={`touch touch--small${tool === t ? " touch--on" : ""}`} aria-pressed={tool === t} onClick={() => setTool(t)} title={title}>{label}</button>
  );
  return (
    <div className="curve">
      <div className="curve-tools">
        <div className="curve-group" role="group" aria-label="Outil">
          {toolBtn("draw", "Crayon", "Glisser : dessiner à main levée (à la souris, Maj + glisser trace une ligne droite)")}
          {toolBtn("line", "Ligne", "Appuyer au départ, glisser, relâcher à l'arrivée : ligne droite")}
          {toolBtn("erase", "Gomme", "Glisser : efface (le réglage reprend sa valeur normale). À la souris : clic droit glissé")}
        </div>
        <div className="curve-group" role="group" aria-label="Grille d'écriture">
          <span className="curve-hint">Grille</span>
          {SNAPS.map((s) => (
            <button key={s.div} type="button" className={`touch touch--small${snapDiv === s.div ? " touch--on" : ""}`} aria-pressed={snapDiv === s.div}
              onClick={() => setSnap(s.div)} title={s.div ? `Une valeur tenue par ${s.label} de note` : "Tracé libre, sans grille"}>{s.label}</button>
          ))}
        </div>
        <button type="button" className="touch touch--small touch--quiet" disabled={!committed.length} onClick={() => onCommit([])}
          title="Retire toute la courbe : le réglage reprend sa valeur normale">Effacer la courbe</button>
        <span className="screen-spacer" />
        <span className="curve-read tabular">{hover ? `${unitLabel ? unitLabel(hover.x) : hover.x.toFixed(2)} · ${isGap(hoverValue) ? "libre" : format ? format(hoverValue) : `${Math.round(hoverValue * 100)} %`}` : ""}</span>
        <button type="button" className="touch touch--small" onClick={() => setZoom(ZOOMS[Math.max(0, ZOOMS.indexOf(zoom) - 1)] ?? 1)} aria-label="Dézoomer">−</button>
        <button type="button" className="touch touch--small" onClick={() => setZoom(1)} title="Toute la longueur dans la largeur">Tout voir</button>
        <button type="button" className="touch touch--small" onClick={() => setZoom(ZOOMS[Math.min(ZOOMS.length - 1, ZOOMS.indexOf(zoom) + 1)] ?? 1)} aria-label="Zoomer">+</button>
      </div>
      <div className="curve-scroll" ref={box}>
        <svg className={`curve-svg curve-svg--${tool}`} style={{ width }} viewBox={`0 0 ${span} 1`} preserveAspectRatio="none"
          onPointerDown={down} onPointerMove={move} onPointerUp={up} onPointerCancel={up} onPointerLeave={() => setHover(null)}
          onContextMenu={(e) => e.preventDefault()} role="img" aria-label="Courbe d'automatisation">
          {gaps.map((g, i) => <rect key={`g${i}`} x={g.a} y={0} width={Math.max(0, g.b - g.a)} height={1} className="curve-gap" />)}
          {lines.map((l) => <line key={l.x} x1={l.x} x2={l.x} y1={0} y2={1} className={l.strong ? "curve-bar" : l.fine ? "curve-cell" : "curve-beat"} vectorEffect="non-scaling-stroke" />)}
          <line x1={0} x2={span} y1={0.5} y2={0.5} className="curve-mid" vectorEffect="non-scaling-stroke" />
          {runs.map((d, i) => {
            const m = /^M ([\d.e-]+) /.exec(d);
            const lastX = d.trim().split(" ").slice(-2, -1)[0];
            return (
              <g key={i}>
                {m && <path d={`${d} L ${lastX} 1 L ${m[1]} 1 Z`} className="curve-fill" />}
                <path d={d} className="curve-line" vectorEffect="non-scaling-stroke" />
              </g>
            );
          })}
          {playX >= 0 && <line x1={playX} x2={playX} y1={0} y2={1} className="curve-play" vectorEffect="non-scaling-stroke" />}
        </svg>
      </div>
      {!committed.length && !draft && <p className="curve-hint">Courbe vide : dessinez dessus, ou retirez-la avec × dans la liste.</p>}
    </div>
  );
}
