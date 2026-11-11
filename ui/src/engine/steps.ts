// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : numérotation des pas comme sur les boîtes à rythmes : chaque mesure (page de 16 pas) repart de
// 1 à 16 (repères 1, 5, 9, 13), la mesure est indiquée à part. Le moteur garde ses index 0..63.

/** Numéro du pas dans sa mesure : 1 à 16. */
export const stepInBar = (s: number) => (s % 16) + 1;
/** Mesure du pas : 1, 2, 3… */
export const barOfStep = (s: number) => Math.floor(s / 16) + 1;
/** « 5 » dans la mesure 1, « 5 (mesure 3) » au-delà : messages, infobulles et lecteurs d'écran. */
export const stepLabel = (s: number) => (s < 16 ? String(stepInBar(s)) : `${stepInBar(s)} (mesure ${barOfStep(s)})`);
