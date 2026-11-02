// Groovebox — OFAD — AGPL-3.0-or-later
// Cadre commun des écrans : barre d'état (titre, onglets, transport, position, tempo), contenu,
// et en bas les libellés des 8 encodeurs placés juste sous l'écran (comme sur une machine à écran).

import { createContext, useContext, useEffect, type ReactNode } from "react";
import { createPortal } from "react-dom";
import { Encoder, type EncoderBinding } from "../controls/Encoder";
import { actions, getState, setUi, useStore } from "../engine/store";
import { IconPlay, IconRec, IconStop } from "../controls/Icons";
import { MOD_SOURCE_COLORS, modSourceShort } from "../engine/params";
import { useValueDrag } from "../controls/useValueDrag";

export const EncoderRowContext = createContext<HTMLElement | null>(null);

interface ScreenFrameProps {
  title: string;
  status?: ReactNode;
  tabs?: { id: string; label: string }[];
  tab?: string;
  onTab?: (id: string) => void;
  encoders: (EncoderBinding | null)[];
  children: ReactNode;
}

/** Libellé et valeur d'un encodeur, sur l'écran. On peut aussi tirer la valeur vers le haut ou
    le bas directement ici (souris ou doigt), comme sur l'encodeur. */
function EncoderLabel({ b }: { b: EncoderBinding | null }) {
  const target = b && b.kind === "continuous" ? b.mod : undefined;
  const mod = useStore((s) => (target ? s.channels[target.channel]?.mods.find((m) => m.target === target.local) : undefined));
  const { handlers } = useValueDrag(b);
  return (
    <div className={`enc-label${b ? " enc-label--drag" : " enc-label--idle"}${b && b.locked ? ` enc-label--lock-${b.locked}` : ""}`}
      title={b ? `${b.label} : glisser vers le haut ou le bas (Maj = fin, double-clic = valeur d'origine)` : undefined} {...handlers}>
      <span className="enc-name">{b?.label ?? "—"}</span>
      <span className="enc-value tabular">{b?.text ?? ""}</span>
      {mod && (
        <span className="enc-mod tabular" style={{ color: MOD_SOURCE_COLORS[mod.source] }}>
          {modSourceShort(mod.source)} {mod.amount >= 0 ? "+" : "−"}{Math.round(Math.abs(mod.amount) * 100)} %
        </span>
      )}
    </div>
  );
}

/** v0.43 : barre d'avancement de la boucle la plus longue (pas la position depuis le début, inutile au
    mixage) : un trait qui avance mesure par mesure et repart au début de la boucle. */
function LoopBar({ playing, lq, lp }: { playing: boolean; lq: number; lp: number }) {
  const bars = lq > 0 ? Math.max(1, Math.round(lq / 4)) : 0;
  const on = playing && bars > 0;
  const bar = on ? Math.min(bars, Math.floor(lp * bars) + 1) : 1;
  return (
    <span className="screen-loop" role="progressbar" aria-label="Avancée dans la boucle la plus longue" aria-valuemin={0} aria-valuemax={100}
      aria-valuenow={on ? Math.round(lp * 100) : 0} title={on ? `Boucle la plus longue : ${bars} mesure${bars > 1 ? "s" : ""} — mesure ${bar}` : "Boucle la plus longue (à l'arrêt)"}>
      <span className="screen-loop-track">
        {bars > 1 && bars <= 32 && Array.from({ length: bars - 1 }, (_, i) => <i key={i} style={{ left: `${((i + 1) / bars) * 100}%` }} />)}
        <b style={{ width: `${on ? lp * 100 : 0}%` }} />
      </span>
      <span className="screen-loop-text tabular">{on ? `${bar}/${bars}` : "—"}</span>
    </span>
  );
}

/** Tempo de la barre d'état : se règle en le tirant vers le haut ou le bas (0,1 BPM par cran
    avec Maj), comme toutes les valeurs de l'écran. */
function TempoReadout({ bpm }: { bpm: number }) {
  const g = useStore((s) => s.tick.g.bpm);
  const binding: EncoderBinding = {
    kind: "continuous", label: "Tempo", text: `${g.toFixed(1)} BPM`, normalised: (g - 40) / 200,
    onChange: (v) => actions.setGlobal("bpm", Math.round((40 + v * 200) * 10) / 10),
    reset: () => actions.setGlobal("bpm", 120),
    sensitivity: 0.25, // 0,25 BPM par pixel (Maj : 0,06)
  };
  const { handlers } = useValueDrag(binding);
  return (
    <span className="screen-bpm screen-bpm--drag tabular" role="slider" aria-label="Tempo" aria-valuemin={40} aria-valuemax={240} aria-valuenow={g}
      title="Tempo : glisser vers le haut ou le bas (Maj = fin, double-clic = 120)" {...handlers}>
      {bpm.toFixed(1)} <small>BPM</small>
    </span>
  );
}

/** Bandeau des modes où les encodeurs n'écrivent PAS dans le son de base : verrous de pas,
    scène A / B / Break, affectation de modulation. Toujours visible, avec sa sortie (Échap). */
export function ModeBar() {
  const editSlot = useStore((s) => s.ui.editSlot);
  const editBreak = useStore((s) => s.ui.editBreak);
  const stepEdit = useStore((s) => s.ui.stepEdit);
  const step = useStore((s) => s.ui.step);
  const selCount = useStore((s) => s.ui.selection.length);
  const assign = useStore((s) => s.ui.modAssign);
  const macroAssign = useStore((s) => s.ui.macroAssign);
  const mod = useStore((s) => (assign ? s.channels[assign.channel]?.mod[assign.source]?.[0] ?? 0 : 0));

  useEffect(() => {
    const esc = (e: KeyboardEvent) => {
      if (e.key !== "Escape" || e.defaultPrevented) return;
      const u = getState().ui;
      if (u.modAssign) setUi({ modAssign: null });
      else if (u.macroAssign !== null) setUi({ macroAssign: null });
      else if (u.stepEdit) setUi({ stepEdit: false });
      else if (u.editSlot) setUi({ editSlot: null });
      else if (u.editBreak) setUi({ editBreak: false });
      else return;
      e.preventDefault(); // un seul Échap = une seule sortie (deux bandeaux peuvent être montés)
    };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  }, []);

  let kind = "", text = "", end: (() => void) | null = null, color: string | undefined;
  if (assign) {
    const name = `${["LFO", "Fonction", "Suiveur", "Séquenceur"][Math.min(3, Math.floor(mod * 4))]} ${assign.source + 1}`;
    kind = "assign"; color = MOD_SOURCE_COLORS[assign.source];
    text = `Affectation ${name} : touchez un encodeur en couleur pour le moduler (à nouveau : retirer)`;
    end = () => setUi({ modAssign: null });
  } else if (macroAssign !== null) {
    kind = "assign"; color = "oklch(0.82 0.15 85)";
    text = `Macro ${macroAssign + 1} : touchez les encodeurs ou boutons qu'elle doit piloter (à nouveau : retirer ; 8 au plus)`;
    end = () => setUi({ macroAssign: null });
  } else if (stepEdit && (selCount > 1 || step !== null)) {
    kind = "A";
    text = selCount > 1 ? `Verrous de ${selCount} pas : les encodeurs règlent ces pas seulement, pas le son de base`
      : `Verrous du pas ${(step ?? 0) + 1} : les encodeurs règlent ce pas seulement, pas le son de base`;
    end = () => setUi({ stepEdit: false });
  } else if (editSlot) {
    kind = editSlot; text = `Édition de la scène ${editSlot} : les encodeurs écrivent dans la scène, le son de base ne change pas.`;
    end = () => setUi({ editSlot: null });
  } else if (editBreak) {
    kind = "B"; text = "Édition de la scène Break : les encodeurs écrivent dans le Break.";
    end = () => setUi({ editBreak: false });
  }
  if (!end) return <div className="screen-mode screen-mode--off" />;
  return (
    <div className={`screen-mode screen-mode--${kind}`} role="status" style={color ? { "--assign": color } as React.CSSProperties : undefined}>
      <span className="screen-mode-text">{text}</span>
      <button type="button" onClick={end}>Terminer <kbd>Échap</kbd></button>
    </div>
  );
}

export function ScreenFrame({ title, status, tabs, tab, onTab, encoders, children }: ScreenFrameProps) {
  const row = useContext(EncoderRowContext);
  const t = useStore((s) => s.tick.t);
  const message = useStore((s) => s.ui.message);
  const learn = useStore((s) => s.tick.learn ?? -1);
  const slots = Array.from({ length: 8 }, (_, i) => encoders[i] ?? null);

  return (
    <div className="screen-frame">
      <header className={`screen-status${tabs ? " screen-status--tabs" : ""}`}>
        <span className="screen-title">{title}</span>
        {tabs && (
          <span className="screen-tabs" role="tablist" aria-label={`Sections de l'écran ${title}`}>
            {tabs.map((x) => (
              <button key={x.id} type="button" role="tab" aria-selected={x.id === tab} className={x.id === tab ? "on" : ""} onClick={() => onTab?.(x.id)}>{x.label}</button>
            ))}
          </span>
        )}
        {status}
        <span className="screen-spacer" />
        {(t.fill || t.brk) && <span className="chip chip--edit chip--B">{t.brk ? "BREAK" : "FILL"}</span>}
        {learn !== -1 && <button type="button" className="chip chip--edit chip--A chip--learn" onClick={() => void actions.midiLearn(-1)} title="Annuler l'apprentissage">MIDI : touchez une commande…</button>}
        <span className={`transport-state${t.rec ? " rec" : ""}${t.playing ? " play" : ""}`}>
          {t.rec ? <><IconRec /> ENR</> : t.playing ? <><IconPlay /> LECTURE</> : <><IconStop /> ARRÊT</>}
        </span>
        <LoopBar playing={t.playing} lq={t.lq ?? 0} lp={t.lp ?? 0} />
        <TempoReadout bpm={t.bpm} />
      </header>

      <ModeBar />

      <div className="screen-content">
        {children}
        {message && <div className="screen-toast" role="status">{message}</div>}
      </div>

      <footer className="screen-encoders" aria-hidden>
        {slots.map((b, i) => <EncoderLabel key={i} b={b} />)}
      </footer>

      {row && createPortal(slots.map((b, i) => <Encoder key={i} binding={b} />), row)}
    </div>
  );
}
