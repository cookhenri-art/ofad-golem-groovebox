// Groovebox — OFAD — AGPL-3.0-or-later
// Rangée de pas d'une basse à séquenceur (principe des basses à séquenceur des années 80) :
// pour chacun des 16 pas de la page, la note, et les fonctions ACCENT et GLISSÉ, avec
// l'octave haut / bas. Toucher la case pose ou retire la note ; la tirer vers le haut ou le bas
// change la note d'un demi-ton ; ▲ ▼ changent d'octave. C'est la séquence de la piste (ligne 1).

import { useRef } from "react";
import { actions, getState, setUi, useStore } from "../../engine/store";
import { STEP_ACCENT, STEP_SLIDE } from "../../engine/protocol";
import { noteName } from "../../engine/params";
import type { EncoderBinding } from "../../controls/Encoder";
import { useValueDrag } from "../../controls/useValueDrag";

/** Durées proposées (en huitièmes de pas, comme le moteur) : 1/4 de pas à 16 pas. */
const LENS = [2, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 256, 512, 1024];
const lenText = (l8: number) => (l8 % 8 === 0 ? `${l8 / 8}` : `${(l8 / 8).toFixed(2).replace(/0$/, "")}`);

/** Petite valeur réglée en glissant (vélocité, durée d'un pas). */
function StepValue({ label, text, binding, disabled }: { label: string; text: string; binding: EncoderBinding; disabled: boolean }) {
  const { handlers } = useValueDrag(disabled ? null : binding);
  return (
    <span className={`step303-val${disabled ? " off" : ""}`} role="slider" aria-label={label} aria-valuetext={text} title={`${label} : ${text} — glisser vers le haut ou le bas`} {...handlers}>{text}</span>
  );
}

export function StepStrip() {
  const pattern = useStore((s) => s.pattern);
  const page = useStore((s) => s.ui.page);
  const pos = useStore((s) => s.tick.pos);
  const playing = useStore((s) => s.tick.t.playing);
  const drag = useRef<{ id: number; y: number; step: number; note: number; moved: boolean } | null>(null);
  if (!pattern) return null;
  const lane = pattern.lanes[0];
  const pages = Math.max(1, Math.ceil(pattern.length / 16));
  const pg = Math.min(page, pages - 1);
  const first = pg * 16;
  const flag = (s: number, bit: number) => ((pattern.flags[s] ?? 0) & bit) !== 0;
  const toggleFlag = (s: number, bit: number) => void actions.seq("seqStepsFlag", JSON.stringify([s]), bit, !flag(s, bit));
  const setNote = (s: number, n: number) => {
    const note = Math.max(0, Math.min(127, n));
    void actions.seq("seqSetNote", 0, s, note);
    if ((getState().pattern?.lanes[0]?.vel[s] ?? 0) > 0) { actions.noteOn(pattern.track, note, 0.7); window.setTimeout(() => actions.noteOff(pattern.track, note), 140); }
  };
  return (
    <div className="steps303">
      <div className="steps303-pages">
        {Array.from({ length: pages }, (_, i) => (
          <button key={i} type="button" className={i === pg ? "on" : ""} onClick={() => setUi({ page: i })}>{i + 1}</button>
        ))}
      </div>
      <div className="steps303-row">
        {Array.from({ length: 16 }, (_, i) => {
          const s = first + i;
          const out = s >= pattern.length;
          const on = !out && (lane?.vel[s] ?? 0) > 0;
          const note = lane?.note[s] ?? 48;
          const here = playing && (pos[0] ?? -1) === s;
          return (
            <div key={i} className={`step303${on ? " on" : ""}${out ? " out" : ""}${here ? " here" : ""}${i % 4 === 0 ? " beat" : ""}`}>
              <button type="button" className="step303-key" disabled={out} aria-pressed={on}
                aria-label={`Pas ${s + 1}${on ? `, ${noteName(note)}` : ", silence"} (tirer : note)`}
                onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); drag.current = { id: e.pointerId, y: e.clientY, step: s, note, moved: false }; }}
                onPointerMove={(e) => {
                  const d = drag.current;
                  if (!d || d.id !== e.pointerId) return;
                  const k = Math.trunc((d.y - e.clientY) / 10);
                  if (k === 0) return;
                  d.moved = true; d.y -= k * 10; d.note += k;
                  setNote(d.step, d.note);
                }}
                onPointerUp={() => {
                  const d = drag.current;
                  drag.current = null;
                  if (d && !d.moved) void actions.seq("seqSetStep", 0, s, on ? 0 : 100);
                }}
                onWheel={(e) => setNote(s, note + (e.deltaY < 0 ? 1 : -1))}>
                <span className="step303-num">{s + 1}</span>
                <span className="step303-note">{on ? noteName(note) : "—"}</span>
              </button>
              <div className="step303-fn">
                <button type="button" disabled={out} title="Octave plus haut" onClick={() => setNote(s, note + 12)}>▲</button>
                <button type="button" disabled={out} title="Octave plus bas" onClick={() => setNote(s, note - 12)}>▼</button>
              </div>
              <div className="step303-vals">
                <StepValue label={`Vélocité du pas ${s + 1}`} text={on ? String(lane?.vel[s] ?? 100) : "—"} disabled={!on}
                  binding={{ kind: "continuous", label: "Vélocité", text: String(lane?.vel[s] ?? 100), normalised: ((lane?.vel[s] ?? 100) - 1) / 126,
                    onChange: (n) => void actions.seq("seqSetStep", 0, s, Math.max(1, Math.min(127, Math.round(1 + n * 126)))) }} />
                <StepValue label={`Durée du pas ${s + 1} (en pas)`} text={on ? lenText(lane?.len[s] ?? 8) : "—"} disabled={!on}
                  binding={{ kind: "stepped", label: "Durée", text: lenText(lane?.len[s] ?? 8), normalised: 0,
                    onStep: (d) => { const cur = lane?.len[s] ?? 8; const i = LENS.findIndex((x) => x >= cur); const k = Math.max(0, Math.min(LENS.length - 1, (i < 0 ? LENS.length - 1 : i) + d)); void actions.seq("seqSetLength8", 0, s, LENS[k]); } }} />
              </div>
              <div className="step303-fn">
                <button type="button" disabled={out} className={flag(s, STEP_ACCENT) ? "acc on" : "acc"} aria-pressed={flag(s, STEP_ACCENT)} onClick={() => toggleFlag(s, STEP_ACCENT)}>Acc.</button>
                <button type="button" disabled={out} className={flag(s, STEP_SLIDE) ? "sld on" : "sld"} aria-pressed={flag(s, STEP_SLIDE)} onClick={() => toggleFlag(s, STEP_SLIDE)}>Gl.</button>
              </div>
            </div>
          );
        })}
      </div>
    </div>
  );
}
