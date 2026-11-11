// Groovebox — OFAD — AGPL-3.0-or-later
// v0.32 : enregistrer en audio ce qu'on joue (boucles, scènes, mode morceau) : le master, chaque
// piste séparément, ou les deux ; en WAV 24 bits ou en MP3 320 kbit/s. Le moteur écrit les
// fichiers ; l'interface ne voit que leurs noms et le dossier à partir de « Groovebox ».

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { actions, hasNative, setUi, showMessage, useStore } from "../engine/store";
import { finishTake, loadTake, startTake } from "../screens/AudioScreen";
import type { AudioRecStatus } from "../engine/protocol";

const clock = (s: number) => { const m = Math.floor(s / 60), r = Math.floor(s % 60); return `${m}:${String(r).padStart(2, "0")}`; };

export function AudioRecorder() {
  const [open, setOpen] = useState(false);
  const follow = loadTake().follow;
  const [st, setSt] = useState<AudioRecStatus | null>(null);
  const boxRef = useRef<HTMLDivElement>(null);
  const popRef = useRef<HTMLDivElement>(null);
  const available = hasNative("audioRecStart");

  // état : toutes les 0,5 s pendant une prise ou une conversion, ou quand le panneau est ouvert
  const screenOpen = useStore((s) => !!s.ui.audioScreen);
  const busy = !!st?.active || !!st?.encoding || !!st?.processing;
  useEffect(() => {
    if (!available) return;
    let alive = true;
    const poll = async () => { try { const s = await actions.audioRecStatus(); if (alive) setSt(s); } catch { /* moteur occupé */ } };
    void poll();
    if (!busy && !open) return () => { alive = false; };
    const id = window.setInterval(() => void poll(), 500);
    return () => { alive = false; window.clearInterval(id); };
  }, [busy, open, available, screenOpen]);

  // fin de prise (arrêtée à la main ou d'elle-même, prise calée) : en WAV, un mot pour dire où sont les fichiers
  const wasActive = useRef(false);
  useEffect(() => {
    if (wasActive.current && st && !st.active) {
      if (st.format === "wav" && !st.processing) showMessage(`Enregistré : ${st.files.length} fichier${st.files.length > 1 ? "s" : ""} WAV dans ${st.folder}`);
      if (!st.processing && !st.encoding && !screenOpen) setOpen(true);
    }
    wasActive.current = !!st?.active;
  }, [st]); // eslint-disable-line react-hooks/exhaustive-deps

  // 1.1 (lot 3) : fin du traitement de la prise : un mot, et la prise va dans son pad si demandé
  const wasProcessing = useRef(false);
  useEffect(() => {
    if (wasProcessing.current && st && !st.processing) {
      if (st.error) showMessage(`Traitement : ${st.error}`);
      else if ((st.treated?.length ?? 0) > 0 && !st.encoding) showMessage(`Prise traitée : ${st.treated!.length} fichier${st.treated!.length > 1 ? "s" : ""} dans ${st.treatedFolder}`);
    }
    wasProcessing.current = !!st?.processing;
    if (st && !st.active && !st.processing && !st.encoding) void finishTake(st);
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

  // 1.1 : un bandeau qui masque ce qui dépasse (barre du haut des écrans sur téléphone) coupait la
  // fenêtre : elle n'apparaissait plus. Dans ce cas elle sort du bandeau (position fixe, calée sous le
  // bouton, dans le repère de l'appareil mis à l'échelle) et reste dans l'écran.
  useLayoutEffect(() => {
    const pop = popRef.current, box = boxRef.current;
    if (!open || !pop || !box) return;
    let clipped = false;
    for (let x = box.parentElement; x && x !== document.body; x = x.parentElement) {
      const ov = getComputedStyle(x).overflow;
      if (ov !== "visible") { clipped = x.getBoundingClientRect().bottom < box.getBoundingClientRect().bottom + 60; break; }
    }
    if (!clipped) return;
    let frame: HTMLElement | null = null; // ancêtre transformé : repère des positions fixes
    for (let x = box.parentElement; x && x !== document.body; x = x.parentElement)
      if (getComputedStyle(x).transform !== "none") { frame = x; break; }
    const place = () => {
      const b = box.getBoundingClientRect();
      const fr = frame ? frame.getBoundingClientRect() : { left: 0, top: 0, width: window.innerWidth, height: window.innerHeight };
      const k = frame && frame.offsetWidth > 0 ? fr.width / frame.offsetWidth : 1;
      const fw = fr.width / k, fh = fr.height / k;
      const w = Math.min(pop.offsetWidth || 380, fw - 16);
      const right = (b.right - fr.left) / k;
      pop.style.position = "fixed";
      pop.style.left = `${Math.max(8, Math.min(fw - w - 8, right - w))}px`;
      pop.style.right = "auto";
      pop.style.top = `${(b.bottom - fr.top) / k + 8}px`;
      pop.style.maxHeight = `${Math.max(160, fh - (b.bottom - fr.top) / k - 20)}px`;
      pop.style.overflowY = "auto";
    };
    place();
    window.addEventListener("resize", place);
    return () => window.removeEventListener("resize", place);
  }, [open]);

  if (!available) return null;

  const stop = async () => {
    const s = await actions.audioRecStop();
    setSt(s);
    if (s.active && s.stopping) showMessage("L'enregistrement s'arrêtera pile à la fin du tour de boucle (rappuyez pour arrêter tout de suite)");
  };

  // 1.1 (lot 3) : « ● Audio » ouvre l'écran AUDIO ; un appui long démarre tout de suite (derniers réglages)
  const press = useRef<number | null>(null);
  const longDone = useRef(false);
  const down = () => {
    longDone.current = false;
    if (st?.active || busy) return;
    press.current = window.setTimeout(() => { longDone.current = true; press.current = null; void startTake().then((s) => { if (s) setSt(s); }); }, 650);
  };
  const up = () => { if (press.current !== null) { window.clearTimeout(press.current); press.current = null; } };
  const click = () => {
    if (longDone.current) { longDone.current = false; return; }
    if (st?.active) void stop();
    else { setOpen(false); setUi({ audioScreen: true }); }
  };

  const label = st?.active
    ? (st.waiting ? (follow === 2 ? "● Tour d'élan" : "● Attente lecture") : st.stopping ? `■ Fin de boucle ${clock(st.seconds)}` : `■ ${clock(st.seconds)}`)
    : st?.processing ? `Traitement ${Math.round((st.progress ?? 0) * 100)} %`
    : st?.encoding ? `MP3 ${Math.round((st.progress ?? 0) * 100)} %` : "● Audio";
  return (
    <div className="top-rec-wrap" ref={boxRef}>
      <button type="button" className={`top-rec${st?.active ? " is-rec" : ""}${st?.encoding ? " is-enc" : ""}`}
        onClick={click} onPointerDown={down} onPointerUp={up} onPointerLeave={up} onPointerCancel={up}
        onContextMenu={(e) => e.preventDefault()}
        title={st?.active ? "Arrêter l'enregistrement audio" : "Écran AUDIO (appui long : démarrer tout de suite avec les derniers réglages)"}>
        {label}
      </button>
      {open && (
        <div ref={popRef} className="rec-pop" role="dialog" aria-label="Enregistrement audio">
          <span className="rec-pop-title">Dernière prise</span>
          <button type="button" className="rec-pop-go" onClick={() => { setOpen(false); setUi({ audioScreen: true }); }}>Écran AUDIO…</button>
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
