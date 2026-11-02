// Groovebox — OFAD — AGPL-3.0-or-later
// v0.45.2 : onglet « Pas » — choix de l'écriture : séquenceur d'origine (rangée acide des synthés,
// rangée de pas des boîtes à rythmes) ou séquenceur à verrous (pages, mesures, verrous par pas).
// Les deux écrivent la même séquence que l'écran Séquence.
import { useSyncExternalStore, type ReactNode } from "react";
import { useStore } from "../../engine/store";
import { LockSteps } from "./OctaPanel";

type Kind = "synth" | "drum";
const KEY = (k: Kind) => `grv.stepView.${k}`;

function readView(k: Kind): "classic" | "locks" {
  try { return localStorage.getItem(KEY(k)) === "locks" ? "locks" : "classic"; } catch { return "classic"; }
}
// choix partagé entre l'onglet Pas et la rangée sous la page Son (même valeur partout)
const views: Record<Kind, "classic" | "locks"> = { synth: readView("synth"), drum: readView("drum") };
const listeners = new Set<() => void>();
function setView(k: Kind, v: "classic" | "locks") {
  views[k] = v;
  try { localStorage.setItem(KEY(k), v); } catch { /* stockage absent */ }
  listeners.forEach((f) => f());
}
export function useStepView(k: Kind) {
  return useSyncExternalStore((f) => { listeners.add(f); return () => { listeners.delete(f); }; }, () => views[k]);
}

/** Sous la page Son d'un synthé : la rangée à verrous de la ligne 1, si c'est l'écriture choisie. */
export function StepsDock() {
  const view = useStepView("synth");
  if (view !== "locks") return null;
  return <div className="steps-dock"><LockSteps T={0} name="Ligne 1" /></div>;
}

export function StepsChoice({ kind, children }: { kind: Kind; children: ReactNode }) {
  const view = useStepView(kind);
  const lane = useStore((s) => s.ui.lane);
  const set = (v: "classic" | "locks") => setView(kind, v);
  const T = kind === "synth" ? 0 : lane;
  return (
    <div className="steps-choice">
      <div className="steps-choice-head" role="radiogroup" aria-label="Écriture des pas">
        <small>Écriture</small>
        <button type="button" role="radio" aria-checked={view === "classic"} className={view === "classic" ? "on" : ""} onClick={() => set("classic")}
          title={kind === "synth" ? "Rangée de pas façon basse acide : note, octave, accent, glissé" : "Rangée de pas classique de la boîte à rythmes"}>
          {kind === "synth" ? "Acide" : "Classique"}
        </button>
        <button type="button" role="radio" aria-checked={view === "locks"} className={view === "locks" ? "on" : ""} onClick={() => set("locks")}
          title="Séquenceur à verrous : pages de 16 pas, mesures, et réglages verrouillés pas par pas (appui long ou mode Verrous)">
          À verrous
        </button>
      </div>
      {view === "classic" ? children : <LockSteps T={T} name={kind === "synth" ? "Ligne 1" : `Pad ${lane + 1}`} />}
    </div>
  );
}
