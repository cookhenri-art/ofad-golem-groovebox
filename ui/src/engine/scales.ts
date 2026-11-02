// Groovebox — OFAD — AGPL-3.0-or-later
// Gammes des pads pour les instruments mélodiques : les 16 pads jouent 16 degrés de la gamme
// choisie, à partir de la tonique, en montant d'octave en octave.

// Ordre figé : la gamme choisie est rangée par son numéro (on ajoute à la fin).
export const SCALES: { name: string; steps: number[]; group: string }[] = [
  { name: "Chromatique", steps: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11], group: "Chromatique" },
  { name: "Majeure (ionien)", steps: [0, 2, 4, 5, 7, 9, 11], group: "Modes" },
  { name: "Mineure (éolien)", steps: [0, 2, 3, 5, 7, 8, 10], group: "Modes" },
  { name: "Mineure harmonique", steps: [0, 2, 3, 5, 7, 8, 11], group: "Mineures et majeures" },
  { name: "Dorien", steps: [0, 2, 3, 5, 7, 9, 10], group: "Modes" },
  { name: "Phrygien", steps: [0, 1, 3, 5, 7, 8, 10], group: "Modes" },
  { name: "Mixolydien", steps: [0, 2, 4, 5, 7, 9, 10], group: "Modes" },
  { name: "Pentatonique mineure", steps: [0, 3, 5, 7, 10], group: "Pentatoniques et blues" },
  { name: "Pentatonique majeure", steps: [0, 2, 4, 7, 9], group: "Pentatoniques et blues" },
  { name: "Blues", steps: [0, 3, 5, 6, 7, 10], group: "Pentatoniques et blues" },
  // v0.44.1 : les autres modes et gammes courantes
  { name: "Lydien", steps: [0, 2, 4, 6, 7, 9, 11], group: "Modes" },
  { name: "Locrien", steps: [0, 1, 3, 5, 6, 8, 10], group: "Modes" },
  { name: "Mineure mélodique", steps: [0, 2, 3, 5, 7, 9, 11], group: "Mineures et majeures" },
  { name: "Majeure harmonique", steps: [0, 2, 4, 5, 7, 8, 11], group: "Mineures et majeures" },
  { name: "Dorien ♯4", steps: [0, 2, 3, 6, 7, 9, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Phrygien dominant", steps: [0, 1, 4, 5, 7, 8, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Lydien ♯2", steps: [0, 3, 4, 6, 7, 9, 11], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Ionien ♯5", steps: [0, 2, 4, 5, 8, 9, 11], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Dorien ♭2", steps: [0, 1, 3, 5, 7, 9, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Lydien augmenté", steps: [0, 2, 4, 6, 8, 9, 11], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Lydien dominant", steps: [0, 2, 4, 6, 7, 9, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Mixolydien ♭6", steps: [0, 2, 4, 5, 7, 8, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Locrien ♮2", steps: [0, 2, 3, 5, 6, 8, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Altérée (super-locrien)", steps: [0, 1, 3, 4, 6, 8, 10], group: "Modes de la mineure harmonique et mélodique" },
  { name: "Blues majeur", steps: [0, 2, 3, 4, 7, 9], group: "Pentatoniques et blues" },
  { name: "Pentatonique égyptienne", steps: [0, 2, 5, 7, 10], group: "Pentatoniques et blues" },
  { name: "Par tons", steps: [0, 2, 4, 6, 8, 10], group: "Symétriques" },
  { name: "Diminuée (ton, demi-ton)", steps: [0, 2, 3, 5, 6, 8, 9, 11], group: "Symétriques" },
  { name: "Diminuée (demi-ton, ton)", steps: [0, 1, 3, 4, 6, 7, 9, 10], group: "Symétriques" },
  { name: "Bebop dominante", steps: [0, 2, 4, 5, 7, 9, 10, 11], group: "Jazz" },
  { name: "Bebop majeure", steps: [0, 2, 4, 5, 7, 8, 9, 11], group: "Jazz" },
  { name: "Double harmonique (arabe)", steps: [0, 1, 4, 5, 7, 8, 11], group: "Du monde" },
  { name: "Hongroise mineure", steps: [0, 2, 3, 6, 7, 8, 11], group: "Du monde" },
  { name: "Napolitaine mineure", steps: [0, 1, 3, 5, 7, 8, 11], group: "Du monde" },
  { name: "Napolitaine majeure", steps: [0, 1, 3, 5, 7, 9, 11], group: "Du monde" },
  { name: "Persane", steps: [0, 1, 4, 5, 6, 8, 11], group: "Du monde" },
  { name: "Hirajoshi (japonaise)", steps: [0, 2, 3, 7, 8], group: "Du monde" },
  { name: "In sen (japonaise)", steps: [0, 1, 5, 7, 10], group: "Du monde" },
  { name: "Pélog (balinaise)", steps: [0, 1, 3, 7, 8], group: "Du monde" },
  { name: "Énigmatique", steps: [0, 1, 4, 6, 8, 10, 11], group: "Du monde" },
];

/** Groupes de gammes, dans l'ordre d'affichage. */
export const SCALE_GROUPS = [...new Set(SCALES.map((s) => s.group))];

export const ROOTS = ["Do", "Do♯", "Ré", "Ré♯", "Mi", "Fa", "Fa♯", "Sol", "Sol♯", "La", "La♯", "Si"];

/** Note MIDI jouée par un pad. Batteries : numéro du pad. */
export function padNote(pad: number, drum: boolean, octave: number, scale = 0, root = 0): number {
  if (drum) return pad;
  const st = SCALES[scale]?.steps ?? SCALES[0].steps;
  const n = st.length;
  const note = 48 + root + octave * 12 + Math.floor(pad / n) * 12 + st[pad % n];
  return Math.max(0, Math.min(127, note));
}

/** Vrai si la note appartient à la gamme (affichage du rouleau de notes). */
export function inScale(note: number, scale: number, root: number): boolean {
  const st = SCALES[scale]?.steps ?? SCALES[0].steps;
  return st.includes((((note - root) % 12) + 12) % 12);
}
