// Groovebox — OFAD — AGPL-3.0-or-later
// Matrice de modulation et éditeurs de pas des façades : 16 ou 32 routages « source × via →
// destination, profondeur », pas d'un LFO à pas, pas de l'arpège (transposition, vélocité).

import { useRef, useState } from "react";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress, useParam } from "../../controls/bindings";
import { useValueDrag } from "../../controls/useValueDrag";

/** Liste déroulante liée à un réglage à choix. */
function ChoiceSelect({ global, label }: { global: number; label: string }) {
  const b = useParam(global, label);
  const e = b?.entry;
  if (!b || !e?.choices) return null;
  const cur = Math.round(e.plain);
  return (
    <select className={`mx-select${cur > 0 ? " on" : ""}${b.locked ? ` fc--lock-${b.locked}` : ""}`} aria-label={label} value={cur}
      onChange={(ev) => e.pick(Number(ev.target.value))}>
      {e.choices.map((c, i) => <option key={i} value={i} disabled={c === "—"}>{c}</option>)}
    </select>
  );
}

/** Profondeur : barre bipolaire compacte (glisser, molette ; double-clic : 0). */
function AmountBar({ global, label }: { global: number; label: string }) {
  const b = useParam(global, label);
  const { handlers } = useValueDrag(b);
  if (!b) return null;
  const n = b.normalised;
  const lo = Math.min(n, 0.5), hi = Math.max(n, 0.5);
  return (
    <div className={`mx-amt${b.locked ? ` fc--lock-${b.locked}` : ""}`} role="slider" aria-label={label} tabIndex={0}
      aria-valuemin={0} aria-valuemax={1} aria-valuenow={Number(n.toFixed(3))} aria-valuetext={b.text} title={`${label} : ${b.text}`} {...handlers}>
      <span className="mx-amt-track"><span className="mx-amt-fill" style={{ left: `${lo * 100}%`, width: `${(hi - lo) * 100}%` }} /></span>
      <span className="mx-amt-text tabular">{b.text}</span>
    </div>
  );
}

/** Les routages de la matrice : par pages de 8. */
export function MatrixPanel({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const count = m.params.filter((p) => /^mx\d+_src$/.test(p.id)).length;
  const [page, setPage] = useState(0);
  if (count === 0) return null;
  const pages = Math.ceil(count / 8);
  const at = (id: string) => { const i = m.params.findIndex((p) => p.id === id); return i < 0 ? -1 : paramAddress(channel, "inst", 0, i); };
  const rows = Array.from({ length: Math.min(8, count - page * 8) }, (_, k) => page * 8 + k + 1);
  return (
    <div className="mx">
      {pages > 1 && (
        <div className="mx-pages" role="tablist" aria-label="Routages">
          {Array.from({ length: pages }, (_, i) => (
            <button key={i} type="button" role="tab" aria-selected={i === page} className={i === page ? "on" : ""} onClick={() => setPage(i)}>
              {i * 8 + 1}–{Math.min(count, i * 8 + 8)}
            </button>
          ))}
        </div>
      )}
      <div className="mx-head" aria-hidden><span>N°</span><span>Source</span><span>× Via</span><span>→ Destination</span><span>Profondeur</span></div>
      {rows.map((n) => (
        <div key={n} className="mx-row">
          <span className="mx-num tabular">{n}</span>
          <ChoiceSelect global={at(`mx${n}_src`)} label={`Routage ${n} : source`} />
          <ChoiceSelect global={at(`mx${n}_via`)} label={`Routage ${n} : via`} />
          <ChoiceSelect global={at(`mx${n}_dst`)} label={`Routage ${n} : destination`} />
          <AmountBar global={at(`mx${n}_amt`)} label={`Prof. ${n}`} />
        </div>
      ))}
    </div>
  );
}

/** Une barre de pas : glisser verticalement règle la valeur (bipolaire si le réglage l'est). */
export function StepBar({ global, index }: { global: number; index: number }) {
  const b = useParam(global, `Pas ${index + 1}`);
  const ref = useRef<HTMLDivElement>(null);
  if (!b) return null;
  const e = b.entry;
  const bipolar = !!e && e.min < 0 && e.max > 0;
  const n = b.normalised;
  const setFrom = (clientY: number) => {
    const r = ref.current?.getBoundingClientRect();
    if (!r || !e) return;
    const t = Math.min(1, Math.max(0, 1 - (clientY - r.top) / r.height));
    e.set(e.min + t * (e.max - e.min));
  };
  const zero = bipolar && e ? (0 - e.min) / (e.max - e.min) : 0;
  const lo = Math.min(n, zero), hi = Math.max(n, zero);
  return (
    <div ref={ref} className={`stepbar${b.locked ? ` fc--lock-${b.locked}` : ""}`} role="slider" aria-label={`Pas ${index + 1}`} tabIndex={0}
      aria-valuemin={e?.min} aria-valuemax={e?.max} aria-valuenow={e?.plain} aria-valuetext={b.text} title={`Pas ${index + 1} : ${b.text}`}
      onPointerDown={(ev) => { ev.currentTarget.setPointerCapture(ev.pointerId); setFrom(ev.clientY); }}
      onPointerMove={(ev) => { if (ev.buttons & 1) setFrom(ev.clientY); }}
      onDoubleClick={() => e?.set(bipolar ? 0 : e.min)}>
      <span className="stepbar-fill" style={{ bottom: `${lo * 100}%`, height: `${Math.max(1, (hi - lo) * 100)}%` }} />
      {bipolar && <span className="stepbar-zero" style={{ bottom: `${zero * 100}%` }} />}
      <span className="stepbar-num">{index + 1}</span>
    </div>
  );
}

/** Éditeur de 16 pas : identifiants « prefix1 » … « prefix16 » (lf3_s, arp_t, arp_v). */
export function StepsEditor({ channel, module: m, prefix, count }: { channel: number; module: ModuleDesc; prefix: string; count?: string }) {
  const at = (id: string) => { const i = m.params.findIndex((p) => p.id === id); return i < 0 ? -1 : paramAddress(channel, "inst", 0, i); };
  const lenGlobal = count ? at(count) : -1;
  const len = useParam(lenGlobal >= 0 ? lenGlobal : null, "Pas");
  const used = len?.entry ? Math.round(len.entry.plain) : 16;
  return (
    <div className="steps-ed">
      <div className="steps-ed-bars">
        {Array.from({ length: 16 }, (_, i) => (
          <div key={i} className={i < used || used === 0 ? "" : "steps-ed-out"}><StepBar global={at(`${prefix}${i + 1}`)} index={i} /></div>
        ))}
      </div>
    </div>
  );
}
