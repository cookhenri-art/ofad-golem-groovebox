// Groovebox — OFAD — AGPL-3.0-or-later
// v0.32 : choisir l'instrument de la piste en touchant l'image de sa façade (depuis la façade).
// Les miniatures sont embarquées dans l'interface (ui/public/thumbs/<type>.webp, ~10 ko chacune).

import { useEffect } from "react";
import { actions, useStore } from "../engine/store";

/** Adresse de la miniature d'un instrument (relative : marche dans la vue web et dans l'aperçu). */
export const thumbUrl = (type: string) => `thumbs/${type}.webp`;

/** full (v0.46) : grand choix sur toute la fenêtre, comme le choix d'effet (depuis l'écran Pistes). */
export function InstrumentPicker({ track, onClose, full = false }: { track: number; onClose: () => void; full?: boolean }) {
  const cat = useStore((s) => s.catalog);
  const current = useStore((s) => s.tracks.tracks[track]?.instrument ?? "");
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === "Escape") { e.stopPropagation(); onClose(); } };
    window.addEventListener("keydown", onKey, true);
    return () => window.removeEventListener("keydown", onKey, true);
  }, [onClose]);
  if (!cat) return null;
  const families = cat.instruments.map((i) => i.family).filter((f, i, a) => a.indexOf(f) === i);
  const pick = async (type: string) => {
    if (type !== current) await actions.setInstrument(track, type);
    onClose();
  };
  return (
    <div className={full ? "fxpick ipick-full" : "ipick"} role="dialog" aria-modal={full || undefined} aria-label={`Choisir l'instrument de la piste ${track + 1}`}>
      <header className="ipick-head">
        <h3>Changer l'instrument de la piste {track + 1}</h3>
        <span className="ipick-hint">Toucher une façade pour la mettre sur la piste (Échap : garder l'instrument actuel)</span>
        <button type="button" className="face-close" onClick={onClose} aria-label="Fermer le choix d'instrument">✕</button>
      </header>
      <div className="ipick-body">
        {families.map((f) => (
          <section key={f} className="ipick-family">
            <h4>{f}</h4>
            <div className="ipick-grid">
              {cat.instruments.filter((i) => i.family === f).map((i) => (
                <button key={i.type} type="button" className={`ipick-card${i.type === current ? " is-current" : ""}`}
                  onClick={() => void pick(i.type)} title={i.principle}>
                  <img src={thumbUrl(i.type)} alt="" loading="lazy" draggable={false} />
                  <span className="ipick-name">{i.name}{i.type === current ? " · sur la piste" : ""}</span>
                </button>
              ))}
            </div>
          </section>
        ))}
      </div>
    </div>
  );
}
