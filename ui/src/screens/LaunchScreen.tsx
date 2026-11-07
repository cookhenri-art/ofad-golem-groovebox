// Groovebox — OFAD — AGPL-3.0-or-later
// Écran LANCEMENT (principe des clips MPC / Force) : 8 patterns par piste, en grille
// pistes × patterns. Toucher une case lance ce pattern sur la piste (calé sur la quantification
// de lancement) et le met en édition ; Maj + toucher : édition seule, sans lancer.
// Bouton de rangée : lance la même lettre sur toutes les pistes, et la scène de réglages liée.
// ■ sous une piste : l'arrêter. Copier : toucher la source, puis la destination (même piste ou une autre piste).
// Rangée entière : « Copier la rangée » puis toucher la rangée de destination ; « Effacer la rangée »
// (confirmé). Les deux portent sur les 16 pistes ; Annuler revient en arrière.
// Onglet MORCEAU : l'arrangeur (rangées jouées à la suite, jeu et mouvements enregistrés).

import { useEffect, useRef, useState } from "react";
import { actions, getState, setUi, showMessage, useStore } from "../engine/store";
import { LAUNCH_QUANTS, PATTERN_NAMES, ROW_PAGE, ROW_PAGES, STEPS_PER_BAR, STOP_PATTERN, rowPageName } from "../engine/protocol";
import { trackColor } from "../engine/params";
import { steppedBinding } from "../controls/bindings";
import { ScreenFrame } from "./ScreenFrame";
import { SongView } from "./SongView";
import { MontageView } from "./MontageView";

let cellClipboard: { track: number; pattern: number } | null = null; // Ctrl+C / Ctrl+V des cases

export function LaunchScreen() {
  const launch = useStore((s) => s.launch);
  const tracks = useStore((s) => s.tracks.tracks);
  const pat = useStore((s) => s.tick.pat);
  const playing = useStore((s) => s.tick.t.playing);
  const focus = useStore((s) => s.focus);
  const shift = useStore((s) => s.ui.shift);
  const [bank, setBank] = useState(Math.floor(Math.min(focus, 15) / 8));
  const [row, setRowState] = useState(0);
  // v0.46 fix29 : la grille montre une page de 8 rangées (A–H ou I–P), la même que la bande des rangées
  const rowPage = useStore((s) => s.ui.rowPage);
  const setRow = (r: number) => { setRowState(r); const pg = Math.floor(r / ROW_PAGE); if (pg !== getState().ui.rowPage) setUi({ rowPage: pg }); };
  const pageRows = Array.from({ length: ROW_PAGE }, (_, i) => rowPage * ROW_PAGE + i);
  const [copyFrom, setCopyFrom] = useState<{ track: number; pattern: number } | null>(null);
  const [rowCopy, setRowCopy] = useState<number | null>(null);
  const [confirmClear, setConfirmClear] = useState(false);
  const view = useStore((s) => s.ui.launchView);
  const all = useStore((s) => s.ui.launchAll); // les 16 pistes en défilement (réglage)
  const hscroll = useRef<HTMLDivElement>(null);
  // la piste choisie reste visible quand on fait défiler la grille
  useEffect(() => {
    if (!all) return;
    hscroll.current?.querySelector<HTMLElement>(`[data-col="${Math.min(focus, 15)}"]`)?.scrollIntoView({ inline: "nearest", block: "nearest" });
  }, [all, focus]);
  // v0.23 : les boutons M / S sous les encodeurs suivent les 8 pistes visibles de la grille
  // (comme au mixeur) ; une colonne = 104 px + 4 px d'espace
  const syncMuteRow = () => {
    const el = hscroll.current;
    if (!el) return;
    const k = all ? Math.max(0, Math.min(8, Math.round(el.scrollLeft / 108))) : bank * 8;
    if (k !== getState().ui.mixerFirst) setUi({ mixerFirst: k });
  };
  useEffect(syncMuteRow, [all, bank]);
  const song = useStore((s) => s.song);
  const [songSel, setSongSel] = useState(-1);

  useEffect(() => { void actions.refreshLaunch(); }, []);

  // Ctrl+C / Ctrl+V sur les cases : copie la case choisie (piste et pattern édités), colle sur la
  // case choisie ensuite — sur la même piste ou une autre
  useEffect(() => {
    if (view !== "grid") return;
    const onKey = (e: KeyboardEvent) => {
      if (!(e.ctrlKey || e.metaKey) || e.altKey || e.repeat) return;
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement || e.target instanceof HTMLSelectElement) return;
      const st = getState();
      const t = Math.min(st.focus, 15);
      const p = st.launch?.tracks[t]?.editing ?? 0;
      if (e.code === "KeyC") {
        e.preventDefault();
        cellClipboard = { track: t, pattern: p };
        showMessage(`Pattern ${PATTERN_NAMES[p]} (piste ${t + 1}) copié · touchez une case puis Ctrl+V`);
      } else if (e.code === "KeyV" && cellClipboard) {
        e.preventDefault();
        const c = cellClipboard;
        if (c.track === t && c.pattern === p) { showMessage("Touchez d'abord la case de destination"); return; }
        if (c.track === t) void actions.copyPattern(t, c.pattern, p);
        else void actions.copyPatternTo(c.track, c.pattern, t, p);
        showMessage(`${PATTERN_NAMES[c.pattern]} (piste ${c.track + 1}) collé en ${PATTERN_NAMES[p]} (piste ${t + 1})`);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [view]);

  if (!launch) return <ScreenFrame title="Clips" encoders={[]}><div className="empty-screen"><h2>Chargement…</h2></div></ScreenFrame>;

  const tabs = [{ id: "grid", label: "Grille" }, { id: "song", label: "Morceau" }, { id: "audio", label: "Audio" }];
  type View = "grid" | "song" | "audio";
  if (view === "audio") {
    // v0.46 fix8 : montage audio
    return (
      <ScreenFrame title="Clips" encoders={[]} tabs={tabs} tab="audio" onTab={(id) => setUi({ launchView: id as View })}
        status={<MontageChip />}>
        <MontageView />
      </ScreenFrame>
    );
  }
  if (view === "song") {
    const rows = song?.rows ?? [];
    const sr = songSel >= 0 && songSel < rows.length ? rows[songSel] : null;
    const setRowField = (k: 0 | 1, v: number) => {
      if (!sr) return;
      void actions.setSongRows(rows.map((r, i) => (i === songSel ? (k === 0 ? [v, r[1], r[2]] : [r[0], v, r[2]]) as typeof r : r)));
    };
    const songEncoders = [
      steppedBinding("Rangée", Array.from({ length: Math.max(1, rows.length) }, (_, i) => i), Math.max(0, songSel), setSongSel, (r) => (rows.length ? `${r + 1} / ${rows.length}` : "aucune")),
      sr ? steppedBinding("Mesures", Array.from({ length: 64 }, (_, i) => i + 1), sr[0], (v) => setRowField(0, v), (v) => `${v} mes.`) : null,
      sr ? steppedBinding("Scène", [-1, ...Array.from({ length: 32 }, (_, i) => i)], sr[1], (v) => setRowField(1, v), (v) => (v < 0 ? "aucune" : `scène ${v + 1}`)) : null,
    ];
    return (
      <ScreenFrame title="Clips" encoders={songEncoders} tabs={tabs} tab="song" onTab={(id) => setUi({ launchView: id as View })}
        status={<span className="chip">{song?.mode ? "Mode morceau" : "Jeu libre"}</span>}>
        <SongView sel={songSel} setSel={setSongSel} />
      </ScreenFrame>
    );
  }

  const cols = all ? Array.from({ length: 16 }, (_, i) => i) : Array.from({ length: 8 }, (_, i) => bank * 8 + i);
  const ft = Math.min(focus, 15);
  const editing = launch.tracks[ft]?.editing ?? 0;
  const scenes = [-1, ...Array.from({ length: 32 }, (_, i) => i)];

  const press = (t: number, p: number) => {
    if (copyFrom) {
      // même piste : autre emplacement ; autre piste : le même motif sur cet instrument
      if (copyFrom.track === t) { if (copyFrom.pattern !== p) void actions.copyPattern(t, copyFrom.pattern, p); }
      else {
        const src = tracks[copyFrom.track]?.instrument, dst = tracks[t]?.instrument;
        void actions.copyPatternTo(copyFrom.track, copyFrom.pattern, t, p);
        showMessage(`${PATTERN_NAMES[copyFrom.pattern]} (piste ${copyFrom.track + 1}) copié vers ${PATTERN_NAMES[p]} (piste ${t + 1})${src && dst && src !== dst ? " · réglages verrouillés propres à l'autre instrument retirés" : ""}`);
      }
      setCopyFrom(null);
      return;
    }
    if (getState().focus !== t) void actions.focus(t);
    void actions.editPattern(t, p);
    if (!shift) void actions.launchPattern(t, p);
  };

  const encoders = [
    steppedBinding("Piste", Array.from({ length: 16 }, (_, i) => i), ft, (t) => { void actions.focus(t); setBank(Math.floor(t / 8)); }, (t) => `${t + 1} · ${tracks[t]?.name ?? ""}`),
    steppedBinding("Pattern édité", PATTERN_NAMES.map((_, i) => i), editing, (p) => { void actions.editPattern(ft, p); const pg = Math.floor(p / ROW_PAGE); if (pg !== getState().ui.rowPage) setUi({ rowPage: pg }); }, (p) => PATTERN_NAMES[p]),
    steppedBinding("Lancement", [0, 1, 2, 3, 4], launch.quant, (q) => void actions.setLaunchQuant(q), (q) => LAUNCH_QUANTS[q]),
    all ? null : steppedBinding("Pistes", [0, 1], bank, setBank, (b) => (b === 0 ? "1 – 8" : "9 – 16")),
    steppedBinding("Rangée", PATTERN_NAMES.map((_, i) => i), row, setRow, (r) => PATTERN_NAMES[r]),
    steppedBinding("Scène liée", scenes, launch.rowScenes[row] ?? -1, (s) => void actions.setRowScene(row, s), (s) => (s < 0 ? "aucune" : `scène ${s + 1}`)),
  ];

  return (
    <ScreenFrame title="Clips" encoders={encoders} tabs={tabs} tab="grid" onTab={(id) => setUi({ launchView: id as View })}
      status={<span className="chip">Lancement · {LAUNCH_QUANTS[launch.quant]}</span>}>
      <div className="launch">
        <div className={`launch-hscroll${all ? " is-all" : ""}`} ref={hscroll} onScroll={syncMuteRow}
          onWheel={(e) => { const el = e.currentTarget; if (all && !e.shiftKey && el.scrollWidth > el.clientWidth && Math.abs(e.deltaY) > Math.abs(e.deltaX)) el.scrollLeft += e.deltaY; }}>
        <div className="launch-grid" style={{ gridTemplateColumns: all ? `64px repeat(16, 104px)` : `64px repeat(8, 1fr)` }}>
          <span className="launch-pages" role="group" aria-label="Page de rangées">
            {Array.from({ length: ROW_PAGES }, (_, pg) => (
              <button key={pg} type="button" className={`launch-page${pg === rowPage ? " on" : ""}`} aria-pressed={pg === rowPage}
                onClick={() => setUi({ rowPage: pg })} title={`Rangées ${rowPageName(pg)}`}>{rowPageName(pg)}</button>
            ))}
          </span>
          {cols.map((t) => (
            <button key={t} type="button" data-col={t} className={`launch-head${t === ft ? " is-focus" : ""}`} style={{ "--k": trackColor(tracks[t]?.color ?? 0) } as React.CSSProperties}
              onClick={() => void actions.focus(t)}>
              <span className="launch-num tabular">{t + 1}</span>
              <span className="launch-name">{tracks[t]?.instrument ? tracks[t].name : "Libre"}</span>
            </button>
          ))}

          {pageRows.map((p) => { const name = PATTERN_NAMES[p]; return (
            <div key={p} className="launch-row" style={{ display: "contents" }}>
              <button type="button" className={`launch-rowbtn${row === p ? " is-sel" : ""}${rowCopy === p ? " is-source" : ""}${rowCopy !== null && rowCopy !== p ? " is-target" : ""}`}
                onClick={() => {
                  if (rowCopy !== null) { if (rowCopy !== p) void actions.copyRow(rowCopy, p); setRowCopy(null); setRow(p); return; }
                  setRow(p); setConfirmClear(false); void actions.launchRow(p);
                }}
                title={`Lancer ${name} sur toutes les pistes${(launch.rowScenes[p] ?? -1) >= 0 ? ` et la scène ${launch.rowScenes[p] + 1}` : ""}`}>
                <span>▶ {name}</span>
                {(launch.rowScenes[p] ?? -1) >= 0 && <small>scène {launch.rowScenes[p] + 1}</small>}
              </button>
              {cols.map((t) => {
                const lt = launch.tracks[t];
                const live = pat[t] ?? [lt?.playing ?? 0, lt?.queued ?? -1];
                const filled = !!lt?.filled[p];
                const isPlaying = live[0] === p;
                const isQueued = live[1] === p;
                const isEditing = lt?.editing === p;
                const isSource = copyFrom?.track === t && copyFrom.pattern === p;
                const cls = ["launch-cell", filled ? "is-filled" : "", isPlaying && tracks[t]?.instrument ? (playing && filled ? "is-playing" : "is-armed") : "", isQueued ? "is-queued" : "",
                  isEditing ? "is-editing" : "", isSource ? "is-source" : "", !tracks[t]?.instrument ? "is-free" : ""].join(" ");
                return (
                  <button key={t} type="button" className={cls} style={{ "--k": trackColor(tracks[t]?.color ?? 0) } as React.CSSProperties}
                    onClick={() => press(t, p)}
                    onContextMenu={(e) => { e.preventDefault(); setCopyFrom({ track: t, pattern: p }); }}
                    aria-label={`Piste ${t + 1}, pattern ${name}${filled ? "" : " (vide)"}${isPlaying ? ", joué" : ""}${isQueued ? ", en attente" : ""}`}>
                    <span className="launch-letter">{name}</span>
                    {filled && <span className="launch-len tabular">{`${Math.max(1, Math.round((lt?.length[p] ?? 16) / (STEPS_PER_BAR[lt?.division?.[p] ?? 2] ?? 16)))} mes.`}</span>}
                  </button>
                );
              })}
            </div>
          ); })}

          <span className="launch-foot-label">Arrêt</span>
          {cols.map((t) => {
            const live = pat[t] ?? [0, -1];
            return (
              <button key={t} type="button" className={`launch-stop${live[0] < 0 ? " is-stopped" : ""}${live[1] === STOP_PATTERN ? " is-queued" : ""}`}
                onClick={() => void actions.launchPattern(t, STOP_PATTERN)} aria-label={`Arrêter la piste ${t + 1}`}>■</button>
            );
          })}
        </div>
        </div>

        <div className="launch-actions">
          <button type="button" className={`touch${copyFrom ? " touch--on" : ""}`}
            onClick={() => setCopyFrom(copyFrom ? null : { track: ft, pattern: editing })}>
            {copyFrom ? `Copier ${PATTERN_NAMES[copyFrom.pattern]} (piste ${copyFrom.track + 1}) vers… touchez une case` : `Copier le pattern ${PATTERN_NAMES[editing]}`}
          </button>
          <button type="button" className="touch touch--quiet" onClick={() => void actions.clearPattern(ft, editing)}>Effacer le pattern {PATTERN_NAMES[editing]}</button>
          <button type="button" className="touch touch--quiet" onClick={() => setUi({ screen: "sequence" })}>Éditer dans SÉQUENCE</button>
          <span className="launch-sep" aria-hidden />
          <button type="button" className={`touch${rowCopy !== null ? " touch--on" : ""}`} onClick={() => { setRowCopy(rowCopy !== null ? null : row); setConfirmClear(false); setCopyFrom(null); }}>
            {rowCopy !== null ? `Rangée ${PATTERN_NAMES[rowCopy]} vers… touchez une rangée` : `Copier la rangée ${PATTERN_NAMES[row]}`}
          </button>
          {confirmClear
            ? <>
                <button type="button" className="touch touch--on" onClick={() => { void actions.clearRow(row); setConfirmClear(false); }}>Effacer {PATTERN_NAMES[row]} sur les 16 pistes</button>
                <button type="button" className="touch touch--quiet" onClick={() => setConfirmClear(false)}>Annuler</button>
              </>
            : <button type="button" className="touch touch--quiet" onClick={() => { setConfirmClear(true); setRowCopy(null); }}>Effacer la rangée {PATTERN_NAMES[row]}</button>}
          <span className="launch-hint">Toucher : lancer et éditer · Maj : sans lancer · Ctrl+C / Ctrl+V : copier une case · clic droit : copier depuis cette case</span>
        </div>
      </div>
    </ScreenFrame>
  );
}

/** v0.46 fix8 : état du montage dans l'en-tête de l'écran. */
function MontageChip() {
  const on = useStore((s) => s.montage?.on ?? false);
  const rec = useStore((s) => s.tick.t.mrec ?? false);
  return <span className="chip">{rec ? "● Prise en cours" : on ? "Mode montage" : "Instruments"}</span>;
}
