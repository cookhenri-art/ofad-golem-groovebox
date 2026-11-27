// Groovebox — OFAD — AGPL-3.0-or-later
// Croc Glacé (1.3) : l'ORB (le pavé : quatre coins colorés selon leur part du son, le point, l'orbite et
// l'orbe 3 lus dans l'état de l'instrument ; tenir le point au doigt), les quatre coins (charger un son
// d'usine, capturer le son en cours, vider), un couloir de 32 pas du séquenceur de LFO et les tables perso
// importées d'un WAV.
// v2 : page ORB « pavé fixe à gauche, onglets à droite » (Morph, Orbite, Physique, Routes) — sur téléphone
// le pavé passe en haut, réduit ; chaque pas montre l'allure du LFO qu'il joue ; la sortie du couloir est
// tracée sous les pas ; 32 motifs classiques au menu, retouchables pas à pas ; les WAV se prennent dans la
// Bibliothèque de GOLEM.

import { useEffect, useRef, useState } from "react";
import { actions, hasNative, useStore } from "../../engine/store";
import { toNorm, toPlain } from "../../engine/params";
import type { ModuleDesc } from "../../engine/protocol";
import { paramAddress } from "../../controls/bindings";
import { FolderBrowser } from "../../controls/FolderBrowser";
import { Control, type Ctl } from "./SynthPanel";
import { CROC_MOTIFS } from "./crocMotifs";
import { translate } from "../../i18n";

/** Libellés lus par le navigateur hors du texte de la page (attribut label des groupes de liste) : traduits ici. */
const tr = (t: string) => (document.documentElement.lang === "en" ? translate(t) : t);

const CORNERS = ["A", "B", "C", "D"] as const;
const CORNER_NAMES = ["Haut gauche", "Haut droite", "Bas gauche", "Bas droite"];
const CORNER_COLORS = ["oklch(0.82 0.13 85)", "oklch(0.75 0.16 55)", "oklch(0.66 0.2 25)", "oklch(0.9 0.03 250)"];
const STEPS = 32;
const FIXED = 8; // un pas « fixe » : son niveau seul, sans LFO

/** Lecture et écriture des réglages de la piste (valeurs réelles). */
function useInst(channel: number, m: ModuleDesc) {
  const values = useStore((s) => s.channels[channel]?.inst);
  const idx = (id: string) => m.params.findIndex((p) => p.id === id);
  const get = (id: string) => {
    const i = idx(id);
    return i < 0 || !values ? 0 : toPlain(m.params[i], values[i] ?? 0);
  };
  const norm = (id: string) => { const i = idx(id); return i < 0 || !values ? 0 : values[i] ?? 0; };
  const set = (id: string, plain: number) => {
    const i = idx(id);
    if (i >= 0) actions.setParam(paramAddress(channel, "inst", 0, i), toNorm(m.params[i], plain));
  };
  const setNorm = (id: string, n: number) => { const i = idx(id); if (i >= 0) actions.setParam(paramAddress(channel, "inst", 0, i), n); };
  return { get, set, norm, setNorm, idx };
}

/** Réglages mélangés par l'ORB : ceux qui ont une copie dans chaque coin (« ka_cutoff » → « cutoff »). */
function morphIds(m: ModuleDesc) {
  return m.params.filter((p) => p.id.startsWith("ka_") && p.id !== "ka_on" && p.id !== "ka_src").map((p) => p.id.slice(3));
}

// 27 nov. 2026 : nom du préréglage perso chargé dans un coin (le réglage « src » ne garde que les sons d'usine ;
// un son perso y est rangé comme un son capturé : 0)
const userCornerNames = new Map<string, string>();
function cornerLabel(m: ModuleDesc, get: (id: string) => number, c: number, channel = -1) {
  const k = `k${"abcd"[c]}_`;
  if (get(`${k}on`) < 0.5) return "Vide : joue le son en cours";
  const src = Math.round(get(`${k}src`));
  return src > 0 ? m.presets[src - 1] ?? "Son d'usine" : userCornerNames.get(`${channel}:${k}`) ?? "Son capturé";
}

/** Le pavé de l'ORB. */
export function CrocOrb({ channel, module: m, compact = false }: { channel: number; module: ModuleDesc; compact?: boolean }) {
  const { get, set } = useInst(channel, m);
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const cv = useRef<HTMLCanvasElement>(null);
  const trail = useRef<[number, number][]>([]);
  const dragging = useRef(false);
  const px = status[0] ?? get("morphx") / 100, py = status[1] ?? get("morphy") / 100;
  const ox = status[2] ?? px, oy = status[3] ?? py, qx = status[4] ?? 0, qy = status[5] ?? 0;
  const morphOn = get("morphon") > 0.5, orbitOn = get("orbon") > 0.5;
  const w = [status[24] ?? 0.25, status[25] ?? 0.25, status[26] ?? 0.25, status[27] ?? 0.25];

  useEffect(() => {
    const c = cv.current;
    const g = c?.getContext("2d");
    if (!c || !g) return;
    const W = c.width, H = c.height;
    g.fillStyle = "oklch(0.13 0.02 250)";
    g.fillRect(0, 0, W, H);
    // les quatre coins : une lueur de leur couleur, d'autant plus forte que leur part du son est grande
    const pos: [number, number][] = [[0, 0], [W, 0], [0, H], [W, H]];
    pos.forEach(([x, y], i) => {
      const r = g.createRadialGradient(x, y, 0, x, y, W * 0.85);
      const a = morphOn ? 0.12 + w[i] * 0.55 : 0.08;
      r.addColorStop(0, CORNER_COLORS[i].replace(")", ` / ${a.toFixed(2)})`));
      r.addColorStop(1, "oklch(0 0 0 / 0)");
      g.fillStyle = r; g.fillRect(0, 0, W, H);
    });
    // rayons du point (le « mandala ») et croix du centre
    const X = ((px + 1) / 2) * W, Y = (1 - (py + 1) / 2) * H;
    g.strokeStyle = "oklch(0.85 0.06 220 / 0.18)"; g.lineWidth = 1;
    g.beginPath(); g.moveTo(W / 2, 0); g.lineTo(W / 2, H); g.moveTo(0, H / 2); g.lineTo(W, H / 2); g.stroke();
    for (let k = 0; k < 48; k++) {
      const a = (k / 48) * Math.PI * 2, len = W * (0.05 + 0.12 * (0.5 + 0.5 * Math.sin(k * 2.3 + px * 5 + py * 7)));
      g.strokeStyle = `oklch(0.9 0.08 ${200 + k * 3} / 0.12)`;
      g.beginPath(); g.moveTo(X, Y); g.lineTo(X + Math.cos(a) * len, Y + Math.sin(a) * len); g.stroke();
    }
    // trace du point, orbite, orbe 3
    const tr = trail.current;
    tr.push([X, Y]); if (tr.length > 40) tr.shift();
    for (let i = 1; i < tr.length; i++) {
      g.strokeStyle = `oklch(0.85 0.12 200 / ${(i / tr.length) * 0.7})`; g.lineWidth = 1 + i / 12;
      g.beginPath(); g.moveTo(tr[i - 1][0], tr[i - 1][1]); g.lineTo(tr[i][0], tr[i][1]); g.stroke();
    }
    const cx = ((get("morphx") / 100 + 1) / 2) * W, cy = (1 - (get("morphy") / 100 + 1) / 2) * H;
    g.strokeStyle = "oklch(0.9 0.02 250 / 0.6)"; g.lineWidth = 1.5;
    g.beginPath(); g.arc(cx, cy, W / 40, 0, Math.PI * 2); g.stroke();
    if (orbitOn) {
      const OX = ((ox + 1) / 2) * W, OY = (1 - (oy + 1) / 2) * H, QX = ((qx + 1) / 2) * W, QY = (1 - (qy + 1) / 2) * H;
      g.setLineDash([3, 4]); g.strokeStyle = "oklch(0.8 0.05 250 / 0.5)";
      g.beginPath(); g.moveTo(cx, cy); g.lineTo(OX, OY); g.lineTo(X, Y); g.stroke(); g.setLineDash([]);
      g.fillStyle = "oklch(0.7 0.2 330)"; g.beginPath(); g.arc(QX, QY, W / 70, 0, Math.PI * 2); g.fill();
    }
    const R = W / 30;
    const rg = g.createRadialGradient(X - R / 3, Y - R / 3, R / 8, X, Y, R * 1.3);
    rg.addColorStop(0, "#f2fbff"); rg.addColorStop(0.45, "oklch(0.82 0.13 210)"); rg.addColorStop(1, "oklch(0.45 0.14 240)");
    g.shadowColor = "oklch(0.8 0.15 210)"; g.shadowBlur = R * 1.5;
    g.fillStyle = rg; g.beginPath(); g.arc(X, Y, R, 0, Math.PI * 2); g.fill(); g.shadowBlur = 0;
  });

  const at = (e: React.PointerEvent) => {
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    return [Math.max(-1, Math.min(1, ((e.clientX - r.left) / r.width) * 2 - 1)), Math.max(-1, Math.min(1, 1 - ((e.clientY - r.top) / r.height) * 2))];
  };
  const put = (e: React.PointerEvent) => { const [x, y] = at(e); set("morphx", Math.round(x * 100)); set("morphy", Math.round(y * 100)); };
  return (
    <div className={`cg-orb${compact ? " cg-orb--compact" : ""}`}>
      <div className="cg-orb-pad">
        <canvas ref={cv} width={480} height={480} className="cg-orb-cv"
          onPointerDown={(e) => { e.preventDefault(); (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId); dragging.current = true; put(e); }}
          onPointerMove={(e) => { if (dragging.current) put(e); }}
          onPointerUp={() => { dragging.current = false; }} onPointerCancel={() => { dragging.current = false; }}
          title="Tenir et glisser : le point mélange les quatre coins (avec l'orbite en marche, l'orbite tourne autour de lui)" />
        {CORNERS.map((n, i) => (
          <span key={n} className={`cg-orb-corner cg-orb-corner--${i}`} style={{ color: CORNER_COLORS[i] }}>
            {i + 1} · {CORNER_NAMES[i]}<b>{cornerLabel(m, get, i, channel)}</b><em>{Math.round(w[i] * 100)} %</em>
          </span>
        ))}
      </div>
      <p className="cg-hint">{morphOn ? "Le point fait muter le son entre les quatre coins." : "ORB à l'arrêt : mettez-le en marche (onglet Morph) pour mélanger les coins."} Un coin vide joue le son en cours.</p>
    </div>
  );
}

/** Les quatre coins : charger un son d'usine ou un de ses propres sons, capturer le son en cours, vider. */
export function CrocCorners({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const { get, set, norm, setNorm } = useInst(channel, m);
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const ids = morphIds(m);
  // 27 nov. 2026 : ses propres sons (préréglages de l'utilisateur de Croc Glacé), relus à l'ouverture d'une liste
  const [mine, setMine] = useState<[string, string][]>([]);
  const refreshMine = () => { void actions.listPresets("inst", channel, -1, 0).then((l) => setMine(l ? l.user.map(([b, n]) => [b, n] as [string, string]) : [])); };
  useEffect(refreshMine, [channel]);
  const loadMine = async (c: number, idx: number) => {
    const k = `k${"abcd"[c]}_`;
    const e = mine[idx];
    if (!e) return;
    if (!(await actions.userPresetToPrefix(channel, e[0], e[1], k))) return;
    userCornerNames.set(`${channel}:${k}`, e[1]);
    set(`${k}src`, 0);
    set(`${k}on`, 1);
  };
  const load = async (c: number, presetIndex: number) => {
    const k = `k${"abcd"[c]}_`;
    const name = m.presets[presetIndex];
    if (!name) return;
    await actions.presetToPrefix(channel, name, k); // le moteur copie les réglages du son d'usine dans le coin
    set(`${k}src`, presetIndex + 1);
    set(`${k}on`, 1);
  };
  const capture = (c: number) => {
    const k = `k${"abcd"[c]}_`;
    userCornerNames.delete(`${channel}:${k}`);
    for (const id of ids) setNorm(k + id, norm(id));
    set(`${k}src`, 0);
    set(`${k}on`, 1);
  };
  return (
    <div className="cg-corners">
      {CORNERS.map((n, i) => {
        const k = `k${"abcd"[i]}_`;
        const on = get(`${k}on`) > 0.5, src = Math.round(get(`${k}src`));
        const share = status[24 + i] ?? 0;
        return (
          <div key={n} className={`cg-corner${on ? " on" : ""}`}>
            <div className="cg-corner-head" style={{ color: CORNER_COLORS[i] }}>{i + 1} · {CORNER_NAMES[i]}</div>
            <select value={on && src > 0 ? String(src - 1) : on ? "cap" : ""} onFocus={refreshMine}
              onChange={(e) => { const v = e.target.value; if (v.startsWith("u:")) void loadMine(i, Number(v.slice(2))); else if (v !== "" && v !== "cap") void load(i, Number(v)); }}
              aria-label={`Son du coin ${n}`}>
              <option value="">Vide : joue le son en cours</option>
              {on && src === 0 && <option value="cap">{userCornerNames.get(`${channel}:${k}`) ? `Mes sons · ${userCornerNames.get(`${channel}:${k}`)}` : "Son capturé"}</option>}
              {mine.length > 0 && (
                <optgroup label={tr("Mes sons")}>
                  {mine.map(([b, nm], j) => <option key={`u${j}`} value={`u:${j}`}>{`${b} · ${nm}`}</option>)}
                </optgroup>
              )}
              <optgroup label={tr("Sons d'usine")}>
                {m.presets.map((p, j) => p === "Init" ? null : <option key={p} value={String(j)}>{(m.presetCats?.[j] ? `${m.presetCats[j]} · ` : "") + p}</option>)}
              </optgroup>
            </select>
            <div className="cg-corner-share"><i style={{ width: `${Math.round(share * 100)}%`, background: CORNER_COLORS[i] }} /><span>{Math.round(share * 100)} % du son</span></div>
            <div className="cg-corner-btns">
              <button type="button" className="fc-toggle" onClick={() => capture(i)} title="Copier le son en cours dans ce coin">Capturer</button>
              <button type="button" className="fc-toggle" disabled={!on} onClick={() => set(`${k}on`, 0)} title="Vider ce coin (il joue alors le son en cours)">Vider</button>
            </div>
          </div>
        );
      })}
    </div>
  );
}

/* ---------- page ORB : pavé fixe, onglets ---------- */

const TABS = [
  { id: "morph", label: "Morph" },
  { id: "orbit", label: "Orbite" },
  { id: "phys", label: "Physique" },
  { id: "routes", label: "Routes" },
] as const;
type TabId = (typeof TABS)[number]["id"];

// 27 nov. : onglet Orbite en trois blocs alignés (Orbite, Sous-orbite, Orbe 3) : une ligne de menus puis une
// ligne de boutons de même taille, sur la même grille de 4 colonnes d'un bloc à l'autre (« _ » = case vide)
const GAP: Ctl = { id: "_", label: "" };
const ORBIT_BLOCKS: { title: string; menus: Ctl[]; knobs: Ctl[] }[] = [
  { title: "Orbite",
    menus: [{ id: "orbon", k: "toggle", label: "Orbite", on: 1, tip: "Le point tourne tout seul autour de la position choisie au doigt" },
      { id: "o1shape", k: "menu", label: "Forme" }, { id: "o1dir", k: "menu", label: "Sens" },
      { id: "o1sync", k: "menu", label: "Tempo", tip: "« Libre » : la vitesse se règle en Hz ; sinon un tour par division du tempo" }],
    knobs: [{ id: "o1rate", label: "Vitesse", dimUnless: { id: "o1sync", eq: 0 } }, { id: "o1size", label: "Taille", tip: "Taille 0 : le point reste où on le pose" },
      { id: "o1rot", label: "Rotation" }, { id: "o1chaos", label: "Chaos" }] },
  { title: "Sous-orbite (sens opposé)",
    menus: [GAP, { id: "o2shape", k: "menu", label: "Forme" }, { id: "o2dir", k: "menu", label: "Sens" }, GAP],
    knobs: [{ id: "o2ratio", label: "Vitesse ×", tip: "Vitesse de la sous-orbite par rapport à l'orbite (sens opposé possible)" },
      { id: "o2size", label: "Taille" }, { id: "o2phase", label: "Phase" }, GAP] },
  { title: "Orbe 3",
    menus: [GAP, { id: "o3shape", k: "menu", label: "Forme" }, { id: "o3dir", k: "menu", label: "Sens" }, GAP],
    knobs: [{ id: "o3ratio", label: "Vitesse ×" }, { id: "o3size", label: "Taille" }, { id: "o3phase", label: "Phase" }, GAP] },
];
const PHYS: Ctl[] = [
  { id: "orbphys", k: "vswitch", label: "Mode", short: ["Rigide", "Élastique", "Gravité"], tip: "Rigide : le point suit l'orbite ; Élastique : il est tiré par un ressort ; Gravité : il tombe et rebondit" },
  { id: "orbspring", k: "big", label: "Élasticité" }, { id: "orbdamp", label: "Amortissement" },
  { id: "orbbounce", label: "Rebond" }, { id: "orbretrig", k: "menu", label: "Départ", tip: "Où repart le point à chaque note" },
];

function Grid({ channel, module: m, ctls, cols, even }: { channel: number; module: ModuleDesc; ctls: Ctl[]; cols: number; even?: boolean }) {
  return (
    <div className={`fp-grid cg-grid${even ? " cg-grid--even" : ""}`} style={{ "--cols": cols } as React.CSSProperties}>
      {ctls.map((c, i) => c.id === "_" ? <div key={"_" + i} className="cg-gap" aria-hidden /> : <Control key={c.id} channel={channel} module={m} ctl={c} />)}
    </div>
  );
}

/** Page ORB : le pavé reste à gauche (en haut sur téléphone), les réglages sont dans des onglets à droite. */
export function CrocOrbPage({ channel, module: m }: { channel: number; module: ModuleDesc }) {
  const phone = useStore((s) => !!s.ui.phone);
  const [tab, setTab] = useState<TabId>("morph");
  return (
    <div className={`cg-orbpage${phone ? " cg-orbpage--phone" : ""}`}>
      <div className="cg-orbpage-pad"><CrocOrb channel={channel} module={m} compact={phone} /></div>
      <div className="cg-orbpage-side">
        <div className="cg-tabs" role="tablist" aria-label="Réglages de l'ORB">
          {TABS.map((t) => (
            <button key={t.id} type="button" role="tab" aria-selected={tab === t.id} className={tab === t.id ? "on" : ""} onClick={() => setTab(t.id)}>
              <span className="fc-led" aria-hidden />{t.label}
            </button>
          ))}
        </div>
        <div className="cg-tab-body" role="tabpanel">
          {tab === "morph" && (<>
            <Grid channel={channel} module={m} cols={4} ctls={[
              { id: "morphon", k: "toggle", label: "ORB", on: 1, tip: "Mélange des quatre coins par le point" },
              { id: "morphx", k: "big", label: "X" }, { id: "morphy", k: "big", label: "Y" },
              { id: "morphamt", label: "Profondeur", tip: "0 : le son en cours seul ; 100 : le mélange des coins en entier" },
            ]} />
            <CrocCorners channel={channel} module={m} />
          </>)}
          {tab === "orbit" && ORBIT_BLOCKS.map((bl) => (
            <section key={bl.title} className="cg-orbblock">
              <h4 className="cg-orbblock-title">{bl.title}</h4>
              <Grid channel={channel} module={m} cols={4} even ctls={bl.menus} />
              <Grid channel={channel} module={m} cols={4} even ctls={bl.knobs} />
            </section>
          ))}
          {tab === "phys" && <Grid channel={channel} module={m} cols={5} ctls={PHYS} />}
          {tab === "routes" && (
            <div className="cg-routes">
              <p className="cg-hint">ORB X, ORB Y et l'orbe 3 modulent directement le son, sans passer par la matrice.</p>
              {[1, 2, 3, 4, 5, 6].map((r) => (
                <div key={r} className="cg-route"><span className="cg-route-n">{r}</span>
                  <Grid channel={channel} module={m} cols={3} ctls={[
                    { id: `r${r}src`, k: "menu", label: "Source" }, { id: `r${r}dst`, k: "menu", label: "Destination" }, { id: `r${r}amt`, label: "Quantité" },
                  ]} />
                </div>
              ))}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

/* ---------- couloir de 32 pas ---------- */

/** Petite allure d'un LFO (un cycle), -1..1, pour la dessiner dans le pas. Les formes au hasard sont figées (dessin). */
function shapePoints(shape: number, n = 16): number[] {
  const h = (i: number) => { const x = Math.sin(i * 12.9898 + shape * 78.233) * 43758.5453; return (x - Math.floor(x)) * 2 - 1; };
  return Array.from({ length: n + 1 }, (_, i) => {
    const f = i / n;
    switch (shape) {
      case 0: return Math.sin(f * Math.PI * 2);
      case 1: return 1 - 4 * Math.abs(((f + 0.25) % 1) - 0.5);
      case 2: return 1 - 2 * f;
      case 3: return 2 * f - 1;
      case 4: return f < 0.5 ? 1 : -1;
      case 5: return h(Math.floor(f * 4));
      case 6: { const a = Math.floor(f * 3), t = f * 3 - a, s = t * t * (3 - 2 * t); return h(a) + (h(a + 1) - h(a)) * s; }
      case 7: return h(Math.floor(f * 6) * 3 + 1);
      case 8: { const a = Math.floor(f * 4), t = f * 4 - a; return h(a * 3 + 1) + (h(a * 3 + 4) - h(a * 3 + 1)) * t; }
      case 9: return h(i * 5 + 2);
      case 10: return (h(i * 5 + 2) + h(Math.floor(i / 2) * 7 + 3) + h(Math.floor(i / 4) * 11 + 5)) / 3;
      default: return 0;
    }
  });
}

/** Courbe de sortie du couloir, sur les dernières secondes. */
function LaneScope({ value, color }: { value: number; color: string }) {
  const cv = useRef<HTMLCanvasElement>(null);
  const hist = useRef<number[]>([]);
  useEffect(() => {
    const h = hist.current;
    h.push(Number.isFinite(value) ? Math.max(-1, Math.min(1, value)) : 0);
    if (h.length > 160) h.shift();
    const c = cv.current, g = c?.getContext("2d");
    if (!c || !g) return;
    const W = c.width, H = c.height;
    g.clearRect(0, 0, W, H);
    g.strokeStyle = "oklch(0.8 0.05 230 / 0.18)"; g.lineWidth = 1;
    g.beginPath(); g.moveTo(0, H / 2); g.lineTo(W, H / 2); g.stroke();
    g.strokeStyle = color; g.lineWidth = 1.6;
    g.beginPath();
    h.forEach((v, i) => { const x = W - (h.length - 1 - i) * (W / 159), y = H / 2 - v * (H / 2 - 2); if (i === 0) g.moveTo(x, y); else g.lineTo(x, y); });
    g.stroke();
  });
  return <canvas ref={cv} width={640} height={44} className="cg-scope" aria-label="Sortie du couloir" />;
}

/** Un couloir de 32 pas : motif (menu), source, LFO joué par chaque pas et son allure, niveau (glisser), sortie. */
export function CrocSteps({ channel, module: m, lane }: { channel: number; module: ModuleDesc; lane: string }) {
  const { get, set } = useInst(channel, m);
  const status = useStore((s) => (s.focus === channel ? s.tick.inst : undefined)) ?? [];
  const l = Math.max(1, Math.min(4, Number(lane) || 1));
  const c = `c${l}`;
  const len = Math.round(get(`${c}len`)), on = get(`${c}on`) > 0.5;
  const motif = Math.round(get(`${c}motif`)), single = Math.round(get(`${c}src`));
  const mt = motif > 0 ? CROC_MOTIFS[motif - 1] : undefined;
  const playing = status[5 + l] ?? -1;
  const drag = useRef(false);
  const stepOf = (i: number) => mt
    ? { lfo: mt.src[i % 16], lvl: mt.lvl[i % 16] }
    : { lfo: Math.round(get(`${c}l${i + 1}`)), lvl: get(`${c}v${i + 1}`) };
  // retoucher un motif : il est d'abord recopié dans les pas, puis le couloir repasse en « Perso »
  const toCustom = () => {
    if (!mt) return;
    for (let i = 0; i < STEPS; i++) { set(`${c}l${i + 1}`, mt.src[i % 16]); set(`${c}v${i + 1}`, mt.lvl[i % 16]); }
    set(`${c}motif`, 0);
  };
  const levelAt = (e: React.PointerEvent, i: number) => {
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    toCustom();
    set(`${c}v${i + 1}`, Math.round(Math.max(0, Math.min(1, 1 - (e.clientY - r.top) / r.height)) * 100));
  };
  const cycle = (i: number, d: number) => { const cur = stepOf(i).lfo; toCustom(); set(`${c}l${i + 1}`, (cur + d + 9) % 9); };
  const shapes = Array.from({ length: 8 }, (_, k) => Math.round(get(`l${k + 1}shape`)));
  const lfoName = (k: number) => (k >= FIXED ? "F" : `L${k + 1}`);
  const hue = (k: number) => (k >= FIXED ? 230 : 300 + k * 8);
  return (
    <div className={`cg-lane${on ? "" : " off"}`}>
      <div className="cg-lane-tools">
        <label className="cg-lane-motif" title="Motifs classiques (portes, euclidiens, escaliers, mouvements de LFO). Toucher un pas le recopie pour le retoucher.">
          <span>Motif</span>
          <select value={String(motif)} onChange={(e) => set(`${c}motif`, Number(e.target.value))} aria-label={`Couloir ${l} : motif`}>
            <option value="0">Perso (pas à la main)</option>
            {CROC_MOTIFS.map((x, j) => <option key={x.name} value={String(j + 1)}>{j + 1}. {x.name}</option>)}
          </select>
        </label>
        <button type="button" className="fc-toggle" disabled={!mt} onClick={toCustom} title="Recopier le motif dans les 32 pas pour le retoucher pas à pas">Copier dans les pas</button>
        <Control channel={channel} module={m} ctl={{ id: `${c}src`, k: "menu", label: "Source", tip: "« Séquence » : chaque pas joue son LFO ; « LFO n seul » : un seul LFO, sans les pas (le LFO Switch)" }} />
      </div>
      <div className={`cg-steps${single > 0 ? " single" : ""}${mt ? " motif" : ""}`}>
        {Array.from({ length: STEPS }, (_, i) => {
          const { lfo, lvl } = stepOf(i);
          const pts = shapePoints(lfo >= FIXED ? -1 : shapes[lfo] ?? 0);
          const mid = 100 - lvl / 2, amp = lvl / 2 - 2;
          const d = pts.map((v, k) => `${k === 0 ? "M" : "L"}${(k / (pts.length - 1)) * 10} ${mid - v * Math.max(0, amp)}`).join(" ");
          return (
            <div key={i} className={`cg-step${i >= len ? " out" : ""}${i === playing && on ? " now" : ""}`}>
              <button type="button" className="cg-step-lfo" onClick={() => cycle(i, 1)} onContextMenu={(e) => { e.preventDefault(); cycle(i, -1); }}
                title={`Pas ${i + 1} : ${lfo >= FIXED ? "fixe (le niveau seul)" : `LFO ${lfo + 1}`} — toucher : suivant, clic droit : précédent`}>{lfoName(lfo)}</button>
              <div className="cg-step-bar" onPointerDown={(e) => { e.preventDefault(); (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId); drag.current = true; levelAt(e, i); }}
                onPointerMove={(e) => { if (drag.current) levelAt(e, i); }} onPointerUp={() => { drag.current = false; }} onPointerCancel={() => { drag.current = false; }}
                title={`Pas ${i + 1} : niveau ${Math.round(lvl)} % (glisser)`}>
                <i style={{ height: `${lvl}%`, background: `oklch(0.55 0.16 ${hue(lfo)} / 0.55)` }} />
                <svg viewBox="0 0 10 100" preserveAspectRatio="none" aria-hidden>
                  <path d={d} fill="none" stroke={`oklch(0.88 0.14 ${hue(lfo)})`} strokeWidth={1.4} vectorEffect="non-scaling-stroke" />
                </svg>
              </div>
              <span className="cg-step-n">{i + 1}</span>
            </div>
          );
        })}
        {single > 0 && <div className="cg-steps-note">LFO {single} seul : les pas sont ignorés</div>}
      </div>
      <LaneScope value={status[10 + l] ?? 0} color={`oklch(0.85 0.15 ${200 + l * 30})`} />
    </div>
  );
}

/** Tables perso : un WAV pour la porteuse (emplacement 1) et un pour le modulateur (emplacement 2), pris dans
    la Bibliothèque de GOLEM (ou, sur PC, par le sélecteur du système). */
export function CrocWav({ channel }: { channel: number; module: ModuleDesc }) {
  const samples = useStore((s) => s.channels[channel]?.samples);
  const [lib, setLib] = useState<number | null>(null);
  return (
    <div className="cg-wav">
      {["Porteuse", "Modulateur"].map((n, i) => (
        <div key={n} className="cg-wav-row">
          <span title={samples?.[i] ?? undefined}>{n} · {samples?.[i] ?? "aucune table perso"}</span>
          <span className="cg-wav-btns">
            <button type="button" className={`fc-toggle${lib === i ? " on" : ""}`} onClick={() => setLib(lib === i ? null : i)}
              title="Prendre un WAV dans la Bibliothèque de GOLEM : des ondes de 2048 points à la suite (tables courantes), ou un seul cycle. Puis choisir la table « Perso (WAV) ».">Bibliothèque</button>
            {hasNative("chooseSample") && <button type="button" className="fc-toggle" onClick={() => actions.chooseSample(channel, i)} title="Choisir un fichier ailleurs sur l'ordinateur">Fichier…</button>}
          </span>
        </div>
      ))}
      {lib !== null && (
        <div className="cg-wav-lib">
          <FolderBrowser kind="samples" compact title={`Table perso · ${lib === 0 ? "porteuse" : "modulateur"}`}
            onPick={(rel) => { actions.loadSampleFile(channel, lib, rel); setLib(null); }} onClose={() => setLib(null)} />
        </div>
      )}
    </div>
  );
}
