// Groovebox — OFAD — AGPL-3.0-or-later
import { actions, useStore } from "../../engine/store";

/** v0.46 fix16 : décaler l'écriture d'un pas à gauche ou à droite, en boucle (le pas qui sort revient
    de l'autre côté) ; notes, accents, glissés, conditions et verrous suivent. lane −1 : tout le pattern. */
export function ShiftButtons({ lane = -1, label, what }: { lane?: number; label: string; what: string }) {
  const track = useStore((s) => s.focus);
  return (
    <span className="steps-shift" role="group" aria-label={`Décaler ${what}`}>
      <small>{label}</small>
      <button type="button" onClick={() => void actions.shiftPattern(track, -1, lane)}
        title={`Décaler ${what} d'un pas vers la gauche (le premier pas revient à la fin ; accents, conditions et verrous suivent)`}>◀</button>
      <button type="button" onClick={() => void actions.shiftPattern(track, 1, lane)}
        title={`Décaler ${what} d'un pas vers la droite (le dernier pas revient au début ; accents, conditions et verrous suivent)`}>▶</button>
    </span>
  );
}
