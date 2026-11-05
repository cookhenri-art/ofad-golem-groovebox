// Groovebox — OFAD — AGPL-3.0-or-later
// v0.48 : durée des notes visible dans les rangées de pas (écriture Acide et À verrous). Une note posée sur
// le pas s dont la durée dépasse un pas « couvre » les pas suivants : ils sont dessinés comme sa suite (grand
// contour, intérieur rempli). On peut toujours poser une autre note sur un pas couvert (synthé polyphonique) :
// elle s'affiche par-dessus, le contour reste visible.
import type { PatternData } from "../../engine/protocol";

export type Span = { start: number; end: number; note: number };

/** Pour chaque pas couvert par une note plus longue qu'un pas : la note qui le couvre (départ, dernier pas, hauteur).
    `lanesAt(s)` donne les lignes à lire au pas s (une seule ligne, ou les notes d'un accord). */
export function noteSpans(p: PatternData, lanesAt: (s: number) => number[]): Map<number, Span> {
  const out = new Map<number, Span>();
  const n = p.length;
  for (let s = 0; s < n; ++s) {
    let len8 = 0, note = -1;
    for (const l of lanesAt(s)) {
      const ln = p.lanes[l];
      if (!ln || (ln.vel[s] ?? 0) <= 0) continue;
      const v = ln.len[s] ?? 8;
      if (v > len8) { len8 = v; note = ln.note[s] ?? -1; }
    }
    const steps = Math.ceil(len8 / 8);
    if (steps <= 1) continue;
    const end = Math.min(n - 1, s + steps - 1);
    const span = { start: s, end, note };
    out.set(s, span);
    for (let k = s + 1; k <= end; ++k) if (!out.has(k) || out.get(k)!.start === k) out.set(k, span);
  }
  return out;
}

/** Classes d'un pas : départ, suite ou fin d'une note longue. */
export function spanClass(spans: Map<number, Span>, s: number): string {
  const sp = spans.get(s);
  if (!sp) return "";
  const c = sp.start === s ? " len-start" : " len-cov";
  return c + (sp.end === s ? " len-end" : "");
}
