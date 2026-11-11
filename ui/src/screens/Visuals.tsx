// Groovebox — OFAD — AGPL-3.0-or-later
// Visuels des écrans SON et EFFETS : enveloppes, réponse des filtres, formes d'onde, courbes de
// saturation et de compression, réponse de l'égaliseur, sources de modulation, échantillons.
// Tout est calculé à partir des valeurs réelles des réglages.

import { shapeOf } from "./distShape";

const W = 330, H = 150;

// 1.1 : enveloppes, filtres et formes d'onde de l'écran SON : voir CurveEditors.tsx (réglables).

/** Courbe de transfert d'un algorithme de distorsion. */
export function DistCurve({ type, drive, bias = 0 }: { type: number; drive: number; bias?: number }) {
  const pts: string[] = [];
  for (let i = 0; i <= 80; ++i) {
    const x = -1 + (i / 80) * 2;
    const y = Math.max(-1, Math.min(1, shapeOf(type, x, drive, bias)));
    pts.push(`${(i / 80) * 100},${50 - y * 46}`);
  }
  return (
    <svg viewBox="0 0 100 100" preserveAspectRatio="none" className="dist-curve" aria-hidden>
      <line x1={0} y1={50} x2={100} y2={50} className="grid" />
      <line x1={50} y1={0} x2={50} y2={100} className="grid" />
      <line x1={0} y1={96} x2={100} y2={4} className="unity" />
      <path d={"M" + pts.join(" L")} className="curve" />
    </svg>
  );
}

/** Courbe entrée → sortie d'un compresseur (dB), avec la réduction de gain en cours. */
export function CompCurve({ threshold, ratio, knee, gr }: { threshold: number; ratio: number; knee: number; gr: number }) {
  const toX = (db: number) => ((db + 60) / 60) * 100;
  const pts: string[] = [];
  for (let db = -60; db <= 0; db += 1) {
    const over = db - threshold;
    let out = db;
    if (knee > 0 && Math.abs(over) <= knee / 2) out = db + ((1 / ratio - 1) * Math.pow(over + knee / 2, 2)) / (2 * knee);
    else if (over > 0) out = threshold + over / ratio;
    pts.push(`${toX(db)},${100 - toX(out)}`);
  }
  return (
    <div className="transfer" aria-hidden>
      <svg viewBox="0 0 100 100" preserveAspectRatio="none">
        {[25, 50, 75].map((g) => <g key={g}><line x1={g} x2={g} y1={0} y2={100} className="grid" /><line y1={g} y2={g} x1={0} x2={100} className="grid" /></g>)}
        <line x1={0} y1={100} x2={100} y2={0} className="unity" />
        <path className="curve" d={"M" + pts.join(" L")} />
      </svg>
      <div className="transfer-legend">
        <span>Entrée → sortie</span>
        <span>Seuil <b className="tabular">{Math.round(threshold)} dB</b></span>
        <span>Rapport <b className="tabular">{ratio >= 20 ? "∞" : ratio.toFixed(1)}:1</b></span>
      </div>
      <div className="gr-meter gr-meter--inline">
        <span className="detail-label">Réduction</span>
        <div className="gr-bar"><span style={{ width: `${Math.min(100, (gr / 24) * 100)}%` }} /></div>
        <span className="gr-val tabular">−{gr.toFixed(1)} dB</span>
      </div>
    </div>
  );
}

/** Réponse en fréquence de plusieurs courbes (dB de -36 à +18), avec légende. */
export interface ResponseCurve { db: (f: number) => number; color: string; label: string; off?: boolean }
export function ResponseView({ curves, marks = [] }: { curves: ResponseCurve[]; marks?: number[] }) {
  const N = 160;
  const y = (db: number) => H * (18 - Math.max(-36, Math.min(18, db))) / 54;
  return (
    <div className="response" aria-hidden>
      <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve" preserveAspectRatio="none">
        {[100, 1000, 10000].map((f) => { const x = (Math.log10(f / 20) / 3) * W; return <line key={f} x1={x} x2={x} y1={0} y2={H} className="grid" />; })}
        <line x1={0} x2={W} y1={y(0)} y2={y(0)} className="grid" />
        {marks.map((f, i) => { const x = (Math.log10(Math.max(20, f) / 20) / 3) * W; return <line key={i} x1={x} x2={x} y1={0} y2={H} className="unity" />; })}
        {curves.map((c, k) => {
          const pts: string[] = [];
          for (let i = 0; i <= N; ++i) { const f = 20 * Math.pow(1000, i / N); pts.push(`${(i / N) * W},${y(c.db(f))}`); }
          return <polyline key={k} points={pts.join(" ")} fill="none" stroke={c.color} strokeWidth={2} vectorEffect="non-scaling-stroke" opacity={c.off ? 0.35 : 1} strokeDasharray={c.off ? "4 4" : undefined} />;
        })}
      </svg>
      <div className="transfer-legend">
        {curves.map((c, k) => <span key={k}><i className="legend-dot" style={{ background: c.color }} /> {c.label}</span>)}
      </div>
    </div>
  );
}

/** Filtre à 2 pôles (variable d'état) : 1 passe-bas, 2 passe-haut, 3 passe-bande, 4 coupe-bande. */
export function svfDb(type: number, f: number, fc: number, q: number): number {
  const r = f / Math.max(1, fc);
  const den = Math.sqrt(Math.pow(1 - r * r, 2) + Math.pow(r / q, 2));
  const num = type === 1 ? 1 : type === 2 ? r * r : type === 3 ? r / q : type === 4 ? Math.abs(1 - r * r) : den;
  return 20 * Math.log10(Math.max(1e-5, num / den));
}

/** Porte : courbe entrée → sortie (-80 à 0 dB), seuil d'ouverture et de fermeture (hystérésis). */
export function GateCurve({ threshold, range, hyst, gr, off }: { threshold: number; range: number; hyst: number; gr: number; off: boolean }) {
  const toX = (db: number) => ((db + 80) / 80) * 100;
  const pts: string[] = [];
  for (let db = -80; db <= 0; db += 1) {
    const out = off || db >= threshold ? db : Math.max(-80, db - range);
    pts.push(`${toX(db)},${100 - toX(out)}`);
  }
  const close = threshold - hyst;
  return (
    <div className="transfer" aria-hidden>
      <svg viewBox="0 0 100 100" preserveAspectRatio="none">
        {[25, 50, 75].map((g) => <g key={g}><line x1={g} x2={g} y1={0} y2={100} className="grid" /><line y1={g} y2={g} x1={0} x2={100} className="grid" /></g>)}
        <line x1={0} y1={100} x2={100} y2={0} className="unity" />
        {!off && hyst > 0 && <line x1={toX(close)} x2={toX(close)} y1={0} y2={100} className="grid" strokeDasharray="2 2" />}
        <path className="curve" d={"M" + pts.join(" L")} />
      </svg>
      <div className="transfer-legend">
        <span>Entrée → sortie</span>
        <span>Seuil <b className="tabular">{off ? "coupé" : `${Math.round(threshold)} dB`}</b></span>
        <span>Atténuation <b className="tabular">{Math.round(range)} dB</b></span>
      </div>
      <div className="gr-meter gr-meter--inline">
        <span className="detail-label">Porte</span>
        <div className="gr-bar"><span style={{ width: `${Math.min(100, (gr / Math.max(1, range)) * 100)}%` }} /></div>
        <span className="gr-val tabular">−{gr.toFixed(1)} dB</span>
      </div>
    </div>
  );
}

/** Dé-esseur : bande traitée (au-dessus de la fréquence, ou tout le spectre en mode large) et réduction en cours. */
export function DeessView({ hz, gr, max, wide, listen }: { hz: number; gr: number; max: number; wide: boolean; listen: boolean }) {
  const pts: string[] = [];
  const depth = Math.max(gr, max * 0.35);
  for (let i = 0; i <= 120; ++i) {
    const f = 20 * Math.pow(1000, i / 120);
    const r = f / Math.max(20, hz);
    const hi = 1 / Math.sqrt(1 + Math.pow(1 / r, 4));
    const db = listen ? Math.max(-36, 20 * Math.log10(Math.max(1e-4, hi))) : wide ? -depth : 20 * Math.log10(1 - hi + hi * Math.pow(10, -depth / 20));
    pts.push(`${(i / 120) * W},${H * 0.15 - (db / 40) * H * 0.85}`);
  }
  const x = (Math.log10(hz / 20) / 3) * W;
  return (
    <div className="transfer" aria-hidden>
      <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve" preserveAspectRatio="none">
        {[100, 1000, 10000].map((f) => { const gx = (Math.log10(f / 20) / 3) * W; return <line key={f} x1={gx} x2={gx} y1={0} y2={H} className="grid" />; })}
        <line x1={x} x2={x} y1={0} y2={H} className="unity" />
        <polyline points={pts.join(" ")} className="stroke" />
      </svg>
      <div className="transfer-legend">
        <span>{listen ? "Écoute de la bande détectée" : wide ? "Mode large" : "Bande haute"}</span>
        <span>Fréquence <b className="tabular">{hz >= 1000 ? `${(hz / 1000).toFixed(1)} kHz` : `${Math.round(hz)} Hz`}</b></span>
      </div>
      <div className="gr-meter gr-meter--inline">
        <span className="detail-label">Sifflantes</span>
        <div className="gr-bar"><span style={{ width: `${Math.min(100, (gr / Math.max(1, max)) * 100)}%` }} /></div>
        <span className="gr-val tabular">−{gr.toFixed(1)} dB</span>
      </div>
    </div>
  );
}

export interface EqBand { on: boolean; type: number; freq: number; gain: number; q: number }

/** Réponse d'une bande d'égaliseur en dB (approximation des formules des filtres). */
export function eqBandDb(b: EqBand, f: number): number {
  if (!b.on) return 0;
  const r = f / b.freq;
  const lr = Math.log2(r);
  const bw = 1 / Math.max(0.1, b.q);
  switch (b.type) {
    case 0: return b.gain * Math.exp(-Math.pow(lr / (bw * 0.75), 2));                    // cloche
    case 1: return b.gain / (1 + Math.pow(r, 2.2));                                       // plateau bas
    case 2: return b.gain / (1 + Math.pow(1 / r, 2.2));                                   // plateau haut
    case 3: return -10 * Math.log10(1 + Math.pow(1 / r, 4));                              // coupe-bas 12
    case 4: return -10 * Math.log10(1 + Math.pow(1 / r, 8));
    case 5: return -10 * Math.log10(1 + Math.pow(r, 4));                                  // coupe-haut 12
    case 6: return -10 * Math.log10(1 + Math.pow(r, 8));
    case 8: return -10 * Math.log10(1 + Math.pow(1 / r, 12));                             // coupe-bas 36
    case 9: return -10 * Math.log10(1 + Math.pow(1 / r, 16));                             // coupe-bas 48
    case 10: return -10 * Math.log10(1 + Math.pow(r, 12));                                // coupe-haut 36
    case 11: return -10 * Math.log10(1 + Math.pow(r, 16));                                // coupe-haut 48
    default: return -30 * Math.exp(-Math.pow(lr / (bw * 0.2), 2));                        // coupe-bande
  }
}

/** Réponse de l'égaliseur (somme des bandes, approximation des formules des filtres). */
export function EqCurve({ bands, selected, spectrum }: { bands: EqBand[]; selected: number; spectrum?: number[] }) {
  const N = 160;
  const pts: string[] = [];
  for (let i = 0; i <= N; ++i) {
    const f = 20 * Math.pow(1000, i / N);
    const db = bands.reduce((acc, b) => acc + eqBandDb(b, f), 0);
    pts.push(`${(i / N) * W},${H / 2 - (Math.max(-24, Math.min(24, db)) / 24) * (H / 2 - 6)}`);
  }
  return (
    <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve eq-curve" preserveAspectRatio="none" aria-hidden>
      {spectrum && spectrum.length > 1 && (
        // analyseur : spectre de la sortie de l'égaliseur, de -90 à 0 dB sur toute la hauteur
        <path className="spectrum" d={`M0 ${H} ` + spectrum.map((db, i) => `L${((i + 0.5) / spectrum.length) * W} ${H - Math.max(0, Math.min(1, (db + 90) / 90)) * H}`).join(" ") + ` L${W} ${H} Z`} />
      )}
      {[100, 1000, 10000].map((f) => { const x = (Math.log10(f / 20) / 3) * W; return <line key={f} x1={x} x2={x} y1={0} y2={H} className="grid" />; })}
      <line x1={0} x2={W} y1={H / 2} y2={H / 2} className="grid" />
      <polyline points={pts.join(" ")} className="stroke" />
      {bands.map((b, i) => b.on && (
        <circle key={i} cx={(Math.log10(b.freq / 20) / 3) * W} cy={H / 2 - (Math.max(-24, Math.min(24, b.type <= 2 ? b.gain : 0)) / 24) * (H / 2 - 6)}
          r={i === selected ? 6 : 4} className={`eq-node${i === selected ? " on" : ""}`} />
      ))}
    </svg>
  );
}

/** Forme d'onde d'un échantillon, avec la zone lue (début / fin en fractions). */
export function SampleView({ peaks, start, end, zones }: { peaks: number[] | null; start?: number; end?: number; zones?: [number, number][] }) {
  if (!peaks) return <div className="sample-empty">Aucun échantillon · déposez un fichier audio sur un pad ou utilisez « Charger »</div>;
  const n = peaks.length / 2;
  let d = "";
  for (let i = 0; i < n; ++i) {
    const x = (i / n) * W;
    d += `M${x.toFixed(1)} ${(H / 2 - peaks[i * 2 + 1] * (H / 2 - 4)).toFixed(1)} L${x.toFixed(1)} ${(H / 2 - peaks[i * 2] * (H / 2 - 4)).toFixed(1)} `;
  }
  return (
    <svg viewBox={`0 0 ${W} ${H}`} className="filter-curve sample-view" preserveAspectRatio="none" aria-hidden>
      {zones?.map(([a, b], i) => <rect key={i} x={a * W} y={0} width={Math.max(0, (b - a) * W)} height={H} className={`zone${i % 2 ? " zone--odd" : ""}`} />)}
      {start !== undefined && end !== undefined && <rect x={Math.min(start, end) * W} y={0} width={Math.abs(end - start) * W} height={H} className="zone zone--on" />}
      <path d={d} className="wave-stroke" />
    </svg>
  );
}

/** Aperçu d'une source de modulation (LFO, fonction, suiveur, séquenceur). */
export function ModSourceView({ type, shape, steps, levels, color, live }: { type: number; shape: number; steps: number[]; levels: number[]; color: string; live?: [number, number] }) {
  const w = 120, h = 40;
  // valeur en cours (jauge à droite) : bipolaire pour le LFO
  const meter = live ? (type === 0
    ? <rect x={w - 3} y={Math.min(h / 2, h / 2 - live[0] * (h / 2 - 2))} width={3} height={Math.max(1, Math.abs(live[0]) * (h / 2 - 2))} className="mod-live" />
    : <rect x={w - 3} y={h - 2 - Math.max(0, Math.min(1, live[0])) * (h - 4)} width={3} height={Math.max(1, Math.max(0, Math.min(1, live[0])) * (h - 4))} className="mod-live" />) : null;
  if (type === 3) {
    const here = live ? live[1] : -1;
    return (
      <svg viewBox={`0 0 ${w} ${h}`} className="mod-visual" preserveAspectRatio="none" aria-hidden>
        {steps.map((v, i) => <rect key={i} x={(i / 16) * (w - 6) + 1} y={h - v * (h - 4)} width={(w - 6) / 16 - 2} height={v * (h - 4)} rx={1}
          style={{ fill: color, opacity: here < 0 || i === here ? 1 : 0.45 }} />)}
        {here >= 0 && <rect x={(here / 16) * (w - 6) + 0.5} y={0.5} width={(w - 6) / 16 - 1} height={h - 1} rx={1} className="mod-here" />}
        {meter}
      </svg>
    );
  }
  let d = "";
  if (type === 0) {
    const pts: string[] = [];
    for (let i = 0; i <= 60; ++i) {
      const ph = (i / 60) * 2 % 1;
      const v = shape === 0 ? Math.sin(2 * Math.PI * ph) : shape === 1 ? 1 - 4 * Math.abs(ph - 0.5) : shape === 2 ? 2 * ph - 1 : shape === 3 ? (ph < 0.5 ? 1 : -1) : Math.sin(i * 1.7) * Math.cos(i * 0.6);
      pts.push(`${(i / 60) * w},${h / 2 - v * (h / 2 - 4)}`);
    }
    d = "M" + pts.join(" L");
  } else if (type === 1) {
    const [l1, l2, l3, l4] = levels;
    d = `M0 ${h - 4} L${w * 0.15} ${h - 4 - l1 * (h - 8)} L${w * 0.4} ${h - 4 - l2 * (h - 8)} L${w * 0.65} ${h - 4 - l3 * (h - 8)} L${w} ${h - 4 - l4 * (h - 8)}`;
  } else {
    d = `M0 ${h - 4} L${w * 0.08} 6 C${w * 0.3} 10, ${w * 0.35} ${h * 0.7}, ${w * 0.55} ${h * 0.62} L${w * 0.62} 12 C${w * 0.8} 14, ${w * 0.85} ${h - 6}, ${w} ${h - 4}`;
  }
  return (
    <svg viewBox={`0 0 ${w} ${h}`} className="mod-visual" preserveAspectRatio="none" aria-hidden>
      <path d={d} style={{ stroke: color }} />
      {meter}
    </svg>
  );
}

/** Mastering (v0.31) : réduction de chaque bande, du maximiseur, écrêtage et sonie de sortie. */
export function MasterMeters({ meters }: { meters: number[] }) {
  const rows: [string, number][] = [
    ["Grave", meters[3] ?? 0], ["Médium", meters[4] ?? 0], ["Aigu", meters[5] ?? 0],
    ["Maximiseur", meters[0] ?? 0], ["Écrêtage", meters[7] ?? 0],
  ];
  const lufs = meters[6] ?? -100;
  return (
    <div className="master-meters" role="img" aria-label="Réductions de gain du mastering">
      {rows.map(([name, db]) => (
        <div key={name} className="master-meter">
          <span className="master-meter-name">{name}</span>
          <span className="master-meter-bar"><i style={{ width: `${Math.min(100, (Math.max(0, db) / 12) * 100)}%` }} /></span>
          <span className="master-meter-val tabular">{db > 0.05 ? `−${db.toFixed(1)} dB` : "0 dB"}</span>
        </div>
      ))}
      <div className="master-lufs tabular">Sonie ≈ {lufs > -70 ? `${lufs.toFixed(1)} LUFS` : "—"} <small>(court terme, approchée)</small></div>
    </div>
  );
}
