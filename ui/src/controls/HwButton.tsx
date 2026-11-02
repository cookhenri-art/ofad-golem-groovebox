// Groovebox — OFAD — AGPL-3.0-or-later
// Bouton en caoutchouc avec voyant, sérigraphie au-dessus ou dessus.

import type { ReactNode } from "react";

interface HwButtonProps {
  label: ReactNode;
  active?: boolean;
  led?: "amber" | "red" | "green" | "blue" | "magenta" | "none";
  size?: "sm" | "md" | "lg";
  title?: string;
  disabled?: boolean;
  onPress: () => void;
  ariaLabel?: string;
}

export function HwButton({ label, active = false, led = "amber", size = "md", title, disabled, onPress, ariaLabel }: HwButtonProps) {
  return (
    <button
      type="button"
      className={`hw hw--${size}${active ? " hw--on" : ""}`}
      data-led={led}
      aria-pressed={active}
      aria-label={ariaLabel}
      title={title}
      disabled={disabled}
      onPointerDown={(e) => { if (e.button === 0) onPress(); }}
      onKeyDown={(e) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); onPress(); } }}
    >
      {led !== "none" && <span className="hw-led" aria-hidden />}
      <span className="hw-label">{label}</span>
    </button>
  );
}
