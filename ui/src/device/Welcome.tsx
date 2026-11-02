// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix6 : ACCUEIL de l'application autonome. Au lancement (une fois), la mascotte OFAD GOLEM
// arrive en grand, puis on choisit : reprendre la dernière séance, un projet du dossier, un nouveau
// projet vide, le projet de démonstration ou un fichier. Échap ou « Reprendre » : on garde la séance
// rouverte par l'application. Dans un logiciel hôte, pas d'accueil (le projet vient de la session).

import { useEffect, useState } from "react";
import { FolderBrowser } from "../controls/FolderBrowser";
import { actions, hasNative, readPref, useStore, writePref } from "../engine/store";

export function Welcome() {
  const ready = useStore((s) => s.ready);
  const folders = useStore((s) => s.folders);
  const [open, setOpen] = useState(false);
  const [closing, setClosing] = useState(false);
  const [atStart, setAtStart] = useState(() => readPref("welcome", true));

  useEffect(() => {
    if (!ready) return;
    let alive = true;
    void actions.appStart().then((r) => { if (alive && r.welcome && readPref("welcome", true)) setOpen(true); });
    return () => { alive = false; };
  }, [ready]);

  const close = () => {
    setClosing(true);
    window.setTimeout(() => { setOpen(false); setClosing(false); }, 220);
  };
  useEffect(() => {
    if (!open) return;
    const key = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); e.stopPropagation(); close(); } };
    window.addEventListener("keydown", key, true);
    return () => window.removeEventListener("keydown", key, true);
  }, [open]);

  if (!open) return null;
  const files = hasNative("openProject");
  const run = (f: () => unknown) => { close(); void f(); };

  return (
    <div className={`welcome${closing ? " is-closing" : ""}`} role="dialog" aria-modal="true" aria-label="Accueil : choisir un projet">
      <div className="welcome-hero" aria-hidden>
        <div className="welcome-halo" />
        <img className="welcome-golem" src="golem/golem-512.png" alt="" draggable={false} />
      </div>
      <div className="welcome-panel">
        <div className="welcome-brand">
          <span className="welcome-word">OFAD GOLEM</span>
          <span className="welcome-sub">Groovebox · 16 pistes</span>
        </div>
        <button type="button" className="welcome-main" onClick={close} autoFocus>
          <span className="welcome-main-t">Reprendre la dernière séance</span>
          <span className="welcome-main-s">Le projet tel que vous l'avez laissé</span>
        </button>
        <div className="welcome-row">
          <button type="button" className="touch welcome-btn" onClick={() => run(() => actions.newProject(true))}>Nouveau projet vide</button>
          <button type="button" className="touch welcome-btn" onClick={() => run(() => actions.newProject(false))}>Projet de démonstration</button>
          {files && <button type="button" className="touch welcome-btn" onClick={() => run(() => actions.openProject())}>Ouvrir un fichier…</button>}
        </div>
        {folders && (
          <div className="welcome-list">
            <FolderBrowser kind="projects" title="Mes projets" compact onPick={(rel) => run(() => actions.openProjectFile(rel))} />
          </div>
        )}
        <label className="welcome-foot">
          <input type="checkbox" checked={atStart} onChange={(e) => { setAtStart(e.target.checked); writePref("welcome", e.target.checked); }} />
          Afficher cet accueil au démarrage
          <span className="welcome-esc">Échap : reprendre</span>
        </label>
      </div>
    </div>
  );
}
