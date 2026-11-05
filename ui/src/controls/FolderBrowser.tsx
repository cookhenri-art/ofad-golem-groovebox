// Groovebox — OFAD — AGPL-3.0-or-later
// Bibliothèque : parcourt le dossier des échantillons ou des projets choisi dans les Réglages.
// Toucher un dossier l'ouvre ; toucher un fichier le charge (échantillon sur le pad ou la zone
// affichés, ou projet). Seuls des chemins relatifs au dossier circulent : le moteur refuse tout
// ce qui sortirait du dossier.

import { useCallback, useEffect, useState } from "react";
import { actions, useStore, type FolderList } from "../engine/store";

interface Props {
  kind: "samples" | "projects";
  title: string;
  onPick: (relative: string, name: string) => void;
  onClose?: () => void;
  compact?: boolean;
}

const sizeText = (b: number) => (b >= 1048576 ? `${(b / 1048576).toFixed(1)} Mo` : `${Math.max(1, Math.round(b / 1024))} ko`);

export function FolderBrowser({ kind, title, onPick, onClose, compact }: Props) {
  const folders = useStore((s) => s.folders);
  const [path, setPath] = useState<string[]>([]);
  const [list, setList] = useState<FolderList | null>(null);
  const [filter, setFilter] = useState("");
  const [picked, setPicked] = useState<string>("");
  const root = folders?.[kind]?.path ?? "";

  const rel = path.join("/");
  const reload = useCallback(async () => {
    const l = await actions.listFolder(kind, rel);
    if (!l.ok && rel) { setPath([]); return; }       // sous-dossier disparu : retour à la racine
    setList(l);
  }, [kind, rel]);
  useEffect(() => { void reload(); }, [reload, root]);

  const f = filter.trim().toLowerCase();
  const items = (list?.items ?? []).filter(([name, dir]) => dir === 1 || !f || name.toLowerCase().includes(f));
  const rootName = root.split(/[\\/]/).filter(Boolean).pop() ?? (kind === "samples" ? "Échantillons" : "Projets");

  return (
    <div className={`fb${compact ? " fb--compact" : ""}`} role="region" aria-label={title}>
      <div className="fb-head">
        <span className="fb-title">{title}</span>
        <nav className="fb-crumbs" aria-label="Dossier ouvert">
          <button type="button" onClick={() => setPath([])} title={root}>{rootName}</button>
          {path.map((p, i) => (
            <span key={i}><span aria-hidden>›</span><button type="button" onClick={() => setPath(path.slice(0, i + 1))}>{p}</button></span>
          ))}
        </nav>
        <input className="fb-filter" type="search" placeholder="Filtrer…" value={filter} onChange={(e) => setFilter(e.target.value)} aria-label="Filtrer les fichiers" />
        <button type="button" className="touch touch--small touch--quiet" onClick={() => void reload()} title="Relire le dossier">↻</button>
        {onClose && <button type="button" className="x" onClick={onClose} aria-label="Fermer la bibliothèque">×</button>}
      </div>
      <ul className="fb-list">
        {path.length > 0 && (
          <li><button type="button" className="fb-item fb-item--up" onClick={() => setPath(path.slice(0, -1))}>‹ Dossier parent</button></li>
        )}
        {items.map(([name, dir, bytes]) => {
          const r = [...path, name].join("/");
          return (
            <li key={name}>
              {dir === 1 ? (
                <button type="button" className="fb-item fb-item--dir" onClick={() => { setPath([...path, name]); setFilter(""); }}>
                  <span className="fb-icon" aria-hidden>▸</span><span className="fb-name">{name}</span>
                </button>
              ) : (
                <button type="button" className={`fb-item${picked === r ? " on" : ""}`} onClick={() => { setPicked(r); onPick(r, name); }}>
                  <span className="fb-icon" aria-hidden>{kind === "samples" ? "♪" : "▤"}</span>
                  <span className="fb-name">{name}</span><span className="fb-size tabular">{sizeText(bytes)}</span>
                </button>
              )}
            </li>
          );
        })}
        {list && items.length === 0 && (
          <li className="more">{f ? "Aucun fichier ne correspond." : kind === "samples"
            ? (folders?.canChoose === false
              ? "Aucun fichier audio ici (WAV, AIFF, FLAC, OGG, MP3). Pour ajouter vos sons : « Échantillon… » sur une piste, ou Réglages › Importer des fichiers… / Dossier perso…"
              : "Aucun fichier audio ici (WAV, AIFF, FLAC, OGG, MP3). Choisissez le dossier dans les Réglages.")
            : "Aucun projet ici. Enregistrez-en un dans le dossier, ou choisissez le dossier dans les Réglages."}</li>
        )}
        {list && list.items.length >= 1000 && <li className="more">Les 1000 premiers éléments seulement : rangez le dossier en sous-dossiers.</li>}
      </ul>
    </div>
  );
}
