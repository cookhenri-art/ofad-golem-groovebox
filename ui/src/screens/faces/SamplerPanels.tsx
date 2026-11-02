// Groovebox — OFAD — AGPL-3.0-or-later
// Façades des samplers (v0.28) : forme d'onde en grand (zones, découpes, boucle) et grille de
// pads qui choisit la zone / le pad réglé. Les réglages restent ceux des sections du moteur.

import { setUi, useStore } from "../../engine/store";
import type { ModuleDesc } from "../../engine/protocol";
import { SectionVisual } from "../ModuleView";

/** Forme d'onde en grand : le graphique de la section « sectionId » (zone choisie, échantillon). */
export function BigWave({ channel, module: m, sectionId }: { channel: number; module: ModuleDesc; sectionId: string }) {
  const lane = useStore((s) => s.ui.lane);
  const section = m.sections.find((s) => s.id === sectionId);
  if (!section) return null;
  return (
    <div className="fp-bigwave">
      <SectionVisual channel={channel} area="inst" slot={0} module={m} section={section} lane={lane} />
    </div>
  );
}

/** Grille 4 × 4 : toucher un pad choisit la zone / le pad dont on règle les paramètres. */
export function ZonePads({ module: m, big }: { module: ModuleDesc; big?: boolean }) {
  const lane = useStore((s) => s.ui.lane);
  // rangée du bas = pads 1 à 4, comme sur le pad de l'appareil
  const order = [12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3];
  return (
    <div className={`zone-pads${big ? " zone-pads--big" : ""}`} role="group" aria-label="Choisir la zone réglée">
      {order.map((i) => (
        <button key={i} type="button" className={`zone-pad${i === lane ? " on" : ""}`} aria-pressed={i === lane}
          onClick={() => setUi({ lane: i })}>
          <span className="zone-pad-n tabular">{i + 1}</span>
          <span className="zone-pad-name">{m.padNames[i] ?? `Pad ${i + 1}`}</span>
        </button>
      ))}
    </div>
  );
}
