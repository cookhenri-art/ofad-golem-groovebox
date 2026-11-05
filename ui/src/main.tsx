// Groovebox — OFAD — AGPL-3.0-or-later
import "./boot";
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "./styles.css";
import "./skin/skin.css";

// v0.32.1 : pas de menu du navigateur au clic droit (Retour, Actualiser, Imprimer…) : « Actualiser »
// rechargerait l'interface en plein jeu. Les champs de texte gardent le leur (copier, coller).
document.addEventListener("contextmenu", (e) => {
  const t = e.target as HTMLElement | null;
  if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
  e.preventDefault();
});

// v0.46 fix28 : l'écran ne se zoome ni ne se déplace (pincer, Ctrl + molette, Ctrl + / - / 0 ; le double
// toucher est coupé par touch-action, sans perdre les touchers rapides des boutons) : le zoom du navigateur cassait la disposition. Les gestes à deux doigts des commandes
// (pavé vectoriel : taille et rotation) passent par les évènements de pointeur et restent actifs.
document.addEventListener("wheel", (e) => { if (e.ctrlKey) e.preventDefault(); }, { passive: false });
document.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && ["+", "-", "=", "0", "_"].includes(e.key)) e.preventDefault();
}, { capture: true });
document.addEventListener("touchmove", (e) => { if (e.touches.length > 1) e.preventDefault(); }, { passive: false });
for (const g of ["gesturestart", "gesturechange", "gestureend"]) document.addEventListener(g, (e) => e.preventDefault(), { passive: false });

// v0.46.2 : l'appareil reste fixe, quoi qu'il arrive : ni la page ni la zone de l'appareil (téléphone) ne
// défilent, ni au doigt ni par un champ qui prend la main (clavier) ; les listes et panneaux gardent
// leur propre défilement.
const pinned = (el: Element | null) => !!el && (el === document.scrollingElement || el === document.documentElement || el === document.body
  || (el instanceof HTMLElement && (el.classList.contains("phone-scroll") || el.classList.contains("stage") || el.id === "root")));
document.addEventListener("scroll", (e) => {
  const t = e.target === document ? document.scrollingElement : (e.target as Element | null);
  if (t && pinned(t) && (t.scrollTop !== 0 || t.scrollLeft !== 0)) { t.scrollTop = 0; t.scrollLeft = 0; }
}, { capture: true, passive: true });
// v0.47.0 : le geste qui ferait bouger l'écran est arrêté AVANT qu'il bouge (la remise en place après coup
// faisait trembler l'écran une seconde). Un glissé au doigt n'est laissé au navigateur que si une liste
// ou un panneau sous le doigt peut défiler dans ce sens ; sinon, rien ne défile (les commandes, qui lisent
// les évènements de pointeur, continuent de recevoir le geste).
{
  let x0 = 0, y0 = 0, decided = false, allow = true;
  const canScroll = (from: EventTarget | null, dx: number, dy: number) => {
    for (let el = from instanceof Element ? from : null; el && el !== document.body && el !== document.documentElement; el = el.parentElement) {
      if (!(el instanceof HTMLElement) || pinned(el)) continue;
      const st = getComputedStyle(el);
      const vertical = Math.abs(dy) >= Math.abs(dx);
      const ov = vertical ? st.overflowY : st.overflowX;
      if (ov !== "auto" && ov !== "scroll") continue;
      if (vertical) {
        if (el.scrollHeight - el.clientHeight < 1) continue;
        // le doigt monte (dy < 0) : la liste descend, il faut de la place en bas ; et inversement
        if ((dy < 0 && el.scrollTop + el.clientHeight < el.scrollHeight - 1) || (dy > 0 && el.scrollTop > 0)) return true;
      } else {
        if (el.scrollWidth - el.clientWidth < 1) continue;
        if ((dx < 0 && el.scrollLeft + el.clientWidth < el.scrollWidth - 1) || (dx > 0 && el.scrollLeft > 0)) return true;
      }
    }
    return false;
  };
  document.addEventListener("touchstart", (e) => {
    if (e.touches.length !== 1) return;
    x0 = e.touches[0].clientX; y0 = e.touches[0].clientY; decided = false; allow = true;
  }, { passive: true, capture: true });
  document.addEventListener("touchmove", (e) => {
    if (e.touches.length !== 1 || !e.cancelable) return;
    if (!decided) {
      const dx = e.touches[0].clientX - x0, dy = e.touches[0].clientY - y0;
      if (Math.abs(dx) < 2 && Math.abs(dy) < 2) return;
      decided = true;
      allow = canScroll(e.target, dx, dy);
    }
    if (!allow) e.preventDefault();
  }, { passive: false, capture: true });
}
// v0.46.2 : la zone visible (téléphone) ne doit pas glisser non plus : hors saisie, on la ramène en (0, 0)
{
  const vv = window.visualViewport;
  const typing = () => { const a = document.activeElement as HTMLElement | null; return !!a && (a.tagName === "INPUT" || a.tagName === "TEXTAREA" || a.isContentEditable); };
  const back = () => { if (vv && !typing() && (Math.abs(vv.offsetTop) > 0.5 || Math.abs(vv.offsetLeft) > 0.5)) window.scrollTo(0, 0); };
  vv?.addEventListener("scroll", back);
  vv?.addEventListener("resize", back);
  document.addEventListener("focusout", () => window.setTimeout(back, 300));
}

/** v0.46 fix30 : premier lancement sans langue connue (APK, ou PC sans choix à l'installation) : on la demande. */
function askLanguage(): Promise<"fr" | "en"> {
  return new Promise((resolve) => {
    const box = document.createElement("div");
    box.className = "lang-first";
    box.setAttribute("translate", "no");
    box.innerHTML = `<div class="lang-first-card"><img src="golem/golem-128.png" alt="" width="96" height="96">
      <b>OFAD GOLEM</b><span>Langue de l'interface · Interface language</span>
      <div class="lang-first-btns"><button type="button" data-l="fr">Français</button><button type="button" data-l="en">English</button></div>
      <small>Modifiable ensuite dans Réglages · You can change it later in Settings</small></div>`;
    box.addEventListener("click", (e) => {
      const l = (e.target as HTMLElement).closest("button")?.dataset.l;
      if (l !== "fr" && l !== "en") return;
      box.remove();
      resolve(l);
    });
    document.body.appendChild(box);
  });
}

async function start() {
  // Dans un simple navigateur, le moteur simulé remplace le plugin (développement, captures).
  const mock = typeof window.__JUCE__ === "undefined";
  if (mock) (await import("./engine/mock")).installMockBackend();
  // la bibliothèque JUCE lit window.__JUCE__ dès son chargement : tout le reste est chargé ensuite
  // v0.46 fix30 : interface en anglais (choix de Réglages, sinon la langue du système) : la traduction se fait à
  // l'affichage ; en français, son dictionnaire n'est même pas chargé
  //   ordre : choix fait dans l'application > choix d'une nouvelle installation (Windows) > question au premier
  //   lancement ; la langue est gardée par l'application (src/PluginEditor.cpp, fonction setLang)
  const { startLang, saveLang } = await import("./lang");
  let lang: "fr" | "en" | null = await startLang();
  if (!lang && !mock) { lang = await askLanguage(); await saveLang(lang); }
  if (!lang) lang = "fr"; // maquette de développement : français (captures)
  document.documentElement.lang = lang;
  if (lang === "en") (await import("./i18n")).startI18n("en");
  const [{ connect }, { App }] = await Promise.all([import("./engine/store"), import("./App")]);
  createRoot(document.getElementById("root")!).render(
    <StrictMode>
      <App />
    </StrictMode>,
  );
  await connect();
}

void start();
