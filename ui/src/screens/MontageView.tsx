// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix8 : MONTAGE AUDIO (Lancement › Audio). Une prise enregistre chaque piste (après ses
// effets et son volume) et chaque retour ; à l'arrêt, chaque piste devient des clips là où elle a
// sonné. On retravaille ensuite le morceau en audio : couper, déplacer, copier, dupliquer,
// supprimer, volume et fondus des clips, aimant à la mesure, annuler / rétablir, export WAV / MP3.
// Mode montage allumé : les clips jouent à la place des instruments (groupes et principal en direct).

import { useEffect, useMemo, useRef, useState } from "react";
import { actions, getState, hasNative, showMessage, useStore } from "../engine/store";
import { trackColor } from "../engine/params";
import type { MontageClip } from "../engine/protocol";

const LABEL_W = 150;
const LANE_H = 46;
const RULER_H = 26;
const ZOOMS = [3, 5, 8, 12, 18, 26, 36, 50, 70, 100, 140];
const SNAPS: { id: string; label: string; beats: number }[] = [
  { id: "bar", label: "Mesure", beats: 4 }, { id: "beat", label: "Temps", beats: 1 },
  { id: "16", label: "1/16", beats: 0.25 }, { id: "free", label: "Libre", beats: 0 },
];
const RETURNS = ["A", "B", "C", "D"];

// historique (gardé quand on change d'onglet)
const undoStack: MontageClip[][] = [];
const redoStack: MontageClip[][] = [];
let clipboard: { lane: number; clip: MontageClip }[] = [];
const peaksCache = new Map<string, Uint8Array | null>();

const clipBeats = (c: MontageClip, rate: number, bpm: number) => (c[3] / rate) * (bpm / 60);

export function MontageView() {
  const m = useStore((s) => s.montage);
  const tracks = useStore((s) => s.tracks.tracks);
  const playing = useStore((s) => s.tick.t.playing);
  const mp = useStore((s) => s.tick.t.mp);
  const mrec = useStore((s) => s.tick.t.mrec);
  const [pxb, setPxb] = useState(18);
  const [snapId, setSnapId] = useState("beat");
  const [tool, setTool] = useState<"select" | "cut">("select");
  const [sel, setSel] = useState<number[]>([]);
  const [help, setHelp] = useState(false);
  const [drag, setDrag] = useState<{ kind: "move" | "left" | "right"; dBeat: number; dLane: number } | null>(null);
  const [loopDrag, setLoopDrag] = useState<[number, number] | null>(null);
  const [headDrag, setHeadDrag] = useState<number | null>(null);
  const [rect, setRect] = useState<{ x0: number; y0: number; x1: number; y1: number } | null>(null);
  const [ctx, setCtx] = useState<{ x: number; y: number; beat: number; lane: number; clip: number } | null>(null);
  const root = useRef<HTMLDivElement>(null);
  const [, setPeaksVersion] = useState(0);
  const scroller = useRef<HTMLDivElement>(null);

  useEffect(() => { void actions.refreshMontage(); }, []);
  // prise, découpage ou export en cours : on suit l'état
  const busy = !!m && (/…$/.test(m.status) || !!mrec);
  useEffect(() => {
    if (!busy) return;
    const id = window.setInterval(() => void actions.refreshMontage(), 600);
    return () => window.clearInterval(id);
  }, [busy]);
  useEffect(() => { if (!mrec) void actions.refreshMontage(); }, [mrec]);

  const snap = SNAPS.find((s) => s.id === snapId)?.beats ?? 0;
  const snapB = (b: number) => (snap > 0 ? Math.round(b / snap) * snap : Math.round(b * 64) / 64);

  const takes = useMemo(() => new Map((m?.takes ?? []).map((t) => [t[0], t])), [m?.takes]);
  const bpm = m?.bpm ?? 120;
  const clips = m?.clips ?? [];
  const rateOf = (c: MontageClip) => takes.get(c[1])?.[2] ?? 48000;
  const lenB = (c: MontageClip) => clipBeats(c, rateOf(c), bpm);
  const endBeat = clips.reduce((e, c) => Math.max(e, c[4] + lenB(c)), 0);

  // voies montrées : pistes avec un instrument, et toute voie qui a des clips
  const lanes = useMemo(() => {
    const set = new Set<number>();
    tracks.forEach((t, i) => { if (i < 16 && t.instrument) set.add(i); });
    clips.forEach((c) => set.add(c[0]));
    return [...set].sort((a, b) => a - b);
  }, [tracks, clips]);
  const laneRow = new Map(lanes.map((l, i) => [l, i]));
  const laneName = (l: number) => (l < 16 ? `P${l + 1} · ${tracks[l]?.name ?? ""}` : `Retour ${RETURNS[l - 16] ?? l - 15}`);
  const laneColor = (l: number) => (l < 16 ? trackColor(tracks[l]?.color ?? 0) : "oklch(0.7 0.02 250)");

  // crêtes des prises (pour la forme d'onde), demandées une fois
  useEffect(() => {
    for (const id of takes.keys()) {
      if (peaksCache.has(id)) continue;
      peaksCache.set(id, null);
      void actions.montagePeaks(id).then((b64) => {
        try { const bin = atob(b64); const a = new Uint8Array(bin.length); for (let i = 0; i < bin.length; ++i) a[i] = bin.charCodeAt(i); peaksCache.set(id, a); }
        catch { peaksCache.set(id, new Uint8Array(0)); }
        setPeaksVersion((v) => v + 1);
      });
    }
  }, [takes]);

  // raccourcis clavier : le gestionnaire du dernier rendu (posé plus bas)
  const keyRef = useRef<((e: KeyboardEvent) => void) | null>(null);
  // v0.46.2 : export : la boucle ou tout le montage (question posée seulement s'il y a une boucle)
  const [exportAsk, setExportAsk] = useState<number | null>(null);
  useEffect(() => {
    const onKey = (e: KeyboardEvent) => keyRef.current?.(e);
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  if (!m) return <div className="empty-screen"><h2>Montage audio</h2><p>Disponible dans l'application.</p></div>;

  const totalBeats = Math.max(64, Math.ceil((endBeat + 16) / 4) * 4, m.loop ? m.loopB + 8 : 0);
  const width = totalBeats * pxb;
  const head = playing && m.on ? (mp ?? m.playhead) : m.playhead;

  // --- modifications (avec annuler / rétablir) ---------------------------------------------
  const commit = (next: MontageClip[], select: number[] = []) => {
    undoStack.push(clips.map((c) => [...c] as MontageClip));
    if (undoStack.length > 100) undoStack.shift();
    redoStack.length = 0;
    setSel(select);
    void actions.montageSetClips(next);
  };
  const undo = () => {
    const prev = undoStack.pop();
    if (!prev) return;
    redoStack.push(clips.map((c) => [...c] as MontageClip));
    setSel([]);
    void actions.montageSetClips(prev);
  };
  const redo = () => {
    const next = redoStack.pop();
    if (!next) return;
    undoStack.push(clips.map((c) => [...c] as MontageClip));
    setSel([]);
    void actions.montageSetClips(next);
  };
  /** Coupe les clips donnés à la noire « at » (ceux qui la contiennent). */
  const splitAt = (idx: number[], at: number) => {
    const next: MontageClip[] = [];
    const selNext: number[] = [];
    clips.forEach((c, i) => {
      const b0 = c[4], b1 = b0 + lenB(c);
      if (!idx.includes(i) || at <= b0 + 1e-3 || at >= b1 - 1e-3) { next.push(c); return; }
      const cut = Math.round(((at - b0) * 60 / bpm) * rateOf(c));
      next.push([c[0], c[1], c[2], cut, b0, c[5], c[6], 0.004]);
      next.push([c[0], c[1], c[2] + cut, c[3] - cut, at, c[5], 0.004, c[7]]);
      selNext.push(next.length - 1);
    });
    if (next.length !== clips.length) commit(next, selNext);
  };
  const removeSel = () => { if (sel.length) commit(clips.filter((_, i) => !sel.includes(i))); };
  const copySel = () => {
    if (!sel.length) return;
    const first = Math.min(...sel.map((i) => clips[i][4]));
    clipboard = sel.map((i) => ({ lane: clips[i][0], clip: [...clips[i]] as MontageClip })).map((x) => ({ ...x, clip: [x.clip[0], x.clip[1], x.clip[2], x.clip[3], x.clip[4] - first, x.clip[5], x.clip[6], x.clip[7]] as MontageClip }));
    showMessage(`${sel.length} clip${sel.length > 1 ? "s" : ""} copié${sel.length > 1 ? "s" : ""} · Ctrl+V : collé à la tête de lecture`);
  };
  const paste = (atBeat?: number) => {
    if (!clipboard.length) return;
    const at = snapB(atBeat ?? m.playhead);
    const added = clipboard.map((x) => [x.clip[0], x.clip[1], x.clip[2], x.clip[3], at + x.clip[4], x.clip[5], x.clip[6], x.clip[7]] as MontageClip);
    commit([...clips, ...added], added.map((_, k) => clips.length + k));
  };
  const duplicateSel = () => {
    if (!sel.length) return;
    const a = Math.min(...sel.map((i) => clips[i][4]));
    const b = Math.max(...sel.map((i) => clips[i][4] + lenB(clips[i])));
    const shift = snap > 0 ? Math.ceil((b - a) / snap - 1e-6) * snap : b - a;
    const added = sel.map((i) => { const c = clips[i]; return [c[0], c[1], c[2], c[3], c[4] + shift, c[5], c[6], c[7]] as MontageClip; });
    commit([...clips, ...added], added.map((_, k) => clips.length + k));
  };
  const setField = (k: 5 | 6 | 7, v: number) => {
    if (!sel.length) return;
    commit(clips.map((c, i) => (sel.includes(i) ? (Object.assign([...c], { [k]: v }) as MontageClip) : c)), sel);
  };

  const askExport = (f: number) => {
    if (m.loop && m.loopB - m.loopA > 1e-6) setExportAsk(exportAsk === f ? null : f);
    else void actions.montageExport(f, false);
  };

  keyRef.current = (e: KeyboardEvent) => {
      // v0.46.2 : seuls les champs de texte gardent leurs touches (après le choix de l'aimant ou un curseur,
      // Ctrl+A, Suppr… agissent encore sur les clips)
      const t = e.target;
      if (t instanceof HTMLTextAreaElement || (t instanceof HTMLInputElement && !["range", "checkbox", "radio", "button"].includes(t.type))) return;
      const ctrl = e.ctrlKey || e.metaKey;
      if (e.key === "Delete" || e.key === "Backspace") { if (sel.length) { e.preventDefault(); removeSel(); } }
      else if (ctrl && e.code === "KeyZ") { e.preventDefault(); if (e.shiftKey) redo(); else undo(); }
      else if (ctrl && e.code === "KeyY") { e.preventDefault(); redo(); }
      else if (ctrl && e.code === "KeyC") { e.preventDefault(); copySel(); }
      else if (ctrl && e.code === "KeyV") { e.preventDefault(); paste(); }
      else if (ctrl && e.code === "KeyA") { e.preventDefault(); setSel(clips.map((_, i) => i)); }
      else if (e.key === "Escape" && (ctx || sel.length)) { setCtx(null); setSel([]); }
      else if (ctrl && e.code === "KeyD") { e.preventDefault(); duplicateSel(); }
      else if (!ctrl && e.code === "KeyS" && sel.length) { e.preventDefault(); splitAt(sel, snapB(head)); }
  };

  // --- gestes sur un clip ------------------------------------------------------------------
  const beatAt = (clientX: number) => {
    const el = scroller.current;
    if (!el) return 0;
    const r = el.getBoundingClientRect();
    const scale = r.width / el.offsetWidth || 1; // l'appareil est mis à l'échelle
    return Math.max(0, ((clientX - r.left) / scale + el.scrollLeft - LABEL_W) / pxb);
  };
  /** Point de la souris dans le contenu de la ligne de temps (pixels non mis à l'échelle). */
  const contentAt = (clientX: number, clientY: number) => {
    const el = scroller.current;
    if (!el) return { x: 0, y: 0 };
    const r = el.getBoundingClientRect();
    const scale = r.width / el.offsetWidth || 1;
    return { x: (clientX - r.left) / scale + el.scrollLeft, y: (clientY - r.top) / scale + el.scrollTop };
  };
  /** Position dans le cadre du montage (menu clic droit). */
  const rootAt = (clientX: number, clientY: number) => {
    const el = root.current;
    if (!el) return { x: 0, y: 0 };
    const r = el.getBoundingClientRect();
    const scale = r.width / el.offsetWidth || 1;
    return { x: (clientX - r.left) / scale, y: (clientY - r.top) / scale };
  };
  const openMenu = (e: React.MouseEvent, clip: number) => {
    e.preventDefault(); e.stopPropagation();
    const c = contentAt(e.clientX, e.clientY);
    const lane = lanes[Math.max(0, Math.min(lanes.length - 1, Math.floor((c.y - RULER_H) / LANE_H)))] ?? 0;
    if (clip >= 0 && !sel.includes(clip)) setSel([clip]);
    const at = rootAt(e.clientX, e.clientY);
    setCtx({ x: at.x, y: at.y, beat: beatAt(e.clientX), lane, clip });
  };
  /** Au doigt : n'agit que sur un toucher bref sans glisser ; si le doigt glisse, le navigateur
   *  fait défiler la vue (il envoie alors pointercancel) et rien n'est choisi. */
  const onTap = (e: React.PointerEvent, act: () => void) => {
    const el = e.currentTarget as HTMLElement;
    const x0 = e.clientX, y0 = e.clientY;
    let far = false;
    const move = (ev: PointerEvent) => { if (Math.hypot(ev.clientX - x0, ev.clientY - y0) > 10) far = true; };
    const end = (ev: PointerEvent) => {
      el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", end); el.removeEventListener("pointercancel", end);
      if (ev.type === "pointerup" && !far) act();
    };
    el.addEventListener("pointermove", move); el.addEventListener("pointerup", end); el.addEventListener("pointercancel", end);
  };
  // tirer dans le vide : rectangle de choix (Ctrl ou Maj : ajoute à ce qui est choisi)
  const onAreaDown = (e: React.PointerEvent) => {
    setCtx(null);
    if (e.button !== 0) return;
    // au doigt : glisser fait défiler (comme la grille) ; toucher sans glisser enlève le choix
    if (e.pointerType === "touch") { onTap(e, () => setSel([])); return; }
    const add = e.ctrlKey || e.metaKey || e.shiftKey;
    const p0 = contentAt(e.clientX, e.clientY);
    if (p0.x < LABEL_W || p0.y < RULER_H) return;
    const el = e.currentTarget as HTMLElement;
    el.setPointerCapture(e.pointerId);
    let moved = false, cur = { x0: p0.x, y0: p0.y, x1: p0.x, y1: p0.y };
    const move = (ev: PointerEvent) => {
      const p = contentAt(ev.clientX, ev.clientY);
      if (!moved && Math.hypot(p.x - p0.x, p.y - p0.y) < 4) return;
      moved = true;
      cur = { ...cur, x1: p.x, y1: p.y };
      setRect(cur);
    };
    const up = () => {
      el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", up); el.removeEventListener("pointercancel", up);
      setRect(null);
      if (!moved) { if (!add) setSel([]); return; }
      const x0 = Math.min(cur.x0, cur.x1), x1 = Math.max(cur.x0, cur.x1), y0 = Math.min(cur.y0, cur.y1), y1 = Math.max(cur.y0, cur.y1);
      const hit: number[] = [];
      clips.forEach((c, i) => {
        const row = laneRow.get(c[0]);
        if (row === undefined) return;
        const cx0 = LABEL_W + c[4] * pxb, cx1 = cx0 + lenB(c) * pxb, cy0 = RULER_H + row * LANE_H, cy1 = cy0 + LANE_H;
        if (cx1 >= x0 && cx0 <= x1 && cy1 >= y0 && cy0 <= y1) hit.push(i);
      });
      setSel(add ? [...new Set([...sel, ...hit])] : hit);
    };
    el.addEventListener("pointermove", move); el.addEventListener("pointerup", up); el.addEventListener("pointercancel", up);
  };
  const onClipDown = (e: React.PointerEvent, i: number) => {
    e.stopPropagation();
    if (e.button !== 0) return;
    const c = clips[i];
    if (tool === "cut") { splitAt([i], snapB(beatAt(e.clientX))); return; }
    // au doigt, un clip pas encore choisi : toucher le choisit, glisser fait défiler la vue.
    // Une fois choisi, on le tire pour le déplacer (voir touch-action dans styles.css).
    if (e.pointerType === "touch" && !sel.includes(i)) { onTap(e, () => setSel([i])); return; }
    let selected = sel;
    if (e.shiftKey || e.ctrlKey || e.metaKey) selected = sel.includes(i) ? sel.filter((k) => k !== i) : [...sel, i];
    else if (!sel.includes(i)) selected = [i];
    setSel(selected);
    const el = e.currentTarget as HTMLElement;
    const r = el.getBoundingClientRect();
    const scale = r.width / el.offsetWidth || 1;
    const xIn = (e.clientX - r.left) / scale;
    const kind: "move" | "left" | "right" = xIn < 8 ? "left" : xIn > el.offsetWidth - 8 ? "right" : "move";
    const x0 = e.clientX, y0 = e.clientY;
    const b0 = c[4], len0 = lenB(c);
    let dBeat = 0, dLane = 0;
    el.setPointerCapture(e.pointerId);
    const move = (ev: PointerEvent) => {
      const dx = (ev.clientX - x0) / scale / pxb;
      const dy = (ev.clientY - y0) / scale;
      if (kind === "move") {
        dBeat = snapB(b0 + dx) - b0;
        if (b0 + dBeat < 0) dBeat = -b0;
        dLane = Math.round(dy / LANE_H);
      } else if (kind === "left") dBeat = Math.min(snapB(b0 + dx), b0 + len0 - 1 / 16) - b0;
      else dBeat = Math.max(snapB(b0 + len0 + dx), b0 + 1 / 16) - (b0 + len0);
      setDrag({ kind, dBeat, dLane });
    };
    const up = () => {
      el.removeEventListener("pointermove", move);
      el.removeEventListener("pointerup", up);
      el.removeEventListener("pointercancel", up);
      setDrag(null);
      if (dBeat === 0 && dLane === 0) return;
      if (kind === "move") {
        const next = clips.map((k, j) => {
          if (!selected.includes(j)) return k;
          const row = laneRow.get(k[0]) ?? 0;
          const lane = lanes[Math.max(0, Math.min(lanes.length - 1, row + dLane))] ?? k[0];
          return [lane, k[1], k[2], k[3], Math.max(0, k[4] + dBeat), k[5], k[6], k[7]] as MontageClip;
        });
        commit(next, selected);
      } else {
        // v0.46.2 : bord tiré : tous les clips choisis bougent du même bord, de la même quantité
        const resize = (k: MontageClip): MontageClip => {
          const rate = rateOf(k), total = takes.get(k[1])?.[3] ?? k[2] + k[3];
          const dS = Math.round((dBeat * 60 / bpm) * rate);
          if (kind === "left") {
            const src = Math.max(0, Math.min(k[2] + k[3] - 64, k[2] + dS));
            const realD = src - k[2];
            return [k[0], k[1], src, k[3] - realD, k[4] + (realD / rate) * (bpm / 60), k[5], k[6], k[7]];
          }
          return [k[0], k[1], k[2], Math.max(64, Math.min(total - k[2], k[3] + dS)), k[4], k[5], k[6], k[7]];
        };
        commit(clips.map((k, j) => (selected.includes(j) ? resize(k) : k)), selected);
      }
    };
    el.addEventListener("pointermove", move);
    el.addEventListener("pointerup", up);
    el.addEventListener("pointercancel", up);
  };

  // règle (comme FL Studio) : clic gauche = tête de lecture (glisser la déplace) ; clic droit ou
  // Maj / Ctrl + glisser = nouvelle boucle. La barre de la boucle se déplace, ses bords s'étirent.
  const onRulerDown = (e: React.PointerEvent) => {
    setCtx(null);
    const el = e.currentTarget as HTMLElement;
    el.setPointerCapture(e.pointerId);
    const a = snapB(beatAt(e.clientX));
    const loopMode = e.button === 2 || e.shiftKey || e.ctrlKey || e.metaKey;
    if (!loopMode) {
      let b = a;
      setHeadDrag(a);
      const move = (ev: PointerEvent) => { b = snapB(beatAt(ev.clientX)); setHeadDrag(b); };
      const up = () => {
        el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", up); el.removeEventListener("pointercancel", up);
        setHeadDrag(null);
        void actions.montagePlayhead(b);
      };
      el.addEventListener("pointermove", move); el.addEventListener("pointerup", up); el.addEventListener("pointercancel", up);
      return;
    }
    let b = a;
    setLoopDrag([a, a]);
    const move = (ev: PointerEvent) => { b = snapB(beatAt(ev.clientX)); setLoopDrag([a, b]); };
    const up = () => {
      el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", up); el.removeEventListener("pointercancel", up);
      setLoopDrag(null);
      const lo = Math.min(a, b), hi = Math.max(a, b);
      if (hi - lo >= 0.25) void actions.montageSetFlags(m.on, true, lo, hi);
    };
    el.addEventListener("pointermove", move); el.addEventListener("pointerup", up); el.addEventListener("pointercancel", up);
  };
  /** Barre de la boucle : glisser le milieu la déplace, glisser un bord l'agrandit ou la raccourcit. */
  const onLoopDown = (e: React.PointerEvent) => {
    e.stopPropagation();
    if (e.button !== 0) return;
    const el = e.currentTarget as HTMLElement;
    el.setPointerCapture(e.pointerId);
    const r = el.getBoundingClientRect();
    const scale = r.width / el.offsetWidth || 1;
    const xIn = (e.clientX - r.left) / scale;
    const edge = xIn < 7 ? "a" : xIn > el.offsetWidth - 7 ? "b" : "move";
    const b0 = beatAt(e.clientX), A = m.loopA, B = m.loopB;
    let na = A, nb = B;
    setLoopDrag([A, B]);
    const move = (ev: PointerEvent) => {
      const d = beatAt(ev.clientX) - b0;
      if (edge === "move") { const s2 = Math.max(-A, snapB(A + d) - A); na = A + s2; nb = B + s2; }
      else if (edge === "a") { na = Math.min(snapB(A + d), B - Math.max(0.25, snap)); nb = B; }
      else { nb = Math.max(snapB(B + d), A + Math.max(0.25, snap)); na = A; }
      setLoopDrag([Math.max(0, na), nb]);
    };
    const up = () => {
      el.removeEventListener("pointermove", move); el.removeEventListener("pointerup", up); el.removeEventListener("pointercancel", up);
      setLoopDrag(null);
      if (na !== A || nb !== B) void actions.montageSetFlags(m.on, true, Math.max(0, na), nb);
    };
    el.addEventListener("pointermove", move); el.addEventListener("pointerup", up); el.addEventListener("pointercancel", up);
  };

  const record = async () => {
    if (mrec) { await actions.montageRecord(false); return; }
    if ((await actions.montageRecord(true)) !== "") return;
    // v0.46 fix12 : Boucle allumée → la prise dure exactement la boucle et s'arrête seule
    if (m.loop && m.loopB > m.loopA) {
      const bars = (m.loopB - m.loopA) / 4;
      showMessage(`Prise de la boucle : ${Number.isInteger(bars) ? bars : bars.toFixed(2)} mesure${bars > 1 ? "s" : ""}, arrêt automatique à sa fin`);
    }
    if (!getState().tick.t.playing) actions.play();
  };
  const fit = () => {
    const w = (scroller.current?.clientWidth ?? 900) - LABEL_W - 20;
    const z = Math.max(2, Math.min(140, Math.floor(w / Math.max(16, endBeat + 4))));
    setPxb(z);
  };
  const zoomBy = (d: number) => setPxb((p) => { const i = ZOOMS.findIndex((z) => z >= p); return ZOOMS[Math.max(0, Math.min(ZOOMS.length - 1, (i < 0 ? ZOOMS.length - 1 : i) + d))]; });

  const selClips = sel.map((i) => clips[i]).filter(Boolean);
  const loopA = loopDrag ? Math.min(...loopDrag) : m.loopA, loopB = loopDrag ? Math.max(...loopDrag) : m.loopB;

  return (
    <div className="mt" ref={root} style={{ "--pxb": `${pxb}px` } as React.CSSProperties} onPointerDown={(e) => { if (ctx && !(e.target as HTMLElement).closest(".mt-menu")) setCtx(null); }}>
      <div className="song-bar mt-bar">
        <button type="button" className={`touch touch--small${m.on ? " touch--on" : ""}`} onClick={() => void actions.montageSetFlags(!m.on, m.loop, m.loopA, m.loopB)}
          title="Allumé : la lecture joue les clips à la place des instruments (groupes et principal en direct)">Mode montage</button>
        <button type="button" className={`touch touch--small song-rec${mrec ? " is-on" : ""}`} onClick={() => void record()}
          title={mrec ? "Finir la prise (arrête la lecture) : les pistes deviennent des clips" : (m.loop ? "Enregistrer la boucle : la prise part à la mesure suivante, dure exactement la boucle et s'arrête seule ; chaque piste jouée devient un clip pile de la longueur de la boucle, posé au début de la boucle" : "Enregistrer une prise : chaque piste (après effets et volume) et chaque retour, pendant que vous jouez ; à l'arrêt, des clips là où chaque piste a sonné")}>
          {mrec ? "■ Finir la prise" : "● Enregistrer une prise"}</button>
        <button type="button" className={`touch touch--small${m.loop ? " touch--on" : ""}`} onClick={() => void actions.montageSetFlags(m.on, !m.loop, m.loopA, m.loopB)}
          title="Boucle (clic droit ou Maj + glisser sur la règle pour la placer). Allumée pendant une prise : la prise s'arrête seule à la fin de la boucle">Boucle</button>
        <span className="launch-sep" aria-hidden />
        <span className="segmented--screen" role="group" aria-label="Outil">
          <button type="button" className={tool === "select" ? "on" : ""} onClick={() => setTool("select")} title="Choisir, déplacer, rogner (bords du clip)">↖ Choisir</button>
          <button type="button" className={tool === "cut" ? "on" : ""} onClick={() => setTool("cut")} title="Ciseaux : toucher un clip le coupe à cet endroit (touche S : coupe à la tête de lecture)">✂ Ciseaux</button>
        </span>
        <label className="song-loopsel" title="Aimant : les clips se calent sur la grille quand on les déplace, les rogne ou les coupe">aimant
          <select value={snapId} onChange={(e) => setSnapId(e.target.value)} aria-label="Aimant">
            {SNAPS.map((s) => <option key={s.id} value={s.id}>{s.label}</option>)}
          </select>
        </label>
        <button type="button" className="touch touch--small" onClick={undo} disabled={!undoStack.length} title="Annuler (Ctrl+Z)" aria-label="Annuler">↶</button>
        <button type="button" className="touch touch--small" onClick={redo} disabled={!redoStack.length} title="Rétablir (Ctrl+Y)" aria-label="Rétablir">↷</button>
        <span className="song-zoom" role="group" aria-label="Zoom">
          <button type="button" className="touch touch--small" onClick={() => zoomBy(-1)} aria-label="Dézoomer">−</button>
          <button type="button" className="touch touch--small" onClick={fit}>Tout voir</button>
          <button type="button" className="touch touch--small" onClick={() => zoomBy(1)} aria-label="Zoomer">+</button>
        </span>
        <button type="button" className={`touch touch--small${exportAsk === 0 ? " touch--on" : ""}`} disabled={!clips.length || busy} onClick={() => askExport(0)} title="Exporter en WAV 24 bits (en temps réel, dans le dossier des enregistrements) : la boucle ou tout le montage">Export WAV</button>
        <button type="button" className={`touch touch--small${exportAsk === 1 ? " touch--on" : ""}`} disabled={!clips.length || busy} onClick={() => askExport(1)} title="Exporter en MP3 320 kbit/s : la boucle ou tout le montage">MP3</button>
        {exportAsk !== null && (
          <span className="mt-export-ask" role="group" aria-label="Que faut-il exporter ?">
            <button type="button" className="touch touch--small touch--on" onClick={() => { void actions.montageExport(exportAsk, true); setExportAsk(null); }}>
              La boucle (mes. {Math.floor(m.loopA / 4) + 1} à {Math.round(m.loopB / 4)})</button>
            <button type="button" className="touch touch--small" onClick={() => { void actions.montageExport(exportAsk, false); setExportAsk(null); }}>Tout le montage</button>
            <button type="button" className="touch touch--small touch--quiet" onClick={() => setExportAsk(null)}>Annuler</button>
          </span>
        )}
        {hasNative("audioRecReveal") && /export/i.test(m.status) && !busy && <button type="button" className="touch touch--small touch--quiet" onClick={() => void actions.audioRecReveal()}>Ouvrir le dossier</button>}
        <button type="button" className={`touch touch--small${help ? " touch--on" : ""}`} onClick={() => setHelp(!help)} aria-expanded={help}>?</button>
        {m.status && <span className="mt-status">{m.status}</span>}
      </div>

      {help && (
        <div className="song-help">
          <p><b>● Enregistrer une prise</b> : jouez votre morceau (grille, morceau, scènes, mutes…). Chaque piste, après ses effets et son volume, et chaque retour d'effet sont enregistrés. À la fin, chaque piste devient des <b>clips</b> là où elle a sonné : une coupure ou un silence d'une mesure sépare deux clips. Une nouvelle prise remplace ce qui était au même endroit.</p>
          <p><b>Mode montage</b> : la lecture joue les clips à la place des instruments ; les groupes et le principal restent réglables. <b>Règle</b> : clic = tête de lecture (glisser pour la déplacer) ; clic droit (ou Maj) + glisser = nouvelle boucle ; la barre orange de la boucle se déplace, ses bords s'étirent. La prise part au début de la mesure suivante et se pose à la tête de lecture (ramenée à la mesure). <b>Boucle allumée</b> : la prise dure exactement la boucle, <b>s'arrête toute seule</b> à sa fin (à l'échantillon près) et se pose au début de la boucle : chaque piste jouée devient un clip pile de la longueur de la boucle.</p>
          <p><b>Clips</b> : glisser pour déplacer (aussi vers une autre piste), les bords pour rogner, Ctrl ou Maj + clic pour en choisir plusieurs, tirer un rectangle dans le vide pour en prendre plusieurs, Ctrl+A : tout. Clic droit : menu (couper ici, copier, dupliquer, supprimer ; dans le vide : coller ici). ✂ ou touche S : couper. Ctrl+C / Ctrl+V : copier, coller à la tête de lecture. Ctrl+D : dupliquer à la suite. Suppr : supprimer. Ctrl+Z / Ctrl+Y : annuler, rétablir.</p>
          <p>Les fichiers des prises sont rangés dans le dossier « &lt;projet&gt; - audio » à côté du projet quand vous l'enregistrez.</p>
        </div>
      )}

      <div className="mt-scroll" ref={scroller} onPointerDown={onAreaDown} onContextMenu={(e) => openMenu(e, -1)}>
        <div className="mt-inner" style={{ width: width + LABEL_W, height: RULER_H + lanes.length * LANE_H }}>
          {/* règle */}
          <div className="mt-ruler" style={{ height: RULER_H }} onPointerDown={(e) => { e.stopPropagation(); onRulerDown(e); }} onContextMenu={(e) => { e.preventDefault(); e.stopPropagation(); }}
            title="Clic : tête de lecture · clic droit (ou Maj) + glisser : boucle · glisser la barre orange : déplacer la boucle, ses bords : l'agrandir">
            <span className="mt-corner" style={{ width: LABEL_W }}>{bpm.toFixed(1)} BPM</span>
            {Array.from({ length: Math.ceil(totalBeats / 4) }, (_, k) => k).filter((k) => pxb * 4 >= 24 || k % Math.ceil(24 / (pxb * 4)) === 0).map((k) => (
              <span key={k} className="mt-bar-n" style={{ left: LABEL_W + k * 4 * pxb }}>{k + 1}</span>
            ))}
            {(m.loop || loopDrag) && <span className="mt-loop" style={{ left: LABEL_W + loopA * pxb, width: Math.max(8, (loopB - loopA) * pxb) }} onPointerDown={onLoopDown}
              title="Glisser : déplacer la boucle · glisser un bord : l'agrandir ou la raccourcir" />}
          </div>
          {/* voies */}
          {lanes.map((l, row) => (
            <div key={l} className="mt-lane" style={{ top: RULER_H + row * LANE_H, height: LANE_H, "--k": laneColor(l) } as React.CSSProperties}>
              <span className="mt-lab" style={{ width: LABEL_W }} title={`${laneName(l)} — toucher : choisir tous ses clips (Ctrl / Maj : en plus)`}
                onPointerDown={(e) => {
                  e.stopPropagation();
                  const mine = clips.map((c, i) => (c[0] === l ? i : -1)).filter((i) => i >= 0);
                  if (e.pointerType === "touch") { onTap(e, () => setSel(mine)); return; }
                  setSel(e.ctrlKey || e.metaKey || e.shiftKey ? [...new Set([...sel, ...mine])] : mine);
                }}><i style={{ background: laneColor(l) }} />{laneName(l)}</span>
            </div>
          ))}
          {/* clips */}
          {clips.map((c, i) => {
            const row = laneRow.get(c[0]);
            if (row === undefined) return null;
            const isSel = sel.includes(i);
            let b0 = c[4], lb = lenB(c), r = row;
            if (drag && isSel) {
              if (drag.kind === "move") { b0 += drag.dBeat; r = Math.max(0, Math.min(lanes.length - 1, row + drag.dLane)); }
              else if (drag.kind === "left") { b0 += drag.dBeat; lb -= drag.dBeat; }
              else if (drag.kind === "right") lb += drag.dBeat;
            }
            const w = Math.max(3, lb * pxb);
            const rate = rateOf(c);
            const fi = Math.min(w / 2, ((c[6] * bpm) / 60) * pxb), fo = Math.min(w / 2, ((c[7] * bpm) / 60) * pxb);
            return (
              <div key={i} className={`mt-clip${isSel ? " is-sel" : ""}${tool === "cut" ? " is-cut" : ""}${takes.get(c[1])?.[4] === 0 ? " is-missing" : ""}`}
                style={{ left: LABEL_W + b0 * pxb, top: RULER_H + r * LANE_H + 3, width: w, height: LANE_H - 6, "--k": laneColor(lanes[r] ?? c[0]) } as React.CSSProperties}
                onPointerDown={(e) => onClipDown(e, i)} onContextMenu={(e) => openMenu(e, i)}
                title={`${laneName(c[0])} · ${(c[3] / rate).toFixed(2)} s${c[5] ? ` · ${c[5] > 0 ? "+" : ""}${c[5].toFixed(1)} dB` : ""}${takes.get(c[1])?.[4] === 0 ? " · fichier introuvable" : ""}`}>
                <Wave peaks={peaksCache.get(c[1]) ?? null} src={c[2]} len={c[3]} width={w} height={LANE_H - 6} gain={c[5]} />
                {fi > 2 && <svg className="mt-fade" style={{ left: 0, width: fi }} viewBox="0 0 10 10" preserveAspectRatio="none"><polygon points="0,0 10,0 0,10" /></svg>}
                {fo > 2 && <svg className="mt-fade" style={{ right: 0, width: fo }} viewBox="0 0 10 10" preserveAspectRatio="none"><polygon points="0,0 10,0 10,10" /></svg>}
              </div>
            );
          })}
          {/* boucle, tête de lecture */}
          {(m.loop || loopDrag) && <div className="mt-loopzone" style={{ left: LABEL_W + loopA * pxb, width: (loopB - loopA) * pxb, top: RULER_H, height: lanes.length * LANE_H }} />}
          <div className="mt-head" style={{ left: LABEL_W + (headDrag ?? head) * pxb, height: RULER_H + lanes.length * LANE_H }} />
          {rect && <div className="mt-rect" style={{ left: Math.min(rect.x0, rect.x1), top: Math.min(rect.y0, rect.y1), width: Math.abs(rect.x1 - rect.x0), height: Math.abs(rect.y1 - rect.y0) }} />}
          {!clips.length && (
            <div className="mt-empty" style={{ left: LABEL_W + 16, top: RULER_H + 12 }}>
              <img src="golem/golem-128.png" alt="" />
              <p><b>Aucun clip.</b> Touchez « ● Enregistrer une prise » et jouez votre morceau : chaque piste devient des clips audio, à couper, déplacer et dupliquer ici.</p>
            </div>
          )}
        </div>
      </div>

      {ctx && (
        <div className="mt-menu" role="menu" style={{ left: ctx.x, top: ctx.y }} onPointerDown={(e) => e.stopPropagation()}>
          {ctx.clip >= 0 ? (<>
            <button type="button" role="menuitem" onClick={() => { splitAt(sel.includes(ctx.clip) ? sel : [ctx.clip], snapB(ctx.beat)); setCtx(null); }}>✂ Couper ici</button>
            <button type="button" role="menuitem" onClick={() => { copySel(); setCtx(null); }}>Copier <kbd>Ctrl+C</kbd></button>
            <button type="button" role="menuitem" onClick={() => { duplicateSel(); setCtx(null); }}>Dupliquer <kbd>Ctrl+D</kbd></button>
            <button type="button" role="menuitem" onClick={() => { setField(5, 0); setCtx(null); }}>Volume à 0 dB</button>
            <button type="button" role="menuitem" className="danger" onClick={() => { removeSel(); setCtx(null); }}>Supprimer <kbd>Suppr</kbd></button>
          </>) : (<>
            <button type="button" role="menuitem" disabled={!clipboard.length} onClick={() => { paste(ctx.beat); setCtx(null); }}>Coller ici <kbd>Ctrl+V</kbd></button>
            <button type="button" role="menuitem" onClick={() => { void actions.montagePlayhead(snapB(ctx.beat)); setCtx(null); }}>Tête de lecture ici</button>
            <button type="button" role="menuitem" disabled={!clips.length} onClick={() => { setSel(clips.map((_, i) => i)); setCtx(null); }}>Tout choisir <kbd>Ctrl+A</kbd></button>
          </>)}
        </div>
      )}

      {selClips.length > 0 && (
        <div className="song-actions mt-insp" onPointerDown={(e) => e.stopPropagation()}>
          <span className="song-sel">{selClips.length} clip{selClips.length > 1 ? "s" : ""}</span>
          <label className="mt-field">Volume
            <input type="range" min={-24} max={12} step={0.5} value={selClips[0][5]} onChange={(e) => setField(5, Number(e.target.value))} />
            <b className="tabular">{selClips[0][5] > 0 ? "+" : ""}{selClips[0][5].toFixed(1)} dB</b>
          </label>
          <label className="mt-field">Fondu entrée
            <input type="range" min={0} max={4} step={0.01} value={selClips[0][6]} onChange={(e) => setField(6, Number(e.target.value))} />
            <b className="tabular">{Math.round(selClips[0][6] * 1000)} ms</b>
          </label>
          <label className="mt-field">Fondu sortie
            <input type="range" min={0} max={4} step={0.01} value={selClips[0][7]} onChange={(e) => setField(7, Number(e.target.value))} />
            <b className="tabular">{Math.round(selClips[0][7] * 1000)} ms</b>
          </label>
          <button type="button" className="touch touch--small" onClick={() => splitAt(sel, snapB(head))} title="Couper à la tête de lecture (S)">✂ Couper ici</button>
          <button type="button" className="touch touch--small" onClick={copySel}>Copier</button>
          <button type="button" className="touch touch--small" onClick={() => paste()} disabled={!clipboard.length}>Coller</button>
          <button type="button" className="touch touch--small" onClick={duplicateSel}>Dupliquer</button>
          <button type="button" className="touch touch--small touch--quiet" onClick={removeSel}>Supprimer</button>
        </div>
      )}
    </div>
  );
}

/** Forme d'onde d'un clip (crêtes de la prise, un octet par 1024 échantillons). */
function Wave({ peaks, src, len, width, height, gain }: { peaks: Uint8Array | null; src: number; len: number; width: number; height: number; gain: number }) {
  const d = useMemo(() => {
    if (!peaks || !peaks.length || width < 4) return "";
    const g = Math.min(2, Math.pow(10, gain / 20));
    const step = 2;
    const mid = height / 2;
    let path = "";
    for (let x = 0; x < width; x += step) {
      const a = Math.floor((src + (x / width) * len) / 1024), b = Math.max(a + 1, Math.floor((src + ((x + step) / width) * len) / 1024));
      let v = 0;
      for (let k = a; k < b && k < peaks.length; ++k) v = Math.max(v, peaks[k]);
      const h = Math.max(0.5, Math.min(mid - 1, (v / 255) * mid * g));
      path += `M${x + 0.5},${(mid - h).toFixed(1)}V${(mid + h).toFixed(1)}`;
    }
    return path;
  }, [peaks, src, len, width, height, gain]);
  return (
    <svg className="mt-wave" width={width} height={height} aria-hidden>
      {d && <path d={d} />}
    </svg>
  );
}
