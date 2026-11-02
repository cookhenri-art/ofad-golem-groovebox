// Groovebox — OFAD — AGPL-3.0-or-later
// Vocodeurs (v0.25) : choix du modulateur (la voix) et barres des bandes analysées, lues dans
// l'état de l'instrument ([nombre de bandes, part non voisée, niveau de chaque bande, puis (v0.33)
// état de la prise, secondes, avancement de la lecture, phrase présente, mot prononcé, crête de l'entrée]).
// v0.33 : phrase tapée de la voix interne ; prise de son (enregistrée au micro, puis en boucle).

import { useContext, useEffect, useRef, useState } from "react";
import { actions, useStore } from "../../engine/store";
import { BigTextContext } from "./SynthPanel";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress } from "../../controls/bindings";

const DEMO_TEXT = "Phrase de démonstration (syllabes « a o è ou i »)";

const HINTS: Record<number, string> = {
  0: "Voix interne : la phrase tapée (ou la phrase de démonstration) fait parler le vocodeur sans micro ; débit et hauteur réglables. Jouez des notes ou un accord : la porteuse prend la voix.",
  1: "Entrée audio : branchez un micro à la carte son (ou la piste du logiciel hôte). La porte de bruit évite que le souffle du micro fasse parler la porteuse.",
  2: "Piste précédente : la piste juste au-dessus sert de voix (une piste « Entrée audio », un sampler de voix, une batterie…). Elle peut être retirée du mixage.",
};
const CAPTURE_HINT = "Prise de son : appuyez sur « Enregistrer », puis dites votre phrase au micro. L'enregistrement démarre avec la voix et s'arrête seul après un court silence ; la phrase tourne ensuite en boucle et reste avec le projet.";

/** Les champs de l'état ajoutés en v0.33 (après les bandes). */
function vocExtra(status: number[]) {
  const n = Math.max(0, Math.round(status[0] ?? 0));
  const b = 2 + n;
  return {
    capState: Math.round(status[b] ?? 0),     // 0 rien, 1 armée, 2 en cours, 3 prête, 4-6 gardée / en train de l'être
    seconds: status[b + 1] ?? 0,
    play: status[b + 2] ?? 0,
    hasPhrase: (status[b + 3] ?? 0) > 0.5,
    word: Math.round(status[b + 4] ?? -1),
    inPeak: status[b + 5] ?? 0,
  };
}

export function VocSourcePanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const tracks = useStore((s) => s.tracks.tracks); // (sélecteur stable : pas de nouveau tableau à chaque lecture)
  const names = tracks.map((t) => t.name);
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const srcIdx = idx("src"), muteIdx = idx("srcmute");
  if (srcIdx < 0) return null;
  const desc = m.params[srcIdx];
  const count = desc.choices?.length ?? 3;
  const capture = desc.choices?.[count - 1] === "Prise de son" ? count - 1 : -1; // v0.33 : dernier choix
  const src = values ? Math.round(toPlain(desc, values[srcIdx] ?? 0)) : 0;
  const isTrackSrc = (k: number) => k >= 3 && k !== capture;
  const muteDesc = muteIdx >= 0 ? m.params[muteIdx] : null;
  const removed = muteDesc && values ? Math.round(toPlain(muteDesc, values[muteIdx] ?? 1)) === 1 : true;
  const set = (i: number, d: typeof desc, v: number) => actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(d, v));
  const label = (k: number) => {
    if (k < 3 || k === capture) return desc.choices?.[k] ?? String(k);
    const t = k - 3;
    return t === channel ? `Piste ${t + 1} (celle-ci)` : `Piste ${t + 1} · ${names[t] ?? ""}`;
  };
  // grandes inscriptions (vocodeurs v0.29) : l'explication passe en bulle, seuls les avertissements restent écrits
  const compact = useContext(BigTextContext);
  const warn = (src === 2 && channel === 0) || (isTrackSrc(src) && src - 3 === channel);
  const hint = src === 2 && channel === 0 ? "La piste 1 n'a pas de piste au-dessus d'elle : choisissez l'entrée audio ou une autre piste."
    : isTrackSrc(src) && src - 3 === channel ? "Une piste ne peut pas être sa propre voix (le son tournerait en boucle) : le vocodeur reste muet. Choisissez une autre source."
    : src === capture ? CAPTURE_HINT
    : HINTS[src] ?? `Piste ${src - 2} : sa sortie (après ses effets, avant son volume) sert de voix. Baissez son volume pour ne garder que le vocodeur ; la voix arrive avec moins d'un millième de seconde de décalage si la piste est plus bas dans la liste.`;
  const order = capture >= 0 ? [0, capture, 1, 2] : [0, 1, 2];
  const withDetail = src === 0 || (capture >= 0 && src === capture);
  return (
    <div className={`vocsrc${withDetail ? " vocsrc--detail" : ""}`}>
      <div className="vocsrc-main">
        <div className="vocsrc-btns" role="radiogroup" aria-label="Modulateur du vocodeur" title={hint}>
          {order.map((k) => (
            <button key={k} type="button" role="radio" aria-checked={src === k} className={`vocsrc-btn${src === k ? " on" : ""}`}
              disabled={k === 2 && channel === 0} onClick={() => set(srcIdx, desc, k)}>{label(k)}</button>
          ))}
          <label className={`vocsrc-btn vocsrc-track${isTrackSrc(src) ? " on" : ""}`}>
            <span className="vocsrc-sr">Une piste comme voix</span>
            <select value={isTrackSrc(src) ? src : ""} aria-label="Une piste comme voix" onChange={(e) => set(srcIdx, desc, Number(e.target.value))}>
              <option value="" disabled>Une piste…</option>
              {(desc.choices ?? []).map((_, k) => (isTrackSrc(k) ? <option key={k} value={k} disabled={k - 3 === channel}>{label(k)}</option> : null))}
            </select>
          </label>
        </div>
        {src === 2 && muteDesc && (
          <button type="button" className={`touch touch--small${removed ? " touch--on" : ""}`} onClick={() => set(muteIdx, muteDesc, removed ? 0 : 1)}
            title="Retirée : la piste précédente n'est plus envoyée au mixage, seul le vocodeur l'utilise">
            {removed ? "Piste précédente retirée du mixage" : "Piste précédente entendue"}
          </button>
        )}
        {(warn || !compact) && <p className="vocsrc-hint">{hint}</p>}
      </div>
      {src === 0 && <VocPhrase channel={channel} />}
      {(src === 1 || (capture >= 0 && src === capture)) && <MicGuard />}
      {capture >= 0 && src === capture && <VocCapture channel={channel} module={m} />}
    </div>
  );
}

/** v0.33 : la phrase de la voix interne, tapée au clavier ; le mot prononcé est surligné.
    v0.39 : français ou anglais (« @en » en tête de la phrase enregistrée), écriture expressive. */
const EN_PREFIX = "@en ";
const EXPR_HELP = "Écriture expressive : plusieurs espaces = un silence plus long · « ... » = phrase en suspens · « muuutants » = voyelle étirée · MOT EN CAPITALES = appuyé · « ! » et « ? » changent la mélodie (doublés : plus marqués).";
function VocPhrase({ channel }: { channel: number }) {
  const stored = useStore((s) => s.tracks.tracks[channel]?.speech ?? "");
  const english = stored.startsWith(EN_PREFIX) || stored === "@en";
  const saved = english ? stored.slice(EN_PREFIX.length) : stored;
  const word = useStore((s) => (s.focus === channel ? vocExtra(s.tick.inst ?? []).word : -1));
  const [text, setText] = useState(saved);
  const [editing, setEditing] = useState(false);
  useEffect(() => { if (!editing) setText(saved); }, [saved, editing]);
  const dirty = text.trimEnd() !== saved;
  const send = (body: string, en: boolean) => actions.setSpeech(channel, body ? (en ? EN_PREFIX : "") + body : (en ? "@en" : ""));
  const apply = () => { void send(text.trimEnd(), english); setEditing(false); };
  const words = saved.split(/\s+/).filter(Boolean);
  return (
    <div className="vocsrc-detail">
      <div className="vocsrc-row vocsrc-row--head">
        <label className="vocsrc-label" htmlFor={`voc-phrase-${channel}`}>Phrase de la voix interne</label>
        <span className="vocsrc-lang" role="radiogroup" aria-label="Langue de la voix interne">
          {[["Français", false], ["English", true]].map(([l, en]) => (
            <button key={String(l)} type="button" role="radio" aria-checked={english === en} className={english === en ? "on" : undefined}
              onMouseDown={(e) => e.preventDefault()} onClick={() => { if (english !== en) void send(text.trimEnd(), en as boolean); }}>{l as string}</button>
          ))}
        </span>
      </div>
      <textarea id={`voc-phrase-${channel}`} className="vocsrc-text" rows={3} maxLength={160} value={text} spellCheck={false}
        placeholder={english ? "Type a sentence in English, then « Dire »" : "Tapez une phrase (en français), puis « Dire »"} title={EXPR_HELP}
        onFocus={() => setEditing(true)} onBlur={() => setEditing(false)}
        onChange={(e) => setText(e.target.value.replace(/[\r\n\t]/g, " "))}
        onKeyDown={(e) => { if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); apply(); } e.stopPropagation(); }} />
      <div className="vocsrc-row">
        <button type="button" className="vocsrc-act vocsrc-act--main" disabled={!dirty} onMouseDown={(e) => e.preventDefault()} onClick={apply}>Dire</button>
        <button type="button" className="vocsrc-act" disabled={!saved && !text} onMouseDown={(e) => e.preventDefault()}
          onClick={() => { setText(""); void send("", english); setEditing(false); }} title="Revenir à la phrase de démonstration">Démonstration</button>
        <span className="vocsrc-count">{text.length}/160</span>
      </div>
      <p className="vocsrc-say" aria-live="off">
        {words.length === 0 ? <span className="vocsrc-demo">{DEMO_TEXT}</span>
          : words.map((w, k) => <span key={k}><span className={k === word ? "on" : undefined}>{w}</span>{" "}</span>)}
      </p>
      <p className="vocsrc-tip">Espaces = silence · « ... » = suspens · « muuu » = étiré · CAPITALES = appuyé · ! et ? = mélodie</p>
    </div>
  );
}

/** v0.39 : l'application autonome coupe l'entrée audio par sécurité (« Mute audio input ») : on le dit et on propose de l'ouvrir. */
function MicGuard() {
  const [state, setState] = useState(-1);
  useEffect(() => {
    let alive = true;
    const poll = () => { void actions.inputMute(-1).then((v) => { if (alive) setState(v); }); };
    poll();
    const t = window.setInterval(poll, 2000);
    return () => { alive = false; window.clearInterval(t); };
  }, []);
  if (state !== 1) return null;
  return (
    <div className="vocsrc-mic" role="alert">
      <p>Le micro est coupé par l'application (sécurité anti-larsen « Mute audio input ») : rien n'arrive au vocodeur.</p>
      <button type="button" className="vocsrc-act vocsrc-act--main" onClick={() => { void actions.inputMute(0).then(setState); }}>Activer le micro</button>
      <small>Au casque de préférence : avec des enceintes, le son peut revenir dans le micro et siffler.</small>
    </div>
  );
}

/** v0.33 : prise de son — enregistrer une phrase au micro, la faire tourner en boucle. */
function VocCapture({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const values = useStore((s) => s.channels[channel]?.inst);
  const sample = useStore((s) => s.channels[channel]?.samples?.[0] ?? null);
  const x = vocExtra(status);
  const loopIdx = m.params.findIndex((p) => p.id === "ploop");
  const loopDesc = loopIdx >= 0 ? m.params[loopIdx] : null;
  const loop = loopDesc && values ? Math.round(toPlain(loopDesc, values[loopIdx] ?? 0)) : 0;
  // armée depuis un moment sans aucun signal : l'entrée est coupée ou absente
  const armedAt = useRef(0);
  const [silentInput, setSilentInput] = useState(false);
  useEffect(() => {
    if (x.capState !== 1) { armedAt.current = 0; if (silentInput) setSilentInput(false); return; }
    if (!armedAt.current) armedAt.current = Date.now();
    const silent = x.inPeak <= 1e-7 && Date.now() - armedAt.current > 2500;
    if (silent !== silentInput) setSilentInput(silent);
  }, [x.capState, x.inPeak, silentInput]);
  const busy = x.capState === 1 || x.capState === 2;
  const has = x.hasPhrase || !!sample;
  const recLabel = x.capState === 1 ? "Parlez… (annuler)" : x.capState === 2 ? `■ Arrêter · ${x.seconds.toFixed(1)} s` : has ? "● Refaire la prise" : "● Enregistrer";
  const stateText = x.capState === 1 ? "En attente de la voix" : x.capState === 2 ? "Enregistrement…"
    : x.capState === 6 ? "Prise non gardée (mémoire pleine ?) : elle tourne quand même" : has ? `Phrase en boucle${x.seconds > 0 ? ` · ${x.seconds.toFixed(1)} s` : ""}` : "Aucune phrase enregistrée";
  const loopShort = ["Phrase", "1 mes.", "2 mes.", "4 mes."];
  return (
    <div className="vocsrc-detail">
      <span className="vocsrc-label">Prise de son</span>
      <button type="button" className={`vocsrc-rec${busy ? " on" : ""}${x.capState === 1 ? " armed" : ""}`} onClick={() => actions.recordAction(channel, 0)}
        aria-pressed={busy}>{recLabel}</button>
      <div className="vocsrc-state" role="status">{stateText}</div>
      {!busy && !has && <p className="vocsrc-hint">Appuyez, puis parlez au micro : la prise démarre avec la voix et s'arrête seule après un court silence.</p>}
      <div className="vocsrc-play" aria-hidden="true"><i style={{ width: `${Math.round((busy ? 0 : x.play) * 100)}%` }} /></div>
      {silentInput && (
        <p className="vocsrc-hint vocsrc-warn">Rien n'arrive à l'entrée audio. Vérifiez l'entrée choisie dans « Settings… » (bandeau du haut) : le micro de la carte son, canaux d'entrée cochés. Sur la tablette, autorisez le micro.</p>
      )}
      {loopDesc && (
        <div className="vocsrc-loop" role="radiogroup" aria-label="Boucle de la prise" title="Longueur de la phrase : elle tourne librement. 1, 2 ou 4 mesures : elle repart au début de chaque groupe de mesures du séquenceur.">
          <span>Boucle</span>
          {loopShort.map((l, k) => (
            <button key={k} type="button" role="radio" aria-checked={loop === k} className={loop === k ? "on" : undefined}
              onClick={() => actions.setParam(paramAddress(channel, "inst", 0, loopIdx), toNorm(loopDesc, k))}>{l}</button>
          ))}
        </div>
      )}
      <button type="button" className="vocsrc-act" disabled={!has && !busy}
        onClick={() => { actions.recordAction(channel, 2); if (sample) void actions.clearSample(channel, 0); }}>Effacer la phrase</button>
    </div>
  );
}

export function VocBandsPanel({ channel }: { channel: number; module: ModuleDesc }) {
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const n = Math.max(0, Math.min(40, Math.round(status[0] ?? 0)));
  const unvoiced = status[1] ?? 0;
  return (
    <div className="vocbands">
      <div className="vocbands-bars" role="img" aria-label={`${n} bandes analysées`}>
        {Array.from({ length: n || 16 }, (_, k) => (
          <span key={k} className="vocbands-bar"><i style={{ height: `${Math.round((status[2 + k] ?? 0) * 100)}%` }} /></span>
        ))}
      </div>
      <div className="vocbands-legend">
        <span>grave</span>
        <span className={`vocbands-uv${unvoiced > 0.45 ? " on" : ""}`} title="Consonnes sifflantes détectées (« s », « ch », « f »)">sifflante</span>
        <span>aigu</span>
      </div>
    </div>
  );
}
