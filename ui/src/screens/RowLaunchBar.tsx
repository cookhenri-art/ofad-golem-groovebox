// Groovebox — OFAD — AGPL-3.0-or-later
// Bande de lancement des rangées (v0.27) : les 8 rangées A à H de l'écran LANCEMENT, jouables
// depuis le Mixeur et les Scènes sans changer d'écran. Même action que le bouton de rangée de la
// grille (même lettre sur toutes les pistes, scène de réglages liée, calée sur la quantification
// de lancement) ; ■ arrête toutes les pistes au même moment.
// v0.46 : −1 / +1 lancent la rangée précédente / suivante ; clic droit (appui long au doigt) sur un
// bouton : l'affecter au contrôleur MIDI.

import { useEffect } from "react";
import { actions, getState, useStore } from "../engine/store";
import { LAUNCH_QUANTS, MIDI_ROW_BASE, MIDI_ROW_NEXT, MIDI_ROW_PREV, MIDI_ROW_STOP, PATTERN_NAMES, STOP_PATTERN } from "../engine/protocol";
import { useLearnPress } from "../controls/useLearnPress";

export function RowLaunchBar() {
  const launch = useStore((s) => s.launch);
  const tracks = useStore((s) => s.tracks.tracks);
  const pat = useStore((s) => s.tick.pat);
  const playing = useStore((s) => s.tick.t.playing);

  const learn = useLearnPress();
  useEffect(() => { if (!getState().launch) void actions.refreshLaunch(); }, []);
  if (!launch) return null;

  // pistes qui ont un instrument : leur rangée jouée et leur rangée en attente
  const used = tracks.map((t, i) => (t?.instrument ? i : -1)).filter((i) => i >= 0);
  const live = (t: number) => pat[t] ?? [launch.tracks[t]?.playing ?? 0, launch.tracks[t]?.queued ?? -1];
  const rowState = (p: number) => {
    const on = used.filter((t) => live(t)[0] >= 0);
    const current = on.length > 0 && on.every((t) => live(t)[0] === p);
    const queued = used.some((t) => live(t)[1] === p);
    return { current, queued };
  };
  const allStopped = used.every((t) => live(t)[0] < 0);
  const stopAll = () => { for (const t of used) void actions.launchPattern(t, STOP_PATTERN); };
  // rangée de départ pour −1 / +1 : celle que jouent les pistes (sinon A)
  const currentRow = () => { const t = used.find((x) => live(x)[0] >= 0); return t === undefined ? -1 : live(t)[0]; };
  const step = (d: 1 | -1) => { const c = currentRow(); void actions.launchRow(c < 0 ? 0 : (c + d + PATTERN_NAMES.length) % PATTERN_NAMES.length); };

  return (
    <div className="rowlaunch" role="group" aria-label="Lancer une rangée sur toutes les pistes">
      <span className="rowlaunch-label">Rangées <small>· {LAUNCH_QUANTS[launch.quant]}</small></span>
      <button type="button" className="rowlaunch-btn rowlaunch-step" onClick={() => step(-1)} {...learn(MIDI_ROW_PREV, "Rangée précédente")}
        title="Rangée précédente (B → A…) — clic droit : affecter au contrôleur MIDI" aria-label="Rangée précédente">−1</button>
      <button type="button" className="rowlaunch-btn rowlaunch-step" onClick={() => step(1)} {...learn(MIDI_ROW_NEXT, "Rangée suivante")}
        title="Rangée suivante (A → B…) — clic droit : affecter au contrôleur MIDI" aria-label="Rangée suivante">+1</button>
      {PATTERN_NAMES.map((name, p) => {
        const { current, queued } = rowState(p);
        const scene = launch.rowScenes[p] ?? -1;
        return (
          <button key={p} type="button"
            className={`rowlaunch-btn${current ? (playing ? " is-playing" : " is-armed") : ""}${queued ? " is-queued" : ""}`}
            onClick={() => void actions.launchRow(p)} {...learn(MIDI_ROW_BASE - p, `Rangée ${name}`)}
            title={`Lancer ${name} sur toutes les pistes${scene >= 0 ? ` et la scène ${scene + 1}` : ""} — clic droit : affecter au contrôleur MIDI`}
            aria-pressed={current}>
            <span>▶ {name}</span>
            {scene >= 0 && <small>sc. {scene + 1}</small>}
          </button>
        );
      })}
      <button type="button" className={`rowlaunch-btn rowlaunch-stop${allStopped ? " is-stopped" : ""}`} onClick={stopAll}
        {...learn(MIDI_ROW_STOP, "Arrêter toutes les pistes")}
        title="Arrêter toutes les pistes — clic droit : affecter au contrôleur MIDI" aria-label="Arrêter toutes les pistes">■</button>
    </div>
  );
}
