// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix30 : langue de l'interface (français / anglais). Elle est gardée par l'application (fonction native
// « setLang », fichier dans les données de l'application) : la WebView d'Android ne garde pas la mémoire de la
// page (localStorage), le choix s'y perdait à chaque rechargement. Le navigateur de développement (maquette)
// garde le choix dans localStorage.

import { getNativeFunction } from "./juce/index.js";

export type Lang = "fr" | "en";

const hasNative = (name: string) => !!window.__JUCE__?.initialisationData?.__juce__functions?.includes(name);

const asLang = (raw: unknown): Lang | null => {
  const v = String(Array.isArray(raw) ? (raw[0] ?? "") : (raw ?? ""));
  return v === "fr" || v === "en" ? v : null;
};

/** Langue connue au démarrage : celle que l'application a gardée (ou celle de l'installateur), sinon celle de la
    maquette ; null : inconnue (premier lancement : on la demande).
    v0.46 fix30f : elle est redemandée à l'application à chaque chargement (fonction « getLang ») : les données
    d'initialisation sont figées à l'ouverture de la fenêtre, et l'interface rechargée après un changement de langue
    y retrouvait l'ancienne (la langue tournait en rond sur Android). */
export async function startLang(): Promise<Lang | null> {
  if (hasNative("getLang")) {
    try {
      const v = await Promise.race([getNativeFunction("getLang")(), new Promise((r) => window.setTimeout(() => r(undefined), 800))]);
      if (v !== undefined) return asLang(v);
    } catch { /* ancienne version de l'application : données d'initialisation */ }
  }
  const data = (window.__JUCE__ as { initialisationData?: Record<string, unknown> } | undefined)?.initialisationData;
  const l = asLang(data?.lang);
  if (l) return l;
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
