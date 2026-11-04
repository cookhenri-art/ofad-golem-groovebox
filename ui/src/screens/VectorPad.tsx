// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix22 : pavé vectoriel. On reprend des synthés vectoriels la seule dynamique du pavé :
// une position X / Y (au doigt, à la souris, ou automatisée par le logiciel hôte), un mouvement
// qui tourne autour d'elle (cercle, huit, losange, hasard), sa vitesse calée sur les mesures et
// sa taille. Les quatre coins A B C D sont quatre scènes, mêlées selon la position (le pavé
// remplace alors le crossfader A / B tant qu'il est actif). Coins : A haut gauche, B haut droite,
// C bas gauche, D bas droite.

import { useEffect, useRef, useState } from "react";
import { AssignKnobs, openKnobMods } from "../device/AssignKnobs";
import { hostCode } from "../engine/protocol";
import { actions, setUi, showMessage, useStore } from "../engine/store";
import { busName, groupBase, groupChannels, trackColor } from "../engine/params";
import { RowLaunchBar } from "./RowLaunchBar";
import { TouchFader } from "./MixerScreen";
import { SlotCard } from "./ScenesScreen";
import { KnobLfoAssignBar } from "./ModSourcesDialog";
import { BREAK_SCENE, MIDI_XFADE } from "../engine/protocol";
import { paramAddress, useParam } from "../controls/bindings";

export const VEC_ORBITS = ["Arrêt", "Cercle", "Infini", "Losange", "Carré", "Triangle", "Ricochet", "Hasard", "Dessinée"] as const;
const DRAWN = 8;
export const VEC_RATES = ["1/2 mes.", "1 mes.", "2 mes.", "4 mes.", "8 mes.", "16 mes.", "32 mes.", "64 mes.", "128 mes."] as const;
const CORNERS = ["A", "B", "C", "D"] as const;

const RANDOM = 7;
const clamp01 = (v: number) => Math.min(1, Math.max(0, v));
/** v0.46 fix24 : sommets (-1..1) des mouvements, mêmes formes que le moteur (PluginProcessor, pavé vectoriel). */
function orbitShape(orbit: number): [number, number][] {
  const curve = (f: (t: number) => [number, number]) => Array.from({ length: 64 }, (_, i) => f((i / 64) * 2 * Math.PI));
  switch (orbit) {
    case 1: return curve((t) => [Math.cos(t), Math.sin(t)]);                 // cercle
    case 2: return curve((t) => [Math.sin(t), Math.sin(2 * t) * 0.5]);       // infini
    case 3: return [[0, -1], [1, 0], [0, 1], [-1, 0]];                        // losange
    case 4: return [[-1, -1], [1, -1], [1, 1], [-1, 1]];                      // carré
    case 5: return [[0, -1], [0.87, 0.5], [-0.87, 0.5]];                      // triangle
    case 6: return [[-1, -1], [-0.5, 1], [0, -1], [0.5, 1], [1, -1], [0.5, 1], [0, -1], [-0.5, 1]]; // ricochet
    default: return [];
  }
}

export function VectorPad({ full = false }: { full?: boolean }) {
  const g = useStore((s) => s.tick.g);
  const scenes = useStore((s) => s.scenes);
  const on = (g.vecOn ?? 0) >= 0.5;
  const x = g.vecX ?? 0.5, y = g.vecY ?? 0.5;
  const px = g.vecPX ?? x, py = g.vecPY ?? y;
  const orbit = Math.round(g.vecOrbit ?? 0), rate = Math.round(g.vecRate ?? 2), depth = g.vecDepth ?? 0.35;
  const rot = g.vecRot ?? 0; // v0.46 fix28 : rotation de la forme (degrés)
  const rad = (rot * Math.PI) / 180, rc = Math.cos(rad), rs = Math.sin(rad);
  const hgt = g.vecHeight ?? 1; // v0.46 fix28 : hauteur de la forme (aplatie < 1 < étirée)
  const turn = ([a, b0]: [number, number] | number[]): [number, number] => { const b = b0 * hgt; return [a * rc - b * rs, a * rs + b * rc]; };
  const sceneOf = (k: number) => Math.round(g[`vecS${CORNERS[k]}`] ?? k + 1);
  const pad = useRef<HTMLDivElement>(null);
  const drag = useRef<{ id: number } | null>(null);
  // v0.46 fix28 : forme en mouvement : glisser la déplace (sans saut), clic droit tiré vers le haut / bas
  // ou pincer à deux doigts (ou la molette) l'agrandit / la rétrécit
  const grab = useRef<{ id: number; cx: number; cy: number; x: number; y: number; depth: number; rot: number; size: boolean } | null>(null);
  const touches = useRef(new Map<number, [number, number]>());
  const pinch = useRef<{ d: number; a: number; depth: number; rot: number } | null>(null);
  const setDepth = (v: number) => actions.setGlobal("vecDepth", Math.min(1, Math.max(0.02, v)));
  const setRot = (v: number) => actions.setGlobal("vecRot", Math.round(((v % 360) + 360) % 360));
  const moving = orbit > 0 && orbit !== DRAWN;
  // v0.46 fix22 : forme dessinée au doigt (le mouvement suit exactement ce tracé)
  const [drawing, setDrawing] = useState(false);
  const [path, setPath] = useState<[number, number][]>([]);
  const stroke = useRef<[number, number][]>([]);
  useEffect(() => { void actions.getVecPath().then((t) => setPath(parsePath(t))); }, []);
  const toPad = (cx: number, cy: number): [number, number] | null => {
    const el = pad.current; if (!el) return null;
    const r = el.getBoundingClientRect();
    return [Math.min(1, Math.max(0, (cx - r.left) / r.width)), Math.min(1, Math.max(0, (cy - r.top) / r.height))];
  };
  const frame = useRef(0);
  const pending = useRef<[number, number] | null>(null);

  const setXY = (cx: number, cy: number) => {
    const el = pad.current; if (!el) return;
    const r = el.getBoundingClientRect();
    const nx = Math.min(1, Math.max(0, (cx - r.left) / r.width)), ny = Math.min(1, Math.max(0, (cy - r.top) / r.height));
    pending.current = [nx, ny];
    if (frame.current) return;
    frame.current = requestAnimationFrame(() => {
      frame.current = 0;
      const p = pending.current; if (!p) return;
      actions.setGlobal("vecX", p[0]); actions.setGlobal("vecY", p[1]);
    });
  };
  // poids des coins (bilinéaires), pour l'éclairage
  const w = [(1 - px) * (1 - py), px * (1 - py), (1 - px) * py, px * py];
  const cornerPos = [{ left: 0, top: 0 }, { right: 0, top: 0 }, { left: 0, bottom: 0 }, { right: 0, bottom: 0 }];

  const padEl = (

        <div ref={pad} className={`vec-pad${on ? " on" : ""}`} role="slider" aria-label="Pavé vectoriel : position entre les quatre scènes"
          aria-valuetext={`X ${Math.round(x * 100)} %, Y ${Math.round(y * 100)} %`}
          onContextMenu={(e) => { if (moving) e.preventDefault(); }}
          onWheel={(e) => { if (!moving || drawing) return; if (e.shiftKey) setRot(rot + (e.deltaY < 0 ? 5 : -5)); else setDepth(depth + (e.deltaY < 0 ? 0.03 : -0.03)); }}
          onPointerDown={(e) => {
            e.preventDefault(); e.currentTarget.setPointerCapture(e.pointerId);
            touches.current.set(e.pointerId, [e.clientX, e.clientY]);
            if (!drawing && moving && touches.current.size === 2) {
              const [a, b] = [...touches.current.values()];
              pinch.current = { d: Math.max(10, Math.hypot(a[0] - b[0], a[1] - b[1])), a: Math.atan2(b[1] - a[1], b[0] - a[0]), depth, rot };
              grab.current = null; drag.current = null;
              return;
            }
            drag.current = { id: e.pointerId };
            if (drawing) { const p = toPad(e.clientX, e.clientY); stroke.current = p ? [p] : []; setPath(stroke.current); return; }
            if (!on) actions.setGlobal("vecOn", 1); // toucher le pavé l'active
            if (moving) { grab.current = { id: e.pointerId, cx: e.clientX, cy: e.clientY, x, y, depth, rot, size: e.button === 2 }; return; }
            setXY(e.clientX, e.clientY);
          }}
          onPointerMove={(e) => {
            if (touches.current.has(e.pointerId)) touches.current.set(e.pointerId, [e.clientX, e.clientY]);
            if (pinch.current && touches.current.size >= 2) {
              const [a, b] = [...touches.current.values()];
              setDepth(pinch.current.depth * Math.hypot(a[0] - b[0], a[1] - b[1]) / pinch.current.d);
              setRot(pinch.current.rot + ((Math.atan2(b[1] - a[1], b[0] - a[0]) - pinch.current.a) * 180) / Math.PI); // tourner les deux doigts
              return;
            }
            const gr = grab.current;
            if (gr && gr.id === e.pointerId) {
              const r = pad.current?.getBoundingClientRect(); if (!r) return;
              // clic droit : vers le haut / bas = taille, vers la gauche / droite = rotation
              if (gr.size) { setDepth(gr.depth + (gr.cy - e.clientY) / 250); setRot(gr.rot + (e.clientX - gr.cx) / 2); return; }
              setXY(r.left + (gr.x + (e.clientX - gr.cx) / r.width) * r.width, r.top + (gr.y + (e.clientY - gr.cy) / r.height) * r.height);
              return;
            }
            if (drag.current?.id !== e.pointerId) return;
            if (drawing) {
              const p = toPad(e.clientX, e.clientY); const last = stroke.current[stroke.current.length - 1];
              if (p && (!last || Math.hypot(p[0] - last[0], p[1] - last[1]) > 0.012)) { stroke.current = [...stroke.current, p]; setPath(stroke.current); }
              return;
            }
            setXY(e.clientX, e.clientY);
          }}
          onPointerUp={(e) => {
            touches.current.delete(e.pointerId);
            if (touches.current.size < 2) pinch.current = null;
            if (grab.current?.id === e.pointerId) grab.current = null;
            drag.current = null;
            if (!drawing) return;
            const pts = simplify(stroke.current, 128);
            setDrawing(false);
            if (pts.length < 3) { setPath([]); return; }
            setPath(pts);
            void actions.setVecPath(pts.map(([px0, py0]) => `${px0.toFixed(3)},${py0.toFixed(3)}`).join(";"));
            actions.setGlobal("vecOrbit", DRAWN);
            if (!on) actions.setGlobal("vecOn", 1);
          }}
          onPointerCancel={(e) => { touches.current.delete(e.pointerId); pinch.current = null; grab.current = null; drag.current = null; }}
          onDoubleClick={() => { actions.setGlobal("vecX", 0.5); actions.setGlobal("vecY", 0.5); }}>
          <span className="vec-grid" aria-hidden />
          {CORNERS.map((c, k) => (
            <span key={c} className="vec-corner" style={{ ...cornerPos[k], ["--w" as string]: on ? w[k] : 0 } as React.CSSProperties}>
              <b>{c}</b><small>Scène {sceneOf(k)}{scenes[sceneOf(k) - 1]?.locks.length ? "" : " · vide"}</small>
            </span>
          ))}
          {(orbit === DRAWN || drawing) && path.length > 1 && (
            <svg className="vec-path" viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden>
              <polyline points={path.map(([a, b]) => `${a * 100},${b * 100}`).join(" ")} fill="none" vectorEffect="non-scaling-stroke" />
            </svg>
          )}
          {drawing && <span className="vec-draw-hint">Dessinez la forme d'un trait, puis levez le doigt</span>}
          {/* v0.46 fix24 : la forme du mouvement choisi, à sa place et à sa taille (comme le moteur la parcourt) */}
          {orbit > 0 && orbit !== DRAWN && (
            <svg className={`vec-path vec-path--shape${on ? "" : " off"}`} viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden>
              <polygon points={(orbit === RANDOM ? [[-1, -1], [1, -1], [1, 1], [-1, 1]] : orbitShape(orbit)).map((p) => { const [a, b] = turn(p); return `${clamp01(x + a * depth / 2) * 100},${clamp01(y + b * depth / 2) * 100}`; }).join(" ")}
                fill="none" vectorEffect="non-scaling-stroke" />
            </svg>
          )}
          <span className="vec-home" style={{ left: `${x * 100}%`, top: `${y * 100}%` }} aria-hidden />
          <span className={`vec-dot${on ? " on" : ""}`} style={{ left: `${px * 100}%`, top: `${py * 100}%` }} aria-hidden />
        </div>
  );
  // v0.46 fix28 : réglages du pavé (coins, mouvement, durée, taille, hauteur, rotation) : à côté du pavé,
  // et sur grand écran aussi dans le pavé en grand
  const settingsEl = (
    <>
    <div className="vec-group">
      <span className="vec-label">Coins</span>
      <div className="vec-corners">
        {CORNERS.map((c, k) => (
          <label key={c} className="vec-sel">
            <b>{c}</b>
            <select value={sceneOf(k)} onChange={(e) => actions.setGlobal(`vecS${c}`, Number(e.target.value))} aria-label={`Scène du coin ${c}`}>
              {Array.from({ length: 32 }, (_, i) => <option key={i} value={i + 1}>Scène {i + 1}</option>)}
            </select>
          </label>
        ))}
      </div>
    </div>

    <div className="vec-group">
      <span className="vec-label">Mouvement</span>
      <div className="vec-chips">
        {VEC_ORBITS.map((o, i) => (
          <button key={o} type="button" className={`chip${orbit === i ? " chip--on" : ""}`} onClick={() => actions.setGlobal("vecOrbit", i)}>{o}</button>
        ))}
      </div>
    </div>
    <div className="vec-row">
      <button type="button" className={`chip${drawing ? " chip--on" : ""}`} aria-pressed={drawing} onClick={() => setDrawing(!drawing)}
        title="Dessiner sur le pavé la forme que suivra le mouvement (elle est gardée avec le projet)">✎ Dessiner une forme</button>
      {path.length > 1 && orbit !== DRAWN && <button type="button" className="chip" onClick={() => actions.setGlobal("vecOrbit", DRAWN)}>Suivre le dessin</button>}
    </div>
    <div className="vec-group">
      <span className="vec-label">Un tour en</span>
      <div className="vec-chips">
        {VEC_RATES.map((r, i) => (
          <button key={r} type="button" className={`chip${rate === i ? " chip--on" : ""}`} onClick={() => actions.setGlobal("vecRate", i)}>{r}</button>
        ))}
      </div>
    </div>
    <div className="vec-group">
      <span className="vec-label">Taille {Math.round(depth * 100)} %</span>
      <input type="range" min={0} max={100} value={Math.round(depth * 100)} onChange={(e) => actions.setGlobal("vecDepth", Number(e.target.value) / 100)} aria-label="Taille du mouvement" />
    </div>
    <div className="vec-group">
      <span className="vec-label">Hauteur de la forme {Math.round(hgt * 100)} %</span>
      <input type="range" min={5} max={200} value={Math.round(hgt * 100)} onChange={(e) => actions.setGlobal("vecHeight", Number(e.target.value) / 100)} aria-label="Hauteur de la forme : aplatir ou étirer" />
      <div className="vec-chips">
        {[25, 50, 100, 150].map((h) => (
          <button key={h} type="button" className={`chip${Math.round(hgt * 100) === h ? " chip--on" : ""}`} onClick={() => actions.setGlobal("vecHeight", h / 100)}>{h} %</button>
        ))}
      </div>
    </div>
    <div className="vec-group">
      <span className="vec-label">Rotation {Math.round(rot)}°</span>
      <input type="range" min={0} max={359} value={Math.round(rot)} onChange={(e) => setRot(Number(e.target.value))} aria-label="Rotation de la forme" />
      <div className="vec-chips">
        {[0, 45, 90, 135].map((d) => (
          <button key={d} type="button" className={`chip${Math.round(rot) === d ? " chip--on" : ""}`} onClick={() => setRot(d)}>{d}°</button>
        ))}
      </div>
    </div>
    </>
  );
  if (full) return <VectorFull pad={padEl} settings={settingsEl} on={on} orbit={orbit} rate={rate} drawing={drawing} setDrawing={setDrawing} />;
  return (
    <div className="vec">
      <div className="vec-pad-wrap">{padEl}</div>

      <div className="vec-side">
        <div className="vec-row">
          <button type="button" className={`chip${on ? " chip--on" : ""}`} aria-pressed={on} onClick={() => actions.setGlobal("vecOn", on ? 0 : 1)}
            title="Pavé actif : les quatre scènes des coins sont mêlées selon la position (à la place du crossfader A / B). Toucher le pavé l'active.">
            {on ? "● Pavé actif" : "○ Pavé arrêté"}
          </button>
          <small className="vec-note">{on ? "remplace le crossfader A / B" : "le crossfader A / B joue"}</small>
          <button type="button" className="chip chip--live" onClick={() => setUi({ vecFull: true })} title="Vue live : le pavé ou le crossfader, leurs réglages, la console des 16 pistes et les 16 boutons, sur un seul écran">⛶ Vue live</button>
        </div>

        {settingsEl}
        <p className="vec-help">Glissez sur le pavé (double-clic : au centre). Forme en mouvement : glisser la déplace ; clic droit tiré vers le haut / le bas : taille, vers la gauche / la droite : rotation ; molette : taille (Maj + molette : rotation) ; à deux doigts : pincer pour la taille, tourner pour la rotation. Le mouvement est calé sur les mesures en lecture.</p>
      </div>
    </div>
  );
}

function parsePath(t: string): [number, number][] {
  return t.split(";").map((p) => p.split(",").map(Number)).filter((a) => a.length === 2 && a.every(Number.isFinite)).map((a) => [a[0], a[1]] as [number, number]);
}

/** Au plus max points, répartis le long du tracé. */
function simplify(pts: [number, number][], max: number): [number, number][] {
  if (pts.length <= max) return pts;
  const out: [number, number][] = [];
  for (let i = 0; i < max; ++i) out.push(pts[Math.round((i * (pts.length - 1)) / (max - 1))]);
  return out;
}

/** v0.46 fix28 : bas de la colonne du milieu de la vue live : position dans le morceau (mesure, temps,
    boucle la plus longue, rangée du morceau) et Photo / Rappel / Mod. boutons. */
function LivePerf() {
  const t = useStore((s) => s.tick.t);
  const song = useStore((s) => s.tick.song);
  const hasPhoto = !!useStore((s) => s.tick.hist)?.[2];
  const ppq = Math.max(0, t.ppq ?? 0);
  const bar = Math.floor(ppq / 4) + 1, beat = Math.floor(ppq % 4) + 1;
  const loopBars = t.lq ? Math.max(1, Math.round(t.lq / 4)) : 0;
  const lp = t.lp ?? 0;
  const songRow = song && song[0] >= 0 ? song[0] + 1 : 0;
  return (
    <div className="liveperf">
      <div className="liveperf-pos" aria-label="Position dans le morceau">
        <span className="liveperf-label">Position</span>
        <div className="liveperf-clock tabular"><b>{t.playing ? bar : "—"}</b><small>mesure</small><b>{t.playing ? beat : "—"}</b><small>temps</small>
          {songRow > 0 && <><b>{songRow}</b><small>rangée du morceau</small></>}</div>
        <div className="liveperf-loop" title={loopBars ? `Boucle la plus longue : ${loopBars} mesure${loopBars > 1 ? "s" : ""}` : "À l'arrêt"}>
          {Array.from({ length: Math.min(16, Math.max(1, loopBars)) }, (_, k) => {
            const n = Math.min(16, Math.max(1, loopBars));
            const on = t.playing && lp * n >= k && lp * n < k + 1;
            const past = t.playing && lp * n >= k + 1;
            return <i key={k} className={on ? "on" : past ? "past" : ""} />;
          })}
        </div>
        <span className="liveperf-sub">{loopBars ? `boucle de ${loopBars} mesure${loopBars > 1 ? "s" : ""}` : "lancez la lecture"}</span>
      </div>
      <div className="liveperf-btns">
        <button type="button" onClick={() => void actions.takePhoto()} title="Photo de tous les réglages du projet (remplace la précédente)">Photo</button>
        <button type="button" className={hasPhoto ? "" : "is-off"} onClick={() => { if (hasPhoto) void actions.recallPhoto(); else showMessage("Aucune photo prise : touchez d'abord Photo"); }}
          title="Revenir à la photo des réglages">Rappel</button>
        <button type="button" className="mod" onClick={openKnobMods} title="Modulations des 16 boutons assignables">Mod. boutons</button>
      </div>
    </div>
  );
}

/** v0.46 fix28 : tranche de la console du pavé en grand (PC) : couleur, nom, fader de volume, crête-mètre,
    niveau, coupure et solo. Piste ou groupe du mixeur. */
function LiveStrip({ ch, kind, g }: { ch: number; kind: "track" | "group"; g?: number }) {
  const L = useStore((s) => s.catalog?.layout);
  const info = useStore((s) => (kind === "track" ? s.tracks.tracks[ch] : undefined));
  const bus = useStore((s) => (kind === "group" && L ? s.tracks.buses?.[ch - L.numTracks] : undefined));
  const peak = useStore((s) => s.tick.peaks[ch] ?? 0);
  const volume = useParam(paramAddress(ch, "mix", 0, 0), "Volume");
  const used = useStore((s) => (kind === "group" ? s.tracks.tracks.some((t) => t.instrument && t.out === g) : !!s.tracks.tracks[ch]?.instrument));
  if (!L) return null;
  const name = kind === "track" ? info?.name ?? `Piste ${ch + 1}` : busName(L, ch);
  const tag = kind === "track" ? String(ch + 1) : `G${g}`;
  const muted = kind === "track" ? !!info?.mute : !!bus?.mute;
  const solo = kind === "track" ? !!info?.solo : !!bus?.solo;
  const db = peak > 0 ? 20 * Math.log10(peak) : -90;
  const meter = Math.max(0, Math.min(1, (db + 60) / 66));
  const color = kind === "track" ? trackColor(info?.color ?? ch) : "oklch(0.75 0.12 200)";
  const picked = useStore((s) => s.focus === ch);
  const fxCount = useStore((s) => (s.tracks.inserts[ch] ?? []).filter((t) => !!t).length);
  // v0.46 fix28 : toucher le nom ou le fader choisit la piste (Ouvrir l'instrument, Effets, boutons assignables)
  const pick = () => { if (!picked) void actions.focus(ch); };
  return (
    <div className={`lstrip${used ? "" : " is-empty"}${muted ? " is-muted" : ""}${solo ? " is-solo" : ""}${picked ? " is-picked" : ""}`} style={{ "--k": color } as React.CSSProperties}>
      <button type="button" className="lstrip-head" title={`${name} — toucher : choisir la piste`} onClick={pick}><b>{tag}</b><span>{name}</span></button>
      <div className="lstrip-fader" onPointerDownCapture={pick}>
        <TouchFader binding={volume} label={`Volume ${name}`} />
        <span className="lstrip-meter" aria-hidden><span style={{ height: `${meter * 100}%` }} className={db > -1 ? "hot" : ""} /></span>
      </div>
      <span className="lstrip-db tabular">{volume?.text ?? ""}</span>
      {/* v0.46 fix28 : FX comme dans le mixeur : ouvre directement les effets de cette piste (✕ : retour à la vue live) */}
      <button type="button" className={`lstrip-fx${fxCount ? " has" : ""}`} title={fxCount ? `Effets de ${name} : ${fxCount} — ouvrir` : `Ajouter des effets à ${name}`}
        onClick={() => { void actions.focus(ch).then(() => setUi({ pedals: true })); }}>FX{fxCount ? <b className="tabular">{fxCount}</b> : null}</button>
      <div className="lstrip-ms">
        <button type="button" className={`m${muted ? " on" : ""}`} aria-pressed={muted}
          onClick={() => void (kind === "track" ? actions.trackMute(ch, !muted) : actions.busMute(ch, !muted))}>M</button>
        <button type="button" className={`s${solo ? " on" : ""}`} aria-pressed={solo}
          onClick={() => void (kind === "track" ? actions.trackSolo(ch, !solo) : actions.busSolo(ch, !solo))}>S</button>
      </div>
    </div>
  );
}

/** v0.46 fix22 : le pavé en grand, sur tout l'écran (téléphone, tablette, ordinateur) : le pavé, et à
    côté les 16 pistes avec coupure (M) et solo (S) ; en haut, retour, transport et mouvement. */
function VectorFull({ pad, settings, on, orbit, rate, drawing, setDrawing }: { pad: React.ReactNode; settings: React.ReactNode; on: boolean; orbit: number; rate: number; drawing: boolean; setDrawing: (v: boolean) => void }) {
  const tracks = useStore((s) => s.tracks.tracks);
  const buses = useStore((s) => s.tracks.buses);
  const L = useStore((s) => s.catalog?.layout);
  const t = useStore((s) => s.tick.t);
  const focus = useStore((s) => s.focus);
  const focusIsTrack = !!L && focus < L.numTracks && !!tracks[focus]?.instrument;
  const focusName = L ? (focus < L.numTracks ? `${focus + 1} · ${tracks[focus]?.name ?? ""}` : busName(L, focus)) : "";
  const focusColor = L && focus < L.numTracks ? trackColor(tracks[focus]?.color ?? focus) : "oklch(0.75 0.12 200)";
  // mode crossfader : scènes A / B, banque, grille
  const gg = useStore((s) => s.tick.g);
  const scenes = useStore((s) => s.scenes);
  const bank = useStore((s) => s.ui.sceneBank);
  const sceneA = Math.round(gg.sceneA) - 1, sceneB = Math.round(gg.sceneB) - 1;
  // v0.46 fix28 : à côté du pavé, les 16 pistes ou les groupes du mixeur ; en vue Pistes, les groupes
  // utilisés (une piste au moins y est envoyée) prennent la place des pistes sans instrument
  const [view, setViewState] = useState<"tracks" | "groups">(() => { try { return localStorage.getItem("grv.vecSide") === "groups" ? "groups" : "tracks"; } catch { return "tracks"; } });
  const setView = (v: "tracks" | "groups") => { setViewState(v); try { localStorage.setItem("grv.vecSide", v); } catch { /* stockage absent */ } };
  type Slot = { kind: "track"; i: number } | { kind: "group"; ch: number; g: number; used: boolean };
  const groups: Slot[] = L ? groupChannels(L).map((ch) => { const g = ch - groupBase(L) + 1; return { kind: "group" as const, ch, g, used: tracks.some((tr) => tr.instrument && tr.out === g) }; }) : [];
  const usedGroups = groups.filter((g) => g.kind === "group" && g.used);
  const slots: Slot[] = view === "groups" ? groups : (() => {
    const queue = [...usedGroups];
    return tracks.slice(0, 16).map((tr, i) => (!tr.instrument && queue.length ? queue.shift()! : { kind: "track" as const, i }));
  })();
  useEffect(() => {
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); setUi({ vecFull: false }); } };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  }, []);
  return (
    <div className="vecfull" role="dialog" aria-label="Pavé vectoriel en grand">
      <KnobLfoAssignBar />
      <header className="vecfull-top">
        <button type="button" className="vecfull-back" onClick={() => setUi({ vecFull: false })} title="Revenir (Échap)">← Retour</button>
        <span className="vecfull-transport">
          <button type="button" className={`ft-rec${t.rec ? " on" : ""}`} onClick={actions.toggleRecord} aria-label="Enregistrer">●</button>
          <button type="button" className={`ft-play${t.playing ? " on" : ""}`} onClick={() => (t.playing ? actions.stop() : actions.play())} aria-label={t.playing ? "Arrêter" : "Lecture"}>{t.playing ? "❚❚" : "▶"}</button>
          <button type="button" className="ft-stop" onClick={actions.stop} aria-label="Arrêt">■</button>
        </span>
        {/* v0.46 fix28 : mode de la vue live : crossfader A / B ou pavé vectoriel */}
        <span className="vecfull-mode" role="radiogroup" aria-label="Crossfader ou vecteur" data-ctl={hostCode("vecOn")} data-ctl-label="Pavé actif" data-ctl-norm={on ? 1 : 0}>
          <button type="button" role="radio" aria-checked={!on} className={!on ? "on" : ""} onClick={() => actions.setGlobal("vecOn", 0)}>Crossfader</button>
          <button type="button" role="radio" aria-checked={on} className={on ? "on" : ""} onClick={() => actions.setGlobal("vecOn", 1)}>Vecteur</button>
        </span>
        {on && <>
        <select className="vecfull-sel" value={orbit} onChange={(e) => actions.setGlobal("vecOrbit", Number(e.target.value))} aria-label="Mouvement"
          data-ctl={hostCode("vecOrbit")} data-ctl-label="Pavé : mouvement" data-ctl-norm={orbit / 8}>
          {VEC_ORBITS.map((o, i) => <option key={o} value={i}>{i === 0 ? "Sans mouvement" : o}</option>)}
        </select>
        <select className="vecfull-sel" value={rate} onChange={(e) => actions.setGlobal("vecRate", Number(e.target.value))} aria-label="Un tour en"
          data-ctl={hostCode("vecRate")} data-ctl-label="Pavé : un tour en" data-ctl-norm={rate / 8}>
          {VEC_RATES.map((r, i) => <option key={r} value={i}>{r}</option>)}
        </select>
        <button type="button" className={`chip${drawing ? " chip--on" : ""}`} onClick={() => setDrawing(!drawing)}>✎ Dessiner</button>
        </>}
        <span className="vecfull-spacer" />
        {/* v0.46 fix28 : piste choisie (toucher une tranche de la console) : son instrument, ses effets */}
        {focusName && <span className="vecfull-focus" style={{ "--k": focusColor } as React.CSSProperties}><i aria-hidden />{focusName}</span>}
        {focusIsTrack && <button type="button" className="chip chip--inst" onClick={() => setUi({ face: true })}
          title="Ouvrir la façade de l'instrument de la piste choisie (✕ pour revenir à la vue live)">Ouvrir l'instrument</button>}
      </header>
      {/* v0.46 fix26 : les 16 boutons assignables sur une seule rangée (le pavé garde sa hauteur) */}
      <div className="vecfull-knobs"><div className="vecfull-knobs-grid"><AssignKnobs /></div></div>
      {/* rangées A à H (lancement de tout un tableau de patterns), comme sur l'écran Lancement */}
      <div className="vecfull-rows"><RowLaunchBar /></div>
      <div className="vecfull-body">
        {on ? <div className="vecfull-pad">{pad}</div> : (
          <div className="vecfull-pad vecfull-xf">
            <SlotCard slot="A" scene={sceneA} copying={false} onCopy={() => setUi({ vecFull: false, screen: "scenes" })} />
            <div className="livexf" data-ctl={MIDI_XFADE} data-ctl-label="Crossfader" data-ctl-norm={gg.xfade}>
              <button type="button" className={`livexf-end a${gg.xfade <= 0.001 ? " on" : ""}`} onClick={() => actions.setGlobal("xfade", 0)}>A</button>
              <div className="livexf-track">
                <input type="range" min={0} max={1000} step={1} value={Math.round(gg.xfade * 1000)} aria-label="Crossfader"
                  onChange={(e) => actions.setGlobal("xfade", Number(e.target.value) / 1000)} onDoubleClick={() => actions.setGlobal("xfade", 0.5)} />
                <span className="livexf-marks" aria-hidden><i>tout A</i><i>moitié</i><i>tout B</i></span>
              </div>
              <button type="button" className={`livexf-end b${gg.xfade >= 0.999 ? " on" : ""}`} onClick={() => actions.setGlobal("xfade", 1)}>B</button>
            </div>
            <SlotCard slot="B" scene={sceneB} copying={false} onCopy={() => setUi({ vecFull: false, screen: "scenes" })} />
          </div>
        )}
        {/* v0.46 fix28 : grand écran (PC) : tous les réglages à côté (pavé, ou scènes du crossfader) */}
        {on ? <div className="vecfull-settings vec-side">{settings}<LivePerf /></div> : (
          <div className="vecfull-settings vec-side vecfull-scenes">
            <div className="vec-group">
              <span className="vec-label">Scènes · toucher : lancer (A ou B suivant le crossfader)</span>
              <div className="vec-chips">
                {[0, 1].map((b) => <button key={b} type="button" className={`chip${bank === b ? " chip--on" : ""}`} onClick={() => setUi({ sceneBank: b })}>{b === 0 ? "1 – 16" : "17 – 32"}</button>)}
              </div>
              <div className="livescenes">
                {scenes.slice(bank * 16, bank * 16 + 16).map((sc, i) => {
                  const n = bank * 16 + i;
                  return (
                    <button key={n} type="button" className={`livescene${n === sceneA ? " is-a" : ""}${n === sceneB ? " is-b" : ""}${sc.locks.length ? " is-filled" : ""}`}
                      onClick={() => actions.launchScene(n)} title={`Scène ${n + 1} : ${sc.locks.length} réglage${sc.locks.length > 1 ? "s" : ""}`}>
                      <b>{n + 1}</b><small>{sc.locks.length ? `${sc.locks.length} régl.` : "vide"}</small>
                      {n === sceneA && <em className="a">A</em>}{n === sceneB && <em className="b">B</em>}
                    </button>
                  );
                })}
              </div>
            </div>
            <SlotCard slot="Break" scene={BREAK_SCENE} copying={false} onCopy={() => setUi({ vecFull: false, screen: "scenes" })} />
            <LivePerf />
          </div>
        )}
        {/* v0.46 fix28 : grand écran : console des 16 pistes (ou des groupes) avec leurs faders */}
        <div className="vecfull-console">
          <div className="vecfull-console-head">
            <b>Console</b>
            {groups.length > 0 && (
              <span className="vecfull-switch" role="radiogroup" aria-label="Pistes ou groupes">
                <button type="button" role="radio" aria-checked={view === "tracks"} className={view === "tracks" ? "on" : ""} onClick={() => setView("tracks")}>Pistes{usedGroups.length ? ` + ${usedGroups.length} gr.` : ""}</button>
                <button type="button" role="radio" aria-checked={view === "groups"} className={view === "groups" ? "on" : ""} onClick={() => setView("groups")}>Groupes</button>
              </span>
            )}
          </div>
          <div className={`vecfull-console-strips${view === "groups" ? " is-groups" : ""}`}>
            {slots.map((sl, k) => sl.kind === "track"
              ? <LiveStrip key={k} ch={sl.i} kind="track" />
              : <LiveStrip key={k} ch={sl.ch} kind="group" g={sl.g} />)}
          </div>
        </div>
        <div className="vecfull-side">
          {groups.length > 0 && (
            <div className="vecfull-switch" role="radiogroup" aria-label="Pistes ou groupes">
              <button type="button" role="radio" aria-checked={view === "tracks"} className={view === "tracks" ? "on" : ""} onClick={() => setView("tracks")}
                title="Les 16 pistes ; les groupes utilisés prennent la place des pistes sans instrument">Pistes{usedGroups.length ? ` + ${usedGroups.length} gr.` : ""}</button>
              <button type="button" role="radio" aria-checked={view === "groups"} className={view === "groups" ? "on" : ""} onClick={() => setView("groups")}
                title="Les groupes du mixeur (coupure et solo du groupe entier)">Groupes</button>
            </div>
          )}
          <div className="vecfull-tracks" role="group" aria-label={view === "groups" ? "Coupure et solo des groupes" : "Coupure et solo des pistes"}>
            {slots.map((sl, k) => {
              if (sl.kind === "track") {
                const tr = tracks[sl.i];
                return (
                  <div key={k} className={`vecfull-trk${tr.instrument ? "" : " is-empty"}`} style={{ "--k": trackColor(tr.color ?? sl.i) } as React.CSSProperties}>
                    <span className={`vecfull-name${focus === sl.i ? " is-picked" : ""}`} title={`${tr.name} — toucher : choisir la piste`} onClick={() => void actions.focus(sl.i)}>{sl.i + 1} · {tr.name}</span>
                    <button type="button" className={`vecfull-m${tr.mute ? " on" : ""}`} aria-pressed={!!tr.mute} onClick={() => void actions.trackMute(sl.i, !tr.mute)}>M</button>
                    <button type="button" className={`vecfull-s${tr.solo ? " on" : ""}`} aria-pressed={!!tr.solo} onClick={() => void actions.trackSolo(sl.i, !tr.solo)}>S</button>
                  </div>
                );
              }
              const b = L ? buses?.[sl.ch - L.numTracks] : undefined;
              const name = L ? busName(L, sl.ch) : `Groupe ${sl.g}`;
              const members = tracks.filter((tr) => tr.instrument && tr.out === sl.g).map((tr) => tr.name).join(", ");
              return (
                <div key={k} className={`vecfull-trk vecfull-trk--group${sl.used ? "" : " is-empty"}`}>
                  <span className="vecfull-name" title={members ? `${name} : ${members}` : `${name} (aucune piste envoyée)`}>G{sl.g} · {name}</span>
                  <button type="button" className={`vecfull-m${b?.mute ? " on" : ""}`} aria-pressed={!!b?.mute} onClick={() => void actions.busMute(sl.ch, !b?.mute)} title={`Couper le groupe ${name}`}>M</button>
                  <button type="button" className={`vecfull-s${b?.solo ? " on" : ""}`} aria-pressed={!!b?.solo} onClick={() => void actions.busSolo(sl.ch, !b?.solo)} title={`Solo du groupe ${name}`}>S</button>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </div>
  );
}
