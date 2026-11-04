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
  let lang: "fr" | "en" | null = startLang();
  if (!lang && !mock) { lang = await askLanguage(); await saveLang(lang); }
  if (!lang) lang = "fr"; // maquette de développement : français (captures)
  document.documentElement.lang = lang;
  if (lang === "en") (await import("./i18n")).startI18n();
  const [{ connect }, { App }] = await Promise.all([import("./engine/store"), import("./App")]);
  createRoot(document.getElementById("root")!).render(
    <StrictMode>
      <App />
    </StrictMode>,
  );
  await connect();
}

void start();
