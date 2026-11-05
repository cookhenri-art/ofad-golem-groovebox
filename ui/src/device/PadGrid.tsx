// Groovebox — OFAD — AGPL-3.0-or-later
// Les 16 pads (pad 1 en bas à gauche), selon le rôle choisi sous les pads :
//  - Jouer : les sons de la batterie de la piste choisie, ou les notes (chromatique) d'un
//    instrument mélodique ; la vélocité dépend de la hauteur de la frappe ;
//  - Couper : coupure des 16 pistes ; - Scènes : lancement des scènes ; - Effacer : efface le pad.
// Déposer un fichier audio sur un pad le charge (batteries, samplers).

import { padNote } from "../engine/scales";
import { useEffect, useState, useRef } from "react";
import { actions, getState, setUi, showMessage, useStore } from "../engine/store";
import { beginStepPaint } from "../controls/stepPaint";
import { noteName } from "../engine/params";

const KEY_CODES: Record<string, number> = {
  KeyZ: 0, KeyX: 1, KeyC: 2, KeyV: 3, KeyA: 4, KeyS: 5, KeyD: 6, KeyF: 7,
  KeyQ: 8, KeyW: 9, KeyE: 10, KeyR: 11, Digit1: 12, Digit2: 13, Digit3: 14, Digit4: 15,
};
const KEY_LABELS = ["W", "X", "C", "V", "Q", "S", "D", "F", "A", "Z", "E", "R", "1", "2", "3", "4"]; // AZERTY
const ROWS = [[12, 13, 14, 15], [8, 9, 10, 11], [4, 5, 6, 7], [0, 1, 2, 3]];


export function PadGrid() {
  const cat = useStore((s) => s.catalog);
  const focus = useStore((s) => s.focus);
  const tracks = useStore((s) => s.tracks.tracks);
  const mode = useStore((s) => s.ui.padMode);
  const octave = useStore((s) => s.ui.octave);
  const scale = useStore((s) => s.ui.scale);
  const levels = useStore((s) => s.ui.padLevels && s.ui.padMode === "play");
  const root = useStore((s) => s.ui.root);
  const lane = useStore((s) => s.ui.lane);
  const litLanes = useStore((s) => s.litLanes);
  const litTracks = useStore((s) => s.litTracks);
  const litNotes = useStore((s) => s.litNotes);
  const scenes = useStore((s) => s.scenes);
  const sceneBank = useStore((s) => s.ui.sceneBank);
  const g = useStore((s) => s.tick.g);
  const samples = useStore((s) => s.channels[s.focus]?.samples);
  const page = useStore((s) => s.ui.page);
  const laneVel = useStore((s) => s.pattern?.lanes[s.ui.lane]?.vel);
  const patLength = useStore((s) => s.pattern?.length ?? 16);
  const playStep = useStore((s) => (s.tick.t.playing && s.pattern && s.pattern.playing === s.pattern.pattern ? s.tick.pos[s.ui.lane] ?? -1 : -1));
  const [dropTarget, setDropTarget] = useState<number | null>(null);
  const t = Math.min(focus, 15);
  const module = cat?.instruments.find((i) => i.type === tracks[t]?.instrument);
  const drum = !!module?.drumKit;
  // v0.43 : Pieuvre 8 — les pads (et les touches du clavier d'ordinateur) jouent en chromatique la piste
  // interne choisie (ou les zones du clavier) ; la coupure et l'effacement restent par ligne (T1 à T8)
  const drumPads = drum && module?.type !== "octa";
  // v0.31 : rôle Couper sur une batterie — les pads coupent ses sons (lignes du motif) au lieu des pistes
  const muteTarget = useStore((s) => s.ui.muteTarget);
  const padMute = mode === "mute" && drum && muteTarget === "pads";
  const laneMs = useStore((s) => s.pattern?.lanes.map((l) => (l.solo ? 2 : 0) + (l.mute ? 1 : 0)).join(",") ?? "");
  const laneFlags = laneMs.split(",").map(Number);

  const press = (pad: number, velocity: number) => {
    const s = getState();
    switch (s.ui.padMode) {
      case "play": {
        if (!module) { showMessage("Piste libre : choisissez un instrument (écran PISTES)"); return; }
        // « 16 niveaux » (principe MPC) : le même son sur les 16 pads, de la vélocité 8 à 127
        if (s.ui.padLevels) { actions.noteOn(t, levelNote(s.ui.lane), (pad + 1) / 16); break; }
        actions.noteOn(t, padNote(pad, drumPads, s.ui.octave, s.ui.scale, s.ui.root), velocity);
        if (drumPads) setUi({ lane: pad });
        break;
      }
      case "mute": {
        if (drum && s.ui.muteTarget === "pads") {
          // Maj + pad : solo du son ; pad : coupure du son
          const ln = s.pattern?.lanes[pad];
          if (s.ui.shift) void actions.seq("seqLaneSolo", pad, !ln?.solo);
          else void actions.seq("seqLaneMute", pad, !ln?.mute);
          break;
        }
        void actions.trackMute(pad, !s.tracks.tracks[pad]?.mute);
        break;
      }
      case "scenes": actions.launchScene(s.ui.sceneBank * 16 + pad); break;
      case "steps": {
        // Les 16 pads = les 16 pas de la page affichée, sur la ligne choisie (comme le pas à pas d'une MPC)
        if (!s.pattern) return;
        const step = s.ui.page * 16 + pad;
        if (step >= s.pattern.length) { showMessage(`Pas ${step + 1} au-delà de la longueur du pattern`); return; }
        actions.toggleStep(s.ui.lane, step);
        break;
      }
      case "erase": {
        const p = s.pattern;
        if (!p) return;
        if (drum) { void actions.seq("seqClearLane", pad); showMessage(`${module?.padNames[pad] ?? "Pad"} effacé`); }
        else {
          const n = padNote(pad, false, s.ui.octave, s.ui.scale, s.ui.root);
          p.lanes.forEach((ln, l) => ln.vel.forEach((v, st) => { if (v > 0 && ln.note[st] === n) void actions.seq("seqSetStep", l, st, 0); }));
          showMessage(`${noteName(n)} effacé de la séquence`);
        }
        break;
      }
    }
  };
  const levelNote = (lane: number) => (drumPads ? lane : padNote(0, false, getState().ui.octave, 0, getState().ui.root));
  const release = (pad: number) => {
    const s = getState();
    if (s.ui.padMode === "play" && module) actions.noteOff(t, s.ui.padLevels ? levelNote(s.ui.lane) : padNote(pad, drumPads, s.ui.octave, s.ui.scale, s.ui.root));
  };

  // touches de pad enfoncées : seule une touche qui a joué un pad le relâche (Ctrl+Z ne coupe rien).
  // L'ensemble survit aux rendus : une touche relâchée après un rendu relâche bien sa note (mode Porte).
  const heldKeys = useRef(new Set<string>());
  useEffect(() => {
    const held = heldKeys.current;
    const down = (e: KeyboardEvent) => {
      if (e.repeat || e.ctrlKey || e.metaKey || e.altKey) return;
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLSelectElement || e.target instanceof HTMLTextAreaElement) return;
      if (e.code === "Space") { e.preventDefault(); actions.togglePlay(); return; }
      if (e.code === "KeyB") { e.preventDefault(); actions.breakKey(true); return; }
      if (e.code === "KeyN") { e.preventDefault(); actions.fill(true); return; }
      const pad = KEY_CODES[e.code];
      if (pad === undefined) return;
      e.preventDefault();
      held.add(e.code);
      press(pad, e.shiftKey ? 1 : 0.8);
    };
    const up = (e: KeyboardEvent) => {
      if (e.code === "KeyB") { actions.breakKey(false); return; }
      if (e.code === "KeyN") { actions.fill(false); return; }
      const pad = KEY_CODES[e.code];
      if (pad !== undefined && held.delete(e.code)) release(pad);
    };
    window.addEventListener("keydown", down);
    window.addEventListener("keyup", up);
    return () => { window.removeEventListener("keydown", down); window.removeEventListener("keyup", up); };
  });

  return (
    <div className="padgrid" role="grid" aria-label="Pads" data-paint-row>
      {ROWS.flat().map((pad) => {
        const note = padNote(pad, drumPads, octave, scale, root);
        const kind = mode === "play" || padMute ? (drumPads ? module?.padKinds[pad] ?? "sample" : padMute ? "sample" : "melodic") : mode === "mute" ? "track" : "sample";
        const scene = sceneBank * 16 + pad;
        const classes = ["pad", `pad--${kind}`, `pad--mode-${mode}`];
        const isLit = padMute ? (litLanes[pad] ?? 0) > 0 : mode === "mute" ? (litTracks[pad] ?? 0) > 0 : mode === "play" ? (drumPads ? (litLanes[pad] ?? 0) > 0 : (litNotes[note] ?? 0) > 0) : false; // mélodique : la note exacte du pad, sinon rien
        if (isLit) classes.push("pad--lit");
        if (mode === "play" && drumPads && pad === lane) classes.push("pad--selected");
        if (padMute) {
          if ((laneFlags[pad] ?? 0) & 1) classes.push("pad--muted");
          if ((laneFlags[pad] ?? 0) & 2) classes.push("pad--solo");
        } else {
          if (mode === "mute" && tracks[pad]?.mute) classes.push("pad--muted");
          if (mode === "mute" && !tracks[pad]?.instrument) classes.push("pad--empty");
        }
        if (mode === "scenes") {
          if (scene === Math.round(g.sceneA) - 1) classes.push("pad--sceneA");
          if (scene === Math.round(g.sceneB) - 1) classes.push("pad--sceneB");
          if ((scenes[scene]?.locks.length ?? 0) > 0) classes.push("pad--filled");
        }
        if (mode === "play" && drumPads && module?.padKinds[pad] === "sample" && !samples?.[module.type === "sampler" ? 0 : pad]) classes.push("pad--empty");
        if (mode === "steps") {
          const st = page * 16 + pad;
          if ((laneVel?.[st] ?? 0) > 0) classes.push("pad--filled");
          if (st >= patLength) classes.push("pad--empty");
          if (playStep === st) classes.push("pad--lit");
        }
        if (dropTarget === pad) classes.push("pad--drop");
        const caption = mode === "steps" ? `Pas ${page * 16 + pad + 1}` : mode === "scenes" ? `Scène ${scene + 1}` : padMute ? `${module?.padNames[pad] ?? ""}${(laneFlags[pad] ?? 0) & 2 ? " · solo" : ""}` : mode === "mute" ? tracks[pad]?.name ?? "" : !module ? "" : levels ? `Vélocité ${Math.round(((pad + 1) / 16) * 127)}` : drumPads ? module.padNames[pad] ?? "" : noteName(note);

        return (
          <button key={pad} type="button" className={classes.join(" ")} data-paint-step={mode === "steps" ? page * 16 + pad : undefined}
            aria-label={mode === "scenes" ? `Lancer la scène ${scene + 1}` : `Pad ${pad + 1} : ${caption}`}
            onPointerDown={(e) => {
              if (e.button !== 0) return;
              e.currentTarget.setPointerCapture(e.pointerId);
              const rect = e.currentTarget.getBoundingClientRect();
              const v = 1 - (e.clientY - rect.top) / rect.height;
              // v0.46.2 : mode Pas : glisser sur les pads voisins pose (ou retire) tous ces pas
              if (mode === "steps") {
                const st = page * 16 + pad, lane = getState().ui.lane;
                const velOf = (x: number) => getState().pattern?.lanes[lane]?.vel[x] ?? 0;
                const len = getState().pattern?.length ?? 0;
                if (st < len) beginStepPaint(e, st, {
                  originWasOn: velOf(st) > 0,
                  isOn: (x) => velOf(x) > 0,
                  toggle: (x) => { if (x < (getState().pattern?.length ?? 0)) actions.toggleStep(lane, x); },
                });
              }
              press(pad, Math.min(1, Math.max(0.2, 0.4 + v * 0.6)));
            }}
            onPointerUp={() => release(pad)}
            onPointerCancel={() => release(pad)}
            onContextMenu={(e) => { e.preventDefault(); if (drum) setUi({ lane: pad, screen: "sound" }); }}
            onDragOver={(e) => { if (drum || module?.type === "keys") { e.preventDefault(); setDropTarget(pad); } }}
            onDragLeave={() => setDropTarget(null)}
            onDrop={(e) => {
              e.preventDefault();
              setDropTarget(null);
              const file = e.dataTransfer.files[0];
              if (!file || !module) return;
              const index = module.type === "keys" ? 0 : pad;
              setUi({ lane: pad, screen: "sound" });
              void actions.loadSample(t, index, file);
            }}>
            <span className="pad-led" aria-hidden />
            <span className="pad-number">{pad + 1}</span>
            <span className="pad-key" aria-hidden>{KEY_LABELS[pad]}</span>
            <span className="pad-caption">{caption}</span>
          </button>
        );
      })}
    </div>
  );
}
