// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46.2 : pinceau des rangées de pas (façades des boîtes à rythmes et des synthés, rangée à
// verrous, pads en mode Pas). Appuyer sur un pas puis glisser sur ses voisins : tous les pas
// survolés prennent l'état que le premier aurait pris (posé s'il était vide, retiré s'il était
// posé), en un seul geste, au doigt comme à la souris. Un simple toucher garde son effet habituel
// (et l'appui long son menu) : le pinceau ne démarre qu'en quittant le premier pas.
//
// Les pas portent data-paint-step="<n>" et sont dans un même conteneur (la rangée) : au doigt, le
// pointeur reste capturé par le premier pas ; on retrouve donc la case survolée par sa position.

interface Stroke {
  id: number;
  row: Element;
  origin: number;
  mode: "set" | "erase" | null;
  armed?: boolean;               // pads : le geste ne compte comme pinceau qu'en quittant le premier pas
  done: Set<number>;
  isOn: (step: number) => boolean;
  toggle: (step: number) => void;
  onStart?: () => void;
}

let stroke = null as Stroke | null;
let paintedAt = 0;

/** Vrai juste après un geste de pinceau : le clic (ou le relâché) du premier pas ne doit rien faire. */
export function justPainted(): boolean { return (stroke !== null && stroke.mode !== null && stroke.armed !== false) || performance.now() - paintedAt < 400; }

function stepUnder(x: number, y: number, row: Element): number | null {
  const el = document.elementFromPoint(x, y)?.closest("[data-paint-step]");
  if (!el || !row.contains(el) || (el as HTMLButtonElement).disabled) return null;
  const n = Number((el as HTMLElement).dataset.paintStep);
  return Number.isFinite(n) ? n : null;
}

function apply(s: Stroke, step: number) {
  if (s.done.has(step)) return;
  s.done.add(step);
  if (s.isOn(step) !== (s.mode === "set")) s.toggle(step);
}

function move(e: PointerEvent) {
  const s = stroke;
  if (!s || e.pointerId !== s.id) return;
  const step = stepUnder(e.clientX, e.clientY, s.row);
  if (step === null) return;
  if (s.armed === false) { if (step === s.origin) return; s.armed = true; }
  if (s.mode === null) {
    if (step === s.origin) return;
    s.mode = s.isOn(s.origin) ? "erase" : "set";
    s.onStart?.();
    apply(s, s.origin);
  }
  apply(s, step);
}

function end(e: PointerEvent) {
  const s = stroke;
  if (!s || e.pointerId !== s.id) return;
  if (s.mode !== null && s.armed !== false) paintedAt = performance.now();
  stroke = null;
  window.removeEventListener("pointermove", move);
  window.removeEventListener("pointerup", end);
  window.removeEventListener("pointercancel", end);
}

/** À appeler dans le onPointerDown d'un pas (bouton principal). « row » : la rangée qui contient
    les pas ; isOn / toggle lisent et changent l'état ACTUEL du pas (lu dans le magasin). */
export function beginStepPaint(e: React.PointerEvent, step: number, opts: {
  isOn: (step: number) => boolean; toggle: (step: number) => void; onStart?: () => void;
  /** Le premier pas a déjà été basculé à l'appui (pads) : son état AVANT l'appui décide du geste. */
  originWasOn?: boolean;
}) {
  if (e.button !== 0) return;
  const row = (e.currentTarget as Element).closest("[data-paint-row]");
  if (!row) return;
  if (stroke) end({ pointerId: stroke.id } as PointerEvent);
  const { originWasOn, ...rest } = opts;
  stroke = { id: e.pointerId, row, origin: step, mode: null, done: new Set(), ...rest };
  if (originWasOn !== undefined) { stroke.mode = originWasOn ? "erase" : "set"; stroke.done.add(step); stroke.armed = false; }
  window.addEventListener("pointermove", move);
  window.addEventListener("pointerup", end);
  window.addEventListener("pointercancel", end);
}
