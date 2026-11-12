// Groovebox — OFAD — AGPL-3.0-or-later
// Écran RÉGLAGES : projet (nouveau, ouvrir, enregistrer, projets du dossier), dossiers de travail
// (échantillons, projets, préréglages), enregistrement et lancement,
// contrôleur MIDI (apprentissage des commandes du crossfader, de Break, de Fill et de la lecture,
// liste de toutes les affectations), raccourcis du clavier.

import { switchLang } from "../lang";
import { useEffect, useRef, useState } from "react";
import { actions, CURVE_CPU_LIMIT, getState, hasNative, readPref, setUi, showMessage, useStore, writePref, type FolderKind } from "../engine/store";
import { busName } from "../engine/params";
import { FolderBrowser } from "../controls/FolderBrowser";
import { cleanName } from "../controls/PresetPanel";
import { ROOTS, SCALE_GROUPS, SCALES } from "../engine/scales";
import { LAUNCH_QUANTS, MIDI_BREAK, MIDI_FILL, MIDI_BUS_MUTE_BASE, MIDI_BUS_SOLO_BASE, MIDI_MUTE_BASE, MIDI_PLAY, MIDI_ROW_BASE, PATTERN_NAMES, MIDI_ROW_NEXT, MIDI_ROW_PREV, MIDI_ROW_STOP, MIDI_PHOTO, MIDI_RECALL, MIDI_REC_MOVES, MIDI_RECORD, MIDI_SOLO_BASE, MIDI_SONG_MODE, MIDI_START, MIDI_STOP, MIDI_XFADE, MIDI_KNOB_BASE, MIDI_HOST_BASE, HOST_CTL, HOST_CTL_NAMES, isKnobCode, isHostCode, isUiCode } from "../engine/protocol";
import { uiCtlName } from "../engine/ctl";
import { describeGlobal } from "../engine/describe";
import { steppedBinding } from "../controls/bindings";
import { ScreenFrame } from "./ScreenFrame";

const SPECIALS: { target: number; label: string; hint: string }[] = [
  { target: MIDI_XFADE, label: "Crossfader", hint: "un fader ou un bouton rotatif (CC)" },
  { target: MIDI_BREAK, label: "Break", hint: "un pad (note) ou un bouton (CC), maintenu" },
  { target: MIDI_FILL, label: "Fill", hint: "un pad (note) ou un bouton (CC), maintenu" },
  { target: MIDI_PLAY, label: "Lecture / arrêt", hint: "un pad ou un bouton" },
  { target: MIDI_START, label: "Lecture", hint: "un pad ou un bouton" },
  { target: MIDI_STOP, label: "Arrêt", hint: "un pad ou un bouton" },
  { target: MIDI_RECORD, label: "Enregistrement", hint: "un pad ou un bouton (bascule)" },
  { target: MIDI_REC_MOVES, label: "Enregistrer les mouvements", hint: "morceau : un bouton (bascule)" },
  { target: MIDI_ROW_PREV, label: "Rangée précédente", hint: "un pad ou un bouton : lance la rangée d'avant (B → A…), en sautant les rangées vides" },
  { target: MIDI_ROW_NEXT, label: "Rangée suivante", hint: "un pad ou un bouton : lance la rangée d'après (A → B, H → I…), en sautant les rangées vides" },
  { target: MIDI_ROW_STOP, label: "Arrêter toutes les pistes", hint: "un pad ou un bouton" },
  { target: MIDI_PHOTO, label: "Photo des réglages", hint: "un pad ou un bouton" },
  { target: MIDI_RECALL, label: "Rappel de la photo", hint: "un pad ou un bouton" },
  { target: MIDI_SONG_MODE, label: "Mode morceau", hint: "un bouton (bascule)" },
];

/** v0.46 : nom d'un bus pour la liste des commandes apprises (16-19 retours, 20 principal, 21-24 groupes). */
function busLabel(ch: number): string {
  const cat = getState().catalog;
  return cat ? busName(cat.layout, ch) : `bus ${ch + 1}`;
}

const CHANNELS = Array.from({ length: 17 }, (_, i) => i);

export function targetName(target: number): string {
  const s = SPECIALS.find((x) => x.target === target);
  if (s) return s.label;
  if (target <= MIDI_SOLO_BASE && target > MIDI_SOLO_BASE - 16) return `Solo piste ${MIDI_SOLO_BASE - target + 1}`;
  if (target <= MIDI_MUTE_BASE && target > MIDI_MUTE_BASE - 16) return `Coupure piste ${MIDI_MUTE_BASE - target + 1}`;
  if (target <= MIDI_ROW_BASE && target > MIDI_ROW_BASE - PATTERN_NAMES.length) return `Lancer la rangée ${PATTERN_NAMES[MIDI_ROW_BASE - target]}`;
  if (target <= MIDI_BUS_SOLO_BASE && target > MIDI_BUS_SOLO_BASE - 12) return `Solo ${busLabel(16 + MIDI_BUS_SOLO_BASE - target)}`;
  if (target <= MIDI_BUS_MUTE_BASE && target > MIDI_BUS_MUTE_BASE - 12) return `Coupure ${busLabel(16 + MIDI_BUS_MUTE_BASE - target)}`;
  // v0.46 fix24 : boutons assignables, réglages globaux, valeurs d'écran
  if (isKnobCode(target)) return `Bouton assignable ${MIDI_KNOB_BASE - target + 1}`;
  if (isHostCode(target)) { const id = HOST_CTL[MIDI_HOST_BASE - target]; return HOST_CTL_NAMES[id] ?? id; }
  if (isUiCode(target)) return uiCtlName(target);
  return describeGlobal(target);
}

const FOLDERS: { kind: FolderKind; label: string; hint: string }[] = [
  { kind: "samples", label: "Échantillons", hint: "la Bibliothèque des pads et des samplers s'ouvre ici" },
  { kind: "projects", label: "Projets", hint: "Ouvrir… et Enregistrer… partent d'ici ; la liste ci-dessus le montre" },
  { kind: "presets", label: "Préréglages", hint: "un dossier par synthé ou par effet, puis vos banques" },
];

export const midiSource = (kind: number, ch: number, num: number) => `${kind === 0 ? "CC" : "Note"} ${num} · canal ${ch + 1}`;

export function SettingsScreen() {
  const midi = useStore((s) => s.midi);
  const route = midi.route ?? { note: 0, ctrl: 0, ctrlOn: true, tracks: [] };
  const recQuant = useStore((s) => s.pattern?.recQuant ?? 100);
  const launch = useStore((s) => s.launch);
  const ui = { launchAll: useStore((s) => s.ui.launchAll), curveAnim: useStore((s) => s.ui.curveAnim), curveAutoOff: useStore((s) => s.ui.curveAutoOff) };
  const files = hasNative("saveProject");
  // v0.46 fix27 : retour visuel des boutons du projet
  const [lit, setLit] = useState<string | null>(null);
  const litTimer = useRef(0);
  useEffect(() => () => window.clearTimeout(litTimer.current), []);
  const press = (id: string, run: () => unknown, done?: string) => {
    setLit(id);
    window.clearTimeout(litTimer.current);
    litTimer.current = window.setTimeout(() => setLit(null), 1500);
    void Promise.resolve(run()).then(() => { if (done) showMessage(done); });
  };
  const scale = useStore((s) => s.ui.scale);
  const root = useStore((s) => s.ui.root);
  const padLevels = useStore((s) => s.ui.padLevels);
  const folders = useStore((s) => s.folders);
  const [projectName, setProjectName] = useState("");
  const [confirmReplace, setConfirmReplace] = useState(false);
  const [listKey, setListKey] = useState(0);
  const [asTemplate, setAsTemplate] = useState(false); // v0.46 : la demande de remplacement concerne un modèle
  const saveNamed = async (overwrite: boolean, tpl = asTemplate, given?: string) => {
    const name = cleanName(given ?? projectName);
    if (!name) return;
    setAsTemplate(tpl);
    const r = await actions.saveProjectNamed(name, overwrite, tpl);
    setConfirmReplace(r === "exists");
    if (r === "ok") { setListKey((k) => k + 1); showMessage(`${tpl ? "Modèle" : "Projet"} « ${name} » enregistré : il est dans la liste ci-dessous`); }
    else if (r !== "exists") showMessage("Enregistrement impossible");
  };
  // v0.46.2 : tablette et téléphone : Enregistrer / Ouvrir restent DANS l'application (dossier des
  // projets et sa liste). Le sélecteur du système (qui sortait de l'application) ne sert plus qu'à
  // exporter une copie ou importer un fichier venu d'ailleurs.
  const inApp = !!folders && !folders.canChoose;
  const listRef = useRef<HTMLDivElement>(null);
  const quickSave = (tpl: boolean) => {
    const d = new Date(), z = (n: number) => String(n).padStart(2, "0");
    const name = cleanName(projectName) || `Projet ${d.getFullYear()}-${z(d.getMonth() + 1)}-${z(d.getDate())} ${z(d.getHours())}h${z(d.getMinutes())}`;
    setProjectName(name);
    void saveNamed(false, tpl, name);
  };

  const quantSteps = [0, 25, 50, 75, 100];
  const encoders = [
    steppedBinding("Quantif. enr.", quantSteps, quantSteps.includes(recQuant) ? recQuant : 100, (q) => void actions.seq("seqRecQuant", q), (q) => `${q} %`),
    steppedBinding("Lancement", [0, 1, 2, 3, 4], launch?.quant ?? 2, (q) => void actions.setLaunchQuant(q), (q) => LAUNCH_QUANTS[q]),
    steppedBinding("Gamme des pads", SCALES.map((_, i) => i), scale, (i) => setUi({ scale: i }), (i) => SCALES[i].name),
    steppedBinding("Tonique", ROOTS.map((_, i) => i), root, (i) => setUi({ root: i }), (i) => ROOTS[i]),
  ];

  const mapped = (target: number) => midi.maps.filter((m) => m[3] === target);

  return (
    <ScreenFrame title="Réglages" encoders={encoders}>
      <div className="settings">
        {/* 1.0 (7 nov.) : bloc Projet rangé par lignes étiquetées (Nouveau, Enregistrer, liste, Fichiers, Langue) */}
        <section className="settings-block settings-project">
          <h3>Projet</h3>
          <div className="proj-line">
            <span className="proj-label">Nouveau</span>
            <span className="proj-btns">
              {/* v0.46 fix27 : le bouton touché s'allume en jaune un instant (on voit qu'il a pris) */}
              <button type="button" className={`touch${lit === "demo" ? " touch--on" : ""}`}
                onClick={() => press("demo", () => actions.newProject(false), "Démo Techno chargée")}>Démo Techno</button>
              <button type="button" className={`touch${lit === "tribe" ? " touch--on" : ""}`}
                onClick={() => press("tribe", () => actions.newProject(false, "tribe"), "Démo Tribe chargée")}>Démo Tribe</button>
              <button type="button" className={`touch${lit === "empty" ? " touch--on" : ""}`}
                onClick={() => press("empty", () => actions.newProject(true), "Nouveau projet vide")}>Nouveau projet vide</button>
            </span>
          </div>
          {folders && (
            <div className="proj-line">
              <span className="proj-label">Enregistrer</span>
              <div className="project-save">
                <input type="text" value={projectName} maxLength={60} placeholder={inApp ? "Nom (sinon : la date)" : "Nom du projet"} aria-label="Nom du projet à enregistrer dans le dossier des projets"
                  onChange={(e) => { setProjectName(e.target.value); setConfirmReplace(false); }}
                  onKeyDown={(e) => { if (e.key === "Enter") void saveNamed(false); }} />
                {confirmReplace
                  ? <><span className="settings-note">« {cleanName(projectName)} » existe déjà.</span>
                      <button type="button" className="touch touch--small touch--on" onClick={() => void saveNamed(true)}>Remplacer</button>
                      <button type="button" className="touch touch--small touch--quiet" onClick={() => setConfirmReplace(false)}>Annuler</button></>
                  : inApp ? <>
                      <button type="button" className="touch touch--on" onClick={() => quickSave(false)}
                        title="Enregistre le projet dans l'application, sous le nom écrit ici (ou la date)">Enregistrer</button>
                      <button type="button" className="touch" onClick={() => quickSave(true)}
                        title="Modèle : tous les réglages restent (sons, échantillons, effets, mixeur, scènes, macros, MIDI) ; patterns, notes, verrous de pas et morceau sont retirés">Comme modèle</button>
                    </> : <>
                      <button type="button" className="touch touch--small touch--on" disabled={!cleanName(projectName)} onClick={() => void saveNamed(false, false)}
                        title="Enregistrer dans le dossier des projets, sous ce nom">Dans le dossier</button>
                      <button type="button" className="touch touch--small" disabled={!cleanName(projectName)} onClick={() => void saveNamed(false, true)}
                        title="Modèle : mêmes réglages et sons, sans patterns, verrous ni morceau">Comme modèle</button>
                    </>}
              </div>
            </div>
          )}
          {folders && <div ref={listRef} className="proj-list"><FolderBrowser key={listKey} kind="projects" title={inApp ? "Projets enregistrés (toucher pour ouvrir)" : "Projets du dossier"} compact onPick={(rel) => actions.openProjectFile(rel)} /></div>}
          <div className="proj-line">
            <span className="proj-label">Fichiers</span>
            <span className="proj-btns">
              {inApp ? <>
                {!folders && <>
                  <button type="button" className="touch touch--on" onClick={() => quickSave(false)}>Enregistrer</button>
                  <button type="button" className="touch" onClick={() => quickSave(true)}>Enregistrer comme modèle</button>
                </>}
                <button type="button" className="touch touch--quiet" disabled={!files} onClick={() => press("open", () => actions.openProject())}
                  title="Ouvrir un projet venu d'ailleurs (Téléchargements, Drive…) avec le sélecteur du système">Importer un fichier…</button>
                <button type="button" className="touch touch--quiet" disabled={!files} onClick={() => actions.saveProject()}
                  title="Enregistrer une copie hors de l'application (Téléchargements, Drive…) avec le sélecteur du système">Exporter une copie…</button>
              </> : <>
                <button type="button" className={`touch${lit === "open" ? " touch--on" : ""}`} disabled={!files}
                  onClick={() => press("open", () => actions.openProject())}>Ouvrir…</button>
                <button type="button" className="touch touch--on" disabled={!files} onClick={() => actions.saveProject()}>Enregistrer sous…</button>
                <button type="button" className="touch" disabled={!files} onClick={() => actions.saveProject(true)}
                  title="Modèle : tous les réglages restent (sons, échantillons, effets, mixeur, scènes, macros, MIDI) ; patterns, notes, verrous de pas et morceau sont retirés">Comme modèle…</button>
              </>}
            </span>
          </div>
          <div className="proj-line">
            <span className="proj-label">Langue</span>
            <span className="proj-btns"><LangChoice />{hasNative("appStart") && <WelcomeToggle />}</span>
          </div>
          <p className="settings-note">Un projet (.gbx) contient tout : pistes, réglages, patterns, scènes, échantillons. Dans un logiciel hôte, le projet est aussi enregistré avec la session. Toucher un projet de la liste l'ouvre.</p>
        </section>

        {folders && (
          <section className="settings-block">
            <h3>Dossiers</h3>
            {FOLDERS.map(({ kind, label, hint }) => (
              <div key={kind} className="folder-row">
                <span className="folder-name">{label}</span>
                <span className="folder-path" title={`${folders[kind].path} — ${hint}`}><bdi>{folders[kind].path}</bdi></span>
                <span className="folder-actions">
                  {folders.canChoose && <button type="button" className="touch touch--small" onClick={() => actions.chooseFolder(kind)}>Choisir…</button>}
                  {hasNative("revealFolder") && <button type="button" className="touch touch--small touch--quiet" onClick={() => actions.revealFolder(kind)}>Ouvrir</button>}
                  {kind === "samples" && !folders.canChoose && hasNative("importSamples") && (
                    <>
                      <button type="button" className="touch touch--small" onClick={() => actions.importSamples(false)}
                        title="Copier des échantillons du téléphone dans l'application (une fois) : ils sont ensuite dans la Bibliothèque, dossier Importés">Importer des fichiers…</button>
                      <button type="button" className="touch touch--small" onClick={() => actions.importSamples(true)}
                        title="Votre dossier de sons sur le téléphone : tous ses sons (sous-dossiers compris) sont copiés dans la Bibliothèque › Perso. Le choisir de nouveau ajoute les nouveaux sons.">Dossier perso…</button>
                    </>)}
                  {folders[kind].custom && <button type="button" className="touch touch--small touch--quiet" onClick={() => void actions.resetFolder(kind)}>Par défaut</button>}
                </span>
              </div>
            ))}
            <p className="settings-note">{folders.canChoose
              ? "Les dossiers servent dans tous les projets. Préréglages : un sous-dossier par synthé ou par effet (« Mono semi-modulaire », « Égaliseur »…), puis un par banque."
              : "Sur tablette, les sons sont gardés dans l'application. Un son pris par « Échantillon… » sur une piste est chargé et copié dans Bibliothèque › Importés ; « Dossier perso… » copie tous les sons d'un dossier du téléphone dans Bibliothèque › Perso ; vos prises sont dans « Mes enregistrements »."}</p>
          </section>
        )}

        <section className="settings-block">
          <h3>Enregistrement et lancement</h3>
          <div className="settings-row">
            <span className="settings-label">Quantification de l'enregistrement</span>
            {quantSteps.map((q) => (
              <button key={q} type="button" className={`touch touch--small${recQuant === q ? " touch--on" : ""}`} onClick={() => void actions.seq("seqRecQuant", q)}>{q} %</button>
            ))}
          </div>
          <div className="settings-row">
            <span className="settings-label">Coupure et solo des pistes</span>
            <button type="button" className={`touch touch--small${!launch?.muteTails ? " touch--on" : ""}`} onClick={() => void actions.setMuteTails(false)}>Coupe le son tout de suite</button>
            <button type="button" className={`touch touch--small${launch?.muteTails ? " touch--on" : ""}`} onClick={() => void actions.setMuteTails(true)}>Laisse finir les sons et les traînes</button>
          </div>
          <div className="settings-row">
            <span className="settings-label">Grille de lancement</span>
            <button type="button" className={`touch touch--small${ui.launchAll ? " touch--on" : ""}`} onClick={() => { setUi({ launchAll: true }); writePref("launchAll", true); }}>16 pistes, on fait défiler</button>
            <button type="button" className={`touch touch--small${!ui.launchAll ? " touch--on" : ""}`} onClick={() => { setUi({ launchAll: false }); writePref("launchAll", false); }}>8 pistes par écran (1 – 8 / 9 – 16)</button>
          </div>
          <div className="settings-row">
            <span className="settings-label">Lancement des patterns</span>
            {LAUNCH_QUANTS.map((name, q) => (
              <button key={q} type="button" className={`touch touch--small${launch?.quant === q ? " touch--on" : ""}`} onClick={() => void actions.setLaunchQuant(q)}>{name}</button>
            ))}
          </div>
        </section>

        <section className="settings-block">
          <h3>Affichage</h3>
          <div className="settings-row">
            <span className="settings-label">Courbes de l'écran SON et des modulations</span>
            <button type="button" className={`touch touch--small${ui.curveAnim ? " touch--on" : ""}`} onClick={() => { setUi({ curveAnim: true, curveAutoOff: null }); writePref("curveAnim", true); }}
              title="La courbe suit le son joué (LFO, enveloppes, modulations) et un point court sur l'enveloppe à chaque note">Animées</button>
            <button type="button" className={`touch touch--small${!ui.curveAnim ? " touch--on" : ""}`} onClick={() => { setUi({ curveAnim: false, curveAutoOff: null }); writePref("curveAnim", false); }}
              title="Les courbes montrent seulement le réglage : moins de dessins, plus de processeur pour le son (utile sur les petits téléphones)">Fixes (économise le processeur)</button>
          </div>
          <p className="settings-note">{ui.curveAutoOff !== null
            ? `Coupées automatiquement : le calcul audio a atteint ${ui.curveAutoOff} %. Elles restent fixes jusqu'à ce que vous choisissiez « Animées ».`
            : `Au-delà de ${CURVE_CPU_LIMIT} % de calcul audio, l'animation se coupe d'elle-même avec un message, et reste coupée jusqu'à ce que vous la remettiez ici.`}</p>
        </section>

        <section className="settings-block">
          <h3>Pads</h3>
          <div className="settings-row">
            <span className="settings-label">En mode Jouer, les 16 pads jouent</span>
            <button type="button" className={`touch touch--small${!padLevels ? " touch--on" : ""}`} onClick={() => setUi({ padLevels: false })}>16 sons / 16 notes de la gamme</button>
            <button type="button" className={`touch touch--small${padLevels ? " touch--on" : ""}`} onClick={() => setUi({ padLevels: true })}>16 niveaux (vélocités du son choisi)</button>
          </div>
          <div className="settings-row settings-row--wrap">
            <span className="settings-label">Gamme</span>
            {/* v0.44.1 : 40 gammes et modes, rangés par famille */}
            <select className="settings-select" aria-label="Gamme" value={scale} onChange={(e) => setUi({ scale: Number(e.target.value) })}>
              {SCALE_GROUPS.map((gname) => (
                <optgroup key={gname} label={gname}>
                  {SCALES.map((sc, i) => (sc.group === gname ? <option key={sc.name} value={i}>{sc.name}</option> : null))}
                </optgroup>
              ))}
            </select>
            <button type="button" className={`touch touch--small${scale === 0 ? " touch--on" : ""}`} onClick={() => setUi({ scale: 0 })}>Chromatique</button>
          </div>
          <div className="settings-row settings-row--wrap">
            <span className="settings-label">Tonique</span>
            {ROOTS.map((r, i) => (
              <button key={r} type="button" className={`touch touch--small${root === i ? " touch--on" : ""}`} onClick={() => setUi({ root: i })}>{r}</button>
            ))}
          </div>
        </section>

        <section className="settings-block">
          <h3>Contrôleur MIDI</h3>
          <BluetoothMidi />
          <WifiRemoteSetting />
          {/* 1.1 : le téléphone comme télécommande ; « Retour à GOLEM » revient ici sans redémarrer */}
          <div className="settings-row">
            <button type="button" className="touch touch--small" onClick={() => setUi({ remote: true })}
              title="Le téléphone pilote GOLEM sur le PC (câble USB) ou un appareil MIDI branché (FM-1) : mixage, boutons, pavé Chaos.">
              Mode télécommande…
            </button>
            <small>Le téléphone pilote GOLEM sur le PC ou un FM-1. Le son du téléphone est mis en pause.</small>
          </div>
          {/* v0.46 : canaux, comme dans Ableton ou Fruity : un canal pour jouer, un pour les commandes */}
          <div className="midi-route">
            <label>Notes (piste choisie)
              <select className="chip chip--select" value={route.note} onChange={(e) => void actions.setMidiRoute(Number(e.target.value), route.ctrl, route.ctrlOn)}>
                {CHANNELS.map((c) => <option key={c} value={c}>{c === 0 ? "Tous les canaux" : `Canal ${c}`}</option>)}
              </select>
            </label>
            <label>Commandes apprises
              <select className="chip chip--select" value={route.ctrl} onChange={(e) => void actions.setMidiRoute(route.note, Number(e.target.value), route.ctrlOn)}>
                {CHANNELS.map((c) => <option key={c} value={c}>{c === 0 ? "Tous les canaux" : `Canal ${c}`}</option>)}
              </select>
            </label>
            <button type="button" className={`touch touch--small${route.ctrlOn ? " touch--on" : ""}`} aria-pressed={route.ctrlOn}
              title={route.ctrlOn ? "Les commandes apprises agissent (toucher pour les couper : le contrôleur ne fait plus que jouer)" : "Commandes apprises coupées (toucher pour les réactiver)"}
              onClick={() => void actions.setMidiRoute(route.note, route.ctrl, !route.ctrlOn)}>{route.ctrlOn ? "Commandes actives" : "Commandes coupées"}</button>
          </div>
          <p className="settings-note">Une piste peut aussi avoir son propre canal (écran PISTES, « Canal MIDI ») : elle joue alors ce canal même quand elle n'est pas choisie, et la piste choisie ne reçoit plus les notes de ce canal.</p>
          {midi.learn !== -1 && (
            <div className="learn-banner" role="status">
              <span>En attente : touchez la commande du contrôleur pour <b>{targetName(midi.learn)}</b>.</span>
              <button type="button" className="touch touch--small" onClick={() => void actions.midiLearn(-1)}>Annuler</button>
            </div>
          )}
          <div className="midi-specials">
            {SPECIALS.map((sp) => {
              const m = mapped(sp.target);
              return (
                <div key={sp.target} className="midi-special">
                  <span className="midi-name">{sp.label}</span>
                  <span className="midi-src tabular">{m.length ? midiSource(m[0][0], m[0][1], m[0][2]) : sp.hint}</span>
                  <button type="button" className={`touch touch--small${midi.learn === sp.target ? " touch--on" : ""}`} onClick={() => void actions.midiLearn(sp.target)}>Apprendre</button>
                  {m.length > 0 && <button type="button" className="touch touch--small touch--quiet" onClick={() => void actions.midiForget(sp.target)}>Oublier</button>}
                </div>
              );
            })}
          </div>
          <ul className="midi-list">
            {midi.maps.map((m) => (
              <li key={`${m[0]}:${m[1]}:${m[2]}`}>
                <span className="tabular">{midiSource(m[0], m[1], m[2])}</span>
                <span>{targetName(m[3])}</span>
                <button type="button" className="x" onClick={() => void actions.midiForget(m[3])} aria-label={`Oublier ${targetName(m[3])}`}>×</button>
              </li>
            ))}
            {midi.maps.length === 0 && <li className="more">Aucune commande apprise. Clic droit (appui long) sur un encodeur, un bouton M / S, puis « Apprendre MIDI » ; ou « Apprendre » ci-dessus.</li>}
          </ul>
          <p className="settings-note">Les notes 36 à 51 jouent les pads de la piste affichée ; Program Change 0 à 31 lance les scènes 1 à 32.</p>
        </section>

        <section className="settings-block">
          <h3>Clavier de l'ordinateur</h3>
          <p className="settings-note">W X C V · Q S D F · A Z E R · 1 2 3 4 : pads 1 à 16 (Maj : accent). Espace : lecture / arrêt. B maintenu : Break. N maintenu : Fill.</p>
        </section>

        <section className="settings-block">
          <h3>Infos écran</h3>
          <ScreenInfo />
        </section>
      </div>
    </ScreenFrame>
  );
}

/** v0.46 fix6 : l'accueil (choix du projet, mascotte) au lancement de l'application autonome. */
/** 1.1 : télécommande Wi-Fi (côté PC) : accepter un téléphone GOLEM du réseau local ; adresse à taper sur
    le téléphone si la recherche ne trouve pas le PC. Coupé par défaut. */
function WifiRemoteSetting() {
  const [info, setInfo] = useState<{ on: boolean; port: number; addresses: string[] } | null>(null);
  useEffect(() => { void actions.wifiListen().then(setInfo); }, []);
  if (!info) return null;
  const toggle = async (on: boolean) => {
    const r = await actions.wifiListen(on);
    setInfo(r);
    writePref("wifiRemote", on);
    if (on && r && !r.on) showMessage("Le port Wi-Fi de la télécommande est déjà pris par une autre application");
  };
  return (
    <div className="settings-row">
      <label className="settings-check">
        <input type="checkbox" checked={info.on} onChange={(e) => void toggle(e.target.checked)} />
        Accepter une télécommande Wi-Fi
      </label>
      <small>{info.on ? `Adresse à taper sur le téléphone si besoin : ${info.addresses.join(" ou ") || "réseau introuvable"}` : "Un téléphone GOLEM du même réseau pilote ce GOLEM (mixage, boutons, pavé)."}</small>
    </div>
  );
}

/** v0.46 fix22 : appairer un clavier ou un contrôleur MIDI Bluetooth (téléphone, tablette). Le son
    peut alors sortir par la prise USB (carte son) pendant que le contrôleur joue sans fil. Sur
    ordinateur, l'appairage se fait dans les réglages Bluetooth du système. */
function BluetoothMidi() {
  const [ok, setOk] = useState<boolean | null>(null);
  useEffect(() => { void actions.bluetoothMidi(false).then(setOk); }, []);
  if (!ok) return null;
  return (
    <div className="midi-bt">
      <button type="button" className="touch touch--small" onClick={() => void actions.bluetoothMidi(true)}
        title="Cherche les claviers et contrôleurs MIDI Bluetooth allumés à proximité ; touchez-en un pour le connecter. Il joue ensuite comme un clavier USB.">
        Appairer un contrôleur Bluetooth…
      </button>
      <small>Clavier ou contrôleur MIDI sans fil (Bluetooth MIDI). Le son peut sortir par la prise USB.</small>
    </div>
  );
}

/** v0.46 fix30 : langue de l'interface (français / anglais), gardée sur cet appareil ; l'interface se recharge
    (le projet en cours reste dans le moteur). Les noms des langues ne se traduisent pas. */
function LangChoice() {
  const cur = document.documentElement.lang === "en" ? "en" : "fr";
  const pick = (l: "fr" | "en") => { void switchLang(l); }; // gardée par l'application, puis rechargement
  return (
    <span className="lang-choice" role="radiogroup" aria-label="Langue / Language" translate="no">
      <button type="button" role="radio" aria-checked={cur === "fr"} className={`touch touch--small${cur === "fr" ? " touch--on" : ""}`} onClick={() => pick("fr")}>Français</button>
      <button type="button" role="radio" aria-checked={cur === "en"} className={`touch touch--small${cur === "en" ? " touch--on" : ""}`} onClick={() => pick("en")}>English</button>
    </span>
  );
}

/** 1.1.1 : tailles d'écran annoncées par l'appareil (fenêtre, zone visible, page, écran, densité, zone
    retenue, échelle), à copier en texte pour signaler un écran mal ajusté. */
function ScreenInfo() {
  const read = () => {
    const i = (window as unknown as { __golemScreen?: Record<string, number | string> }).__golemScreen ?? {};
    return ["win", "vv", "doc", "scr", "dpr", "zone", "scale", "mode"].map((k) => `${k} ${i[k] ?? "-"}`).join(" · ")
      + ` · ${/Android [\d.]+/.exec(navigator.userAgent)?.[0] ?? navigator.platform}`;
  };
  const [txt, setTxt] = useState(read);
  const [done, setDone] = useState(false);
  useEffect(() => { const t = window.setInterval(() => setTxt(read()), 1000); return () => window.clearInterval(t); }, []);
  const copy = async () => {
    try { await navigator.clipboard.writeText(txt); setDone(true); window.setTimeout(() => setDone(false), 1500); }
    catch { showMessage("Copie impossible : sélectionnez le texte et copiez-le à la main"); }
  };
  return (
    <div className="settings-row">
      <input className="screen-info" readOnly value={txt} translate="no" onFocus={(e) => e.currentTarget.select()} aria-label="Infos écran" />
      <button type="button" className="touch touch--small" onClick={() => void copy()}>{done ? "Copié" : "Copier"}</button>
      <small>À copier dans un signalement si un écran est mal ajusté (bande vide, bas coupé).</small>
    </div>
  );
}

function WelcomeToggle() {
  const [on, setOn] = useState(() => readPref("welcome", true));
  return (
    <button type="button" className={`touch touch--small${on ? " touch--on" : ""}`} onClick={() => { setOn(!on); writePref("welcome", !on); }}
      title="Au lancement de l'application : la mascotte, puis le choix du projet (reprendre, nouveau, démonstration, dossier)">Accueil au démarrage</button>
  );
}
