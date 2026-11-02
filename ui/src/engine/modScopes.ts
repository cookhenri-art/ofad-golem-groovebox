// Groovebox — OFAD — AGPL-3.0-or-later
// Familles de modulations d'une piste (rangement de l'interface ; le moteur a 6 sources par canal).

import { layout } from "./store";

/** Deux familles de modulations par piste : celles du son (sources 1 à 4, pour les réglages de
    l'instrument) et celles des effets et du mixage (sources 5 et 6 : inserts, volume, panoramique,
    envois). Le moteur garde 6 sources par canal ; l'interface les range ainsi. */
export type ModScope = "sound" | "fx";
export const MOD_SCOPE_SOURCES: Record<ModScope, number[]> = { sound: [0, 1, 2, 3], fx: [4, 5] };
export const MOD_SCOPE_TITLE: Record<ModScope, string> = { sound: "Modulations du son", fx: "Modulations des effets et du mixage" };
/** Famille d'un réglage modulé (emplacement local) : instrument → son ; mixage et inserts → effets. */
export function modScopeOf(local: number): ModScope {
  const L = layout();
  return local >= L.instrumentBase && local < L.insertBase ? "sound" : "fx";
}
