// Groovebox — OFAD — AGPL-3.0-or-later
// v0.45.2 : onglet « Pas » — choix de l'écriture : séquenceur d'origine (rangée acide des synthés,
// rangée de pas des boîtes à rythmes) ou séquenceur à verrous (pages, mesures, verrous par pas).
// Les deux écrivent la même séquence que l'écran Séquence.
import { useSyncExternalStore, type ReactNode } from "react";
import { actions, useStore } from "../../engine/store";
import { PATTERN_NAMES } from "../../engine/protocol";
import { LockSteps } from "./OctaPanel";
import { ShiftButtons } from "./ShiftButtons";
import { StepStrip } from "./StepStrip";

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

/** Sous les pages d'un synthé (au-dessus du clavier) : la rangée de pas choisie — acide ou à verrous —
    avec le choix de l'écriture en grand. v0.46 fix27 : la rangée acide y est aussi ; « Masquer » rend
    la place (choix gardé). */
const DOCK_KEY = "grv.stepDockHidden";
let dockHidden = (() => { try { return localStorage.getItem(DOCK_KEY) === "1"; } catch { return false; } })();
function setDockHidden(v: boolean) {
  dockHidden = v;
  try { localStorage.setItem(DOCK_KEY, v ? "1" : "0"); } catch { /* stockage absent */ }
  listeners.forEach((f) => f());
}
export function StepsDock() {
  const hidden = useSyncExternalStore((f) => { listeners.add(f); return () => { listeners.delete(f); }; }, () => dockHidden);
  return (
    <div className={`steps-dock${hidden ? " is-hidden" : ""}`}>
      <StepsChoice kind="synth" dock hidden={hidden} onHide={setDockHidden}><StepStrip /></StepsChoice>
    </div>
  );
}

/** v0.46 fix28 : pattern montré par la rangée ; s'il n'est pas celui qui joue, on le dit et on peut
    passer d'un toucher au pattern joué (sinon : « la séquence qui joue n'apparaît pas »). */
function PatternBadge() {
  const track = useStore((s) => s.pattern?.track ?? -1);
  const edited = useStore((s) => s.pattern?.pattern ?? -1);
  const playing = useStore((s) => (s.pattern ? s.tick.pat[s.pattern.track]?.[0] ?? -1 : -1));
  const running = useStore((s) => s.tick.t.playing);
  if (track < 0 || edited < 0) return null;
  const other = running && playing >= 0 && playing !== edited;
  return (
    <span className={`steps-pat${other ? " steps-pat--other" : ""}`}>
      Pattern <b>{PATTERN_NAMES[edited]}</b>
      {other && (
        <button type="button" onClick={() => void actions.editPattern(track, playing)}
          title={`La piste joue le pattern ${PATTERN_NAMES[playing]} ; cette rangée montre le pattern ${PATTERN_NAMES[edited]}. Toucher : montrer le pattern joué`}>
          joue {PATTERN_NAMES[playing]} · voir
        </button>
      )}
    </span>
  );
}

export function StepsChoice({ kind, children, dock = false, hidden = false, onHide }: {
  kind: Kind; children: ReactNode; dock?: boolean; hidden?: boolean; onHide?: (hidden: boolean) => void;
}) {
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
        {dock && <small className="steps-dock-title">Pas · ligne 1</small>}
        <PatternBadge />
        <span className="steps-choice-shift">
          {kind === "drum" && <ShiftButtons lane={lane} label={`Pad ${lane + 1}`} what={`le pad ${lane + 1}`} />}
          {!hidden && <ShiftButtons label={kind === "drum" ? "Tout" : "Décaler"} what={kind === "drum" ? "tous les pads" : "la séquence"} />}
          {onHide && <button type="button" className="steps-dock-hide" onClick={() => onHide(!hidden)}
            title={hidden ? "Afficher la rangée de pas au-dessus du clavier" : "Masquer la rangée de pas (plus de place pour les réglages)"}>{hidden ? "Afficher ▴" : "Masquer ▾"}</button>}
        </span>
      </div>
      {hidden ? null : view === "classic" ? children : <LockSteps T={T} name={kind === "synth" ? "Ligne 1" : `Pad ${lane + 1}`} shift="none" melodic={kind === "synth"} />}
    </div>
  );
}
