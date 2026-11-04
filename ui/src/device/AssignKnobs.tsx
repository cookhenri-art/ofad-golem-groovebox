// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix24 : 16 boutons assignables (à la place des 8 encodeurs qui suivaient l'écran).
// Chacun pilote une cible choisie : appui long (clic droit) › « Assigner à… », le bouton clignote
// en bleu, puis on touche n'importe quelle valeur (bas d'un écran, bouton d'un synthé ou d'un effet,
// macro, crossfader, tempo, transport…). Chaque bouton s'apprend aussi au contrôleur MIDI.
// La valeur part au moteur, qui l'applique à la cible (au bloc audio suivant) ; une valeur d'écran
// revient à l'interface par le tick (engine/ctl.ts).

import { useEffect, useRef, useState } from "react";
import { Encoder, MidiLearnRow, sourceLabel, type EncoderBinding } from "../controls/Encoder";
import { actions, getState, setUi, showMessage, useStore } from "../engine/store";
import { MOD_SOURCE_COLORS } from "../engine/params";

/** v0.46 fix28 : ouvre le panneau des modulations des 16 boutons (le même que celui des synthés). */
export function openKnobMods() {
  const L = getState().catalog?.layout;
  if (L) setUi({ modPanel: { channel: L.masterChannel, scope: "sound", knobs: true }, modSlot: 0, knobLfoAssign: null });
}
import { MIDI_KNOB_BASE, NUM_KNOBS } from "../engine/protocol";
import { uiCode } from "../engine/ctl";
import { targetName } from "../screens/SettingsScreen";

/** Nom court d'une cible pour la légende sous le bouton (le dernier morceau « A · B · C » → « C »). */
const short = (name: string) => { const cut = name.lastIndexOf(" · "); return cut > 0 ? name.slice(cut + 3) : name; };

function KnobMenu({ k, onDone }: { k: number; onDone: () => void }) {
  const target = useStore((s) => s.midi.knobs?.[k]?.[0] ?? -1);
  useStore((s) => s.midi.ctl);
  const lfo = useStore((s) => s.midi.knobs?.[k]?.[2] ?? 0);
  const depth = useStore((s) => s.midi.knobs?.[k]?.[3] ?? 0);
  const L = useStore((s) => s.catalog?.layout);
  return (
    <>
      <span className="mod-menu-title">Bouton assignable {k + 1}{target !== -1 ? ` → ${targetName(target)}` : " · libre"}</span>
      <button type="button" className="knob-assign-go" onClick={() => {
        onDone();
        setUi({ knobAssign: k });
        showMessage(`Bouton ${k + 1} : touchez la valeur à piloter (Échap pour annuler)`);
      }}>Assigner à…</button>
      {target !== -1 && <button type="button" className="mod-remove" onClick={() => { void actions.setKnobLfo(k, 0, 0).then(() => actions.setKnobTarget(k, -1)); showMessage(`Bouton ${k + 1} libéré`); onDone(); }}>Libérer le bouton</button>}
      {/* v0.46 fix28 : la modulation d'un bouton vient de SES sources (les 4 des boutons), pas de celles de la
          piste : la retirer ou libérer le bouton rend le réglage tel qu'il était, le synthé n'en garde rien */}
      <span className="mod-menu-title">Moduler le bouton avec</span>
      {[1, 2, 3, 4].map((i) => (
        <button key={i} type="button" role="menuitemradio" aria-checked={lfo === i} className={lfo === i ? "on" : ""}
          onClick={() => { void actions.setKnobLfo(k, i, Math.abs(depth) > 0.01 ? depth : 0.5); onDone(); }}>
          <span className="mod-dot" style={{ background: MOD_SOURCE_COLORS[i - 1] }} />{L ? sourceLabel(L.masterChannel, i - 1) : `Source ${i}`}
        </button>
      ))}
      {lfo > 0 && (
        <label className="knob-depth">Profondeur
          <input type="range" min={-100} max={100} step={1} value={Math.round(depth * 100)} onChange={(e) => void actions.setKnobLfo(k, lfo, Number(e.target.value) / 100)} />
          <b className="tabular">{depth >= 0 ? "+" : "−"}{Math.round(Math.abs(depth) * 100)} %</b>
        </label>
      )}
      {lfo > 0 && <button type="button" className="mod-remove" onClick={() => { void actions.setKnobLfo(k, 0, 0); onDone(); }}>Retirer la modulation</button>}
      <button type="button" className="mod-edit" onClick={() => { openKnobMods(); onDone(); }}>∿ Régler les sources des 16 boutons…</button>
      <MidiLearnRow global={MIDI_KNOB_BASE - k} label={`Bouton assignable ${k + 1}`} onDone={onDone} />
    </>
  );
}

function AssignKnob({ k }: { k: number }) {
  const tickV = useStore((s) => s.tick.kv?.[k]);
  // v0.46 fix28 : le bouton bouge avec la modulation (LFO…) du réglage relié, comme les boutons des synthés
  const played = useStore((s) => { const o = s.tick.ko?.[k]; if (o !== undefined && o >= 0) return Math.round(o * 1000) / 1000; const e = s.tick.ke?.[k]; return e !== undefined && e >= 0 ? Math.round(e * 1000) / 1000 : null; });
  // v0.46 fix28 : modulation propre au bouton (sources des 16 boutons)
  const lfo = useStore((s) => s.midi.knobs?.[k]?.[2] ?? 0);
  const depth = useStore((s) => s.midi.knobs?.[k]?.[3] ?? 0);
  const lfoAssign = useStore((s) => s.ui.knobLfoAssign);
  const stored = useStore((s) => s.midi.knobs?.[k]?.[1] ?? 0);
  const target = useStore((s) => s.midi.knobs?.[k]?.[0] ?? -1);
  const waiting = useStore((s) => s.ui.knobAssign === k);
  useStore((s) => s.midi.ctl); // nom des valeurs d'écran
  // pendant un geste, la position suit le doigt (le tick la confirme ensuite)
  const [local, setLocal] = useState<number | null>(null);
  const idle = useRef(0);
  const hinted = useRef(0);
  useEffect(() => () => window.clearTimeout(idle.current), []);
  const v = local ?? played ?? tickV ?? stored;
  const name = target === -1 ? "" : targetName(target);
  const binding: EncoderBinding = {
    kind: "continuous",
    label: name || `Bouton ${k + 1}`,
    text: target === -1 ? "libre" : `${Math.round(v * 100)} %`,
    normalised: v,
    hint: target === -1 ? `Bouton assignable ${k + 1} : appui long (clic droit) › Assigner à…, puis touchez une valeur` : `Bouton ${k + 1} → ${name}`,
    onChange: (x) => {
      if (target === -1 && performance.now() - hinted.current > 4000) {
        hinted.current = performance.now();
        showMessage(`Bouton ${k + 1} libre : appui long (clic droit) › « Assigner à… », puis touchez une valeur`);
      }
      setLocal(x);
      window.clearTimeout(idle.current);
      idle.current = window.setTimeout(() => setLocal(null), 400);
      actions.setKnob(k, x);
    },
    sensitivity: 2.5, // v0.46 fix26 : toute la course en un geste (80 pixels vers le haut ou la droite ; Maj = fin)
    menu: (onDone) => <KnobMenu k={k} onDone={onDone} />,
  };
  return (
    <div className={`aknob${target === -1 ? " aknob--free" : ""}${waiting ? " aknob--wait" : ""}${lfo > 0 ? " aknob--lfo" : ""}${lfoAssign !== null ? " aknob--lfoassign" : ""}`}
      style={lfo > 0 || lfoAssign !== null ? { "--mod": MOD_SOURCE_COLORS[(lfoAssign ?? lfo) - 1] } as React.CSSProperties : undefined}
      onPointerDownCapture={(e) => {
        if (lfoAssign === null) return;
        e.preventDefault(); e.stopPropagation();
        if (lfo === lfoAssign) { void actions.setKnobLfo(k, 0, 0); showMessage(`Bouton ${k + 1} : modulation retirée`); }
        else { void actions.setKnobLfo(k, lfoAssign, Math.abs(depth) > 0.01 ? depth : 0.5); showMessage(`Bouton ${k + 1} modulé par la source ${lfoAssign}`); }
      }}>
      {lfo > 0 && <span className="aknob-lfo" title={`Modulé par la source ${lfo} des boutons (${Math.round(depth * 100)} %)`}>{lfo}</span>}
      <Encoder binding={binding} />
      <span className="aknob-cap" title={name || "libre"}><b>{k + 1}</b>{name ? <span>{short(name)}</span> : null}</span>
      {/* v0.46 fix28 : vue live : la fonction reliée en entier (piste · réglage), lisible sans la connaître par cœur */}
      <span className={`aknob-full${name ? "" : " is-free"}`} title={name || "libre"}><b>{k + 1}</b>{name ? name.replace(/^P\d+ · /, "") : "libre"}</span>
    </div>
  );
}

export function AssignKnobs() {
  return <>{Array.from({ length: NUM_KNOBS }, (_, k) => <AssignKnob key={k} k={k} />)}</>;
}

/** En attente d'affectation : la prochaine valeur touchée (élément marqué data-ctl / data-ctl-ui)
    est reliée au bouton. Les autres touchers passent (changer d'écran, ouvrir un synthé…). */
export function KnobAssignCapture() {
  const k = useStore((s) => s.ui.knobAssign);
  useEffect(() => {
    if (k === null) return;
    document.body.classList.add("knob-assigning");
    // le geste qui a servi à choisir ne fait rien d'autre (relâcher, clic) : écouteurs à usage unique
    const swallowRest = () => {
      const once = (ev: Event) => { ev.preventDefault(); ev.stopPropagation(); if (ev.type === "click") off(); };
      const off = () => { window.removeEventListener("pointerup", once, true); window.removeEventListener("click", once, true); };
      window.addEventListener("pointerup", once, true);
      window.addEventListener("click", once, true);
      window.setTimeout(off, 700);
    };
    const down = (e: PointerEvent) => {
      const el = (e.target as Element | null)?.closest?.("[data-ctl],[data-ctl-ui]") as HTMLElement | null;
      if (!el || el.closest(".aknob")) return;
      e.preventDefault(); e.stopPropagation();
      swallowRest();
      const label = el.dataset.ctlLabel ?? "";
      const norm = Number(el.dataset.ctlNorm ?? 0);
      setUi({ knobAssign: null });
      void (async () => {
        const code = el.dataset.ctl !== undefined ? Number(el.dataset.ctl) : await uiCode(el.dataset.ctlUi as string, label);
        if (code === null || !Number.isFinite(code)) return;
        await actions.setKnobTarget(k, code, Number.isFinite(norm) ? norm : 0);
        showMessage(`Bouton ${k + 1} → ${label || targetName(code)}`);
      })();
    };
    const key = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); setUi({ knobAssign: null }); } };
    window.addEventListener("pointerdown", down, true);
    window.addEventListener("keydown", key, true);
    return () => {
      document.body.classList.remove("knob-assigning");
      window.removeEventListener("pointerdown", down, true);
      window.removeEventListener("keydown", key, true);
    };
  }, [k]);
  return null;
}
