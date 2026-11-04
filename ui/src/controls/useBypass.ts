// Groovebox — OFAD — AGPL-3.0-or-later
// v0.47 : interrupteur Actif / Contourné d'un effet = un réglage comme les autres (mixeur : « Insert 1..4 »,
// pads de batterie : contournement de l'insert du pad). Passer par le réglage permet de l'affecter au
// contrôleur MIDI, à un bouton assignable, de le verrouiller dans une scène (mode d'édition de scène),
// et l'état affiché est celui qu'on entend (scène au crossfader, modulation comprises).
import { paramAddress, useParam } from "./bindings";
import { actions } from "../engine/store";

export function useBypass(channel: number, area: "fx" | "pfx", slot: number, name?: string) {
  const global = channel >= 0 && slot >= 0 ? (area === "fx" ? paramAddress(channel, "mix", 0, 6 + slot) : paramAddress(channel, "padbyp", slot, 0)) : null;
  const binding = useParam(global, name ? `${name} : actif / contourné` : undefined);
  const bypassed = binding?.kind === "stepped" ? binding.normalised > 0.5 : false;
  const toggle = () => {
    if (binding?.kind === "stepped") binding.onStep(bypassed ? -1 : 1);
    else if (global !== null) void actions.setParam(global, bypassed ? 0.25 : 0.75);
  };
  return { binding, bypassed, toggle };
}
