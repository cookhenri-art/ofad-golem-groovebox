// Groovebox — OFAD — AGPL-3.0-or-later
// Encodeur physique sans butée, avec couronne de LED qui montre la valeur.
// Son rôle change selon l'écran affiché (libellé et valeur affichés juste au-dessus, sur l'écran).
// Glisser verticalement (Maj = fin), molette, flèches du clavier, double-clic = valeur d'origine.
// Clic droit (ou appui long au doigt) : menu — valeur exacte ou liste des choix, modulation,
// macro, MIDI. En mode « affectation » (onglet Modulation), toucher l'encodeur le module.

import { useContext, useEffect, useLayoutEffect, useRef, useState } from "react";
import { MatrixMenuRow, SectionMenuContext, SectionMenuRow } from "./sectionMenu";
import { actions, getState, globalOf, showMessage, useStore } from "../engine/store";
import { MOD_SOURCE_COLORS, modSourceName } from "../engine/params";
import { useValueDrag } from "./useValueDrag";
import { MOD_SCOPE_SOURCES, modScopeOf } from "../engine/modScopes";

/** Saisie exacte d'un réglage du moteur (menu de l'encodeur). */
export interface ValueEntry {
  plain: number; min: number; max: number; unit: string; integer: boolean; choices?: string[];
  set: (plain: number) => void;
  pick: (index: number) => void;
}

export type EncoderBinding =
  | {
      kind: "continuous";
      label: string;
      text: string;
      normalised: number;
      bipolar?: boolean;
      locked?: "A" | "B" | null;     // le réglage écrit dans une scène ou un pas
      hint?: string;                 // explication (réglage remplacé par une scène…)
      mod?: { channel: number; local: number }; // réglage modulable : clic droit pour lui assigner une modulation
      onChange: (normalised: number) => void;
      reset?: () => void;
      sensitivity?: number;          // vitesse du glissé (1 = toute la plage en 200 pixels)
      entry?: ValueEntry;
    }
  | {
      kind: "stepped";
      label: string;
      text: string;
      normalised: number;
      locked?: "A" | "B" | null;
      hint?: string;
      mod?: { channel: number; local: number }; // réglage à crans modulable (octave, note, forme…) : valeurs arrondies au cran
      onStep: (direction: 1 | -1) => void;
      reset?: () => void;
      entry?: ValueEntry;
    };

const LEDS = 15;
const START = -135;
const SWEEP = 270;

function polar(r: number, deg: number) {
  const a = (deg * Math.PI) / 180;
  return { x: 40 + r * Math.sin(a), y: 40 - r * Math.cos(a) };
}
function arc(r: number, from: number, to: number) {
  const [a, b] = from <= to ? [from, to] : [to, from];
  const p = polar(r, a), q = polar(r, b);
  return `M ${p.x} ${p.y} A ${r} ${r} 0 ${b - a > 180 ? 1 : 0} 1 ${q.x} ${q.y}`;
}

/** Modulation d'un réglage : arc de couleur autour de l'encodeur, de la valeur vers + ou −
    selon la profondeur, et petite molette de profondeur. */
function useModulation(target: { channel: number; local: number } | undefined) {
  const mod = useStore((s) => (target ? s.channels[target.channel]?.mods.find((m) => m.target === target.local) : undefined));
  const assign = (source: number | null) => {
    if (!target) return;
    if (source === null) void actions.removeMod(target.channel, target.local);
    else void actions.addMod(target.channel, source, target.local, mod?.amount ?? 0.3);
  };
  const setAmount = (amount: number) => {
    if (!target || !mod) return;
    const a = Math.max(-1, Math.min(1, amount));
    actions.setParam(globalOf(target.channel, mod.amountSlot), (a + 1) / 2);
  };
  return { mod, assign, setAmount };
}


/** Nom d'une source : son type (LFO, fonction…) d'après ses réglages. */
export function sourceLabel(channel: number, i: number) {
  if (i >= 6) return "Vélocité";
  const c = getState().channels[channel];
  const typeNorm = c?.mod[i]?.[0] ?? 0;
  const types = ["LFO", "Fonction", "Suiveur", "Séquenceur"];
  return `${types[Math.min(3, Math.floor(typeNorm * 4))]} ${i + 1}`;
}

/** variant « knob » : bouton de façade de synthé (libellé au-dessus, valeur dessous, arc continu),
    même comportement que l'encodeur (glisser, molette, menu, modulation, affectation). */
export function Encoder({ binding, variant = "hw" }: { binding: EncoderBinding | null; variant?: "hw" | "knob" }) {
  const isKnob = variant === "knob";
  const { handlers, nudge } = useValueDrag(binding);
  const depthDrag = useRef<{ y: number; value: number } | null>(null);
  const modTarget = binding?.mod;
  const { mod, assign, setAmount } = useModulation(modTarget);
  const [menu, setMenu] = useState(false);
  const root = useRef<HTMLDivElement>(null);
  const menuRef = useRef<HTMLDivElement>(null);
  const press = useRef<{ x: number; y: number; timer: number } | null>(null);
  const entry = binding?.entry;
  const section = useContext(SectionMenuContext);
  const hasMenu = !!(modTarget || entry || section);

  // Affectation de modulation : la source choisie dans l'onglet Modulation module le réglage touché
  const assignMode = useStore((s) => s.ui.modAssign);
  const macros = useStore((s) => s.macros);
  // Affectation de macro (onglet Macros) : toucher un encodeur l'ajoute à la macro (ou l'en retire)
  const macroAssign = useStore((s) => s.ui.macroAssign);
  const macroAssignable = macroAssign !== null && !!modTarget;
  const macroHas = macroAssignable && !!macros[macroAssign!]?.links.some((l) => l[0] === globalOf(modTarget!.channel, modTarget!.local));
  const assignMacro = () => {
    if (macroAssign === null || !modTarget) return;
    const k = macroAssign;
    void actions.toggleMacro(k, globalOf(modTarget.channel, modTarget.local)).then((now) =>
      showMessage(now ? `Macro ${k + 1} → ${binding?.label ?? ""}` : `« ${binding?.label ?? ""} » ${macroHas ? "retiré de" : "non ajouté à (8 réglages au plus)"} la macro ${k + 1}`));
  };
  // une source du son ne module que l'instrument ; une source des effets, les inserts et le mixage
  const scope = modTarget ? modScopeOf(modTarget.local) : null;
  const assignable = !!(assignMode && modTarget && scope && modTarget.channel === assignMode.channel && MOD_SCOPE_SOURCES[scope].includes(assignMode.source));
  const assignTo = () => {
    if (!assignMode || !modTarget) return;
    const name = sourceLabel(assignMode.channel, assignMode.source);
    if (mod?.source === assignMode.source) { assign(null); showMessage(`${name} ne module plus « ${binding?.label ?? ""} »`); }
    else { assign(assignMode.source); showMessage(`${name} → ${binding?.label ?? ""} · profondeur : petite molette colorée`); }
  };

  // Appui long au doigt : ouvre le menu (le clic droit n'existe pas sur tablette)
  const cancelPress = () => { if (press.current) { window.clearTimeout(press.current.timer); press.current = null; } };
  const capHandlers = {
    ...handlers,
    onPointerDown: (e: React.PointerEvent<HTMLElement>) => {
      handlers.onPointerDown(e);
      if (e.pointerType === "touch" && hasMenu)
        press.current = { x: e.clientX, y: e.clientY, timer: window.setTimeout(() => { press.current = null; handlers.onPointerCancel(); setMenu(true); }, 550) };
    },
    onPointerMove: (e: React.PointerEvent<HTMLElement>) => {
      if (press.current && Math.hypot(e.clientX - press.current.x, e.clientY - press.current.y) > 6) cancelPress();
      handlers.onPointerMove(e);
    },
    onPointerUp: (e: React.PointerEvent<HTMLElement>) => { cancelPress(); handlers.onPointerUp(e); },
    onPointerCancel: () => { cancelPress(); handlers.onPointerCancel(); },
  };

  useMenuPlacement(menu, root, menuRef, variant === "knob");
  useMenuDismiss(menu, root, () => setMenu(false));


  const n = binding?.normalised ?? 0;
  const lit = binding ? Math.round(n * (LEDS - 1)) : -1;
  const centre = (LEDS - 1) / 2;

  const valueAngle = START + n * SWEEP;
  const modEnd = mod ? START + Math.max(0, Math.min(1, n + mod.amount)) * SWEEP : valueAngle;
  const modColor = mod ? MOD_SOURCE_COLORS[mod.source] : undefined;

  return (
    <div ref={root} className={`encoder${isKnob ? " encoder--knob" : ""}${binding ? "" : " encoder--idle"}${mod ? " encoder--mod" : ""}${assignable ? " encoder--assign" : assignMode ? " encoder--noassign" : ""}${macroAssignable ? ` encoder--assign${macroHas ? " encoder--macro-on" : ""}` : macroAssign !== null ? " encoder--noassign" : ""}`}
      style={assignable ? { "--assign": MOD_SOURCE_COLORS[assignMode!.source] } as React.CSSProperties : macroAssignable ? { "--assign": "oklch(0.82 0.15 85)" } as React.CSSProperties : undefined}
      title={binding?.hint}>
      {isKnob && <FitText className="knob-label" text={binding?.label ?? ""} />}
      {isKnob && binding?.locked && <span className={`knob-lock knob-lock--${binding.locked}`} aria-hidden />}
      <svg viewBox="0 0 80 80" className="encoder-ring" aria-hidden>
        {mod && Math.abs(modEnd - valueAngle) > 0.5 && (
          <path d={arc(39, valueAngle, modEnd)} className="mod-arc" style={{ stroke: modColor }} />
        )}
        {isKnob && <path d={arc(35, START, START + SWEEP)} className="knob-track" />}
        {isKnob && binding && (binding.kind === "continuous" && binding.bipolar
          ? Math.abs(valueAngle) > 0.5 && <path d={arc(35, 0, valueAngle)} className="knob-arc" />
          : n > 0.002 && <path d={arc(35, START, valueAngle)} className="knob-arc" />)}
        {!isKnob && Array.from({ length: LEDS }, (_, i) => {
          const a = ((START + (i / (LEDS - 1)) * SWEEP) * Math.PI) / 180;
          const on = binding
            ? binding.kind === "continuous" && binding.bipolar
              ? (i >= Math.min(centre, lit) && i <= Math.max(centre, lit))
              : i <= lit
            : false;
          return <circle key={i} cx={40 + 35 * Math.sin(a)} cy={40 - 35 * Math.cos(a)} r={2.4} className={on ? "led led--on" : "led"} />;
        })}
      </svg>
      <button
        type="button"
        className="encoder-cap"
        disabled={!binding}
        role="slider"
        aria-label={binding?.label ?? "Encodeur libre"}
        aria-valuemin={0}
        aria-valuemax={1}
        aria-valuenow={Number(n.toFixed(3))}
        aria-valuetext={binding?.text}
        style={{ transform: `rotate(${START + n * SWEEP}deg)`, "--rot": `${START + n * SWEEP}deg` } as React.CSSProperties}
        title={assignable ? `Moduler « ${binding?.label ?? ""} » avec ${sourceLabel(assignMode!.channel, assignMode!.source)}` : undefined}
        {...(assignable ? { onClick: assignTo } : macroAssignable ? { onClick: assignMacro } : capHandlers)}
        onContextMenu={(e) => { if (hasMenu) { e.preventDefault(); cancelPress(); setMenu(true); } }}
        onKeyDown={(e) => {
          if (e.key === "ArrowUp" || e.key === "ArrowRight") { nudge(1, e.shiftKey); e.preventDefault(); }
          if (e.key === "ArrowDown" || e.key === "ArrowLeft") { nudge(-1, e.shiftKey); e.preventDefault(); }
        }}
      >
        <span className="encoder-mark" />
      </button>
      {isKnob && <FitText className="knob-value tabular" text={binding?.text ?? ""} />}

      {mod && (
        <button
          type="button"
          className="mod-depth"
          style={{ "--mod": modColor } as React.CSSProperties}
          role="slider"
          aria-label={`Profondeur de modulation ${modSourceName(mod.source)}`}
          aria-valuemin={-100}
          aria-valuemax={100}
          aria-valuenow={Math.round(mod.amount * 100)}
          title="Profondeur de modulation : glisser haut / bas (double-clic : zéro)"
          onPointerDown={(e) => { e.stopPropagation(); e.currentTarget.setPointerCapture(e.pointerId); depthDrag.current = { y: e.clientY, value: mod.amount }; }}
          onPointerMove={(e) => { if (depthDrag.current) setAmount(depthDrag.current.value + (depthDrag.current.y - e.clientY) * 0.008); }}
          onPointerUp={() => { depthDrag.current = null; }}
          onWheel={(e) => setAmount(mod.amount + (e.deltaY < 0 ? 0.02 : -0.02))}
          onDoubleClick={() => setAmount(0)}
          onContextMenu={(e) => { e.preventDefault(); setMenu(true); }}
        >
          <span style={{ transform: `rotate(${mod.amount * 135}deg)` }} />
        </button>
      )}

      {menu && hasMenu && (
        <div ref={menuRef} className="mod-menu" role="menu" aria-label={`Réglage ${binding?.label ?? ""}`}>
          <ParamMenuBody binding={binding} onDone={() => setMenu(false)} />
        </div>
      )}
    </div>
  );
}

/** Valeur exacte : liste des choix, ou champ numérique dans les unités du réglage. */
function ValueEntryRow({ entry, label, onDone }: { entry: ValueEntry; label: string; onDone: () => void }) {
  const decimals = entry.integer ? 0 : Math.abs(entry.max - entry.min) <= 2 ? 3 : Math.abs(entry.max - entry.min) <= 50 ? 2 : 1;
  const [text, setText] = useState(() => String(Number(entry.plain.toFixed(decimals))));
  const input = useRef<HTMLInputElement>(null);
  useEffect(() => { if (!entry.choices) input.current?.select(); }, [entry.choices]);
  if (entry.choices) {
    const cur = Math.round(entry.plain);
    return (
      <>
        <span className="mod-menu-title">{label}</span>
        <div className="entry-choices">
          {entry.choices.map((c, i) => (
            <button key={c + i} type="button" role="menuitemradio" aria-checked={i === cur} className={i === cur ? "on" : ""}
              onClick={() => { entry.pick(i); onDone(); }}>{c}</button>
          ))}
        </div>
      </>
    );
  }
  const commit = () => {
    // « 1,2k » = 1200 ; le signe moins typographique (−) compte comme « - »
    const t = text.trim().replace(/\u2212/g, "-").replace(",", ".");
    const kilo = /k\s*[a-z]*$/i.test(t) && !/^[-+]?[0-9.]+\s*(ms|dB|%)?$/i.test(t);
    const clean = t.replace(/[^0-9.eE+\-]/g, "");
    const v = clean ? Number(clean) * (kilo ? 1000 : 1) : NaN;
    if (Number.isFinite(v)) { entry.set(v); onDone(); }
    else showMessage("Valeur non reconnue");
  };
  return (
    <>
      <span className="mod-menu-title">Valeur exacte · {label}</span>
      <form className="entry-row" onSubmit={(e) => { e.preventDefault(); commit(); }}>
        <input ref={input} type="text" inputMode="decimal" value={text} maxLength={16} aria-label={`Valeur de ${label}`}
          onChange={(e) => setText(e.target.value)} onKeyDown={(e) => { e.stopPropagation(); if (e.key === "Escape") onDone(); }} />
        <span className="entry-unit">{entry.unit}</span>
        <button type="submit">OK</button>
      </form>
      <span className="entry-range">de {entry.min} à {entry.max} {entry.unit}</span>
    </>
  );
}

/** Apprentissage MIDI d'un réglage : le prochain CC reçu pilote ce réglage. */
function MidiLearnRow({ global, label, onDone }: { global: number; label: string; onDone: () => void }) {
  const maps = useStore((s) => s.midi.maps);
  const mine = maps.find((m) => m[3] === global);
  return (
    <>
      <span className="mod-menu-title mod-menu-title--macro">Contrôleur MIDI{mine ? ` · ${mine[0] === 0 ? "CC" : "note"} ${mine[2]}, canal ${mine[1] + 1}` : ""}</span>
      <button type="button" onClick={() => { void actions.midiLearn(global); showMessage(`Touchez une commande du contrôleur pour « ${label} »`); onDone(); }}>Apprendre MIDI</button>
      {mine && <button type="button" className="mod-remove" onClick={() => { void actions.midiForget(global); onDone(); }}>Oublier la commande MIDI</button>}
    </>
  );
}

/** Texte d'un bouton rotatif qui s'adapte à la place disponible : taille réduite d'abord, puis,
    pour un nom composé « Section · Paramètre », le paramètre seul (la section reste dans la bulle). */
function FitText({ className, text }: { className: string; text: string }) {
  const ref = useRef<HTMLSpanElement>(null);
  const [level, setLevel] = useState(0);
  useLayoutEffect(() => { setLevel(0); }, [text]);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el || level >= 2) return;
    if (el.scrollWidth > el.clientWidth + 1) setLevel(level + 1);
  }, [level, text]);
  const cut = text.lastIndexOf(" \u00b7 ");
  const shown = level >= 2 && cut > 0 ? text.slice(cut + 3) : text;
  return <span ref={ref} className={`${className}${level >= 1 ? " fit-small" : ""}`} title={text}>{shown}</span>;
}

// ---------------------------------------------------------------------------
// v0.32.1 : menu d'un réglage partagé par tous les contrôles (bouton, curseur, interrupteur,
// sélecteur, afficheur) : valeur exacte, modulation, macros, MIDI, et la section (copier,
// coller, remettre à zéro) quand le contrôle est dans une section qui le permet.

/** Place le menu pour qu'il reste visible (fenêtre et cadres qui coupent ce qui dépasse). */
export function useMenuPlacement(open: boolean, root: React.RefObject<HTMLElement | null>, menuRef: React.RefObject<HTMLDivElement | null>, preferBelow: boolean) {
  // Le menu reste entièrement visible : dans la fenêtre et dans les cadres qui coupent ce qui
  // dépasse (façade d'un synthé, écran). Il s'ouvre du côté où il tient (au-dessus ou en dessous
  // du bouton) ; s'il ne tient d'aucun côté, il prend le plus grand et sa liste défile.
  // Positions en pixels de mise en page (offsetWidth), indépendantes des mises à l'échelle.
  useLayoutEffect(() => {
    const el = menuRef.current, host = root.current;
    if (!open || !el || !host) return;
    const hostRect = host.getBoundingClientRect();
    const scale = host.offsetWidth > 0 ? hostRect.width / host.offsetWidth : 1;
    // zone visible : la fenêtre, réduite par chaque ancêtre qui coupe son contenu
    let top = 0, bottom = window.innerHeight, leftB = 0, rightB = window.innerWidth;
    for (let a = host.parentElement; a && a !== document.body; a = a.parentElement) {
      const cs = getComputedStyle(a);
      const clipY = cs.overflowY !== "visible", clipX = cs.overflowX !== "visible";
      if (!clipX && !clipY) continue;
      const r = a.getBoundingClientRect();
      if (clipY) { top = Math.max(top, r.top); bottom = Math.min(bottom, r.bottom); }
      if (clipX) { leftB = Math.max(leftB, r.left); rightB = Math.min(rightB, r.right); }
    }
    const gap = 6, edge = 8;
    // horizontal : centré sur le bouton, ramené dans la zone visible
    const w = el.offsetWidth;
    let left = (host.offsetWidth - w) / 2;
    left = Math.min(Math.max(left, (leftB - hostRect.left + edge) / scale), (rightB - hostRect.left - edge) / scale - w);
    el.style.left = `${left}px`;
    el.style.transform = "none";
    // vertical : côté préféré (en dessous pour les boutons de façade, au-dessus pour les encodeurs)
    el.style.maxHeight = "";
    const h = el.scrollHeight * scale;
    const below = bottom - hostRect.bottom - gap - edge, above = hostRect.top - top - gap - edge;
    const prefersBelow = preferBelow;
    const first = prefersBelow ? below : above, second = prefersBelow ? above : below;
    const useBelow = h <= first ? prefersBelow : h <= second ? !prefersBelow : (below >= above);
    const room = Math.max(80, useBelow ? below : above);
    if (useBelow) { el.style.top = `calc(100% + ${gap}px)`; el.style.bottom = "auto"; el.style.transformOrigin = "50% 0"; }
    else { el.style.bottom = `calc(100% + ${gap}px)`; el.style.top = "auto"; el.style.transformOrigin = "50% 100%"; }
    if (h > room) { el.style.maxHeight = `${room / scale}px`; el.style.overflowY = "auto"; }
    else el.style.overflowY = "";
  }, [open, preferBelow, root, menuRef]);
}

/** Ferme le menu : appui ailleurs, ou Échap (en capture : avant le reste de l'écran). */
export function useMenuDismiss(open: boolean, root: React.RefObject<HTMLElement | null>, close: () => void) {
  const closeRef = useRef(close);
  closeRef.current = close;
  useEffect(() => {
    if (!open) return;
    const onEvent = (e: PointerEvent | KeyboardEvent) => {
      if (e instanceof KeyboardEvent) { if (e.key === "Escape") { e.preventDefault(); closeRef.current(); } }
      else if (!root.current?.contains(e.target as Node)) closeRef.current();
    };
    window.addEventListener("pointerdown", onEvent, true);
    window.addEventListener("keydown", onEvent, true);
    return () => { window.removeEventListener("pointerdown", onEvent, true); window.removeEventListener("keydown", onEvent, true); };
  }, [open, root]);
}

/** Contenu du menu d'un réglage. */
export function ParamMenuBody({ binding, onDone }: { binding: EncoderBinding | null; onDone: () => void }) {
  const entry = binding?.entry;
  const modTarget = binding?.mod;
  const { mod, assign } = useModulation(modTarget);
  const macros = useStore((s) => s.macros);
  const scope = modTarget ? modScopeOf(modTarget.local) : null;
  const menuSources = scope ? [...MOD_SCOPE_SOURCES[scope], 6] : [];
  return (
    <>
      <SectionMenuRow onDone={onDone} />
          {entry && <ValueEntryRow entry={entry} label={binding?.label ?? ""} onDone={() => onDone()} />}
          {modTarget && <>
          <span className="mod-menu-title">Moduler « {binding?.label} » avec</span>
          {menuSources.map((src) => (
            <button key={src} type="button" role="menuitemradio" aria-checked={mod?.source === src}
              className={mod?.source === src ? "on" : ""} onClick={() => { assign(src); onDone(); }}>
              <span className="mod-dot" style={{ background: MOD_SOURCE_COLORS[src] }} />{sourceLabel(modTarget.channel, src)}
            </button>
          ))}
          {mod && <button type="button" className="mod-remove" onClick={() => { assign(null); onDone(); }}>Retirer la modulation</button>}
          <MatrixMenuRow target={modTarget} label={binding?.label ?? ""} onDone={onDone} />
          <span className="mod-menu-title mod-menu-title--macro">Macros (logiciel hôte, contrôleur) : ajouter ou retirer</span>
          <div className="macro-row">
            {[0, 1, 2, 3, 4, 5, 6, 7].map((k) => {
              const g = globalOf(modTarget.channel, modTarget.local);
              const on = !!macros[k]?.links.some((l) => l[0] === g);
              return (
                <button key={k} type="button" className={on ? "on" : ""} aria-pressed={on}
                  title={on ? `Retirer ce réglage de la macro ${k + 1}` : `La macro ${k + 1} pilote aussi ce réglage (jusqu'à 8 réglages par macro)`}
                  onClick={() => {
                    void actions.toggleMacro(k, g).then((now) => showMessage(now
                      ? `Macro ${k + 1} → ${binding?.label ?? ""} (${getState().macros[k]?.links.length ?? 1} réglage(s) ; plages : onglet Macros)`
                      : (getState().macros[k]?.links.length ?? 0) >= 8 && !getState().macros[k]?.links.some((l) => l[0] === g)
                        ? `La macro ${k + 1} pilote déjà 8 réglages` : `« ${binding?.label ?? ""} » retiré de la macro ${k + 1}`));
                    onDone();
                  }}>{k + 1}</button>
              );
            })}
          </div>
          <MidiLearnRow global={globalOf(modTarget.channel, modTarget.local)} label={binding?.label ?? ""} onDone={() => onDone()} />
          </>}
    </>
  );
}

/** Zone qui ouvre le menu d'un réglage au clic droit (ou appui long au doigt) : pour les curseurs,
    interrupteurs, sélecteurs et afficheurs des façades. Ne gêne pas le geste du contrôle. */
export function ParamMenuZone({ binding, className, children, preferBelow = true }: {
  binding: EncoderBinding | null; className?: string; children: React.ReactNode; preferBelow?: boolean;
}) {
  const [open, setOpen] = useState(false);
  const root = useRef<HTMLDivElement>(null);
  const menuRef = useRef<HTMLDivElement>(null);
  const press = useRef<{ x: number; y: number; timer: number } | null>(null);
  const section = useContext(SectionMenuContext);
  const hasMenu = !!(binding?.mod || binding?.entry || section);
  useMenuPlacement(open, root, menuRef, preferBelow);
  useMenuDismiss(open, root, () => setOpen(false));
  const cancel = () => { if (press.current) { window.clearTimeout(press.current.timer); press.current = null; } };
  return (
    <div ref={root} className={`pm-zone${className ? ` ${className}` : ""}`}
      onContextMenu={(e) => { if (!hasMenu) return; e.preventDefault(); e.stopPropagation(); cancel(); setOpen(true); }}
      onPointerDownCapture={(e) => { if (e.pointerType === "touch" && hasMenu) press.current = { x: e.clientX, y: e.clientY, timer: window.setTimeout(() => { press.current = null; setOpen(true); }, 550) }; }}
      onPointerMoveCapture={(e) => { if (press.current && Math.hypot(e.clientX - press.current.x, e.clientY - press.current.y) > 6) cancel(); }}
      onPointerUpCapture={cancel} onPointerCancelCapture={cancel}>
      {children}
      {open && hasMenu && (
        <div ref={menuRef} className="mod-menu" role="menu" aria-label={`Réglage ${binding?.label ?? ""}`}>
          <ParamMenuBody binding={binding} onDone={() => setOpen(false)} />
        </div>
      )}
    </div>
  );
}

