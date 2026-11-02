// Groovebox — OFAD — AGPL-3.0-or-later
// Batterie Impact (v0.26) : façade disposée comme la boîte compacte dont elle reprend le principe —
// une bande par instrument (les deux toms, cymbale et cloche, les deux charleys partagent la leur),
// chacune avec son bouton COULEUR à voyant et son NIVEAU en bas ; la sortie (distorsion, accent)
// à droite. Inscriptions en grands caractères.

import { actions, setUi, useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, steppedBinding } from "../../controls/bindings";
import { BigKnob, BigKnobView } from "./BigKnob";

type K = [pad: number, id: string, label: string];
interface Strip { name: string; pads: number[]; knobs: K[]; levels: K[]; color: number[] }

const STRIPS: Strip[] = [
  { name: "Grosse caisse", pads: [0], knobs: [[0, "tune", "Accord"], [0, "decay", "Déclin"], [0, "tone", "Balayage"]], levels: [[0, "level", "Niveau"]], color: [0] },
  { name: "Caisse claire 1", pads: [1], knobs: [[1, "tune", "Accord"], [1, "tone", "Timbre"], [1, "snap", "Claquant"]], levels: [[1, "level", "Niveau"]], color: [1] },
  { name: "Caisse claire 2", pads: [2], knobs: [[2, "decay", "Déclin"], [2, "tone", "Filtre"], [2, "snap", "Netteté"]], levels: [[2, "level", "Niveau"]], color: [2] },
  { name: "Toms", pads: [3, 4], knobs: [[3, "tune", "Aigu"], [4, "tune", "Grave"], [3, "decay", "Déclin"]], levels: [[3, "level", "Aigu"], [4, "level", "Grave"]], color: [3, 4] },
  { name: "Cymbale · cloche", pads: [5, 6], knobs: [[5, "decay", "Cymbale"], [5, "tone", "Timbre"], [6, "tune", "Cloche"]], levels: [[5, "level", "Cymb."], [6, "level", "Cloche"]], color: [5] },
  { name: "Charleys", pads: [7, 8], knobs: [[7, "tone", "Timbre"], [7, "decay", "Fermé"], [8, "decay", "Ouvert"]], levels: [[7, "level", "Fermé"], [8, "level", "Ouvert"]], color: [7, 8] },
  { name: "Tambour FM", pads: [9], knobs: [[9, "tune", "Porteuse"], [9, "tone", "Qté FM"], [9, "decay", "Déclin"]], levels: [[9, "level", "Niveau"]], color: [9] },
  { name: "Colorés · éch.", pads: [10, 11, 12, 13, 14, 15], knobs: [[10, "level", "G.C. col."], [11, "level", "C.C. col."], [12, "level", "FM col."]], levels: [[13, "level", "Éch. 1"], [14, "level", "Éch. 2"], [15, "level", "Éch. 3"]], color: [] },
];

const pid = (pad: number, id: string) => `p${String(pad + 1).padStart(2, "0")}_${id}`;

export function ImpactStrips({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const lane = useStore((s) => s.ui.lane);
  const lit = useStore((s) => s.litLanes);
  const values = useStore((s) => s.channels[channel]?.inst);
  const idx = (pad: number, id: string) => m.params.findIndex((p) => p.id === pid(pad, id));
  const g = (i: number) => (i >= 0 ? paramAddress(channel, "inst", 0, i) : null);
  // COULEUR : le dernier réglage de la voix ; le bouton le met à fond ou l'éteint (la valeur fine
  // reste réglable dans « Pad choisi »)
  const colorOn = (pads: number[]) => pads.some((p) => {
    const i = idx(p, "x2");
    return i >= 0 && values ? toPlain(m.params[i], values[i] ?? 0) > 50 : false;
  });
  const toggleColor = (pads: number[]) => {
    const on = colorOn(pads);
    for (const p of pads) { const i = idx(p, "x2"); if (i >= 0) actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(m.params[i], on ? 0 : 100)); }
  };
  return (
    <div className="imp-strips">
      {STRIPS.map((s) => {
        const selected = s.pads.includes(lane);
        const isLit = s.pads.some((p) => (lit[p] ?? 0) > 0);
        return (
          <div key={s.name} className={`imp-strip${selected ? " is-selected" : ""}`}>
            <button type="button" className={`imp-strip-name${isLit ? " is-lit" : ""}`} onClick={() => setUi({ lane: s.pads[0] })} title={`Choisir ${s.name}`}>{s.name}</button>
            <div className="imp-strip-knobs">
              {s.knobs.map(([p, id, label]) => <BigKnob key={`${p}${id}`} global={g(idx(p, id))} label={label} size={1} />)}
            </div>
            {s.color.length > 0 ? (
              <button type="button" className={`imp-color${colorOn(s.color) ? " on" : ""}`} aria-pressed={colorOn(s.color)} onClick={() => toggleColor(s.color)}
                title="Couleur : variante de la voix (surmultiplication, claquant, timbre…)">
                <span className="fc-led" aria-hidden />Couleur
              </button>
            ) : <span className="imp-color imp-color--none" />}
            <div className={`imp-strip-levels n${s.levels.length}`}>
              {s.levels.map(([p, id, label]) => <BigKnob key={`${p}${id}`} global={g(idx(p, id))} label={label} size={s.levels.length === 1 ? 1.35 : s.levels.length === 2 ? 1 : 0.8} className="imp-level" />)}
            </div>
          </div>
        );
      })}
    </div>
  );
}

/** Sortie du kit : distorsion et accent. */
export function ImpactOut({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const i = m.params.findIndex((p) => p.id === "kitdist");
  const accent = useStore((s) => s.pattern?.accent ?? 50);
  const acc = steppedBinding("Accent", Array.from({ length: 21 }, (_, k) => k * 5), Math.round(accent / 5) * 5, (a) => void actions.seq("seqAccent", a), (a) => `${a} %`);
  return (
    <div className="imp-out">
      {i >= 0 && <BigKnob global={paramAddress(channel, "inst", 0, i)} label="Distorsion" size={1.6} />}
      <BigKnobView binding={acc} label="Accent" size={1.15} />
    </div>
  );
}
