// Groovebox — OFAD — AGPL-3.0-or-later
// Onglet Macros : les 8 macros du logiciel hôte. Chacune pilote jusqu'à 8 réglages (de n'importe
// quelle piste, insert ou mixage), chacun sur sa plage « De → À » (À plus petit que De : sens
// inverse). On affecte un réglage par le menu d'un encodeur (clic droit ou appui long), ou ici
// avec « Affecter », puis en touchant les encodeurs et boutons à piloter.
// v0.46 : deux modes par macro. Réel : tous les réglages prennent la position de la macro sur leur
// plage. Relatif : affecter ne change rien, chaque réglage part de sa valeur et bouge avec la macro
// (vers « De » quand elle descend, vers « À » quand elle monte) ; le retoucher à la main le réancre.

import { useEffect } from "react";
import { HOST_CTL, hostCode } from "../engine/protocol";
import { actions, resolveGlobal, setUi, useStore } from "../engine/store";
import { busName, formatPlain, toPlain } from "../engine/params";
import type { MacroData } from "../engine/protocol";
import { Encoder, type EncoderBinding } from "../controls/Encoder";
import { useParam } from "../controls/bindings";
import { useValueDrag } from "../controls/useValueDrag";

export function MacroPage() {
  const macros = useStore((s) => s.macros);
  const live = useStore((s) => s.tick.mac);
  const assign = useStore((s) => s.ui.macroAssign);
  useEffect(() => { void actions.refreshMacros(); }, []);
  return (
    <div className="macro-page">
      <p className="macro-intro">
        Chaque macro pilote jusqu'à 8 réglages à la fois, chacun sur sa plage. Le logiciel hôte les automatise (« Macro 1 » à « Macro 8 »).
        Pour affecter : « Affecter », puis touchez les encodeurs ou boutons à piloter (ailleurs aussi : menu d'un encodeur, clic droit ou appui long).
        <br /><b>Réel</b> : tous les réglages prennent la position de la macro sur leur plage. <b>Relatif</b> : chaque réglage garde sa valeur
        à l'affectation et bouge avec la macro (vers « De » en descendant, vers « À » en montant).
      </p>
      <div className="macro-grid">
        {Array.from({ length: 8 }, (_, k) => (
          <MacroCard key={k} index={k} data={macros[k]} value={live?.[k] ?? macros[k]?.v ?? 0.5} assigning={assign === k} />
        ))}
      </div>
    </div>
  );
}

function MacroCard({ index, data, value, assigning }: { index: number; data?: MacroData; value: number; assigning: boolean }) {
  const links = data?.links ?? [];
  const rel = data?.rel ?? true;
  const binding: EncoderBinding = {
    kind: "continuous", label: `Macro ${index + 1}`, text: `${Math.round(value * 100)} %`, normalised: value,
    onChange: (n) => actions.setMacroValue(index, n), reset: () => actions.setMacroValue(index, 0.5),
    ctl: hostCode(`macro${index + 1}` as (typeof HOST_CTL)[number]), // v0.46 fix24 : MIDI, bouton assignable
  };
  return (
    <section className={`macro-card${assigning ? " is-assigning" : ""}`} aria-label={`Macro ${index + 1}`}>
      <header className="macro-card-head">
        <b>Macro {index + 1}</b>
        <span className="mp-seg macro-mode" role="radiogroup" aria-label={`Mode de la macro ${index + 1}`}>
          {([[false, "Réel", "Tous les réglages prennent la position de la macro sur leur plage (l'affectation les y place)"],
            [true, "Relatif", "Chaque réglage garde sa valeur à l'affectation et bouge avec la macro"]] as const).map(([r, t, help]) => (
            <button key={t} type="button" role="radio" aria-checked={rel === r} className={`mp-lb${rel === r ? " on" : ""}`} title={help}
              onClick={() => { if (rel !== r) void actions.setMacroMode(index, r); }}><i />{t}</button>
          ))}
        </span>
        <span className="macro-count">{links.length === 0 ? "libre" : `${links.length} / 8`}</span>
      </header>
      <div className="macro-card-knob"><Encoder binding={binding} variant="knob" /></div>
      <div className="macro-card-actions">
        <button type="button" className={`touch touch--small${assigning ? " touch--on" : " touch--quiet"}`}
          onClick={() => setUi({ macroAssign: assigning ? null : index, modAssign: null })}>{assigning ? "Terminer" : "Affecter…"}</button>
        {links.length > 0 && <button type="button" className="touch touch--small touch--quiet" onClick={() => void actions.clearMacro(index)}>Tout retirer</button>}
      </div>
      {links.length === 0 ? <p className="macro-empty">Aucun réglage piloté.</p> : (
        <ul className="macro-links">
          {links.map(([g, lo, hi]) => <MacroLinkRow key={g} macro={index} global={g} lo={lo} hi={hi} />)}
        </ul>
      )}
    </section>
  );
}

function MacroLinkRow({ macro, global, lo, hi }: { macro: number; global: number; lo: number; hi: number }) {
  const r = resolveGlobal(global);
  const cat = useStore((s) => s.catalog);
  const where = r && cat ? (r.channel < cat.layout.numTracks ? `P${r.channel + 1}` : busName(cat.layout, r.channel)) : "";
  const name = r ? `${where} · ${r.module ? `${r.module.name} · ` : ""}${r.desc.name}` : `Réglage ${global}`;
  const fmt = (n: number) => (r ? formatPlain(r.desc, toPlain(r.desc, n)) : `${Math.round(n * 100)} %`);
  const now = useParam(global); // valeur actuelle du réglage (suit la macro)
  return (
    <li className="macro-link">
      <span className="macro-link-name" title={name}>{name}{now && <em className="macro-link-now tabular"> · {now.text}</em>}</span>
      <RangeEnd label={`De (macro à 0) · ${name}`} value={lo} text={fmt(lo)} onChange={(n) => void actions.setMacroRange(macro, global, n, hi)} />
      <span className="macro-link-arrow" aria-hidden>→</span>
      <RangeEnd label={`À (macro au maximum) · ${name}`} value={hi} text={fmt(hi)} onChange={(n) => void actions.setMacroRange(macro, global, lo, n)} />
      <button type="button" className="mod-dest-remove" onClick={() => void actions.toggleMacro(macro, global)} aria-label={`Retirer ${name}`} title="Retirer ce réglage de la macro">×</button>
    </li>
  );
}

/** Une borne de la plage : se règle en glissant vers le haut ou le bas (molette, double tape : retour). */
function RangeEnd({ label, value, text, onChange }: { label: string; value: number; text: string; onChange: (n: number) => void }) {
  const binding: EncoderBinding = { kind: "continuous", label, text, normalised: value, onChange };
  const { handlers } = useValueDrag(binding);
  return (
    <span className="macro-range drag-value" role="slider" aria-label={label} aria-valuetext={text} title={`${label} : ${text} — glisser vers le haut ou le bas`} {...handlers}>
      <span className="macro-range-bar"><span style={{ width: `${value * 100}%` }} /></span>
      <b className="tabular">{text}</b>
    </span>
  );
}
