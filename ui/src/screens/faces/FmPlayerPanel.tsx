// Groovebox — OFAD — AGPL-3.0-or-later
// FM Player (v0.49) : la machine émulée du firmware chargé — son écran 240 × 240 en direct, ses boutons
// rotatifs, son potentiomètre MASTER, ses 14 touches de fonction et ses 27 touches de clavier (avec leurs
// diodes), dessinés dans le style de GOLEM (même disposition fonctionnelle, dessin propre), et la section
// « Firmware » : charger un .fwsc, état de la machine (démarrée, vitesse, trous dans le son).

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { actions, setUi, showMessage, useStore } from "../../engine/store";
import { Keyboard } from "../InstrumentFace";
import { FM1_FONDS, fm1FondStyle, setFm1Fond, useFm1Fond, type Fm1Fond } from "../../skin/skins";
import { paramAddress, useParam } from "../../controls/bindings";
import { FolderBrowser } from "../../controls/FolderBrowser";
import { LearnChips } from "../ScreenFrame";
import { LockSteps } from "./OctaPanel";
import { ShiftButtons } from "./ShiftButtons";
import { ParamMenuZone, useKnobExtras } from "../../controls/Encoder";

/** Boutons rotatifs : (libellé, n° du firmware). */
const ENC_LEFT: [string, number][] = [["Knob 1", 2], ["Knob 2", 3], ["Knob 3", 4], ["Knob 4", 5]];
const ENC_RIGHT: [string, number][] = [["Select", 0], ["Presets", 6], ["Algorithm", 1]];
/** Touches de fonction : (libellé, n° du firmware). */
const BTN_ROWS: [string, number][][] = [
  [["FX", 2], ["SCL", 3], ["ENV", 4], ["LFO", 5], ["EDIT", 6], ["GLO", 7]],
  [["HOME", 8], ["SAVE", 9], ["ARP", 10], ["SEQ", 11], ["PLAY", 12], ["REC", 13]],
];
const OCT: [string, number][] = [["OCT −", 0], ["OCT +", 1]];
/** Clavier F3..G5 : demi-tons 0..26 depuis Fa (touches 14..40). */
const BLACK = new Set([1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25]);
const WHITES = Array.from({ length: 27 }, (_, i) => i).filter((i) => !BLACK.has(i));
const POT_CH = 4;
/** GOLEM 1.0 : réglage de l'instrument qui pilote chaque bouton (Knob 1 à Algorithm, puis Master) :
    appui long ou clic droit → contrôleur MIDI, bouton assignable, automation (comme tous les synthés). */
const SLOT_OF_ENC: Record<number, number> = { 2: 6, 3: 7, 4: 8, 5: 9, 0: 10, 6: 11, 1: 12 };
const MASTER_SLOT = 13;
/** GOLEM 1.0 : course de chaque bouton (Knob 1 … Algorithm), réglages 28 à 34 : crans pour un tour complet. */
const TRAVEL_FIRST = 28;
const TRAVEL_CLICKS = [4, 8, 16, 32, 64, 128, 256, 512];
const TRAVEL_NAMES = ["Knob 1", "Knob 2", "Knob 3", "Knob 4", "Select", "Presets", "Algorithm"];
const travelIndex = (n: number | null) => Math.min(TRAVEL_CLICKS.length - 1, Math.floor((n ?? 6.5 / 8) * TRAVEL_CLICKS.length));
/** 1.0 (6 nov.) : position d'une liste « Course » (réglage à choix : 0 à 7) ; 6 = 256 crans si inconnue */
const travelIdxOf = (b: ReturnType<typeof useParam>) =>
  !b ? 6 : b.kind === "continuous" ? travelIndex(b.normalised) : Math.max(0, Math.min(TRAVEL_CLICKS.length - 1, Math.round(b.normalised * (TRAVEL_CLICKS.length - 1))));
const useBoxParam = (channel: number, index: number) => useParam(paramAddress(channel, "inst", 0, index));
const normOf = (b: ReturnType<typeof useParam>) => (b && b.kind === "continuous" ? b.normalised : null);

function useScreen(channel: number) {
  const [scr, setScr] = useState<{ img: ImageData | null; leds: number[]; booted: boolean; halted: boolean }>({ img: null, leds: [], booted: false, halted: false });
  useEffect(() => {
    let alive = true, busy = false;
    const tick = async () => {
      if (busy || document.hidden) return;
      busy = true;
      try {
        const r = await actions.fm1Screen(channel);
        if (!alive) return;
        if (!r) { setScr({ img: null, leds: [], booted: false, halted: false }); return; }
        const bin = atob(r.lcd);
        const img = new ImageData(240, 240);
        const d = img.data;
        for (let i = 0, p = 0; i + 1 < bin.length && p < d.length; i += 2, p += 4) {
          const v = bin.charCodeAt(i) | (bin.charCodeAt(i + 1) << 8); // RGB565 petit-boutiste
          d[p] = ((v >> 11) & 31) * 255 / 31;
          d[p + 1] = ((v >> 5) & 63) * 255 / 63;
          d[p + 2] = (v & 31) * 255 / 31;
          d[p + 3] = 255;
        }
        setScr({ img, leds: r.leds ?? [], booted: !!r.booted, halted: !!r.halted });
      } finally { busy = false; }
    };
    const id = window.setInterval(() => void tick(), 66); // ~15 images par seconde
    void tick();
    return () => { alive = false; window.clearInterval(id); };
  }, [channel]);
  return scr;
}

/** v0.49 : réglage par rotation — un doigt (ou un bouton physique posé sur l'écran, en caoutchouc
    conducteur) qui tourne autour du centre du bouton ; appui au centre : glisser vers le haut ou le bas. */
function angleAt(el: HTMLElement, x: number, y: number) {
  const r = el.getBoundingClientRect();
  const dx = x - (r.left + r.width / 2), dy = y - (r.top + r.height / 2);
  return { deg: (Math.atan2(dy, dx) * 180) / Math.PI, far: Math.hypot(dx, dy) > r.width * 0.3 };
}
const wrap = (d: number) => (d > 180 ? d - 360 : d < -180 ? d + 360 : d);

function Encoder({ channel, label, enc }: { channel: number; label: string; enc: number }) {
  // GOLEM 1.0 : bouton borné de 0 à 127, comme les autres (sur le vrai boîtier, un encodeur sans fin) :
  // on voit où il en est, et un contrôleur aussi. Chaque pas du réglage tourne le bouton du firmware d'un
  // cran (le moteur les envoie) ; aux butées, il s'arrête.
  const b = useBoxParam(channel, SLOT_OF_ENC[enc] ?? 6);
  // course complète = « Course des boutons » crans (64 à 512) : un cran de geste = un cran du firmware
  const full = TRAVEL_CLICKS[travelIdxOf(useBoxParam(channel, TRAVEL_FIRST + ((SLOT_OF_ENC[enc] ?? 6) - 6)))];
  const n = normOf(b) ?? 0.5;
  const v = Math.round(n * 100);
  const cur = useRef(n);
  cur.current = n;
  // 1.0 (6 nov.) : au doigt, toute la course en ~180 px de glissé (ou 270° en tournant), comme MASTER, quelle
  // que soit la course choisie : la position visée part de la valeur au toucher (pas de la valeur relue à chaque
  // image, qui faisait revenir le bouton) ; le moteur envoie les crans au firmware
  const drag = useRef<{ y: number; n0: number; rot: boolean; a: number; acc: number; clk: number } | null>(null);
  const aim = (target: number) => {
    const next = Math.max(0, Math.min(1, Math.round(target * full) / full));
    if (next === cur.current) return;
    cur.current = next;
    if (b && b.kind === "continuous") b.onChange(next);
  };
  const turn = (clicks: number) => {
    if (!clicks) return;
    if (!b || b.kind !== "continuous") { actions.fm1Touch(channel, 2, enc, clicks); return; }
    aim(cur.current + clicks / full);
  };
  const glide = (d: { n0: number; clk: number }, dn: number) => {
    if (b && b.kind === "continuous") { aim(d.n0 + dn); return; }
    const want = Math.round(dn * full); // sans réglage relié : crans envoyés tels quels
    if (want !== d.clk) { turn(want - d.clk); d.clk = want; }
  };
  const step = Math.max(1, Math.round(full / 100)); // molette, flèches : 1 % par cran
  const shown = -135 + n * 270;
  const extras = useKnobExtras(b);
  return (
    <ParamMenuZone binding={b} className="fm1-enc">
      <span {...extras.zone} className={`fm1-knobwrap ${extras.zone.className}`}>
      <button type="button" className="fm1-knob" role="slider" aria-valuemin={0} aria-valuemax={100} aria-valuenow={v}
        aria-label={`${label} (tourner : glisser vers le haut ou le bas, ou molette)`} title={`${label} : ${v} %`}
        onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); const g = angleAt(e.currentTarget, e.clientX, e.clientY); drag.current = { y: e.clientY, n0: cur.current, rot: g.far, a: g.deg, acc: 0, clk: 0 }; }}
        onPointerMove={(e) => {
          const d = drag.current; if (!d) return;
          if (d.rot) { const a = angleAt(e.currentTarget, e.clientX, e.clientY).deg; d.acc += wrap(a - d.a); d.a = a; glide(d, d.acc / 270); return; }
          glide(d, (d.y - e.clientY) / 180);
        }}
        onPointerUp={() => { drag.current = null; }} onPointerCancel={() => { drag.current = null; }}
        onWheel={(e) => turn(e.deltaY < 0 ? step : -step)}
        onKeyDown={(e) => { if (e.key === "ArrowUp" || e.key === "ArrowRight") turn(step); if (e.key === "ArrowDown" || e.key === "ArrowLeft") turn(-step); }}>
        <span className="fm1-knob-cap" style={{ transform: `rotate(${shown}deg)` }} />
      </button>
      {extras.decor}
      {b?.locked && <span className={`knob-lock knob-lock--${b.locked}`} aria-hidden />}
      </span>
      <small>{label}</small>
    </ParamMenuZone>
  );
}

function Pot({ channel }: { channel: number }) {
  // GOLEM 1.0 : le potentiomètre MASTER est un réglage de l'instrument (gardé dans le projet, pilotable au
  // contrôleur MIDI) ; le moteur le transmet à la machine
  const b = useBoxParam(channel, MASTER_SLOT);
  const n = normOf(b);
  const [local, setLocal] = useState(1023);
  const v = n === null ? local : Math.round(n * 1023);
  const extras = useKnobExtras(b);
  const drag = useRef<{ y: number; v: number; rot: boolean; a: number } | null>(null);
  const set = (x: number) => {
    const k = Math.max(0, Math.min(1023, Math.round(x)));
    if (b && b.kind === "continuous") b.onChange(k / 1023);
    else { setLocal(k); actions.fm1Touch(channel, 3, POT_CH, k); }
  };
  return (
    <ParamMenuZone binding={b} className="fm1-enc">
      <span {...extras.zone} className={`fm1-knobwrap ${extras.zone.className}`}>
      <button type="button" className="fm1-knob fm1-knob--pot" aria-label={`Master ${Math.round(v / 10.23)} %`}
        onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); const g = angleAt(e.currentTarget, e.clientX, e.clientY); drag.current = { y: e.clientY, v, rot: g.far, a: g.deg }; }}
        onPointerMove={(e) => {
          const d = drag.current; if (!d) return;
          if (d.rot) { const a = angleAt(e.currentTarget, e.clientX, e.clientY).deg; d.v = Math.max(0, Math.min(1023, d.v + (wrap(a - d.a) * 1023) / 270)); d.a = a; set(d.v); return; }
          set(d.v + (d.y - e.clientY) * 6);
        }}
        onPointerUp={() => { drag.current = null; }} onPointerCancel={() => { drag.current = null; }}
        onWheel={(e) => set(v + (e.deltaY < 0 ? 32 : -32))}>
        <span className="fm1-knob-cap" style={{ transform: `rotate(${-135 + (v / 1023) * 270}deg)` }} />
      </button>
      {extras.decor}
      {b?.locked && <span className={`knob-lock knob-lock--${b.locked}`} aria-hidden />}
      </span>
      <small>Master</small>
    </ParamMenuZone>
  );
}

/** GOLEM 1.0 : réglage de chaque touche du boîtier (FX…REC, puis OCT − / OCT +), pour le contrôleur MIDI. */
const keySlot = (id: number) => (id >= 2 && id <= 13 ? 12 + id : id === 0 ? 26 : id === 1 ? 27 : -1);

function Key({ channel, id, label, led, className }: { channel: number; id: number; label?: string; led: number; className: string }) {
  const down = useRef(false);
  // GOLEM 1.0 : clic droit ou appui long = apprendre une commande du contrôleur (seulement : pas de
  // modulation ni de macro sur une touche, qui la ferait clignoter sans fin dans le firmware)
  const slot = keySlot(id);
  const global = slot >= 0 ? paramAddress(channel, "inst", 0, slot) : null;
  const learning = useStore((s) => global !== null && (s.tick.learn ?? -1) === global);
  const mapped = useStore((s) => global !== null && s.midi.maps.some((m) => m[3] === global));
  const hold = useRef(0);
  const learn = () => {
    if (global === null) return;
    if (learning) { void actions.midiLearn(-1); return; }
    void actions.midiLearn(global);
    showMessage(`Touchez un pad ou une touche du contrôleur pour « ${label ?? "la touche"} »`);
  };
  const press = (on: boolean, vel?: number) => {
    if (down.current === on) return;
    down.current = on;
    if (on && vel !== undefined) actions.fm1Touch(channel, 4, vel, 0);
    actions.fm1Touch(channel, 1, id, on ? 1 : 0);
  };
  return (
    <button type="button" className={`${className}${led > 0.3 ? " lit" : ""}${learning ? " fm1-btn--learn" : ""}${mapped ? " fm1-btn--midi" : ""}`}
      style={{ "--led": led } as React.CSSProperties} aria-label={label ?? `Touche ${id}`}
      title={global !== null ? "Clic droit ou appui long : la piloter avec un pad ou une touche du contrôleur MIDI" : undefined}
      onContextMenu={(e) => { e.preventDefault(); learn(); }}
      onPointerDown={(e) => {
        if (e.button === 2) return;
        e.currentTarget.setPointerCapture(e.pointerId);
        const r = e.currentTarget.getBoundingClientRect();
        press(true, Math.max(1, Math.min(127, Math.round(1 + 126 * ((e.clientY - r.top) / r.height)))));
        if (e.pointerType === "touch") { window.clearTimeout(hold.current); hold.current = window.setTimeout(learn, 650); }
      }}
      onPointerUp={() => { window.clearTimeout(hold.current); press(false); }}
      onPointerCancel={() => { window.clearTimeout(hold.current); press(false); }}
      onKeyDown={(e) => { if (e.key === " " || e.key === "Enter") press(true, 100); }} onKeyUp={() => press(false)}>
      {label}
    </button>
  );
}

/** v0.49 : pistes T1 à T4 (une par canal MIDI du firmware : canal de T1, puis les suivants) — la T choisie
    est celle qu'on écrit (pages Pas, écran Séquence) et que jouent le piano, les pads et le MIDI. */
export function Fm1Lanes({ ms = false }: { ms?: boolean } = {}) {
  const lane = useStore((s) => s.ui.lane);
  const lit = useStore((s) => s.litLanes);
  // GOLEM 1.0 : en grand, coupure et solo de chaque T au-dessus de son bouton (comme les lignes de Pieuvre 8)
  const lanes = useStore((s) => (ms ? s.pattern?.lanes : undefined));
  return (
    <div className={`fm1-lanes${ms ? " fm1-lanes--ms" : ""}`} role="radiogroup" aria-label="Piste du firmware écrite et jouée">
      {[0, 1, 2, 3].map((k) => {
        const btn = (
          <button key={k} type="button" role="radio" aria-checked={lane === k} className={`fm1-lane${lane === k ? " on" : ""}${(lit?.[k] ?? 0) > 0 ? " is-lit" : ""}`}
            onClick={() => setUi({ lane: k })} title={`T${k + 1} : écrire cette piste et la jouer au clavier (canal de T1 + ${k})`}>
            <span className="fc-led" aria-hidden />T{k + 1}
          </button>
        );
        if (!ms) return btn;
        const ln = lanes?.[k];
        return (
          <div key={k} className="fm1-lane-ms">
            <span className="fm1-ms">
              <button type="button" className={`fm1-m${ln?.mute ? " on" : ""}`} aria-pressed={!!ln?.mute} aria-label={`Couper T${k + 1}`}
                onClick={() => void actions.seq("seqLaneMute", k, !ln?.mute)}>M</button>
              <button type="button" className={`fm1-s${ln?.solo ? " on" : ""}`} aria-pressed={!!ln?.solo} aria-label={`T${k + 1} en solo`}
                onClick={() => void actions.seq("seqLaneSolo", k, !ln?.solo)}>S</button>
            </span>
            {btn}
          </div>
        );
      })}
    </div>
  );
}

/** GOLEM 1.0 : en grand, au-dessus du piano : les pas de la T choisie (séquenceur à verrous, notes),
    et les flèches pour décaler la T ou toutes les T d'un pas. */
function Fm1BigSteps() {
  const lane = useStore((s) => s.ui.lane);
  // 1.0 (6 nov.) : Copier / Coller / Effacer / Verrous à gauche de T1 et Tout, sous l'écran : la rangée des
  // pas reste sur une ligne et ne cache plus le piano quand un pas est choisi
  const [tools, setTools] = useState<HTMLSpanElement | null>(null);
  // 1.0 (6 nov., 16 h 15) : les outils des verrous centrés sous l'écran de la machine
  const head = useRef<HTMLDivElement>(null);
  useLayoutEffect(() => {
    const h = head.current; if (!h) return;
    const place = () => {
      const scr = h.closest(".fm1big")?.querySelector(".fm1big-screen");
      if (!scr) return;
      const a = scr.getBoundingClientRect(), b = h.getBoundingClientRect();
      if (b.width > 0) h.style.setProperty("--tools-x", `${a.left + a.width / 2 - b.left}px`);
    };
    place();
    const ro = new ResizeObserver(place);
    ro.observe(h);
    window.addEventListener("resize", place);
    return () => { ro.disconnect(); window.removeEventListener("resize", place); };
  }, []);
  return (
    <div className="fm1big-steps">
      <div className="fm1big-steps-head" ref={head}>
        <span className="fm1big-steps-tools" ref={setTools} />
        <ShiftButtons lane={lane} label={`T${lane + 1}`} what={`la piste T${lane + 1}`} />
        <ShiftButtons label="Tout" what="toutes les pistes T" />
      </div>
      <LockSteps T={lane} name={`T${lane + 1}`} shift="none" octaNote={(n) => n} toolsHost={tools} />
    </div>
  );
}

/** 1.0 : petit bouton de couleur du fond (ambre, nuit, émeraude, sobre), gardé sur l'appareil. */
function Fm1FondPicker() {
  const fond = useFm1Fond();
  return (
    <span className="fm1-fonds" role="radiogroup" aria-label="Couleur du fond">
      {(Object.keys(FM1_FONDS) as Fm1Fond[]).map((f) => (
        <button key={f} type="button" role="radio" aria-checked={fond === f} className={`fm1-fond${fond === f ? " on" : ""}`}
          style={{ background: FM1_FONDS[f].swatch }} onClick={() => setFm1Fond(f)} title={`Fond ${FM1_FONDS[f].label}`} aria-label={`Fond ${FM1_FONDS[f].label}`} />
      ))}
    </span>
  );
}

/** La machine : écran, commandes et clavier. */
export function Fm1Panel({ channel }: { channel: number }) {
  const scr = useScreen(channel);
  const cv = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const g = cv.current?.getContext("2d");
    if (!g) return;
    if (scr.img) g.putImageData(scr.img, 0, 0);
    else { g.fillStyle = "#07090c"; g.fillRect(0, 0, 240, 240); }
  }, [scr.img]);
  const led = (id: number) => scr.leds[id] ?? 0;
  const [big, setBig] = useState(false);
  return (
    <div className="fm1">
      <span className="fm1-tools"><Fm1FondPicker />
      <button type="button" className="fm1-bigbtn" onClick={() => setBig(true)} aria-label="Ouvrir la machine en grand (tout l'écran)">⤢ Grand</button></span>
      {big && <Fm1Big channel={channel} scr={scr} onClose={() => setBig(false)} />}
      <div className="fm1-top">
        <div className="fm1-side">
          <div className="fm1-encs">{ENC_LEFT.map(([l, e]) => <Encoder key={e} channel={channel} label={l} enc={e} />)}</div>
          {BTN_ROWS.map((row, r) => (
            <div key={r} className="fm1-btns">{row.map(([l, id]) => <Key key={id} channel={channel} id={id} label={l} led={led(id)} className="fm1-btn" />)}</div>
          ))}
        </div>
        <div className="fm1-screen">
          <canvas ref={cv} width={240} height={240} aria-label="Écran de la machine" />
          {!scr.img && <span className="fm1-empty">Aucun firmware chargé</span>}
          {scr.halted && <span className="fm1-empty fm1-empty--err">Le firmware s'est arrêté</span>}
        </div>
        <div className="fm1-side fm1-side--right">
          <div className="fm1-encs fm1-encs--2"><Pot channel={channel} />{ENC_RIGHT.slice(0, 1).map(([l, e]) => <Encoder key={e} channel={channel} label={l} enc={e} />)}</div>
          <div className="fm1-encs fm1-encs--2">{ENC_RIGHT.slice(1).map(([l, e]) => <Encoder key={e} channel={channel} label={l} enc={e} />)}</div>
          <div className="fm1-btns">{OCT.map(([l, id]) => <Key key={id} channel={channel} id={id} label={l} led={led(id)} className="fm1-btn fm1-btn--wide" />)}</div>
        </div>
      </div>
      <div className="fm1-keys" role="group" aria-label="Clavier de la machine (Fa3 à Sol5)">
        {WHITES.map((n) => <Key key={n} channel={channel} id={14 + n} led={led(14 + n)} className="fm1-key" />)}
        {[...BLACK].map((n) => {
          const after = WHITES.filter((w) => w < n).length; // touche noire posée entre deux blanches
          return <span key={n} className="fm1-key-slot" style={{ "--at": after } as React.CSSProperties}><Key channel={channel} id={14 + n} led={led(14 + n)} className="fm1-key fm1-key--black" /></span>;
        })}
      </div>
    </div>
  );
}

/** GOLEM 1.0 : course de chaque bouton : combien de crans du firmware pour un tour complet (0 à 100 %).
    Peu de crans pour un choix court (Algorithm en piste 1 à 4 : 4 crans, un quart de tour chacun). */
function Fm1Travels({ channel }: { channel: number }) {
  const binds = TRAVEL_NAMES.map((_, i) => useBoxParam(channel, TRAVEL_FIRST + i)); // eslint-disable-line react-hooks/rules-of-hooks
  return (
    // 1.0 (6 nov.) : repliée par défaut (on n'y touche qu'en changeant de firmware) ; les listes sont des
    // réglages à choix : on choisit la position (elles étaient bloquées, traitées comme continues)
    <details className="fm1-travels" aria-label="Course des boutons : crans du firmware pour un tour complet">
      <summary className="fm1-travels-title">Course des boutons (crans par tour)</summary>
      <div className="fm1-travels-grid">
      {TRAVEL_NAMES.map((name, i) => {
        const b = binds[i];
        const idx = travelIdxOf(b);
        return (
          <label key={name} className="fm1-travel">
            <span>{name}</span>
            <select value={idx} disabled={!b}
              onChange={(e) => {
                const k = Number(e.target.value);
                if (!b) return;
                if (b.kind === "continuous") b.onChange((k + 0.5) / TRAVEL_CLICKS.length);
                else b.entry?.pick(k);
              }}>
              {TRAVEL_CLICKS.map((c, k) => <option key={c} value={k}>{c}</option>)}
            </select>
          </label>
        );
      })}
      </div>
    </details>
  );
}

/** Section « Firmware » : le fichier chargé, charger un autre, état de la machine. */
export function Fm1Firmware({ channel }: { channel: number }) {
  const name = useStore((s) => s.channels[channel]?.samples?.[0]);
  const st = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const absent = st[0] === -1;
  const loaded = (st[0] ?? 0) > 0.5, booted = (st[1] ?? 0) > 0.5, halted = (st[2] ?? 0) > 0.5;
  const speed = st[3] ?? 0;
  const paused = (st[5] ?? 0) > 0.5;
  // GOLEM 1.0 : changer de firmware = la bibliothèque (déjà importés) ; « Firmware… » en haut sert seulement
  // à importer un nouveau fichier (moins de risque de se tromper en jouant, surtout au téléphone)
  const [lib, setLib] = useState(false);
  if (lib && !absent)
    return (
      <div className="fm1-fw fm1-fw--lib">
        <FolderBrowser kind="samples" title="Bibliothèque · firmwares" start={["Firmwares"]} compact
          onPick={(rel) => { void actions.loadSampleFile(channel, 0, rel); setLib(false); }} onClose={() => setLib(false)} />
      </div>
    );
  return (
    <div className="fm1-fw">
      <div className="fm1-fw-name">{absent ? "Lecteur absent de cette version" : loaded ? name ?? "Firmware" : "Aucun firmware"}</div>
      <div className="fm1-fw-state">
        {absent ? "GOLEM a été compilé sans Rust" : !loaded ? "Choisissez un firmware dans la bibliothèque (« Firmware… » en haut pour en importer un nouveau)"
          : halted ? "Arrêté sur une erreur du firmware" : paused ? "En pause (rien ne joue depuis 20 s) : une note ou une touche la relance" : booted ? `En marche · ${speed.toFixed(1)} × le temps réel${speed > 0 && speed < 1.05 ? " (ordinateur juste : augmentez l'Avance)" : ""}` : "Démarrage…"}
      </div>
      <button type="button" className="touch" disabled={absent} onClick={() => setLib(true)}
        title="Les firmwares déjà dans la bibliothèque (dossier Firmwares). Pour en importer un nouveau : « Firmware… » en haut">Changer de firmware…</button>
      {!absent && <Fm1Travels channel={channel} />}
    </div>
  );
}

/** v0.49 : la machine en grand, sur toute la zone de l'appli (téléphone et tablette) : écran agrandi,
    boutons à la taille du doigt et, en bas, le piano de GOLEM avec T1 à T4 (les touches du boîtier,
    elles, restent sur la façade). Posée dans la scène (hors de l'appareil mis à l'échelle), tailles en
    unités du conteneur : elle tient dans la zone visible, quelle que soit la taille annoncée. */
function Fm1Big({ channel, scr, onClose }: { channel: number; scr: ReturnType<typeof useScreen>; onClose: () => void }) {
  const cv = useRef<HTMLCanvasElement>(null);
  const fond = useFm1Fond();
  const name = useStore((s) => s.channels[channel]?.samples?.[0]);
  useEffect(() => {
    const g = cv.current?.getContext("2d");
    if (!g) return;
    if (scr.img) g.putImageData(scr.img, 0, 0);
    else { g.fillStyle = "#07090c"; g.fillRect(0, 0, 240, 240); }
  }, [scr.img]);
  useEffect(() => {
    const k = (e: KeyboardEvent) => { if (e.key === "Escape") onClose(); };
    window.addEventListener("keydown", k);
    return () => window.removeEventListener("keydown", k);
  }, [onClose]);
  const led = (id: number) => scr.leds[id] ?? 0;
  const host = document.querySelector(".stage") ?? document.body;
  // GOLEM 1.0 : changer de firmware sans quitter le mode Grand (bibliothèque, ouverte sur « Firmwares »)
  const [lib, setLib] = useState(false);
  return createPortal(
    <div className="fm1big" role="dialog" aria-label="FM Player en grand" style={fm1FondStyle(fond)}>
      <div className="fm1big-bar">
        <button type="button" className={`fm1big-lib-btn${lib ? " on" : ""}`} aria-pressed={lib} onClick={() => setLib(!lib)}
          title="Changer de firmware : la bibliothèque, ouverte sur le dossier Firmwares">Bibliothèque</button>
        <span className="fm1big-name">{name ?? "FM Player"}</span>
        <LearnChips />
        <Fm1FondPicker />
        <button type="button" className="fm1big-close" onClick={onClose}>✕ Fermer</button>
      </div>
      {lib && (
        <div className="fm1big-lib">
          <FolderBrowser kind="samples" title="Bibliothèque · firmwares" start={["Firmwares"]}
            onPick={(rel) => { void actions.loadSampleFile(channel, 0, rel); setLib(false); }} onClose={() => setLib(false)} />
        </div>
      )}
      <div className="fm1big-main">
        <div className="fm1big-side">
          <div className="fm1big-encs">{ENC_LEFT.map(([l, e]) => <Encoder key={e} channel={channel} label={l} enc={e} />)}</div>
          {BTN_ROWS.map((row, r) => (
            <div key={r} className="fm1big-btns">{row.map(([l, id]) => <Key key={id} channel={channel} id={id} label={l} led={led(id)} className="fm1-btn" />)}</div>
          ))}
        </div>
        <div className="fm1big-screen">
          <canvas ref={cv} width={240} height={240} aria-label="Écran de la machine" />
          {!scr.img && <span className="fm1-empty">Aucun firmware chargé</span>}
          {scr.halted && <span className="fm1-empty fm1-empty--err">Le firmware s'est arrêté</span>}
        </div>
        <div className="fm1big-side fm1big-side--right">
          <div className="fm1big-encs fm1big-encs--2"><Pot channel={channel} />{ENC_RIGHT.map(([l, e]) => <Encoder key={e} channel={channel} label={l} enc={e} />)}</div>
          {/* GOLEM 1.0 : pas d'OCT − / OCT + en grand : ils décalent les touches du boîtier, absentes ici ;
              le piano de GOLEM a ses propres boutons d'octave */}
        </div>
      </div>
      <div className="fm1big-play">
        <Fm1Lanes ms />
        <div className="fm1big-keys">
          <Fm1BigSteps />
          <Keyboard track={channel} color="oklch(0.8 0.15 70)" />
        </div>
      </div>
    </div>,
    host,
  );
}
