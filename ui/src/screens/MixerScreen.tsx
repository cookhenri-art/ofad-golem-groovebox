// Groovebox — OFAD — AGPL-3.0-or-later
// Écran MIXEUR : les 16 pistes en défilement (8 visibles à la fois, les encodeurs suivent), ou les
// retours A-D + bus principal : volume, panoramique, égaliseur 3 bandes, crête-mètre, coupure et
// solo, envois, et le bouton FX qui ouvre les effets de la piste.

import { useEffect, useRef, useState } from "react";
import { actions, setUi, useStore } from "../engine/store";
import { busName, busTag, groupBase, groupChannels, isGroup, numGroups, trackColor } from "../engine/params";
import { paramAddress, useParam } from "../controls/bindings";
import { ParamMenuZone, type EncoderBinding } from "../controls/Encoder";
import { RowLaunchBar } from "./RowLaunchBar";
import { ScreenFrame } from "./ScreenFrame";
import { SpectrumPanel } from "./SpectrumPanel";
import { DragParam } from "../controls/DragParam";
import { useLearnPress } from "../controls/useLearnPress";
import { MIDI_BUS_MUTE_BASE, MIDI_BUS_SOLO_BASE, MIDI_MUTE_BASE, MIDI_SOLO_BASE } from "../engine/protocol";

export function TouchFader({ binding, label }: { binding: EncoderBinding | null; label: string }) {
  const ref = useRef<HTMLDivElement>(null);
  const grab = useRef<{ y: number; v: number } | null>(null);
  if (!binding || binding.kind !== "continuous") return <div className="tfader tfader--blank" />;
  // v0.45.1 : le fader se tire (il ne saute plus sous le doigt) : toute la course en ~300 px, Maj : fin
  const from = (clientY: number) => ({ y: clientY, v: binding.normalised });
  return (
    <div ref={ref} className="tfader" role="slider" tabIndex={0} aria-label={label} aria-valuetext={binding.text}
      aria-valuenow={Number(binding.normalised.toFixed(3))} aria-valuemin={0} aria-valuemax={1}
      onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); grab.current = from(e.clientY); }}
      onPointerMove={(e) => {
        const g = grab.current;
        if (!(e.buttons & 1) || !g) return;
        const span = Math.max(300, 2 * (ref.current?.getBoundingClientRect().height ?? 150));
        binding.onChange(Math.min(1, Math.max(0, g.v + (g.y - e.clientY) / span * (e.shiftKey ? 0.25 : 1))));
      }}
      onPointerUp={() => { grab.current = null; }}
      onDoubleClick={() => binding.reset?.()}
      onKeyDown={(e) => {
        if (e.key === "ArrowUp") binding.onChange(binding.normalised + 0.02);
        if (e.key === "ArrowDown") binding.onChange(binding.normalised - 0.02);
      }}>
      <span className="tfader-fill" style={{ height: `${binding.normalised * 100}%` }} />
      <span className="tfader-cap" style={{ bottom: `calc(${binding.normalised * 100}% - 5px)` }} />
    </div>
  );
}

/** Petit bouton rotatif de la tranche (on le tire vers le haut ou le bas ; double-clic : valeur
    d'origine). Bipolaire : l'arc part du milieu (gains de l'égaliseur). */
export function MiniKnob({ global, label, short, bipolar }: { global: number; label: string; short: string; bipolar?: boolean }) {
  const b = useParam(global, label);
  const v = b?.normalised ?? (bipolar ? 0.5 : 0);
  const a0 = -135, a = a0 + 270 * v, from = bipolar ? 0 : a0;
  const pt = (deg: number, r: number) => { const t = (deg - 90) * Math.PI / 180; return [12 + r * Math.cos(t), 12 + r * Math.sin(t)]; };
  const arc = (d0: number, d1: number) => {
    const [x0, y0] = pt(Math.min(d0, d1), 9), [x1, y1] = pt(Math.max(d0, d1), 9);
    return `M${x0.toFixed(2)},${y0.toFixed(2)} A9,9 0 ${Math.abs(d1 - d0) > 180 ? 1 : 0} 1 ${x1.toFixed(2)},${y1.toFixed(2)}`;
  };
  const [px, py] = pt(a, 6);
  const flat = bipolar && Math.abs(v - 0.5) < 0.004;
  // v0.46.2 : appui long (clic droit sur ordinateur) : valeur exacte, modulation, MIDI ; double toucher : valeur d'origine
  return (
    <ParamMenuZone binding={b} className="pm-zone--mknob">
    <DragParam global={global} label={label} className={`mknob${flat ? " flat" : ""}`}>
      <svg viewBox="0 0 24 24" aria-hidden>
        <path d={arc(a0, a0 + 270)} className="mknob-track" />
        {Math.abs(a - from) > 1 && <path d={arc(from, a)} className="mknob-arc" />}
        <circle cx="12" cy="12" r="5.5" className="mknob-cap" />
        <line x1="12" y1="12" x2={px.toFixed(2)} y2={py.toFixed(2)} className="mknob-dot" />
      </svg>
      <span className="mknob-label">{short}</span>
    </DragParam>
    </ParamMenuZone>
  );
}

/** Égaliseur de la tranche : graves (plateau 100 Hz), médiums (cloche, fréquence réglable), aigus
    (plateau 10 kHz), ±15 dB. À plat, le moteur ne calcule rien. */
function StripEq({ channel, name, mix }: { channel: number; name: string; mix: number[] }) {
  const at = (i: number) => paramAddress(channel, "mix", 0, i);
  const freq = useParam(at(14), `Fréquence des médiums ${name}`);
  const flat = [12, 13, 15].every((i) => Math.abs((mix[i] ?? 0.5) - 0.5) < 0.004);
  return (
    <div className={`strip-eq${flat ? " is-flat" : ""}`} role="group" aria-label={`Égaliseur ${name}`}>
      <MiniKnob global={at(12)} label={`Graves ${name}`} short="G" bipolar />
      <MiniKnob global={at(13)} label={`Médiums ${name}`} short="M" bipolar />
      <MiniKnob global={at(15)} label={`Aigus ${name}`} short="A" bipolar />
      <DragParam global={at(14)} label={`Fréquence des médiums ${name}`} className="strip-eq-freq tabular">
        {freq?.text ?? ""}
      </DragParam>
    </div>
  );
}

/** v0.46.2 : envois vers les retours A, B, C, D (boutons) et PRÉ/POST. « grid » : deux rangées
    (A B / C D), à la place de l'égaliseur des pistes ; sinon une rangée sous l'égaliseur (groupes). */
export function StripSends({ channel, name, mix, grid }: { channel: number; name: string; mix: number[]; grid: boolean }) {
  const pre = (mix[11] ?? 0) > 0.5;
  return (
    <div className={`strip-sends2${grid ? " is-grid" : ""}`} role="group" aria-label={`Envois ${name}`}>
      {[0, 1, 2, 3].map((r) => <MiniKnob key={r} global={paramAddress(channel, "mix", 0, 2 + r)} label={`Envoi ${"ABCD"[r]} ${name}`} short={"ABCD"[r]} />)}
      <button type="button" className={`strip-pre${pre ? " on" : ""}`}
        title={pre ? "Envois avant le fader (touchez pour les mettre après)" : "Envois après le fader (touchez pour les mettre avant)"}
        onClick={() => actions.setParam(paramAddress(channel, "mix", 0, 11), pre ? 0.25 : 0.75)}>{pre ? "PRÉ" : "POST"}</button>
    </div>
  );
}

/** v0.46.2 : égaliseur à plat (graves, médiums, aigus au milieu). */
const eqFlat = (mix: number[]) => [12, 13, 15].every((i) => Math.abs((mix[i] ?? 0.5) - 0.5) < 0.004);

function Strip({ channel }: { channel: number }) {
  const cat = useStore((s) => s.catalog)!;
  const L = cat.layout;
  const isTrack = channel < L.numTracks;
  const info = useStore((s) => (isTrack ? s.tracks.tracks[channel] : undefined));
  const peak = useStore((s) => s.tick.peaks[channel] ?? 0);
  const lit = useStore((s) => (isTrack ? s.litTracks[channel] : 0));
  const focus = useStore((s) => s.focus === channel);
  const pan = useParam(paramAddress(channel, "mix", 0, 1), "Panoramique");
  const volume = useParam(paramAddress(channel, "mix", 0, 0), "Volume");
  const mix = useStore((s) => s.channels[channel]?.mix);
  const inserts = useStore((s) => s.tracks.inserts[channel]);
  const fxTypes = useStore((s) => s.catalog?.effects);
  const fxNames = (inserts ?? []).filter((t) => !!t).map((t) => fxTypes?.find((e) => e.type === t)?.name ?? t);
  const fxCount = fxNames.length;
  const group = isGroup(L, channel);
  const hasSends = isTrack || group;
  const out = info?.out ?? 0;
  // pistes envoyées à ce groupe (v0.24)
  const fed = useStore((s) => (group ? s.tracks.tracks.filter((t) => (t.out ?? 0) === channel - groupBase(L) + 1).length : 0));
  // v0.31 : retours et groupes — coupure, solo (groupes), nom donné au groupe
  const bus = useStore((s) => (isTrack ? undefined : s.tracks.buses?.[channel - L.numTracks]));
  const isReturn = !isTrack && !group && channel !== L.masterChannel;
  const [renaming, setRenaming] = useState(false);
  const learn = useLearnPress(); // v0.46 : clic droit / appui long sur M ou S : affecter au contrôleur MIDI
  const name = isTrack ? info?.name ?? `Piste ${channel + 1}` : busName(L, channel);
  const muted = isTrack ? !!info?.mute : !!bus?.mute;
  const db = peak > 0 ? 20 * Math.log10(peak) : -90;
  const meter = Math.max(0, Math.min(1, (db + 60) / 66));
  // v0.46.2 : pas d'égaliseur sur les pistes (les envois A–D prennent sa place) ; gardé sur les
  // groupes, les retours et le principal. Une piste d'un ancien projet dont l'égaliseur n'est pas
  // à plat le garde affiché (son inchangé) jusqu'à ce qu'on le remette à plat.
  const eqShown = !!mix && mix.length > 15 && (!isTrack || !eqFlat(mix));
  const sendsShown = hasSends && !!mix;
  return (
    <div className={`strip${focus ? " strip--selected" : ""}${eqShown && sendsShown ? " strip--eq-sends" : ""}${muted ? " strip--muted" : ""}${channel === L.masterChannel ? " strip--master" : ""}${group ? " strip--group" : ""}`}
      style={{ "--k": isTrack ? trackColor(info?.color ?? 0) : "var(--accent)" } as React.CSSProperties}>
      {renaming ? (
        <input className="strip-rename" autoFocus defaultValue={bus?.name || name} maxLength={40} aria-label={`Nom du ${busTag(L, channel)}`}
          onBlur={(e) => { void actions.renameGroup(channel - groupBase(L), e.currentTarget.value.trim()); setRenaming(false); }}
          onKeyDown={(e) => { if (e.key === "Enter") e.currentTarget.blur(); if (e.key === "Escape") setRenaming(false); }} />
      ) : (
      <button type="button" className="strip-name" onClick={() => void actions.focus(channel)}
        onDoubleClick={() => { if (group) setRenaming(true); }} title={group ? "Double-clic : renommer le groupe" : undefined}>
        <span className={`strip-led${lit ? " on" : ""}`} aria-hidden />
        <span className="strip-num">{busTag(L, channel)}</span>
        <span className="strip-label">{name}</span>
      </button>
      )}
      <DragParam global={paramAddress(channel, "mix", 0, 1)} label={`Panoramique ${name}`} className="strip-pan">
        <span style={{ left: `${(pan?.normalised ?? 0.5) * 100}%` }} />
      </DragParam>
      <span className="strip-pan-text tabular">{pan?.text}</span>
      {eqShown && mix && <StripEq channel={channel} name={name} mix={mix} />}
      {sendsShown && mix && <StripSends channel={channel} name={name} mix={mix} grid={!eqShown} />}
      <div className="strip-fader">
        <TouchFader binding={volume} label={`Volume ${name}`} />
        {channel === L.masterChannel ? <MasterLR /> :
        <span className="strip-meter" aria-hidden><span style={{ height: `${meter * 100}%` }} className={db > -1 ? "hot" : ""} /></span>}
      </div>
      <span className="strip-db tabular">{volume?.text}</span>
      {isTrack ? (
        <div className="strip-ms">
          <button type="button" className={info?.mute ? "on" : ""} onClick={() => void actions.trackMute(channel, !info?.mute)}
            title={`Couper ${name} — clic droit : affecter au contrôleur MIDI`} {...learn(MIDI_MUTE_BASE - channel, `Coupure de la piste ${channel + 1}`)}>M</button>
          <button type="button" className={`solo${info?.solo ? " on" : ""}`} onClick={() => void actions.trackSolo(channel, !info?.solo)}
            title={`Solo de ${name} — clic droit : affecter au contrôleur MIDI`} {...learn(MIDI_SOLO_BASE - channel, `Solo de la piste ${channel + 1}`)}>S</button>
        </div>
      ) : group || isReturn ? (
        <div className="strip-ms" title={group ? `${fed ? `${fed} piste${fed > 1 ? "s" : ""}` : "aucune piste"} dans ce groupe` : undefined}>
          <button type="button" className={bus?.mute ? "on" : ""} aria-pressed={!!bus?.mute} onClick={() => void actions.busMute(channel, !bus?.mute)}
            title={`Couper ${name} — clic droit : affecter au contrôleur MIDI`} {...learn(MIDI_BUS_MUTE_BASE - (channel - L.numTracks), `Coupure de ${name}`)}>M</button>
          {group && <button type="button" className={`solo${bus?.solo ? " on" : ""}`} aria-pressed={!!bus?.solo} onClick={() => void actions.busSolo(channel, !bus?.solo)}
            title={`Solo de ${name} : seules ses pistes restent entendues — clic droit : affecter au contrôleur MIDI`}
            {...learn(MIDI_BUS_SOLO_BASE - (channel - L.numTracks), `Solo de ${name}`)}>S</button>}
          {group && <span className="strip-fed-mini">{fed ? `${fed}` : "vide"}</span>}
        </div>
      ) : <div className="strip-ms" />}
      {isTrack && numGroups(L) > 0 && (
        <select className={`strip-out${out ? " on" : ""}`} value={out} aria-label={`Sortie ${name}`}
          title="Sortie de la piste : bus principal ou bus de groupe"
          onChange={(e) => void actions.trackOutput(channel, Number(e.target.value))}>
          <option value={0}>→ Princ.</option>
          {Array.from({ length: numGroups(L) }, (_, g) => <option key={g} value={g + 1}>→ G{g + 1}</option>)}
        </select>
      )}
      <button type="button" className={`strip-fx${fxCount ? " has" : ""}`} title={fxCount ? `Effets : ${fxNames.join(", ")} — ouvrir EFFETS` : "Aucun effet — ouvrir EFFETS pour en ajouter"}
        onClick={() => { void actions.focus(channel); setUi({ screen: "effects" }); }}>
        FX{fxCount ? <b className="tabular">{fxCount}</b> : null}
      </button>
    </div>
  );
}

/** v0.46 : deux crête-mètres (gauche, droite) sur le bus principal : le panoramique s'y voit. */
function MasterLR() {
  const lr = useStore((s) => s.tick.outlr);
  const bar = (v: number) => { const d = v > 0 ? 20 * Math.log10(v) : -90; return { h: Math.max(0, Math.min(1, (d + 60) / 66)), hot: d > -1 }; };
  const l = bar(lr?.[0] ?? 0), r = bar(lr?.[1] ?? 0);
  return (
    <span className="strip-meter strip-meter--lr" aria-hidden title="Sortie : gauche et droite">
      <span style={{ height: `${l.h * 100}%` }} className={l.hot ? "hot" : ""} />
      <span style={{ height: `${r.h * 100}%` }} className={r.hot ? "hot" : ""} />
    </span>
  );
}

/** v0.46 : la sortie audio n'a qu'une voie : tout est replié en mono, le panoramique ne s'entend pas. */
function MonoWarning() {
  const och = useStore((s) => s.tick.och);
  if (och !== 1) return null;
  return (
    <section className="loudness mono-warning" role="alert">
      <h4>Sortie mono</h4>
      <p>La sortie audio n'a qu'une voie : gauche et droite sont mélangées, le panoramique et la largeur ne peuvent pas s'entendre.</p>
      <p>Choisissez une sortie stéréo (2 voies) dans les réglages audio, ou une piste stéréo dans le logiciel hôte.</p>
    </section>
  );
}

/** Aide de la page Groupes : comment envoyer des pistes dans un bus de groupe. */
function GroupHelp() {
  return (
    <section className="loudness group-help" aria-label="Bus de groupe">
      <h4>Bus de groupe</h4>
      <p>Choisissez la sortie d'une piste sous son fader (page « Pistes 1–16 ») ou dans PISTES : « → G1 » à « → G7 ».</p>
      <p>Le groupe additionne ses pistes, puis passe par ses 4 inserts (EFFETS), son égaliseur, son volume et ses envois vers les retours, avant le bus principal.</p>
      <p>Idéal pour compresser toute la batterie ou baisser toutes les nappes d'un geste.</p>
    </section>
  );
}

const fmt = (v: number, unit: string) => (v <= -99 ? "—" : `${v.toFixed(1)} ${unit}`);

/** Mesures du bus principal : intensité (BS.1770 / R 128), crête vraie, efficace. */
function LoudnessPanel() {
  const loud = useStore((s) => s.tick.loud);
  const [m, st, i, tp, tpMax, rms] = loud ?? [-100, -100, -100, -100, -100, -100];
  const bar = (v: number) => `${Math.max(0, Math.min(100, ((v + 40) / 40) * 100))}%`;
  return (
    <section className="loudness" aria-label="Mesures du bus principal">
      <h4>Intensité · bus principal</h4>
      <div className="loud-row"><span>Instantanée</span><b className="tabular">{fmt(m, "LUFS")}</b><i style={{ width: bar(m) }} /></div>
      <div className="loud-row"><span>Courte (3 s)</span><b className="tabular">{fmt(st, "LUFS")}</b><i style={{ width: bar(st) }} /></div>
      <div className="loud-row loud-row--big"><span>Intégrée</span><b className="tabular">{fmt(i, "LUFS")}</b></div>
      <div className="loud-row"><span>Crête vraie</span><b className={`tabular${tp > -1 ? " hot" : ""}`}>{fmt(tp, "dBTP")}</b></div>
      <div className="loud-row"><span>Crête vraie max.</span><b className={`tabular${tpMax > -1 ? " hot" : ""}`}>{fmt(tpMax, "dBTP")}</b></div>
      <div className="loud-row"><span>Efficace (RMS)</span><b className="tabular">{fmt(rms, "dB")}</b></div>
      <button type="button" className="touch touch--small touch--quiet" onClick={() => actions.resetLoudness()}>Remettre à zéro</button>
      <p className="loud-note">Repères : streaming −14 LUFS, crête vraie −1 dBTP.</p>
    </section>
  );
}

export function MixerScreen() {
  const cat = useStore((s) => s.catalog);
  const rawBank = useStore((s) => s.ui.mixerBank);
  const first = useStore((s) => s.ui.mixerFirst);
  const bank = rawBank === 2 || rawBank === 3 || rawBank === 4 ? rawBank : 0; // 0 : les 16 pistes en défilement, 2 : retours, 3 : groupes
  const L = cat?.layout;
  const scroller = useRef<HTMLDivElement>(null);
  // v0.31 : les groupes suivent les 16 pistes dans le défilement (positions 17 à 20)
  const tracksAll = !L ? [] : [...Array.from({ length: L.numTracks }, (_, i) => i), ...groupChannels(L)];
  const maxFirst = Math.max(0, tracksAll.length - 8);
  const channels = !L ? [] : bank === 2 ? [...Array.from({ length: L.numReturns }, (_, i) => L.numTracks + i), L.masterChannel]
    : bank === 3 ? [...groupChannels(L), L.masterChannel] : tracksAll;
  // les encodeurs suivent les 8 tranches visibles
  const shown = bank >= 2 ? channels : tracksAll.slice(first, first + 8);
  const ids = Array.from({ length: 8 }, (_, i) => (shown[i] !== undefined && L ? paramAddress(shown[i], "mix", 0, 0) : null));
  // eslint-disable-next-line react-hooks/rules-of-hooks
  const volumes = ids.map((g) => useParam(g, "Volume"));
  // ancienne banque 9-16 : on montre les pistes 9 à 16
  useEffect(() => { if (rawBank === 1) setUi({ mixerBank: 0, mixerFirst: 8 }); }, [rawBank]);
  // la piste choisie reste visible ; la position du défilement suit « mixerFirst »
  useEffect(() => {
    const el = scroller.current;
    if (!el || bank >= 2) return;
    const w = (el.clientWidth + 6) / 8; // une tranche et son espace
    if (Math.abs(el.scrollLeft - first * w) > w / 2) el.scrollLeft = first * w;
  }, [first, bank]);
  if (!cat || !L) return <ScreenFrame title="Mixeur" encoders={[]}><div /></ScreenFrame>;
  const onScroll = () => {
    const el = scroller.current;
    if (!el) return;
    const k = Math.max(0, Math.min(maxFirst, Math.round(el.scrollLeft / ((el.clientWidth + 6) / 8))));
    if (k !== first) setUi({ mixerFirst: k });
  };
  const page = (d: number) => setUi({ mixerFirst: Math.max(0, Math.min(maxFirst, first + d)) });
  const volumeLabel = (ch: number) => (ch < L.numTracks ? `Volume ${ch + 1}` : busName(L, ch));

  return (
    <ScreenFrame title="Mixeur" encoders={volumes.map((v, i) => (v ? { ...v, label: volumeLabel(shown[i]) } : null))}
      status={
        <span className="mixer-status">
          <span className="segmented segmented--screen" role="group" aria-label="Tranches affichées">
            <button type="button" className={bank === 0 ? "on" : ""} onClick={() => setUi({ mixerBank: 0 })}>{numGroups(L) > 0 ? "Pistes et groupes" : "Pistes 1–16"}</button>
            <button type="button" className={bank === 2 ? "on" : ""} onClick={() => setUi({ mixerBank: 2 })}>Retours et principal</button>
            {numGroups(L) > 0 && <button type="button" className={bank === 3 ? "on" : ""} onClick={() => setUi({ mixerBank: 3 })}
              title={`Bus de groupe G1 à G${numGroups(L)} : choisissez la sortie d'une piste sous son fader ou dans PISTES ; le groupe passe par ses 4 inserts, son égaliseur, son volume et ses envois avant le bus principal`}>Groupes</button>}
            <button type="button" className={bank === 4 ? "on" : ""} onClick={() => setUi({ mixerBank: 4 })} title="Analyseur de spectre du bus principal">Spectre</button>
          </span>
          {bank === 0 && (
            <span className="mixer-pos" role="group" aria-label="Défilement des pistes">
              <button type="button" className="touch touch--small touch--quiet" disabled={first <= 0} onClick={() => page(-4)} aria-label="Pistes précédentes">◀</button>
              <span className="tabular">{first + 1}–{first + 8}</span>
              <button type="button" className="touch touch--small touch--quiet" disabled={first >= maxFirst} onClick={() => page(4)} aria-label="Pistes suivantes">▶</button>
            </span>
          )}
        </span>
      }>
      <div className="with-rowlaunch">
      <RowLaunchBar />
      {bank === 4 ? (
        <div className="mixer mixer--spec">
          <SpectrumPanel />
        </div>
      ) : bank >= 2 ? (
        <div className="mixer">
          {channels.map((ch) => <Strip key={ch} channel={ch} />)}
          <MonoWarning />
          {/* v0.46 : 7 groupes + principal remplissent la page : l'aide n'y tient plus (bulle du bouton Groupes) */}
          {bank === 2 ? <LoudnessPanel /> : channels.length <= 6 ? <GroupHelp /> : null}
        </div>
      ) : (
        <div className="mixer-scroll" ref={scroller} onScroll={onScroll}
          onWheel={(e) => {
            // 1.0 (7 nov.) : la molette fait défiler les pistes de gauche à droite, une tranche par cran
            // (sauf au-dessus d'un réglage : envois, panoramique, fader, qui se règlent à la molette)
            const el = e.currentTarget;
            if (el.scrollWidth <= el.clientWidth || Math.abs(e.deltaX) > Math.abs(e.deltaY)) return;
            if ((e.target as HTMLElement).closest(".mknob, .drag-value, .tfader, .strip-pan, input, select")) return;
            const strip = el.querySelector<HTMLElement>(".strip");
            el.scrollBy({ left: Math.sign(e.deltaY) * (strip?.offsetWidth ?? 160), behavior: "smooth" });
          }}>
          <div className="mixer mixer--all">
            {channels.map((ch) => <Strip key={ch} channel={ch} />)}
            <MonoWarning />
          </div>
        </div>
      )}
      </div>
    </ScreenFrame>
  );
}
