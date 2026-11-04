// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix27 : valeur d'un pas — force de l'accent, durée du glissé — réglée de la même façon dans la
// Séquence (lignes ACCENT / GLISSÉ) et dans la page Pas des synthés (boutons Acc. / Gl.).
// Toucher : pose ou retire la fonction. Appui long (clic droit) : petite fenêtre avec la valeur,
// ▲ ▼, glisser le doigt sur la valeur, ou la taper. Chaque changement s'applique tout de suite
// (on écoute en réglant) ; la fenêtre se ferme quand on veut.

import { useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { actions, useStore } from "../engine/store";
import { STEP_ACCENT, STEP_SLIDE } from "../engine/protocol";

export type StepAmountKind = "acc" | "sld";

/** Appui long au doigt (550 ms sans bouger) ou clic droit : ouvre ; le toucher qui suit est ignoré. */
export function useLongPress(open: () => void) {
  const press = useRef<{ x: number; y: number; timer: number } | null>(null);
  const fired = useRef(false);
  const cancel = () => { if (press.current) { window.clearTimeout(press.current.timer); press.current = null; } };
  return {
    /** vrai si le geste vient d'ouvrir la fenêtre (le clic qui suit ne doit rien faire) */
    consumed: () => { const f = fired.current; fired.current = false; return f; },
    handlers: {
      onPointerDown: (e: React.PointerEvent) => {
        fired.current = false;
        if (e.pointerType === "mouse") return;
        cancel();
        press.current = { x: e.clientX, y: e.clientY, timer: window.setTimeout(() => { press.current = null; fired.current = true; open(); }, 550) };
      },
      onPointerMove: (e: React.PointerEvent) => { if (press.current && Math.hypot(e.clientX - press.current.x, e.clientY - press.current.y) > 8) cancel(); },
      onPointerUp: cancel,
      onPointerCancel: cancel,
      onContextMenu: (e: React.MouseEvent) => { e.preventDefault(); cancel(); fired.current = true; open(); },
    },
  };
}

/** Fenêtre de la valeur d'un pas (posée sur document.body : grande et lisible sur téléphone). */
export function StepAmountEditor({ step, kind, onClose }: { step: number; kind: StepAmountKind; onClose: () => void }) {
  const stored = useStore((s) => (kind === "acc" ? s.pattern?.accAmt[step] : s.pattern?.sldAmt[step]) ?? 0);
  const flagOn = useStore((s) => (((s.pattern?.flags[step] ?? 0) & (kind === "acc" ? STEP_ACCENT : STEP_SLIDE)) !== 0));
  const trackAcc = useStore((s) => s.pattern?.accent ?? 50);
  // valeur affichée : la locale pendant le réglage (le moteur répond ensuite)
  const [local, setLocal] = useState<number | null>(null);
  const [text, setText] = useState<string | null>(null);
  const idle = useRef(0);
  useEffect(() => () => window.clearTimeout(idle.current), []);
  const value = local ?? stored;
  const shown = value > 0 ? value : kind === "acc" ? trackAcc : 0;
  const name = kind === "acc" ? "Accent" : "Glissé";
  const bit = kind === "acc" ? STEP_ACCENT : STEP_SLIDE;

  const apply = (v: number) => {
    const x = Math.round(Math.min(100, Math.max(0, v)));
    setLocal(x);
    window.clearTimeout(idle.current);
    idle.current = window.setTimeout(() => setLocal(null), 600);
    void actions.seq("seqStepAmount", step, kind === "acc" ? 0 : 1, x);
    // régler une valeur allume la fonction du pas
    if (x > 0 && !flagOn) void actions.seq("seqStepsFlag", JSON.stringify([step]), bit, true);
  };
  const base = value > 0 ? value : kind === "acc" ? trackAcc : 50;

  // glisser le doigt sur la valeur : 1 % tous les 3 pixels (vers le haut ou la droite = plus)
  const drag = useRef<{ x: number; y: number; v: number } | null>(null);
  const repeat = useRef(0);
  const holdStep = (d: number) => {
    let cur = base;
    const once = () => { cur = Math.min(100, Math.max(1, cur + d)); apply(cur); };
    once();
    window.clearInterval(repeat.current);
    const start = window.setTimeout(() => { repeat.current = window.setInterval(once, 70); }, 380);
    const stop = () => { window.clearTimeout(start); window.clearInterval(repeat.current); window.removeEventListener("pointerup", stop); window.removeEventListener("pointercancel", stop); };
    window.addEventListener("pointerup", stop);
    window.addEventListener("pointercancel", stop);
  };

  return createPortal(
    <div className="stepamt-back" onPointerDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className={`stepamt stepamt--${kind}`} role="dialog" aria-label={`${name} du pas ${step + 1}`}>
        <div className="stepamt-head">
          <span className="stepamt-title">{name} · pas {step + 1}</span>
          <button type="button" className="stepamt-close" onClick={onClose} aria-label="Fermer">✕</button>
        </div>
        <div className="stepamt-row">
          <button type="button" className="stepamt-btn" aria-label="Moins" onPointerDown={(e) => { e.preventDefault(); holdStep(-1); }}>▼</button>
          <div className="stepamt-val" role="slider" aria-valuemin={1} aria-valuemax={100} aria-valuenow={shown}
            onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); drag.current = { x: e.clientX, y: e.clientY, v: base }; }}
            onPointerMove={(e) => {
              const d = drag.current; if (!d) return;
              const ux = e.clientX - d.x, uy = d.y - e.clientY;
              const delta = Math.abs(ux) > Math.abs(uy) ? ux : uy;
              if (Math.abs(delta) < 3) return;
              apply(Math.min(100, Math.max(1, d.v + delta / 3)));
            }}
            onPointerUp={() => { drag.current = null; }} onPointerCancel={() => { drag.current = null; }}>
            {text !== null ? (
              <input autoFocus inputMode="numeric" value={text} aria-label={`${name} en %`}
                onChange={(e) => { setText(e.target.value); const n = Number(e.target.value.replace(",", ".")); if (Number.isFinite(n) && n >= 1) apply(n); }}
                onBlur={() => setText(null)} onKeyDown={(e) => { e.stopPropagation(); if (e.key === "Enter" || e.key === "Escape") setText(null); }} />
            ) : (
              <b onDoubleClick={() => setText(String(shown || ""))}>{shown}</b>
            )}
            <small>%</small>
          </div>
          <button type="button" className="stepamt-btn" aria-label="Plus" onPointerDown={(e) => { e.preventDefault(); holdStep(1); }}>▲</button>
        </div>
        <div className="stepamt-foot">
          <span className="stepamt-hint">
            {value === 0 ? (kind === "acc" ? `Accent de la piste (${trackAcc} %)` : "Portamento de l'instrument")
              : kind === "acc" ? "Force de l'accent de ce pas" : "Durée du glissement vers le pas suivant (part du pas)"}
          </span>
          <button type="button" className="stepamt-small" onClick={() => setText(String(shown || ""))}>Taper…</button>
          <button type="button" className="stepamt-small" onClick={() => { setLocal(0); void actions.seq("seqStepAmount", step, kind === "acc" ? 0 : 1, 0); }}
            title={kind === "acc" ? "Revenir à l'accent de la piste" : "Revenir au portamento de l'instrument"}>Par défaut</button>
        </div>
      </div>
    </div>,
    document.body,
  );
}
