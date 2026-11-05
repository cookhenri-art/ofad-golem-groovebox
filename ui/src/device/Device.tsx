// Groovebox — OFAD — AGPL-3.0-or-later
// L'appareil vu de face : écran tactile et ses 8 encodeurs, transport, crossfader,
// boutons d'écran, 16 pads et leurs modes. Dessiné à 1280×800 puis mis à l'échelle
// de la fenêtre en gardant ses proportions, comme un vrai appareil.

import { Welcome } from "./Welcome";
import { VectorPad } from "../screens/VectorPad";
import { AudioRecorder } from "./AudioRecorder";
import { useEffect, useRef, useState } from "react";
import { HwButton } from "../controls/HwButton";
import { TempoRow } from "../controls/TempoBox";
import { AssignKnobs, KnobAssignCapture, openKnobMods } from "./AssignKnobs";
import { ModSourcesDialog } from "../screens/ModSourcesDialog";
import { IconNext, IconPlay, IconPrev, IconRec, IconStop } from "../controls/Icons";
import { actions, hasNative, setPadOctave, setUi, showMessage, useStore, writePref, type PadMode, type ScreenMode } from "../engine/store";
import { MIDI_BREAK, MIDI_PHOTO, MIDI_RECALL, MIDI_BUS_MUTE_BASE, MIDI_BUS_SOLO_BASE, MIDI_FILL, MIDI_MUTE_BASE, MIDI_RECORD, MIDI_SOLO_BASE, MIDI_START, MIDI_STOP, MIDI_XFADE } from "../engine/protocol";
import { MidiConflict } from "./MidiConflict";
import { useLearnPress } from "../controls/useLearnPress";
import { busName, groupBase, numGroups, trackColor } from "../engine/params";
import { MixerScreen } from "../screens/MixerScreen";
import { ScenesScreen } from "../screens/ScenesScreen";
import { EncoderRowContext } from "../screens/ScreenFrame";
import { SequenceScreen } from "../screens/SequenceScreen";
import { SoundScreen } from "../screens/SoundScreen";
import { EffectsScreen } from "../screens/EffectsScreen";
import { InstrumentFace } from "../screens/InstrumentFace";
import { Pedalboard } from "../screens/Pedalboard";
import { TracksScreen } from "../screens/TracksScreen";
import { LaunchScreen } from "../screens/LaunchScreen";
import { SettingsScreen } from "../screens/SettingsScreen";
import { PadGrid } from "./PadGrid";

const DESIGN_W = 1280;
const DESIGN_H = 800;

const SCREENS: { id: ScreenMode; label: string }[] = [
  { id: "tracks", label: "Pistes" },
  { id: "launch", label: "Lancement" },
  { id: "sequence", label: "Séquence" },
  { id: "sound", label: "Son" },
  { id: "effects", label: "Effets" },
  { id: "mixer", label: "Mixeur" },
  { id: "scenes", label: "Scènes" },
  { id: "settings", label: "Réglages" },
];

const PAD_MODES: { id: PadMode; label: string; led: "amber" | "red" | "blue" }[] = [
  { id: "play", label: "Jouer", led: "amber" },
  { id: "mute", label: "Couper", led: "amber" },
  { id: "scenes", label: "Scènes", led: "blue" },
  { id: "steps", label: "Pas", led: "red" },
  { id: "erase", label: "Effacer", led: "red" },
];

/** v0.47 : téléphone en paysage (fenêtre large et basse) : disposition propre, l'écran sur toute la
    hauteur et un panneau à onglets à côté ; tout est ~50 % plus grand qu'avec l'appareil de tablette. */
const PHONE_W = 1177;
const PHONE_H = 530;
/** v0.46 fix20 : largeur de dessin des grandes fenêtres (façade, pédalier) sur un téléphone : bien
    plus étroite que la tablette, donc tout est plus gros ; ce qui ne tient pas passe en dessous. */
const PHONE_FACE_W = 960;
export function isPhoneViewport(w: number, h: number) { return h <= 540 && w / h >= 1.55; }

function useFitScale() {
  const [fit, setFit] = useState({ scale: 1, phone: false, ghost: 1, ghostW: DESIGN_W, ghostH: DESIGN_H, areaW: 0, areaH: 0 });
  useEffect(() => {
    // v0.46.2 : clavier du téléphone ouvert (saisie d'une valeur, d'un nom) : la zone visible rétrécit ;
    // on ne recalcule PAS la disposition (sinon tout grossit ou glisse et peut le rester). On la
    // recalcule quand la saisie se ferme.
    const typing = () => {
      const a = document.activeElement as HTMLElement | null;
      return !!a && (a.tagName === "INPUT" || a.tagName === "TEXTAREA" || a.isContentEditable);
    };
    let laidOut = false;
    const update = () => {
      if (laidOut && typing()) return;
      laidOut = true;
      // v0.46 fix16 : la plus petite des tailles annoncées (fenêtre, zone visible, page) : certaines
      // WebView Android annoncent une fenêtre plus grande que la partie réellement visible
      const vv = window.visualViewport, de = document.documentElement;
      const pick = (...v: (number | undefined)[]) => Math.min(...v.filter((x): x is number => !!x && x > 0));
      let w = pick(window.innerWidth, vv?.width, de?.clientWidth), h = pick(window.innerHeight, vv?.height, de?.clientHeight);
      // v0.46 fix17 : sur Android, la fenêtre de l'application peut être plus grande que l'écran
      // (seul son coin en haut à gauche se voit) : on se limite à la taille de l'écran
      let areaW = 0, areaH = 0;
      if (/Android/i.test(navigator.userAgent) && window.screen?.width > 0 && window.screen?.height > 0) {
        const big = Math.max(screen.width, screen.height), small = Math.min(screen.width, screen.height);
        const sw = w >= h ? big : small, sh = w >= h ? small : big;
        if (sw < w - 1 || sh < h - 1) { w = Math.min(w, sw); h = Math.min(h, sh); areaW = Math.floor(w); areaH = Math.floor(h); }
      }
      const phone = isPhoneViewport(w, h);
      const tablet = Math.min(w / DESIGN_W, h / DESIGN_H);
      // fenêtres de tablette sur un téléphone : hauteur de dessin gardée, largeur étirée à celle de l'écran
      const ghost = phone ? w / PHONE_FACE_W : h / DESIGN_H;
      // v0.46.4 : taille réellement visible, pour ce qui se pose hors de l'appareil (menus du téléphone) :
      // pas d'unités vh (certaines vues web Android les calculent mal)
      document.documentElement.style.setProperty("--app-w", `${Math.floor(w)}px`);
      document.documentElement.style.setProperty("--app-h", `${Math.floor(h)}px`);
      setFit({ scale: (phone ? Math.min(w / PHONE_W, h / PHONE_H) : tablet) * 0.995, phone, ghost, ghostW: phone ? PHONE_FACE_W : Math.round(DESIGN_H * w / h), ghostH: phone ? Math.floor(h / ghost) : DESIGN_H, areaW, areaH });
    };
    update();
    // fin de saisie : le clavier se referme, on recalcule (deux fois : le système finit de rendre la place)
    let t1 = 0, t2 = 0;
    const afterTyping = () => { window.clearTimeout(t1); window.clearTimeout(t2); t1 = window.setTimeout(update, 250); t2 = window.setTimeout(update, 800); };
    window.addEventListener("resize", update);
    window.addEventListener("orientationchange", update);
    window.visualViewport?.addEventListener("resize", update);
    document.addEventListener("focusout", afterTyping);
    return () => {
      window.clearTimeout(t1); window.clearTimeout(t2);
      window.removeEventListener("resize", update); window.removeEventListener("orientationchange", update);
      window.visualViewport?.removeEventListener("resize", update); document.removeEventListener("focusout", afterTyping);
    };
  }, []);
  return fit;
}

function Crossfader() {
  const xfade = useStore((s) => s.tick.g.xfade);
  const jump = (n: number) => actions.setGlobal("xfade", n);
  return (
    <div className="hw-xfade">
      <span className="silk">Crossfader</span>
      <div className="hw-xfade-row">
        <HwButton size="sm" label="A" led="blue" active={xfade <= 0.001} onPress={() => jump(0)} ariaLabel="Crossfader tout à A" />
        <div className="hw-xfade-slot" data-ctl={MIDI_XFADE} data-ctl-label="Crossfader" data-ctl-norm={xfade}>
          <span className="hw-xfade-ticks" aria-hidden />
          <input type="range" min={0} max={1000} step={1} value={Math.round(xfade * 1000)} aria-label="Crossfader"
            aria-valuetext={xfade <= 0.001 ? "A" : xfade >= 0.999 ? "B" : `${Math.round(xfade * 100)} % B`}
            onChange={(e) => jump(Number(e.target.value) / 1000)} onDoubleClick={() => jump(0.5)} />
        </div>
        <HwButton size="sm" label="B" led="magenta" active={xfade >= 0.999} onPress={() => jump(1)} ariaLabel="Crossfader tout à B" />
      </div>
    </div>
  );
}

/** Bouton à maintenir (Fill, Break) : actif tant qu'il est enfoncé ; Maj le verrouille. */
function HoldButton({ label, active, onChange, led }: { label: string; active: boolean; onChange: (on: boolean) => void; led: "amber" | "red" | "blue" | "magenta" }) {
  const shift = useStore((s) => s.ui.shift);
  return (
    <span onPointerDown={(e) => { if (e.button === 0 && !shift) { e.currentTarget.setPointerCapture(e.pointerId); onChange(true); } }}
      onPointerUp={() => { if (!shift) onChange(false); }} onPointerCancel={() => { if (!shift) onChange(false); }}>
      <HwButton size="md" label={label} led={led} active={active} onPress={() => { if (shift) onChange(!active); }}
        title={`${label} : maintenir (Maj : verrouiller)`} />
    </span>
  );
}

/** Break temporisé : un appui = Break pendant N temps, relâché seul sur un temps.
    Maj + appui (ou molette) : changer la durée. */
const TIMED_BEATS = [1, 2, 4, 8, 16];
function TimedBreak() {
  const shift = useStore((s) => s.ui.shift);
  const [i, setI] = useState(() => { try { const k = TIMED_BEATS.indexOf(Number(localStorage.getItem("grv.timedBreak") ?? 4)); return k >= 0 ? k : 2; } catch { return 2; } });
  const timedOn = useStore((s) => s.tick.t.tbrk ?? false);
  const beats = TIMED_BEATS[i];
  const choose = (k: number) => { setI(k); try { localStorage.setItem("grv.timedBreak", String(TIMED_BEATS[k])); } catch { /* stockage indisponible */ } };
  return (
    <span onWheel={(e) => choose((i + (e.deltaY < 0 ? 1 : TIMED_BEATS.length - 1)) % TIMED_BEATS.length)}>
      <HwButton size="md" led="magenta" active={timedOn}
        label={<span className="timed-break">Break<small>{beats >= 4 && beats % 4 === 0 ? `${beats / 4} mes.` : `${beats} t.`}</small></span>}
        onPress={() => (shift ? choose((i + 1) % TIMED_BEATS.length) : timedOn ? actions.breakTimed(0) : actions.breakTimed(beats))}
        title={`Break de ${beats} temps, relâché seul sur un temps (Maj + appui ou molette : changer la durée${timedOn ? " ; appui : arrêter" : ""})`} />
    </span>
  );
}

function ScreenSwitch() {
  const screen = useStore((s) => s.ui.screen);
  const ready = useStore((s) => s.ready);
  if (!ready) return <div className="empty-screen"><h2>Démarrage…</h2></div>;
  switch (screen) {
    case "tracks": return <TracksScreen />;
    case "effects": return <EffectsScreen />;
    case "sound": return <SoundScreen />;
    case "mixer": return <MixerScreen />;
    case "scenes": return <ScenesScreen />;
    case "launch": return <LaunchScreen />;
    case "settings": return <SettingsScreen />;
    default: return <SequenceScreen />;
  }
}

/** Plein écran (touche F11 aussi) : toute la surface de l'écran, puis retour en fenêtre. */
function FullscreenButton({ compact = false }: { compact?: boolean }) {
  const on = useStore((s) => s.ui.fullscreen);
  useEffect(() => {
    const key = (e: KeyboardEvent) => { if (e.key === "F11") { e.preventDefault(); void actions.toggleFullscreen(); } };
    window.addEventListener("keydown", key);
    return () => window.removeEventListener("keydown", key);
  }, []);
  return (
    <button type="button" className="top-full" aria-pressed={on} onClick={() => void actions.toggleFullscreen()}
      title={on ? "Revenir en fenêtre (F11)" : "Plein écran : l'application prend tout l'écran (F11)"}>
      {compact ? "⛶" : on ? "Quitter le plein écran" : "Plein écran"}
    </button>
  );
}

/** Annuler / rétablir (Ctrl+Z, Ctrl+Y ou Ctrl+Maj+Z) et photo des réglages, rappelée d'un bouton. */
function HistoryButtons() {
  const hist = useStore((s) => s.tick.hist);
  const canUndo = !!hist?.[0], canRedo = !!hist?.[1], hasPhoto = !!hist?.[2];
  useEffect(() => {
    const key = (e: KeyboardEvent) => {
      if (!(e.ctrlKey || e.metaKey) || e.altKey) return;
      if (e.repeat) { e.preventDefault(); return; } // touche tenue : un seul retour en arrière
      const el = e.target as HTMLElement | null;
      // dans un champ de texte, Ctrl+Z reste celui du champ
      if (el && (el.tagName === "INPUT" || el.tagName === "TEXTAREA" || el.tagName === "SELECT" || el.isContentEditable)) return;
      const k = e.key.toLowerCase();
      if (k === "z" && !e.shiftKey) { e.preventDefault(); void actions.undo(); }
      else if (k === "y" || (k === "z" && e.shiftKey)) { e.preventDefault(); void actions.redo(); }
    };
    window.addEventListener("keydown", key);
    return () => window.removeEventListener("keydown", key);
  }, []);
  const learn = useLearnPress();
  return (
    <div className="top-hist" role="group" aria-label="Historique et photo des réglages">
      <button type="button" className="top-hist-btn" disabled={!canUndo} onClick={() => void actions.undo()}
        title="Annuler le dernier changement (Ctrl+Z)" aria-label="Annuler">↶</button>
      <button type="button" className="top-hist-btn" disabled={!canRedo} onClick={() => void actions.redo()}
        title="Rétablir (Ctrl+Y)" aria-label="Rétablir">↷</button>
      <button type="button" className="top-photo" onClick={() => void actions.takePhoto()} {...learn(MIDI_PHOTO, "Photo des réglages")}
        title="Photo de tous les réglages du projet, à rappeler d'un bouton (remplace la photo précédente) — clic droit : affecter au contrôleur MIDI">Photo</button>
      {/* v0.46 : jamais « disabled » (le clic droit doit rester possible pour l'affecter au contrôleur) */}
      <button type="button" className={`top-photo${hasPhoto ? "" : " is-disabled"}`} aria-disabled={!hasPhoto}
        onClick={() => { if (hasPhoto) void actions.recallPhoto(); }} {...learn(MIDI_RECALL, "Rappel de la photo")}
        title={`${hasPhoto ? "Revenir à la photo des réglages (Ctrl+Z pour revenir avant le rappel)" : "Aucune photo prise"} — clic droit : affecter au contrôleur MIDI`}>Rappel</button>
      <button type="button" className="top-photo top-knobmods" onClick={openKnobMods} title="Modulations des 16 boutons assignables (LFO, fonction, suiveur, séquenceur propres aux boutons)">Mod. boutons</button>
    </div>
  );
}

/** v0.46 fix22 : pavé vectoriel en grand, par-dessus tout (hors de l'appareil mis à l'échelle). */
function VectorFullLayer() {
  // v0.46 fix28 : la vue live s'efface le temps d'ouvrir un instrument ou des effets, puis revient
  // (et le temps du panneau des modulations ; pendant « Affecter à un bouton… », la vue live revient : ses 16 boutons se touchent)
  const open = useStore((s) => !!s.ui.vecFull && !s.ui.face && !s.ui.pedals && (!s.ui.modPanel || s.ui.knobLfoAssign !== null));
  return open ? <VectorPad full /> : null;
}

/** v0.46 fix28 : bouton d'accès aux modulations des 16 boutons assignables (même panneau que celui des synthés). */
function KnobModButton() {
  const open = useStore((s) => !!s.ui.modPanel?.knobs);
  return <div><HwButton size="md" led="blue" active={open} onPress={openKnobMods} ariaLabel="Modulations des 16 boutons" label="Mod. boutons"
    title="Modulations des 16 boutons assignables : 4 sources (LFO, fonction, suiveur, séquenceur) propres aux boutons" /></div>;
}

/** v0.46 fix22 : Photo et Rappel des réglages en grands boutons (téléphone, onglet Commandes). */
function PhotoRow() {
  const hasPhoto = !!useStore((s) => s.tick.hist)?.[2];
  return (
    <div className="phone-row phone-row--photo">
      <Learn target={MIDI_PHOTO} what="Photo des réglages"><HwButton size="md" led="amber" onPress={() => void actions.takePhoto()} ariaLabel="Photo des réglages" label="Photo"
        title="Photo de tous les réglages du projet, à rappeler d'un bouton (remplace la photo précédente)" /></Learn>
      <Learn target={MIDI_RECALL} what="Rappel de la photo"><HwButton size="md" led="amber" active={hasPhoto} onPress={() => { if (hasPhoto) void actions.recallPhoto(); else showMessage("Aucune photo prise : touchez d'abord Photo"); }}
        ariaLabel="Rappel de la photo" label="Rappel" title="Revenir à la photo des réglages (↶ pour revenir avant le rappel)" /></Learn>
      <KnobModButton />
    </div>
  );
}

/** Témoin de charge (v0.23) : calcul audio (moyenne et pointe, en % du temps disponible ; au-delà
    de 100 % le son craque) et mémoire de l'application, dont les échantillons chargés. */
function PerfMeter({ compact = false }: { compact?: boolean }) {
  const perf = useStore((s) => s.tick.perf);
  if (!perf || perf.length < 4) return null;
  const [avg, peak, mem, smp, rate = 0, block = 0, periodMs = 0, total = 0, android = -1] = perf;
  // v0.46 fix17 : Android — indice de performance et grands cœurs du fil audio
  const androidText = android < 0 ? "" : `\nAndroid : indice de performance ${android >= 100 ? "actif" : "indisponible"}, ${android % 100 > 0 ? `son posé sur ${android % 100} grands cœurs` : "son non posé sur les grands cœurs"}.`;
  // v0.46 fix17 : mémoire en % de celle de l'appareil (en Mo si le système ne la donne pas)
  const memPct = total > 0 ? Math.min(100, (mem / total) * 100) : -1;
  const memShown = memPct < 0 ? null : memPct < 1 ? "< 1 %" : `${Math.round(memPct)} %`;
  const level = peak >= 85 ? " is-hot" : peak >= 60 ? " is-warm" : "";
  const mo = (v: number) => (v >= 1024 ? `${(v / 1024).toFixed(1).replace(".", ",")} Go` : `${Math.round(v)} Mo`);
  // v0.46 fix16 : réglages du son réellement utilisés (utile pour comprendre un appareil qui craque)
  const ms = (v: number) => v.toFixed(1).replace(".", ",");
  const audio = rate > 0 && block > 0
    ? `Son : ${Math.round(rate).toLocaleString("fr-FR")} Hz, blocs de ${Math.round(block)} échantillons (${ms((block / rate) * 1000)} ms), un bloc toutes les ${ms(periodMs)} ms en moyenne.`
    : "Son : pas encore de bloc calculé.";
  const details = `Calcul audio : ${Math.round(avg)} % en moyenne, ${Math.round(peak)} % en pointe (au-delà de 100 % le son craque).\n${audio}\nMémoire de l'application : ${mo(mem)}${total > 0 ? ` sur ${mo(total)} (${memShown})` : ""}, dont ${mo(smp)} d'échantillons.${androidText}`;
  return (
    <div className={`perf-meter${level}${compact ? " perf-meter--compact" : ""}`} role="status" title={details}
      onClick={() => showMessage(details.replace(/\n/g, " · "))}>
      <span className="perf-label">CPU</span>
      <span className="perf-bar" aria-hidden><span style={{ width: `${Math.min(100, avg)}%` }} /><i style={{ left: `${Math.min(100, peak)}%` }} /></span>
      <span className="perf-val perf-val--cpu tabular">{Math.round(avg)} %</span>
      <span className="perf-sep" aria-hidden>·</span>
      <span className="perf-val perf-val--mem tabular">{memShown !== null ? <><small className="perf-label">MÉM</small> {memShown}</> : mo(mem)}</span>
    </div>
  );
}

/** Zone apprenable au contrôleur MIDI : clic droit ou appui long sur le bouton qu'elle entoure. */
export function Learn({ target, what, children }: { target: number; what: string; children: React.ReactNode }) {
  const press = useRef<number | null>(null);
  const at = useRef<[number, number]>([0, 0]);
  const learn = () => { void actions.midiLearn(target); showMessage(`${what} : touchez un bouton ou un pad du contrôleur MIDI`); };
  const cancel = () => { if (press.current !== null) { window.clearTimeout(press.current); press.current = null; } };
  // v0.46 fix30 : seul un appui IMMOBILE apprend (un crossfader glissé vite déclenchait l'apprentissage)
  return (
    <span className="learn-zone" title={`${what} — clic droit : affecter au contrôleur MIDI`} data-ctl={target} data-ctl-label={what} data-ctl-norm={0}
      onContextMenu={(e) => { e.preventDefault(); cancel(); learn(); }}
      onPointerDownCapture={(e) => { if (e.pointerType !== "mouse") { at.current = [e.clientX, e.clientY]; cancel(); press.current = window.setTimeout(() => { press.current = null; learn(); }, 600); } }}
      onPointerMoveCapture={(e) => { if (press.current !== null && Math.hypot(e.clientX - at.current[0], e.clientY - at.current[1]) > 8) cancel(); }}
      onInputCapture={cancel} onChangeCapture={cancel}
      onPointerUpCapture={cancel} onPointerCancelCapture={cancel} onPointerLeave={cancel}>
      {children}
    </span>
  );
}

/** Coupure (M) et solo (S) des 8 pistes de la banque du mixeur, sous les 8 encodeurs.
    Clic droit ou appui long : apprentissage MIDI (un bouton ou un pad du contrôleur). */
function TrackMuteRow() {
  const bank = useStore((s) => s.ui.mixerBank);
  const mixerFirst = useStore((s) => s.ui.mixerFirst);
  const screen = useStore((s) => s.ui.screen);
  const tracks = useStore((s) => s.tracks.tracks);
  // les 8 pistes visibles dans le défilement du mixeur ou de la grille de lancement (sous les
  // 8 encodeurs) ; au mixeur, la page Retours montre les pistes 1 à 8
  // v0.31 : au mixeur, le défilement continue avec les groupes (positions 17 à 20)
  const cat = useStore((s) => s.catalog);
  const nGroups = cat ? numGroups(cat.layout) : 0;
  const buses = useStore((s) => s.tracks.buses);
  // v0.46 : page Groupes : la rangée du bas montre les pistes 13 à 16 puis les groupes G1 à G4
  const first = screen === "mixer" && bank === 3 && nGroups > 0 ? 8 + nGroups
    : screen === "mixer" && bank >= 2 ? 0 : bank === 1 && screen === "mixer" ? 8
    : Math.max(0, Math.min(screen === "mixer" ? 8 + nGroups : 8, mixerFirst));
  const longPress = useLearnPress(); // clic droit / appui long : affecter au contrôleur MIDI
  return (
    <div className="track-ms-row" role="group" aria-label={`Coupure et solo des pistes ${first + 1} à ${first + 8}`}>
      {Array.from({ length: 8 }, (_, i) => {
        const t = first + i;
        const tr = tracks[t];
        if (t >= 16 && cat) {
          // un groupe : coupure et solo du bus de groupe
          const k = t - 16, ch = groupBase(cat.layout) + k, b = buses?.[ch - cat.layout.numTracks];
          const nm = busName(cat.layout, ch);
          return (
            <div key={t} className="track-ms track-ms--group">
              <span className="track-ms-num" title={nm}>G{k + 1}</span>
              <button type="button" className={`track-ms-btn track-ms-btn--m${b?.mute ? " on" : ""}`} aria-pressed={!!b?.mute}
                title={`Couper ${nm} — clic droit : affecter au contrôleur MIDI`} onClick={() => void actions.busMute(ch, !b?.mute)}
                {...longPress(MIDI_BUS_MUTE_BASE - (ch - cat.layout.numTracks), `Coupure de ${nm}`)}>M</button>
              <button type="button" className={`track-ms-btn track-ms-btn--s${b?.solo ? " on" : ""}`} aria-pressed={!!b?.solo}
                title={`Solo de ${nm} — clic droit : affecter au contrôleur MIDI`} onClick={() => void actions.busSolo(ch, !b?.solo)}
                {...longPress(MIDI_BUS_SOLO_BASE - (ch - cat.layout.numTracks), `Solo de ${nm}`)}>S</button>
            </div>
          );
        }
        return (
          <div key={t} className="track-ms" style={{ "--k": trackColor(tr?.color ?? 0) } as React.CSSProperties}>
            <span className="track-ms-num" title={tr?.name}>{t + 1}</span>
            <button type="button" className={`track-ms-btn track-ms-btn--m${tr?.mute ? " on" : ""}`} aria-pressed={!!tr?.mute}
              title={`Couper la piste ${t + 1}${tr?.name ? ` (${tr.name})` : ""} — clic droit : affecter au contrôleur MIDI`}
              onClick={() => void actions.trackMute(t, !tr?.mute)} {...longPress(MIDI_MUTE_BASE - t, `Coupure de la piste ${t + 1}`)}>M</button>
            <button type="button" className={`track-ms-btn track-ms-btn--s${tr?.solo ? " on" : ""}`} aria-pressed={!!tr?.solo}
              title={`Solo de la piste ${t + 1}${tr?.name ? ` (${tr.name})` : ""} — clic droit : affecter au contrôleur MIDI`}
              onClick={() => void actions.trackSolo(t, !tr?.solo)} {...longPress(MIDI_SOLO_BASE - t, `Solo de la piste ${t + 1}`)}>S</button>
          </div>
        );
      })}
    </div>
  );
}

export function Device() {
  const fit = useFitScale();
  const area = fit.areaW > 0 ? { width: fit.areaW, height: fit.areaH, right: "auto", bottom: "auto" } as React.CSSProperties : undefined;
  const phoneUi = useStore((s) => !!s.ui.phone);
  // v0.46 fix21 : signaler qu'on se sert de l'appli (au plus toutes les 20 s) : sur téléphone, l'écran
  // reste allumé tant qu'on joue ou qu'on l'a touchée il y a moins de 10 minutes
  useEffect(() => {
    if (!hasNative("userActive")) return;
    let last = 0;
    const ping = () => { const t = Date.now(); if (t - last > 20000) { last = t; void actions.userActive(); } };
    window.addEventListener("pointerdown", ping, true);
    window.addEventListener("keydown", ping, true);
    return () => { window.removeEventListener("pointerdown", ping, true); window.removeEventListener("keydown", ping, true); };
  }, []);
  useEffect(() => { if (phoneUi !== fit.phone) setUi({ phone: fit.phone }); }, [fit.phone, phoneUi]);
  if (fit.phone) return <PhoneDevice scale={fit.scale} ghost={fit.ghost} ghostW={fit.ghostW} ghostH={fit.ghostH} area={area} />;
  return <TabletDevice scale={fit.scale} area={area} />;
}

/** Les boutons d'écran, ceux du transport et du jeu, l'octave : partagés par la tablette et le téléphone. */
function TransportButtons() {
  const t = useStore((s) => s.tick.t);
  return (
    <>
      <Learn target={MIDI_RECORD} what="Enregistrement"><HwButton size="lg" led="red" active={t.rec} onPress={actions.toggleRecord} ariaLabel="Enregistrer" label={<><IconRec /><span>Enr.</span></>} /></Learn>
      <Learn target={MIDI_START} what="Lecture"><HwButton size="lg" led="green" active={t.playing} onPress={actions.play} ariaLabel="Lecture" label={<><IconPlay /><span>Lecture</span></>} /></Learn>
      <Learn target={MIDI_STOP} what="Arrêt"><HwButton size="lg" led="none" onPress={actions.stop} ariaLabel="Arrêt" label={<><IconStop /><span>Arrêt</span></>} /></Learn>
    </>
  );
}
function PlayButtons() {
  const t = useStore((s) => s.tick.t);
  const shift = useStore((s) => s.ui.shift);
  return (
    <>
      <Learn target={MIDI_FILL} what="Fill"><HoldButton label="Fill" led="amber" active={t.fill && !t.brk} onChange={actions.fill} /></Learn>
      <Learn target={MIDI_BREAK} what="Break"><HoldButton label="Break" led="magenta" active={t.brk} onChange={actions.breakKey} /></Learn>
      <TimedBreak />
      <HwButton size="md" active={shift} onPress={() => setUi({ shift: !shift })} label="Maj"
        title="Maj verrouillée : accent sur les notes posées, scène envoyée de l'autre côté du fader, Fill / Break verrouillés" />
    </>
  );
}
function OctaveButtons() {
  const screen = useStore((s) => s.ui.screen);
  const shift = useStore((s) => s.ui.shift);
  const page = useStore((s) => s.ui.page);
  const octave = useStore((s) => s.ui.octave);
  const length = useStore((s) => s.pattern?.length ?? 16);
  const pages = Math.max(1, Math.ceil(length / 16));
  return (
    <>
      <HwButton size="md" led="none" onPress={() => (screen === "sequence" && shift ? setUi({ page: Math.max(0, page - 1) }) : setPadOctave(octave - 1))} ariaLabel="Octave plus bas (Maj : page précédente au séquenceur)" label={<IconPrev />} />
      <HwButton size="md" led="none" onPress={() => (screen === "sequence" && shift ? setUi({ page: Math.min(pages - 1, page + 1) }) : setPadOctave(octave + 1))} ariaLabel="Octave plus haut (Maj : page suivante au séquenceur)" label={<IconNext />} />
    </>
  );
}
function ScreenButtons() {
  const screen = useStore((s) => s.ui.screen);
  return (
    <div className="button-row button-row--screens">
      {SCREENS.map((s) => (
        <HwButton key={s.id} label={s.label} active={screen === s.id} onPress={() => setUi({ screen: s.id })} />
      ))}
    </div>
  );
}
function PadModeButtons() {
  const padMode = useStore((s) => s.ui.padMode);
  return (
    <div className="button-row button-row--padmodes">
      {PAD_MODES.map((m) => (
        <HwButton key={m.id} label={m.label} led={m.led} active={padMode === m.id} onPress={() => setUi({ padMode: m.id })} />
      ))}
    </div>
  );
}
function SceneBankButtons() {
  const bank = useStore((s) => s.ui.sceneBank);
  const editSlot = useStore((s) => s.ui.editSlot);
  return (
    <div className="button-row">
      <HwButton size="sm" label="1 – 16" led="blue" active={bank === 0} onPress={() => setUi({ sceneBank: 0 })} />
      <HwButton size="sm" label="17 – 32" led="blue" active={bank === 1} onPress={() => setUi({ sceneBank: 1 })} />
      <HwButton size="sm" label="Éditer A" led="blue" active={editSlot === "A"} onPress={() => setUi({ editSlot: editSlot === "A" ? null : "A", editBreak: false })} />
      <HwButton size="sm" label="Éditer B" led="magenta" active={editSlot === "B"} onPress={() => setUi({ editSlot: editSlot === "B" ? null : "B", editBreak: false })} />
    </div>
  );
}

/** v0.47 : l'appareil sur un téléphone en paysage. L'écran garde sa taille de dessin (toute la
    hauteur) ; à côté, deux onglets : Commandes (écrans, 8 encodeurs, transport, jeu, crossfader) et
    Pads (rôle des pads, 16 pads, banque de scènes). Les grandes fenêtres (façade, pédalier, accueil)
    gardent leur dessin de tablette, posées sur toute la fenêtre. */
function PhoneDevice({ scale, ghost, ghostW, ghostH, area }: { scale: number; ghost: number; ghostW: number; ghostH: number; area?: React.CSSProperties }) {
  const [encoderRow, setEncoderRow] = useState<HTMLElement | null>(null);
  const [tab, setTabState] = useState<"ctrl" | "pads">(() => { try { return localStorage.getItem("grv.phoneTab") === "pads" ? "pads" : "ctrl"; } catch { return "ctrl"; } });
  const setTab = (v: "ctrl" | "pads") => { setTabState(v); try { localStorage.setItem("grv.phoneTab", v); } catch { /* stockage indisponible */ } };
  return (
    <div className="stage" style={area}>
      <KnobAssignCapture />
      <VectorFullLayer />
      {/* v0.46 fix16 : l'appareil est posé dans une zone qui défile ; s'il ne tient pas en entier
          (bandeau du système, encoche, taille annoncée fausse), on le fait glisser au doigt. */}
      <div className="phone-scroll">
      <div className="phone-box" style={{ width: Math.ceil(PHONE_W * scale), height: Math.ceil(PHONE_H * scale) }}>
      <div className="device device--phone" style={{ transform: `scale(${scale})` }}>
        <div className="bezel">
          <div className="screen">
            <EncoderRowContext.Provider value={encoderRow}>
              <ScreenSwitch />
            </EncoderRowContext.Provider>
          </div>
        </div>
        <div className="phone-side">
          <header className="phone-top">
            <img className="top-golem" src="golem/golem-128.png" alt="" draggable={false} />
            <button type="button" className="top-synth" onClick={() => setUi({ face: true })}
              title="Ouvrir l'instrument de la piste choisie en grand (Échap ou ✕ pour refermer)">Synthé ↗</button>
            <span className="device-spacer" />
            <HistoryButtons />
            <AudioRecorder />
            <FullscreenButton compact />
          </header>
          <div className="phone-tabs" role="tablist" aria-label="Panneau">
            <button type="button" role="tab" aria-selected={tab === "ctrl"} className={tab === "ctrl" ? "on" : ""} onClick={() => setTab("ctrl")}>Commandes</button>
            <button type="button" role="tab" aria-selected={tab === "pads"} className={tab === "pads" ? "on" : ""} onClick={() => setTab("pads")}>Pads</button>
            {/* v0.46 fix19 : petits ▶ ■ retirés (ils débordaient) ; le transport est dans Commandes */}
            <PerfMeter compact />
          </div>
          {tab === "ctrl" ? (
            <div className="phone-panel phone-panel--ctrl">
              <ScreenButtons />
              <div className="encoder-row encoder-row--knobs" ref={setEncoderRow}><AssignKnobs /></div>
              {/* v0.46 fix28 : Photo / Rappel / Mod. boutons juste sous les 16 boutons ; le transport juste au-dessus du tempo */}
              <PhotoRow />
              <div className="phone-row"><PlayButtons /></div>
              <div className="phone-row phone-row--xf"><OctaveButtons /><Learn target={MIDI_XFADE} what="Crossfader"><Crossfader /></Learn></div>
              <div className="phone-row"><TransportButtons /></div>
              {/* v0.46 fix23 : tempo sous Photo / Rappel (− / valeur à taper / + / Tap) */}
              <TempoRow />
            </div>
          ) : (
            <div className="phone-panel phone-panel--pads">
              <PadModeButtons />
              <PadGrid />
              <SceneBankButtons />
            </div>
          )}
        </div>
      </div>
      </div>
      </div>
      <div className="device device--ghost device--ghost-phone" style={{ width: ghostW, height: ghostH, transform: `translate(-50%, -50%) scale(${ghost})` }}>
        <InstrumentFace />
        <ModSourcesDialog />
        <MidiConflict />
        <Pedalboard />
        <Welcome />
      </div>
    </div>
  );
}

function TabletDevice({ scale, area }: { scale: number; area?: React.CSSProperties }) {
  const [encoderRow, setEncoderRow] = useState<HTMLElement | null>(null);
  const screen = useStore((s) => s.ui.screen);
  const shift = useStore((s) => s.ui.shift);
  const page = useStore((s) => s.ui.page);
  const length = useStore((s) => s.pattern?.length ?? 16);
  const t = useStore((s) => s.tick.t);
  const padMode = useStore((s) => s.ui.padMode);
  const bank = useStore((s) => s.ui.sceneBank);
  const editSlot = useStore((s) => s.ui.editSlot);
  const octave = useStore((s) => s.ui.octave);
  const muteTarget = useStore((s) => s.ui.muteTarget);
  const focusDrum = useStore((s) => !!s.catalog?.instruments.find((i) => i.type === s.tracks.tracks[Math.min(s.focus, 15)]?.instrument)?.drumKit);
  const pages = Math.max(1, Math.ceil(length / 16));
  const lane = useStore((s) => s.ui.lane);

  return (
    <div className="stage" style={area}>
      <KnobAssignCapture />
      <VectorFullLayer />
      <div className="device" style={{ transform: `translate(-50%, -50%) scale(${scale})` }}>
        <InstrumentFace />
        <ModSourcesDialog />
        <MidiConflict />
        <Pedalboard />
        <Welcome />
        <header className="device-top">
          <span className="wordmark">OFAD GOLEM</span>
          {/* v0.46 fix6 : la mascotte OFAD GOLEM à la place du sous-titre */}
          <img className="top-golem" src="golem/golem-128.png" alt="" draggable={false} />
          <button type="button" className="top-synth" onClick={() => setUi({ face: true })}
            title="Ouvrir l'instrument de la piste choisie en grand, comme un synthé, avec son clavier (Échap pour refermer)">Ouvrir le synthé ↗</button>
          <span className="device-spacer" />
          <PerfMeter />
          <HistoryButtons />
          <AudioRecorder />
          <FullscreenButton />
          <span className="silk" title="One For All Distribution">OFAD Sound</span>
        </header>

        <div className="device-left">
          <div className="bezel">
            <div className="screen">
              <EncoderRowContext.Provider value={encoderRow}>
                <ScreenSwitch />
              </EncoderRowContext.Provider>
            </div>
          </div>

          <div className="encoder-row encoder-row--knobs" ref={setEncoderRow}><AssignKnobs /></div>
          <TrackMuteRow />

          <div className="device-bottom">
            <div className="cluster">
              <span className="silk">Transport</span>
              <div className="cluster-row">
                <Learn target={MIDI_RECORD} what="Enregistrement"><HwButton size="lg" led="red" active={t.rec} onPress={actions.toggleRecord} ariaLabel="Enregistrer" label={<><IconRec /><span>Enr.</span></>} /></Learn>
                <Learn target={MIDI_START} what="Lecture"><HwButton size="lg" led="green" active={t.playing} onPress={actions.play} ariaLabel="Lecture" label={<><IconPlay /><span>Lecture</span></>} /></Learn>
                <Learn target={MIDI_STOP} what="Arrêt"><HwButton size="lg" led="none" onPress={actions.stop} ariaLabel="Arrêt" label={<><IconStop /><span>Arrêt</span></>} /></Learn>
              </div>
            </div>

            <div className="cluster">
              <span className="silk">Jeu</span>
              <div className="cluster-row">
                <Learn target={MIDI_FILL} what="Fill"><HoldButton label="Fill" led="amber" active={t.fill && !t.brk} onChange={actions.fill} /></Learn>
                <Learn target={MIDI_BREAK} what="Break"><HoldButton label="Break" led="magenta" active={t.brk} onChange={actions.breakKey} /></Learn>
                <TimedBreak />
                <HwButton size="md" active={shift} onPress={() => setUi({ shift: !shift })} label="Maj"
                  title="Maj verrouillée : accent sur les notes posées, scène envoyée de l'autre côté du fader, Fill / Break verrouillés" />
              </div>
            </div>

            <div className="cluster">
              <span className="silk">Octave</span>
              <div className="cluster-row">
                <HwButton size="md" led="none" onPress={() => (screen === "sequence" && shift ? setUi({ page: Math.max(0, page - 1) }) : setPadOctave(octave - 1))} ariaLabel="Octave plus bas (Maj : page précédente au séquenceur)" label={<IconPrev />} />
                <HwButton size="md" led="none" onPress={() => (screen === "sequence" && shift ? setUi({ page: Math.min(pages - 1, page + 1) }) : setPadOctave(octave + 1))} ariaLabel="Octave plus haut (Maj : page suivante au séquenceur)" label={<IconNext />} />
              </div>
            </div>


            <Learn target={MIDI_XFADE} what="Crossfader"><Crossfader /></Learn>
          </div>
        </div>

        <div className="device-right">
          <div className="button-block">
            <span className="silk">Écran</span>
            <div className="button-row button-row--screens">
              {SCREENS.map((s) => (
                <HwButton key={s.id} label={s.label} active={screen === s.id} onPress={() => setUi({ screen: s.id })} />
              ))}
            </div>
          </div>

          <div className="button-block">
            <span className="silk silk--row">Rôle des pads{padMode === "play" && octave !== 0 ? ` · octave ${octave > 0 ? "+" : ""}${octave}` : ""}{padMode === "steps" ? ` · ligne ${lane + 1}, page ${page + 1}/${pages}` : ""}
              {padMode === "mute" && focusDrum && (
                <span className="mute-target" role="group" aria-label="Ce que coupent les pads">
                  <button type="button" className={muteTarget === "pads" ? "on" : ""} aria-pressed={muteTarget === "pads"}
                    title="Sur une batterie : chaque pad coupe un son (Maj + pad : solo du son)"
                    onClick={() => { setUi({ muteTarget: "pads" }); writePref("muteTracks", false); }}>Sons de la batterie</button>
                  <button type="button" className={muteTarget === "tracks" ? "on" : ""} aria-pressed={muteTarget === "tracks"}
                    title="Chaque pad coupe une des 16 pistes"
                    onClick={() => { setUi({ muteTarget: "tracks" }); writePref("muteTracks", true); }}>16 pistes</button>
                </span>
              )}
            </span>
            <div className="button-row button-row--padmodes">
              {PAD_MODES.map((m) => (
                <HwButton key={m.id} label={m.label} led={m.led} active={padMode === m.id} onPress={() => setUi({ padMode: m.id })} />
              ))}
            </div>
          </div>

          <PadGrid />

          <div className="button-block button-block--bank">
            <span className="silk">Banque de scènes</span>
            <div className="button-row">
              <HwButton size="sm" label="1 – 16" led="blue" active={bank === 0} onPress={() => setUi({ sceneBank: 0 })} />
              <HwButton size="sm" label="17 – 32" led="blue" active={bank === 1} onPress={() => setUi({ sceneBank: 1 })} />
              <HwButton size="sm" label="Éditer A" led="blue" active={editSlot === "A"} onPress={() => setUi({ editSlot: editSlot === "A" ? null : "A", editBreak: false })} />
              <HwButton size="sm" label="Éditer B" led="magenta" active={editSlot === "B"} onPress={() => setUi({ editSlot: editSlot === "B" ? null : "B", editBreak: false })} />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
