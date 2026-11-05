// Groovebox — OFAD — AGPL-3.0-or-later
// Encodeur physique sans butée, avec couronne de LED qui montre la valeur.
// Son rôle change selon l'écran affiché (libellé et valeur affichés juste au-dessus, sur l'écran).
// Glisser verticalement (Maj = fin), molette, flèches du clavier, double-clic = valeur d'origine.
// Clic droit (ou appui long au doigt) : menu — valeur exacte ou liste des choix, modulation,
// macro, MIDI. En mode « affectation » (onglet Modulation), toucher l'encodeur le module.

import { createContext, useContext, useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { MatrixMenuRow, SectionMenuContext, SectionMenuRow } from "./sectionMenu";
import { actions, getState, globalOf, setUi, showMessage, useStore } from "../engine/store";
import { MOD_SOURCE_COLORS, modSourceName } from "../engine/params";
import { markLongPress, useValueDrag } from "./useValueDrag";
import { MOD_SCOPE_SOURCES, modScopeOf } from "../engine/modScopes";
import { bindingCode, peekUiCode, resolveCode } from "../engine/ctl";
import { NUM_KNOBS } from "../engine/protocol";

/** v0.46 fix25 : l'ajout aux macros du synthé ne se propose que dans la façade d'un synthé (ailleurs, ce
    sont les 16 boutons assignables : pas de confusion entre les deux). */
export const MacroMenuContext = createContext(false);

/** v0.46 fix24 : la liaison a-t-elle un menu (valeur, modulation, MIDI, bouton assignable) ? */
export const bindingHasMenu = (b: EncoderBinding | null | undefined) => !!(b && (b.mod || b.entry || b.menu || b.ctl !== undefined || b.ctlId));

/** v0.46 fix24 : attributs qui rendent un contrôle « assignable » (toucher en attente d'un bouton). */
export function ctlAttrs(b: EncoderBinding | null | undefined): Record<string, string> {
  if (!b || b.menu) return {};
  const code = bindingCode(b);
  if (code === null && !b.ctlId) return {};
  const a: Record<string, string> = { "data-ctl-label": b.label, "data-ctl-norm": String(b.normalised) };
  if (code !== null) a["data-ctl"] = String(code);
  else a["data-ctl-ui"] = b.ctlId as string;
  return a;
}

/** Saisie exacte d'un réglage du moteur (menu de l'encodeur). */
export interface ValueEntry {
  plain: number; min: number; max: number; unit: string; integer: boolean; choices?: string[];
  set: (plain: number) => void;
  pick: (index: number) => void;
}

/** v0.46 fix24 : champs communs : cible pilotable (MIDI, bouton assignable) et menu propre. */
interface BindingCtl {
  ctl?: number;                    // cible : réglage du moteur, commande spéciale, réglage global (voir engine/ctl.ts)
  ctlId?: string;                  // valeur d'écran pilotée par l'interface (« Séquence.Longueur »)
  setNorm?: (normalised: number) => void; // réglage à crans : choix direct d'après une position 0..1
  menu?: (onDone: () => void) => React.ReactNode; // menu propre (boutons assignables)
}

export type EncoderBinding = BindingCtl & (
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
    });

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
  const section = useContext(SectionMenuContext);
  const hasMenu = !!(bindingHasMenu(binding) || section);

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
        press.current = { x: e.clientX, y: e.clientY, timer: window.setTimeout(() => { press.current = null; handlers.onPointerCancel(); markLongPress(); setMenu(true); }, 550) };
    },
    onPointerMove: (e: React.PointerEvent<HTMLElement>) => {
      if (press.current && Math.hypot(e.clientX - press.current.x, e.clientY - press.current.y) > 6) cancelPress();
      handlers.onPointerMove(e);
    },
    onPointerUp: (e: React.PointerEvent<HTMLElement>) => { cancelPress(); handlers.onPointerUp(e); },
    onPointerCancel: () => { cancelPress(); handlers.onPointerCancel(); },
  };

  useMenuPlacement(menu, root, menuRef, variant === "knob");
  useMenuDismiss(menu, root, () => setMenu(false), menuRef);


  const n = binding?.normalised ?? 0;
  const lit = binding ? Math.round(n * (LEDS - 1)) : -1;
  const centre = (LEDS - 1) / 2;

  const valueAngle = START + n * SWEEP;
  const modEnd = mod ? START + Math.max(0, Math.min(1, n + mod.amount)) * SWEEP : valueAngle;
  const modColor = mod ? MOD_SOURCE_COLORS[mod.source] : undefined;

  return (
    <div ref={root} className={`encoder${isKnob ? " encoder--knob" : ""}${binding ? "" : " encoder--idle"}${mod ? " encoder--mod" : ""}${assignable ? " encoder--assign" : assignMode ? " encoder--noassign" : ""}${macroAssignable ? ` encoder--assign${macroHas ? " encoder--macro-on" : ""}` : macroAssign !== null ? " encoder--noassign" : ""}`}
      style={assignable ? { "--assign": MOD_SOURCE_COLORS[assignMode!.source] } as React.CSSProperties : macroAssignable ? { "--assign": "oklch(0.82 0.15 85)" } as React.CSSProperties : undefined}
      title={binding?.hint} {...ctlAttrs(binding)}>
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
        <MenuShell menuRef={menuRef} label={binding?.label ?? ""}>
          <ParamMenuBody binding={binding} onDone={() => setMenu(false)} />
        </MenuShell>
      )}
    </div>
  );
}

/** Valeur exacte : liste des choix, ou champ numérique dans les unités du réglage. */
function ValueEntryRow({ entry, label, onDone }: { entry: ValueEntry; label: string; onDone: () => void }) {
  const decimals = entry.integer ? 0 : Math.abs(entry.max - entry.min) <= 2 ? 3 : Math.abs(entry.max - entry.min) <= 50 ? 2 : 1;
  const [text, setText] = useState(() => String(Number(entry.plain.toFixed(decimals))));
  const input = useRef<HTMLInputElement>(null);
  // v0.46 fix20 : au doigt (tablette, téléphone), le champ n'est pas pris d'office : le menu s'ouvre,
  // et le clavier ne vient que si l'on touche le champ. À la souris, la valeur reste présélectionnée.
  useEffect(() => {
    if (entry.choices) return;
    const finePointer = typeof window.matchMedia === "function" && window.matchMedia("(pointer: fine)").matches;
    if (finePointer && !getState().ui.phone) input.current?.select();
  }, [entry.choices]);
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
export function MidiLearnRow({ global, label, onDone, resolve }: { global: number | null; label: string; onDone: () => void; resolve?: () => Promise<number | null> }) {
  const maps = useStore((s) => s.midi.maps);
  const mine = global === null ? undefined : maps.find((m) => m[3] === global);
  return (
    <>
      <span className="mod-menu-title mod-menu-title--macro">Contrôleur MIDI{mine ? ` · ${mine[0] === 0 ? "CC" : "note"} ${mine[2]}, canal ${mine[1] + 1}` : ""}</span>
      <button type="button" onClick={() => {
        onDone();
        void (async () => {
          const code = global ?? (await resolve?.()) ?? null;
          if (code === null) return;
          void actions.midiLearn(code); showMessage(`Touchez une commande du contrôleur pour « ${label} »`);
        })();
      }}>Apprendre MIDI</button>
      {mine && global !== null && <button type="button" className="mod-remove" onClick={() => { void actions.midiForget(global); onDone(); }}>Oublier la commande MIDI</button>}
    </>
  );
}

/** v0.46 fix24 : piloter ce réglage avec l'un des 16 boutons assignables. */
function KnobAssignRow({ binding, onDone }: { binding: EncoderBinding; onDone: () => void }) {
  const knobs = useStore((s) => s.midi.knobs);
  const code = bindingCode(binding) ?? (binding.ctlId ? peekUiCode(binding.ctlId) : null);
  return (
    <>
      <span className="mod-menu-title mod-menu-title--macro">Bouton assignable : piloter « {binding.label} » avec</span>
      <div className="macro-row macro-row--16">
        {Array.from({ length: NUM_KNOBS }, (_, k) => {
          const on = code !== null && knobs?.[k]?.[0] === code;
          const busy = !on && (knobs?.[k]?.[0] ?? -1) !== -1;
          return (
            <button key={k} type="button" className={`${on ? "on" : ""}${busy ? " busy" : ""}`} aria-pressed={on}
              title={on ? `Le bouton ${k + 1} pilote déjà ce réglage (toucher : le libérer)` : busy ? `Le bouton ${k + 1} pilote autre chose : il sera réaffecté` : `Le bouton ${k + 1} pilotera ce réglage`}
              onClick={() => {
                onDone();
                if (on) { void actions.setKnobTarget(k, -1); showMessage(`Bouton ${k + 1} libéré`); return; }
                void resolveCode(binding, binding.ctlId).then((c) => {
                  if (c === null) return;
                  void actions.setKnobTarget(k, c, binding.normalised);
                  showMessage(`Bouton ${k + 1} → ${binding.label}`);
                });
              }}>{k + 1}</button>
          );
        })}
      </div>
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
    if (el.classList.contains("mod-menu--phone")) return; // v0.46.2 : téléphone : menu hors de l'appareil, placé en CSS
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
    // v0.46 fix20 : sur téléphone, le menu est agrandi et posé au milieu de la partie visible
    // (comme une fenêtre), pour se lire et se toucher facilement ; sa liste défile s'il est haut.
    if (getState().ui.phone) {
      // v0.46 fix28 : la zone visible tient aussi compte de la fenêtre réellement affichée (visualViewport)
      const vv = window.visualViewport;
      if (vv) { top = Math.max(top, vv.offsetTop); bottom = Math.min(bottom, vv.offsetTop + vv.height); leftB = Math.max(leftB, vv.offsetLeft); rightB = Math.min(rightB, vv.offsetLeft + vv.width); }
      const availW = rightB - leftB - 2 * edge, availH = bottom - top - 2 * edge;
      // v0.46 fix28 : l'échelle n'est plus déduite du bouton mais MESURÉE sur le menu lui-même, posé à
      // l'origine sans agrandissement (certaines WebView Android l'affichaient ~1,2 fois plus grand que
      // prévu : le bas de la liste sortait de l'écran et ne se laissait jamais atteindre en défilant)
      el.style.zoom = "";
      el.style.transformOrigin = "0 0";
      el.style.transform = "none";
      el.style.maxHeight = "";
      el.style.overflowY = "";
      el.style.left = "0px";
      el.style.top = "0px";
      el.style.bottom = "auto";
      el.classList.add("mod-menu--sheet");
      const w0 = el.offsetWidth, h0 = el.scrollHeight;
      const r0 = el.getBoundingClientRect();
      const anc = w0 > 0 && r0.width > 0 ? r0.width / w0 : scale;
      // v0.46.2 : même taille partout (écran, façade, pédalier) : largeur visible fixe (au plus 9/10 de
      // l'écran), et la liste défile toujours si elle dépasse (avant : agrandi ×1 au moins, d'où des
      // menus tantôt énormes, tantôt petits, et parfois plus hauts que l'écran)
      const z = Math.min(300, availW * 0.9) / (w0 * anc);
      const k = anc * z;
      el.style.transform = `scale(${z})`;
      el.style.overflowY = "auto";
      if (h0 * k > availH * 0.94) el.style.maxHeight = `${(availH * 0.94) / k}px`;
      const visW = w0 * k, visH = Math.min(h0 * k, availH);
      const vx = leftB + edge + (availW - visW) / 2, vy = top + edge + (availH - visH) / 2;
      let left = (vx - r0.left) / anc, topPx = (vy - r0.top) / anc;
      el.style.left = `${left}px`;
      el.style.top = `${topPx}px`;
      // contrôle final sur ce qui est vraiment affiché : le menu entier dans la zone visible
      const r = el.getBoundingClientRect();
      if (r.height > availH + 1) {
        el.style.maxHeight = `${(el.clientHeight * availH) / r.height}px`;
        el.style.overflowY = "auto";
      }
      const r2 = el.getBoundingClientRect();
      const ratio = r2.width > 0 && el.offsetWidth > 0 ? r2.width / (el.offsetWidth * z) : anc;
      let dy = 0, dx = 0;
      if (r2.bottom > bottom - edge + 0.5) dy = (bottom - edge) - r2.bottom;
      if (r2.top + dy < top + edge - 0.5) dy = (top + edge) - r2.top;
      if (r2.right > rightB - edge + 0.5) dx = (rightB - edge) - r2.right;
      if (r2.left + dx < leftB + edge - 0.5) dx = (leftB + edge) - r2.left;
      topPx += dy / ratio;
      left += dx / ratio;
      el.style.left = `${left}px`;
      el.style.top = `${topPx}px`;
      return;
    }
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
export function useMenuDismiss(open: boolean, root: React.RefObject<HTMLElement | null>, close: () => void, menuRef?: React.RefObject<HTMLElement | null>) {
  const closeRef = useRef(close);
  closeRef.current = close;
  useEffect(() => {
    if (!open) return;
    const onEvent = (e: PointerEvent | KeyboardEvent) => {
      if (e instanceof KeyboardEvent) { if (e.key === "Escape") { e.preventDefault(); closeRef.current(); } }
      else if (!root.current?.contains(e.target as Node) && !menuRef?.current?.contains(e.target as Node)) closeRef.current();
    };
    window.addEventListener("pointerdown", onEvent, true);
    window.addEventListener("keydown", onEvent, true);
    return () => { window.removeEventListener("pointerdown", onEvent, true); window.removeEventListener("keydown", onEvent, true); };
  }, [open, root]);
}

/** v0.46.2 : coque des menus de réglage. Sur téléphone, le menu sort de l'appareil (mis à l'échelle) et
    se pose par-dessus tout, au milieu de l'écran, à sa taille normale (pixels de l'écran) : même taille
    partout, et sa liste défile au doigt. Un toucher à côté le referme. */
export function MenuShell({ menuRef, label, children }: { menuRef: React.RefObject<HTMLDivElement | null>; label: string; children: React.ReactNode }) {
  const phone = useStore((s) => !!s.ui.phone);
  const stop = (e: React.SyntheticEvent) => e.stopPropagation();
  const menu = (
    <div ref={menuRef} className={`mod-menu${phone ? " mod-menu--phone" : ""}`} role="menu" aria-label={`Réglage ${label}`}
      onPointerDown={stop} onDoubleClick={stop} onWheel={stop} onContextMenu={(e) => { e.preventDefault(); e.stopPropagation(); }}>
      {children}
    </div>
  );
  if (!phone) return menu;
  return createPortal(<div className="mod-menu-veil" onPointerDown={stop}>{menu}</div>, document.body);
}

/** Modulation d'un réglage du moteur : sources (LFO, fonctions…, vélocité), retrait, matrice. Partagé
    par le menu des réglages et celui des boutons assignables (v0.46 fix26 : leur cible se module). */
export function ModRows({ target, label, onDone }: { target: { channel: number; local: number }; label: string; onDone: () => void }) {
  const { mod, assign } = useModulation(target);
  const scope = modScopeOf(target.local);
  const menuSources = scope ? [...MOD_SCOPE_SOURCES[scope], 6] : [];
  return (
    <>
      <span className="mod-menu-title">Moduler « {label} » avec</span>
      {menuSources.map((src) => (
        <button key={src} type="button" role="menuitemradio" aria-checked={mod?.source === src}
          className={mod?.source === src ? "on" : ""} onClick={() => { assign(src); onDone(); }}>
          <span className="mod-dot" style={{ background: MOD_SOURCE_COLORS[src] }} />{sourceLabel(target.channel, src)}
        </button>
      ))}
      {mod && <button type="button" className="mod-remove" onClick={() => { assign(null); onDone(); }}>Retirer la modulation</button>}
      {/* v0.46 fix28 : régler les sources elles-mêmes (forme, vitesse, synchro…) sans quitter l'écran */}
      {scope && <button type="button" className="mod-edit" onClick={() => { setUi({ modPanel: { channel: target.channel, scope } }); onDone(); }}>⚙ Régler les sources (LFO, fonctions…)</button>}
      <MatrixMenuRow target={target} label={label} onDone={onDone} />
    </>
  );
}

/** Contenu du menu d'un réglage. */
export function ParamMenuBody({ binding, onDone }: { binding: EncoderBinding | null; onDone: () => void }) {
  if (binding?.menu) return <>{binding.menu(onDone)}</>;
  return <ParamMenuStandard binding={binding} onDone={onDone} />;
}

function ParamMenuStandard({ binding, onDone }: { binding: EncoderBinding | null; onDone: () => void }) {
  const macroMenu = useContext(MacroMenuContext);
  const entry = binding?.entry;
  const modTarget = binding?.mod;
  const macros = useStore((s) => s.macros);
  return (
    <>
      <SectionMenuRow onDone={onDone} />
          {entry && <ValueEntryRow entry={entry} label={binding?.label ?? ""} onDone={() => onDone()} />}
          {modTarget && <>
          <ModRows target={modTarget} label={binding?.label ?? ""} onDone={onDone} />
          {macroMenu && <>
          <span className="mod-menu-title mod-menu-title--macro">Macros du synthé : ajouter ou retirer</span>
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
          </>}
          <MidiLearnRow global={globalOf(modTarget.channel, modTarget.local)} label={binding?.label ?? ""} onDone={() => onDone()} />
          </>}
          {/* v0.46 fix24 : réglage sans modulation (commande, réglage global, valeur d'écran) : MIDI */}
          {!modTarget && binding && (binding.ctl !== undefined || binding.ctlId) && (
            <MidiLearnRow global={bindingCode(binding) ?? (binding.ctlId ? peekUiCode(binding.ctlId) : null)} label={binding.label} onDone={() => onDone()}
              resolve={() => resolveCode(binding, binding.ctlId)} />
          )}
          {binding && (modTarget || binding.ctl !== undefined || binding.ctlId) && <KnobAssignRow binding={binding} onDone={onDone} />}
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
  const hasMenu = !!(bindingHasMenu(binding) || section);
  useMenuPlacement(open, root, menuRef, preferBelow);
  useMenuDismiss(open, root, () => setOpen(false), menuRef);
  const cancel = () => { if (press.current) { window.clearTimeout(press.current.timer); press.current = null; } };
  // v0.46.2 : le relâché et le glissé sont suivis sur la fenêtre : quand un parent capture le pointeur
  // (ligne de valeur de l'écran SON / EFFETS), ils n'arrivent pas jusqu'ici, et chaque toucher ouvrait
  // le menu comme un appui long (et le glissé continuait sous le menu)
  const watch = (id: number) => {
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== id) return;
      if (press.current && Math.hypot(ev.clientX - press.current.x, ev.clientY - press.current.y) > 6) stop();
    };
    const up = (ev: PointerEvent) => { if (ev.pointerId === id) stop(); };
    const stop = () => {
      cancel();
      window.removeEventListener("pointermove", move, true);
      window.removeEventListener("pointerup", up, true);
      window.removeEventListener("pointercancel", up, true);
    };
    window.addEventListener("pointermove", move, true);
    window.addEventListener("pointerup", up, true);
    window.addEventListener("pointercancel", up, true);
  };
  return (
    <div ref={root} className={`pm-zone${className ? ` ${className}` : ""}`} {...ctlAttrs(binding)}
      onContextMenu={(e) => { if (!hasMenu) return; e.preventDefault(); e.stopPropagation(); cancel(); setOpen(true); }}
      onPointerDownCapture={(e) => {
        if (e.pointerType !== "touch" || !hasMenu || open) return;
        cancel();
        press.current = { x: e.clientX, y: e.clientY, timer: window.setTimeout(() => { press.current = null; markLongPress(); setOpen(true); }, 550) };
        watch(e.pointerId);
      }}>
      {children}
      {open && hasMenu && (
        <MenuShell menuRef={menuRef} label={binding?.label ?? ""}>
          <ParamMenuBody binding={binding} onDone={() => setOpen(false)} />
        </MenuShell>
      )}
    </div>
  );
}

