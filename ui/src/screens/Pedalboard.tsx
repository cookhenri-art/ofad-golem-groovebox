// Groovebox — OFAD — AGPL-3.0-or-later
// Pédalier : les 4 inserts du canal choisi vus comme des pédales posées côte à côte, dans l'ordre
// du signal. Chaque pédale : ses boutons principaux, ses sélecteurs, son interrupteur au pied
// (effet actif / contourné) et sa diode. Le boucleur a ses commutateurs (enregistrer, lecture,
// surcouche, arrêt, effacer) ; les effets à pas montrent leur grille de 16 pas. Dessin original :
// aucune reprise de la façade d'une pédale existante. Échap ou ✕ : refermer.

import { EffectPicker } from "./EffectPicker";
import { useEffect, useState } from "react";
import { actions, setUi, useStore } from "../engine/store";
import type { ModuleDesc } from "../engine/protocol";
import { channelName, toPlain, trackColor } from "../engine/params";
import { paramAddress, useParam } from "../controls/bindings";
import { Encoder, ParamMenuZone } from "../controls/Encoder";
import { useBypass } from "../controls/useBypass";
import { moduleSkinStyle, pedalKnobStyle, pedalSkin, rackSkinStyle } from "../skin/skins";
import { ModulePanel, StepCell } from "./ModulePanel";


/** Boutons principaux de chaque pédale (6 au plus) ; « time|division » : selon la synchro. */
const KNOBS: Record<string, string[]> = {
  atmos: ["decay", "size", "predelay", "modulation", "shimmer", "mix"],
  mdelay: ["time|division", "feedback", "mix", "tone", "grit", "moddepth"],
  delay: ["time|division", "feedback", "mix", "saturation", "wobble", "highcut"],
  reverb: ["size", "decay", "predelay", "brightness", "distance", "mix"],
  looper: ["level", "dry", "keep"],
  stepfx: ["cutoff", "reso", "bits", "echofb", "mix"],
  flanger: ["manual", "rate|division", "depth", "feedback"],                                       // v0.36
  phaser2: ["rateA|divisionA", "depthA", "feedbackA", "rateB|divisionB", "depthB", "feedbackB"],
  dimension: ["width", "tone", "mix"],
  tapesat: ["drive", "bump", "treble", "wow", "output"],                                           // v0.38
  tubesat: ["drive", "bias", "sag", "tone", "output", "mix"],
  transient: ["attack", "sustain", "output", "mix"],
  chorus3: ["rate|division", "depth", "width", "tone", "mix"],
  // v0.45 : vitesse libre ou division du tempo selon l'interrupteur Synchro
  chorus: ["rate|division", "depth", "delay", "feedback", "tone", "mix"],
  phaser: ["rate|division", "depth", "center", "feedback", "stereo", "mix"],
  filter: ["cutoff", "reso", "rate|division", "lfodepth", "envdepth", "mix"],
  tapeecho: ["speed|division", "intensity", "echo", "bass", "treble", "mix"],
};
const SWITCHES: Record<string, string[]> = {
  atmos: ["mode", "freeze", "persist"], mdelay: ["mode", "sync"], delay: ["mode", "sync"], reverb: ["freeze"],
  looper: ["length", "start", "speed", "reverse"], stepfx: ["division", "repeat", "steps"],
  flanger: ["mode", "sync"], phaser2: ["routing", "sync"], dimension: ["mode"],
  tapesat: ["speed"], tubesat: [], transient: ["speed", "clip"], chorus3: ["mode", "sync"],
  chorus: ["voices", "sync"], phaser: ["stages", "sync"], filter: ["type", "sync"], tapeecho: ["heads", "sync"],
};

const LOOPER_STATES = ["Vide", "Attend la mesure", "Enregistre", "Lecture", "Surcouche", "Arrêtée"];

/** Bouton réel (v0.35.1) : corps précalculé, repère qui tourne ; nom sérigraphié dessous ; la valeur
    apparaît au survol ou en tournant. small : sélecteur à crans (petit bouton, position écrite dessous). */
function Knob({ global, type, index, small = false, center = false, label, onGlobal = -1 }: { global: number; type?: string; index?: number; small?: boolean; center?: boolean; label?: string; onGlobal?: number }) {
  const b = useParam(global >= 0 ? global : null, label);
  const sw = useParam(onGlobal >= 0 ? onGlobal : null);
  const style = type !== undefined && index !== undefined ? pedalKnobStyle(type, index) : undefined;
  // v0.46 : réglage d'un effet qui a son interrupteur (Sculptor) : le toucher allume l'effet
  const off = !!sw?.entry && Math.round(sw.entry.plain) === 0;
  const wake = off ? () => sw?.entry?.pick(1) : undefined;
  return (
    <div className={`pd-knob${small ? " pd-knob--small" : ""}${center ? " pd-knob--center" : ""}${off ? " pd-knob--off" : ""}`} style={style}
      onPointerDownCapture={wake} onWheelCapture={wake} title={off ? `${b?.label ?? ""} : effet éteint, touchez pour l'allumer` : undefined}>
      <Encoder binding={b} variant="knob" />
      <span className="pd-lab">{b?.label ?? ""}</span>
      {small && <span className="pd-pos">{b?.text ?? ""}</span>}
    </div>
  );
}

function knobIds(m: ModuleDesc, values: number[]): string[] {
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const curated = KNOBS[m.type];
  const list = curated ?? m.sections.flatMap((s) => s.params).filter((k, i, a) => a.indexOf(k) === i && !m.params[k]?.choices?.length).slice(0, 6).map((k) => m.params[k].id);
  return list.map((id) => {
    if (!id.includes("|")) return id;
    const [free, synced] = id.split("|");
    const si = idx("sync");
    return si >= 0 && toPlain(m.params[si], values[si] ?? 0) > 0.5 ? synced : free;
  }).filter((id) => idx(id) >= 0);
}

function switchIds(m: ModuleDesc): string[] {
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const curated = SWITCHES[m.type];
  return (curated ?? m.sections[0]?.params.filter((k) => (m.params[k]?.choices?.length ?? 0) > 1).slice(0, 2).map((k) => m.params[k].id) ?? []).filter((id) => idx(id) >= 0);
}

function LooperKeys({ global, state }: { global: number; state: number }) {
  const b = useParam(global >= 0 ? global : null);
  const current = Math.round((b?.normalised ?? 0) * 4);
  // l'action est un réglage : la même valeur deux fois ne ferait rien, on passe par « Arrêt » d'abord
  const press = (v: number) => {
    if (v === current && v !== 0) { b?.entry?.pick(0); window.setTimeout(() => b?.entry?.pick(v), 60); }
    else b?.entry?.pick(v);
  };
  const keys: [string, number][] = [["Enregistrer", 1], ["Lecture", 2], ["Surcouche", 3], ["Arrêt", 0], ["Effacer", 4]];
  return (
    <div className="pd-loop-keys">
      {keys.map(([label, v]) => (
        <button key={label} type="button" className={`pd-loop-key${(v === 1 && state === 2) || (v === 2 && state === 3) || (v === 3 && state === 4) ? " on" : ""}`}
          onClick={() => press(v === 1 && state === 2 ? 2 : v === 3 && state === 4 ? 2 : v)}>{label}</button>
      ))}
    </div>
  );
}

/** Un effet du pédalier : en pédale s'il a son boîtier illustré, sinon en rack (v0.35.1). */
function Pedal(props: { channel: number; area?: "fx" | "pfx"; slot: number; label: string; m: ModuleDesc; values: number[]; bypassed: boolean; meter: number[]; onSwap?: () => void; onOpen?: () => void }) {
  return props.m.form === "pedal" && pedalSkin(props.m.type) ? <PedalUnit {...props} /> : <RackUnit {...props} />;
}

/** Modules (v0.35.2) : dans le pédalier, une unité compacte (les réglages principaux) et le bouton
    qui ouvre l'interface dédiée. « nom:libellé » : libellé imposé. */
const MODULE_KNOBS: Record<string, { big: string[]; small: string[] }> = {
  dist:   { big: ["driveA:Drive A", "driveB:Drive B"], small: ["toneA:Tonalité A", "toneB:Tonalité B", "mix", "out"] },
  // v0.46 : Largeur ajoutée ; tourner la quantité d'un effet éteint l'allume (sNamt → sNon)
  sculptor: { big: ["mix:Mélange", "smooth:Lissage"], small: ["s7amt:Volume", "s4amt:Filtre", "s1amt:Temps", "s3amt:Broyage", "s8amt:Panoramique", "s9amt:Largeur"] }, // v0.37
  grinder: { big: ["input:Entrée", "mix:Mélange"], small: ["b1drive:Drive grave", "b2drive:Drive bas-méd.", "b3drive:Drive ht-méd.", "b4drive:Drive aigu"] },
  master: { big: ["drive:Gain", "ceiling"], small: ["lowGain", "airGain", "shine:Éclat", "warmth:Chaleur"] },
  eq:     { big: ["output"], small: [1, 2, 3, 4, 5, 6, 7, 8].map((b) => `b${b}_gain:Bande ${b}`) },
  mband:  { big: ["mix:Mélange", "out:Sortie"], small: ["b1thr:Seuil grave", "b2thr:Seuil bas-méd.", "b3thr:Seuil ht-méd.", "b4thr:Seuil aigu"] },
  stepfx: { big: ["mix", "steps"], small: ["cutoff", "reso", "bits", "echofb"] },
  atmos:  { big: ["decay", "mix"], small: ["size", "predelay", "modulation", "shimmer"] },
};

/** Pédale (insert du canal : area « fx », slot 0..3 ; insert de pad : « pfx », slot = pad × 2 + n).
    Disposition des boîtiers du pack : nom dans le bandeau du haut, 6 boutons (2 colonnes × 3), les
    sélecteurs en petits boutons (une rangée de plus sur les boîtiers hauts), diode, vis, contacteur. */
function PedalUnit({ channel, area = "fx", slot, label, m, values, meter, onSwap }: { channel: number; area?: "fx" | "pfx"; slot: number; label: string; m: ModuleDesc; values: number[]; bypassed: boolean; meter: number[]; onSwap?: () => void }) {
  // v0.47 : contacteur assignable (MIDI, bouton assignable, scènes) : voir useBypass
  const { binding: bypBinding, bypassed, toggle } = useBypass(channel, area, slot, m.name);
  const addr = (id: string) => { const i = m.params.findIndex((p) => p.id === id); return i < 0 ? -1 : paramAddress(channel, area, slot, i); };
  const sk = pedalSkin(m.type)!;
  const sw = switchIds(m).slice(0, 2);
  const knobs = knobIds(m, values).slice(0, sk.tall ? 6 : 6 - sw.length);
  const odd = (knobs.length + (sk.tall ? 0 : sw.length)) % 2 === 1; // la dernière commande, seule sur sa rangée, est centrée
  const loopState = Math.floor(meter[2] ?? 0), loopPhase = (meter[2] ?? 0) - loopState;
  return (
    <div className={`pd pd--skin sk-knobs${sk.tall ? " pd--tall" : ""}${bypassed ? " pd--off" : ""} pd--${m.type}`} style={sk.style}>
      <span className="pd-screw pd-screw--tl" aria-hidden /><span className="pd-screw pd-screw--tr" aria-hidden />
      <span className="pd-screw pd-screw--bl" aria-hidden /><span className="pd-screw pd-screw--br" aria-hidden />
      <span className="pd-led" aria-hidden />
      <span className="pd-n">{label}</span>
      {onSwap && <button type="button" className="pd-swap" onClick={onSwap} title="Changer d'effet (vignettes par type)" aria-label={`Changer l'effet ${label}`}>⇄</button>}
      <div className="pd-name"><b>{m.name}</b><span>{m.role ?? m.family}</span></div>
      <div className="pd-knobs">
        {/* un bouton seul sur sa rangée est centré (nombre impair de commandes) */}
        {knobs.map((id, i) => <Knob key={id} global={addr(id)} type={m.type} index={i} center={odd && i === knobs.length - 1 && (sk.tall || sw.length === 0)} />)}
        {!sk.tall && sw.map((id, i) => <Knob key={id} global={addr(id)} small center={odd && i === sw.length - 1} />)}
      </div>
      {sk.tall && sw.length > 0 && <div className="pd-switches">{sw.map((id) => <Knob key={id} global={addr(id)} small />)}</div>}
      {m.type === "looper" && (
        <>
          <div className="pd-loop-state"><b>{LOOPER_STATES[loopState] ?? ""}</b><span className="pd-loop-bar"><span style={{ width: `${loopPhase * 100}%` }} /></span>
            <em>{(meter[3] ?? 0) > 0 ? `${(meter[3] ?? 0).toFixed(2)} s` : ""}</em></div>
          <LooperKeys global={addr("action")} state={loopState} />
        </>
      )}
      <ParamMenuZone binding={bypBinding} className="pm-zone--byp pm-zone--foot">
        <button type="button" className="pd-foot" aria-pressed={!bypassed} onClick={toggle} aria-label={`${m.name} : ${bypassed ? "contourné" : "actif"}`}
          title={bypassed ? "Effet contourné : appuyer pour l'activer (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)" : "Effet actif : appuyer pour le contourner (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)"} />
      </ParamMenuZone>
    </div>
  );
}

/** Racks dont les commandes sont choisies à la main (v0.36 : écho « rapide » — temps à l'afficheur,
    réinjection et mélange en gros boutons, filtres et lo-fi, synchro, caractère, couleur, gel). */
const RACK_CURATED: Record<string, { big: string[]; small: string[]; sel: string[] }> = {
  delay: { big: ["feedback", "mix"], small: ["highcut", "lofi"], sel: ["sync", "mode", "echocolour", "freeze"] },
  distcomp: { big: ["input", "output"], small: ["attack", "release"], sel: ["ratio", "audio", "detector", "british"] }, // v0.38
  prestige: { big: ["size", "mix"], small: ["predelay", "lowmult", "highmult", "early"], sel: ["mode"] },
};

/** Réglages d'un rack : gros boutons (les deux premiers réglages continus), petits boutons, sélecteurs. */
function rackControls(m: ModuleDesc, values: number[], lcd?: string, vu = false): { big: string[]; small: string[]; sel: string[]; lcdId?: string } {
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const cur = RACK_CURATED[m.type];
  const order = m.sections.flatMap((s) => s.params).filter((k, i, a) => a.indexOf(k) === i).map((k) => m.params[k]).filter(Boolean);
  let lcdId: string | undefined;
  if (lcd) {
    const [free, synced] = lcd.split("|");
    const si = idx("sync");
    lcdId = synced && si >= 0 && toPlain(m.params[si], values[si] ?? 0) > 0.5 ? synced : free;
    if (idx(lcdId) < 0) lcdId = undefined;
  }
  if (cur) return { big: cur.big.filter((id) => idx(id) >= 0), small: cur.small.filter((id) => idx(id) >= 0), sel: cur.sel.filter((id) => idx(id) >= 0), lcdId };
  const display = vu || !!lcdId; // vumètre ou afficheur : moins de place pour les petits boutons
  const cont = order.filter((p) => !p.choices?.length && p.id !== lcdId && !/^s\d+$/.test(p.id)).map((p) => p.id);
  const sel = order.filter((p) => (p.choices?.length ?? 0) > 1 && !/^s\d+$/.test(p.id)).map((p) => p.id).slice(0, 4);
  return { big: cont.slice(0, 2), small: cont.slice(2, 2 + (display ? 4 : 6)), sel: sel.slice(0, display ? 2 : 4), lcdId };
}

/** Vumètre à aiguille : réduction de gain (mesure 0, en dB). */
function VuMeter({ db }: { db: number }) {
  const ang = (v: number) => 42 - Math.sqrt(Math.min(20, Math.max(0, v)) / 20) * 84; // 0 dB à droite, -20 dB à gauche (échelle resserrée vers 20)
  return (
    <div className="rk-vu" aria-label={`Réduction ${db.toFixed(1)} dB`}>
      <svg viewBox="0 0 120 70" aria-hidden>
        <path d="M18 52 A48 48 0 0 1 102 52" fill="none" stroke="oklch(0.25 0.02 60)" strokeWidth="1.1" />
        {[20, 10, 5, 2, 0].map((v) => {
          const t = (ang(v) - 90) * Math.PI / 180;
          return <g key={v}><line x1={60 + 47 * Math.cos(t)} y1={62 + 47 * Math.sin(t)} x2={60 + 53 * Math.cos(t)} y2={62 + 53 * Math.sin(t)} stroke="oklch(0.25 0.02 60)" strokeWidth="1.2" />
            <text x={60 + 40 * Math.cos(t)} y={62 + 40 * Math.sin(t) + 2.5} fontSize="6.5" textAnchor="middle" fill="oklch(0.25 0.02 60)">{v}</text></g>;
        })}
        <text x="60" y="48" fontSize="9" textAnchor="middle" fill="oklch(0.3 0.02 60)" fontWeight="700">VU</text>
      </svg>
      <span className="rk-needle" style={{ transform: `rotate(${ang(db)}deg)` }} />
      <span className="rk-pivot" />
    </div>
  );
}

/** Afficheur à 7 segments : la valeur d'un réglage (temps d'écho, déclin). */
function Lcd({ global }: { global: number }) {
  const b = useParam(global >= 0 ? global : null);
  return <div className="rk-lcd" title={b?.label}><span>{b?.text ?? ""}</span><small>{b?.label ?? ""}</small></div>;
}

/** Rack 19 pouces (v0.35.1) : oreilles vissées, façade, gros boutons pour les réglages principaux,
    petits boutons, sélecteurs à crans, vumètre ou afficheur, nom sérigraphié, interrupteur. */
function RackUnit({ channel, area = "fx", slot, label, m, values, meter, onSwap, onOpen }: { channel: number; area?: "fx" | "pfx"; slot: number; label: string; m: ModuleDesc; values: number[]; bypassed: boolean; meter: number[]; onSwap?: () => void; onOpen?: () => void }) {
  // v0.47 : interrupteur assignable (MIDI, bouton assignable, scènes) : voir useBypass
  const { binding: bypBinding, bypassed, toggle } = useBypass(channel, area, slot, m.name);
  const addr = (id: string) => { const i = m.params.findIndex((p) => p.id === id); return i < 0 ? -1 : paramAddress(channel, area, slot, i); };
  const mod = m.form === "module" ? MODULE_KNOBS[m.type] : undefined;
  const rs = rackSkinStyle(m.type, m.family);
  const { skin } = rs;
  const style = mod ? moduleSkinStyle(m.type).style : rs.style;
  const c = mod ? { big: mod.big, small: mod.small, sel: [] as string[], lcdId: undefined as string | undefined } : rackControls(m, values, skin.lcd, !!skin.vu);
  const kid = (s: string) => s.split(":")[0], klab = (s: string) => s.split(":")[1];
  return (
    <div className={`rk sk-knobs${bypassed ? " rk--off" : ""} rk--${m.type}${mod ? " rk--module" : ""}`} style={style}>
      <span className="rk-ear rk-ear--l" aria-hidden><i /><i /></span>
      <div className="rk-face">
        <span className="rk-n">{label}</span>
        {onSwap && <button type="button" className="pd-swap rk-swap" onClick={onSwap} title="Changer d'effet (vignettes par type)" aria-label={`Changer l'effet ${label}`}>⇄</button>}
        <div className="rk-controls">
          {c.big.length > 0 && <div className="rk-big">{c.big.map((id) => <Knob key={id} global={addr(kid(id))} label={klab(id)} />)}</div>}
          {c.lcdId && <Lcd global={addr(c.lcdId)} />}
          {!mod && skin.vu && <VuMeter db={meter[0] ?? 0} />}
          {c.small.length > 0 && <div className="rk-small" data-n={c.small.length}>{c.small.map((id) => {
            const sh = /^s(\d)amt$/.exec(kid(id));
            return <Knob key={id} global={addr(kid(id))} label={klab(id)} onGlobal={sh && m.type === "sculptor" ? addr(`s${sh[1]}on`) : -1} />;
          })}</div>}
          {mod && onOpen && <button type="button" className="rk-open" onClick={onOpen} title={`Ouvrir l'interface complète de ${m.name}`}>Ouvrir<br />le module <span aria-hidden>↗</span></button>}
          {c.sel.length > 0 && <div className="rk-sel">{c.sel.map((id) => <Knob key={id} global={addr(id)} small />)}</div>}
        </div>
        {m.type === "stepfx" && <div className="rk-steps">{Array.from({ length: 16 }, (_, i) => <StepCell key={i} global={addr(`s${i + 1}`)} index={i} m={m} />)}</div>}
        <div className="rk-name"><b>{m.name}</b><span>{m.role ?? m.family}</span></div>
        <ParamMenuZone binding={bypBinding} className="pm-zone--byp">
        <button type="button" className={`rk-power${bypassed ? "" : " on"}`} aria-pressed={!bypassed} onClick={toggle}
          title={bypassed ? "Effet contourné : appuyer pour l'activer (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)" : "Effet actif : appuyer pour le contourner (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)"}>
          <span className="rk-lamp" aria-hidden />{bypassed ? "Contourné" : "Actif"}
        </button>
        </ParamMenuZone>
      </div>
      <span className="rk-ear rk-ear--r" aria-hidden><i /><i /></span>
    </div>
  );
}

function EmptyPedal({ label, onOpen }: { label: string; onOpen: () => void }) {
  return (
    <div className="pd pd--empty">
      <span className="pd-n">{label}</span>
      <p>Emplacement libre</p>
      <button type="button" className="pd-add" onClick={onOpen} aria-label={`Choisir l'effet de l'emplacement ${label}`}>+ Poser un effet…</button>
    </div>
  );
}

export function Pedalboard() {
  const open = useStore((s) => s.ui.pedals);
  const focus = useStore((s) => s.focus);
  const cat = useStore((s) => s.catalog);
  const tracks = useStore((s) => s.tracks);
  const channel = useStore((s) => s.channels[s.focus]);
  const meters = useStore((s) => s.tick.fxm);
  const lane = useStore((s) => s.ui.lane);
  useEffect(() => {
    if (!open) return;
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape" && !e.defaultPrevented) { e.preventDefault(); setUi({ pedals: false }); } };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  }, [open]);
  const [pick, setPick] = useState<{ pad: number; k: number } | null>(null); // v0.34 : choix par vignettes (pad -1 = insert du canal)
  const [openMod, setOpenMod] = useState<{ area: "fx" | "pfx"; slot: number; type: string } | null>(null); // v0.35.2 : module ouvert
  if (!open || !cat || !channel) return null;
  const L = cat.layout;
  const types = tracks.inserts[focus] ?? [];
  const names = tracks.tracks.map((t) => t.name);
  const color = focus < L.numTracks ? trackColor(tracks.tracks[focus]?.color ?? 0) : "var(--accent)";
  const inst = focus < L.numTracks ? cat.instruments.find((i) => i.type === tracks.tracks[focus]?.instrument) : undefined;
  const drum = inst?.drumKit ? inst : undefined;
  return (
    <div className="pedalboard" role="dialog" aria-label="Pédalier" style={{ "--k": color } as React.CSSProperties}>
      <header className="pb-head">
        <span className="face-stripe" aria-hidden />
        <div><span className="face-tag">Pédalier · dans l'ordre du signal</span><h2>{channelName(cat, focus, names)}</h2></div>
        <button type="button" className="face-close" aria-label="Fermer le pédalier" onClick={() => setUi({ pedals: false })}>✕</button>
      </header>
      <div className="pb-board">
        <span className="pb-jack">{focus < L.numTracks ? "Instrument" : "Entrée"} ▸</span>
        {[0, 1, 2, 3].map((slot) => {
          const m = cat.effects.find((e) => e.type === types[slot]);
          return m
            ? <Pedal key={`${slot}:${m.type}`} channel={focus} slot={slot} label={String(slot + 1)} m={m} values={channel.fx[slot]?.values ?? []} bypassed={(channel.mix[6 + slot] ?? 0) > 0.5} meter={meters[slot] ?? []}
                onSwap={() => setPick({ pad: -1, k: slot })} onOpen={() => setOpenMod({ area: "fx", slot, type: m.type })} />
            : <EmptyPedal key={slot} label={String(slot + 1)} onOpen={() => setPick({ pad: -1, k: slot })} />;
        })}
        <span className="pb-jack">▸ {focus === L.masterChannel ? "Sortie" : "Fader"}</span>
      </div>
      {drum && (
        <div className="pb-pad">
          <div className="pb-pad-head">
            <span className="face-tag">Pédales du pad · avant son niveau et ses envois</span>
            <div className="pb-pad-pick" role="tablist" aria-label="Pad">
              {Array.from({ length: 16 }, (_, p) => {
                const has = (channel.padfx ?? []).some((f) => f.pad === p);
                return <button key={p} type="button" role="tab" aria-selected={p === lane} className={`${p === lane ? "on" : ""}${has ? " has" : ""}`}
                  title={drum.padNames[p] ?? ""} onClick={() => setUi({ lane: p })}>{p + 1}</button>;
              })}
            </div>
          </div>
          <div className="pb-board pb-board--pad">
            <span className="pb-jack">Pad {lane + 1} · {drum.padNames[lane] ?? ""} ▸</span>
            {[0, 1].map((k) => {
              const f = (channel.padfx ?? []).find((x) => x.pad === lane && x.slot === k);
              const m = f ? cat.effects.find((e) => e.type === f.type) : undefined;
              const label = `P${lane + 1}·${k + 1}`;
              return m && f
                ? <div key={`${lane}:${k}:${m.type}`} className="pd-wrap">
                    <Pedal channel={focus} area="pfx" slot={lane * 2 + k} label={label} m={m} values={f.values} bypassed={f.bypass > 0.5} meter={[]} onSwap={() => setPick({ pad: lane, k })}
                      onOpen={() => setOpenMod({ area: "pfx", slot: lane * 2 + k, type: m.type })} />
                    <button type="button" className="pd-remove" onClick={() => void actions.setPadInsert(focus, lane, k, "")}>Retirer</button>
                  </div>
                : <EmptyPedal key={`${lane}:${k}`} label={label} onOpen={() => setPick({ pad: lane, k })} />;
            })}
            <span className="pb-jack">▸ niveau, panoramique, envois</span>
          </div>
        </div>
      )}
      {openMod && (() => {
        const m = cat.effects.find((e) => e.type === openMod.type);
        const still = openMod.area === "fx" ? types[openMod.slot] === openMod.type
          : (channel.padfx ?? []).some((x) => x.pad * 2 + x.slot === openMod.slot && x.type === openMod.type);
        return m && still ? <ModulePanel channel={focus} area={openMod.area} slot={openMod.slot} m={m} onClose={() => setOpenMod(null)} /> : null;
      })()}
      {pick && (() => {
        const cur = pick.pad < 0 ? types[pick.k] ?? "" : (channel.padfx ?? []).find((x) => x.pad === pick.pad && x.slot === pick.k)?.type ?? "";
        const set = (t: string) => void (pick.pad < 0 ? actions.setInsert(focus, pick.k, t) : actions.setPadInsert(focus, pick.pad, pick.k, t));
        const title = pick.pad < 0 ? `Effet de l'emplacement ${pick.k + 1} · ${channelName(cat, focus, names)}` : `Effet du pad ${pick.pad + 1} · emplacement ${pick.k + 1}`;
        return <EffectPicker title={title} current={cur} onPick={set} onRemove={() => set("")} onClose={() => setPick(null)} />;
      })()}
    </div>
  );
}
