// Groovebox — OFAD — AGPL-3.0-or-later
// Écran SCÈNES (principe Octatrack) : les scènes attribuées à A et à B et leur contenu, la
// position du crossfader, les 32 scènes (2 banques) et la scène Break (appliquée tant que la
// touche Break est tenue). Toucher une scène la lance (Maj : de l'autre côté du fader).
// « Éditer » : les encodeurs de tous les écrans écrivent alors dans la scène.
// v0.46 : « Copier vers… » : puis toucher la scène de destination (ses réglages sont remplacés).
// v0.46 : la valeur de chaque réglage d'une scène se règle ici, en glissant (haut / droite = plus) ;
// on l'entend tout de suite si la scène joue (crossfader de son côté).

import { useEffect, useState } from "react";
import { actions, resolveGlobal, setUi, showMessage, useStore } from "../engine/store";
import { describeGlobal } from "../engine/describe";
import { BREAK_SCENE, MIDI_XFADE, hostCode } from "../engine/protocol";
import { formatNorm, toNorm, toPlain } from "../engine/params";
import { useRef } from "react";
import { useValueDrag } from "../controls/useValueDrag";
import { ParamMenuZone, type EncoderBinding } from "../controls/Encoder";
import { rangeEntry, steppedBinding, valueBinding, withCtl } from "../controls/bindings";
import { RowLaunchBar } from "./RowLaunchBar";
import { ScreenFrame } from "./ScreenFrame";
import { VEC_ORBITS, VEC_RATES, VectorPad } from "./VectorPad";

const describe = describeGlobal;
/** Position du crossfader affichée de -100 (tout A) à +100 (tout B). */
const xfText = (x: number) => { const v = Math.round(x * 200 - 100); return v <= -100 ? "A −100" : v >= 100 ? "B +100" : v === 0 ? "0" : v > 0 ? `+${v}` : `−${-v}`; };

/** v0.46 : valeur d'un réglage de la scène, à régler en glissant (molette aussi ; double tape : retirer). */
function LockValue({ scene, g, v }: { scene: number; g: number; v: number }) {
  const r = resolveGlobal(g);
  const pending = useRef<number | null>(null);
  const frame = useRef(0);
  const send = (n: number) => {
    // un envoi par image au plus pendant le glissé
    pending.current = n;
    if (frame.current) return;
    frame.current = requestAnimationFrame(() => {
      frame.current = 0;
      const x = pending.current; pending.current = null;
      if (x !== null) void actions.scene("sceneSetLock", scene, g, x);
    });
  };
  const d = r?.desc;
  const stepped = !!d && (!!d.choices?.length || !!d.integer);
  const binding: EncoderBinding | null = d ? {
    kind: "continuous", label: describe(g), text: formatNorm(d, v), normalised: v,
    // réglage à crans : la valeur glisse d'un cran à l'autre
    onChange: (n) => send(stepped ? toNorm(d, Math.round(toPlain(d, n))) : n),
    reset: () => void actions.scene("sceneClearLock", scene, g),
    // v0.46.2 : appui long (clic droit sur ordinateur) : saisie de la valeur exacte
    entry: {
      plain: toPlain(d, v), min: d.min, max: d.max, unit: d.unit, integer: !!d.integer, choices: d.choices,
      set: (plain) => { if (Number.isFinite(plain)) void actions.scene("sceneSetLock", scene, g, toNorm(d, Math.min(d.max, Math.max(d.min, plain)))); },
      pick: (i) => void actions.scene("sceneSetLock", scene, g, toNorm(d, i)),
    },
  } : null;
  const { handlers } = useValueDrag(binding);
  return (
    <ParamMenuZone binding={binding} className="pm-zone--lock">
      <b className="tabular slot-lock-value drag-value" role="slider" aria-label={`${describe(g)} dans la scène`} aria-valuetext={binding?.text}
        title="Glisser pour régler la valeur de la scène (vers le haut ou la droite = plus) — appui long ou clic droit : valeur exacte ; double toucher : retirer" {...handlers}>{binding?.text ?? ""}</b>
    </ParamMenuZone>
  );
}

/** v0.46 fix29 : live = carte de la vue live, pour jouer : le nom et les réglages de la scène, sans Éditer,
    ni Vider / Copier / Muette (on édite les scènes avant, dans l'écran SCÈNES). */
export function SlotCard({ slot, scene, copying, onCopy, live = false }: { slot: "A" | "B" | "Break"; scene: number; copying: boolean; onCopy: () => void; live?: boolean }) {
  const data = useStore((s) => s.scenes[scene]);
  const editing = useStore((s) => (slot === "Break" ? s.ui.editBreak : s.ui.editSlot === slot));
  const locks = data?.locks ?? [];
  const toggle = () => setUi(slot === "Break" ? { editBreak: !editing, editSlot: null } : { editSlot: editing ? null : slot, editBreak: false });
  return (
    <section className={`slot slot--${slot === "Break" ? "B" : slot}${editing ? " slot--editing" : ""}${live ? " slot--live" : ""}`}>
      <header>
        <span className="slot-letter">{slot === "Break" ? "BRK" : slot}</span>
        <span className="slot-scene">{slot === "Break" ? "Scène Break" : `Scène ${scene + 1}`}{data?.muted ? " · muette" : ""}</span>
        {live && <span className="slot-count tabular">{locks.length ? `${locks.length} réglage${locks.length > 1 ? "s" : ""}` : "vide"}</span>}
        {!live && <button type="button" className={`touch${editing ? " touch--on" : ""}`} onClick={toggle} aria-pressed={editing}>{editing ? "Terminer" : "Éditer"}</button>}
      </header>
      {locks.length === 0
        ? (live ? null : <p className="slot-empty">{slot === "Break" ? "Aucun réglage. « Éditer », puis tournez les encodeurs (SON, EFFETS, MIXEUR…)." : "Aucun réglage. Touchez « Éditer », puis tournez les encodeurs d'un écran (SON, EFFETS, MIXEUR…)."}</p>)
        : (
          <ul className="slot-locks" aria-label={`${locks.length} réglage${locks.length > 1 ? "s" : ""} dans la scène`} tabIndex={0}>
            {locks.map(([g, v]) => {
              const r = resolveGlobal(g);
              return (
                <li key={g}>
                  <span title={describe(g)}>{describe(g)}</span>
                  {r ? <LockValue scene={scene} g={g} v={v} /> : <b />}
                  {!live && <button type="button" className="x" onClick={() => void actions.scene("sceneClearLock", scene, g)} aria-label={`Retirer ${describe(g)}`}>×</button>}
                </li>
              );
            })}
          </ul>
        )}
      {!live && <div className="slot-actions">
        {locks.length > 0 && <span className="slot-count tabular">{locks.length} réglage{locks.length > 1 ? "s" : ""}</span>}
        {locks.length > 0 && <button type="button" className="touch touch--quiet" onClick={() => void actions.scene("sceneClear", scene)}>Vider</button>}
        {locks.length > 0 && <button type="button" className={`touch${copying ? " touch--on" : " touch--quiet"}`} aria-pressed={copying} onClick={onCopy}
          title="Copier cette scène : touchez ensuite la scène de destination dans la grille">{copying ? "Annuler" : "Copier…"}</button>}
        <button type="button" className="touch touch--quiet" title={data?.muted ? "Réactiver la scène" : "Rendre la scène muette (elle ne joue plus, ses réglages restent)"}
          onClick={() => void actions.scene("sceneMute", scene, !data?.muted)}>{data?.muted ? "Réactiver" : "Muette"}</button>
      </div>}
    </section>
  );
}

export function ScenesScreen() {
  const g = useStore((s) => s.tick.g);
  const scenes = useStore((s) => s.scenes);
  const bank = useStore((s) => s.ui.sceneBank);
  const sceneA = Math.round(g.sceneA) - 1, sceneB = Math.round(g.sceneB) - 1;
  const first = bank * 16;
  const list = Array.from({ length: 32 }, (_, i) => i);
  const [copyFrom, setCopyFrom] = useState<number | null>(null); // v0.46 : scène à copier
  const toggleCopy = (n: number) => setCopyFrom(copyFrom === n ? null : n);
  const name = (n: number) => (n === BREAK_SCENE ? "Break" : `${n + 1}`);
  useEffect(() => {
    if (copyFrom === null) return;
    const esc = (e: KeyboardEvent) => { if (e.key === "Escape") { e.preventDefault(); setCopyFrom(null); } };
    window.addEventListener("keydown", esc);
    return () => window.removeEventListener("keydown", esc);
  }, [copyFrom]);
  const copyTo = (n: number) => {
    if (copyFrom === null || n === copyFrom) { setCopyFrom(null); return; }
    void actions.scene("sceneCopy", copyFrom, n);
    showMessage(`Scène ${name(copyFrom)} copiée dans la scène ${name(n)}`);
    setCopyFrom(null);
  };

  // v0.46 fix22 : onglet « Vecteur » (pavé à quatre coins), mémorisé
  const [tab, setTabState] = useState<"xf" | "vec">(() => { try { return localStorage.getItem("grv.scenesTab") === "vec" ? "vec" : "xf"; } catch { return "xf"; } });
  const setTab = (t: string) => { const v = t === "vec" ? "vec" : "xf"; setTabState(v); try { localStorage.setItem("grv.scenesTab", v); } catch { /* */ } };
  const vecOn = (g.vecOn ?? 0) >= 0.5;
  const vecEncoders = [
    withCtl(valueBinding("X", g.vecX ?? 0.5, `${Math.round((g.vecX ?? 0.5) * 100)} %`, (v) => { if (!vecOn) actions.setGlobal("vecOn", 1); actions.setGlobal("vecX", v); }),
      hostCode("vecX"), rangeEntry(g.vecX ?? 0.5, 0, 100, "%", (v) => { if (!vecOn) actions.setGlobal("vecOn", 1); actions.setGlobal("vecX", v); })),
    withCtl(valueBinding("Y", g.vecY ?? 0.5, `${Math.round((g.vecY ?? 0.5) * 100)} %`, (v) => { if (!vecOn) actions.setGlobal("vecOn", 1); actions.setGlobal("vecY", v); }),
      hostCode("vecY"), rangeEntry(g.vecY ?? 0.5, 0, 100, "%", (v) => { if (!vecOn) actions.setGlobal("vecOn", 1); actions.setGlobal("vecY", v); })),
    withCtl(steppedBinding("Mouvement", [0, 1, 2, 3, 4, 5, 6, 7, 8], Math.round(g.vecOrbit ?? 0), (n) => actions.setGlobal("vecOrbit", n), (n) => VEC_ORBITS[n]), hostCode("vecOrbit")),
    withCtl(steppedBinding("Un tour en", [0, 1, 2, 3, 4, 5, 6, 7, 8], Math.round(g.vecRate ?? 2), (n) => actions.setGlobal("vecRate", n), (n) => VEC_RATES[n]), hostCode("vecRate")),
    withCtl(valueBinding("Taille", g.vecDepth ?? 0.35, `${Math.round((g.vecDepth ?? 0.35) * 100)} %`, (v) => actions.setGlobal("vecDepth", v)),
      hostCode("vecDepth"), rangeEntry(g.vecDepth ?? 0.35, 0, 100, "%", (v) => actions.setGlobal("vecDepth", v))),
    // v0.46 fix28 : hauteur (aplatir / étirer) et rotation de la forme du mouvement
    withCtl(valueBinding("Hauteur", ((g.vecHeight ?? 1) - 0.05) / 1.95, `${Math.round((g.vecHeight ?? 1) * 100)} %`, (v) => actions.setGlobal("vecHeight", 0.05 + v * 1.95)),
      hostCode("vecHeight"), rangeEntry(((g.vecHeight ?? 1) - 0.05) / 1.95, 5, 200, "%", (v) => actions.setGlobal("vecHeight", 0.05 + v * 1.95))),
    withCtl(valueBinding("Rotation", (g.vecRot ?? 0) / 360, `${Math.round(g.vecRot ?? 0)}°`, (v) => actions.setGlobal("vecRot", Math.round(v * 360) % 360)),
      hostCode("vecRot"), rangeEntry((g.vecRot ?? 0) / 360, 0, 360, "°", (v) => actions.setGlobal("vecRot", Math.round(v * 360) % 360))),
    withCtl(steppedBinding("Pavé", [0, 1], vecOn ? 1 : 0, (n) => actions.setGlobal("vecOn", n), (n) => (n ? "actif" : "arrêté")), hostCode("vecOn")),
  ];

  const encoders = [
    withCtl(steppedBinding("Scène A", list, sceneA, (n) => actions.setGlobal("sceneA", n + 1), (n) => `${n + 1}`), hostCode("sceneA")),
    withCtl(steppedBinding("Scène B", list, sceneB, (n) => actions.setGlobal("sceneB", n + 1), (n) => `${n + 1}`), hostCode("sceneB")),
    withCtl(valueBinding("Crossfader", g.xfade, xfText(g.xfade), (v) => actions.setGlobal("xfade", v), true),
      MIDI_XFADE, rangeEntry(g.xfade, 0, 100, "% vers B", (v) => actions.setGlobal("xfade", v))),
    steppedBinding("Banque", [0, 1], bank, (b) => setUi({ sceneBank: b }), (b) => (b === 0 ? "1 – 16" : "17 – 32")),
    steppedBinding("Copier A vers", list, sceneB, (n) => { void actions.scene("sceneCopy", sceneA, n); }, (n) => `scène ${n + 1}`),
  ];

  return (
    <ScreenFrame title="Scènes" tabs={[{ id: "xf", label: "Crossfader" }, { id: "vec", label: "Vecteur" }]} tab={tab} onTab={setTab}
      encoders={tab === "vec" ? vecEncoders : encoders}
      status={<><button type="button" className="chip chip--live" onClick={() => setUi({ vecFull: true })}
          title="Vue live : crossfader ou pavé, réglages des scènes, console des 16 pistes et les 16 boutons, sur un seul écran">⛶ Vue live</button>
        <span className="chip tabular">{tab === "vec" ? (vecOn ? "Pavé actif" : "Pavé arrêté") : `Crossfader ${xfText(g.xfade)}`}</span></>}>
      {tab === "vec" ? <VectorPad /> : (
      <div className="with-rowlaunch">
      <RowLaunchBar />
      <div className="scenes">
        <div className="scenes-slots">
          <SlotCard slot="A" scene={sceneA} copying={copyFrom === sceneA} onCopy={() => toggleCopy(sceneA)} />
          <div className="xf-meter" aria-hidden>
            <span className="xf-track" />
            <span className="xf-centre" />
            <span className="xf-thumb" style={{ left: `${g.xfade * 100}%` }} />
          </div>
          <SlotCard slot="B" scene={sceneB} copying={copyFrom === sceneB} onCopy={() => toggleCopy(sceneB)} />
        </div>

        <div className="scenes-bottom">
          <div className={`scenes-grid${copyFrom !== null ? " is-copying" : ""}`}>
            {copyFrom !== null && <p className="scenes-copy-hint">Copier la scène {name(copyFrom)} : touchez la scène de destination (Échap : annuler)</p>}
            {scenes.slice(first, first + 16).map((sc, i) => {
              const n = first + i;
              const count = sc.locks.length;
              return (
                <button key={n} type="button"
                  className={`scene-cell${n === sceneA ? " is-a" : ""}${n === sceneB ? " is-b" : ""}${count ? " is-filled" : ""}${sc.muted ? " is-muted" : ""}`}
                  onClick={() => (copyFrom !== null ? copyTo(n) : actions.launchScene(n))}
                  onContextMenu={(e) => { e.preventDefault(); void actions.scene("sceneMute", n, !sc.muted); }}
                  aria-label={copyFrom !== null ? `Copier dans la scène ${n + 1}` : `Lancer la scène ${n + 1}`}>
                  <span className="scene-n tabular">{n + 1}</span>
                  <span className="scene-c">{sc.muted ? "muette" : count ? `${count} réglage${count > 1 ? "s" : ""}` : "vide"}</span>
                  {n === sceneA && <span className="tag tag--a">A</span>}
                  {n === sceneB && <span className="tag tag--b">B</span>}
                </button>
              );
            })}
          </div>
          <SlotCard slot="Break" scene={BREAK_SCENE} copying={copyFrom === BREAK_SCENE} onCopy={() => toggleCopy(BREAK_SCENE)} />
        </div>
      </div>
      </div>
      )}
    </ScreenFrame>
  );
}
