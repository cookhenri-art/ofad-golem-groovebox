// Groovebox — OFAD — AGPL-3.0-or-later
// Rangée de pas d'une basse à séquenceur (principe des basses à séquenceur des années 80) :
// pour chacun des 16 pas de la page, la note, et les fonctions ACCENT et GLISSÉ, avec
// l'octave haut / bas. Toucher la case pose ou retire la note ; la tirer vers le haut ou le bas
// change la note d'un demi-ton ; ▲ ▼ changent d'octave. C'est la séquence de la piste (ligne 1).

import { useRef, useState } from "react";
import { StepAmountEditor, useLongPress, type StepAmountKind } from "../../controls/StepAmount";
import { actions, getState, setUi, stepLanes, useStore } from "../../engine/store";
import { STEP_ACCENT, STEP_SLIDE } from "../../engine/protocol";
import { noteName } from "../../engine/params";
import type { EncoderBinding } from "../../controls/Encoder";
import { useValueDrag } from "../../controls/useValueDrag";
import { OctaStepEdit } from "./OctaPanel";
import { noteSpans, spanClass } from "./noteSpans";
import { stepInBar, stepLabel } from "../../engine/steps"; // 1.1 : pas 1 à 16 dans chaque mesure

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

/** v0.46 fix27 : Acc. / Gl. : toucher = poser / retirer ; appui long (clic droit) = la valeur du pas. */
function FnButton({ disabled, on, cls, text, amount, onToggle, onEdit }: {
  disabled: boolean; on: boolean; cls: string; text: string; amount: number; onToggle: () => void; onEdit: () => void;
}) {
  const lp = useLongPress(onEdit);
  return (
    <button type="button" disabled={disabled} className={`${cls}${on ? " on" : ""}`} aria-pressed={on}
      title={`${text} ${on && amount > 0 ? `${amount} % ` : ""}— toucher : poser / retirer ; appui long : la valeur`}
      {...lp.handlers} onClick={() => { if (lp.consumed()) return; onToggle(); }}>
      {text}{on && amount > 0 ? <small className="fn-amt">{amount}</small> : null}
    </button>
  );
}

/** lane : v0.49 — instrument à pistes internes (FM Player) : la rangée montre la piste T choisie (ligne = T),
    une note par pas ; sans : ligne 1 d'un synthé et ses notes d'accord. */
export function StepStrip({ lane: laneMode = false }: { lane?: boolean } = {}) {
  const T = useStore((s) => (laneMode ? Math.max(0, s.ui.lane) : 0));
  const pattern = useStore((s) => s.pattern);
  const page = useStore((s) => s.ui.page);
  const pos = useStore((s) => s.tick.pos);
  const playing = useStore((s) => s.tick.t.playing);
  const drag = useRef<{ id: number; y: number; step: number; note: number; moved: boolean } | null>(null);
  // v0.46 fix27 : valeur du pas (force de l'accent, durée du glissé) : appui long sur Acc. / Gl.
  const [edit, setEdit] = useState<{ step: number; kind: StepAmountKind } | null>(null);
  const played = useStore((st) => st.played);
  // v0.47 (maj. 5 nov.) : appui long sur la note d'un pas posé : le menu du pas (grandes touches pour le doigt) —
  // note, accent, glissé et leurs valeurs, vélocité, durée, décalage, condition, probabilité, répétitions
  // v0.47 (5 nov. après-midi) : toucher un AUTRE pas ferme le menu — sinon les notes jouées en réglant cet autre pas
  // (tirer la note, ▲ ▼, touche du clavier) étaient aussi écrites sur le pas du menu resté ouvert
  const [menu, setMenu] = useState<number | null>(null);
  // v0.47 (5 nov. après-midi) : un pas choisi pour ses verrous (écriture À verrous, écran SÉQUENCE) reste actif quand on
  // passe en Acide : « Verrous · pas N ✕ » s'allume ici aussi, le toucher libère le pas
  const lockSel = useStore((st) => (st.ui.stepEdit ? st.ui.step : null));
  const hold = useRef<{ timer: number; fired: boolean } | null>(null);
  // v0.47 (5 nov. après-midi) : le clavier ne change plus la note du pas dont le menu est ouvert (il choisit seulement
  // la note des pas qu'on pose ensuite) ; la note d'un pas posé se change par Note ◀ ▶ du menu ou en la tirant
  if (!pattern) return null;
  const pages = Math.max(1, Math.ceil(pattern.length / 16));
  const pg = Math.min(page, pages - 1);
  const first = pg * 16;
  const flag = (s: number, bit: number) => ((pattern.flags[s] ?? 0) & bit) !== 0;
  // v0.48 : notes plus longues qu'un pas, dessinées sur les pas qu'elles couvrent
  const lanesAt = (s: number) => (laneMode ? ((pattern.lanes[T]?.vel[s] ?? 0) > 0 ? [T] : []) : stepLanes(pattern, s));
  const spans = noteSpans(pattern, (s) => (laneMode ? [T] : stepLanes(pattern, s)));
  const toggleFlag = (s: number, bit: number) => void actions.seq("seqStepsFlag", JSON.stringify([s]), bit, !flag(s, bit));
  const setNote = (s: number, n: number, l = 0) => {
    const note = Math.max(0, Math.min(127, n));
    void actions.seq("seqSetNote", l, s, note);
    if ((getState().pattern?.lanes[l]?.vel[s] ?? 0) > 0) { actions.noteOn(pattern.track, note, 0.7); window.setTimeout(() => actions.noteOff(pattern.track, note), 140); }
  };
  return (
    <div className="steps303">
      {edit && <StepAmountEditor step={edit.step} kind={edit.kind} onClose={() => setEdit(null)} />}
      <div className="steps303-pages">
        <span className={`steps303-note${played.note < 0 ? " none" : ""}`} title="Note écrite par les pas que vous posez : jouez une touche du clavier (ou un pad) pour la changer">
          Note <b>{played.note >= 0 ? noteName(played.note) : "—"}</b>
        </span>
        {Array.from({ length: pages }, (_, i) => (
          <button key={i} type="button" className={i === pg ? "on" : ""} onClick={() => setUi({ page: i })}>{i + 1}</button>
        ))}
        {lockSel !== null && (
          <button type="button" className="steps303-lockchip" onClick={() => setUi({ step: null, stepEdit: false })}
            title={`Pas ${stepLabel(lockSel)} choisi : les boutons écrivent dans ce pas seulement. Toucher pour le libérer.`}>{`Verrous · pas ${stepLabel(lockSel)} ✕`}</button>
        )}
        {menu !== null && (
          <button type="button" className="steps303-selchip" onClick={() => setMenu(null)}
            title="Le menu de ce pas est ouvert : ses réglages changent ce pas seulement. Toucher pour le fermer.">{`Pas ${stepLabel(menu)} choisi ✕`}</button>
        )}
      </div>
      <div className="steps303-row">
        {Array.from({ length: 16 }, (_, i) => {
          const s = first + i;
          const out = s >= pattern.length;
          // v0.46 fix28 : notes d'accord (autres lignes, posées au piano roll) visibles et effaçables ici
          const held = out ? [] : lanesAt(s);
          const on = held.length > 0;
          const ml = laneMode ? T : held.includes(0) || !on ? 0 : held[0];
          const note = pattern.lanes[ml]?.note[s] ?? 48;
          const more = held.length - 1;
          const lane = pattern.lanes[ml];
          const here = playing && (pos[laneMode ? T : 0] ?? -1) === s;
          return (
            <div key={i} className={`step303${on ? " on" : ""}${out ? " out" : ""}${here ? " here" : ""}${i % 4 === 0 ? " beat" : ""}${menu === s ? " sel" : ""}${spanClass(spans, s)}`}
              onPointerDownCapture={() => { if (menu !== null && menu !== s) setMenu(null); }}
              onWheelCapture={() => { if (menu !== null && menu !== s) setMenu(null); }}>
              <button type="button" className="step303-key" disabled={out} aria-pressed={on}
                aria-label={`Pas ${stepLabel(s)}${on ? `, ${noteName(note)}${more > 0 ? ` + ${more} note${more > 1 ? "s" : ""} d'accord` : ""}` : ", silence"} (tirer : note)`}
                title={more > 0 ? `Accord de ${held.length} notes (piano roll) : toucher retire tout le pas` : undefined}
                onPointerDown={(e) => {
                  e.currentTarget.setPointerCapture(e.pointerId); drag.current = { id: e.pointerId, y: e.clientY, step: s, note, moved: false };
                  if (hold.current) window.clearTimeout(hold.current.timer);
                  const h = { fired: false, timer: 0 };
                  h.timer = window.setTimeout(() => { if (drag.current && !drag.current.moved && on) { h.fired = true; setMenu(s); } }, 450);
                  hold.current = h;
                }}
                onPointerMove={(e) => {
                  const d = drag.current;
                  if (!d || d.id !== e.pointerId) return;
                  const k = Math.trunc((d.y - e.clientY) / 10);
                  if (k === 0) return;
                  d.moved = true; d.y -= k * 10; d.note += k;
                  setNote(d.step, d.note, ml);
                }}
                onPointerUp={() => {
                  const d = drag.current;
                  drag.current = null;
                  const h = hold.current;
                  hold.current = null;
                  if (h) { window.clearTimeout(h.timer); if (h.fired) return; }
                  // v0.46 fix28 : un pas posé prend la dernière touche jouée (clavier, pads), comme la rangée à verrous
                  if (d && !d.moved) {
                    if (on) void actions.clearStepLanes(s, held);
                    else if (played.note >= 0) actions.toggleStep(ml, s, played.note);
                    else void actions.seq("seqSetStep", ml, s, 100);
                  }
                }}
                onWheel={(e) => setNote(s, note + (e.deltaY < 0 ? 1 : -1), ml)}>
                <span className="step303-num">{stepInBar(s)}</span>
                <span className="step303-note">{on ? noteName(note) : spans.get(s) && spans.get(s)!.note >= 0 ? <i className="step303-cont">{noteName(spans.get(s)!.note)}</i> : "—"}{more > 0 && <sup className="step303-chord">+{more}</sup>}</span>
              </button>
              <div className="step303-fn">
                <button type="button" disabled={out} title="Octave plus haut" onClick={() => setNote(s, note + 12, ml)}>▲</button>
                <button type="button" disabled={out} title="Octave plus bas" onClick={() => setNote(s, note - 12, ml)}>▼</button>
              </div>
              <div className="step303-vals">
                <StepValue label={`Vélocité du pas ${stepLabel(s)}`} text={on ? String(lane?.vel[s] ?? 100) : "—"} disabled={!on}
                  binding={{ kind: "continuous", label: "Vélocité", text: String(lane?.vel[s] ?? 100), normalised: ((lane?.vel[s] ?? 100) - 1) / 126,
                    onChange: (n) => void actions.seq("seqSetStep", ml, s, Math.max(1, Math.min(127, Math.round(1 + n * 126)))) }} />
                <StepValue label={`Durée du pas ${stepLabel(s)} (en pas)`} text={on ? lenText(lane?.len[s] ?? 8) : "—"} disabled={!on}
                  binding={{ kind: "stepped", label: "Durée", text: lenText(lane?.len[s] ?? 8), normalised: 0,
                    onStep: (d) => { const cur = lane?.len[s] ?? 8; const i = LENS.findIndex((x) => x >= cur); const k = Math.max(0, Math.min(LENS.length - 1, (i < 0 ? LENS.length - 1 : i) + d)); void actions.seq("seqSetLength8", ml, s, LENS[k]); } }} />
              </div>
              <div className="step303-fn">
                <FnButton disabled={out} on={flag(s, STEP_ACCENT)} cls="acc" text="Acc." amount={pattern.accAmt[s] ?? 0}
                  onToggle={() => toggleFlag(s, STEP_ACCENT)} onEdit={() => setEdit({ step: s, kind: "acc" })} />
                <FnButton disabled={out} on={flag(s, STEP_SLIDE)} cls="sld" text="Gl." amount={pattern.sldAmt[s] ?? 0}
                  onToggle={() => toggleFlag(s, STEP_SLIDE)} onEdit={() => setEdit({ step: s, kind: "sld" })} />
              </div>
            </div>
          );
        })}
      </div>
      {menu !== null && lanesAt(menu).length > 0 && (() => {
        const ls = lanesAt(menu);
        const l = ls.includes(0) ? 0 : ls[0];
        const big = (label: string, isOn: boolean, toggle: () => void, amount: number, kind: StepAmountKind) => (
          <span className="ot-stepedit-item">
            <button type="button" className={`ot-stepedit-flag${isOn ? " on" : ""}`} aria-pressed={isOn} onClick={toggle}>{label}{isOn && amount > 0 ? ` ${amount} %` : ""}</button>
            <button type="button" onClick={() => setEdit({ step: menu, kind })} aria-label={`Valeur : ${label}`} title={`Valeur du pas : ${label}`}>…</button>
          </span>
        );
        return (
          <OctaStepEdit T={l} step={menu} onClose={() => setMenu(null)} extra={<>
            {big("Accent", flag(menu, STEP_ACCENT), () => toggleFlag(menu, STEP_ACCENT), pattern.accAmt[menu] ?? 0, "acc")}
            {big("Glissé", flag(menu, STEP_SLIDE), () => toggleFlag(menu, STEP_SLIDE), pattern.sldAmt[menu] ?? 0, "sld")}
            <span className="ot-stepedit-item">
              <small>Octave</small>
              <button type="button" onClick={() => setNote(menu, (pattern.lanes[l]?.note[menu] ?? 48) - 12, l)} aria-label="Octave plus bas">▼</button>
              <button type="button" onClick={() => setNote(menu, (pattern.lanes[l]?.note[menu] ?? 48) + 12, l)} aria-label="Octave plus haut">▲</button>
            </span>
          </>} />
        );
      })()}
    </div>
  );
}
