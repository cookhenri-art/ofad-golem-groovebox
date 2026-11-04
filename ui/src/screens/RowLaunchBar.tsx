// Groovebox — OFAD — AGPL-3.0-or-later
// Bande de lancement des rangées (v0.27) : les 8 rangées A à H de l'écran LANCEMENT, jouables
// depuis le Mixeur et les Scènes sans changer d'écran. Même action que le bouton de rangée de la
// grille (même lettre sur toutes les pistes, scène de réglages liée, calée sur la quantification
// de lancement) ; ■ arrête toutes les pistes au même moment.
// v0.46 : −1 / +1 lancent la rangée précédente / suivante ; clic droit (appui long au doigt) sur un
// bouton : l'affecter au contrôleur MIDI.
// v0.46 fix29 : 16 rangées (A à P) en deux pages de 8 sur une seule ligne. ◀ / ▶ tournent la page ;
// −1 / +1 passent d'une page à l'autre tout seuls (H → I) en sautant les rangées vides ; la page suit
// aussi la rangée lancée au contrôleur MIDI. La page est la même partout (Mixeur, Scènes, vue live,
// grille de LANCEMENT).

import { useEffect, useRef } from "react";
import { actions, getState, setUi, useStore } from "../engine/store";
import { LAUNCH_QUANTS, MIDI_ROW_BASE, MIDI_ROW_NEXT, MIDI_ROW_PREV, MIDI_ROW_STOP, PATTERN_NAMES, ROW_PAGE, ROW_PAGES, STOP_PATTERN, rowPageName } from "../engine/protocol";
import { useLearnPress } from "../controls/useLearnPress";

export function RowLaunchBar() {
  const launch = useStore((s) => s.launch);
  const tracks = useStore((s) => s.tracks.tracks);
  const pat = useStore((s) => s.tick.pat);
  const playing = useStore((s) => s.tick.t.playing);
  const page = useStore((s) => s.ui.rowPage);

  const learn = useLearnPress();
  useEffect(() => { if (!getState().launch) void actions.refreshLaunch(); }, []);
  // la page suit la rangée que jouent les pistes quand elle change (rangée lancée au contrôleur, au morceau…)
  const lastRow = useRef(-1);
  const playedRow = (() => {
    if (!launch) return -1;
    for (let t = 0; t < tracks.length; ++t) {
      if (!tracks[t]?.instrument) continue;
      const p = (pat[t] ?? [launch.tracks[t]?.playing ?? -1])[0];
      if (p >= 0) return p;
    }
    return -1;
  })();
  useEffect(() => {
    if (playedRow >= 0 && playedRow !== lastRow.current) {
      const pg = Math.floor(playedRow / ROW_PAGE);
      if (pg !== getState().ui.rowPage) setUi({ rowPage: pg });
    }
    lastRow.current = playedRow;
  }, [playedRow]);
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
  // v0.46 fix29 : une rangée est écrite si une piste a des notes dans ce pattern ; −1 / +1 sautent les vides
  const N = PATTERN_NAMES.length;
  const rowUsed = (p: number) => used.some((t) => !!launch.tracks[t]?.filled[p]);
  const step = (d: 1 | -1) => {
    const c = currentRow();
    if (c < 0) { void actions.launchRow(page * ROW_PAGE); return; }
    for (let k = 1; k <= N; ++k) {
      const r = (c + d * k + N * 2) % N;
      if (rowUsed(r) || k === N) { void actions.launchRow(r); return; }
    }
  };
  const turn = (d: 1 | -1) => setUi({ rowPage: (page + d + ROW_PAGES) % ROW_PAGES });
  const rows = Array.from({ length: ROW_PAGE }, (_, i) => page * ROW_PAGE + i);
  // la rangée jouée (ou en attente) est sur l'autre page : les boutons de page le montrent
  const others = PATTERN_NAMES.map((_, p) => p).filter((p) => Math.floor(p / ROW_PAGE) !== page);
  const pageCls = `${others.some((p) => rowState(p).current) ? " is-elsewhere" : ""}${others.some((p) => rowState(p).queued) ? " is-queued" : ""}`;

  return (
    <div className="rowlaunch" role="group" aria-label="Lancer une rangée sur toutes les pistes">
      <span className="rowlaunch-label">Rangées <small>· {LAUNCH_QUANTS[launch.quant]}</small></span>
      <button type="button" className="rowlaunch-btn rowlaunch-step" onClick={() => step(-1)} {...learn(MIDI_ROW_PREV, "Rangée précédente")}
        title="Rangée précédente (B → A, I → H…), en sautant les rangées vides — clic droit : affecter au contrôleur MIDI" aria-label="Rangée précédente">−1</button>
      <button type="button" className="rowlaunch-btn rowlaunch-step" onClick={() => step(1)} {...learn(MIDI_ROW_NEXT, "Rangée suivante")}
        title="Rangée suivante (A → B, H → I…), en sautant les rangées vides — clic droit : affecter au contrôleur MIDI" aria-label="Rangée suivante">+1</button>
      <button type="button" className={`rowlaunch-btn rowlaunch-page${pageCls}`}
        onClick={() => turn(-1)} title={`Page de rangées précédente (${rowPageName((page + ROW_PAGES - 1) % ROW_PAGES)})`} aria-label="Page de rangées précédente">
        <span>◀</span><small>{rowPageName((page + ROW_PAGES - 1) % ROW_PAGES)}</small>
      </button>
      {rows.map((p) => {
        const name = PATTERN_NAMES[p];
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
      <button type="button" className={`rowlaunch-btn rowlaunch-page${pageCls}`} onClick={() => turn(1)}
        title={`Page de rangées suivante (${rowPageName((page + 1) % ROW_PAGES)})`} aria-label="Page de rangées suivante">
        <span>▶</span><small>{rowPageName((page + 1) % ROW_PAGES)}</small>
      </button>
      <button type="button" className={`rowlaunch-btn rowlaunch-stop${allStopped ? " is-stopped" : ""}`} onClick={stopAll}
        {...learn(MIDI_ROW_STOP, "Arrêter toutes les pistes")}
        title="Arrêter toutes les pistes — clic droit : affecter au contrôleur MIDI" aria-label="Arrêter toutes les pistes">■</button>
    </div>
  );
}
