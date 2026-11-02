// Groovebox — OFAD — AGPL-3.0-or-later
// Façade d'instrument : l'instrument de la piste ouvert en grand, comme la fenêtre d'un synthé
// logiciel. Tous ses réglages d'un coup, en modules (oscillateurs, filtre, enveloppes…), chacun
// avec son graphique ; page Modulation (LFO de l'instrument et modulations du son de la piste) ;
// clavier jouable en bas (pads pour les batteries). Les boutons se comportent comme les encodeurs :
// glisser, molette, double-clic, clic droit / appui long (valeur exacte, modulation, MIDI),
// verrous de pas et scènes. Échap ou ✕ : refermer.

import { StepsDock } from "./faces/StepsChoice";
import { InstrumentPicker } from "./InstrumentPicker";
import { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { actions, getState, hasNative, setUi, showMessage, useStore, writePref } from "../engine/store";
import type { ModuleDesc, SectionDesc } from "../engine/protocol";
import { noteName, toPlain, trackColor } from "../engine/params";
import { paramAddress, useParam } from "../controls/bindings";
import { Encoder } from "../controls/Encoder";
import { PresetPanel } from "../controls/PresetPanel";
import { FolderBrowser } from "../controls/FolderBrowser";
import { ModSources, SectionVisual, TAB_LABELS, moduleTabs, sectionsFor, useModSourceEncoders } from "./ModuleView";
import { ModeBar } from "./ScreenFrame";
import { MacroPage } from "./MacroPage";
import { FACE_LAYOUTS } from "./faces/layouts";
import { PanelPage, Wheels } from "./faces/SynthPanel";
import { faceSkinStyle } from "../skin/skins";

/** Couleurs de façade par instrument (dessin propre, aucune reprise d'une façade existante). */
const FACES: Record<string, { accent: string; panel: string; cheeks?: boolean; tag: string }> = {
  drumA:   { accent: "oklch(0.72 0.17 30)",  panel: "oklch(0.2 0.012 30)",  tag: "Batterie analogique à machines" },
  drum80:  { accent: "oklch(0.78 0.15 65)",  panel: "oklch(0.22 0.01 70)",  tag: "Boîte analogique du début des années 80" },
  drum90:  { accent: "oklch(0.78 0.14 150)", panel: "oklch(0.2 0.01 160)",  tag: "Boîte hybride de la fin des années 80" },
  acid:    { accent: "oklch(0.86 0.15 100)", panel: "oklch(0.3 0.006 250)", tag: "Basse" },
  mono:    { accent: "oklch(0.8 0.14 75)",   panel: "oklch(0.17 0.006 60)", cheeks: true, tag: "Basse monophonique" },
  poly5:   { accent: "oklch(0.72 0.17 35)",  panel: "oklch(0.18 0.008 40)", cheeks: true, tag: "Polyphonique analogique" },
  polyj:   { accent: "oklch(0.76 0.13 230)", panel: "oklch(0.24 0.008 240)", tag: "Polyphonique à chorus" },
  hybrid:  { accent: "oklch(0.78 0.14 180)", panel: "oklch(0.19 0.014 220)", tag: "Polyphonique hybride" },
  morph:   { accent: "oklch(0.74 0.16 300)", panel: "oklch(0.18 0.02 290)", tag: "Morphing d'ondes" },
  pads:    { accent: "oklch(0.8 0.12 210)",  panel: "oklch(0.2 0.02 230)",  tag: "Nappes" },
  sampler: { accent: "oklch(0.78 0.15 55)",  panel: "oklch(0.2 0.01 50)",   tag: "Échantillonneur" },
  keys:    { accent: "oklch(0.78 0.15 55)",  panel: "oklch(0.2 0.01 50)",   tag: "Échantillonneur" },
  sp:      { accent: "oklch(0.82 0.14 140)", panel: "oklch(0.17 0.008 150)", tag: "Sampler à pads" },
  octa:    { accent: "oklch(0.8 0.15 65)",   panel: "oklch(0.17 0.004 250)", tag: "Sampler de performance" },
  input:   { accent: "oklch(0.78 0.02 250)", panel: "oklch(0.2 0.006 250)", tag: "Entrée audio" },
  ms20:    { accent: "oklch(0.92 0.005 250)", panel: "oklch(0.13 0.004 250)", tag: "Mono semi-modulaire" },
  sh101:   { accent: "oklch(0.72 0.16 30)",  panel: "oklch(0.36 0.005 250)", tag: "Mono à mélangeur" },
  blo:     { accent: "oklch(0.82 0.1 230)",  panel: "oklch(0.42 0.006 250)", tag: "Poly à tables d'ondes" },
  ob6:     { accent: "oklch(0.92 0.01 90)",  panel: "oklch(0.12 0.003 250)", cheeks: true, tag: "Poly à filtre d'état" },
  orbit:   { accent: "oklch(0.78 0.15 230)", panel: "oklch(0.15 0.012 260)", tag: "Synthèse vectorielle à orbite" },
  fm6:     { accent: "oklch(0.8 0.12 175)",  panel: "oklch(0.16 0.008 40)",  cheeks: true, tag: "FM à 6 opérateurs" },
  fm4:     { accent: "oklch(0.82 0.15 300)", panel: "oklch(0.17 0.012 290)", tag: "FM à 4 opérateurs" },
  fmx:     { accent: "oklch(0.8 0.16 55)",   panel: "oklch(0.16 0.01 50)",   tag: "FM à 6 opérateurs, matrice libre" },
  hots:    { accent: "oklch(0.72 0.2 30)",   panel: "oklch(0.17 0.02 25)",   tag: "Hybride à deux moteurs" },
  lyra:    { accent: "oklch(0.78 0.13 55)",  panel: "oklch(0.2 0.02 60)",    cheeks: true, tag: "8 voix organiques" },
  bitm:    { accent: "oklch(0.82 0.17 140)", panel: "oklch(0.15 0.015 150)", tag: "Synthèse par masques de bits" },
  muse:    { accent: "oklch(0.82 0.13 70)",  panel: "oklch(0.18 0.012 60)",  cheeks: true, tag: "Poly analogique 8 voix" },
  drumImp: { accent: "oklch(0.74 0.18 45)",  panel: "oklch(0.2 0.006 260)",  tag: "Boîte compacte et agressive" },
  drumNrv: { accent: "oklch(0.8 0.15 165)",  panel: "oklch(0.16 0.01 200)",  tag: "Batterie à échantillons" },
  dfam:    { accent: "oklch(0.93 0.01 90)",  panel: "oklch(0.11 0.003 250)", cheeks: true, tag: "Percussion à séquenceur" },
  voc16:   { accent: "oklch(0.82 0.13 75)",  panel: "oklch(0.16 0.006 70)",  tag: "Vocodeur analogique de laboratoire" },
  vocstr:  { accent: "oklch(0.78 0.13 350)", panel: "oklch(0.3 0.01 20)",    tag: "Clavier cordes et vocodeur" },
  vocspec: { accent: "oklch(0.76 0.17 55)",  panel: "oklch(0.19 0.01 250)",  tag: "Vocodeur à algorithmes" },
  vocsyn:  { accent: "oklch(0.8 0.15 160)",  panel: "oklch(0.2 0.03 260)",   tag: "Processeur vocal à modules" },
};

/** Visuels utiles en façade (les autres sections n'ont que leurs boutons). */
const FACE_VISUALS = /^(env|amp|filter|osc|wave|zone|sample|padfilter|padamp|drumpad)/;

/** Titre d'un module : les sections par pad portent le nom du pad, la façade dit leur rôle. */
const titleOf = (s: SectionDesc) => (s.visual.startsWith("drumpad") ? "Voix du pad" : s.visual === "zone" ? "Zone"
  : ["padfilter", "padamp", "padfx"].includes(s.tab) ? TAB_LABELS[s.tab] ?? s.name : s.name);

/** Bouton de façade pour un réglage (une liaison par bouton : hooks stables). */
function Knob({ global, dim }: { global: number; dim?: boolean }) {
  const b = useParam(global);
  return <div className={dim ? "knob-dim" : undefined} title={dim ? "Vitesse donnée par la synchro au tempo" : undefined}><Encoder binding={b} variant="knob" /></div>;
}

/** Un module de la façade : titre, graphique, boutons sur 1 ou 2 rangées. */
function FaceModule({ channel, module: m, section, lane }: { channel: number; module: ModuleDesc; section: SectionDesc; lane: number }) {
  const n = section.params.length;
  const cols = n <= 4 ? n : Math.ceil(n / 2);
  const visual = FACE_VISUALS.test(section.visual);
  // LFO synchronisé au tempo : sa vitesse en Hz ne sert plus, son bouton s'efface
  const inst = useStore((s) => s.channels[channel]?.inst);
  const synced = new Set<string>();
  for (const k of section.params) {
    const id = m.params[k].id;
    if (/sync$/.test(id) && m.params[k].choices && Math.floor((inst?.[k] ?? 0) * m.params[k].choices!.length) > 0) synced.add(id.replace(/sync$/, "rate"));
  }
  return (
    <section className="face-module" style={{ "--cols": cols } as React.CSSProperties}>
      <h3 className="face-module-title">{titleOf(section)}</h3>
      {visual && (
        <div className="face-visual">
          <SectionVisual channel={channel} area="inst" slot={0} module={m} section={section} lane={lane} />
        </div>
      )}
      <div className="face-knobs">
        {section.params.map((k) => <Knob key={k} global={paramAddress(channel, "inst", 0, k)} dim={synced.has(m.params[k].id)} />)}
      </div>
    </section>
  );
}

/** Clavier : 2 octaves et demie ; la hauteur de l'appui donne la vélocité ; glisser d'une
    touche à l'autre rejoue (legato) ; plusieurs doigts à la fois. */
function Keyboard({ track, color }: { track: number; color: string }) {
  const [octave, setOctave] = useState(3);
  const [ownHeld, setHeld] = useState<number[]>([]);
  // touches allumées : jouées ici, au contrôleur MIDI, par la séquence ou l'arpège
  const litNotes = useStore((s) => s.litNotes);
  const held = useMemo(() => [...ownHeld, ...litNotes.flatMap((v, n) => (v > 0 ? [n] : []))], [ownHeld, litNotes]);
  const byPointer = useRef(new Map<number, number>());
  const base = octave * 12;
  const keys = Array.from({ length: 30 }, (_, i) => base + i).filter((n) => n <= 127);
  const isBlack = (n: number) => [1, 3, 6, 8, 10].includes(n % 12);
  const whites = keys.filter((n) => !isBlack(n));
  const velocityAt = (e: React.PointerEvent<HTMLElement>) => {
    const r = e.currentTarget.getBoundingClientRect();
    return Math.min(1, Math.max(0.15, (e.clientY - r.top) / Math.max(1, r.height)));
  };
  const press = (id: number, n: number, v: number) => {
    const prev = byPointer.current.get(id);
    if (prev === n) return;
    if (prev !== undefined) actions.noteOff(track, prev);
    byPointer.current.set(id, n);
    actions.noteOn(track, n, v);
    setHeld([...byPointer.current.values()]);
  };
  const release = (id: number) => {
    const prev = byPointer.current.get(id);
    if (prev === undefined) return;
    actions.noteOff(track, prev);
    byPointer.current.delete(id);
    setHeld([...byPointer.current.values()]);
  };
  useEffect(() => () => { for (const n of byPointer.current.values()) actions.noteOff(track, n); byPointer.current.clear(); }, [track]);
  const keyProps = (n: number) => ({
    onPointerDown: (e: React.PointerEvent<HTMLElement>) => { e.preventDefault(); e.currentTarget.releasePointerCapture?.(e.pointerId); press(e.pointerId, n, velocityAt(e)); },
    onPointerEnter: (e: React.PointerEvent<HTMLElement>) => { if (byPointer.current.has(e.pointerId)) press(e.pointerId, n, velocityAt(e)); },
    onPointerUp: (e: React.PointerEvent<HTMLElement>) => release(e.pointerId),
    onPointerCancel: (e: React.PointerEvent<HTMLElement>) => release(e.pointerId),
  });
  return (
    <div className="face-keys" style={{ "--key": color } as React.CSSProperties} onPointerLeave={(e) => { if (!e.buttons) release(e.pointerId); }}
      onPointerUp={(e) => release(e.pointerId)}>
      <div className="face-oct">
        <button type="button" onClick={() => setOctave(Math.min(8, octave + 1))} aria-label="Octave plus haut">+</button>
        <span>{noteName(base)}</span>
        <button type="button" onClick={() => setOctave(Math.max(0, octave - 1))} aria-label="Octave plus bas">−</button>
      </div>
      <div className="keybed" style={{ "--whites": whites.length } as React.CSSProperties}>
        {whites.map((n) => (
          <button key={n} type="button" className={`key key--white${held.includes(n) ? " is-down" : ""}`} aria-label={noteName(n)} {...keyProps(n)}>
            {n % 12 === 0 && <span>{noteName(n)}</span>}
          </button>
        ))}
        {keys.filter(isBlack).map((n) => {
          const left = whites.filter((w) => w < n).length;
          return (
            <button key={n} type="button" className={`key key--black${held.includes(n) ? " is-down" : ""}`} aria-label={noteName(n)}
              style={{ left: `calc(${left} * 100% / var(--whites) - 100% / var(--whites) * 0.32)` }} {...keyProps(n)} />
          );
        })}
      </div>
    </div>
  );
}

/** Batteries : 16 pads en bas de la façade ; toucher joue le pad et le choisit. */
/** Changer de synthé (piste) sans quitter la façade, et couper / isoler la piste. */
function TrackNav({ focus }: { focus: number }) {
  const tracks = useStore((s) => s.tracks.tracks);
  const used = tracks.map((t, i) => (t?.instrument ? i : -1)).filter((i) => i >= 0);
  const pos = used.indexOf(focus);
  const go = (d: number) => {
    if (!used.length) return;
    const i = pos < 0 ? 0 : (pos + d + used.length) % used.length;
    if (used[i] !== focus) void actions.focus(used[i]);
  };
  const t = tracks[focus];
  return (
    <div className="face-tracknav" role="group" aria-label="Piste de la façade">
      <button type="button" onClick={() => go(-1)} disabled={used.length < 2} aria-label="Synthé de la piste précédente">◀</button>
      <select value={focus} onChange={(e) => void actions.focus(Number(e.target.value))} aria-label="Piste affichée">
        {used.map((i) => <option key={i} value={i}>P{i + 1} · {tracks[i].name}</option>)}
      </select>
      <button type="button" onClick={() => go(1)} disabled={used.length < 2} aria-label="Synthé de la piste suivante">▶</button>
      <button type="button" className={`face-ms${t?.mute ? " is-mute" : ""}`} onClick={() => void actions.trackMute(focus, !t?.mute)}
        aria-pressed={!!t?.mute} title="Couper la piste">M</button>
      <button type="button" className={`face-ms${t?.solo ? " is-solo" : ""}`} onClick={() => void actions.trackSolo(focus, !t?.solo)}
        aria-pressed={!!t?.solo} title="Isoler la piste (solo)">S</button>
    </div>
  );
}

function PadStrip({ track, module: m, lane }: { track: number; module: ModuleDesc; lane: number }) {
  const lit = useStore((s) => s.litLanes);
  return (
    <div className="face-pads">
      {Array.from({ length: 16 }, (_, i) => (
        <button key={i} type="button" className={`face-pad kind--${m.padKinds[i] ?? "sample"}${i === lane ? " is-selected" : ""}${(lit[i] ?? 0) > 0 ? " is-lit" : ""}`}
          onPointerDown={(e) => { e.preventDefault(); e.currentTarget.setPointerCapture(e.pointerId); setUi({ lane: i }); const r = e.currentTarget.getBoundingClientRect(); actions.noteOn(track, i, Math.min(1, Math.max(0.2, (r.bottom - e.clientY) / r.height + 0.2))); }}
          onPointerUp={() => actions.noteOff(track, i)} onPointerCancel={() => actions.noteOff(track, i)}>
          <b>{i + 1}</b><span>{m.padNames[i]}</span>
        </button>
      ))}
    </div>
  );
}

/** Batteries : vue d'ensemble du kit, comme une table de mixage : pour chacun des 16 pads,
    accord, déclin et niveau (le pad choisi est détaillé au-dessus). */
function KitOverview({ channel, module: m, lane }: { channel: number; module: ModuleDesc; lane: number }) {
  const pick = (sec: SectionDesc | undefined, re: RegExp, fallback: number) =>
    sec ? sec.params.find((k) => re.test(m.params[k].name)) ?? sec.params[fallback] : undefined;
  return (
    <section className="face-module face-kit">
      <h3 className="face-module-title">Le kit</h3>
      <div className="face-kit-cols">
        {Array.from({ length: 16 }, (_, i) => {
          const voice = sectionsFor(m, "pad", i)[0];
          const amp = sectionsFor(m, "padamp", i)[0];
          const knobs = [pick(voice, /^(Accord|Hauteur)/, 0), pick(voice, /^Déclin/, 1), pick(amp, /^Niveau/, 3)].filter((k): k is number => k !== undefined);
          return (
            <div key={i} className={`face-kit-col kind--${m.padKinds[i] ?? "sample"}${i === lane ? " is-selected" : ""}`}>
              <button type="button" className="face-kit-name" onClick={() => setUi({ lane: i })} title={`Détailler le pad ${i + 1}`}>
                <b>{i + 1}</b> {m.padNames[i]}
              </button>
              {knobs.map((k) => <Knob key={k} global={paramAddress(channel, "inst", 0, k)} />)}
            </div>
          );
        })}
      </div>
    </section>
  );
}

/** Page Modulation : sections de modulation de l'instrument, puis les modulations du son de la piste. */
function ModPage({ channel, module: m, lane, trackOnly }: { channel: number; module: ModuleDesc; lane: number; trackOnly?: boolean }) {
  const own = trackOnly ? [] : m.sections.filter((s) => s.tab === "mod");
  const srcKnobs = useModSourceEncoders(channel, "sound");
  return (
    <div className="face-modpage">
      {own.length > 0 && <div className="face-col">{own.map((s) => <FaceModule key={s.id} channel={channel} module={m} section={s} lane={lane} />)}</div>}
      <section className="face-module face-module--sources">
        <h3 className="face-module-title">Modulations du son de la piste</h3>
        <div className="face-sources"><ModSources channel={channel} scope="sound" /></div>
        <div className="face-knobs face-knobs--src">
          {srcKnobs.map((b, i) => <Encoder key={i} binding={b} variant="knob" />)}
        </div>
      </section>
    </div>
  );
}

/** Agrandit le contenu de la façade pour remplir la place libre (plein écran, grande fenêtre) :
    boutons et inscriptions plus grands, jusqu'à 1,8 fois. Le contenu est remis en page à la largeur
    disponible divisée par l'agrandissement, puis agrandi d'une transformation (les menus des
    boutons tiennent déjà compte des transformations). */
function FitScale({ children, fitKey }: { children: React.ReactNode; fitKey: string }) {
  const outer = useRef<HTMLDivElement>(null);
  const inner = useRef<HTMLDivElement>(null);
  const [fit, setFit] = useState({ s: 1, w: 0 });
  useLayoutEffect(() => {
    const o = outer.current, i = inner.current;
    if (!o || !i) return;
    let raf = 0;
    const measure = () => {
      const W = o.clientWidth, H = o.clientHeight;
      if (W <= 0 || H <= 0) return;
      // plus grand agrandissement dont le contenu remis en page tient en hauteur ET en largeur
      const fits = (s: number) => {
        i.style.width = `${W / s}px`;
        return i.offsetHeight * s <= H + 1 && i.scrollWidth <= W / s + 1;
      };
      let lo = 1, hi = 1.8;
      if (! fits (1)) { lo = 1; hi = 1; }
      else if (fits (hi)) lo = hi;
      else for (let k = 0; k < 7; ++k) { const mid = (lo + hi) / 2; if (fits (mid)) lo = mid; else hi = mid; }
      const s = Math.floor (lo * 50) / 50;
      i.style.width = `${W / s}px`;
      setFit ((f) => (f.s === s && f.w === W ? f : { s, w: W }));
    };
    const schedule = () => { cancelAnimationFrame (raf); raf = requestAnimationFrame (measure); };
    measure();
    const ro = new ResizeObserver (schedule);
    ro.observe (o);
    ro.observe (i); // contenu qui change sans changer de page (machine d'un pad, rangées ajoutées)
    return () => { ro.disconnect(); cancelAnimationFrame (raf); };
  }, [fitKey]);
  return (
    <div ref={outer} className="face-fit">
      <div ref={inner} className="face-fit-inner" style={{ width: fit.w ? fit.w / fit.s : undefined, transform: fit.s !== 1 ? `scale(${fit.s})` : undefined }}>
        {children}
      </div>
    </div>
  );
}

export function InstrumentFace() {
  const open = useStore((s) => s.ui.face);
  const focus = useStore((s) => s.focus);
  const tracks = useStore((s) => s.tracks.tracks);
  const cat = useStore((s) => s.catalog);
  const lane = useStore((s) => s.ui.lane);
  const faceBig = useStore((s) => s.ui.faceBig);
  const samples = useStore((s) => s.channels[focus]?.samples);
  const instValues = useStore((s) => s.channels[focus]?.inst);
  const [page, setPage] = useState<string>("sound");
  const [presets, setPresets] = useState<null | "inst" | "pad">(null);
  const [picker, setPicker] = useState(false); // v0.32 : choix de l'instrument par les images des façades
  const [library, setLibrary] = useState(false);
  const track = focus < (cat?.layout.numTracks ?? 16) ? tracks[focus] : undefined;
  const m = cat?.instruments.find((i) => i.type === track?.instrument) ?? null;

  useEffect(() => {
    if (!open) return;
    // le pédalier ouvert par-dessus (bouton Effets) se referme d'abord : Échap le laisse faire
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape" && !e.defaultPrevented && !getState().ui.pedals) { e.preventDefault(); setUi({ face: false }); } };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  }, [open]);
  useEffect(() => { if (open && !m) { setUi({ face: false }); showMessage("Choisissez d'abord une piste qui a un instrument"); } }, [open, m]);

  if (!open || !m || !track) return null;
  const face = FACES[m.type] ?? FACES.input;
  // panneau « comme la machine » si l'instrument en a un, sinon la façade générique
  const layout = FACE_LAYOUTS[m.type];
  // synthés mélodiques : page « Pas » (rangée de pas façon basse acide, même séquence que l'écran Séquence)
  const stepsPage = layout && !m.drumKit && !["acid", "octa"].includes(m.type)
    ? [{ id: "steps", label: "Pas", rows: [[{ title: "Pas de la séquence (ligne 1) · note, octave, vélocité, durée, accent, glissé", cols: 1, grow: 1, ctl: [], custom: "steps303" }]] }] : [];
  const facePages = layout ? [...layout.pages, ...stepsPage] : [];
  const pages = layout ? [...facePages.map((p) => ({ id: p.id, label: p.label })), { id: "track", label: "Modulations de la piste" }, { id: "macros", label: "Macros" }]
    : [{ id: "sound", label: "Son" }, { id: "mod", label: "Modulation" }, { id: "macros", label: "Macros" }];
  const current = pages.some((p) => p.id === page) ? page : pages[0].id;
  const layoutPage = facePages.find((p) => p.id === current);
  const sk = faceSkinStyle(layout?.theme ?? "generic"); // v0.35.1 : habillage réaliste (matières précalculées)
  const perPad = m.sections.some((s) => s.visual.startsWith("drumpad") || s.visual === "zone");
  const tabs = moduleTabs(m, "inst").map((t) => t.id).filter((id) => id !== "mod" && id !== "modsrc");
  // page Son : tous les modules, dans l'ordre des onglets (un seul pad ou zone à la fois)
  const modules = tabs.flatMap((t) => sectionsFor(m, t, lane)).filter((s, i, a) => a.indexOf(s) === i);
  const color = trackColor(track.color ?? 0);
  const knobCount = modules.reduce((n, s) => n + s.params.length, 0);
  // v0.42 : Pieuvre 8 — l'emplacement joué par la piste interne choisie (T = ligne) dans la réserve de 128 ;
  // son enregistreur : on charge dans son premier emplacement
  const octa = m.type === "octa";
  const slotIdx = m.params.findIndex((p) => p.id === `t${Math.min(7, lane) + 1}_slot`);
  const octaRaw = octa && slotIdx >= 0 ? Math.round(toPlain(m.params[slotIdx], instValues?.[slotIdx] ?? 0)) : 0;
  const octaSlot = octaRaw >= 1 && octaRaw <= 16 ? Math.min(7, lane) * 16 + octaRaw - 1 : octaRaw >= 17 && octaRaw < 145 ? octaRaw - 17 : Math.min(7, lane) * 16;
  const sampleIndex = m.type === "sampler" || m.type === "keys" ? 0 : m.type === "octa" ? octaSlot : lane;
  const takesSamples = m.drumKit || m.type === "keys" || m.type === "octa";

  return (
    <div className={`face face--skin${sk.skin.cheek ? " face--cheeks" : ""}${layout ? ` face--panel face--t-${layout.theme}` : ""}`} role="dialog" aria-label={`${m.name} — façade de l'instrument`}
      style={{ ...(layout ? { "--k": color } : { "--face-accent": face.accent, "--face-panel": face.panel, "--k": color }), ...sk.style } as React.CSSProperties}>
      <header className="face-head">
        <span className="face-stripe" aria-hidden />
        <div className="face-title">
          <span className="face-tag">P{focus + 1} · {track.name} · {face.tag}</span>
          <h2>{m.name}{perPad ? <small> · {lane + 1} {m.padNames[lane] ?? ""}</small> : null}</h2>
        </div>
        <TrackNav focus={focus} />
        <div className="face-pages" role="tablist" aria-label="Pages de la façade">
          {pages.map((p) => (
            <button key={p.id} type="button" role="tab" aria-selected={current === p.id} className={current === p.id ? "on" : ""} onClick={() => setPage(p.id)}>{p.label}</button>
          ))}
        </div>
        <span className="face-spacer" />
        {takesSamples && (
          <span className="face-sample">
            <span>{samples?.[sampleIndex] ?? "Voix de synthèse"}</span>
            <button type="button" onClick={() => (hasNative("chooseSample") ? actions.chooseSample(focus, sampleIndex) : setUi({ face: false, screen: "sound" }))}>Échantillon…</button>
            <button type="button" className={library ? "on" : ""} onClick={() => { setLibrary(!library); setPresets(null); }} title="Les échantillons du dossier choisi dans les Réglages">Bibliothèque</button>
          </span>
        )}
        <button type="button" className={`face-btn face-btn--swap${picker ? " on" : ""}`} onClick={() => { setPicker(!picker); setPresets(null); setLibrary(false); }}
          title="Choisir un autre instrument pour cette piste, d'après l'image de sa façade">⇄ Instrument</button>
        {/* v0.46 fix6 : raccourci vers les effets de la piste (et du pad), en pédales par-dessus la façade ;
            on les referme (✕ ou Échap) pour revenir à l'instrument. L'écran EFFETS reste là. */}
        <button type="button" className="face-btn face-btn--fx" onClick={() => setUi({ pedals: true })}
          title={m.drumKit ? `Les effets de la piste et ceux du ${octa ? `T${lane + 1}` : `pad ${lane + 1}`}, en pédales, sans quitter l'instrument (✕ ou Échap pour revenir)` : "Les effets de cette piste, en pédales, sans quitter l'instrument (✕ ou Échap pour revenir)"}>FX · Effets</button>
        {!octa && <button type="button" className={`face-btn${presets === "inst" ? " on" : ""}`} onClick={() => { setPresets(presets === "inst" ? null : "inst"); setLibrary(false); }}>
          {m.drumKit ? "Kit · préréglages" : "Préréglages"}{track.preset ? ` · ${track.preset}` : ""}
        </button>}
        {m.drumKit && <button type="button" className={`face-btn${presets === "pad" ? " on" : ""}`} onClick={() => { setPresets(presets === "pad" ? null : "pad"); setLibrary(false); }}>{octa ? `T${lane + 1}` : `Pad ${lane + 1}`} · préréglages</button>}
        {layout && <button type="button" className={`face-btn${faceBig ? " on" : ""}`} aria-pressed={faceBig}
          onClick={() => { setUi({ faceBig: !faceBig }); writePref("faceBig", !faceBig); }}
          title={faceBig ? "Grand : une façade trop chargée n'est plus réduite, on la fait défiler vers le bas (toucher pour tout faire tenir dans l'écran)" : "Tout tient dans l'écran, quitte à réduire (toucher pour garder la taille normale et faire défiler)"}>{faceBig ? "Taille : grande" : "Taille : ajustée"}</button>}
        <button type="button" className="face-close" onClick={() => setUi({ face: false })} aria-label="Refermer la façade" title="Refermer (Échap)">✕</button>
      </header>
      <ModeBar />

      {picker && <InstrumentPicker track={focus} full onClose={() => setPicker(false)} />}
      <div className="face-body">
        {layoutPage ? <PanelPage channel={focus} module={m} page={layoutPage} lane={lane} bigText={!!layout?.bigText} /> : (
        <FitScale fitKey={`${m.type}:${current}:${lane}`}>
        {current === "macros" ? <MacroPage />
        : current === "track" ? <ModPage channel={focus} module={m} lane={lane} trackOnly />
        : current === "sound" ? (<>
          <div className={`face-row face-row--wrap${knobCount <= 22 ? " face-row--roomy" : ""}`}>
            {modules.map((s) => <FaceModule key={s.id} channel={focus} module={m} section={s} lane={lane} />)}
          </div>
          {m.drumKit && m.sections.some((x) => x.visual.startsWith("drumpad")) && <KitOverview channel={focus} module={m} lane={lane} />}
        </>
        ) : (
          <ModPage channel={focus} module={m} lane={lane} />
        )}
        </FitScale>)}
        {library && takesSamples && !presets && (
          <div className="face-presets">
            <FolderBrowser kind="samples" title={`Bibliothèque · ${m.drumKit ? `pad ${lane + 1}` : m.type === "octa" ? `emplacement ${sampleIndex + 1}` : "échantillon"}`}
              onPick={(rel) => actions.loadSampleFile(focus, sampleIndex, rel)} onClose={() => setLibrary(false)} />
          </div>
        )}
        {presets && (
          <div className="face-presets">
            <PresetPanel key={`${presets}:${focus}:${presets === "pad" ? lane : ""}`} scope={presets} channel={focus} slot={-1} pad={presets === "pad" ? lane : -1}
              title={presets === "pad" ? (octa ? `Pieuvre 8 · T${lane + 1}` : `Pad ${lane + 1} · ${m.padNames[lane] ?? ""}`) : m.name}
              current={presets === "inst" ? track.preset : undefined}
              factory={presets === "inst" || (octa && presets === "pad") ? m.presets : []} factoryCats={presets === "inst" ? m.presetCats : undefined}
              onPickFactory={(n) => void actions.applyPreset(focus, -1, n)}
              onClose={() => setPresets(null)} />
          </div>
        )}
      </div>
      {stepsPage.length > 0 && current !== "steps" && <StepsDock />}

      <footer className="face-foot">
        {(m.drumKit && !octa) || m.type === "sampler" ? <PadStrip track={focus} module={m} lane={lane} />
          : <div className={layout?.wheels ? "face-play face-play--wheels" : "face-play"}>{layout?.wheels && <Wheels track={focus} />}<Keyboard track={focus} color={layout ? "var(--fp-led)" : face.accent} /></div>}
      </footer>
    </div>
  );
}
