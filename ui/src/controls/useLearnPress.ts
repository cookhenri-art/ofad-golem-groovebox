// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 : affecter un bouton de l'interface (coupure, solo…) au contrôleur MIDI : clic droit à la
// souris, appui long (0,6 s) au doigt ; puis toucher un bouton ou un pad du contrôleur.
import { useRef } from "react";
import { actions, showMessage } from "../engine/store";

export function useLearnPress() {
  const press = useRef<number | null>(null);
  const learnOn = (target: number, what: string) => {
    void actions.midiLearn(target);
    showMessage(`${what} : touchez un bouton ou un pad du contrôleur MIDI`);
  };
  const cancel = () => { if (press.current !== null) { window.clearTimeout(press.current); press.current = null; } };
  return (target: number, what: string) => ({
    onPointerDown: (e: React.PointerEvent) => {
      if (e.pointerType === "mouse") return;
      cancel();
      press.current = window.setTimeout(() => { press.current = null; learnOn(target, what); }, 600);
    },
    onPointerUp: cancel,
    onPointerLeave: cancel,
    onContextMenu: (e: React.MouseEvent) => { e.preventDefault(); learnOn(target, what); },
  });
}
