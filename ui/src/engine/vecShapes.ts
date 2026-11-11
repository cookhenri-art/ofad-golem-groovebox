// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : formes des mouvements du pavé vectoriel, partagées par l'écran du PC (VectorPad) et la
// télécommande (onglet Chaos). Mêmes sommets que le moteur (PluginProcessor, pavé vectoriel).

/** Sommets (-1..1) du mouvement choisi (1 cercle … 6 ricochet ; vide sinon). */
export function orbitShape(orbit: number): [number, number][] {
  const curve = (f: (t: number) => [number, number]) => Array.from({ length: 64 }, (_, i) => f((i / 64) * 2 * Math.PI));
  switch (orbit) {
    case 1: return curve((t) => [Math.cos(t), Math.sin(t)]);                 // cercle
    case 2: return curve((t) => [Math.sin(t), Math.sin(2 * t) * 0.5]);       // infini
    case 3: return [[0, -1], [1, 0], [0, 1], [-1, 0]];                        // losange
    case 4: return [[-1, -1], [1, -1], [1, 1], [-1, 1]];                      // carré
    case 5: return [[0, -1], [0.87, 0.5], [-0.87, 0.5]];                      // triangle
    case 6: return [[-1, -1], [-0.5, 1], [0, -1], [0.5, 1], [1, -1], [0.5, 1], [0, -1], [-0.5, 1]]; // ricochet
    default: return [];
  }
}

/** Forme tracée sur le pavé (0..100) : centre x / y (0..1), taille, rotation (degrés), hauteur ; le
    hasard (7) montre le carré qu'il parcourt. */
export function orbitPoints(orbit: number, x: number, y: number, depth: number, rotDeg: number, height: number): string {
  const rad = (rotDeg * Math.PI) / 180, rc = Math.cos(rad), rs = Math.sin(rad);
  const c = (v: number) => Math.min(1, Math.max(0, v));
  const pts = orbit === 7 ? ([[-1, -1], [1, -1], [1, 1], [-1, 1]] as [number, number][]) : orbitShape(orbit);
  return pts.map(([a, b0]) => {
    const b = b0 * height;
    const ra = a * rc - b * rs, rb = a * rs + b * rc;
    return `${c(x + ra * depth / 2) * 100},${c(y + rb * depth / 2) * 100}`;
  }).join(" ");
}
