// Groovebox — OFAD — AGPL-3.0-or-later
// Bouton de façade à grandes inscriptions (v0.26) : le nom au-dessus et la valeur en dessous sont
// écrits hors du bouton, en gros caractères ; le bouton lui-même garde toutes ses fonctions
// (glisser, double-clic, modulation, macros, verrous de scène).

import type { CSSProperties } from "react";
import { useParam } from "../../controls/bindings";
import { Encoder, type EncoderBinding } from "../../controls/Encoder";

export function BigKnobView({ binding, label, size = 1, className }: { binding: EncoderBinding | null; label?: string; size?: number; className?: string }) {
  return (
    <div className={`bk${className ? ` ${className}` : ""}`} style={{ "--bk": size } as CSSProperties}>
      <span className="bk-label">{label ?? binding?.label ?? ""}</span>
      <div className="bk-knob"><Encoder binding={binding} variant="knob" /></div>
      <span className="bk-value tabular">{binding?.text ?? ""}</span>
    </div>
  );
}

/** Bouton d'un réglage du moteur (adresse globale). */
export function BigKnob({ global, label, size, className }: { global: number | null; label?: string; size?: number; className?: string }) {
  const b = useParam(global, label);
  return <BigKnobView binding={b} label={label} size={size} className={className} />;
}
