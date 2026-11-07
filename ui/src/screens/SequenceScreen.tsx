// Groovebox — OFAD — AGPL-3.0-or-later
// Écran SÉQUENCE de la piste choisie (principe des séquenceurs à pas Elektron / MPC) :
//  - batterie ou sampler à découpes : une ligne par pad ;
//  - instrument mélodique : 4 lignes de notes (accords), la note est écrite dans chaque pas.
// Clic = poser / retirer (Maj : accent). Clic droit sur un pas posé = l'éditer : note, vélocité,
// durée, décalage, condition (probabilité, A:B, FILL, PRÉ, VOISIN, 1ER), répétitions, flam,
// verrous de réglages.
// Fonctions de pas de la piste (principe des séquenceurs de basse et de batterie des années 80) :
// une ligne ACCENT (tous les sons du pas, dosé par l'encodeur Accent) et, en mélodique, une ligne
// GLISSÉ (la note tient jusqu'au pas suivant et glisse vers lui). Longueur propre par pad (polymétrie).
// Longueur par blocs de mesures (jusqu'à 32), vitesse et sens de lecture par piste ou par ligne.
// 8 patterns par piste (A à H) : choisir celui qu'on édite ; clic droit (appui long) : le lancer.
// Sélection (outils de séquenceur classiques) : Ctrl+clic ajoute / retire un pas ; tirer depuis
// une case vide dessine un rectangle de sélection (Maj : ajoute) ; tirer un pas sélectionné le
// déplace (dans la ligne ou vers un autre pad ; en mélodique, vers une autre note), Ctrl+tirer
// le copie. Ctrl+C / X / V / D (dupliquer), Suppr, Ctrl+A, Échap, flèches (déplacer d'un pas,
// d'une ligne ; Maj : d'une mesure ou d'une octave). Avec plusieurs pas sélectionnés, les
// encodeurs règlent tous les pas à la fois. Sur tablette : boutons Sélection, Copier, Coller…

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { actions, baseValue, getState, resolveGlobal, ROLL_MAX, ROLL_MIN, setRollBase, setUi, showMessage, slots, useStore, writePref } from "../engine/store";
import { StepAmountEditor, useLongPress, type StepAmountKind } from "../controls/StepAmount";
import { allKeys, cellKey, flagMoves, getClipboard, getLockClip, laneOf, origin, payload, place, retarget, setClipboard, snapshot, stepOf, type CellData, type Plan } from "../engine/selection";
import { condChoices, DIRECTIONS, RATCHET_CHOICES, condKey, condText, parseCond, ratchetKey, ratchetText, DIVISIONS, DIVISION_ORDER, PATTERN_NAMES, ROW_PAGE, ROW_PAGES, rowPageName, SPEEDS, STEPS_PER_BAR, FLAM_CHOICES, flamText, STEP_ACCENT, STEP_SLIDE, STEP_REC, STEP_TRIGLESS, type PatternData, hostCode } from "../engine/protocol";
import { inScale, padNote } from "../engine/scales";
import { formatNorm, noteName, toPlain, trackColor } from "../engine/params";
import { rangeEntry, steppedBinding, valueBinding, withCtl } from "../controls/bindings";
import type { EncoderBinding } from "../controls/Encoder";

/** Durée d'une note en huitièmes de pas : jusqu'à 128 pas (8 mesures en double croches). */
const MAX_LEN8 = 1024;
const LEN_CHOICES = [1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 640, 768, 896, MAX_LEN8];
import { ScreenFrame } from "./ScreenFrame";
import { registerCtl, uiCode } from "../engine/ctl";
import { CurveEditor } from "../controls/CurveEditor";
import { describeGlobal } from "../engine/describe";

const PAGE = 16;
const LANE_LENGTHS = [0, ...Array.from({ length: 32 }, (_, i) => i + 1), 48, 64, 96, 128, 192, 256, 384, 512];
const BAR_CHOICES = [1, 2, 3, 4, 8, 12, 16, 24, 32];
const ROLL_ROWS = 24; // deux octaves affichées
/** 1.0 : vue sur une octave (cases deux fois plus hautes) ou sur deux */
const rollRows = (u: { rollOne?: boolean }) => (u.rollOne ? 12 : ROLL_ROWS);
const toggleRollOne = () => { const v = !getState().ui.rollOne; setUi({ rollOne: v }); writePref("rollOne", v); };

/** v0.46 fix28 : clavier du rouleau de notes : toucher joue la note ; glisser le doigt vers le haut ou le
    bas fait défiler les octaves (comme la molette sur PC), sans jouer. */
const keyDrag: { id: number; y: number; base: number; n: number; t: number; playing: boolean; step: number } = { id: -1, y: 0, base: 0, n: 0, t: 0, playing: false, step: 60 };
function rollKeyHandlers(t: number, n: number) {
  return {
    onPointerDown: (e: React.PointerEvent<HTMLButtonElement>) => {
      e.currentTarget.setPointerCapture(e.pointerId);
      const rowH = e.currentTarget.getBoundingClientRect().height || 12;
      Object.assign(keyDrag, { id: e.pointerId, y: e.clientY, base: getState().ui.rollBase, n, t, playing: true, step: Math.max(36, rowH * 6) });
      actions.noteOn(t, n, 0.8, false);
    },
    onPointerMove: (e: React.PointerEvent<HTMLButtonElement>) => {
      if (keyDrag.id !== e.pointerId) return;
      const dy = e.clientY - keyDrag.y;
      if (keyDrag.playing && Math.abs(dy) > 10) { keyDrag.playing = false; actions.noteOff(keyDrag.t, keyDrag.n); }
      if (keyDrag.playing) return;
      // tirer vers le bas fait apparaître les notes du dessus (comme une liste qu'on fait défiler)
      const oct = Math.round(dy / keyDrag.step);
      const b = keyDrag.base + oct * 12;
      if (b !== getState().ui.rollBase) setRollBase(b);
    },
    onPointerUp: (e: React.PointerEvent<HTMLButtonElement>) => {
      if (keyDrag.id !== e.pointerId) return;
      keyDrag.id = -1;
      if (keyDrag.playing) { keyDrag.playing = false; actions.noteOff(keyDrag.t, keyDrag.n); }
    },
    onPointerCancel: (e: React.PointerEvent<HTMLButtonElement>) => {
      if (keyDrag.id !== e.pointerId) return;
      keyDrag.id = -1;
      if (keyDrag.playing) { keyDrag.playing = false; actions.noteOff(keyDrag.t, keyDrag.n); }
    },
  };
}
/** v0.44.1 : avec une gamme choisie, les lignes de la gamme sont claires (à jouer) et les autres
    sombres, dièse ou pas ; sans gamme (chromatique), le dessin du clavier (touches noires). */
const scaleRowClass = (_n: number, black: boolean, out: boolean, scale: number) =>
  scale > 0 ? (out ? " roll-row--out" : " roll-row--in") : black ? " roll-row--black" : "";

/** 1.0 (7 nov.) : boutons A à P du bas de la séquence au contrôleur MIDI (appui long ou clic droit, comme les
    rangées du mixeur) : la commande apprise lance ce pattern sur la piste choisie. Valeurs d'écran
    « Séquence.Pattern X » enregistrées dès le chargement : elles marchent quel que soit l'écran affiché.
    Un appui (note : bascule 0/1 ; CC : appui puis relâché) = un lancement (relâché ignoré pendant 250 ms). */
const patLaunchAt = new Map<number, number>();
const patCtlId = (p: number) => `Séquence.Pattern ${PATTERN_NAMES[p]}`;
const patCtlLabel = (p: number) => `Lancer le pattern ${PATTERN_NAMES[p]} (piste choisie)`;
PATTERN_NAMES.forEach((_, p) => registerCtl(patCtlId(p), {
  kind: "continuous", label: patCtlLabel(p), text: "", normalised: 0,
  onChange: () => {
    const now = performance.now();
    if (now - (patLaunchAt.get(p) ?? 0) < 250) return;
    patLaunchAt.set(p, now);
    void actions.launchPattern(Math.min(getState().focus, 15), p);
  },
}));
async function learnPattern(p: number) {
  const code = await uiCode(patCtlId(p), patCtlLabel(p));
  if (code === null) return;
  void actions.midiLearn(code);
  showMessage(`Pattern ${PATTERN_NAMES[p]} : touchez un bouton ou un pad du contrôleur MIDI`);
}

/** 1.0 : note jouée en touchant le nom d'une ligne (relâchée au lever du doigt). */
let auditionNote: { t: number; n: number } | null = null;

export function SequenceScreen() {
  const cat = useStore((s) => s.catalog);
  const pattern = useStore((s) => s.pattern);
  const extras = useStore((s) => s.extras);
  const focus = useStore((s) => s.focus);
  const tracks = useStore((s) => s.tracks.tracks);
  const pos = useStore((s) => s.tick.pos);
  const playing = useStore((s) => s.tick.t.playing);
  const g = useStore((s) => s.tick.g);
  const ui = useStore((s) => s.ui);
  const lit = useStore((s) => s.litLanes);
  const launch = useStore((s) => s.launch);
  const live = useStore((s) => s.tick.pat[Math.min(s.focus, 15)]);
  const played = useStore((s) => s.played);
  const instVals = useStore((s) => s.channels[Math.min(s.focus, 15)]?.inst);
  const L = cat?.layout;
  const t = Math.min(focus, (L?.numTracks ?? 16) - 1);
  const track = tracks[t];
  const module = cat?.instruments.find((i) => i.type === track?.instrument);
  const drum = !!module?.drumKit;
  const [drag, setDrag] = useState<Drag | null>(null);
  const [cursor, setCursor] = useState<{ row: number; step: number } | null>(null);
  const [patOpen, setPatOpen] = useState(false); // réglages du pattern (mesures, division, vitesse) depuis la pastille
  const [laneRoll, setLaneRoll] = useState(false); // v0.44.1 : Pieuvre 8 — la piste T choisie dépliée en clavier chromatique
  const octa = track?.instrument === "octa";
  // v0.49 : instruments à pistes internes (Pieuvre 8, FM Player T1 à T4) : chaque ligne T joue sa note
  const laneInst = !!module?.laneNotes;
  const showLaneRoll = laneInst && laneRoll;
  const patRef = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!patOpen) return;
    // Échap ou un appui ailleurs referme la fenêtre du pattern (sans quitter le mode de l'écran)
    const key = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); setPatOpen(false); } };
    const down = (e: PointerEvent) => {
      const el = e.target as HTMLElement | null;
      if (!patRef.current?.contains(el) && !el?.closest(".chip--button")) setPatOpen(false);
    };
    window.addEventListener("keydown", key, true);
    window.addEventListener("pointerdown", down, true);
    return () => { window.removeEventListener("keydown", key, true); window.removeEventListener("pointerdown", down, true); };
  }, [patOpen]);
  const hover = useRef<{ row: number; step: number } | null>(null);
  const gesture = useRef<Gesture | null>(null);
  useSelectionKeys(drum, t, hover, cursor);

  // v0.47 (maj. 5 nov.) : Pieuvre 8 comme les synthés — la dernière touche jouée (clavier, pads, MIDI à l'arrêt)
  // devient la note du pas choisi (ou du seul pas sélectionné), et la note des pas posés ensuite
  // sur la ligne de la T qui l'a jouée. « Zones » : la note est ramenée dans la zone de sa T
  // (bas de zone = 60, hauteur d'origine), comme le fait le moteur.
  const octaParam = (id: string) => {
    if (!module) return 0;
    const i = module.params.findIndex((p) => p.id === id);
    return i < 0 ? 0 : Math.round(toPlain(module.params[i], instVals?.[i] ?? 0));
  };
  const octaLaneNote = (note: number, T?: number): { lane: number; note: number } | null => {
    if (octaParam("kbmode") !== 1) return { lane: T ?? Math.min(getState().ui.lane, 7), note };
    for (let k = 0; k < 8; ++k) {
      if (T !== undefined && k !== T) continue;
      const lo = octaParam(`t${k + 1}_zlo`), hi = octaParam(`t${k + 1}_zhi`);
      if (note >= lo && note <= hi) return { lane: k, note: Math.max(0, Math.min(127, note - lo + 60)) };
    }
    return null;
  };
  const lastPlayed = useRef(played.n);
  const octaWrite = useRef<{ lane: number; note: number } | null>(null);
  useEffect(() => { octaWrite.current = null; }, [t]);
  useEffect(() => {
    if (played.n === lastPlayed.current) return;
    lastPlayed.current = played.n;
    if ((drum && !laneInst) || played.note < 0 || played.track !== t) return;
    // v0.47 (5 nov. après-midi) : le clavier ne change plus la note du pas choisi ; il choisit seulement la note des
    // pas qu'on pose ensuite (Pieuvre 8 : sur la ligne T de sa zone). Note d'un pas posé : Note ◀ ▶ de son menu.
    if (laneInst) {
      const mapped = octaLaneNote(played.note);
      if (mapped) octaWrite.current = mapped;
    }
  }, [played]); // eslint-disable-line react-hooks/exhaustive-deps
  /** Note écrite par un pas posé sur la ligne T de Pieuvre 8 (undefined : la note de la ligne reste). */
  const octaNewNote = (row: number) => (laneInst && octaWrite.current && octaWrite.current.lane === row ? octaWrite.current.note : undefined);

  // v0.46 fix29 : page des puces de pattern (A–H, I–P), calée sur le pattern édité
  const editedPat = pattern?.pattern ?? 0;
  const [chipPage, setChipPage] = useState(Math.floor(editedPat / ROW_PAGE));
  useEffect(() => { setChipPage(Math.floor(editedPat / ROW_PAGE)); }, [editedPat, t]);
  if (!cat || !L || !pattern) return <ScreenFrame title="Séquence" encoders={[]}><div /></ScreenFrame>;

  const spb = STEPS_PER_BAR[pattern.division] ?? 16;
  // la tête de lecture n'est montrée que si le pattern édité est celui qui joue
  const showsPlaying = playing && (live ? live[0] : pattern.playing) === pattern.pattern;
  const filled = launch?.tracks[t]?.filled ?? [];
  const stepsPerBeat = spb / 4;
  const pages = Math.max(1, Math.ceil(pattern.length / PAGE));
  const page = Math.min(ui.page, pages - 1);
  const first = page * PAGE;
  const bars = pattern.length / spb;
  const rows = Array.from({ length: drum && module?.padNames.length ? Math.min(16, module.padNames.length) : 16 }, (_, i) => i); // v0.42 : Pieuvre 8 : 8 lignes (T1 à T8)
  // v0.47 (maj. 5 nov.) : un seul pas sélectionné (mode Sélection, rectangle) ouvre son menu, comme un pas choisi
  const single = ui.selection.length === 1 ? ui.selection[0] : null;
  const lane = single !== null ? laneOf(single) : Math.min(ui.lane, 15);
  const step = single !== null ? stepOf(single) : ui.step;
  const L0 = pattern.lanes[lane];
  const stepOn = step !== null && (L0?.vel[step] ?? 0) > 0;
  const extra = step !== null ? extras.find((e) => e.lane === lane && e.step === step) : undefined;
  // Mélodique : quelle ligne porte la note n au pas s (accords : une ligne par note)
  const laneWith = (n: number, s: number) => pattern.lanes.findIndex((ln) => ln.vel[s] > 0 && ln.note[s] === n);
  const freeLane = (s: number) => pattern.lanes.findIndex((ln) => ln.vel[s] === 0);
  // la note n est-elle tenue au pas s par une note commencée avant ? (table calculée une fois par rendu)
  const held = new Set<number>();
  for (const ln of pattern.lanes)
    for (let st = 0; st < pattern.length; ++st)
      if (ln.vel[st] > 0) for (let k = 1; ln.len[st] > k * 8 && st + k < pattern.length; ++k) held.add(ln.note[st] * 4096 + st + k);
  const heldBy = (n: number, s: number) => held.has(n * 4096 + s);
  const toggleNote = (n: number, s: number) => {
    const l = laneWith(n, s);
    if (l >= 0) {
      if (ui.lane === l && ui.step === s) { void actions.seq("seqSetStep", l, s, 0); setUi({ step: null, stepEdit: false }); }
      else setUi({ lane: l, step: s, stepEdit: false });
      return;
    }
    const f = freeLane(s);
    if (f < 0) return;
    void actions.seq("seqSetNote", f, s, n).then(() => actions.seq("seqSetStep", f, s, ui.shift ? 127 : 100));
    setUi({ lane: f, step: s, stepEdit: false });
    actions.noteOn(t, n, 0.75, false);
    window.setTimeout(() => actions.noteOff(t, n), 160);
  };

  const setCond = (patch: Partial<{ cond: number; a: number; b: number; prob: number }>) => {
    if (step === null) return;
    const c = { cond: extra?.cond ?? 0, a: extra?.a ?? 1, b: extra?.b ?? 2, prob: extra?.prob ?? 100, ...patch };
    void actions.extras("stepCondition", lane, step, c.cond, c.a, c.b, c.prob);
  };

  // ---------------------------------------------------------------- sélection
  const selection = ui.selection;
  const selSet = new Set(selection);
  const selCells = selection.length > 1 ? snapshot(pattern, extras, selection) : [];
  /** Ligne affichée d'un pas : le pad (batterie) ou la note (mélodique). */
  const rowOfKey = (k: number) => (drum ? laneOf(k) : pattern.lanes[laneOf(k)]?.note[stepOf(k)] ?? 0);
  const ghostSet = new Set<string>();
  if (drag && drag.kind === "move") for (const k of drag.keys) ghostSet.add(`${rowOfKey(k) + drag.dRow}:${stepOf(k) + drag.dStep}`);
  const inMarquee = (row: number, st: number) => !!drag && drag.kind === "rect"
    && row >= Math.min(drag.r0, drag.r1) && row <= Math.max(drag.r0, drag.r1) && st >= Math.min(drag.s0, drag.s1) && st <= Math.max(drag.s0, drag.s1);

  // le tempo se règle dans la barre d'état (tirer la valeur) : les 8 encodeurs servent à la piste
  const accentBinding = steppedBinding("Accent", Array.from({ length: 21 }, (_, i) => i * 5), Math.round((pattern.accent ?? 50) / 5) * 5,
    (a) => void actions.seq("seqAccent", a), (a) => `${a} %`);
  const trackEncoders: (EncoderBinding | null)[] = [
    withCtl(valueBinding("Swing", (g.swing - 50) / 25, `${Math.round(g.swing)} %`, (v) => actions.setGlobal("swing", 50 + v * 25)),
      hostCode("swing"), rangeEntry((g.swing - 50) / 25, 50, 75, "%", (v) => actions.setGlobal("swing", 50 + v * 25))),
    steppedBinding("Longueur", BAR_CHOICES.filter((b) => b * spb <= L.maxSteps), BAR_CHOICES.includes(bars) ? bars : 1,
      (b) => { void actions.seq("seqTrackLength", b * spb); setUi({ page: 0 }); }, (b) => `${b} mes.`),
    steppedBinding("Pas (fin)", Array.from({ length: Math.min(L.maxSteps, 64) }, (_, i) => i + 1), Math.min(64, pattern.length),
      (n) => void actions.seq("seqTrackLength", n), (n) => `${n} pas`),
    steppedBinding("Vitesse", [0, 1, 2, 3, 4, 5, 6], pattern.speed, (s) => void actions.seq("seqTrackSpeed", s), (s) => SPEEDS[s]),
    steppedBinding("Division", DIVISION_ORDER, pattern.division, (d) => void actions.seq("seqDivision", d), (d) => DIVISIONS[d].replace("T", " tern.")),
    steppedBinding(drum ? "Sens du pad" : "Sens", [0, 1, 2, 3], L0?.dir ?? 0, (d) => void actions.seq("seqLaneDir", lane, d), (d) => DIRECTIONS[d]),
    drum ? steppedBinding("Longueur du pad", LANE_LENGTHS.filter((n) => n <= L.maxSteps), LANE_LENGTHS.includes(L0?.length ?? 0) ? L0?.length ?? 0 : 0,
      (n) => void actions.seq("seqLaneLength", lane, n), (n) => (n === 0 ? "Pattern" : `${n} pas`)) : accentBinding,
    drum ? accentBinding
      : steppedBinding("Octaves vues", Array.from({ length: ROLL_MAX / 12 + 1 }, (_, i) => i * 12), Math.min(ROLL_MAX, Math.max(ROLL_MIN, Math.round(ui.rollBase / 12) * 12)), (b) => setRollBase(b), (b) => `${noteName(b)} – ${noteName(b + rollRows(ui) - 1)}`),
  ];

  const flamBinding = (cur: number, set: (ms: number) => void) => steppedBinding("Flam", FLAM_CHOICES, FLAM_CHOICES.includes(cur) ? cur : 0, set, flamText);
  const stepEncoders: (EncoderBinding | null)[] = step === null || !L0 ? [] : [
    drum && !laneInst ? flamBinding(extra?.flam ?? 0, (ms) => void actions.extras("stepsFlam", JSON.stringify([[lane, step]]), ms)) : steppedBinding("Note", Array.from({ length: 128 }, (_, i) => i), L0.note[step], (n) => { void actions.seq("seqSetNote", lane, step, n); actions.noteOn(t, n, 0.7, false); window.setTimeout(() => actions.noteOff(t, n), 150); }, noteName),
    valueBinding("Vélocité", (L0.vel[step] - 1) / 126, `${L0.vel[step]}`, (v) => void actions.seq("seqSetStep", lane, step, Math.max(1, Math.round(1 + v * 126)))),
    steppedBinding("Durée", LEN_CHOICES, L0.len[step], (n) => void actions.seq("seqSetLength8", lane, step, n),
      (n) => (n % 8 === 0 ? `${n / 8} pas` : `${(n / 8).toFixed(2)} pas`)),
    steppedBinding("Décalage", Array.from({ length: 65 }, (_, i) => i - 32), L0.micro[step], (m) => void actions.seq("seqSetMicro", lane, step, m),
      (m) => (m === 0 ? "0" : `${m > 0 ? "+" : "−"}${Math.abs(m)}/64`)),
    steppedBinding("Condition", condChoices(condKey(extra?.cond ?? 0, extra?.a ?? 1, extra?.b ?? 2)), condKey(extra?.cond ?? 0, extra?.a ?? 1, extra?.b ?? 2), (k) => setCond(parseCond(k)), condText),
    steppedBinding("Probabilité", Array.from({ length: 21 }, (_, i) => i * 5), Math.round((extra?.prob ?? 100) / 5) * 5, (p) => setCond({ prob: p }), (p) => `${p} %`),
    steppedBinding("Répétitions", RATCHET_CHOICES, ratchetKey(extra?.rat ?? 1, extra?.rs ?? 0),
      (k) => { const [n, sl] = k.split(":").map(Number); void actions.extras("stepRatchet", lane, step, n, sl); }, ratchetText),
    { kind: "stepped", label: "Verrous", text: ui.stepEdit ? "En cours" : `${extra?.locks.length ?? 0} réglage(s)`, normalised: ui.stepEdit ? 1 : 0,
      onStep: (d) => setUi({ stepEdit: d > 0 }) },
  ];

  // Encodeurs d'une sélection de plusieurs pas : tous les pas à la fois
  // chaque réglage part des pas tels qu'ils sont au moment de l'écrire (file d'attente)
  const editSel = (fn: (c: CellData, all: CellData[]) => CellData) => void queued(async () => {
    const st = getState();
    if (!st.pattern) return;
    const cells = snapshot(st.pattern, st.extras, st.ui.selection);
    if (cells.length) await actions.editCells(payload({ clear: [], writes: cells.map((c) => fn(c, cells)) }));
  });
  const avgVel = selCells.length ? selCells.reduce((a, c) => a + c.vel, 0) / selCells.length : 100;
  const firstSel = selCells[0];
  const exOf = (c: CellData) => c.ex ?? { cond: 0, a: 1, b: 2, prob: 100, rat: 1, rs: 0, flam: 0, locks: [] };
  const selEncoders: (EncoderBinding | null)[] = selCells.length < 2 ? [] : [
    drum ? flamBinding(firstSel ? exOf(firstSel).flam ?? 0 : 0, (ms) => editSel((c) => ({ ...c, ex: { ...exOf(c), flam: ms } }))) : { kind: "stepped", label: "Transposer", text: `${selCells.length} notes`, normalised: 0.5,
      onStep: (d) => void moveCells("selection", d, 0, false, false) },
    valueBinding("Vélocité", (avgVel - 1) / 126, `moy. ${Math.round(avgVel)}`, (v) => {
      // la moyenne va à la valeur demandée, les écarts entre pas sont gardés
      const target = 1 + v * 126;
      editSel((c, all) => {
        const avg = all.reduce((a, x) => a + x.vel, 0) / all.length;
        return { ...c, vel: Math.max(1, Math.min(127, Math.round(c.vel + target - avg))) };
      });
    }),
    steppedBinding("Durée", LEN_CHOICES, firstSel?.len ?? 8, (n) => editSel((c) => ({ ...c, len: n })),
      (n) => (n % 8 === 0 ? `${n / 8} pas` : `${(n / 8).toFixed(2)} pas`)),
    steppedBinding("Décalage", Array.from({ length: 65 }, (_, i) => i - 32), firstSel?.micro ?? 0, (m) => editSel((c) => ({ ...c, micro: m })),
      (m) => (m === 0 ? "0" : `${m > 0 ? "+" : "−"}${Math.abs(m)}/64`)),
    steppedBinding("Condition", condChoices(firstSel ? condKey(exOf(firstSel).cond, exOf(firstSel).a, exOf(firstSel).b) : "0"), firstSel ? condKey(exOf(firstSel).cond, exOf(firstSel).a, exOf(firstSel).b) : "0",
      (k) => editSel((c) => ({ ...c, ex: { ...exOf(c), ...parseCond(k) } })), condText),
    steppedBinding("Probabilité", Array.from({ length: 21 }, (_, i) => i * 5), Math.round((firstSel ? exOf(firstSel).prob : 100) / 5) * 5,
      (pr) => editSel((c) => ({ ...c, ex: { ...exOf(c), prob: pr } })), (pr) => `${pr} %`),
    steppedBinding("Répétitions", RATCHET_CHOICES, firstSel ? ratchetKey(exOf(firstSel).rat ?? 1, exOf(firstSel).rs ?? 0) : "1:0",
      (k) => { const [n, sl] = k.split(":").map(Number); editSel((c) => ({ ...c, ex: { ...exOf(c), rat: n, rs: sl } })); }, ratchetText),
    { kind: "stepped", label: "Verrous", text: ui.stepEdit ? "En cours" : `${selCells.length} pas`, normalised: ui.stepEdit ? 1 : 0, onStep: (d) => setUi({ stepEdit: d > 0 }) },
  ];

  // Gestes sur la grille : clic, Ctrl+clic, rectangle, glisser (déplacer), Ctrl+glisser (copier)
  const clickCell = (row: number, st: number, hit: number | null, ctrl: boolean) => {
    const u = getState().ui;
    if (ctrl || u.selectMode) {
      if (hit === null) { setCursor({ row, step: st }); return; }
      const sel = u.selection.includes(hit) ? u.selection.filter((k) => k !== hit) : [...u.selection, hit];
      setUi({ selection: sel, step: null, stepEdit: false });
      return;
    }
    if (u.selection.length) setUi({ selection: [] });
    setCursor(null);
    if (!drum) { toggleNote(row, st); return; }
    const vel = getState().pattern?.lanes[row]?.vel[st] ?? 0;
    if (vel > 0 && !(u.lane === row && u.step === st)) { setUi({ lane: row, step: st, stepEdit: false }); return; }
    actions.toggleStep(row, st, octaNewNote(row));
  };
  const rectKeys = (r0: number, s0: number, r1: number, s1: number) => {
    const p = getState().pattern;
    const out: number[] = [];
    if (!p) return out;
    const [ra, rb] = [Math.min(r0, r1), Math.max(r0, r1)], [sa, sb] = [Math.min(s0, s1), Math.max(s0, s1)];
    p.lanes.forEach((ln, l) => {
      for (let st = sa; st <= sb && st < p.length; ++st) {
        if ((ln.vel[st] ?? 0) <= 0) continue;
        const row = drum ? l : ln.note[st];
        if (row >= ra && row <= rb) out.push(cellKey(l, st));
      }
    });
    return out;
  };
  // Longueur d'une note au glisser (rouleau) : on attrape la queue d'une note tenue, ou le bord droit
  // d'une note courte, et on tire vers la droite ou la gauche ; la durée suit, pas par pas.
  const startResize = (e: React.PointerEvent, l: number, s0: number) => {
    const id = e.pointerId;
    let moved = false;
    let last = getState().pattern?.lanes[l]?.len[s0] ?? 8;
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      const c = cellAt(ev.clientX, ev.clientY);
      if (!c) return;
      const len8 = Math.min(MAX_LEN8, Math.max(1, c.step - s0 + 1) * 8);
      if (len8 === last) return;
      if (!moved && octa && octaParam(`t${l + 1}_holdmode`) !== 1) showMessage(`Durée du pas : elle s'entend quand le Déclenchement de T${l + 1} est sur « Tant qu'appuyé » (page Ampli)`);
      moved = true; last = len8;
      void actions.seq("seqSetLength8", l, s0, len8);
    };
    const end = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", end);
      window.removeEventListener("pointercancel", end);
      if (!moved) setUi({ lane: l, step: s0, stepEdit: false, selection: [] });
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", end);
    window.addEventListener("pointercancel", end);
  };
  // appui sur le nom d'un pad : le choisir et sélectionner tous ses pas ; 2e appui : désélectionner
  // 1.0 (7 nov.) : toucher le nom d'une ligne (batterie, Pieuvre, FM Player) joue son son tant que le doigt
  // reste posé, comme le clavier du rouleau pour les synthés (la vue agrandie n'a pas de pads)
  const laneAudition = (r: number, on: boolean) => {
    const cur = auditionNote;
    if (cur) { actions.noteOff(cur.t, cur.n); auditionNote = null; }
    if (!on || !module) return;
    let n = r;
    if (laneInst) {
      const u = getState().ui, ln = getState().pattern?.lanes[r];
      if (u.lane !== r) setUi({ lane: r }); // la piste interne jouée suit la ligne
      const st = ln ? ln.vel.findIndex((v) => v > 0) : -1;
      n = st >= 0 && ln ? ln.note[st] : padNote(0, false, u.octave, 0, u.root);
    }
    actions.noteOn(t, n, 0.8, false);
    auditionNote = { t, n };
  };
  const chipPress = useRef<number | null>(null);
  const chipLearn = (p: number) => {
    const cancel = () => { if (chipPress.current !== null) { window.clearTimeout(chipPress.current); chipPress.current = null; } };
    return {
      onPointerDown: (e: React.PointerEvent) => { if (e.pointerType === "mouse") return; cancel(); chipPress.current = window.setTimeout(() => { chipPress.current = null; void learnPattern(p); }, 600); },
      onPointerUp: cancel, onPointerLeave: cancel,
      onContextMenu: (e: React.MouseEvent) => { e.preventDefault(); cancel(); void learnPattern(p); },
    };
  };
  const selectLane = (r: number) => {
    const u = getState().ui;
    const p = getState().pattern;
    const keys: number[] = [];
    if (p) for (let st = 0; st < p.length; ++st) if ((p.lanes[r]?.vel[st] ?? 0) > 0) keys.push(cellKey(r, st));
    const same = u.lane === r && keys.length > 0 && u.selection.length === keys.length && keys.every((k) => u.selection.includes(k));
    setUi({ lane: r, step: null, stepEdit: false, selection: same ? [] : keys });
    setCursor(null);
  };
  // Pinceau : glisser pose les pas sur toutes les cases survolées (ou les efface si le geste
  // commence sur un pas posé). Une seule opération par case et par geste.
  const startPaint = (e: React.PointerEvent, row: number, st: number) => {
    const id = e.pointerId;
    const p0 = getState().pattern;
    if (!p0) return;
    const erase = hitAt(drum, row, st) !== null;
    const done = new Set<string>();
    const taken = new Map<number, Set<number>>(); // mélodique : lignes prises pendant le geste, par pas
    const vel = getState().ui.shift ? 127 : 100;
    const apply = (r: number, s: number) => {
      const key = `${r}:${s}`;
      const p = getState().pattern;
      if (done.has(key) || !p || s < 0 || s >= p.length) return;
      done.add(key);
      if (drum) {
        const on = (p.lanes[r]?.vel[s] ?? 0) > 0;
        if (erase && on) void actions.seq("seqSetStep", r, s, 0);
        else if (!erase && !on) {
          const n = octaNewNote(r);
          if (n !== undefined) void actions.seq("seqSetNote", r, s, n).then(() => actions.seq("seqSetStep", r, s, vel));
          else void actions.seq("seqSetStep", r, s, vel);
        }
        return;
      }
      const l = p.lanes.findIndex((ln) => ln.vel[s] > 0 && ln.note[s] === r);
      if (erase) { if (l >= 0) void actions.seq("seqSetStep", l, s, 0); return; }
      if (l >= 0) return;
      const used = taken.get(s) ?? new Set<number>();
      const f = p.lanes.findIndex((ln, k) => ln.vel[s] === 0 && !used.has(k));
      if (f < 0) return;
      used.add(f); taken.set(s, used);
      void actions.seq("seqSetNote", f, s, r).then(() => actions.seq("seqSetStep", f, s, vel));
    };
    apply(row, st);
    if (!drum && !erase) { actions.noteOn(t, row, 0.75, false); window.setTimeout(() => actions.noteOff(t, row), 150); }
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      const c = cellAt(ev.clientX, ev.clientY);
      if (c) apply(c.row, c.step);
    };
    const end = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", end);
      window.removeEventListener("pointercancel", end);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", end);
    window.addEventListener("pointercancel", end);
  };
  const onCellDown = (e: React.PointerEvent, row: number, st: number) => {
    if (e.button !== 0) return;
    if (getState().ui.paint && !(e.ctrlKey || e.metaKey)) { e.preventDefault(); startPaint(e, row, st); return; }
    // v0.47 (maj. 5 nov.) : Pieuvre 8 — les pas d'une ligne T s'allongent comme des notes : tirer le bord droit
    // d'un pas (ou la queue d'un pas long) vers la droite ou la gauche
    if (laneInst && drum && !(e.ctrlKey || e.metaKey || e.shiftKey) && !getState().ui.selectMode) {
      const p = getState().pattern;
      const ln = p?.lanes[row];
      if (p && ln) {
        if ((ln.vel[st] ?? 0) > 0) {
          const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
          if (e.clientX > r.right - r.width * 0.3) { e.preventDefault(); startResize(e, row, st); return; }
        } else {
          const o = laneHeldOrigin(ln, st);
          if (o >= 0) { e.preventDefault(); startResize(e, row, o); return; }
        }
      }
    }
    if (!drum && !(e.ctrlKey || e.metaKey || e.shiftKey) && !getState().ui.selectMode) {
      const p = getState().pattern;
      const l = p ? p.lanes.findIndex((ln) => ln.vel[st] > 0 && ln.note[st] === row) : -1;
      if (p && l >= 0) {
        // note courte (un pas ou moins) : son bord droit sert de poignée
        const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
        if ((p.lanes[l].len[st] ?? 8) <= 8 && e.clientX > r.right - r.width * 0.3) { e.preventDefault(); startResize(e, l, st); return; }
      } else if (p) {
        const o = heldOrigin(p, row, st);
        if (o) { e.preventDefault(); startResize(e, o.lane, o.step); return; }
      }
    }
    const hit = hitAt(drum, row, st);
    gesture.current = { id: e.pointerId, row0: row, step0: st, row, step: st, hit, ctrl: e.ctrlKey || e.metaKey, shift: e.shiftKey, moved: false, keys: [] };
    const copyMod = (ev: PointerEvent) => ev.ctrlKey || ev.metaKey || ev.altKey || getState().ui.copyDrag;
    const move = (ev: PointerEvent) => {
      const g = gesture.current;
      if (!g || ev.pointerId !== g.id) return;
      const c = cellAt(ev.clientX, ev.clientY);
      if (!c || (c.row === g.row && c.step === g.step)) return;
      g.row = c.row; g.step = c.step;
      if (!g.moved) {
        g.moved = true;
        if (g.hit !== null) {
          const sel = getState().ui.selection;
          g.keys = sel.includes(g.hit) ? sel : [g.hit];
          if (!sel.includes(g.hit)) setUi({ selection: [g.hit], step: null, stepEdit: false });
        }
      }
      if (g.hit !== null) setDrag({ kind: "move", keys: g.keys, dRow: g.row - g.row0, dStep: g.step - g.step0, copy: copyMod(ev) });
      else setDrag({ kind: "rect", r0: g.row0, s0: g.step0, r1: g.row, s1: g.step });
    };
    const finish = (ev: PointerEvent, cancelled: boolean) => {
      if (gesture.current && ev.pointerId !== gesture.current.id) return; // un autre doigt : on l'ignore
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", up);
      window.removeEventListener("pointercancel", cancel);
      const g = gesture.current;
      gesture.current = null;
      setDrag(null);
      if (!g || cancelled) return;
      if (!g.moved) { clickCell(g.row0, g.step0, g.hit, g.ctrl); return; }
      if (g.hit !== null) { void moveCells(g.keys, g.row - g.row0, g.step - g.step0, copyMod(ev), drum); return; }
      const keys = rectKeys(g.row0, g.step0, g.row, g.step);
      setUi({ selection: g.shift || g.ctrl ? [...new Set([...getState().ui.selection, ...keys])] : keys, step: null, stepEdit: false });
      if (keys.length) setCursor(null);
    };
    const up = (ev: PointerEvent) => finish(ev, false);
    const cancel = (ev: PointerEvent) => finish(ev, true);
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", up);
    window.addEventListener("pointercancel", cancel);
  };
  const cellProps = (row: number, st: number) => ({
    "data-row": row, "data-step": st,
    onPointerDown: (e: React.PointerEvent) => onCellDown(e, row, st),
    onPointerEnter: (e: React.PointerEvent) => { if (e.pointerType === "mouse") hover.current = { row, step: st }; },
  });
  const cancelGesture = () => { gesture.current = null; setDrag(null); };
  const selClasses = (classes: string[], k: number | null, row: number, st: number) => {
    if (k !== null && selSet.has(k)) classes.push("cell--sel");
    if (ghostSet.has(`${row}:${st}`)) classes.push(drag && drag.kind === "move" && drag.copy ? "cell--ghost cell--ghost-copy" : "cell--ghost");
    if (k !== null && inMarquee(row, st)) classes.push("cell--marquee");
    // v0.43.8 : les cases prises dans le rectangle sont marquées (cadre posé par-dessus, transparent)
    if (drag && drag.kind === "rect" && inMarquee(row, st)) classes.push("cell--inrect");
    if (cursor && cursor.row === row && cursor.step === st) classes.push("cell--cursor");
  };

  const encoders = selCells.length > 1 ? selEncoders : stepOn ? stepEncoders : trackEncoders;
  const rowName = (r: number) => module?.padNames[r] ?? `Pad ${r + 1}`;

  return (
    <ScreenFrame title="Séquence" encoders={encoders}
      tabs={[{ id: "notes", label: "Notes" }, { id: "auto", label: "Courbes" }]} tab={ui.seqView}
      onTab={(id) => setUi({ seqView: id as "notes" | "auto" })}
      status={<>
        {/* v0.23 : la piste se change ici même, sans repasser par l'écran Pistes */}
        <span className="seq-tracknav" style={{ borderColor: trackColor(track?.color ?? 0) }} role="group" aria-label="Piste du séquenceur">
          <button type="button" onClick={() => void actions.focus((t + tracks.length - 1) % tracks.length)} aria-label="Piste précédente" title="Piste précédente">◀</button>
          <select value={t} onChange={(e) => void actions.focus(Number(e.target.value))} aria-label="Piste du séquenceur" title="Changer de piste">
            {tracks.map((tr, i) => <option key={i} value={i}>P{i + 1} · {tr.instrument ? tr.name : "Libre"}</option>)}
          </select>
          <button type="button" onClick={() => void actions.focus((t + 1) % tracks.length)} aria-label="Piste suivante" title="Piste suivante">▶</button>
        </span>
        <button type="button" className={`chip chip--button${patOpen ? " on" : ""}`} aria-expanded={patOpen} onClick={() => setPatOpen(!patOpen)}
          title="Mesures, division (binaire ou ternaire) et vitesse du pattern">
          Pattern {PATTERN_NAMES[pattern.pattern] ?? "A"} · {bars % 1 === 0 ? `${bars} mes.` : `${pattern.length} pas`} · {DIVISIONS[pattern.division]} · {SPEEDS[pattern.speed]} ▾
        </button>
        {/* v0.46 : flèches et numéro de page (les pastilles ne tenaient plus dans le bandeau au-delà de 4 pages) */}
        {pages > 1 && (
          <span className="page-nav" role="group" aria-label="Pages du pattern">
            <button type="button" disabled={page <= 0} onClick={() => setUi({ page: Math.max(0, page - 1) })} aria-label="Page précédente" title="Page précédente">◀</button>
            <b className="tabular">{page + 1}/{pages}</b>
            <button type="button" disabled={page >= pages - 1} onClick={() => setUi({ page: Math.min(pages - 1, page + 1) })} aria-label="Page suivante" title="Page suivante">▶</button>
          </span>
        )}
        {pages <= 4 && <span className="page-dots" aria-label={`Page ${page + 1} sur ${pages}`}>
          {Array.from({ length: Math.min(pages, 32) }, (_, i) => (
            <button key={i} type="button" className={`${i === page ? "on" : ""}${i % (spb / PAGE || 1) === 0 ? " bar" : ""}`} onClick={() => setUi({ page: i })} aria-label={`Page ${i + 1}`} />
          ))}
        </span>}
      </>}>
      {patOpen && (
        <div ref={patRef} className="pat-pop" role="dialog" aria-label="Réglages du pattern">
          <div className="pat-row">
            <span className="detail-label">Mesures</span>
            {BAR_CHOICES.filter((b) => b * spb <= L.maxSteps).map((b) => (
              <button key={b} type="button" className={bars === b ? "on" : ""} onClick={() => { void actions.seq("seqTrackLength", b * spb); setUi({ page: 0 }); }}>{b}</button>
            ))}
          </div>
          <div className="pat-row">
            <span className="detail-label">Binaire</span>
            {DIVISION_ORDER.filter((d) => !DIVISIONS[d].includes("T")).map((d) => (
              <button key={d} type="button" className={pattern.division === d ? "on" : ""} onClick={() => void actions.seq("seqDivision", d)}>{DIVISIONS[d]}</button>
            ))}
            <span className="detail-label">Ternaire</span>
            {DIVISION_ORDER.filter((d) => DIVISIONS[d].includes("T")).map((d) => (
              <button key={d} type="button" className={pattern.division === d ? "on" : ""} onClick={() => void actions.seq("seqDivision", d)}>{DIVISIONS[d].replace("T", "")} tern.</button>
            ))}
          </div>
          <div className="pat-row">
            <span className="detail-label">Vitesse</span>
            {SPEEDS.map((sp, i) => (
              <button key={sp} type="button" className={pattern.speed === i ? "on" : ""} onClick={() => void actions.seq("seqTrackSpeed", i)}>{sp}</button>
            ))}
            <span className="screen-spacer" />
            <button type="button" className="touch touch--small" onClick={() => setPatOpen(false)}>Fermer</button>
          </div>
          <p className="sound-hint">Ces réglages valent pour ce pattern de cette piste seulement. Ils sont aussi sur les encodeurs quand aucun pas n'est ouvert.</p>
        </div>
      )}
      {drag && drag.kind === "rect" && <MarqueeBox />}
      {ui.seqView === "auto" ? <PatternCurves t={t} spb={spb} /> : (
      <div className={`seq${drum ? "" : " seq--melodic"}`} style={drum ? undefined : { "--k": trackColor(track?.color ?? 0) } as React.CSSProperties}>
        <div className={`seq-head${drum && !showLaneRoll ? "" : " seq-head--roll"}`}>
          {drum && !showLaneRoll ? (laneInst
            ? <button type="button" className="seq-head-name seq-head-roll" onClick={() => { setLaneRoll(true); if (ui.rollBase > 60 || ui.rollBase + rollRows(ui) - 1 < 60) setRollBase(48); }} aria-label={`T${Math.min(lane, 7) + 1} en notes (clavier chromatique)`}
                title="Déplier la piste T choisie en clavier chromatique : chaque pas reçoit sa note (Clavier « Chromatique » de la piste)">T{Math.min(lane, 7) + 1} en notes ♪</button>
            : <span className="seq-head-name">Pad</span>) : (
          <span className="seq-head-name seq-octave" title="Octave affichée (accordée avec l'octave des pads ; molette sur le clavier, ou Maj + Page / octave)">
            <button type="button" onClick={() => setRollBase(ui.rollBase - 12)} disabled={ui.rollBase <= ROLL_MIN} aria-label="Octave plus bas">▼</button>
            <button type="button" onClick={() => setRollBase(ui.rollBase + 12)} disabled={ui.rollBase >= ROLL_MAX} aria-label="Octave plus haut">▲</button>
            <button type="button" className={`seq-oct-one${ui.rollOne ? " on" : ""}`} onClick={toggleRollOne} aria-pressed={!!ui.rollOne}
              title="Afficher une seule octave, cases deux fois plus hautes (toucher à nouveau : deux octaves)">{ui.rollOne ? "1 oct." : "2 oct."}</button>
          </span>)}
          <div className="seq-ruler">
            {Array.from({ length: PAGE }, (_, i) => {
              const s = first + i;
              const isBeat = s % stepsPerBeat === 0, isBar = s % spb === 0;
              return <span key={i} className={`ruler${isBeat ? " ruler--beat" : ""}${s >= pattern.length ? " ruler--out" : ""}`}>{isBar ? `${s / spb + 1}` : isBeat ? "·" : ""}</span>;
            })}
          </div>
        </div>

        <FlagRows drum={drum} roll={showLaneRoll} first={first} length={pattern.length} flags={pattern.flags} stepsPerBeat={stepsPerBeat}
          playStep={showsPlaying ? (pos[0] ?? -1) : -1} />

        {showLaneRoll ? (
          <OctaLaneRoll t={t} T={Math.min(lane, 7)} first={first} stepsPerBeat={stepsPerBeat} playStep={showsPlaying ? (pos[Math.min(lane, 7)] ?? -1) : -1}
            lengthHint={(T) => { if (octa && octaParam(`t${T + 1}_holdmode`) !== 1) showMessage(`Durée de la note : elle s'entend quand le Déclenchement de T${T + 1} est sur « Tant qu'appuyé » (page Ampli)`); }} />
        ) : drum ? (
        <div className="seq-body" style={{ "--rows": Math.max(1, rows.length) } as React.CSSProperties} onPointerLeave={() => { hover.current = null; }}>
          {rows.map((r) => {
            const ln = pattern.lanes[r];
            if (!ln) return null;
            const position = pos[r] ?? -1;
            const kind = module?.padKinds[r] ?? "sample";
            // v0.43 : Pieuvre 8 → chaque ligne T prend la couleur de sa piste interne (sinon cases éteintes, sombres)
            const kStyle = kind === "octa" || kind === "fm1" ? { "--k": trackColor(r) } as React.CSSProperties : undefined;
            return (
              <div key={r} className={`seq-row${r === lane ? " seq-row--selected" : ""}${ln.mute ? " seq-row--muted" : ""}`}>
                <button type="button" className={`seq-name kind--${kind}${(lit[r] ?? 0) > 0 ? " is-lit" : ""}`} style={kStyle} onClick={() => selectLane(r)}
                  onPointerDown={(e) => { if (e.button === 0) laneAudition(r, true); }} onPointerUp={() => laneAudition(r, false)}
                  onPointerCancel={() => laneAudition(r, false)} onPointerLeave={() => laneAudition(r, false)}
                  title="Choisir ce pad et sélectionner tous ses pas (puis ← → ou ◀ ▶ pour les décaler en boucle ; un 2e appui désélectionne)">
                  <span className="seq-swatch" aria-hidden />
                  <span className="seq-num">{r + 1}</span>
                  <span className="seq-label">{rowName(r)}</span>
                  {ln.length > 0 && <span className="seq-lanelen" title={`Ce pad boucle sur ${ln.length} pas`}>{ln.length}</span>}
                </button>
                <button type="button" className={`seq-ms${ln.mute ? " on" : ""}`} onClick={() => void actions.seq("seqLaneMute", r, !ln.mute)} aria-label={`Couper ${rowName(r)}`}>M</button>
                <button type="button" className={`seq-ms seq-ms--solo${ln.solo ? " on" : ""}`} onClick={() => void actions.seq("seqLaneSolo", r, !ln.solo)} aria-label={`${rowName(r)} en solo`}>S</button>
                <div className={`seq-cells kind--${kind}`} style={kStyle}>
                  {Array.from({ length: PAGE }, (_, i) => {
                    const s = first + i;
                    const vel = ln.vel[s] ?? 0;
                    const inPattern = s < pattern.length;
                    const ex = extras.find((e) => e.lane === r && e.step === s);
                    const classes = ["cell"];
                    if (ln.length > 0 && s >= ln.length) classes.push("cell--lane-out");
                    if (laneInst && vel === 0 && laneHeldOrigin(ln, s) >= 0) classes.push("cell--held");
                    if (laneInst && vel > 0) classes.push("cell--grip");
                    if (vel > 0 && (ex?.flam ?? 0) > 0) classes.push("cell--flam");
                    if (vel > 0 && ((pattern.flags[s] ?? 0) & STEP_ACCENT)) classes.push("cell--acc");
                    if (vel > 0) classes.push("cell--on");
                    if (vel >= 120) classes.push("cell--accent");
                    if (s % stepsPerBeat === 0) classes.push("cell--beat");
                    if (!inPattern) classes.push("cell--out");
                    if (showsPlaying && position === s) classes.push("cell--play");
                    if (r === lane && step === s) classes.push("cell--selected");
                    if (ex && (ex.cond > 0 || ex.prob < 100)) classes.push("cell--cond");
                    if (ex && ex.locks.length > 0) classes.push("cell--lock");
                    const rat = vel > 0 && ex && (ex.rat ?? 1) > 1 ? ex.rat ?? 1 : 0;
                    if (rat) classes.push("cell--rat");
                    selClasses(classes, vel > 0 ? cellKey(r, s) : null, r, s);
                    return (
                      <button key={i} type="button" className={classes.join(" ")}
                        style={vel > 0 ? { "--vel": `${0.35 + (vel / 127) * 0.65}`, ...(rat ? { "--rat": rat } : {}) } as React.CSSProperties : undefined}
                        disabled={!inPattern}
                        aria-label={`${rowName(r)}, pas ${s + 1}${vel > 0 ? `, vélocité ${vel}` : ""}${rat ? `, ${rat} répétitions` : ""}`}
                        aria-pressed={vel > 0}
                        {...cellProps(r, s)}
                        onContextMenu={(e) => { e.preventDefault(); cancelGesture(); if (vel > 0) setUi({ lane: r, step: s, stepEdit: true, selection: [] }); }} />
                    );
                  })}
                </div>
              </div>
            );
          })}
        </div>
        ) : (
        <div className={`roll${ui.rollOne ? " roll--one" : ""}`} role="grid" aria-label="Notes de la séquence" onPointerLeave={() => { hover.current = null; }}
          onWheel={(e) => { if (!(e.target as HTMLElement).closest(".roll-key")) return; setRollBase(getState().ui.rollBase + (e.deltaY < 0 ? 12 : -12)); }}>
          {Array.from({ length: rollRows(ui) }, (_, k) => {
            const n = ui.rollBase + rollRows(ui) - 1 - k;
            const black = [1, 3, 6, 8, 10].includes(n % 12);
            const outOfScale = ui.scale > 0 && !inScale(n, ui.scale, ui.root);
            return (
              <div key={n} className={`roll-row${scaleRowClass(n, black, outOfScale, ui.scale)}${n % 12 === 0 ? " roll-row--c" : ""}${ui.scale > 0 && ((n - ui.root) % 12 + 12) % 12 === 0 ? " roll-row--root" : ""}`}>
                <button type="button" className="roll-key" {...rollKeyHandlers(t, n)} title="Toucher : jouer · glisser vers le haut ou le bas : changer d'octave">{noteName(n)}</button>
                <div className="roll-cells">
                  {Array.from({ length: PAGE }, (_, i) => {
                    const s = first + i;
                    const l = laneWith(n, s);
                    const on = l >= 0;
                    const ln = on ? pattern.lanes[l] : null;
                    const ex = on ? extras.find((e) => e.lane === l && e.step === s) : undefined;
                    const classes = ["rcell"];
                    if (on) classes.push("rcell--on");
                    if (on && ln && ln.len[s] <= 8 && s < pattern.length) classes.push("rcell--short");
                    if (!on && heldBy(n, s)) classes.push("rcell--held");
                    if (on && ((pattern.flags[s] ?? 0) & STEP_SLIDE)) classes.push("rcell--slide");
                    if (on && ((pattern.flags[s] ?? 0) & STEP_ACCENT)) classes.push("cell--acc");
                    if (s % stepsPerBeat === 0) classes.push("rcell--beat");
                    if (s >= pattern.length) classes.push("rcell--out");
                    if (showsPlaying && (pos[0] ?? -1) === s) classes.push("rcell--play");
                    if (on && l === lane && step === s) classes.push("rcell--selected");
                    if (ex && (ex.cond > 0 || ex.prob < 100)) classes.push("cell--cond");
                    if (ex && ex.locks.length > 0) classes.push("cell--lock");
                    const rat = on && ex && (ex.rat ?? 1) > 1 ? ex.rat ?? 1 : 0;
                    if (rat) classes.push("cell--rat");
                    selClasses(classes, on ? cellKey(l, s) : null, n, s);
                    return (
                      <button key={i} type="button" className={classes.join(" ")} disabled={s >= pattern.length}
                        style={on && ln ? { "--vel": `${0.4 + (ln.vel[s] / 127) * 0.6}`, "--len": `${Math.min(PAGE - i, ln.len[s] / 8)}`, ...(rat ? { "--rat": rat } : {}) } as React.CSSProperties : undefined}
                        aria-label={`${noteName(n)}, pas ${s + 1}${on ? " (note posée)" : ""}`} aria-pressed={on}
                        {...cellProps(n, s)}
                        onContextMenu={(e) => { e.preventDefault(); cancelGesture(); if (on) setUi({ lane: l, step: s, stepEdit: true, selection: [] }); }} />
                    );
                  })}
                </div>
              </div>
            );
          })}
        </div>
        )}

        {selection.length > 0 ? (
        <div className="seq-tools seq-tools--sel" role="toolbar" aria-label="Sélection">
          <span className="chip chip--sel">{selection.length} pas sélectionné{selection.length > 1 ? "s" : ""}</span>
          <button type="button" className="touch touch--small touch--quiet" onClick={(e) => void moveCells("selection", 0, e.shiftKey ? -spb : -1, false, drum)}
            title="← : décale d'un pas vers la gauche, en boucle (Maj+← : d'une mesure)" aria-label="Décaler d'un pas vers la gauche">◀</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={(e) => void moveCells("selection", 0, e.shiftKey ? spb : 1, false, drum)}
            title="→ : décale d'un pas vers la droite, en boucle (Maj+→ : d'une mesure)" aria-label="Décaler d'un pas vers la droite">▶</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => copySelection(t, drum, false)} title="Ctrl+C">Copier</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => copySelection(t, drum, true)} title="Ctrl+X">Couper</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => void pasteAt(t, drum, cursor)} title="Ctrl+V : colle sous la souris (ou à la case marquée)">Coller</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => void duplicateSelection(drum)} title="Ctrl+D : recopie la sélection juste après elle">Dupliquer</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => void deleteSelection()} title="Suppr">Effacer</button>
          <button type="button" className={`touch touch--small${ui.copyDrag ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ copyDrag: !ui.copyDrag })}
            title="Glisser la sélection la copie au lieu de la déplacer (comme Ctrl)">Copie au glisser</button>
          {selection.length >= 1 && <button type="button" className={`touch touch--small${ui.stepEdit ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ stepEdit: !ui.stepEdit })}>
            {ui.stepEdit ? "Verrous en cours" : "Verrous"}</button>}
          <button type="button" className="touch touch--small touch--quiet" onClick={() => setUi({ selection: allKeys(pattern), step: null, stepEdit: false })} title="Ctrl+A">Tout</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={() => setUi({ selection: [], stepEdit: false })} title="Échap">Désélectionner</button>
        </div>
        ) : (
        <div className="seq-tools">
          {showLaneRoll ? <>
          {/* v0.45 : même barre que partout (Sélection, Pinceau) ; la T se choisit dans la liste */}
          <button type="button" className={`touch${ui.selectMode ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ selectMode: !ui.selectMode, paint: false })}
            title="Toucher sélectionne au lieu de poser (au clavier : Ctrl+clic, ou Ctrl + tirer un rectangle)">Sélection</button>
          <button type="button" className={`touch${ui.paint ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ paint: !ui.paint, selectMode: false })}
            title="Pinceau : glisser pose une note sur chaque pas survolé (touche P)">Pinceau</button>
          <select className="seq-lanesel" aria-label="Piste T dépliée en notes" value={Math.min(lane, 7)}
            onChange={(e) => { const v = Number(e.target.value); if (v < 0) setLaneRoll(false); else setUi({ lane: v, step: null, stepEdit: false, selection: [] }); }}
            title="Piste T dépliée en notes (ou retour aux 8 lignes)">
            {Array.from({ length: 8 }, (_, k) => <option key={k} value={k}>T{k + 1} ♪</option>)}
            <option value={-1}>8 lignes</option>
          </select>
          </> : <>
          <button type="button" className={`touch${ui.selectMode ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ selectMode: !ui.selectMode, paint: false })}
            title="Toucher sélectionne au lieu de poser (au clavier : Ctrl+clic, ou tirer un rectangle depuis une case vide)">Sélection</button>
          <button type="button" className={`touch${ui.paint ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ paint: !ui.paint, selectMode: false })}
            title="Pinceau : glisser pose un pas sur chaque case survolée ; commencer sur un pas posé efface au lieu de poser (touche P)">Pinceau</button>
          {/* v0.43.6 : toutes les notes du pattern, pour les déplacer ensemble (◀ ▶ ou en les tirant) */}
          <button type="button" className="touch touch--quiet" onClick={() => { const keys = allKeys(pattern); setUi({ selection: keys, step: null, stepEdit: false }); showMessage(keys.length ? `${keys.length} pas sélectionnés : ◀ ▶ pour les décaler, ou tirez-en un pour tout déplacer` : "Le pattern est vide"); }}
            title="Sélectionner toutes les notes du pattern (Ctrl+A)">Tout sélect.</button>
          </>}
          {getClipboard() && <button type="button" className="touch touch--quiet" onClick={() => void pasteAt(t, drum, cursor)} title="Ctrl+V">Coller</button>}
          <span className="pattern-chips" role="tablist" aria-label="Pattern édité">
            {/* v0.46 fix29 : 16 patterns (A à P) en deux pages de 8 ; la page suit le pattern édité */}
            <PatternPageChip page={chipPage} other={PATTERN_NAMES.some((_, p) => Math.floor(p / ROW_PAGE) !== chipPage && ((live ? live[0] : pattern.playing) === p || live?.[1] === p))}
              onTurn={() => setChipPage((chipPage + 1) % ROW_PAGES)} />
            {PATTERN_NAMES.map((name, p) => Math.floor(p / ROW_PAGE) !== chipPage ? null : (() => {
              const isPlay = (live ? live[0] : pattern.playing) === p, isQueued = live?.[1] === p;
              return (
                <button key={name} type="button" role="tab" aria-selected={p === pattern.pattern}
                  className={`pchip${p === pattern.pattern ? " on" : ""}${filled[p] ? " filled" : ""}${isPlay ? " playing" : ""}${isQueued ? " queued" : ""}`}
                  onClick={() => void actions.editPattern(t, p)}
                  {...chipLearn(p)}
                  title={`Éditer le pattern ${name} — appui long ou clic droit : affecter au contrôleur MIDI (la commande lance ${name} sur la piste choisie)`}>{name}</button>
              );
            })())}
          </span>
          {(live ? live[0] : pattern.playing) !== pattern.pattern && (
            <button type="button" className="touch touch--small" onClick={() => void actions.launchPattern(t, pattern.pattern)}>▶ Lancer {PATTERN_NAMES[pattern.pattern]}</button>
          )}
          <button type="button" className="touch touch--quiet" onClick={async () => {
            // duplique les mesures : la séquence double de longueur (blocs de 4, 8, 16, 32 mesures)
            const n = pattern.length;
            if (n * 2 > L.maxSteps) return;
            await actions.seq("seqTrackLength", n * 2);
            for (let k = 0; k < n; k += 64) await actions.seq("seqCopyPage", k, n + k, Math.min(64, n - k));
          }}>Dupliquer ×2</button>
          <button type="button" className="touch touch--quiet" onClick={() => void actions.seq("seqClearLane", lane)}>Effacer ligne</button>
          <button type="button" className="touch touch--quiet" onClick={() => void actions.clearPattern(t, pattern.pattern)}>Effacer pattern</button>
          {/* v0.46 fix16 : décaler toute l'écriture d'un pas (Maj : seulement la ligne choisie), en boucle */}
          <button type="button" className="touch touch--quiet seq-shift" onClick={(e) => void actions.shiftPattern(t, -1, e.shiftKey ? lane : -1)}
            aria-label="Décaler à gauche" title="Décaler tout le pattern d'un pas vers la gauche : le premier pas revient à la fin ; accents, conditions et verrous suivent (Maj : seulement la ligne choisie)">◀</button>
          <button type="button" className="touch touch--quiet seq-shift" onClick={(e) => void actions.shiftPattern(t, 1, e.shiftKey ? lane : -1)}
            aria-label="Décaler à droite" title="Décaler tout le pattern d'un pas vers la droite : le dernier pas revient au début ; accents, conditions et verrous suivent (Maj : seulement la ligne choisie)">▶</button>
          {stepOn && <button type="button" className={`touch${ui.stepEdit ? " touch--on" : " touch--quiet"}`} onClick={() => setUi({ stepEdit: !ui.stepEdit })}>
            {ui.stepEdit ? "Verrous : tournez les encodeurs (SON, EFFETS)" : "Verrouiller des réglages sur ce pas"}</button>}
          {stepOn && extra && <button type="button" className="touch touch--quiet" onClick={() => void actions.extras("stepClear", lane, step!)}>Retirer condition et verrous</button>}
          {/* v0.43.4 : verrous d'un pas copiés sur un autre pas de la même piste */}
          {stepOn && extra && extra.locks.length > 0 && <button type="button" className="touch touch--quiet" onClick={() => actions.copyStepLocks(lane, step!)}
            title="Copier les verrous de ce pas, pour les coller sur un autre pas de cette piste">Copier les verrous</button>}
          {stepOn && getLockClip()?.track === t && <button type="button" className="touch touch--quiet" onClick={() => void actions.pasteStepLocks(lane, step!)}
            title="Coller ici les verrous copiés (ceux des mêmes réglages sont remplacés)">Coller les verrous</button>}
          {stepOn && <button type="button" className="touch touch--quiet" onClick={() => setUi({ step: null, stepEdit: false })}>Fermer le pas</button>}
        </div>
        )}
      </div>)}
    </ScreenFrame>
  );
}

/** Vue « Courbes » : les mouvements enregistrés avec ENR. dans ce pattern, à lire et redessiner. */
function PatternCurves({ t, spb }: { t: number; spb: number }) {
  const data = useStore((s) => s.patLanes);
  const editing = useStore((s) => s.pattern?.pattern ?? 0);
  const lastParam = useStore((s) => s.lastParam);
  const phase = useStore((s) => s.tick.ph ?? -1);
  const playingPat = useStore((s) => s.tick.pat[t]?.[0] ?? -1);
  const [sel, setSel] = useState<number | null>(null);
  useEffect(() => { void actions.refreshPatLanes(); }, [t, editing]);
  const lanes = data && data.track === t ? data.lanes : [];
  const len = data?.len ?? 16;
  const cur = lanes.find((l) => l[0] === sel) ?? lanes[0];
  const curG = cur?.[0] ?? null;
  const lastOnTrack = lastParam >= 0 && Math.floor(lastParam / slots()) === t && !lanes.some((l) => l[0] === lastParam);
  const addLast = () => {
    const v = baseValue(lastParam);
    void actions.setPatternLane(lastParam, [0, v, Math.max(0, len - 0.001), v]);
    setSel(lastParam);
  };
  const fmt = (g: number) => (v: number) => { const r = resolveGlobal(g); return r ? formatNorm(r.desc, v) : `${Math.round(v * 100)} %`; };
  return (
    <div className="pcurves">
      <div className="pcurves-list">
        {lanes.map(([g, on]) => (
          <span key={g} className={`pcurve-chip${g === curG ? " is-sel" : ""}${on ? "" : " is-off"}`}>
            <button type="button" className="pcurve-on" onClick={() => { const l = lanes.find((x) => x[0] === g); if (l) void actions.setPatternLane(g, l[2], !l[1]); }}
              aria-label={on ? "Ne plus relire la courbe" : "Relire la courbe"} title={on ? "Relue (toucher : ne plus relire)" : "Pas relue (toucher : relire)"}>{on ? "●" : "○"}</button>
            <button type="button" className="pcurve-name" onClick={() => setSel(g)}>{describeGlobal(g)}</button>
            <button type="button" className="x" onClick={() => void actions.setPatternLane(g, [])} aria-label={`Effacer la courbe ${describeGlobal(g)}`}>×</button>
          </span>
        ))}
        <button type="button" className="touch touch--small" disabled={!lastOnTrack} onClick={addLast}
          title="Bougez d'abord un réglage de cette piste (écran SON, façade…), puis touchez ici pour lui créer une courbe à dessiner">
          + Courbe{lastOnTrack ? ` : ${describeGlobal(lastParam)}` : " (bougez un réglage de la piste)"}
        </button>
      </div>
      {curG !== null && cur ? (
        <CurveEditor key={`${t}:${editing}:${curG}`} points={cur[2]} span={len} grid={spb / 4} bar={spb} res={0.125} quarter={spb / 4}
          playX={playingPat === editing ? phase : -1} format={fmt(curG)}
          unitLabel={(x) => `mes. ${Math.floor(x / spb) + 1} · pas ${Math.floor(x % spb) + 1}`}
          onCommit={(pts) => void actions.setPatternLane(curG, pts, !!cur[1])} />
      ) : (
        <div className="pcurves-empty">
          <p><b>Aucune courbe dans ce pattern.</b></p>
          <p>En lecture avec <b>ENR.</b> allumé, tournez un bouton de cette piste (à l'écran ou au contrôleur) : son mouvement s'écrit ici en courbe lisse, qui tourne avec le pattern.</p>
          <p>Ou bougez un réglage de la piste puis touchez « + Courbe » pour la dessiner à la main.</p>
        </div>
      )}
    </div>
  );
}

/** v0.46 fix27 : case d'une fonction de pas : toucher = poser / retirer ; appui long (clic droit) =
    sa valeur ; une valeur réglée se voit à la force de la couleur. */
function FlagCell({ disabled, on, amount, className, label, onToggle, onEdit }: {
  disabled: boolean; on: boolean; amount: number; className: string; label: string; onToggle: () => void; onEdit?: () => void;
}) {
  const lp = useLongPress(() => onEdit?.());
  return (
    <button type="button" disabled={disabled} aria-pressed={on} className={`${className}${amount > 0 ? " fcell--amt" : ""}`} aria-label={label}
      title={amount > 0 ? `${label} : ${amount} %` : label}
      style={amount > 0 ? { "--amt": amount / 100 } as React.CSSProperties : undefined}
      {...(onEdit ? lp.handlers : {})}
      onClick={() => { if (onEdit && lp.consumed()) return; onToggle(); }}>
      {amount > 0 && <span className="fcell-amt">{amount}</span>}
    </button>
  );
}

/** Lignes des fonctions de pas de la piste : ACCENT (toutes les pistes) et GLISSÉ (mélodique).
    Toucher une case pose ou retire la fonction sur ce pas ; avec des pas sélectionnés, sur leurs
    positions. */
function FlagRows({ drum, roll = false, first, length, flags, stepsPerBeat, playStep }: {
  drum: boolean; roll?: boolean; first: number; length: number; flags: number[]; stepsPerBeat: number; playStep: number;
}) {
  const instrument = useStore((s) => s.tracks.tracks[s.focus]?.instrument);
  const octa = instrument === "octa";
  const rows: { bit: number; name: string; hint: string; cls: string }[] = [
    { bit: STEP_ACCENT, name: drum ? "Accent total" : "Accent", hint: "renforce tous les sons du pas (force : encodeur Accent)", cls: "acc" },
    ...(drum ? [] : [{ bit: STEP_SLIDE, name: "Glissé", hint: octa ? "les verrous du pas glissent depuis les valeurs du moment, et la note tient jusqu'au pas suivant" : "la note tient jusqu'au pas suivant et glisse vers lui (même note : liée) ; les verrous du pas glissent aussi", cls: "slide" }]),
    { bit: STEP_TRIGLESS, name: "Verrous seuls", hint: "le pas pose ses verrous sans jouer de note", cls: "trigless" },
    ...(octa ? [{ bit: STEP_REC, name: "Enregistrer", hint: "le pas lance l'enregistreur de la piste (source et durée : vue synthé, page Enregistreur)", cls: "rec" }] : []),
  ];
  // v0.46 fix27 : valeur du pas (force de l'accent, durée du glissé) : appui long sur la case
  const [edit, setEdit] = useState<{ step: number; kind: StepAmountKind } | null>(null);
  const accAmt = useStore((st) => st.pattern?.accAmt) ?? [];
  const sldAmt = useStore((st) => st.pattern?.sldAmt) ?? [];
  const toggle = (bit: number, s: number) => {
    const st = getState();
    const sel = [...new Set(st.ui.selection.map(stepOf))];
    const steps = sel.length > 1 && sel.includes(s) ? sel : [s];
    const on = !((st.pattern?.flags[s] ?? 0) & bit);
    void actions.seq("seqStepsFlag", JSON.stringify(steps), bit, on);
  };
  return (
    <div className={`seq-flags${drum && !roll ? "" : " seq-flags--roll"}`}>
      {edit && <StepAmountEditor step={edit.step} kind={edit.kind} onClose={() => setEdit(null)} />}
      {rows.map((r) => (
        <div key={r.bit} className={`flag-row flag-row--${r.cls}`}>
          <span className="flag-name" title={`${r.name} : ${r.hint}`}>{r.name}</span>
          <div className="flag-cells">
            {Array.from({ length: PAGE }, (_, i) => {
              const s = first + i;
              const on = ((flags[s] ?? 0) & r.bit) !== 0;
              const out = s >= length;
              const kind: StepAmountKind | null = r.bit === STEP_ACCENT ? "acc" : r.bit === STEP_SLIDE ? "sld" : null;
              const amt = kind === "acc" ? accAmt[s] ?? 0 : kind === "sld" ? sldAmt[s] ?? 0 : 0;
              return (
                <FlagCell key={i} disabled={out} on={on} amount={on ? amt : 0}
                  className={`fcell${on ? " fcell--on" : ""}${s % stepsPerBeat === 0 ? " fcell--beat" : ""}${out ? " fcell--out" : ""}${playStep === s ? " fcell--play" : ""}`}
                  label={`${r.name}, pas ${s + 1}${kind ? " (appui long : valeur)" : ""}`}
                  onToggle={() => toggle(r.bit, s)} onEdit={kind ? () => setEdit({ step: s, kind }) : undefined} />
              );
            })}
          </div>
        </div>
      ))}
    </div>
  );
}

// =====================================================================================
// Sélection : gestes, presse-papiers, clavier. Les fonctions lisent l'état au moment de
// l'action (getState) : pas de valeur périmée pendant un glissé.

type Drag =
  | { kind: "move"; keys: number[]; dRow: number; dStep: number; copy: boolean }
  | { kind: "rect"; r0: number; s0: number; r1: number; s1: number };

interface Gesture {
  id: number;
  row0: number; step0: number; row: number; step: number;
  hit: number | null;          // pas posé sous le doigt au départ
  ctrl: boolean; shift: boolean;
  moved: boolean;
  keys: number[];              // pas déplacés (glisser un pas)
}

const BAR = 16;

function cellAt(x: number, y: number): { row: number; step: number } | null {
  const el = document.elementFromPoint(x, y)?.closest<HTMLElement>("[data-row][data-step]");
  if (!el) return null;
  return { row: Number(el.dataset.row), step: Number(el.dataset.step) };
}

/** Note qui tient la hauteur `n` au pas `s` après avoir commencé plus tôt (queue d'une note longue). */
function heldOrigin(p: NonNullable<ReturnType<typeof getState>["pattern"]>, n: number, s: number): { lane: number; step: number } | null {
  for (let k = 1; k <= MAX_LEN8 / 8 && s - k >= 0; ++k)
    for (let l = 0; l < p.lanes.length; ++l) {
      const ln = p.lanes[l];
      if (ln.vel[s - k] > 0 && ln.note[s - k] === n && ln.len[s - k] > k * 8) return { lane: l, step: s - k };
    }
  return null;
}

/** v0.47 (maj. 5 nov.) : pas d'une même ligne dont la durée couvre le pas `s` (−1 : aucun). */
function laneHeldOrigin(ln: { vel: number[]; len: number[] }, s: number): number {
  for (let k = 1; k <= MAX_LEN8 / 8 && s - k >= 0; ++k) {
    if ((ln.vel[s - k] ?? 0) > 0) return (ln.len[s - k] ?? 8) > k * 8 ? s - k : -1;
  }
  return -1;
}

function hitAt(drum: boolean, row: number, step: number): number | null {
  const p = getState().pattern;
  if (!p) return null;
  if (drum) return (p.lanes[row]?.vel[step] ?? 0) > 0 ? cellKey(row, step) : null;
  const l = p.lanes.findIndex((ln) => ln.vel[step] > 0 && ln.note[step] === row);
  return l >= 0 ? cellKey(l, step) : null;
}

/** Les éditions de la sélection passent l'une après l'autre : chacune part du pattern tel que
    l'a laissé la précédente (sinon deux déplacements rapides pourraient dupliquer des pas). */
let editQueue: Promise<unknown> = Promise.resolve();
function queued(job: () => Promise<unknown>) {
  editQueue = editQueue.then(job).catch(() => undefined);
  return editQueue;
}

/** Déplace (ou copie) des pas : dRow en lignes (batterie) ou en demi-tons (mélodique). */
function moveCells(keys: number[] | "selection", dRow: number, dStep: number, copy: boolean, drum: boolean) {
  return queued(() => moveCellsNow(keys, dRow, dStep, copy, drum));
}
async function moveCellsNow(keys: number[] | "selection", dRow: number, dStep: number, copy: boolean, drum: boolean) {
  const s = getState();
  if (keys === "selection") keys = s.ui.selection;
  const p = s.pattern;
  if (!p || keys.length === 0 || (dRow === 0 && dStep === 0 && !copy)) return;
  const cells = snapshot(p, s.extras, keys);
  // déplacer tourne dans la boucle (aucun pas perdu) ; copier abandonne ce qui sort du pattern
  const plan = place(p, cells, dRow, dStep, drum, copy ? [] : cells.map((c) => [c.l, c.s] as [number, number]), !copy);
  if (plan.writes.length < cells.length) showMessage(`${cells.length - plan.writes.length} pas hors du pattern : non ${copy ? "copiés" : "déplacés"}`);
  await writePlan(p, plan);
  setUi({ selection: plan.keys, step: null, stepEdit: false });
}

/** v0.31 : écrit les pas puis fait suivre accent total, glissé et verrous seuls. */
async function writePlan(p: PatternData, plan: Plan) {
  await actions.editCells(payload(plan));
  for (const [bit, steps, on] of flagMoves(p, plan.writes, plan.clear)) await actions.seq("seqStepsFlag", JSON.stringify(steps), bit, on);
}

function copySelection(t: number, drum: boolean, cut: boolean) {
  const s = getState();
  if (!s.pattern || s.ui.selection.length === 0) { showMessage("Rien de sélectionné"); return; }
  const cells = snapshot(s.pattern, s.extras, s.ui.selection);
  setClipboard({ track: t, instrument: s.tracks.tracks[t]?.instrument ?? "", drum, cells });
  if (cut) void deleteSelection();
  showMessage(`${cells.length} pas ${cut ? "coupés" : "copiés"}`);
}

function deleteSelection() { return queued(deleteSelectionNow); }
async function deleteSelectionNow() {
  const s = getState();
  if (!s.pattern || s.ui.selection.length === 0) return;
  const clear = s.ui.selection.map((k) => [laneOf(k), stepOf(k)] as [number, number]);
  await actions.editCells(payload({ clear, writes: [] }));
  setUi({ selection: [], step: null, stepEdit: false });
}

function pasteAt(t: number, drum: boolean, at: { row: number; step: number } | null) { return queued(() => pasteAtNow(t, drum, at)); }
async function pasteAtNow(t: number, drum: boolean, at: { row: number; step: number } | null) {
  const s = getState();
  const clip = getClipboard();
  const p = s.pattern;
  const L = s.catalog?.layout;
  if (!p || !L) return;
  if (!clip || clip.cells.length === 0) { showMessage("Le presse-papiers est vide"); return; }
  if (clip.drum !== drum) { showMessage(clip.drum ? "Ce sont des pas de batterie : collez-les sur une batterie" : "Ce sont des notes : collez-les sur un instrument mélodique"); return; }
  const sameInserts = JSON.stringify(s.tracks.inserts[clip.track] ?? []) === JSON.stringify(s.tracks.inserts[t] ?? []);
  const cells: CellData[] = retarget(clip.cells, clip.track, t, clip.instrument === (s.tracks.tracks[t]?.instrument ?? ""), sameInserts, L);
  const o = origin(cells, drum);
  const target = at ?? { row: o.row, step: Math.min(p.length - 1, s.ui.page * BAR) };
  const plan = place(p, cells, target.row - o.row, target.step - o.step, drum, []);
  if (plan.writes.length < cells.length) showMessage(`${cells.length - plan.writes.length} pas hors du pattern : non collés`);
  await writePlan(p, plan);
  setUi({ selection: plan.keys, step: null, stepEdit: false });
}

function duplicateSelection(drum: boolean) { return queued(() => duplicateSelectionNow(drum)); }
async function duplicateSelectionNow(drum: boolean) {
  const s = getState();
  const p = s.pattern;
  if (!p || s.ui.selection.length === 0) { showMessage("Rien de sélectionné"); return; }
  const cells = snapshot(p, s.extras, s.ui.selection);
  const steps = cells.map((c) => c.s);
  const span = Math.max(...steps) - Math.min(...steps) + 1;
  const plan = place(p, cells, 0, span, drum, []);
  if (plan.writes.length === 0) { showMessage("Pas de place après la sélection : allongez le pattern"); return; }
  await writePlan(p, plan);
  const first = Math.min(...plan.writes.map((w) => w.s));
  setUi({ selection: plan.keys, page: Math.floor(first / BAR), step: null, stepEdit: false });
}

/** v0.43.8 : rectangle de sélection d'un seul tenant, transparent (on voit les notes dessous), comme
    dans les séquenceurs à piano roll : posé par-dessus la grille, autour des cases prises. */
function MarqueeBox() {
  const [box, setBox] = useState<{ x: number; y: number; w: number; h: number } | null>(null);
  useLayoutEffect(() => {
    const cells = document.querySelectorAll<HTMLElement>(".cell--inrect");
    if (cells.length === 0) { setBox(null); return; }
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    cells.forEach((c) => { const r = c.getBoundingClientRect(); x0 = Math.min(x0, r.left); y0 = Math.min(y0, r.top); x1 = Math.max(x1, r.right); y1 = Math.max(y1, r.bottom); });
    const nb = { x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
    setBox((b) => (b && b.x === nb.x && b.y === nb.y && b.w === nb.w && b.h === nb.h ? b : nb));
  });
  if (!box) return null;
  return createPortal(<div className="seq-marquee" style={{ left: box.x - 2, top: box.y - 2, width: box.w + 4, height: box.h + 4 }} aria-hidden />, document.body);
}

/** Raccourcis clavier de la sélection (écran Séquence). */
function useSelectionKeys(drum: boolean, t: number, hover: React.MutableRefObject<{ row: number; step: number } | null>, cursor: { row: number; step: number } | null) {
  useEffect(() => {
    const down = (e: KeyboardEvent) => {
      if (e.defaultPrevented) return; // déjà traité (encodeur qui a le focus, menu…)
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLSelectElement || e.target instanceof HTMLTextAreaElement) return;
      if (e.target instanceof Element && e.target.closest('[role="slider"], [role="menu"], [role="dialog"]')) return;
      const s = getState();
      if (s.ui.screen !== "sequence" || !s.pattern || s.ui.seqView === "auto") return;
      const ctrl = e.ctrlKey || e.metaKey;
      const sel = s.ui.selection;
      const key = e.key.toLowerCase();
      if (ctrl && key === "a") { e.preventDefault(); setUi({ selection: allKeys(s.pattern), step: null, stepEdit: false }); return; }
      if (ctrl && key === "c") { e.preventDefault(); copySelection(t, drum, false); return; }
      if (ctrl && key === "x") { e.preventDefault(); copySelection(t, drum, true); return; }
      if (ctrl && key === "v") { e.preventDefault(); void pasteAt(t, drum, hover.current ?? cursor); return; }
      if (ctrl && key === "d") { e.preventDefault(); void duplicateSelection(drum); return; }
      if (!ctrl && !e.altKey && key === "p" && !e.repeat) { e.preventDefault(); setUi({ paint: !s.ui.paint, selectMode: false }); return; }
      if (key === "escape" && sel.length) { setUi({ selection: [] }); return; }
      if ((key === "delete" || key === "backspace") && sel.length) { e.preventDefault(); void deleteSelection(); return; }
      if (!sel.length || ctrl) return;
      const spb = STEPS_PER_BAR[s.pattern.division] ?? BAR;
      if (key === "arrowleft") { e.preventDefault(); void moveCells("selection", 0, e.shiftKey ? -spb : -1, false, drum); }
      else if (key === "arrowright") { e.preventDefault(); void moveCells("selection", 0, e.shiftKey ? spb : 1, false, drum); }
      // batterie : la ligne du dessus est le pad précédent ; mélodique : un demi-ton (Maj : une octave)
      else if (key === "arrowup") { e.preventDefault(); void moveCells("selection", drum ? -1 : (e.shiftKey ? 12 : 1), 0, false, drum); }
      else if (key === "arrowdown") { e.preventDefault(); void moveCells("selection", drum ? 1 : (e.shiftKey ? -12 : -1), 0, false, drum); }
    };
    window.addEventListener("keydown", down);
    return () => window.removeEventListener("keydown", down);
  }, [drum, t, hover, cursor]);
}

/** v0.44.1 — Pieuvre 8 : la piste T choisie dépliée en clavier chromatique (principe du rouleau de
    notes). Une note par pas et par T : toucher une case pose la note (ou déplace celle du pas) ;
    toucher la note posée la choisit (encodeurs du pas, verrous) ; la toucher choisie, ou clic
    droit, l'enlève. Glisser depuis une case vide écrit une note par pas survolé. La T joue ces
    notes quand son Clavier est sur « Chromatique » (60 = hauteur d'origine de l'emplacement). */
function OctaLaneRoll({ t, T, first, stepsPerBeat, playStep, lengthHint }: { t: number; T: number; first: number; stepsPerBeat: number; playStep: number; lengthHint: (T: number) => void }) {
  const pattern = useStore((s) => s.pattern);
  const extras = useStore((s) => s.extras);
  const ui = useStore((s) => s.ui);
  const [rect, setRect] = useState<{ n0: number; s0: number; n1: number; s1: number } | null>(null);
  if (!pattern) return null;
  const ln = pattern.lanes[T];
  if (!ln) return null;
  const len = ln.length > 0 ? ln.length : pattern.length;
  const listen = (n: number) => { actions.noteOn(t, n, 0.75, false); window.setTimeout(() => actions.noteOff(t, n), 160); };
  const write = (n: number, s: number, vel: number) => {
    const cur = getState().pattern?.lanes[T];
    if (!cur || s < 0 || s >= len) return;
    const on = (cur.vel[s] ?? 0) > 0;
    if (on && cur.note[s] === n) return;
    void actions.seq("seqSetNote", T, s, n).then(() => (on ? undefined : actions.seq("seqSetStep", T, s, vel)));
  };
  const cellFrom = (x: number, y: number) => {
    const el = document.elementFromPoint(x, y)?.closest<HTMLElement>("[data-ln]");
    return el ? { n: Number(el.dataset.ln), s: Number(el.dataset.ls) } : null;
  };
  const inRect = (n: number, s: number) => !!rect && n >= Math.min(rect.n0, rect.n1) && n <= Math.max(rect.n0, rect.n1) && s >= Math.min(rect.s0, rect.s1) && s <= Math.max(rect.s0, rect.s1);
  // v0.45 : mêmes gestes que partout dans la séquence — Ctrl (ou mode Sélection) : cliquer choisit une
  // note, tirer un rectangle en prend plusieurs ; Pinceau : glisser écrit une note par pas survolé ;
  // sinon un clic pose (ou choisit) la note.
  const startRect = (e: React.PointerEvent, n: number, s: number, add: boolean) => {
    const id = e.pointerId;
    let cur = { n0: n, s0: s, n1: n, s1: s };
    let moved = false;
    setRect(cur);
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      const c = cellFrom(ev.clientX, ev.clientY);
      if (!c || (c.n === cur.n1 && c.s === cur.s1)) return;
      moved = true;
      cur = { ...cur, n1: c.n, s1: c.s };
      setRect(cur);
    };
    const end = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", end);
      window.removeEventListener("pointercancel", end);
      setRect(null);
      const st = getState();
      const lane = st.pattern?.lanes[T];
      if (!lane) return;
      const sel = new Set(add ? st.ui.selection : []);
      if (!moved) {
        // clic simple : la note de cette case entre ou sort de la sélection
        if ((lane.vel[s] ?? 0) > 0 && lane.note[s] === n) { const k = cellKey(T, s); if (sel.has(k)) sel.delete(k); else sel.add(k); }
      } else {
        const [na, nb] = [Math.min(cur.n0, cur.n1), Math.max(cur.n0, cur.n1)], [sa, sb] = [Math.min(cur.s0, cur.s1), Math.max(cur.s0, cur.s1)];
        for (let x = sa; x <= sb && x < len; ++x) if ((lane.vel[x] ?? 0) > 0 && lane.note[x] >= na && lane.note[x] <= nb) sel.add(cellKey(T, x));
      }
      setUi({ selection: [...sel], step: null, stepEdit: false });
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", end);
    window.addEventListener("pointercancel", end);
  };
  // v0.47 (maj. 5 nov.) : longueur d'une note au glisser, comme le rouleau des synthés : bord droit d'une note
  // courte, ou queue d'une note longue ; la durée suit le doigt, pas par pas
  const startLen = (e: React.PointerEvent, s0: number) => {
    const id = e.pointerId;
    let moved = false;
    let last = getState().pattern?.lanes[T]?.len[s0] ?? 8;
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      const c = cellFrom(ev.clientX, ev.clientY);
      if (!c) return;
      const len8 = Math.min(MAX_LEN8, Math.max(1, c.s - s0 + 1) * 8);
      if (len8 === last) return;
      if (!moved) lengthHint(T);
      moved = true; last = len8;
      void actions.seq("seqSetLength8", T, s0, len8);
    };
    const end = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", end);
      window.removeEventListener("pointercancel", end);
      if (!moved) setUi({ lane: T, step: s0, stepEdit: false, selection: [] });
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", end);
    window.addEventListener("pointercancel", end);
  };
  const down = (e: React.PointerEvent, n: number, s: number) => {
    if (s >= len) return;
    e.preventDefault();
    const u = getState().ui;
    const on = (ln.vel[s] ?? 0) > 0;
    if (e.button === 0 && !(e.ctrlKey || e.metaKey || e.shiftKey) && !u.selectMode) {
      if (on && ln.note[s] === n) {
        const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
        if ((ln.len[s] ?? 8) <= 8 && e.clientX > r.right - r.width * 0.3) { startLen(e, s); return; }
      } else if (!on) {
        const o = laneHeldOrigin(ln, s);
        if (o >= 0 && ln.note[o] === n) { startLen(e, o); return; }
      }
    }
    if (e.button === 2) {
      if (on) { void actions.seq("seqSetStep", T, s, 0); if (u.step === s) setUi({ step: null, stepEdit: false }); }
      return;
    }
    if (e.button !== 0) return;
    const ctrl = e.ctrlKey || e.metaKey;
    if (ctrl || u.selectMode) { startRect(e, n, s, ctrl); return; }
    if (u.selection.length) setUi({ selection: [] });
    if (!u.paint) {
      // clic : pose la note (ou déplace celle du pas), choisit la note posée, l'enlève si elle l'est déjà
      if (on && ln.note[s] === n) {
        if (u.lane === T && u.step === s) { void actions.seq("seqSetStep", T, s, 0); setUi({ step: null, stepEdit: false }); }
        else setUi({ lane: T, step: s, stepEdit: false, selection: [] });
        return;
      }
      write(n, s, u.shift ? 127 : 100);
      setUi({ lane: T, step: s, stepEdit: false, selection: [] });
      listen(n);
      return;
    }
    if (on && ln.note[s] === n) {
      if (u.lane === T && u.step === s) { void actions.seq("seqSetStep", T, s, 0); setUi({ step: null, stepEdit: false }); }
      else setUi({ lane: T, step: s, stepEdit: false, selection: [] });
      return;
    }
    // case vide : la note s'écrit, puis chaque pas survolé en glissant reçoit la note sous le doigt
    const vel = u.shift ? 127 : 100;
    write(n, s, vel);
    setUi({ lane: T, step: s, stepEdit: false, selection: [] });
    listen(n);
    const id = e.pointerId;
    const seen = new Map<number, number>([[s, n]]);
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      const c = cellFrom(ev.clientX, ev.clientY);
      if (!c || seen.get(c.s) === c.n) return;
      seen.set(c.s, c.n);
      write(c.n, c.s, vel);
    };
    const end = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", end);
      window.removeEventListener("pointercancel", end);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", end);
    window.addEventListener("pointercancel", end);
  };
  return (
    <>
    {rect && <MarqueeBox />}
    <div className={`roll roll--lane${ui.rollOne ? " roll--one" : ""}`} role="grid" aria-label={`Notes de T${T + 1}`} style={{ "--k": trackColor(T) } as React.CSSProperties}
      onWheel={(e) => { if (!(e.target as HTMLElement).closest(".roll-key")) return; setRollBase(getState().ui.rollBase + (e.deltaY < 0 ? 12 : -12)); }}>
      {Array.from({ length: rollRows(ui) }, (_, k) => {
        const n = ui.rollBase + rollRows(ui) - 1 - k;
        const black = [1, 3, 6, 8, 10].includes(n % 12);
        const outOfScale = ui.scale > 0 && !inScale(n, ui.scale, ui.root);
        return (
          <div key={n} className={`roll-row${scaleRowClass(n, black, outOfScale, ui.scale)}${n % 12 === 0 ? " roll-row--c" : ""}${(ui.scale > 0 ? ((n - ui.root) % 12 + 12) % 12 === 0 : n === 60) ? " roll-row--root" : ""}`}>
            <button type="button" className="roll-key" {...rollKeyHandlers(t, n)} title={n === 60 ? "Hauteur d'origine de l'emplacement (glisser : changer d'octave)" : "Toucher : jouer · glisser vers le haut ou le bas : changer d'octave"}>{noteName(n)}{n === 60 ? " ●" : ""}</button>
            <div className="roll-cells">
              {Array.from({ length: PAGE }, (_, i) => {
                const s = first + i;
                const on = (ln.vel[s] ?? 0) > 0 && ln.note[s] === n;
                const ex = on ? extras.find((x) => x.lane === T && x.step === s) : undefined;
                const classes = ["rcell"];
                if (on) classes.push("rcell--on");
                if (on && ln.len[s] <= 8) classes.push("rcell--short");
                if (!on && (ln.vel[s] ?? 0) === 0) { const o = laneHeldOrigin(ln, s); if (o >= 0 && ln.note[o] === n) classes.push("rcell--held"); }
                if (on && ((pattern.flags[s] ?? 0) & STEP_SLIDE)) classes.push("rcell--slide");
                if (on && ((pattern.flags[s] ?? 0) & STEP_ACCENT)) classes.push("cell--acc");
                if (s % stepsPerBeat === 0) classes.push("rcell--beat");
                if (s >= len) classes.push("rcell--out");
                if (playStep === s) classes.push("rcell--play");
                if (on && ui.lane === T && ui.step === s) classes.push("rcell--selected");
                if (ex && (ex.cond > 0 || ex.prob < 100)) classes.push("cell--cond");
                if (ex && ex.locks.length > 0) classes.push("cell--lock");
                if (on && ui.selection.includes(cellKey(T, s))) classes.push("cell--sel");
                if (inRect(n, s)) classes.push("cell--inrect");
                return (
                  <button key={i} type="button" className={classes.join(" ")} disabled={s >= len} data-ln={n} data-ls={s}
                    style={on ? { "--vel": `${0.4 + (ln.vel[s] / 127) * 0.6}`, "--len": `${Math.min(PAGE - i, ln.len[s] / 8)}` } as React.CSSProperties : undefined}
                    aria-label={`T${T + 1}, ${noteName(n)}, pas ${s + 1}${on ? " (note posée)" : ""}`} aria-pressed={on}
                    onPointerDown={(e) => down(e, n, s)}
                    onContextMenu={(e) => e.preventDefault()} />
                );
              })}
            </div>
          </div>
        );
      })}
    </div>
    </>
  );
}

/** v0.46 fix29 : bascule de la page des patterns (A–H ⇄ I–P) ; allumée quand le pattern joué est sur l'autre page. */
function PatternPageChip({ page, other, onTurn }: { page: number; other: boolean; onTurn: () => void }) {
  const next = (page + 1) % ROW_PAGES;
  return (
    <button type="button" className={`pchip pchip-page${other ? " playing" : ""}`} onClick={onTurn}
      title={`Montrer les patterns ${rowPageName(next)}`} aria-label={`Patterns ${rowPageName(next)}`}>{rowPageName(next)}</button>
  );
}
