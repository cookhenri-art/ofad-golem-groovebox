// Groovebox — OFAD — AGPL-3.0-or-later
// v0.32.1 : copier / coller une section d'un synthé (enveloppe, LFO, oscillateur, filtre) sur une
// autre section du même genre du MÊME synthé : l'oscillateur 1 sur le 2, l'enveloppe ampli sur
// l'enveloppe de modulation, le LFO 1 sur le LFO 2… Les réglages se répondent par leur nom
// (« Attaque », « LFO 1 vitesse » ↔ « LFO 2 vitesse », « Demi-tons 1 » ↔ « Demi-tons 2 ») et leur
// unité ; les valeurs passent en unités réelles (800 ms reste 800 ms), ramenées à la plage du
// réglage qui reçoit. Ce qui n'a pas d'équivalent n'est pas touché, et le message le dit.
// v0.39 : les coins A à D de l'orbiteur se copient de la même façon (coin A sur coin C…).

import { createContext, useContext } from "react";
import { globalOf, layout, useStore } from "../engine/store";
import type { ModuleDesc, ParamDesc } from "../engine/protocol";
import { actions, getState, showMessage } from "../engine/store";
import { toNorm, toPlain } from "../engine/params";
import { paramAddress } from "./bindings";

export type SectionKind = "env" | "lfo" | "osc" | "filter" | "envvers" | "lfovers" | "corner";

/** Genre d'une section d'après son titre (les sections « … vers » sont des routages). */
export function sectionKind(title: string): SectionKind | null {
  const t = title.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "");
  const vers = /\bvers\b/.test(t);
  if (/^(env\b|env\.|enveloppe)/.test(t)) return vers ? "envvers" : "env";
  if (/^lfo/.test(t)) return vers ? "lfovers" : "lfo";
  if (/^(oscillateur|osc\b|osc\.|vco|dco)/.test(t)) return "osc";
  if (/^(filtre|passe-haut|passe-bas|commande des filtres|vcf)/.test(t)) return "filter";
  if (/^coin\b/.test(t)) return "corner";
  return null;
}

const KIND_NAMES: Record<SectionKind, string> = {
  env: "enveloppe", lfo: "LFO", osc: "oscillateur", filter: "filtre", envvers: "destinations d'enveloppe", lfovers: "destinations de LFO", corner: "coin",
};

/** Nom d'un réglage débarrassé de son numéro et de sa section : « LFO 1 vitesse » → « vitesse ». */
function roleKey(p: ParamDesc): string {
  const name = p.name.toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "")
    .replace(/\d+/g, " ").replace(/[.·:()]/g, " ")
    .replace(/\b(lfo|osc|env|enveloppe|oscillateur|coin)\b/g, " ")
    .replace(/(^|\s)[abcd](?=\s|$)/g, " ")
    .replace(/\s+/g, " ").trim();
  return `${name}|${p.unit ?? ""}|${p.choices ? p.choices.join("/") : ""}`;
}

interface Copied { moduleType: string; moduleName: string; kind: SectionKind; title: string; entries: { key: string; plain: number; name: string }[] }
let clipboard: Copied | null = null;

export interface SectionMenu {
  title: string;
  kind: SectionKind;
  /** Ce qui peut être collé ici, ou la raison de ne pas pouvoir coller (lu à l'ouverture du menu). */
  paste: () => { ok: boolean; label: string };
  copy: () => void;
  pasteHere: () => void;
  reset: () => void;
}

export const SectionMenuContext = createContext<SectionMenu | null>(null);

/** Menu d'une section : `ids` = identifiants des réglages affichés (banque comprise). */
export function makeSectionMenu(channel: number, m: ModuleDesc, title: string, ids: string[]): SectionMenu | null {
  const kind = sectionKind(title);
  if (!kind) return null;
  const params = ids.map((id) => {
    const index = m.params.findIndex((p) => p.id === id);
    return index < 0 ? null : { index, desc: m.params[index], global: paramAddress(channel, "inst", 0, index) };
  }).filter((x): x is { index: number; desc: ParamDesc; global: number } => !!x);
  const valueOf = (index: number) => getState().channels[channel]?.inst?.[index] ?? m.params[index].def;
  const paste = () => { const c = clipboard; return !c ? { ok: false, label: "Rien de copié" }
    : c.moduleType !== m.type ? { ok: false, label: `Copié sur ${c.moduleName} : se colle sur ce synthé seulement` }
    : c.kind !== kind ? { ok: false, label: `Copié : « ${c.title} » (${KIND_NAMES[c.kind]}), pas un${kind === "env" || kind === "envvers" ? "e" : ""} ${KIND_NAMES[kind]}` }
    : { ok: true, label: `Coller « ${c.title} »` }; };
  return {
    title, kind, paste,
    copy: () => {
      clipboard = {
        moduleType: m.type, moduleName: m.name, kind, title,
        entries: params.map((p) => ({ key: roleKey(p.desc), name: p.desc.name, plain: toPlain(p.desc, valueOf(p.index)) })),
      };
      showMessage(`Copié : « ${title} » (${params.length} réglages) — « Coller » sur une autre section du même genre`);
    },
    pasteHere: () => {
      const src = clipboard;
      if (!src || src.moduleType !== m.type || src.kind !== kind) return;
      // file d'attente par nom : deux « Attaque » se répondent dans l'ordre
      const queue = new Map<string, { plain: number; name: string }[]>();
      for (const e of src.entries) { const q = queue.get(e.key) ?? []; q.push(e); queue.set(e.key, q); }
      let done = 0;
      const missing: string[] = [];
      for (const p of params) {
        const e = queue.get(roleKey(p.desc))?.shift();
        if (!e) { missing.push(p.desc.name); continue; }
        const d = p.desc;
        const plain = Math.min(Math.max(e.plain, Math.min(d.min, d.max)), Math.max(d.min, d.max));
        actions.setParam(p.global, Math.min(1, Math.max(0, toNorm(d, plain))));
        ++done;
      }
      showMessage(done === 0 ? `Rien à coller : « ${src.title} » et « ${title} » n'ont pas de réglages en commun`
        : `Collé dans « ${title} » : ${done} réglage${done > 1 ? "s" : ""}${missing.length ? ` — sans équivalent : ${missing.slice(0, 3).join(", ")}${missing.length > 3 ? "…" : ""}` : ""} (Ctrl+Z pour revenir)`);
    },
    reset: () => {
      for (const p of params) actions.setParam(p.global, Math.min(1, Math.max(0, toNorm(p.desc, p.desc.def))));
      showMessage(`« ${title} » remis aux valeurs d'origine (Ctrl+Z pour revenir)`);
    },
  };
}

/** Ligne de menu : copier, coller, remettre à zéro la section. */
export function SectionMenuRow({ onDone, menu }: { onDone: () => void; menu?: SectionMenu | null }) {
  const ctx = useContext(SectionMenuContext);
  const s = menu ?? ctx;
  if (!s) return null;
  const paste = s.paste();
  return (
    <>
      <span className="mod-menu-title">Section « {s.title} »</span>
      <div className="section-menu-row">
        <button type="button" onClick={() => { s.copy(); onDone(); }}>Copier</button>
        <button type="button" disabled={!paste.ok} title={paste.label} onClick={() => { s.pasteHere(); onDone(); }}>
          {paste.ok ? paste.label : "Coller"}
        </button>
        <button type="button" onClick={() => { s.reset(); onDone(); }} title="Valeurs d'origine de cette section">Remettre à zéro</button>
      </div>
      {!paste.ok && <span className="section-menu-note">{paste.label}</span>}
    </>
  );
}

// ---------------------------------------------------------------------------
// v0.32.1 : envoyer un réglage dans la matrice de modulation du synthé, depuis son menu : on
// choisit la source, le premier routage libre est pris (profondeur +50 %, à régler ensuite dans
// la page Matrice). Toucher une source déjà routée vers ce réglage retire le routage.

interface MatrixInfo { rows: { src: number; dst: number; amt: number }[]; dstIndex: number; sources: string[]; m: ModuleDesc }

function matrixFor(channel: number, local: number): MatrixInfo | null {
  const s = getState();
  const L = s.catalog?.layout;
  if (!L || channel >= L.numTracks || local < L.instrumentBase || local >= L.insertBase) return null;
  const m = s.catalog?.instruments.find((i) => i.type === s.tracks.tracks[channel]?.instrument);
  if (!m) return null;
  const index = local - L.instrumentBase;
  const p = m.params[index];
  if (!p) return null;
  const rows: MatrixInfo["rows"] = [];
  for (let n = 1; ; ++n) {
    const src = m.params.findIndex((x) => x.id === `mx${n}_src`);
    const dst = m.params.findIndex((x) => x.id === `mx${n}_dst`);
    const amt = m.params.findIndex((x) => x.id === `mx${n}_amt`);
    if (src < 0 || dst < 0 || amt < 0) break;
    rows.push({ src, dst, amt });
  }
  if (!rows.length) return null;
  const dstChoices = m.params[rows[0].dst].choices ?? [];
  // destination : « Section · Nom » de la section qui contient le réglage, sinon le seul « … · Nom »
  const sec = m.sections.find((x) => x.params.includes(index));
  let dstIndex = sec ? dstChoices.indexOf(`${sec.name} · ${p.name}`) : -1;
  if (dstIndex < 0) {
    const hits = dstChoices.map((c, i) => (c === p.name || c.endsWith(` · ${p.name}`) ? i : -1)).filter((i) => i > 0);
    if (hits.length === 1) dstIndex = hits[0];
  }
  if (dstIndex <= 0) return null;
  return { rows, dstIndex, sources: m.params[rows[0].src].choices ?? [], m };
}

/** Ligne de menu « Matrice : moduler ce réglage avec… » (synthés qui ont une matrice). */
export function MatrixMenuRow({ target, label, onDone }: { target: { channel: number; local: number }; label: string; onDone: () => void }) {
  useStore((st) => st.channels[target.channel]?.inst); // routages à jour
  const info = matrixFor(target.channel, target.local);
  if (!info) return null;
  const ch = target.channel, m = info.m;
  const L = layout();
  const inst = getState().channels[ch]?.inst ?? [];
  const plainAt = (idx: number) => toPlain(m.params[idx], inst[idx] ?? 0);
  const g = (idx: number) => globalOf(ch, L.instrumentBase + idx);
  const routed = (srcIdx: number) => info.rows.find((r) => plainAt(r.dst) === info.dstIndex && plainAt(r.src) === srcIdx);
  const set = (idx: number, plain: number) => actions.setParam(g(idx), toNorm(m.params[idx], plain));
  const toggle = (srcIdx: number) => {
    const have = routed(srcIdx);
    if (have) {
      set(have.src, 0); set(have.dst, 0); set(have.amt, 0);
      showMessage(`Matrice : ${info.sources[srcIdx]} ne module plus « ${label} »`);
      return;
    }
    const free = info.rows.findIndex((r) => plainAt(r.src) === 0 || plainAt(r.dst) === 0);
    if (free < 0) { showMessage("Matrice pleine : libérez un routage dans la page Matrice"); return; }
    const r = info.rows[free];
    set(r.src, srcIdx); set(r.dst, info.dstIndex);
    const a = m.params[r.amt];
    set(r.amt, a.min < 0 ? a.max * 0.5 : a.min + (a.max - a.min) * 0.5); // +50 %
    showMessage(`Matrice, routage ${free + 1} : ${info.sources[srcIdx]} → ${label} (+50 % ; profondeur dans la page Matrice)`);
  };
  return (
    <>
      <span className="mod-menu-title mod-menu-title--macro">Matrice du synthé : moduler « {label} » avec</span>
      <div className="matrix-menu-grid">
        {info.sources.map((name, i) => (i === 0 || name === "—" ? null : (
          <button key={i} type="button" className={routed(i) ? "on" : ""} aria-pressed={!!routed(i)}
            title={routed(i) ? "Retirer ce routage" : `Ajouter « ${name} → ${label} » à la matrice`}
            onClick={() => { toggle(i); onDone(); }}>{name}</button>
        )))}
      </div>
    </>
  );
}
