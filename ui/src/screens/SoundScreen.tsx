// Groovebox — OFAD — AGPL-3.0-or-later
// Écran SON : l'instrument de la piste choisie. Onglets en haut (Global, Oscillateur, Filtre,
// Enveloppe, Amplitude, Modulation — ou Pad / Zone pour les batteries et le sampler), pages de
// 8 réglages aux encodeurs, visuel de la section et valeurs. Clic droit sur un encodeur :
// le moduler ; double-clic : valeur d'origine (ou retrait du verrou en édition de scène / de pas).

import { useEffect, useRef, useState } from "react";
import { PresetPanel } from "../controls/PresetPanel";
import { FolderBrowser } from "../controls/FolderBrowser";
import { actions, hasNative, setUi, useStore } from "../engine/store";
import { trackColor } from "../engine/params";
import { ScreenFrame } from "./ScreenFrame";
import { ModSources, PER_PAD_TABS, SectionVisual, moduleTabs, sectionsFor, useModSourceEncoders, useSectionEncoders } from "./ModuleView";

export function SoundScreen() {
  const cat = useStore((s) => s.catalog);
  const focus = useStore((s) => s.focus);
  const tracks = useStore((s) => s.tracks.tracks);
  const ui = useStore((s) => s.ui);
  const channel = useStore((s) => s.channels[s.focus]);
  const fileInput = useRef<HTMLInputElement>(null);
  const [presets, setPresets] = useState<null | "inst" | "pad">(null);
  const [library, setLibrary] = useState(false);
  const [presetCat, setPresetCat] = useState<string>(""); // banque d'usine montée dans l'onglet Global
  // autre piste ou autre préréglage choisi ailleurs : la banque affichée suit le son en place
  const presetNow = useStore((s) => s.tracks.tracks[s.focus]?.preset ?? "");
  const focusNow = useStore((s) => s.focus);
  useEffect(() => { setPresetCat(""); }, [presetNow, focusNow]);
  const isTrack = focus < (cat?.layout.numTracks ?? 16);
  const track = isTrack ? tracks[focus] : undefined;
  const module = cat?.instruments.find((i) => i.type === track?.instrument) ?? null;

  const tabs = module ? moduleTabs(module, "inst") : [];
  const tab = tabs.some((t) => t.id === ui.soundTab) ? ui.soundTab : tabs[0]?.id ?? "";
  const sections = module && tab !== "modsrc" ? sectionsFor(module, tab, ui.lane) : [];
  // Onglet Modulation : les sections de l'instrument, puis les 6 sources de la piste (clic droit)
  const chips = tab === "mod" ? [...sections.map((s) => s.name), "Modulations du son"] : sections.map((s) => s.name);
  const sectionIndex = Math.min(ui.soundSection, Math.max(0, chips.length - 1));
  const section = sections[sectionIndex] ?? null;
  const showMods = tab === "modsrc" || (tab === "mod" && !section);

  const sectionEncoders = useSectionEncoders(focus, "inst", 0, showMods ? null : section);
  const modEncoders = useModSourceEncoders(focus, "sound");
  const encoders = showMods ? modEncoders : sectionEncoders;

  const sampleIndex = module?.type === "sampler" || module?.type === "keys" ? 0 : ui.lane;
  const sampleName = channel?.samples?.[sampleIndex] ?? null;
  const takesSamples = !!module && (module.drumKit || module.type === "keys");
  const padKind = module?.drumKit ? module.padKinds[ui.lane] : undefined;

  if (!isTrack || !module) {
    return (
      <ScreenFrame title="Son" encoders={[]}>
        <div className="empty-screen">
          <h2>{isTrack ? `Piste ${focus + 1} libre` : "Retour, groupe ou bus principal choisi"}</h2>
          <p>{isTrack ? "Choisissez un instrument pour cette piste dans l'écran PISTES." : "Les retours, les groupes et le bus principal n'ont que des effets : écran EFFETS."}</p>
          <button type="button" className="touch touch--on" onClick={() => setUi({ screen: isTrack ? "tracks" : "effects" })}>{isTrack ? "Ouvrir PISTES" : "Ouvrir EFFETS"}</button>
        </div>
      </ScreenFrame>
    );
  }

  return (
    <ScreenFrame title="Son" encoders={encoders} tabs={tabs} tab={tab}
      onTab={(id) => setUi({ soundTab: id, soundSection: 0 })}
      status={<><span className="screen-spacer" /><span className="chip">P{focus + 1} · {track?.name}</span></>}>
      <div className="synth" style={{ "--k": trackColor(track?.color ?? 0) } as React.CSSProperties}>
        <header className="synth-head">
          <span className="sound-swatch" aria-hidden />
          <div>
            <h2 className="sound-name" onDoubleClick={() => setUi({ face: true })} title="Double-clic : ouvrir le synthé">{module.drumKit ? `${ui.lane + 1} · ${module.padNames[ui.lane] ?? ""}` : module.name}</h2>
            <p className="sound-kind">{module.drumKit ? `${module.name} · ` : ""}{module.principle}{track?.preset ? ` · ${track.preset}` : ""}</p>
          </div>
          <div className="sample-actions sample-actions--presets">
            <button type="button" className="touch touch--small touch--on face-open" onClick={() => setUi({ face: true })}
              title="Ouvrir l'instrument en grand, comme un synthé, avec son clavier (Échap pour refermer)">Ouvrir le synthé ↗</button>
            <button type="button" className={`touch touch--small touch--quiet${presets === "inst" ? " touch--on" : ""}`} onClick={() => setPresets(presets === "inst" ? null : "inst")}>
              {module.drumKit ? "Kit · préréglages" : "Préréglages"}
            </button>
            {module.drumKit && (
              <button type="button" className={`touch touch--small touch--quiet${presets === "pad" ? " touch--on" : ""}`} onClick={() => setPresets(presets === "pad" ? null : "pad")}>
                Pad {ui.lane + 1} · préréglages
              </button>
            )}
          </div>
          {takesSamples && (
            <div className="sample-actions">
              <span className="sample-name">{sampleName ?? (padKind === "sample" || module.type !== "drumA" ? "Pas d'échantillon" : "Voix de synthèse")}</span>
              <button type="button" className="touch touch--quiet" onClick={() => (hasNative("chooseSample") ? actions.chooseSample(focus, sampleIndex) : fileInput.current?.click())}>Charger…</button>
              <button type="button" className={`touch touch--quiet${library ? " touch--on" : ""}`} onClick={() => { setLibrary(!library); setPresets(null); }}
                title="Les échantillons du dossier choisi dans les Réglages">Bibliothèque</button>
              {sampleName && <button type="button" className="touch touch--quiet" onClick={() => void actions.clearSample(focus, sampleIndex)}>Retirer</button>}
              {module.type === "sampler" && sampleName && <>
                <button type="button" className="touch touch--quiet" onClick={() => void actions.autoSlice(focus, false, 16)}>Découper ×16</button>
                <button type="button" className="touch touch--quiet" onClick={() => void actions.autoSlice(focus, true, 16)}>Sur les attaques</button>
              </>}
              <input ref={fileInput} type="file" accept=".wav,.aif,.aiff,.flac,.ogg,.mp3,audio/*" className="sr-only"
                onChange={(e) => { const f = e.target.files?.[0]; if (f) void actions.loadSample(focus, sampleIndex, f); e.target.value = ""; }} />
            </div>
          )}
        </header>

        {chips.length > 1 && (
          <div className="section-chips" role="tablist" aria-label="Pages de réglages">
            {chips.map((name, i) => (
              <button key={name + i} type="button" role="tab" aria-selected={i === sectionIndex} className={i === sectionIndex ? "on" : ""}
                onClick={() => setUi({ soundSection: i })}>{name}</button>
            ))}
          </div>
        )}
        {(tab === "pad" || tab === "zone" || PER_PAD_TABS.includes(tab)) && (
          <div className="section-chips section-chips--pads" role="tablist" aria-label="Pad édité">
            {Array.from({ length: 16 }, (_, i) => (
              <button key={i} type="button" role="tab" aria-selected={i === ui.lane} className={i === ui.lane ? "on" : ""}
                onClick={() => setUi({ lane: i })} title={module.padNames[i]}>{i + 1}</button>
            ))}
          </div>
        )}

        {showMods ? <ModSources channel={focus} scope="sound" /> : section && (
          <div className="synth-detail">
            <div className="block block--big">
              <span className="block-title">{section.name}</span>
              <SectionVisual channel={focus} area="inst" slot={0} module={module} section={section} lane={ui.lane} />
            </div>
          </div>
        )}

        {tab === "global" && module.presets.length > 0 && (() => {
          // banques d'usine par catégorie : on n'affiche que celle choisie (50 sons et plus par synthé)
          const catOf = (i: number) => module.presetCats?.[i] || "Classiques";
          const ORDER = ["Init", "Acide", "Basses", "Leads", "Nappes", "Plucks", "Claviers", "Cordes", "Cuivres", "Arpèges", "Effets", "Classiques"];
          const cats = [...new Set(module.presets.map((_, i) => catOf(i)))].sort((x, y) => ORDER.indexOf(x) - ORDER.indexOf(y));
          const curIdx = track?.preset ? module.presets.indexOf(track.preset) : -1;
          const cat = cats.includes(presetCat) ? presetCat : curIdx >= 0 ? catOf(curIdx) : cats[0];
          return (
            <div className="presets-row">
              <span className="detail-label">Préréglages</span>
              {cats.length > 1 && (
                <div className="preset-cats" role="tablist" aria-label="Banques d'usine">
                  {cats.map((c) => (
                    <button key={c} type="button" role="tab" aria-selected={c === cat} className={c === cat ? "on" : ""} onClick={() => setPresetCat(c)}>{c}</button>
                  ))}
                </div>
              )}
              <ul className="preset-list preset-list--row">
                {module.presets.map((p, i) => (catOf(i) !== cat ? null : (
                  <li key={p}><button type="button" className={track?.preset === p ? "on" : ""} onClick={() => void actions.applyPreset(focus, -1, p)}>{p}</button></li>
                )))}
              </ul>
            </div>
          );
        })()}
        {library && takesSamples && (
          <FolderBrowser kind="samples" title={`Bibliothèque · ${module.drumKit ? `pad ${ui.lane + 1}` : "échantillon"}`}
            onPick={(rel) => actions.loadSampleFile(focus, sampleIndex, rel)} onClose={() => setLibrary(false)} />
        )}
        {presets && (
          <PresetPanel key={`${presets}:${focus}:${presets === "pad" ? ui.lane : ""}`} scope={presets} channel={focus} slot={-1} pad={presets === "pad" ? ui.lane : -1}
            title={presets === "pad" ? `Pad ${ui.lane + 1} · ${module.padNames[ui.lane] ?? ""}` : module.name}
            current={presets === "inst" ? track?.preset : undefined}
            factory={presets === "inst" ? module.presets : []} factoryCats={presets === "inst" ? module.presetCats : undefined}
            onPickFactory={(n) => void actions.applyPreset(focus, -1, n)}
            onClose={() => setPresets(null)} />
        )}
        <p className="sound-hint">Clic droit (ou appui long) sur un encodeur : valeur exacte, modulation, MIDI. Onglet Modulation : « Affecter à un réglage… » puis touchez les encodeurs. Double-clic : valeur d'origine.</p>
      </div>
    </ScreenFrame>
  );
}
