// Groovebox — OFAD — AGPL-3.0-or-later
// Écran EFFETS : chaîne du canal choisi (piste, retour ou bus principal).
//   - 4 inserts traités dans l'ordre ; l'effet choisi s'affiche en grand, avec ses onglets,
//     son visuel, ses préréglages, et prend les 8 encodeurs ;
//   - pour une piste : 4 envois vers les retours partagés A à D.

import { formClass, formLabel, GAIN_REDUCTION_FAMILIES, groupEffects } from "../engine/effectGroups";
import { EffectPicker, fxThumbUrl } from "./EffectPicker";
import { useState } from "react";
import { actions, setUi, useStore } from "../engine/store";
import { PresetPanel } from "../controls/PresetPanel";
import { channelName, groupChannels, isGroup, toPlain, trackColor } from "../engine/params";
import { paramAddress, useParam } from "../controls/bindings";
import { ParamMenuZone, type EncoderBinding } from "../controls/Encoder";
import { useBypass } from "../controls/useBypass";
import type { ModuleDesc } from "../engine/protocol";
import { ScreenFrame } from "./ScreenFrame";
import { ModulePanel } from "./ModulePanel";
import { ModSources, SectionVisual, moduleTabs, sectionsFor, useModSourceEncoders, useSectionEncoders } from "./ModuleView";

const SEND_SLOT = 4;
const CHAIN_SLOT = 5;

function useSendEncoders(channel: number, active: boolean): (EncoderBinding | null)[] {
  const ids = [2, 3, 4, 5, 0, 1, 10].map((k) => (active ? paramAddress(channel, "mix", 0, k) : null));
  // eslint-disable-next-line react-hooks/rules-of-hooks
  return ids.map((g) => useParam(g));
}

export function EffectsScreen() {
  const cat = useStore((s) => s.catalog);
  const focus = useStore((s) => s.focus);
  const tracks = useStore((s) => s.tracks);
  const channel = useStore((s) => s.channels[s.focus]);
  const ui = useStore((s) => s.ui);
  const meters = useStore((s) => s.tick.fxm);
  const [picker, setPicker] = useState(false); // v0.34 : choix de l'effet par vignettes
  const [presetsOpen, setPresetsOpen] = useState(false);
  const [moduleOpen, setModuleOpen] = useState(false); // v0.35.2 : interface dédiée des modules
  const L = cat?.layout;
  const isTrack = !!L && focus < L.numTracks;
  const hasSends = isTrack || (!!L && isGroup(L, focus)); // v0.24 : un groupe a ses envois vers les retours
  const slot = !hasSends && ui.fxSlot === SEND_SLOT ? 0 : ui.fxSlot;
  const chainView = slot === CHAIN_SLOT;
  const types = tracks.inserts[focus] ?? [];
  const fxType = slot < 4 ? types[slot] : "";
  const module = cat?.effects.find((e) => e.type === fxType) ?? null;

  const tabs = module ? moduleTabs(module, "fx") : [];
  const tab = tabs.some((t) => t.id === ui.fxTab) ? ui.fxTab : tabs[0]?.id ?? "";
  const sections = module && tab !== "modsrc" ? sectionsFor(module, tab, 0) : [];
  const chips = tab === "mod" ? [...sections.map((s) => s.name), "Modulations des effets"] : sections.map((s) => s.name);
  const sectionIndex = Math.min(ui.fxSection, Math.max(0, chips.length - 1));
  const section = sections[sectionIndex] ?? null;
  const showMods = !!module && (tab === "modsrc" || (tab === "mod" && !section));

  const fxEncoders = useSectionEncoders(focus, "fx", Math.min(slot, 3), showMods || slot >= SEND_SLOT ? null : section);
  const modEncoders = useModSourceEncoders(focus, "fx");
  const sendEncoders = useSendEncoders(focus, hasSends && slot === SEND_SLOT);
  const encoders = slot === SEND_SLOT ? sendEncoders : chainView ? [] : showMods ? modEncoders : fxEncoders;

  if (!cat || !L || !channel) return <ScreenFrame title="Effets" encoders={[]}><div /></ScreenFrame>;
  const bypassed = (i: number) => (channel.mix[6 + i] ?? 0) > 0.5;
  const names = tracks.tracks.map((t) => t.name);
  const channels = [...Array.from({ length: L.numTracks }, (_, i) => i), ...groupChannels(L), ...Array.from({ length: L.numReturns }, (_, i) => L.numTracks + i), L.masterChannel];

  return (
    <ScreenFrame title="Effets" encoders={encoders} tabs={slot < 4 && module ? tabs : undefined} tab={tab}
      onTab={(id) => setUi({ fxTab: id, fxSection: 0 })}
      status={<><span className="screen-spacer" />
        <select className="chip chip--select" value={focus} aria-label="Canal" onChange={(e) => void actions.focus(Number(e.target.value))}>
          {channels.map((c) => <option key={c} value={c}>{channelName(cat, c, names)}</option>)}
        </select></>}>
      <div className="fxs" style={{ "--k": isTrack ? trackColor(tracks.tracks[focus]?.color ?? 0) : "var(--accent)" } as React.CSSProperties}>
        <section className="fx-chain" aria-label="Chaîne du canal">
          <span className="detail-label">Inserts · dans l'ordre</span>
          <div className="fx-slots">
            <span className="fx-io">{isTrack ? "Instrument" : "Entrée"}</span>
            {types.map((t, i) => {
              const m = cat.effects.find((e) => e.type === t);
              return (
                <div key={i} className={`fx-slot${slot === i ? " is-selected" : ""}${m ? "" : " is-empty"}${m && bypassed(i) ? " is-bypassed" : ""}`}>
                  <button type="button" className="fx-slot-main" onClick={() => setUi({ fxSlot: i, fxTab: "", fxSection: 0 })}>
                    <span className="fx-slot-n">{i + 1}</span>
                    <span className="fx-slot-name">{m ? m.name : "+ Ajouter"}{m?.role && <small className="fx-role fx-role--block">{m.role}</small>}</span>
                    {m && tracks.insertPresets?.[focus]?.[i] && <span className="fx-slot-preset">{tracks.insertPresets[focus][i]}</span>}
                    {m && <span className={`fx-form fx-form--${formClass(m)}`}>{formLabel(m)}</span>}
                    {m && <span className="fx-slot-meter" title={GAIN_REDUCTION_FAMILIES.has(m.family) ? "Réduction de gain" : "Niveau de sortie"}>
                      <span style={{ width: `${Math.min(100, GAIN_REDUCTION_FAMILIES.has(m.family) ? ((meters[i]?.[0] ?? 0) / 18) * 100 : (meters[i]?.[1] ?? 0) * 100)}%` }} /></span>}
                  </button>
                  {m && (
                    <span className="fx-slot-tools">
                      <button type="button" className="fx-tool" disabled={i === 0} title="Monter (avant dans la chaîne)" aria-label={`Monter l'effet ${i + 1}`}
                        onClick={() => { void actions.moveInsert(focus, i, i - 1); setUi({ fxSlot: i - 1 }); }}>↑</button>
                      <button type="button" className="fx-tool" disabled={i === types.length - 1} title="Descendre (après dans la chaîne)" aria-label={`Descendre l'effet ${i + 1}`}
                        onClick={() => { void actions.moveInsert(focus, i, i + 1); setUi({ fxSlot: i + 1 }); }}>↓</button>
                      <BypassTool channel={focus} slot={i} name={m.name} />
                      <button type="button" className="fx-tool fx-tool--trash" title="Retirer l'effet" aria-label={`Retirer l'effet ${i + 1}`}
                        onClick={() => void actions.setInsert(focus, i, "")}>
                        <svg viewBox="0 0 16 16" aria-hidden><path d="M3 4.5h10M6.5 4.5V3h3v1.5M4.5 4.5l.7 8.5h5.6l.7-8.5M7 7v4M9 7v4" /></svg>
                      </button>
                    </span>
                  )}
                </div>
              );
            })}
            <span className="fx-io">{focus === L.masterChannel ? "Sortie" : "Fader"}</span>
          </div>
          <div className="fx-chain-btns">
            <button type="button" className={`touch touch--small${chainView ? " touch--on" : " touch--quiet"} fx-chain-btn`} onClick={() => setUi({ fxSlot: chainView ? 0 : CHAIN_SLOT })}>
              Chaînes prêtes à l'emploi…</button>
            <button type="button" className="touch touch--small touch--quiet fx-chain-btn" onClick={() => setUi({ pedals: true })}
              title="Les 4 inserts du canal en pédales : boutons, interrupteurs au pied">Pédalier ↗</button>
          </div>

          {hasSends && (
            <>
              <span className="detail-label">Envois · vers les retours partagés</span>
              <div className="fx-sends">
                {[0, 1, 2, 3].map((r) => {
                  const ret = tracks.inserts[L.numTracks + r]?.filter(Boolean).map((t) => cat.effects.find((e) => e.type === t)?.name).join(" + ") || "Libre";
                  const v = toPlain(cat.mixerTrack.params[2 + r], channel.mix[2 + r] ?? 0);
                  return (
                    <button key={r} type="button" className={`fx-send${slot === SEND_SLOT ? " is-selected" : ""}`} onClick={() => setUi({ fxSlot: SEND_SLOT })}>
                      <span className="fx-send-letter">{String.fromCharCode(65 + r)}</span>
                      <span className="fx-send-name">{ret}</span>
                      <span className="fx-send-bar"><span style={{ width: `${v}%` }} /></span>
                      <span className="fx-send-val tabular">{Math.round(v)} %</span>
                    </button>
                  );
                })}
              </div>
            </>
          )}
        </section>

        <section className="fx-detail" aria-label="Effet choisi">
          {chainView ? (
            <>
              <header><h3>Chaînes prêtes à l'emploi</h3><p>Remplace les 4 inserts de « {channelName(cat, focus, names)} » par une chaîne réglée pour une source.</p></header>
              <ul className="chain-list">
                {(cat.chains ?? []).map((c) => (
                  <li key={c.name}>
                    <button type="button" onClick={() => void actions.applyChain(focus, c.name)}>
                      <span className="bi-name">{c.name}</span>
                      <span className="bi-principle">{c.use}</span>
                      <span className="chain-steps">{c.inserts.map(([t, p]) => `${cat.effects.find((e) => e.type === t)?.name ?? t} · ${p}`).join("  →  ")}</span>
                    </button>
                  </li>
                ))}
              </ul>
            </>
          ) : slot === SEND_SLOT ? (
            <>
              <header><h3>Envois et mixage</h3><p>Les encodeurs règlent les envois A à D, le volume, le panoramique et la largeur de la piste.</p></header>
              <ul className="return-list">
                {[0, 1, 2, 3].map((r) => (
                  <li key={r}><button type="button" className="touch touch--quiet" onClick={() => { void actions.focus(L.numTracks + r); setUi({ fxSlot: 0 }); }}>
                    Ouvrir le retour {String.fromCharCode(65 + r)}</button></li>
                ))}
              </ul>
            </>
          ) : module ? (
            <>
              <header className="fx-head">
                <div><h3>{module.name}{module.role && <small className="fx-role"> · {module.role}</small>}</h3><p>{module.principle}</p></div>
                {module.form === "module" && <button type="button" className="touch touch--small touch--on fx-open" onClick={() => setModuleOpen(true)}
                  title="Tous les réglages sur l'interface dédiée du module">Ouvrir le module ↗</button>}
                <button type="button" className="touch touch--small fx-swap" onClick={() => setPicker(true)} title="Changer d'effet (vignettes par type)">⇄ Effet</button>
              </header>
              {chips.length > 1 && (
                <div className="section-chips" role="tablist" aria-label="Pages de réglages">
                  {chips.map((name, i) => (
                    <button key={name + i} type="button" role="tab" aria-selected={i === sectionIndex} className={i === sectionIndex ? "on" : ""}
                      onClick={() => setUi({ fxSection: i })}>{name}</button>
                  ))}
                </div>
              )}
              {showMods ? <ModSources channel={focus} scope="fx" /> : section && <SectionVisual channel={focus} area="fx" slot={slot} module={module} section={section} lane={0} />}
              {!showMods && module.presets.length > 0 && (
                <>
                  <span className="detail-label">Préréglages</span>
                  <ul className="preset-list preset-list--row">
                    {module.presets.map((p) => (
                      <li key={p}><button type="button" onClick={() => void actions.applyPreset(focus, slot, p)}>{p}</button></li>
                    ))}
                  </ul>
                </>
              )}
              {presetsOpen && (
                <PresetPanel key={`${focus}:${slot}:${fxType}`} scope="fx" channel={focus} slot={slot} pad={-1} title={module.name}
                  current={tracks.insertPresets?.[focus]?.[slot] || undefined} factory={module.presets}
                  onPickFactory={(n) => void actions.applyPreset(focus, slot, n)} onClose={() => setPresetsOpen(false)} />
              )}
              <div className="fx-actions">
                <button type="button" className={`touch touch--quiet${presetsOpen ? " touch--on" : ""}`} onClick={() => setPresetsOpen(!presetsOpen)}>Mes préréglages</button>
              </div>
            </>
          ) : (
            <>
              <header className="fx-head"><div><h3>Emplacement {slot + 1} libre</h3><p>Choisissez un effet à insérer.</p></div>
                <button type="button" className="touch touch--small fx-swap" onClick={() => setPicker(true)}>Choisir avec les vignettes…</button></header>
              <EffectBrowser effects={cat.effects} onAssign={(t) => void actions.setInsert(focus, slot, t)} slotLabel={`l'emplacement ${slot + 1}`} />
            </>
          )}
        </section>
      </div>
      {moduleOpen && module?.form === "module" && slot < 4 && <ModulePanel channel={focus} area="fx" slot={slot} m={module} onClose={() => setModuleOpen(false)} />}
      {picker && slot < 4 && (
        <EffectPicker title={`Effet de l'emplacement ${slot + 1} · ${channelName(cat, focus, names)}`} current={types[slot] ?? ""}
          onPick={(t) => void actions.setInsert(focus, slot, t)} onRemove={() => void actions.setInsert(focus, slot, "")} onClose={() => setPicker(false)} />
      )}
    </ScreenFrame>
  );
}

/** Choix d'un effet (v0.35.1), présenté comme le choix des instruments de PISTES : un onglet par type,
    la liste du type, et à droite la vignette (celle survolée, sinon celle choisie), le principe et
    les préréglages. Double-clic ou bouton : poser l'effet. */
function EffectBrowser({ effects, onAssign, slotLabel }: { effects: ModuleDesc[]; onAssign: (type: string) => void; slotLabel: string }) {
  const groups = groupEffects(effects);
  const [family, setFamily] = useState(groups[0]?.family ?? "");
  const [index, setIndex] = useState(0);
  const [hover, setHover] = useState<string | null>(null);
  const list = groups.find((g) => g.family === family)?.items ?? groups[0]?.items ?? [];
  const current = list[Math.min(index, list.length - 1)];
  if (!current) return null;
  const shown = effects.find((e) => e.type === hover) ?? current;
  return (
    <section className="browser fx-browser" aria-label="Choix de l'effet">
      <div className="browser-tabs" role="tablist">
        {groups.map((g) => (
          <button key={g.family} type="button" role="tab" aria-selected={g.family === family} className={g.family === family ? "on" : ""}
            onClick={() => { setFamily(g.family); setIndex(0); }}>{g.family}</button>
        ))}
      </div>
      <div className="browser-body">
        <ul className="browser-list">
          {list.map((e, i) => (
            <li key={e.type}>
              <button type="button" className={e === current ? "on" : ""} onClick={() => setIndex(i)} onDoubleClick={() => onAssign(e.type)}
                onPointerEnter={(ev) => { if (ev.pointerType === "mouse") setHover(e.type); }} onPointerLeave={() => setHover(null)}>
                <span className="bi-name">{e.name}{e.role && <small className="fx-role"> · {e.role}</small>}<span className={`fx-form fx-form--${formClass(e)}`}>{formLabel(e)}</span></span>
                <span className="bi-principle">{e.principle}</span>
              </button>
            </li>
          ))}
        </ul>
        <div className="browser-detail">
          <img className="browser-thumb fx-browser-thumb" src={fxThumbUrl(shown.type)} alt="" draggable={false} key={shown.type} />
          <h3>{shown.name}{shown.role && <small className="fx-role"> · {shown.role}</small>}</h3>
          <p>{shown.principle}</p>
          {current.presets.length > 0 && <>
            <span className="detail-label">Préréglages</span>
            <ul className="preset-list">
              {current.presets.slice(0, 12).map((p) => <li key={p}><span className="fx-preset-name">{p}</span></li>)}
            </ul>
          </>}
          <div className="browser-actions">
            <button type="button" className="touch touch--on browser-assign" onClick={() => onAssign(current.type)}>Mettre sur {slotLabel}</button>
          </div>
        </div>
      </div>
    </section>
  );
}

/** v0.47 : bouton « M » (contourner l'effet) d'un insert : assignable (MIDI, bouton assignable, scènes). */
function BypassTool({ channel, slot, name }: { channel: number; slot: number; name: string }) {
  const { binding, bypassed, toggle } = useBypass(channel, "fx", slot, name);
  return (
    <ParamMenuZone binding={binding} className="pm-zone--byp">
      <button type="button" className={`fx-tool fx-tool--mute${bypassed ? " on" : ""}`} aria-pressed={bypassed}
        title={bypassed ? "Réactiver l'effet (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)" : "Couper l'effet, contourné (appui long / clic droit : affecter au contrôleur, à un bouton, à une scène)"}
        aria-label={`Couper l'effet ${slot + 1}`} onClick={toggle}>M</button>
    </ParamMenuZone>
  );
}
