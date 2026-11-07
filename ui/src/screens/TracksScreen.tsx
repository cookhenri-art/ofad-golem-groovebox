// Groovebox — OFAD — AGPL-3.0-or-later
// Écran PISTES : les 16 pistes du projet et l'instrument de chacune. À gauche les pistes ;
// à droite le navigateur d'instruments par famille, leur principe et leurs préréglages.

import { InstrumentPicker, thumbUrl } from "./InstrumentPicker";
import { useContext, useEffect, useRef, useState } from "react";
import { actions, setUi, showMessage, useStore } from "../engine/store";
import { numGroups, trackColor } from "../engine/params";
import { paramAddress, steppedBinding, useParam } from "../controls/bindings";
import type { EncoderBinding } from "../controls/Encoder";
import { ScreenFrame, ZoomContext } from "./ScreenFrame";

export function TracksScreen() {
  const cat = useStore((s) => s.catalog);
  const tracks = useStore((s) => s.tracks);
  const focus = useStore((s) => s.focus);
  const lit = useStore((s) => s.litTracks);
  const family = useStore((s) => s.ui.browserFamily);
  const browserIndex = useStore((s) => s.ui.browserIndex);
  const clip = useStore((s) => s.ui.trackClip);
  const listRef = useRef<HTMLUListElement>(null);
  // glisser-déposer d'une piste (souris) : la piste tirée vient se placer sur celle survolée
  const [dragFrom, setDragFrom] = useState<number | null>(null);
  const [dragOver, setDragOver] = useState<number | null>(null);
  // l'instrument choisi (encodeur, clavier, retour sur la piste) reste visible dans la liste
  useEffect(() => {
    const el = listRef.current?.querySelector<HTMLElement>("button.on");
    el?.scrollIntoView({ block: "nearest" });
  }, [family, browserIndex]);
  const L = cat?.layout;
  const t = Math.min(focus, (L?.numTracks ?? 16) - 1);
  const isTrack = !!L && focus < L.numTracks;
  const vol = useParam(isTrack ? paramAddress(t, "mix", 0, 0) : null, "Volume");
  const pan = useParam(isTrack ? paramAddress(t, "mix", 0, 1) : null, "Panoramique");
  const sendA = useParam(isTrack ? paramAddress(t, "mix", 0, 2) : null, "Envoi A");
  const sendB = useParam(isTrack ? paramAddress(t, "mix", 0, 3) : null, "Envoi B");
  if (!cat || !L) return <ScreenFrame title="Pistes" encoders={[]}><div /></ScreenFrame>;

  const families = cat.instruments.map((i) => i.family).filter((f, i, a) => a.indexOf(f) === i);
  const list = cat.instruments.filter((i) => i.family === family);
  const current = list[Math.min(browserIndex, list.length - 1)] ?? cat.instruments[0];
  const track = tracks.tracks[t];
  const assign = (type: string) => { void actions.setInstrument(t, type); };
  const [hover, setHover] = useState<string | null>(null);
  const zoom = useContext(ZoomContext);
  const [picker, setPicker] = useState(false);
  const midiRoute = useStore((s) => s.midi.route); // v0.46 : canal MIDI propre à la piste
  // GOLEM 1.0 : pistes internes T jouables au clavier (FM Player : 4, Pieuvre 8 : 8)
  const trackInst = cat.instruments.find((x) => x.type === track?.instrument);
  const laneCount = trackInst?.drumKit && trackInst.laneNotes ? Math.min(8, trackInst.padNames?.length ?? 0) : 0;

  const encoders: (EncoderBinding | null)[] = [
    steppedBinding("Piste", Array.from({ length: L.numTracks }, (_, i) => i), t, (i) => void actions.focus(i), (i) => `${i + 1} · ${tracks.tracks[i]?.name ?? ""}`),
    steppedBinding("Famille", families, family, (f) => setUi({ browserFamily: f, browserIndex: 0 })),
    steppedBinding("Instrument", list.map((_, i) => i), Math.min(browserIndex, list.length - 1), (i) => setUi({ browserIndex: i }), (i) => list[i]?.name ?? ""),
    current.presets.length ? steppedBinding("Préréglage", current.presets, current.type === track?.instrument && current.presets.includes(track.preset) ? track.preset : current.presets[0],
      (p) => { if (current.type === track?.instrument) void actions.applyPreset(t, -1, p); }) : null,
    vol, pan, sendA, sendB,
  ];

  return (
    <ScreenFrame title="Pistes" encoders={encoders}>
      <div className="tracks">
        {picker && isTrack && <InstrumentPicker track={t} full onClose={() => setPicker(false)} />}
        <section className="tracks-list" aria-label="Pistes du projet">
          <div className="tracks-tools" role="group" aria-label="Ordre des pistes">
            <button type="button" className="touch touch--quiet" disabled={!isTrack || t === 0} title="Monter la piste d'un cran"
              onClick={() => void actions.moveTrack(t, t - 1)}>▲</button>
            <button type="button" className="touch touch--quiet" disabled={!isTrack || t >= L.numTracks - 1} title="Descendre la piste d'un cran"
              onClick={() => void actions.moveTrack(t, t + 1)}>▼</button>
            <button type="button" className="touch touch--quiet" disabled={!isTrack} title="Une piste libre vient se placer ici, les suivantes glissent d'un cran"
              onClick={() => void actions.insertTrack(t)}>+ Insérer avant {t + 1}</button>
            {/* v0.41 : copie de piste à piste (principe de l'Octatrack) */}
            <button type="button" className={`touch touch--quiet${clip === t ? " is-on" : ""}`} disabled={!isTrack || !track?.instrument}
              title="Copier la piste : son, effets, séquences, verrous, échantillons (puis Coller sur une autre piste)"
              onClick={() => { setUi({ trackClip: t }); showMessage(`Piste ${t + 1} copiée : choisissez une piste puis Coller`); }}>Copier</button>
            <button type="button" className="touch touch--quiet" disabled={!isTrack || clip === null || clip === t}
              title={clip === null ? "Copiez d'abord une piste" : `Remplacer la piste ${t + 1} par une copie de la piste ${clip + 1} (Ctrl+Z pour revenir)`}
              onClick={() => { if (clip !== null) void actions.copyTrack(clip, t); }}>Coller{clip !== null && clip !== t ? ` ${clip + 1} ici` : ""}</button>
            <button type="button" className="touch touch--quiet" disabled={!isTrack || !track?.instrument}
              title="Copier la piste sur la première piste libre"
              onClick={() => void actions.duplicateTrack(t)}>Dupliquer</button>
            <span className="tracks-tools-hint">ou glisser une piste</span>
            <label className="tracks-out" title="Canal MIDI de la piste : « Piste choisie » = elle joue le clavier quand elle est choisie ; un canal = elle joue toujours ce canal (comme dans Ableton ou Fruity)">
              Canal MIDI
              <select className="chip chip--select" disabled={!isTrack} value={midiRoute?.tracks[t] ?? 0}
                onChange={(e) => void actions.setTrackMidiChannel(t, Number(e.target.value))}>
                <option value={0}>Piste choisie</option>
                {Array.from({ length: 16 }, (_, c) => <option key={c} value={c + 1}>Canal {c + 1}</option>)}
              </select>
            </label>
            {/* GOLEM 1.0 : FM Player, Pieuvre 8 — un canal MIDI par piste interne T */}
            {isTrack && laneCount > 0 && (
              <span className="tracks-lanes" role="group" aria-label="Canal MIDI de chaque piste interne T">
                {Array.from({ length: laneCount }, (_, l) => (
                  <label key={l} className="tracks-out" title={`T${l + 1} : « T choisie » = elle joue quand on la choisit à l'écran ; un canal = ce canal joue toujours T${l + 1}, même si une autre T est choisie`}>
                    T{l + 1}
                    <select className="chip chip--select" value={midiRoute?.lanes?.[t]?.[l] ?? 0}
                      onChange={(e) => void actions.setLaneMidiChannel(t, l, Number(e.target.value))}>
                      <option value={0}>T choisie</option>
                      {Array.from({ length: 16 }, (_, c) => <option key={c} value={c + 1}>Canal {c + 1}</option>)}
                    </select>
                  </label>
                ))}
              </span>
            )}
            {numGroups(L) > 0 && (
              <label className="tracks-out" title="Sortie de la piste : bus principal ou bus de groupe (inserts et volume communs au mixeur)">
                Sortie
                <select className="chip chip--select" disabled={!isTrack} value={tracks.tracks[t]?.out ?? 0}
                  onChange={(e) => void actions.trackOutput(t, Number(e.target.value))}>
                  <option value={0}>Bus principal</option>
                  {Array.from({ length: numGroups(L) }, (_, g) => <option key={g} value={g + 1}>Groupe {g + 1}</option>)}
                </select>
              </label>
            )}
          </div>
          {tracks.tracks.map((tr, i) => {
            const inst = cat.instruments.find((x) => x.type === tr.instrument);
            const fxCount = (tracks.inserts[i] ?? []).filter(Boolean).length;
            return (
              <button key={i} type="button" className={`track-row${i === focus ? " is-selected" : ""}${inst ? "" : " is-free"}${dragOver === i && dragFrom !== null && dragFrom !== i ? (dragFrom > i ? " is-drop-above" : " is-drop-below") : ""}`} style={{ "--k": trackColor(tr.color) } as React.CSSProperties}
                draggable
                onDragStart={(e) => { setDragFrom(i); e.dataTransfer.effectAllowed = "move"; e.dataTransfer.setData("text/plain", String(i)); }}
                onDragOver={(e) => { if (dragFrom === null) return; e.preventDefault(); e.dataTransfer.dropEffect = "move"; if (dragOver !== i) setDragOver(i); }}
                onDragLeave={() => { if (dragOver === i) setDragOver(null); }}
                onDrop={(e) => { e.preventDefault(); const from = dragFrom; setDragFrom(null); setDragOver(null); if (from !== null && from !== i) void actions.moveTrack(from, i); }}
                onDragEnd={() => { setDragFrom(null); setDragOver(null); }}
                onPointerEnter={(e) => { if (e.pointerType === "mouse" && inst) setHover(inst.type); }} onPointerLeave={() => setHover(null)}
                onClick={() => { void actions.focus(i); if (inst) setUi({ browserFamily: inst.family, browserIndex: cat.instruments.filter((x) => x.family === inst.family).indexOf(inst) }); }}
                onDoubleClick={() => setUi({ screen: inst ? "sound" : "tracks" })}>
                <span className={`track-swatch${(lit[i] ?? 0) > 0 ? " is-lit" : ""}`} aria-hidden />
                <span className="track-num tabular">{i + 1}</span>
                <span className="track-text">
                  <span className="track-name">{tr.name}</span>
                  <span className="track-inst">{inst ? `${inst.name}${tr.preset ? ` · ${tr.preset}` : ""}` : "Libre"}</span>
                </span>
                <span className="track-badges">
                  {(tr.out ?? 0) > 0 && <span className="track-out" title={`Sortie : groupe ${tr.out}`}>→ G{tr.out}</span>}
                  {fxCount > 0 && <span className="track-fx">{fxCount} effet{fxCount > 1 ? "s" : ""}</span>}
                </span>
              </button>
            );
          })}
          {tracks.tracks.some((tr) => !tr.instrument) && (
            <button type="button" className="track-row track-row--add" onClick={() => {
              const free = tracks.tracks.findIndex((tr) => !tr.instrument);
              if (free >= 0) { void actions.focus(free); setUi({ browserIndex: 0 }); }
            }}>+ Ajouter une piste</button>
          )}
        </section>

        <section className="browser" aria-label="Navigateur d'instruments">
          <div className="browser-tabs" role="tablist">
            {families.map((f) => (
              <button key={f} type="button" role="tab" aria-selected={f === family} className={f === family ? "on" : ""}
                onClick={() => setUi({ browserFamily: f, browserIndex: 0 })}>{f}</button>
            ))}
            <button type="button" className="touch touch--small browser-thumbs" disabled={!isTrack} onClick={() => setPicker(true)}
              title={`Choisir l'instrument de la piste ${t + 1} en touchant l'image de sa façade`}>Choisir avec les vignettes…</button>
          </div>
          {/* 1.0 : vue agrandie — la piste en haut (couleur, nom, Mettre sur la piste), puis une seule liste qui
              défile : vignette de la façade à gauche, nom et principe à droite (les préréglages sont dans SON) */}
          {zoom ? (<>
            <div className="bz-head" style={{ "--k": trackColor(track?.color ?? 0) } as React.CSSProperties}>
              <span className="bz-swatch" />
              <input className="bz-name" type="text" maxLength={40} defaultValue={track?.name ?? ""} key={`${t}:${track?.name}`} aria-label="Nom de la piste"
                onKeyDown={(e) => { if (e.key === "Enter") (e.target as HTMLInputElement).blur(); e.stopPropagation(); }}
                onBlur={(e) => { const v = e.target.value.trim(); if (v && v !== track?.name) void actions.renameTrack(t, v); }} />
              <span className="bz-colors" role="group" aria-label="Couleur de la piste">
                {Array.from({ length: 8 }, (_, c) => (
                  <button key={c} type="button" className={`color-chip${track?.color === c ? " on" : ""}`} style={{ background: trackColor(c) }}
                    onClick={() => void actions.setTrackColor(t, c)} aria-label={`Couleur ${c + 1}`} />
                ))}
              </span>
              {track?.instrument && <button type="button" className="touch touch--quiet bz-free" onClick={() => assign("")}>Libérer</button>}
              {/* 1.0 (7 nov.) : comme « Ouvrir le module » des effets */}
              {track?.instrument && <button type="button" className="touch touch--on bz-open face-open" onClick={() => setUi({ face: true })}
                title="Ouvrir l'instrument de la piste en grand, comme un synthé, avec son clavier (Échap pour refermer)">Ouvrir le synthé ↗</button>}
              <button type="button" className="touch touch--on bz-assign" disabled={!isTrack} onClick={() => assign(current.type)}>
                {current.type === track?.instrument ? `Recharger « ${current.name} »` : `Mettre « ${current.name} » sur la piste ${t + 1}`}
              </button>
            </div>
            <ul className="bz-list" ref={listRef}>
              {list.map((inst, i) => (
                <li key={inst.type}>
                  <button type="button" className={`bz-item${i === browserIndex ? " on" : ""}${inst.type === track?.instrument ? " is-used" : ""}`}
                    onClick={() => setUi({ browserIndex: i })} onDoubleClick={() => assign(inst.type)}>
                    <img className="bz-thumb" src={thumbUrl(inst.type)} alt="" draggable={false} loading="lazy" />
                    <span className="bz-text">
                      <span className="bi-name">{inst.name}{inst.type === track?.instrument && <em> · sur la piste</em>}</span>
                      <span className="bi-principle">{inst.principle}</span>
                    </span>
                  </button>
                </li>
              ))}
            </ul>
          </>) : (
          <div className="browser-body">
            <ul className="browser-list" ref={listRef}>
              {list.map((inst, i) => (
                <li key={inst.type}>
                  <button type="button" className={`${i === browserIndex ? "on" : ""}${inst.type === track?.instrument ? " is-used" : ""}`}
                    onClick={() => setUi({ browserIndex: i })} onDoubleClick={() => assign(inst.type)}
                    onPointerEnter={(e) => { if (e.pointerType === "mouse") setHover(inst.type); }} onPointerLeave={() => setHover(null)}>
                    <span className="bi-name">{inst.name}</span>
                    <span className="bi-principle">{inst.principle}</span>
                  </button>
                </li>
              ))}
            </ul>
            <div className="browser-detail">
              {/* v0.32 : miniature de la façade (celle survolée à la souris, sinon celle choisie) */}
              <img className="browser-thumb" src={thumbUrl(hover ?? current.type)} alt="" draggable={false} key={hover ?? current.type} />
              <h3>{current.name}</h3>
              <p>{current.principle}</p>
              {current.presets.length > 0 && <>
                <span className="detail-label">Préréglages</span>
                <ul className="preset-list">
                  {current.presets.map((p) => (
                    <li key={p}>
                      <button type="button" className={current.type === track?.instrument && track.preset === p ? "on" : ""}
                        onClick={async () => { if (current.type !== track?.instrument) await actions.setInstrument(t, current.type); void actions.applyPreset(t, -1, p); }}>{p}</button>
                    </li>
                  ))}
                </ul>
              </>}
              <div className="browser-actions">
                <button type="button" className="touch touch--on browser-assign" onClick={() => assign(current.type)}>
                  {current.type === track?.instrument ? `Recharger sur la piste ${t + 1}` : `Mettre sur la piste ${t + 1}`}
                </button>
                {track?.instrument && <button type="button" className="touch touch--quiet" onClick={() => assign("")}>Libérer la piste</button>}
              </div>
              <div className="color-row" role="group" aria-label="Couleur de la piste">
                <span className="detail-label">Couleur</span>
                {Array.from({ length: 8 }, (_, c) => (
                  <button key={c} type="button" className={`color-chip${track?.color === c ? " on" : ""}`} style={{ background: trackColor(c) }}
                    onClick={() => void actions.setTrackColor(t, c)} aria-label={`Couleur ${c + 1}`} />
                ))}
              </div>
              <label className="rename">
                <span className="detail-label">Nom de la piste</span>
                <input type="text" maxLength={40} defaultValue={track?.name ?? ""} key={`${t}:${track?.name}`}
                  onKeyDown={(e) => { if (e.key === "Enter") (e.target as HTMLInputElement).blur(); e.stopPropagation(); }}
                  onBlur={(e) => { const v = e.target.value.trim(); if (v && v !== track?.name) void actions.renameTrack(t, v); }} />
              </label>
            </div>
          </div>
          )}
        </section>
      </div>
    </ScreenFrame>
  );
}
