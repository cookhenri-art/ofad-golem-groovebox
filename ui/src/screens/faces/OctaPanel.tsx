// Groovebox — OFAD — AGPL-3.0-or-later
// Façade du sampler de performance : principe des samplers de performance à écran et pages —
// machines Flex et Pickup (Thru et Neighbor retirées en v0.12.7, Static en v0.15 : elle jouait comme Flex) ;
// la machine de la piste, l'écran (forme d'onde, découpes, tête de lecture, enregistreur), six
// boutons A à F dont le rôle suit la page (Lecture, Réglages, Ampli, Filtre, Lo-fi, LFO, Effet 1, Effet 2,
// Enregistreur), les touches de l'enregistreur. Dessin original.
// v0.40 (principe des samplers de performance à 8 pistes) : écran au centre ; à gauche les touches des
// 8 pistes Pieuvre (on passe de l'une à l'autre) ; à droite les touches de page ; l'écran montre la
// lecture (région, point de boucle, fondus), les emplacements (réserve commune de 8 × 16 et les
// 8 enregistreurs : choisir, charger, copier, vider) ou l'éditeur audio (sélection, inverser,
// normaliser, fondus, rogner, silence) ; page Attributs : réglages propres à l'emplacement joué.
// v0.42 : Pieuvre 8 est UNE piste du mixeur à 8 pistes internes T1 à T8 (touches à gauche, avec leur
// coupure et leur solo) ; chaque T a ses réglages et ses 2 effets ; page Sortie : égaliseur et
// compresseur communs (inserts 1 et 2 de la piste du mixeur).

import { useEffect, useRef, useState } from "react";
import type { ModuleDesc } from "../../engine/protocol";
import { actions, getState, setUi, showMessage, useStore } from "../../engine/store";
import { paramAddress, useParam, useWriteTarget, writeParam } from "../../controls/bindings";
import { getLockClip } from "../../engine/selection";
import { Encoder } from "../../controls/Encoder";
import { toNorm, toPlain, trackColor } from "../../engine/params";
import { SampleView } from "../Visuals";
import { StepsEditor } from "./MatrixPanel";
import { ShiftButtons } from "./ShiftButtons";

const MACHINES = ["Flex", "Pickup"];
const MACHINE_HINTS = [
  "un échantillon en mémoire, ou le tampon de l'enregistreur",
  "un échantillon long (le tampon de l'enregistreur n'y est pas lu)",
  "boucle enregistrée en direct (entrée, pistes choisies de la groovebox ou mixage) : enregistrer, fermer, surcouches",
];
const SOURCES = ["Entrée audio", "Pistes choisies", "Mixage principal"];
type Page = { id: string; label: string; ids?: string[] };
const PAGES: Page[] = [
  // v0.43 : lecture et réglages de lecture sur la même page (12 boutons : tout ce qui sert ensemble)
  { id: "src", label: "Lecture", ids: ["tune", "start", "length", "rate", "retrig", "rtim", "stretch", "loop", "loops", "slices", "keymode", "bars"] },
  { id: "attr", label: "Attributs" },
  { id: "amp", label: "Ampli", ids: ["attack", "hold", "holdmode", "release", "volume", "balance"] },
  { id: "filter", label: "Filtre", ids: ["ftype", "fcut", "freso", "fenv", "fatk", "fdec"] },
  { id: "lofi", label: "Lo-fi", ids: ["drive", "bits", "srr"] },
  { id: "lfo", label: "LFO" },
  { id: "fx1", label: "Effet 1" },
  { id: "fx2", label: "Effet 2" },
  { id: "rec", label: "Enregistreur" },
  { id: "keys", label: "Clavier" },
  { id: "out", label: "Sortie" },
];
const LETTERS = ["A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L"];
// choix d'emplacement d'une piste interne (moteur, Octa.h) : 0 son enregistreur, 1-16 ses emplacements,
// 17-144 la réserve (8 × 16), 145-152 les enregistreurs des 8 pistes internes
const POOL_BASE = 17, REC_BASE = 145;
const NOTE_NAMES = ["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"];
const noteName = (n: number) => `${NOTE_NAMES[((n % 12) + 12) % 12]}${Math.floor(n / 12) - 2}`;
type View = "play" | "slots" | "edit";

/** Ce que désigne le choix d'emplacement de la piste interne T : un emplacement de la réserve
    (0 à 127) ou un enregistreur (rec : numéro de la piste interne). */
function slotOwner(choice: number, T: number): { index: number; rec: number } {
  if (choice <= 0) return { index: -1, rec: T };
  if (choice <= 16) return { index: T * 16 + choice - 1, rec: -1 };
  if (choice < REC_BASE) return { index: choice - POOL_BASE, rec: -1 };
  return { index: -1, rec: choice - REC_BASE };
}

/** v0.43.5 : modes de déclenchement des LFO (valeur du réglage « Mode », nom, aide). */
const LFO_MODES: [number, string, string][] = [
  [0, "Redéclenché", "Chaque déclenchement remet le LFO au début (phase réglée)"],
  [2, "Libre", "Part au premier déclenchement, puis tourne sans jamais être remis au début (même entre les notes)"],
  [3, "Une fois", "Remis au début à chaque déclenchement, fait un seul cycle puis s'arrête sur sa dernière valeur"],
  [5, "Demi-cycle", "Comme « Une fois », mais s'arrête au milieu du cycle"],
  [4, "Tenu", "Tourne librement ; à chaque déclenchement, sa valeur du moment est prise et gardée jusqu'au suivant"],
];

/** v0.43.4 : nom court d'un choix d'emplacement, vu de la piste T (badge des touches de pas). */
function slotShort(choice: number, T: number): string {
  if (choice <= 0) return "R";
  if (choice <= 16) return String(choice);
  if (choice < REC_BASE) { const p = choice - POOL_BASE; return Math.floor(p / 16) === T ? String((p % 16) + 1) : `T${Math.floor(p / 16) + 1}·${(p % 16) + 1}`; }
  return `R${choice - REC_BASE + 1}`;
}

function Knob({ global, label }: { global: number; label?: string }) {
  const b = useParam(global, label);
  return <div className="fc ot-knob"><Encoder binding={b} variant="knob" /></div>;
}

// v0.42 : effets de sortie de Pieuvre 8 (inserts 1 et 2 de sa piste du mixeur)
const OUT_FX = [{ slot: 0, label: "Égaliseur", def: "eq" }, { slot: 1, label: "Compresseur", def: "comp" }];

export function OctaPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const [page, setPage] = useState("src");
  const [view, setView] = useState<View>("play");
  const [attrBank, setAttrBank] = useState(0);
  const [lfo, setLfo] = useState(1);
  const [outBank, setOutBank] = useState(0);
  const [keepTo, setKeepTo] = useState(0);
  // v0.42 : la piste interne réglée (T1 à T8) est la ligne choisie (pads, séquence, touches T)
  const T = useStore((s) => Math.max(0, Math.min(7, s.ui.lane)));
  const pre = `t${T + 1}_`;
  const values = useStore((s) => s.channels[channel]?.inst);
  const fx = useStore((s) => s.channels[channel]?.fx);
  const padfx = useStore((s) => s.channels[channel]?.padfx);
  const samples = useStore((s) => s.channels[channel]?.samples);
  const effects = useStore((s) => s.catalog?.effects);
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const uidx = (id: string) => idx(pre + id);
  const plain = (id: string) => { const i = uidx(id); return i < 0 ? 0 : toPlain(m.params[i], values?.[i] ?? 0); };
  const addr = (id: string) => { const i = uidx(id); return i < 0 ? -1 : paramAddress(channel, "inst", 0, i); };
  // nom d'un réglage de la piste interne, sans « T3 · » (la piste est déjà dite à l'écran)
  const nameOf = (id: string) => { const i = uidx(id); return i < 0 ? undefined : m.params[i].name.replace(/^T\d · /, ""); };
  const k = (id: string) => ({ global: addr(id), label: nameOf(id) });

  // destination du LFO réglé (page LFO)
  const lfoDstI = uidx(`mx${lfo}_dst`);
  const lfoDstName = lfoDstI >= 0 ? m.params[lfoDstI].choices?.[Math.round(plain(`mx${lfo}_dst`))] ?? "" : "";
  const machine = Math.round(plain("machine"));
  // v0.43.4 : emplacement joué — en mode verrous, celui verrouillé sur le pas choisi (sinon celui de base)
  const target = useWriteTarget(channel);
  const extras = useStore((s) => s.extras);
  const slotI = uidx("slot");
  const slotAddr = addr("slot");
  const slotLock = target.kind === "step" ? extras.find((e) => e.lane === target.lane && e.step === target.step)?.locks.find(([g]) => g === slotAddr)?.[1] : undefined;
  const slot = slotLock !== undefined && slotI >= 0 ? Math.round(toPlain(m.params[slotI], slotLock)) : Math.round(plain("slot"));
  const owner = slotOwner(slot, T);
  const sampleIndex = Math.max(0, owner.index);
  const wave = useStore((s) => s.waves[`${channel}:${sampleIndex}`]);
  const hasSample = owner.index >= 0 && !!samples?.[sampleIndex];
  useEffect(() => { if (hasSample && wave === undefined) void actions.wave(channel, sampleIndex); }, [hasSample, wave, channel, sampleIndex]);
  // attributs de l'emplacement joué (réglages de l'instrument « e1… e128 »)
  const attrAddr = (id: string) => { const i = idx(`e${sampleIndex + 1}${id}`); return i < 0 || owner.index < 0 ? -1 : paramAddress(channel, "inst", 0, i); };
  const attrPlain = (id: string) => {
    const i = idx(`e${sampleIndex + 1}${id}`);
    if (i < 0 || owner.index < 0) return 0;
    const v = values?.[i];
    return v === undefined ? m.params[i].def : toPlain(m.params[i], v);
  };

  // découpes de la grille (dans la région de l'emplacement) ; « Attaques » : celles détectées par le moteur
  const sliceChoice = Math.round(plain("slices"));
  const grid = [0, 2, 4, 8, 16, 32, 64, 0][sliceChoice] ?? 0;
  const zones: [number, number][] = grid > 0 ? Array.from({ length: grid }, (_, k) => [k / grid, (k + 1) / grid]) : [];
  const onsets = sliceChoice === 7 ? wave?.onsets ?? [] : [];
  const start = plain("start") / 100, len = plain("length") / 100;
  const hasAttr = owner.index >= 0 && !!values;
  let trimA = hasAttr ? attrPlain("st") / 100 : 0, trimB = hasAttr ? attrPlain("en") / 100 : 1;
  if (trimB < trimA) [trimA, trimB] = [trimB, trimA];
  if (trimB - trimA < 0.001) { trimA = 0; trimB = 1; }
  const span = trimB - trimA;
  const rel = (x: number) => trimA + x * span;
  const shownStart = rel(grid > 0 ? Math.min(grid - 1, Math.floor(start * grid)) / grid : start);
  const shownEnd = rel(grid > 0 ? (Math.min(grid - 1, Math.floor(start * grid)) + len) / grid : start + (1 - start) * len);
  const loopPt = hasAttr ? attrPlain("lp") / 100 : 0;
  // v0.43.3 : découpes en positions de l'échantillon entier (vue Lecture et Éditeur)
  const absZones: [number, number][] = onsets.length > 0
    ? onsets.map((x, k) => [x, k + 1 < onsets.length ? onsets[k + 1] : 1] as [number, number])
    : zones.map(([x, y]) => [rel(x), rel(y)] as [number, number]);
  const secs = wave?.seconds ?? 0;
  const fadeIn = hasAttr && secs > 0 ? Math.min(1, attrPlain("fi") / 1000 / secs) : 0, fadeOut = hasAttr && secs > 0 ? Math.min(1, attrPlain("fo") / 1000 / secs) : 0;

  // état : piste jouée, puis 5 valeurs par piste interne (enregistreur, secondes, position, boucle, machine)
  const st = (k: number) => status[1 + 5 * T + k] ?? 0;
  const [recState, recSeconds, playPos, loopState] = [st(0), st(1), st(2), st(3)];
  const pickup = machine === 1;
  const source = Math.round(plain("recsrc"));
  const recLabel = pickup
    ? ["Boucle vide", "Enregistre la boucle…", "Boucle jouée", "Surcouche…"][loopState] ?? ""
    : ["Enregistreur vide", "Enregistre…", `Enregistré · ${recSeconds.toFixed(2)} s`][recState] ?? "";

  // effets de la piste interne (inserts de pad T) et de sortie (inserts 1 et 2 de la piste du mixeur)
  const fxSlot = page === "fx1" ? 0 : page === "fx2" ? 1 : -1;
  const tFx = fxSlot >= 0 ? padfx?.find((x) => x.pad === T && x.slot === fxSlot) : undefined;
  const tFxDesc = tFx ? effects?.find((e) => e.type === tFx.type) : undefined;
  const outDesc = page === "out" ? effects?.find((e) => e.type === fx?.[outBank]?.type) : undefined;

  // v0.44.1 : les zones d'une T n'agissent que si le Clavier est sur « Zones » ; leur nom le dit
  const kbAddr = idx("kbmode") >= 0 ? paramAddress(channel, "inst", 0, idx("kbmode")) : -1;
  const zonesOn = idx("kbmode") >= 0 && Math.round(toPlain(m.params[idx("kbmode")], values?.[idx("kbmode")] ?? 0)) === 1;
  const zoneKnobs = [
    { global: addr("zlo"), label: `Zone basse · ${noteName(Math.round(plain("zlo")))}`, off: !zonesOn },
    { global: addr("zhi"), label: `Zone haute · ${noteName(Math.round(plain("zhi")))}`, off: !zonesOn },
  ];
  // les six boutons de la page
  const knobs: { global: number; label?: string; off?: boolean }[] = (() => {
    if (page === "lfo") return [
      { global: addr(`lf${lfo}_shape`), label: "Forme" }, { global: addr(`lf${lfo}_rate`), label: "Vitesse" },
      { global: addr(`lf${lfo}_sync`), label: "Synchro" }, { global: addr(`mx${lfo}_dst`), label: "Destination" },
      { global: addr(`mx${lfo}_amt`), label: "Profondeur" }, { global: addr(`lf${lfo}_delay`), label: "Retard" },
    ];
    if (fxSlot >= 0) return tFxDesc ? tFxDesc.params.slice(0, 6).map((_, i) => ({ global: paramAddress(channel, "pfx", T * 2 + fxSlot, i) })) : [];
    if (page === "keys") return [
      { global: kbAddr, label: "Clavier" },
      ...zoneKnobs,
      k("keymode"), k("tune"), k("fine"),
    ];
    if (page === "out") return outDesc ? outDesc.params.slice(0, 6).map((_, i) => ({ global: paramAddress(channel, "fx", outBank, i) })) : [];
    if (page === "attr") return attrBank === 0
      ? [{ global: attrAddr("bars"), label: "Tempo" }, { global: attrAddr("st"), label: "Début" }, { global: attrAddr("en"), label: "Fin" },
         { global: attrAddr("lp"), label: "Pt de boucle" }, { global: attrAddr("xf"), label: "Fondu boucle" }, { global: attrAddr("gain"), label: "Gain" }]
      : [{ global: attrAddr("fi"), label: "Fondu entrée" }, { global: attrAddr("fo"), label: "Fondu sortie" }, { global: attrAddr("note"), label: `Note · ${noteName(Math.round(attrPlain("note")))}` },
         // v0.43 : plage de notes de la T (mode Zones du clavier), réglable ici à côté de la note ;
         // v0.44.1 : le mode du Clavier est à côté — les zones ne jouent que sur « Zones »
         ...zoneKnobs, { global: kbAddr, label: "Clavier" }];
    if (page === "rec") return pickup
      ? [k("recsrc"), k("pickgain"), k("pickdir"), k("picklen"), k("pickkeep"), k("thrugain")]
      : [k("recsrc"), k("reclen"), k("thrugain"), { global: addr("fine"), label: "Fin" }, k("velamp"), k("slot")];
    return (PAGES.find((p) => p.id === page)?.ids ?? []).map((id) => k(id));
  })();

  const ownerLabel = owner.index < 0 ? (owner.rec === T ? `Enregistreur de T${T + 1}` : `Enregistreur de T${owner.rec + 1}`)
    : samples?.[sampleIndex] ?? `Emplacement T${Math.floor(sampleIndex / 16) + 1} · ${(sampleIndex % 16) + 1} vide`;

  return (
    <div className={knobs.length > 6 ? "ot ot--wide" : "ot"}>
      <OtTrackKeys channel={channel} module={m} T={T} />

      <div className="ot-screen">
        <div className="ot-screen-head">
          <b>T{T + 1} · {MACHINES[machine]}</b>
          <span>{ownerLabel}</span>
          <span className={`ot-rec ot-rec--${pickup ? loopState : recState}`}>{recLabel}</span>
          <span className="ot-views" role="tablist" aria-label="Écran">
            {([["play", "Lecture"], ["slots", "Emplacements"], ["edit", "Éditeur"]] as [View, string][]).map(([v, l]) => (
              <button key={v} type="button" role="tab" aria-selected={view === v} className={view === v ? "on" : ""} onClick={() => setView(v)}>{l}</button>
            ))}
          </span>
        </div>
        {view === "slots" ? (
          <OtSlots key={T} channel={channel} T={T} slot={slot} setSlot={(v) => { if (slotI >= 0) writeParam(target, slotAddr, toNorm(m.params[slotI], v)); }}
            lockStep={target.kind === "step" ? target.step : target.kind === "steps" ? -2 : -1} />
        ) : view === "edit" ? (
          <OtEditor track={channel} index={owner.index} peaks={owner.index >= 0 ? wave?.peaks ?? null : null} seconds={secs} zones={absZones} onCopied={() => setView("slots")}
            module={m} playPos={!pickup && playPos > 0 ? playPos : -1} previewPos={status[1 + 5 * 8] ?? -1} />
        ) : (
          <div className="ot-wave">
            {pickup || owner.index < 0 ? <div className="ot-buffer"><span style={{ width: `${Math.min(100, (recSeconds / 16) * 100)}%` }} /><em>Tampon : {recSeconds.toFixed(2)} s sur 16 · source : {SOURCES[source] ?? ""}</em></div>
              : <SampleView peaks={wave?.peaks ?? null} start={shownStart} end={Math.min(1, shownEnd)} zones={absZones} />}
            {hasSample && !pickup && (
              <>
                {trimA > 0 && <span className="ot-trim" style={{ left: 0, width: `${trimA * 100}%` }} />}
                {trimB < 1 && <span className="ot-trim" style={{ left: `${trimB * 100}%`, right: 0 }} />}
                {loopPt > trimA && loopPt < trimB && <span className="ot-loop" style={{ left: `${loopPt * 100}%` }}><i>boucle</i></span>}
                {fadeIn > 0 && <span className="ot-fade ot-fade--in" style={{ left: `${trimA * 100}%`, width: `${fadeIn * 100}%` }} />}
                {fadeOut > 0 && <span className="ot-fade ot-fade--out" style={{ left: `${(trimB - fadeOut) * 100}%`, width: `${fadeOut * 100}%` }} />}
              </>
            )}
            {!pickup && playPos > 0 && <span className="ot-head" style={{ left: `${playPos * 100}%` }} />}
          </div>
        )}
      </div>

      <div className={knobs.length > 6 ? "ot-knobs ot-knobs--12" : "ot-knobs"}>
        {page === "lfo" && (
          // v0.39 : le LFO réglé, choisi au-dessus des boutons (diode allumée sur celui qu'on règle)
          <div className="ot-lfo-top" role="tablist" aria-label="LFO réglé">
            {[1, 2, 3].map((n) => (
              <button key={n} type="button" role="tab" aria-selected={n === lfo} className={n === lfo ? "on" : ""} onClick={() => setLfo(n)}>
                <span className="fc-led" aria-hidden />LFO {n}
              </button>
            ))}
            {/* v0.41 : Début modulé lu au déclenchement (principe d'origine) ou suivi pendant la lecture */}
            {/* v0.43.5 : seulement quand ce LFO module « Début » (le réglage ne sert qu'à ça) */}
            {uidx("startlive") >= 0 && lfoDstName === "Début" && (
              <span className="ot-live" role="group" aria-label="Début suivi" title="Destination « Début » : « En continu » déplace la lecture pendant qu'elle joue (frotté sans étirement, grains avec étirement) ; « Au départ » ne lit Début qu'à chaque déclenchement">
                <small>Début</small>
                {["Au départ", "En continu"].map((l, v) => (
                  <button key={l} type="button" aria-pressed={Math.round(plain("startlive")) === v} className={Math.round(plain("startlive")) === v ? "on" : ""}
                    onClick={() => actions.setParam(addr("startlive"), toNorm(m.params[uidx("startlive")], v))}>{l}</button>
                ))}
              </span>
            )}
          </div>
        )}
        {page === "lfo" && uidx(`lf${lfo}_mode`) >= 0 && (
          // v0.43.5 : déclenchement du LFO (principe des modes FREE / TRIG / HOLD / ONE / HALF de la machine)
          <div className="ot-lfo-top ot-lfo-mode" role="group" aria-label={`Déclenchement du LFO ${lfo}`}>
            <small>Déclenchement</small>
            {LFO_MODES.map(([v, l, h]) => {
              const cur = Math.round(plain(`lf${lfo}_mode`));
              const on = cur === v || (v === 2 && cur === 1);
              return (
                <button key={v} type="button" aria-pressed={on} className={on ? "on" : ""} title={h}
                  onClick={() => actions.setParam(addr(`lf${lfo}_mode`), toNorm(m.params[uidx(`lf${lfo}_mode`)], v))}>{l}</button>
              );
            })}
          </div>
        )}
        {page === "attr" && (
          <div className="ot-lfo-top" role="tablist" aria-label="Attributs de l'emplacement">
            {["Région et boucle", "Fondus de lecture et note"].map((l, n) => (
              <button key={l} type="button" role="tab" aria-selected={n === attrBank} className={n === attrBank ? "on" : ""} onClick={() => setAttrBank(n)}>
                <span className="fc-led" aria-hidden />{l}
              </button>
            ))}
            <span className="ot-attr-note">{owner.index < 0 ? "Enregistreur : pas d'attributs" : `Emplacement T${Math.floor(sampleIndex / 16) + 1} · ${(sampleIndex % 16) + 1} · note ${noteName(Math.round(attrPlain("note")))}`}</span>
          </div>
        )}
        {fxSlot >= 0 && (
          <div className="ot-lfo-top" aria-label={`Effet ${fxSlot + 1} de T${T + 1}`}>
            <OtFxBar label={`Effet ${fxSlot + 1} de T${T + 1}`} type={tFx?.type ?? ""} effects={effects ?? []} bypass={paramAddress(channel, "padbyp", T * 2 + fxSlot, 0)}
              onType={(t) => void actions.setPadInsert(channel, T, fxSlot, t)} />
          </div>
        )}
        {page === "out" && (
          <div className="ot-lfo-top" role="tablist" aria-label="Effets de sortie">
            {OUT_FX.map((o) => (
              <button key={o.slot} type="button" role="tab" aria-selected={outBank === o.slot} className={outBank === o.slot ? "on" : ""} onClick={() => setOutBank(o.slot)}>
                <span className="fc-led" aria-hidden />{o.label}
              </button>
            ))}
            <OtFxBar label="" type={fx?.[outBank]?.type ?? ""} effects={effects ?? []} bypass={paramAddress(channel, "mix", 0, 6 + outBank)}
              onType={(t) => void actions.setInsert(channel, outBank, t)} suggest={OUT_FX[outBank].def} />
          </div>
        )}
        {Array.from({ length: knobs.length > 6 ? 12 : 6 }, (_, i) => {
          const k = knobs[i];
          return (
            <div key={`${T}:${page}:${lfo}:${attrBank}:${outBank}:${slot}:${i}`} className={`ot-knob-cell${k?.off ? " is-off" : ""}`}
              title={k?.off ? "Inactif : passez le Clavier sur « Zones » pour régler et jouer les zones" : undefined} aria-disabled={k?.off || undefined}>
              <span className="ot-letter">{LETTERS[i]}</span>
              {k && k.global >= 0 ? <Knob global={k.global} label={k.label} /> : <div className="fc ek-empty" />}
            </div>
          );
        })}
      </div>
      <LockSteps T={T} slotAddr={slotAddr} slotName={(norm) => (slotI >= 0 ? slotShort(Math.round(toPlain(m.params[slotI], norm)), T) : "")} />
      {(page === "rec" || (pickup && page === "src")) && source === 1 && <TrackPicks channel={channel} addr={addr} />}
      {page === "keys" && <OtZones channel={channel} module={m} T={T} />}
      {page === "out" && <p className="fp-hint">Sortie de Pieuvre 8 : les 8 pistes T passent ensemble dans ces deux effets (inserts 1 et 2 de sa piste du mixeur).</p>}
      {page === "lfo" && (
        <div className="ot-lfo">
          <div className="ot-lfo-pick" role="tablist" aria-label="LFO">
            {[1, 2, 3].map((n) => <button key={n} type="button" role="tab" aria-selected={n === lfo} className={n === lfo ? "on" : ""} onClick={() => setLfo(n)}><span className="fc-led" aria-hidden />LFO {n}</button>)}
          </div>
          <StepsEditor key={T} channel={channel} module={m} prefix={`${pre}lf${lfo}_s`} count={`${pre}lf${lfo}_steps`} />
          <span className="fp-hint">Forme « Pas » : dessinez la courbe du LFO</span>
        </div>
      )}

      <div className="ot-pages" role="tablist" aria-label="Pages">
        {PAGES.map((p) => (
          <button key={p.id} type="button" role="tab" aria-selected={p.id === page} className={p.id === page ? "on" : ""} onClick={() => setPage(p.id)}>
            <span className="fc-led" aria-hidden />{p.label}
          </button>
        ))}
      </div>

      <div className="ot-rec-keys">
        <MachineKeys key={T} global={addr("machine")} current={machine} />
        <button type="button" className={`ot-key ot-key--rec${(pickup ? loopState === 1 || loopState === 3 : recState === 1) ? " on" : ""}`} onClick={() => actions.recordAction(channel, 0)}
          title={pickup ? "1er appui : enregistrer ; 2e : fermer la boucle ; ensuite : surcouche / lecture" : "Lancer ou arrêter l'enregistrement de T (durée : page Enregistreur)"}>
          <span className="fc-led" aria-hidden />{pickup ? (loopState === 0 ? "Enregistrer" : loopState === 1 ? "Fermer la boucle" : loopState === 2 ? "Surcouche" : "Lecture") : "Enregistrer"}
        </button>
        <button type="button" className="ot-key" onClick={() => actions.recordAction(channel, 1)}>Arrêter</button>
        <button type="button" className="ot-key" onClick={() => actions.recordAction(channel, 2)}>Effacer</button>
        {!pickup && (
          <span className="ot-keep">
            <select aria-label="Emplacement où garder l'enregistrement" value={keepTo} onChange={(e) => setKeepTo(Number(e.target.value))}>
              {Array.from({ length: 16 }, (_, i) => <option key={i} value={i}>Garder dans T{T + 1} · {i + 1}{samples?.[T * 16 + i] ? " (occupé)" : ""}</option>)}
            </select>
            <button type="button" className="ot-key" disabled={recState !== 2} onClick={() => void actions.keepRecording(channel, T, T * 16 + keepTo)}>Garder</button>
          </span>
        )}
      </div>
    </div>
  );
}

/** v0.43 : le clavier de Pieuvre 8 — « Piste choisie » : tout le clavier (et le MIDI) joue la piste T
    choisie ; « Zones » : chaque T a sa plage de notes, toutes jouables en même temps (la note basse de
    la zone joue l'échantillon à sa hauteur d'origine). */
function OtZones({ channel, module: m, T }: { channel: number; module: ModuleDesc; T: number }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const val = (id: string) => { const i = m.params.findIndex((p) => p.id === id); return i < 0 ? 0 : Math.round(toPlain(m.params[i], values?.[i] ?? 0)); };
  const zones = val("kbmode") === 1;
  return (
    <div className="ot-zones">
      <span className="fp-hint">{zones
        ? "Zones : chaque piste joue dans sa plage de notes (boutons B et C pour la piste choisie) ; toutes ensemble au clavier et en MIDI."
        : "Piste choisie : tout le clavier (et le MIDI, et les pads) joue la piste T choisie, en chromatique. Passez le bouton A sur « Zones » pour jouer les 8 pistes ensemble."}</span>
      {zones && (
        <div className="ot-zone-list">
          {Array.from({ length: 8 }, (_, k) => (
            <span key={k} className={k === T ? "on" : ""}><b>T{k + 1}</b> {noteName(val(`t${k + 1}_zlo`))} – {noteName(val(`t${k + 1}_zhi`))}</span>
          ))}
        </div>
      )}
    </div>
  );
}

/** v0.43 : les 16 touches de pas de la piste interne choisie, sur 4 pages (jusqu'à 4 mesures de
    16 pas), comme les touches de pas des samplers de performance. Toucher : poser ou retirer le pas ;
    appui long (ou mode « Verrous ») : choisir le pas, puis tourner les boutons pour y écrire des
    verrous. Longueur de la ligne de T : 1 à 4 mesures (les autres T gardent la leur). */
/** Rangée de pas à verrous (principe des séquenceurs à verrous) : pages de 16, Suivre, Mesures, Verrous.
    v0.45.2 : aussi dans l'onglet Pas des synthés (ligne 1) et des boîtes à rythmes (pad choisi). */
export function LockSteps({ T, slotAddr = -1, slotName, name, shift = "lane", melodic = false }: { T: number; slotAddr?: number; slotName?: (norm: number) => string; name?: string; shift?: "lane" | "all" | "none"; melodic?: boolean }) {
  const nm = name ?? `T${T + 1}`;
  const pattern = useStore((s) => s.pattern);
  const extras = useStore((s) => s.extras);
  const pos = useStore((s) => s.tick.pos);
  const playing = useStore((s) => s.tick.t.playing);
  const sel = useStore((s) => (s.ui.stepEdit && s.ui.lane === T ? s.ui.step : null));
  const focus = useStore((s) => s.focus);
  useStore((s) => s.ui.message); // le bouton Coller suit la copie (message affiché)
  const [page, setPage] = useState(0);
  const [lockMode, setLockMode] = useState(false);
  const [follow, setFollow] = useState(true);
  const press = useRef<{ step: number; timer: number; long: boolean } | null>(null);
  // v0.46 fix16 (synthés) : la dernière touche jouée (pads, clavier, clavier MIDI à l'arrêt) est la
  // note écrite par les pas posés ; jouée pendant qu'un pas posé est choisi, elle devient sa note.
  const played = useStore((s) => s.played);
  const lastPlayed = useRef(played.n);
  useEffect(() => {
    if (played.n === lastPlayed.current) return;
    lastPlayed.current = played.n;
    if (!melodic || sel === null || played.note < 0) return;
    const l = getState().pattern?.lanes[T];
    if ((l?.vel[sel] ?? 0) > 0 && l?.note[sel] !== played.note) void actions.seq("seqSetNote", T, sel, played.note);
  }, [played, melodic, sel, T]);
  const ln = pattern?.lanes[T];
  const patLen = pattern?.length ?? 16;
  const len = ln && ln.length > 0 ? ln.length : patLen;
  const pages = Math.max(1, Math.min(4, Math.ceil(len / 16)));
  const playStep = playing ? pos[T] ?? -1 : -1;
  const playPage = playStep >= 0 ? Math.floor(playStep / 16) : -1;
  const shown = follow && playPage >= 0 && playPage < pages ? playPage : Math.min(page, pages - 1);
  if (!pattern || !ln) return null;

  const choose = (step: number) => {
    if (sel === step) { setUi({ step: null, stepEdit: false }); return; }
    setUi({ lane: T, step, stepEdit: true, selection: [] });
    showMessage(`Pas ${step + 1} de ${nm} : tournez les boutons pour y poser des verrous`);
  };
  const down = (step: number) => {
    const timer = window.setTimeout(() => { if (press.current) { press.current.long = true; choose(step); } }, 380);
    press.current = { step, timer, long: false };
  };
  const up = (step: number) => {
    const p = press.current;
    press.current = null;
    if (!p || p.step !== step) return;
    window.clearTimeout(p.timer);
    if (p.long) return;
    if (lockMode) choose(step);
    else { actions.toggleStep(T, step, melodic && played.note >= 0 ? played.note : undefined); if (sel === step) setUi({ step: null, stepEdit: false }); }
  };
  // longueur de la ligne de T en mesures ; le pattern s'allonge si besoin, sans changer les autres T
  const setBars = async (b: number) => {
    const n = b * 16;
    if (n > patLen) {
      for (let l = 0; l < pattern.lanes.length; ++l) if (l !== T && (pattern.lanes[l]?.length ?? 0) === 0) await actions.seq("seqLaneLength", l, patLen);
      await actions.seq("seqTrackLength", n);
      await actions.seq("seqLaneLength", T, 0);
    } else await actions.seq("seqLaneLength", T, n === patLen ? 0 : n);
    setPage(Math.min(page, b - 1));
  };
  const bars = Math.max(1, Math.min(4, Math.ceil(len / 16)));
  return (
    <div className="ot-steps" aria-label={`Pas de ${nm}`}>
      <div className="ot-steps-head">
        <b>{nm} · pas</b>
        <span className="ot-steps-pages" role="tablist" aria-label="Page de 16 pas">
          {Array.from({ length: 4 }, (_, k) => (
            <button key={k} type="button" role="tab" aria-selected={shown === k} disabled={k >= pages}
              className={`${shown === k ? "on" : ""}${playPage === k ? " play" : ""}`}
              onClick={() => { setPage(k); setFollow(false); }} title={k >= pages ? "Allongez la ligne (Mesures) pour cette page" : `Pas ${k * 16 + 1} à ${k * 16 + 16}`}>
              <span className="fc-led" aria-hidden />{k + 1}
            </button>
          ))}
        </span>
        <button type="button" className={`ot-steps-opt${follow ? " on" : ""}`} aria-pressed={follow} onClick={() => setFollow(!follow)} title="La page suit la lecture">Suivre</button>
        {melodic && (
          <span className={`ot-steps-note${played.note < 0 ? " none" : ""}`} title="Note écrite par les pas que vous posez : jouez une touche (pads, clavier) pour la changer. Un pas choisi (appui long) prend la touche jouée.">
            Note <b>{played.note >= 0 ? noteName(played.note) : "—"}</b>
          </span>
        )}
        {shift !== "none" && <ShiftButtons lane={shift === "lane" ? T : -1} label="Décaler" what={shift === "lane" ? `les pas de ${nm}` : "la séquence"} />}
        <span className="ot-steps-bars" role="group" aria-label="Longueur de la ligne">
          <small>Mesures</small>
          {[1, 2, 3, 4].map((b) => (
            <button key={b} type="button" className={bars === b && len === b * 16 ? "on" : ""} onClick={() => void setBars(b)} title={`${nm} sur ${b} mesure${b > 1 ? "s" : ""} (${b * 16} pas)`}>{b}</button>
          ))}
        </span>
        {sel !== null && (
          <span className="ot-steps-locks" role="group" aria-label="Verrous du pas">
            <button type="button" className="ot-steps-opt" disabled={!extras.some((e) => e.lane === T && e.step === sel && e.locks.length > 0)}
              onClick={() => actions.copyStepLocks(T, sel)} title={`Copier les verrous du pas ${sel + 1} (emplacement, réglages, effets)`}>Copier</button>
            <button type="button" className="ot-steps-opt" disabled={getLockClip()?.track !== focus}
              onClick={() => void actions.pasteStepLocks(T, sel)} title={`Coller les verrous copiés sur le pas ${sel + 1} (d'une T à une autre : réglages de la T d'arrivée)`}>Coller</button>
            <button type="button" className="ot-steps-opt" disabled={!extras.some((e) => e.lane === T && e.step === sel && e.locks.length > 0)}
              onClick={() => void actions.clearLocks([T], sel).then((n) => showMessage(`${n} verrou${n > 1 ? "s" : ""} retiré${n > 1 ? "s" : ""} du pas ${sel + 1}`))}
              title={`Retirer tous les verrous du pas ${sel + 1} (emplacement, réglages, effets) : le pas rejoue les réglages de ${nm}`}>Effacer</button>
          </span>
        )}
        {sel === null && extras.some((e) => e.lane === T && e.locks.length > 0) && (
          <button type="button" className="ot-steps-opt" onClick={() => void actions.clearLocks([T]).then((n) => showMessage(`${n} verrou${n > 1 ? "s" : ""} retiré${n > 1 ? "s" : ""} des pas de ${nm}`))}
            title={`Retirer les verrous de tous les pas de ${nm} dans ce pattern`}>Sans verrous</button>
        )}
        <button type="button" className={`ot-steps-opt${lockMode ? " on" : ""}`} aria-pressed={lockMode}
          onClick={() => { setLockMode(!lockMode); if (lockMode) setUi({ step: null, stepEdit: false }); }}
          title="Mode verrous : toucher un pas le choisit (sans le poser ni l'enlever) ; les boutons écrivent alors dans ce pas. Sinon : appui long sur un pas.">Verrous</button>
      </div>
      <div className="ot-steps-keys">
        {Array.from({ length: 16 }, (_, k) => {
          const s = shown * 16 + k;
          const on = (ln.vel[s] ?? 0) > 0;
          const out = s >= len;
          const ex = extras.find((e) => e.lane === T && e.step === s);
          const cls = ["ot-step"];
          if (on) cls.push("on");
          if (out) cls.push("out");
          if (k % 4 === 0) cls.push("beat");
          if (playStep === s) cls.push("play");
          if (sel === s) cls.push("sel");
          if (ex && ex.locks.length > 0) cls.push("lock");
          // v0.43.4 : emplacement verrouillé sur ce pas (lecture d'un autre son à ce pas)
          const slotLock = ex?.locks.find(([g]) => g === slotAddr)?.[1];
          return (
            <button key={k} type="button" className={cls.join(" ")} disabled={out}
              onPointerDown={() => down(s)} onPointerUp={() => up(s)} onPointerLeave={() => { if (press.current?.step === s) { window.clearTimeout(press.current.timer); press.current = null; } }}
              title={`Pas ${s + 1}${on ? ` (posé${melodic ? `, ${noteName(ln.note[s] ?? 60)}` : ""})` : ""}${ex && ex.locks.length ? ` · ${ex.locks.length} verrou${ex.locks.length > 1 ? "s" : ""}` : ""} — toucher : poser / retirer ; appui long : verrous`}>
              <span className="fc-led" aria-hidden /><i>{melodic && on ? noteName(ln.note[s] ?? 60) : s + 1}</i>
              {slotLock !== undefined && slotName && <em className="ot-step-slot" title="Emplacement joué sur ce pas">{slotName(slotLock)}</em>}
            </button>
          );
        })}
      </div>
    </div>
  );
}

/** v0.42 : effet d'une piste T ou de sortie — son type (liste du catalogue) et son interrupteur. */
function OtFxBar({ label, type, effects, bypass, onType, suggest }: { label: string; type: string; effects: ModuleDesc[]; bypass: number; onType: (t: string) => void; suggest?: string }) {
  const b = useParam(type && bypass >= 0 ? bypass : null, "Contourné");
  const off = (b?.normalised ?? 0) >= 0.5;
  return (
    <span className="ot-fxbar">
      {label && <small>{label}</small>}
      <select aria-label="Effet" value={type} onChange={(e) => onType(e.target.value)}>
        <option value="">— aucun —</option>
        {effects.map((e) => <option key={e.type} value={e.type}>{e.name}{e.role ? ` · ${e.role}` : ""}</option>)}
      </select>
      {type && <button type="button" className={off ? "" : "on"} aria-pressed={!off} onClick={() => b?.entry?.pick(off ? 0 : 1)} title={off ? "Effet contourné : toucher pour l'activer" : "Effet actif : toucher pour le contourner"}>{off ? "Contourné" : "Actif"}</button>}
      {!type && suggest && <button type="button" onClick={() => onType(suggest)}>Mettre {effects.find((e) => e.type === suggest)?.name ?? suggest}</button>}
    </span>
  );
}

/** Pistes enregistrées par la source « Pistes choisies » : une touche par piste (la piste elle-même
    n'est jamais enregistrée, pour éviter le larsen). */
function TrackPicks({ channel, addr }: { channel: number; addr: (id: string) => number }) {
  const tracks = useStore((s) => s.tracks.tracks);
  return (
    <div className="ot-tracks" role="group" aria-label="Pistes enregistrées">
      <span className="ot-tracks-title">Pistes enregistrées</span>
      {Array.from({ length: 16 }, (_, i) => (
        <TrackPick key={i} global={addr(`rt${i + 1}`)} index={i} self={i === channel} name={tracks[i]?.instrument ? tracks[i]?.name ?? `Piste ${i + 1}` : ""} color={tracks[i]?.color ?? i} />
      ))}
    </div>
  );
}

function TrackPick({ global, index, self, name, color }: { global: number; index: number; self: boolean; name: string; color: number }) {
  const b = useParam(global >= 0 ? global : null, `Piste ${index + 1}`);
  const on = !self && (b?.normalised ?? 1) >= 0.5;
  return (
    <button type="button" className={`ot-track${on ? " on" : ""}`} disabled={self || !b} aria-pressed={on}
      style={{ "--trk": trackColor(color) } as React.CSSProperties}
      title={self ? "Cette piste (jamais enregistrée par elle-même)" : `${name || "Piste libre"} : ${on ? "enregistrée" : "non enregistrée"}`}
      onClick={() => b?.entry?.pick(on ? 0 : 1)}>
      <b>{index + 1}</b><span>{self ? "ici" : name || "—"}</span>
    </button>
  );
}

/** Les deux machines : une touche chacune. */
function MachineKeys({ global, current }: { global: number; current: number }) {
  const b = useParam(global >= 0 ? global : null, "Machine");
  return (
    <div className="ot-machines" role="radiogroup" aria-label="Machine de la piste">
      {MACHINES.map((name, i) => (
        <button key={name} type="button" role="radio" aria-checked={i === current} className={i === current ? "on" : ""} title={MACHINE_HINTS[i]}
          onClick={() => b?.entry?.pick(i)}>
          <span className="fc-led" aria-hidden />{name}
        </button>
      ))}
    </div>
  );
}

/** v0.42 : touches des 8 pistes internes T1 à T8 — choisir la piste réglée (et jouée au clavier),
    la couper (M) ou l'isoler (S) : coupure et solo de sa ligne, sans toucher aux autres pistes du mixeur. */
function OtTrackKeys({ channel, module: m, T }: { channel: number; module: ModuleDesc; T: number }) {
  const lanes = useStore((s) => s.pattern?.lanes);
  const values = useStore((s) => s.channels[channel]?.inst);
  const samples = useStore((s) => s.channels[channel]?.samples);
  const [clip, setClip] = useState<number | null>(null);
  // v0.43 : remise à l'origine en deux appuis (le premier arme, le second confirme)
  const [armed, setArmed] = useState<number | null>(null); // T, ou -1 : les 8
  useEffect(() => { if (armed === null) return; const id = window.setTimeout(() => setArmed(null), 3000); return () => window.clearTimeout(id); }, [armed]);
  const reset = (lane: number) => {
    if (armed !== lane) { setArmed(lane); showMessage(lane < 0 ? "Encore une fois pour remettre les 8 pistes internes à l'origine" : `Encore une fois pour remettre T${lane + 1} à l'origine`); return; }
    setArmed(null);
    void actions.resetLanes(channel, lane);
  };
  const nameOf = (k: number) => {
    const i = m.params.findIndex((p) => p.id === `t${k + 1}_slot`);
    const choice = i < 0 ? 1 : Math.round(toPlain(m.params[i], values?.[i] ?? 0));
    const o = slotOwner(choice, k);
    return o.index < 0 ? `enreg. T${o.rec + 1}` : samples?.[o.index] ?? "—";
  };
  return (
    <div className="ot-trk" role="tablist" aria-label="Pistes internes">
      {Array.from({ length: 8 }, (_, k) => {
        const on = k === T;
        const ln = lanes?.[k];
        return (
          <div key={k} className="ot-trk-row">
            <button type="button" role="tab" aria-selected={on} className={`ot-trk-key${on ? " on" : ""}${clip === k ? " clip" : ""}`} style={{ "--trk": trackColor(k) } as React.CSSProperties}
              onClick={() => setUi({ lane: k })} title={`T${k + 1} : régler cette piste (et la jouer au clavier)`}>
              <span className="fc-led" aria-hidden /><b>T{k + 1}</b><span>{nameOf(k)}</span>
            </button>
            <button type="button" className={`ot-trk-ms${ln?.mute ? " is-mute" : ""}`} aria-pressed={!!ln?.mute} title={`Couper T${k + 1}`}
              onClick={() => void actions.seq("seqLaneMute", k, !ln?.mute)}>M</button>
            <button type="button" className={`ot-trk-ms${ln?.solo ? " is-solo" : ""}`} aria-pressed={!!ln?.solo} title={`Isoler T${k + 1} (solo parmi les 8)`}
              onClick={() => void actions.seq("seqLaneSolo", k, !ln?.solo)}>S</button>
          </div>
        );
      })}
      {/* copie d'une piste interne sur une autre, comme les touches COPY / PASTE de la machine */}
      <div className="ot-trk-copy ot-trk-copy--2" role="group" aria-label="Copie de piste interne">
        <button type="button" className={clip === T ? "on" : ""} title={`Copier T${T + 1} (réglages, effets, pas, verrous, et ses 16 sons)`}
          onClick={() => { setClip(T); showMessage(`T${T + 1} copiée : choisissez une autre T puis Coller`); }}>Copier</button>
        <button type="button" disabled={clip === null || clip === T}
          title={clip === null ? "Copiez d'abord une T" : `Remplacer T${T + 1} par une copie de T${clip + 1} : ses 16 emplacements reçoivent leurs propres copies des sons (indépendantes de T${clip + 1}) — Ctrl+Z pour revenir`}
          onClick={() => { if (clip !== null) void actions.copyLane(channel, clip, T); }}>Coller</button>
      </div>
      <div className="ot-trk-copy ot-trk-copy--2" role="group" aria-label="Remise à l'origine">
        <button type="button" className={armed === T ? "on" : ""} title={`Remettre les réglages et les 2 effets de T${T + 1} à l'origine (pas et échantillons gardés ; Ctrl+Z pour revenir)`}
          onClick={() => reset(T)}>{armed === T ? "Sûr ?" : `Réinit. T${T + 1}`}</button>
        <button type="button" className={armed === -1 ? "on" : ""} title="Remettre les 8 pistes internes à l'origine (pas et échantillons gardés ; Ctrl+Z pour revenir)"
          onClick={() => reset(-1)}>{armed === -1 ? "Sûr ?" : "Réinit. tout"}</button>
      </div>
    </div>
  );
}

/** Les emplacements — réserve de 128 (16 par piste interne) et les 8 enregistreurs : toucher pour
    jouer, charger, copier / coller, vider. */
// v0.41 : a / b = sélection de l'Éditeur (une partie de l'échantillon) ; sans : tout l'emplacement
let slotClip: { track: number; index: number; a?: number; b?: number } | null = null;
function OtSlots({ channel, T, slot, setSlot, lockStep }: { channel: number; T: number; slot: number; setSlot: (v: number) => void; lockStep: number }) {
  const cur = slot <= 0 ? { bank: 8, k: T } : slot <= 16 ? { bank: T, k: slot - 1 }
    : slot < REC_BASE ? { bank: Math.floor((slot - POOL_BASE) / 16), k: (slot - POOL_BASE) % 16 } : { bank: 8, k: slot - REC_BASE };
  const [bank, setBank] = useState(cur.bank === 8 ? T : cur.bank);
  const samples = useStore((s) => s.channels[channel]?.samples);
  const [sel, setSel] = useState(cur.bank === bank ? Math.max(0, cur.k) : 0);
  const isRec = bank === 8;
  const at = (k: number) => bank * 16 + k;
  const names = isRec ? [] : Array.from({ length: 16 }, (_, k) => samples?.[at(k)] ?? null);
  const choiceOf = (k: number) => (isRec ? (k === T ? 0 : REC_BASE + k) : bank === T ? k + 1 : POOL_BASE + bank * 16 + k);
  const pick = (k: number) => { setSel(k); setSlot(choiceOf(k)); };
  return (
    <div className={`ot-slots${lockStep !== -1 ? " ot-slots--lock" : ""}`}>
      {lockStep !== -1 && <div className="ot-slot-lock" role="status">{lockStep >= 0
        ? `Verrou du pas ${lockStep + 1} : l'emplacement touché sera joué sur ce pas seulement`
        : "Verrou des pas choisis : l'emplacement touché sera joué sur ces pas seulement"}</div>}
      <div className="ot-slot-banks" role="tablist" aria-label="Emplacements de la réserve">
        {Array.from({ length: 8 }, (_, b) => (
          <button key={b} type="button" role="tab" aria-selected={bank === b} className={`${bank === b ? "on" : ""}${b === T ? " own" : ""}`} onClick={() => setBank(b)}
            title={`Emplacements de T${b + 1}${b === T ? " (celle-ci)" : ""}`}>T{b + 1}</button>
        ))}
        <button type="button" role="tab" aria-selected={isRec} className={isRec ? "on" : ""} onClick={() => setBank(8)} title="Les enregistreurs des 8 pistes internes">Enreg.</button>
      </div>
      <div className="ot-slot-grid">
        {Array.from({ length: isRec ? 8 : 16 }, (_, k) => {
          const name = isRec ? `Enregistreur de T${k + 1}` : names[k];
          const playing = isRec ? cur.bank === 8 && cur.k === k : cur.bank === bank && cur.k === k;
          return (
            <button key={k} type="button" className={`ot-slot${playing ? " on" : ""}${name ? " full" : ""}${sel === k && !isRec ? " sel" : ""}`}
              onClick={() => pick(k)} title={name ? `${name} — toucher pour le jouer` : "Emplacement vide — toucher pour le choisir, puis « Charger… »"}
              onDragOver={(e) => { if (!isRec) e.preventDefault(); }}
              onDrop={(e) => { e.preventDefault(); const f = e.dataTransfer.files?.[0]; if (f && !isRec) { void actions.loadSample(channel, at(k), f); pick(k); } }}>
              <b>{isRec ? `R${k + 1}` : k + 1}</b><span>{name ?? "vide"}</span>
            </button>
          );
        })}
      </div>
      {!isRec && (
        <div className="ot-slot-tools">
          <span>T{bank + 1} · {sel + 1} :</span>
          <button type="button" className="ot-key" onClick={() => actions.chooseSample(channel, at(sel))}>Charger…</button>
          <button type="button" className="ot-key" onClick={() => { slotClip = { track: channel, index: at(sel) }; showMessage(`Emplacement T${bank + 1} · ${sel + 1} copié : « Coller » sur un autre`); }} disabled={!names[sel]}>Copier</button>
          <button type="button" className="ot-key" disabled={!slotClip} title={slotClip?.a !== undefined ? "Coller la sélection copiée dans l'Éditeur" : "Coller l'emplacement copié"}
            onClick={() => { const c = slotClip; if (!c) return; if (c.a !== undefined && c.b !== undefined) void actions.copySampleRegion(c.track, c.index, c.a, c.b, channel, at(sel)); else void actions.copySample(c.track, c.index, channel, at(sel)); }}>{slotClip?.a !== undefined ? "Coller la sélection" : "Coller"}</button>
          <button type="button" className="ot-key" disabled={!names[sel]} onClick={() => void actions.clearSample(channel, at(sel))}>Vider</button>
          <small>Glissez un fichier audio sur un emplacement pour l'y charger.</small>
        </div>
      )}
    </div>
  );
}

/** v0.40 : éditeur audio de l'emplacement — sélection à la souris (ou au doigt), puis une opération.
    v0.43 : tête de lecture (la piste qui joue l'échantillon, et l'écoute de la sélection) ; écouter la
    sélection ; l'envoyer directement dans une piste T (premier emplacement libre, joué aussitôt) ;
    fondus d'une durée choisie (sans sélection : depuis le début, ou jusqu'à la fin). */
function OtEditor({ track, index, peaks, seconds, zones, onCopied, module: m, playPos, previewPos }: {
  track: number; index: number; peaks: number[] | null; seconds: number; zones: [number, number][]; onCopied: () => void; module: ModuleDesc; playPos: number; previewPos: number;
}) {
  const [selA, setSelA] = useState(0);
  const [selB, setSelB] = useState(1);
  const [fadeMs, setFadeMs] = useState(200);
  const [sendTo, setSendTo] = useState(-1);
  const samples = useStore((s) => s.channels[track]?.samples);
  const box = useRef<HTMLDivElement>(null);
  const drag = useRef<number | null>(null);
  if (index < 0 || track < 0) return <div className="ot-wave"><div className="sample-empty">L'enregistreur ne s'édite pas : « Garder » le range dans un emplacement.</div></div>;
  const at = (e: React.PointerEvent) => { const r = box.current?.getBoundingClientRect(); return r ? Math.max(0, Math.min(1, (e.clientX - r.left) / r.width)) : 0; };
  const a = Math.min(selA, selB), b = Math.max(selA, selB);
  const whole = a === 0 && b === 1;
  // v0.43.3 : découpes de la vue Lecture, touchables ici (un appui sur une découpe la choisit)
  const zoneAt = (x: number) => zones.findIndex(([p, q]) => x >= p && x < q);
  const zoneSel = zones.findIndex(([p, q]) => Math.abs(p - a) < 1e-4 && Math.abs(q - b) < 1e-4);
  const pickZone = (k: number) => { const z = zones[(k + zones.length) % zones.length]; if (z) { setSelA(z[0]); setSelB(z[1]); } };
  // fondus : la sélection, sinon la durée choisie depuis le début (entrée) ou jusqu'à la fin (sortie)
  const fadeSpan = seconds > 0 ? Math.min(1, fadeMs / 1000 / seconds) : 0.1;
  const fadeRange = (op: number): [number, number] => (whole ? (op === 2 ? [0, fadeSpan] : [1 - fadeSpan, 1]) : [a, b]);
  const ops: [number, string, string][] = [[0, "Inverser", "La sélection jouée à l'envers"], [1, "Normaliser", "Crête de la sélection à -0,3 dB"],
    [2, "Fondu d'entrée", "Sur la sélection (avant elle : silence), ou sans sélection : depuis le début, sur la durée choisie"],
    [3, "Fondu de sortie", "Sur la sélection (après elle : silence), ou sans sélection : jusqu'à la fin, sur la durée choisie"],
    [4, "Rogner", "Ne garder que la sélection"], [5, "Silence", "La sélection devient silencieuse"]];
  const run = (op: number) => { const [x, y] = op === 2 || op === 3 ? fadeRange(op) : [a, b]; void actions.editSample(track, index, op, x, y); };
  // envoi direct : premier emplacement libre de la piste T choisie, qui le joue tout de suite
  const send = async (T: number) => {
    let free = -1;
    for (let k = 0; k < 16 && free < 0; ++k) if (!samples?.[T * 16 + k]) free = T * 16 + k;
    if (free < 0) { showMessage(`T${T + 1} n'a plus d'emplacement libre (16 pleins) : videz-en un, ou passez par Copier / Coller`); return; }
    if (await actions.copySampleRegion(track, index, a, b, track, free)) {
      const i = m.params.findIndex((p) => p.id === `t${T + 1}_slot`);
      if (i >= 0) actions.setParam(paramAddress(track, "inst", 0, i), toNorm(m.params[i], (free % 16) + 1));
    }
  };
  const fmt = (x: number) => (x * seconds).toFixed(2).replace(".", ",");
  return (
    <div className="ot-edit">
      <div ref={box} className="ot-wave ot-wave--edit" style={{ touchAction: "none" }}
        onPointerDown={(e) => { (e.target as HTMLElement).setPointerCapture?.(e.pointerId); const x = at(e); drag.current = x; setSelA(x); setSelB(x); }}
        onPointerMove={(e) => { if (drag.current !== null) setSelB(at(e)); }}
        onPointerUp={() => {
          const x = drag.current;
          drag.current = null;
          if (Math.abs(selB - selA) >= 0.003) return;
          const k = x !== null ? zoneAt(x) : -1;
          if (k >= 0) pickZone(k); else { setSelA(0); setSelB(1); }
        }}>
        <SampleView peaks={peaks} start={whole && zones.length > 0 ? undefined : a} end={whole && zones.length > 0 ? undefined : b} zones={zones} />
        {playPos > 0 && <span className="ot-head" style={{ left: `${playPos * 100}%` }} />}
        {previewPos >= 0 && <span className="ot-head ot-head--prev" style={{ left: `${previewPos * 100}%` }} />}
      </div>
      <div className="ot-edit-tools">
        <span className="ot-edit-sel">{whole ? "Tout l'échantillon" : `Sélection ${fmt(a)} s → ${fmt(b)} s (${fmt(b - a)} s)`}</span>
        <button type="button" className="ot-key ot-key--accent" disabled={!peaks} title="Écouter la sélection (ou tout l'échantillon)"
          onClick={() => actions.previewSample(track, previewPos >= 0 ? -1 : index, a, b)}>{previewPos >= 0 ? "■ Arrêter" : "▶ Écouter"}</button>
        {ops.map(([op, l, h]) => <button key={op} type="button" className="ot-key" title={h} disabled={!peaks} onClick={() => run(op)}>{l}</button>)}
        <button type="button" className="ot-key" onClick={() => { setSelA(0); setSelB(1); }}>Tout</button>
        {zones.length > 0 && (
          <span className="ot-edit-zones" role="group" aria-label="Découpes">
            <button type="button" className="ot-key" aria-label="Découpe précédente" onClick={() => pickZone(zoneSel < 0 ? zones.length - 1 : zoneSel - 1)}>◀</button>
            <b className="tabular">{zoneSel >= 0 ? `Découpe ${zoneSel + 1}/${zones.length}` : `${zones.length} découpes`}</b>
            <button type="button" className="ot-key" aria-label="Découpe suivante" onClick={() => pickZone(zoneSel < 0 ? 0 : zoneSel + 1)}>▶</button>
          </span>
        )}
      </div>
      <div className="ot-edit-tools">
        <label className="ot-edit-fade" title="Durée des fondus écrits dans le fichier quand rien n'est sélectionné (les fondus de lecture, sans toucher au fichier, sont dans Attributs)">
          Fondus fichier <input type="range" min={5} max={4000} step={5} value={fadeMs} onChange={(e) => setFadeMs(Number(e.target.value))} />
          <b>{fadeMs < 1000 ? `${fadeMs} ms` : `${(fadeMs / 1000).toFixed(2).replace(".", ",")} s`}</b>
        </label>
        <span className="ot-edit-send">
          <select aria-label="Envoyer la sélection vers une piste" value={sendTo} onChange={(e) => setSendTo(Number(e.target.value))}>
            <option value={-1}>Envoyer la sélection vers…</option>
            {Array.from({ length: 8 }, (_, k) => <option key={k} value={k}>T{k + 1}</option>)}
          </select>
          <button type="button" className="ot-key ot-key--accent" disabled={!peaks || sendTo < 0} onClick={() => void send(sendTo)}
            title="Copie la sélection dans le premier emplacement libre de cette T, qui la joue tout de suite">Envoyer</button>
        </span>
        <button type="button" className="ot-key" disabled={!peaks} title="Copier la sélection, puis la coller dans un emplacement de T1 à T8 (vue Emplacements)"
          onClick={() => { slotClip = { track, index, a, b }; showMessage(`Sélection copiée (${fmt(b - a)} s) : choisissez une piste T et un emplacement, puis « Coller la sélection »`); onCopied(); }}>Copier la sélection →</button>
      </div>
      <small className="ot-edit-hint">Glisser sur la forme d'onde pour choisir{zones.length > 0 ? " · toucher une découpe pour la choisir (◀ ▶ pour passer à la voisine)" : ""} · Ctrl+Z pour revenir · ces fondus modifient le fichier ; les fondus de lecture (fichier intact) sont dans Attributs</small>
    </div>
  );
}
