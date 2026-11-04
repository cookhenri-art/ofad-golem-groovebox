// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix28 : réglages des sources de modulation (LFO, fonctions, suiveur, séquenceur) d'une piste,
// ouverts depuis le menu d'un réglage (boutons assignables, façades, valeurs du bas) : le même panneau
// que l'onglet « Modulations de la piste », par-dessus l'écran en cours.
import { useEffect } from "react";
import { faceSkinStyle } from "../skin/skins";
import { setUi, useStore } from "../engine/store";
import { channelName } from "../engine/params";
import { ModSources, useModSourceEncoders, type KnobModMode } from "./ModuleView";
import { actions } from "../engine/store";
import { MOD_SOURCE_COLORS } from "../engine/params";
import { targetName } from "./SettingsScreen";
import { Encoder } from "../controls/Encoder";

/** Les réglages de la source choisie (vitesse, forme, synchro, phase…) en boutons, sous les sources. */
function SourceKnobs({ channel, scope }: { channel: number; scope: "sound" | "fx" }) {
  const encs = useModSourceEncoders(channel, scope);
  return (
    <div className="face-knobs face-knobs--src">
      {encs.map((b, i) => <Encoder key={i} binding={b} variant="knob" />)}
    </div>
  );
}

export function ModSourcesDialog() {
  const panel = useStore((s) => s.ui.modPanel);
  const cat = useStore((s) => s.catalog);
  const names = useStore((s) => s.tracks.tracks).map((t) => t.name);
  useEffect(() => {
    if (!panel) return;
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); setUi({ modPanel: null }); } };
    window.addEventListener("keydown", esc, true);
    return () => window.removeEventListener("keydown", esc, true);
  }, [panel]);
  const knobs = useStore((s) => s.midi.knobs);
  const modSlot = useStore((s) => s.ui.modSlot);
  const lfoAssign = useStore((s) => s.ui.knobLfoAssign);
  if (!panel || !cat) return null;
  const close = () => setUi({ modPanel: null, knobLfoAssign: null });
  // v0.46 fix28 : mode « 16 boutons » : sources 1 à 4 du bus principal, destinations = les boutons
  let km: KnobModMode | undefined;
  if (panel.knobs) {
    const src = (MOD_SOURCE_COLORS.length && modSlot >= 0 && modSlot < 4 ? modSlot : 0) + 1;
    const using = (knobs ?? []).map((kn, k) => ({ k, t: kn?.[0] ?? -1, l: kn?.[2] ?? 0, d: kn?.[3] ?? 0 })).filter((x) => x.l === src);
    km = {
      title: "Modulations des 16 boutons",
      note: "4 sources propres aux boutons assignables : chacune peut moduler plusieurs boutons (et tout ce qu'ils pilotent)",
      assigning: lfoAssign === src,
      onAssign: () => setUi({ knobLfoAssign: lfoAssign === src ? null : src }),
      count: (i) => (knobs ?? []).filter((kn) => (kn?.[2] ?? 0) === i + 1).length,
      empty: using.length === 0,
      dests: (
        <ul className="mod-dest-list">
          {using.map((x) => (
            <li key={x.k}>
              <span className="mod-dest-name">Bouton {x.k + 1}{x.t !== -1 ? ` → ${targetName(x.t)}` : " (libre)"}</span>
              <input type="range" className="mod-dest-range" min={-100} max={100} step={1} value={Math.round(x.d * 100)} aria-label={`Profondeur sur le bouton ${x.k + 1}`}
                onChange={(e) => void actions.setKnobLfo(x.k, src, Number(e.target.value) / 100)} />
              <b className="tabular">{x.d >= 0 ? "+" : "−"}{Math.round(Math.abs(x.d) * 100)} %</b>
              <button type="button" className="mod-dest-remove" onClick={() => void actions.setKnobLfo(x.k, 0, 0)} aria-label={`Retirer le bouton ${x.k + 1}`} title="Retirer ce bouton">×</button>
            </li>
          ))}
        </ul>
      ),
    };
  }
  // affectation aux boutons : le panneau s'efface pour laisser toucher les 16 boutons
  if (panel.knobs && lfoAssign !== null) {
    return (
      <div className="modsrc-assignbar" role="status" style={{ "--mod": MOD_SOURCE_COLORS[lfoAssign - 1] } as React.CSSProperties}>
        <span className="mod-dot" style={{ background: MOD_SOURCE_COLORS[lfoAssign - 1] }} />
        Touchez les boutons assignables à moduler par la source {lfoAssign} (toucher à nouveau : retirer)
        <button type="button" onClick={() => setUi({ knobLfoAssign: null })}>Terminer</button>
      </div>
    );
  }
  // v0.46 fix28 : même cadre que la façade d'un synthé (page « Modulations de la piste »), dans l'appareil
  const sk = faceSkinStyle("generic");
  return (
    <div className="face face--skin face--modsrc" role="dialog" aria-label="Sources de modulation" style={{ "--face-accent": "oklch(0.8 0.12 220)", "--face-panel": "oklch(0.2 0.01 250)", ...sk.style } as React.CSSProperties}>
      <header className="face-head face-head--2rows">
        <div className="face-head-row">
          <span className="face-stripe" aria-hidden />
          <div className="face-title">
            <span className="face-tag">{panel.knobs ? "Sources propres aux boutons assignables" : panel.scope === "sound" ? "Sources 1 à 4 (son)" : "Sources 5 et 6 (effets, mixage)"}</span>
            <h2>{panel.knobs ? "Modulations des 16 boutons" : `Modulations · ${channelName(cat, panel.channel, names)}`}</h2>
          </div>
          <button type="button" className="face-close" onClick={close} aria-label="Refermer" title="Refermer (Échap)">✕</button>
        </div>
      </header>
      <div className="face-body">
        <div className="face-modpage">
          <section className="face-module face-module--sources">
            <h3 className="face-module-title">{panel.knobs ? "Modulations des 16 boutons" : "Modulations de la piste"}</h3>
            <div className="face-sources"><ModSources channel={panel.channel} scope={panel.scope} knobs={km} /></div>
            <SourceKnobs channel={panel.channel} scope={panel.scope} />
          </section>
        </div>
      </div>
    </div>
  );
}
