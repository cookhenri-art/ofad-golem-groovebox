// Groovebox — OFAD — AGPL-3.0-or-later
// Courbes de transfert de la distorsion : mêmes formules que le moteur (src/effects/Distortion.h, dist::shape).

const fastTanh = (x: number) => { const c = Math.max(-3, Math.min(3, x)); const x2 = c * c; return c * (27 + x2) / (27 + 9 * x2); };
const sin2pi = (x: number) => Math.sin(2 * Math.PI * x);

export function shapeOf(type: number, x: number, drive01: number, bias = 0): number {
  const g = 1 + 63 * drive01 * drive01;
  if (bias === 0) return shapeRaw(type, x * g, drive01);
  return shapeRaw(type, x * g + bias, drive01) - shapeRaw(type, bias, drive01);
}

function shapeRaw(type: number, v: number, drive01: number): number {
  switch (type) {
    case 0: return fastTanh(v * 0.7);
    case 1: { const t = fastTanh(v); return fastTanh(v + 0.35 * t * t); }
    case 2: { const v2 = v * v; return v / Math.sqrt(Math.sqrt(1 + v2 * v2)); }
    case 3: return fastTanh(v) * 0.9 + 0.1 * sin2pi(v * 0.15915494309189535);
    case 4: return Math.abs(fastTanh(v)) * 2 - 1;
    case 5: return v > 0 ? fastTanh(v * 1.2) : fastTanh(v * 0.6);
    case 6: return Math.max(-1, Math.min(1, v * 2));
    case 7: { const steps = Math.floor(16.5 - drive01 * 14); return Math.floor(fastTanh(v) * steps + 0.5) / steps; }
    case 8: { const y = fastTanh(v); return Math.abs(y) > 0.6 ? -y * 0.6 : y; }
    case 9: return sin2pi(v * 0.9 * 0.15915494309189535);
    case 11: return v >= 0 ? 1 - Math.exp(-v * 1.1) : -0.55 * (1 - Math.exp(v * 1.4));
    case 12: { const a = v * 0.25 + 0.25; const f = a - Math.floor(a); return 1 - 4 * Math.abs(f - 0.5); }
    case 13: return Math.max(-0.45, Math.min(1, v * 1.5));
    default: { const v3 = v * v * v; return fastTanh(v - 0.3 * v3 / (1 + Math.abs(v3))); }
  }
}
