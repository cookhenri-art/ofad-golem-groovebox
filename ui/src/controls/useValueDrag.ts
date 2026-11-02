// Groovebox — OFAD — AGPL-3.0-or-later
// Réglage d'une valeur en la tirant vers le haut ou le bas, à la souris comme au doigt :
// encodeurs, valeurs affichées sur l'écran, tempo. Maj = réglage fin, molette, double-clic
// (ou double tape) = valeur d'origine. Un appui sans mouvement reste un simple clic.

import { useRef } from "react";
import type { EncoderBinding } from "./Encoder";

/** Pixels de déplacement avant qu'un appui devienne un glissé (un clic reste un clic). */
const DEAD_ZONE = 3;

/** opts.span : hauteur (px) qui parcourt toute la course (faders : la piste) ; opts.startAt : valeur
    de départ du glissé choisie à l'appui (toucher la piste d'un fader place la manette). */
/** Nombre de crans d'un réglage à crans. */
const stepCount = (en: NonNullable<EncoderBinding["entry"]>) => (en.choices ? en.choices.length : Math.round(en.max - en.min) + 1);

export function useValueDrag(binding: EncoderBinding | null, opts?: { span?: () => number; startAt?: (e: React.PointerEvent<HTMLElement>) => number | null }) {
  const drag = useRef<{ x: number; y: number; value: number; acc: number; moved: boolean; id: number; index: number } | null>(null);
  const wheelAcc = useRef(0);
  const lastTap = useRef(0);

  const continuousTo = (v: number) => {
    if (binding?.kind === "continuous") binding.onChange(Math.min(1, Math.max(0, v)));
  };
  const nudge = (dir: 1 | -1, fine: boolean) => {
    if (!binding) return;
    if (binding.kind === "stepped") binding.onStep(dir);
    else continuousTo(binding.normalised + dir * (fine ? 0.004 : 0.02) * (binding.sensitivity ?? 1));
  };

  return {
    nudge,
    handlers: {
      onPointerDown: (e: React.PointerEvent<HTMLElement>) => {
        if (!binding || e.button !== 0) return;
        e.currentTarget.setPointerCapture(e.pointerId);
        let start = binding.normalised;
        if (binding.kind === "continuous" && opts?.startAt) { const v = opts.startAt(e); if (v !== null) { start = Math.min(1, Math.max(0, v)); binding.onChange(start); } }
        const en = binding.entry;
        const startIndex = en ? (en.choices ? Math.round(en.plain) : Math.round(en.plain - en.min)) : 0;
        drag.current = { x: e.clientX, y: e.clientY, value: start, acc: 0, moved: false, id: e.pointerId, index: startIndex };
        // double tape au doigt = valeur d'origine (le double-clic n'existe pas au toucher) : deux
        // touchers SANS glissé à moins de 300 ms (un glissé rapide n'est pas un toucher)
        if (e.pointerType === "touch" && performance.now() - lastTap.current < 300) { binding.reset?.(); drag.current = null; lastTap.current = 0; }
      },
      onPointerMove: (e: React.PointerEvent<HTMLElement>) => {
        const d = drag.current;
        if (!d || !binding || e.pointerId !== d.id) return;
        // v0.46 : vers le haut OU vers la droite = plus (l'axe le plus parcouru l'emporte) : un
        // panoramique ou une barre horizontale se règle en glissant de gauche à droite
        const ux = e.clientX - d.x, uy = d.y - e.clientY;
        const dy = Math.abs(ux) > Math.abs(uy) ? ux : uy;
        if (!d.moved && Math.abs(dy) < DEAD_ZONE) return;
        d.moved = true;
        if (binding.kind === "continuous") {
          const span = opts?.span?.() ?? 0;
          const per = span > 20 ? 1 / span : 0.005 * (binding.sensitivity ?? 1); // fader : la course suit la souris
          continuousTo(d.value + dy * per * (e.shiftKey ? 0.24 : 1));
        } else if (binding.entry && stepCount(binding.entry) > 24) {
          // v0.45.1 : réglage à beaucoup de crans (0 à 99…) : la course suit la souris comme un réglage
          // continu (toute la plage en 200 px, ou sur la course d'un fader), au lieu d'un cran tous les 14 px
          const en = binding.entry, n = stepCount(en);
          const span = opts?.span?.() ?? 0;
          const per = (n - 1) * (span > 20 ? 1 / span : 0.005) * (e.shiftKey ? 0.24 : 1);
          const idx = Math.max(0, Math.min(n - 1, Math.round(d.index + dy * per)));
          if (en.choices) en.pick(idx); else en.set(en.min + idx);
        } else {
          // un cran tous les 14 pixels
          const steps = Math.trunc(dy / 14) - d.acc;
          if (steps !== 0) {
            for (let i = 0; i < Math.abs(steps); ++i) binding.onStep(steps > 0 ? 1 : -1);
            d.acc += steps;
          }
        }
      },
      onPointerUp: (e: React.PointerEvent<HTMLElement>) => {
        const d = drag.current;
        if (d && e.pointerType === "touch") lastTap.current = d.moved ? 0 : performance.now();
        drag.current = null;
      },
      onPointerCancel: () => { drag.current = null; },
      onWheel: (e: React.WheelEvent<HTMLElement>) => {
        if (!binding) return;
        if (binding.kind === "stepped") {
          wheelAcc.current += e.deltaY;
          if (Math.abs(wheelAcc.current) >= 60) { binding.onStep(wheelAcc.current < 0 ? 1 : -1); wheelAcc.current = 0; }
        } else nudge(e.deltaY < 0 ? 1 : -1, e.shiftKey);
      },
      onDoubleClick: () => { binding?.reset?.(); },
    },
  };
}
