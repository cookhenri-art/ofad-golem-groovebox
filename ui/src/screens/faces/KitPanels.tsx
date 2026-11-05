// Groovebox — OFAD — AGPL-3.0-or-later
// Sections particulières des façades de batteries :
//  - ElektronPages : édition du pad choisi par PAGES (machine, échantillon, filtre, amplitude,
//    LFO, effet) — un petit écran et 8 boutons dessous, principe des machines à écran et pages ;
//  - KitColumns : une colonne de boutons par instrument, ceux qui comptent pour cet instrument
//    (principe des boîtes à rythmes à façade de boutons des années 80) ;
//  - StepKeys : les 16 touches de pas de l'instrument choisi, colorées par groupes de 4.

import { useState } from "react";
import { STEP_ACCENT } from "../../engine/protocol";
import type { ModuleDesc, SectionDesc } from "../../engine/protocol";
import { actions, getState, setUi, useStore } from "../../engine/store";
import { beginStepPaint, justPainted } from "../../controls/stepPaint";
import { paramAddress, steppedBinding, useParam } from "../../controls/bindings";
import { Encoder } from "../../controls/Encoder";
import { SectionVisual, TAB_LABELS, sectionsFor } from "../ModuleView";

function Knob({ global, label, size }: { global: number; label?: string; size?: "small" | "big" }) {
  const b = useParam(global, label);
  return <div className={`fc fc--${size ?? "knob"}`}><Encoder binding={b} variant="knob" /></div>;
}

const PAGES: { tab: string; label: string }[] = [
  { tab: "pad", label: "Machine" }, { tab: "padsmp", label: "Échantillon" }, { tab: "padfilter", label: "Filtre" },
  { tab: "padamp", label: "Amplitude" }, { tab: "padpc", label: "Précalcul" }, { tab: "padlfo", label: "LFO" }, { tab: "padfx", label: "Effet" },
];

/** Pad choisi, page par page : écran + 8 boutons. */
export function ElektronPages({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const lane = useStore((s) => s.ui.lane);
  const inst = useStore((s) => s.channels[channel]?.inst);
  const [page, setPage] = useState("pad");
  const pages = PAGES.filter((p) => sectionsFor(m, p.tab, lane).length > 0);
  const tab = pages.some((p) => p.tab === page) ? page : pages[0]?.tab ?? "pad";
  const section: SectionDesc | undefined = sectionsFor(m, tab, lane)[0];
  const machIdx = m.params.findIndex((p) => p.id === `p${String(lane + 1).padStart(2, "0")}_mach`);
  const machine = machIdx >= 0 && m.machines ? m.machines[Math.min(m.machines.length - 1, Math.floor((inst?.[machIdx] ?? 0) * m.machines.length))]?.name : undefined;
  return (
    <div className="ek">
      <div className="ek-screen">
        <div className="ek-screen-head">
          <b>{String(lane + 1).padStart(2, "0")} · {m.padNames[lane]}</b>
          <span>{machine ?? TAB_LABELS[tab] ?? ""}</span>
          <span className="ek-page">{pages.find((p) => p.tab === tab)?.label}</span>
        </div>
        <div className="ek-screen-visual">
          {section && <SectionVisual channel={channel} area="inst" slot={0} module={m} section={section} lane={lane} />}
        </div>
      </div>
      <div className="ek-knobs">
        {Array.from({ length: 8 }, (_, i) => {
          const k = section?.params[i];
          return k !== undefined ? <Knob key={`${tab}:${k}`} global={paramAddress(channel, "inst", 0, k)} /> : <div key={i} className="fc ek-empty" />;
        })}
      </div>
      <div className="ek-pages" role="tablist" aria-label="Pages du pad">
        {pages.map((p) => (
          <button key={p.tab} type="button" role="tab" aria-selected={p.tab === tab} className={p.tab === tab ? "on" : ""} onClick={() => setPage(p.tab)}>
            <span className="fc-led" aria-hidden />{p.label}
          </button>
        ))}
      </div>
    </div>
  );
}

/** Réglages montrés pour chaque instrument (les premiers trouvés, dans cet ordre). */
const BY_KIND: Record<string, RegExp[]> = {
  kick: [/_level$/, /_tune$/, /_tone$/, /_snap$/, /_decay$/],
  snare: [/_level$/, /_tune$/, /_tone$/, /_snap$/],
  clap: [/_level$/, /_tone$/],
  rim: [/_level$/, /_tune$/],
  tom: [/_level$/, /_tune$/, /_decay$/],
  hatClosed: [/_level$/, /_decay$/],
  hatOpen: [/_level$/, /_decay$/, /_tune$/],
  cowbell: [/_level$/, /_tune$/],
  cymbal: [/_level$/, /_decay$/, /_tone$/, /_x2$/],
  fm: [/_level$/, /_tune$/, /_tone$/, /_x1$/, /_decay$/],
  sample: [/_level$/, /_tune$/],
};

/** Une colonne de boutons par instrument (le pad choisi est surligné ; toucher son nom le choisit). */
export function KitColumns({ channel, module: m, max }: { channel: number; module: ModuleDesc; max?: number }) {
  const lane = useStore((s) => s.ui.lane);
  const lit = useStore((s) => s.litLanes);
  return (
    <div className="kitcols">
      {Array.from({ length: 16 }, (_, i) => {
        const prefix = `p${String(i + 1).padStart(2, "0")}_`;
        const kind = m.padKinds[i] ?? "sample";
        const res = BY_KIND[kind] ?? BY_KIND.sample;
        const ids = res.map((re) => m.params.findIndex((p) => p.id.startsWith(prefix) && re.test(p.id))).filter((k) => k >= 0).slice(0, max ?? 99);
        return (
          <div key={i} className={`kitcol kind--${kind}${i === lane ? " is-selected" : ""}${(lit[i] ?? 0) > 0 ? " is-lit" : ""}`}>
            <button type="button" className="kitcol-name" onClick={() => setUi({ lane: i })} title={`Choisir ${m.padNames[i]}`}>{m.padNames[i]}</button>
            {ids.map((k, j) => <Knob key={k} global={paramAddress(channel, "inst", 0, k)} size={j === 0 ? undefined : "small"} />)}
          </div>
        );
      })}
    </div>
  );
}

/** Les 16 touches de pas de l'instrument choisi (page en cours), colorées par groupes de 4. */
export function StepKeys({ palette }: { palette: "boum" | "club" | "machine" | "dragon" }) {
  const pattern = useStore((s) => s.pattern);
  const lane = useStore((s) => s.ui.lane);
  const page = useStore((s) => s.ui.page);
  const pos = useStore((s) => s.tick.pos);
  const playing = useStore((s) => s.tick.t.playing);
  // « AC » : comme le sélecteur d'instrument des boîtes à rythmes, les 16 touches écrivent alors
  // la piste d'accent (accent total du pas, pour tous les instruments) au lieu des pas du pad
  const [accentMode, setAccentMode] = useState(false);
  if (!pattern) return null;
  const ln = pattern.lanes[lane];
  const pages = Math.max(1, Math.ceil(pattern.length / 16));
  const pg = Math.min(page, pages - 1);
  const acc = (s: number) => ((pattern.flags[s] ?? 0) & STEP_ACCENT) !== 0;
  const playStep = Math.max(-1, ...pattern.lanes.map((_, l) => pos[l] ?? -1));
  return (
    <div className="stepkeys-wrap">
      <button type="button" className={`stepkey stepkey-ac${accentMode ? " on" : ""}`} aria-pressed={accentMode}
        title="Piste d'accent : les 16 touches posent ou retirent l'accent du pas (tous les instruments du pas sont accentués, force : bouton Accent). Rappuyer pour revenir aux pas de l'instrument."
        onClick={() => setAccentMode(!accentMode)}>
        <span className="stepkey-lamp" aria-hidden />
        <span className="stepkey-num">AC</span>
      </button>
      <div className={`stepkeys stepkeys--${palette}${accentMode ? " stepkeys--acc" : ""}`} data-paint-row>
        {Array.from({ length: 16 }, (_, i) => {
          const s = pg * 16 + i;
          const on = accentMode ? acc(s) : (ln?.vel[s] ?? 0) > 0;
          const out = s >= pattern.length;
          const here = playing && (accentMode ? playStep : pos[lane] ?? -1) === s;
          return (
            <button key={i} type="button" disabled={out} aria-pressed={on} aria-label={accentMode ? `Accent du pas ${s + 1}` : `Pas ${s + 1}`} data-paint-step={s}
              onPointerDown={(e) => beginStepPaint(e, s, {
                // v0.46.2 : pinceau : glisser sur les touches voisines pose (ou retire) tous ces pas
                isOn: (x) => { const p = getState().pattern; return accentMode ? ((p?.flags[x] ?? 0) & STEP_ACCENT) !== 0 : (p?.lanes[lane]?.vel[x] ?? 0) > 0; },
                toggle: (x) => {
                  const p = getState().pattern;
                  if (accentMode) void actions.seq("seqStepsFlag", JSON.stringify([x]), STEP_ACCENT, ((p?.flags[x] ?? 0) & STEP_ACCENT) === 0);
                  else void actions.seq("seqSetStep", lane, x, (p?.lanes[lane]?.vel[x] ?? 0) > 0 ? 0 : 100);
                },
              })}
              className={`stepkey g${Math.floor(i / 4)}${on ? " on" : ""}${here ? " here" : ""}${!accentMode && acc(s) ? " has-acc" : ""}`}
              onClick={() => justPainted() ? undefined : accentMode ? void actions.seq("seqStepsFlag", JSON.stringify([s]), STEP_ACCENT, !acc(s))
                : void actions.seq("seqSetStep", lane, s, on ? 0 : 100)}>
              <span className="stepkey-lamp" aria-hidden />
              <span className="stepkey-num">{s + 1}</span>
            </button>
          );
        })}
      </div>
    </div>
  );
}

/** Force de l'accent de la piste (séquenceur) : le bouton « accent » des boîtes à rythmes. */
export function AccentKnob() {
  const accent = useStore((s) => s.pattern?.accent ?? 50);
  const b = steppedBinding("Accent", Array.from({ length: 21 }, (_, i) => i * 5), Math.round(accent / 5) * 5, (a) => void actions.seq("seqAccent", a), (a) => `${a} %`);
  return <div className="fc fc--big"><Encoder binding={b} variant="knob" /></div>;
}
