// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix30 : langue de l'interface (français / anglais). Elle est gardée par l'application (fonction native
// « setLang », fichier dans les données de l'application) : la WebView d'Android ne garde pas la mémoire de la
// page (localStorage), le choix s'y perdait à chaque rechargement. Le navigateur de développement (maquette)
// garde le choix dans localStorage.

import { getNativeFunction } from "./juce/index.js";

export type Lang = "fr" | "en";

const hasNative = (name: string) => !!window.__JUCE__?.initialisationData?.__juce__functions?.includes(name);

/** Langue connue au démarrage : celle que l'application a gardée (ou celle de l'installateur), sinon celle de la
    maquette ; null : inconnue (premier lancement : on la demande). */
export function startLang(): Lang | null {
  const data = (window.__JUCE__ as { initialisationData?: Record<string, unknown> } | undefined)?.initialisationData;
  const raw = data?.lang;
  const v = String(Array.isArray(raw) ? (raw[0] ?? "") : (raw ?? ""));
  if (v === "fr" || v === "en") return v;
  if (!hasNative("setLang")) {
    try { const s = window.localStorage.getItem("groovebox.lang"); if (s === "fr" || s === "en") return s; } catch { /* rien */ }
  }
  return null;
}

/** Garde la langue choisie (dans l'application ; dans la maquette, dans le navigateur). */
export async function saveLang(l: Lang): Promise<void> {
  try { window.localStorage.setItem("groovebox.lang", l); } catch { /* stockage indisponible (Android) */ }
  if (!hasNative("setLang")) return;
  try {
    await Promise.race([getNativeFunction("setLang")(l), new Promise((r) => window.setTimeout(r, 800))]);
  } catch { /* rien */ }
}

/** Change de langue : gardée, puis l'interface se recharge (le projet en cours reste dans le moteur). */
export async function switchLang(l: Lang): Promise<void> {
  if (document.documentElement.lang === l) return;
  await saveLang(l);
  window.location.reload();
}
