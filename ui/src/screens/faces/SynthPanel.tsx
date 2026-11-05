// Groovebox — OFAD — AGPL-3.0-or-later
// Façades « comme la vraie machine » : chaque instrument décrit son panneau (layouts.ts) en
// sections disposées comme sur la machine dont il reprend l'architecture — boutons rotatifs,
// gros bouton de coupure, curseurs, interrupteurs à LED, sélecteurs à plusieurs positions,
// banques (bus de modulation 1 / 2…). L'aspect évoque la famille de machines (couleurs, joues
// en bois, curseurs…) sans reprendre de nom, de logo ni de dessin d'origine.
// Chaque commande se comporte comme un encodeur : glisser, molette, double-clic, clic droit
// (valeur exacte, modulation, MIDI), verrous de pas et de scène.

import { createContext, useContext, useEffect, useLayoutEffect, useRef, useState, type ReactNode } from "react";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam } from "../../controls/bindings";
import { Encoder, ParamMenuZone } from "../../controls/Encoder";
import { makeSectionMenu, SectionMenuContext, SectionMenuRow } from "../../controls/sectionMenu";
import { useValueDrag } from "../../controls/useValueDrag";
import { SectionVisual } from "../ModuleView";
import { actions, useStore } from "../../engine/store";
import { StepStrip } from "./StepStrip";
import { StepsChoice } from "./StepsChoice";
import { AccentKnob, ElektronPages, KitColumns, StepKeys } from "./KitPanels";
import { MatrixPanel, StepsEditor } from "./MatrixPanel";
import { OctaPanel } from "./OctaPanel";
import { WaveScanPanel } from "./WaveScanPanel";
import { OrbiterPanel, OrbWavesPanel } from "./OrbiterPanel";
import { GlutaLane, GlutaOsc, GlutaPad } from "./GlutaPanel";
import { DfamSeqPanel, DfamTransport } from "./DfamPanel";
import { ImpactOut, ImpactStrips } from "./ImpactPanel";
import { NervePads, NervePattern } from "./NervePanel";
import { VocBandsPanel, VocSourcePanel } from "./VocoderPanel";
import { BigWave, ZonePads } from "./SamplerPanels";
import { BigKnobView } from "./BigKnob";
import { Fm4AlgoPanel, FmAlgoPanel, setGroupBank, useGroupBank } from "./FmPanels";
import { MaskPanel } from "./DronePanels";

/** Sections particulières (autre chose que des commandes de réglage). */
const CUSTOM: Record<string, (p: { channel: number; module: ModuleDesc; args: string[] }) => ReactNode> = {
  matrix: ({ channel, module }) => <MatrixPanel channel={channel} module={module} />,
  octa: ({ channel, module }) => <OctaPanel channel={channel} module={module} />,
  // « steps:préfixe:longueur » — pas d'un LFO à pas, transposition / vélocité de l'arpège
  steps: ({ channel, module, args }) => <StepsEditor channel={channel} module={module} prefix={args[0] ?? ""} count={args[1]} />,
  steps303: () => <StepsChoice kind="synth"><StepStrip /></StepsChoice>,
  elektron: ({ channel, module }) => <ElektronPages channel={channel} module={module} />,
  // « kitcols:3 » : trois boutons au plus par instrument (le pad choisi a tous ses réglages au-dessus)
  kitcols: ({ channel, module, args }) => <KitColumns channel={channel} module={module} max={args[0] ? Number(args[0]) : undefined} />,
  stepkeys8: () => <StepsChoice kind="drum"><StepKeys palette="boum" /></StepsChoice>,
  stepkeys9: () => <StepsChoice kind="drum"><StepKeys palette="club" /></StepsChoice>,
  stepkeysR: () => <StepsChoice kind="drum"><StepKeys palette="machine" /></StepsChoice>,
  accent: () => <AccentKnob />,
  wavescan: ({ channel, module }) => <WaveScanPanel channel={channel} module={module} />,
  orbiter: ({ channel, module }) => <OrbiterPanel channel={channel} module={module} />,
  // v0.47 : Glutamake — pavé de la physique, écran d'un oscillateur (« glutaosc:1 »), couloir (« glutalane:a »)
  glutapad: ({ channel, module }) => <GlutaPad channel={channel} module={module} />,
  glutaosc: ({ channel, module, args }) => <GlutaOsc channel={channel} module={module} osc={args[0] ?? "1"} />,
  glutalane: ({ channel, module, args }) => <GlutaLane channel={channel} module={module} lane={args[0] ?? "a"} />,
  dfamseq: ({ channel, module }) => <DfamSeqPanel channel={channel} module={module} />,
  dfamtransport: ({ channel, module }) => <DfamTransport channel={channel} module={module} />,
  impstrips: ({ channel, module }) => <ImpactStrips channel={channel} module={module} />,
  impout: ({ channel, module }) => <ImpactOut channel={channel} module={module} />,
  stepkeysI: () => <StepsChoice kind="drum"><StepKeys palette="dragon" /></StepsChoice>,
  nervepat: ({ channel, module }) => <NervePattern channel={channel} module={module} />,
  nervepads: ({ channel, module }) => <NervePads channel={channel} module={module} />,
  vocsrc: ({ channel, module }) => <VocSourcePanel channel={channel} module={module} />,
  vocbands: ({ channel, module }) => <VocBandsPanel channel={channel} module={module} />,
  // « orbwaves:a » : les ondes des deux générateurs du coin choisi
  orbwaves: ({ channel, module, args }) => <OrbWavesPanel channel={channel} module={module} corner={args[0] ?? ""} />,
  // « bigwave:zone3 » : forme d'onde en grand d'une section (« {n} » = zone choisie)
  bigwave: ({ channel, module, args }) => <BigWave channel={channel} module={module} sectionId={args[0] ?? ""} />,
  zonepads: ({ module, args }) => <ZonePads module={module} big={args[0] === "big"} />,
  fmalgo: ({ channel, module }) => <FmAlgoPanel channel={channel} module={module} />,
  fm4algo: ({ channel, module }) => <Fm4AlgoPanel channel={channel} module={module} />,
  bitmask: ({ channel, module }) => <MaskPanel channel={channel} module={module} />,
};

export type CtlKind = "knob" | "big" | "small" | "fader" | "toggle" | "switch" | "vswitch" | "screen" | "menu";

/** Façade à grandes inscriptions (v0.29) : noms et valeurs des boutons écrits hors du bouton. */
export const BigTextContext = createContext(false);

export interface Ctl {
  id: string;            // identifiant du réglage ; « {b} » est remplacé par la banque choisie
  k?: CtlKind;
  label?: string;        // libellé de façade (sinon celui du réglage)
  short?: string[];      // libellés courts des positions (interrupteurs)
  on?: number;           // interrupteur : position allumée (par défaut toute position autre que 0)
  bit?: number;          // interrupteur qui allume / éteint un bit de la valeur (chorus I et II cumulables)
}
export type CtlSpec = string | Ctl | { stack: (string | Ctl)[] } | { gap: true };

export interface Panel {
  title: string;
  ctl: CtlSpec[];
  cols?: number;          // colonnes de la grille des commandes
  grow?: number;          // part de la largeur de la rangée
  visual?: string;        // identifiant d'une section de l'instrument : son graphique en petit écran
  banks?: { label: string; value: string }[]; // banques (« {b} » dans les identifiants)
  tone?: string;          // couleur propre de la section (bandeau, curseurs)
  custom?: string;        // section particulière (voir CUSTOM)
  copyExtra?: string[];   // réglages copiés avec la section sans être des boutons (ondes W de l'orbiteur, v0.39)
  wideLabels?: boolean;   // v0.43.6 : noms des boutons plus larges que le bouton (section aérée : noms entiers)
  bankGroup?: string;     // v0.44 : banque partagée par plusieurs sections (même opérateur choisi partout)
}

export interface FacePage { id: string; label: string; rows: Panel[][] }
export interface FaceLayout {
  theme: string;          // habillage : fatsub, five, chorus, pointe, onde, nappe, acid, boum, club, machine…
  pages: FacePage[];
  wheels?: boolean;       // molettes de hauteur et de modulation à gauche du clavier
  bigText?: boolean;      // noms et valeurs des boutons en grandes inscriptions, hors du bouton (v0.29)
}

const normCtl = (c: string | Ctl): Ctl => (typeof c === "string" ? { id: c } : c);

/** Une page de façade : ses rangées de sections. */
export function PanelPage({ channel, module: m, page, lane, bigText = false }: { channel: number; module: ModuleDesc; page: FacePage; lane: number; bigText?: boolean }) {
  // Le panneau garde ses proportions : il est mis à l'échelle pour tenir dans la façade.
  // v0.28, « Agrandir » (réglage gardé) : une page qui devrait être réduite sous la taille
  // normale pour tenir passe en grand — sections à la suite, passées à la ligne, et la façade
  // défile vers le bas. Une page qui tient déjà à sa taille normale garde sa disposition.
  const ref = useRef<HTMLDivElement>(null);
  // v0.46 fix20 : sur téléphone, toujours « grand » : les sections passent à la ligne et la façade défile
  const phone = useStore((s) => !!s.ui.phone);
  const big = useStore((s) => s.ui.faceBig) || phone;
  const [flow, setFlow] = useState(false);
  // v0.31 : une page qui déborde en largeur même passée à la ligne (bandeau de pistes d'une
  // batterie, d'un seul tenant) n'est pas mise en grand : elle reste ajustée à l'écran.
  const noFlow = useRef(false);
  useLayoutEffect(() => { setFlow(phone); noFlow.current = false; }, [page, m, big, phone]);
  useLayoutEffect(() => {
    const el = ref.current, host = el?.parentElement;
    if (!el || !host) return;
    host.classList.toggle("is-flow", flow);
    if (flow) {
      let w0 = host.clientWidth;
      const fitBig = () => {
        const W = host.clientWidth * 0.99;
        if (W <= 0) return;
        // fenêtre nettement agrandie : on revoit si la page tient sans défiler
        if (Math.abs(host.clientWidth - w0) > 0.1 * w0) { w0 = host.clientWidth; setFlow(false); return; }
        let z = Math.floor(Math.max(1, Math.min(1.5, W / 1500)) * 50) / 50;
        el.style.width = `${W / z}px`;
        el.style.zoom = String(z);
        // v0.45.2 : une section plus large que la page ne fait plus repasser la façade en « ajustée » :
        // la page grande est un peu réduite en largeur pour la contenir, et défile toujours vers le bas.
        for (let k = 0; k < 4 && el.scrollWidth > el.clientWidth + 2; ++k) {
          z = Math.floor((W / (el.scrollWidth + 4)) * 100) / 100;
          el.style.width = `${W / z}px`;
          el.style.zoom = String(z);
        }
        // seule une page impossible à contenir même très réduite reste ajustée à l'écran
        if (z < 0.55) { noFlow.current = true; setFlow(false); }
      };
      fitBig();
      const ro = new ResizeObserver(fitBig);
      ro.observe(host);
      return () => { ro.disconnect(); host.classList.remove("is-flow"); };
    }
    // Le plus grand agrandissement (jusqu'à 1,8) qui tient dans la place, en largeur comme en
    // hauteur (plein écran, grande fenêtre : boutons et inscriptions plus grands).
    const fit = () => {
      const W = host.clientWidth * 0.99, H = host.clientHeight * 0.99;
      if (W <= 0 || H <= 0) return;
      el.style.zoom = "1";
      const fits = (z: number) => {
        el.style.width = `${W / z}px`;
        return el.scrollHeight * z <= H && el.scrollWidth <= W / z + 3; // tolérance : arrondis de sous-pixels (filets des sections)
      };
      let lo = 0.5, hi = 1.8;
      if (fits(hi)) lo = hi;
      else if (!fits(lo)) hi = lo;
      else for (let k = 0; k < 8; ++k) { const mid = (lo + hi) / 2; if (fits(mid)) lo = mid; else hi = mid; }
      const z = Math.floor(lo * 50) / 50;
      // grandes inscriptions : déjà 40 % plus grandes, on accepte de les réduire jusqu'à 0,88
      if (big && !noFlow.current && z < (bigText ? 0.88 : 1)) { setFlow(true); return; }
      el.style.width = `${W / z}px`;
      el.style.zoom = String(z);
    };
    fit();
    const ro = new ResizeObserver(fit);
    ro.observe(host);
    // v0.35.1 : les polices de l'habillage arrivent après le premier calcul ; on recalcule une fois chargées
    let live = true;
    void document.fonts?.ready.then(() => { if (live) fit(); });
    return () => { live = false; ro.disconnect(); };
  }, [page, m, big, flow, bigText]);
  return (
    <BigTextContext.Provider value={bigText}>
    <div ref={ref} className={`fp-page${flow ? " fp-page--big" : ""}${bigText ? " fp-page--bigtext" : ""}`}>
      {flow ? (
        <div className="fp-row fp-row--flow">
          {page.rows.flatMap((row, r) => row.map((p, i) => <PanelSection key={`${r}:${i}:${p.title}`} channel={channel} module={m} panel={p} lane={lane} />))}
        </div>
      ) : page.rows.map((row, r) => (
        <div key={r} className="fp-row">
          {row.map((p, i) => <PanelSection key={`${r}:${i}:${p.title}`} channel={channel} module={m} panel={p} lane={lane} />)}
        </div>
      ))}
    </div>
    </BigTextContext.Provider>
  );
}

function PanelSection({ channel, module: m, panel, lane }: { channel: number; module: ModuleDesc; panel: Panel; lane: number }) {
  const [localBank, setLocalBank] = useState(0);
  const shared = useGroupBank(panel.bankGroup);
  const bank = shared ?? localBank;
  const setBank = (i: number) => { if (panel.bankGroup) setGroupBank(panel.bankGroup, i); else setLocalBank(i); };
  const b = panel.banks?.[Math.min(bank, panel.banks.length - 1)]?.value ?? "";
  // « {z} » : numéro de la zone / du pad choisi sur 2 chiffres, « {n} » : sans zéro (samplers)
  const z = String(lane + 1).padStart(2, "0"), n = String(lane + 1);
  const sub = (c: Ctl): Ctl => ({ ...c, id: c.id.replace("{b}", b).replace("{z}", z).replace("{n}", n), ...(c.label ? { label: c.label.replace("{b}", b) } : {}) });
  const visual = panel.visual ? m.sections.find((s) => s.id === panel.visual) : undefined;
  // v0.32.1 : copier / coller / remettre à zéro la section (enveloppes, LFO, oscillateurs, filtres)
  const bankLabel = panel.banks?.[Math.min(bank, panel.banks.length - 1)]?.label;
  const sectionTitle = bankLabel ? `${panel.title} · ${bankLabel}` : panel.title;
  const ctlIds = panel.ctl.flatMap((c) => (typeof c === "object" && "gap" in c ? [] : typeof c === "object" && "stack" in c ? c.stack.map((x) => sub(normCtl(x)).id) : [sub(normCtl(c as string | Ctl)).id]));
  if (panel.copyExtra) ctlIds.push(...panel.copyExtra.map((id) => id.replace("{b}", b)));
  useStore((s) => s.channels[channel]?.inst); // valeurs à jour pour « Copier »
  const sectionMenu = makeSectionMenu(channel, m, sectionTitle, ctlIds);
  const [sectionOpen, setSectionOpen] = useState(false);
  // v0.46 fix26 : façade « en flux » (téléphone, grande taille) : un petit panneau rangé en colonne
  // (Mélangeur, Séquence…) passe à l'horizontale : une colonne par commande, jusqu'à 6
  const nItems = panel.ctl.filter((c) => !(typeof c === "object" && "gap" in c)).length;
  const flowCols = (panel.cols ?? 4) < nItems && nItems <= 6 ? nItems : panel.cols ?? 4;
  return (
    <SectionMenuContext.Provider value={sectionMenu}>
    <section className={panel.wideLabels ? "fp fp--wide-labels" : "fp"} style={{ "--cols": panel.cols ?? 4, "--cols-flow": flowCols, "--grow": panel.grow ?? 1, ...(panel.tone ? { "--tone": panel.tone } : {}),
      // le bandeau (titre + banques) est posé au-dessus du cadre : la section garde assez de largeur pour lui
      ...(panel.banks ? { minWidth: `${36 + panel.title.length * 9 + panel.banks.reduce((n, x) => n + 30 + x.label.length * 7, 0)}px` } : {}) } as React.CSSProperties}>
      <header className="fp-head">
        <h3 className="fp-title">{panel.title}</h3>
        {panel.banks && (
          <span className="fp-banks" role="tablist" aria-label={`${panel.title} : banque`}>
            {panel.banks.map((x, i) => (
              <button key={x.value} type="button" role="tab" aria-selected={i === bank} className={i === bank ? "on" : ""} onClick={() => setBank(i)}>
                <span className="fc-led" aria-hidden />{x.label}
              </button>
            ))}
          </span>
        )}
        {sectionMenu && <SectionCopyButton open={sectionOpen} setOpen={setSectionOpen} />}
      </header>
      {visual && (
        <div className="fp-screen">
          <SectionVisual channel={channel} area="inst" slot={0} module={m} section={visual} lane={lane} />
        </div>
      )}
      {(() => {
        if (!panel.custom) return null;
        const [key, ...args] = panel.custom.replace(/\{b\}/g, b).replace(/\{n\}/g, n).split(":");
        return CUSTOM[key] ? CUSTOM[key]({ channel, module: m, args }) : null;
      })()}
      <div className="fp-grid">
        {panel.ctl.map((c, i) => {
          if (typeof c === "object" && "gap" in c) return <span key={i} className="fc-gap" />;
          if (typeof c === "object" && "stack" in c)
            return <div key={i} className="fc-stack">{c.stack.map((x, k) => <Control key={k} channel={channel} module={m} ctl={sub(normCtl(x))} />)}</div>;
          return <Control key={i} channel={channel} module={m} ctl={sub(normCtl(c as string | Ctl))} />;
        })}
      </div>
    </section>
    </SectionMenuContext.Provider>
  );
}

/** Petit bouton « ⧉ » du titre d'une section : copier, coller, remettre à zéro (v0.32.1). */
function SectionCopyButton({ open, setOpen }: { open: boolean; setOpen: (v: boolean) => void }) {
  const root = useRef<HTMLSpanElement>(null);
  useEffect(() => {
    if (!open) return;
    const close = (e: PointerEvent | KeyboardEvent) => {
      if (e instanceof KeyboardEvent) { if (e.key === "Escape") setOpen(false); }
      else if (!root.current?.contains(e.target as Node)) setOpen(false);
    };
    window.addEventListener("pointerdown", close, true);
    window.addEventListener("keydown", close, true);
    return () => { window.removeEventListener("pointerdown", close, true); window.removeEventListener("keydown", close, true); };
  }, [open, setOpen]);
  return (
    <span ref={root} className="fp-copy-wrap">
      <button type="button" className={`fp-copy${open ? " on" : ""}`} onClick={() => setOpen(!open)}
        title="Copier, coller ou remettre à zéro cette section (aussi au clic droit sur ses boutons)" aria-label="Copier / coller la section">⧉</button>
      {open && <div className="mod-menu fp-copy-menu" role="menu"><SectionMenuRow onDone={() => setOpen(false)} /></div>}
    </span>
  );
}

function Control({ channel, module: m, ctl }: { channel: number; module: ModuleDesc; ctl: Ctl }) {
  const index = m.params.findIndex((p) => p.id === ctl.id);
  if (index < 0) return null;
  const global = paramAddress(channel, "inst", 0, index);
  const d = m.params[index];
  const kind: CtlKind = ctl.k ?? (ctl.bit !== undefined || (d.choices && d.choices.length === 2) ? "toggle" : "knob");
  // v0.43.6 : marche / arrêt de l'arpège (le mode reste choisi)
  if (ctl.id === "arp_on") {
    const mi = m.params.findIndex((p) => p.id === "arp_mode");
    if (mi >= 0) return <ArpToggle global={global} modeGlobal={paramAddress(channel, "inst", 0, mi)} ctl={ctl} />;
  }
  switch (kind) {
    case "toggle": return <Toggle global={global} ctl={ctl} />;
    case "switch": case "vswitch": return <Switch global={global} ctl={ctl} vertical={kind === "vswitch"} />;
    case "fader": return <Fader global={global} ctl={ctl} />;
    case "menu": return <Menu global={global} ctl={ctl} />;
    default: return <KnobCtl global={global} ctl={ctl} size={kind} selector={!!d.choices?.length} />;
  }
}

function KnobCtl({ global, ctl, size, selector = false }: { global: number; ctl: Ctl; size: CtlKind; selector?: boolean }) {
  const b = useParam(global, ctl.label);
  const bigText = useContext(BigTextContext);
  if (bigText) return <BigKnobView binding={b} label={ctl.label ?? b?.label} size={size === "big" ? 1.3 : size === "small" ? 0.85 : 1} className={`fc fc-bk fc-bk--${size}`} />;
  // v0.35.1 : un sélecteur à crans (liste de choix) prend le bouton à bec de l'habillage
  return <div className={`fc fc--${size}${selector ? " fc--sel" : ""}`}><Encoder binding={b} variant="knob" /></div>;
}

/** Afficheur à flèches (v0.29) : ◀ valeur ▶, pour les listes de plus de quatre choix. */
function Menu({ global, ctl }: { global: number; ctl: Ctl }) {
  const b = useParam(global, ctl.label);
  const e = b?.entry;
  if (!b || !e) return null;
  const count = e.choices ? e.choices.length : Math.round(e.max - e.min) + 1;
  const cur = Math.round(e.choices ? e.plain : e.plain - e.min);
  const go = (d: number) => { const n = (cur + d + count) % count; e.pick(e.choices ? n : e.min + n); };
  return (
    <ParamMenuZone binding={b}>
    <div className={`fc fc-menu${b.locked ? ` fc--lock-${b.locked}` : ""}`}>
      <span className="fc-menu-label">{ctl.label ?? b.label}</span>
      <div className="fc-menu-box">
        <button type="button" onClick={() => go(-1)} aria-label={`${b.label} : précédent`}>◀</button>
        <span className="fc-menu-value" aria-live="polite">{ctl.short?.[cur] ?? b.text}</span>
        <button type="button" onClick={() => go(1)} aria-label={`${b.label} : suivant`}>▶</button>
      </div>
    </div>
    </ParamMenuZone>
  );
}

/** Interrupteur à LED : deux positions, ou plusieurs (chaque appui passe à la suivante). */
function Toggle({ global, ctl }: { global: number; ctl: Ctl }) {
  const b = useParam(global, ctl.label);
  const e = b?.entry;
  if (!b || !e) return null;
  const count = e.choices ? e.choices.length : Math.round(e.max - e.min) + 1;
  const cur = Math.round(e.choices ? e.plain : e.plain - e.min);
  const lit = ctl.bit !== undefined ? (cur & ctl.bit) !== 0 : ctl.on !== undefined ? cur === ctl.on : cur !== 0;
  const press = () => {
    const next = ctl.bit !== undefined ? Math.min(count - 1, cur ^ ctl.bit)
      : ctl.on !== undefined && count === 2 ? (lit ? 1 - ctl.on : ctl.on) : (cur + 1) % count;
    e.pick(e.choices ? next : e.min + next);
  };
  return (
    <ParamMenuZone binding={b} className="pm-zone--toggle">
    <button type="button" className={`fc-toggle${lit ? " on" : ""}${b.locked ? ` fc--lock-${b.locked}` : ""}`} aria-pressed={lit}
      title={`${b.label} : ${b.text}`} onClick={press} onDoubleClick={(ev) => ev.preventDefault()}>
      <span className="fc-led" aria-hidden />
      <span className="fc-toggle-label">{ctl.label ?? b.label}</span>
      {count > 2 && ctl.bit === undefined && <span className="fc-toggle-value">{b.text}</span>}
    </button>
    </ParamMenuZone>
  );
}

/** v0.43.6 : arpège en marche / à l'arrêt. Allumé = en marche avec un mode choisi ; l'arrêter garde le
    mode ; le remettre en marche sans mode choisi (« Arrêt ») prend « Haut ». */
function ArpToggle({ global, modeGlobal, ctl }: { global: number; modeGlobal: number; ctl: Ctl }) {
  const b = useParam(global, ctl.label);
  const mb = useParam(modeGlobal);
  const e = b?.entry, me = mb?.entry;
  if (!b || !e || !me) return null;
  const lit = Math.round(e.plain) !== 0 && Math.round(me.plain) > 0;
  const press = () => {
    if (lit) { e.pick(0); return; }
    e.pick(1);
    if (Math.round(me.plain) === 0) me.pick(1);
  };
  return (
    <ParamMenuZone binding={b} className="pm-zone--toggle">
    <button type="button" className={`fc-toggle${lit ? " on" : ""}${b.locked ? ` fc--lock-${b.locked}` : ""}`} aria-pressed={lit}
      title={lit ? "Arrêter l'arpège (le mode reste choisi)" : "Mettre l'arpège en marche"} onClick={press} onDoubleClick={(ev) => ev.preventDefault()}>
      <span className="fc-led" aria-hidden />
      <span className="fc-toggle-label">{ctl.label ?? b.label}</span>
    </button>
    </ParamMenuZone>
  );
}

/** Sélecteur à positions (une LED par position), horizontal ou vertical. */
function Switch({ global, ctl, vertical }: { global: number; ctl: Ctl; vertical: boolean }) {
  const b = useParam(global, ctl.label);
  const e = b?.entry;
  if (!b || !e) return null;
  const labels = e.choices ?? Array.from({ length: Math.round(e.max - e.min) + 1 }, (_, i) => String(e.min + i));
  const cur = Math.round(e.choices ? e.plain : e.plain - e.min);
  return (
    <ParamMenuZone binding={b}>
    <div className={`fc-switch${vertical ? " fc-switch--v" : ""}${b.locked ? ` fc--lock-${b.locked}` : ""}`} role="radiogroup" aria-label={b.label}>
      <span className="fc-switch-label">{ctl.label ?? b.label}</span>
      <div className="fc-switch-opts">
        {labels.map((l, i) => (
          <button key={i} type="button" role="radio" aria-checked={i === cur} className={i === cur ? "on" : ""} title={l}
            onClick={() => e.pick(e.choices ? i : e.min + i)}>
            <span className="fc-led" aria-hidden /><span>{ctl.short?.[i] ?? l}</span>
          </button>
        ))}
      </div>
    </div>
    </ParamMenuZone>
  );
}

/** Curseur vertical (principe des polyphoniques à curseurs) : tirer la manette ou la piste. */
function Fader({ global, ctl }: { global: number; ctl: Ctl }) {
  const b = useParam(global, ctl.label);
  const track = useRef<HTMLDivElement>(null);
  // v0.45.1 : course de toute la plage sur ~300 px (ou deux hauteurs de piste) — ni trop lent
  // (plusieurs reprises), ni trop vif (un petit geste qui balaie tout) ; Maj : réglage fin
  const { handlers } = useValueDrag(b, { span: () => Math.max(300, 2 * ((track.current?.getBoundingClientRect().height ?? 0) - 14)) });
  if (!b) return null;
  const n = b.normalised;
  return (
    <ParamMenuZone binding={b}>
    <div className={`fc fc-fader${b.locked ? ` fc--lock-${b.locked}` : ""}`}>
      <span className="fc-fader-label" title={b.label}>{ctl.label ?? b.label}</span>
      <div ref={track} className="fc-fader-track" role="slider" aria-label={b.label} aria-valuemin={0} aria-valuemax={1}
        aria-valuenow={Number(n.toFixed(3))} aria-valuetext={b.text} tabIndex={0} {...handlers}>
        <span className="fc-fader-cap" style={{ bottom: `calc(${n} * (100% - 14px))` }} />
      </div>
      <span className="fc-fader-value tabular">{b.text}</span>
    </div>
    </ParamMenuZone>
  );
}

/** Molettes de hauteur (revient au centre) et de modulation (reste où on la laisse). */
export function Wheels({ track }: { track: number }) {
  // v0.32.1 : les molettes suivent aussi celles du contrôleur MIDI (valeurs appliquées par le moteur)
  const bend = useStore((s) => (s.focus === track ? s.tick.whl?.[0] : undefined));
  const wheel = useStore((s) => (s.focus === track ? s.tick.whl?.[1] : undefined));
  return (
    <div className="wheels">
      <Wheel label="Hauteur" spring ext={bend} onValue={(v) => actions.expression(track, 0, v)} />
      <Wheel label="Modulation" ext={wheel} onValue={(v) => actions.expression(track, 1, v)} />
    </div>
  );
}

function Wheel({ label, spring, ext, onValue }: { label: string; spring?: boolean; ext?: number; onValue: (v: number) => void }) {
  const [local, setV] = useState(0);
  const drag = useRef<{ y: number; v: number; id: number } | null>(null);
  // pendant qu'on la tire à l'écran : la valeur locale ; sinon celle du moteur (contrôleur compris)
  const v = drag.current || ext === undefined || !Number.isFinite(ext) ? local : Math.max(spring ? -1 : 0, Math.min(1, ext));
  const set = (x: number) => {
    const c = Math.max(spring ? -1 : 0, Math.min(1, x));
    setV(c);
    onValue(c);
  };
  const span = spring ? 2 : 1;
  const pos = spring ? (v + 1) / 2 : v;
  return (
    <div className="wheel">
      <div className={`wheel-slot${spring ? " wheel-slot--spring" : ""}`} role="slider" aria-label={label} tabIndex={0}
        aria-valuemin={spring ? -1 : 0} aria-valuemax={1} aria-valuenow={Number(v.toFixed(2))}
        onPointerDown={(e) => { e.currentTarget.setPointerCapture(e.pointerId); drag.current = { y: e.clientY, v, id: e.pointerId }; }}
        onPointerMove={(e) => { const d = drag.current; if (d && d.id === e.pointerId) set(d.v + ((d.y - e.clientY) / 70) * span); }}
        onPointerUp={() => { drag.current = null; if (spring) set(0); }}
        onPointerCancel={() => { drag.current = null; if (spring) set(0); }}
        onWheel={(e) => { if (!spring) set(v + (e.deltaY < 0 ? 0.05 : -0.05)); }}
        onDoubleClick={() => set(0)}
        onKeyDown={(e) => { if (e.key === "ArrowUp") set(v + 0.05); if (e.key === "ArrowDown") set(v - 0.05); }}>
        <span className="wheel-drum" style={{ backgroundPositionY: `${-pos * 40}px` }} />
        <span className="wheel-mark" style={{ top: `calc(${1 - pos} * (100% - 6px))` }} />
      </div>
      <span className="wheel-label">{label}</span>
    </div>
  );
}

export function PanelHint({ children }: { children: ReactNode }) { return <p className="fp-hint">{children}</p>; }
