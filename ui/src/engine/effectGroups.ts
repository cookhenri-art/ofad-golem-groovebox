// Groovebox — OFAD — AGPL-3.0-or-later
// v0.34 : effets rangés par type (familles décrites par le moteur) et forme (pédalier ou rack).

import type { ModuleDesc } from "./protocol";

/** Ordre d'affichage des familles d'effets (une famille inconnue va à la fin, dans l'ordre du catalogue). */
export const FX_FAMILIES = ["Égaliseurs", "Compresseurs et dynamique", "Saturation", "Modulation", "Échos", "Réverbérations", "Filtres et performance", "Mastering"];

/** Familles dont l'afficheur montre la réduction de gain (sinon le niveau de sortie). */
export const GAIN_REDUCTION_FAMILIES = new Set(["Égaliseurs", "Compresseurs et dynamique", "Mastering"]);

export function groupEffects(effects: ModuleDesc[]): { family: string; items: ModuleDesc[] }[] {
  const order = [...FX_FAMILIES, ...effects.map((e) => e.family).filter((f, i, a) => !FX_FAMILIES.includes(f) && a.indexOf(f) === i)];
  return order.map((family) => ({ family, items: effects.filter((e) => e.family === family) })).filter((g) => g.items.length > 0);
}

/** « Pédalier », « Rack » ou « Module » (interface dédiée, v0.35.1). */
export function formLabel(m: ModuleDesc): string {
  return m.form === "pedal" ? "Pédalier" : m.form === "module" ? "Module" : "Rack";
}
/** Classe CSS de l'étiquette de forme. */
export function formClass(m: ModuleDesc): string {
  return m.form === "pedal" ? "pedal" : m.form === "module" ? "module" : "rack";
}
