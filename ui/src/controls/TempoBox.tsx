// v0.46 fix23 : tempo saisissable au clavier (toucher la valeur) et rangée Tempo du téléphone
// (− / valeur / + / Tap), sous le crossfader et Photo / Rappel.
// Le tempo du projet est le réglage global « bpm » (40 à 240 BPM, au dixième).

import { useEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { actions, showMessage, useStore } from "../engine/store";
import { useValueDrag } from "./useValueDrag";
import { hostCode } from "../engine/protocol";
import type { EncoderBinding } from "./Encoder";

export const BPM_MIN = 40;
export const BPM_MAX = 240;

const clampBpm = (v: number) => Math.round(Math.min(BPM_MAX, Math.max(BPM_MIN, v)) * 10) / 10;
const fmt = (v: number) => v.toFixed(1).replace(".", ",");

export function setBpm(v: number) {
  actions.setGlobal("bpm", clampBpm(v));
}

/** Fenêtre de saisie du tempo : champ numérique (clavier du téléphone), OK, Annuler.
    Rendue sur document.body, hors de l'appareil mis à l'échelle : elle garde une taille lisible. */
export function TempoEntry({ onClose }: { onClose: () => void }) {
  const bpm = useStore((s) => s.tick.g.bpm);
  const [text, setText] = useState(() => fmt(bpm));
  const input = useRef<HTMLInputElement>(null);
  const openedAt = useRef(performance.now());
  useEffect(() => {
    // on a touché le tempo pour le taper : le champ est pris et sa valeur sélectionnée
    input.current?.focus();
    input.current?.select();
  }, []);
  const commit = () => {
    const clean = text.trim().replace(",", ".").replace(/[^0-9.]/g, "");
    const v = clean ? Number(clean) : NaN;
    if (!Number.isFinite(v)) { showMessage("Tempo non reconnu : tapez un nombre, par exemple 128"); return; }
    if (v < BPM_MIN || v > BPM_MAX) showMessage(`Tempo ramené entre ${BPM_MIN} et ${BPM_MAX} BPM`);
    setBpm(v);
    onClose();
  };
  return createPortal(
    <div className="tempo-entry-back"
      onClick={(e) => { if (e.target === e.currentTarget && performance.now() - openedAt.current > 400) onClose(); }}>
      <div className="tempo-entry" role="dialog" aria-label="Tempo">
        <span className="tempo-entry-title">Tempo</span>
        <form className="tempo-entry-row" onSubmit={(e) => { e.preventDefault(); commit(); }}>
          <input ref={input} type="text" inputMode="decimal" enterKeyHint="done" value={text} maxLength={6} aria-label="Tempo en BPM"
            onChange={(e) => setText(e.target.value)} onKeyDown={(e) => { e.stopPropagation(); if (e.key === "Escape") onClose(); }} />
          <span className="tempo-entry-unit">BPM</span>
          <button type="submit" className="tempo-entry-ok">OK</button>
        </form>
        <span className="tempo-entry-range">de {BPM_MIN} à {BPM_MAX} BPM, au dixième (ex. 128 ou 92,5)</span>
        <button type="button" className="tempo-entry-cancel" onClick={onClose}>Annuler</button>
      </div>
    </div>,
    document.body,
  );
}

/** Valeur du tempo : glisser vers le haut / le bas pour régler, toucher (sans glisser) pour taper
    le nombre. Sert à la barre d'état et à la rangée du téléphone. */
export function TempoValue({ className, children, title }: { className: string; children: React.ReactNode; title?: string }) {
  const g = useStore((s) => s.tick.g.bpm);
  const [open, setOpen] = useState(false);
  const down = useRef<{ x: number; y: number } | null>(null);
  const binding: EncoderBinding = {
    kind: "continuous", label: "Tempo", text: `${g.toFixed(1)} BPM`, normalised: (g - BPM_MIN) / (BPM_MAX - BPM_MIN),
    onChange: (v) => setBpm(BPM_MIN + v * (BPM_MAX - BPM_MIN)),
    sensitivity: 0.25, // 0,25 BPM par pixel (Maj : 0,06)
  };
  const { handlers } = useValueDrag(binding);
  return (
    <>
      <span className={className} role="slider" tabIndex={0} data-ctl={hostCode("bpm")} data-ctl-label="Tempo" data-ctl-norm={(g - BPM_MIN) / (BPM_MAX - BPM_MIN)} aria-label="Tempo" aria-valuemin={BPM_MIN} aria-valuemax={BPM_MAX} aria-valuenow={g}
        title={title ?? "Tempo : toucher pour taper la valeur, glisser vers le haut ou le bas pour la régler (Maj = fin)"}
        {...handlers}
        onDoubleClick={undefined}
        onPointerDown={(e) => { down.current = { x: e.clientX, y: e.clientY }; handlers.onPointerDown(e); }}
        onPointerUp={(e) => {
          handlers.onPointerUp(e);
          const d = down.current; down.current = null;
          if (d && e.button === 0 && Math.hypot(e.clientX - d.x, e.clientY - d.y) < 6) setOpen(true);
        }}
        onKeyDown={(e) => {
          if (e.key === "Enter") { e.preventDefault(); setOpen(true); }
          else if (e.key === "ArrowUp" || e.key === "ArrowRight") { e.preventDefault(); setBpm(g + (e.shiftKey ? 0.1 : 1)); }
          else if (e.key === "ArrowDown" || e.key === "ArrowLeft") { e.preventDefault(); setBpm(g - (e.shiftKey ? 0.1 : 1)); }
        }}>
        {children}
      </span>
      {open && <TempoEntry onClose={() => setOpen(false)} />}
    </>
  );
}

/** Tap tempo : la moyenne des 4 derniers écarts entre les touchers ; au-delà de 2 s on repart. */
function useTapTempo() {
  const taps = useRef<number[]>([]);
  return () => {
    const now = performance.now();
    const t = taps.current;
    if (t.length && now - t[t.length - 1] > 2000) t.length = 0;
    t.push(now);
    if (t.length > 5) t.shift();
    if (t.length < 2) { showMessage("Tap : touchez en rythme, au moins deux fois"); return; }
    const avg = (t[t.length - 1] - t[0]) / (t.length - 1);
    setBpm(60000 / avg);
  };
}

/** Téléphone : rangée Tempo sous Photo / Rappel. */
export function TempoRow() {
  const g = useStore((s) => s.tick.g.bpm);
  const tap = useTapTempo();
  return (
    <div className="phone-row phone-row--tempo">
      <button type="button" className="hw hw--md tempo-btn" data-led="amber" aria-label="Tempo moins 1"
        onClick={() => setBpm(Math.ceil(g - 0.05) - 1)}>−</button>
      <TempoValue className="tempo-phone-value tabular">
        <b>{fmt(g)}</b> <small>BPM</small>
      </TempoValue>
      <button type="button" className="hw hw--md tempo-btn" data-led="amber" aria-label="Tempo plus 1"
        onClick={() => setBpm(Math.floor(g + 0.05) + 1)}>+</button>
      <button type="button" className="hw hw--md tempo-btn tempo-btn--tap" data-led="amber" aria-label="Tap tempo"
        title="Touchez en rythme : le tempo suit"
        onPointerDown={(e) => { if (e.button === 0) tap(); }}>Tap</button>
    </div>
  );
}
