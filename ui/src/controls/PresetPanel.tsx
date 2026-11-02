// Groovebox — OFAD — AGPL-3.0-or-later
// Préréglages : ceux d'usine et ceux de l'utilisateur, rangés en banques qu'il nomme lui-même.
// Toucher un nom le charge ; « Enregistrer » range le son actuel (instrument, effet ou pad)
// dans une banque. Les préréglages perso servent dans tous les projets.

import { useCallback, useEffect, useState } from "react";
import { actions, hasNative } from "../engine/store";
import type { PresetScope, UserPresetList } from "../engine/protocol";

const KIND_NAMES: Record<string, string> = {
  kick: "grosse caisse", snare: "caisse claire", clap: "clap", rim: "rimshot", hatClosed: "charley fermé", hatOpen: "charley ouvert",
  tom: "tom", cowbell: "cloche", sample: "échantillon", cymbal: "cymbale", fm: "tambour FM",
};
const FACTORY = "Usine";
/** Ordre des banques d'usine (catégories classiques) ; « Classiques » : sons livrés avant les banques. */
const CATEGORY_ORDER = ["Init", "Acide", "Basses", "Leads", "Nappes", "Plucks", "Claviers", "Cordes", "Cuivres", "Arpèges", "Effets", "Classiques"];
const factoryKey = (cat: string) => `${FACTORY}\u0001${cat}`;
const isFactory = (bank: string) => bank === FACTORY || bank.startsWith(`${FACTORY}\u0001`);

/** Même nettoyage que le moteur (UserPresets::cleanName) : ce qui est affiché est ce qui est rangé. */
export function cleanName(raw: string): string {
  let s = raw.slice(0, 200).replace(/[\u0000-\u001f\u007f]/g, "").replace(/[<>:"/\\|?*]/g, " ").trim();
  s = s.replace(/^[.\s]+|[.\s]+$/g, "").replace(/ {2,}/g, " ");
  s = s.slice(0, 60).trim();
  const stem = s.split(".")[0].trim().toUpperCase();
  if (["CON", "PRN", "AUX", "NUL"].includes(stem) || /^(COM|LPT)\d$/.test(stem)) s = "_" + s;
  return s;
}

interface Props {
  scope: PresetScope;
  channel: number;
  slot: number;          // insert (scope « fx »), -1 sinon
  pad: number;           // pad (scope « pad »), -1 sinon
  title: string;         // « Batterie 80 », « Pad 5 · Charley fermé », « Égaliseur »…
  current?: string;      // préréglage actuel (affiché)
  factory?: string[];    // préréglages d'usine du module
  factoryCats?: string[]; // banque d'usine de chacun (même ordre) ; absent = une seule banque « Usine »
  onPickFactory?: (name: string) => void;
  onClose: () => void;
}

export function PresetPanel({ scope, channel, slot, pad, title, current, factory = [], factoryCats, onPickFactory, onClose }: Props) {
  // banques d'usine : une par catégorie quand l'instrument en a, sinon « Usine »
  const catOf = (i: number) => (factoryCats?.[i] || "Classiques");
  const grouped = !!factoryCats && factoryCats.some((c) => c);
  const factoryBanks = !factory.length ? [] : grouped
    ? CATEGORY_ORDER.filter((c) => factory.some((_, i) => catOf(i) === c)).map(factoryKey)
    : [FACTORY];
  const currentIdx = current ? factory.indexOf(current) : -1;
  const firstFactory = factoryBanks.length ? (grouped && currentIdx >= 0 ? factoryKey(catOf(currentIdx)) : factoryBanks[0]) : "";
  const [list, setList] = useState<UserPresetList | null>(null);
  const [bank, setBank] = useState<string>(firstFactory);
  const [saveBank, setSaveBank] = useState("");
  const [saveName, setSaveName] = useState(current ?? "");
  const [confirm, setConfirm] = useState<{ kind: "replace" | "delete"; bank: string; name: string } | null>(null);
  const native = hasNative("listPresets");

  const reload = useCallback(async () => {
    const l = await actions.listPresets(scope, channel, slot, pad);
    setList(l);
    if (l) {
      setBank((b) => (b && (isFactory(b) || l.banks.includes(b)) ? b : l.banks[0] ?? firstFactory));
      setSaveBank((b) => b || l.banks[0] || "Mes sons");
    }
  }, [scope, channel, slot, pad, factory.length, firstFactory]);

  useEffect(() => { void reload(); }, [reload]);
  useEffect(() => {
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape" && !e.defaultPrevented) { e.preventDefault(); onClose(); } };
    window.addEventListener("keydown", esc, true); // en capture : passe avant le bandeau de mode
    return () => window.removeEventListener("keydown", esc, true);
  }, [onClose]);

  const banks = [...factoryBanks, ...(list?.banks ?? [])];
  const items = bank === FACTORY ? factory.map((n) => ({ name: n, kind: "" }))
    : isFactory(bank) ? factory.map((n, i) => ({ name: n, kind: "", cat: catOf(i) })).filter((x) => factoryKey(x.cat) === bank).map(({ name, kind }) => ({ name, kind }))
    : (list?.user ?? []).filter((u) => u[0] === bank).map((u) => ({ name: u[1], kind: u[2] }));

  const save = async (overwrite: boolean) => {
    const b = cleanName(saveBank), n = cleanName(saveName);
    if (!b || !n) return;
    const r = await actions.savePreset(scope, channel, slot, pad, b, n, overwrite);
    if (r === "exists") { setConfirm({ kind: "replace", bank: b, name: n }); return; }
    setConfirm(null);
    if (r === "ok") { await reload(); setBank(b); }
  };

  return (
    <div className="preset-panel" role="dialog" aria-label={`Préréglages · ${title}`}>
      <header className="preset-panel-head">
        <span className="preset-panel-title">Préréglages · {title}</span>
        {current && <span className="chip">{current}</span>}
        <span className="screen-spacer" />
        {hasNative("revealPresets") && <button type="button" className="touch touch--small touch--quiet" onClick={() => actions.revealPresets()} title={list?.root}>Ouvrir le dossier</button>}
        <button type="button" className="touch touch--small" onClick={onClose} aria-label="Fermer">Fermer</button>
      </header>

      <div className="preset-panel-body">
        <nav className="preset-banks" aria-label="Banques">
          <span className="detail-label">Banques</span>
          {banks.length === 0 && <p className="sound-hint">Aucune banque pour l'instant : enregistrez un premier son ci-dessous.</p>}
          {banks.map((b) => (
            <button key={b} type="button" className={`${b === bank ? "on" : ""}${isFactory(b) ? " preset-bank--factory" : ""}`} onClick={() => setBank(b)}
              title={isFactory(b) ? "Banque d'usine" : "Banque perso"}>{b.startsWith(`${FACTORY}\u0001`) ? b.slice(FACTORY.length + 1) : b}</button>
          ))}
        </nav>
        <ul className="preset-items" aria-label={`Préréglages de la banque ${bank}`}>
          {items.length === 0 && <li className="sound-hint">{bank ? "Banque vide." : ""}</li>}
          {items.map(({ name, kind }) => {
            const deleting = confirm?.kind === "delete" && confirm.bank === bank && confirm.name === name;
            const otherKind = scope === "pad" && kind && list?.kind && kind !== list.kind;
            return (
              <li key={name} className={name === current ? "on" : ""}>
                {deleting ? (
                  <span className="preset-confirm">
                    Mettre « {name} » à la corbeille ?
                    <button type="button" className="touch touch--small" onClick={async () => { await actions.deletePreset(scope, channel, slot, pad, bank, name); setConfirm(null); await reload(); }}>Oui</button>
                    <button type="button" className="touch touch--small touch--quiet" onClick={() => setConfirm(null)}>Non</button>
                  </span>
                ) : (
                  <>
                    <button type="button" className="preset-name" onClick={() => {
                      if (isFactory(bank)) onPickFactory?.(name);
                      else void actions.loadPreset(scope, channel, slot, pad, bank, name);
                      setSaveName(name);
                    }}>
                      {name}{otherKind ? <small> · son de {KIND_NAMES[kind] ?? kind}</small> : null}
                    </button>
                    {!isFactory(bank) && (
                      <button type="button" className="preset-del" aria-label={`Mettre « ${name} » à la corbeille`} title="Mettre à la corbeille"
                        onClick={() => setConfirm({ kind: "delete", bank, name })}>×</button>
                    )}
                  </>
                )}
              </li>
            );
          })}
        </ul>
      </div>

      <footer className="preset-save">
        {!native ? (
          <p className="sound-hint">Les préréglages perso sont enregistrés par l'application (dans l'aperçu, ils restent en mémoire).</p>
        ) : null}
        {confirm?.kind === "replace" ? (
          <span className="preset-confirm">
            « {confirm.name} » existe déjà dans « {confirm.bank} ». Le remplacer ?
            <button type="button" className="touch touch--small touch--on" onClick={() => void save(true)}>Remplacer</button>
            <button type="button" className="touch touch--small touch--quiet" onClick={() => setConfirm(null)}>Annuler</button>
          </span>
        ) : (
          <form className="preset-save-form" onSubmit={(e) => { e.preventDefault(); void save(false); }}>
            <label>
              <span className="detail-label">Banque</span>
              <input type="text" value={saveBank} maxLength={60} list="preset-banks" onChange={(e) => setSaveBank(e.target.value)} placeholder="Mes sons" />
              <datalist id="preset-banks">{(list?.banks ?? []).map((b) => <option key={b} value={b} />)}</datalist>
            </label>
            <label>
              <span className="detail-label">Nom</span>
              <input type="text" value={saveName} maxLength={60} onChange={(e) => setSaveName(e.target.value)} placeholder={scope === "pad" ? "Mon son de pad" : "Mon son"} />
            </label>
            <button type="submit" className="touch touch--on" disabled={!saveBank.trim() || !saveName.trim()}>
              {scope === "pad" ? "Enregistrer ce pad" : "Enregistrer ce son"}
            </button>
          </form>
        )}
      </footer>
    </div>
  );
}
