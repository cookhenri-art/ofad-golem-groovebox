// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 (lots 2 et 3) : CHAÎNE DE TRAITEMENT de 4 effets quelconques (Restauration comprise), pour
// l'éditeur d'échantillon (« Traiter… ») et l'écran Audio (chaîne d'enregistrement). Les réglages
// vivent dans l'interface (valeurs réelles), gardés d'une fois sur l'autre ; le moteur les reçoit au
// moment du calcul. Chaque effet garde ses réglages et ses préréglages d'usine.

import { useState } from "react";
import { actions, useStore, type ChainSlotUi } from "../engine/store";
import { formatPlain, toNorm, toPlain } from "../engine/params";
import { Encoder, type EncoderBinding } from "../controls/Encoder";
import { EffectPicker } from "./EffectPicker";
import type { ModuleDesc } from "../engine/protocol";

export const CHAIN_LEN = 4;
export const emptyChain = (): ChainSlotUi[] => Array.from({ length: CHAIN_LEN }, () => ({ type: "", values: [], bypass: false }));

/** Chaîne gardée d'une fois sur l'autre (par usage : « proc » éditeur, « rec » écran Audio). */
export function loadChain(key: string): ChainSlotUi[] {
  try {
    const v = JSON.parse(localStorage.getItem(`grv.chain.${key}`) ?? "null");
    if (Array.isArray(v)) return emptyChain().map((e, i) => (v[i] && typeof v[i].type === "string" ? { type: v[i].type, values: Array.isArray(v[i].values) ? v[i].values.map(Number) : [], bypass: !!v[i].bypass } : e));
  } catch { /* stockage indisponible */ }
  return emptyChain();
}
export function saveChain(key: string, chain: ChainSlotUi[]) { try { localStorage.setItem(`grv.chain.${key}`, JSON.stringify(chain)); } catch { /* stockage indisponible */ } }

/** Latence d'un effet de Restauration en direct (affichée sur la pastille ; 0 pour les autres). */
export function liveLatencyMs(m: ModuleDesc | undefined): number {
  const t = m?.type ?? "";
  return t === "rdenoise" ? 43 : t === "rdeclick" ? 21 : t === "rdereverb" ? 43 : t === "rdeclip" ? 11 : t === "rdeess" ? 0 : 0;
}

export function ChainEditor({ chain, onChange, title = "Chaîne · 4 effets" }: { chain: ChainSlotUi[]; onChange: (c: ChainSlotUi[]) => void; title?: string }) {
  const effects = useStore((s) => s.catalog?.effects ?? []);
  const [sel, setSel] = useState(() => Math.max(0, chain.findIndex((c) => c.type)));
  const [picking, setPicking] = useState<number | null>(null);
  const descOf = (type: string) => effects.find((e) => e.type === type);
  const update = (i: number, slot: ChainSlotUi) => onChange(chain.map((c, k) => (k === i ? slot : c)));
  const pick = (i: number, type: string) => {
    const m = descOf(type);
    update(i, { type, values: m ? m.params.map((p) => p.def) : [], bypass: false });
    setSel(i); setPicking(null);
  };
  const cur = chain[sel];
  const m = cur?.type ? descOf(cur.type) : undefined;
  const values = m ? m.params.map((p, k) => cur.values[k] ?? p.def) : [];
  const setValue = (k: number, v: number) => { if (!m) return; const vals = [...values]; vals[k] = v; update(sel, { ...cur, values: vals }); };
  const bindings: EncoderBinding[] = m ? m.params.map((p, k) => {
    const v = values[k];
    if (p.choices && p.choices.length > 1) {
      const i = Math.round(v);
      return { kind: "stepped", label: p.name, text: p.choices[i] ?? "", normalised: toNorm(p, v),
        onStep: (d: 1 | -1) => setValue(k, Math.max(0, Math.min(p.choices!.length - 1, i + d))), reset: () => setValue(k, p.def) };
    }
    return { kind: "continuous", label: p.name, text: formatPlain(p, v), normalised: toNorm(p, v),
      onChange: (n: number) => setValue(k, toPlain(p, n)), reset: () => setValue(k, p.def) };
  }) : [];
  const applyPreset = async (idx: number) => {
    if (!m || idx < 0) return;
    const v = await actions.effectPreset(m.type, idx);
    if (Array.isArray(v)) update(sel, { ...cur, values: v.map(Number) });
  };
  return (
    <div className="chain">
      <span className="chain-title">{title}</span>
      <div className="chain-slots">
        {chain.map((c, i) => {
          const d = c.type ? descOf(c.type) : undefined;
          const lat = liveLatencyMs(d);
          return (
            <div key={i} className={`chain-slot${i === sel ? " sel" : ""}${c.bypass ? " off" : ""}${c.type ? "" : " empty"}`}>
              <button type="button" className="chain-slot-main" onClick={() => (c.type ? setSel(i) : setPicking(i))}
                title={c.type ? "Toucher : ses réglages" : "Ajouter un effet ici"}>
                <span className="chain-n">{i + 1}</span>
                <span className="chain-name">{d ? d.name : "+ Ajouter un effet"}<small>{d ? (d.role ?? d.family) : "Restauration, ou n'importe quel effet"}</small></span>
                {d && <span className={`chain-lat${lat > 0 ? " warn" : ""}`} title={lat > 0 ? "Latence si on l'utilise en direct ; aucune pendant un traitement" : "Sans latence"}>{lat > 0 ? `${lat} ms` : "0 ms"}</span>}
              </button>
              {c.type && <button type="button" className={`chain-pw${c.bypass ? "" : " on"}`} aria-pressed={!c.bypass} aria-label={c.bypass ? "Activer l'effet" : "Contourner l'effet"}
                onClick={() => update(i, { ...c, bypass: !c.bypass })} />}
            </div>
          );
        })}
      </div>
      {m && (
        <div className="chain-edit">
          <div className="chain-edit-head">
            <b>{sel + 1} · {m.name}</b>
            {m.presets.length > 0 && (
              <select className="chip chip--select" value="" onChange={(e) => void applyPreset(Number(e.target.value))} aria-label="Préréglage d'usine">
                <option value="">Préréglages…</option>
                {m.presets.map((p, k) => <option key={k} value={k}>{p}</option>)}
              </select>
            )}
            <span style={{ flex: 1 }} />
            <button type="button" className="touch touch--small" onClick={() => setPicking(sel)}>Changer</button>
            <button type="button" className="touch touch--small" onClick={() => update(sel, { type: "", values: [], bypass: false })}>Retirer</button>
          </div>
          <div className="chain-knobs">
            {bindings.map((b, k) => <Encoder key={`${m.type}:${k}`} binding={b} variant="knob" />)}
          </div>
        </div>
      )}
      {picking !== null && (
        <div className="chain-picker">
          <EffectPicker title={`Effet ${picking + 1} de la chaîne`} current={chain[picking]?.type ?? ""} onPick={(t) => pick(picking, t)}
            onRemove={chain[picking]?.type ? () => { update(picking, { type: "", values: [], bypass: false }); setPicking(null); } : undefined}
            onClose={() => setPicking(null)} />
        </div>
      )}
    </div>
  );
}
