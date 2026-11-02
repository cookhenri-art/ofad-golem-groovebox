// Groovebox — OFAD — AGPL-3.0-or-later
// Percussion à séquenceur : les 8 pas (une rangée de hauteurs, une rangée de vélocités) et la
// diode du pas joué, lue dans l'état de l'instrument. Le séquenceur avance d'un pas à chaque note.

import { actions, useStore } from "../../engine/store";
import { toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";

function StepKnob({ channel, index, label }: { channel: number; index: number; label: string }) {
  const b = useParam(index >= 0 ? paramAddress(channel, "inst", 0, index) : null, label);
  return <div className="fc fc--knob"><Encoder binding={b} variant="knob" /></div>;
}

export function DfamSeqPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const values = useStore((s) => s.channels[channel]?.inst);
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const lenIdx = idx("seqlen"), modeIdx = idx("seqmode");
  const len = lenIdx >= 0 && values ? Math.round(toPlain(m.params[lenIdx], values[lenIdx] ?? 1)) : 8;
  const noteMode = modeIdx >= 0 && values ? Math.round(toPlain(m.params[modeIdx], values[modeIdx] ?? 0)) === 1 : false;
  const now = Math.round(status[0] ?? -1);
  return (
    <div className={`dfamseq${noteMode ? " is-note" : ""}`}>
      <div className="dfamseq-grid" role="group" aria-label="Séquenceur 8 pas">
        {Array.from({ length: 8 }, (_, k) => (
          <div key={k} className={`dfamseq-step${k === now ? " now" : ""}${k >= len ? " off" : ""}`}>
            <span className="dfamseq-led" aria-hidden />
            <span className="dfamseq-num">{k + 1}</span>
            <StepKnob channel={channel} index={idx(`p${k + 1}`)} label={`Hauteur ${k + 1}`} />
            <StepKnob channel={channel} index={idx(`v${k + 1}`)} label={`Vélocité ${k + 1}`} />
          </div>
        ))}
      </div>
      <p className="dfamseq-hint">{noteMode
        ? "Mode note jouée : la hauteur suit la note, la vélocité suit le jeu."
        : "Rangée du haut : hauteur ; rangée du bas : vélocité (dose les trois enveloppes). Le pas avance à chaque note reçue et repart du 1 au lancement de la lecture."}</p>
    </div>
  );
}

/** Transport de la percussion (v0.26) : déclencher un coup (le pas suivant), marche / arrêt de la lecture. */
export function DfamTransport({ channel }: { channel: number; module: ModuleDesc }) {
  const playing = useStore((s) => s.tick.t.playing);
  const trigger = () => { actions.noteOn(channel, 60, 1); window.setTimeout(() => actions.noteOff(channel, 60), 120); };
  return (
    <div className="dfam-transport">
      <button type="button" className="dfam-btn" onClick={trigger} title="Joue un coup : le séquenceur avance d'un pas">Déclencher</button>
      <button type="button" className={`dfam-btn dfam-btn--run${playing ? " on" : ""}`} aria-pressed={playing} onClick={() => actions.togglePlay()}
        title="Marche / arrêt de la lecture (toutes les pistes)">Marche / arrêt</button>
    </div>
  );
}
