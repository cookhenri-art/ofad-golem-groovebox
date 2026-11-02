// Groovebox — OFAD — AGPL-3.0-or-later
// Élément d'affichage d'un réglage qu'on règle en le tirant vers le haut ou le bas.

import type { ReactNode } from "react";
import { useParam } from "./bindings";
import { useValueDrag } from "./useValueDrag";

export function DragParam({ global, label, className, children }: { global: number; label: string; className?: string; children: ReactNode }) {
  const binding = useParam(global, label);
  const { handlers } = useValueDrag(binding);
  return (
    <span className={`${className ?? ""} drag-value`} role="slider" aria-label={label} aria-valuetext={binding?.text}
      title={`${label}${binding ? ` : ${binding.text}` : ""} — glisser vers le haut ou le bas`} {...handlers}>
      {children}
    </span>
  );
}
