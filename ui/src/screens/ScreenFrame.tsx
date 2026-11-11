// Groovebox — OFAD — AGPL-3.0-or-later
// Cadre commun des écrans : barre d'état (titre, onglets, transport, position, tempo), contenu,
// et en bas les libellés des 8 encodeurs placés juste sous l'écran (comme sur une machine à écran).

import { createContext, useContext, useEffect, useRef, type ReactNode } from "react";
import { ParamMenuZone, type EncoderBinding } from "../controls/Encoder";
import { bindingCode, registerCtl } from "../engine/ctl";
import { actions, getState, globalOf, setUi, useStore } from "../engine/store";
import type { ModAssign } from "../engine/protocol";
import { IconPlay, IconRec, IconStop } from "../controls/Icons";
import { MOD_SOURCE_COLORS, modSourceShort } from "../engine/params";
import { useValueDrag } from "../controls/useValueDrag";
import { TempoValue } from "../controls/TempoBox";

export const EncoderRowContext = createContext<HTMLElement | null>(null);
/** 1.0 : l'écran est en vue agrandie (téléphone ou PC) — certains écrans changent de disposition. */
export const ZoomContext = createContext(false);
/** 1.0 (7 nov.) : outils de l'en-tête de l'appareil (annuler / rétablir, enregistreur audio) montrés dans la
    barre d'état de chaque écran au téléphone : la vue agrandie, seule vue, n'a plus l'en-tête de l'appareil. */
export const HeaderExtraContext = createContext<ReactNode>(null);

interface ScreenFrameProps {
  title: string;
  status?: ReactNode;
  tabs?: { id: string; label: string }[];
  tab?: string;
  onTab?: (id: string) => void;
  encoders: (EncoderBinding | null)[];
  children: ReactNode;
}

/** 1.1 : profondeur de la modulation (« M1 +30 % ») sous la valeur : elle se règle en la tirant vers le
    haut ou le bas (−100 → +100 % en 260 px, Maj : fin), comme la petite molette des façades ; double-clic :
    0 %. La courbe de l'écran SON montre l'amplitude. Le glissé part de la profondeur affichée. */
function ModDepth({ channel, mod }: { channel: number; mod: ModAssign }) {
  const drag = useRef<{ y: number; a: number; id: number; moved: boolean } | null>(null);
  const set = (a: number) => actions.setParam(globalOf(channel, mod.amountSlot), (Math.max(-1, Math.min(1, a)) + 1) / 2);
  return (
    <span className="enc-mod enc-mod--drag tabular" style={{ color: MOD_SOURCE_COLORS[mod.source] }} role="slider"
      aria-label={`Profondeur de ${modSourceShort(mod.source)}`} aria-valuetext={`${Math.round(mod.amount * 100)} %`}
      title="Profondeur de la modulation : glisser vers le haut ou le bas (double-clic : 0 %)"
      onPointerDown={(e) => { if (e.button !== 0) return; e.stopPropagation(); e.currentTarget.setPointerCapture(e.pointerId); drag.current = { y: e.clientY, a: mod.amount, id: e.pointerId, moved: false }; }}
      onPointerMove={(e) => {
        e.stopPropagation();
        const d = drag.current;
        if (!d || d.id !== e.pointerId) return;
        const dy = d.y - e.clientY;
        if (!d.moved && Math.abs(dy) < 3) return;
        d.moved = true;
        set(d.a + (dy / 130) * (e.shiftKey ? 0.25 : 1));
      }}
      onPointerUp={(e) => { e.stopPropagation(); drag.current = null; }}
      onPointerCancel={() => { drag.current = null; }}
      onWheel={(e) => { e.stopPropagation(); set(mod.amount + (e.deltaY < 0 ? 0.02 : -0.02)); }}
      onDoubleClick={(e) => { e.stopPropagation(); set(0); }}
      // clic droit / appui long : le menu du réglage (celui de l'encodeur), jamais le menu du navigateur ;
      // pendant un glissé de la profondeur (appui long au doigt), ni l'un ni l'autre
      onContextMenu={(e) => { e.preventDefault(); if (drag.current?.moved) e.stopPropagation(); }}>
      {modSourceShort(mod.source)} {mod.amount >= 0 ? "+" : "−"}{Math.round(Math.abs(mod.amount) * 100)} %
    </span>
  );
}

/** Libellé et valeur d'un réglage, en bas de l'écran. On tire la valeur vers le haut ou le bas
    (souris ou doigt). v0.46 fix24 : appui long (clic droit) : le menu du réglage — valeur exacte,
    modulation et macros s'il y a lieu, MIDI, bouton assignable. */
function EncoderLabel({ b, screen }: { b: EncoderBinding | null; screen: string }) {
  const target = b && b.kind === "continuous" ? b.mod : undefined;
  const mod = useStore((s) => (target ? s.channels[target.channel]?.mods.find((m) => m.target === target.local) : undefined));
  // valeur propre à l'écran (pas un réglage du moteur) : identifiant stable « Écran.Libellé »
  const bb: EncoderBinding | null = b && bindingCode(b) === null && !b.ctlId ? { ...b, ctlId: `${screen}.${b.label}` } : b;
  if (bb?.ctlId) registerCtl(bb.ctlId, bb);
  const { handlers } = useValueDrag(bb);
  return (
    <ParamMenuZone binding={bb} preferBelow={false} className="enc-label-zone">
      <div className={`enc-label${bb ? " enc-label--drag" : " enc-label--idle"}${bb && bb.locked ? ` enc-label--lock-${bb.locked}` : ""}`}
        title={bb ? `${bb.label} : glisser vers le haut ou le bas (Maj = fin, double-clic = valeur d'origine) — appui long ou clic droit : menu (valeur exacte, MIDI, bouton assignable)` : undefined} {...handlers}>
        <span className="enc-name">{bb?.label ?? "—"}</span>
        <span className="enc-value tabular">{bb?.text ?? ""}</span>
        {mod && target && <ModDepth channel={target.channel} mod={mod} />}
      </div>
    </ParamMenuZone>
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

/** Tempo de la barre d'état : glisser vers le haut ou le bas pour le régler, toucher pour taper
    la valeur au clavier (v0.46 fix23). */
function TempoReadout({ bpm }: { bpm: number }) {
  return (
    <TempoValue className="screen-bpm screen-bpm--drag tabular">
      {bpm.toFixed(1)} <small>BPM</small>
    </TempoValue>
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

/** Attente d'une affectation (bouton assignable à toucher, commande MIDI à toucher) : voyant clignotant.
    v0.46 fix30f : aussi dans la vue live (elle couvre l'écran : le voyant n'y était pas visible). */
/** GOLEM 1.0 : le voyant d'apprentissage MIDI (et d'affectation des boutons) aussi par-dessus la façade
    d'un synthé et les pédales d'effets, qui couvrent l'écran (on ne voyait pas que GOLEM attendait). */
export function LearnFloat({ force = false }: { force?: boolean } = {}) {
  const learn = useStore((s) => s.tick.learn ?? -1);
  const knobWait = useStore((s) => s.ui.knobAssign);
  const covered = useStore((s) => force || s.ui.face || s.ui.pedals);
  if (!covered || (learn === -1 && knobWait === null)) return null;
  return <div className="learn-float" role="status"><LearnChips /></div>;
}

/** 1.0 (6 nov.) : le message court (« verrous copiés », « collés », « retirés »…) aussi par-dessus la façade
    d'un synthé, les pédales et le mode Grand du FM Player : il s'affichait derrière, on ne le voyait pas. */
export function MessageFloat({ force = false }: { force?: boolean } = {}) {
  const message = useStore((s) => s.ui.message);
  const covered = useStore((s) => force || s.ui.face || s.ui.pedals);
  // 1.0 (7 nov.) : pendant l'apprentissage MIDI ou « Assigner à… », le bandeau bleu suffit (pas deux
  // messages qui disent la même chose)
  const waiting = useStore((s) => (s.tick.learn ?? -1) !== -1 || s.ui.knobAssign !== null);
  if (!covered || !message || waiting) return null;
  return <div className="message-float" role="status">{message}</div>;
}

export function LearnChips() {
  const learn = useStore((s) => s.tick.learn ?? -1);
  const knobWait = useStore((s) => s.ui.knobAssign);
  return (
    <>
      {knobWait !== null && <button type="button" className="chip chip--edit chip--A chip--learn chip--knob" onClick={() => setUi({ knobAssign: null })}
        title="Annuler (Échap)">Bouton {knobWait + 1} : touchez la valeur à piloter… (annuler)</button>}
      {learn !== -1 && <button type="button" className="chip chip--edit chip--A chip--learn" onClick={() => void actions.midiLearn(-1)} title="Annuler l'apprentissage">MIDI : touchez une commande…</button>}
    </>
  );
}

/** v0.46 fix30f : message court (ex. « touchez un bouton du contrôleur ») affiché dans la vue live. */
export function LiveToast() {
  const message = useStore((s) => s.ui.message);
  return message ? <div className="screen-toast vecfull-toast" role="status">{message}</div> : null;
}

function HeaderExtra() {
  const extra = useContext(HeaderExtraContext);
  return extra ? <span className="screen-extra">{extra}</span> : null;
}

export function ScreenFrame({ title, status, tabs, tab, onTab, encoders, children }: ScreenFrameProps) {
  const t = useStore((s) => s.tick.t);
  const message = useStore((s) => s.ui.message);
  const slots = Array.from({ length: 8 }, (_, i) => encoders[i] ?? null);

  return (
    <div className="screen-frame">
      <header className={`screen-status${tabs ? " screen-status--tabs" : ""}`} data-screen={title}>
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
        <LearnChips />
        <HeaderExtra />
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

      <footer className="screen-encoders">
        {slots.map((b, i) => <EncoderLabel key={i} b={b} screen={title} />)}
      </footer>
    </div>
  );
}
