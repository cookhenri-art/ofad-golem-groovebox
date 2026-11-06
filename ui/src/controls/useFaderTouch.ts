// Groovebox — OFAD — AGPL-3.0-or-later
// v1.0.0 : crossfader au doigt. Le curseur natif (input range) se laissait mal saisir dans la
// WebView Android (zone de toucher plus petite que le bouton dessiné, geste parfois pris pour un
// défilement) et la valeur renvoyée par le moteur à chaque image le ramenait en arrière pendant
// le geste. Ici, toute la glissière reçoit le doigt (touch-action: none, capture du pointeur) ;
// pendant le geste, la position affichée est celle du doigt, et la valeur part au moteur une fois
// par image. Le curseur natif reste pour le clavier et les lecteurs d'écran.
import { useEffect, useRef, useState } from "react";

const clamp01 = (v: number) => Math.min(1, Math.max(0, v));

/** Largeur du bouton dessiné du curseur (pour que le doigt tombe sous le bouton). */
function thumbWidth(input: HTMLInputElement | null, fallback: number) {
  // le bouton dessiné est aussi haut que le curseur dans ces glissières : on prend la plus petite des
  // deux mesures (getComputedStyle ne lit pas le pseudo-élément du bouton dans Chromium)
  if (!input) return fallback;
  const h = input.getBoundingClientRect().height;
  return h >= 12 ? Math.min(fallback, h) : fallback;
}

export function useFaderTouch(value: number, onValue: (v: number) => void, fallbackThumb = 30) {
  const [drag, setDrag] = useState<number | null>(null);
  const pointer = useRef<number | null>(null);
  const pending = useRef<number | null>(null);
  const raf = useRef(0);
  const release = useRef(0);
  const send = useRef(onValue);
  send.current = onValue;

  useEffect(() => () => { cancelAnimationFrame(raf.current); window.clearTimeout(release.current); }, []);

  const flush = () => {
    raf.current = 0;
    if (pending.current !== null) { const v = pending.current; pending.current = null; send.current(v); }
  };
  const push = (v: number) => {
    setDrag(v);
    pending.current = v;
    if (!raf.current) raf.current = requestAnimationFrame(flush);
  };
  const at = (el: HTMLElement, x: number) => {
    const input = el.querySelector<HTMLInputElement>('input[type="range"]');
    const r = (input ?? el).getBoundingClientRect();
    const t = Math.min(thumbWidth(input, fallbackThumb), r.width / 2);
    return clamp01((x - r.left - t / 2) / Math.max(1, r.width - t));
  };
  const end = () => {
    if (pointer.current === null) return;
    pointer.current = null;
    cancelAnimationFrame(raf.current);
    flush();
    // la valeur du moteur arrive à l'image suivante : garder la position du doigt un instant
    window.clearTimeout(release.current);
    release.current = window.setTimeout(() => setDrag(null), 250);
  };

  return {
    shown: drag ?? value,
    handlers: {
      onPointerDown: (e: React.PointerEvent<HTMLElement>) => {
        if (e.pointerType === "mouse" && e.button !== 0) return;
        e.preventDefault();
        window.clearTimeout(release.current);
        try { e.currentTarget.setPointerCapture(e.pointerId); } catch { /* déjà capturé */ }
        pointer.current = e.pointerId;
        push(at(e.currentTarget, e.clientX));
      },
      onPointerMove: (e: React.PointerEvent<HTMLElement>) => {
        if (pointer.current === e.pointerId) push(at(e.currentTarget, e.clientX));
      },
      onPointerUp: end,
      onPointerCancel: end,
      onLostPointerCapture: end,
    },
  };
}
