// Groovebox — OFAD — AGPL-3.0-or-later
// Micro Tonnerre (après la 1.1) : sections particulières de la façade.
//  - MtChannels : les 8 canaux en liste — le numéro joue le canal (s'allume au jeu), le nom le
//    choisit ; la forme d'onde et le dosage oscillateur / bruit du canal sont rappelés à droite ;
//  - MtShape : l'allure du canal choisi, calculée à partir de ses réglages — enveloppes de
//    l'oscillateur et du bruit (salves du mode « claquée » comprises) et courbe de hauteur ;
//  - MtKit : le kit en colonnes, comme une table de mixage (niveau, hauteur, déclin, mélange).

import { actions, setUi, useStore } from "../../engine/store";
import { toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";

const pid = (ch: number, id: string) => `p${String(ch + 1).padStart(2, "0")}_${id}`;
const WAVES = ["∿", "⋀", "⩘"];

function useChannelValues(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  return (ch: number, id: string): number => {
    const i = m.params.findIndex((p) => p.id === pid(ch, id));
    return i >= 0 ? toPlain(m.params[i], values?.[i] ?? 0) : 0;
  };
}

/** Les 8 canaux : jouer (numéro), choisir (nom). */
export function MtChannels({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const lane = useStore((s) => s.ui.lane);
  const lit = useStore((s) => s.litLanes);
  const val = useChannelValues(channel, m);
  return (
    <div className="mt-chans" role="list">
      {m.padNames.slice(0, 8).map((name, i) => {
        const mix = val(i, "mix");
        return (
          <div key={i} role="listitem" className={`mt-chan${i === lane ? " is-selected" : ""}${(lit[i] ?? 0) > 0 ? " is-lit" : ""}`}>
            <button type="button" className="mt-chan-num" title={`Jouer le canal ${i + 1}`}
              onPointerDown={(e) => { e.preventDefault(); e.currentTarget.setPointerCapture(e.pointerId); setUi({ lane: i }); actions.noteOn(channel, i, 0.9, false); }}
              onPointerUp={() => actions.noteOff(channel, i)} onPointerCancel={() => actions.noteOff(channel, i)}>{i + 1}</button>
            <button type="button" className="mt-chan-name" onClick={() => setUi({ lane: i })} title={`Régler le canal ${i + 1} : ${name}`}>{name}</button>
            <span className="mt-chan-wave" aria-hidden title="Forme d'onde de l'oscillateur">{WAVES[Math.round(val(i, "wave"))] ?? ""}</span>
            <span className="mt-chan-mix" aria-label={`Oscillateur ${Math.round(100 - mix)} %, bruit ${Math.round(mix)} %`}><span style={{ width: `${mix}%` }} /></span>
          </div>
        );
      })}
    </div>
  );
}

/** Allure du canal choisi : enveloppes (oscillateur, bruit) et hauteur, sur la durée du son. */
export function MtShape({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const lane = Math.min(7, useStore((s) => s.ui.lane));
  const val = useChannelValues(channel, m);
  const v = (id: string) => val(lane, id);
  const mix = v("mix") / 100;
  const oscG = Math.min(1, 2 * (1 - mix)), noiseG = Math.min(1, 2 * mix);
  const oatk = v("oatk"), odec = v("odec"), natk = v("natk"), ndec = v("ndec"), nenv = Math.round(v("nenv"));
  const mmode = Math.round(v("mmode")), mamt = v("mamt"), mtime = v("mtime"), mrate = v("mrate");
  // durée montrée : le plus long des deux sons (à -40 dB), bornée
  const total = Math.min(4000, Math.max(60, Math.max(oscG > 0 ? oatk + odec * 0.7 : 0, noiseG > 0 ? natk + ndec * 0.7 : 0)));
  const W = 320, H = 120, N = 160;
  const x = (t: number) => (t / total) * W;
  const env = (t: number, atk: number, dec: number, mode: number) => {
    if (mode === 2 && atk > 0 && t < atk) { const per = atk / 3; return Math.exp(-(t % per) / (0.28 * per)); }
    if (mode !== 2 && t < atk) return t / Math.max(0.001, atk);
    const td = t - (mode === 2 || atk <= 0 ? Math.max(0, atk) : atk);
    if (mode === 1) return Math.max(0, 1 - td / Math.max(1, dec));
    return Math.pow(0.001, td / Math.max(1, dec));
  };
  const line = (f: (t: number) => number) => Array.from({ length: N + 1 }, (_, k) => {
    const t = (k / N) * total;
    return `${x(t).toFixed(1)},${(H - 8 - f(t) * (H - 22)).toFixed(1)}`;
  }).join(" ");
  const pitch = (t: number) => {
    if (mamt === 0) return 0;
    const m0 = mmode === 0 ? Math.pow(0.001, t / Math.max(1, mtime)) : mmode === 1 ? Math.sin(2 * Math.PI * (Math.min(mrate, 40) * t) / 1000) : Math.sin(t * 0.37) * Math.cos(t * 0.11);
    return 0.5 + 0.5 * Math.max(-1, Math.min(1, (mamt * m0) / 48));
  };
  return (
    <div className="mt-shape">
      <svg viewBox={`0 0 ${W} ${H}`} preserveAspectRatio="none" aria-label={`Allure du canal ${lane + 1}`}>
        <line x1="0" x2={W} y1={H - 8} y2={H - 8} className="mt-shape-axis" />
        {noiseG > 0 && <polyline className="mt-shape-noise" points={line((t) => env(t, natk, ndec, nenv) * noiseG)} />}
        {oscG > 0 && <polyline className="mt-shape-osc" points={line((t) => env(t, oatk, odec, 0) * oscG)} />}
        {oscG > 0 && mamt !== 0 && <polyline className="mt-shape-pitch" points={line(pitch)} />}
      </svg>
      <p className="mt-shape-legend">
        {oscG > 0 && <span className="mt-k mt-k--osc">Oscillateur</span>}
        {noiseG > 0 && <span className="mt-k mt-k--noise">Bruit{nenv === 2 ? " (claqué)" : ""}</span>}
        {oscG > 0 && mamt !== 0 && <span className="mt-k mt-k--pitch">Modulation de hauteur</span>}
        <span className="mt-shape-time">{total >= 1000 ? `${(total / 1000).toFixed(1)} s` : `${Math.round(total)} ms`}</span>
      </p>
    </div>
  );
}

function Knob({ global, label }: { global: number; label?: string }) {
  const b = useParam(global, label);
  return <div className="fc fc--small"><Encoder binding={b} variant="knob" /></div>;
}

/** Le kit en colonnes : niveau, hauteur, déclin de l'oscillateur, mélange. */
export function MtKit({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const lane = useStore((s) => s.ui.lane);
  const lit = useStore((s) => s.litLanes);
  const ids: [string, string][] = [["level", "Niveau"], ["freq", "Fréquence"], ["odec", "Déclin osc."], ["ndec", "Déclin bruit"], ["mix", "Osc. / bruit"], ["pan", "Pan"]];
  return (
    <div className="mt-kit">
      {m.padNames.slice(0, 8).map((name, i) => (
        <div key={i} className={`mt-kitcol kind--${m.padKinds[i] ?? "sample"}${i === lane ? " is-selected" : ""}${(lit[i] ?? 0) > 0 ? " is-lit" : ""}`}>
          <button type="button" className="mt-kitcol-name" onClick={() => setUi({ lane: i })} title={`Choisir ${name}`}><b>{i + 1}</b> {name}</button>
          {ids.map(([id, label]) => {
            const k = m.params.findIndex((p) => p.id === pid(i, id));
            return k >= 0 ? <Knob key={id} global={paramAddress(channel, "inst", 0, k)} label={label} /> : null;
          })}
        </div>
      ))}
    </div>
  );
}
