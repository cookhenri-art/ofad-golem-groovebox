// Groovebox — OFAD — AGPL-3.0-or-later
// v0.34 : choisir un effet en touchant sa vignette, rangé par type (même principe que le choix des
// instruments). Sert aux inserts des canaux et aux pédales des pads. Vignettes embarquées dans
// l'interface (ui/public/fxthumbs/<type>.webp), refaites par scripts/make-fx-thumbs.cjs.

import { useEffect } from "react";
import { useStore } from "../engine/store";
import { formClass, formLabel, groupEffects } from "../engine/effectGroups";

export const fxThumbUrl = (type: string) => `fxthumbs/${type}.webp`;

export function EffectPicker({ title, current, onPick, onRemove, onClose }: {
  title: string; current: string; onPick: (type: string) => void; onRemove?: () => void; onClose: () => void;
}) {
  const cat = useStore((s) => s.catalog);
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); onClose(); } };
    window.addEventListener("keydown", onKey, true);
    return () => window.removeEventListener("keydown", onKey, true);
  }, [onClose]);
  if (!cat) return null;
  return (
    <div className="fxpick" role="dialog" aria-modal="true" aria-label={title}>
      <header className="ipick-head">
        <h3>{title}</h3>
        <span className="ipick-hint">Toucher un effet pour le poser (Échap : garder l'effet actuel)</span>
        {current && onRemove && <button type="button" className="touch touch--small touch--quiet" onClick={() => { onRemove(); onClose(); }}>Retirer l'effet</button>}
        <button type="button" className="face-close" onClick={onClose} aria-label="Fermer le choix d'effet">✕</button>
      </header>
      <div className="ipick-body">
        {groupEffects(cat.effects).map((g) => (
          <section key={g.family} className="ipick-family">
            <h4>{g.family}</h4>
            <div className="ipick-grid fxpick-grid">
              {g.items.map((e) => (
                <button key={e.type} type="button" className={`ipick-card${e.type === current ? " is-current" : ""}`}
                  onClick={() => { if (e.type !== current) onPick(e.type); onClose(); }} title={e.principle}>
                  <span className="fxpick-img"><img src={fxThumbUrl(e.type)} alt="" loading="lazy" draggable={false} /></span>
                  <span className="ipick-name">{e.name}{e.role && <small className="fx-role"> · {e.role}</small>}<span className={`fx-form fx-form--${formClass(e)}`}>{formLabel(e)}</span>
                    {e.type === current ? " · en place" : ""}</span>
                </button>
              ))}
            </div>
          </section>
        ))}
      </div>
    </div>
  );
}
