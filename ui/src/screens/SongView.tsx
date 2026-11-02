// Groovebox — OFAD — AGPL-3.0-or-later
// MORCEAU (arrangeur, principe repris de Sillage) : une ligne de temps de rangées jouées l'une
// après l'autre en mode morceau. Chaque rangée dure N mesures et dit, pour chaque piste, le pattern
// à lancer (A à H), l'arrêt ou « garder » ce qui joue, plus une scène de réglages.
//  - « Enregistrer le jeu » : en jouant (mode morceau éteint), les lancements de patterns
//    deviennent des rangées, mesure par mesure ;
//  - « Enregistrer les mouvements » : les boutons bougés (écran ou contrôleur) deviennent des
//    courbes, relues ensuite en ligne droite ; un bouton tenu à la main fait taire sa courbe.

import { useEffect, useMemo, useRef, useState } from "react";
import { CurveEditor } from "../controls/CurveEditor";
import { resolveGlobal } from "../engine/store";
import { formatNorm } from "../engine/params";
import { actions, useStore } from "../engine/store";
import { PATTERN_NAMES, SONG_KEEP, SONG_MAX_BARS, SONG_MAX_ROWS, SONG_STOP, type SongRowData } from "../engine/protocol";
import { trackColor } from "../engine/params";
import { anySwitchLaneOf, describeGlobal, isFaderLane } from "../engine/describe";

const BAR_ZOOMS = [6, 9, 13, 18, 25, 34, 48, 68]; // largeur d'une mesure (px) : du plus loin au plus près

const patLabel = (v: number) => (v === SONG_KEEP ? "·" : v === SONG_STOP ? "■" : PATTERN_NAMES[v] ?? "?");

export function SongView({ sel, setSel }: { sel: number; setSel: (r: number) => void }) {
  const song = useStore((s) => s.song);
  const tracks = useStore((s) => s.tracks.tracks);
  const tickSong = useStore((s) => s.tick.song);
  const playing = useStore((s) => s.tick.t.playing);
  const [pick, setPick] = useState<{ row: number; track: number } | null>(null);
  const [confirmClear, setConfirmClear] = useState<"rows" | "lanes" | null>(null);
  const [BAR_W, setBarW] = useState(34);
  const [help, setHelp] = useState(false);
  const [editLane, setEditLane] = useState<{ g: number; pts: number[] } | null>(null);
  const scroller = useRef<HTMLDivElement>(null);
  const zoomBy = (d: number) => setBarW((w) => { const i = BAR_ZOOMS.indexOf(w); return BAR_ZOOMS[Math.max(0, Math.min(BAR_ZOOMS.length - 1, (i < 0 ? 5 : i) + d))]; });
  // Ctrl + molette : zoom de la ligne de temps
  useEffect(() => {
    const el = scroller.current;
    if (!el) return;
    const wheel = (e: WheelEvent) => { if (!e.ctrlKey) return; e.preventDefault(); zoomBy(e.deltaY < 0 ? 1 : -1); };
    el.addEventListener("wheel", wheel, { passive: false });
    return () => el.removeEventListener("wheel", wheel);
  });
  const openLane = async (g: number) => {
    if (editLane?.g === g) { setEditLane(null); return; }
    setEditLane({ g, pts: await actions.getSongLane(g) });
  };

  useEffect(() => { void actions.refreshSong(); }, []);

  const rows = song?.rows ?? [];
  const starts = useMemo(() => { const out: number[] = []; let b = 0; rows.forEach((r) => { out.push(b); b += r[0]; }); out.push(b); return out; }, [rows]);
  const totalBars = starts[starts.length - 1] ?? 0;
  const span = Math.max(song?.span ?? 16, totalBars * 4);
  const width = Math.max(totalBars, Math.ceil(span / 4)) * BAR_W;
  const cols = Array.from({ length: 16 }, (_, i) => i); // les 16 pistes, on descend à la molette
  const rowNow = tickSong?.[0] ?? -1;
  const beat = tickSong?.[1] ?? -1;
  const recPlay = (tickSong?.[3] ?? (song?.recPlay ? 1 : 0)) === 1;
  const recMoves = (tickSong?.[4] ?? (song?.recMoves ? 1 : 0)) === 1;

  if (!song) return <div className="empty-screen"><h2>Morceau</h2><p>Disponible dans l'application.</p></div>;

  const editRows = (next: SongRowData[]) => void actions.setSongRows(next);
  const setRow = (i: number, f: (r: SongRowData) => SongRowData) => editRows(rows.map((r, k) => (k === i ? f([r[0], r[1], [...r[2]]]) : r)));
  const selRow = sel >= 0 && sel < rows.length ? rows[sel] : null;
  const cycleOrder = [SONG_KEEP, 0, 1, 2, 3, 4, 5, 6, 7, SONG_STOP];
  // boucle : première et dernière rangée répétées (la dernière -1 : jusqu'à la fin)
  const loopFirst = rows.length ? Math.min(Math.max(0, song.loopFrom ?? 0), rows.length - 1) : 0;
  const loopToRaw = song.loopTo ?? -1;
  const loopLast = rows.length ? Math.max(loopFirst, loopToRaw < 0 || loopToRaw >= rows.length ? rows.length - 1 : loopToRaw) : 0;
  const setLoop = (from: number, to: number) => void actions.setSongLoopRange(from, to);
  const loopToEnd = loopToRaw < 0 || loopToRaw >= rows.length - 1;
  /** Indices de la boucle après l'insertion (+1) ou la suppression (-1) de la rangée « at ». */
  const shiftLoop = (at: number, d: 1 | -1) => {
    const f = song.loopFrom ?? 0, t = song.loopTo ?? -1;
    // insertion : les rangées à partir de « at » glissent d'un cran ; suppression : celles d'après reculent
    const nf = d > 0 ? (at <= f ? f + 1 : f) : (at < f ? f - 1 : f);
    const nt = t < 0 ? -1 : d > 0 ? (at <= t ? t + 1 : t) : Math.max(nf, at <= t ? t - 1 : t);
    if (nf !== f || nt !== t) setLoop(nf, nt);
  };

  return (
    <div className="song">
      <div className="song-bar">
        <button type="button" className={`touch touch--small${song.mode ? " touch--on" : ""}`} onClick={() => void actions.setSongFlags(!song.mode, song.loop, song.read)}
          title="Allumé : les rangées lancent les patterns à leur mesure, du début à la fin">Mode morceau</button>
        <button type="button" className={`touch touch--small${song.loop ? " touch--on" : ""}`} onClick={() => void actions.setSongFlags(song.mode, !song.loop, song.read)}
          title="Allumé : les rangées choisies se répètent (sinon arrêt en fin de morceau)">Boucle</button>
        {song.loop && rows.length > 1 && (
          <span className="song-loopsel" role="group" aria-label="Rangées bouclées">
            <label>de
              <select value={loopFirst} onChange={(e) => { const f = Number(e.target.value); setLoop(f, loopToEnd ? -1 : Math.max(f, loopLast)); }} aria-label="Première rangée de la boucle">
                {rows.map((_, i) => <option key={i} value={i}>{i + 1}</option>)}
              </select>
            </label>
            <label>à
              <select value={loopToEnd ? -1 : loopLast} onChange={(e) => { const t = Number(e.target.value); setLoop(t >= 0 ? Math.min(loopFirst, t) : loopFirst, t); }} aria-label="Dernière rangée de la boucle">
                {rows.map((_, i) => i >= loopFirst && i < rows.length - 1 ? <option key={i} value={i}>{i + 1}</option> : null)}
                <option value={-1}>{rows.length} (fin)</option>
              </select>
            </label>
            {(loopFirst > 0 || !loopToEnd) && <button type="button" className="touch touch--small touch--quiet" onClick={() => setLoop(0, -1)} title="Boucler tout le morceau">Tout</button>}
          </span>
        )}
        {song.loop && rows.length > 0 && (
          <label className="song-loopsel" title="Nombre de passages de la boucle ; ensuite le morceau joue les rangées qui suivent la boucle et s'arrête. ∞ : la boucle tourne sans fin.">
            passages
            <select value={song.loopCount ?? 0} onChange={(e) => void actions.setSongLoopCount(Number(e.target.value))} aria-label="Nombre de passages de la boucle">
              <option value={0}>∞</option>
              {[...new Set([1, 2, 3, 4, 6, 8, 12, 16, 24, 32, song.loopCount ?? 0])].filter((n) => n > 0).sort((x, y) => x - y)
                .map((n) => <option key={n} value={n}>{n}×</option>)}
            </select>
          </label>
        )}
        <span className="launch-sep" aria-hidden />
        <button type="button" className={`touch touch--small song-rec${recPlay ? " is-on" : ""}`} disabled={song.mode}
          onClick={() => void actions.setSongRecord(!recPlay, recMoves)}
          title={song.mode ? "Éteindre le mode morceau pour enregistrer le jeu" : "En jouant, chaque mesure et ses patterns s'ajoutent en rangées à la fin du morceau"}>● Enr. le jeu</button>
        <button type="button" className={`touch touch--small song-rec${recMoves ? " is-on" : ""}`} onClick={() => void actions.setSongRecord(recPlay, !recMoves)}
          title="En lecture, chaque bouton bougé (écran ou contrôleur), chaque fader, le crossfader des scènes, chaque coupure (M) et solo (S) de piste, de groupe ou de retour écrit sa courbe ; les anciens points recouverts sont remplacés. Allumé, il l'emporte sur ENR. : tout va au morceau">● Enr. les mouvements</button>
        <button type="button" className={`touch touch--small${song.read ? " touch--on" : ""}`} onClick={() => void actions.setSongFlags(song.mode, song.loop, !song.read)}
          title="Les courbes enregistrées font bouger les réglages en lecture">Relire les courbes</button>
        <button type="button" className={`touch touch--small${help ? " touch--on" : ""}`} onClick={() => setHelp(!help)} aria-expanded={help} title="Comment ça marche">?</button>
        <span className="song-zoom" role="group" aria-label="Zoom de la ligne de temps">
          <button type="button" className="touch touch--small" onClick={() => zoomBy(-1)} disabled={BAR_W <= BAR_ZOOMS[0]} aria-label="Dézoomer" title="Dézoomer (Ctrl + molette)">−</button>
          <button type="button" className="touch touch--small" title="Tout le morceau dans la largeur"
            onClick={() => { const w = (scroller.current?.clientWidth ?? 800) - 180; const bars = Math.max(1, Math.max(totalBars, Math.ceil(span / 4))); setBarW(Math.max(3, Math.min(68, Math.floor(w / bars)))); }}>Tout voir</button>
          <button type="button" className="touch touch--small" onClick={() => zoomBy(1)} disabled={BAR_W >= BAR_ZOOMS[BAR_ZOOMS.length - 1]} aria-label="Zoomer" title="Zoomer (Ctrl + molette)">+</button>
        </span>
      </div>

      {help && (
        <div className="song-help">
          <p><b>Mode morceau</b> : les rangées jouent l'une après l'autre (chaque rangée dit quel pattern joue sur chaque piste, pendant N mesures). Éteint : vous lancez les patterns à la main (Grille, pads).</p>
          <p><b>● Enr. le jeu</b> (mode morceau éteint) : lancez la lecture et jouez avec la Grille ; chaque mesure jouée devient une rangée. Rallumez « Mode morceau » pour rejouer ce que vous avez fait.</p>
          <p><b>● Enr. les mouvements</b> : en lecture, tout bouton tourné (écran ou contrôleur), tout fader, le crossfader des scènes, et chaque coupure (M) ou solo (S) de piste, de groupe ou de retour dessine sa courbe sur la ligne de temps du morceau, dans la partie « Courbes » en bas. Touchez le nom d'une courbe pour la voir en grand et la redessiner. Allumé, il l'emporte sur ENR. (qui, seul, écrit les courbes du pattern joué).</p>
          <p><b>Relire les courbes</b> : allumé, les courbes font bouger les boutons pendant la lecture ; éteint, elles sont gardées mais ne jouent pas. Un bouton tenu à la main passe toujours avant sa courbe.</p>
          <p><b>ENR.</b> suit le mode : <b>mode morceau allumé</b>, tout ce que vous bougez va au morceau (comme « Enr. les mouvements ») ; <b>en boucle</b> (mode morceau éteint), les boutons et tirettes d'une piste s'écrivent dans la boucle du pattern qu'elle joue (écran Séquence › Courbes), pour un mouvement qui revient à chaque tour.</p>
          <p>« Enr. le jeu » et « Enr. les mouvements » marchent ensemble, en une seule prise : les courbes tombent sur les rangées enregistrées. Changer la longueur d'une rangée, en insérer ou en supprimer emporte ses courbes ; les rangées suivantes gardent les leurs.</p>
        </div>
      )}

      <div className="song-scroll-wrap">
        {/* v0.46 fix6 : la mascotte, en grand, dans la partie vide de la ligne de temps */}
        <img className="song-golem" src="golem/golem-512.png" alt="" draggable={false} />
      <div className="song-scroll" ref={scroller}>
        <div className="song-lines" style={{ width: width + 64 }}>
          {/* rangées */}
          <div className="song-line song-line--head">
            <span className="song-lab song-lab--pos tabular">{playing && beat >= 0 ? `${rowNow >= 0 ? `Rangée ${rowNow + 1} · ` : ""}mes. ${Math.floor(beat / 4) + 1}.${Math.floor(beat % 4) + 1}` : `${rows.length} rangée${rows.length > 1 ? "s" : ""} · ${totalBars} mes.`}</span>
            <div className="song-track" style={{ width }}>
              {rows.map((r, i) => (
                <button key={i} type="button" className={`song-row${i === sel ? " is-sel" : ""}${i === rowNow ? " is-now" : ""}`}
                  style={{ left: starts[i] * BAR_W, width: r[0] * BAR_W - 2 }} onClick={() => setSel(i === sel ? -1 : i)}
                  title={`Rangée ${i + 1} : ${r[0]} mesure${r[0] > 1 ? "s" : ""}${r[1] >= 0 ? `, scène ${r[1] + 1}` : ""}`}>
                  <b>{i + 1}</b><span>{r[0]} m.{r[1] >= 0 ? ` · S${r[1] + 1}` : ""}</span>
                </button>
              ))}
              {song.loop && rows.length > 0 && (
                <span className="song-loop" style={{ left: starts[loopFirst] * BAR_W, width: (starts[loopLast + 1] - starts[loopFirst]) * BAR_W - 2 }}
                  title={`Boucle : rangée${loopFirst === loopLast ? ` ${loopFirst + 1}` : `s ${loopFirst + 1} à ${loopLast + 1}`}`} aria-hidden />
              )}
              {beat >= 0 && song.mode && <span className="song-head" style={{ left: (beat / 4) * BAR_W }} aria-hidden />}
            </div>
          </div>
          {/* pistes */}
          {cols.map((t) => (
            <div key={t} className="song-line" style={{ "--k": trackColor(tracks[t]?.color ?? 0) } as React.CSSProperties}>
              <span className="song-lab">{t + 1} · {tracks[t]?.instrument ? tracks[t].name : "Libre"}</span>
              <div className="song-track" style={{ width }}>
                {rows.map((r, i) => {
                  const v = r[2][t] ?? SONG_KEEP;
                  return (
                    <button key={i} type="button" className={`song-cell${v >= 0 ? " is-pat" : v === SONG_STOP ? " is-stop" : ""}${pick?.row === i && pick.track === t ? " is-pick" : ""}`}
                      style={{ left: starts[i] * BAR_W, width: r[0] * BAR_W - 2 }}
                      onClick={() => { setSel(i); setPick(pick?.row === i && pick.track === t ? null : { row: i, track: t }); }}
                      onContextMenu={(e) => { e.preventDefault(); const k = cycleOrder.indexOf(v); setRow(i, (x) => { x[2][t] = cycleOrder[(k + 1) % cycleOrder.length]; return x; }); }}
                      aria-label={`Rangée ${i + 1}, piste ${t + 1} : ${v === SONG_KEEP ? "garder" : v === SONG_STOP ? "arrêt" : `pattern ${patLabel(v)}`}`}>
                      {patLabel(v)}
                    </button>
                  );
                })}
              </div>
            </div>
          ))}
          {/* courbes */}
          <div className="song-line song-line--title">
            <span className="song-lab">Courbes</span>
            <div className="song-track song-track--note" style={{ width }}>
              {song.lanes.length === 0 && <span>Aucune courbe. Allumez « ● Enr. les mouvements », lancez la lecture et tournez un bouton : sa courbe apparaît ici.</span>}
            </div>
          </div>
          {song.lanes.map(([g, on, count, , vals]) => (
            <div key={g} className={`song-line song-line--auto${on ? "" : " is-off"}`}>
              <span className="song-lab" title={describeGlobal(g)}>
                <button type="button" className={`song-lane-on${on ? " is-on" : ""}`} onClick={() => void actions.songLane(g, 0)} aria-label={on ? "Couper la courbe" : "Relire la courbe"}>{on ? "●" : "○"}</button>
                <button type="button" className={`song-lane-name${editLane?.g === g ? " is-sel" : ""}`} onClick={() => void openLane(g)}
                  title="Toucher : voir et redessiner la courbe">{describeGlobal(g)}</button>
                <button type="button" className="x" onClick={() => { if (editLane?.g === g) setEditLane(null); void actions.songLane(g, 1); }} aria-label={`Effacer la courbe ${describeGlobal(g)}`}>×</button>
              </span>
              <svg className="song-track song-curve" width={Math.ceil((span / 4) * BAR_W)} height="100%" viewBox={`0 0 96 100`} preserveAspectRatio="none" aria-label={`${count} points`}>
                {vals.reduce<number[][][]>((runs, v, k) => { if (v < 0) runs.push([]); else runs[runs.length - 1].push([k, v]); return runs; }, [[]]).filter((r) => r.length).map((r, i) => (
                  <polyline key={i} vectorEffect="non-scaling-stroke"
                    points={(anySwitchLaneOf(g)
                      // v0.46 fix7 : coupure / solo en marches nettes (et non en pentes entre deux échantillons)
                      ? r.flatMap(([k, v], j) => { const y = v >= 0.5 ? 4 : 96; return j === 0 ? [[k + 0.5, y]] : [[k + 0.5, r[j - 1][1] >= 0.5 ? 4 : 96], [k + 0.5, y]]; })
                      : r.map(([k, v]) => [k + 0.5, (1 - v) * 92 + 4])).map(([x, y]) => `${x},${y}`).join(" ")} />
                ))}
              </svg>
            </div>
          ))}
        </div>
      </div>
      </div>

      {editLane && (
        <div className="song-editor">
          <div className="song-editor-head">
            <b>{describeGlobal(editLane.g)}</b>
            <span className="screen-spacer" />
            <button type="button" className="touch touch--small touch--quiet" onClick={() => setEditLane(null)}>Fermer</button>
          </div>
          <CurveEditor key={editLane.g} points={editLane.pts} span={Math.max(span, 4)} grid={1} bar={4} res={1 / 16}
            playX={playing && beat >= 0 ? beat : -1}
            format={(v) => { const sw = anySwitchLaneOf(editLane.g); if (sw) return v >= 0.5 ? (sw[1] ? "solo" : "coupée") : "jouée";
              if (isFaderLane(editLane.g)) { const x = Math.round(v * 200 - 100); return x <= -100 ? "A" : x >= 100 ? "B" : x > 0 ? `B ${x}` : x < 0 ? `A ${-x}` : "milieu"; }
              const r = resolveGlobal(editLane.g); return r ? formatNorm(r.desc, v) : `${Math.round(v * 100)} %`; }}
            unitLabel={(x) => `mes. ${Math.floor(x / 4) + 1}.${Math.floor(x % 4) + 1}`}
            onCommit={(pts) => { setEditLane({ g: editLane.g, pts }); void actions.setSongLane(editLane.g, pts); }} />
        </div>
      )}

      {pick && rows[pick.row] && (
        <div className="song-pick" role="group" aria-label={`Rangée ${pick.row + 1}, piste ${pick.track + 1}`}>
          <span>Rangée {pick.row + 1} · piste {pick.track + 1}</span>
          {cycleOrder.map((v) => (
            <button key={v} type="button" className={`touch touch--small${rows[pick.row][2][pick.track] === v ? " touch--on" : ""}`}
              onClick={() => { setRow(pick.row, (x) => { x[2][pick.track] = v; return x; }); setPick(null); }}>
              {v === SONG_KEEP ? "Garder" : v === SONG_STOP ? "Arrêt" : PATTERN_NAMES[v]}
            </button>
          ))}
        </div>
      )}

      <div className="song-actions">
        {selRow ? (<>
          <span className="song-sel">Rangée {sel + 1}</span>
          <button type="button" className="touch touch--small" disabled={selRow[0] <= 1} onClick={() => setRow(sel, (x) => { x[0] = Math.max(1, x[0] - 1); return x; })}>−</button>
          <span className="tabular">{selRow[0]} mes.</span>
          <button type="button" className="touch touch--small" disabled={selRow[0] >= SONG_MAX_BARS} onClick={() => setRow(sel, (x) => { x[0] = Math.min(SONG_MAX_BARS, x[0] + 1); return x; })}>+</button>
          <select className="song-scene" value={selRow[1]} aria-label="Scène lancée avec la rangée"
            onChange={(e) => setRow(sel, (x) => { x[1] = Number(e.target.value); return x; })}>
            <option value={-1}>Sans scène</option>
            {Array.from({ length: 32 }, (_, i) => <option key={i} value={i}>Scène {i + 1}</option>)}
          </select>
          <button type="button" className="touch touch--small touch--quiet" disabled={sel <= 0}
            onClick={() => { const n = [...rows]; [n[sel - 1], n[sel]] = [n[sel], n[sel - 1]]; editRows(n); setSel(sel - 1); }}>◀</button>
          <button type="button" className="touch touch--small touch--quiet" disabled={sel >= rows.length - 1}
            onClick={() => { const n = [...rows]; [n[sel + 1], n[sel]] = [n[sel], n[sel + 1]]; editRows(n); setSel(sel + 1); }}>▶</button>
          <button type="button" className="touch touch--small touch--quiet" disabled={rows.length >= SONG_MAX_ROWS}
            onClick={() => { const n = [...rows]; n.splice(sel + 1, 0, [selRow[0], selRow[1], [...selRow[2]]]); editRows(n); shiftLoop(sel + 1, 1); setSel(sel + 1); }}>Dupliquer</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => { editRows(rows.filter((_, k) => k !== sel)); shiftLoop(sel, -1); setSel(Math.min(sel, rows.length - 2)); setPick(null); }}>Supprimer</button>
          <button type="button" className={`touch touch--small touch--quiet${song.loop && loopFirst === sel && loopLast === sel ? " touch--on" : ""}`}
            onClick={() => { if (!song.loop) void actions.setSongFlags(song.mode, true, song.read); setLoop(sel, sel); }} title="Ne boucler que cette rangée">Boucler</button>
          {song.loop && sel > loopFirst && sel !== loopLast && <button type="button" className="touch touch--small touch--quiet" onClick={() => setLoop(loopFirst, sel)} title="La boucle s'arrête à la fin de cette rangée">Fin de boucle ici</button>}
          {song.loop && sel < loopLast && sel !== loopFirst && <button type="button" className="touch touch--small touch--quiet" onClick={() => setLoop(sel, loopToEnd ? -1 : loopLast)} title="La boucle repart au début de cette rangée">Début de boucle ici</button>}
        </>) : <span className="song-sel song-sel--hint">Touchez une rangée pour la régler ; une case : son pattern (clic droit : suivant).</span>}
        <span className="launch-sep" aria-hidden />
        <button type="button" className="touch touch--small" disabled={rows.length >= SONG_MAX_ROWS} onClick={() => { void actions.songRowFromPlaying(sel); }}
          title="Ajoute une rangée avec les patterns qui jouent (après la rangée choisie, sinon à la fin)">+ Rangée (ce qui joue)</button>
        {confirmClear
          ? <>
              <button type="button" className="touch touch--small touch--on"
                onClick={() => { if (confirmClear === "rows") { editRows([]); setSel(-1); } else void actions.songLane(-1, 2); setConfirmClear(null); }}>
                {confirmClear === "rows" ? "Effacer toutes les rangées" : "Effacer toutes les courbes"}
              </button>
              <button type="button" className="touch touch--small touch--quiet" onClick={() => setConfirmClear(null)}>Annuler</button>
            </>
          : <>
              <button type="button" className="touch touch--small touch--quiet" disabled={!rows.length} onClick={() => setConfirmClear("rows")}>Vider les rangées</button>
              <button type="button" className="touch touch--small touch--quiet" disabled={!song.lanes.length} onClick={() => setConfirmClear("lanes")}>Vider les courbes</button>
            </>}
      </div>
    </div>
  );
}
