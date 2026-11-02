// Groovebox — OFAD — AGPL-3.0-or-later
// Analyseur de spectre du bus principal (v0.45.1), principe des analyseurs de mixage à courbe
// lissée : 96 bandes de 20 Hz à 20 kHz calculées par le moteur (FFT 4096 points) tant que le panneau
// est affiché ; ici : pente d'affichage (dB par octave autour de 1 kHz, le mix « équilibré » paraît
// plat), lissage dans le temps (montée rapide, descente réglable), crêtes tenues, côtés (stéréo),
// lecture de la fréquence, de la note et du niveau sous le pointeur.

import { useEffect, useMemo, useRef, useState } from "react";
import { actions, useStore } from "../engine/store";
import { noteName } from "../engine/params";

const BANDS = 96, F_LO = 20, F_HI = 20000;
const bandHz = (b: number) => F_LO * Math.pow(F_HI / F_LO, (b + 0.5) / BANDS);
const xOfHz = (hz: number) => Math.log(hz / F_LO) / Math.log(F_HI / F_LO);
const SLOPES = [0, 3, 4.5, 6];
const FALLS = [{ label: "Rapide", db: 2.4 }, { label: "Moyen", db: 0.9 }, { label: "Lent", db: 0.35 }];
const RANGES = [60, 90, 120];
let prefs = { slope: 2, fall: 1, range: 1, side: true, hold: true }; // gardés pendant la séance

export function SpectrumPanel({ compact = false }: { compact?: boolean }) {
  const spec = useStore((s) => s.tick.spec);
  const [opt, setOptState] = useState(prefs);
  const setOpt = (o: Partial<typeof prefs>) => { prefs = { ...prefs, ...o }; setOptState(prefs); };
  const smooth = useRef<Float32Array>(new Float32Array(BANDS * 2).fill(-140));
  const peaks = useRef<Float32Array>(new Float32Array(BANDS).fill(-140));
  const [, redraw] = useState(0);
  const [hover, setHover] = useState<{ x: number; y: number } | null>(null);
  const box = useRef<SVGSVGElement>(null);

  useEffect(() => { actions.setSpectrum(true); return () => actions.setSpectrum(false); }, []);
  // lissage et crêtes à chaque image reçue
  useEffect(() => {
    if (!spec || spec.length < BANDS * 2) return;
    const fall = FALLS[opt.fall].db;
    const sm = smooth.current, pk = peaks.current;
    for (let i = 0; i < BANDS * 2; ++i) {
      const v = spec[i] / 10;
      sm[i] = v > sm[i] ? sm[i] + (v - sm[i]) * 0.6 : Math.max(v, sm[i] - fall);
    }
    for (let b = 0; b < BANDS; ++b) pk[b] = Math.max(sm[b], pk[b] - (opt.hold ? 0.02 : 0.6));
    redraw((n) => n + 1);
  }, [spec, opt.fall, opt.hold]);

  const range = RANGES[opt.range];
  const slope = SLOPES[opt.slope];
  const tilt = (b: number) => slope * Math.log2(bandHz(b) / 1000);
  const top = 6;
  const yOf = (db: number) => Math.max(0, Math.min(1, (top - db) / (range + top)));
  const W = 1000, H = 300;
  const path = (arr: Float32Array, off: number) => {
    let d = "";
    for (let b = 0; b < BANDS; ++b) {
      const x = xOfHz(bandHz(b)) * W, y = yOf(arr[off + b] + tilt(b)) * H;
      d += `${b ? " L" : "M"}${x.toFixed(1)} ${y.toFixed(1)}`;
    }
    return d;
  };
  const mid = path(smooth.current, 0);
  const side = path(smooth.current, BANDS);
  const pk = path(peaks.current, 0);
  const grid = useMemo(() => [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000], []);
  const hoverInfo = (() => {
    if (!hover) return null;
    const hz = F_LO * Math.pow(F_HI / F_LO, hover.x);
    const db = top - hover.y * (range + top) - slope * Math.log2(hz / 1000);
    const midi = 69 + 12 * Math.log2(hz / 440);
    return `${hz >= 1000 ? `${(hz / 1000).toFixed(2)} kHz` : `${Math.round(hz)} Hz`} · ${noteName(Math.round(midi))} · ${db.toFixed(1)} dB`;
  })();

  return (
    <div className={`spec${compact ? " spec--compact" : ""}`}>
      <div className="spec-tools">
        <b className="spec-title">Spectre du mix</b>
        <span className="spec-group">Pente
          {SLOPES.map((sl, i) => <button key={sl} type="button" className={opt.slope === i ? "on" : ""} onClick={() => setOpt({ slope: i })}
            title={sl ? `+${sl} dB par octave autour de 1 kHz : un mix équilibré paraît plat` : "Sans pente (valeurs brutes)"}>{sl ? `${String(sl).replace(".", ",")} dB` : "0"}</button>)}
        </span>
        <span className="spec-group">Retombée
          {FALLS.map((f, i) => <button key={f.label} type="button" className={opt.fall === i ? "on" : ""} onClick={() => setOpt({ fall: i })}>{f.label}</button>)}
        </span>
        <span className="spec-group">Plage
          {RANGES.map((r, i) => <button key={r} type="button" className={opt.range === i ? "on" : ""} onClick={() => setOpt({ range: i })}>{r} dB</button>)}
        </span>
        <button type="button" className={opt.side ? "on" : ""} onClick={() => setOpt({ side: !opt.side })} title="Courbe des côtés (ce qui est en stéréo)">Côtés</button>
        <button type="button" className={opt.hold ? "on" : ""} onClick={() => setOpt({ hold: !opt.hold })} title="Crêtes tenues (redescendent lentement)">Crêtes</button>
        <button type="button" onClick={() => { peaks.current.fill(-140); redraw((n) => n + 1); }} title="Effacer les crêtes tenues">Effacer</button>
        <span className="spec-read tabular">{hoverInfo ?? (spec ? "" : "En attente du son…")}</span>
      </div>
      <svg ref={box} className="spec-svg" viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" role="img" aria-label="Spectre du bus principal"
        onPointerMove={(e) => { const r = e.currentTarget.getBoundingClientRect(); setHover({ x: (e.clientX - r.left) / r.width, y: (e.clientY - r.top) / r.height }); }}
        onPointerLeave={() => setHover(null)}>
        {grid.map((hz) => <line key={hz} x1={xOfHz(hz) * W} x2={xOfHz(hz) * W} y1={0} y2={H} className={hz === 100 || hz === 1000 || hz === 10000 ? "spec-grid spec-grid--major" : "spec-grid"} vectorEffect="non-scaling-stroke" />)}
        {Array.from({ length: Math.floor((range + top) / 12) + 1 }, (_, i) => top - 6 - i * 12).filter((db) => db > -range).map((db) => (
          <line key={db} x1={0} x2={W} y1={yOf(db) * H} y2={yOf(db) * H} className="spec-grid" vectorEffect="non-scaling-stroke" />
        ))}
        <path d={`${mid} L ${W} ${H} L 0 ${H} Z`} className="spec-fill" />
        <path d={mid} className="spec-mid" vectorEffect="non-scaling-stroke" />
        {opt.side && <path d={side} className="spec-side" vectorEffect="non-scaling-stroke" />}
        {opt.hold && <path d={pk} className="spec-peak" vectorEffect="non-scaling-stroke" />}
        {hover && <line x1={hover.x * W} x2={hover.x * W} y1={0} y2={H} className="spec-cursor" vectorEffect="non-scaling-stroke" />}
      </svg>
      <div className="spec-axis" aria-hidden>
        {grid.map((hz) => <span key={hz} style={{ left: `${xOfHz(hz) * 100}%` }}>{hz >= 1000 ? `${hz / 1000}k` : hz}</span>)}
      </div>
    </div>
  );
}
