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

const readNum = (k: string, def: number) => { try { const v = localStorage.getItem(`grv.${k}`); return v === null ? def : Number(v) || 0; } catch { return def; } };
const writeNum = (k: string, v: number) => { try { localStorage.setItem(`grv.${k}`, String(v)); } catch { /* stockage indisponible */ } };
const clock = (s: number) => { const m = Math.floor(s / 60), r = Math.floor(s % 60); return `${m}:${String(r).padStart(2, "0")}`; };

export function AudioRecorder() {
  const [open, setOpen] = useState(false);
  const [mode, setMode] = useState(() => Math.min(2, Math.max(0, readNum("recMode", 0))));
  const [format, setFormat] = useState(() => (readNum("recFormat", 0) === 1 ? 1 : 0));
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
    const s = await actions.audioRecStart(mode, format);
    setSt(s);
    if (s.startError) { showMessage(`Enregistrement impossible : ${s.startError}`); return; }
    setOpen(false);
    showMessage(`Enregistrement audio : ${MODES[mode].label.toLowerCase()} en ${FORMATS[format].label} — rejouez « ● Audio » pour arrêter`);
  };
  const stop = async () => {
    const s = await actions.audioRecStop();
    setSt(s);
    setOpen(true);
    if (s.format === "wav") showMessage(`Enregistré : ${s.files.length} fichier${s.files.length > 1 ? "s" : ""} WAV dans ${s.folder}`);
  };

  const label = st?.active ? `■ ${clock(st.seconds)}` : st?.encoding ? `MP3 ${Math.round((st.progress ?? 0) * 100)} %` : "● Audio";
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
          <p className="rec-pop-hint">{FORMATS[format].hint}. L'enregistrement prend tout ce qui joue : boucles, scènes, mode morceau, jeu aux pads.</p>
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
