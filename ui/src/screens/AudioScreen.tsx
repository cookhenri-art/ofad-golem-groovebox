// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 (lot 3) : ÉCRAN AUDIO (plein écran), à la place de la petite fenêtre d'enregistrement.
// Sources cochées (master, master traité, pistes une à une ou en somme, retours), chaîne de 4 effets
// appliquée après la prise (qualité maximale, la prise brute reste) ou pendant (fichier recalé à
// l'écriture), format, destination : la prise, « Son traité », un pad, ou à la place de
// l'échantillon d'origine (annulable). Gardé : boucle parfaite, prise calée, MP3, dossier.

import { useEffect, useMemo, useState } from "react";
import { actions, getState, setUi, showMessage, useStore, type ChainSlotUi } from "../engine/store";
import type { AudioRecStatus, AudioTakeOptions } from "../engine/protocol";
import { ChainEditor, loadChain, saveChain } from "./ChainEditor";

export interface TakeSettings {
  master: boolean; masterTreated: boolean; tracks: number; sum: boolean; returns: number; input: boolean;
  during: boolean; dest: number; format: number; follow: number; bars: number;
  padTrack: number; replaceSlot: number;
}
const DEFAULTS: TakeSettings = { master: true, masterTreated: false, tracks: 0, sum: false, returns: 0, input: false, during: false, dest: 0, format: 0, follow: 1, bars: 4, padTrack: -1, replaceSlot: 0 };
export function loadTake(): TakeSettings {
  try { const v = JSON.parse(localStorage.getItem("grv.take") ?? "null"); return v && typeof v === "object" ? { ...DEFAULTS, ...v } : { ...DEFAULTS }; } catch { return { ...DEFAULTS }; }
}
const saveTake = (t: TakeSettings) => { try { localStorage.setItem("grv.take", JSON.stringify(t)); } catch { /* stockage indisponible */ } };

const FOLLOWS = [
  { id: 2, label: "Boucle parfaite", hint: "Un tour d'élan (échos et compression en place), puis exactement un tour : la fin se recolle pile au début." },
  { id: 1, label: "Calée", hint: "Part avec la lecture (ou au prochain tour de boucle) et s'arrête pile : fin du morceau, ARRÊT, ou fin du tour en cours." },
  { id: 3, label: "Mesures", hint: "Part avec la lecture et s'arrête seule après le nombre de mesures choisi." },
  { id: 0, label: "Libre", hint: "Tout, même sans lecture (jeu aux pads), jusqu'à ce qu'on rappuie sur ■." },
];
const FORMATS = [
  { id: 0, label: "WAV 24 bits", hint: "Qualité studio, sans perte" },
  { id: 1, label: "WAV 32 flottant", hint: "Aucune saturation possible dans le fichier (pour retravailler)" },
  { id: 2, label: "MP3 320", hint: "Léger ; converti juste après l'arrêt" },
];
const DESTS = [
  { id: 0, label: "Enregistrements", hint: "Dans le dossier de la prise ; les fichiers traités à côté des bruts" },
  { id: 1, label: "Son traité", hint: "Dossier « Son traité », fichiers nommés « Piste – 128 BPM – 4 mesures – traité »" },
  { id: 2, label: "Dans un pad…", hint: "La prise va dans le premier emplacement libre de la piste choisie (annulable)" },
  { id: 3, label: "Remplacer l'original", hint: "Une seule piste d'échantillons : la prise traitée prend la place de son échantillon (annulable)" },
];

/** Pad visé par la prise en cours (rempli au démarrage, vidé une fois chargé). */
let pendingPad: { track: number; index: number } | null = null;

export function takeOptions(t: TakeSettings, chain: ChainSlotUi[]): AudioTakeOptions {
  return {
    master: t.master, masterTreated: t.masterTreated, tracks: t.tracks, sum: t.sum, returns: t.returns, input: t.input,
    chain: chain.filter((c) => c.type).map((c) => ({ type: c.type, values: c.values, bypass: c.bypass })),
    during: t.during, dest: t.dest, format: t.dest >= 2 && t.format === 2 ? 0 : t.format, follow: t.follow, bars: t.bars,
  };
}

/** Démarre une prise avec ces réglages (écran AUDIO ou appui long sur « ● Audio »). */
export async function startTake(t: TakeSettings = loadTake(), chain: ChainSlotUi[] = loadChain("rec")): Promise<AudioRecStatus | null> {
  const target = padTarget(t);
  if (t.dest >= 2 && !target) { showMessage(t.dest === 2 ? "Choisissez la piste qui reçoit la prise" : "Cochez une seule piste d'échantillons"); return null; }
  const s = await actions.audioRecStartEx(takeOptions(t, chain));
  if (s.startError) { showMessage(`Enregistrement impossible : ${s.startError}`); return s; }
  pendingPad = target;
  const f = FOLLOWS.find((x) => x.id === t.follow);
  showMessage(t.follow === 2 ? "Boucle parfaite : lancez la lecture — un tour d'élan, puis un tour enregistré"
    : t.follow === 0 ? "Enregistrement audio — rappuyez sur ■ pour arrêter"
    : `Prise ${f?.label.toLowerCase() ?? "calée"}${t.follow === 3 ? ` (${t.bars} mesure${t.bars > 1 ? "s" : ""})` : ""} : elle part avec la lecture et s'arrête pile`);
  return s;
}

/** Pistes qui jouent des échantillons (emplacements connus). */
function sampleTracks(): number[] {
  const st = getState();
  return st.tracks.tracks.map((_, i) => i).filter((i) => Array.isArray(st.channels[i]?.samples) && (st.channels[i]?.samples?.length ?? 0) > 0);
}
function padTarget(t: TakeSettings): { track: number; index: number } | null {
  const st = getState();
  if (t.dest === 2) {
    const tr = t.padTrack;
    const smp = st.channels[tr]?.samples;
    if (tr < 0 || !smp?.length) return null;
    const free = smp.findIndex((x) => !x);
    return { track: tr, index: free >= 0 ? free : smp.length - 1 };
  }
  if (t.dest === 3) {
    const bits = [...Array(16).keys()].filter((i) => (t.tracks & (1 << i)) !== 0);
    if (bits.length !== 1 || t.sum && bits.length !== 1) return null;
    const smp = st.channels[bits[0]]?.samples;
    if (!smp?.length) return null;
    return { track: bits[0], index: Math.min(Math.max(0, t.replaceSlot), smp.length - 1) };
  }
  return null;
}

/** Après la prise (traitement fini) : le fichier va dans le pad visé. Appelé par le bouton « ● Audio ». */
export async function finishTake(s: AudioRecStatus) {
  if (!pendingPad || s.active || s.processing || s.encoding) return;
  const p = pendingPad; pendingPad = null;
  if (!s.hasResult) { showMessage("Prise sans fichier à charger"); return; }
  const err = await actions.audioRecToPad(p.track, p.index);
  if (err) { showMessage(`Pad : ${err}`); return; }
  const name = getState().tracks.tracks[p.track]?.name ?? `Piste ${p.track + 1}`;
  showMessage(`Prise chargée dans ${name}, emplacement ${p.index + 1} (Annuler dans l'éditeur d'échantillon)`);
}

/** Petit vu-mètre d'une source (crête, en dB : −48 à 0). */
function Vu({ peak }: { peak: number }) {
  const db = peak > 1e-5 ? 20 * Math.log10(peak) : -60;
  const w = Math.max(0, Math.min(1, (db + 48) / 48));
  return <span className={`aud-vu${db > -1 ? " hot" : ""}`} aria-hidden><i style={{ width: `${w * 100}%` }} /></span>;
}

const clock = (s: number) => { const m = Math.floor(s / 60), r = Math.floor(s % 60); return `${m}:${String(r).padStart(2, "0")}`; };

export function AudioScreen() {
  const open = useStore((s) => !!s.ui.audioScreen);
  if (!open) return null;
  return <AudioScreenBody />;
}

function AudioScreenBody() {
  const tracks = useStore((s) => s.tracks);
  const channels = useStore((s) => s.channels);
  const [t, setT] = useState<TakeSettings>(loadTake);
  const [chain, setChainState] = useState<ChainSlotUi[]>(() => loadChain("rec"));
  const [st, setSt] = useState<AudioRecStatus | null>(null);
  const upd = (p: Partial<TakeSettings>) => setT((o) => { const n = { ...o, ...p }; saveTake(n); return n; });
  const setChain = (c: ChainSlotUi[]) => { setChainState(c); saveChain("rec", c); };
  const close = () => { if (monMs !== null) void actions.audioMonitor([]); setUi({ audioScreen: false }); };
  const peaks = useStore((s) => s.tick.peaks);
  const inPeak = useStore((s) => s.tick.perf?.[10] ?? 0);
  const [mic, setMic] = useState<number>(-1);           // 1 coupé, 0 ouvert, -1 pas d'application autonome
  useEffect(() => { void actions.inputMute(-1).then(setMic); }, []);
  // « Écouter le traitement » : la sortie passe dans la chaîne (avec sa latence) tant que c'est allumé
  const [monMs, setMonMs] = useState<number | null>(null);
  const chainKey = JSON.stringify(chain);
  useEffect(() => {
    if (monMs === null) return;
    void actions.audioMonitor(chain.filter((c) => c.type)).then((ms) => setMonMs(ms));
  }, [chainKey]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => () => { void actions.audioMonitor([]); void actions.audioMonitorSources({ on: false, tracks: [], returns: [], input: false }); }, []);
  // 1.1 : à l'écoute, on n'entend que ce qu'on enregistre (pistes, retours, entrée cochés), traité par la chaîne
  const monOn = monMs !== null;
  useEffect(() => {
    const bits = (m: number, n: number) => Array.from({ length: n }, (_, i) => i).filter((i) => (m >> i) & 1);
    void actions.audioMonitorSources({ on: monOn, tracks: bits(t.tracks, 16), returns: bits(t.returns, 4), input: t.input });
  }, [monOn, t.tracks, t.returns, t.input]);
  const toggleMon = async () => {
    if (monMs !== null) { await actions.audioMonitor([]); setMonMs(null); return; }
    setMonMs(await actions.audioMonitor(chain.filter((c) => c.type)));
  };

  // les emplacements des pistes (pads libres, échantillon à remplacer)
  useEffect(() => { tracks.tracks.forEach((tr, i) => { if (tr.instrument) void actions.refreshChannel(i); }); }, [tracks.tracks.length]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    let alive = true;
    const poll = async () => { try { const s = await actions.audioRecStatus(); if (alive) setSt(s); } catch { /* moteur occupé */ } };
    void poll();
    const id = window.setInterval(() => void poll(), 400);
    return () => { alive = false; window.clearInterval(id); };
  }, []);
  useEffect(() => {
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape" && !e.defaultPrevented) { e.preventDefault(); close(); } };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  });

  const used = useMemo(() => tracks.tracks.map((tr) => !!tr.instrument), [tracks.tracks]);
  const retUsed = [0, 1, 2, 3].map((r) => (tracks.inserts[16 + r] ?? []).some((x) => !!x));
  const smpTracks = useMemo(() => sampleTracks(), [channels, tracks]); // eslint-disable-line react-hooks/exhaustive-deps
  const active = chain.filter((c) => c.type && !c.bypass).length;
  const busy = !!st?.active || !!st?.processing || !!st?.encoding;
  const checked = [...Array(16).keys()].filter((i) => (t.tracks & (1 << i)) !== 0 && used[i]);
  const replaceTrack = checked.length === 1 ? checked[0] : -1;
  const target = padTarget(t);
  const none = !t.master && !t.masterTreated && !t.input && checked.length === 0 && !retUsed.some((u, r) => u && (t.returns & (1 << r)) !== 0);

  const start = async () => {
    const s = await startTake(t, chain);
    if (s) setSt(s);
    if (s && !s.startError) close();
  };
  const stop = async () => setSt(await actions.audioRecStop());

  const destNote = t.dest === 2 ? (target ? `→ ${tracks.tracks[target.track]?.name ?? ""}, emplacement ${target.index + 1}${channels[target.track]?.samples?.[target.index] ? " (occupé : remplacé, annulable)" : " (libre)"}` : "Choisissez une piste d'échantillons")
    : t.dest === 3 ? (target ? `→ remplace « ${channels[target.track]?.samples?.[target.index] ?? "?"} » (annulable)` : "Cochez une seule piste d'échantillons (et pas la somme)")
    : DESTS[t.dest].hint;

  return (
    <div className="sedit aud" role="dialog" aria-label="Écran AUDIO">
      <header className="sedit-head">
        <b className="sedit-title">AUDIO</b>
        <span className="sedit-name">
          {st?.active ? (st.waiting ? "Prise armée : attend la lecture" : `Enregistrement ${clock(st.seconds)}`) : st?.processing ? `Traitement de la prise ${Math.round((st.progress ?? 0) * 100)} %` : st?.encoding ? `MP3 ${Math.round((st.progress ?? 0) * 100)} %` : "Prêt"}
        </span>
        <span style={{ flex: 1 }} />
        <button type="button" className="touch touch--small" onClick={close}>Fermer</button>
      </header>
      <div className="aud-body">
        <section className="sedit-box">
          <span className="chain-title">Ce qu'on enregistre</span>
          <div className="aud-row">
            <button type="button" className={`aud-chip${t.master ? " on" : ""}`} disabled={busy} onClick={() => upd({ master: !t.master })}>Master</button>
            <button type="button" className={`aud-chip${t.masterTreated ? " on" : ""}`} disabled={busy} onClick={() => upd({ masterTreated: !t.masterTreated })}
              title="Le master passé dans la chaîne d'enregistrement">Master traité</button>
            <Vu peak={peaks[20] ?? 0} />
          </div>
          <span className="aud-label">Pistes</span>
          <div className="aud-row aud-grid">
            {tracks.tracks.slice(0, 16).map((tr, i) => (
              <button key={i} type="button" className={`aud-chip aud-num${t.tracks & (1 << i) ? " on" : ""}`} disabled={busy || !used[i]} title={tr.name}
                onClick={() => upd({ tracks: t.tracks ^ (1 << i) })}>{i + 1}</button>
            ))}
          </div>
          <div className="aud-row">
            <button type="button" className="touch touch--small touch--quiet" disabled={busy} onClick={() => upd({ tracks: used.reduce((m, u, i) => (u ? m | (1 << i) : m), 0) })}>Toutes</button>
            <button type="button" className="touch touch--small touch--quiet" disabled={busy} onClick={() => upd({ tracks: 0 })}>Aucune</button>
            <label className="sedit-check"><input type="checkbox" checked={t.sum} disabled={busy} onChange={(e) => upd({ sum: e.target.checked })} /> Une seule piste (somme des cochées)</label>
          </div>
          {checked.length > 0 && (
            <div className="aud-vus">
              {checked.map((i) => <span key={i} className="aud-vurow"><small>{i + 1} {tracks.tracks[i]?.name ?? ""}</small><Vu peak={peaks[i] ?? 0} /></span>)}
            </div>
          )}
          <span className="aud-label">Retours</span>
          <div className="aud-row">
            {["A", "B", "C", "D"].map((l, r) => (
              <button key={l} type="button" className={`aud-chip${t.returns & (1 << r) ? " on" : ""}`} disabled={busy || !retUsed[r]} onClick={() => upd({ returns: t.returns ^ (1 << r) })}>{l}</button>
            ))}
            {[0, 1, 2, 3].some((r) => (t.returns & (1 << r)) !== 0 && retUsed[r]) && <Vu peak={Math.max(...[0, 1, 2, 3].map((r) => ((t.returns & (1 << r)) !== 0 ? peaks[16 + r] ?? 0 : 0)))} />}
          </div>
          <span className="aud-label">Entrée</span>
          <div className="aud-row">
            <button type="button" className={`aud-chip${t.input ? " on" : ""}`} disabled={busy} onClick={() => upd({ input: !t.input })}
              title="Le micro ou l'entrée ligne de l'appareil, tel qu'il arrive (sans les effets des pistes)">Micro / ligne</button>
            {t.input && mic === 1 && <button type="button" className="touch touch--small" onClick={() => void actions.inputMute(0).then(setMic)}>Activer le micro</button>}
            {t.input && <Vu peak={inPeak} />}
          </div>
          {t.input && mic === 1 && <small className="sedit-note">Le micro est coupé (sécurité anti-larsen) : activez-le, sinon la prise de l'entrée sera muette.</small>}
          <span className="aud-label">Début et fin</span>
          <div className="aud-row">
            {FOLLOWS.map((f) => (
              <button key={f.id} type="button" className={`aud-chip${t.follow === f.id ? " on" : ""}`} title={f.hint} disabled={busy} onClick={() => upd({ follow: f.id })}>{f.label}</button>
            ))}
            {t.follow === 3 && (
              <select value={t.bars} disabled={busy} onChange={(e) => upd({ bars: Number(e.target.value) })} aria-label="Nombre de mesures">
                {[1, 2, 4, 8, 16, 32].map((b) => <option key={b} value={b}>{b} mes.</option>)}
              </select>
            )}
          </div>
          <small className="sedit-note">{FOLLOWS.find((f) => f.id === t.follow)?.hint}</small>
          <span className="aud-label">Format</span>
          <div className="aud-row">
            {FORMATS.map((f) => (
              <button key={f.id} type="button" className={`aud-chip${t.format === f.id ? " on" : ""}`} title={f.hint} disabled={busy || (f.id === 2 && t.dest >= 2)} onClick={() => upd({ format: f.id })}>{f.label}</button>
            ))}
          </div>
          <span className="aud-label">Où ça va</span>
          <div className="aud-row">
            {DESTS.map((d) => (
              <button key={d.id} type="button" className={`aud-chip${t.dest === d.id ? " on" : ""}`} title={d.hint} disabled={busy} onClick={() => upd({ dest: d.id })}>{d.label}</button>
            ))}
          </div>
          {t.dest === 2 && (
            <div className="aud-row">
              <select value={t.padTrack} disabled={busy} onChange={(e) => upd({ padTrack: Number(e.target.value) })} aria-label="Piste qui reçoit la prise">
                <option value={-1}>Piste…</option>
                {smpTracks.map((i) => <option key={i} value={i}>{i + 1} · {tracks.tracks[i]?.name}</option>)}
              </select>
            </div>
          )}
          {t.dest === 3 && replaceTrack >= 0 && (channels[replaceTrack]?.samples?.length ?? 0) > 0 && (
            <div className="aud-row">
              <select value={t.replaceSlot} disabled={busy} onChange={(e) => upd({ replaceSlot: Number(e.target.value) })} aria-label="Échantillon remplacé">
                {(channels[replaceTrack]?.samples ?? []).map((n, i) => (n ? <option key={i} value={i}>{i + 1} · {n}</option> : null))}
              </select>
            </div>
          )}
          <small className="sedit-note">{destNote}</small>
        </section>
        <section className="sedit-box">
          <ChainEditor chain={chain} onChange={setChain} title="Chaîne d'enregistrement · 4 effets" />
          <span className="aud-label">Quand</span>
          <div className="aud-row">
            <button type="button" className={`aud-chip${!t.during ? " on" : ""}`} disabled={busy} onClick={() => upd({ during: false })}
              title="La prise brute est gardée, puis traitée hors temps réel (qualité maximale)">Après la prise</button>
            <button type="button" className={`aud-chip${t.during ? " on" : ""}`} disabled={busy} onClick={() => upd({ during: true })}
              title="Traitée au fil de l'écriture (versions directes des outils), recalée : prête dès l'arrêt">Pendant la prise</button>
          </div>
          <div className="aud-row">
            <button type="button" className={`aud-chip${monMs !== null ? " on" : ""}`} onClick={() => void toggleMon()}
              title="On entend ce qu'on enregistre : seulement les pistes, retours et l'entrée cochés (Master seul : tout le mixage), passés dans la chaîne comme le fichier traité (avec la latence des outils) ; la prise, elle, reste le son d'origine">
              {monMs !== null ? `■ ${active === 0 ? "Écoute de la prise" : "Écoute du traitement"}${monMs >= 0.5 ? ` (latence ${Math.round(monMs)} ms)` : ""}` : active === 0 ? "Écouter la prise" : "Écouter le traitement"}
            </button>
          </div>
          {monMs !== null && <small className="sedit-note">{checked.length > 0 || t.returns !== 0 || t.input
            ? "On n'entend que les pistes, retours et l'entrée cochés : le mixage revient en arrêtant l'écoute ou en fermant l'écran."
            : "Seul le Master est coché : on entend tout le mixage."}</small>}
          <small className="sedit-note">
            {active === 0 ? "Sans effet : la prise est enregistrée telle quelle."
              : t.during ? "Ce qu'on entend reste le son d'origine, sans latence ; le fichier est traité et recalé à l'écriture."
              : "Qualité maximale : la prise brute est gardée, traitée à l'arrêt (le début reste pile au temps)."}
 {active > 0 && !t.masterTreated && !t.input && checked.length === 0 ? " Cochez « Master traité », des pistes ou l'entrée pour qu'elle serve." : ""}
          </small>
          <span style={{ flex: 1 }} />
          {st && !st.active && (st.files.length > 0 || (st.treated?.length ?? 0) > 0) && (
            <div className="rec-pop-last">
              <span className="rec-pop-label">Dernière prise · {clock(st.seconds)}{st.processing ? ` · traitement ${Math.round(st.progress * 100)} %` : st.encoding ? ` · MP3 ${Math.round(st.progress * 100)} %` : ""}</span>
              <span className="rec-pop-folder">{st.folder}</span>
              <ul>{st.files.slice(0, 6).map((f) => <li key={f}>{f}</li>)}{st.files.length > 6 && <li>… et {st.files.length - 6} autres</li>}</ul>
              {(st.treated?.length ?? 0) > 0 && (<>
                <span className="rec-pop-folder">Traités : {st.treatedFolder}</span>
                <ul>{st.treated!.slice(0, 6).map((f) => <li key={f}>{f}</li>)}</ul>
              </>)}
              {st.dropped > 0 && <span className="rec-pop-warn">Attention : {st.dropped.toFixed(2).replace(".", ",")} s perdues (disque trop lent)</span>}
              {st.error && <span className="rec-pop-warn">{st.error}</span>}
              {st.canReveal && <button type="button" className="rec-pop-open" onClick={() => void actions.audioRecReveal()}>Ouvrir le dossier</button>}
            </div>
          )}
          <div className="sedit-row">
            <span style={{ flex: 1 }} />
            {st?.active
              ? <button type="button" className="touch touch--on sedit-apply" onClick={() => void stop()}>■ Arrêter</button>
              : <button type="button" className="touch touch--on sedit-apply" disabled={busy || none || (t.dest >= 2 && !target)} onClick={() => void start()}>● Démarrer</button>}
          </div>
        </section>
      </div>
    </div>
  );
}
