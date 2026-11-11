// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : courbes de l'écran SON qu'on règle au doigt ou à la souris, en tirant leurs points.
// Enveloppes (retard, attaque, tenue, déclin, maintien, déclin 2, relâchement ; vitesses et niveaux
// des synthés FM), filtres (coupure en largeur, résonance en hauteur ; un ou deux filtres) et
// oscillateurs (largeur d'impulsion). Chaque point écrit le réglage qu'il représente, là où écrivent
// les encodeurs (son de base, verrou de pas, scène) ; la courbe suit aussi les encodeurs.
// Modulation (LFO…) : le point reste sur le réglage ; un point fantôme et une courbe en pointillés
// montrent le son joué, et un trait montre toute l'amplitude de la modulation.

import { useEffect, useRef, useState } from "react";
import { useStore } from "../engine/store";
import type { ModuleDesc, ParamDesc } from "../engine/protocol";
import { formatPlain, toNorm, toPlain } from "../engine/params";
import {
  type EnvGroup, type EnvRole, type FilterGroup, type OscGroup, type RateLevelEnv,
  levelOf, plainForLevel, plainForMs, shapeOfChoice, shapeOfWaveName, timeMs, norm,
} from "./curveRoles";

const W = 330, H = 150, TOP = 12, BOT = 8;
const yOf = (lv: number) => H - BOT - lv * (H - TOP - BOT);
const lvOf = (y: number) => (H - BOT - y) / (H - TOP - BOT);
const tw = (ms: number) => Math.log10(1 + Math.max(0, ms) / 5);
const msOfTw = (t: number) => 5 * (Math.pow(10, Math.max(0, t)) - 1);
const PLATEAU = 1.2;

export type SetPlain = (k: number, plain: number) => void;
/** Modulations des réglages du module : index du réglage → [bas, haut] en valeurs normalisées. */
export type ModRanges = Map<number, [number, number]>;

/** Ce que les éditeurs reçoivent : valeurs écrites (points), valeurs jouées (fantômes), modulations. */
export interface CurveProps {
  m: ModuleDesc;
  values: number[];        // réglage écrit (son de base, verrou, scène)
  played?: number[];       // son joué, modulation comprise (courbes animées seulement)
  mods?: ModRanges;
  onSet: SetPlain;
  readOnly?: boolean;
}

interface Handle {
  id: string;
  x: number; y: number;          // coordonnées de la courbe (viewBox)
  title: string;
  params: number[];              // réglages représentés (pour l'amplitude de leur modulation)
  /** x de départ du segment (le temps se mesure depuis là) */
  x0?: number;
  xSet?: (svgX: number, k: number) => void; // k : échelle px par unité de temps (gelée pendant le glissé)
  ySet?: (svgY: number) => void;
  /** touche Suppr : valeurs d'origine */
  reset?: () => void;
  color?: string;
}
interface Ghost { id: string; x: number; y: number; color?: string }
interface Range { id: string; x1: number; y1: number; x2: number; y2: number; color?: string }

/** Fantômes (son joué) et traits d'amplitude des modulations, d'après une fonction qui place les
    points pour des valeurs données. */
function ghostsAndRanges(build: (vals: number[]) => Handle[], base: Handle[], p: CurveProps): { ghosts: Ghost[]; ranges: Range[] } {
  const ghosts: Ghost[] = [], ranges: Range[] = [];
  if (p.played) {
    const pos = build(p.played);
    for (const h of base) {
      const g = pos.find((x) => x.id === h.id);
      if (g && Math.hypot(g.x - h.x, g.y - h.y) > 1.5) ghosts.push({ id: h.id, x: g.x, y: g.y, color: h.color });
    }
  }
  if (p.mods && p.mods.size) {
    for (const h of base) {
      for (const k of h.params) {
        const r = p.mods.get(k);
        if (!r) continue;
        const at = (n: number) => { const v = p.values.slice(); v[k] = n; return build(v).find((x) => x.id === h.id); };
        const a = at(r[0]), b = at(r[1]);
        if (a && b && Math.hypot(a.x - b.x, a.y - b.y) > 1) ranges.push({ id: `${h.id}:${k}`, x1: a.x, y1: a.y, x2: b.x, y2: b.y, color: h.color });
      }
    }
  }
  return { ghosts, ranges };
}

/** Zone de courbe avec des points à tirer (positionnés en pourcentage : la courbe s'étire). Pendant le
    geste, la valeur s'affiche au-dessus du point ; au clavier : flèches (Maj : fin), Suppr : valeur
    d'origine. */
function CurveBox({ children, handles: raw, scale, readOnly, label, ghosts = [], ranges = [] }: { children: React.ReactNode; handles: Handle[]; scale: number; readOnly?: boolean; label?: string; ghosts?: Ghost[]; ranges?: Range[] }) {
  // deux points confondus (tenue à 0 au sommet de l'attaque…) : le second se décale pour rester prenable
  const handles = raw.map((h) => ({ ...h }));
  for (let i = 1; i < handles.length; ++i) {
    const h = handles[i];
    for (let j = 0; j < i; ++j) {
      const a = handles[j];
      if (Math.abs(h.x - a.x) < 9 && Math.abs(h.y - a.y) < 9) h.x = Math.min(W, a.x + 9);
    }
  }
  const box = useRef<HTMLDivElement>(null);
  const drag = useRef<{ id: string; k: number; pid: number } | null>(null);
  const [active, setActive] = useState<string | null>(null);
  const toSvg = (e: React.PointerEvent) => {
    const r = box.current!.getBoundingClientRect();
    return { x: ((e.clientX - r.left) / Math.max(1, r.width)) * W, y: ((e.clientY - r.top) / Math.max(1, r.height)) * H };
  };
  const move = (e: React.PointerEvent) => {
    const d = drag.current;
    if (!d || d.pid !== e.pointerId) return;
    const h = raw.find((x) => x.id === d.id);
    if (!h) return;
    const p = toSvg(e);
    h.xSet?.(Math.min(W, Math.max(0, p.x)), d.k);
    h.ySet?.(Math.min(H, Math.max(0, p.y)));
  };
  const key = (h: Handle, e: React.KeyboardEvent) => {
    const step = e.shiftKey ? 1 : 5;
    const src = raw.find((x) => x.id === h.id) ?? h;
    if ((e.key === "ArrowLeft" || e.key === "ArrowRight") && src.xSet) { e.preventDefault(); src.xSet(Math.min(W, Math.max(0, src.x + (e.key === "ArrowRight" ? step : -step))), scale); }
    else if ((e.key === "ArrowUp" || e.key === "ArrowDown") && src.ySet) { e.preventDefault(); src.ySet(Math.min(H, Math.max(0, src.y + (e.key === "ArrowUp" ? -step : step)))); }
    else if ((e.key === "Delete" || e.key === "Backspace") && src.reset) { e.preventDefault(); src.reset(); }
  };
  const shown = handles.find((h) => h.id === active);
  const pct = (x: number, y: number) => ({ left: `${(x / W) * 100}%`, top: `${(y / H) * 100}%` });
  return (
    <div ref={box} className={`curve-box${readOnly ? " curve-box--ro" : ""}`} aria-label={label}
      onPointerMove={move}
      onPointerUp={(e) => { if (drag.current?.pid === e.pointerId) { drag.current = null; setActive(null); } }}
      onPointerCancel={() => { drag.current = null; setActive(null); }}>
      {children}
      {ranges.length > 0 && (
        <svg viewBox={`0 0 ${W} ${H}`} className="curve-ranges" preserveAspectRatio="none" aria-hidden>
          {ranges.map((r) => <line key={r.id} x1={r.x1} y1={r.y1} x2={r.x2} y2={r.y2} style={r.color ? { ["--k" as string]: r.color } : undefined} />)}
        </svg>
      )}
      {ghosts.map((g) => <span key={g.id} className="curve-ghost" style={{ ...pct(g.x, g.y), ...(g.color ? { ["--k" as string]: g.color } : {}) }} aria-hidden />)}
      {!readOnly && handles.map((h) => (
        <button key={h.id} type="button" className={`curve-handle${active === h.id ? " is-active" : ""}${h.xSet && !h.ySet ? " curve-handle--x" : ""}${!h.xSet && h.ySet ? " curve-handle--y" : ""}`}
          style={{ ...pct(h.x, h.y), ...(h.color ? { ["--k" as string]: h.color } : {}) }}
          title={h.title} aria-label={h.title}
          onKeyDown={(e) => key(h, e)}
          onPointerDown={(e) => {
            if (e.button !== 0) return;
            e.preventDefault(); e.stopPropagation();
            box.current?.setPointerCapture(e.pointerId);
            drag.current = { id: h.id, k: scale, pid: e.pointerId };
            setActive(h.id);
          }} />
      ))}
      {shown && (
        <span className={`curve-bubble${shown.x > W * 0.7 ? " curve-bubble--left" : ""}`} style={pct(shown.x, shown.y)} aria-live="polite">{shown.title}</span>
      )}
    </div>
  );
}

// ---------------------------------------------------------------------------------------------
// Enveloppes

interface Seg { role: EnvRole | "plateau" | "start"; ms: number; to: number; curve: "lin" | "exp" | "att" }
const ENV_COLORS: (string | undefined)[] = [undefined, "var(--led-blue)", "var(--led-green)"]; // undefined : couleur de la piste
const ROLE_LABEL: Record<string, string> = { delay: "Retard", attack: "Attaque", hold: "Tenue", decay: "Déclin", decay2: "Déclin 2", release: "Relâchement", sustain: "Maintien", sustain2: "Niveau 2", end: "Fin" };

interface EnvShape {
  segs: Seg[];
  /** segments rejoués tant que la note est tenue (indices inclus) */
  loop?: [number, number];
  label?: string;
  /** « un coup » : l'enveloppe va jusqu'au bout, relâchement compris, quelle que soit la note */
  oneShot?: boolean;
}

/** Étapes d'une enveloppe d'après ses réglages, son mode (ADSR, ADS1DS2R, un coup, boucles) et sa boucle. */
function envShape(m: ModuleDesc, values: number[], g: EnvGroup): EnvShape {
  const r = { ...g.roles };
  const pl = (k: number) => toPlain(m.params[k], values[k] ?? 0);
  const modeName = g.mode !== undefined ? norm(m.params[g.mode].choices?.[pl(g.mode)] ?? "") : "";
  const loopOn = g.loop !== undefined && pl(g.loop) > 0;
  // ADSR : le second déclin ne sert pas (comme dans le moteur) ; « un coup » : A, D, D2 puis relâchement, sans maintien
  const unusedD2 = modeName === "adsr" && r.decay2 !== undefined;
  if (modeName === "adsr") { delete r.decay2; delete r.sustain2; }
  const oneShot = modeName === "un coup";
  const t = (role: EnvRole) => { const k = r[role]; return k === undefined ? 0 : timeMs(m.params[k], pl(k)); };
  const lv = (role: EnvRole) => { const k = r[role]; return k === undefined ? 0 : levelOf(m.params[k], pl(k)); };
  const segs: Seg[] = [];
  if (r.delay !== undefined) segs.push({ role: "delay", ms: t("delay"), to: 0, curve: "lin" });
  if (r.attack !== undefined) segs.push({ role: "attack", ms: t("attack"), to: 1, curve: "att" });
  else segs.push({ role: "start", ms: 0, to: 1, curve: "lin" }); // sans attaque : départ au sommet
  if (r.hold !== undefined) segs.push({ role: "hold", ms: t("hold"), to: 1, curve: "lin" });
  let level = 1;
  if (r.decay !== undefined) {
    level = r.sustain !== undefined ? lv("sustain") : r.end !== undefined ? lv("end") : 0;
    segs.push({ role: "decay", ms: t("decay"), to: level, curve: "exp" });
  }
  if (r.decay2 !== undefined) { level = r.sustain2 !== undefined ? lv("sustain2") : level; segs.push({ role: "decay2", ms: t("decay2"), to: level, curve: "exp" }); }
  let loop: [number, number] | undefined;
  const iOf = (role: Seg["role"]) => segs.findIndex((x) => x.role === role);
  if (modeName === "boucle s1-s2" && iOf("decay2") >= 0) loop = [iOf("decay2"), iOf("decay2")];
  else if (modeName === "boucle tout" || loopOn) {
    const from = segs.findIndex((x) => x.role === "attack" || x.role === "start");
    loop = [Math.max(0, from), segs.length - 1];
  }
  const plateau = !oneShot && !loop && (r.sustain !== undefined || (r.decay === undefined && r.release !== undefined));
  if (plateau) segs.push({ role: "plateau", ms: 0, to: level, curve: "lin" });
  if (r.release !== undefined) segs.push({ role: "release", ms: t("release"), to: 0, curve: "exp" });
  const label = loop ? "⟲ boucle tant que la note est tenue" : oneShot ? "un coup : sans maintien, va jusqu'au bout"
    : unusedD2 ? "mode ADSR : déclin 2 et niveau 2 ne servent pas (mode ADS1DS2R pour les utiliser)" : undefined;
  return { segs, loop, label, oneShot };
}
const segW = (s: Seg) => (s.role === "plateau" ? PLATEAU : s.role === "start" ? 0 : tw(s.ms));

const shapeAt = (curve: Seg["curve"], u: number) => curve === "lin" ? u
  : curve === "att" ? (1 - Math.exp(-2.2 * u)) / (1 - Math.exp(-2.2)) : (1 - Math.exp(-4.5 * u)) / (1 - Math.exp(-4.5));

/** 1.1 : horloge de la note jouée sur la piste choisie (déclenchement, relâchement), pour faire
    courir un point sur l'enveloppe comme sur les sources de modulation. */
function useNoteClock(live: boolean) {
  const kh = useStore((s) => (live ? s.tick.kh : undefined));
  const held = useStore((s) => (live ? (s.tick.kn?.length ?? 0) > 0 : false));
  const st = useRef({ on: -1, off: -1, held: false });
  const [, setFrame] = useState(0);
  const running = useRef(false);
  const kick = () => {
    if (running.current) return;
    running.current = true;
    const loop = () => { if (!running.current) return; setFrame((n) => (n + 1) % 1e6); window.setTimeout(loop, 33); };
    loop();
  };
  useEffect(() => { if (kh && kh.length) { st.current.on = performance.now(); st.current.off = -1; kick(); } }, [kh]);
  useEffect(() => {
    const c = st.current;
    if (held && !c.held && c.on < 0) { c.on = performance.now(); c.off = -1; kick(); }
    if (!held && c.held && c.on >= 0 && c.off < 0) c.off = performance.now();
    c.held = held;
  }, [held]);
  useEffect(() => () => { running.current = false; }, []);
  const stop = () => { running.current = false; st.current.on = -1; };
  return { clock: st.current, stop };
}

function segPath(x0: number, l0: number, x1: number, l1: number, curve: Seg["curve"]): string {
  if (curve === "lin" || x1 - x0 < 0.5) return ` L ${x1.toFixed(1)} ${yOf(l1).toFixed(1)}`;
  let d = "";
  for (let i = 1; i <= 16; ++i) {
    const u = i / 16;
    const f = curve === "att" ? (1 - Math.exp(-2.2 * u)) / (1 - Math.exp(-2.2)) : (1 - Math.exp(-4.5 * u)) / (1 - Math.exp(-4.5));
    d += ` L ${(x0 + (x1 - x0) * u).toFixed(1)} ${yOf(l0 + (l1 - l0) * f).toFixed(1)}`;
  }
  return d;
}

/** Enveloppes d'une section (une ou plusieurs superposées), aux points réglables. */
export function EnvEditor(props: CurveProps & { groups: EnvGroup[]; live?: boolean }) {
  const { m, values, played, groups, onSet, readOnly, live } = props;
  const { clock, stop } = useNoteClock(!!live);
  const shapesOf = (vals: number[]) => groups.map((g) => envShape(m, vals, g));
  const shapes = shapesOf(values);
  const playedShapes = played ? shapesOf(played) : null;
  const totalOf = (sh: EnvShape[]) => Math.max(0.5, ...sh.map((x) => x.segs.reduce((a, s) => a + segW(s), 0)));
  const k = (W - 6) / Math.max(totalOf(shapes), playedShapes ? totalOf(playedShapes) : 0); // px par unité de temps
  const resetOf = (...ks: (number | undefined)[]) => () => { for (const kk of ks) if (kk !== undefined) onSet(kk, m.params[kk].def); };
  const val = (vals: number[], kk: number | undefined) => (kk !== undefined ? formatPlain(m.params[kk], toPlain(m.params[kk], vals[kk] ?? 0)) : "");

  /** Points de toutes les enveloppes pour des valeurs données. */
  const build = (vals: number[]): Handle[] => {
    const hs: Handle[] = [];
    shapesOf(vals).forEach((sh, gi) => {
      const g = groups[gi], segs = sh.segs;
      const color = groups.length > 1 ? ENV_COLORS[gi % ENV_COLORS.length] : undefined;
      let x = 0;
      for (const s of segs) {
        const x1 = x + segW(s) * k, x0 = x;
        const role = s.role;
        if (role !== "start" && role !== "plateau") {
          const kx = g.roles[role as EnvRole];
          const yRole: EnvRole | undefined = role === "decay" ? (g.roles.sustain !== undefined ? "sustain" : g.roles.end !== undefined ? "end" : undefined)
            : role === "decay2" ? (g.roles.sustain2 !== undefined ? "sustain2" : undefined) : undefined;
          const ky = yRole ? g.roles[yRole] : undefined;
          const pk = kx !== undefined ? m.params[kx] : null, py = ky !== undefined ? m.params[ky] : null;
          hs.push({
            id: `${gi}:${role}`, x: x1, y: yOf(s.to), x0, color, params: [kx, ky].filter((q): q is number => q !== undefined),
            title: `${g.label ? g.label + " · " : ""}${ROLE_LABEL[role]} ${val(vals, kx)}${py ? ` · ${ROLE_LABEL[yRole!]} ${val(vals, ky)}` : ""}`,
            xSet: pk && kx !== undefined ? (sx, kk) => onSet(kx, plainForMs(pk, msOfTw((sx - x0) / kk))) : undefined,
            ySet: py && ky !== undefined ? (sy) => onSet(ky, plainForLevel(py, lvOf(sy))) : undefined,
            reset: resetOf(kx, ky),
          });
        } else if (role === "plateau" && g.roles.sustain !== undefined && segs.every((q) => q.role !== "decay")) {
          // maintien sans déclin : la hauteur du plateau
          const ky = g.roles.sustain, py = m.params[ky];
          hs.push({ id: `${gi}:sus`, x: (x + x1) / 2, y: yOf(s.to), color, params: [ky], title: `Maintien ${val(vals, ky)}`, ySet: (sy) => onSet(ky, plainForLevel(py, lvOf(sy))), reset: resetOf(ky) });
        }
        x = x1;
      }
    });
    return hs;
  };
  const handles = build(values);
  const { ghosts, ranges } = ghostsAndRanges(build, handles, props);

  const pathOf = (sh: EnvShape) => {
    let x = 0, l = 0, d = `M 0 ${yOf(0)}`;
    const marks: number[] = [], xs: number[] = [];
    for (const s of sh.segs) { const x1 = x + segW(s) * k; xs.push(x); d += segPath(x, l, x1, s.to, s.curve); marks.push(x1); x = x1; l = s.to; }
    if (x < W) d += ` L ${W} ${yOf(l)}`;
    const loopX = sh.loop ? [xs[sh.loop[0]], xs[sh.loop[1]] + segW(sh.segs[sh.loop[1]]) * k] as const : null;
    return { d, marks, loopX };
  };
  const paths = shapes.map((sh, gi) => ({ ...pathOf(sh), color: groups.length > 1 ? ENV_COLORS[gi % ENV_COLORS.length] : undefined }));
  const ghostPaths = playedShapes && ghosts.length ? playedShapes.map((sh) => pathOf(sh).d) : null;

  // point qui court : la première enveloppe, au temps de la note jouée (son joué)
  let head: { x: number; lv: number } | null = null;
  const runShape = (playedShapes ?? shapes)[0];
  if (live && clock.on >= 0 && runShape) {
    const { segs, loop, oneShot } = runShape;
    const now = performance.now();
    const xs: number[] = []; let acc = 0;
    for (const sg of segs) { xs.push(acc); acc += segW(sg) * k; }
    const pi = segs.findIndex((sg) => sg.role === "plateau");
    const ri = segs.findIndex((sg) => sg.role === "release");
    const holdEnd = pi >= 0 ? pi : ri >= 0 ? ri : segs.length; // étapes jouées note tenue
    const fold = (t: number) => {
      if (!loop) return t;
      let t0 = 0; for (let i = 0; i < loop[0]; ++i) t0 += segs[i].ms;
      let len = 0; for (let i = loop[0]; i <= loop[1]; ++i) len += segs[i].ms;
      return t <= t0 || len <= 0 ? t : t0 + ((t - t0) % len);
    };
    const walk = (t: number, upto: number): { x: number; lv: number; done: boolean } => {
      let l = 0;
      for (let i = 0; i < upto; ++i) {
        const sg = segs[i], x0 = xs[i], x1 = x0 + segW(sg) * k;
        if (t <= sg.ms && sg.ms > 0) { const u = t / sg.ms; return { x: x0 + (x1 - x0) * u, lv: l + (sg.to - l) * shapeAt(sg.curve, u), done: false }; }
        t -= sg.ms; l = sg.to;
      }
      return { x: upto < segs.length ? xs[upto] : acc, lv: l, done: true };
    };
    const releaseFrom = (lv0: number) => {
      if (ri < 0) return null;
      const rs = segs[ri], u = rs.ms > 0 ? (now - clock.off) / rs.ms : 1;
      return u >= 1 ? null : { x: xs[ri] + segW(rs) * k * u, lv: lv0 * (1 - shapeAt("exp", u)) };
    };
    if (oneShot) {
      const w = walk(now - clock.on, segs.length);
      head = w.done ? null : { x: w.x, lv: w.lv };
      if (!head) stop();
    } else if (clock.off < 0 || loop) {
      const held = clock.off < 0;
      const t = (held ? now : clock.off) - clock.on;
      const w = walk(loop ? fold(t) : t, holdEnd);
      if (held) head = w.done ? (pi >= 0 ? { x: xs[pi] + PLATEAU * k * 0.5, lv: segs[pi].to } : ri >= 0 ? { x: xs[ri], lv: w.lv } : null) : { x: w.x, lv: w.lv };
      else head = releaseFrom(w.lv);
      if (!head && pi < 0 && ri < 0) stop();
    } else if (pi < 0) {
      const w = walk(now - clock.on, holdEnd);
      if (!w.done) head = { x: w.x, lv: w.lv };
      else head = releaseFrom(walk(clock.off - clock.on, holdEnd).lv);
    } else {
      head = releaseFrom(walk(clock.off - clock.on, pi).lv);
    }
    if (!head && clock.off >= 0) stop();
  }
  const note = shapes.find((sh) => sh.label)?.label;
  return (
    <CurveBox handles={handles} scale={k} readOnly={readOnly} label="Enveloppe : tirez les points" ghosts={ghosts} ranges={ranges}>
      {head && <span className="curve-head" style={{ left: `${(head.x / W) * 100}%`, top: `${(yOf(head.lv) / H) * 100}%` }} aria-hidden />}
      <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve" preserveAspectRatio="none" aria-hidden>
        {head && <line x1={head.x} x2={head.x} y1={0} y2={H} className="curve-head-line" />}
        {paths.length === 1 && paths[0].marks.slice(0, -1).map((x, i) => <line key={i} x1={x} x2={x} y1={0} y2={H} className="grid" />)}
        {paths.map((p, i) => (
          <g key={i} style={p.color ? { ["--k" as string]: p.color } : undefined}>
            {i === 0 && <path d={`${p.d} L ${W} ${H} L 0 ${H} Z`} className="fill" />}
            <path d={p.d} className="stroke" />
            {ghostPaths && <path d={ghostPaths[i]} className="stroke stroke--ghost" />}
            {p.loopX && <path d={`M ${p.loopX[1]} 6 L ${p.loopX[0]} 6 M ${p.loopX[0] + 6} 2 L ${p.loopX[0]} 6 L ${p.loopX[0] + 6} 10`} className="curve-loop" />}
          </g>
        ))}
      </svg>
      {(groups.length > 1 || note) && (
        <div className="curve-legend">
          {groups.length > 1 && groups.map((g, i) => <span key={g.key} style={ENV_COLORS[i % ENV_COLORS.length] ? { ["--k" as string]: ENV_COLORS[i % ENV_COLORS.length] } : undefined}>{g.label}</span>)}
          {note && <em>{note}</em>}
        </div>
      )}
    </CurveBox>
  );
}

/** Durée d'une étape d'enveloppe à vitesse (0 à 99) : 99 ≈ 2 ms, 0 ≈ 7 s. */
const rateMs = (r: number) => 2 * Math.pow(10, (99 - r) / 28);
const rateOfMs = (ms: number) => Math.round(Math.min(99, Math.max(0, 99 - 28 * Math.log10(Math.max(2, ms) / 2))));

/** Enveloppe à 4 vitesses et 4 niveaux (synthés FM à 6 opérateurs) : le point n règle la vitesse n
    (en largeur) et le niveau n (en hauteur). Le dernier point est le relâchement. */
export function RateLevelEditor(props: CurveProps & { env: RateLevelEnv }) {
  const { m, values, played, env, onSet, readOnly } = props;
  const layout = (vals: number[]) => {
    const pl = (k: number) => toPlain(m.params[k], vals[k] ?? 0);
    const L = env.levels.map((k) => pl(k) / 99), R = env.rates.map((k) => pl(k));
    const ws = [tw(rateMs(R[0])), tw(rateMs(R[1])), tw(rateMs(R[2])), PLATEAU, tw(rateMs(R[3]))];
    return { L, R, ws, pl, total: ws.reduce((a, b) => a + b, 0) };
  };
  const base = layout(values), pla = played ? layout(played) : null;
  const k = (W - 6) / Math.max(base.total, pla?.total ?? 0);
  const pathOf = (lay: ReturnType<typeof layout>) => {
    const lv = [lay.L[3], lay.L[0], lay.L[1], lay.L[2], lay.L[2], lay.L[3]];
    let x = 0, d = `M 0 ${yOf(lv[0])}`;
    const xs: number[] = [0];
    for (let i = 0; i < 5; ++i) { const x1 = x + lay.ws[i] * k; d += segPath(x, lv[i], x1, lv[i + 1], i === 3 ? "lin" : "exp"); xs.push(x1); x = x1; }
    return { d: `${d} L ${W} ${yOf(lv[5])}`, xs };
  };
  const build = (vals: number[]): Handle[] => {
    const lay = layout(vals), { xs } = pathOf(lay);
    return [0, 1, 2, 3].map((i) => {
      const seg = i < 3 ? i : 4, x0 = xs[seg], x1 = xs[seg + 1];
      const kr = env.rates[i], kl = env.levels[i];
      return {
        id: `rl${i}`, x: x1, y: yOf(lay.L[i]), x0, params: [kr, kl],
        title: `Vitesse ${i + 1} : ${lay.R[i]} · niveau ${i + 1} : ${Math.round(lay.pl(kl))}`,
        xSet: (sx: number, kk: number) => onSet(kr, rateOfMs(msOfTw((sx - x0) / kk))),
        ySet: (sy: number) => onSet(kl, Math.round(Math.min(1, Math.max(0, lvOf(sy))) * 99)),
        reset: () => { onSet(kr, m.params[kr].def); onSet(kl, m.params[kl].def); },
      };
    });
  };
  const handles = build(values);
  const { ghosts, ranges } = ghostsAndRanges(build, handles, props);
  const { d, xs } = pathOf(base);
  return (
    <CurveBox handles={handles} scale={k} readOnly={readOnly} label="Enveloppe : tirez les points" ghosts={ghosts} ranges={ranges}>
      <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve" preserveAspectRatio="none" aria-hidden>
        {env.bipolar && <line x1={0} x2={W} y1={yOf(50 / 99)} y2={yOf(50 / 99)} className="grid" />}
        {xs.slice(1, -1).map((xx, i) => <line key={i} x1={xx} x2={xx} y1={0} y2={H} className="grid" />)}
        <path d={`${d} L ${W} ${H} L 0 ${H} Z`} className="fill" />
        <path d={d} className="stroke" />
        {pla && ghosts.length > 0 && <path d={pathOf(pla).d} className="stroke stroke--ghost" />}
      </svg>
    </CurveBox>
  );
}

// ---------------------------------------------------------------------------------------------
// Filtres

const F_MIN = 20, F_DEC = 3; // 20 Hz → 20 kHz
const xOfHz = (hz: number) => (Math.log10(Math.max(F_MIN, hz) / F_MIN) / F_DEC) * W;
const hzOfX = (x: number) => F_MIN * Math.pow(10, (x / W) * F_DEC);
const DB_LO = -36, DB_HI = 18;
const yOfDb = (db: number) => H - ((Math.max(DB_LO, Math.min(DB_HI, db)) - DB_LO) / (DB_HI - DB_LO)) * H;
const dbOfY = (y: number) => DB_LO + ((H - y) / H) * (DB_HI - DB_LO);
const PEAK_DB = 14;

function magOf(shape: "lp" | "bp" | "hp" | "notch", r: number, poles: number): number {
  const band = 1 / Math.sqrt(1 + Math.pow((r - 1 / r) * 1.5, 2));
  return shape === "lp" ? 1 / Math.sqrt(1 + Math.pow(r, 2 * poles)) : shape === "hp" ? 1 / Math.sqrt(1 + Math.pow(1 / r, 2 * poles))
    : shape === "notch" ? Math.max(0.001, 1 - band) : band;
}

interface FilterState { hz: number; reso: number; shape: "lp" | "bp" | "hp" | "notch" | "off"; poles: number; morph?: { v: number; notch: boolean } ; name: string }

function filterState(m: ModuleDesc, values: number[], g: FilterGroup, acid: boolean): FilterState {
  const pl = (k: number) => toPlain(m.params[k], values[k] ?? 0);
  const hz = pl(g.cutoff);
  const rp = g.reso !== undefined ? m.params[g.reso] : null;
  const reso = rp && g.reso !== undefined ? levelOf(rp, pl(g.reso)) : 0;
  let shape: FilterState["shape"] = g.shapeFromName ?? "lp", poles = acid ? 4 : 2, name = "";
  if (g.type !== undefined) {
    name = m.params[g.type].choices?.[pl(g.type)] ?? "";
    const s = shapeOfChoice(name); shape = s.shape; poles = s.poles;
  } else if (g.shapeFromName) poles = 2;
  else poles = 4;
  if (g.bandpass !== undefined && pl(g.bandpass) > 0) shape = "bp";
  const morph = g.morph !== undefined && shape !== "off" && shape !== "bp" ? { v: levelOf(m.params[g.morph], pl(g.morph)), notch: !!g.morphNotch } : undefined;
  // « Multi (fondu) » : la forme suit le fondu
  if (morph && /multi|multimode/.test(norm(name))) poles = 2;
  return { hz, reso, shape, poles, morph, name };
}

function filterDb(st: FilterState, f: number): number {
  if (st.shape === "off") return 0;
  const r = f / Math.max(20, st.hz);
  let mag: number;
  if (st.morph && (!st.name || /multi|fondu|multimode/.test(norm(st.name)))) {
    // fondu continu : bas (0) → bande ou réjection (0,5) → haut (1)
    const mid = st.morph.notch ? "notch" : "bp";
    const v = st.morph.v;
    mag = v < 0.5 ? magOf("lp", r, st.poles) * (1 - v * 2) + magOf(mid, r, st.poles) * v * 2
      : magOf(mid, r, st.poles) * (2 - v * 2) + magOf("hp", r, st.poles) * (v * 2 - 1);
  } else mag = magOf(st.shape, r, st.poles);
  if (st.shape !== "notch") mag *= 1 + st.reso * 4 * Math.exp(-Math.pow(Math.log2(r) * 2.4, 2));
  return 20 * Math.log10(Math.max(1e-5, mag));
}

const F_COLORS: (string | undefined)[] = [undefined, "var(--led-blue)"];

/** Un ou deux filtres : le point se tire en largeur (coupure) et en hauteur (résonance). */
export function FilterEditor(props: CurveProps & { groups: FilterGroup[] }) {
  const { m, values, played, groups, onSet, readOnly } = props;
  const acid = m.type === "acid";
  const statesOf = (vals: number[]) => groups.map((g) => filterState(m, vals, g, acid));
  const states = statesOf(values);
  const ptsOf = (st: FilterState) => {
    const pts: string[] = [];
    for (let i = 0; i <= 120; ++i) pts.push(`${((i / 120) * W).toFixed(1)},${yOfDb(filterDb(st, F_MIN * Math.pow(10, (i / 120) * F_DEC))).toFixed(1)}`);
    return pts.join(" ");
  };
  const build = (vals: number[]): Handle[] => {
    const hs: Handle[] = [];
    statesOf(vals).forEach((st, gi) => {
      const g = groups[gi];
      if (st.shape === "off") return;
      const pc = m.params[g.cutoff], pr = g.reso !== undefined ? m.params[g.reso] : null;
      hs.push({
        id: `f${gi}`, x: xOfHz(st.hz), y: yOfDb(-3 + st.reso * PEAK_DB), color: groups.length > 1 ? F_COLORS[gi % 2] : undefined,
        params: [g.cutoff, ...(g.reso !== undefined ? [g.reso] : [])],
        title: `${g.label && g.label.length <= 2 ? "Filtre " + g.label + " · " : ""}${pc.name} ${formatPlain(pc, st.hz)}${pr ? ` · ${pr.name} ${formatPlain(pr, toPlain(pr, vals[g.reso!] ?? 0))}` : ""}`,
        xSet: (sx) => onSet(g.cutoff, Math.min(pc.max, Math.max(pc.min, hzOfX(sx)))),
        ySet: pr ? (sy) => onSet(g.reso!, plainForLevel(pr, (dbOfY(sy) + 3) / PEAK_DB)) : undefined,
        reset: () => { onSet(g.cutoff, pc.def); if (pr) onSet(g.reso!, pr.def); },
      });
    });
    return hs;
  };
  const handles = build(values);
  const { ghosts, ranges } = ghostsAndRanges(build, handles, props);
  const ghostStates = played && ghosts.length ? statesOf(played) : null;
  const curves = states.map((st, gi) => ({ pts: ptsOf(st), ghost: ghostStates ? ptsOf(ghostStates[gi]) : null, color: groups.length > 1 ? F_COLORS[gi % 2] : undefined, off: st.shape === "off", label: groups[gi].label || "" }));
  return (
    <CurveBox handles={handles} scale={1} readOnly={readOnly} label="Filtre : tirez le point (coupure, résonance)" ghosts={ghosts} ranges={ranges}>
      <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve" preserveAspectRatio="none" aria-hidden>
        {[100, 1000, 10000].map((f) => <line key={f} x1={xOfHz(f)} x2={xOfHz(f)} y1={0} y2={H} className="grid" />)}
        <line x1={0} x2={W} y1={yOfDb(0)} y2={yOfDb(0)} className="grid grid--zero" />
        {curves.map((c, i) => (
          <g key={i} style={c.color ? { ["--k" as string]: c.color } : undefined} opacity={c.off ? 0.35 : 1}>
            {i === 0 && !c.off && <polyline points={`0,${H} ${c.pts} ${W},${H}`} className="fill" />}
            <polyline points={c.pts} className="stroke" />
            {c.ghost && !c.off && <polyline points={c.ghost} className="stroke stroke--ghost" />}
          </g>
        ))}
      </svg>
      {groups.length > 1 && <div className="curve-legend">{curves.map((c, i) => <span key={i} style={F_COLORS[i % 2] ? { ["--k" as string]: F_COLORS[i % 2] } : undefined}>{c.label}{c.off ? " (arrêt)" : ""}</span>)}</div>}
    </CurveBox>
  );
}

// ---------------------------------------------------------------------------------------------
// Oscillateurs

/** Largeur réglée → rapport cyclique, selon la machine (certaines vont de 50 % à étroit). */
const PW_CENTERED = new Set(["sh101", "blo", "polyj"]);
function dutyOf(m: ModuleDesc, p: ParamDesc, plain: number): number {
  const u = levelOf({ ...p, min: 0, max: 100 }, plain);
  return PW_CENTERED.has(m.type) ? 0.5 - 0.45 * u : Math.min(0.95, Math.max(0.05, u));
}
function plainOfDuty(m: ModuleDesc, p: ParamDesc, duty: number): number {
  const u = PW_CENTERED.has(m.type) ? (0.5 - Math.min(0.5, Math.max(0.05, duty))) / 0.45 : duty;
  return Math.min(p.max, Math.max(p.min, u * 100));
}

function wave(k: number, ph: number, duty: number): number {
  switch (k) {
    case 0: return Math.sin(2 * Math.PI * ph);
    case 1: return ph < 0.5 ? 4 * ph - 1 : 3 - 4 * ph;
    case 2: return 2 * ph - 1;
    default: return ph < duty ? 1 : -1;
  }
}

interface OscDraw { shape: number; duty: number; dutyParam?: number; mix?: { saw: number; pulse: number; tri: number; sub: number }; label: string }

function oscDraw(m: ModuleDesc, values: number[], g: OscGroup): OscDraw | null {
  const pl = (k: number) => toPlain(m.params[k], values[k] ?? 0);
  let shape = 2, label = "";
  if (g.wave !== undefined) {
    const name = m.params[g.wave].choices?.[pl(g.wave)] ?? "";
    const s = shapeOfWaveName(name);
    if (s === null) return null;
    shape = s; label = name;
  } else if (g.shape !== undefined) {
    const p = m.params[g.shape], v = pl(g.shape), n = norm(p.name);
    if (/scie → impulsion/.test(n)) shape = 2 + 2 * levelOf(p, v);
    else if (/^forme osc/.test(n)) shape = 2 + 2 * levelOf(p, v);
    else shape = p.max > 4 ? Math.min(4, (v / p.max) * 4) : v;
    label = formatPlain(p, v);
  }
  const duty = g.pw !== undefined ? dutyOf(m, m.params[g.pw], pl(g.pw)) : shape >= 3.5 ? 0.12 : 0.5;
  return { shape, duty, dutyParam: g.pw, label };
}

const MIX_RE = /^(scie|impulsion|triangle|sous-osc\.?)$/;
const PW_RE = /^largeur( [ab])?$/;
/** La section a-t-elle un mélangeur de formes ou une largeur d'impulsion ? */
export function hasMix(m: ModuleDesc, params: number[]): boolean {
  return params.some((k) => MIX_RE.test(norm(m.params[k].name)) || (PW_RE.test(norm(m.params[k].name)) && m.params[k].unit === "%"));
}

/** Mélangeur à formes superposées (scie, impulsion, triangle, sous-oscillateur) : la somme. Les
    niveaux et la largeur se cherchent dans la section, sinon dans tout l'instrument. */
export function mixDraw(m: ModuleDesc, values: number[], params: number[]): OscDraw | null {
  const pl = (k: number) => toPlain(m.params[k], values[k] ?? 0);
  const amt = (k: number) => { const p = m.params[k]; return p.choices ? (pl(k) > 0 ? 1 : 0) : levelOf(p, pl(k)); };
  const scan = (list: number[]) => {
    const mix = { saw: 0, pulse: 0, tri: 0, sub: 0 };
    let found = false, pw: number | undefined;
    for (const k of list) {
      const n = norm(m.params[k].name);
      if (/^scie$/.test(n)) { mix.saw = amt(k); found = true; }
      else if (/^impulsion$/.test(n)) { mix.pulse = amt(k); found = true; }
      else if (/^triangle$/.test(n)) { mix.tri = amt(k); found = true; }
      else if (/^sous-osc\.?$/.test(n)) { mix.sub = amt(k); found = true; }
      else if (PW_RE.test(n) && m.params[k].unit === "%" && pw === undefined) pw = k;
    }
    return { mix, found, pw };
  };
  const all = m.params.map((_, i) => i);
  const here = scan(params);
  const lv = here.found ? here : scan(all);
  if (!lv.found) return null;
  const pw = here.pw ?? (here.found ? undefined : lv.pw) ?? scan(all).pw;
  const duty = pw !== undefined ? dutyOf(m, m.params[pw], pl(pw)) : 0.5;
  return { shape: 2, duty, dutyParam: pw, mix: lv.mix, label: "" };
}

function oscPoints(d: OscDraw): string {
  const pts: string[] = [];
  const i = Math.min(3, Math.floor(d.shape)), f = d.shape - i;
  const N = 240;
  const ys: number[] = [];
  for (let n = 0; n <= N; ++n) {
    const ph = ((n / N) * 2) % 1;
    let y: number;
    if (d.mix) {
      const ph2 = ((n / N)) % 1; // sous-oscillateur : une octave plus bas
      y = d.mix.saw * wave(2, ph, d.duty) + d.mix.pulse * wave(3, ph, d.duty) + d.mix.tri * wave(1, ph, d.duty) + d.mix.sub * wave(3, ph2, 0.5);
    } else {
      const a = i >= 3 ? wave(3, ph, i === 3 && d.shape < 3.5 && d.dutyParam === undefined ? 0.5 : d.duty) : wave(i, ph, d.duty);
      const b = i + 1 >= 3 ? wave(3, ph, d.duty) : wave(i + 1, ph, d.duty);
      y = a * (1 - f) + b * f;
    }
    ys.push(y);
  }
  const peak = Math.max(1e-6, ...ys.map(Math.abs));
  ys.forEach((y, n) => pts.push(`${((n / N) * W).toFixed(1)},${(H / 2 - (y / peak) * (H / 2 - 14)).toFixed(1)}`));
  return pts.join(" ");
}

/** Forme d'onde (une ou deux) ; si l'impulsion a une largeur, son front se tire en largeur. */
export function OscEditor(props: CurveProps & { groups: OscGroup[]; mixParams?: number[] }) {
  const { m, values, played, groups, mixParams, onSet, readOnly } = props;
  const drawOf = (vals: number[]) => (mixParams ? [mixDraw(m, vals, mixParams)] : groups.map((g) => oscDraw(m, vals, g))).filter((d): d is OscDraw => !!d);
  const draws = drawOf(values);
  const build = (vals: number[]): Handle[] => {
    const hs: Handle[] = [];
    drawOf(vals).forEach((d, i) => {
      const pulseVisible = d.mix ? d.mix.pulse > 0 : d.shape > 2.5;
      if (d.dutyParam === undefined || !pulseVisible) return;
      const p = m.params[d.dutyParam], k = d.dutyParam;
      hs.push({
        id: `pw${i}`, x: d.duty * (W / 2), y: H / 2, color: i ? "var(--led-blue)" : undefined, params: [k],
        title: `${p.name} ${formatPlain(p, toPlain(p, vals[k] ?? 0))}`,
        xSet: (sx) => onSet(k, plainOfDuty(m, p, Math.min(0.98, Math.max(0.02, sx / (W / 2))))),
        reset: () => onSet(k, p.def),
      });
    });
    return hs;
  };
  const handles = build(values);
  const { ghosts, ranges } = ghostsAndRanges(build, handles, props);
  const ghostDraws = played && ghosts.length ? drawOf(played) : null;
  return (
    <CurveBox handles={handles} scale={1} readOnly={readOnly} label={draws.map((d) => d.label).filter(Boolean).join(" · ")} ghosts={ghosts} ranges={ranges}>
      <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve" preserveAspectRatio="none" aria-hidden>
        <line x1={0} x2={W} y1={H / 2} y2={H / 2} className="grid" />
        {draws.slice().reverse().map((d, ri) => {
          const i = draws.length - 1 - ri;
          return <polyline key={i} points={oscPoints(d)} className="stroke" style={i ? { ["--k" as string]: "var(--led-blue)", opacity: 0.6 } : undefined} />;
        })}
        {ghostDraws?.[0] && <polyline points={oscPoints(ghostDraws[0])} className="stroke stroke--ghost" />}
      </svg>
      {draws.length > 1 && <div className="curve-legend">{draws.map((d, i) => <span key={i} style={i ? { ["--k" as string]: "var(--led-blue)" } : undefined}>{i + 1} · {d.label}</span>)}</div>}
    </CurveBox>
  );
}

/** Amplitude des modulations des réglages d'un module (même calcul que le moteur : valeur + profondeur
    × source ; les LFO vont de −1 à 1, les autres sources de 0 à 1). */
export function modRangesOf(m: ModuleDesc, values: number[], base: number, mods: { source: number; target: number; amount: number }[] | undefined, modValues: number[][] | undefined): ModRanges {
  const out: ModRanges = new Map();
  for (const a of mods ?? []) {
    const k = a.target - base;
    if (k < 0 || k >= m.params.length || !a.amount) continue;
    const n0 = values[k] ?? 0;
    const type = Math.min(3, Math.max(0, Math.floor((modValues?.[a.source]?.[0] ?? 0) * 4)));
    const bipolar = a.source < (modValues?.length ?? 0) && type === 0;
    const lo = bipolar ? n0 - Math.abs(a.amount) : Math.min(n0, n0 + a.amount);
    const hi = bipolar ? n0 + Math.abs(a.amount) : Math.max(n0, n0 + a.amount);
    const prev = out.get(k);
    out.set(k, [Math.max(0, Math.min(lo, prev?.[0] ?? 1)), Math.min(1, Math.max(hi, prev?.[1] ?? 0))]);
  }
  return out;
}
void toNorm;
