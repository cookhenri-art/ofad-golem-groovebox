// Groovebox — OFAD — AGPL-3.0-or-later
// Après la 1.1 : façade de la « Piste MIDI ».
//  - Bandeau du haut, en listes : appareil, canal d'émission, canal de réception, banque, son.
//  - 32 boutons en 4 sections de 8, chaque section renommable (« Filtre », « Enveloppe »…).
//    Le bouton tourne la VALEUR (séquence, verrous, scènes, modulations agissent dessus) ; la petite
//    roue ⚙ du bouton ouvre son réglage (Apprendre, numéro de CC, canal) — posé une fois pour toutes,
//    jamais par une LFO, une scène ou un verrou (réglages figés dans le moteur).
//  - 16 pads, comme ceux de la télécommande : CC maintenu, CC marche / arrêt, CC par pas, ou note.
//    Un pas choisi (appui long dans la rangée de pas) ou une scène en écriture : toucher un pad le
//    verrouille (ou le retire) sur ce pas — le pad est « appuyé » pendant ce pas à chaque passage.

import { useEffect, useRef, useState } from "react";
import { actions, useStore } from "../../engine/store";
import type { ModuleDesc } from "../../engine/protocol";
import { toNorm, toPlain } from "../../engine/params";
import { clearParamLock, lockedValue, paramAddress, useParam, useWriteTarget, writeParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";

// après la 1.1 : type de liaison affiché dans la liste des appareils (quand le système permet de le dire)
const LINK: Record<string, string> = { usb: "USB", bt: "Bluetooth", net: "réseau", virt: "virtuel" };
const range = (a: number, b: number) => Array.from({ length: b - a + 1 }, (_, i) => a + i);
const DEFAULT_SECTIONS = ["Boutons CC 1 à 8", "Boutons CC 9 à 16", "Boutons CC 17 à 24", "Boutons CC 25 à 32", "Pads"];
const PAD_MODES: [number, string][] = [[0, "CC maintenu"], [1, "CC marche / arrêt"], [2, "CC par pas"], [3, "Note"]];
const NOTE_NAMES = ["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"];
const noteName = (n: number) => `${NOTE_NAMES[n % 12]}${Math.floor(n / 12) - 1}`;

type Editing = { kind: "knob" | "pad"; k: number } | null;

function MidiKnob({ global, label, onCfg, active }: { global: number; label: string; onCfg: () => void; active: boolean }) {
  const b = useParam(global, label);
  return (
    <div className={`midi-knob${active ? " on" : ""}`}>
      <Encoder binding={b} variant="knob" />
      <button type="button" className="midi-knob-gear" onClick={onCfg} aria-label={`Régler le bouton ${label} (CC, canal)`}
        title="Régler ce bouton : Apprendre, numéro de CC, canal">⚙</button>
    </div>
  );
}

function SectionTitle({ name, onRename }: { name: string; onRename: (v: string) => void }) {
  const [edit, setEdit] = useState(false);
  const [v, setV] = useState(name);
  useEffect(() => { if (!edit) setV(name); }, [name, edit]);
  if (!edit) return (
    <button type="button" className="midi-sec-title" onClick={() => setEdit(true)} title="Toucher pour renommer la section">{name} <span aria-hidden>✎</span></button>
  );
  const done = () => { setEdit(false); onRename(v.replace(/\|/g, "/").trim().slice(0, 30)); };
  return (
    <input className="midi-sec-input" autoFocus value={v} maxLength={30} aria-label="Nom de la section"
      onChange={(e) => setV(e.target.value)} onBlur={done}
      onKeyDown={(e) => { if (e.key === "Enter") done(); if (e.key === "Escape") { setV(name); setEdit(false); } }} />
  );
}

export function MidiTrackStrip({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const route = useStore((s) => s.midi.route);
  const labelsRaw = useStore((s) => s.tracks.tracks[channel]?.labels ?? "");
  const [outs, setOuts] = useState<{ id: string; name: string; kind?: string }[]>([]);
  const [bt, setBt] = useState(false); // appairage Bluetooth possible (téléphone ; PC avec le MIDI moderne)
  const [current, setCurrent] = useState("");
  const [host, setHost] = useState(false);
  const [editing, setEditing] = useState<Editing>(null);
  const [learning, setLearning] = useState(false);
  const [padView, setPadView] = useState<number[]>(() => Array(16).fill(0)); // marche / arrêt, pas en cours (affichage)
  const [held, setHeld] = useState<boolean[]>(() => Array(16).fill(false));
  const pressAt = useRef<number[]>(Array(16).fill(0));
  const seq0 = useRef(-1);
  const refresh = async (id?: string) => {
    const r = await actions.midiTrackOut(id);
    setOuts(r.outs); setCurrent(r.current); setHost(!!r.host);
  };
  useEffect(() => { void refresh(); void actions.bluetoothMidi(false).then(setBt); }, []);
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const plain = (id: string) => { const i = idx(id); return i >= 0 ? Math.round(toPlain(m.params[i], values?.[i] ?? 0)) : 0; };
  const setPlain = (id: string, v: number) => { const i = idx(id); if (i >= 0) actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(m.params[i], v)); };
  const addr = (id: string) => paramAddress(channel, "inst", 0, idx(id));
  const target = useWriteTarget(channel);
  useStore((s) => s.extras); useStore((s) => s.scenes); // verrous des pads affichés
  // après la 1.1 : banque et son verrouillables par pas (ou tenus par une scène en écriture) ; jamais modulés
  const lockedPlain = (id: string) => { const i = idx(id); const v = i >= 0 && target.kind !== "base" ? lockedValue(target, addr(id)) : undefined; return v === undefined || v === null ? undefined : Math.round(toPlain(m.params[i], v)); };
  const setLockable = (id: string, v: number) => {
    const i = idx(id); if (i < 0) return;
    if (target.kind === "base") { setPlain(id, v); return; }
    if (v <= 0) clearParamLock(target, addr(id)); else writeParam(target, addr(id), toNorm(m.params[i], v));
  };
  const padLocked = (k: number) => target.kind !== "base" && (lockedValue(target, addr(`pad${k + 1}`)) ?? 0) >= 0.5;

  const parts = labelsRaw.split("|");
  const sectionName = (s: number) => parts[s]?.trim() || DEFAULT_SECTIONS[s];
  const rename = (s: number, v: string) => {
    const n = DEFAULT_SECTIONS.map((_, i) => (i === s ? v : parts[i]?.trim() ?? ""));
    const joined = n.map((x, i) => (x === DEFAULT_SECTIONS[i] ? "" : x)).join("|").replace(/\|+$/, "");
    void actions.setTrackLabels(channel, joined);
  };

  // réglage en cours : identifiants du numéro et du canal
  const numId = editing ? (editing.kind === "knob" ? `ccnum${editing.k + 1}` : `padnum${editing.k + 1}`) : "";
  const chId = editing ? (editing.kind === "knob" ? `ccch${editing.k + 1}` : `padch${editing.k + 1}`) : "";
  const padMode = editing?.kind === "pad" ? plain(`padmode${editing.k + 1}`) : 0;

  // apprentissage : le prochain CC reçu (numéro et canal) se pose sur le bouton ou le pad
  useEffect(() => {
    if (!learning || !editing) return;
    let alive = true;
    seq0.current = -1;
    void actions.midiLastCc().then((r) => { seq0.current = r.seq; });
    const id = window.setInterval(async () => {
      const r = await actions.midiLastCc();
      if (!alive || seq0.current < 0 || r.seq === seq0.current || r.num < 0) return;
      setPlain(numId, Math.min(r.num, 119));
      setPlain(chId, r.ch >= 1 && r.ch <= 16 && r.ch !== plain("outch") ? r.ch : 0);
      setLearning(false);
    }, 100);
    const stop = window.setTimeout(() => setLearning(false), 15000);
    return () => { alive = false; window.clearInterval(id); window.clearTimeout(stop); };
  }, [learning, editing?.kind, editing?.k]); // eslint-disable-line react-hooks/exhaustive-deps

  // pads : appui / relâchement (tenu au moins 40 ms : un toucher bref compte toujours)
  const padDown = (k: number) => {
    if (target.kind !== "base") {
      // pas choisi ou scène en écriture : le pad se verrouille (appuyé pendant le pas), ou le verrou s'en va
      const g = addr(`pad${k + 1}`);
      if (padLocked(k)) clearParamLock(target, g); else writeParam(target, g, 1);
      return;
    }
    pressAt.current[k] = performance.now();
    setHeld((h) => h.map((x, i) => (i === k ? true : x)));
    setPlain(`pad${k + 1}`, 1);
    const mode = plain(`padmode${k + 1}`), steps = Math.max(2, plain(`padsteps${k + 1}`));
    if (mode === 1) setPadView((v) => v.map((x, i) => (i === k ? (x ? 0 : 1) : x)));
    if (mode === 2) setPadView((v) => v.map((x, i) => (i === k ? (x + 1) % steps : x)));
  };
  const padUp = (k: number) => {
    if (pressAt.current[k] === 0) return;
    const left = 40 - (performance.now() - pressAt.current[k]);
    pressAt.current[k] = 0;
    setHeld((h) => h.map((x, i) => (i === k ? false : x)));
    window.setTimeout(() => setPlain(`pad${k + 1}`, 0), Math.max(0, left));
  };

  const outCh = plain("outch");
  const rx = route?.tracks?.[channel] ?? 0;
  const open = (e: Editing) => {
    const same = editing && e && editing.kind === e.kind && editing.k === e.k;
    setEditing(same ? null : e); setLearning(false);
  };

  return (
    <div className="midi-strip">
      <div className="midi-strip-out">
        <label className="midi-field"><span>Appareil</span>
          {host ? <em>sortie MIDI du logiciel hôte</em> : (
            <select value={current} onChange={(e) => void refresh(e.target.value)} aria-label="Appareil joué (sortie MIDI)">
              <option value="">— aucun —</option>
              {outs.map((o) => <option key={o.id} value={o.id}>{o.name}{o.kind && LINK[o.kind] ? ` · ${LINK[o.kind]}` : ""}</option>)}
            </select>)}
          {!host && <button type="button" className="midi-strip-btn" onClick={() => void refresh()} title="Chercher les appareils branchés ou appairés" aria-label="Chercher les appareils">↻</button>}
          {!host && bt && <button type="button" className="midi-strip-btn" onClick={() => void actions.bluetoothMidi(true)}
            title="Appairer un appareil MIDI Bluetooth, puis toucher ↻ : il apparaît dans la liste">Bluetooth…</button>}
        </label>
        {!host && outs.length === 0 && (
          <span className="midi-hint">Aucun appareil : branchez-le en USB (téléphone : câble ou hub USB-C « OTG »){bt ? " ou appairez-le en Bluetooth" : ""}, puis touchez ↻.</span>
        )}
        <label className="midi-field"><span>Émission</span>
          <select value={outCh} onChange={(e) => setPlain("outch", Number(e.target.value))} aria-label="Canal d'émission">
            {range(1, 16).map((c) => <option key={c} value={c}>Canal {c}</option>)}
          </select>
        </label>
        <label className="midi-field"><span>Réception</span>
          <select value={rx} onChange={(e) => void actions.setTrackMidiChannel(channel, Number(e.target.value))} aria-label="Canal de réception">
            <option value={0}>Piste choisie</option>
            {range(1, 16).map((c) => <option key={c} value={c}>Canal {c}</option>)}
          </select>
        </label>
        <label className="midi-field"><span>Banque</span>
          <select value={lockedPlain("bank") ?? plain("bank")} className={lockedPlain("bank") !== undefined ? "is-locked" : ""} onChange={(e) => setLockable("bank", Number(e.target.value))}
            aria-label={target.kind === "base" ? "Banque de l'appareil" : "Banque verrouillée sur ce pas"}>
            <option value={0}>—</option>
            {range(1, 128).map((b) => <option key={b} value={b}>Banque {b - 1}</option>)}
          </select>
        </label>
        <label className="midi-field"><span>Son</span>
          <select value={lockedPlain("program") ?? plain("program")} className={lockedPlain("program") !== undefined ? "is-locked" : ""} onChange={(e) => setLockable("program", Number(e.target.value))}
            aria-label={target.kind === "base" ? "Son (programme) de l'appareil" : "Son verrouillé sur ce pas"}>
            <option value={0}>—</option>
            {range(1, 128).map((p) => <option key={p} value={p}>Son {p}</option>)}
          </select>
        </label>
      </div>

      {editing && (
        <div className="midi-knob-cfg" role="dialog" aria-label={editing.kind === "knob" ? `Régler le bouton ${editing.k + 1}` : `Régler le pad ${editing.k + 1}`}>
          <strong>{editing.kind === "knob" ? `Bouton ${editing.k + 1}` : `Pad ${editing.k + 1}`}</strong>
          {editing.kind === "pad" && (
            <label className="midi-field"><span>Type</span>
              <select value={padMode} onChange={(e) => { setPlain(`padmode${editing.k + 1}`, Number(e.target.value)); setPadView((v) => v.map((x, i) => (i === editing.k ? 0 : x))); }} aria-label="Type de pad">
                {PAD_MODES.map(([v, l]) => <option key={v} value={v}>{l}</option>)}
              </select>
            </label>
          )}
          {padMode !== 3 && (
            <button type="button" className={`midi-learn${learning ? " on" : ""}`} aria-pressed={learning} onClick={() => setLearning(!learning)}
              title="Touchez, puis tournez le bouton du synthé (ou du contrôleur) : son numéro de CC et son canal se posent ici">
              <span className="midi-learn-dot" aria-hidden />{learning ? "Tournez un bouton…" : "Apprendre"}
            </button>
          )}
          <label className="midi-field"><span>{padMode === 3 ? "Note" : "CC"}</span>
            <select value={plain(numId)} onChange={(e) => setPlain(numId, Number(e.target.value))} aria-label={padMode === 3 ? "Note du pad" : "Numéro de CC"}>
              {(padMode === 3 ? range(0, 127) : range(0, 119)).map((n) => <option key={n} value={n}>{padMode === 3 ? `${noteName(n)} (${n})` : `CC ${n}`}</option>)}
            </select>
          </label>
          <label className="midi-field"><span>Canal</span>
            <select value={plain(chId)} onChange={(e) => setPlain(chId, Number(e.target.value))} aria-label="Canal">
              <option value={0}>Émission ({outCh})</option>
              {range(1, 16).map((c) => <option key={c} value={c}>Canal {c}</option>)}
            </select>
          </label>
          {editing.kind === "pad" && padMode === 2 && (
            <label className="midi-field"><span>Pas</span>
              <select value={plain(`padsteps${editing.k + 1}`)} onChange={(e) => { setPlain(`padsteps${editing.k + 1}`, Number(e.target.value)); setPadView((v) => v.map((x, i) => (i === editing.k ? 0 : x))); }} aria-label="Nombre de pas">
                {range(2, 8).map((n) => <option key={n} value={n}>{n} valeurs</option>)}
              </select>
            </label>
          )}
          <button type="button" className="midi-strip-btn midi-close" onClick={() => { setEditing(null); setLearning(false); }}>OK</button>
        </div>
      )}

      <div className="midi-secs">
        {range(0, 3).map((s) => (
          <section key={s} className="midi-sec" aria-label={sectionName(s)}>
            <SectionTitle name={sectionName(s)} onRename={(v) => rename(s, v)} />
            <div className="midi-sec-knobs">
              {range(0, 7).map((j) => {
                const k = s * 8 + j;
                return <MidiKnob key={k} global={addr(`cc${k + 1}`)} label={String(k + 1)}
                  active={editing?.kind === "knob" && editing.k === k} onCfg={() => open({ kind: "knob", k })} />;
              })}
            </div>
          </section>
        ))}
      </div>

      <section className="midi-sec midi-sec--pads" aria-label={sectionName(4)}>
        <SectionTitle name={sectionName(4)} onRename={(v) => rename(4, v)} />
        <div className="midi-pads">
          {range(0, 15).map((k) => {
            const mode = plain(`padmode${k + 1}`), steps = Math.max(2, plain(`padsteps${k + 1}`));
            const locked = padLocked(k);
            const lit = held[k] || locked || (target.kind === "base" && ((mode === 1 && padView[k] === 1) || (mode === 2 && padView[k] > 0)));
            const sub = mode === 0 ? "maintenu" : mode === 1 ? (padView[k] ? "marche" : "arrêt") : mode === 2 ? `pas ${padView[k] + 1}/${steps}` : noteName(plain(`padnum${k + 1}`));
            return (
              <div key={k} className={`midi-pad-wrap${editing?.kind === "pad" && editing.k === k ? " cfg" : ""}`}>
                <button type="button" className={`midi-pad${lit ? " on" : ""}${locked ? " locked" : ""}`} aria-pressed={lit}
                  title={target.kind !== "base" ? (locked ? "Pad verrouillé sur ce pas : toucher pour retirer" : "Toucher : verrouiller ce pad sur ce pas") : undefined}
                  onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); padDown(k); }}
                  onPointerUp={() => padUp(k)} onPointerCancel={() => padUp(k)} onLostPointerCapture={() => padUp(k)}
                  onKeyDown={(e) => { if ((e.key === " " || e.key === "Enter") && !e.repeat) { e.preventDefault(); padDown(k); } }}
                  onKeyUp={(e) => { if (e.key === " " || e.key === "Enter") padUp(k); }}
                  onContextMenu={(e) => e.preventDefault()}>
                  <b>{k + 1}</b><span>{sub}</span>
                </button>
                <button type="button" className="midi-knob-gear" onClick={() => open({ kind: "pad", k })} aria-label={`Régler le pad ${k + 1}`} title="Régler ce pad : type, CC ou note, canal">⚙</button>
              </div>
            );
          })}
        </div>
      </section>
    </div>
  );
}
