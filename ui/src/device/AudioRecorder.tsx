// Groovebox — OFAD — AGPL-3.0-or-later
// v0.32 : enregistrer en audio ce qu'on joue (boucles, scènes, mode morceau) : le master, chaque
// piste séparément, ou les deux ; en WAV 24 bits ou en MP3 320 kbit/s. Le moteur écrit les
// fichiers ; l'interface ne voit que leurs noms et le dossier à partir de « Groovebox ».

import { useEffect, useRef, useState } from "react";
import { actions, hasNative, showMessage } from "../engine/store";
import type { AudioRecStatus } from "../engine/protocol";

const MODES = [
  { id: 0, label: "Master", hint: "Le mixage complet, tel qu'on l'entend" },
  { id: 1, label: "Pistes séparées", hint: "Un fichier par piste (après ses effets et son volume) et par retour d'effet" },
  { id: 2, label: "Les deux", hint: "Le master et chaque piste" },
];
const FORMATS = [
  { id: 0, label: "WAV 24 bits", hint: "Qualité studio, sans perte (≈ 17 Mo par minute et par fichier)" },
  { id: 1, label: "MP3 320 kbit/s", hint: "Haute qualité, léger (≈ 2,4 Mo par minute) ; converti juste après l'arrêt" },
];

// v0.46.2 : fin de la prise. Calée : elle part avec la lecture et s'arrête pile (boucle propre)
const ENDS = [
  { id: 2, label: "Boucle parfaite", hint: "Laisse passer un tour de la boucle (échos et compression en place), puis enregistre exactement un tour et s'arrête seule : la fin se recolle pile au début. Pour une boucle à rejouer (Pieuvre, autre logiciel) : en WAV." },
  { id: 1, label: "Calée sur la lecture", hint: "Part avec la lecture (ou au début du prochain tour de boucle) et s'arrête pile : fin du morceau, ARRÊT, ou fin du tour en cours quand on rappuie sur « ■ Audio ». Pour exporter une boucle." },
  { id: 0, label: "Libre", hint: "Enregistre tout, même sans lecture (jeu aux pads), jusqu'à ce qu'on rappuie sur « ■ Audio »." },
];

const readNum = (k: string, def: number) => { try { const v = localStorage.getItem(`grv.${k}`); return v === null ? def : Number(v) || 0; } catch { return def; } };
const writeNum = (k: string, v: number) => { try { localStorage.setItem(`grv.${k}`, String(v)); } catch { /* stockage indisponible */ } };
const clock = (s: number) => { const m = Math.floor(s / 60), r = Math.floor(s % 60); return `${m}:${String(r).padStart(2, "0")}`; };

export function AudioRecorder() {
  const [open, setOpen] = useState(false);
  const [mode, setMode] = useState(() => Math.min(2, Math.max(0, readNum("recMode", 0))));
  const [format, setFormat] = useState(() => (readNum("recFormat", 0) === 1 ? 1 : 0));
  const [follow, setFollow] = useState(() => { const v = readNum("recFollow", 1); return v === 0 || v === 2 ? v : 1; });
  const [st, setSt] = useState<AudioRecStatus | null>(null);
  const boxRef = useRef<HTMLDivElement>(null);
  const available = hasNative("audioRecStart");

  // état : toutes les 0,5 s pendant une prise ou une conversion, ou quand le panneau est ouvert
  const busy = !!st?.active || !!st?.encoding;
  useEffect(() => {
    if (!available) return;
    let alive = true;
    const poll = async () => { try { const s = await actions.audioRecStatus(); if (alive) setSt(s); } catch { /* moteur occupé */ } };
    void poll();
    if (!busy && !open) return () => { alive = false; };
    const id = window.setInterval(() => void poll(), 500);
    return () => { alive = false; window.clearInterval(id); };
  }, [busy, open, available]);

  // fin de prise (arrêtée à la main ou d'elle-même, prise calée) : en WAV, un mot pour dire où sont les fichiers
  const wasActive = useRef(false);
  useEffect(() => {
    if (wasActive.current && st && !st.active) {
      if (st.format === "wav") showMessage(`Enregistré : ${st.files.length} fichier${st.files.length > 1 ? "s" : ""} WAV dans ${st.folder}`);
      setOpen(true);
    }
    wasActive.current = !!st?.active;
  }, [st]);

  // fin de conversion : un mot pour dire où sont les fichiers
  const wasEncoding = useRef(false);
  useEffect(() => {
    if (wasEncoding.current && st && !st.encoding && !st.active)
      showMessage(st.error ? `MP3 : ${st.error}` : `Enregistrement prêt : ${st.files.length} fichier${st.files.length > 1 ? "s" : ""} MP3 dans ${st.folder}`);
    wasEncoding.current = !!st?.encoding;
  }, [st]);

  // clic en dehors : le panneau se ferme
  useEffect(() => {
    if (!open) return;
    const close = (e: PointerEvent) => { if (boxRef.current && !boxRef.current.contains(e.target as Node)) setOpen(false); };
    window.addEventListener("pointerdown", close);
    return () => window.removeEventListener("pointerdown", close);
  }, [open]);

  if (!available) return null;

  const start = async () => {
    const s = await actions.audioRecStart(mode, format, follow);
    setSt(s);
    if (s.startError) { showMessage(`Enregistrement impossible : ${s.startError}`); return; }
    setOpen(false);
    showMessage(follow === 2
      ? `Boucle parfaite : ${MODES[mode].label.toLowerCase()} en ${FORMATS[format].label} — lancez la lecture : un tour d'élan, puis un tour enregistré`
      : follow === 1
      ? `Enregistrement audio calé : ${MODES[mode].label.toLowerCase()} en ${FORMATS[format].label} — il part avec la lecture et s'arrête pile`
      : `Enregistrement audio : ${MODES[mode].label.toLowerCase()} en ${FORMATS[format].label} — rejouez « ● Audio » pour arrêter`);
  };
  const stop = async () => {
    const s = await actions.audioRecStop();
    setSt(s);
    if (s.active && s.stopping) showMessage("L'enregistrement s'arrêtera pile à la fin du tour de boucle (rappuyez pour arrêter tout de suite)");
  };

  const label = st?.active
    ? (st.waiting ? (follow === 2 ? "● Tour d'élan" : "● Attente lecture") : st.stopping ? `■ Fin de boucle ${clock(st.seconds)}` : `■ ${clock(st.seconds)}`)
    : st?.encoding ? `MP3 ${Math.round((st.progress ?? 0) * 100)} %` : "● Audio";
  return (
    <div className="top-rec-wrap" ref={boxRef}>
      <button type="button" className={`top-rec${st?.active ? " is-rec" : ""}${st?.encoding ? " is-enc" : ""}`}
        onClick={() => (st?.active ? void stop() : setOpen(!open))}
        title={st?.active ? "Arrêter l'enregistrement audio" : "Enregistrer en audio ce qui joue (master ou pistes séparées, WAV ou MP3)"}>
        {label}
      </button>
      {open && (
        <div className="rec-pop" role="dialog" aria-label="Enregistrement audio">
          <span className="rec-pop-title">Enregistrement audio</span>
          <span className="rec-pop-label">Ce qu'on enregistre</span>
          <div className="rec-pop-row" role="radiogroup">
            {MODES.map((m) => (
              <button key={m.id} type="button" role="radio" aria-checked={mode === m.id} className={mode === m.id ? "on" : ""} title={m.hint}
                disabled={busy} onClick={() => { setMode(m.id); writeNum("recMode", m.id); }}>{m.label}</button>
            ))}
          </div>
          <span className="rec-pop-label">Format</span>
          <div className="rec-pop-row" role="radiogroup">
            {FORMATS.map((f) => (
              <button key={f.id} type="button" role="radio" aria-checked={format === f.id} className={format === f.id ? "on" : ""} title={f.hint}
                disabled={busy} onClick={() => { setFormat(f.id); writeNum("recFormat", f.id); }}>{f.label}</button>
            ))}
          </div>
          <span className="rec-pop-label">Début et fin</span>
          <div className="rec-pop-row" role="radiogroup">
            {ENDS.map((e) => (
              <button key={e.id} type="button" role="radio" aria-checked={follow === e.id} className={follow === e.id ? "on" : ""} title={e.hint}
                disabled={busy} onClick={() => { setFollow(e.id); writeNum("recFollow", e.id); }}>{e.label}</button>
            ))}
          </div>
          <p className="rec-pop-hint">{FORMATS[format].hint}. {ENDS.find((e) => e.id === follow)?.hint}{format === 1 && follow !== 0 ? " Pour recoller une boucle dans un autre logiciel, le WAV est le plus sûr." : ""}</p>
          <button type="button" className="rec-pop-go" disabled={busy} onClick={() => void start()}>● Démarrer l'enregistrement</button>
          {st && !st.active && st.files.length > 0 && (
            <div className="rec-pop-last">
              <span className="rec-pop-label">Dernière prise · {clock(st.seconds)}{st.encoding ? ` · conversion MP3 ${Math.round(st.progress * 100)} %` : ""}</span>
              <span className="rec-pop-folder">{st.folder}</span>
              <ul>{st.files.slice(0, 8).map((f) => <li key={f}>{f}</li>)}{st.files.length > 8 && <li>… et {st.files.length - 8} autres</li>}</ul>
              {st.dropped > 0 && <span className="rec-pop-warn">Attention : {st.dropped.toFixed(2).replace(".", ",")} s perdues (disque trop lent)</span>}
              {st.error && <span className="rec-pop-warn">{st.error}</span>}
              {st.canReveal && <button type="button" className="rec-pop-open" onClick={() => void actions.audioRecReveal()}>Ouvrir le dossier</button>}
            </div>
          )}
        </div>
      )}
    </div>
  );
}
