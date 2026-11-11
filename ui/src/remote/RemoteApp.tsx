// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : TÉLÉCOMMANDE — reprise exacte des maquettes (« GOLEM — téléphone contrôleur ») : un cadre de
// 915 × 412 mis à l'échelle de l'écran de l'appareil (il remplit l'écran, quelle que soit sa taille).
// Le téléphone pilote GOLEM sur le PC (câble USB en mode MIDI, Wi-Fi) ou un appareil MIDI branché (FM-1,
// appareil Bluetooth MIDI). Écrans : Connexion, Mixage, Boutons, Chaos (pavé vectoriel), FM-1.
// On y entre depuis l'accueil ou les Réglages ; « ‹ GOLEM » (en haut à gauche) revient sans redémarrer.
// Le son du téléphone est en pause par défaut (le moteur ne calcule plus) ; « Garder le son » le laisse.
// Avec GOLEM (application) sur le PC, le PC renvoie son état : volumes, M, S, crossfader, boutons et ce
// qu'ils pilotent, pavé, lecture, noms, couleurs et niveaux des pistes ; le téléphone suit.

import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { paramAddress } from "../controls/bindings";
import { actions, getState, onRemoteIn, resolveGlobal, setUi, useStore } from "../engine/store";
import { formatPlain, toPlain, trackColor } from "../engine/params";
import * as M from "./midiLink";
import { orbitPoints } from "../engine/vecShapes";
import "./remote.css";

type Tab = "link" | "mix" | "btn" | "chaos" | "fm1";
type Via = "usb" | "wifi" | "bt" | "";
interface Corner { kind: "scene" | "params" | "cc"; scene: number; knobs: number[]; ccs: { ch: number; num: number }[]; vals: number[] }
/** Un des 16 boutons : bouton assignable de GOLEM (messages de GOLEM) ou CC vers n'importe quel appareil. */
interface BtnCfg { name: string; kind: "golem" | "cc"; ch: number; num: number }
/** Un des 8 réglages de l'onglet FM-1 / appareil : nom, CC, canal (0 = canal des notes, -1 = canal des
    effets, 1..16), ou repris d'un des 16 boutons (link ≥ 0). Vide = valeur du profil. */
interface FmKnobCfg { name?: string; cc?: number; ch?: number; link?: number }
/** Un des 8 pads : CC maintenu (gate), CC bascule (marche / arrêt) ou note. Canal 0 = canal des notes. */
interface PadCfg { name: string; mode: "gate" | "toggle" | "note"; ch: number; num: number }
interface RemoteSave {
  out: string; via: Via; keepSound: boolean;
  vol: number[]; mute: boolean[]; solo: boolean[]; master: number; xfade: number; mixPage: number;
  busVol: number[]; busMute: boolean[]; busSolo: boolean[];
  knobs: number[]; btnCfg: BtnCfg[]; fmCfg: Record<string, FmKnobCfg[]>; pads: PadCfg[];
  corners: Corner[]; x: number; y: number; orbit: number; orbitRate: number; orbitSize: number; spring: boolean; tempo: boolean;
  fmProfile: string; fmCh: number; fmFxCh: number; fmTrack: number; fmOct: number; fmProg: number; fmBank: number; fmTarget: number; fmVal: number[]; fmMod: number;
  profile?: string; // après la 1.1 : nom du profil personnalisé chargé ou enregistré
}

const KEY = "golem.remote2";
const DW = 915, DH = 412;
const C = { blue: "oklch(0.74 0.14 240)", mag: "oklch(0.72 0.2 330)", green: "oklch(0.8 0.18 150)", acc: "oklch(0.82 0.15 78)", line: "oklch(0.28 0.02 245)", fg: "oklch(0.95 0.008 245)", knob: "oklch(0.245 0.022 245)", muted: "oklch(0.62 0.02 245)" };
const CORNER_COLORS = [C.blue, C.mag, C.green, C.acc];
const DEF: RemoteSave = {
  out: "", via: "", keepSound: false,
  vol: Array(16).fill(0.75), mute: Array(16).fill(false), solo: Array(16).fill(false), master: 0.75, xfade: 0.5, mixPage: 0,
  busVol: Array(12).fill(0.75), busMute: Array(12).fill(false), busSolo: Array(12).fill(false),
  knobs: Array(16).fill(0.5),
  btnCfg: Array.from({ length: 16 }, (_, k) => ({ name: "", kind: "golem" as const, ch: 1, num: 20 + k })),
  fmCfg: {},
  pads: Array.from({ length: 8 }, (_, i) => ({ name: "", mode: "gate" as const, ch: 0, num: 80 + i })),
  corners: [
    { kind: "scene", scene: 1, knobs: [0, 1, 2, 3], ccs: [{ ch: 1, num: 74 }, { ch: 1, num: 91 }, { ch: 1, num: 11 }, { ch: 1, num: 1 }], vals: [0.5, 0.5, 0.5, 0.5] },
    { kind: "scene", scene: 5, knobs: [0, 1, 2, 3], ccs: [{ ch: 1, num: 74 }, { ch: 1, num: 91 }, { ch: 1, num: 11 }, { ch: 1, num: 1 }], vals: [0.5, 0.5, 0.5, 0.5] },
    { kind: "params", scene: 1, knobs: [0, 1, 2, 3], ccs: [{ ch: 1, num: 74 }, { ch: 1, num: 91 }, { ch: 1, num: 11 }, { ch: 1, num: 1 }], vals: [0.72, 0.4, 0.85, 0.3] },
    { kind: "cc", scene: 1, knobs: [0, 1, 2, 3], ccs: [{ ch: 1, num: 74 }, { ch: 1, num: 91 }, { ch: 1, num: 11 }, { ch: 1, num: 1 }], vals: [0.8, 0.5, 0.9, 0.2] },
  ],
  x: 0.5, y: 0.5, orbit: 1, orbitRate: 2, orbitSize: 0.35, spring: false, tempo: false,
  fmProfile: "v16", fmCh: 1, fmFxCh: 2, fmTrack: 0, fmOct: 4, fmProg: 0, fmBank: 0, fmTarget: 0, fmVal: Array(8).fill(0.5), fmMod: 0,
};
function load(): RemoteSave {
  try {
    const r = JSON.parse(localStorage.getItem(KEY) ?? "null");
    if (r && typeof r === "object") {
      const m = { ...DEF, ...r } as RemoteSave;
      if (!Array.isArray(m.btnCfg) || m.btnCfg.length !== 16) m.btnCfg = DEF.btnCfg;
      if (!Array.isArray(m.pads) || m.pads.length !== 8) m.pads = DEF.pads;
      if (!Array.isArray(m.busVol) || m.busVol.length !== 12) { m.busVol = DEF.busVol; m.busMute = DEF.busMute; m.busSolo = DEF.busSolo; }
      if (!m.fmCfg || typeof m.fmCfg !== "object") m.fmCfg = {};
      if (!(m.fmTarget >= 0 && m.fmTarget <= 4)) m.fmTarget = 0;
      if (m.fmBank > 4 || !(m.fmBank >= 0)) m.fmBank = 0; // 1.1 : l'ancienne « banque 1 à 4 = inserts » n'existe plus
      return m;
    }
  } catch { /* rien */ }
  return { ...DEF };
}
function save(s: RemoteSave) { try { localStorage.setItem(KEY, JSON.stringify(s)); } catch { /* rien */ } }

// --- après la 1.1 : profils personnalisés (« Profil Sub 37 », « Profil Electribe »…) ------------------
/** Ce qu'un profil garde : le paramétrage (boutons, pads, pavé Chaos, onglet appareil), pas le mixage. */
const PROFILE_KEYS = ["btnCfg", "knobs", "pads", "corners", "spring", "tempo", "fmCfg", "fmProfile", "fmCh", "fmFxCh", "fmOct", "fmBank", "fmProg", "fmTarget", "fmVal", "fmMod"] as const;
type Profile = Partial<Pick<RemoteSave, (typeof PROFILE_KEYS)[number]>>;
const PROFILES_KEY = "golem.remoteProfiles";
function loadProfiles(): Record<string, Profile> {
  try { const r = JSON.parse(localStorage.getItem(PROFILES_KEY) ?? "null"); return r && typeof r === "object" ? r : {}; } catch { return {}; }
}
function saveProfiles(p: Record<string, Profile>) { try { localStorage.setItem(PROFILES_KEY, JSON.stringify(p)); } catch { /* rien */ } }
function profileOf(st: RemoteSave): Profile {
  const o: Record<string, unknown> = {};
  for (const k of PROFILE_KEYS) o[k] = JSON.parse(JSON.stringify(st[k]));
  return o as Profile;
}

function ProfileSheet({ st, current, onLoad, onSaved, onClose }: {
  st: RemoteSave; current: string; onLoad: (name: string, p: Profile) => void; onSaved: (name: string) => void; onClose: () => void;
}) {
  const [all, setAll] = useState(loadProfiles);
  const [name, setName] = useState(current);
  const names = Object.keys(all).sort((a, b) => a.localeCompare(b, "fr"));
  const store = () => {
    const n = name.trim().slice(0, 32);
    if (!n) return;
    const next = { ...all, [n]: profileOf(st) };
    saveProfiles(next); setAll(next); onSaved(n);
  };
  const remove = (n: string) => { const next = { ...all }; delete next[n]; saveProfiles(next); setAll(next); };
  return (
    <div className="rc-sheet-bg" onPointerDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="rc-sheet rc-edit" role="dialog" aria-label="Profils">
        <div className="rc-sheet-h">Profils de la télécommande<button type="button" onClick={onClose} aria-label="Fermer">✕</button></div>
        <div className="rc-learn-row">
          <input className="rc-input" value={name} placeholder="Nom du profil (ex. Synthé basse)" maxLength={32} onChange={(e) => setName(e.target.value)} aria-label="Nom du profil" />
          <button type="button" className="rc-learn" onClick={store} disabled={!name.trim()}>{all[name.trim()] ? "Remplacer" : "Enregistrer"}</button>
        </div>
        <span className="rc-note">Garde les 16 boutons, les pads, le pavé Chaos et l'onglet de l'appareil (pas le mixage).</span>
        <div className="rc-list">
          {names.length === 0 && <span className="rc-note">Aucun profil enregistré.</span>}
          {names.map((n) => (
            <div key={n} className="rc-field">
              <button type="button" className={`rc-selbtn${n === current ? " on" : ""}`} onClick={() => { onLoad(n, all[n]); onClose(); }} title={`Charger « ${n} »`}>{n}</button>
              <button type="button" className="rc-learn" onClick={() => remove(n)} aria-label={`Supprimer le profil ${n}`}>Supprimer</button>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
const clamp01 = (v: number) => Math.max(0, Math.min(1, v));
/** Envoyer la valeur d'un des 16 boutons selon son réglage (GOLEM ou CC). */
function sendButton(cfg: BtnCfg | undefined, k: number, v: number) {
  if (cfg?.kind === "cc") M.cc(cfg.ch, cfg.num, v); else M.setTarget(M.T.knob(k), v);
}
/** Nom affiché d'un des 16 boutons : nom donné, sinon ce qu'il pilote sur le PC, sinon son CC. */
function buttonName(cfg: BtnCfg | undefined, k: number, lab?: { label: string; sub: string }) {
  if (cfg?.name) return cfg.name;
  if (cfg?.kind === "cc") return `CC ${cfg.num}`;
  return lab && lab.label !== "—" ? lab.label : `Bouton ${k + 1}`;
}
const BUS_NAMES = ["Retour A", "Retour B", "Retour C", "Retour D"];
/** Canal → index de bus (0..11 : retours A-D, puis groupes 1-7 aux canaux 21-27 ; le principal à part). */
const busIdx = (ch: number) => (ch < 20 ? ch - 16 : ch - 17);
/** Index du niveau renvoyé par le PC pour un canal (16 pistes, principal, retours, groupes). */
const levelIdx = (ch: number) => (ch < 16 ? ch : ch === 20 ? 16 : ch < 20 ? ch + 1 : ch);

/** Profils du FM-1 : 8 réglages. Les correspondances d'effets dépendent du firmware : le numéro de CC
    de chaque réglage se change d'un toucher sur « CC n ». */
interface FmProfile { id: string; name: string; knobs: { label: string; cc: number; fx?: boolean }[]; perTrack?: boolean }
const FM_PROFILES: FmProfile[] = [
  { id: "v16", name: "officiel V16", knobs: [{ label: "Volume", cc: 7 }, { label: "Expression", cc: 11 }, { label: "Modulation", cc: 1 }, { label: "Souffle", cc: 2 },
    { label: "Effet", cc: 70, fx: true }, { label: "Effet", cc: 71, fx: true }, { label: "Effet", cc: 72, fx: true }, { label: "Effet", cc: 73, fx: true }] },
  { id: "v15", name: "officiel V15", knobs: [{ label: "Modulation", cc: 1 }, { label: "Souffle", cc: 2 }, { label: "Réverb", cc: 7, fx: true }, { label: "Écho", cc: 11, fx: true },
    { label: "Effet", cc: 0, fx: true }, { label: "Effet", cc: 1, fx: true }, { label: "Effet", cc: 2, fx: true }, { label: "Effet", cc: 3, fx: true }] },
  { id: "open", name: "FM1 Open", perTrack: true, knobs: [{ label: "Volume", cc: 7 }, { label: "Panoramique", cc: 10 }, { label: "Expression", cc: 11 }, { label: "Coupure", cc: 74 },
    { label: "Réverb", cc: 91 }, { label: "Modulation", cc: 1 }, { label: "Souffle", cc: 2 }, { label: "CC 4", cc: 4 }] },
  { id: "free", name: "personnalisé", knobs: [20, 21, 22, 23, 24, 25, 26, 27].map((cc) => ({ label: `CC ${cc}`, cc })) },
];
const OPEN_CH = [1, 2, 3, 4, 5, 6, 7, 10];

export function RemoteApp() {
  const open = useStore((s) => s.ui.remote);
  if (!open) return null;
  return <Remote />;
}

/** Cadre de 915 × 412 à l'échelle de l'écran : il remplit tout l'écran (plus large ou plus haut si besoin). */
function useFrame() {
  const [f, setF] = useState({ scale: 1, w: DW, h: DH, vw: DW, vh: DH });
  useLayoutEffect(() => {
    const fit = () => {
      // clavier du téléphone ouvert (saisie d'un nom) : on ne recalcule pas (Device.tsx fait de même)
      const a = document.activeElement as HTMLElement | null;
      if (a && (a.tagName === "INPUT" || a.tagName === "TEXTAREA")) return;
      // même mesure que l'appareil (Device.tsx) : la plus petite des tailles annoncées, et sur Android
      // pas plus que l'écran (certaines vues web annoncent une fenêtre plus grande que la partie visible)
      const vv = window.visualViewport, de = document.documentElement;
      const pick = (...v: (number | undefined)[]) => Math.min(...v.filter((x): x is number => !!x && x > 0));
      let W = pick(window.innerWidth, vv?.width, de?.clientWidth), H = pick(window.innerHeight, vv?.height, de?.clientHeight);
      if (/Android/i.test(navigator.userAgent) && window.screen?.width > 0 && window.screen?.height > 0) {
        const big = Math.max(screen.width, screen.height), small = Math.min(screen.width, screen.height);
        W = Math.min(W, W >= H ? big : small); H = Math.min(H, W >= H ? small : big);
      }
      const portrait = H > W;
      const scale = portrait ? Math.min(W / 560, H / 760) : Math.min(W / DW, H / DH);
      setF({ scale, w: W / scale, h: H / scale, vw: W, vh: H });
    };
    fit();
    const refit = () => window.setTimeout(fit, 350); // après la fermeture du clavier
    document.addEventListener("focusout", refit);
    window.addEventListener("resize", fit);
    window.visualViewport?.addEventListener("resize", fit);
    return () => { document.removeEventListener("focusout", refit); window.removeEventListener("resize", fit); window.visualViewport?.removeEventListener("resize", fit); };
  }, []);
  return f;
}

interface PresetInfo { ch: number; focus: boolean; noteCh: number; ctrlOn: boolean; ctrlCh: number; slots: { prog: number; total: number; name: string; type: string }[] }
interface PcInfo { names: string[]; colors: number[]; knobLabels: { label: string; sub: string; color: number }[]; levels: number[]; presets: PresetInfo[] }

/** 1.1 : piste du PC dont le son change avec ce canal MIDI : seulement une piste réglée sur ce canal
    (comme dans un logiciel de musique). Sinon, la piste choisie reçoit les notes, et le changement de
    programme lance une scène (commandes MIDI actives sur ce canal) ou ne fait rien. */
function pcTrackFor(pc: PcInfo, ch: number): { track: number; scene: boolean; none: boolean } {
  const own = pc.presets.findIndex((p) => p && p.ch === ch);
  if (own >= 0) return { track: own, scene: false, none: false };
  const f = pc.presets.findIndex((p) => p && p.focus);
  if (f < 0) return { track: -1, scene: false, none: false };
  const fp = pc.presets[f];
  if (fp.ch !== 0 || (fp.noteCh !== 0 && fp.noteCh !== ch)) return { track: -1, scene: false, none: false };
  const scene = fp.ctrlOn && (fp.ctrlCh === 0 || fp.ctrlCh === ch);
  return { track: f, scene, none: !scene };
}
/** « 12 · Nom » (son d'usine) ou « Nom » (son à vous), vide sans préréglage. */
function presetText(p?: { prog: number; name: string }): string {
  if (!p?.name) return "";
  return p.prog >= 0 ? `${p.prog + 1} · ${p.name}` : p.name;
}
/** Noms des sons d'usine d'un module (même catalogue sur le téléphone et le PC). */
function factoryNames(type: string, effect: boolean): string[] {
  const c = getState().catalog;
  if (!c || !type) return [];
  return (effect ? c.effects : c.instruments).find((m) => m.type === type)?.presets ?? [];
}

function Remote() {
  const frame = useFrame();
  const [st, setSt] = useState<RemoteSave>(load);
  const [tab, setTab] = useState<Tab>("link");
  const [linkName, setLinkName] = useState("");
  const [pc, setPc] = useState<PcInfo>({ names: [], colors: [], knobLabels: [], levels: [], presets: [] });
  const [pcSeen, setPcSeen] = useState(0);
  const [playing, setPlaying] = useState(false);
  // 1.1 : pavé de GOLEM vu du PC : position jouée (avec le mouvement), rotation et hauteur de la forme
  const [vec, setVec] = useState<VecLive>({ px: -1, py: -1, rot: 0, height: 1 });
  const up = (p: Partial<RemoteSave>) => setSt((s) => { const n = { ...s, ...p }; save(n); return n; });
  const [profiles, setProfiles] = useState(false); // après la 1.1 : fenêtre des profils

  // retour du PC
  useEffect(() => onRemoteIn((bytes) => {
    const m = M.parseRemote(bytes);
    if (!m) return;
    if (m.kind === "learn") { M.feedLearn(m); return; } // 1.1 : « Apprendre »
    setPcSeen(Date.now());
    if (m.kind === "name") { setPc((p) => { const names = [...p.names], colors = [...p.colors]; names[m.track] = m.name; colors[m.track] = m.color; return { ...p, names, colors }; }); return; }
    if (m.kind === "knob") { setPc((p) => { const k = [...p.knobLabels]; k[m.knob] = { label: m.label, sub: m.sub, color: m.color }; return { ...p, knobLabels: k }; }); return; }
    if (m.kind === "levels") { setPc((p) => ({ ...p, levels: m.levels })); return; }
    if (m.kind === "preset") { setPc((p) => { const presets = [...p.presets]; presets[m.track] = { ch: m.ch, focus: m.focus, noteCh: m.noteCh, ctrlOn: m.ctrlOn, ctrlCh: m.ctrlCh, slots: m.slots }; if (m.focus) presets.forEach((x, i) => { if (x && i !== m.track) presets[i] = { ...x, focus: false }; }); return { ...p, presets }; }); return; }
    const S = getState().catalog?.layout.slotsPerChannel ?? 8192;
    setSt((s0) => {
      const s = { ...s0, vol: [...s0.vol], mute: [...s0.mute], solo: [...s0.solo], knobs: [...s0.knobs], corners: s0.corners.map((c) => ({ ...c })),
        busVol: [...s0.busVol], busMute: [...s0.busMute], busSolo: [...s0.busSolo] };
      for (const [t, v] of m.values) {
        if (M.recentlyTouched(t)) continue;
        if (t >= 0) {
          if (t % S !== 0) continue;
          const ch = t / S;
          if (ch < 16) s.vol[ch] = v; else if (ch === 20) s.master = v; else if (ch < 28) s.busVol[busIdx(ch)] = v;
        }
        else if (t <= -100 && t > -116) s.mute[-100 - t] = v >= 0.5;
        else if (t <= -200 && t > -216) s.solo[-200 - t] = v >= 0.5;
        else if (t <= -300 && t > -312) { const ch = 16 - (t + 300); if (ch !== 20) s.busMute[busIdx(ch)] = v >= 0.5; }
        else if (t <= -400 && t > -412) { const ch = 16 - (t + 400); if (ch >= 21) s.busSolo[busIdx(ch)] = v >= 0.5; }
        else if (t <= -500 && t > -516) s.knobs[-500 - t] = v;
        else if (t === M.T.xfade) s.xfade = v;
        else if (t === M.T.play) setPlaying(v >= 0.5);
        else if (t === M.T.host(M.HOST.vecX)) s.x = v; // seul GOLEM renvoie le pavé
        else if (t === M.T.host(M.HOST.vecY)) s.y = v;
        // 1.1 : le mouvement choisi sur le PC (ou ici) : forme, durée d'un tour, taille
        else if (t === M.T.host(M.HOST.vecOrbit)) { const o = Math.round(v * 8); if (o > 0) { s.orbit = o; s.tempo = true; } else s.tempo = false; }
        else if (t === M.T.host(M.HOST.vecRate)) s.orbitRate = Math.round(v * 8);
        else if (t === M.T.host(M.HOST.vecDepth)) s.orbitSize = v;
      }
      save(s);
      return s;
    });
    // 1.1 : position jouée, rotation (0..360°) et hauteur (0,05..2) de la forme
    let nv: Partial<VecLive> | null = null;
    for (const [t, v] of m.values) {
      if (t === M.VEC_SHOWN_X) (nv ??= {}).px = v;
      else if (t === M.VEC_SHOWN_Y) (nv ??= {}).py = v;
      else if (t === M.T.host(M.HOST.vecRot) && !M.recentlyTouched(t)) (nv ??= {}).rot = v * 360;
      else if (t === M.T.host(M.HOST.vecHeight) && !M.recentlyTouched(t)) (nv ??= {}).height = 0.05 + v * 1.95;
    }
    if (nv) { const p = nv; setVec((o) => ({ ...o, ...p })); }
  }), []);

  const connect = async (id: string, via: Via) => {
    const name = id.startsWith("wifi:") ? await actions.wifiOpen(id.slice(5)) : await actions.midiOpen(id);
    M.setLinked(!!name);
    setLinkName(name);
    up({ out: name ? id : "", via: name ? via : "" });
    setPcSeen(0);
    if (name) M.hello(true);
    return !!name;
  };
  useEffect(() => {
    if (!st.keepSound) { actions.stop(); actions.audioPause(true); }
    if (st.out) void connect(st.out, st.via);
    return () => { M.setLinked(false); actions.midiClose(); actions.audioPause(false); };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  // « bonjour » répété (10 fois au plus) tant que le PC n'a pas répondu, puis toutes les 20 s
  useEffect(() => {
    if (!linkName) return;
    let n = 0;
    const id = window.setInterval(() => { if (pcSeen) M.hello(false); else if (++n <= 10) M.hello(true); }, pcSeen ? 20000 : 2000);
    return () => window.clearInterval(id);
  }, [linkName, pcSeen > 0]);
  const pcOn = pcSeen > 0 && Date.now() - pcSeen < 30000;
  // relié à GOLEM (Wi-Fi, ou GOLEM sur le PC qui répond par le câble) : les boutons et le pavé pilotent GOLEM
  // (nom seulement, scènes) ; sinon un appareil MIDI (CC, canaux, pas de scènes)
  const golem = st.via === "wifi" || pcOn;
  const keepSound = (on: boolean) => { up({ keepSound: on }); if (on) actions.audioPause(false); else { actions.stop(); actions.audioPause(true); } };

  const pill = !linkName ? "Non reliée"
    : tab === "fm1" && st.via !== "wifi" ? `${linkName} · ${st.via === "bt" ? "Bluetooth" : "USB"}`
    : st.via === "wifi" ? `Wi-Fi · ${linkName.replace(/^GOLEM · /, "").replace(/ \(Wi-Fi\)$/, "")}${pcOn ? "" : " · …"}`
    : `${st.via === "bt" ? "Bluetooth" : "USB"} · ${pcOn ? "GOLEM sur le PC" : linkName}`;
  const tabs: [Tab, string][] = [["link", "Connexion"], ["mix", "Mixage"], ["btn", "Boutons"], ["chaos", "Chaos"], ["fm1", "FM-1"]];

  return (
    <div className="rc-stage" role="dialog" aria-modal="true" aria-label="Télécommande" style={{ width: frame.vw, height: frame.vh }}>
      <div className="rc-frame" style={{ width: frame.w, height: frame.h, transform: `scale(${frame.scale})` }}>
        <div className="rc-shell">
          <header className="rc-top">
            <button type="button" className="rc-logo" onClick={() => setUi({ remote: false })} title="Retour à GOLEM (le son du téléphone revient)" aria-label="Retour à GOLEM">
              <i>‹</i><span>GOLEM&nbsp;<b>TÉLÉCOMMANDE</b></span></button>
            {tabs.map(([k, l]) => <button key={k} type="button" className={`rc-tab${tab === k ? " on" : ""}`} aria-pressed={tab === k} onClick={() => setTab(k)}>{l}</button>)}
            <span className="rc-flex" />
            <button type="button" className="rc-tab rc-profile" onClick={() => setProfiles(true)} title="Profils : enregistrer ou rappeler le paramétrage (boutons, pads, pavé Chaos, appareil)">
              {st.profile ? `Profil · ${st.profile}` : "Profils"}</button>
            <span className={`rc-pill${linkName ? "" : " off"}`} title={linkName}><i />{pill}</span>
            <div className="rc-tr">
              <button type="button" aria-label="Lecture" className={`play${playing ? " on" : ""}`} onClick={() => M.pulse(M.T.play)}>
                <svg width="14" height="14" viewBox="0 0 14 14" aria-hidden="true"><path d="M3 1.5v11l9-5.5z" fill="oklch(0.165 0.018 245)" /></svg></button>
              <button type="button" aria-label="Arrêt" onClick={() => M.pulse(M.T.stop)}>
                <svg width="12" height="12" viewBox="0 0 12 12" aria-hidden="true"><rect x="1" y="1" width="10" height="10" rx="1.5" fill={C.fg} /></svg></button>
              <button type="button" aria-label="Enregistrer" className="rec" onClick={() => M.pulse(M.T.record)}><span /></button>
            </div>
          </header>
          {tab === "link" && <LinkTab st={st} linkName={linkName} onConnect={connect} onKeep={keepSound} />}
          {tab === "mix" && <MixTab st={st} up={up} pc={pc} />}
          {tab === "btn" && <BtnTab st={st} up={up} pc={pc} golem={golem} />}
          {tab === "chaos" && <ChaosTab st={st} up={up} pc={pc} golem={golem} vec={vec} />}
          {tab === "fm1" && <Fm1Tab st={st} up={up} pc={pc} golem={golem} />}
          {profiles && <ProfileSheet st={st} current={st.profile ?? ""} onClose={() => setProfiles(false)}
            onSaved={(n) => up({ profile: n })}
            onLoad={(n, p) => up({ ...p, profile: n })} />}
        </div>
      </div>
    </div>
  );
}

// --- gestes ------------------------------------------------------------------------------------
function drag(onMove: (ev: PointerEvent) => void, onEnd?: () => void) {
  return (e: React.PointerEvent) => {
    e.preventDefault();
    const el = e.currentTarget as HTMLElement;
    el.setPointerCapture(e.pointerId);
    onMove(e.nativeEvent);
    const move = (ev: PointerEvent) => onMove(ev);
    const end = () => { el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", end); el.removeEventListener("pointercancel", end); onEnd?.(); };
    el.addEventListener("pointermove", move); el.addEventListener("pointerup", end); el.addEventListener("pointercancel", end);
  };
}
/** Glisser relatif (boutons rotatifs) : haut / droite = plus. */
function relDrag(get: () => number, set: (v: number) => void, fine = false) {
  return (e: React.PointerEvent) => {
    e.preventDefault();
    const el = e.currentTarget as HTMLElement;
    el.setPointerCapture(e.pointerId);
    let lx = e.clientX, ly = e.clientY;
    const move = (ev: PointerEvent) => { const d = (ev.clientX - lx) - (ev.clientY - ly); lx = ev.clientX; ly = ev.clientY; set(clamp01(get() + d / (fine ? 600 : 160))); };
    const end = () => { el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", end); el.removeEventListener("pointercancel", end); };
    el.addEventListener("pointermove", move); el.addEventListener("pointerup", end); el.addEventListener("pointercancel", end);
  };
}

function KnobSvg({ v, size, color }: { v: number; size: number; color: string }) {
  const c = size / 2, r = size * 0.413, inner = size * 0.26, needle = size * 0.22;
  const pt = (deg: number, rad: number) => [c + rad * Math.cos((deg * Math.PI) / 180), c + rad * Math.sin((deg * Math.PI) / 180)];
  const [sx, sy] = pt(135, r), [ex, ey] = pt(45, r);
  const a = 135 + 270 * clamp01(v);
  const [vx, vy] = pt(a, r), [nx, ny] = pt(a, needle);
  const f = (n: number) => n.toFixed(1);
  return (
    <svg width={size} height={size} viewBox={`0 0 ${size} ${size}`} aria-hidden="true">
      <path d={`M ${f(sx)} ${f(sy)} A ${f(r)} ${f(r)} 0 1 1 ${f(ex)} ${f(ey)}`} fill="none" stroke={C.line} strokeWidth="4" strokeLinecap="round" />
      {v > 0.003 && <path d={`M ${f(sx)} ${f(sy)} A ${f(r)} ${f(r)} 0 ${270 * v > 180 ? 1 : 0} 1 ${f(vx)} ${f(vy)}`} fill="none" stroke={color} strokeWidth="4" strokeLinecap="round" />}
      <circle cx={c} cy={c} r={inner} fill={C.knob} />
      <line x1={c} y1={c} x2={f(nx)} y2={f(ny)} stroke={C.fg} strokeWidth="2.5" strokeLinecap="round" />
    </svg>
  );
}
const colorOf = (c: number | undefined) => (c === undefined || c >= 127 ? C.muted : trackColor(c));

// --- Connexion ---------------------------------------------------------------------------------
function LinkTab({ st, linkName, onConnect, onKeep }: { st: RemoteSave; linkName: string; onConnect: (id: string, via: Via) => Promise<boolean>; onKeep: (on: boolean) => void }) {
  const [sheet, setSheet] = useState<Via>("");
  const active: Via = linkName ? st.via : "";
  const cards: { via: Via; title: string; badge: string; best?: boolean; steps: string[] }[] = [
    { via: "usb", title: "Câble USB", badge: "le plus sûr", best: true, steps: ["Brancher le téléphone au PC", "Dans la notification USB : choisir « MIDI »", "Sur le PC : activer l'entrée MIDI du téléphone dans GOLEM"] },
    { via: "wifi", title: "Wi-Fi", badge: "sans fil · GOLEM sur le PC", steps: ["Téléphone et PC sur le même Wi-Fi", "Sur le PC : Réglages › « Accepter une télécommande Wi-Fi »", "Toucher son nom : c'est relié (aucun pilote)"] },
    { via: "bt", title: "Bluetooth", badge: "appareils MIDI sans fil", steps: ["Allumer l'appareil Bluetooth MIDI", "« Choisir » › Appairer, puis le toucher", "Pour GOLEM sur le PC : câble ou Wi-Fi"] },
  ];
  return (
    <div className="rc-body">
      <div className="rc-col">
        <div className="rc-title">
          <span>Piloter GOLEM sur le PC</span><span>ou un vrai FM-1 branché au téléphone (onglet FM-1)</span>
          <button type="button" className={`rc-sound${st.keepSound ? " on" : ""}`} onClick={() => onKeep(!st.keepSound)}
            title="Par défaut, le son du téléphone est en pause : il ne calcule plus rien et la commande reste fluide.">
            {st.keepSound ? "Son du téléphone : gardé" : "Son du téléphone : en pause"}</button>
        </div>
        <div className="rc-cards">
          {cards.map((c) => (
            <div key={c.via} className={`rc-card${active === c.via ? " on" : ""}`}>
              <div className="rc-card-h"><span>{c.title}</span><span className={`rc-badge${c.best ? " best" : ""}`}>{c.badge}</span></div>
              <ol>{c.steps.map((s) => <li key={s}>{s}</li>)}</ol>
              <span className="rc-flex" />
              <button type="button" className="rc-card-btn" onClick={() => setSheet(c.via)}>{active === c.via ? `Connecté · ${linkName.replace(/^GOLEM · /, "").replace(/ \(Wi-Fi\)$/, "")}` : "Choisir"}</button>
            </div>
          ))}
        </div>
      </div>
      {sheet && <ConnectSheet via={sheet} st={st} linkName={linkName} onClose={() => setSheet("")} onConnect={onConnect} />}
    </div>
  );
}

function ConnectSheet({ via, st, linkName, onClose, onConnect }: { via: Via; st: RemoteSave; linkName: string; onClose: () => void; onConnect: (id: string, via: Via) => Promise<boolean> }) {
  const [items, setItems] = useState<{ id: string; name: string }[]>([]);
  const [busy, setBusy] = useState(false);
  const [ip, setIp] = useState("");
  const search = async () => {
    setBusy(true);
    setItems(via === "wifi" ? await actions.wifiScan(ip) : await actions.midiOuts());
    setBusy(false);
  };
  useEffect(() => { void search(); /* eslint-disable-next-line react-hooks/exhaustive-deps */ }, []);
  const pick = async (id: string) => { if (await onConnect(id, via)) onClose(); };
  const title = via === "usb" ? "Câble USB · sortie MIDI" : via === "wifi" ? "Wi-Fi · GOLEM du réseau" : "Bluetooth MIDI";
  return (
    <div className="rc-sheet-bg" onPointerDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="rc-sheet" role="dialog" aria-label={title}>
        <div className="rc-sheet-h">{title}<button type="button" onClick={onClose} aria-label="Fermer">✕</button></div>
        <div className="rc-list">
          {items.length === 0 && <p className="rc-empty">{busy ? "Recherche…" : via === "wifi"
            ? "Aucun GOLEM trouvé. Sur le PC : Réglages › Contrôleur MIDI › « Accepter une télécommande Wi-Fi ». Sinon, tapez l'adresse affichée sur le PC."
            : via === "usb" ? "Aucune sortie. Branchez le câble et choisissez « MIDI » dans la notification USB du téléphone, puis « Chercher »."
            : "Aucun appareil. « Appairer » cherche les appareils Bluetooth MIDI allumés à proximité."}</p>}
          {items.map((o) => <button key={o.id} type="button" className={`rc-item${linkName && st.out === o.id ? " on" : ""}`} onClick={() => void pick(o.id)}>{o.name}</button>)}
        </div>
        {via === "wifi" && (
          <div className="rc-row">
            <input className="rc-input" inputMode="decimal" placeholder="Adresse du PC (ex. 192.168.1.20)" value={ip} aria-label="Adresse du PC"
              onChange={(e) => setIp(e.target.value.replace(/[^0-9.]/g, "").slice(0, 15))} />
            <button type="button" className="rc-btn acc" disabled={!/^\d{1,3}(\.\d{1,3}){3}$/.test(ip)} onClick={() => void pick(`wifi:${ip}`)}>Relier</button>
          </div>
        )}
        <div className="rc-row">
          <button type="button" className="rc-btn" onClick={() => void search()} disabled={busy}>Chercher</button>
          {via === "bt" && <button type="button" className="rc-btn" onClick={() => void actions.bluetoothMidi(true).then(search)}>Appairer…</button>}
          <span className="rc-flex" />
          {linkName && st.via === via && <button type="button" className="rc-btn" onClick={() => { void onConnect("", ""); onClose(); }}>Déconnecter</button>}
        </div>
      </div>
    </div>
  );
}

// --- Mixage ------------------------------------------------------------------------------------
function Fader({ v, onChange, fill, wide, label }: { v: number; onChange: (v: number) => void; fill: string; wide?: boolean; label: string }) {
  const ref = useRef<HTMLDivElement>(null);
  const at = (ev: PointerEvent) => { const r = ref.current!.getBoundingClientRect(); onChange(clamp01(1 - (ev.clientY - r.top - 6) / (r.height - 12))); };
  return (
    <div ref={ref} className={`rc-fader${wide ? " wide" : ""}`} onPointerDown={drag(at)} role="slider" aria-label={label} aria-valuenow={Math.round(v * 100)} aria-valuemin={0} aria-valuemax={100}>
      <div className="rc-fader-fill" style={{ height: `calc(${v * 100}% - 8px)`, background: fill }} />
      <div className="rc-fader-cap" style={{ bottom: `calc(${v * 100}% - 7px)` }} />
    </div>
  );
}
function dbText(global: number, v: number): string {
  const r = resolveGlobal(global);
  if (!r) return `${Math.round(v * 100)} %`;
  const plain = toPlain(r.desc, v);
  return plain <= r.desc.min + 1e-6 && r.desc.unit === "dB" ? "−∞ dB" : formatPlain(r.desc, plain);
}

function MixTab({ st, up, pc }: { st: RemoteSave; up: (p: Partial<RemoteSave>) => void; pc: PcInfo }) {
  const tracks = useStore((s) => s.tracks.tracks);
  const S = useStore((s) => s.catalog?.layout.slotsPerChannel ?? 8192);
  const scroller = useRef<HTMLDivElement>(null);
  const [view, setView] = useState<"tracks" | "groups">("tracks");
  const anySolo = st.solo.some(Boolean);
  const setVol = (t: number, v: number) => { const vol = [...st.vol]; vol[t] = v; up({ vol }); M.setTarget(paramAddress(t, "mix", 0, 0), v); };
  const tog = (key: "mute" | "solo", t: number) => {
    const arr = [...st[key]]; arr[t] = !arr[t]; up({ [key]: arr });
    M.setTarget(key === "mute" ? M.T.mute(t) : M.T.solo(t), arr[t] ? 1 : 0, true);
  };
  const setBusVol = (ch: number, v: number) => { const busVol = [...st.busVol]; busVol[busIdx(ch)] = v; up({ busVol }); M.setTarget(ch * S, v); };
  const togBus = (key: "busMute" | "busSolo", ch: number) => {
    const arr = [...st[key]]; const i = busIdx(ch); arr[i] = !arr[i]; up({ [key]: arr });
    M.setTarget((key === "busMute" ? -300 : -400) - (ch - 16), arr[i] ? 1 : 0, true);
  };
  // défilement : 16 pistes puis 4 retours et 7 groupes ; glisser de gauche à droite (pas sur un fader)
  const go = (v: "tracks" | "groups") => {
    const el = scroller.current;
    if (!el) return;
    const strip = el.children[v === "tracks" ? 0 : 16] as HTMLElement | undefined;
    el.scrollTo({ left: strip ? strip.offsetLeft - el.offsetLeft : 0, behavior: "smooth" });
  };
  const onScroll = () => {
    const el = scroller.current;
    if (!el) return;
    const g = el.children[16] as HTMLElement | undefined;
    setView(g && el.scrollLeft + el.clientWidth / 2 > g.offsetLeft - el.offsetLeft ? "groups" : "tracks");
  };
  const busList = [16, 17, 18, 19, 21, 22, 23, 24, 25, 26, 27];
  return (
    <div className="rc-body">
      <div ref={scroller} className="rc-strips rc-scroll" onScroll={onScroll}>
        {Array.from({ length: 16 }, (_, t) => t).map((t) => {
          const name = pc.names[t] ?? tracks[t]?.name ?? `Piste ${t + 1}`;
          const free = pc.colors[t] !== undefined ? pc.colors[t] >= 127 : !tracks[t]?.instrument;
          const col = trackColor(pc.colors[t] !== undefined && pc.colors[t] < 127 ? pc.colors[t] : tracks[t]?.color ?? t);
          const solo = st.solo[t];
          const dim = free || (anySolo && !solo);
          return (
            <div key={t} className={`rc-strip${solo ? " solo" : ""}${dim ? " dim" : ""}`} style={{ "--k": col } as React.CSSProperties}>
              <div className="rc-strip-bar" />
              <span className="rc-strip-name"><span>{t + 1}</span> {free ? "Libre" : name}</span>
              <span className="rc-strip-preset">{free ? "\u00a0" : presetText(pc.presets[t]?.slots[0]) || "\u00a0"}</span>
              <div className="rc-fz">
                <Fader v={st.vol[t]} onChange={(v) => setVol(t, v)} fill={solo ? col : "oklch(0.62 0.02 245)"} label={`Volume piste ${t + 1}`} />
                <div className="rc-meter"><i style={{ height: `${(pc.levels[t] ?? 0) * 100}%` }} /></div>
              </div>
              <span className="rc-db">{free ? "— dB" : dbText(t * S, st.vol[t])}</span>
              <div className="rc-ms">
                <button type="button" className={`m${st.mute[t] ? " on" : ""}`} onClick={() => tog("mute", t)} aria-pressed={st.mute[t]}>M</button>
                <button type="button" className={`s${solo ? " on" : ""}`} onClick={() => tog("solo", t)} aria-pressed={solo}>S</button>
              </div>
            </div>
          );
        })}
        {busList.map((ch) => {
          const i = busIdx(ch), group = ch >= 21;
          const name = group ? (pc.names[ch] || `Groupe ${ch - 20}`) : BUS_NAMES[ch - 16];
          const col = group ? C.mag : C.blue;
          return (
            <div key={ch} className={`rc-strip rc-strip--bus${st.busSolo[i] ? " solo" : ""}`} style={{ "--k": col } as React.CSSProperties}>
              <div className="rc-strip-bar" />
              <span className="rc-strip-name">{name}</span>
              <div className="rc-fz">
                <Fader v={st.busVol[i]} onChange={(v) => setBusVol(ch, v)} fill={st.busSolo[i] ? col : "oklch(0.62 0.02 245)"} label={`Volume ${name}`} />
                <div className="rc-meter"><i style={{ height: `${(pc.levels[levelIdx(ch)] ?? 0) * 100}%` }} /></div>
              </div>
              <span className="rc-db">{dbText(ch * S, st.busVol[i])}</span>
              <div className="rc-ms">
                <button type="button" className={`m${st.busMute[i] ? " on" : ""}`} onClick={() => togBus("busMute", ch)} aria-pressed={st.busMute[i]}>M</button>
                {group ? <button type="button" className={`s${st.busSolo[i] ? " on" : ""}`} onClick={() => togBus("busSolo", ch)} aria-pressed={st.busSolo[i]}>S</button>
                  : <button type="button" className="s" disabled style={{ opacity: 0.3 }}>S</button>}
              </div>
            </div>
          );
        })}
      </div>
      <div className="rc-side">
        <div className="rc-pages">
          <button type="button" className={view === "tracks" ? "on" : ""} onClick={() => go("tracks")}>1–16</button>
          <button type="button" className={view === "groups" ? "on" : ""} onClick={() => go("groups")}>Groupes</button>
        </div>
        <div className="rc-master">
          <span className="rc-caption">Principal</span>
          <div className="rc-fz">
            <Fader v={st.master} wide fill={C.acc} label="Volume principal" onChange={(v) => { up({ master: v }); M.setTarget(20 * S, v); }} />
            <div className="rc-meter"><i style={{ height: `${(pc.levels[16] ?? 0) * 100}%` }} /></div>
            <div className="rc-meter"><i style={{ height: `${(pc.levels[16] ?? 0) * 92}%` }} /></div>
          </div>
          <span className="rc-db">{dbText(20 * S, st.master)}</span>
        </div>
        <span className="rc-note">Glisser pour passer des pistes aux retours et groupes. Noms, couleurs et niveaux viennent du PC.</span>
      </div>
    </div>
  );
}

// --- Réglage d'un bouton, d'un réglage ou d'un pad (fenêtre dans le cadre) ------------------------
/** 1.1 : « Apprendre » : petit bouton à côté d'un réglage de CC : on le touche, on tourne le bouton (ou on
    joue la note) sur l'appareil, et le numéro et le canal se posent tout seuls. Retoucher : annuler. */
export function LearnChip({ accept, onLearn }: { accept: "cc" | "note" | "any"; onLearn: (e: M.LearnEvent) => void }) {
  const [, force] = useState(0);
  const mine = useRef<M.LearnFn | null>(null);
  const cb = useRef(onLearn);
  cb.current = onLearn;
  useEffect(() => M.onLearnChange(() => force((n) => n + 1)), []);
  useEffect(() => () => { if (mine.current && M.learnArmed() === mine.current) M.learnArm(null); }, []);
  const armed = mine.current !== null && M.learnArmed() === mine.current;
  const toggle = () => {
    if (armed) { mine.current = null; M.learnArm(null); return; }
    const fn: M.LearnFn = (e) => {
      if (accept !== "any" && e.type !== accept) return false;
      mine.current = null;
      cb.current(e);
      return true;
    };
    mine.current = fn;
    M.learnArm(fn);
  };
  return (
    <button type="button" className={`rc-learn${armed ? " on" : ""}`} onClick={toggle} aria-pressed={armed}
      title={armed ? "Annuler l'apprentissage" : accept === "note" ? "Apprendre : jouez la note sur l'appareil" : "Apprendre : tournez le bouton de l'appareil, son numéro de CC et son canal se posent tout seuls"}>
      <span className="rc-learn-dot" aria-hidden />{armed ? (accept === "note" ? "Jouez une note…" : accept === "any" ? "Tournez ou jouez…" : "Tournez un bouton…") : "Apprendre"}
    </button>
  );
}

type Field =
  | { kind: "text"; key: string; label: string; value: string; placeholder?: string }
  | { kind: "seg"; key: string; label: string; value: string; options: [string, string][] }
  | { kind: "select"; key: string; label: string; value: number; options: [number, string][] };
function EditSheet({ title, fields, onSave, onClose, onReset, learn }: {
  title: string; fields: Field[]; onSave: (v: Record<string, string | number>) => void; onClose: () => void; onReset?: () => void;
  /** 1.1 : « Apprendre » : champs du numéro et du canal ; mode (pads) : passe en note ou en CC selon ce qui arrive. */
  learn?: { num: string; ch: string; accept: "cc" | "note" | "any"; mode?: string; extra?: Record<string, string | number> };
}) {
  const [v, setV] = useState<Record<string, string | number>>(() => Object.fromEntries(fields.map((f) => [f.key, f.value])));
  const set = (k: string, x: string | number) => setV((o) => ({ ...o, [k]: x }));
  return (
    <div className="rc-sheet-bg" onPointerDown={(e) => { if (e.target === e.currentTarget) onClose(); }}>
      <div className="rc-sheet rc-edit" role="dialog" aria-label={title}>
        <div className="rc-sheet-h">{title}<button type="button" onClick={onClose} aria-label="Fermer">✕</button></div>
        {learn && (
          <div className="rc-learn-row">
            <LearnChip accept={learn.accept} onLearn={(e) => setV((o) => {
              const n = { ...o, ...learn.extra, [learn.num]: e.num, [learn.ch]: e.ch };
              if (learn.mode) n[learn.mode] = e.type === "note" ? "note" : o[learn.mode] === "note" ? "gate" : o[learn.mode];
              return n;
            })} />
            <span className="rc-note">Touchez, puis tournez le bouton de l'appareil.</span>
          </div>
        )}
        <div className="rc-list">
          {fields.map((f) => {
            if (f.kind === "text") return (
              <label key={f.key} className="rc-field"><span>{f.label}</span>
                <input className="rc-input" value={String(v[f.key] ?? "")} placeholder={f.placeholder} maxLength={18} onChange={(e) => set(f.key, e.target.value)} /></label>);
            if (f.kind === "seg") return (
              <div key={f.key} className="rc-field"><span>{f.label}</span>
                <div className="rc-seg">{f.options.map(([val, lab]) => <button key={val} type="button" className={v[f.key] === val ? "on" : ""} onClick={() => set(f.key, val)}>{lab}</button>)}</div></div>);
            return (
              <label key={f.key} className="rc-field"><span>{f.label}</span>
                <span className="rc-selbtn">{f.options.find(([n]) => n === Number(v[f.key]))?.[1] ?? v[f.key]} ▾
                  <select value={Number(v[f.key])} onChange={(e) => set(f.key, Number(e.target.value))}>
                    {f.options.map(([n, lab]) => <option key={n} value={n}>{lab}</option>)}
                  </select></span></label>);
          })}
        </div>
        <div className="rc-row">
          {onReset && <button type="button" className="rc-btn" onClick={() => { onReset(); onClose(); }}>Par défaut</button>}
          <span className="rc-flex" />
          <button type="button" className="rc-btn" onClick={onClose}>Annuler</button>
          <button type="button" className="rc-btn acc" onClick={() => { onSave(v); onClose(); }}>Valider</button>
        </div>
      </div>
    </div>
  );
}
const CHANNELS: [number, string][] = Array.from({ length: 16 }, (_, i) => [i + 1, `Canal ${i + 1}`]);
const CC_NUMS: [number, string][] = Array.from({ length: 128 }, (_, n) => [n, `CC ${n}`]);
const NOTE_NUMS: [number, string][] = Array.from({ length: 128 }, (_, n) => [n, `${["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"][n % 12]}${Math.floor(n / 12) - 1} (${n})`]);

// --- Boutons -----------------------------------------------------------------------------------
function BtnTab({ st, up, pc, golem }: { st: RemoteSave; up: (p: Partial<RemoteSave>) => void; pc: PcInfo; golem: boolean }) {
  const [fill, setFill] = useState(false), [brk, setBrk] = useState(false), [fine, setFine] = useState(false);
  const [edit, setEdit] = useState(false), [editing, setEditing] = useState(-1);
  const ref = useRef(st); ref.current = st;
  // relié à GOLEM : toujours le bouton de GOLEM (le réglage CC ne sert qu'avec un appareil MIDI)
  const cfgOf = (k: number): BtnCfg => (golem ? { ...ref.current.btnCfg[k], kind: "golem" } : ref.current.btnCfg[k]);
  const setKnob = (k: number, v: number) => { const knobs = [...ref.current.knobs]; knobs[k] = v; up({ knobs }); sendButton(cfgOf(k), k, v); };
  const xf = useRef<HTMLDivElement>(null);
  const hold = (on: boolean, which: "fill" | "brk") => {
    if (which === "fill") { setFill(on); M.setTarget(M.T.fill, on ? 1 : 0, true); } else { setBrk(on); M.setTarget(M.T.breakHold, on ? 1 : 0, true); }
  };
  const holdProps = (which: "fill" | "brk") => ({
    onPointerDown: (e: React.PointerEvent) => { e.preventDefault(); (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId); hold(true, which); },
    onPointerUp: () => hold(false, which), onPointerCancel: () => hold(false, which),
  });
  const cfgE = editing >= 0 ? st.btnCfg[editing] : null;
  return (
    <div className="rc-body">
      <div className="rc-col" style={{ gap: 8 }}>
        <div className={`rc-knobs${edit ? " editing" : ""}`}>
          {st.knobs.map((v, k) => {
            const cfg = golem ? { ...st.btnCfg[k], kind: "golem" as const } : st.btnCfg[k];
            const lab = cfg.kind === "golem" ? pc.knobLabels[k] : undefined;
            const free = cfg.kind === "golem" && (!lab || lab.label === "—");
            const col = cfg.kind === "cc" ? C.blue : colorOf(lab?.color);
            const sub = cfg.kind === "cc" ? `CC ${cfg.num} · canal ${cfg.ch}` : lab ? (cfg.name ? lab.label + " · " + lab.sub : lab.sub) : "bouton de GOLEM";
            return (
              <button key={k} type="button" className={`rc-kcell${free && lab ? " free" : ""}`} aria-label={`Bouton ${k + 1} : ${buttonName(cfg, k, lab)}`}
                onPointerDown={edit ? undefined : relDrag(() => ref.current.knobs[k], (x) => setKnob(k, x), fine)} onClick={edit ? () => setEditing(k) : undefined}>
                <KnobSvg v={v} size={46} color={(lab && !free) || cfg.kind === "cc" ? col : C.acc} />
                <span className="rc-kname">{buttonName(cfg, k, lab)}</span>
                <span className="rc-ksub" style={{ color: (lab && !free) || cfg.kind === "cc" ? col : undefined }}>{sub}</span>
              </button>
            );
          })}
        </div>
        <div className="rc-bottom">
          {/* 1.1 : crossfader à gauche, loin du bord droit (barre de navigation d'Android en paysage) */}
          <div className="rc-xf rc-xf--left">
            <b style={{ color: C.blue }}>A</b>
            <div ref={xf} className="rc-xf-track" role="slider" aria-label="Crossfader" aria-valuenow={Math.round(st.xfade * 100)}
              onPointerDown={drag((ev) => { const r = xf.current!.getBoundingClientRect(); const v = clamp01((ev.clientX - r.left) / r.width); up({ xfade: v }); M.setTarget(M.T.xfade, v); })}
              onDoubleClick={() => { up({ xfade: 0.5 }); M.setTarget(M.T.xfade, 0.5, true); }}>
              <div className="rc-xf-cap" style={{ left: `${st.xfade * 100}%` }} />
            </div>
            <b style={{ color: C.mag }}>B</b>
          </div>
          <button type="button" className={fill ? "on" : ""} {...holdProps("fill")}>Fill</button>
          <button type="button" className={brk ? "on" : ""} {...holdProps("brk")}>Break</button>
          <button type="button" className={fine ? "on" : ""} onClick={() => setFine(!fine)} title="Maj : réglage fin des boutons">Maj</button>
          <button type="button" className={edit ? "on" : ""} onClick={() => setEdit(!edit)} title={golem ? "Toucher un bouton pour le renommer (il pilote le bouton de GOLEM)" : "Toucher un bouton pour le renommer et choisir son CC et son canal"}>{edit ? "Régler ✓" : "Régler"}</button>
        </div>
      </div>
      {cfgE && (
        <EditSheet title={`Bouton ${editing + 1}`} onClose={() => setEditing(-1)} learn={golem ? undefined : { num: "num", ch: "ch", accept: "cc" }}
          onReset={() => { const btnCfg = [...st.btnCfg]; btnCfg[editing] = DEF.btnCfg[editing]; up({ btnCfg }); }}
          fields={golem ? [
            { kind: "text", key: "name", label: "Nom", value: cfgE.name, placeholder: buttonName({ ...cfgE, kind: "golem", name: "" }, editing, pc.knobLabels[editing]) },
          ] : [
            { kind: "text", key: "name", label: "Nom", value: cfgE.name, placeholder: buttonName({ ...cfgE, name: "" }, editing) },
            { kind: "select", key: "ch", label: "Canal", value: cfgE.ch, options: CHANNELS },
            { kind: "select", key: "num", label: "Numéro de CC", value: cfgE.num, options: CC_NUMS },
          ]}
          onSave={(v) => {
            const btnCfg = [...st.btnCfg];
            btnCfg[editing] = golem ? { ...cfgE, name: String(v.name).trim() } : { name: String(v.name).trim(), kind: "cc", ch: Number(v.ch), num: Number(v.num) };
            up({ btnCfg });
          }} />
      )}
    </div>
  );
}

// --- Chaos (pavé vectoriel) --------------------------------------------------------------------
const ORBITS = ["Arrêt", "Cercle", "Infini", "Losange", "Carré", "Triangle", "Ricochet", "Hasard"];
const RATES = ["1/2", "1", "2", "4", "8", "16", "32", "64", "128"];
/** 1.1 : pavé de GOLEM vu du PC (retour) : position jouée (-1 : pas encore reçue), rotation, hauteur. */
interface VecLive { px: number; py: number; rot: number; height: number }
function ChaosTab({ st, up, pc, golem, vec }: { st: RemoteSave; up: (p: Partial<RemoteSave>) => void; pc: PcInfo; golem: boolean; vec: VecLive }) {
  // relié à GOLEM : les coins ne sont que des scènes ; avec un appareil MIDI : boutons ou CC, pas de scènes
  const eff = (c: Corner): Corner => ({ ...c, kind: golem ? "scene" : c.kind === "scene" ? "cc" : c.kind });
  const golemRef = useRef(golem); golemRef.current = golem;
  const effAll = (cs: Corner[]) => cs.map((c) => ({ ...c, kind: golemRef.current ? "scene" as const : c.kind === "scene" ? "cc" as const : c.kind }));
  const pad = useRef<HTMLDivElement>(null);
  const [sel, setSel] = useState(2);
  const [side, setSide] = useState<"corner" | "orbit">("corner");
  const ref = useRef(st); ref.current = st;
  const base = useRef(new Map<string, number>());
  const trail = useRef<{ x: number; y: number }[]>([]);
  const [, redraw] = useState(0);
  const hasScene = golem;
  const knobName = (k: number) => {
    const cfg = st.btnCfg[k], l = cfg.kind === "golem" ? pc.knobLabels[k] : undefined;
    const n = buttonName(cfg, k, l);
    return l && l.label !== "—" && !cfg.name ? `${n} · ${l.sub.replace(/^P(\d+) .*/, "P$1")}` : n;
  };

  // coins « scène » : le pavé de GOLEM sur le PC (un coin qui n'est pas une scène prend la première scène)
  const sendScenes = (raw = ref.current.corners) => {
    const corners = effAll(raw);
    const first = corners.find((c) => c.kind === "scene");
    if (!first) { M.setTarget(M.T.host(M.HOST.vecOn), 0, true); return; }
    M.setTarget(M.T.host(M.HOST.vecMode), 0);
    corners.forEach((c, k) => M.setTarget(M.T.host(M.HOST.vecSA + k), ((c.kind === "scene" ? c.scene : first.scene) - 1) / (M.NUM_SCENES - 1)));
    M.setTarget(M.T.host(M.HOST.vecOn), 1, true);
  };
  const destKey = (c: Corner, i: number) => (c.kind === "params" ? `k${c.knobs[i]}` : `c${c.ccs[i].ch}:${c.ccs[i].num}`);
  const captureBase = () => {
    base.current.clear();
    for (const c of effAll(ref.current.corners)) for (let i = 0; i < 4; i++) {
      if (c.kind === "params") base.current.set(destKey(c, i), ref.current.knobs[c.knobs[i]] ?? 0.5);
      else if (c.kind === "cc") base.current.set(destKey(c, i), M.ccValue(c.ccs[i].ch, c.ccs[i].num));
    }
  };
  const place = (x: number, y: number) => {
    up({ x, y });
    trail.current = [...trail.current.slice(-5), { x, y }];
    const s = ref.current;
    if (golemRef.current) { M.setTarget(M.T.host(M.HOST.vecX), x); M.setTarget(M.T.host(M.HOST.vecY), y); }
    const w = [(1 - x) * (1 - y), x * (1 - y), (1 - x) * y, x * y];
    const acc = new Map<string, { sum: number; wsum: number; send: (v: number) => void }>();
    effAll(s.corners).forEach((c, k) => {
      if (c.kind === "scene") return;
      for (let i = 0; i < 4; i++) {
        const key = destKey(c, i);
        const e = acc.get(key) ?? { sum: 0, wsum: 0, send: c.kind === "params" ? ((kn: number) => (v: number) => sendButton(ref.current.btnCfg[kn], kn, v))(c.knobs[i]) : ((cc: { ch: number; num: number }) => (v: number) => M.cc(cc.ch, cc.num, v))(c.ccs[i]) };
        e.sum += w[k] * (c.vals[i] ?? 0); e.wsum += w[k];
        acc.set(key, e);
      }
    });
    for (const [key, e] of acc) e.send(clamp01(e.sum + (1 - e.wsum) * (base.current.get(key) ?? 0.5)));
  };
  const at = (ev: PointerEvent) => { const r = pad.current!.getBoundingClientRect(); place(clamp01((ev.clientX - r.left) / r.width), clamp01((ev.clientY - r.top) / r.height)); };
  const springBack = () => {
    if (!ref.current.spring) { trail.current = []; redraw((n) => n + 1); return; }
    const x0 = ref.current.x, y0 = ref.current.y, t0 = performance.now();
    const step = () => {
      const k = Math.min(1, (performance.now() - t0) / 260), e = 1 - Math.pow(1 - k, 3);
      place(x0 + (0.5 - x0) * e, y0 + (0.5 - y0) * e);
      if (k < 1) requestAnimationFrame(step); else { trail.current = []; redraw((n) => n + 1); }
    };
    requestAnimationFrame(step);
  };
  const onPad = drag(at, springBack);
  const onPadDown = (e: React.PointerEvent) => { captureBase(); if (golemRef.current) sendScenes(); onPad(e); };

  const setCorner = (k: number, p: Partial<Corner>) => {
    const corners = st.corners.map((c, i) => (i === k ? { ...c, ...p } : c));
    up({ corners });
    if (p.kind !== undefined || p.scene !== undefined) sendScenes(corners);
  };
  const cur = eff(st.corners[sel]), col = CORNER_COLORS[sel];
  const cornerTitle = (c: Corner, k: number) => `${"ABCD"[k]} · ${c.kind === "scene" ? `Scène ${c.scene}` : c.kind === "params" ? "4 boutons" : "4 CC"}`;
  const cornerSub = (c: Corner) => (c.kind === "scene" ? "scène de GOLEM" : c.kind === "params" ? c.knobs.map((k) => knobName(k).split(" · ")[0]).join(" · ") : c.ccs.map((x) => `CC ${x.num}`).join(" · "));
  const valText = (c: Corner, i: number) => (c.kind === "cc" ? String(Math.round((c.vals[i] ?? 0) * 127)) : `${Math.round((c.vals[i] ?? 0) * 100)} %`);
  const take = () => {
    if (cur.kind === "params") setCorner(sel, { vals: cur.knobs.map((k) => st.knobs[k] ?? 0.5) });
    else if (cur.kind === "cc") setCorner(sel, { vals: cur.ccs.map((x) => M.ccValue(x.ch, x.num)) });
  };
  const barRefs = useRef<(HTMLSpanElement | null)[]>([]);
  // 1.1 : la forme du mouvement, à sa place et à sa taille, comme sur le PC ; l'orbe vert suit la
  // position jouée que renvoie le PC (toutes les 40 ms, glissement entre deux)
  const orbitOn = st.tempo && hasScene && st.orbit > 0;
  const shape = orbitOn && st.orbit !== 8 ? orbitPoints(st.orbit, st.x, st.y, st.orbitSize, vec.rot, vec.height) : "";
  const live = golem && vec.px >= 0 && vec.py >= 0;

  return (
    <div className="rc-body">
      <div ref={pad} className="rc-pad" onPointerDown={onPadDown} role="slider" aria-label="Pavé vectoriel" aria-valuetext={`${Math.round(st.x * 100)} / ${Math.round(st.y * 100)}`}>
        {[25, 50, 75].map((p) => <span key={`v${p}`} className="rc-grid-v" style={{ left: `${p}%` }} />)}
        {[25, 50, 75].map((p) => <span key={`h${p}`} className="rc-grid-h" style={{ top: `${p}%` }} />)}
        {shape && (
          <svg className="rc-orbit" viewBox="0 0 100 100" preserveAspectRatio="none" aria-hidden>
            <polygon points={shape} fill="none" vectorEffect="non-scaling-stroke" />
          </svg>
        )}
        {trail.current.slice(0, -1).map((p, i, a) => {
          const sz = 10 + i * 2;
          return <span key={i} className="rc-trail" style={{ left: `calc(${p.x * 100}% - ${sz / 2}px)`, top: `calc(${p.y * 100}% - ${sz / 2}px)`, width: sz, height: sz, opacity: 0.18 + (0.4 * i) / Math.max(1, a.length) }} />;
        })}
        <span className="rc-cross-h" /><span className="rc-cross-v" />
        <span className="rc-orb" style={{ left: `${st.x * 100}%`, top: `${st.y * 100}%` }} />
        {live && orbitOn && <span className="rc-play" style={{ left: `${vec.px * 100}%`, top: `${vec.py * 100}%` }} aria-hidden />}
        {st.corners.map(eff).map((c, k) => (
          <button key={k} type="button" className={`rc-corner ${"abcd"[k]}${sel === k ? " sel" : ""}`} style={{ "--c": CORNER_COLORS[k] } as React.CSSProperties}
            onPointerDown={(e) => e.stopPropagation()} onClick={() => { setSel(k); setSide("corner"); }} aria-label={`Coin ${cornerTitle(c, k)}`}>
            <b>{cornerTitle(c, k)}</b><span>{cornerSub(c)}</span>
          </button>
        ))}
        <span className="rc-padhint">{golem ? "Bleu : l'orbe · vert : l'orbite autour du centre (+)" : "L'orbe mélange les valeurs des 4 coins"}</span>
      </div>
      <div className="rc-cside">
        <span className="rc-caption">Chaos · pavé vectoriel</span>
        <div className="rc-seg">
          <button type="button" className={side === "corner" ? "on" : ""} onClick={() => setSide("corner")}>Coin {"ABCD"[sel]}</button>
          {golem && <button type="button" className={side === "orbit" ? "on" : ""} onClick={() => setSide("orbit")}>Orbite</button>}
        </div>
        {side === "corner" || !golem ? (
          <div className="rc-cbox" style={{ "--c": col } as React.CSSProperties}>
            <div className="rc-kinds">
              {(golem ? (["scene"] as const) : (["params", "cc"] as const)).map((k) => (
                <button key={k} type="button" className={cur.kind === k ? "on" : ""} onClick={() => { if (k !== "scene") setCorner(sel, { kind: k }); }}>{k === "scene" ? "Scène de GOLEM" : k === "params" ? "4 boutons" : "4 CC"}</button>
              ))}
            </div>
            {cur.kind === "scene" ? (
              <>
                <div className="rc-scenes">
                  {Array.from({ length: 16 }, (_, i) => i + 1).map((n) => <button key={n} type="button" className={cur.scene === n ? "on" : ""} onClick={() => setCorner(sel, { scene: n })}>{n}</button>)}
                </div>
                <span className="rc-note">Les scènes de GOLEM sur le PC : le pavé de GOLEM mélange les coins.</span>
              </>
            ) : (
              <>
                {[0, 1, 2, 3].map((i) => (
                  <div key={i} className="rc-prow">
                    <span className="rc-plabel">
                      {cur.kind === "params" ? knobName(cur.knobs[i]) : `CC ${cur.ccs[i].num} · canal ${cur.ccs[i].ch}`}
                      {cur.kind === "params" ? (
                        <select value={cur.knobs[i]} aria-label="Bouton assignable" onChange={(e) => { const knobs = [...cur.knobs]; knobs[i] = Number(e.target.value); setCorner(sel, { knobs }); }}>
                          {Array.from({ length: 16 }, (_, k) => <option key={k} value={k}>{`${k + 1} · ${knobName(k)}`}</option>)}
                        </select>
                      ) : (
                        <select value={cur.ccs[i].num} aria-label="Numéro de CC" onChange={(e) => { const ccs = cur.ccs.map((x, j) => (j === i ? { ...x, num: Number(e.target.value) } : x)); setCorner(sel, { ccs }); }}>
                          {Array.from({ length: 128 }, (_, n) => <option key={n} value={n}>{`CC ${n}`}</option>)}
                        </select>
                      )}
                    </span>
                    {cur.kind === "cc" && <LearnChip accept="cc" onLearn={(e) => {
                      // le canal appris vaut pour les 4 CC du coin (un seul canal par coin)
                      const ccs = ref.current.corners[sel].ccs.map((x, j) => ({ ch: e.ch, num: j === i ? e.num : x.num }));
                      setCorner(sel, { ccs });
                    }} />}
                    <span ref={(el) => { barRefs.current[i] = el; }} className="rc-pbar"
                      onPointerDown={drag((ev) => { const r = barRefs.current[i]!.getBoundingClientRect(); const vals = [...ref.current.corners[sel].vals]; vals[i] = clamp01((ev.clientX - r.left) / r.width); setCorner(sel, { vals }); })}>
                      <i style={{ width: `${(cur.vals[i] ?? 0) * 100}%` }} /></span>
                    <b>{valText(cur, i)}</b>
                  </div>
                ))}
                {cur.kind === "cc" && (
                  <div className="rc-prow"><span className="rc-plabel">Canal des 4 CC</span>
                    <span className="rc-selbtn" style={{ height: 24, padding: "0 8px" }}>{cur.ccs[0].ch}
                      <select value={cur.ccs[0].ch} aria-label="Canal" onChange={(e) => setCorner(sel, { ccs: cur.ccs.map((x) => ({ ...x, ch: Number(e.target.value) })) })}>
                        {Array.from({ length: 16 }, (_, n) => <option key={n} value={n + 1}>{`canal ${n + 1}`}</option>)}
                      </select></span></div>
                )}
                <button type="button" className="rc-take" onClick={take}>Prendre le son actuel</button>
              </>
            )}
          </div>
        ) : (
          <div className="rc-cbox" style={{ "--c": C.green } as React.CSSProperties}>
            <span className="rc-caption">Mouvement</span>
            <div className="rc-scenes">
              {ORBITS.slice(1).map((o, i) => <button key={o} type="button" className={st.orbit === i + 1 ? "on" : ""} onClick={() => { up({ orbit: i + 1 }); if (st.tempo) M.setTarget(M.T.host(M.HOST.vecOrbit), (i + 1) / 8, true); }}>{o}</button>)}
            </div>
            <span className="rc-caption">Mesures par tour</span>
            <div className="rc-scenes">
              {RATES.slice(0, 8).map((r, i) => <button key={r} type="button" className={st.orbitRate === i ? "on" : ""} onClick={() => { up({ orbitRate: i }); M.setTarget(M.T.host(9), i / 8, true); }}>{r}</button>)}
            </div>
            <div className="rc-prow"><span className="rc-plabel">Taille</span>
              <span ref={(el) => { barRefs.current[4] = el; }} className="rc-pbar"
                onPointerDown={drag((ev) => { const r = barRefs.current[4]!.getBoundingClientRect(); const v = clamp01((ev.clientX - r.left) / r.width); up({ orbitSize: v }); M.setTarget(M.T.host(10), v); })}>
                <i style={{ width: `${st.orbitSize * 100}%` }} /></span>
              <b>{Math.round(st.orbitSize * 100)} %</b></div>
            <span className="rc-note">L'orbite fait tourner le pavé de GOLEM sur le PC (coins « scène »).</span>
          </div>
        )}
        <span className="rc-flex" />
        <div className="rc-2" style={golem ? undefined : { gridTemplateColumns: "1fr" }}>
          <button type="button" className={st.spring ? "on" : ""} onClick={() => up({ spring: !st.spring })} title="L'orbe revient au centre quand on lâche">Ressort</button>
          {golem && <button type="button" className={st.tempo ? "on" : ""} disabled={!hasScene} title="L'orbe tourne au tempo de GOLEM (coins « scène »)"
            onClick={() => { const on = !st.tempo; up({ tempo: on }); sendScenes(); M.setTarget(M.T.host(M.HOST.vecOrbit), on ? st.orbit / 8 : 0, true); }}>Au tempo</button>}
        </div>
      </div>
    </div>
  );
}

// --- FM-1 / appareil MIDI -----------------------------------------------------------------------
const WHITE = [0, 2, 4, 5, 7, 9, 11];
const NOTE_NAMES = ["Do", "Do#", "Ré", "Ré#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"];
function Fm1Tab({ st, up, pc, golem }: { st: RemoteSave; up: (p: Partial<RemoteSave>) => void; pc: PcInfo; golem: boolean }) {
  const prof = FM_PROFILES.find((p) => p.id === st.fmProfile) ?? FM_PROFILES[0];
  const noteCh = prof.perTrack ? OPEN_CH[st.fmTrack] : st.fmCh;
  const cfgs = st.fmCfg[prof.id] ?? [];
  const ref = useRef(st); ref.current = st;
  const [edit, setEdit] = useState(false);
  const [editing, setEditing] = useState<{ kind: "knob" | "pad"; i: number } | null>(null);
  // réglage i : canal (0 = notes, -1 = effets, 1..16), CC, nom, ou repris d'un des 16 boutons
  const knobOf = (i: number) => {
    const c = cfgs[i] ?? {}, d = prof.knobs[i];
    // 16 canaux, comme en MIDI : le canal choisi, sinon celui du profil (effets du FM-1 : canal 2 par
    // défaut), sinon le canal des notes
    const set = c.ch ?? 0;
    const realCh = set > 0 ? set : set === -1 || (set === 0 && d.fx) ? st.fmFxCh : noteCh;
    const ch = realCh;
    const link = c.link ?? -1;
    if (link >= 0) {
      const b = st.btnCfg[link];
      return { link, name: c.name || buttonName(b, link, b.kind === "golem" ? pc.knobLabels[link] : undefined), sub: `bouton ${link + 1}`, fx: false, ch, realCh, cc: b.num };
    }
    const cc = c.cc ?? d.cc;
    return { link, name: c.name || d.label, sub: `CC ${cc}${realCh !== noteCh ? ` · canal ${realCh}` : ""}`, fx: !!d.fx, ch, realCh, cc };
  };
  const valOf = (i: number) => { const k = knobOf(i); return k.link >= 0 ? ref.current.knobs[k.link] ?? 0.5 : ref.current.fmVal[i] ?? 0.5; };
  const setVal = (i: number, v: number) => {
    const k = knobOf(i);
    if (k.link >= 0) { const knobs = [...ref.current.knobs]; knobs[k.link] = v; up({ knobs }); sendButton(ref.current.btnCfg[k.link], k.link, v); return; }
    const fmVal = [...ref.current.fmVal]; fmVal[i] = v; up({ fmVal }); M.cc(k.realCh, k.cc, v);
  };
  // 1.1 : changement de son en MIDI standard. Instrument : banque (CC 0, CC 32 à 0) puis programme ;
  // avec GOLEM le n° est continu (n° 140 = banque 1, programme 11). Effets des inserts 1 à 4 de GOLEM :
  // CC 102 à 105, valeur = n° - 1.
  const lastSent = useRef(0);
  const target = golem ? st.fmTarget : 0;
  const sendProg = (n: number) => {
    lastSent.current = Date.now(); up({ fmProg: n });
    if (golem && target > 0) { M.ccNow(noteCh, 101 + target, Math.min(127, n) / 127); return; }
    const bank = golem ? Math.floor(n / 128) : st.fmBank;
    M.ccNow(noteCh, 0, Math.min(127, bank) / 127); M.ccNow(noteCh, 32, 0); M.program(noteCh, n % 128);
  };
  // avec GOLEM, le PC dit quel son joue sur la piste qui reçoit ce canal (instrument ou insert 1 à 4),
  // et le téléphone suit les changements faits sur le PC
  const route = golem ? pcTrackFor(pc, noteCh) : { track: -1, scene: false, none: false };
  const tgt = route.track;
  const bankSlot = target;
  const cur = tgt >= 0 && !route.scene && !route.none ? pc.presets[tgt]?.slots[bankSlot] : undefined;
  const names = cur ? factoryNames(cur.type, bankSlot > 0) : [];
  const total = cur ? Math.max(cur.total, names.length) : 0;
  const progMax = golem && target === 0 ? Math.max(127, total - 1) : 127; // sons d'usine, puis ceux de l'utilisateur
  const page = Math.floor(Math.min(st.fmProg, progMax) / 128);
  const pages = Math.floor(progMax / 128) + 1;
  const prog = (d: number) => sendProg(Math.max(0, Math.min(progMax, st.fmProg + d)));
  useEffect(() => {
    if (cur && cur.prog >= 0 && cur.prog !== ref.current.fmProg && Date.now() - lastSent.current > 1500) up({ fmProg: cur.prog });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [cur?.prog, tgt, bankSlot]);
  const progLabel = () => {
    if (sent) return sent;
    if (!golem) return <>Son {st.fmProg + 1} <span>· changement de programme ▾</span></>;
    if (tgt < 0) return <>Son {st.fmProg + 1} <span>· aucune piste sur le canal {noteCh}</span></>;
    const track = `P${tgt + 1}`;
    if (route.scene) return <>Son {st.fmProg + 1} <span>· lance la scène {st.fmProg + 1} (réglez le canal MIDI de la piste {tgt + 1} pour changer son son)</span></>;
    if (route.none) return <>{track} <span>· réglez le canal MIDI de la piste {tgt + 1} pour changer son son</span></>;
    if (bankSlot > 0 && !cur?.type) return <>{track} <span>· pas d'effet dans l'insert {bankSlot}</span></>;
    if (cur && cur.prog >= 0) return <>{track} · Son {cur.prog + 1} <span>· {cur.name} ▾</span></>;
    if (cur?.name) return <>{track} · {cur.name} <span>· son à vous ▾</span></>;
    return <>{track} <span>· aucun préréglage ▾</span></>;
  };
  const progOptions = Array.from({ length: Math.min(128, progMax + 1 - page * 128) }, (_, i) => page * 128 + i);
  const fileRef = useRef<HTMLInputElement>(null);
  const [sent, setSent] = useState("");
  const syx = async (f: File | undefined) => {
    if (!f) return;
    if (f.size > 1024 * 1024) { setSent("Fichier trop gros (1 Mo au plus)"); return; }
    const n = M.sendSyx(new Uint8Array(await f.arrayBuffer()));
    setSent(n ? `${f.name} envoyé` : "Aucun son SysEx dans ce fichier, ou pas de connexion");
    window.setTimeout(() => setSent(""), 3000);
  };
  useEffect(() => () => M.allNotesOff(noteCh), [noteCh]);

  // pads : CC maintenu, CC bascule ou note
  const [padOn, setPadOn] = useState<boolean[]>(Array(8).fill(false));
  const padCh = (p: PadCfg) => (p.ch === 0 ? noteCh : p.ch);
  const padDown = (i: number) => {
    const p = st.pads[i];
    if (p.mode === "toggle") { const on = !padOn[i]; setPadOn((o) => o.map((x, j) => (j === i ? on : x))); M.ccNow(padCh(p), p.num, on ? 1 : 0); return; }
    setPadOn((o) => o.map((x, j) => (j === i ? true : x)));
    if (p.mode === "note") M.noteOn(padCh(p), p.num, 110); else M.ccNow(padCh(p), p.num, 1);
  };
  const padUp = (i: number) => {
    const p = st.pads[i];
    if (p.mode === "toggle") return;
    setPadOn((o) => o.map((x, j) => (j === i ? false : x)));
    if (p.mode === "note") M.noteOff(padCh(p), p.num); else M.ccNow(padCh(p), p.num, 0);
  };
  const padName = (p: PadCfg, i: number) => p.name || (p.mode === "note" ? NOTE_NUMS[p.num][1].split(" ")[0] : `CC ${p.num}`) || `Pad ${i + 1}`;

  // clavier : 2 octaves (14 touches blanches), plusieurs doigts
  const [held, setHeld] = useState<number[]>([]);
  const byPtr = useRef(new Map<number, number>());
  const base = 12 * (st.fmOct + 1);
  const noteAt = (ev: PointerEvent | React.PointerEvent): number | null => {
    const el = document.elementFromPoint(ev.clientX, ev.clientY) as HTMLElement | null;
    const n = el?.dataset.note;
    return n !== undefined ? Number(n) : null;
  };
  const press = (pid: number, n: number | null) => {
    const prev = byPtr.current.get(pid);
    if (prev === n) return;
    if (prev !== undefined) { M.noteOff(noteCh, prev); byPtr.current.delete(pid); }
    if (n !== null) { M.noteOn(noteCh, n, 100); byPtr.current.set(pid, n); }
    setHeld([...byPtr.current.values()]);
  };
  const kbDown = (e: React.PointerEvent) => {
    e.preventDefault();
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
    press(e.pointerId, noteAt(e));
  };
  const keys: { n: number; black: boolean; left: number }[] = [];
  for (let o = 0; o < 2; o++) for (let s2 = 0; s2 < 12; s2++) {
    const black = !WHITE.includes(s2);
    const wi = o * 7 + WHITE.filter((w) => w < s2).length;
    keys.push({ n: base + o * 12 + s2, black, left: black ? (wi / 14) * 100 : wi });
  }

  // roues : hauteur (revient au centre) et modulation (CC 1)
  const bendRef = useRef<HTMLDivElement>(null), modRef = useRef<HTMLDivElement>(null);
  const [bend, setBend] = useState(0);
  const yIn = (el: HTMLDivElement, ev: PointerEvent) => { const r = el.getBoundingClientRect(); return clamp01(1 - (ev.clientY - r.top) / r.height); };

  const ek = editing?.kind === "knob" ? knobOf(editing.i) : null;
  const ep = editing?.kind === "pad" ? st.pads[editing.i] : null;
  const chOpts: [number, string][] = CHANNELS;
  const linkOpts: [number, string][] = [[-1, "Non : un CC"], ...Array.from({ length: 16 }, (_, k) => [k, `Bouton ${k + 1} · ${buttonName(st.btnCfg[k], k, st.btnCfg[k].kind === "golem" ? pc.knobLabels[k] : undefined)}`] as [number, string])];

  return (
    <div className="rc-body">
      <div className="rc-wheels">
        <div ref={bendRef} className="rc-wheel" role="slider" aria-label="Hauteur" aria-valuenow={Math.round(bend * 100)}
          onPointerDown={drag((ev) => { const v = yIn(bendRef.current!, ev) * 2 - 1; setBend(v); M.bend(noteCh, v); }, () => { setBend(0); M.bend(noteCh, 0); })}>
          <div className="cap" style={{ top: `calc(${(1 - (bend + 1) / 2) * 100}% - 8px)` }} /><span>Hauteur</span></div>
        <div ref={modRef} className="rc-wheel modw" role="slider" aria-label="Modulation" aria-valuenow={Math.round(st.fmMod * 127)}
          onPointerDown={drag((ev) => { const v = yIn(modRef.current!, ev); up({ fmMod: v }); M.cc(noteCh, 1, v); })}>
          <div className="mod" style={{ height: `calc(${st.fmMod * 100}% - 8px)`, minHeight: 14 }} /><span>Mod.</span></div>
      </div>
      <div className="rc-col" style={{ gap: 6 }}>
        <div className="rc-fmtop">
          <button type="button" className="arrow" aria-label="Son précédent" onClick={() => prog(-1)}>◀</button>
          <span className="rc-prog rc-prog--sel" title={cur?.name || undefined}>{progLabel()}
            <select value={Math.min(st.fmProg, progMax)} aria-label="Son (changement de programme)" onChange={(e) => sendProg(Number(e.target.value))}>
              {progOptions.map((n) => <option key={n} value={n}>{n < names.length ? `${n + 1} · ${names[n]}` : names.length ? `${n + 1} · son à vous` : `Son ${n + 1}`}</option>)}
            </select></span>
          <button type="button" className="arrow" aria-label="Son suivant" onClick={() => prog(1)}>▶</button>
          {golem ? <>
            <span className="rc-selbtn" title="Ce que change le son choisi sur la piste : l'instrument, ou l'effet d'un insert">{target === 0 ? "Instrument" : `Insert ${target}`}
              <select value={target} aria-label="Changer" onChange={(e) => up({ fmTarget: Number(e.target.value) })}>
                <option value={0}>Changer : instrument</option>
                {[1, 2, 3, 4].map((k) => <option key={k} value={k}>{`Changer : effet de l'insert ${k}`}</option>)}
              </select></span>
            {pages > 1 && <span className="rc-selbtn" title="Sons par série de 128 (banque MIDI)">{`${page * 128 + 1}–${Math.min(progMax, page * 128 + 127) + 1}`}
              <select value={page} aria-label="Série de sons" onChange={(e) => up({ fmProg: Number(e.target.value) * 128 })}>
                {Array.from({ length: pages }, (_, p) => <option key={p} value={p}>{`Sons ${p * 128 + 1} à ${Math.min(progMax, p * 128 + 127) + 1} (banque ${p})`}</option>)}
              </select></span>}
          </> : (
            <span className="rc-selbtn" title="Banque (CC 0) envoyée avant le son">Banque {st.fmBank}
              <select value={st.fmBank} aria-label="Banque" onChange={(e) => up({ fmBank: Number(e.target.value) })}>
                {Array.from({ length: 128 }, (_, n) => <option key={n} value={n}>{`Banque ${n}`}</option>)}
              </select></span>
          )}
          <span className="rc-selbtn">{prof.perTrack ? `Piste ${st.fmTrack + 1}` : `Canal ${st.fmCh}`}
            {prof.perTrack ? (
              <select value={st.fmTrack} aria-label="Piste" onChange={(e) => up({ fmTrack: Number(e.target.value) })}>
                {OPEN_CH.map((c, i) => <option key={i} value={i}>{`Piste ${i + 1} (canal ${c})`}</option>)}
              </select>
            ) : (
              <select value={st.fmCh} aria-label="Canal des notes" onChange={(e) => up({ fmCh: Number(e.target.value) })}>
                {CHANNELS.map(([n, l]) => <option key={n} value={n}>{l}</option>)}
              </select>
            )}
          </span>
          <span className="rc-selbtn">Profil : {prof.name} ▾
            <select value={prof.id} aria-label="Profil" onChange={(e) => up({ fmProfile: e.target.value })}>
              {FM_PROFILES.map((p) => <option key={p.id} value={p.id}>{p.name}</option>)}
            </select>
          </span>
          <button type="button" className="rc-btn acc" onClick={() => fileRef.current?.click()}>Envoyer un son (.syx)</button>
          <input ref={fileRef} type="file" accept=".syx" hidden onChange={(e) => { void syx(e.target.files?.[0]); e.target.value = ""; }} />
        </div>
        <div className={`rc-fmknobs${edit ? " editing" : ""}`}>
          {prof.knobs.map((_, i) => {
            const k = knobOf(i);
            return (
              <div key={`${prof.id}${i}`} className="rc-fmk" role="button" tabIndex={0} aria-label={k.name}
                onPointerDown={edit ? undefined : relDrag(() => valOf(i), (v) => setVal(i, v))} onClick={edit ? () => setEditing({ kind: "knob", i }) : undefined}>
                <KnobSvg v={valOf(i)} size={38} color={k.link >= 0 ? C.green : k.fx ? C.blue : C.acc} />
                <span className="n">{k.name}</span>
                <span className="c">{k.sub}</span>
              </div>
            );
          })}
        </div>
        <div className={`rc-pads${edit ? " editing" : ""}`}>
          <button type="button" className={`rc-padedit${edit ? " on" : ""}`} onClick={() => setEdit(!edit)}
            title="Toucher un réglage ou un pad pour le renommer, choisir son CC, son canal, ou reprendre un des 16 boutons">{edit ? "Régler ✓" : "Régler"}</button>
          {st.pads.map((p, i) => (
            <button key={i} type="button" className={`rc-pad${padOn[i] ? " on" : ""} ${p.mode}`}
              onPointerDown={edit ? undefined : (e) => { e.preventDefault(); (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId); padDown(i); }}
              onPointerUp={edit ? undefined : () => padUp(i)} onPointerCancel={edit ? undefined : () => padUp(i)}
              onClick={edit ? () => setEditing({ kind: "pad", i }) : undefined}>
              <b>{padName(p, i)}</b><span>{p.mode === "gate" ? "maintenu" : p.mode === "toggle" ? "marche/arrêt" : "note"}</span>
            </button>
          ))}
        </div>
        <div className="rc-kb" onPointerDown={kbDown} onPointerMove={(e) => { if (byPtr.current.has(e.pointerId)) press(e.pointerId, noteAt(e)); }}
          onPointerUp={(e) => press(e.pointerId, null)} onPointerCancel={(e) => press(e.pointerId, null)}>
          {keys.filter((k) => !k.black).map((k) => <button key={k.n} type="button" data-note={k.n} className={`rc-wk${held.includes(k.n) ? " on" : ""}`} aria-label={`${NOTE_NAMES[k.n % 12]}${Math.floor(k.n / 12) - 1}`} />)}
          {keys.filter((k) => k.black).map((k) => <button key={k.n} type="button" data-note={k.n} className={`rc-bk${held.includes(k.n) ? " on" : ""}`} style={{ left: `calc(${k.left}% - 2.6%)` }} aria-label={`${NOTE_NAMES[k.n % 12]}${Math.floor(k.n / 12) - 1}`} />)}
          <div className="rc-oct" onPointerDown={(e) => e.stopPropagation()}>
            <button type="button" onClick={() => up({ fmOct: Math.max(0, st.fmOct - 1) })} aria-label="Octave plus bas">−</button>
            <span>Do{st.fmOct}</span>
            <button type="button" onClick={() => up({ fmOct: Math.min(8, st.fmOct + 1) })} aria-label="Octave plus haut">+</button>
          </div>
        </div>
      </div>
      {ek && editing && (
        <EditSheet title={`Réglage ${editing.i + 1} · profil ${prof.name}`} onClose={() => setEditing(null)} learn={{ num: "cc", ch: "ch", accept: "cc", extra: { link: -1 } }}
          onReset={() => { const list = [...cfgs]; list[editing.i] = {}; up({ fmCfg: { ...st.fmCfg, [prof.id]: list } }); }}
          fields={[
            { kind: "text", key: "name", label: "Nom", value: cfgs[editing.i]?.name ?? "", placeholder: prof.knobs[editing.i].label },
            { kind: "select", key: "link", label: "Reprendre un des 16 boutons", value: ek.link, options: linkOpts },
            { kind: "select", key: "cc", label: "Numéro de CC", value: cfgs[editing.i]?.cc ?? prof.knobs[editing.i].cc, options: CC_NUMS },
            { kind: "select", key: "ch", label: "Canal", value: ek.ch, options: chOpts },
          ]}
          onSave={(v) => {
            const list = [...cfgs];
            while (list.length < 8) list.push({});
            list[editing.i] = { name: String(v.name).trim() || undefined, link: Number(v.link) >= 0 ? Number(v.link) : undefined, cc: Number(v.cc), ch: Number(v.ch) };
            up({ fmCfg: { ...st.fmCfg, [prof.id]: list } });
          }} />
      )}
      {ep && editing && (
        <EditSheet title={`Pad ${editing.i + 1}`} onClose={() => setEditing(null)} learn={{ num: "num", ch: "ch", accept: "any", mode: "mode" }}
          onReset={() => { const pads = [...st.pads]; pads[editing.i] = DEF.pads[editing.i]; up({ pads }); }}
          fields={[
            { kind: "text", key: "name", label: "Nom", value: ep.name, placeholder: padName({ ...ep, name: "" }, editing.i) },
            { kind: "seg", key: "mode", label: "Mode", value: ep.mode, options: [["gate", "CC maintenu"], ["toggle", "CC marche/arrêt"], ["note", "Note"]] },
            { kind: "select", key: "num", label: "Numéro (CC ou note)", value: ep.num, options: ep.mode === "note" ? NOTE_NUMS : CC_NUMS },
            { kind: "select", key: "ch", label: "Canal", value: ep.ch || noteCh, options: CHANNELS },
          ]}
          onSave={(v) => { const pads = [...st.pads]; pads[editing.i] = { name: String(v.name).trim(), mode: v.mode === "toggle" ? "toggle" : v.mode === "note" ? "note" : "gate", ch: Number(v.ch), num: Number(v.num) }; up({ pads }); }} />
      )}
    </div>
  );
}
